// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor.c: the editor part. The lines an answer holds are copied out of its tree into cells of the part's own, so the
// tree goes as the answer is read; a run's column counts the page's units, UTF-16's, as the CLI writes them, so a
// letter past the basic plane counts two
#include "editor.h"

#include "../../../cli/src/text/text.h"
#include "../answer.h"
#include "../frame.h"

#include <math.h>

void gui_editor_init(const GuiEditorCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->editor, .val = 0, .bytes = sizeof(*args->editor)});
    args->editor->caret_ink = 255u;
}

typedef struct
{
    void **cell_bytess;
    unsigned long long *room;
    unsigned long long want;
    GuiEditor *editor_out;
    const CliJson *json;
    unsigned int numbers;
    unsigned long long from;
    unsigned int at;
    unsigned int colors;
    unsigned long long line;
    unsigned int count;
    unsigned int flat;
    unsigned int *run;
    unsigned int used;
    unsigned int lines;
    unsigned int folds;
    unsigned int node;
    unsigned int find;
    unsigned int matches;
    unsigned long long lines_u64;
    const GuiEditor *editor;
    const GuiLine *line_line_pointer;
    unsigned long long col;
    const GuiLine *stored_line;
    const GuiPalette *palette;
    unsigned int ink;
    GuiRaster *raster;
    int x_i32;
    int middle;
    int tall;
    GuiColor color;
    GuiRect clip;
    const GuiFold *fold;
    GuiGlyphs *glyphs;
    float x;
    int top;
    int baseline;
    int line_tall;
    float cell;
    unsigned int size;
    int scale;
    int y;
    GuiRect rect;
    int thick;
    GuiRect row_rect;
    float text_x;
    int boxed;
    GuiRect place;
} EditorCtx;
// `*cell` with room for `want` bytes: the cell it is where its `*room` has them, else a cell half again as large taken
// and the one before given back, its bytes not saved: 1, or 0 with the cell unchanged where none could be taken
EMBED_INLINE int editor_room(const EditorCtx *args)
{
    void **cell = args->cell_bytess;
    unsigned long long *room = args->room;
    const unsigned long long asked = (args->want != 0ull) ? args->want : 1ull;
    if ((*cell != NULL) && (asked <= *room))
    {
        return 1;
    }
    const unsigned long long bytes = asked + (asked / 2ull);
    void *const taken = g_cli_cells->persistent_buf_alloc((size_t)bytes, &g_gui_editor_cells);
    if (taken == NULL)
    {
        return 0;
    }
    g_cli_cells->persistent_buf_release(*cell, &g_gui_editor_cells);
    *cell = taken;
    *room = bytes;
    return 1;
}

// the lines stored emptied, with the folds that open on them, find's state and the matches, every cell saved for the
// next frame
EMBED_INLINE void editor_lines_clear(const EditorCtx *args)
{
    char *const query = args->editor_out->find.query;
    memor.set(&(MemoriaCfg){.dst = &args->editor_out->find, .val = 0, .bytes = sizeof(args->editor_out->find)});
    args->editor_out->find.query = query;
    args->editor_out->run_total = 0u;
    args->editor_out->count = 0u;
    args->editor_out->folds = 0u;
    args->editor_out->matches = 0ull;
}

// the lines' cells given back, with those of the folds, find's query and the matches
EMBED_INLINE void editor_lines_free(const EditorCtx *args)
{
    g_cli_cells->persistent_buf_release(args->editor_out->line, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor_out->bytes, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor_out->run, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor_out->run_laid, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor_out->fold, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor_out->closers, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor_out->find.query, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor_out->match, &g_gui_editor_cells);
    args->editor_out->line = NULL;
    args->editor_out->bytes = NULL;
    args->editor_out->run = NULL;
    args->editor_out->run_laid = NULL;
    args->editor_out->fold = NULL;
    args->editor_out->closers = NULL;
    args->editor_out->find.query = NULL;
    args->editor_out->match = NULL;
    memor.set(&(MemoriaCfg){.dst = &args->editor_out->room, .val = 0, .bytes = sizeof(args->editor_out->room)});
    editor_lines_clear(args);
}

void gui_editor_free(const GuiEditorJsonNamesCfg *args)
{
    EMBED_CALL(editor_lines_free, EditorCtx, .editor_out = args->editor);
    g_cli_cells->persistent_buf_release(args->editor->names, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor->name_at, &g_gui_editor_cells);
    args->editor->names = NULL;
    args->editor->name_at = NULL;
    args->editor->name_count = 0u;
}

void gui_editor_shape(const GuiEditorCfg *args)
{
    unsigned long long lines = args->lines;
    args->editor->lines = lines;
    if ((lines != 0ull) && (args->editor->top >= lines))
    {
        args->editor->top = lines - 1ull;
    }
}

int gui_editor_by_name(const GuiEditorJsonNamesCfg *args)
{
    unsigned int names = args->names;
    g_cli_cells->persistent_buf_release(args->editor->names, &g_gui_editor_cells);
    g_cli_cells->persistent_buf_release(args->editor->name_at, &g_gui_editor_cells);
    args->editor->names = NULL;
    args->editor->name_at = NULL;
    args->editor->name_count = 0u;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = names,
                    .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    const unsigned int count = args->json->node[names].count;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        (void)EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                         .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = args->json, .array = names, .index = at),
                         .length_pointer = &length);
        bytes += length + 1ull;
    }
    args->editor->names = g_cli_cells->persistent_buf_alloc((bytes != 0ull) ? bytes : 1ull, &g_gui_editor_cells);
    args->editor->name_at = g_cli_cells->persistent_buf_alloc(
        ((count != 0u) ? count : 1u) * sizeof(*args->editor->name_at), &g_gui_editor_cells);
    if ((args->editor->names == NULL) || (args->editor->name_at == NULL))
    {
        gui_editor_json_names.free(args);
        return 0;
    }
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = args->json, .array = names, .index = at),
                       .length_pointer = &length);
        args->editor->name_at[at] = used;
        memor.cpy(&(MemoriaCfg){.dst = args->editor->names + used, .src = (text != NULL) ? text : "", .bytes = length});
        args->editor->names[used + length] = '\0';
        used += length + 1ull;
    }
    args->editor->name_count = count;
    return 1;
}

// the line the row `at` of the array `lines` is: the item `at` of the array `numbers`, or `from` + `at` where the
// array gives none
EMBED_INLINE unsigned long long editor_number_get(const EditorCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int node = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                         .node = args->numbers, .kind = CFG_JSON_ARRAY)
                                  ? EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = json, .array = args->numbers, .index = args->at)
                                  : CLI_JSON_NONE;
    return (node != CLI_JSON_NONE) ? EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json, .node = node,
                                                .otherwise = args->from + args->at)
                                   : args->from + args->at;
}

// a line's runs array node of `colors`, each `{"from", "runs"}`, CLI_JSON_NONE where none colors the line; and the
// runs counted for the lines numbered as `numbers` gives the `count` rows from `from`: how many pairs they hold in all
EMBED_INLINE unsigned int editor_runs_get(const EditorCtx *args);

EMBED_INLINE unsigned int editor_runs_count(const EditorCtx *args)
{
    const CliJson *json = args->json;
    unsigned int total = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const unsigned int flat =
            EMBED_CALL(editor_runs_get, EditorCtx, .json = json, .colors = args->colors,
                       .line = EMBED_CALL(editor_number_get, EditorCtx, .json = json, .numbers = args->numbers,
                                          .from = args->from, .at = at));
        total += EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = flat,
                            .kind = CFG_JSON_ARRAY)
                     ? (json->node[flat].count / 2u)
                     : 0u;
    }
    return total;
}

EMBED_INLINE unsigned int editor_runs_get(const EditorCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int ranges = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = args->colors, .kind = CFG_JSON_ARRAY)
                                    ? json->node[args->colors].count
                                    : 0u;
    unsigned int found = CLI_JSON_NONE;
    for (unsigned int range = 0u; range < ranges; range += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = args->colors, .index = range);
        const unsigned long long start = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                                    .object = one, .name = "from", .otherwise = 0ull);
        const unsigned int runs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = one, .name = "runs");
        const unsigned int lines = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = json, .node = runs, .kind = CFG_JSON_ARRAY)
                                       ? json->node[runs].count
                                       : 0u;
        if ((args->line >= start) && (args->line < start + lines))
        {
            // the last range to color a line wins
            found = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                               .array = runs, .index = (unsigned int)(args->line - start));
        }
    }
    return found;
}

// the pairs of the flat runs array `flat` put after `used` of `run`: how many were put
EMBED_INLINE unsigned int editor_runs_put(const EditorCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int values = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = args->flat, .kind = CFG_JSON_ARRAY)
                                    ? json->node[args->flat].count
                                    : 0u;
    unsigned int put = 0u;
    for (unsigned int at = 0u; (at + 1u) < values; at += 2u)
    {
        // a column and a name's index are far inside what an unsigned int represents
        args->run[args->used + (2u * put)] =
            (unsigned int)EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                     .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                        .json_read = json, .array = args->flat, .index = at),
                                     .otherwise = 0ull);
        args->run[args->used + (2u * put) + 1u] =
            (unsigned int)EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                     .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                        .json_read = json, .array = args->flat, .index = at + 1u),
                                     .otherwise = 0ull);
        put += 1u;
    }
    return put;
}

// the strings of the array `lines` saved as the lines numbered as the array `numbers` gives them, from `from` where it
// gives none, each with its runs from `colors`
static int editor_array_put(const EditorCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long from = args->from;
    editor_lines_clear(args);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->lines,
                    .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    const unsigned int count = json->node[args->lines].count;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        (void)EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                         .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = args->lines, .index = at),
                         .length_pointer = &length);
        bytes += length;
    }
    const unsigned int pairs = EMBED_CALL(editor_runs_count, EditorCtx, .json = json, .colors = args->colors,
                                          .numbers = args->numbers, .from = from, .count = count);
    if (!EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor_out->line,
                    .room = &args->editor_out->room.line,
                    .want = (unsigned long long)count * sizeof(*args->editor_out->line)) ||
        !EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor_out->bytes,
                    .room = &args->editor_out->room.bytes, .want = bytes) ||
        !EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor_out->run,
                    .room = &args->editor_out->room.run,
                    .want = (unsigned long long)pairs * 2ull * sizeof(*args->editor_out->run)))
    {
        return 0;
    }
    unsigned long long used = 0ull;
    unsigned int runs_used = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = args->lines, .index = at),
                       .length_pointer = &length);
        GuiLine *const line = &args->editor_out->line[at];
        line->number =
            EMBED_CALL(editor_number_get, EditorCtx, .json = json, .numbers = args->numbers, .from = from, .at = at);
        line->text = used;
        line->length = length;
        memor.cpy(
            &(MemoriaCfg){.dst = args->editor_out->bytes + used, .src = (text != NULL) ? text : "", .bytes = length});
        used += length;
        line->runs = runs_used;
        line->run_count = EMBED_CALL(
            editor_runs_put, EditorCtx, .json = json,
            .flat = EMBED_CALL(editor_runs_get, EditorCtx, .json = json, .colors = args->colors, .line = line->number),
            .run = args->editor_out->run, .used = runs_used);
        runs_used += 2u * line->run_count;
    }
    args->editor_out->from = (count != 0u) ? args->editor_out->line[0].number : from;
    args->editor_out->count = count;
    args->editor_out->run_total = runs_used;
    return 1;
}

int gui_editor_lines_put(const GuiEditorJsonReadCfg *args)
{
    return EMBED_CALL(editor_array_put, EditorCtx, .editor_out = args->editor, .json = args->json,
                      .lines = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = args->read, .name = "lines"),
                      .colors = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = args->read, .name = "colors"),
                      .numbers = CLI_JSON_NONE, .from = args->from);
}

// the folds of the array `folds`, each `[first, last, shown, closed, closer]`, saved with their closers' texts: 1, or 0
// where a cell could not be taken, the part then holding none
EMBED_INLINE int editor_folds_put(const EditorCtx *args)
{
    const CliJson *json = args->json;
    unsigned int folds = args->folds;
    const unsigned int count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = folds, .kind = CFG_JSON_ARRAY)
                                   ? json->node[folds].count
                                   : 0u;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        (void)EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                               .array = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                   .json_read = json, .array = folds, .index = at),
                               .index = 4u),
            .length_pointer = &length);
        bytes += length;
    }
    if (!EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor_out->fold,
                    .room = &args->editor_out->room.fold,
                    .want = (unsigned long long)count * sizeof(*args->editor_out->fold)) ||
        !EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor_out->closers,
                    .room = &args->editor_out->room.closers, .want = bytes))
    {
        args->editor_out->folds = 0u;
        return 0;
    }
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = folds, .index = at);
        unsigned long long length = 0ull;
        const char *const closer =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = one, .index = 4u),
                       .length_pointer = &length);
        GuiFold *const fold = &args->editor_out->fold[at];
        fold->first = EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                 .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                    .json_read = json, .array = one, .index = 0u),
                                 .otherwise = 0ull);
        fold->last = EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                   .json_read = json, .array = one, .index = 1u),
                                .otherwise = 0ull);
        fold->shown = EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                 .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                    .json_read = json, .array = one, .index = 2u),
                                 .otherwise = 0ull) != 0ull;
        fold->closed = EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                  .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                     .json_read = json, .array = one, .index = 3u),
                                  .otherwise = 0ull) != 0ull;
        fold->closer = used;
        fold->closer_length = length;
        memor.cpy(&(MemoriaCfg){
            .dst = args->editor_out->closers + used, .src = (closer != NULL) ? closer : "", .bytes = length});
        used += length;
    }
    args->editor_out->folds = count;
    return 1;
}

// a place `{"line", "col"}` read
EMBED_INLINE GuiPlace editor_place_get(const EditorCtx *args)
{
    const CliJson *json = args->json;
    const GuiPlace place = {EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                       .object = args->node, .name = "line", .otherwise = 0ull),
                            EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                       .object = args->node, .name = "col", .otherwise = 0ull)};
    return place;
}

// find's object `find`, null where its bar is closed, and the array `matches` of `[line, from, to]` saved: 1, or 0
// where a cell could not be taken
EMBED_INLINE int editor_find_put(const EditorCtx *args)
{
    const CliJson *json = args->json;
    unsigned int matches = args->matches;
    args->editor_out->find.open = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                             .node = args->find, .kind = CFG_JSON_OBJECT);
    args->editor_out->find.current = -1ll;
    if (!args->editor_out->find.open)
    {
        return 1;
    }
    unsigned long long length = 0ull;
    const char *const query =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->find, .name = "query"),
                   .length_pointer = &length);
    const unsigned long long count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = json, .node = matches, .kind = CFG_JSON_ARRAY)
                                         ? json->node[matches].count
                                         : 0u;
    if (!EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor_out->find.query,
                    .room = &args->editor_out->room.query, .want = length) ||
        !EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor_out->match,
                    .room = &args->editor_out->room.match, .want = count * sizeof(*args->editor_out->match)))
    {
        args->editor_out->find.open = 0;
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->editor_out->find.query, .src = (query != NULL) ? query : "", .bytes = length});
    args->editor_out->find.query_length = length;
    args->editor_out->find.focus = EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = json,
                                              .object = args->find, .name = "focus") ||
                                   (EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                               .object = args->find, .name = "focus", .otherwise = 0ull) != 0ull);
    args->editor_out->find.at = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                           .object = args->find, .name = "at", .otherwise = 0ull);
    args->editor_out->find.regex = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                              .object = args->find, .name = "regex", .otherwise = 0ull) != 0ull;
    args->editor_out->find.match_case = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                                   .object = args->find, .name = "case", .otherwise = 0ull) != 0ull;
    args->editor_out->find.word = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                             .object = args->find, .name = "word", .otherwise = 0ull) != 0ull;
    args->editor_out->find.count = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                              .object = args->find, .name = "count", .otherwise = 0ull);
    const unsigned int current = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->find, .name = "current");
    // the current match is counted among the matches, which a long long counts
    args->editor_out->find.current = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = json, .node = current, .kind = CFG_JSON_NUMBER)
                                         ? (long long)EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                                                 .node = current, .otherwise = 0ull)
                                         : -1ll;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        // the matches among the rows are far inside what an unsigned int represents
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = matches, .index = (unsigned int)at);
        args->editor_out->match[at][0] =
            EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = one, .index = 0u),
                       .otherwise = 0ull);
        args->editor_out->match[at][1] =
            EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = one, .index = 1u),
                       .otherwise = 0ull);
        args->editor_out->match[at][2] =
            EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = one, .index = 2u),
                       .otherwise = 0ull);
    }
    args->editor_out->matches = count;
    return 1;
}

int gui_editor_frame_put(const GuiEditorJsonFrameCfg *args)
{
    const CliJson *json = args->json;
    const unsigned long long from = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                               .object = args->frame, .name = "from", .otherwise = 0ull);
    if (!EMBED_CALL(editor_array_put, EditorCtx, .editor_out = args->editor, .json = json,
                    .lines = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->frame, .name = "rows"),
                    .colors = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->frame, .name = "colors"),
                    .numbers = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->frame, .name = "numbers"),
                    .from = from) ||
        !EMBED_CALL(editor_folds_put, EditorCtx, .editor_out = args->editor, .json = json,
                    .folds = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->frame, .name = "folds")) ||
        !EMBED_CALL(editor_find_put, EditorCtx, .editor_out = args->editor, .json = json,
                    .find = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                       .object = args->frame, .name = "find"),
                    .matches = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->frame, .name = "matches")))
    {
        return 0;
    }
    args->editor->top = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                   .object = args->frame, .name = "top", .otherwise = 0ull);
    args->editor->lines = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                     .object = args->frame, .name = "lines", .otherwise = 0ull);
    // the indentation's tabs come as 1, 0 or null, null and a frame without one read as nothing set
    const unsigned int indent = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->frame, .name = "indent");
    const unsigned long long tabs = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                               .object = indent, .name = "tabs", .otherwise = 2ull);
    args->editor->indent_tabs = (tabs == 1ull) ? 1 : ((tabs == 0ull) ? 0 : -1);
    // a step is far inside what an unsigned int represents
    args->editor->indent_size =
        (unsigned int)EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = indent,
                                 .name = "size", .otherwise = 4ull);
    args->editor->cursor = EMBED_CALL(editor_place_get, EditorCtx, .json = json,
                                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                         .json_read = json, .object = args->frame, .name = "cursor"));
    args->editor->anchor = EMBED_CALL(editor_place_get, EditorCtx, .json = json,
                                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                         .json_read = json, .object = args->frame, .name = "anchor"));
    args->editor->has_cursor = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->frame, .name = "cursor") != CLI_JSON_NONE;
    const unsigned int pair = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->frame, .name = "pair");
    args->editor->has_pair = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = pair, .kind = CFG_JSON_ARRAY) &&
                             (json->node[pair].count == 2u);
    args->editor->pair[0] = EMBED_CALL(editor_place_get, EditorCtx, .json = json,
                                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                          .json_read = json, .array = pair, .index = 0u));
    args->editor->pair[1] = EMBED_CALL(editor_place_get, EditorCtx, .json = json,
                                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                          .json_read = json, .array = pair, .index = 1u));
    return 1;
}

int gui_editor_on_color(const GuiEditorJsonCfg *args)
{
    if (args->editor->count == 0u)
    {
        return 1;
    }
    // the runs made anew: each line's from the colors where they color it, else as it saves them
    unsigned int pairs = 0u;
    for (unsigned int at = 0u; at < args->editor->count; at += 1u)
    {
        const unsigned int flat = EMBED_CALL(editor_runs_get, EditorCtx, .json = args->json, .colors = args->colors,
                                             .line = args->editor->line[at].number);
        pairs += (flat != CLI_JSON_NONE) ? (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                       .json_read = args->json, .node = flat, .kind = CFG_JSON_ARRAY)
                                                ? args->json->node[flat].count / 2u
                                                : 0u)
                                         : args->editor->line[at].run_count;
    }
    // the runs laid into the second cell from the first, the two then trading places
    if (!EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor->run_laid,
                    .room = &args->editor->room.run_laid,
                    .want = (unsigned long long)pairs * 2ull * sizeof(*args->editor->run)))
    {
        return 0;
    }
    unsigned int *const run = args->editor->run_laid;
    unsigned int used = 0u;
    for (unsigned int at = 0u; at < args->editor->count; at += 1u)
    {
        GuiLine *const line = &args->editor->line[at];
        const unsigned int flat =
            EMBED_CALL(editor_runs_get, EditorCtx, .json = args->json, .colors = args->colors, .line = line->number);
        const unsigned int start = used;
        if (flat != CLI_JSON_NONE)
        {
            line->run_count =
                EMBED_CALL(editor_runs_put, EditorCtx, .json = args->json, .flat = flat, .run = run, .used = used);
        }
        else
        {
            memor.cpy(&(MemoriaCfg){.dst = run + used,
                                    .src = args->editor->run + line->runs,
                                    .bytes = 2ull * line->run_count * sizeof(*run)});
        }
        line->runs = start;
        used += 2u * line->run_count;
    }
    const unsigned long long room = args->editor->room.run;
    args->editor->run_laid = args->editor->run;
    args->editor->room.run = args->editor->room.run_laid;
    args->editor->room.run_laid = room;
    args->editor->run = run;
    args->editor->run_total = used;
    return 1;
}

int gui_editor_text_put(const GuiEditorTextCfg *args)
{
    EMBED_CALL(editor_lines_clear, EditorCtx, .editor_out = args->editor);
    unsigned int count = 1u;
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        count += (args->text[at] == '\n') ? 1u : 0u;
    }
    if (!EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor->line,
                    .room = &args->editor->room.line,
                    .want = (unsigned long long)count * sizeof(*args->editor->line)) ||
        !EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor->bytes,
                    .room = &args->editor->room.bytes, .want = args->length) ||
        !EMBED_CALL(editor_room, EditorCtx, .cell_bytess = (void **)&args->editor->run, .room = &args->editor->room.run,
                    .want = 2ull * sizeof(*args->editor->run)))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->editor->bytes, .src = args->text, .bytes = args->length});
    unsigned int line = 0u;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; at <= args->length; at += 1ull)
    {
        if ((at == args->length) || (args->text[at] == '\n'))
        {
            const unsigned long long end = ((at > start) && (args->text[at - 1ull] == '\r')) ? at - 1ull : at;
            args->editor->line[line] = (GuiLine){line, start, end - start, 0u, 0u};
            line += 1u;
            start = at + 1ull;
        }
    }
    args->editor->from = 0ull;
    args->editor->count = count;
    args->editor->run_total = 0u;
    args->editor->lines = count;
    return 1;
}

void gui_editor_scroll(const GuiEditorDeltaCfg *args)
{
    const unsigned long long last = (args->editor->lines != 0ull) ? args->editor->lines - 1ull : 0ull;
    if (args->delta < 0ll)
    {
        // a move up is clamped to the top: it is the count of lines above the top
        const unsigned long long up = (unsigned long long)(-args->delta);
        args->editor->top = (up > args->editor->top) ? 0ull : args->editor->top - up;
        return;
    }
    // a move down, which is not negative here
    const unsigned long long down = (unsigned long long)args->delta;
    args->editor->top = ((args->editor->top + down) > last) ? last : args->editor->top + down;
}

// how many digits the file's line count takes, three at the least, as the page gives its gutter room
EMBED_INLINE unsigned int editor_digits(const EditorCtx *args)
{
    unsigned int digits = 1u;
    for (unsigned long long left = args->lines_u64; left >= 10ull; left /= 10ull)
    {
        digits += 1u;
    }
    return (digits < 3u) ? 3u : digits;
}

int gui_editor_gutter(const GuiEditorPlaceCellCfg *args)
{
    int scale = args->scale;
    const float numbers =
        (float)EMBED_CALL(editor_digits, EditorCtx, .lines_u64 = args->editor_read->lines) * args->cell;
    return (int)(numbers + 0.5f) + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                              .pixels = GUI_GUTTER_BEFORE + GUI_GUTTER_AFTER, .scale = scale);
}

// the cell column a line's unit column stands at, tabs laid to their stops, a letter past the basic plane counting
// two units; and the line's cells in all where `col` is past its end
EMBED_INLINE unsigned long long editor_cell_get(const EditorCtx *args)
{
    const char *const text = args->editor->bytes + args->line_line_pointer->text;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    unsigned long long cell = 0ull;
    while ((at < args->line_line_pointer->length) && (units < args->col))
    {
        unsigned long long took = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text,
                                               .size = args->line_line_pointer->length, .at = at, .length = &took);
        at += (took != 0ull) ? took : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
        cell += (letter == '\t') ? (GUI_TAB_COLUMNS - (cell % GUI_TAB_COLUMNS)) : 1ull;
    }
    return cell;
}

int gui_editor_cell_at(const GuiEditorPlaceCellScaleCfg *args)
{
    int scale = args->scale;
    unsigned long long *row = args->row;
    unsigned long long *column = args->column;
    const int line_tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const int text_x = args->place.x + EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg,
                                                  .editor_read = args->editor_read, .cell = args->cell, .scale = scale);
    if ((line_tall <= 0) || (args->cell <= 0.0f) || (args->y < args->place.y) ||
        (args->y >= args->place.y + args->place.height) || (args->x < text_x) ||
        (args->x >= args->place.x + args->place.width))
    {
        return 0;
    }
    // the row and the column are not negative, the pixel standing in the text
    *row = (unsigned long long)((args->y - args->place.y) / line_tall);
    *column = (unsigned long long)(((float)(args->x - text_x) / args->cell) + 0.5f);
    return 1;
}

int gui_editor_row_at(const GuiEditorPlaceCellScaleCfg *args)
{
    int scale = args->scale;
    unsigned long long *row = args->row;
    const int line_tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const int text_x = args->place.x + EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg,
                                                  .editor_read = args->editor_read, .cell = args->cell, .scale = scale);
    if ((line_tall <= 0) || (args->y < args->place.y) || (args->y >= args->place.y + args->place.height) ||
        (args->x < args->place.x) || (args->x >= text_x))
    {
        return 0;
    }
    // the row is not negative, the pixel standing in the gutter
    *row = (unsigned long long)((args->y - args->place.y) / line_tall);
    return 1;
}

// the byte of the stored line `stored_line` where its unit `col` starts, the line's length where the column is past it
EMBED_INLINE unsigned long long editor_byte_get(const EditorCtx *args)
{
    const char *const text = args->editor->bytes + args->stored_line->text;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    while ((at < args->stored_line->length) && (units < args->col))
    {
        unsigned long long took = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text,
                                               .size = args->stored_line->length, .at = at, .length = &took);
        at += (took != 0ull) ? took : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
    }
    return at;
}

int gui_editor_units_text(const GuiEditorFromCfg *args)
{
    const char **text = args->text_pointer;
    unsigned long long *length = args->length_pointer;
    for (unsigned int at = 0u; at < args->editor_read->count; at += 1u)
    {
        const GuiLine *const stored_line = &args->editor_read->line[at];
        if (stored_line->number == args->line)
        {
            const unsigned long long start = EMBED_CALL(editor_byte_get, EditorCtx, .editor = args->editor_read,
                                                        .stored_line = stored_line, .col = args->from);
            const unsigned long long end =
                EMBED_CALL(editor_byte_get, EditorCtx, .editor = args->editor_read, .stored_line = stored_line,
                           .col = (args->to > args->from) ? args->to : args->from);
            *text = args->editor_read->bytes + stored_line->text + start;
            *length = end - start;
            return 1;
        }
    }
    return 0;
}

GuiRect gui_editor_caret_rect(const GuiEditorPlaceCfg *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    int scale = args->scale;
    const GuiRect none = {0, 0, 0, 0};
    if (!args->editor_read->has_cursor || (args->editor_read->find.open && args->editor_read->find.focus) ||
        EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->place))
    {
        return none;
    }
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_SIZE, .scale = scale);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = glyphs, .size = size);
    const int gutter = EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg, .editor_read = args->editor_read,
                                  .cell = cell, .scale = scale);
    const GuiRect text_clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place,
        .other = (GuiRect){args->place.x + gutter, args->place.y, args->place.width - gutter, args->place.height});
    GuiRect at;
    if (!EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = args->editor_read,
                    .place = args->place, .cell = cell, .scale = scale, .line = args->editor_read->cursor.line,
                    .col = args->editor_read->cursor.col, .rect = &at))
    {
        return none;
    }
    return EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg,
        .one = (GuiRect){at.x, at.y, EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale),
                         at.height},
        .other = text_clip);
}

// the caret's color at `ink`'s share of its alpha: a byte's share of a byte, rounded, is a byte
EMBED_INLINE GuiColor editor_caret_color(const EditorCtx *args)
{
    return GUI_RGBA(GUI_RED_OF(args->palette->caret), GUI_GREEN_OF(args->palette->caret),
                    GUI_BLUE_OF(args->palette->caret),
                    ((GUI_ALPHA_OF(args->palette->caret) * args->ink) + 127u) / 255u);
}

GuiRect gui_editor_caret_draw(const GuiEditorRasterCfg *args)
{
    args->editor->caret_saved = 0;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->caret))
    {
        return args->caret;
    }
    // the rectangle's sides are not negative where it is not empty
    const unsigned int wide = (unsigned int)args->caret.width;
    const unsigned int tall = (unsigned int)args->caret.height;
    if (wide * tall <= GUI_CARET_UNDER_MOST)
    {
        for (unsigned int row = 0u; row < tall; row += 1u)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = &args->editor->caret_under[row * wide],
                              .src = &args->raster->pixels[(((unsigned int)args->caret.y + row) * args->raster->width) +
                                                           (unsigned int)args->caret.x],
                              .bytes = wide * sizeof(GuiColor)});
        }
        args->editor->caret_rect = args->caret;
        args->editor->caret_saved = 1;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->caret,
               .color =
                   EMBED_CALL(editor_caret_color, EditorCtx, .palette = args->palette, .ink = args->editor->caret_ink));
    return args->caret;
}

GuiRect gui_editor_caret_redrawn(const GuiEditorRasterCfg *args)
{
    if (!args->editor->caret_saved)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect caret = args->editor->caret_rect;
    // the rectangle's sides are not negative, the rectangle saved being drawn
    const unsigned int wide = (unsigned int)caret.width;
    const unsigned int tall = (unsigned int)caret.height;
    for (unsigned int row = 0u; row < tall; row += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = &args->raster->pixels[(((unsigned int)caret.y + row) * args->raster->width) + (unsigned int)caret.x],
            .src = &args->editor->caret_under[row * wide],
            .bytes = wide * sizeof(GuiColor)});
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = caret,
               .color =
                   EMBED_CALL(editor_caret_color, EditorCtx, .palette = args->palette, .ink = args->editor->caret_ink));
    return caret;
}

int gui_editor_place_rect(const GuiEditorPlaceCellCfg *args)
{
    int scale = args->scale;
    GuiRect *rect = args->rect;
    const int line_tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const float text_x =
        (float)(args->place.x + EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg,
                                           .editor_read = args->editor_read, .cell = args->cell, .scale = scale));
    unsigned int start = 0u;
    while ((start < args->editor_read->count) && (args->editor_read->line[start].number < args->editor_read->top))
    {
        start += 1u;
    }
    for (unsigned int at = start; at < args->editor_read->count; at += 1u)
    {
        if (args->editor_read->line[at].number == args->line)
        {
            // the row's count is far inside what an int represents
            const int row = (int)(at - start);
            const int y = args->place.y + (row * line_tall);
            if (y + line_tall > args->place.y + args->place.height)
            {
                return 0;
            }
            const int x = (int)roundf(
                text_x + ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor_read,
                                            .line_line_pointer = &args->editor_read->line[at], .col = args->col) *
                          args->cell));
            *rect = (GuiRect){x, y, (int)(args->cell + 0.5f), line_tall};
            return 1;
        }
    }
    return 0;
}

// the fold that opens on `line`, or NULL
EMBED_INLINE const GuiFold *editor_fold_on(const EditorCtx *args)
{
    for (unsigned int at = 0u; at < args->editor->folds; at += 1u)
    {
        if (args->editor->fold[at].first == args->line)
        {
            return &args->editor->fold[at];
        }
    }
    return NULL;
}

// the gutter's mark of a fold closed: a triangle pointing right, `tall` pixels tall, its point at the right, drawn
// with its left side at `x` and its middle on `middle`
EMBED_INLINE void editor_fold_mark(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const int half = args->tall / 2;
    for (int step = 0; step <= half; step += 1)
    {
        const int wide = half - step + 1;
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){args->x_i32, args->middle - step, wide, 1}, .other = args->clip),
                   .color = color);
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){args->x_i32, args->middle + step, wide, 1}, .other = args->clip),
                   .color = color);
    }
}

// the folded mark after a line that opens a fold closed, from `x`: a rounded ground two cells wide holding three
// dots, then the closer's text half a cell after it; how far it reaches
EMBED_INLINE float editor_folded_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    float x = args->x;
    unsigned int size = args->size;
    int scale = args->scale;
    GuiRect clip = args->clip;
    const int wide = (int)roundf(2.0f * args->cell);
    const int inset = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale);
    const GuiRect box = {(int)roundf(x), args->top + inset, wide, args->line_tall - (2 * inset)};
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = box, .other = clip);
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
    {
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = met,
                   .radius = (unsigned int)inset, .color = args->palette->same);
    }
    const int dot = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale);
    const int middle = args->top + (args->line_tall / 2);
    for (int at = 0; at < 3; at += 1)
    {
        const int dot_x = box.x + ((wide * (at + 1)) / 4) - (dot / 2);
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){dot_x, middle - (dot / 2), dot, dot}, .other = clip),
                   .color = args->palette->text_2);
    }
    float reach = x + (float)wide + (args->cell / 2.0f);
    if (args->fold->shown && (args->fold->closer_length != 0ull))
    {
        const char *const closer = args->editor->closers + args->fold->closer;
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_CODE, .size = size, .x_f32 = reach, .y = args->baseline, .text = closer,
                   .length = args->fold->closer_length, .color = args->palette->editor_text, .clip = clip);
        reach += EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                            .face = GUI_FACE_CODE, .size = size, .text = closer, .length = args->fold->closer_length);
    }
    return reach;
}

GuiColor gui_editor_color(const GuiEditorPaletteCfg *args)
{
    const GuiPalette *palette = args->palette;
    unsigned int name = args->name;
    if (name >= args->editor_read->name_count)
    {
        return palette->editor_text;
    }
    const char *const text = args->editor_read->names + args->editor_read->name_at[name];
    unsigned long long length = 0ull;
    length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette, .name_read = text,
                      .length = length);
}

// one line's text drawn from `x` on the baseline `y`, each letter in its run's color, a tab laid out to its stop
EMBED_INLINE void editor_line_draw(const EditorCtx *args)
{
    const GuiEditor *editor = args->editor;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    float x = args->x;
    int y = args->y;
    float cell = args->cell;
    unsigned int size = args->size;
    GuiRect clip = args->clip;
    const char *const text = editor->bytes + args->line_line_pointer->text;
    const unsigned int *const run = editor->run + args->line_line_pointer->runs;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    unsigned long long column = 0ull;
    unsigned int next = 0u;
    GuiColor color = palette->editor_text;
    const float start = x;
    while (at < args->line_line_pointer->length)
    {
        while ((next < args->line_line_pointer->run_count) && (units >= run[2u * next]))
        {
            color = EMBED_CALL(gui_editor_palette.color, GuiEditorPaletteCfg, .editor_read = editor, .palette = palette,
                               .name = run[(2u * next) + 1u]);
            next += 1u;
        }
        unsigned long long took = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text,
                                               .size = args->line_line_pointer->length, .at = at, .length = &took);
        at += (took != 0ull) ? took : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
        if (letter == '\t')
        {
            column += GUI_TAB_COLUMNS - (column % GUI_TAB_COLUMNS);
        }
        else
        {
            float advance = 0.0f;
            const GuiGlyphCell *const glyph =
                EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .letter = letter, .size = size, .color = color, .advance = &advance);
            if (glyph != NULL)
            {
                const float here = start + ((float)column * cell);
                // a pixel's place, rounded, is far inside what an int represents
                EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = raster, .cell = glyph,
                           .x = (int)roundf(here), .y = y, .clip = clip);
            }
            column += 1ull;
        }
        if (start + ((float)column * cell) > (float)(clip.x + clip.width))
        {
            return;
        }
    }
}

// a box drawn around `rect` on `raster`, its lines `thick` pixels wide inside the rect, in `color`, within `clip`
EMBED_INLINE void editor_box_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiRect rect = args->rect;
    GuiColor color = args->color;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x, rect.y, rect.width, args->thick}, .other = args->clip),
               .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x, rect.y + rect.height - args->thick, rect.width, args->thick},
                                  .other = args->clip),
               .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x, rect.y, args->thick, rect.height}, .other = args->clip),
               .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x + rect.width - args->thick, rect.y, args->thick, rect.height},
                                  .other = args->clip),
               .color = color);
}

// the matches on the line `line` drawn on the row `row_rect`, each cell they span in the match ground, the current one
// in the stronger ground and boxed where `boxed` is 1
EMBED_INLINE void editor_matches_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    int scale = args->scale;
    for (unsigned long long at = 0ull; at < args->editor->matches; at += 1ull)
    {
        if (args->editor->match[at][0] != args->line)
        {
            continue;
        }
        const int current =
            (args->editor->find.current >= 0ll) && ((unsigned long long)args->editor->find.current == at);
        const int left =
            (int)roundf(args->text_x +
                        ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                           .line_line_pointer = args->stored_line, .col = args->editor->match[at][1]) *
                         args->cell));
        const int right =
            (int)roundf(args->text_x +
                        ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                           .line_line_pointer = args->stored_line, .col = args->editor->match[at][2]) *
                         args->cell));
        const GuiRect span = {left, args->row_rect.y, right - left, args->row_rect.height};
        if (args->boxed)
        {
            if (current)
            {
                EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = span,
                           .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                           .color = args->palette->signal, .clip = args->clip);
            }
        }
        else
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = span, .other = args->clip),
                       .color = current ? args->palette->match_on : args->palette->match);
        }
    }
}

// find's bar drawn at the top right of `place`: its field with the query, or "Find" dim where the query is empty, the
// caret where the keys are its own, the toggles for case, whole words and a pattern, each lit where it is on, and the
// count, "No results" where there is none
EMBED_INLINE void editor_find_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    int scale = args->scale;
    unsigned int size = args->size;
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_PAD, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale);
    const int field_wide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_FIELD, .scale = scale);
    const int toggle_wide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_TOGGLE, .scale = scale);
    const int count_wide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_COUNT, .scale = scale);
    const int field_tall =
        args->line_tall + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale);
    const int wide = pad + field_wide + gap + (3 * (toggle_wide + gap)) + count_wide + pad;
    const GuiRect bar = {
        args->place.x + args->place.width -
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_RIGHT, .scale = scale) - wide,
        args->place.y, wide, field_tall + (2 * pad)};
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = bar, .other = args->place);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = met,
               .radius =
                   (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale),
               .color = args->palette->surface);
    EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = bar,
               .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
               .color = args->palette->widget_line, .clip = args->place);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    const GuiRect field = {bar.x + pad, bar.y + pad, field_wide, field_tall};
    const int baseline = field.y + (int)((((float)field_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = field, .other = args->place),
               .color = args->palette->editor_ground);
    EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = field,
               .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
               .color = args->editor->find.focus ? args->palette->hover : args->palette->line, .clip = args->place);
    const float text_x =
        (float)(field.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale));
    const GuiRect field_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = field, .other = args->place);
    if (args->editor->find.query_length == 0ull)
    {
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = size, .x_f32 = text_x, .y = baseline, .text = "Find", .length = 4ull,
                   .color = args->palette->text_3, .clip = field_clip);
    }
    else
    {
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = size, .x_f32 = text_x, .y = baseline,
                   .text = args->editor->find.query, .length = args->editor->find.query_length,
                   .color = args->palette->editor_text, .clip = field_clip);
    }
    if (args->editor->find.focus)
    {
        // the caret after as many letters as the cursor's units count, a letter past the basic plane two of them
        unsigned long long bytes = 0ull;
        unsigned long long units = 0ull;
        while ((bytes < args->editor->find.query_length) && (units < args->editor->find.at))
        {
            unsigned long long took = 0ull;
            const unsigned int letter =
                EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->editor->find.query,
                           .size = args->editor->find.query_length, .at = bytes, .length = &took);
            bytes += (took != 0ull) ? took : 1ull;
            units += (letter >= 0x10000u) ? 2ull : 1ull;
        }
        const float before =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                       .size = size, .text = args->editor->find.query, .length = bytes);
        EMBED_CALL(
            gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
            .rect = EMBED_CALL(
                gui_raster_one.rect_intersect, GuiRasterOneCfg,
                .one = (GuiRect){(int)roundf(text_x + before),
                                 field.y +
                                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale),
                                 EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                 field_tall - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 6,
                                                         .scale = scale)},
                .other = args->place),
            .color = args->palette->caret);
    }
    static const char *const s_toggle[3] = {"Aa", "ab", ".*"};
    const int on[3] = {args->editor->find.match_case, args->editor->find.word, args->editor->find.regex};
    int x = field.x + field_wide + gap;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const GuiRect toggle = {x, field.y, toggle_wide, field_tall};
        if (on[at])
        {
            EMBED_CALL(
                gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = toggle, .other = args->place),
                .radius =
                    (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale),
                .color = args->palette->selection);
            EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = toggle,
                       .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                       .color = args->palette->hover, .clip = args->place);
        }
        const float label_wide = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                            .face = GUI_FACE_HEAD, .size = size, .text = s_toggle[at], .length = 2ull);
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = size,
                   .x_f32 = (float)toggle.x + (((float)toggle_wide - label_wide) / 2.0f), .y = baseline,
                   .text = s_toggle[at], .length = 2ull,
                   .color = on[at] ? args->palette->editor_text : args->palette->text_2, .clip = args->place);
        x += toggle_wide + gap;
    }
    char count_text[64];
    VerbaLinea count = {count_text, sizeof(count_text), 0ull};
    if (args->editor->find.count == 0ull)
    {
        mmgr_line_put(&count, "No results");
    }
    else
    {
        // the current match is counted among the matches
        mmgr_line_decimal(
            &count, (args->editor->find.current >= 0ll) ? (unsigned long long)args->editor->find.current + 1ull : 0ull,
            1u);
        mmgr_line_put(&count, " of ");
        mmgr_line_decimal(&count, args->editor->find.count, 1u);
    }
    EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
               .face = GUI_FACE_HEAD, .size = size,
               .x_f32 = (float)(x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale)),
               .y = baseline, .text = count_text, .length = count.at, .color = args->palette->text_2,
               .clip = args->place);
}

void gui_editor_draw(const GuiEditorPlaceScaleCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    int scale = args->scale;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->place))
    {
        return;
    }
    // the code's size and its line in pixels at the scale, the size a whole pixel
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_SIZE, .scale = scale);
    const int line_tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = glyphs, .size = size);
    const int gutter = EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg, .editor_read = args->editor,
                                  .cell = cell, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_CODE, .size = size);
    // the baseline inside a row: the row's height less the face's, halved, then the ascent down
    const int baseline = (int)((((float)line_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    const int numbers_right =
        args->place.x + gutter -
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_GUTTER_AFTER, .scale = scale);
    const float text_x = (float)(args->place.x + gutter);
    const GuiRect text_clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place,
        .other = (GuiRect){args->place.x + gutter, args->place.y, args->place.width - gutter, args->place.height});
    const int rows = (line_tall > 0) ? (args->place.height + line_tall - 1) / line_tall : 0;
    // the selection's ends in order
    const int anchored = (args->editor->anchor.line < args->editor->cursor.line) ||
                         ((args->editor->anchor.line == args->editor->cursor.line) &&
                          (args->editor->anchor.col < args->editor->cursor.col));
    const GuiPlace selected_from = anchored ? args->editor->anchor : args->editor->cursor;
    const GuiPlace selected_to = anchored ? args->editor->cursor : args->editor->anchor;
    const int selected = args->editor->has_cursor &&
                         ((selected_from.line != selected_to.line) || (selected_from.col != selected_to.col));
    // the first row drawn is the first line stored at or below the view's top
    unsigned int start = 0u;
    while ((start < args->editor->count) && (args->editor->line[start].number < args->editor->top))
    {
        start += 1u;
    }
    for (int row = 0; row < rows; row += 1)
    {
        // a row's count is not negative
        const unsigned int at = start + (unsigned int)row;
        if ((at >= args->editor->count) || (args->editor->line[at].number >= args->editor->lines))
        {
            break;
        }
        const unsigned long long line = args->editor->line[at].number;
        const int y = args->place.y + (row * line_tall) + baseline;
        const GuiRect row_rect = {text_clip.x, args->place.y + (row * line_tall), text_clip.width, line_tall};
        const GuiLine *const stored_line = &args->editor->line[at];
        const GuiFold *const fold = EMBED_CALL(editor_fold_on, EditorCtx, .editor = args->editor, .line = line);
        // the current line's ground, the selection's span on the line, and the caret
        if (args->editor->has_cursor && (line == args->editor->cursor.line) && !selected)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = row_rect,
                                          .other = args->place),
                       .color = args->palette->current);
        }
        if (selected && (line >= selected_from.line) && (line <= selected_to.line))
        {
            const unsigned long long start_cell =
                (line == selected_from.line) ? EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                          .line_line_pointer = stored_line, .col = selected_from.col)
                                             : 0ull;
            const unsigned long long end_cell =
                (line == selected_to.line) ? EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                        .line_line_pointer = stored_line, .col = selected_to.col)
                                           : EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                        .line_line_pointer = stored_line, .col = ~0ull) +
                                                 1ull;
            const int left = (int)roundf(text_x + ((float)start_cell * cell));
            const int right = (int)roundf(text_x + ((float)end_cell * cell));
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){left, row_rect.y, right - left, line_tall}, .other = text_clip),
                       .color = args->palette->selection);
        }
        if (args->editor->find.open)
        {
            EMBED_CALL(editor_matches_draw, EditorCtx, .editor = args->editor, .stored_line = stored_line, .line = line,
                       .raster = raster, .palette = args->palette, .row_rect = row_rect, .text_x = text_x, .cell = cell,
                       .boxed = 0, .scale = scale, .clip = text_clip);
        }
        char digits[24];
        VerbaLinea number = {digits, sizeof(digits), 0ull};
        mmgr_line_decimal(&number, line + 1ull, 1u);
        const float number_wide = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                             .face = GUI_FACE_CODE, .size = size, .text = digits, .length = number.at);
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_CODE, .size = size, .x_f32 = (float)numbers_right - number_wide, .y = y,
                   .text = digits, .length = number.at, .color = args->palette->number, .clip = args->place);
        if ((fold != NULL) && fold->closed)
        {
            EMBED_CALL(editor_fold_mark, EditorCtx, .raster = raster,
                       .x_i32 = numbers_right +
                                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale),
                       .middle = row_rect.y + (line_tall / 2),
                       .tall = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale),
                       .color = args->palette->number, .clip = args->place);
        }
        EMBED_CALL(editor_line_draw, EditorCtx, .editor = args->editor, .line_line_pointer = stored_line,
                   .raster = raster, .glyphs = glyphs, .palette = args->palette, .x = text_x, .y = y, .cell = cell,
                   .size = size, .clip = text_clip);
        if ((fold != NULL) && fold->closed)
        {
            const float after = text_x + ((float)(EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                             .line_line_pointer = stored_line, .col = ~0ull) +
                                                  1ull) *
                                          cell);
            (void)EMBED_CALL(editor_folded_draw, EditorCtx, .editor = args->editor, .fold = fold, .raster = raster,
                             .glyphs = glyphs, .palette = args->palette, .x = after, .top = row_rect.y, .baseline = y,
                             .line_tall = line_tall, .cell = cell, .size = size, .scale = scale, .clip = text_clip);
        }
        for (unsigned int each = 0u; args->editor->has_pair && (each < 2u); each += 1u)
        {
            if (args->editor->pair[each].line == line)
            {
                const int left = (int)roundf(
                    text_x + ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                .line_line_pointer = stored_line, .col = args->editor->pair[each].col) *
                              cell));
                const int right =
                    (int)roundf(text_x + ((float)(EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                             .line_line_pointer = stored_line,
                                                             .col = args->editor->pair[each].col) +
                                                  1ull) *
                                          cell));
                EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster,
                           .rect = (GuiRect){left, row_rect.y, right - left, line_tall},
                           .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                           .color = args->palette->caret, .clip = text_clip);
            }
        }
        if (args->editor->find.open)
        {
            EMBED_CALL(editor_matches_draw, EditorCtx, .editor = args->editor, .stored_line = stored_line, .line = line,
                       .raster = raster, .palette = args->palette, .row_rect = row_rect, .text_x = text_x, .cell = cell,
                       .boxed = 1, .scale = scale, .clip = text_clip);
        }
    }
    if (args->editor->find.open)
    {
        EMBED_CALL(editor_find_draw, EditorCtx, .editor = args->editor, .raster = raster, .glyphs = glyphs,
                   .palette = args->palette, .place = args->place, .scale = scale, .size = size,
                   .line_tall = line_tall);
    }
}
