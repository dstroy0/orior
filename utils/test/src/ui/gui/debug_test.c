// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_test.c: the Debug pane's check through its table's hooks, against a session of the check's own whose requests
// answer fixed data: the Debug tool has a foot table located in the panel at the work's foot; with no session the
// rows are the notice and the pane draws in the panel and nowhere in the side; the steps hit where the module shows
// them, a step not displayed taking no box; a choice shows its view and the rows follow it; after a stop a frame in the
// tree opens at its line and one outside does not; an arrow opens a variable and a press beside it does not; the field
// takes the keys, text typed and Enter add a watch, and Escape lets the keys go; a step pressed reaches its request;
// and the console's lines scroll under the wheel
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/debug/debug_pane.h"
#include "../../../../../ui/gui/src/debug.h"
#include "../cli/cli_check.h"

#define CHECK_ROOT "/check/tree"
#define CHECK_HEIGHT 260

// what the check's adapter was last asked to step
static char s_how[32];

static void sessions_written(const CliDebugOutErrMsgIdStepCfg *args)
{
    mmgr_line_put(args->out, "[{\"id\":1,\"name\":\"prog.py\",\"tool\":\"debugpy\",\"language\":\"python\","
                             "\"step_back\":false}]");
}

static int threads_written(const CliDebugOutErrMsgIdStepCfg *args)
{
    mmgr_line_put(args->out, "[{\"id\":7,\"name\":\"main\"}]");
    return 1;
}

static int stack_written(const CliDebugOutErrMsgIdThreadCfg *args)
{
    mmgr_line_put(args->out, "[{\"id\":100,\"name\":\"inner\",\"path\":");
    args->named(&(CliDebugPathWrittenCfg){.context = args->context, .path = CHECK_ROOT "/src/prog.py", .out = args->out});
    mmgr_line_put(args->out, ",\"line\":11,\"col\":0,\"ip\":null},{\"id\":101,\"name\":\"outer\",\"path\":");
    args->named(&(CliDebugPathWrittenCfg){.context = args->context, .path = "/usr/lib/other.py", .out = args->out});
    mmgr_line_put(args->out, ",\"line\":3,\"col\":0,\"ip\":null}]");
    return 1;
}

static int scopes_written(const CliDebugOutErrMsgCfg *args)
{
    mmgr_line_put(args->out, "[{\"name\":\"Locals\",\"reference\":10,\"expensive\":false}]");
    return 1;
}

static int variables_written(const CliDebugOutCfg *args)
{
    if (args->reference == 10LL)
    {
        mmgr_line_put(args->out, "[{\"name\":\"items\",\"value\":\"[1, 2]\",\"type\":\"list\",\"reference\":30,"
                                 "\"memory\":null}]");
        return 1;
    }
    mmgr_line_put(args->out, "[{\"name\":\"0\",\"value\":\"1\",\"type\":\"int\",\"reference\":0,\"memory\":null},"
                             "{\"name\":\"1\",\"value\":\"2\",\"type\":\"int\",\"reference\":0,\"memory\":null}]");
    return 1;
}

static int evaluated(const CliDebugOutErrMsgCfg *args)
{
    (void)args->expression;
    mmgr_line_put(args->out, "{\"name\":\"count\",\"value\":\"3\",\"type\":\"int\",\"reference\":0,\"memory\":null}");
    return 1;
}

static int stepped(const CliDebugOutErrMsgIdThreadHowCfg *args)
{
    VerbaLinea how = {s_how, sizeof(s_how), 0ULL};
    mmgr_line_put(&how, args->how);
    (void)mmgr_line_finish(&how);
    return 1;
}

static int stepped_across(const CliDebugRootOutErrMsgCfg *args)
{
    (void)args;
    return 1;
}

static void stopped(const CliDebugCfg *args)
{
    (void)args;
}

static const CliDebugPaneRequestsNs s_requests = {
    .sessions = sessions_written,
    .threads = threads_written,
    .stack = stack_written,
    .scopes = scopes_written,
    .variables = variables_written,
    .evaluate = evaluated,
    .step = stepped,
    .step_across = stepped_across,
    .stop = stopped,
};

// the event `event` with `body` of the check's session passed to the pane's sink and read
static void event_read(const char *event, const char *body)
{
    const CliDebugSink sink = EMBED_CALL(cli_debug_pane_view.sink, CliDebugPaneViewCfg, .root = CHECK_ROOT);
    sink.emit(&(CliDebugEventCfg){.context = sink.context, .session = 1ULL, .event = event, .body = body,
                                  .size = cellul.len(&(CatenaFinitaCfg){.src = body, .cap = 1ULL << 16u})});
    (void)EMBED_CALL(cli_debug_pane_view.step, CliDebugPaneViewCfg, .root = CHECK_ROOT, .now_ms = 1ULL);
}

// whether any pixel of `rect` in the raster differs from `ground`
static int rect_touched(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            // the rectangle is inside the raster
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
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
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(gui_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .faces = &g_gui_faces, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = CHECK_HEIGHT, .away = 0);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    EMBED_CALL(cli_debug_pane_view.begin, CliDebugPaneViewCfg, .root = CHECK_ROOT, .requests = &s_requests);

    const GuiPaneTable *const table = &gui_debug_table;
    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "debug") == table, "the Debug tool has the table");
    cli_check(table->foot, "the Debug table is a foot table");
    const GuiRect region = EMBED_CALL(gui_pane.region, GuiPaneCfg, .table = table, .frame = &frame);
    cli_check((region.y == frame.terminal.y) && (region.height == frame.terminal.height) && (region.height > 0),
              "the Debug pane is in the panel at the work's foot");
    cli_check(table->rows(&(GuiPaneCfg){.table = table}) == 1u, "with no session Frames has its notice alone");

    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    const GuiColor ground = 0xFF102030u;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster,
               .rect = (GuiRect){0, 0, 1440, 920}, .color = ground);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Debug", .tree = "");
    cli_check(rect_touched(&raster, frame.terminal, ground), "the Debug pane draws inside the panel");
    cli_check(!rect_touched(&raster, frame.side, ground), "the Debug pane draws nothing in the side");

    // the head: the bar's row, the views' row and the field's row under the panel's title
    const int head_y = frame.terminal.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE;
    const int top_menu_y = head_y + (GUI_PANE_CHOICE / 2);
    const int views_y = head_y + GUI_PANE_CHOICE + (GUI_PANE_CHOICE / 2);
    const int field_y = head_y + GUI_PANE_CHOICE + GUI_PANE_CHOICE + (GUI_PANE_BOX / 2);
    const int step_x = frame.terminal.x + GUI_PANE_PAD_LEFT + (GUI_DEBUG_STEP / 2);
    const int pitch = GUI_DEBUG_STEP + GUI_DEBUG_STEP_GAP;
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = step_x, .y = top_menu_y) == GUI_DEBUG_STEP_FIRST + CLI_DEBUG_STEP_CONTINUE,
              "the bar's first box is Continue");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = step_x + (5 * pitch), .y = top_menu_y) ==
                  GUI_DEBUG_STEP_FIRST + CLI_DEBUG_STEP_RESTART,
              "the steps backward not displayed take no box, Restart located sixth");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.terminal.x + GUI_DEBUG_VIEW + (GUI_DEBUG_VIEW / 2),
                         .y = views_y) == GUI_DEBUG_VIEW_FIRST + CLI_DEBUG_VIEW_VARIABLES,
              "the second choice is Variables");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.terminal.x + 40, .y = field_y) == 0u,
              "Frames has no field to press");

    event_read("stopped", "{\"reason\":\"breakpoint\",\"threadId\":7}");
    cli_check(table->rows(&(GuiPaneCfg){.table = table}) == 2u, "a stop lists the stopped thread's two frames");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 0u, .root = CHECK_ROOT,
                         .open_out = &open) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = open.path, .other = "src/prog.py", .cap = 64ULL}) &&
                  open.placed && (open.place.line == 11ULL),
              "a frame in the tree opens its file at its line");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 1u, .root = CHECK_ROOT,
                          .open_out = &open),
              "a frame outside the tree opens nothing");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Debug", .tree = "");
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 0u, .root = CHECK_ROOT,
                         .open_out = &open),
              "the first frame chosen again");

    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane,
               .which = GUI_DEBUG_VIEW_FIRST + CLI_DEBUG_VIEW_VARIABLES, .root = CHECK_ROOT);
    cli_check(EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u) == CLI_DEBUG_VIEW_VARIABLES,
              "the choice shows Variables");
    cli_check(table->rows(&(GuiPaneCfg){.table = table}) == 2u, "Variables lists the open scope and its variable");
    const GuiRect items = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame,
                                     .pane_read = &pane, .row = 1u);
    (void)EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 1u,
                     .x = items.x + items.width - 20, .root = CHECK_ROOT, .open_out = &open);
    cli_check(table->rows(&(GuiPaneCfg){.table = table}) == 2u, "a press beside a variable's arrow opens nothing");
    (void)EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 1u,
                     .x = items.x + GUI_PANE_PAD_LEFT + GUI_PANE_GUIDE + 4, .root = CHECK_ROOT, .open_out = &open);
    cli_check(table->rows(&(GuiPaneCfg){.table = table}) == 4u, "a press on a variable's arrow opens its children");

    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.terminal.x + 40, .y = field_y) == GUI_DEBUG_FIELD,
              "Variables has its field to press");
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = GUI_DEBUG_FIELD,
               .root = CHECK_ROOT);
    cli_check(pane.keys, "a press on the field takes the keys");
    cli_check(EMBED_CALL(gui_pane.text, GuiPaneCfg, .table = table, .pane = &pane, .text = "count", .length = 5ULL) &&
                  EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Enter",
                             .root = CHECK_ROOT),
              "text typed and Enter go to the field");
    cli_check(table->rows(&(GuiPaneCfg){.table = table}) == 5u, "the watch entered is over the scopes");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Debug", .tree = "");
    cli_check(EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Escape") && !pane.keys,
              "Escape lets the keys go");

    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane,
               .which = GUI_DEBUG_STEP_FIRST + CLI_DEBUG_STEP_OVER, .root = CHECK_ROOT);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_how, .other = "next", .cap = 32ULL}),
              "Step Over pressed reaches the adapter as next");

    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane,
               .which = GUI_DEBUG_VIEW_FIRST + CLI_DEBUG_VIEW_CONSOLE, .root = CHECK_ROOT);
    char lines[2048];
    VerbaLinea body = {lines, sizeof(lines), 0ULL};
    mmgr_line_put(&body, "{\"category\":\"stderr\",\"output\":\"");
    for (unsigned int at = 0u; at < 40u; at += 1u)
    {
        mmgr_line_put(&body, "line ");
        mmgr_line_decimal(&body, at, 1u);
        mmgr_line_put(&body, "\\n");
    }
    mmgr_line_put(&body, "\"}");
    (void)mmgr_line_finish(&body);
    event_read("output", lines);
    cli_check(table->rows(&(GuiPaneCfg){.table = table}) == 40u, "the console lists each line of the output");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Debug", .tree = "");
    cli_check(EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .rows = 3) &&
                  (pane.scroll == 3u),
              "the wheel scrolls the console's lines");

    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
