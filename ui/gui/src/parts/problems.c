// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// problems.c: the problems pane. Each file's diagnostics are copied out of the event's tree into the part's own
// cells, a file replaced whole as its event comes again; the texts the files and diagnostics read are packed again
// into a fresh line as the bytes no longer read outgrow those read
#include "problems.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "../../../cli/src/text/text.h"
#include "../answer.h"
#include "explorer.h"

#include <stdlib.h>

int gui_problems_made(GuiProblems *problems)
{
    memor.set(&(MemoriaCfg){.dst = problems, .val = 0, .bytes = sizeof(*problems)});
    problems->pointed = GUI_PROBLEMS_NONE;
    problems->file =
        g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_FILES * sizeof(GuiProblemFile), &g_gui_problems_cells);
    problems->item = g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_ITEMS * sizeof(GuiProblem), &g_gui_problems_cells);
    if ((problems->file == NULL) || (problems->item == NULL))
    {
        gui_problems_given(problems);
        return 0;
    }
    return 1;
}

void gui_problems_given(GuiProblems *problems)
{
    g_cli_cells->persistent_buf_release(problems->file, &g_gui_problems_cells);
    g_cli_cells->persistent_buf_release(problems->item, &g_gui_problems_cells);
    g_cli_cells->persistent_buf_release(problems->texts.out, &g_gui_problems_cells);
    problems->file = NULL;
    problems->item = NULL;
    memor.set(&(MemoriaCfg){.dst = &problems->texts, .val = 0, .bytes = sizeof(problems->texts)});
    problems->files = 0u;
    problems->items = 0u;
    problems->dead = 0ull;
}

// the texts packed again: each file's path and each diagnostic's message copied into a fresh line, the bytes no
// longer read left behind
static void problems_packed(GuiProblems *problems)
{
    const VerbaLinea old = problems->texts;
    char *const cell = g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_TEXT_ROOM, &g_gui_problems_cells);
    if (cell == NULL)
    {
        return;
    }
    problems->texts = (VerbaLinea){cell, GUI_PROBLEMS_TEXT_ROOM, 0ull};
    for (unsigned int at = 0u; at < problems->files; at += 1u)
    {
        GuiProblemFile *const file = &problems->file[at];
        const unsigned long long start = problems->texts.at;
        mmgr_line_put_n(&problems->texts, old.out + file->path, file->length);
        file->path = start;
    }
    for (unsigned int at = 0u; at < problems->items; at += 1u)
    {
        GuiProblem *const item = &problems->item[at];
        const unsigned long long start = problems->texts.at;
        mmgr_line_put_n(&problems->texts, old.out + item->message, item->message_length);
        item->message = start;
    }
    g_cli_cells->persistent_buf_release(old.out, &g_gui_problems_cells);
    problems->dead = 0ull;
}

// `length` bytes of `text` stored among the texts: where they start into `at`: 1, or 0 where the line is full
static int problems_stored(GuiProblems *problems, const char *text, unsigned long long length, unsigned long long *at)
{
    if (problems->texts.out == NULL)
    {
        char *const cell = g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_TEXT_ROOM, &g_gui_problems_cells);
        problems->texts = mmgr_line_over(cell, GUI_PROBLEMS_TEXT_ROOM);
    }
    *at = problems->texts.at;
    mmgr_line_put_n(&problems->texts, text, length);
    return mmgr_line_ok(&problems->texts);
}

// the file whose path is `length` bytes of `path`, or -1
static int problems_file_of(const GuiProblems *problems, const char *path, unsigned long long length)
{
    for (unsigned int at = 0u; at < problems->files; at += 1u)
    {
        const GuiProblemFile *const file = &problems->file[at];
        if ((file->length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = problems->texts.out + file->path, .other = path, .bytes = length}) == 0))
        {
            // the files are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the file `at` taken out with its diagnostics, the files and diagnostics after it moved down
static void problems_file_removed(GuiProblems *problems, unsigned int at)
{
    const GuiProblemFile file = problems->file[at];
    problems->dead += file.length;
    for (unsigned int each = file.first; each < file.first + file.count; each += 1u)
    {
        problems->dead += problems->item[each].message_length;
    }
    for (unsigned int each = file.first + file.count; each < problems->items; each += 1u)
    {
        problems->item[each - file.count] = problems->item[each];
        problems->item[each - file.count].file -= (problems->item[each].file > at) ? 1u : 0u;
    }
    problems->items -= file.count;
    for (unsigned int each = at + 1u; each < problems->files; each += 1u)
    {
        problems->file[each - 1u] = problems->file[each];
        problems->file[each - 1u].first -= (problems->file[each].first > file.first) ? file.count : 0u;
    }
    problems->files -= 1u;
}

// two diagnostics in the order shown: the worse first, then the earlier
static int problems_order(const void *one, const void *other)
{
    const GuiProblem *const a = (const GuiProblem *)one;
    const GuiProblem *const b = (const GuiProblem *)other;
    if (a->severity != b->severity)
    {
        return (a->severity < b->severity) ? -1 : 1;
    }
    if (a->from_line != b->from_line)
    {
        return (a->from_line < b->from_line) ? -1 : 1;
    }
    if (a->from_col != b->from_col)
    {
        return (a->from_col < b->from_col) ? -1 : 1;
    }
    return 0;
}

// the errors counted again
static void problems_counted(GuiProblems *problems)
{
    problems->errors = 0u;
    for (unsigned int at = 0u; at < problems->items; at += 1u)
    {
        problems->errors += (problems->item[at].severity == GUI_PROBLEMS_ERROR) ? 1u : 0u;
    }
}

int gui_problems_put(GuiProblems *problems, const CliJson *json, unsigned int root)
{
    unsigned long long length = 0ull;
    const char *const path = cli_json_string(json, cli_json_member(json, root, "path"), &length);
    const unsigned int items = cli_json_member(json, root, "items");
    if ((path == NULL) || (length == 0ull) || !cli_json_is(json, items, CFG_JSON_ARRAY))
    {
        return 0;
    }
    const int had = problems_file_of(problems, path, length);
    if (had >= 0)
    {
        // the file is among the files
        problems_file_removed(problems, (unsigned int)had);
    }
    const unsigned int count = json->node[items].count;
    if (count == 0u)
    {
        problems_counted(problems);
        return 1;
    }
    if ((problems->files >= GUI_PROBLEMS_FILES) || (problems->items + count > GUI_PROBLEMS_ITEMS))
    {
        problems_counted(problems);
        return 0;
    }
    if ((problems->texts.out != NULL) && (problems->dead > GUI_PROBLEMS_DEAD_MOST) &&
        (problems->dead * 2ull > problems->texts.at))
    {
        problems_packed(problems);
    }
    GuiProblemFile *const file = &problems->file[problems->files];
    memor.set(&(MemoriaCfg){.dst = file, .val = 0, .bytes = sizeof(*file)});
    if (!problems_stored(problems, path, length, &file->path))
    {
        return 0;
    }
    // the path's length is far inside what an unsigned int represents
    file->length = (unsigned int)length;
    file->name = 0u;
    for (unsigned int at = 0u; at < file->length; at += 1u)
    {
        file->name = ((path[at] == '/') || (path[at] == '\\')) ? at + 1u : file->name;
    }
    file->first = problems->items;
    file->count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int one = cli_json_item(json, items, at);
        const unsigned int from = cli_json_member(json, one, "from");
        const unsigned int to = cli_json_member(json, one, "to");
        unsigned long long message_length = 0ull;
        const char *const message = cli_json_string(json, cli_json_member(json, one, "message"), &message_length);
        unsigned long long first_line = 0ull;
        if (message != NULL)
        {
            first_line = cli_text_to_any(message, 0ull, message_length, "\n");
        }
        GuiProblem *const item = &problems->item[problems->items];
        memor.set(&(MemoriaCfg){.dst = item, .val = 0, .bytes = sizeof(*item)});
        item->file = problems->files;
        item->from_line = gui_answer_member_whole(json, from, "line", 0ull);
        item->from_col = gui_answer_member_whole(json, from, "col", 0ull);
        item->to_line = gui_answer_member_whole(json, to, "line", item->from_line);
        item->to_col = gui_answer_member_whole(json, to, "col", item->from_col);
        // a severity is 1 to 4
        item->severity = (unsigned int)gui_answer_member_whole(json, one, "severity", 1ull);
        if (!problems_stored(problems, (message != NULL) ? message : "", first_line, &item->message))
        {
            return 0;
        }
        // a message's first line is far inside what an unsigned int represents
        item->message_length = (unsigned int)first_line;
        problems->items += 1u;
        file->count += 1u;
    }
    qsort(problems->item + file->first, file->count, sizeof(GuiProblem), problems_order);
    problems->files += 1u;
    problems_counted(problems);
    return 1;
}

void gui_problems_checking(GuiProblems *problems, unsigned long long done, unsigned long long total)
{
    problems->done = done;
    problems->total = total;
}

int gui_problems_running(const GuiProblems *problems)
{
    return (problems->total != 0ull) && (problems->done < problems->total);
}

unsigned int gui_problems_of(const GuiProblems *problems, const char *path, unsigned int *first)
{
    const int at =
        problems_file_of(problems, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}));
    if (at < 0)
    {
        return 0u;
    }
    // the file is among the files
    *first = problems->file[(unsigned int)at].first;
    return problems->file[(unsigned int)at].count;
}

unsigned int gui_problems_rows(const GuiProblems *problems)
{
    unsigned int rows = 0u;
    for (unsigned int at = 0u; at < problems->files; at += 1u)
    {
        rows += 1u + (problems->file[at].closed ? 0u : problems->file[at].count);
    }
    return rows;
}

int gui_problems_row(const GuiProblems *problems, unsigned int row, unsigned int *file, int *item)
{
    unsigned int at = 0u;
    for (unsigned int each = 0u; each < problems->files; each += 1u)
    {
        const GuiProblemFile *const one = &problems->file[each];
        if (row == at)
        {
            *file = each;
            *item = GUI_PROBLEMS_NONE;
            return 1;
        }
        const unsigned int under = one->closed ? 0u : one->count;
        if (row < at + 1u + under)
        {
            *file = each;
            // the diagnostics are fewer than an int counts
            *item = (int)(one->first + (row - at - 1u));
            return 1;
        }
        at += 1u + under;
    }
    return 0;
}

int gui_problems_rows_top(const GuiFrame *frame)
{
    return frame->side.y + gui_scaled(GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_PROBLEMS_HEAD, frame->scale);
}

// the height of the row `row`: a file's or a diagnostic's
static int problems_row_tall(const GuiProblems *problems, const GuiFrame *frame, unsigned int row)
{
    unsigned int file = 0u;
    int item = GUI_PROBLEMS_NONE;
    const int is_file = gui_problems_row(problems, row, &file, &item) && (item == GUI_PROBLEMS_NONE);
    return gui_scaled(is_file ? GUI_PROBLEMS_FILE_ROW : GUI_PROBLEMS_ROW, frame->scale);
}

GuiRect gui_problems_row_rect(const GuiProblems *problems, const GuiFrame *frame, unsigned int row)
{
    int y = gui_problems_rows_top(frame);
    for (unsigned int each = problems->scroll; each < row; each += 1u)
    {
        y += problems_row_tall(problems, frame, each);
    }
    for (unsigned int each = row; each < problems->scroll; each += 1u)
    {
        y -= problems_row_tall(problems, frame, each);
    }
    return (GuiRect){frame->side.x, y, frame->side.width - gui_scaled(1, frame->scale),
                     problems_row_tall(problems, frame, row)};
}

int gui_problems_hit(const GuiProblems *problems, const GuiFrame *frame, int x, int y, unsigned int *row)
{
    if (!gui_rect_holds(frame->side, x, y) || (y < gui_problems_rows_top(frame)))
    {
        return 0;
    }
    const unsigned int rows = gui_problems_rows(problems);
    int top = gui_problems_rows_top(frame);
    for (unsigned int at = problems->scroll; at < rows; at += 1u)
    {
        const int tall = problems_row_tall(problems, frame, at);
        if (y < top + tall)
        {
            *row = at;
            return 1;
        }
        top += tall;
    }
    return 0;
}

int gui_problems_moved(GuiProblems *problems, const GuiFrame *frame, int x, int y)
{
    unsigned int row = 0u;
    // the rows are fewer than an int counts
    const int pointed = gui_problems_hit(problems, frame, x, y, &row) ? (int)row : GUI_PROBLEMS_NONE;
    const int changed = pointed != problems->pointed;
    problems->pointed = pointed;
    return changed;
}

// the pane's head rectangle in the frame's side
static GuiRect problems_head_rect(const GuiFrame *frame)
{
    const int top = frame->side.y + gui_scaled(GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE, frame->scale);
    return (GuiRect){frame->side.x, top, frame->side.width - gui_scaled(1, frame->scale),
                     gui_scaled(GUI_PROBLEMS_HEAD, frame->scale)};
}

int gui_problems_pressed(GuiProblems *problems, const GuiFrame *frame, int x, int y, GuiProblemPick *pick)
{
    pick->kind = GUI_PROBLEMS_PICK_NONE;
    pick->path[0] = '\0';
    pick->line = 0ull;
    pick->col = 0ull;
    if (!gui_rect_holds(frame->side, x, y))
    {
        return 0;
    }
    if (gui_rect_holds(problems_head_rect(frame), x, y))
    {
        pick->kind = gui_problems_running(problems) ? GUI_PROBLEMS_PICK_STOP : GUI_PROBLEMS_PICK_CHECK;
        return 1;
    }
    unsigned int row = 0u;
    unsigned int file = 0u;
    int item = GUI_PROBLEMS_NONE;
    if (!gui_problems_hit(problems, frame, x, y, &row) || !gui_problems_row(problems, row, &file, &item))
    {
        return 1;
    }
    if (item == GUI_PROBLEMS_NONE)
    {
        problems->file[file].closed = !problems->file[file].closed;
        return 1;
    }
    // the diagnostic is among the diagnostics
    const GuiProblem *const one = &problems->item[(unsigned int)item];
    VerbaLinea path = {pick->path, sizeof(pick->path), 0ull};
    mmgr_line_put_n(&path, problems->texts.out + problems->file[file].path, problems->file[file].length);
    (void)mmgr_line_finish(&path);
    pick->kind = GUI_PROBLEMS_PICK_OPEN;
    pick->line = one->from_line;
    pick->col = one->from_col;
    return 1;
}

int gui_problems_wheeled(GuiProblems *problems, const GuiFrame *frame, int rows)
{
    const int tall = gui_scaled(GUI_PROBLEMS_ROW, frame->scale);
    const int room = frame->side.y + frame->side.height - gui_problems_rows_top(frame);
    const int shown = (tall > 0) ? room / tall : 0;
    // the rows are far fewer than an int counts
    const int count = (int)gui_problems_rows(problems);
    const int most = (count > shown) ? count - shown : 0;
    const int wanted = (int)problems->scroll + rows;
    const int clamped = (wanted < 0) ? 0 : ((wanted > most) ? most : wanted);
    // the scroll clamped is not negative
    if ((unsigned int)clamped == problems->scroll)
    {
        return 0;
    }
    problems->scroll = (unsigned int)clamped;
    return 1;
}

// the baseline of text in `face` at `size` centered in a row of `tall` from `top`
static int problems_baseline(GuiGlyphs *glyphs, unsigned int face, unsigned int size, int top, int tall)
{
    const float ascent = gui_glyphs_ascent(glyphs, face, size);
    const float descent = gui_glyphs_descent(glyphs, face, size);
    return top + (int)((((float)tall - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// a file row's arrow, pointing down where its rows are open and right where closed, `side` pixels across at x, y
static void problems_arrow_drawn(GuiRaster *raster, int x, int y, int side, int open, GuiColor color)
{
    const int half = side / 2;
    if (open)
    {
        gui_raster_line(raster, x, y + 1, x + half, y + half + 1, color);
        gui_raster_line(raster, x + half, y + half + 1, x + side - 1, y + 1, color);
    }
    else
    {
        gui_raster_line(raster, x + 1, y, x + half + 1, y + half, color);
        gui_raster_line(raster, x + half + 1, y + half, x + 1, y + side - 1, color);
    }
}

// the color of a severity's mark
static GuiColor problems_color(const GuiPalette *palette, unsigned int severity)
{
    if (severity == GUI_PROBLEMS_ERROR)
    {
        return palette->error;
    }
    return (severity == GUI_PROBLEMS_WARNING) ? palette->warning : palette->hover;
}

// the pane's head drawn: the check's progress and Stop while it runs, else the problems and files counted and the
// check offered
static void problems_head_drawn(const GuiProblems *problems, GuiRaster *raster, GuiGlyphs *glyphs,
                                const GuiPalette *palette, const GuiFrame *frame, GuiRect inside)
{
    const float scale = frame->scale;
    const GuiRect head = problems_head_rect(frame);
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_PROBLEMS_HEAD_SIZE, scale);
    const int baseline = problems_baseline(glyphs, GUI_FACE_HEAD, size, head.y, head.height);
    const int pad = gui_scaled(GUI_PROBLEMS_PAD_LEFT, scale);
    char text[160];
    VerbaLinea line = {text, sizeof(text), 0ull};
    const int running = gui_problems_running(problems);
    if (running)
    {
        mmgr_line_put(&line, "Checking the tree: ");
        mmgr_line_decimal(&line, problems->done, 1u);
        mmgr_line_put(&line, " of ");
        mmgr_line_decimal(&line, problems->total, 1u);
        mmgr_line_put(&line, " files");
    }
    else if (problems->items > 0u)
    {
        mmgr_line_decimal(&line, problems->items, 1u);
        mmgr_line_put(&line, (problems->items == 1u) ? " problem in " : " problems in ");
        mmgr_line_decimal(&line, problems->files, 1u);
        mmgr_line_put(&line, (problems->files == 1u) ? " file" : " files");
    }
    else
    {
        mmgr_line_put(&line, "No problems in the tree");
    }
    (void)mmgr_line_finish(&line);
    (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)(head.x + pad), baseline, text, line.at,
                         palette->text_2, inside);
    const char *const action = running ? "Stop" : "Check the tree";
    const unsigned long long action_length = cellul.len(&(CatenaFinitaCfg){.src = action, .cap = 32ull});
    const float action_wide = gui_text_width(glyphs, GUI_FACE_HEAD, size, action, action_length);
    (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)(head.x + head.width - pad) - action_wide,
                         baseline, action, action_length, palette->link, inside);
}

void gui_problems_drawn(const GuiProblems *problems, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                        const GuiFrame *frame)
{
    const float scale = frame->scale;
    const GuiRect side = frame->side;
    if (gui_rect_empty(side))
    {
        return;
    }
    const int one = gui_scaled(1, scale);
    const GuiRect inside = {side.x, side.y, side.width - one, side.height};
    const int line_y = side.y + gui_scaled(GUI_EXPLORER_HEAD, scale);
    gui_raster_filled(raster, gui_rect_met((GuiRect){side.x, line_y, inside.width, one}, inside), palette->line);
    problems_head_drawn(problems, raster, glyphs, palette, frame, inside);
    const int rows_top = gui_problems_rows_top(frame);
    const GuiRect body =
        gui_rect_met((GuiRect){side.x, rows_top, inside.width, side.y + side.height - rows_top}, inside);
    if (gui_rect_empty(body))
    {
        return;
    }
    gui_raster_filled(raster, body, palette->editor_ground);
    const unsigned int name_size = (unsigned int)gui_scaled((int)GUI_PROBLEMS_NAME_SIZE, scale);
    const unsigned int where_size = (unsigned int)gui_scaled((int)GUI_PROBLEMS_WHERE_SIZE, scale);
    const unsigned int message_size = (unsigned int)gui_scaled((int)GUI_PROBLEMS_MESSAGE_SIZE, scale);
    const unsigned int count_size = (unsigned int)gui_scaled((int)GUI_PROBLEMS_COUNT_SIZE, scale);
    const int pad = gui_scaled(GUI_PROBLEMS_PAD_LEFT, scale);
    const int gap = gui_scaled(GUI_PROBLEMS_GAP, scale);
    const int guide = gui_scaled(GUI_PROBLEMS_GUIDE, scale);
    const int mark = gui_scaled(GUI_PROBLEMS_MARK, scale);
    const int arrow = gui_scaled(GUI_EXPLORER_ARROW, scale);
    const unsigned int rows = gui_problems_rows(problems);
    for (unsigned int row = problems->scroll; row < rows; row += 1u)
    {
        const GuiRect rect = gui_problems_row_rect(problems, frame, row);
        if (rect.y >= body.y + body.height)
        {
            break;
        }
        const GuiRect on = gui_rect_met(rect, body);
        // the rows are fewer than an int counts
        if (problems->pointed == (int)row)
        {
            gui_raster_filled(raster, on, palette->bar_title_hover);
        }
        unsigned int at = 0u;
        int item = GUI_PROBLEMS_NONE;
        if (!gui_problems_row(problems, row, &at, &item))
        {
            break;
        }
        const GuiProblemFile *const file = &problems->file[at];
        if (item == GUI_PROBLEMS_NONE)
        {
            // a file's row: the arrow, the name, the folder after it quieter, and the count at the right
            problems_arrow_drawn(raster, rect.x + pad, rect.y + ((rect.height - arrow) / 2), arrow, !file->closed,
                                 palette->text);
            const int baseline = problems_baseline(glyphs, GUI_FACE_HEAD, name_size, rect.y, rect.height);
            char digits[24];
            VerbaLinea count = {digits, sizeof(digits), 0ull};
            mmgr_line_decimal(&count, file->count, 1u);
            const int count_pad = gui_scaled(GUI_PROBLEMS_COUNT_PAD, scale);
            const int count_wide =
                (int)(gui_text_width(glyphs, GUI_FACE_CODE, count_size, digits, count.at) + 0.5f) + (2 * count_pad);
            const int count_tall = gui_scaled(GUI_PROBLEMS_FILE_ROW - 8, scale);
            const GuiRect badge = {rect.x + rect.width - pad - count_wide, rect.y + ((rect.height - count_tall) / 2),
                                   count_wide, count_tall};
            gui_raster_rounded(raster, gui_rect_met(badge, on),
                               (unsigned int)gui_scaled(GUI_PROBLEMS_COUNT_RADIUS, scale), palette->line);
            (void)gui_text_drawn(raster, glyphs, GUI_FACE_CODE, count_size, (float)(badge.x + count_pad),
                                 problems_baseline(glyphs, GUI_FACE_CODE, count_size, badge.y, badge.height), digits,
                                 count.at, palette->text_2, on);
            const GuiRect text_clip = gui_rect_met((GuiRect){rect.x, rect.y, badge.x - gap - rect.x, rect.height}, on);
            const char *const path = problems->texts.out + file->path;
            float x = (float)(rect.x + pad + arrow + gap);
            x = gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, name_size, x, baseline, path + file->name,
                               file->length - file->name, palette->text, text_clip);
            if (file->name > 1u)
            {
                (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, where_size, x + (float)gap, baseline, path,
                                     file->name - 1u, palette->text_3, text_clip);
            }
            continue;
        }
        // a diagnostic's row: the guide, the mark in its severity's color, the message and its place at the right
        // the diagnostic is among the diagnostics
        const GuiProblem *const one = &problems->item[(unsigned int)item];
        gui_raster_filled(
            raster, gui_rect_met((GuiRect){rect.x + pad + (arrow / 2), rect.y, gui_scaled(1, scale), rect.height}, on),
            palette->line);
        const int mark_x = rect.x + pad + guide;
        gui_raster_rounded(raster, gui_rect_met((GuiRect){mark_x, rect.y + ((rect.height - mark) / 2), mark, mark}, on),
                           (unsigned int)(mark / 2), problems_color(palette, one->severity));
        char where_text[48];
        VerbaLinea where = {where_text, sizeof(where_text), 0ull};
        mmgr_line_decimal(&where, one->from_line + 1ull, 1u);
        mmgr_line_ch(&where, ':');
        mmgr_line_decimal(&where, one->from_col + 1ull, 1u);
        const float where_wide = gui_text_width(glyphs, GUI_FACE_CODE, where_size, where_text, where.at);
        const int baseline = problems_baseline(glyphs, GUI_FACE_HEAD, message_size, rect.y, rect.height);
        const float where_x = (float)(rect.x + rect.width - pad) - where_wide;
        (void)gui_text_drawn(raster, glyphs, GUI_FACE_CODE, where_size, where_x, baseline, where_text, where.at,
                             palette->text_3, on);
        const GuiRect text_clip = gui_rect_met((GuiRect){rect.x, rect.y, (int)where_x - gap - rect.x, rect.height}, on);
        (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, message_size, (float)(mark_x + mark + gap), baseline,
                             problems->texts.out + one->message, one->message_length, palette->text, text_clip);
    }
}

void gui_problems_underlined(const GuiProblems *problems, const char *path, const GuiEditor *editor, GuiRaster *raster,
                             const GuiPalette *palette, GuiRect place, float cell, float scale)
{
    unsigned int first = 0u;
    const unsigned int count = gui_problems_of(problems, path, &first);
    const int thick = gui_scaled(1, scale);
    const int up = gui_scaled(2, scale);
    for (unsigned int at = first; at < first + count; at += 1u)
    {
        const GuiProblem *const one = &problems->item[at];
        const GuiColor color = problems_color(palette, one->severity);
        for (unsigned long long line = one->from_line; line <= one->to_line; line += 1ull)
        {
            const unsigned long long from = (line == one->from_line) ? one->from_col : 0ull;
            const unsigned long long to = (line == one->to_line) ? one->to_col : ~0ull;
            GuiRect start;
            GuiRect end;
            if (!gui_editor_place_rect(editor, place, cell, scale, line, from, &start) ||
                !gui_editor_place_rect(editor, place, cell, scale, line, to, &end))
            {
                continue;
            }
            // a span of no width underlines its one cell
            const int right = (end.x > start.x) ? end.x : start.x + start.width;
            const GuiRect under = {start.x, start.y + start.height - up, right - start.x, thick};
            gui_raster_filled(raster, gui_rect_met(under, place), color);
        }
    }
}
