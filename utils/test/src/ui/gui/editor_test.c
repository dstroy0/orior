// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor_test.c: the editor part's check: the lines an answer holds are saved with their runs and the grammar's names,
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

// `color` over `under` by the color's alpha, as the raster blends
static GuiColor over(GuiColor under, GuiColor color)
{
    const unsigned int share = GUI_ALPHA_OF(color);
    const unsigned int rest = 255u - share;
    return GUI_RGB(((GUI_RED_OF(color) * share) + (GUI_RED_OF(under) * rest) + 127u) / 255u,
                   ((GUI_GREEN_OF(color) * share) + (GUI_GREEN_OF(under) * rest) + 127u) / 255u,
                   ((GUI_BLUE_OF(color) * share) + (GUI_BLUE_OF(under) * rest) + 127u) / 255u);
}

static const char s_names[] = "[\"\",\"keyword\",\"comment\"]";
static const char s_read[] = "{\"lines\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
                             "\"colors\":[{\"from\":0,\"runs\":[[0,1,3,0],[1,1,7,0],[0,2]]}]}";

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
    // the editor's ground at its opacity over the raster's black: a bare pixel's color
    const GuiColor bare = over(GUI_RGB(0u, 0u, 0u), palette->editor_ground);
    GuiEditor editor;
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &editor);
    EMBED_CALL(gui_editor_table.shape, GuiEditorCfg, .editor = &editor, .lines = 40ull);
    CliJson json;
    char err_text[1024];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_names, .size = sizeof(s_names) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_editor_json_names.by_name, GuiEditorJsonNamesCfg, .editor = &editor, .json = &json, .names = root), "the grammar's names are kept");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_read, .size = sizeof(s_read) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_editor_json_read.lines_put, GuiEditorJsonReadCfg, .editor = &editor, .json = &json, .read = root, .from = 0ull), "the lines read are kept");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check((editor.from == 0ull) && (editor.count == 3u), "the three lines are stored from the first");

    const GuiRect place = {0, 0, 320, 80};
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 320u, .height = 80u), "a raster for the editor is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = place, .color = palette->editor_ground);
    EMBED_CALL(gui_editor_place_scale.draw, GuiEditorPlaceScaleCfg, .editor = &editor, .raster = &raster, .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .size = GUI_CODE_SIZE);
    const int text_x = place.x + EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg, .editor_read = &editor, .cell = cell, .scale = 100);
    const int line_tall = GUI_CODE_LINE;
    cli_check(painted(&raster, (GuiRect){0, 0, text_x - 4, line_tall}, bare),
              "the gutter shows the first line's number");
    cli_check(bluish(&raster, (GuiRect){text_x, 0, (int)(3.0f * cell), line_tall}, 0x30u),
              "the keyword int is drawn in the keyword's color");
    cli_check(!bluish(&raster, (GuiRect){text_x + (int)(4.0f * cell), 0, (int)(4.0f * cell), line_tall}, 0x30u),
              "the word main is drawn in the text's color");
    cli_check(!painted(&raster, (GuiRect){text_x, line_tall, (int)(4.0f * cell) - 1, line_tall}, bare),
              "a tab lays the second line out to the fourth cell");
    cli_check(bluish(&raster, (GuiRect){text_x + (int)(4.0f * cell), line_tall, (int)(6.0f * cell), line_tall}, 0x30u),
              "the keyword return is drawn after the tab in the keyword's color");
    cli_check(painted(&raster, (GuiRect){text_x, 2 * line_tall, (int)(6.0f * cell), line_tall}, bare),
              "the third line, a comment, is drawn");
    cli_check(!painted(&raster, (GuiRect){text_x, 3 * line_tall, 200, line_tall}, bare),
              "a row past the lines stored is ground");

    EMBED_CALL(gui_editor_delta.scroll, GuiEditorDeltaCfg, .editor = &editor, .delta = 3ll, .rows = 4u);
    cli_check(editor.top == 3ull, "the wheel moves the top down three lines");
    EMBED_CALL(gui_editor_delta.scroll, GuiEditorDeltaCfg, .editor = &editor, .delta = -10ll, .rows = 4u);
    cli_check(editor.top == 0ull, "the top goes no further up than the first line");
    EMBED_CALL(gui_editor_delta.scroll, GuiEditorDeltaCfg, .editor = &editor, .delta = 100ll, .rows = 4u);
    cli_check(editor.top == 39ull, "the top goes no further down than the last line");
    EMBED_CALL(gui_editor_delta.scroll, GuiEditorDeltaCfg, .editor = &editor, .delta = -100ll, .rows = 4u);
    cli_check(editor.top == 0ull, "the top returns to the first line");

    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 320, 80}, .color = GUI_RGB(0u, 0u, 0u));
    EMBED_CALL(gui_editor_place_scale.draw, GuiEditorPlaceScaleCfg, .editor = &editor, .raster = &raster, .glyphs = &glyphs, .palette = palette, .place = (GuiRect){0, 0, 0, 0}, .scale = 100);
    cli_check(!painted(&raster, (GuiRect){0, 0, 320, 80}, GUI_RGB(0u, 0u, 0u)), "an editor of no room draws nothing");

    cli_check(EMBED_CALL(gui_editor_text.text_put, GuiEditorTextCfg, .editor = &editor, .text = "one\ntwo\r\nthree\n", .length = 15ull), "a text put whole is split into lines");
    cli_check((editor.lines == 4ull) && (editor.count == 4u) && (editor.from == 0ull),
              "a text ending in a newline has an empty line last");
    cli_check((editor.line[1].length == 3ull) && (editor.bytes[editor.line[1].text + 2u] == 'o'),
              "a carriage return before the newline is taken off");
    cli_check(editor.line[3].length == 0ull, "the last line is empty");
    cli_check(EMBED_CALL(gui_editor_text.text_put, GuiEditorTextCfg, .editor = &editor, .text = "", .length = 0ull) && (editor.lines == 1ull), "an empty text is one empty line");

    static const char s_frame[] =
        "{\"key\":\"k\",\"seq\":1,\"top\":0,\"lines\":3,\"from\":0,\"rows\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
        "\"cursor\":{\"line\":0,\"col\":3},\"anchor\":{\"line\":0,\"col\":0},"
        "\"pair\":[{\"line\":2,\"col\":2},{\"line\":0,\"col\":0}],\"indent\":{\"tabs\":0,\"size\":2},"
        "\"colors\":[{\"from\":0,\"runs\":[[0,1,3,0],[1,1,7,0],[0,2]]}]}";
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_frame, .size = sizeof(s_frame) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_editor_json_frame.frame_put, GuiEditorJsonFrameCfg, .editor = &editor, .json = &json, .frame = root), "a view's frame is taken whole");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check((editor.lines == 3ull) && (editor.count == 3u) && (editor.from == 0ull) && (editor.top == 0ull),
              "the frame's rows are the lines stored");
    cli_check((editor.indent_tabs == 0) && (editor.indent_size == 2u), "the frame's indentation is kept");
    cli_check(editor.has_cursor && (editor.cursor.line == 0ull) && (editor.cursor.col == 3ull) &&
                  (editor.anchor.col == 0ull),
              "the frame's cursor and anchor are kept");
    const char *between = NULL;
    unsigned long long between_length = 0ull;
    cli_check(EMBED_CALL(gui_editor_from.units_text, GuiEditorFromCfg, .editor_read = &editor, .line = 1ull, .from = 1ull, .to = 7ull, .text_pointer = &between, .length_pointer = &between_length) && (between_length == 6ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = between, .other = "return", .bytes = 6u}) == 0),
              "the bytes between two units of a stored line are read off it");
    cli_check(EMBED_CALL(gui_editor_from.units_text, GuiEditorFromCfg, .editor_read = &editor, .line = 1ull, .from = 8ull, .to = 40ull, .text_pointer = &between, .length_pointer = &between_length) && (between_length == 2ull),
              "units past the line's end are cut to it");
    cli_check(!EMBED_CALL(gui_editor_from.units_text, GuiEditorFromCfg, .editor_read = &editor, .line = 5ull, .from = 0ull, .to = 1ull, .text_pointer = &between, .length_pointer = &between_length),
              "a line not stored has no bytes");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 320, 80}, .color = GUI_RGB(0u, 0u, 0u));
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = place, .color = palette->editor_ground);
    EMBED_CALL(gui_editor_place_scale.draw, GuiEditorPlaceScaleCfg, .editor = &editor, .raster = &raster, .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100);
    const int caret_x = text_x + (int)(3.0f * cell + 0.5f);
    const GuiColor under_caret = raster.pixels[(10u * 320u) + (unsigned int)caret_x];
    const GuiRect caret = EMBED_CALL(gui_editor_place.caret_rect, GuiEditorPlaceCfg, .editor_read = &editor, .glyphs = &glyphs, .place = place, .scale = 100);
    cli_check((caret.x == caret_x) && (caret.width == 2) && (caret.height == GUI_CODE_LINE),
              "the caret's rectangle stands after the third cell");
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_editor_raster.caret_draw, GuiEditorRasterCfg, .editor = &editor, .raster = &raster, .palette = palette, .caret = caret)) && editor.caret_saved &&
                  (raster.pixels[(10u * 320u) + (unsigned int)caret_x] == palette->caret),
              "the caret drawn stands after the third cell");
    editor.caret_ink = 0u;
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_editor_raster.caret_redrawn, GuiEditorRasterCfg, .editor = &editor, .raster = &raster, .palette = palette)) &&
                  (raster.pixels[(10u * 320u) + (unsigned int)caret_x] == under_caret),
              "the caret drawn again at no ink puts the pixels under it back");
    editor.caret_ink = 255u;
    cli_check(raster.pixels[(10u * 320u) + (unsigned int)caret_x] != palette->caret &&
                  !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_editor_raster.caret_redrawn, GuiEditorRasterCfg, .editor = &editor, .raster = &raster, .palette = palette)) &&
                  (raster.pixels[(10u * 320u) + (unsigned int)caret_x] == palette->caret),
              "the caret drawn again at full ink stands again");
    cli_check(raster.pixels[(10u * 320u) + (unsigned int)text_x + 1u] == palette->selection,
              "the selection's ground fills the cells selected");
    cli_check(raster.pixels[(30u * 320u) + (unsigned int)text_x + 1u] == bare,
              "a row outside the selection keeps the ground");
    cli_check(editor.has_pair && (editor.pair[0].line == 2ull) && (editor.pair[0].col == 2ull) &&
                  (editor.pair[1].line == 0ull),
              "the frame's bracket pair is kept");
    cli_check((raster.pixels[(40u * 320u) + (unsigned int)(text_x + (int)(2.0f * cell)) + 1u] == palette->caret) &&
                  (raster.pixels[(0u * 320u) + (unsigned int)text_x + 1u] == palette->caret),
              "each bracket of the pair is boxed in the caret's color");
    cli_check(raster.pixels[(50u * 320u) + (unsigned int)(text_x + (int)(2.0f * cell)) + 2u] == bare,
              "the box is a line around the cell, not a ground");
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    cli_check(EMBED_CALL(gui_editor_place_cell_scale.cell_at, GuiEditorPlaceCellScaleCfg, .editor_read = &editor, .place = place, .cell = cell, .scale = 100, .x = text_x + (int)(2.0f * cell) + 1, .y = 25, .row = &row, .column = &column) &&
                  (row == 1ull) && (column == 2ull),
              "a pixel in the text reads as its row and cell");
    cli_check(!EMBED_CALL(gui_editor_place_cell_scale.cell_at, GuiEditorPlaceCellScaleCfg, .editor_read = &editor, .place = place, .cell = cell, .scale = 100, .x = 2, .y = 25, .row = &row, .column = &column),
              "a pixel in the gutter is outside the text");

    static const char s_folded[] =
        "{\"key\":\"k\",\"seq\":2,\"top\":0,\"lines\":6,\"from\":0,\"rows\":[\"int main\",\"{\",\"}\"],"
        "\"numbers\":[0,1,5],\"folds\":[[1,4,1,1,\"}\"]],"
        "\"cursor\":{\"line\":1,\"col\":1},\"anchor\":{\"line\":1,\"col\":1},\"pair\":null,\"colors\":[]}";
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_folded, .size = sizeof(s_folded) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_editor_json_frame.frame_put, GuiEditorJsonFrameCfg, .editor = &editor, .json = &json, .frame = root), "a frame with a fold closed is taken");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check((editor.count == 3u) && (editor.line[2].number == 5ull),
              "the rows carry their line numbers, the lines the fold hides left out");
    cli_check((editor.folds == 1u) && (editor.fold[0].first == 1ull) && editor.fold[0].closed &&
                  (editor.fold[0].closer_length == 1ull) && (editor.closers[editor.fold[0].closer] == '}'),
              "the frame's fold is kept with its closer's text");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 320, 80}, .color = GUI_RGB(0u, 0u, 0u));
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = place, .color = palette->editor_ground);
    EMBED_CALL(gui_editor_place_scale.draw, GuiEditorPlaceScaleCfg, .editor = &editor, .raster = &raster, .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100);
    cli_check(painted(&raster, (GuiRect){text_x + (int)(2.0f * cell), 20, (int)(2.0f * cell), 20}, palette->current),
              "a closed fold's row ends in the folded mark");
    cli_check(painted(&raster, (GuiRect){text_x - 16, 20, 12, 20}, bare), "the gutter marks the fold closed");
    cli_check(!painted(&raster, (GuiRect){text_x - 16, 40, 12, 20}, bare), "a row that opens no fold has no mark");
    cli_check(EMBED_CALL(gui_editor_place_cell_scale.row_at, GuiEditorPlaceCellScaleCfg, .editor_read = &editor, .place = place, .cell = cell, .scale = 100, .x = 2, .y = 25, .row = &row) && (row == 1ull),
              "a pixel in the gutter reads as its row");
    cli_check(!EMBED_CALL(gui_editor_place_cell_scale.row_at, GuiEditorPlaceCellScaleCfg, .editor_read = &editor, .place = place, .cell = cell, .scale = 100, .x = text_x + 3, .y = 25, .row = &row),
              "a pixel in the text is not the gutter's");

    static const char s_found[] =
        "{\"key\":\"k\",\"seq\":3,\"top\":0,\"lines\":3,\"from\":0,\"rows\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
        "\"cursor\":{\"line\":0,\"col\":6},\"anchor\":{\"line\":0,\"col\":4},\"pair\":null,"
        "\"find\":{\"open\":1,\"focus\":1,\"query\":\"ma\",\"at\":2,\"regex\":0,\"case\":0,\"word\":1,\"count\":1,"
        "\"current\":0},"
        "\"matches\":[[0,4,6]],\"colors\":[]}";
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_found, .size = sizeof(s_found) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_editor_json_frame.frame_put, GuiEditorJsonFrameCfg, .editor = &editor, .json = &json, .frame = root), "a frame with find open is taken");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check(editor.find.open && editor.find.focus && (editor.find.query_length == 2ull) && (editor.find.at == 2ull) &&
                  editor.find.word && (editor.find.count == 1ull) && (editor.find.current == 0ll) &&
                  (editor.matches == 1ull) && (editor.match[0][1] == 4ull),
              "find's bar and the matches are kept");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 320, 80}, .color = GUI_RGB(0u, 0u, 0u));
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = place, .color = palette->editor_ground);
    EMBED_CALL(gui_editor_place_scale.draw, GuiEditorPlaceScaleCfg, .editor = &editor, .raster = &raster, .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100);
    const GuiColor under_match = raster.pixels[(1u * 320u) + (unsigned int)(text_x + (int)(4.0f * cell)) + 1u];
    cli_check((under_match != palette->selection) && (under_match != bare),
              "the current match is drawn in its own ground over the selection");
    cli_check(painted(&raster, (GuiRect){place.x + place.width - 40, place.y + 2, 30, 8}, bare),
              "find's bar is drawn at the editor's top right");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_editor_json_names.free, GuiEditorJsonNamesCfg, .editor = &editor);
    cli_check(editor.count == 0u, "the editor given back holds no lines");
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
