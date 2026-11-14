// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_grid.c: a statement's result as the grid draws it. The adopt reads the run's answer into a tree for the call
// alone and positions two movable cells, each positioned in a cell of the call and written once: the head, a column a slot of
// numbers and a message line a slot, then every label, type, line and the title as texts with their ends; and the
// stretch, a start, a length and a null flag a cell, then every cell's bytes. Every reader copies out of the cells. The
// result's sentence is in its own bytes, positioned again as the rows read or the end change. The step follows the first
// row in view with its speed and the smoothed time a read takes, and reads the stretch the view heads into in one read
// of the run, the rows outside it let go
#include "db_grid.h"

#include "db_pane.h"

// the bytes a read of rows may answer; the words a waiting row and the row past the read rows are as; and the word
// a null cell is as
#define GRID_ANSWER (1ULL << 22u)
#define GRID_WAIT "\xE2\x80\xA6"
#define GRID_MORE "reading more\xE2\x80\xA6"
#define GRID_NULL "NULL"

// A column's numbers: its label's and its type's places in the head's texts with their bytes, whether its values are
// numbers, its width and its start in characters from the first column
typedef struct
{
    unsigned long long label;
    unsigned long long label_length;
    unsigned long long type;
    unsigned long long type_length;
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

// The grid: the head's movable cell, its columns from its start, its lines from `line_at`, whether they are a
// failure's, its texts from `text_at`, and the head as it is positioned in a cell of the call with its columns, lines and
// texts, NULL between layings; the title in its own bytes and its place and bytes in the head's texts, as the tag's and
// the zone's; the run's connection; the stretch's movable cell, its first row, how many rows it stores, its cells'
// starts from its start, their lengths from `length_at`, their null flags from `null_at` and its text from
// `cell_text_at`; the rows read and whether the run ended, with its time; the sentence in its own bytes with its end,
// and how many; the column panned to; the rows the view reached; and the motion's first row followed, when, its speed
// in rows a second and the smoothed time a read takes
typedef struct
{
    const CarcerMovableEntry *head_cell;
    unsigned long long line_at;
    unsigned long long text_at;
    unsigned char *head_positioned;
    unsigned long long head_bytes;
    GridColumn *column;
    unsigned int columns;
    GridLine *line;
    unsigned int lines;
    int failed;
    char *head_text;
    char title_text[CLI_NAME_MAX_LEN];
    unsigned long long title;
    unsigned long long title_length;
    unsigned long long tag;
    unsigned long long tag_length;
    unsigned long long zone;
    unsigned long long zone_length;
    unsigned long long ms;
    CliDbOpen *open;
    const CarcerMovableEntry *stretch_cell;
    unsigned long long length_at;
    unsigned long long null_at;
    unsigned long long cell_text_at;
    unsigned long long first;
    unsigned long long stored;
    unsigned long long read;
    int done;
    char sentence[CLI_ERR_MSG_MAX_LEN];
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

// the place of the column `at_`'s numbers in the head cell, and of the line `at_`'s place in it
#define GRID_COLUMN_AT(at_) ((unsigned long long)(at_) * sizeof(GridColumn))
#define GRID_LINE_AT(at_) (s_grid.line_at + ((unsigned long long)(at_) * sizeof(GridLine)))

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
    char *dst;
    unsigned long long capacity;
    GridColumn *column_out;
    unsigned long long *start_out;
    unsigned long long *length_pointer;
    unsigned char *null_out;
} GridCtx;

// the bytes of a string node, 0 for a null one
EMBED_INLINE unsigned long long string_length(const GridCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->json, .node = args->node, .length_pointer = &length);
    return (text != NULL) ? length : 0ULL;
}

// the characters of `size` bytes of `text`, each a lead byte of UTF-8
EMBED_INLINE unsigned long long characters_of(const GridCtx *args)
{
    unsigned long long characters = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        // a byte read as its bits
        characters += ((((unsigned char)args->text[at]) & 0xC0u) != 0x80u) ? 1ULL : 0ULL;
    }
    return characters;
}

// `size` bytes of the head's texts from `from` copied into `dst` of `capacity` bytes with its end, cut to fit: how many
EMBED_INLINE unsigned long long head_text_read(const GridCtx *args)
{
    const unsigned long long copied = (args->size < args->capacity) ? args->size : args->capacity - 1ULL;
    if ((copied != 0ULL) && (s_grid.head_cell != NULL))
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_grid.head_cell, .offset = s_grid.text_at + args->from, .dst = args->dst, .bytes = copied});
    }
    args->dst[copied] = '\0';
    return copied;
}

// the column `count`'s numbers read out of the head cell into `column_out`
EMBED_INLINE void column_read(const GridCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_grid.head_cell,
                                                            .offset = GRID_COLUMN_AT(args->count),
                                                            .dst = args->column_out,
                                                            .bytes = sizeof(GridColumn)});
}

// the cell `count` of the stretch read: its start into `start_out`, its bytes into `length_pointer` and whether it is
// null into `null_out`
EMBED_INLINE void cell_numbers_read(const GridCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_grid.stretch_cell,
                                                            .offset = args->count * sizeof(unsigned long long),
                                                            .dst = args->start_out,
                                                            .bytes = sizeof(unsigned long long)});
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_grid.stretch_cell,
                            .offset = s_grid.length_at + (args->count * sizeof(unsigned long long)),
                            .dst = args->length_pointer,
                            .bytes = sizeof(unsigned long long)});
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_grid.stretch_cell, .offset = s_grid.null_at + args->count, .dst = args->null_out, .bytes = 1ULL});
}

// `size` bytes of the stretch's text from `from` copied into `dst` of `capacity` bytes with its end, cut to fit: how
// many
EMBED_INLINE unsigned long long cell_text_read(const GridCtx *args)
{
    const unsigned long long copied = (args->size < args->capacity) ? args->size : args->capacity - 1ULL;
    if (copied != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_grid.stretch_cell,
                                                                .offset = s_grid.cell_text_at + args->from,
                                                                .dst = args->dst,
                                                                .bytes = copied});
    }
    args->dst[copied] = '\0';
    return copied;
}

// the stretch's cell released, no row stored
EMBED_INLINE void stretch_released(const GridCtx *args)
{
    (void)args;
    if (s_grid.stretch_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_grid.stretch_cell, g_cli_db_pane_owner);
    }
    s_grid.stretch_cell = NULL;
    s_grid.stored = 0ULL;
    s_grid.first = 0ULL;
    s_grid.length_at = 0ULL;
    s_grid.null_at = 0ULL;
    s_grid.cell_text_at = 0ULL;
}

// the rows array `node` of the tree, `rows` of them from the row `from`, positioned as the stretch in a cell of this call
// and written once into a movable cell sized to them: 1, or 0 where the cells cannot meet them, the stretch as it was
EMBED_INLINE int stretch_positioned(const GridCtx *args)
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
    const unsigned long long bytes = numbers + flags + text_bytes + 1ULL;
    unsigned char *const stretch = g_cli_guard->temporary_buf_alloc(bytes, g_cli_db_pane_owner);
    if (stretch == NULL)
    {
        return 0;
    }
    unsigned long long *const cell_at = (unsigned long long *)(void *)stretch;
    unsigned long long *const cell_length = cell_at + cells;
    unsigned char *const cell_null = stretch + numbers;
    char *const cell_text = (char *)(stretch + numbers + flags);
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
            cell_at[at] = used;
            cell_length[at] = (text != NULL) ? length : 0ULL;
            cell_null[at] = (text == NULL) ? 1u : 0u;
            if ((text != NULL) && (length != 0ULL))
            {
                memor.cpy(&(MemoriaCfg){.dst = cell_text + used, .src = text, .bytes = length});
            }
            cell_text[used + cell_length[at]] = '\0';
            used += cell_length[at] + 1ULL;
        }
    }
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(bytes, g_cli_db_pane_owner);
    if (cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = stretch, .bytes = bytes});
        EMBED_CALL(stretch_released, GridCtx, .rows = 0u);
        s_grid.stretch_cell = cell;
        s_grid.length_at = cells * sizeof(unsigned long long);
        s_grid.null_at = numbers;
        s_grid.cell_text_at = numbers + flags;
        s_grid.first = args->from;
        s_grid.stored = args->rows;
    }
    g_cli_guard->persistent_buf_release(stretch, g_cli_db_pane_owner);
    return cell != NULL;
}

// the result's sentence positioned again in its bytes: the title, the rows read with a plus while the server has more, the
// time, the zone and the tag, or the title and the failure
EMBED_INLINE void sentence_positioned(const GridCtx *args)
{
    (void)args;
    VerbaLinea line = {s_grid.sentence, sizeof(s_grid.sentence), 0ULL};
    (void)mmgr_line_put_n(&line, s_grid.title_text, s_grid.title_length);
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
        char text[CLI_ERR_MSG_MAX_LEN];
        if (s_grid.zone_length != 0ULL)
        {
            (void)mmgr_line_put(&line, " \xC2\xB7 times in ");
            (void)mmgr_line_put_n(&line, text,
                                  EMBED_CALL(head_text_read, GridCtx, .from = s_grid.zone, .size = s_grid.zone_length,
                                             .dst = text, .capacity = sizeof(text)));
        }
        if (s_grid.done && (s_grid.tag_length != 0ULL))
        {
            (void)mmgr_line_put(&line, " \xC2\xB7 ");
            (void)mmgr_line_put_n(&line, text,
                                  EMBED_CALL(head_text_read, GridCtx, .from = s_grid.tag, .size = s_grid.tag_length,
                                             .dst = text, .capacity = sizeof(text)));
        }
    }
    s_grid.sentence_length = mmgr_line_finish(&line);
}

// a text of `size` bytes put into the head's texts as they are positioned at `count` bytes in, with its end: the place it
// is at
EMBED_INLINE unsigned long long head_put(const GridCtx *args)
{
    if (args->size != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_grid.head_text + args->count, .src = args->text, .bytes = args->size});
    }
    s_grid.head_text[args->count + args->size] = '\0';
    return args->count;
}

// the head positioned in a cell of this call for `count` columns, `rows` lines and `size` bytes of texts, its columns, lines
// and texts pointed into it until it is stored: 1, or 0 where the cell cannot meet it
EMBED_INLINE int head_taken(const GridCtx *args)
{
    const unsigned long long columns = ((unsigned long long)args->count * sizeof(GridColumn) + 7ULL) & ~7ULL;
    const unsigned long long lines = (unsigned long long)args->rows * sizeof(GridLine);
    const unsigned long long bytes = columns + lines + args->size + 1ULL;
    unsigned char *const head = g_cli_guard->temporary_buf_alloc(bytes, g_cli_db_pane_owner);
    if (head == NULL)
    {
        return 0;
    }
    s_grid.head_positioned = head;
    s_grid.head_bytes = bytes;
    // the head's columns' count is a node's children, far inside an unsigned int
    s_grid.column = (GridColumn *)(void *)head;
    s_grid.columns = (unsigned int)args->count;
    s_grid.line_at = columns;
    s_grid.line = (GridLine *)(void *)(head + columns);
    s_grid.lines = args->rows;
    s_grid.text_at = columns + lines;
    s_grid.head_text = (char *)(head + columns + lines);
    return 1;
}

// the head positioned written once into a movable cell sized to it, the one before released, and the cell of the call
// released: 1, or 0 where the cell cannot meet it, the head gone
EMBED_INLINE int head_stored(const GridCtx *args)
{
    (void)args;
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(s_grid.head_bytes, g_cli_db_pane_owner);
    if (cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = s_grid.head_positioned, .bytes = s_grid.head_bytes});
    }
    if (s_grid.head_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_grid.head_cell, g_cli_db_pane_owner);
    }
    s_grid.head_cell = cell;
    g_cli_guard->persistent_buf_release(s_grid.head_positioned, g_cli_db_pane_owner);
    s_grid.head_positioned = NULL;
    s_grid.head_bytes = 0ULL;
    s_grid.column = NULL;
    s_grid.line = NULL;
    s_grid.head_text = NULL;
    return cell != NULL;
}

// the title `text` of `size` bytes retained in the grid's own bytes
EMBED_INLINE void title_retained(const GridCtx *args)
{
    if (args->size != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_grid.title_text, .src = args->text, .bytes = args->size});
    }
    s_grid.title_text[args->size] = '\0';
    s_grid.title_length = args->size;
}

void cli_db_grid_clear(const CliDbGridCfg *args)
{
    if ((args->open != NULL) && (args->open != s_grid.open))
    {
        return;
    }
    EMBED_CALL(stretch_released, GridCtx, .rows = 0u);
    if (s_grid.head_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_grid.head_cell, g_cli_db_pane_owner);
    }
    g_cli_guard->persistent_buf_release(s_grid.head_positioned, g_cli_db_pane_owner);
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
    EMBED_CALL(title_retained, GridCtx, .text = args->title, .size = title_length);
    (void)EMBED_CALL(head_stored, GridCtx, .rows = 0u);
    VerbaLinea line = {s_grid.sentence, sizeof(s_grid.sentence), 0ULL};
    (void)mmgr_line_put(&line, "Running on ");
    (void)mmgr_line_put(&line, args->title);
    (void)mmgr_line_put(&line, "\xE2\x80\xA6");
    s_grid.sentence_length = mmgr_line_finish(&line);
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
    EMBED_CALL(title_retained, GridCtx, .text = args->title, .size = title_length);
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
    if (!EMBED_CALL(head_stored, GridCtx, .rows = 0u))
    {
        return 0;
    }
    EMBED_CALL(sentence_positioned, GridCtx, .rows = 0u);
    return 1;
}

// each column's width positioned as the longest of its label and its first rows' cells, cut to the max, with the gap
// after, and its start the widths before it, into the head as it is positioned
EMBED_INLINE void widths_positioned(const GridCtx *args)
{
    (void)args;
    const unsigned long long width_rows =
        (s_grid.stored < CLI_DB_GRID_WIDTH_ROWS) ? s_grid.stored : CLI_DB_GRID_WIDTH_ROWS;
    unsigned long long start = 0ULL;
    char text[CLI_DB_GRID_CELL_TEXT];
    for (unsigned int column = 0u; column < s_grid.columns; column += 1u)
    {
        GridColumn *const positioned = &s_grid.column[column];
        unsigned long long widest =
            EMBED_CALL(characters_of, GridCtx, .text = s_grid.head_text + positioned->label, .size = positioned->label_length);
        for (unsigned long long row = 0ULL; row < width_rows; row += 1ULL)
        {
            const unsigned long long at = (row * s_grid.columns) + column;
            unsigned long long cell_start = 0ULL;
            unsigned long long cell_length = 0ULL;
            unsigned char null = 0u;
            EMBED_CALL(cell_numbers_read, GridCtx, .count = at, .start_out = &cell_start,
                       .length_pointer = &cell_length, .null_out = &null);
            // the width is cut to the max characters, which the cell's first bytes copied out hold
            const unsigned long long characters =
                (null != 0u)
                    ? sizeof(GRID_NULL) - 1ULL
                    : EMBED_CALL(characters_of, GridCtx, .text = text,
                                 .size = EMBED_CALL(cell_text_read, GridCtx, .from = cell_start, .size = cell_length,
                                                    .dst = text, .capacity = sizeof(text)));
            widest = (characters > widest) ? characters : widest;
        }
        // the width is cut to the max, far inside an unsigned int
        positioned->width =
            (unsigned int)((widest < CLI_DB_GRID_WIDTH_MAX) ? widest : CLI_DB_GRID_WIDTH_MAX) + CLI_DB_GRID_WIDTH_GAP;
        positioned->start = start;
        start += positioned->width;
    }
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
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = args->answer, .size = args->size, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_db_pane_owner);
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
    const int has_columns = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                                       .node = columns, .type = CFG_JSON_ARRAY);
    const unsigned int column_count = has_columns ? json.node[columns].count : 0u;
    const unsigned int notice_count = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                 .json_read = &json, .node = notices, .type = CFG_JSON_ARRAY)
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
                                               .json_read = &json, .object = column, .name = "type")) +
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
        EMBED_CALL(title_retained, GridCtx, .text = args->title, .size = title_length);
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
            GridColumn *const positioned = &s_grid.column[at];
            *positioned = (GridColumn){0};
            const char *const label =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = column, .name = "name"),
                           .length_pointer = &length);
            positioned->label_length = (label != NULL) ? length : 0ULL;
            positioned->label = EMBED_CALL(head_put, GridCtx, .text = label, .size = positioned->label_length, .count = used);
            used += positioned->label_length + 1ULL;
            const char *const type =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = column, .name = "type"),
                           .length_pointer = &length);
            positioned->type_length = (type != NULL) ? length : 0ULL;
            positioned->type = EMBED_CALL(head_put, GridCtx, .text = type, .size = positioned->type_length, .count = used);
            used += positioned->type_length + 1ULL;
            positioned->numeric = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                          .json_read = &json, .object = column, .name = "numeric"),
                                       .type = CFG_JSON_TRUE);
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
        const unsigned int page = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                             .json_read = &json, .node = rows, .type = CFG_JSON_ARRAY)
                                      ? json.node[rows].count
                                      : 0u;
        adopted =
            !has_columns || EMBED_CALL(stretch_positioned, GridCtx, .json = &json, .node = rows, .rows = page, .from = 0ULL);
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
    if (adopted)
    {
        EMBED_CALL(widths_positioned, GridCtx, .rows = 0u);
        adopted = EMBED_CALL(head_stored, GridCtx, .rows = 0u);
    }
    if (!adopted)
    {
        EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .title = NULL);
        return 0;
    }
    s_grid.open = args->open;
    s_grid.view_first = 0ULL;
    s_grid.view_last = 0ULL;
    s_grid.read_ms = s_grid.ms;
    EMBED_CALL(sentence_positioned, GridCtx, .rows = 0u);
    return 1;
}

const char *cli_db_grid_title(const CliDbGridCfg *args)
{
    *args->length_out = s_grid.title_length;
    return s_grid.title_text;
}

const char *cli_db_grid_head_text(const CliDbGridCfg *args)
{
    *args->length_out = s_grid.sentence_length;
    return s_grid.sentence;
}

unsigned int cli_db_grid_columns(const CliDbGridCfg *args)
{
    (void)args;
    return s_grid.columns;
}

int cli_db_grid_column(const CliDbGridCfg *args)
{
    if ((args->column_at >= s_grid.columns) || (s_grid.head_cell == NULL))
    {
        return 0;
    }
    GridColumn positioned;
    GridColumn panned;
    EMBED_CALL(column_read, GridCtx, .count = args->column_at, .column_out = &positioned);
    EMBED_CALL(column_read, GridCtx, .count = s_grid.offset, .column_out = &panned);
    CliDbGridColumn *const column = args->column;
    *column = (CliDbGridColumn){0};
    column->label_length = EMBED_CALL(head_text_read, GridCtx, .from = positioned.label, .size = positioned.label_length,
                                      .dst = column->label_copied, .capacity = sizeof(column->label_copied));
    column->label = column->label_copied;
    column->type_length = EMBED_CALL(head_text_read, GridCtx, .from = positioned.type, .size = positioned.type_length,
                                     .dst = column->type_copied, .capacity = sizeof(column->type_copied));
    column->type = column->type_copied;
    column->numeric = positioned.numeric;
    column->width = positioned.width;
    // the starts are sums of widths of at max forty characters, far inside a signed count
    column->start = (long long)positioned.start - (long long)panned.start;
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

// the row `row` set to `type` with `size` bytes of `text`, which are as long as the program
EMBED_INLINE void row_worded(const GridCtx *args)
{
    CliDbGridRow *const row = (CliDbGridRow *)(void *)args->dst;
    *row = (CliDbGridRow){0};
    row->type = (unsigned int)args->count;
    row->text = args->text;
    row->length = args->size;
}

int cli_db_grid_row(const CliDbGridCfg *args)
{
    CliDbGridRow *const row = args->row;
    if (s_grid.columns == 0u)
    {
        if ((args->at >= s_grid.lines) || (s_grid.head_cell == NULL))
        {
            return 0;
        }
        GridLine line;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_grid.head_cell, .offset = GRID_LINE_AT(args->at), .dst = &line, .bytes = sizeof(line)});
        *row = (CliDbGridRow){0};
        row->type = s_grid.failed ? CLI_DB_GRID_ROW_FAILURE : CLI_DB_GRID_ROW_NOTICE;
        row->length = EMBED_CALL(head_text_read, GridCtx, .from = line.text, .size = line.length,
                                 .dst = row->text_copied, .capacity = sizeof(row->text_copied));
        row->text = row->text_copied;
        return 1;
    }
    if (args->at >= EMBED_CALL(cli_db_grid.rows, CliDbGridCfg, .at = 0ULL))
    {
        return 0;
    }
    if (args->at >= s_grid.read)
    {
        EMBED_CALL(row_worded, GridCtx, .dst = (char *)(void *)row, .count = CLI_DB_GRID_ROW_MORE, .text = GRID_MORE,
                   .size = sizeof(GRID_MORE) - 1ULL);
        return 1;
    }
    const int stored = (args->at >= s_grid.first) && (args->at < s_grid.first + s_grid.stored);
    if (stored)
    {
        EMBED_CALL(row_worded, GridCtx, .dst = (char *)(void *)row, .count = CLI_DB_GRID_ROW_CELLS, .text = "",
                   .size = 0ULL);
    }
    else
    {
        EMBED_CALL(row_worded, GridCtx, .dst = (char *)(void *)row, .count = CLI_DB_GRID_ROW_WAIT, .text = GRID_WAIT,
                   .size = sizeof(GRID_WAIT) - 1ULL);
    }
    return 1;
}

int cli_db_grid_cell(const CliDbGridCfg *args)
{
    if ((args->column_at >= s_grid.columns) || (args->at < s_grid.first) ||
        (args->at >= s_grid.first + s_grid.stored) || (s_grid.stretch_cell == NULL) || (s_grid.head_cell == NULL))
    {
        return 0;
    }
    const unsigned long long at = ((args->at - s_grid.first) * s_grid.columns) + args->column_at;
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    unsigned char null = 0u;
    EMBED_CALL(cell_numbers_read, GridCtx, .count = at, .start_out = &start, .length_pointer = &length,
               .null_out = &null);
    GridColumn column;
    EMBED_CALL(column_read, GridCtx, .count = args->column_at, .column_out = &column);
    CliDbGridCell *const cell = args->cell;
    *cell = (CliDbGridCell){0};
    cell->null = null != 0u;
    cell->numeric = column.numeric;
    if (cell->null)
    {
        cell->text = GRID_NULL;
        cell->length = sizeof(GRID_NULL) - 1ULL;
        return 1;
    }
    cell->length = EMBED_CALL(cell_text_read, GridCtx, .from = start, .size = length, .dst = cell->text_copied,
                              .capacity = sizeof(cell->text_copied));
    cell->text = cell->text_copied;
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
    // the motion followed: the rows the view moved a second, smoothed, stationary after a rest
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
    const unsigned int root = (read && mmgr_line_ok(&out))
                                  ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                               .json = &json, .text = answer, .size = out.at, .nodes_more = 0u,
                                               .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_db_pane_owner)
                                  : CLI_JSON_NONE;
    if (root != CLI_JSON_NONE)
    {
        const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                             .object = root, .name = "rows");
        const unsigned int count = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                              .json_read = &json, .node = rows, .type = CFG_JSON_ARRAY)
                                       ? json.node[rows].count
                                       : 0u;
        changed = EMBED_CALL(stretch_positioned, GridCtx, .json = &json, .node = rows, .rows = count, .from = from);
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
            EMBED_CALL(sentence_positioned, GridCtx, .rows = 0u);
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
