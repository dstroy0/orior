// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor_test.c: the editor part's check: the lines an answer holds are kept with their runs and the grammar's names,
// drawn from the view's top row by row, each run in its token's color, the line numbers in the gutter, a tab laid out
// to the next stop, a row the part does not hold left as ground, the wheel moving the top inside the file, a view's
// frame taken whole with its cursor, selection and bracket pair drawn, and a fold closed drawn with its marks
#include "../../../../../ui/gui/src/parts/editor.h"
#include "../cli/cli_check.h"

// whether a pixel of `rect` on `raster` leans blue over red by more than `by`, as the keyword's color does blended
// over the ground and the text's color does not
static int bluish(const GuiRaster *raster, GuiRect rect, unsigned int by)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            const GuiColor pixel = raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
            if (GUI_BLUE_OF(pixel) > GUI_RED_OF(pixel) + by)
            {
                return 1;
            }
        }
    }
    return 0;
}

// whether any pixel of `rect` is not `ground`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
            {
                return 1;
            }
        }
    }
    return 0;
}

static const char s_names[] = "[\"\",\"keyword\",\"comment\"]";
static const char s_read[] = "{\"lines\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
                             "\"colors\":[{\"from\":0,\"runs\":[[0,1,3,0],[1,1,7,0],[0,2]]}]}";

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
    GuiEditor editor;
    gui_editor_made(&editor);
    gui_editor_shaped(&editor, 40ull);
    CliJson json;
    char err_text[1024];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = cli_json_taken(&json, s_names, sizeof(s_names) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_editor_named(&editor, &json, root), "the grammar's names are kept");
    cli_json_given(&json);
    root = cli_json_taken(&json, s_read, sizeof(s_read) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_editor_lines_put(&editor, &json, root, 0ull), "the lines read are kept");
    cli_json_given(&json);
    cli_check((editor.from == 0ull) && (editor.count == 3u), "the three lines are held from the first");

    const GuiRect place = {0, 0, 320, 80};
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 320u, 80u), "a raster for the editor is made");
    gui_editor_drawn(&editor, &raster, &glyphs, palette, place, 1.0f);
    const float cell = gui_glyphs_cell(&glyphs, GUI_CODE_SIZE);
    const int text_x = place.x + gui_editor_gutter(&editor, cell, 1.0f);
    const int line_tall = GUI_CODE_LINE;
    cli_check(painted(&raster, (GuiRect){0, 0, text_x - 4, line_tall}, palette->editor_ground),
              "the gutter shows the first line's number");
    cli_check(bluish(&raster, (GuiRect){text_x, 0, (int)(3.0f * cell), line_tall}, 0x30u),
              "the keyword int is drawn in the keyword's color");
    cli_check(!bluish(&raster, (GuiRect){text_x + (int)(4.0f * cell), 0, (int)(4.0f * cell), line_tall}, 0x30u),
              "the word main is drawn in the text's color");
    cli_check(!painted(&raster, (GuiRect){text_x, line_tall, (int)(4.0f * cell) - 1, line_tall}, palette->editor_ground),
              "a tab lays the second line out to the fourth cell");
    cli_check(bluish(&raster, (GuiRect){text_x + (int)(4.0f * cell), line_tall, (int)(6.0f * cell), line_tall}, 0x30u),
              "the keyword return is drawn after the tab in the keyword's color");
    cli_check(painted(&raster, (GuiRect){text_x, 2 * line_tall, (int)(6.0f * cell), line_tall}, palette->editor_ground),
              "the third line, a comment, is drawn");
    cli_check(!painted(&raster, (GuiRect){text_x, 3 * line_tall, 200, line_tall}, palette->editor_ground),
              "a row past the lines held is ground");

    gui_editor_scrolled(&editor, 3ll, 4u);
    cli_check(editor.top == 3ull, "the wheel moves the top down three lines");
    gui_editor_scrolled(&editor, -10ll, 4u);
    cli_check(editor.top == 0ull, "the top goes no further up than the first line");
    gui_editor_scrolled(&editor, 100ll, 4u);
    cli_check(editor.top == 39ull, "the top goes no further down than the last line");
    gui_editor_scrolled(&editor, -100ll, 4u);
    cli_check(editor.top == 0ull, "the top returns to the first line");

    gui_raster_filled(&raster, (GuiRect){0, 0, 320, 80}, GUI_RGB(0u, 0u, 0u));
    gui_editor_drawn(&editor, &raster, &glyphs, palette, (GuiRect){0, 0, 0, 0}, 1.0f);
    cli_check(!painted(&raster, (GuiRect){0, 0, 320, 80}, GUI_RGB(0u, 0u, 0u)), "an editor of no room draws nothing");

    cli_check(gui_editor_text_put(&editor, "one\ntwo\r\nthree\n", 15ull), "a text put whole is split into lines");
    cli_check((editor.lines == 4ull) && (editor.count == 4u) && (editor.from == 0ull),
              "a text ending in a newline has an empty line last");
    cli_check((editor.line[1].length == 3ull) && (editor.bytes[editor.line[1].text + 2u] == 'o'),
              "a carriage return before the newline is taken off");
    cli_check(editor.line[3].length == 0ull, "the last line is empty");
    cli_check(gui_editor_text_put(&editor, "", 0ull) && (editor.lines == 1ull), "an empty text is one empty line");

    static const char s_frame[] = "{\"key\":\"k\",\"seq\":1,\"top\":0,\"lines\":3,\"from\":0,\"rows\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
                                  "\"cursor\":{\"line\":0,\"col\":3},\"anchor\":{\"line\":0,\"col\":0},"
                                  "\"pair\":[{\"line\":2,\"col\":2},{\"line\":0,\"col\":0}],\"indent\":{\"tabs\":0,\"size\":2},"
                                  "\"colors\":[{\"from\":0,\"runs\":[[0,1,3,0],[1,1,7,0],[0,2]]}]}";
    root = cli_json_taken(&json, s_frame, sizeof(s_frame) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_editor_frame_put(&editor, &json, root), "a view's frame is taken whole");
    cli_json_given(&json);
    cli_check((editor.lines == 3ull) && (editor.count == 3u) && (editor.from == 0ull) && (editor.top == 0ull),
              "the frame's rows are the lines held");
    cli_check((editor.indent_tabs == 0) && (editor.indent_size == 2u), "the frame's indentation is kept");
    cli_check(editor.has_cursor && (editor.cursor.line == 0ull) && (editor.cursor.col == 3ull) && (editor.anchor.col == 0ull),
              "the frame's cursor and anchor are kept");
    gui_raster_filled(&raster, (GuiRect){0, 0, 320, 80}, GUI_RGB(0u, 0u, 0u));
    gui_editor_drawn(&editor, &raster, &glyphs, palette, place, 1.0f);
    const int caret_x = text_x + (int)(3.0f * cell + 0.5f);
    cli_check(raster.pixels[(10u * 320u) + (unsigned int)caret_x] == palette->caret, "the caret stands after the third cell");
    cli_check(raster.pixels[(10u * 320u) + (unsigned int)text_x + 1u] == palette->selection,
              "the selection's ground fills the cells selected");
    cli_check(raster.pixels[(30u * 320u) + (unsigned int)text_x + 1u] == palette->editor_ground,
              "a row outside the selection keeps the ground");
    cli_check(editor.has_pair && (editor.pair[0].line == 2ull) && (editor.pair[0].col == 2ull) && (editor.pair[1].line == 0ull),
              "the frame's bracket pair is kept");
    cli_check((raster.pixels[(40u * 320u) + (unsigned int)(text_x + (int)(2.0f * cell)) + 1u] == palette->caret) &&
                  (raster.pixels[(0u * 320u) + (unsigned int)text_x + 1u] == palette->caret),
              "each bracket of the pair is boxed in the caret's color");
    cli_check(raster.pixels[(50u * 320u) + (unsigned int)(text_x + (int)(2.0f * cell)) + 2u] == palette->editor_ground,
              "the box is a line around the cell, not a ground");
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    cli_check(gui_editor_cell_at(&editor, place, cell, 1.0f, text_x + (int)(2.0f * cell) + 1, 25, &row, &column) &&
                  (row == 1ull) && (column == 2ull),
              "a pixel in the text reads as its row and cell");
    cli_check(!gui_editor_cell_at(&editor, place, cell, 1.0f, 2, 25, &row, &column), "a pixel in the gutter is outside the text");

    static const char s_folded[] = "{\"key\":\"k\",\"seq\":2,\"top\":0,\"lines\":6,\"from\":0,\"rows\":[\"int main\",\"{\",\"}\"],"
                                   "\"numbers\":[0,1,5],\"folds\":[[1,4,1,1,\"}\"]],"
                                   "\"cursor\":{\"line\":1,\"col\":1},\"anchor\":{\"line\":1,\"col\":1},\"pair\":null,\"colors\":[]}";
    root = cli_json_taken(&json, s_folded, sizeof(s_folded) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_editor_frame_put(&editor, &json, root), "a frame with a fold closed is taken");
    cli_json_given(&json);
    cli_check((editor.count == 3u) && (editor.line[2].number == 5ull),
              "the rows carry their line numbers, the lines the fold hides left out");
    cli_check((editor.folds == 1u) && (editor.fold[0].first == 1ull) && editor.fold[0].closed &&
                  (editor.fold[0].closer_length == 1ull) && (editor.closers[editor.fold[0].closer] == '}'),
              "the frame's fold is kept with its closer's text");
    gui_raster_filled(&raster, (GuiRect){0, 0, 320, 80}, GUI_RGB(0u, 0u, 0u));
    gui_editor_drawn(&editor, &raster, &glyphs, palette, place, 1.0f);
    cli_check(painted(&raster, (GuiRect){text_x + (int)(2.0f * cell), 20, (int)(2.0f * cell), 20}, palette->current),
              "a closed fold's row ends in the folded mark");
    cli_check(painted(&raster, (GuiRect){text_x - 16, 20, 12, 20}, palette->editor_ground), "the gutter marks the fold closed");
    cli_check(!painted(&raster, (GuiRect){text_x - 16, 40, 12, 20}, palette->editor_ground), "a row that opens no fold has no mark");
    cli_check(gui_editor_row_at(&editor, place, cell, 1.0f, 2, 25, &row) && (row == 1ull), "a pixel in the gutter reads as its row");
    cli_check(!gui_editor_row_at(&editor, place, cell, 1.0f, text_x + 3, 25, &row), "a pixel in the text is not the gutter's");

    static const char s_found[] = "{\"key\":\"k\",\"seq\":3,\"top\":0,\"lines\":3,\"from\":0,\"rows\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
                                  "\"cursor\":{\"line\":0,\"col\":6},\"anchor\":{\"line\":0,\"col\":4},\"pair\":null,"
                                  "\"find\":{\"open\":1,\"focus\":1,\"query\":\"ma\",\"at\":2,\"regex\":0,\"case\":0,\"word\":1,\"count\":1,\"current\":0},"
                                  "\"matches\":[[0,4,6]],\"colors\":[]}";
    root = cli_json_taken(&json, s_found, sizeof(s_found) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_editor_frame_put(&editor, &json, root), "a frame with find open is taken");
    cli_json_given(&json);
    cli_check(editor.find.open && editor.find.focus && (editor.find.query_length == 2ull) && (editor.find.at == 2ull) &&
                  editor.find.word && (editor.find.count == 1ull) && (editor.find.current == 0ll) && (editor.matches == 1ull) &&
                  (editor.match[0][1] == 4ull),
              "find's bar and the matches are kept");
    gui_raster_filled(&raster, (GuiRect){0, 0, 320, 80}, GUI_RGB(0u, 0u, 0u));
    gui_editor_drawn(&editor, &raster, &glyphs, palette, place, 1.0f);
    const GuiColor under_match = raster.pixels[(1u * 320u) + (unsigned int)(text_x + (int)(4.0f * cell)) + 1u];
    cli_check((under_match != palette->selection) && (under_match != palette->editor_ground),
              "the current match is drawn in its own ground over the selection");
    cli_check(painted(&raster, (GuiRect){place.x + place.width - 40, place.y + 2, 30, 8}, palette->editor_ground),
              "find's bar is drawn at the editor's top right");

    gui_raster_given(&raster);
    gui_editor_given(&editor);
    cli_check(editor.count == 0u, "the editor given back holds no lines");
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
