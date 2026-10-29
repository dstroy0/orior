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
    "{\"path\":\"ui/gui/src/raster.c\",\"items\":[{\"from\":{\"line\":4,\"col\":2},\"to\":{\"line\":4,\"col\":9},\"severity\":2,\"message\":\"unused\\nmore\"},"
    "{\"from\":{\"line\":0,\"col\":4},\"to\":{\"line\":0,\"col\":8},\"severity\":1,\"message\":\"undeclared\"}]}";
static const char s_second[] =
    "{\"path\":\"glyphs.c\",\"items\":[{\"from\":{\"line\":2,\"col\":0},\"to\":{\"line\":2,\"col\":3},\"severity\":3,\"message\":\"note\"}]}";
static const char s_none[] = "{\"path\":\"ui/gui/src/raster.c\",\"items\":[]}";
static const char s_frame[] = "{\"key\":\"k\",\"seq\":1,\"top\":0,\"lines\":3,\"from\":0,\"rows\":[\"int main\",\"\\treturn 0;\",\"// end\"],"
                              "\"cursor\":{\"line\":0,\"col\":0},\"anchor\":{\"line\":0,\"col\":0},\"pair\":null,\"indent\":{\"tabs\":0,\"size\":2},"
                              "\"colors\":[]}";

static int taken(GuiProblems *problems, const char *text, unsigned long long size)
{
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = cli_json_taken(&json, text, size, 0u, 0ull, &err_msg);
    const int put = gui_problems_put(problems, &json, root);
    cli_json_given(&json);
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
    GuiProblems problems;
    cli_check(gui_problems_made(&problems) && (problems.files == 0u) && (gui_problems_rows(&problems) == 0u), "problems made hold none");
    cli_check(taken(&problems, s_first, sizeof(s_first) - 1u), "a file's event is taken");
    cli_check((problems.files == 1u) && (problems.items == 2u) && (problems.errors == 1u), "the file has two diagnostics, one an error");
    cli_check((problems.item[0].severity == 1u) && (problems.item[0].from_line == 0ull) && (problems.item[1].severity == 2u),
              "the diagnostics stand the worst first");
    cli_check((problems.item[1].message_length == 6ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = problems.texts.out + problems.item[1].message, .other = "unused", .bytes = 6u}) == 0),
              "a message is kept to its first line");
    cli_check((problems.file[0].name == 11u) && (problems.file[0].length == 19u), "the file's name starts after its last folder");
    cli_check(gui_problems_rows(&problems) == 3u, "the rows are the file and its two diagnostics");
    unsigned int file = 0u;
    int item = GUI_PROBLEMS_NONE;
    cli_check(gui_problems_row(&problems, 0u, &file, &item) && (file == 0u) && (item == GUI_PROBLEMS_NONE), "the first row is the file's");
    cli_check(gui_problems_row(&problems, 2u, &file, &item) && (file == 0u) && (item == 1), "the third row is the second diagnostic's");
    cli_check(!gui_problems_row(&problems, 3u, &file, &item), "there is no fourth row");
    cli_check(taken(&problems, s_second, sizeof(s_second) - 1u) && (problems.files == 2u) && (problems.items == 3u) &&
                  (gui_problems_rows(&problems) == 5u),
              "a second file's event adds its rows");
    unsigned int first = 0u;
    cli_check((gui_problems_of(&problems, "glyphs.c", &first) == 1u) && (first == 2u) && (gui_problems_of(&problems, "none.c", &first) == 0u),
              "a file's diagnostics are found by its path");
    cli_check(taken(&problems, s_first, sizeof(s_first) - 1u) && (problems.files == 2u) && (problems.items == 3u) &&
                  (problems.file[0].first == 0u) && (problems.file[0].count == 1u) && (problems.item[0].file == 0u) &&
                  (problems.file[1].first == 1u) && (problems.item[1].file == 1u),
              "a file's event come again replaces its diagnostics, the other file's moved down");
    cli_check(taken(&problems, s_none, sizeof(s_none) - 1u) && (problems.files == 1u) && (problems.items == 1u) && (problems.errors == 0u),
              "a file's event with no diagnostics drops the file");
    cli_check(taken(&problems, s_first, sizeof(s_first) - 1u) && (problems.files == 2u) && (problems.items == 3u), "the file's event taken again brings it back");

    // the frame's side and the rows in it
    GuiFrame frame;
    gui_frame_laid(&frame, 800u, 500u, 1.0f);
    gui_frame_sided(&frame, 1);
    const int top = gui_problems_rows_top(&frame);
    cli_check(top == frame.side.y + GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_PROBLEMS_HEAD, "the rows start under the title and the head");
    const GuiRect second = gui_problems_row_rect(&problems, &frame, 1u);
    cli_check((second.y == top + GUI_PROBLEMS_FILE_ROW) && (second.height == GUI_PROBLEMS_ROW), "a diagnostic's row stands under its file's taller row");
    unsigned int row = 0u;
    cli_check(gui_problems_hit(&problems, &frame, frame.side.x + 20, second.y + 5, &row) && (row == 1u), "the pixel in the second row hits it");
    cli_check(gui_problems_moved(&problems, &frame, frame.side.x + 20, second.y + 5) && (problems.pointed == 1), "the pointer marks the row it stands on");
    GuiProblemPick pick;
    cli_check(gui_problems_pressed(&problems, &frame, frame.side.x + 20, second.y + 5, &pick) && (pick.kind == GUI_PROBLEMS_PICK_OPEN) &&
                  (pick.line == 2ull) && (pick.col == 0ull) && cellul.eq(&(CatenaFinitaCfg){.src = pick.path, .other = "glyphs.c", .cap = 32ull}),
              "a diagnostic's row pressed picks its place to open");
    const GuiRect head = {frame.side.x, frame.side.y + GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE, frame.side.width, GUI_PROBLEMS_HEAD};
    cli_check(gui_problems_pressed(&problems, &frame, head.x + 10, head.y + 5, &pick) && (pick.kind == GUI_PROBLEMS_PICK_CHECK),
              "the head pressed picks the check");
    gui_problems_checking(&problems, 3ull, 10ull);
    cli_check(gui_problems_pressed(&problems, &frame, head.x + 10, head.y + 5, &pick) && (pick.kind == GUI_PROBLEMS_PICK_STOP),
              "the head pressed while the check runs picks its stop");
    gui_problems_checking(&problems, 10ull, 10ull);
    const GuiRect first_row = gui_problems_row_rect(&problems, &frame, 0u);
    cli_check(gui_problems_pressed(&problems, &frame, first_row.x + 20, first_row.y + 5, &pick) && (pick.kind == GUI_PROBLEMS_PICK_NONE) &&
                  problems.file[0].closed && (gui_problems_rows(&problems) == 4u),
              "a file's row pressed closes its diagnostics");
    cli_check(gui_problems_pressed(&problems, &frame, first_row.x + 20, first_row.y + 5, &pick) && !problems.file[0].closed &&
                  (gui_problems_rows(&problems) == 5u),
              "pressed again it opens them");
    cli_check(!gui_problems_pressed(&problems, &frame, frame.editor.x + 50, frame.editor.y + 50, &pick), "a press outside the side is not the pane's");
    cli_check(!gui_problems_wheeled(&problems, &frame, 3), "the wheel moves nothing while every row is shown");

    // drawn
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 800u, 500u), "a raster is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 800, 500}, palette->ground);
    gui_problems_drawn(&problems, &raster, &glyphs, palette, &frame);
    cli_check(has(&raster, second, palette->hover), "a note's mark is drawn in the hover color");
    const GuiRect fourth = gui_problems_row_rect(&problems, &frame, 3u);
    cli_check(has(&raster, fourth, palette->error), "an error's mark is drawn in the error color");
    cli_check(has(&raster, head, palette->link), "the head offers the check in the link color");
    const GuiColor badge = raster.pixels[((unsigned int)(first_row.y + (first_row.height / 2)) * 800u) +
                                         (unsigned int)(first_row.x + first_row.width - GUI_PROBLEMS_PAD_LEFT - 8)];
    cli_check((badge != palette->editor_ground) && (badge != palette->ground), "a file's count stands on its badge");

    // underlined in the editor
    GuiEditor editor;
    gui_editor_made(&editor);
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = cli_json_taken(&json, s_frame, sizeof(s_frame) - 1u, 0u, 0ull, &err_msg);
    cli_check(gui_editor_frame_put(&editor, &json, root), "a frame is taken");
    cli_json_given(&json);
    const float cell = gui_glyphs_cell(&glyphs, GUI_CODE_SIZE);
    const GuiRect place = {100, 100, 400, 80};
    gui_raster_filled(&raster, place, palette->editor_ground);
    gui_problems_underlined(&problems, "ui/gui/src/raster.c", &editor, &raster, palette, place, cell, 1.0f);
    const int text_x = place.x + gui_editor_gutter(&editor, cell, 1.0f);
    cli_check(has(&raster, (GuiRect){text_x + (int)(4.0f * cell), place.y + GUI_CODE_LINE - 2, (int)(4.0f * cell), 1}, palette->error),
              "the error's span on the first line is underlined in the error color");
    cli_check(!has(&raster, (GuiRect){text_x, place.y + GUI_CODE_LINE - 2, (int)(3.0f * cell), 1}, palette->error),
              "the cells before the span are not underlined");
    gui_raster_filled(&raster, place, palette->editor_ground);
    gui_problems_underlined(&problems, "glyphs.c", &editor, &raster, palette, place, cell, 1.0f);
    cli_check(has(&raster, (GuiRect){text_x, place.y + (3 * GUI_CODE_LINE) - 2, (int)(3.0f * cell), 1}, palette->hover),
              "another file's note underlines its line in the hover color");
    gui_editor_given(&editor);
    gui_raster_given(&raster);
    gui_problems_given(&problems);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
