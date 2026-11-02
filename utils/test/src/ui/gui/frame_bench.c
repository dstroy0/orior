// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_bench.c: the frame bench, run by `harness.py window bench`: the window begun without a system, sized to a
// monitor at the scale of one with the explorer's side shown and a text of many lines in the editor, its frame drawn
// many times and timed, then drawn again with no text, the text's own cost the difference; each part of the frame
// drawn alone and timed, the base through the map and the map laid, the ground's copy and the editor's ground each alone, the editor's text, the explorer, the tabs, the map,
// the panes' contents and corners and the work's outline; and a blend of the whole raster at half alpha, timed alone, the one blend
// every drawing goes through
#include "../../../../../ui/gui/src/parts/chrome.h"
#include "../../../../../ui/gui/src/parts/glass.h"
#include "../../../../../ui/gui/src/window.h"

#include <stdio.h>

#define BENCH_WIDTH 1878u
#define BENCH_HEIGHT 1392u
#define BENCH_FRAMES 100u
#define BENCH_FILLS 50u
#define BENCH_LINES 120u

static void woken(void *context)
{
    (void)context;
}

typedef struct
{
    GuiWindow *window;
} BenchPartCfg;

// a part's drawing, run `BENCH_FRAMES` times on `window` and its milliseconds a run printed under `name`
typedef void (*BenchPart)(const BenchPartCfg *args);

static void bench_timed(const char *name, BenchPart part, GuiWindow *window)
{
    const unsigned long long began = cli_clock_microseconds();
    for (unsigned int run = 0u; run < BENCH_FRAMES; run += 1u)
    {
        EMBED_CALL(part, BenchPartCfg, .window = window);
    }
    const unsigned long long took = cli_clock_microseconds() - began;
    printf("  %s: %.3f ms\n", name, ((double)took / 1000.0) / (double)BENCH_FRAMES);
}

static void part_frame(GuiWindow *window)
{
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = window);
}

// the table the window's panes lie on
static GuiUnder bench_table(const GuiWindow *window)
{
    return EMBED_CALL(gui_glass.table, GuiGlassCfg, .ground = &window->ground.raster, .color = window->palette->ground, .glass = window->palette->glass);
}

static void part_base(const BenchPartCfg *args)
{
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &args->window->base, .raster = args->window->raster, .ground = &args->window->ground.raster, .color = args->window->palette->ground);
}

static void part_map_laid(const BenchPartCfg *args)
{
    args->window->base.laid = 0;
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &args->window->base, .frame = &args->window->frame, .palette = args->window->palette, .controls_steady = 0);
}

static void part_ground_copied(const BenchPartCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->window->raster->pixels,
                            .src = args->window->ground.raster.pixels,
                            .bytes = (unsigned long long)args->window->raster->width * args->window->raster->height * sizeof(GuiColor)});
}

static void part_editor_ground(const BenchPartCfg *args)
{
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->window->raster, .rect = args->window->frame.editor, .color = args->window->palette->editor_ground);
}

static void part_pane_corners(const BenchPartCfg *args)
{
    const GuiUnder table = bench_table(args->window);
    const GuiColor pane = EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = args->window->palette->glass);
    const GuiFrame *const frame = &args->window->frame;
    EMBED_CALL(gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = args->window->raster, .ground = table, .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->bar_slat, .other = frame->bar), .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
    EMBED_CALL(gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = args->window->raster, .ground = table, .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->strip_slat, .other = frame->strip), .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
    EMBED_CALL(gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = args->window->raster, .ground = table, .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->status_slat, .other = frame->status), .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
}

static void part_outline(const BenchPartCfg *args)
{
    const GuiPalette *const palette = args->window->palette;
    const GuiFrame *const frame = &args->window->frame;
    const GuiColor right_line = GUI_RGB((GUI_RED_OF(palette->link) * 65u) / 100u, (GUI_GREEN_OF(palette->link) * 65u) / 100u,
                                        (GUI_BLUE_OF(palette->link) * 65u) / 100u);
    const GuiOutline lines = {palette->signal, right_line, right_line, (palette->signal & 0x00ffffffu) | (0x59u << 24u), 2, 2, 1, 1};
    const GuiUnder clear = {&args->window->ground.raster, 0u, NULL, 0u, palette->ground};
    GuiRect holes[4];
    const unsigned int count = EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = frame, .holes = holes);
    EMBED_CALL(gui_raster_radius_work.outline, GuiRasterRadiusWorkCfg, .raster = args->window->raster, .work = frame->work, .inside = frame->inside, .radius = frame->radius, .lines = &lines, .holes = holes, .count = count, .table = bench_table(args->window), .clear = clear);
}

static void part_editor(const BenchPartCfg *args)
{
    EMBED_CALL(gui_editor_place_scale.draw, GuiEditorPlaceScaleCfg, .editor = &args->window->editor, .raster = args->window->raster, .glyphs = &args->window->glyphs, .palette = args->window->palette, .place = args->window->frame.editor, .scale = args->window->frame.scale);
}

static void part_explorer(const BenchPartCfg *args)
{
    EMBED_CALL(gui_explorer_frame_xy.draw, GuiExplorerFrameXYCfg, .explorer_read = &args->window->explorer, .raster = args->window->raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = args->window->palette, .frame = &args->window->frame, .title = EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = 0u)->label, .folders = 1, .tree = args->window->title, .shown = args->window->path);
}

static void part_tabs(const BenchPartCfg *args)
{
    EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .raster = args->window->raster, .glyphs = &args->window->glyphs, .palette = args->window->palette, .frame = &args->window->frame, .tab = args->window->tab, .count = args->window->tabs, .active = args->window->active);
}

static void part_map(const BenchPartCfg *args)
{
    EMBED_CALL(gui_minimap_place.draw, GuiMinimapPlaceCfg, .map = &args->window->minimap, .raster = args->window->raster, .palette = args->window->palette, .editor = &args->window->editor, .place = args->window->frame.map, .scale = args->window->frame.scale, .on_screen = 60.0f);
}

static void part_glass(const BenchPartCfg *args)
{
    EMBED_CALL(gui_strip_frame_raster.draw, GuiStripFrameRasterCfg, .raster = args->window->raster, .icons = &args->window->icons, .palette = args->window->palette, .frame = &args->window->frame, .strip_read = &args->window->strip);
    EMBED_CALL(gui_chrome_raster.bar_draw, GuiChromeRasterCfg, .raster = args->window->raster, .glyphs = &args->window->glyphs, .palette = args->window->palette, .frame = &args->window->frame, .title = args->window->title, .table = bench_table(args->window), .shadow = &args->window->controls_shadow);
    const GuiStatus status = {"ui/gui/src/raster.c", 1, 3ull, 4ull, 3ull, 4ull, 0, 0, 4u, 100000000ull};
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = args->window->raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = args->window->palette, .frame = &args->window->frame, .status = &status);
}

static void part_strip(const BenchPartCfg *args)
{
    EMBED_CALL(gui_strip_frame_raster.draw, GuiStripFrameRasterCfg, .raster = args->window->raster, .icons = &args->window->icons, .palette = args->window->palette, .frame = &args->window->frame, .strip_read = &args->window->strip);
}

static void part_bar(const BenchPartCfg *args)
{
    EMBED_CALL(gui_chrome_raster.bar_draw, GuiChromeRasterCfg, .raster = args->window->raster, .glyphs = &args->window->glyphs, .palette = args->window->palette, .frame = &args->window->frame, .title = args->window->title, .table = bench_table(args->window), .shadow = &args->window->controls_shadow);
}

// the name as it stands or goes at the moment the bench has set
static unsigned long long s_name_ms;

static void part_name(const BenchPartCfg *args)
{
    g_test_clock_ms = s_name_ms;
    EMBED_CALL(gui_wordmark_frame.draw, GuiWordmarkFrameCfg, .wordmark = &args->window->wordmark, .raster = args->window->raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = args->window->palette, .frame = &args->window->frame);
}

// the frames of a slide run over and over from `s_slide_from`, each laid and drawn as the window's tick lays and draws it
static unsigned long long s_slide_from;
static unsigned long long s_busy_ms;
static unsigned int s_slide_frame;

static void part_sliding(const BenchPartCfg *args)
{
    GuiWindow *window = args->window;
    s_slide_frame = (s_slide_frame + 1u) % 18u;
    EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = window, .time_ms = s_slide_from + (16ull * s_slide_frame));
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = window);
}

static void part_busy_step(const BenchPartCfg *args)
{
    s_busy_ms += GUI_BUSY_STAGE_MS;
    g_test_clock_ms = s_busy_ms;
    (void)EMBED_CALL(gui_busy_frame.redrawn, GuiBusyFrameCfg, .busy = &args->window->busy, .raster = args->window->raster);
}

static void part_status(const BenchPartCfg *args)
{
    const GuiStatus status = {"ui/gui/src/raster.c", 1, 3ull, 4ull, 3ull, 4ull, 0, 0, 4u, 100000000ull};
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = args->window->raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = args->window->palette, .frame = &args->window->frame, .status = &status);
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    static GuiWindow window;
    if (!EMBED_CALL(gui_window_woken.begin, GuiWindowWokenCfg, .window = &window, .woken = woken, .context = NULL, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        printf("the window did not begin: %s\n", why_text);
        return 1;
    }
    window.strip.shown = 0;
    if (!EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = &window, .width = BENCH_WIDTH, .height = BENCH_HEIGHT, .system_scale = 100))
    {
        printf("the window was not sized\n");
        return 1;
    }
    static char text[BENCH_LINES * 128u];
    VerbaLinea lines = {text, sizeof(text), 0ull};
    for (unsigned int line = 0u; line < BENCH_LINES; line += 1u)
    {
        mmgr_line_put(&lines, "    const unsigned int value = (color * share) + (ground * rest) + 127u; // a line of code\n");
    }
    (void)EMBED_CALL(gui_window_text_table.text_put, GuiWindowTextCfg, .window = &window, .text = text, .length = mmgr_line_ok(&lines) ? lines.at : 0ull);
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    const unsigned long long frames_began = cli_clock_microseconds();
    for (unsigned int frame = 0u; frame < BENCH_FRAMES; frame += 1u)
    {
        EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    }
    const unsigned long long frames_took = cli_clock_microseconds() - frames_began;
    printf("frame: %.3f ms over %u frames at %u by %u\n", ((double)frames_took / 1000.0) / (double)BENCH_FRAMES, BENCH_FRAMES, BENCH_WIDTH, BENCH_HEIGHT);
    printf("the frame's parts, each drawn alone:\n");
    bench_timed("the base through the map", part_base, &window);
    bench_timed("the map laid", part_map_laid, &window);
    bench_timed("  the ground copied alone", part_ground_copied, &window);
    bench_timed("  the editor's ground alone", part_editor_ground, &window);
    bench_timed("the editor's text", part_editor, &window);
    bench_timed("the explorer", part_explorer, &window);
    bench_timed("the tabs", part_tabs, &window);
    bench_timed("the map", part_map, &window);
    bench_timed("the panes' contents", part_glass, &window);
    bench_timed("  the strip", part_strip, &window);
    bench_timed("  the bar", part_bar, &window);
    bench_timed("  the status bar", part_status, &window);
    bench_timed("the panes' corners", part_pane_corners, &window);
    bench_timed("the work's outline", part_outline, &window);
    // the busy circle: its stages made once as it comes, and a stage's step, all a frame of it owes
    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_busy_ticket.request, GuiBusyTicketCfg, .busy = &window.busy, .ticket = window.link.tickets + 1ull);
    g_test_clock_ms = 1000ull + GUI_BUSY_SHOW_DELAY_MS;
    (void)EMBED_CALL(gui_busy_ticket.tick, GuiBusyTicketCfg, .busy = &window.busy);
    s_busy_ms = window.busy.visible_start_ms;
    const unsigned long long busy_made = cli_clock_microseconds();
    g_test_clock_ms = s_busy_ms;
    EMBED_CALL(gui_busy_frame.draw, GuiBusyFrameCfg, .busy = &window.busy, .raster = window.raster, .palette = window.palette, .frame = &window.frame);
    printf("  the busy circle's stages made, once: %.3f ms for %u stages in %llu bytes\n", (double)(cli_clock_microseconds() - busy_made) / 1000.0, window.busy.stages,
           window.busy.cell_bytes);
    bench_timed("a step of the busy circle", part_busy_step, &window);
    EMBED_CALL(cli_busy_still.free, GuiBusyStillCfg, .busy = &window.busy);
    EMBED_CALL(cli_busy_still.init, GuiBusyStillCfg, .busy = &window.busy);
    // the claim coming on, and the bar coming in over the claim with its room coming after it
    s_slide_from = 100000ull;
    g_test_clock_ms = s_slide_from;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &window.claim, .on = 1);
    const unsigned long long made_on = cli_clock_microseconds();
    EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = &window, .time_ms = s_slide_from);
    printf("  the claim's stages made, once: %.3f ms for %u stages\n", (double)(cli_clock_microseconds() - made_on) / 1000.0, window.stages.count);
    bench_timed("a frame of the claim coming on", part_sliding, &window);
    s_slide_from = 200000ull;
    g_test_clock_ms = s_slide_from - GUI_CLAIM_SHOW_DELAY_MS;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &window.claim, .x = 900, .y = 5, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    g_test_clock_ms = s_slide_from;
    (void)EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &window.claim);
    const unsigned long long made_in = cli_clock_microseconds();
    EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = &window, .time_ms = s_slide_from);
    const GuiStage *const last_stage = &window.stages.stage[window.stages.count - 1u];
    printf("  the bar's stages made, once: %.3f ms for %u stages in %llu bytes\n", (double)(cli_clock_microseconds() - made_in) / 1000.0, window.stages.count,
           (unsigned long long)((const char *)(last_stage->title + last_stage->titles) - (const char *)window.stages.stage));
    bench_timed("a frame of the bar coming in", part_sliding, &window);
    EMBED_CALL(cli_claim_still.init, GuiClaimStillCfg, .claim = &window.claim);
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &window.frame, .width = BENCH_WIDTH, .height = BENCH_HEIGHT, .scale = 100);
    EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = &window, .time_ms = 300000ull);
    // the name on its pane whole, and up its funnel three quarters of the way through the flight
    const unsigned long long stood = 1000ull;
    g_test_clock_ms = stood;
    EMBED_CALL(gui_wordmark_frame.stand, GuiWordmarkFrameCfg, .wordmark = &window.wordmark);
    s_name_ms = stood + 800ull;
    // the pictures are made by the first drawing, once a start: timed on their own and left out of the row
    const unsigned long long made_began = cli_clock_microseconds();
    EMBED_CALL(part_name, BenchPartCfg, .window = &window);
    printf("  the name's pictures made, once: %.3f ms\n", (double)(cli_clock_microseconds() - made_began) / 1000.0);
    bench_timed("the name standing", part_name, &window);
    EMBED_CALL(cli_wordmark_still.tree_read, GuiWordmarkStillCfg, .wordmark = &window.wordmark);
    const unsigned long long went = stood + GUI_WORDMARK_MIN_VISIBLE_MS;
    const unsigned long long flight_on = cli_clock_microseconds();
    g_test_clock_ms = went;
    (void)EMBED_CALL(gui_wordmark_frame.tick, GuiWordmarkFrameCfg, .wordmark = &window.wordmark, .glyphs = &window.glyphs, .icons = &window.icons, .palette = window.palette, .frame = &window.frame);
    const unsigned int flight_stages = (unsigned int)(GUI_WORDMARK_FLIGHT_MS / GUI_WORDMARK_FRAME_MS) + 1u;
    const unsigned int flight_spans = window.wordmark.flight_first[flight_stages];
    const GuiWordmarkSpan *const flight_last = &window.wordmark.flight_span[flight_spans - 1u];
    printf("  the flight's stages made, once: %.3f ms for %u stages, %u spans, %u bytes of coverage\n", (double)(cli_clock_microseconds() - flight_on) / 1000.0,
           flight_stages, flight_spans, flight_last->cover + flight_last->count);
    s_name_ms = went + ((GUI_WORDMARK_FLIGHT_MS * 3ull) / 4ull);
    bench_timed("the name up the funnel", part_name, &window);
    EMBED_CALL(cli_wordmark_still.free, GuiWordmarkStillCfg, .wordmark = &window.wordmark);
    EMBED_CALL(cli_wordmark_still.init, GuiWordmarkStillCfg, .wordmark = &window.wordmark);
    // the same frame with the editor holding no lines: the text's own cost is the difference
    EMBED_CALL(gui_editor_json_names.free, GuiEditorJsonNamesCfg, .editor = &window.editor);
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &window.editor);
    const unsigned long long bare_began = cli_clock_microseconds();
    for (unsigned int frame = 0u; frame < BENCH_FRAMES; frame += 1u)
    {
        part_frame(&window);
    }
    const unsigned long long bare_took = cli_clock_microseconds() - bare_began;
    printf("frame with no text: %.3f ms, the text %.3f ms of the frame\n", ((double)bare_took / 1000.0) / (double)BENCH_FRAMES,
           (((double)frames_took - (double)bare_took) / 1000.0) / (double)BENCH_FRAMES);
    const GuiRect whole = {0, 0, (int)BENCH_WIDTH, (int)BENCH_HEIGHT};
    const unsigned long long fills_began = cli_clock_microseconds();
    for (unsigned int fill = 0u; fill < BENCH_FILLS; fill += 1u)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = window.raster, .rect = whole, .color = GUI_RGBA(0x80u, 0x40u, 0xc0u, 0x80u));
    }
    const unsigned long long fills_took = cli_clock_microseconds() - fills_began;
    printf("blend of the whole raster: %.3f ms over %u fills\n", ((double)fills_took / 1000.0) / (double)BENCH_FILLS, BENCH_FILLS);
    EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &window);
    return 0;
}
