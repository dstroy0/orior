// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_test.c: the Run pane's check through its table's hooks, against a catalog of the check's own: the Run tool has
// the table and it stands in the panel at the work's foot; with no tree it lists no rows and draws its head under its
// title; a group's row pressed opens it and a job's chooses it, its file and params standing under it; a param pressed
// takes the keys and the text typed reaches its value, Escape lets them go; the head's field takes the keys for the
// filter, which the CLI applies; Start with a required value missing draws the refusal and no run; Stop with no run
// going does nothing; a run's lines are drawn in their spans' colors with the escape bytes gone, its end last; the pane
// follows its last row as lines arrive; and the wheel scrolls no further than the last row fills the panel
#include "../../../../../ui/cli/src/runs/job_runs.h"
#include "../../../../../ui/cli/src/view/parts/run.h"
#include "../cli/cli_check.h"

#include <string.h>

#define RUN_HEIGHT 260

static CliJob s_jobs[8];
static CliParam s_params[8];
static CliStep s_steps[8];
static CliArg s_args[8];
static unsigned int s_lists[16];
static char s_text[4096];
static CliCatalog s_catalog;

// the check's catalog: a build job with a required word, and a sim job
static void catalog_laid(void)
{
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_max = 8u,
               .params = s_params, .params_max = 8u, .steps = s_steps, .steps_max = 8u, .request = s_args,
               .args_max = 8u, .lists = s_lists, .lists_max = 16u, .text_out = s_text, .text_max = sizeof(s_text));
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "build/alpha",
                     .group = "build", .title = "Alpha build", .file = "src/alpha.sh", .about = "builds the alpha",
                     .opens = "");
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = &s_catalog, .key = "name", .type = "word",
               .required = 1);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = &s_catalog, .text = "job.sh",
               .program = CLI_PROGRAM_BASH);
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "sim/gamma",
                     .group = "sim", .title = "Gamma sim", .file = "src/gamma.sh", .about = "a simulation", .opens = "");
}

// the row `at` of the CLI's rows, its type into `type`: 1, or 0 past the last
static int row_type(unsigned int at, unsigned int *type)
{
    CliJobRunRow row;
    const int read = EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = at, .row_out = &row);
    *type = read ? row.type : 0xffffffffu;
    return read;
}

// the first row of `type`, the rows' count where none is
static unsigned int row_of(unsigned int type)
{
    unsigned int at = 0u;
    unsigned int each = 0u;
    while (row_type(at, &each) && (each != type))
    {
        at += 1u;
    }
    return at;
}

// a report stored as the store's sink stores a run's event from the run's thread
static void reported(unsigned int type, unsigned long long run, const char *job, unsigned int stream, const char *text)
{
    EMBED_CALL(cli_job_runs_report.store, CliJobRunsReportCfg, .type = type, .run = run, .job = job, .stream = stream,
               .text = text, .has_code = type == CLI_JOB_RUNS_ENDED);
}

// whether any pixel of `rect` differs from `ground`
static int rect_touched(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            // the rectangle stands inside the raster
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
            {
                return 1;
            }
        }
    }
    return 0;
}

// whether a pixel of `rect` is red before all else: its red channel well over its green and blue
static int rect_reddened(const GuiRaster *raster, GuiRect rect)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            // the rectangle stands inside the raster
            const GuiColor pixel = raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
            const unsigned int red = (pixel >> 16u) & 0xffu;
            const unsigned int green = (pixel >> 8u) & 0xffu;
            const unsigned int blue = pixel & 0xffu;
            if ((red > green + 60u) && (red > blue + 60u))
            {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = RUN_HEIGHT, .away = 0);
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const GuiColor ground = 0xFF102030u;
    const GuiPaneTable *const table = &gui_run_table;
    EMBED_CALL(cli_job_runs_woken.begin, CliJobRunsWokenCfg, .woken = NULL);

    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "run") == table, "the Run tool has the table");
    const GuiRect region = EMBED_CALL(gui_pane.region, GuiPaneCfg, .table = table, .frame = &frame);
    cli_check(table->foot && (region.y == frame.terminal.y) && (region.height == frame.terminal.height),
              "the Run pane stands in the panel at the work's foot");

    // no tree: no rows, the head drawn under the title
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_SHOWN, .root = NULL);
    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    cli_check(EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u) == 0u, "with no tree no row stands");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1440, 920},
               .color = ground);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Run", .tree = "");
    const int rows_top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = table, .frame = &frame);
    cli_check(rect_touched(&raster, (GuiRect){region.x, region.y, region.width, rows_top - region.y}, ground),
              "the head is drawn under the title in the panel");
    cli_check(!rect_touched(&raster, frame.editor, ground), "nothing is drawn in the editor");

    // the check's catalog: a group's row pressed opens it, a job's chooses it
    catalog_laid();
    EMBED_CALL(cli_job_runs_root.read, CliJobRunsRootCfg, .root = NULL, .catalog = &s_catalog);
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_VIEW, .root = NULL);
    cli_check(EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u) == 2u,
              "the two groups are listed and the view's change reads nothing");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 0u, .pane = &pane, .open_out = &open) &&
                  (EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u) == 3u),
              "a group's row pressed opens it and opens no file");
    (void)EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 1u, .pane = &pane, .open_out = &open);
    unsigned int type = 0u;
    cli_check(row_type(2u, &type) && (type == CLI_JOB_RUN_ABOUT) && row_type(3u, &type) && (type == CLI_JOB_RUN_PARAM),
              "a job pressed is chosen, its file and its param under it");

    // a param pressed takes the keys, and the text and the keys reach its value
    cli_check(!pane.keys, "the pane has no keys yet");
    (void)EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 3u, .pane = &pane, .open_out = &open);
    cli_check(pane.keys, "a param's field pressed takes the keys");
    cli_check(EMBED_CALL(gui_pane.text, GuiPaneCfg, .table = table, .pane = &pane, .text = "ab", .length = 2ULL),
              "the text typed is taken");
    CliJobRunRow param;
    cli_check(EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = 3u, .row_out = &param) &&
                  (param.second_length == 2ULL) && (memcmp(param.second, "ab", 2u) == 0),
              "the text typed is the param's value");
    cli_check(EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Backspace") &&
                  EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = 3u, .row_out = &param) &&
                  (param.second_length == 1ULL),
              "Backspace erases the value's last letter");
    (void)EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Backspace");
    cli_check(EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Escape") && !pane.keys,
              "Escape lets the keys go");

    // the head: the field takes the keys for the filter; Start refused; Stop with nothing going
    const GuiRect head = {region.x, rows_top - EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg,
                                                          .pane = &(GuiPaneCfg){.table = table, .frame = &frame},
                                                          .x = GUI_PANE_BOX + GUI_PANE_CHOICE + GUI_RUN_REFUSAL),
                          region.width, 0};
    const int scaled_box = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg,
                                      .pane = &(GuiPaneCfg){.table = table, .frame = &frame}, .x = GUI_PANE_BOX);
    const int scaled_choice = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg,
                                         .pane = &(GuiPaneCfg){.table = table, .frame = &frame}, .x = GUI_PANE_CHOICE);
    const unsigned int field_action = EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane,
                                                 .frame = &frame, .x = region.x + 40, .y = head.y + (scaled_box / 2));
    const unsigned int start_action =
        EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                   .x = region.x + 40, .y = head.y + scaled_box + (scaled_choice / 2));
    const unsigned int stop_action =
        EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                   .x = region.x + region.width - 40, .y = head.y + scaled_box + (scaled_choice / 2));
    cli_check((field_action == GUI_RUN_FIELD) && (start_action == GUI_RUN_START) && (stop_action == GUI_RUN_STOP),
              "the head's field, Start and Stop are found under the pointer");
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = GUI_RUN_FIELD, .root = NULL);
    cli_check(pane.keys && EMBED_CALL(cli_job_runs_field.focused, CliJobRunsFieldCfg, .length_out = NULL),
              "the field takes the keys for the filter");
    (void)EMBED_CALL(gui_pane.text, GuiPaneCfg, .table = table, .pane = &pane, .text = "gamma", .length = 5ULL);
    cli_check((EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u) == 1u) && row_type(0u, &type) &&
                  (type == CLI_JOB_RUN_GROUP),
              "the filter typed lists the groups whose jobs contain it alone");
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        (void)EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Backspace");
    }
    (void)EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Escape");
    const unsigned int rows_before = EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u);
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = GUI_RUN_START, .root = ".");
    unsigned long long refused = 0ULL;
    (void)EMBED_CALL(cli_job_runs_run.refusal, CliJobRunsRunCfg, .length_out = &refused);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    cli_check((refused != 0ULL) && (row_of(CLI_JOB_RUN_RUN) == rows_before),
              "Start with the required value missing is refused and no run stands");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1440, 920},
               .color = ground);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Run", .tree = "");
    const int scaled_refusal = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg,
                                          .pane = &(GuiPaneCfg){.table = table, .frame = &frame}, .x = GUI_RUN_REFUSAL);
    cli_check(rect_reddened(&raster, (GuiRect){region.x, rows_top - scaled_refusal, region.width, scaled_refusal}),
              "the refusal is drawn under the buttons in the error color");
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = GUI_RUN_STOP, .root = ".");
    cli_check(!EMBED_CALL(cli_job_runs_run.live, CliJobRunsRunCfg, .root = ".") &&
                  (EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u) == rows_before),
              "Stop with no run going does nothing");

    // a run's report: its lines drawn in their spans' colors, the escape bytes gone, its end after them
    reported(CLI_JOB_RUNS_STARTED, 77ULL, "build/alpha", CLI_JOB_RUN_STDOUT, NULL);
    reported(CLI_JOB_RUNS_LINE, 77ULL, NULL, CLI_JOB_RUN_STDOUT, "\x1b[31mREDREDRED\x1b[0m");
    cli_check(EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL), "the step takes the report in");
    const unsigned int run_row = row_of(CLI_JOB_RUN_RUN);
    const unsigned int line_row = row_of(CLI_JOB_RUN_LINE);
    CliJobRunRow line;
    cli_check((line_row == run_row + 1u) &&
                  EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = line_row, .row_out = &line) &&
                  (line.text_length == 9ULL) && (memchr(line.text, 0x1b, line.text_length) == NULL),
              "the line stands under its run without its escape bytes");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1440, 920},
               .color = ground);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Run", .tree = "");
    const GuiRect line_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = line_row);
    cli_check(rect_reddened(&raster, line_rect), "the line's span is drawn in the red of the sixteen");
    reported(CLI_JOB_RUNS_ENDED, 77ULL, NULL, CLI_JOB_RUN_STDOUT, NULL);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    cli_check(row_of(CLI_JOB_RUN_END) == line_row + 1u, "the end stands after the run's last line");

    // the pane at its last row follows the lines as they arrive; the wheel stops at the last row
    reported(CLI_JOB_RUNS_STARTED, 78ULL, "build/alpha", CLI_JOB_RUN_STDOUT, NULL);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    const int row_height = EMBED_CALL(gui_pane.row_height, GuiPaneCfg, .table = table, .frame = &frame);
    // the panel's rows are a few, a positive count
    const unsigned int displayed = (unsigned int)((region.y + region.height - rows_top) / row_height);
    const unsigned int standing = EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u);
    pane.scroll = (standing > displayed) ? standing - displayed : 0u;
    for (unsigned int at = 0u; at < 40u; at += 1u)
    {
        reported(CLI_JOB_RUNS_LINE, 78ULL, NULL, CLI_JOB_RUN_STDOUT, "a line");
    }
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    const unsigned int count = EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u);
    EMBED_CALL(gui_run_pane.follow, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame);
    cli_check(pane.scroll == count - displayed, "the pane follows its last row as the lines arrive");
    pane.scroll = 0u;
    cli_check(EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .rows = 1000) &&
                  (pane.scroll == count - displayed),
              "the wheel scrolls no further than the last row fills the panel");
    pane.scroll = 0u;
    reported(CLI_JOB_RUNS_LINE, 78ULL, NULL, CLI_JOB_RUN_STDOUT, "one more");
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    EMBED_CALL(gui_run_pane.follow, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame);
    cli_check(pane.scroll == 0u, "a pane scrolled above its last row stands where it is as a line arrives");

    EMBED_CALL(cli_job_runs_woken.end, CliJobRunsWokenCfg, .woken = NULL);
    cli_check(mmgr_owner_in_use(g_cli_job_runs_owner) == 0u, "the store lets every cell go as it ends");
    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
