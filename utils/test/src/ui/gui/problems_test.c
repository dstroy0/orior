// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// problems_test.c: the problems pane's check: a file's diagnostics taken from its event, the worst first, a file's
// event come again replacing them and one with none dropping the file, the rows a file and its diagnostics, a file's
// row pressed closing them, a diagnostic's row pressed picking its place, the head pressed picking the check or its
// stop, the pane drawn with each mark in its severity's color, and the shown file's spans underlined in the editor
#include "../../../../../ui/gui/src/parts/explorer.h"
#include "../../../../../ui/gui/src/parts/problems.h"
#include "../cli/cli_check.h"

static const char s_first[] =
    "{\"path\":\"ui/gui/src/"
    "raster.c\",\"items\":[{\"from\":{\"line\":4,\"col\":2},\"to\":{\"line\":4,\"col\":9},\"severity\":2,\"message\":"
    "\"unused\\nmore\"},"
    "{\"from\":{\"line\":0,\"col\":4},\"to\":{\"line\":0,\"col\":8},\"severity\":1,\"message\":\"undeclared\"}]}";
static const char s_second[] = "{\"path\":\"glyphs.c\",\"items\":[{\"from\":{\"line\":2,\"col\":0},\"to\":{\"line\":2,"
                               "\"col\":3},\"severity\":3,\"message\":\"note\"}]}";
static const char s_none[] = "{\"path\":\"ui/gui/src/raster.c\",\"items\":[]}";
static const char s_frame[] =
    "{\"key\":\"k\",\"seq\":1,\"top\":0,\"lines\":3,\"from\":0,\"rows\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
    "\"cursor\":{\"line\":0,\"col\":0},\"anchor\":{\"line\":0,\"col\":0},\"pair\":null,\"indent\":{\"tabs\":0,\"size\":"
    "2},"
    "\"colors\":[]}";

static int taken(GuiProblems *problems, const char *text, unsigned long long size)
{
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = text, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    const int put = EMBED_CALL(gui_problems_json.put, GuiProblemsJsonCfg, .problems_out = problems, .json = &json, .root = root);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return put;
}

// whether any pixel of `rect` is `color`
static int has(const GuiRaster *raster, GuiRect rect, GuiColor color)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] == color)
            {
                return 1;
            }
        }
    }
    return 0;
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
    GuiProblems problems;
    cli_check(EMBED_CALL(cli_problems.init, GuiProblemsCfg, .problems_out = &problems) && (problems.files == 0u) && (EMBED_CALL(gui_problems_row_frame.rows, GuiProblemsRowFrameCfg, .problems = &problems) == 0u),
              "problems made hold none");
    cli_check(taken(&problems, s_first, sizeof(s_first) - 1u), "a file's event is taken");
    cli_check((problems.files == 1u) && (problems.items == 2u) && (problems.errors == 1u),
              "the file has two diagnostics, one an error");
    cli_check((problems.item[0].severity == 1u) && (problems.item[0].from_line == 0ull) &&
                  (problems.item[1].severity == 2u),
              "the diagnostics stand the worst first");
    cli_check((problems.item[1].message_length == 6ull) &&
                  (memor.cmp(&(MemoriaCfg){
                       .src = problems.texts.out + problems.item[1].message, .other = "unused", .bytes = 6u}) == 0),
              "a message is kept to its first line");
    cli_check((problems.file[0].name == 11u) && (problems.file[0].length == 19u),
              "the file's name starts after its last folder");
    cli_check(EMBED_CALL(gui_problems_row_frame.rows, GuiProblemsRowFrameCfg, .problems = &problems) == 3u, "the rows are the file and its two diagnostics");
    unsigned int file = 0u;
    int item = GUI_PROBLEMS_NONE;
    cli_check(EMBED_CALL(cli_problems_row.row, GuiProblemsRowCfg, .problems = &problems, .row = 0u, .file = &file, .item = &item) && (file == 0u) && (item == GUI_PROBLEMS_NONE),
              "the first row is the file's");
    cli_check(EMBED_CALL(cli_problems_row.row, GuiProblemsRowCfg, .problems = &problems, .row = 2u, .file = &file, .item = &item) && (file == 0u) && (item == 1),
              "the third row is the second diagnostic's");
    cli_check(!EMBED_CALL(cli_problems_row.row, GuiProblemsRowCfg, .problems = &problems, .row = 3u, .file = &file, .item = &item), "there is no fourth row");
    cli_check(taken(&problems, s_second, sizeof(s_second) - 1u) && (problems.files == 2u) && (problems.items == 3u) &&
                  (EMBED_CALL(gui_problems_row_frame.rows, GuiProblemsRowFrameCfg, .problems = &problems) == 5u),
              "a second file's event adds its rows");
    unsigned int first = 0u;
    cli_check((EMBED_CALL(gui_problems_path.get, GuiProblemsPathCfg, .problems = &problems, .path = "glyphs.c", .first = &first) == 1u) && (first == 2u) &&
                  (EMBED_CALL(gui_problems_path.get, GuiProblemsPathCfg, .problems = &problems, .path = "none.c", .first = &first) == 0u),
              "a file's diagnostics are found by its path");
    cli_check(taken(&problems, s_first, sizeof(s_first) - 1u) && (problems.files == 2u) && (problems.items == 3u) &&
                  (problems.file[0].first == 0u) && (problems.file[0].count == 1u) && (problems.item[0].file == 0u) &&
                  (problems.file[1].first == 1u) && (problems.item[1].file == 1u),
              "a file's event come again replaces its diagnostics, the other file's moved down");
    cli_check(taken(&problems, s_none, sizeof(s_none) - 1u) && (problems.files == 1u) && (problems.items == 1u) &&
                  (problems.errors == 0u),
              "a file's event with no diagnostics drops the file");
    cli_check(taken(&problems, s_first, sizeof(s_first) - 1u) && (problems.files == 2u) && (problems.items == 3u),
              "the file's event taken again brings it back");

    // the frame's side and the rows in it
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 800u, .height = 500u, .scale = 100);
    EMBED_CALL(gui_frame_shown.side, GuiFrameShownCfg, .frame = &frame, .shown = 1);
    const int top = EMBED_CALL(gui_problems_row_frame.rows_top, GuiProblemsRowFrameCfg, .frame = &frame);
    cli_check(top == frame.side.y + GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_PROBLEMS_HEAD,
              "the rows start under the title and the head");
    const GuiRect second = EMBED_CALL(gui_problems_row_frame.row_rect, GuiProblemsRowFrameCfg, .problems = &problems, .frame = &frame, .row = 1u);
    cli_check((second.y == top + GUI_PROBLEMS_FILE_ROW) && (second.height == GUI_PROBLEMS_ROW),
              "a diagnostic's row stands under its file's taller row");
    unsigned int row = 0u;
    cli_check(EMBED_CALL(gui_problems_row_frame.hit, GuiProblemsRowFrameCfg, .problems = &problems, .frame = &frame, .x = frame.side.x + 20, .y = second.y + 5, .row_pointer = &row) && (row == 1u),
              "the pixel in the second row hits it");
    cli_check(EMBED_CALL(gui_problems_frame.move, GuiProblemsFrameCfg, .problems_out = &problems, .frame = &frame, .x = frame.side.x + 20, .y = second.y + 5) && (problems.pointed == 1),
              "the pointer marks the row it stands on");
    GuiProblemPick pick;
    cli_check(EMBED_CALL(gui_problems_frame.press, GuiProblemsFrameCfg, .problems_out = &problems, .frame = &frame, .x = frame.side.x + 20, .y = second.y + 5, .pick = &pick) &&
                  (pick.kind == GUI_PROBLEMS_PICK_OPEN) && (pick.line == 2ull) && (pick.col == 0ull) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = pick.path, .other = "glyphs.c", .cap = 32ull}),
              "a diagnostic's row pressed picks its place to open");
    const GuiRect head = {frame.side.x, frame.side.y + GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE, frame.side.width,
                          GUI_PROBLEMS_HEAD};
    cli_check(EMBED_CALL(gui_problems_frame.press, GuiProblemsFrameCfg, .problems_out = &problems, .frame = &frame, .x = head.x + 10, .y = head.y + 5, .pick = &pick) &&
                  (pick.kind == GUI_PROBLEMS_PICK_CHECK),
              "the head pressed picks the check");
    EMBED_CALL(cli_problems.checking, GuiProblemsCfg, .problems_out = &problems, .done = 3ull, .total = 10ull);
    cli_check(EMBED_CALL(gui_problems_frame.press, GuiProblemsFrameCfg, .problems_out = &problems, .frame = &frame, .x = head.x + 10, .y = head.y + 5, .pick = &pick) &&
                  (pick.kind == GUI_PROBLEMS_PICK_STOP),
              "the head pressed while the check runs picks its stop");
    EMBED_CALL(cli_problems.checking, GuiProblemsCfg, .problems_out = &problems, .done = 10ull, .total = 10ull);
    const GuiRect first_row = EMBED_CALL(gui_problems_row_frame.row_rect, GuiProblemsRowFrameCfg, .problems = &problems, .frame = &frame, .row = 0u);
    cli_check(EMBED_CALL(gui_problems_frame.press, GuiProblemsFrameCfg, .problems_out = &problems, .frame = &frame, .x = first_row.x + 20, .y = first_row.y + 5, .pick = &pick) &&
                  (pick.kind == GUI_PROBLEMS_PICK_NONE) && problems.file[0].closed &&
                  (EMBED_CALL(gui_problems_row_frame.rows, GuiProblemsRowFrameCfg, .problems = &problems) == 4u),
              "a file's row pressed closes its diagnostics");
    cli_check(EMBED_CALL(gui_problems_frame.press, GuiProblemsFrameCfg, .problems_out = &problems, .frame = &frame, .x = first_row.x + 20, .y = first_row.y + 5, .pick = &pick) &&
                  !problems.file[0].closed && (EMBED_CALL(gui_problems_row_frame.rows, GuiProblemsRowFrameCfg, .problems = &problems) == 5u),
              "pressed again it opens them");
    cli_check(!EMBED_CALL(gui_problems_frame.press, GuiProblemsFrameCfg, .problems_out = &problems, .frame = &frame, .x = frame.editor.x + 50, .y = frame.editor.y + 50, .pick = &pick),
              "a press outside the side is not the pane's");
    cli_check(!EMBED_CALL(gui_problems_row_frame.wheel, GuiProblemsRowFrameCfg, .problems_out = &problems, .frame = &frame, .rows = 3), "the wheel moves nothing while every row is shown");

    // drawn
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 800u, .height = 500u), "a raster is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 800, 500}, .color = palette->ground);
    EMBED_CALL(gui_problems_row_frame.draw, GuiProblemsRowFrameCfg, .problems = &problems, .raster = &raster, .glyphs = &glyphs, .palette = palette, .frame = &frame);
    cli_check(has(&raster, second, palette->hover), "a note's mark is drawn in the hover color");
    const GuiRect fourth = EMBED_CALL(gui_problems_row_frame.row_rect, GuiProblemsRowFrameCfg, .problems = &problems, .frame = &frame, .row = 3u);
    cli_check(has(&raster, fourth, palette->error), "an error's mark is drawn in the error color");
    cli_check(has(&raster, head, palette->link), "the head offers the check in the link color");
    const GuiColor badge = raster.pixels[((unsigned int)(first_row.y + (first_row.height / 2)) * 800u) +
                                         (unsigned int)(first_row.x + first_row.width - GUI_PROBLEMS_PAD_LEFT - 8)];
    cli_check((badge != palette->editor_ground) && (badge != palette->ground), "a file's count stands on its badge");

    // underlined in the editor
    GuiEditor editor;
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &editor);
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_frame, .size = sizeof(s_frame) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_editor_json_frame.frame_put, GuiEditorJsonFrameCfg, .editor = &editor, .json = &json, .frame = root), "a frame is taken");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .size = GUI_CODE_SIZE);
    const GuiRect place = {100, 100, 400, 80};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = place, .color = palette->editor_ground);
    EMBED_CALL(gui_problems_path_raster.underlined, GuiProblemsPathRasterCfg, .problems = &problems, .path = "ui/gui/src/raster.c", .editor = &editor, .raster = &raster, .palette = palette, .place = place, .cell = cell, .scale = 100);
    const int text_x = place.x + EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg, .editor_read = &editor, .cell = cell, .scale = 100);
    cli_check(has(&raster, (GuiRect){text_x + (int)(4.0f * cell), place.y + GUI_CODE_LINE - 2, (int)(4.0f * cell), 1},
                  palette->error),
              "the error's span on the first line is underlined in the error color");
    cli_check(!has(&raster, (GuiRect){text_x, place.y + GUI_CODE_LINE - 2, (int)(3.0f * cell), 1}, palette->error),
              "the cells before the span are not underlined");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = place, .color = palette->editor_ground);
    EMBED_CALL(gui_problems_path_raster.underlined, GuiProblemsPathRasterCfg, .problems = &problems, .path = "glyphs.c", .editor = &editor, .raster = &raster, .palette = palette, .place = place, .cell = cell, .scale = 100);
    cli_check(has(&raster, (GuiRect){text_x, place.y + (3 * GUI_CODE_LINE) - 2, (int)(3.0f * cell), 1}, palette->hover),
              "another file's note underlines its line in the hover color");
    EMBED_CALL(gui_editor_json_names.free, GuiEditorJsonNamesCfg, .editor = &editor);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(cli_problems.free, GuiProblemsCfg, .problems_out = &problems);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
