// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest_test.c: the completion part's check: a label's score against the prefix, an answer's items stored with the
// file's words after the server's and a word the server gave left out, the items ranked, the keys moving the choice
// and accepting or closing, the one item that is the prefix closing the list unless forced, the list drawn under the
// prefix's cell with the chosen row on the selection's ground and a press on a row accepting it
#include "../../../../../ui/gui/src/parts/suggest.h"
#include "../cli/cli_check.h"

static const char s_answer[] = "{\"from\":{\"line\":0,\"col\":4},\"prefix\":\"al\",\"items\":[{\"label\":\"alpha\","
                               "\"kind\":\"function\",\"detail\":\"int alpha(void)\",\"insert\":\"alpha()\"},"
                               "{\"label\":\"Beta\",\"kind\":\"word\",\"insert\":\"Beta\"}],\"words\":[{\"label\":"
                               "\"alpha\",\"kind\":\"word\",\"insert\":\"alpha\"},"
                               "{\"label\":\"total\",\"kind\":\"word\",\"insert\":\"total\"},{\"label\":\"ALias\","
                               "\"kind\":\"word\",\"insert\":\"ALias\"}]}";
static const char s_alone[] = "{\"from\":{\"line\":0,\"col\":0},\"prefix\":\"total\",\"items\":[],\"words\":[{"
                              "\"label\":\"total\",\"kind\":\"word\",\"insert\":\"total\"}]}";
static const char s_empty[] =
    "{\"from\":{\"line\":0,\"col\":0},\"prefix\":\"\",\"items\":[],\"words\":[{\"label\":\"alpha\",\"kind\":\"word\","
    "\"insert\":\"alpha\"},{\"label\":\"beta\",\"kind\":\"word\",\"insert\":\"beta\"}]}";
static const char s_snippet[] =
    "{\"from\":{\"line\":0,\"col\":0},\"prefix\":\"f\",\"items\":[{\"label\":\"fill\",\"kind\":\"function\",\"detail\":"
    "\"\",\"insert\":\"fill(${1:rect}, ${2|red,blue|}, $3)\\\\$${4}\",\"snippet\":true}],\"words\":[]}";

// whether the item chosen inserts `insert`
static int chosen_is(const GuiSuggest *suggest, const char *insert)
{
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(cli_suggest.choose, GuiSuggestCfg, .suggest_read = suggest, .length_pointer = &length);
    return (text != NULL) && (length == cellul.len(&(CatenaFinitaCfg){.src = insert, .cap = 64ull})) &&
           (memor.cmp(&(MemoriaCfg){.src = text, .other = insert, .bytes = length}) == 0);
}

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
    cli_check(EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = "alpha", .length = 5ull, .typed = "al") == 5u, "a label starting with the prefix scores five");
    cli_check(EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = "ALias", .length = 5ull, .typed = "al") == 4u, "a label starting with it by case scores four");
    cli_check(EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = "total", .length = 5ull, .typed = "al") == 3u, "a label containing it scores three");
    cli_check(EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = "apple", .length = 5ull, .typed = "al") == 2u, "a label with its letters in order scores two");
    cli_check(EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = "beta", .length = 4ull, .typed = "al") == 0u, "a label without its letters scores nothing");
    cli_check(EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = "beta", .length = 4ull, .typed = "") == 1u, "every label answers an empty prefix");

    GuiSuggest suggest;
    cli_check(EMBED_CALL(gui_suggest_rows.init, GuiSuggestRowsCfg, .suggest = &suggest) && !suggest.open, "a completion made is closed");
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_answer, .size = sizeof(s_answer) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 0), "the answer opens the list");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check(suggest.open && (suggest.items == 4u),
              "the server's items come first and a word the server gave is left out");
    cli_check((suggest.count == 3u) && (suggest.index == 0u), "three items answer the prefix");
    cli_check(chosen_is(&suggest, "alpha()"), "the best item is chosen first and inserts the server's text");
    cli_check((suggest.line == 0ull) && (suggest.from == 4ull) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = suggest.prefix, .other = "al", .cap = 8ull}),
              "the prefix and its place are kept");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Down") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "ALias"),
              "Down moves to the next item");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Down") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "total"),
              "the items rank a part below a start");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Down") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "alpha()"),
              "Down past the last wraps to the first");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Up") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "total"),
              "Up before the first wraps to the last");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "PageUp") == GUI_SUGGEST_MOVED && (suggest.index == 0u),
              "PageUp goes to the first");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "PageDown") == GUI_SUGGEST_MOVED && (suggest.index == 2u),
              "PageDown goes to the last");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Left") == GUI_SUGGEST_NONE, "a key not the list's is not taken");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Enter") == GUI_SUGGEST_ACCEPTED && suggest.open,
              "Enter accepts the item chosen");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Escape") == GUI_SUGGEST_CLOSED && !suggest.open && (suggest.items == 0u),
              "Escape closes the list");
    cli_check(EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &suggest, .name = "Down") == GUI_SUGGEST_NONE, "a closed list takes no key");

    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_alone, .size = sizeof(s_alone) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(!EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 0) && !suggest.open,
              "the one item that is the prefix closes the list");
    cli_check(EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 1) && suggest.open && (suggest.count == 1u),
              "forced, it stands open on that item");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_snippet, .size = sizeof(s_snippet) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 0) && chosen_is(&suggest, "fill(rect, red, )$"),
              "a snippet inserts its placeholders, first choices and escaped bytes, its stops left out");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &suggest);
    cli_check(!EMBED_CALL(gui_suggest_json.prefix, GuiSuggestJsonCfg, .suggest = &suggest, .prefix = "al", .length = 2ull), "a closed list is not filtered");
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_answer, .size = sizeof(s_answer) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 0) && (suggest.count == 3u),
              "the answer opens the list on three items");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check(EMBED_CALL(gui_suggest_json.prefix, GuiSuggestJsonCfg, .suggest = &suggest, .prefix = "alp", .length = 3ull) && (suggest.count == 1u) && chosen_is(&suggest, "alpha()"),
              "a letter typed filters the items the list stands on");
    cli_check(EMBED_CALL(gui_suggest_json.prefix, GuiSuggestJsonCfg, .suggest = &suggest, .prefix = "al", .length = 2ull) && (suggest.count == 3u),
              "a letter taken back widens them again");
    cli_check(!EMBED_CALL(gui_suggest_json.prefix, GuiSuggestJsonCfg, .suggest = &suggest, .prefix = "", .length = 0ull) && !suggest.open,
              "the prefix taken back whole closes the list");
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_answer, .size = sizeof(s_answer) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 0), "the answer opens the list again");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check(!EMBED_CALL(gui_suggest_json.prefix, GuiSuggestJsonCfg, .suggest = &suggest, .prefix = "alq", .length = 3ull) && !suggest.open,
              "a prefix no item answers closes the list");
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_empty, .size = sizeof(s_empty) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(!EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 0) && !suggest.open, "an empty prefix opens no list");
    cli_check(EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 1) && suggest.open && (suggest.count == 2u),
              "forced, an empty prefix lists every item");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);

    // drawn
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_answer, .size = sizeof(s_answer) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &suggest, .json = &json, .root = root, .forced = 0), "the answer opens the list again");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 800u, .height = 500u), "a raster is made");
    const GuiRect bounds = {0, 0, 800, 500};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = bounds, .color = palette->editor_ground);
    EMBED_CALL(gui_suggest_raster.draw, GuiSuggestRasterCfg, .suggest = &suggest, .raster = &raster, .glyphs = &glyphs, .palette = palette, .anchor = (GuiRect){200, 100, 8, 20}, .bounds = bounds, .scale = 100);
    cli_check((suggest.rect.y == 100 + 20 + GUI_PANEL_OFF) && (suggest.rect.x == 200 - GUI_SUGGEST_BACK),
              "the list stands under the prefix's cell");
    cli_check((suggest.list.height == 3 * GUI_SUGGEST_ROW) && (suggest.list.width >= GUI_SUGGEST_LEAST),
              "the list is a row an item, no narrower than the least");
    cli_check(suggest.detail.width > 0, "the chosen item's detail has its pane");
    cli_check(raster.pixels[((unsigned int)(suggest.list.y + 11) * 800u) +
                            (unsigned int)(suggest.list.x + suggest.list.width - 2)] == palette->selection,
              "the chosen row stands on the selection's ground");
    cli_check(raster.pixels[((unsigned int)(suggest.list.y + GUI_SUGGEST_ROW + 11) * 800u) +
                            (unsigned int)(suggest.list.x + suggest.list.width - 2)] == palette->surface,
              "the row below stands on the widget's ground");
    cli_check(EMBED_CALL(gui_suggest_x.holds, GuiSuggestXCfg, .suggest_read = &suggest, .x = suggest.rect.x + 4, .y = suggest.rect.y + 4) &&
                  !EMBED_CALL(gui_suggest_x.holds, GuiSuggestXCfg, .suggest_read = &suggest, .x = 10, .y = 10),
              "the list holds its pixels alone");
    cli_check((EMBED_CALL(gui_suggest_xy.press, GuiSuggestXYCfg, .suggest = &suggest, .x = suggest.list.x + 20, .y = suggest.list.y + (2 * GUI_SUGGEST_ROW) + 5) ==
               GUI_SUGGEST_ACCEPTED) &&
                  chosen_is(&suggest, "total"),
              "a press on a row chooses and accepts its item");
    cli_check(EMBED_CALL(gui_suggest_xy.press, GuiSuggestXYCfg, .suggest = &suggest, .x = 10, .y = 10) == GUI_SUGGEST_NONE, "a press outside the list is not its");
    cli_check(!EMBED_CALL(gui_suggest_rows.wheel, GuiSuggestRowsCfg, .suggest = &suggest, .rows = 3), "the wheel moves nothing while every row is shown");
    EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &suggest);
    cli_check(!suggest.open && (suggest.rect.width == 0), "the list closed has no rectangle");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_suggest_rows.free, GuiSuggestRowsCfg, .suggest = &suggest);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
