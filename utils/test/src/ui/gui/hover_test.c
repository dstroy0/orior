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
    GuiHover hover;
    gui_hover_made(&hover);
    cli_check(!hover.shown && (gui_hover_due(&hover, 1000ull) == 0ull),
              "a hover made shows nothing and wants no clock");

    // the rest
    int ask = 0;
    gui_hover_moved(&hover, 1, 1ull, 3ull, 50, 30, 1000ull);
    cli_check(gui_hover_due(&hover, 1000ull) == GUI_HOVER_RESTS,
              "a pointer in the text wants the clock after the rest");
    cli_check(!gui_hover_ticked(&hover, 1000ull + GUI_HOVER_RESTS - 1ull, &ask) && !ask,
              "the clock before the rest is up asks nothing");
    gui_hover_moved(&hover, 1, 1ull, 4ull, 58, 30, 1200ull);
    cli_check(!gui_hover_ticked(&hover, 1000ull + GUI_HOVER_RESTS, &ask) && !ask,
              "a pointer moved to another cell begins the rest again");
    cli_check(!gui_hover_ticked(&hover, 1200ull + GUI_HOVER_RESTS, &ask) && ask && (hover.asked_row == 1ull) &&
                  (hover.asked_column == 4ull),
              "the rest up asks about the cell the pointer stands on");
    cli_check(gui_hover_due(&hover, 1200ull + GUI_HOVER_RESTS) == 0ull, "a rest asked about wants the clock no more");
    gui_hover_moved(&hover, 0, 0ull, 0ull, 5, 5, 1700ull);
    cli_check(gui_hover_due(&hover, 1700ull) == 0ull, "a pointer out of the text wants no clock");

    // the answer
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = cli_json_taken(&json, s_answer, sizeof(s_answer) - 1u, 0u, 0ull, &err_msg, &g_checks_cells);
    cli_check(gui_hover_put(&hover, &json, root), "the answer is taken");
    cli_json_given(&json);
    cli_check(hover.shown && (hover.line == 1ull) && (hover.from == 2ull) && (hover.to == 6ull),
              "the box is shown for the word's span");
    cli_check(contains(hover.text, hover.length, "int beta") && contains(hover.text, hover.length, "\n---\n") &&
                  contains(hover.text, hover.length, "the server's say"),
              "the parts are joined with a rule between them");

    // drawn
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 800u, 400u), "a raster is made");
    const GuiRect bounds = {0, 0, 800, 400};
    gui_raster_filled(&raster, bounds, palette->editor_ground);
    gui_hover_drawn(&hover, &raster, &glyphs, palette, (GuiRect){100, 60, 8, 20}, (GuiRect){124, 60, 8, 20}, bounds,
                    1.0f);
    cli_check((hover.rect.y == 60 + 20 + GUI_PANEL_OFF) && (hover.rect.x == 100) && (hover.rect.height > 0),
              "the box stands under the word's row");
    cli_check(raster.pixels[((unsigned int)(hover.rect.y + 10) * 800u) + (unsigned int)(hover.rect.x + 10)] ==
                  palette->surface,
              "the box is filled with the widget's ground");
    cli_check((hover.word.x == 100 - 8) && (hover.word.width == 32 + 16),
              "the word's rectangle reaches a cell either side of it");
    cli_check(gui_hover_holds(&hover, hover.rect.x + 5, hover.rect.y + 5) && !gui_hover_holds(&hover, 5, 5),
              "the box holds its pixels alone");

    // the linger
    gui_hover_moved(&hover, 1, 1ull, 4ull, 110, 70, 2000ull);
    cli_check(hover.left_ms == 0ull, "a pointer on the word keeps the box");
    gui_hover_moved(&hover, 1, 2ull, 4ull, hover.rect.x + 20, hover.rect.y + 20, 2100ull);
    cli_check(hover.left_ms == 0ull, "a pointer in the box keeps it");
    gui_hover_moved(&hover, 1, 5ull, 1ull, 20, 300, 2200ull);
    cli_check((hover.left_ms == 2200ull) && (gui_hover_due(&hover, 2200ull) == GUI_HOVER_LINGER),
              "a pointer off the word and the box begins the linger");
    cli_check(!gui_hover_ticked(&hover, 2200ull + GUI_HOVER_LINGER - 1ull, &ask) && hover.shown,
              "the box stands through the linger");
    cli_check(gui_hover_ticked(&hover, 2200ull + GUI_HOVER_LINGER, &ask) && !hover.shown,
              "the linger up hides the box");

    // a null answer
    root = cli_json_taken(&json, "null", 4ull, 0u, 0ull, &err_msg, &g_checks_cells);
    cli_check(gui_hover_put(&hover, &json, root) && !hover.shown, "a null answer shows nothing");
    cli_json_given(&json);
    gui_raster_given(&raster);
    gui_hover_given(&hover);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
