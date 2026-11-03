// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_merge.c: a notebook's cells as a diff compares them, and the cells two sides of a merge make of their base
#include "notebook_internal.h"

// A merge's work state, one cell sized to the cells of its three books: the cells of each side, where each base cell
// stands on each side, CLI_JSON_NONE where it stands on none, and the table of the longest runs of the same cells, a
// row for each base cell and one more, each as wide as the longer side and one more
typedef struct
{
    unsigned int *base;
    unsigned int *ours;
    unsigned int *theirs;
    unsigned int *ours_at;
    unsigned int *theirs_at;
    unsigned int *table;
} CliMergeWorkState;
// the most lines of one cell's source two cells are compared by
#define CLI_NOTEBOOK_LINES 4096u

int cli_notebook_for_diff(const CliNotebookTextCfg *args)
{
    const unsigned int book = EMBED_CALL(
        cli_notebook_path.read, CliNotebookPathCfg, .json = args->json, .path = "x.ipynb", .text = args->text,
        .size = args->size, .tokens = args->tokens, .tokens_max = args->tokens_max, .err_msg = args->err_msg);
    if (book == CLI_JSON_NONE)
    {
        return 0;
    }
    const unsigned long long begun = args->out->at;
    const unsigned int cells = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = book, .name = "cells");
    unsigned int index = 0u;
    for (unsigned int cell = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = cells, .kind = CFG_JSON_ARRAY)
                                 ? args->json->node[cells].first
                                 : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = args->json->node[cell].next)
    {
        index += 1u;
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = cell, .name = "cell_type"),
                       .length_pointer = NULL);
        mmgr_line_put(args->out, "# cell ");
        mmgr_line_decimal(args->out, index, 1u);
        mmgr_line_put(args->out, " (");
        mmgr_line_put(args->out, (named != NULL) ? named : "code");
        mmgr_line_put(args->out, ")\n");
        unsigned long long length = 0ULL;
        const char *const source = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                              .json_read = args->json, .cell = cell, .length_pointer = &length);
        mmgr_line_put_n(args->out, source, length);
        if (mmgr_line_ok(args->out) &&
            !EMBED_CALL(cli_notebook_text_internal.ends, CliNotebookTextCtx, .text = args->out->out + begun,
                        .length = args->out->at - begun, .ending = "\n"))
        {
            mmgr_line_ch(args->out, '\n');
        }
        const unsigned int outputs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = args->json, .object = cell, .name = "outputs");
        for (unsigned int output = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = args->json, .node = outputs, .kind = CFG_JSON_ARRAY)
                                       ? args->json->node[outputs].first
                                       : CLI_JSON_NONE;
             output != CLI_JSON_NONE; output = args->json->node[output].next)
        {
            const unsigned int data = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = args->json, .object = output, .name = "data");
            const char *displayed =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = output, .name = "text"),
                           .length_pointer = NULL);
            displayed =
                (displayed != NULL)
                    ? displayed
                    : EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                 .json_read = args->json,
                                 .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = args->json, .object = data, .name = "text/plain"),
                                 .length_pointer = NULL);
            const char *const evalue =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = output, .name = "evalue"),
                           .length_pointer = NULL);
            const char *const ename =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = output, .name = "ename"),
                           .length_pointer = NULL);
            const char *const typed =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = output, .name = "output_type"),
                           .length_pointer = NULL);
            mmgr_line_put(args->out, "## ");
            mmgr_line_put(args->out, (typed != NULL) ? typed : "output");
            if ((displayed != NULL) || (evalue != NULL))
            {
                mmgr_line_ch(args->out, '\n');
                if (displayed != NULL)
                {
                    mmgr_line_put(args->out, displayed);
                }
                else
                {
                    mmgr_line_put(args->out, (ename != NULL) ? ename : "error");
                    mmgr_line_put(args->out, ": ");
                    mmgr_line_put(args->out, evalue);
                }
                if (mmgr_line_ok(args->out) &&
                    !EMBED_CALL(cli_notebook_text_internal.ends, CliNotebookTextCtx, .text = args->out->out + begun,
                                .length = args->out->at - begun, .ending = "\n"))
                {
                    mmgr_line_ch(args->out, '\n');
                }
                continue;
            }
            mmgr_line_put(args->out, ": ");
            int first = 1;
            for (unsigned int kind = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = args->json, .node = data, .kind = CFG_JSON_OBJECT)
                                         ? args->json->node[data].first
                                         : CLI_JSON_NONE;
                 kind != CLI_JSON_NONE; kind = args->json->node[kind].next)
            {
                mmgr_line_put(args->out, first ? "" : ", ");
                mmgr_line_put(args->out, args->json->text + args->json->node[kind].key);
                first = 0;
            }
            mmgr_line_ch(args->out, '\n');
        }
    }
    return 1;
}

typedef struct
{
    const char *text;
    unsigned long long length;
    const char *(*lines)[2];
    const CliJson *json;
    unsigned int one;
    unsigned int other;
    const unsigned int *base_pointer;
    unsigned int n;
    const unsigned int *other_pointer;
    unsigned int m;
    unsigned int *found;
    unsigned int *runs;
    unsigned int book;
    unsigned int *cells;
    const unsigned int *sides;
    unsigned int bases;
    unsigned int index;
    CliJson *json_out;
    unsigned int entries;
    const char *key;
    unsigned int cell;
    unsigned int base;
    const CliMergeWorkState *work_state;
    unsigned int after;
    unsigned int ours;
    unsigned int theirs;
    unsigned int ours_index;
    unsigned int theirs_index;
} NotebookMergeCtx;
// the lines of a text, CLI_NOTEBOOK_LINES at the most, each its start and its end, into `lines` where it is given:
// their count
EMBED_INLINE unsigned int cli_lines_split(const NotebookMergeCtx *args)
{
    unsigned int count = 0u;
    const char *at = args->text;
    while ((at < (args->text + args->length)) && (count < CLI_NOTEBOOK_LINES))
    {
        const char *stop = NULL;
        const char *const next = EMBED_CALL(cli_notebook_at_internal.line, CliNotebookAtCtx, .at = at,
                                            .end = args->text + args->length, .stop = &stop);
        if (args->lines != NULL)
        {
            args->lines[count][0] = at;
            args->lines[count][1] = stop;
        }
        count += 1u;
        at = next;
    }
    return count;
}

// whether two cells' sources are alike enough to be one cell: at least half their lines shared, each line of
// one matched to the first line of the other like it not matched yet; the lines of both, and which of the other's are
// matched, in a cell of their count, two that cannot be given one not alike
EMBED_INLINE int cli_alike(const NotebookMergeCtx *args)
{
    const CliJson *json = args->json;
    unsigned int other = args->other;
    unsigned long long left_length = 0ULL;
    unsigned long long right_length = 0ULL;
    const char *const left_text = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                             .json_read = json, .cell = args->one, .length_pointer = &left_length);
    const char *const right_text = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                              .json_read = json, .cell = other, .length_pointer = &right_length);
    if ((left_length == right_length) &&
        (memor.cmp(&(MemoriaCfg){.src = left_text, .other = right_text, .bytes = left_length}) == 0))
    {
        return 1;
    }
    const unsigned int left =
        EMBED_CALL(cli_lines_split, NotebookMergeCtx, .text = left_text, .length = left_length, .lines = NULL);
    const unsigned int right =
        EMBED_CALL(cli_lines_split, NotebookMergeCtx, .text = right_text, .length = right_length, .lines = NULL);
    const char *(*const lines)[2] =
        ((left != 0u) && (right != 0u))
            ? g_cli_cells->persistent_buf_alloc(((left + right) * sizeof(*lines)) + right, &g_cli_notebook_cells)
            : NULL;
    if (lines == NULL)
    {
        return 0;
    }
    const char *(*const left_lines)[2] = lines;
    const char *(*const right_lines)[2] = lines + left;
    // the matched marks after the lines, one byte each
    unsigned char *const used = (unsigned char *)(void *)(lines + left + right);
    (void)EMBED_CALL(cli_lines_split, NotebookMergeCtx, .text = left_text, .length = left_length, .lines = left_lines);
    (void)EMBED_CALL(cli_lines_split, NotebookMergeCtx, .text = right_text, .length = right_length,
                     .lines = right_lines);
    memor.set(&(MemoriaCfg){.dst = used, .val = 0, .bytes = right});
    unsigned int shared = 0u;
    for (unsigned int line = 0u; line < left; line += 1u)
    {
        const unsigned long long size = (unsigned long long)(left_lines[line][1] - left_lines[line][0]);
        for (unsigned int at = 0u; at < right; at += 1u)
        {
            if (!used[at] && ((unsigned long long)(right_lines[at][1] - right_lines[at][0]) == size) &&
                (memor.cmp(&(MemoriaCfg){.src = left_lines[line][0], .other = right_lines[at][0], .bytes = size}) == 0))
            {
                used[at] = 1u;
                shared += 1u;
                break;
            }
        }
    }
    g_cli_cells->persistent_buf_release(lines, &g_cli_notebook_cells);
    // twice the lines in common over all the lines of both at least a half
    return (4ULL * shared) >= ((unsigned long long)left + right);
}

// Whether two cells are the same cell: by their ids where both carry one, and else of one kind and alike
EMBED_INLINE int cli_same_cell(const NotebookMergeCtx *args)
{
    const CliJson *json = args->json;
    unsigned int one = args->one;
    unsigned int other = args->other;
    const char *const left =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = one, .name = "id"),
                   .length_pointer = NULL);
    const char *const right =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = other, .name = "id"),
                   .length_pointer = NULL);
    if ((left != NULL) && (right != NULL))
    {
        return cellul.eq(&(CatenaFinitaCfg){.src = left, .other = right, .cap = CLI_CELLS_POOL});
    }
    return EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json,
                      .one = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = one, .name = "cell_type"),
                      .other_json = json,
                      .other = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = other, .name = "cell_type")) &&
           EMBED_CALL(cli_alike, NotebookMergeCtx, .json = json, .one = one, .other = other);
}

// For each cell of `base`, where the same cell stands in `other`, by the longest run of matches in order, as a diff
// aligns lines, the table of runs stored in `runs`, a row of `m` and one more for each of `n` and one more
static void cli_align(const NotebookMergeCtx *args)
{
    const unsigned long long width = (unsigned long long)args->m + 1ULL;
#define CLI_RUN(row_, column_) args->runs[((row_) * width) + (column_)]
    for (unsigned int j = 0u; j <= args->m; j += 1u)
    {
        CLI_RUN(args->n, j) = 0u;
    }
    for (unsigned int i = args->n; i > 0u; i -= 1u)
    {
        CLI_RUN(i - 1u, args->m) = 0u;
        for (unsigned int j = args->m; j > 0u; j -= 1u)
        {
            const unsigned int down = CLI_RUN(i, j - 1u);
            const unsigned int across = CLI_RUN(i - 1u, j);
            CLI_RUN(i - 1u, j - 1u) =
                EMBED_CALL(cli_same_cell, NotebookMergeCtx, .json = args->json, .one = args->base_pointer[i - 1u],
                           .other = args->other_pointer[j - 1u])
                    ? (CLI_RUN(i, j) + 1u)
                : (down > across) ? down
                                  : across;
        }
    }
    unsigned int i = 0u;
    unsigned int j = 0u;
    for (unsigned int at = 0u; at < args->n; at += 1u)
    {
        args->found[at] = CLI_JSON_NONE;
    }
    while ((i < args->n) && (j < args->m))
    {
        if (EMBED_CALL(cli_same_cell, NotebookMergeCtx, .json = args->json, .one = args->base_pointer[i],
                       .other = args->other_pointer[j]) &&
            (CLI_RUN(i, j) == (CLI_RUN(i + 1u, j + 1u) + 1u)))
        {
            args->found[i] = j;
            i += 1u;
            j += 1u;
        }
        else if (CLI_RUN(i + 1u, j) >= CLI_RUN(i, j + 1u))
        {
            i += 1u;
        }
        else
        {
            j += 1u;
        }
    }
#undef CLI_RUN
}

// the cells of a book's `cells`, at most CLI_NOTEBOOK_CELLS, into `cells` where it is given: their count
EMBED_INLINE unsigned int cli_cells(const NotebookMergeCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->book, .name = "cells");
    unsigned int count = 0u;
    for (unsigned int cell = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = list, .kind = CFG_JSON_ARRAY)
                                 ? json->node[list].first
                                 : CLI_JSON_NONE;
         (cell != CLI_JSON_NONE) && (count < CLI_NOTEBOOK_CELLS); cell = json->node[cell].next)
    {
        if (args->cells != NULL)
        {
            args->cells[count] = cell;
        }
        count += 1u;
    }
    return count;
}

// the base cell a cell one side added follows there: the nearest cell before it that stands for a base cell, by that
// base cell's place, or CLI_JSON_NONE for the head
EMBED_INLINE unsigned int cli_follows(const NotebookMergeCtx *args)
{
    for (unsigned int before = args->index; before > 0u; before -= 1u)
    {
        for (unsigned int base = 0u; base < args->bases; base += 1u)
        {
            if (args->sides[base] == (before - 1u))
            {
                return base;
            }
        }
    }
    return CLI_JSON_NONE;
}

// whether a side's cell `index` stands for a base cell
EMBED_INLINE int cli_on_alloc(const NotebookMergeCtx *args)
{
    for (unsigned int base = 0u; base < args->bases; base += 1u)
    {
        if (args->sides[base] == args->index)
        {
            return 1;
        }
    }
    return 0;
}

// `{"key": cell}` put after the entries, the cell copied
EMBED_INLINE void cli_entry(const NotebookMergeCtx *args)
{
    CliJson *json = args->json_out;
    const unsigned int entry =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = entry,
               .name = args->key,
               .value = EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json, .node = args->cell));
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = args->entries,
               .value = entry);
}

// whether a side's cell changed from its base cell: its source or its kind
EMBED_INLINE int cli_change(const NotebookMergeCtx *args)
{
    const CliJson *json = args->json;
    unsigned int cell = args->cell;
    unsigned long long length = 0ULL;
    unsigned long long base_length = 0ULL;
    const char *const source = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                          .json_read = json, .cell = cell, .length_pointer = &length);
    const char *const base_source = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                               .json_read = json, .cell = args->base, .length_pointer = &base_length);
    return (length != base_length) ||
           (memor.cmp(&(MemoriaCfg){.src = source, .other = base_source, .bytes = length}) != 0) ||
           !EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json,
                       .one = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = cell, .name = "cell_type"),
                       .other_json = json,
                       .other = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->base, .name = "cell_type"));
}

// whether two cells contain the same source
EMBED_INLINE int cli_same_source(const NotebookMergeCtx *args)
{
    const CliJson *json = args->json;
    unsigned int other = args->other;
    unsigned long long length = 0ULL;
    unsigned long long other_length = 0ULL;
    const char *const source = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                          .json_read = json, .cell = args->one, .length_pointer = &length);
    const char *const other_source = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                                .json_read = json, .cell = other, .length_pointer = &other_length);
    return (length == other_length) &&
           (memor.cmp(&(MemoriaCfg){.src = source, .other = other_source, .bytes = length}) == 0);
}

// the cells each side added after the base cell `after`, CLI_JSON_NONE for the head, put as entries: ours, then each
// of theirs no cell of ours added there contains the source of
EMBED_INLINE void cli_added_append(const NotebookMergeCtx *args)
{
    for (unsigned int index = 0u; index < args->ours; index += 1u)
    {
        if (!EMBED_CALL(cli_on_alloc, NotebookMergeCtx, .sides = args->work_state->ours_at, .bases = args->bases,
                        .index = index) &&
            (EMBED_CALL(cli_follows, NotebookMergeCtx, .sides = args->work_state->ours_at, .bases = args->bases,
                        .index = index) == args->after))
        {
            EMBED_CALL(cli_entry, NotebookMergeCtx, .json_out = args->json_out, .entries = args->entries, .key = "take",
                       .cell = args->work_state->ours[index]);
        }
    }
    for (unsigned int index = 0u; index < args->theirs; index += 1u)
    {
        if (EMBED_CALL(cli_on_alloc, NotebookMergeCtx, .sides = args->work_state->theirs_at, .bases = args->bases,
                       .index = index) ||
            (EMBED_CALL(cli_follows, NotebookMergeCtx, .sides = args->work_state->theirs_at, .bases = args->bases,
                        .index = index) != args->after))
        {
            continue;
        }
        int found = 0;
        for (unsigned int ours_index = 0u; !found && (ours_index < args->ours); ours_index += 1u)
        {
            found = !EMBED_CALL(cli_on_alloc, NotebookMergeCtx, .sides = args->work_state->ours_at,
                                .bases = args->bases, .index = ours_index) &&
                    (EMBED_CALL(cli_follows, NotebookMergeCtx, .sides = args->work_state->ours_at, .bases = args->bases,
                                .index = ours_index) == args->after) &&
                    EMBED_CALL(cli_same_source, NotebookMergeCtx, .json = args->json_out,
                               .one = args->work_state->ours[ours_index], .other = args->work_state->theirs[index]);
        }
        if (!found)
        {
            EMBED_CALL(cli_entry, NotebookMergeCtx, .json_out = args->json_out, .entries = args->entries, .key = "take",
                       .cell = args->work_state->theirs[index]);
        }
    }
}

// `{"conflict": {"base", "ours", "theirs"}}` put after the entries, a side that deleted the cell null
EMBED_INLINE void cli_conflict(const NotebookMergeCtx *args)
{
    CliJson *json = args->json_out;
    const unsigned int conflict =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = conflict,
               .name = "base",
               .value = EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json, .node = args->base));
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = conflict, .name = "ours",
        .value =
            (args->ours_index != CLI_JSON_NONE)
                ? EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json, .node = args->ours_index)
                : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_NULL));
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = conflict,
        .name = "theirs",
        .value =
            (args->theirs_index != CLI_JSON_NONE)
                ? EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json, .node = args->theirs_index)
                : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_NULL));
    const unsigned int entry =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = entry,
               .name = "conflict", .value = conflict);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = args->entries,
               .value = entry);
}

// the entry for one base cell, as each side stores it
EMBED_INLINE void cli_base_entry(const NotebookMergeCtx *args)
{
    CliJson *json = args->json_out;
    if ((args->ours_index != CLI_JSON_NONE) && (args->theirs_index != CLI_JSON_NONE))
    {
        const int ours_changed =
            EMBED_CALL(cli_change, NotebookMergeCtx, .json = json, .cell = args->ours_index, .base = args->base);
        const int theirs_changed =
            EMBED_CALL(cli_change, NotebookMergeCtx, .json = json, .cell = args->theirs_index, .base = args->base);
        if (!theirs_changed ||
            (ours_changed &&
             EMBED_CALL(cli_same_source, NotebookMergeCtx, .json = json, .one = args->ours_index,
                        .other = args->theirs_index) &&
             EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json,
                        .one = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->ours_index, .name = "cell_type"),
                        .other_json = json,
                        .other = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->theirs_index, .name = "cell_type"))))
        {
            EMBED_CALL(cli_entry, NotebookMergeCtx, .json_out = json, .entries = args->entries, .key = "take",
                       .cell = args->ours_index);
        }
        else if (!ours_changed)
        {
            EMBED_CALL(cli_entry, NotebookMergeCtx, .json_out = json, .entries = args->entries, .key = "take",
                       .cell = args->theirs_index);
        }
        else
        {
            EMBED_CALL(cli_conflict, NotebookMergeCtx, .json_out = json, .entries = args->entries, .base = args->base,
                       .ours_index = args->ours_index, .theirs_index = args->theirs_index);
        }
    }
    else if ((args->ours_index != CLI_JSON_NONE) &&
             EMBED_CALL(cli_change, NotebookMergeCtx, .json = json, .cell = args->ours_index, .base = args->base))
    {
        EMBED_CALL(cli_conflict, NotebookMergeCtx, .json_out = json, .entries = args->entries, .base = args->base,
                   .ours_index = args->ours_index, .theirs_index = CLI_JSON_NONE);
    }
    else if ((args->theirs_index != CLI_JSON_NONE) &&
             EMBED_CALL(cli_change, NotebookMergeCtx, .json = json, .cell = args->theirs_index, .base = args->base))
    {
        EMBED_CALL(cli_conflict, NotebookMergeCtx, .json_out = json, .entries = args->entries, .base = args->base,
                   .ours_index = CLI_JSON_NONE, .theirs_index = args->theirs_index);
    }
}

unsigned int cli_notebook_merge(const CliNotebookCfg *args)
{
    const unsigned int bases =
        EMBED_CALL(cli_cells, NotebookMergeCtx, .json = args->json, .book = args->base, .cells = NULL);
    const unsigned int our_count =
        EMBED_CALL(cli_cells, NotebookMergeCtx, .json = args->json, .book = args->ours, .cells = NULL);
    const unsigned int their_count =
        EMBED_CALL(cli_cells, NotebookMergeCtx, .json = args->json, .book = args->theirs, .cells = NULL);
    const unsigned long long wider = ((our_count > their_count) ? our_count : their_count) + 1ULL;
    const unsigned long long lists = (3ULL * bases) + our_count + their_count;
    unsigned int *const cell =
        g_cli_cells->temporary_buf_alloc((lists + ((bases + 1ULL) * wider)) * sizeof(*cell), &g_cli_notebook_cells);
    if (cell == NULL)
    {
        return CLI_JSON_NONE;
    }
    const CliMergeWorkState work_state = {cell,
                                          cell + bases,
                                          cell + bases + our_count,
                                          cell + bases + our_count + their_count,
                                          cell + (2u * bases) + our_count + their_count,
                                          cell + lists};
    (void)EMBED_CALL(cli_cells, NotebookMergeCtx, .json = args->json, .book = args->base, .cells = work_state.base);
    (void)EMBED_CALL(cli_cells, NotebookMergeCtx, .json = args->json, .book = args->ours, .cells = work_state.ours);
    (void)EMBED_CALL(cli_cells, NotebookMergeCtx, .json = args->json, .book = args->theirs, .cells = work_state.theirs);
    EMBED_CALL(cli_align, NotebookMergeCtx, .json = args->json, .base_pointer = work_state.base, .n = bases,
               .other_pointer = work_state.ours, .m = our_count, .found = work_state.ours_at, .runs = work_state.table);
    EMBED_CALL(cli_align, NotebookMergeCtx, .json = args->json, .base_pointer = work_state.base, .n = bases,
               .other_pointer = work_state.theirs, .m = their_count, .found = work_state.theirs_at,
               .runs = work_state.table);
    const unsigned int entries =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_ARRAY);
    EMBED_CALL(cli_added_append, NotebookMergeCtx, .json_out = args->json, .work_state = &work_state,
               .entries = entries, .after = CLI_JSON_NONE, .bases = bases, .ours = our_count, .theirs = their_count);
    for (unsigned int index = 0u; index < bases; index += 1u)
    {
        const unsigned int ours_index =
            (work_state.ours_at[index] != CLI_JSON_NONE) ? work_state.ours[work_state.ours_at[index]] : CLI_JSON_NONE;
        const unsigned int theirs_index = (work_state.theirs_at[index] != CLI_JSON_NONE)
                                              ? work_state.theirs[work_state.theirs_at[index]]
                                              : CLI_JSON_NONE;
        EMBED_CALL(cli_base_entry, NotebookMergeCtx, .json_out = args->json, .entries = entries,
                   .base = work_state.base[index], .ours_index = ours_index, .theirs_index = theirs_index);
        EMBED_CALL(cli_added_append, NotebookMergeCtx, .json_out = args->json, .work_state = &work_state,
                   .entries = entries, .after = index, .bases = bases, .ours = our_count, .theirs = their_count);
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_notebook_cells);
    const unsigned int book =
        EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = args->json, .node = args->ours);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = book,
               .name = "cells",
               .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                   .kind = CFG_JSON_ARRAY));
    const unsigned int merged =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = merged,
               .name = "book", .value = book);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = merged,
               .name = "entries", .value = entries);
    return args->json->full ? CLI_JSON_NONE : merged;
}
