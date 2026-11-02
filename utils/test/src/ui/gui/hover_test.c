// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hover_test.c: the hover part's check: a pointer resting on a cell asks about it as the rest is up and not before, a
// pointer moved begins the rest again, an answer's parts are shown joined with a rule between them under the word's
// row, the box stays while the pointer stands on the word or in the box, goes a linger after it leaves them, and a
// null answer leaves nothing shown
#include "../../../../../ui/gui/src/parts/hover.h"
#include "../cli/cli_check.h"

static int contains(const char *text, unsigned long long length, const char *part)
{
    const unsigned long long want = cellul.len(&(CatenaFinitaCfg){.src = part, .cap = 256ull});
    for (unsigned long long at = 0ull; at + want <= length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = text + at, .other = part, .bytes = want}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static const char s_answer[] = "{\"from\":{\"line\":1,\"col\":2},\"to\":{\"line\":1,\"col\":6},\"parts\":[\"```\\nint "
                               "beta\\n```\\n\\nfunction `a.c:3`\",\"the server's say\"]}";

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiHover hover;
    EMBED_CALL(gui_hover_state.init, GuiHoverStateCfg, .hover = &hover);
    g_test_clock_ms = 1000ull;
    cli_check(!hover.shown && (EMBED_CALL(gui_hover.next_deadline_ms, GuiHoverCfg, .hover_read = &hover) == 0ull),
              "a hover made shows nothing and wants no clock");

    // the rest
    int ask = 0;
    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_hover_in.move, GuiHoverInCfg, .hover = &hover, .in_text = 1, .row = 1ull, .column = 3ull, .x = 50, .y = 30);
    g_test_clock_ms = 1000ull;
    cli_check(EMBED_CALL(gui_hover.next_deadline_ms, GuiHoverCfg, .hover_read = &hover) == 1000ull + GUI_HOVER_SHOW_DELAY_MS,
              "a pointer in the text wants the clock after the rest");
    g_test_clock_ms = 1000ull + GUI_HOVER_SHOW_DELAY_MS - 1ull;
    cli_check(!EMBED_CALL(gui_hover_state.tick, GuiHoverStateCfg, .hover = &hover, .ask = &ask) && !ask,
              "the clock before the rest is up asks nothing");
    g_test_clock_ms = 1200ull;
    EMBED_CALL(gui_hover_in.move, GuiHoverInCfg, .hover = &hover, .in_text = 1, .row = 1ull, .column = 4ull, .x = 58, .y = 30);
    g_test_clock_ms = 1000ull + GUI_HOVER_SHOW_DELAY_MS;
    cli_check(!EMBED_CALL(gui_hover_state.tick, GuiHoverStateCfg, .hover = &hover, .ask = &ask) && !ask,
              "a pointer moved to another cell begins the rest again");
    g_test_clock_ms = 1200ull + GUI_HOVER_SHOW_DELAY_MS;
    cli_check(!EMBED_CALL(gui_hover_state.tick, GuiHoverStateCfg, .hover = &hover, .ask = &ask) && ask && (hover.asked_row == 1ull) &&
                  (hover.asked_column == 4ull),
              "the rest up asks about the cell the pointer stands on");
    g_test_clock_ms = 1200ull + GUI_HOVER_SHOW_DELAY_MS;
    cli_check(EMBED_CALL(gui_hover.next_deadline_ms, GuiHoverCfg, .hover_read = &hover) == 0ull, "a rest asked about wants the clock no more");
    g_test_clock_ms = 1700ull;
    EMBED_CALL(gui_hover_in.move, GuiHoverInCfg, .hover = &hover, .in_text = 0, .row = 0ull, .column = 0ull, .x = 5, .y = 5);
    g_test_clock_ms = 1700ull;
    cli_check(EMBED_CALL(gui_hover.next_deadline_ms, GuiHoverCfg, .hover_read = &hover) == 0ull, "a pointer out of the text wants no clock");

    // the answer
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_answer, .size = sizeof(s_answer) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_hover_state.put, GuiHoverStateCfg, .hover = &hover, .json = &json, .root = root), "the answer is taken");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check(hover.shown && (hover.line == 1ull) && (hover.from == 2ull) && (hover.to == 6ull),
              "the box is shown for the word's span");
    cli_check(contains(hover.text, hover.length, "int beta") && contains(hover.text, hover.length, "\n---\n") &&
                  contains(hover.text, hover.length, "the server's say"),
              "the parts are joined with a rule between them");

    // drawn
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 800u, .height = 400u), "a raster is made");
    const GuiRect bounds = {0, 0, 800, 400};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = bounds, .color = palette->editor_ground);
    EMBED_CALL(gui_hover_raster.draw, GuiHoverRasterCfg, .hover = &hover, .raster = &raster, .glyphs = &glyphs, .palette = palette, .anchor = (GuiRect){100, 60, 8, 20}, .last = (GuiRect){124, 60, 8, 20}, .bounds = bounds, .scale = 100);
    cli_check((hover.rect.y == 60 + 20 + GUI_PANEL_OFF) && (hover.rect.x == 100) && (hover.rect.height > 0),
              "the box stands under the word's row");
    cli_check(raster.pixels[((unsigned int)(hover.rect.y + 10) * 800u) + (unsigned int)(hover.rect.x + 10)] ==
                  palette->surface,
              "the box is filled with the widget's ground");
    cli_check((hover.word.x == 100 - 8) && (hover.word.width == 32 + 16),
              "the word's rectangle reaches a cell either side of it");
    cli_check(EMBED_CALL(gui_hover_x.holds, GuiHoverXCfg, .hover_read = &hover, .x = hover.rect.x + 5, .y = hover.rect.y + 5) && !EMBED_CALL(gui_hover_x.holds, GuiHoverXCfg, .hover_read = &hover, .x = 5, .y = 5),
              "the box holds its pixels alone");

    // the linger
    g_test_clock_ms = 2000ull;
    EMBED_CALL(gui_hover_in.move, GuiHoverInCfg, .hover = &hover, .in_text = 1, .row = 1ull, .column = 4ull, .x = 110, .y = 70);
    cli_check(hover.hide_deadline_ms == 0ull, "a pointer on the word keeps the box");
    g_test_clock_ms = 2100ull;
    EMBED_CALL(gui_hover_in.move, GuiHoverInCfg, .hover = &hover, .in_text = 1, .row = 2ull, .column = 4ull, .x = hover.rect.x + 20, .y = hover.rect.y + 20);
    cli_check(hover.hide_deadline_ms == 0ull, "a pointer in the box keeps it");
    g_test_clock_ms = 2200ull;
    EMBED_CALL(gui_hover_in.move, GuiHoverInCfg, .hover = &hover, .in_text = 1, .row = 5ull, .column = 1ull, .x = 20, .y = 300);
    g_test_clock_ms = 2200ull;
    cli_check((hover.hide_deadline_ms == 2200ull) && (EMBED_CALL(gui_hover.next_deadline_ms, GuiHoverCfg, .hover_read = &hover) == 2200ull + GUI_HOVER_HIDE_DELAY_MS),
              "a pointer off the word and the box begins the linger");
    g_test_clock_ms = 2200ull + GUI_HOVER_HIDE_DELAY_MS - 1ull;
    cli_check(!EMBED_CALL(gui_hover_state.tick, GuiHoverStateCfg, .hover = &hover, .ask = &ask) && hover.shown,
              "the box stands through the linger");
    g_test_clock_ms = 2200ull + GUI_HOVER_HIDE_DELAY_MS;
    cli_check(EMBED_CALL(gui_hover_state.tick, GuiHoverStateCfg, .hover = &hover, .ask = &ask) && !hover.shown,
              "the linger up hides the box");

    // a null answer
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = "null", .size = 4ull, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_hover_state.put, GuiHoverStateCfg, .hover = &hover, .json = &json, .root = root) && !hover.shown, "a null answer shows nothing");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_hover_state.free, GuiHoverStateCfg, .hover = &hover);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
