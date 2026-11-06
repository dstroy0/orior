// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_grid.c: a statement's result as the grid draws it. The adopt reads the run's answer into a tree for the call
// alone and lays two cells: the head, a column a slot of numbers and a message line a slot, then every label, kind,
// line and the title as texts with their ends; and the stretch, a start, a length and a null flag a cell, then every
// cell's bytes. The result's sentence stands in a cell of its own, laid again as the rows read or the end change. The
// step follows the first row in view with its speed and the smoothed time a read takes, and reads the stretch the view
// heads into in one read of the run, the rows outside it let go
#include "db_grid.h"

#include "db_pane.h"

// the bytes a read of rows may answer; the words a waiting row and the row past the read rows stand as; and the word
// a null cell stands as
#define GRID_ANSWER (1ULL << 22u)
#define GRID_WAIT "\xE2\x80\xA6"
#define GRID_MORE "reading more\xE2\x80\xA6"
#define GRID_NULL "NULL"

// A column's numbers: its label's and its kind's places in the head's texts with their bytes, whether its values are
// numbers, its width and its start in letters from the first column
typedef struct
{
    unsigned long long label;
    unsigned long long label_length;
    unsigned long long kind;
    unsigned long long kind_length;
    int numeric;
    unsigned int width;
    unsigned long long start;
} GridColumn;

// A message line's place in the head's texts and its bytes
typedef struct
{
    unsigned long long text;
    unsigned long long length;
} GridLine;

// The grid: the head's cell, its columns, its lines, whether they are a failure's, its texts and the title's place
// and bytes in them; the run's connection; the stretch's cell, its first row, how many rows it stores, its cells'
// starts, lengths and null flags and its text; the rows read and whether the run ended, with its tag, its time and
// its zone; the sentence's cell and bytes; the column panned to; the rows the view reached; and the motion's first row
// followed, when, its speed in rows a second and the smoothed time a read takes
typedef struct
{
    unsigned char *head;
    GridColumn *column;
    unsigned int columns;
    GridLine *line;
    unsigned int lines;
    int failed;
    char *head_text;
    unsigned long long title;
    unsigned long long title_length;
    unsigned long long tag;
    unsigned long long tag_length;
    unsigned long long zone;
    unsigned long long zone_length;
    unsigned long long ms;
    CliDbOpen *open;
    unsigned char *stretch;
    unsigned long long first;
    unsigned long long stored;
    unsigned long long *cell_at;
    unsigned long long *cell_length;
    unsigned char *cell_null;
    char *cell_text;
    unsigned long long read;
    int done;
    char *sentence;
    unsigned long long sentence_length;
    unsigned int offset;
    unsigned long long view_first;
    unsigned long long view_last;
    unsigned long long motion_first;
    unsigned long long motion_ms;
    long long speed;
    unsigned long long read_ms;
    int refused;
} Grid;

static Grid s_grid;
static CliDbFailure s_failure;

typedef struct
{
    const CliJson *json;
    unsigned int node;
    unsigned int rows;
    unsigned long long from;
    const char *text;
    unsigned long long size;
    unsigned long long *length_out;
    unsigned long long count;
} GridCtx;

// the bytes of a string node, 0 for a null one
EMBED_INLINE unsigned long long string_length(const GridCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->json, .node = args->node, .length_pointer = &length);
    return (text != NULL) ? length : 0ULL;
}

// the letters of `size` bytes of `text`, each a lead byte of UTF-8
EMBED_INLINE unsigned long long letters_of(const GridCtx *args)
{
    unsigned long long letters = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        // a byte read as its bits
        letters += ((((unsigned char)args->text[at]) & 0xC0u) != 0x80u) ? 1ULL : 0ULL;
    }
    return letters;
}

// the stretch's cell released, no row stored
EMBED_INLINE void stretch_released(const GridCtx *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(s_grid.stretch, g_cli_db_pane_owner);
    s_grid.stretch = NULL;
    s_grid.stored = 0ULL;
    s_grid.first = 0ULL;
    s_grid.cell_at = NULL;
    s_grid.cell_length = NULL;
    s_grid.cell_null = NULL;
    s_grid.cell_text = NULL;
}

// the rows array `node` of the tree, `rows` of them from the row `from`, laid as the stretch in a cell sized to them:
// 1, or 0 where the cell cannot meet them, the stretch as it was
EMBED_INLINE int stretch_laid(const GridCtx *args)
{
    const unsigned long long cells = (unsigned long long)args->rows * s_grid.columns;
    unsigned long long text_bytes = 0ULL;
    for (unsigned int row = 0u; row < args->rows; row += 1u)
    {
        const unsigned int values = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = args->json, .array = args->node, .index = row);
        for (unsigned int column = 0u; column < s_grid.columns; column += 1u)
        {
            text_bytes += EMBED_CALL(string_length, GridCtx, .json = args->json,
                                     .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                        .json_read = args->json, .array = values, .index = column)) +
                          1ULL;
        }
    }
    const unsigned long long numbers = cells * 2ULL * sizeof(unsigned long long);
    const unsigned long long flags = (cells + 7ULL) & ~7ULL;
    unsigned char *const stretch =
        g_cli_guard->persistent_buf_alloc(numbers + flags + text_bytes + 1ULL, g_cli_db_pane_owner);
    if (stretch == NULL)
    {
        return 0;
    }
    EMBED_CALL(stretch_released, GridCtx, .rows = 0u);
    s_grid.stretch = stretch;
    s_grid.cell_at = (unsigned long long *)(void *)stretch;
    s_grid.cell_length = s_grid.cell_at + cells;
    s_grid.cell_null = stretch + numbers;
    s_grid.cell_text = (char *)(stretch + numbers + flags);
    unsigned long long used = 0ULL;
    for (unsigned int row = 0u; row < args->rows; row += 1u)
    {
        const unsigned int values = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = args->json, .array = args->node, .index = row);
        for (unsigned int column = 0u; column < s_grid.columns; column += 1u)
        {
            const unsigned int value = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                  .json_read = args->json, .array = values, .index = column);
            unsigned long long length = 0ULL;
            const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = args->json, .node = value, .length_pointer = &length);
            const unsigned long long at = ((unsigned long long)row * s_grid.columns) + column;
            s_grid.cell_at[at] = used;
            s_grid.cell_length[at] = (text != NULL) ? length : 0ULL;
            s_grid.cell_null[at] = (text == NULL) ? 1u : 0u;
            if ((text != NULL) && (length != 0ULL))
            {
                memor.cpy(&(MemoriaCfg){.dst = s_grid.cell_text + used, .src = text, .bytes = length});
            }
            s_grid.cell_text[used + s_grid.cell_length[at]] = '\0';
            used += s_grid.cell_length[at] + 1ULL;
        }
    }
    s_grid.first = args->from;
    s_grid.stored = args->rows;
    return 1;
}

// the result's sentence laid again in a cell sized to it: the title, the rows read with a plus while the server has
// more, the time, the zone and the tag, or the title and the failure
EMBED_INLINE void sentence_laid(const GridCtx *args)
{
    (void)args;
    char words[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    (void)mmgr_line_put_n(&line, s_grid.head_text + s_grid.title, s_grid.title_length);
    if (s_grid.failed)
    {
        (void)mmgr_line_put(&line, ": failed");
    }
    else
    {
        (void)mmgr_line_put(&line, ": ");
        mmgr_line_decimal(&line, s_grid.read, 1u);
        (void)mmgr_line_put(&line, s_grid.done ? " rows \xC2\xB7 " : "+ rows \xC2\xB7 ");
        mmgr_line_decimal(&line, s_grid.ms, 1u);
        (void)mmgr_line_put(&line, " ms");
        if (s_grid.zone_length != 0ULL)
        {
            (void)mmgr_line_put(&line, " \xC2\xB7 times in ");
            (void)mmgr_line_put_n(&line, s_grid.head_text + s_grid.zone, s_grid.zone_length);
        }
        if (s_grid.done && (s_grid.tag_length != 0ULL))
        {
            (void)mmgr_line_put(&line, " \xC2\xB7 ");
            (void)mmgr_line_put_n(&line, s_grid.head_text + s_grid.tag, s_grid.tag_length);
        }
    }
    const unsigned long long length = mmgr_line_finish(&line);
    char *const sentence = g_cli_guard->persistent_buf_alloc(length + 1ULL, g_cli_db_pane_owner);
    if (sentence == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = sentence, .src = words, .bytes = length + 1ULL});
    g_cli_guard->persistent_buf_release(s_grid.sentence, g_cli_db_pane_owner);
    s_grid.sentence = sentence;
    s_grid.sentence_length = length;
}

// a text of `size` bytes put into the head's texts at `count` bytes in, with its end: the place it stands at
EMBED_INLINE unsigned long long head_put(const GridCtx *args)
{
    if (args->size != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_grid.head_text + args->count, .src = args->text, .bytes = args->size});
    }
    s_grid.head_text[args->count + args->size] = '\0';
    return args->count;
}

// the head's cell taken for `count` columns, `rows` lines and `size` bytes of texts, the one before released: 1, or 0
// where the cell cannot meet it
EMBED_INLINE int head_taken(const GridCtx *args)
{
    const unsigned long long columns = ((unsigned long long)args->count * sizeof(GridColumn) + 7ULL) & ~7ULL;
    const unsigned long long lines = (unsigned long long)args->rows * sizeof(GridLine);
    unsigned char *const head =
        g_cli_guard->persistent_buf_alloc(columns + lines + args->size + 1ULL, g_cli_db_pane_owner);
    if (head == NULL)
    {
        return 0;
    }
    g_cli_guard->persistent_buf_release(s_grid.head, g_cli_db_pane_owner);
    s_grid.head = head;
    // the head's columns' count is a node's children, far inside an unsigned int
    s_grid.column = (GridColumn *)(void *)head;
    s_grid.columns = (unsigned int)args->count;
    s_grid.line = (GridLine *)(void *)(head + columns);
    s_grid.lines = args->rows;
    s_grid.head_text = (char *)(head + columns + lines);
    return 1;
}

void cli_db_grid_clear(const CliDbGridCfg *args)
{
    if ((args->open != NULL) && (args->open != s_grid.open))
    {
        return;
    }
    EMBED_CALL(stretch_released, GridCtx, .rows = 0u);
    g_cli_guard->persistent_buf_release(s_grid.head, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(s_grid.sentence, g_cli_db_pane_owner);
    memor.set(&(MemoriaCfg){.dst = &s_grid, .val = 0, .bytes = sizeof(s_grid)});
    if (args->title == NULL)
    {
        return;
    }
    // a run that has not answered yet: its title, and the sentence of its running
    const unsigned long long title_length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = CLI_NAME_MAX_LEN});
    if (!EMBED_CALL(head_taken, GridCtx, .count = 0ULL, .rows = 0u, .size = title_length))
    {
        return;
    }
    s_grid.title = EMBED_CALL(head_put, GridCtx, .text = args->title, .size = title_length, .count = 0ULL);
    s_grid.title_length = title_length;
    char words[CLI_NAME_MAX_LEN + 32u];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    (void)mmgr_line_put(&line, "Running on ");
    (void)mmgr_line_put(&line, args->title);
    (void)mmgr_line_put(&line, "\xE2\x80\xA6");
    const unsigned long long length = mmgr_line_finish(&line);
    s_grid.sentence = g_cli_guard->persistent_buf_alloc(length + 1ULL, g_cli_db_pane_owner);
    s_grid.sentence_length = (s_grid.sentence != NULL) ? length : 0ULL;
    if (s_grid.sentence != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_grid.sentence, .src = words, .bytes = length + 1ULL});
    }
}

// the failure `failure` adopted for the connection `title`: its lines, the message with its code, the detail and the
// hint, each a row
EMBED_INLINE int failure_adopted(const CliDbGridCfg *args)
{
    char written[3u * CLI_DB_FIELD + 128u];
    VerbaLinea out = {written, sizeof(written), 0ULL};
    EMBED_CALL(cli_db_wire_out.db_failure_write, CliDbWireOutCfg, .failure_read = args->failure, .out = &out);
    const unsigned long long length = mmgr_line_finish(&out);
    unsigned int lines = 1u;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        lines += (written[at] == '\n') ? 1u : 0u;
    }
    const unsigned long long title_length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = CLI_NAME_MAX_LEN});
    if (!EMBED_CALL(head_taken, GridCtx, .count = 0ULL, .rows = lines, .size = title_length + 1ULL + length))
    {
        return 0;
    }
    s_grid.title = EMBED_CALL(head_put, GridCtx, .text = args->title, .size = title_length, .count = 0ULL);
    s_grid.title_length = title_length;
    const unsigned long long text_at =
        EMBED_CALL(head_put, GridCtx, .text = written, .size = length, .count = title_length + 1ULL);
    unsigned int line = 0u;
    unsigned long long start = 0ULL;
    for (unsigned long long at = 0ULL; at <= length; at += 1ULL)
    {
        if ((at == length) || (written[at] == '\n'))
        {
            s_grid.line[line] = (GridLine){text_at + start, at - start};
            line += 1u;
            start = at + 1ULL;
        }
    }
    s_grid.failed = 1;
    s_grid.done = 1;
    EMBED_CALL(sentence_laid, GridCtx, .rows = 0u);
    return 1;
}

int cli_db_grid_adopt(const CliDbGridCfg *args)
{
    EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .title = NULL);
    if (args->answer == NULL)
    {
        return (args->failure != NULL) && EMBED_CALL(failure_adopted, CliDbGridCfg, .failure = args->failure,
                                                     .title = (args->title != NULL) ? args->title : "");
    }
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->answer, .size = args->size, .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg,
                   .owner = g_cli_db_pane_owner);
    if (root == CLI_JSON_NONE)
    {
        return 0;
    }
    const unsigned int columns = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "columns");
    const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = root, .name = "rows");
    const unsigned int done = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = root, .name = "done");
    const unsigned int notices = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "notices");
    const unsigned int zone = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = root, .name = "zone");
    const unsigned int ms = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                       .object = root, .name = "ms");
    const int has_columns = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                       .node = columns, .kind = CFG_JSON_ARRAY);
    const unsigned int column_count = has_columns ? json.node[columns].count : 0u;
    const unsigned int notice_count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                 .json_read = &json, .node = notices, .kind = CFG_JSON_ARRAY)
                                          ? json.node[notices].count
                                          : 0u;
    const unsigned long long tag_length = EMBED_CALL(string_length, GridCtx, .json = &json, .node = done);
    const unsigned long long zone_length = EMBED_CALL(string_length, GridCtx, .json = &json, .node = zone);
    // with no columns the rows are the notices, or what the statement did where it gave none
    const unsigned int lines = has_columns ? 0u : ((notice_count != 0u) ? notice_count : 1u);
    const unsigned long long title_length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = CLI_NAME_MAX_LEN});
    unsigned long long texts = title_length + 1ULL + tag_length + 1ULL + zone_length + 1ULL;
    for (unsigned int at = 0u; at < column_count; at += 1u)
    {
        const unsigned int column = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = &json, .array = columns, .index = at);
        texts += EMBED_CALL(string_length, GridCtx, .json = &json,
                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = &json, .object = column, .name = "name")) +
                 EMBED_CALL(string_length, GridCtx, .json = &json,
                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = &json, .object = column, .name = "kind")) +
                 2ULL;
    }
    for (unsigned int at = 0u; at < notice_count; at += 1u)
    {
        texts += EMBED_CALL(string_length, GridCtx, .json = &json,
                            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = &json, .array = notices, .index = at)) +
                 1ULL;
    }
    int adopted = EMBED_CALL(head_taken, GridCtx, .count = column_count, .rows = lines, .size = texts);
    unsigned long long used = 0ULL;
    if (adopted)
    {
        s_grid.title = EMBED_CALL(head_put, GridCtx, .text = args->title, .size = title_length, .count = used);
        s_grid.title_length = title_length;
        used += title_length + 1ULL;
        unsigned long long length = 0ULL;
        const char *const tag = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                           .json_read = &json, .node = done, .length_pointer = &length);
        s_grid.tag = EMBED_CALL(head_put, GridCtx, .text = tag, .size = tag_length, .count = used);
        s_grid.tag_length = tag_length;
        used += tag_length + 1ULL;
        const char *const zone_text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                 .json_read = &json, .node = zone, .length_pointer = &length);
        s_grid.zone = EMBED_CALL(head_put, GridCtx, .text = zone_text, .size = zone_length, .count = used);
        s_grid.zone_length = zone_length;
        used += zone_length + 1ULL;
        for (unsigned int at = 0u; at < column_count; at += 1u)
        {
            const unsigned int column = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                   .json_read = &json, .array = columns, .index = at);
            GridColumn *const laid = &s_grid.column[at];
            *laid = (GridColumn){0};
            const char *const label =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = column, .name = "name"),
                           .length_pointer = &length);
            laid->label_length = (label != NULL) ? length : 0ULL;
            laid->label = EMBED_CALL(head_put, GridCtx, .text = label, .size = laid->label_length, .count = used);
            used += laid->label_length + 1ULL;
            const char *const kind =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = column, .name = "kind"),
                           .length_pointer = &length);
            laid->kind_length = (kind != NULL) ? length : 0ULL;
            laid->kind = EMBED_CALL(head_put, GridCtx, .text = kind, .size = laid->kind_length, .count = used);
            used += laid->kind_length + 1ULL;
            laid->numeric = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                          .json_read = &json, .object = column, .name = "numeric"),
                                       .kind = CFG_JSON_TRUE);
        }
        for (unsigned int at = 0u; at < notice_count; at += 1u)
        {
            const char *const notice =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = &json, .array = notices, .index = at),
                           .length_pointer = &length);
            const unsigned long long notice_length = (notice != NULL) ? length : 0ULL;
            s_grid.line[at] = (GridLine){
                EMBED_CALL(head_put, GridCtx, .text = notice, .size = notice_length, .count = used), notice_length};
            used += notice_length + 1ULL;
        }
        if (!has_columns && (notice_count == 0u))
        {
            s_grid.line[0] = (GridLine){s_grid.tag, s_grid.tag_length};
        }
        // the first page of rows, as many as the answer carries
        const unsigned int page = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                             .json_read = &json, .node = rows, .kind = CFG_JSON_ARRAY)
                                      ? json.node[rows].count
                                      : 0u;
        adopted =
            !has_columns || EMBED_CALL(stretch_laid, GridCtx, .json = &json, .node = rows, .rows = page, .from = 0ULL);
        s_grid.read = has_columns ? page : 0ULL;
        s_grid.done = (tag_length != 0ULL) || !has_columns;
        unsigned long long ms_length = 0ULL;
        const char *const ms_text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                               .json_read = &json, .node = ms, .length_pointer = &ms_length);
        for (unsigned long long at = 0ULL; (ms_text == NULL) && (ms != CLI_JSON_NONE) && (at < json.node[ms].length);
             at += 1ULL)
        {
            // a digit's value, 0 to 9
            s_grid.ms = (s_grid.ms * 10ULL) + (unsigned long long)(json.text[json.node[ms].text + at] - '0');
        }
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_db_pane_owner);
    if (!adopted)
    {
        EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .title = NULL);
        return 0;
    }
    // each column's width the longest of its label and its first rows' cells, cut to the most, with the gap after,
    // and its start the widths before it
    const unsigned long long width_rows =
        (s_grid.stored < CLI_DB_GRID_WIDTH_ROWS) ? s_grid.stored : CLI_DB_GRID_WIDTH_ROWS;
    unsigned long long start = 0ULL;
    for (unsigned int column = 0u; column < s_grid.columns; column += 1u)
    {
        GridColumn *const laid = &s_grid.column[column];
        unsigned long long widest =
            EMBED_CALL(letters_of, GridCtx, .text = s_grid.head_text + laid->label, .size = laid->label_length);
        for (unsigned long long row = 0ULL; row < width_rows; row += 1ULL)
        {
            const unsigned long long at = (row * s_grid.columns) + column;
            const unsigned long long letters =
                s_grid.cell_null[at] ? sizeof(GRID_NULL) - 1ULL
                                     : EMBED_CALL(letters_of, GridCtx, .text = s_grid.cell_text + s_grid.cell_at[at],
                                                  .size = s_grid.cell_length[at]);
            widest = (letters > widest) ? letters : widest;
        }
        // the width is cut to the most, far inside an unsigned int
        laid->width =
            (unsigned int)((widest < CLI_DB_GRID_WIDTH_MOST) ? widest : CLI_DB_GRID_WIDTH_MOST) + CLI_DB_GRID_WIDTH_GAP;
        laid->start = start;
        start += laid->width;
    }
    s_grid.open = args->open;
    s_grid.view_first = 0ULL;
    s_grid.view_last = 0ULL;
    s_grid.read_ms = s_grid.ms;
    EMBED_CALL(sentence_laid, GridCtx, .rows = 0u);
    return 1;
}

const char *cli_db_grid_title(const CliDbGridCfg *args)
{
    *args->length_out = s_grid.title_length;
    return (s_grid.head_text != NULL) ? s_grid.head_text + s_grid.title : "";
}

const char *cli_db_grid_head_text(const CliDbGridCfg *args)
{
    *args->length_out = s_grid.sentence_length;
    return (s_grid.sentence != NULL) ? s_grid.sentence : "";
}

unsigned int cli_db_grid_columns(const CliDbGridCfg *args)
{
    (void)args;
    return s_grid.columns;
}

int cli_db_grid_column(const CliDbGridCfg *args)
{
    if (args->column_at >= s_grid.columns)
    {
        return 0;
    }
    const GridColumn *const laid = &s_grid.column[args->column_at];
    // the starts are sums of widths of at most forty letters, far inside a signed count
    *args->column = (CliDbGridColumn){s_grid.head_text + laid->label,
                                      laid->label_length,
                                      s_grid.head_text + laid->kind,
                                      laid->kind_length,
                                      laid->numeric,
                                      laid->width,
                                      (long long)laid->start - (long long)s_grid.column[s_grid.offset].start};
    return 1;
}

unsigned long long cli_db_grid_rows(const CliDbGridCfg *args)
{
    (void)args;
    if (s_grid.columns == 0u)
    {
        return s_grid.lines;
    }
    return s_grid.read + (s_grid.done ? 0ULL : 1ULL);
}

int cli_db_grid_row(const CliDbGridCfg *args)
{
    CliDbGridRow *const row = args->row;
    if (s_grid.columns == 0u)
    {
        if (args->at >= s_grid.lines)
        {
            return 0;
        }
        *row = (CliDbGridRow){s_grid.failed ? CLI_DB_GRID_ROW_FAILURE : CLI_DB_GRID_ROW_NOTICE,
                              s_grid.head_text + s_grid.line[args->at].text, s_grid.line[args->at].length};
        return 1;
    }
    if (args->at >= EMBED_CALL(cli_db_grid.rows, CliDbGridCfg, .at = 0ULL))
    {
        return 0;
    }
    if (args->at >= s_grid.read)
    {
        *row = (CliDbGridRow){CLI_DB_GRID_ROW_MORE, GRID_MORE, sizeof(GRID_MORE) - 1ULL};
        return 1;
    }
    const int stored = (args->at >= s_grid.first) && (args->at < s_grid.first + s_grid.stored);
    *row = stored ? (CliDbGridRow){CLI_DB_GRID_ROW_CELLS, "", 0ULL}
                  : (CliDbGridRow){CLI_DB_GRID_ROW_WAIT, GRID_WAIT, sizeof(GRID_WAIT) - 1ULL};
    return 1;
}

int cli_db_grid_cell(const CliDbGridCfg *args)
{
    if ((args->column_at >= s_grid.columns) || (args->at < s_grid.first) || (args->at >= s_grid.first + s_grid.stored))
    {
        return 0;
    }
    const unsigned long long at = ((args->at - s_grid.first) * s_grid.columns) + args->column_at;
    const int null = s_grid.cell_null[at] != 0u;
    *args->cell = (CliDbGridCell){null ? GRID_NULL : s_grid.cell_text + s_grid.cell_at[at],
                                  null ? sizeof(GRID_NULL) - 1ULL : s_grid.cell_length[at], null,
                                  s_grid.column[args->column_at].numeric};
    return 1;
}

void cli_db_grid_reach(const CliDbGridCfg *args)
{
    const unsigned long long last = (args->last >= args->first) ? args->last : args->first;
    s_grid.refused = s_grid.refused && (args->first == s_grid.view_first) && (last == s_grid.view_last);
    s_grid.view_first = args->first;
    s_grid.view_last = last;
}

// whether the stretch stored covers the rows in view the run has or can give
EMBED_INLINE int view_covered(const GridCtx *args)
{
    (void)args;
    const unsigned long long top = s_grid.done ? s_grid.read : s_grid.read + CLI_DB_GRID_PAGE;
    const unsigned long long seen_last = (s_grid.view_last < top) ? s_grid.view_last + 1ULL : top;
    return (s_grid.view_first >= top) ||
           ((s_grid.view_first >= s_grid.first) && (seen_last <= s_grid.first + s_grid.stored));
}

unsigned long long cli_db_grid_deadline_ms(const CliDbGridStepCfg *args)
{
    const int wanting = (s_grid.open != NULL) && (s_grid.columns != 0u) && !s_grid.refused &&
                        !EMBED_CALL(view_covered, GridCtx, .rows = 0u);
    return wanting ? args->now_ms : 0ULL;
}

unsigned long long cli_db_grid_stored(const CliDbGridCfg *args)
{
    *args->first_out = s_grid.first;
    return s_grid.stored;
}

CliDbOpen *cli_db_grid_open(const CliDbGridCfg *args)
{
    (void)args;
    return s_grid.open;
}

int cli_db_grid_read_ahead(const CliDbGridStepCfg *args)
{
    if ((s_grid.open == NULL) || (s_grid.columns == 0u))
    {
        return 0;
    }
    // the motion followed: the rows the view moved a second, smoothed, standing still after a rest
    const unsigned long long since = args->now_ms - s_grid.motion_ms;
    if (s_grid.view_first != s_grid.motion_first)
    {
        // the rows moved and the milliseconds between are far inside a signed count
        const long long moved = (long long)s_grid.view_first - (long long)s_grid.motion_first;
        const long long rate = (since != 0ULL) ? (moved * 1000LL) / (long long)since : moved * 1000LL;
        s_grid.speed =
            ((s_grid.speed * (100LL - (long long)CLI_DB_GRID_SHARE)) + (rate * (long long)CLI_DB_GRID_SHARE)) / 100LL;
        s_grid.motion_first = s_grid.view_first;
        s_grid.motion_ms = args->now_ms;
    }
    else if (since > CLI_DB_GRID_REST_MS)
    {
        s_grid.speed = 0LL;
    }
    // the stretch: the rows in view, the spare behind the motion, and ahead as far as the speed carries over two reads
    const unsigned long long speed =
        (s_grid.speed < 0LL) ? (unsigned long long)(-s_grid.speed) : (unsigned long long)s_grid.speed;
    const unsigned long long carried = (speed * 2ULL * s_grid.read_ms) / 1000ULL;
    const unsigned long long ahead = (carried < CLI_DB_GRID_SPARE)           ? CLI_DB_GRID_SPARE
                                     : (carried > (3ULL * CLI_DB_GRID_PAGE)) ? (3ULL * CLI_DB_GRID_PAGE)
                                                                             : carried;
    const int upward = s_grid.speed < 0LL;
    const unsigned long long before = upward ? ahead : CLI_DB_GRID_SPARE;
    const unsigned long long after = upward ? CLI_DB_GRID_SPARE : ahead;
    const unsigned long long top = s_grid.done ? s_grid.read : s_grid.read + CLI_DB_GRID_PAGE;
    const unsigned long long from = (s_grid.view_first > before) ? s_grid.view_first - before : 0ULL;
    const unsigned long long reach = s_grid.view_last + after + 1ULL;
    const unsigned long long to = (reach < top) ? reach : top;
    // nothing to read where the stretch stored covers the rows in view that the run has or can give
    if (EMBED_CALL(view_covered, GridCtx, .rows = 0u) || (from >= to))
    {
        return 0;
    }
    char *const answer = g_cli_guard->temporary_buf_alloc(GRID_ANSWER, g_cli_db_pane_owner);
    if (answer == NULL)
    {
        return 0;
    }
    VerbaLinea out = {answer, GRID_ANSWER, 0ULL};
    const unsigned long long started = cli_clock_milliseconds();
    const int read = EMBED_CALL(cli_db_out_failure_open.rows, CliDbOutFailureOpenCfg, .open = s_grid.open, .from = from,
                                .count_u64 = to - from, .out = &out, .failure = &s_failure);
    const unsigned long long took = cli_clock_milliseconds() - started;
    s_grid.read_ms = ((s_grid.read_ms * (100ULL - CLI_DB_GRID_SHARE)) + (took * CLI_DB_GRID_SHARE)) / 100ULL;
    int changed = 0;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        (read && mmgr_line_ok(&out))
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                         .json = &json, .text = answer, .size = out.at, .nodes_more = 0u, .text_more = 0ULL,
                         .err_msg = &err_msg, .owner = g_cli_db_pane_owner)
            : CLI_JSON_NONE;
    if (root != CLI_JSON_NONE)
    {
        const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                             .object = root, .name = "rows");
        const unsigned int count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = &json, .node = rows, .kind = CFG_JSON_ARRAY)
                                       ? json.node[rows].count
                                       : 0u;
        changed = EMBED_CALL(stretch_laid, GridCtx, .json = &json, .node = rows, .rows = count, .from = from);
        const unsigned int read_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = root, .name = "read");
        unsigned long long read_rows = 0ULL;
        for (unsigned long long at = 0ULL; (read_node != CLI_JSON_NONE) && (at < json.node[read_node].length);
             at += 1ULL)
        {
            // a digit's value, 0 to 9
            read_rows = (read_rows * 10ULL) + (unsigned long long)(json.text[json.node[read_node].text + at] - '0');
        }
        const unsigned int done = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                             .object = root, .name = "done");
        const unsigned long long tag_length = EMBED_CALL(string_length, GridCtx, .json = &json, .node = done);
        const int ended = tag_length != 0ULL;
        if ((read_rows != s_grid.read) || (ended != s_grid.done))
        {
            s_grid.read = read_rows;
            s_grid.done = ended;
            EMBED_CALL(sentence_laid, GridCtx, .rows = 0u);
            changed = 1;
        }
        g_cli_guard->persistent_buf_release(json.node, g_cli_db_pane_owner);
    }
    g_cli_guard->persistent_buf_release(answer, g_cli_db_pane_owner);
    // a read that leaves the view uncovered, refused or short of it, is not tried again until the view moves
    s_grid.refused = !EMBED_CALL(view_covered, GridCtx, .rows = 0u);
    return changed;
}

int cli_db_grid_pan(const CliDbGridStepCfg *args)
{
    if (s_grid.columns == 0u)
    {
        return 0;
    }
    // the columns are far fewer than a signed count reaches
    const long long wanted = (long long)s_grid.offset + (long long)args->columns;
    const long long last = (long long)s_grid.columns - 1LL;
    const long long panned = (wanted < 0LL) ? 0LL : ((wanted > last) ? last : wanted);
    // the offset is inside the columns, as clamped above
    const unsigned int offset = (unsigned int)panned;
    const int moved = offset != s_grid.offset;
    s_grid.offset = offset;
    return moved;
}

int cli_db_grid_key(const CliDbGridStepCfg *args)
{
    // the columns are far fewer than a signed count reaches
    const int all = (int)s_grid.columns;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Left", .cap = 16ULL}))
    {
        (void)EMBED_CALL(cli_db_grid_step.pan, CliDbGridStepCfg, .columns = -1);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Right", .cap = 16ULL}))
    {
        (void)EMBED_CALL(cli_db_grid_step.pan, CliDbGridStepCfg, .columns = 1);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Home", .cap = 16ULL}))
    {
        (void)EMBED_CALL(cli_db_grid_step.pan, CliDbGridStepCfg, .columns = -all);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "End", .cap = 16ULL}))
    {
        (void)EMBED_CALL(cli_db_grid_step.pan, CliDbGridStepCfg, .columns = all);
        return 1;
    }
    return 0;
}
