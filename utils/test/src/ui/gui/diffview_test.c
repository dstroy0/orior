// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diffview_test.c: the compare over the editor on a repository of the check's own: a file changed in two places
// opens against the last commit with the CLI's label, sides and where sentences, its first change in sight; a changed
// row is tinted on both halves, a line put in has a gap on the left and a tinted right half, a row both share is
// tinted on neither; the wheel scrolls no further than the CLI's rows fill the body; the next and previous tools and
// F7 and Shift+F7 step through the changes as the CLI answers them, wrapping, the where sentence following; the white
// space box sets white space aside through the CLI and the rows are read again, while the structure box, which the CLI
// answers cannot be pressed, turns nothing; a file that does not differ draws the CLI's sentence and no row; and Escape
// closes the view through the host. Every sentence is read from the CLI
#include "../../../../../ui/cli/src/code/diff_view.h"
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/view/parts/diffview.h"
#include "../cli/cli_check.h"

// the lines of the check's file, and the bytes they take with their ends at most
#define DIFFVIEW_LINES 40u
#define DIFFVIEW_TEXT 512u

static char s_root[CLI_PATH_MAX_LEN];

// git run at the root with `request`, `count` words: 1, or 0 with why as the check's failure
static int git_ran(const char *const *request, unsigned int count, const char *why_not)
{
    char out_text[4096];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea out = {out_text, sizeof(out_text), 0ULL};
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int ran = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root,
                               .out = &out, .count = count, .err_msg = &err_msg, .request = request);
    cli_check(ran, why_not);
    return ran;
}

// the file `relative` of the tree written with `size` bytes of `text`
static void file_written(const char *relative, const char *text, unsigned long long size)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = relative, .joined = path);
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                         .size = size),
              relative);
}

// the check's file's text into `text`: a line `line N` each, the second written `second` and `added` put after the
// last where not NULL: its bytes
static unsigned long long lines_written(char *text, const char *second, const char *added)
{
    VerbaLinea line = {text, DIFFVIEW_TEXT, 0ULL};
    for (unsigned int at = 0u; at < DIFFVIEW_LINES; at += 1u)
    {
        if ((at == 1u) && (second != NULL))
        {
            mmgr_line_put(&line, second);
        }
        else
        {
            mmgr_line_put(&line, "line ");
            mmgr_line_decimal(&line, at, 1u);
        }
        mmgr_line_ch(&line, '\n');
    }
    if (added != NULL)
    {
        mmgr_line_put(&line, added);
        mmgr_line_ch(&line, '\n');
    }
    return line.at;
}

// whether the CLI's sentence read by `read` with its bytes is `text`
static int sentence_is(const char *read, unsigned long long length, const char *text)
{
    return (length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = 256ULL})) &&
           ((length == 0ULL) || (memor.cmp(&(MemoriaCfg){.src = read, .other = text, .bytes = length}) == 0));
}

static int where_is(const char *text)
{
    unsigned long long length = 0ULL;
    const char *const read =
        EMBED_CALL(cli_diff_view_at_row_length.where, CliDiffViewAtRowLengthCfg, .length_out = &length);
    return sentence_is(read, length, text);
}

// the pixel at x, y of the raster
static GuiColor pixel(const GuiRaster *raster, int x, int y)
{
    // the pixel stands inside the raster
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
}

// the raster laid with `ground` and the view drawn over it
static void drawn(GuiRaster *raster, const GuiWork *work, const GuiFrame *frame, GuiGlyphs *glyphs, GuiIcons *icons,
                  const GuiPalette *palette, GuiColor ground)
{
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){0, 0, 1440, 920}, .color = ground);
    EMBED_CALL(gui_work.draw, GuiWorkCfg, .work_read = work, .frame = frame, .raster = raster, .glyphs = glyphs,
               .icons = icons, .palette = palette);
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
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
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const GuiColor ground = 0xFF102030u;

    // the repository: the file committed, then its second line indented and a line put after its last
    cli_check_folder("diffview_tree", s_root);
    char out_text[1024];
    VerbaLinea out = {out_text, sizeof(out_text), 0ULL};
    cli_check(EMBED_CALL(cli_git_out.init, CliGitOutCfg, .folder_read = s_root, .out = &out, .err_msg = &why),
              "a repository stands in the check's folder");
    const char *const name_set[3] = {"config", "user.name", "Check"};
    const char *const email_set[3] = {"config", "user.email", "check@orior.test"};
    const char *const added[3] = {"add", "-A", "."};
    const char *const committed[6] = {"-c", "commit.gpgsign=false", "commit", "-q", "-m", "first"};
    char text[DIFFVIEW_TEXT];
    file_written("a.txt", text, lines_written(text, NULL, NULL));
    file_written("same.txt", "one\n", 4ULL);
    (void)git_ran(name_set, 3u, "the repository has a committer's name");
    (void)git_ran(email_set, 3u, "and address");
    (void)git_ran(added, 3u, "the files are added");
    (void)git_ran(committed, 6u, "and committed");
    file_written("a.txt", text, lines_written(text, "    line 1", "put in"));

    char open_why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea open_err = {open_why, sizeof(open_why), 0ULL};
    cli_check(EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = s_root,
                         .source = CLI_DIFF_VIEW_HEAD, .path = "a.txt", .err_msg = &open_err),
              "the file opens against the last commit");
    GuiWork work;
    EMBED_CALL(gui_work.init, GuiWorkCfg, .work = &work);
    EMBED_CALL(gui_work.open, GuiWorkCfg, .work = &work, .table = &gui_diffview_table, .frame = &frame,
               .glyphs = &glyphs);
    const unsigned int rows = EMBED_CALL(cli_diff_view_at_row_length.rows, CliDiffViewAtRowLengthCfg, .at = 0u);
    cli_check((rows == DIFFVIEW_LINES + 2u) && (work.scroll == 0u) && where_is("change 1 of 2"),
              "the view opens on the CLI's rows, its first change in sight, the where sentence the CLI's");
    unsigned long long length = 0ULL;
    const char *const label =
        EMBED_CALL(cli_diff_view_at_row_length.bar, CliDiffViewAtRowLengthCfg, .part = CLI_DIFF_VIEW_BAR_LABEL,
                   .length_out = &length);
    cli_check(sentence_is(label, length, "a.txt"), "the bar's label is the CLI's");
    const char *const sides =
        EMBED_CALL(cli_diff_view_at_row_length.bar, CliDiffViewAtRowLengthCfg, .part = CLI_DIFF_VIEW_BAR_SIDES,
                   .length_out = &length);
    cli_check(sentence_is(sides, length, "last commit, then as it stands"), "the sides sentence is the CLI's");

    // the rows as they are drawn: the bar over the body, a row each 20 pixels at the frame's scale
    drawn(&raster, &work, &frame, &glyphs, &icons, palette, ground);
    const int body_y = frame.editor.y + GUI_DIFFVIEW_BAR;
    const int left_end = frame.editor.x + (frame.editor.width / 2) - 4;
    const int right_end = frame.editor.x + frame.editor.width - 4;
    const int changed_y = body_y + (1 * GUI_DIFFVIEW_ROW) + (GUI_DIFFVIEW_ROW / 2);
    const int same_y = body_y + (2 * GUI_DIFFVIEW_ROW) + (GUI_DIFFVIEW_ROW / 2);
    cli_check((pixel(&raster, left_end, changed_y) != ground) && (pixel(&raster, right_end, changed_y) != ground),
              "a changed row is tinted on both halves");
    cli_check((pixel(&raster, left_end, same_y) == ground) && (pixel(&raster, right_end, same_y) == ground),
              "a row both share is tinted on neither");
    cli_check(pixel(&raster, frame.editor.x + 2, body_y - 6) != ground || pixel(&raster, frame.editor.x + 9, body_y - 15) == ground,
              "the bar is drawn over the body");

    // the wheel: no further than the last row fills the body
    const int shown = (frame.editor.y + frame.editor.height - body_y) / GUI_DIFFVIEW_ROW;
    const unsigned int most = rows - (unsigned int)shown;
    cli_check(EMBED_CALL(gui_work.wheel, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs,
                         .x = frame.editor.x + 40, .y = body_y + 40, .rows = 1000) &&
                  (work.scroll == most),
              "the wheel scrolls no further than the CLI's last row fills the body");
    drawn(&raster, &work, &frame, &glyphs, &icons, palette, ground);
    const int added_y = body_y + (int)((rows - 2u - most) * GUI_DIFFVIEW_ROW) + (GUI_DIFFVIEW_ROW / 2);
    cli_check((pixel(&raster, left_end, added_y) != ground) && (pixel(&raster, right_end, added_y) != ground) &&
                  (pixel(&raster, left_end, added_y) != pixel(&raster, right_end, added_y)),
              "a line put in has a gap on the left and a tinted right half, drawn in the rows scrolled to");
    cli_check(where_is("change 2 of 2"), "the where sentence follows the rows in view");

    // the tools and the keys step through the changes, wrapping
    const int tools_y = frame.editor.y + (GUI_DIFFVIEW_BAR / 2);
    const int right = frame.editor.x + frame.editor.width - GUI_DIFFVIEW_PAD;
    const unsigned int next = EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame,
                                         .x = right - (3 * GUI_DIFFVIEW_TOOL) + 4, .y = tools_y);
    const unsigned int previous = EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame,
                                             .x = right - (2 * GUI_DIFFVIEW_TOOL) + 4, .y = tools_y);
    const unsigned int close = EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame,
                                          .x = right - GUI_DIFFVIEW_TOOL + 4, .y = tools_y);
    cli_check((next == GUI_DIFFVIEW_NEXT) && (previous == GUI_DIFFVIEW_PREVIOUS) && (close == GUI_DIFFVIEW_CLOSE),
              "the tools stand at the bar's right");
    (void)EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs, .index = next);
    cli_check(where_is("change 1 of 2") && (work.scroll == 0u), "the next tool wraps to the first change");
    (void)EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs,
                     .index = previous);
    cli_check(where_is("change 2 of 2") && (work.scroll == most), "the previous tool wraps to the last change");
    (void)EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs, .name = "F7");
    cli_check(where_is("change 1 of 2"), "F7 steps to the next change");
    (void)EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs, .name = "Shift+F7");
    cli_check(where_is("change 2 of 2"), "Shift+F7 steps to the change before");
    cli_check(EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs,
                         .name = "Backspace") &&
                  EMBED_CALL(gui_work.text, GuiWorkCfg, .work = &work, .frame = &frame, .text = "x", .length = 1ULL),
              "a key that edits and text typed are taken, the editor under the view left as it is");

    // the boxes: white space set aside through the CLI, the rows read again; structure cannot be pressed
    const unsigned int space = EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame,
                                          .x = right - (3 * GUI_DIFFVIEW_TOOL) - GUI_DIFFVIEW_PAD -
                                               (2 * GUI_DIFFVIEW_BOX_WIDTH) + 4,
                                          .y = tools_y);
    const unsigned int structure = EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame,
                                              .x = right - (3 * GUI_DIFFVIEW_TOOL) - GUI_DIFFVIEW_PAD -
                                                   GUI_DIFFVIEW_BOX_WIDTH + 4,
                                              .y = tools_y);
    cli_check((space == GUI_DIFFVIEW_SPACE) && (structure == GUI_DIFFVIEW_STRUCTURE), "the boxes stand before the tools");
    (void)EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs, .index = space);
    int on = 0;
    int enabled = 0;
    EMBED_CALL(cli_diff_view_option_on.option_get, CliDiffViewOptionOnCfg, .option = CLI_DIFF_VIEW_SPACE,
               .on_out = &on, .enabled_out = &enabled);
    cli_check(on && enabled && where_is("change 1 of 1"),
              "the white space box sets white space aside, the indented line no change");
    (void)EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs,
                     .index = structure);
    EMBED_CALL(cli_diff_view_option_on.option_get, CliDiffViewOptionOnCfg, .option = CLI_DIFF_VIEW_STRUCTURE,
               .on_out = &on, .enabled_out = &enabled);
    cli_check(!on && !enabled, "the structure box, which the CLI answers cannot be pressed, turns nothing");
    (void)EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs, .index = space);
    cli_check(where_is("change 1 of 2"), "the white space box pressed again shows the indented line changed");

    // a file that does not differ: the CLI's sentence and no row
    open_err.at = 0ULL;
    cli_check(EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = s_root,
                         .source = CLI_DIFF_VIEW_HEAD, .path = "same.txt", .err_msg = &open_err),
              "a file that does not differ opens");
    EMBED_CALL(gui_work.open, GuiWorkCfg, .work = &work, .table = &gui_diffview_table, .frame = &frame,
               .glyphs = &glyphs);
    drawn(&raster, &work, &frame, &glyphs, &icons, palette, ground);
    const char *const empty =
        EMBED_CALL(cli_diff_view_at_row_length.empty, CliDiffViewAtRowLengthCfg, .length_out = &length);
    int sentence_drawn = 0;
    for (int x = frame.editor.x; x < frame.editor.x + 300; x += 1)
    {
        for (int y = body_y + GUI_DIFFVIEW_PAD; y < body_y + GUI_DIFFVIEW_PAD + GUI_DIFFVIEW_ROW; y += 1)
        {
            sentence_drawn = sentence_drawn || (pixel(&raster, x, y) != ground);
        }
    }
    cli_check(sentence_is(empty, length, "No change from the last commit.") && sentence_drawn &&
                  (pixel(&raster, right_end, body_y + (3 * GUI_DIFFVIEW_ROW)) == ground),
              "the CLI's sentence is drawn in place of rows");

    // Escape closes the view through the host
    cli_check(EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .glyphs = &glyphs,
                         .name = "Escape") &&
                  !EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &work) &&
                  (EMBED_CALL(cli_diff_view_at_row_length.rows, CliDiffViewAtRowLengthCfg, .at = 0u) == 0u),
              "Escape closes the view and the CLI's compare");
    cli_check(EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame, .x = right - 4,
                         .y = tools_y) == GUI_WORK_OUTSIDE,
              "the editor is the editor's again");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
