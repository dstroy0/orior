// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest_test.c: the completion part's check: a label's score against the prefix, an answer's items stored with the
// file's words after the server's and a word the server gave left out, the items ranked, the keys moving the choice
// and accepting or closing, the one item that is the prefix closing the list unless forced, the list drawn under the
// prefix's cell with the chosen row on the selection's ground and a press on a row accepting it
#include "../../../../../ui/gui/src/parts/suggest.h"
#include "../cli/cli_check.h"

static const char s_answer[] =
    "{\"from\":{\"line\":0,\"col\":4},\"prefix\":\"al\",\"items\":[{\"label\":\"alpha\",\"kind\":\"function\",\"detail\":\"int alpha(void)\",\"insert\":\"alpha()\"},"
    "{\"label\":\"Beta\",\"kind\":\"word\",\"insert\":\"Beta\"}],\"words\":[{\"label\":\"alpha\",\"kind\":\"word\",\"insert\":\"alpha\"},"
    "{\"label\":\"total\",\"kind\":\"word\",\"insert\":\"total\"},{\"label\":\"ALias\",\"kind\":\"word\",\"insert\":\"ALias\"}]}";
static const char s_alone[] = "{\"from\":{\"line\":0,\"col\":0},\"prefix\":\"total\",\"items\":[],\"words\":[{\"label\":\"total\",\"kind\":\"word\",\"insert\":\"total\"}]}";
static const char s_empty[] =
    "{\"from\":{\"line\":0,\"col\":0},\"prefix\":\"\",\"items\":[],\"words\":[{\"label\":\"alpha\",\"kind\":\"word\",\"insert\":\"alpha\"},{\"label\":\"beta\",\"kind\":\"word\",\"insert\":\"beta\"}]}";
static const char s_snippet[] =
    "{\"from\":{\"line\":0,\"col\":0},\"prefix\":\"f\",\"items\":[{\"label\":\"fill\",\"kind\":\"function\",\"detail\":\"\",\"insert\":\"fill(${1:rect}, ${2|red,blue|}, $3)\\\\$${4}\",\"snippet\":true}],\"words\":[]}";

// whether the item chosen inserts `insert`
static int chosen_is(const GuiSuggest *suggest, const char *insert)
{
    unsigned long long length = 0ull;
    const char *const text = gui_suggest_chosen(suggest, &length);
    return (text != NULL) && (length == cellul.len(&(CatenaFinitaCfg){.src = insert, .cap = 64ull})) &&
           (memor.cmp(&(MemoriaCfg){.src = text, .other = insert, .bytes = length}) == 0);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!gui_glyphs_made(&glyphs, &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    cli_check(gui_suggest_score("alpha", 5ull, "al") == 5u, "a label starting with the prefix scores five");
    cli_check(gui_suggest_score("ALias", 5ull, "al") == 4u, "a label starting with it by case scores four");
    cli_check(gui_suggest_score("total", 5ull, "al") == 3u, "a label containing it scores three");
    cli_check(gui_suggest_score("apple", 5ull, "al") == 2u, "a label with its letters in order scores two");
    cli_check(gui_suggest_score("beta", 4ull, "al") == 0u, "a label without its letters scores nothing");
    cli_check(gui_suggest_score("beta", 4ull, "") == 1u, "every label answers an empty prefix");

    GuiSuggest suggest;
    cli_check(gui_suggest_made(&suggest) && !suggest.open, "a completion made is closed");
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = cli_json_taken(&json, s_answer, sizeof(s_answer) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_suggest_put(&suggest, &json, root, 0), "the answer opens the list");
    cli_json_given(&json);
    cli_check(suggest.open && (suggest.items == 4u), "the server's items come first and a word the server gave is left out");
    cli_check((suggest.count == 3u) && (suggest.index == 0u), "three items answer the prefix");
    cli_check(chosen_is(&suggest, "alpha()"), "the best item is chosen first and inserts the server's text");
    cli_check((suggest.line == 0ull) && (suggest.from == 4ull) && cellul.eq(&(CatenaFinitaCfg){.src = suggest.prefix, .other = "al", .cap = 8ull}),
              "the prefix and its place are kept");
    cli_check(gui_suggest_keyed(&suggest, "Down") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "ALias"), "Down moves to the next item");
    cli_check(gui_suggest_keyed(&suggest, "Down") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "total"), "the items rank a part below a start");
    cli_check(gui_suggest_keyed(&suggest, "Down") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "alpha()"), "Down past the last wraps to the first");
    cli_check(gui_suggest_keyed(&suggest, "Up") == GUI_SUGGEST_MOVED && chosen_is(&suggest, "total"), "Up before the first wraps to the last");
    cli_check(gui_suggest_keyed(&suggest, "PageUp") == GUI_SUGGEST_MOVED && (suggest.index == 0u), "PageUp goes to the first");
    cli_check(gui_suggest_keyed(&suggest, "PageDown") == GUI_SUGGEST_MOVED && (suggest.index == 2u), "PageDown goes to the last");
    cli_check(gui_suggest_keyed(&suggest, "Left") == GUI_SUGGEST_NONE, "a key not the list's is not taken");
    cli_check(gui_suggest_keyed(&suggest, "Enter") == GUI_SUGGEST_ACCEPTED && suggest.open, "Enter accepts the item chosen");
    cli_check(gui_suggest_keyed(&suggest, "Escape") == GUI_SUGGEST_CLOSED && !suggest.open && (suggest.items == 0u), "Escape closes the list");
    cli_check(gui_suggest_keyed(&suggest, "Down") == GUI_SUGGEST_NONE, "a closed list takes no key");

    root = cli_json_taken(&json, s_alone, sizeof(s_alone) - 1u, 0u, 0ull, &err_msg);
    cli_check(!gui_suggest_put(&suggest, &json, root, 0) && !suggest.open, "the one item that is the prefix closes the list");
    cli_check(gui_suggest_put(&suggest, &json, root, 1) && suggest.open && (suggest.count == 1u), "forced, it stands open on that item");
    cli_json_given(&json);
    root = cli_json_taken(&json, s_snippet, sizeof(s_snippet) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_suggest_put(&suggest, &json, root, 0) && chosen_is(&suggest, "fill(rect, red, )$"),
              "a snippet inserts its placeholders, first choices and escaped bytes, its stops left out");
    cli_json_given(&json);
    gui_suggest_closed(&suggest);
    cli_check(!gui_suggest_prefixed(&suggest, "al", 2ull), "a closed list is not filtered");
    root = cli_json_taken(&json, s_answer, sizeof(s_answer) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_suggest_put(&suggest, &json, root, 0) && (suggest.count == 3u), "the answer opens the list on three items");
    cli_json_given(&json);
    cli_check(gui_suggest_prefixed(&suggest, "alp", 3ull) && (suggest.count == 1u) && chosen_is(&suggest, "alpha()"),
              "a letter typed filters the items the list stands on");
    cli_check(gui_suggest_prefixed(&suggest, "al", 2ull) && (suggest.count == 3u), "a letter taken back widens them again");
    cli_check(!gui_suggest_prefixed(&suggest, "", 0ull) && !suggest.open, "the prefix taken back whole closes the list");
    root = cli_json_taken(&json, s_answer, sizeof(s_answer) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_suggest_put(&suggest, &json, root, 0), "the answer opens the list again");
    cli_json_given(&json);
    cli_check(!gui_suggest_prefixed(&suggest, "alq", 3ull) && !suggest.open, "a prefix no item answers closes the list");
    root = cli_json_taken(&json, s_empty, sizeof(s_empty) - 1u, 0u, 0ull, &err_msg);
    cli_check(!gui_suggest_put(&suggest, &json, root, 0) && !suggest.open, "an empty prefix opens no list");
    cli_check(gui_suggest_put(&suggest, &json, root, 1) && suggest.open && (suggest.count == 2u), "forced, an empty prefix lists every item");
    cli_json_given(&json);

    // drawn
    root = cli_json_taken(&json, s_answer, sizeof(s_answer) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_suggest_put(&suggest, &json, root, 0), "the answer opens the list again");
    cli_json_given(&json);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 800u, 500u), "a raster is made");
    const GuiRect bounds = {0, 0, 800, 500};
    gui_raster_filled(&raster, bounds, palette->editor_ground);
    gui_suggest_drawn(&suggest, &raster, &glyphs, palette, (GuiRect){200, 100, 8, 20}, bounds, 1.0f);
    cli_check((suggest.rect.y == 100 + 20 + GUI_PANEL_OFF) && (suggest.rect.x == 200 - GUI_SUGGEST_BACK), "the list stands under the prefix's cell");
    cli_check((suggest.list.height == 3 * GUI_SUGGEST_ROW) && (suggest.list.width >= GUI_SUGGEST_LEAST), "the list is a row an item, no narrower than the least");
    cli_check(suggest.detail.width > 0, "the chosen item's detail has its pane");
    cli_check(raster.pixels[((unsigned int)(suggest.list.y + 11) * 800u) + (unsigned int)(suggest.list.x + suggest.list.width - 2)] == palette->selection,
              "the chosen row stands on the selection's ground");
    cli_check(raster.pixels[((unsigned int)(suggest.list.y + GUI_SUGGEST_ROW + 11) * 800u) + (unsigned int)(suggest.list.x + suggest.list.width - 2)] == palette->surface,
              "the row below stands on the widget's ground");
    cli_check(gui_suggest_holds(&suggest, suggest.rect.x + 4, suggest.rect.y + 4) && !gui_suggest_holds(&suggest, 10, 10), "the list holds its pixels alone");
    cli_check((gui_suggest_pressed(&suggest, suggest.list.x + 20, suggest.list.y + (2 * GUI_SUGGEST_ROW) + 5) == GUI_SUGGEST_ACCEPTED) &&
                  chosen_is(&suggest, "total"),
              "a press on a row chooses and accepts its item");
    cli_check(gui_suggest_pressed(&suggest, 10, 10) == GUI_SUGGEST_NONE, "a press outside the list is not its");
    cli_check(!gui_suggest_wheeled(&suggest, 3), "the wheel moves nothing while every row is shown");
    gui_suggest_closed(&suggest);
    cli_check(!suggest.open && (suggest.rect.width == 0), "the list closed has no rectangle");
    gui_raster_given(&raster);
    gui_suggest_given(&suggest);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
