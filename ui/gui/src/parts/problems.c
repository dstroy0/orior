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

int gui_problems_init(const GuiProblemsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->problems_out, .val = 0, .bytes = sizeof(*args->problems_out)});
    args->problems_out->pointed = GUI_PROBLEMS_NONE;
    args->problems_out->file =
        g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_FILES * sizeof(GuiProblemFile), &g_gui_problems_cells);
    args->problems_out->item =
        g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_ITEMS * sizeof(GuiProblem), &g_gui_problems_cells);
    if ((args->problems_out->file == NULL) || (args->problems_out->item == NULL))
    {
        cli_problems.free(args);
        return 0;
    }
    return 1;
}

void gui_problems_free(const GuiProblemsCfg *args)
{
    g_cli_cells->persistent_buf_release(args->problems_out->file, &g_gui_problems_cells);
    g_cli_cells->persistent_buf_release(args->problems_out->item, &g_gui_problems_cells);
    g_cli_cells->persistent_buf_release(args->problems_out->texts.out, &g_gui_problems_cells);
    args->problems_out->file = NULL;
    args->problems_out->item = NULL;
    memor.set(&(MemoriaCfg){.dst = &args->problems_out->texts, .val = 0, .bytes = sizeof(args->problems_out->texts)});
    args->problems_out->files = 0u;
    args->problems_out->items = 0u;
    args->problems_out->dead = 0ull;
}

typedef struct
{
    GuiProblems *problems;
    const char *text;
    unsigned long long length;
    unsigned long long *at;
    const GuiProblems *problems_read;
    const char *path;
    unsigned int at_u32;
    const GuiFrame *frame;
    unsigned int row;
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int size;
    int top;
    int tall;
    GuiRaster *raster;
    int x;
    int y;
    int side;
    int open;
    GuiColor color;
    const GuiPalette *palette;
    unsigned int severity;
    GuiRect inside;
} ProblemsCtx;
// the texts packed again: each file's path and each diagnostic's message copied into a fresh line, the bytes no
// longer read left behind
EMBED_INLINE void problems_pack(const ProblemsCtx *args)
{
    const VerbaLinea old = args->problems->texts;
    char *const cell = g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_TEXT_ROOM, &g_gui_problems_cells);
    if (cell == NULL)
    {
        return;
    }
    args->problems->texts = (VerbaLinea){cell, GUI_PROBLEMS_TEXT_ROOM, 0ull};
    for (unsigned int at = 0u; at < args->problems->files; at += 1u)
    {
        GuiProblemFile *const file = &args->problems->file[at];
        const unsigned long long start = args->problems->texts.at;
        mmgr_line_put_n(&args->problems->texts, old.out + file->path, file->length);
        file->path = start;
    }
    for (unsigned int at = 0u; at < args->problems->items; at += 1u)
    {
        GuiProblem *const item = &args->problems->item[at];
        const unsigned long long start = args->problems->texts.at;
        mmgr_line_put_n(&args->problems->texts, old.out + item->message, item->message_length);
        item->message = start;
    }
    g_cli_cells->persistent_buf_release(old.out, &g_gui_problems_cells);
    args->problems->dead = 0ull;
}

// `length` bytes of `text` stored among the texts: where they start into `at`: 1, or 0 where the line is full
EMBED_INLINE int problems_store(const ProblemsCtx *args)
{
    unsigned long long *at = args->at;
    if (args->problems->texts.out == NULL)
    {
        char *const cell = g_cli_cells->persistent_buf_alloc(GUI_PROBLEMS_TEXT_ROOM, &g_gui_problems_cells);
        args->problems->texts = mmgr_line_over(cell, GUI_PROBLEMS_TEXT_ROOM);
    }
    *at = args->problems->texts.at;
    mmgr_line_put_n(&args->problems->texts, args->text, args->length);
    return mmgr_line_ok(&args->problems->texts);
}

// the file whose path is `length` bytes of `path`, or -1
EMBED_INLINE int problems_file_get(const ProblemsCtx *args)
{
    for (unsigned int at = 0u; at < args->problems_read->files; at += 1u)
    {
        const GuiProblemFile *const file = &args->problems_read->file[at];
        if ((file->length == args->length) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->problems_read->texts.out + file->path, .other = args->path, .bytes = args->length}) == 0))
        {
            // the files are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the file `at` taken out with its diagnostics, the files and diagnostics after it moved down
EMBED_INLINE void problems_file_remove(const ProblemsCtx *args)
{
    const GuiProblemFile file = args->problems->file[args->at_u32];
    args->problems->dead += file.length;
    for (unsigned int each = file.first; each < file.first + file.count; each += 1u)
    {
        args->problems->dead += args->problems->item[each].message_length;
    }
    for (unsigned int each = file.first + file.count; each < args->problems->items; each += 1u)
    {
        args->problems->item[each - file.count] = args->problems->item[each];
        args->problems->item[each - file.count].file -= (args->problems->item[each].file > args->at_u32) ? 1u : 0u;
    }
    args->problems->items -= file.count;
    for (unsigned int each = args->at_u32 + 1u; each < args->problems->files; each += 1u)
    {
        args->problems->file[each - 1u] = args->problems->file[each];
        args->problems->file[each - 1u].first -= (args->problems->file[each].first > file.first) ? file.count : 0u;
    }
    args->problems->files -= 1u;
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
EMBED_INLINE void problems_count(const ProblemsCtx *args)
{
    args->problems->errors = 0u;
    for (unsigned int at = 0u; at < args->problems->items; at += 1u)
    {
        args->problems->errors += (args->problems->item[at].severity == GUI_PROBLEMS_ERROR) ? 1u : 0u;
    }
}

int gui_problems_put(const GuiProblemsJsonCfg *args)
{
    const CliJson *json = args->json;
    unsigned long long length = 0ull;
    const char *const path =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "path"),
                   .length_pointer = &length);
    const unsigned int items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "items");
    if ((path == NULL) || (length == 0ull) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = items,
                    .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    const int had =
        EMBED_CALL(problems_file_get, ProblemsCtx, .problems_read = args->problems_out, .path = path, .length = length);
    if (had >= 0)
    {
        // the file is among the files
        EMBED_CALL(problems_file_remove, ProblemsCtx, .problems = args->problems_out, .at_u32 = (unsigned int)had);
    }
    const unsigned int count = json->node[items].count;
    if (count == 0u)
    {
        EMBED_CALL(problems_count, ProblemsCtx, .problems = args->problems_out);
        return 1;
    }
    if ((args->problems_out->files >= GUI_PROBLEMS_FILES) || (args->problems_out->items + count > GUI_PROBLEMS_ITEMS))
    {
        EMBED_CALL(problems_count, ProblemsCtx, .problems = args->problems_out);
        return 0;
    }
    if ((args->problems_out->texts.out != NULL) && (args->problems_out->dead > GUI_PROBLEMS_DEAD_MOST) &&
        (args->problems_out->dead * 2ull > args->problems_out->texts.at))
    {
        EMBED_CALL(problems_pack, ProblemsCtx, .problems = args->problems_out);
    }
    GuiProblemFile *const file = &args->problems_out->file[args->problems_out->files];
    memor.set(&(MemoriaCfg){.dst = file, .val = 0, .bytes = sizeof(*file)});
    if (!EMBED_CALL(problems_store, ProblemsCtx, .problems = args->problems_out, .text = path, .length = length,
                    .at = &file->path))
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
    file->first = args->problems_out->items;
    file->count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = items, .index = at);
        const unsigned int from = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = one, .name = "from");
        const unsigned int to = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = one, .name = "to");
        unsigned long long message_length = 0ull;
        const char *const message =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "message"),
                       .length_pointer = &message_length);
        unsigned long long first_line = 0ull;
        if (message != NULL)
        {
            first_line = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = message,
                                    .at = 0ull, .size = message_length, .set = "\n");
        }
        GuiProblem *const item = &args->problems_out->item[args->problems_out->items];
        memor.set(&(MemoriaCfg){.dst = item, .val = 0, .bytes = sizeof(*item)});
        item->file = args->problems_out->files;
        item->from_line = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = from,
                                     .name = "line", .otherwise = 0ull);
        item->from_col = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = from,
                                    .name = "col", .otherwise = 0ull);
        item->to_line = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = to,
                                   .name = "line", .otherwise = item->from_line);
        item->to_col = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = to,
                                  .name = "col", .otherwise = item->from_col);
        // a severity is 1 to 4
        item->severity = (unsigned int)EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                                  .object = one, .name = "severity", .otherwise = 1ull);
        if (!EMBED_CALL(problems_store, ProblemsCtx, .problems = args->problems_out,
                        .text = (message != NULL) ? message : "", .length = first_line, .at = &item->message))
        {
            return 0;
        }
        // a message's first line is far inside what an unsigned int represents
        item->message_length = (unsigned int)first_line;
        args->problems_out->items += 1u;
        file->count += 1u;
    }
    qsort(args->problems_out->item + file->first, file->count, sizeof(GuiProblem), problems_order);
    args->problems_out->files += 1u;
    EMBED_CALL(problems_count, ProblemsCtx, .problems = args->problems_out);
    return 1;
}

void gui_problems_checking(const GuiProblemsCfg *args)
{
    unsigned long long done = args->done;
    unsigned long long total = args->total;
    args->problems_out->done = done;
    args->problems_out->total = total;
}

int gui_problems_running(const GuiProblemsPathCfg *args)
{
    return (args->problems->total != 0ull) && (args->problems->done < args->problems->total);
}

unsigned int gui_problems_get(const GuiProblemsPathCfg *args)
{
    unsigned int *first = args->first;
    const int at = EMBED_CALL(problems_file_get, ProblemsCtx, .problems_read = args->problems, .path = args->path,
                              .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN}));
    if (at < 0)
    {
        return 0u;
    }
    // the file is among the files
    *first = args->problems->file[(unsigned int)at].first;
    return args->problems->file[(unsigned int)at].count;
}

unsigned int gui_problems_rows(const GuiProblemsRowFrameCfg *args)
{
    unsigned int rows = 0u;
    for (unsigned int at = 0u; at < args->problems->files; at += 1u)
    {
        rows += 1u + (args->problems->file[at].closed ? 0u : args->problems->file[at].count);
    }
    return rows;
}

int gui_problems_row(const GuiProblemsRowCfg *args)
{
    unsigned int *file = args->file;
    int *item = args->item;
    unsigned int at = 0u;
    for (unsigned int each = 0u; each < args->problems->files; each += 1u)
    {
        const GuiProblemFile *const one = &args->problems->file[each];
        if (args->row == at)
        {
            *file = each;
            *item = GUI_PROBLEMS_NONE;
            return 1;
        }
        const unsigned int under = one->closed ? 0u : one->count;
        if (args->row < at + 1u + under)
        {
            *file = each;
            // the diagnostics are fewer than an int counts
            *item = (int)(one->first + (args->row - at - 1u));
            return 1;
        }
        at += 1u + under;
    }
    return 0;
}

int gui_problems_rows_top(const GuiProblemsRowFrameCfg *args)
{
    return args->frame->side.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                            .pixels = GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_PROBLEMS_HEAD,
                                            .scale = args->frame->scale);
}

// the height of the row `row`: a file's or a diagnostic's
EMBED_INLINE int problems_row_tall(const ProblemsCtx *args)
{
    const GuiProblems *problems = args->problems_read;
    unsigned int row = args->row;
    unsigned int file = 0u;
    int item = GUI_PROBLEMS_NONE;
    const int is_file = EMBED_CALL(cli_problems_row.row, GuiProblemsRowCfg, .problems = problems, .row = row,
                                   .file = &file, .item = &item) &&
                        (item == GUI_PROBLEMS_NONE);
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                      .pixels = is_file ? GUI_PROBLEMS_FILE_ROW : GUI_PROBLEMS_ROW, .scale = args->frame->scale);
}

GuiRect gui_problems_row_rect(const GuiProblemsRowFrameCfg *args)
{
    int y = gui_problems_row_frame.rows_top(args);
    for (unsigned int each = args->problems->scroll; each < args->row; each += 1u)
    {
        y += EMBED_CALL(problems_row_tall, ProblemsCtx, .problems_read = args->problems, .frame = args->frame,
                        .row = each);
    }
    for (unsigned int each = args->row; each < args->problems->scroll; each += 1u)
    {
        y -= EMBED_CALL(problems_row_tall, ProblemsCtx, .problems_read = args->problems, .frame = args->frame,
                        .row = each);
    }
    return (GuiRect){args->frame->side.x, y,
                     args->frame->side.width - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                          .scale = args->frame->scale),
                     EMBED_CALL(problems_row_tall, ProblemsCtx, .problems_read = args->problems, .frame = args->frame,
                                .row = args->row)};
}

int gui_problems_hit(const GuiProblemsRowFrameCfg *args)
{
    unsigned int *row = args->row_pointer;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side, .x = args->x,
                    .y = args->y) ||
        (args->y < gui_problems_row_frame.rows_top(args)))
    {
        return 0;
    }
    const unsigned int rows = gui_problems_row_frame.rows(args);
    int top = gui_problems_row_frame.rows_top(args);
    for (unsigned int at = args->problems->scroll; at < rows; at += 1u)
    {
        const int tall = EMBED_CALL(problems_row_tall, ProblemsCtx, .problems_read = args->problems,
                                    .frame = args->frame, .row = at);
        if (args->y < top + tall)
        {
            *row = at;
            return 1;
        }
        top += tall;
    }
    return 0;
}

int gui_problems_move(const GuiProblemsFrameCfg *args)
{
    unsigned int row = 0u;
    // the rows are fewer than an int counts
    const int pointed = EMBED_CALL(gui_problems_row_frame.hit, GuiProblemsRowFrameCfg, .problems = args->problems_out,
                                   .frame = args->frame, .x = args->x, .y = args->y, .row_pointer = &row)
                            ? (int)row
                            : GUI_PROBLEMS_NONE;
    const int changed = pointed != args->problems_out->pointed;
    args->problems_out->pointed = pointed;
    return changed;
}

// the pane's head rectangle in the frame's side
EMBED_INLINE GuiRect problems_head_rect(const ProblemsCtx *args)
{
    const int top = args->frame->side.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                     .pixels = GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE,
                                                     .scale = args->frame->scale);
    return (GuiRect){args->frame->side.x, top,
                     args->frame->side.width - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                          .scale = args->frame->scale),
                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_HEAD,
                                .scale = args->frame->scale)};
}

int gui_problems_press(const GuiProblemsFrameCfg *args)
{
    args->pick->kind = GUI_PROBLEMS_PICK_NONE;
    args->pick->path[0] = '\0';
    args->pick->line = 0ull;
    args->pick->col = 0ull;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side, .x = args->x,
                    .y = args->y))
    {
        return 0;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(problems_head_rect, ProblemsCtx, .frame = args->frame), .x = args->x,
                   .y = args->y))
    {
        args->pick->kind = EMBED_CALL(gui_problems_path.running, GuiProblemsPathCfg, .problems = args->problems_out)
                               ? GUI_PROBLEMS_PICK_STOP
                               : GUI_PROBLEMS_PICK_CHECK;
        return 1;
    }
    unsigned int row = 0u;
    unsigned int file = 0u;
    int item = GUI_PROBLEMS_NONE;
    if (!EMBED_CALL(gui_problems_row_frame.hit, GuiProblemsRowFrameCfg, .problems = args->problems_out,
                    .frame = args->frame, .x = args->x, .y = args->y, .row_pointer = &row) ||
        !EMBED_CALL(cli_problems_row.row, GuiProblemsRowCfg, .problems = args->problems_out, .row = row, .file = &file,
                    .item = &item))
    {
        return 1;
    }
    if (item == GUI_PROBLEMS_NONE)
    {
        args->problems_out->file[file].closed = !args->problems_out->file[file].closed;
        return 1;
    }
    // the diagnostic is among the diagnostics
    const GuiProblem *const one = &args->problems_out->item[(unsigned int)item];
    VerbaLinea path = {args->pick->path, sizeof(args->pick->path), 0ull};
    mmgr_line_put_n(&path, args->problems_out->texts.out + args->problems_out->file[file].path,
                    args->problems_out->file[file].length);
    (void)mmgr_line_finish(&path);
    args->pick->kind = GUI_PROBLEMS_PICK_OPEN;
    args->pick->line = one->from_line;
    args->pick->col = one->from_col;
    return 1;
}

int gui_problems_wheel(const GuiProblemsRowFrameCfg *args)
{
    const int tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_ROW, .scale = args->frame->scale);
    const int room = args->frame->side.y + args->frame->side.height - gui_problems_row_frame.rows_top(args);
    const int shown = (tall > 0) ? room / tall : 0;
    // the rows are far fewer than an int counts
    const int count =
        (int)EMBED_CALL(gui_problems_row_frame.rows, GuiProblemsRowFrameCfg, .problems = args->problems_out);
    const int most = (count > shown) ? count - shown : 0;
    const int wanted = (int)args->problems_out->scroll + args->rows;
    const int clamped = (wanted < 0) ? 0 : ((wanted > most) ? most : wanted);
    // the scroll clamped is not negative
    if ((unsigned int)clamped == args->problems_out->scroll)
    {
        return 0;
    }
    args->problems_out->scroll = (unsigned int)clamped;
    return 1;
}

// the baseline of text in `face` at `size` centered in a row of `tall` from `top`
EMBED_INLINE int problems_baseline(const ProblemsCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    unsigned int face = args->face;
    unsigned int size = args->size;
    const float ascent =
        EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    const float descent =
        EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    return args->top + (int)((((float)args->tall - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// a file row's arrow, pointing down where its rows are open and right where closed, `side` pixels across at x, y
EMBED_INLINE void problems_arrow_draw(const ProblemsCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const int half = args->side / 2;
    if (args->open)
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x, .y0 = args->y + 1,
                   .x1 = args->x + half, .y1 = args->y + half + 1, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half,
                   .y0 = args->y + half + 1, .x1 = args->x + args->side - 1, .y1 = args->y + 1, .color = color);
    }
    else
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + 1, .y0 = args->y,
                   .x1 = args->x + half + 1, .y1 = args->y + half, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half + 1,
                   .y0 = args->y + half, .x1 = args->x + 1, .y1 = args->y + args->side - 1, .color = color);
    }
}

// the color of a severity's mark
EMBED_INLINE GuiColor problems_color(const ProblemsCtx *args)
{
    if (args->severity == GUI_PROBLEMS_ERROR)
    {
        return args->palette->error;
    }
    return (args->severity == GUI_PROBLEMS_WARNING) ? args->palette->warning : args->palette->hover;
}

// the pane's head drawn: the check's progress and Stop while it runs, else the problems and files counted and the
// check offered
EMBED_INLINE void problems_head_draw(const ProblemsCtx *args)
{
    const GuiProblems *problems = args->problems_read;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect head = problems_head_rect(args);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_PROBLEMS_HEAD_SIZE, .scale = scale);
    const int baseline = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                    .size = size, .top = head.y, .tall = head.height);
    const int pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_PAD_LEFT, .scale = scale);
    char text[160];
    VerbaLinea line = {text, sizeof(text), 0ull};
    const int running = EMBED_CALL(gui_problems_path.running, GuiProblemsPathCfg, .problems = problems);
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
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(head.x + pad), .y = baseline, .text = text,
                     .length = line.at, .color = args->palette->text_2, .clip = args->inside);
    const char *const action = running ? "Stop" : "Check the tree";
    const unsigned long long action_length = cellul.len(&(CatenaFinitaCfg){.src = action, .cap = 32ull});
    const float action_wide = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                         .face = GUI_FACE_HEAD, .size = size, .text = action, .length = action_length);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(head.x + head.width - pad) - action_wide,
                     .y = baseline, .text = action, .length = action_length, .color = args->palette->link,
                     .clip = args->inside);
}

void gui_problems_draw(const GuiProblemsRowFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect side = args->frame->side;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = side))
    {
        return;
    }
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    const GuiRect inside = {side.x, side.y, side.width - one, side.height};
    const int line_y =
        side.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_HEAD, .scale = scale);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){side.x, line_y, inside.width, one}, .other = inside),
               .color = args->palette->line);
    EMBED_CALL(problems_head_draw, ProblemsCtx, .problems_read = args->problems, .raster = raster, .glyphs = glyphs,
               .palette = args->palette, .frame = args->frame, .inside = inside);
    const int rows_top = gui_problems_row_frame.rows_top(args);
    const GuiRect body =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){side.x, rows_top, inside.width, side.y + side.height - rows_top}, .other = inside);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = body))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = body,
               .color = args->palette->editor_ground);
    const unsigned int name_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_PROBLEMS_NAME_SIZE, .scale = scale);
    const unsigned int where_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = (int)GUI_PROBLEMS_WHERE_SIZE, .scale = scale);
    const unsigned int message_size = (unsigned int)EMBED_CALL(
        gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = (int)GUI_PROBLEMS_MESSAGE_SIZE, .scale = scale);
    const unsigned int count_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = (int)GUI_PROBLEMS_COUNT_SIZE, .scale = scale);
    const int pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_PAD_LEFT, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_GAP, .scale = scale);
    const int guide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_GUIDE, .scale = scale);
    const int mark = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_MARK, .scale = scale);
    const int arrow =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ARROW, .scale = scale);
    const unsigned int rows = gui_problems_row_frame.rows(args);
    for (unsigned int row = args->problems->scroll; row < rows; row += 1u)
    {
        const GuiRect rect = EMBED_CALL(gui_problems_row_frame.row_rect, GuiProblemsRowFrameCfg,
                                        .problems = args->problems, .frame = args->frame, .row = row);
        if (rect.y >= body.y + body.height)
        {
            break;
        }
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = body);
        // the rows are fewer than an int counts
        if (args->problems->pointed == (int)row)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = on,
                       .color = args->palette->bar_title_hover);
        }
        unsigned int at = 0u;
        int item = GUI_PROBLEMS_NONE;
        if (!EMBED_CALL(cli_problems_row.row, GuiProblemsRowCfg, .problems = args->problems, .row = row, .file = &at,
                        .item = &item))
        {
            break;
        }
        const GuiProblemFile *const file = &args->problems->file[at];
        if (item == GUI_PROBLEMS_NONE)
        {
            // a file's row: the arrow, the name, the folder after it quieter, and the count at the right
            EMBED_CALL(problems_arrow_draw, ProblemsCtx, .raster = raster, .x = rect.x + pad,
                       .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = !file->closed,
                       .color = args->palette->text);
            const int baseline = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                            .size = name_size, .top = rect.y, .tall = rect.height);
            char digits[24];
            VerbaLinea count = {digits, sizeof(digits), 0ull};
            mmgr_line_decimal(&count, file->count, 1u);
            const int count_pad =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_COUNT_PAD, .scale = scale);
            const int count_wide =
                (int)(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                 .face = GUI_FACE_CODE, .size = count_size, .text = digits, .length = count.at) +
                      0.5f) +
                (2 * count_pad);
            const int count_tall = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                              .pixels = GUI_PROBLEMS_FILE_ROW - 8, .scale = scale);
            const GuiRect badge = {rect.x + rect.width - pad - count_wide, rect.y + ((rect.height - count_tall) / 2),
                                   count_wide, count_tall};
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = badge, .other = on),
                       .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                          .pixels = GUI_PROBLEMS_COUNT_RADIUS, .scale = scale),
                       .color = args->palette->line);
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = count_size, .x_f32 = (float)(badge.x + count_pad),
                             .y = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_CODE,
                                             .size = count_size, .top = badge.y, .tall = badge.height),
                             .text = digits, .length = count.at, .color = args->palette->text_2, .clip = on);
            const GuiRect text_clip =
                EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                           .one = (GuiRect){rect.x, rect.y, badge.x - gap - rect.x, rect.height}, .other = on);
            const char *const path = args->problems->texts.out + file->path;
            float x = (float)(rect.x + pad + arrow + gap);
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_HEAD, .size = name_size, .x_f32 = x, .y = baseline,
                           .text = path + file->name, .length = file->length - file->name, .color = args->palette->text,
                           .clip = text_clip);
            if (file->name > 1u)
            {
                (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster,
                                 .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = where_size, .x_f32 = x + (float)gap,
                                 .y = baseline, .text = path, .length = file->name - 1u, .color = args->palette->text_3,
                                 .clip = text_clip);
            }
            continue;
        }
        // a diagnostic's row: the guide, the mark in its severity's color, the message and its place at the right
        // the diagnostic is among the diagnostics
        const GuiProblem *const one = &args->problems->item[(unsigned int)item];
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){rect.x + pad + (arrow / 2), rect.y,
                                                       EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                  .pixels = 1, .scale = scale),
                                                       rect.height},
                                      .other = on),
                   .color = args->palette->line);
        const int mark_x = rect.x + pad + guide;
        EMBED_CALL(
            gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
            .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                               .one = (GuiRect){mark_x, rect.y + ((rect.height - mark) / 2), mark, mark}, .other = on),
            .radius = (unsigned int)(mark / 2),
            .color = EMBED_CALL(problems_color, ProblemsCtx, .palette = args->palette, .severity = one->severity));
        char where_text[48];
        VerbaLinea where = {where_text, sizeof(where_text), 0ull};
        mmgr_line_decimal(&where, one->from_line + 1ull, 1u);
        mmgr_line_ch(&where, ':');
        mmgr_line_decimal(&where, one->from_col + 1ull, 1u);
        const float where_wide =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                       .size = where_size, .text = where_text, .length = where.at);
        const int baseline = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                        .size = message_size, .top = rect.y, .tall = rect.height);
        const float where_x = (float)(rect.x + rect.width - pad) - where_wide;
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = where_size, .x_f32 = where_x, .y = baseline, .text = where_text,
                         .length = where.at, .color = args->palette->text_3, .clip = on);
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, (int)where_x - gap - rect.x, rect.height}, .other = on);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = message_size, .x_f32 = (float)(mark_x + mark + gap),
                         .y = baseline, .text = args->problems->texts.out + one->message, .length = one->message_length,
                         .color = args->palette->text, .clip = text_clip);
    }
}

void gui_problems_underlined(const GuiProblemsPathRasterCfg *args)
{
    const GuiEditor *editor = args->editor;
    GuiRect place = args->place;
    float cell = args->cell;
    int scale = args->scale;
    unsigned int first = 0u;
    const unsigned int count = EMBED_CALL(gui_problems_path.get, GuiProblemsPathCfg, .problems = args->problems,
                                          .path = args->path, .first = &first);
    const int thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    const int up = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale);
    for (unsigned int at = first; at < first + count; at += 1u)
    {
        const GuiProblem *const one = &args->problems->item[at];
        const GuiColor color =
            EMBED_CALL(problems_color, ProblemsCtx, .palette = args->palette, .severity = one->severity);
        for (unsigned long long line = one->from_line; line <= one->to_line; line += 1ull)
        {
            const unsigned long long from = (line == one->from_line) ? one->from_col : 0ull;
            const unsigned long long to = (line == one->to_line) ? one->to_col : ~0ull;
            GuiRect start;
            GuiRect end;
            if (!EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = editor,
                            .place = place, .cell = cell, .scale = scale, .line = line, .col = from, .rect = &start) ||
                !EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = editor,
                            .place = place, .cell = cell, .scale = scale, .line = line, .col = to, .rect = &end))
            {
                continue;
            }
            // a span of no width underlines its one cell
            const int right = (end.x > start.x) ? end.x : start.x + start.width;
            const GuiRect under = {start.x, start.y + start.height - up, right - start.x, thick};
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = under, .other = place),
                       .color = color);
        }
    }
}
