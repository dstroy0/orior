// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_store.c: the branches here, by key: stored whole as a branch is read, written by the page's edits, marked
// against the last commit's file, and searched. Each text's lines live in a cell sized to the text with capacity to
// grow, moved to a larger cell as an edit outgrows it; and the last commit's file in one sized to it. A paged text is a
// branch too, its lines in the pages and only their states here, carried through each edit the pages write
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "document_internal.h"
#include "pages.h"

// the most units and lines of a text stored
#define CLI_DOC_UNITS (1u << 22u)
#define CLI_DOC_LINES (1u << 18u)

// the words each line saves beside its text: the states it is colored from and ends in, its regions and its runs' place
#define CLI_DOC_LINE_WORDS 4ULL

// the least units and lines a text's capacity is given past what it contains, and the share of what it contains added
// as well, the capacity an edit fits in where it stands
#define CLI_DOC_RESERVE_UNITS 1024ULL
#define CLI_DOC_RESERVE_LINES 64u
#define CLI_DOC_RESERVE_SHARE 8ULL

// the first of the texts stored, the lock they are read and written under, and the last serial a text took
static CliStoredText *s_stored;
static unsigned long long s_serial;

CliStoredText *cli_document_texts(void)
{
    return s_stored;
}

void cli_document_serial_move(const CliDocumentCtx *args)
{
    s_serial += 1ULL;
    args->one->serial = s_serial;
}

void cli_document_holder_add(const CliDocumentCtx *args)
{
    args->one->readers += 1u;
}

typedef struct
{
    const char *text;
    unsigned long long size;
    CliLines *lines;
    unsigned long long units_max;
    unsigned int lines_max;
    const char *what;
    unsigned long long count;
    const char *key;
    CliStoredText *one;
    CliShape *shape;
    const CliEdit *edit;
    const unsigned short *units;
    const CliLines *from_lines_pointer;
    unsigned long long line;
    unsigned short *unit;
    unsigned long long from;
    unsigned long long to;
    unsigned long long *place;
    unsigned int *state;
    CliLines *now;
    CliLines *reserve;
    const CliLines *lines_read;
    const CliPosition *position_position_pointer;
    const CliLines *now_read;
} DocumentStoreCtx;
// how many lines `size` bytes of `text` make, one more than its newlines
EMBED_INLINE unsigned int stored_lines_in(const DocumentStoreCtx *args)
{
    unsigned long long count = 1ULL;
    for (unsigned long long at = 0ULL; at < args->size; count += 1ULL)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = args->text + at, .val = '\n', .bytes = args->size - at});
        if (newline == NULL)
        {
            break;
        }
        // the newline lies in the text, at or past `at`
        at = (unsigned long long)(newline - args->text) + 1ULL;
    }
    // the lines are bounded by CLI_DOC_LINES, which an unsigned int stores
    return (unsigned int)((count < CLI_DOC_LINES) ? count : CLI_DOC_LINES);
}

// `lines` given a capacity of `units_max` units and `lines_max` lines in a cell of its own, each bounded by the most a
// text may have: 1, or 0 with `lines` empty where the cells cannot meet it
EMBED_INLINE int stored_capacity_enter(const DocumentStoreCtx *args)
{
    CliLines *lines = args->lines;
    const unsigned long long units = (args->units_max < CLI_DOC_UNITS) ? args->units_max : CLI_DOC_UNITS;
    const unsigned int count = (args->lines_max < CLI_DOC_LINES) ? args->lines_max : CLI_DOC_LINES;
    // the units' bytes rounded up to a boundary of the starts after them
    const unsigned long long unit_bytes = ((units * sizeof(unsigned short)) + 7ULL) & ~7ULL;
    // each line's start and length, then the two states it is colored from and ends in, its regions and its runs' place
    const unsigned long long bytes = unit_bytes + (2ULL * (unsigned long long)count * sizeof(unsigned long long)) +
                                     (CLI_DOC_LINE_WORDS * (unsigned long long)count * sizeof(unsigned int));
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_editor_cells);
    *lines = (CliLines){NULL, 0ULL, 0ULL, NULL, NULL, 0u, 0u, 0};
    if (cell == NULL)
    {
        return 0;
    }
    unsigned long long *const start = (unsigned long long *)(void *)(cell + unit_bytes);
    unsigned int *const begun = (unsigned int *)(void *)(start + (2ULL * count));
    *lines = (CliLines){(unsigned short *)(void *)cell, units, 0ULL, start, start + count, count, 0u, 0};
    lines->begun = begun;
    lines->ended = begun + count;
    lines->regions = begun + (2ULL * count);
    lines->runs = begun + (3ULL * count);
    return 1;
}

// a capacity given back, its cell starting at its units or, for a paged text's, at its states, `lines` left empty
EMBED_INLINE void stored_capacity_exit(const DocumentStoreCtx *args)
{
    CliLines *lines = args->lines;
    g_cli_cells->persistent_buf_release((lines->unit != NULL) ? (void *)lines->unit : (void *)lines->begun,
                                        &g_cli_editor_cells);
    *lines = (CliLines){NULL, 0ULL, 0ULL, NULL, NULL, 0u, 0u, 0};
}

// a paged text's capacity grown to hold the states of `count` lines with capacity to grow past them, its states saved,
// `lines` counting `count`, each line it did not count not colored: 1, or 0 where the cells cannot meet it, the
// capacity as it was
static int stored_states_grow(const DocumentStoreCtx *args)
{
    CliLines *lines = args->lines;
    if (args->count > 0xffffffffULL)
    {
        return 0;
    }
    if (args->count > lines->lines_max)
    {
        const unsigned long long capacity = args->count + (args->count / CLI_DOC_RESERVE_SHARE) + CLI_DOC_RESERVE_LINES;
        // the lines a capacity holds are counted by an unsigned int
        const unsigned int most = (capacity < 0xffffffffULL) ? (unsigned int)capacity : 0xffffffffu;
        unsigned int *const begun =
            g_cli_cells->persistent_buf_alloc(CLI_DOC_LINE_WORDS * most * sizeof(unsigned int), &g_cli_editor_cells);
        if (begun == NULL)
        {
            return 0;
        }
        if (lines->lines != 0u)
        {
            memor.cpy(&(MemoriaCfg){.dst = begun, .src = lines->begun, .bytes = lines->lines * sizeof(unsigned int)});
            memor.cpy(
                &(MemoriaCfg){.dst = begun + most, .src = lines->ended, .bytes = lines->lines * sizeof(unsigned int)});
            memor.cpy(&(MemoriaCfg){
                .dst = begun + (2ULL * most), .src = lines->regions, .bytes = lines->lines * sizeof(unsigned int)});
            memor.cpy(&(MemoriaCfg){
                .dst = begun + (3ULL * most), .src = lines->runs, .bytes = lines->lines * sizeof(unsigned int)});
        }
        const unsigned int counted = lines->lines;
        EMBED_CALL(stored_capacity_exit, DocumentStoreCtx, .lines = lines);
        lines->begun = begun;
        lines->ended = begun + most;
        lines->regions = begun + (2ULL * most);
        lines->runs = begun + (3ULL * most);
        lines->lines_max = most;
        lines->lines = counted;
    }
    for (unsigned int line = lines->lines; line < args->count; line += 1u)
    {
        lines->begun[line] = CLI_DOC_UNCOLORED;
        lines->ended[line] = CLI_DOC_UNCOLORED;
        lines->regions[line] = 0u;
        lines->runs[line] = CLI_DOC_NO_RUNS;
    }
    // the count is no more than the capacity, which an unsigned int counts
    lines->lines = (args->count > lines->lines) ? (unsigned int)args->count : lines->lines;
    return 1;
}

int cli_document_paged_count(const CliDocumentCtx *args)
{
    unsigned long long count = 0ULL;
    int whole = 0;
    if (!EMBED_CALL(cli_pages.count, CliPagesCfg, .key = args->one->key, .lines = &count, .whole = &whole))
    {
        return 1;
    }
    args->one->whole = whole;
    return (count <= args->one->lines.lines) ||
           EMBED_CALL(stored_states_grow, DocumentStoreCtx, .lines = &args->one->lines, .count = count);
}

// the capacity `size` bytes of `text` take with capacity to grow, taken for `lines` in place of what it contained: 1,
// or 0 where the cells cannot meet it
EMBED_INLINE int stored_capacity_for(const DocumentStoreCtx *args)
{
    stored_capacity_exit(args);
    const unsigned long long units = args->size + (args->size / CLI_DOC_RESERVE_SHARE) + CLI_DOC_RESERVE_UNITS;
    const unsigned int count = stored_lines_in(args);
    return EMBED_CALL(stored_capacity_enter, DocumentStoreCtx, .lines = args->lines, .units_max = units,
                      .lines_max = count + (count / CLI_DOC_RESERVE_SHARE) + CLI_DOC_RESERVE_LINES, .what = args->what);
}

CliStoredText *cli_document_on_store(const CliDocumentKeyCtx *args)
{
    for (CliStoredText *one = s_stored; one != NULL; one = one->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = args->key, .cap = CLI_CELLS_POOL}))
        {
            return one;
        }
    }
    return NULL;
}

// the text stored under `key`, made afresh in a cell of its own where none is: NULL where the cells cannot meet it
EMBED_INLINE CliStoredText *cli_stored_init(const DocumentStoreCtx *args)
{
    const char *key = args->key;
    CliStoredText *const there = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = key);
    if (there != NULL)
    {
        return there;
    }
    CliStoredText *const one = g_cli_cells->persistent_buf_alloc(sizeof(*one), &g_cli_editor_cells);
    if (one == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    one->next = s_stored;
    s_stored = one;
    return one;
}

// a text's cells given back, the text's own last
EMBED_INLINE void stored_cells_free(const DocumentStoreCtx *args)
{
    EMBED_CALL(stored_capacity_exit, DocumentStoreCtx, .lines = &args->one->lines);
    EMBED_CALL(stored_capacity_exit, DocumentStoreCtx, .lines = &args->one->head);
    g_cli_cells->persistent_buf_release(args->one->state, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(args->one->state_index, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(args->one->roles, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(args->one->run, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(args->one, &g_cli_editor_cells);
}

// a text stored let go: taken off the texts stored, and its cells released, at once where nothing pins it, else as
// the last to pin it ends
EMBED_INLINE void cli_stored_let_go(const DocumentStoreCtx *args)
{
    CliStoredText **link = &s_stored;
    while ((*link != NULL) && (*link != args->one))
    {
        link = &(*link)->next;
    }
    if (*link != NULL)
    {
        *link = args->one->next;
    }
    args->one->let_go = 1;
    if (args->one->readers == 0u)
    {
        stored_cells_free(args);
    }
}

void cli_document_holder_end(const CliDocumentCtx *args)
{
    args->one->readers -= (args->one->readers != 0u) ? 1u : 0u;
    if (args->one->let_go && (args->one->readers == 0u))
    {
        EMBED_CALL(stored_cells_free, DocumentStoreCtx, .one = args->one);
    }
}

// `size` bytes of `text` made the lines of the text stored, in a capacity sized to them: 1, or 0 where the cells cannot
// meet it, the text let go
EMBED_INLINE int stored_lines_init(const DocumentStoreCtx *args)
{
    CliStoredText *one = args->one;
    const char *text = args->text;
    unsigned long long size = args->size;
    CliShape *shape = args->shape;
    if (!EMBED_CALL(stored_capacity_for, DocumentStoreCtx, .lines = &one->lines, .text = text, .size = size,
                    .what = "a text stored's lines"))
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
        return 0;
    }
    EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = text, .size = size,
               .lines_out = &one->lines);
    EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = one);
    one->head_known = 0;
    one->changes = 0ULL;
    one->paged = 0;
    one->whole = 1;
    one->regions_moved = 1;
    one->cached_from = 0u;
    one->cached_to = CLI_DOC_CACHED_ALL;
    *shape = EMBED_CALL(cli_document_lines_out.shape_get, CliDocumentLinesOutCfg, .lines = &one->lines);
    return 1;
}

// the text stored under `key` where it is read whole, NULL where none is or it is a paged text's branch
EMBED_INLINE CliStoredText *stored_full_text_get(const DocumentStoreCtx *args)
{
    const char *key = args->key;
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = key);
    return ((one != NULL) && !one->paged) ? one : NULL;
}

int cli_document_page(const CliDocumentKeyFromCfg *args)
{
    unsigned long long count = 0ULL;
    int whole = 0;
    if (!EMBED_CALL(cli_pages.count, CliPagesCfg, .key = args->key, .lines = &count, .whole = &whole))
    {
        return 0;
    }
    CliStoredText *const one = EMBED_CALL(cli_stored_init, DocumentStoreCtx, .key = args->key);
    if (one != NULL)
    {
        EMBED_CALL(stored_capacity_exit, DocumentStoreCtx, .lines = &one->lines);
        EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = one);
        one->paged = 1;
        one->whole = whole;
        one->changes = 0ULL;
        one->head_known = 0;
        one->regions_moved = 1;
        // a line the page holds is one the pages count, which an unsigned int counts
        one->cached_from = (args->from < CLI_DOC_CACHED_ALL) ? (unsigned int)args->from : CLI_DOC_CACHED_ALL;
        one->cached_to = (args->to < CLI_DOC_CACHED_ALL) ? (unsigned int)args->to : CLI_DOC_CACHED_ALL;
    }
    const int grown =
        (one != NULL) && EMBED_CALL(stored_states_grow, DocumentStoreCtx, .lines = &one->lines, .count = count);
    if ((one != NULL) && !grown)
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
    }
    return grown;
}

// the states of a paged text's lines carried through one edit the pages wrote: the lines from its first to its last
// taken out and those its text writes put in their place, not colored, and the lines after moved with them: 1, or 0
// where the cells cannot meet the lines it adds
EMBED_INLINE int stored_states_edit(const DocumentStoreCtx *args)
{
    CliLines *lines = args->lines;
    unsigned long long written = 1ULL;
    for (const char *at = memor.chr(&(MemoriaCfg){.src = args->edit->text, .val = '\n', .bytes = args->edit->length});
         at != NULL;
         at = memor.chr(&(MemoriaCfg){.src = at + 1,
                                      .val = '\n',
                                      .bytes = args->edit->length - (unsigned long long)(at + 1 - args->edit->text)}))
    {
        written += 1ULL;
    }
    const unsigned long long first = args->edit->from.line;
    const unsigned long long after = args->edit->to.line + 1ULL;
    if ((first > args->edit->to.line) || (after > lines->lines))
    {
        return 1;
    }
    const unsigned long long count = lines->lines - (after - first) + written;
    const unsigned long long moved = lines->lines - after;
    if ((count > lines->lines) && !EMBED_CALL(stored_states_grow, DocumentStoreCtx, .lines = lines, .count = count))
    {
        return 0;
    }
    const unsigned long long to = first + written;
    if (to <= after)
    {
        memor.move_down(&(MemoriaCfg){
            .dst = lines->begun + to, .src = lines->begun + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_down(&(MemoriaCfg){
            .dst = lines->ended + to, .src = lines->ended + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_down(&(MemoriaCfg){
            .dst = lines->regions + to, .src = lines->regions + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_down(
            &(MemoriaCfg){.dst = lines->runs + to, .src = lines->runs + after, .bytes = moved * sizeof(unsigned int)});
    }
    else
    {
        memor.move_up(&(MemoriaCfg){
            .dst = lines->begun + to, .src = lines->begun + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_up(&(MemoriaCfg){
            .dst = lines->ended + to, .src = lines->ended + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_up(&(MemoriaCfg){
            .dst = lines->regions + to, .src = lines->regions + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_up(
            &(MemoriaCfg){.dst = lines->runs + to, .src = lines->runs + after, .bytes = moved * sizeof(unsigned int)});
    }
    for (unsigned long long line = first; line < to; line += 1ULL)
    {
        lines->begun[line] = CLI_DOC_UNCOLORED;
        lines->ended[line] = CLI_DOC_UNCOLORED;
        lines->regions[line] = 0u;
        lines->runs[line] = CLI_DOC_NO_RUNS;
    }
    // the count is no more than the capacity, which an unsigned int counts
    lines->lines = (unsigned int)count;
    return 1;
}

int cli_document_paged_edit(const CliDocumentOutCfg *args)
{
    const CliEdit *const *changes = args->changes;
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    CliStoredText *const branch = ((one != NULL) && one->paged) ? one : NULL;
    // the states counted as far as the pages are before the edits, which are placed against those lines
    int carried = (branch == NULL) || EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = branch);
    const int written =
        EMBED_CALL(cli_pages_out_err_msg.edit, CliPagesOutErrMsgCfg, .key = args->key, .changes = changes,
                   .edits = args->edits, .count_u32 = args->count_u32, .out = args->out, .err_msg = args->err_msg);
    for (unsigned int change = 0u; written && carried && (branch != NULL) && (change < args->count_u32); change += 1u)
    {
        unsigned int at = args->edits[change];
        while (carried && (at > 0u))
        {
            carried = EMBED_CALL(stored_states_edit, DocumentStoreCtx, .lines = &branch->lines,
                                 .edit = &changes[change][at - 1u]);
            at -= 1u;
        }
    }
    if (written && (branch != NULL))
    {
        EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = branch);
        branch->changes += args->count_u32;
        branch->regions_moved = 1;
        branch->refused = branch->refused || !carried;
        EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = branch);
    }
    return written;
}

int cli_document_store(const CliDocumentTextCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_stored_init, DocumentStoreCtx, .key = args->key);
    const int stored_ok = (one != NULL) && EMBED_CALL(stored_lines_init, DocumentStoreCtx, .one = one,
                                                      .text = args->text, .size = args->size, .shape = args->shape);
    return stored_ok;
}

void cli_document_release(const CliDocumentCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one != NULL)
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
    }
}

int cli_document_crlf(const CliDocumentCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    return (one != NULL) && one->lines.crlf;
}

int cli_document_shape(const CliDocumentTextCfg *args)
{
    CliShape *shape = args->shape;
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one != NULL)
    {
        *shape = EMBED_CALL(cli_document_lines_out.shape_get, CliDocumentLinesOutCfg, .lines = &one->lines);
    }
    return one != NULL;
}

const unsigned short *cli_document_line_units(const CliDocumentLineUnitsCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if ((one == NULL) || (args->line >= one->lines.lines))
    {
        *args->count = 0ULL;
        return NULL;
    }
    *args->count = one->lines.length[args->line];
    return one->lines.unit + one->lines.start[args->line];
}

const unsigned short *cli_document_head_line_units(const CliDocumentLineUnitsCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if ((one == NULL) || !one->head_has || (args->line >= one->head.lines))
    {
        *args->count = 0ULL;
        return NULL;
    }
    *args->count = one->head.length[args->line];
    return one->head.unit + one->head.start[args->line];
}

// a line's units put after the spare lines, the text of the line being made running on from `open` where it is given
EMBED_INLINE void cli_units_append(const DocumentStoreCtx *args)
{
    if ((args->lines->used + args->count) > args->lines->units_max)
    {
        args->lines->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->lines->unit + args->lines->used,
                            .src = args->units,
                            .bytes = args->count * sizeof(unsigned short)});
    for (unsigned long long at = 0ULL; at < args->count; at += 1ULL)
    {
        EMBED_CALL(cli_lines_histogram_append, CliLinesHistogramAppendCfg, .lines = args->lines,
                   .unit = args->units[at]);
    }
    args->lines->used += args->count;
}

// a line not colored begun after the spare lines; the line begun last given the states the line `line` of `from` was
// colored from and ends in, an edit's line carried whole; and the line being made ended
EMBED_INLINE void cli_line_begin(const DocumentStoreCtx *args)
{
    CliLines *lines = args->lines;
    if (lines->lines >= lines->lines_max)
    {
        lines->full = 1;
        return;
    }
    lines->start[lines->lines] = lines->used;
    lines->length[lines->lines] = 0ULL;
    lines->begun[lines->lines] = CLI_DOC_UNCOLORED;
    lines->ended[lines->lines] = CLI_DOC_UNCOLORED;
    lines->regions[lines->lines] = 0u;
    lines->runs[lines->lines] = CLI_DOC_NO_RUNS;
    lines->lines += 1u;
}

EMBED_INLINE void cli_line_machine_transfer(const DocumentStoreCtx *args)
{
    if (args->lines->lines > 0u)
    {
        args->lines->begun[args->lines->lines - 1u] = args->from_lines_pointer->begun[args->line];
        args->lines->ended[args->lines->lines - 1u] = args->from_lines_pointer->ended[args->line];
        args->lines->regions[args->lines->lines - 1u] = args->from_lines_pointer->regions[args->line];
        args->lines->runs[args->lines->lines - 1u] = args->from_lines_pointer->runs[args->line];
    }
}

EMBED_INLINE void cli_line_end(const DocumentStoreCtx *args)
{
    if (args->lines->lines > 0u)
    {
        args->lines->length[args->lines->lines - 1u] = args->lines->used - args->lines->start[args->lines->lines - 1u];
    }
}

// `count` of a text's units moved from `from` to `to`, the way the move needs where the two overlap
EMBED_INLINE void cli_units_move(const DocumentStoreCtx *args)
{
    const MemoriaCfg moving = {
        .dst = args->unit + args->to, .src = args->unit + args->from, .bytes = args->count * sizeof(unsigned short)};
    if (args->to < args->from)
    {
        memor.move_down(&moving);
    }
    else if (args->to > args->from)
    {
        memor.move_up(&moving);
    }
}

// the starts or the lengths of `count` lines moved from the line `from` to the line `to`, the way the move needs where
// the two overlap; and so their states, their regions or the places of their runs
EMBED_INLINE void cli_line_positions_move(const DocumentStoreCtx *args)
{
    const MemoriaCfg moving = {.dst = args->place + args->to,
                               .src = args->place + args->from,
                               .bytes = args->count * sizeof(unsigned long long)};
    if (args->to < args->from)
    {
        memor.move_down(&moving);
    }
    else if (args->to > args->from)
    {
        memor.move_up(&moving);
    }
}

EMBED_INLINE void cli_line_states_move(const DocumentStoreCtx *args)
{
    const MemoriaCfg moving = {
        .dst = args->state + args->to, .src = args->state + args->from, .bytes = args->count * sizeof(unsigned int)};
    if (args->to < args->from)
    {
        memor.move_down(&moving);
    }
    else if (args->to > args->from)
    {
        memor.move_up(&moving);
    }
}

// one edit written over the text's lines in the capacity they stand in, where the result fits it: the units after the
// edit moved to where its text ends and its text put between, split at each "\n", each line it writes not colored and
// each line after it carried with its states, the census given the units the edit takes out and puts in: 1, or 0 with
// the text as it was where the result does not fit the capacity
EMBED_INLINE int cli_edit_position(const DocumentStoreCtx *args)
{
    // the units and the line breaks the edit's text puts in, counted on a first pass over it
    unsigned long long units = 0ULL;
    unsigned long long breaks = 0ULL;
    for (unsigned long long at = 0ULL; at < args->edit->length;)
    {
        unsigned int letter = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                         .text_bytes = (const unsigned char *)args->edit->text, .length = args->edit->length, .at = at,
                         .letter = &letter);
        breaks += (letter == '\n') ? 1ULL : 0ULL;
        if (letter == '\n')
        {
            units += 0ULL;
        }
        else if (letter > 0xffffu)
        {
            units += 2ULL;
        }
        else
        {
            units += 1ULL;
        }
    }
    const unsigned long long from = args->now->start[args->edit->from.line] + args->edit->from.col;
    const unsigned long long to = args->now->start[args->edit->to.line] + args->edit->to.col;
    const unsigned long long used = args->now->used - (to - from) + units;
    const unsigned long long allocated_lines = args->edit->to.line - args->edit->from.line;
    const unsigned long long lines = (unsigned long long)args->now->lines - allocated_lines + breaks;
    if (args->now->full || (used > args->now->units_max) || (lines > args->now->lines_max))
    {
        return 0;
    }
    for (unsigned long long at = from; at < to; at += 1ULL)
    {
        args->now->histogram.occurrences[args->now->unit[at] & 0xffu] -= 1ULL;
        args->now->histogram.occurrences[args->now->unit[at] >> 8u] -= 1ULL;
        args->now->histogram.total -= 2ULL;
    }
    const unsigned long long tail = args->now->length[args->edit->to.line] - args->edit->to.col;
    EMBED_CALL(cli_units_move, DocumentStoreCtx, .unit = args->now->unit, .from = to, .to = from + units,
               .count = args->now->used - to);
    // the lines after the edit moved to stand after the lines it writes, each starting as many units on
    const unsigned long long after = args->edit->to.line + 1ULL;
    const unsigned long long after_to = args->edit->from.line + breaks + 1ULL;
    const unsigned long long after_count = args->now->lines - after;
    EMBED_CALL(cli_line_positions_move, DocumentStoreCtx, .place = args->now->start, .from = after, .to = after_to,
               .count = after_count);
    EMBED_CALL(cli_line_positions_move, DocumentStoreCtx, .place = args->now->length, .from = after, .to = after_to,
               .count = after_count);
    EMBED_CALL(cli_line_states_move, DocumentStoreCtx, .state = args->now->begun, .from = after, .to = after_to,
               .count = after_count);
    EMBED_CALL(cli_line_states_move, DocumentStoreCtx, .state = args->now->ended, .from = after, .to = after_to,
               .count = after_count);
    EMBED_CALL(cli_line_states_move, DocumentStoreCtx, .state = args->now->regions, .from = after, .to = after_to,
               .count = after_count);
    EMBED_CALL(cli_line_states_move, DocumentStoreCtx, .state = args->now->runs, .from = after, .to = after_to,
               .count = after_count);
    for (unsigned long long line = after_to; line < lines; line += 1ULL)
    {
        args->now->start[line] = args->now->start[line] + (from + units) - to;
    }
    // the edit's text put in, each line it writes begun where the one before it ends and not colored
    unsigned long long line = args->edit->from.line;
    unsigned long long put = from;
    for (unsigned long long at = 0ULL; at <= args->edit->length;)
    {
        unsigned int letter = '\n';
        if (at < args->edit->length)
        {
            at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                             .text_bytes = (const unsigned char *)args->edit->text, .length = args->edit->length,
                             .at = at, .letter = &letter);
        }
        else
        {
            at += 1ULL;
        }
        if (letter == '\n')
        {
            const unsigned long long ends = (at > args->edit->length) ? (put + tail) : put;
            args->now->length[line] = ends - args->now->start[line];
            args->now->begun[line] = CLI_DOC_UNCOLORED;
            args->now->ended[line] = CLI_DOC_UNCOLORED;
            args->now->regions[line] = 0u;
            args->now->runs[line] = CLI_DOC_NO_RUNS;
            line += 1ULL;
            if (at <= args->edit->length)
            {
                args->now->start[line] = put;
            }
            continue;
        }
        // each unit of a letter is below 2^16
        const unsigned short first =
            (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        args->now->unit[put] = first;
        EMBED_CALL(cli_lines_histogram_append, CliLinesHistogramAppendCfg, .lines = args->now, .unit = first);
        put += 1ULL;
        if (letter > 0xffffu)
        {
            const unsigned short second = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
            args->now->unit[put] = second;
            EMBED_CALL(cli_lines_histogram_append, CliLinesHistogramAppendCfg, .lines = args->now, .unit = second);
            put += 1ULL;
        }
    }
    args->now->used = used;
    // the lines counted above to be no more than the capacity's, which an unsigned int counts
    args->now->lines = (unsigned int)lines;
    return 1;
}

// one edit written over the text's lines through `reserve`, a capacity that contains the result: the lines before it,
// its text split at each "\n" with the head of its first line before it and the tail of its last after it, and the
// lines after it, each line it leaves whole with its states and each it writes not colored. The result goes back into
// the text's capacity where it fits, else `reserve` becomes the text's capacity and the old one is given back
EMBED_INLINE void cli_edit_write(const DocumentStoreCtx *args)
{
    CliLines *reserve = args->reserve;
    CliLines *const now = &args->one->lines;
    for (unsigned long long line = 0ULL; line < args->edit->from.line; line += 1ULL)
    {
        EMBED_CALL(cli_line_begin, DocumentStoreCtx, .lines = reserve);
        EMBED_CALL(cli_line_machine_transfer, DocumentStoreCtx, .lines = reserve, .from_lines_pointer = now,
                   .line = line);
        EMBED_CALL(cli_units_append, DocumentStoreCtx, .lines = reserve, .units = now->unit + now->start[line],
                   .count = now->length[line]);
        EMBED_CALL(cli_line_end, DocumentStoreCtx, .lines = reserve);
    }
    EMBED_CALL(cli_line_begin, DocumentStoreCtx, .lines = reserve);
    EMBED_CALL(cli_units_append, DocumentStoreCtx, .lines = reserve,
               .units = now->unit + now->start[args->edit->from.line], .count = args->edit->from.col);
    for (unsigned long long at = 0ULL; at < args->edit->length;)
    {
        unsigned int letter = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                         .text_bytes = (const unsigned char *)args->edit->text, .length = args->edit->length, .at = at,
                         .letter = &letter);
        if (letter == '\n')
        {
            EMBED_CALL(cli_line_end, DocumentStoreCtx, .lines = reserve);
            EMBED_CALL(cli_line_begin, DocumentStoreCtx, .lines = reserve);
            continue;
        }
        unsigned short units[2];
        const unsigned long long count = (letter > 0xffffu) ? 2ULL : 1ULL;
        // each unit of a letter is below 2^16
        units[0] = (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        units[1] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
        EMBED_CALL(cli_units_append, DocumentStoreCtx, .lines = reserve, .units = units, .count = count);
    }
    const unsigned long long tail_line = args->edit->to.line;
    EMBED_CALL(cli_units_append, DocumentStoreCtx, .lines = reserve,
               .units = now->unit + now->start[tail_line] + args->edit->to.col,
               .count = now->length[tail_line] - args->edit->to.col);
    EMBED_CALL(cli_line_end, DocumentStoreCtx, .lines = reserve);
    for (unsigned long long line = tail_line + 1ULL; line < now->lines; line += 1ULL)
    {
        EMBED_CALL(cli_line_begin, DocumentStoreCtx, .lines = reserve);
        EMBED_CALL(cli_line_machine_transfer, DocumentStoreCtx, .lines = reserve, .from_lines_pointer = now,
                   .line = line);
        EMBED_CALL(cli_units_append, DocumentStoreCtx, .lines = reserve, .units = now->unit + now->start[line],
                   .count = now->length[line]);
        EMBED_CALL(cli_line_end, DocumentStoreCtx, .lines = reserve);
    }
    const int full = now->full || reserve->full;
    if ((reserve->used <= now->units_max) && (reserve->lines <= now->lines_max))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = now->unit, .src = reserve->unit, .bytes = reserve->used * sizeof(unsigned short)});
        memor.cpy(&(MemoriaCfg){
            .dst = now->start, .src = reserve->start, .bytes = reserve->lines * sizeof(unsigned long long)});
        memor.cpy(&(MemoriaCfg){
            .dst = now->length, .src = reserve->length, .bytes = reserve->lines * sizeof(unsigned long long)});
        memor.cpy(
            &(MemoriaCfg){.dst = now->begun, .src = reserve->begun, .bytes = reserve->lines * sizeof(unsigned int)});
        memor.cpy(
            &(MemoriaCfg){.dst = now->ended, .src = reserve->ended, .bytes = reserve->lines * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){
            .dst = now->regions, .src = reserve->regions, .bytes = reserve->lines * sizeof(unsigned int)});
        memor.cpy(
            &(MemoriaCfg){.dst = now->runs, .src = reserve->runs, .bytes = reserve->lines * sizeof(unsigned int)});
        now->used = reserve->used;
        now->lines = reserve->lines;
        now->histogram = reserve->histogram;
        EMBED_CALL(stored_capacity_exit, DocumentStoreCtx, .lines = reserve);
    }
    else
    {
        EMBED_CALL(stored_capacity_exit, DocumentStoreCtx, .lines = now);
        *now = *reserve;
        *reserve = (CliLines){NULL, 0ULL, 0ULL, NULL, NULL, 0u, 0u, 0};
    }
    now->full = full;
}

// whether a place stands in the text: on a line it contains, at or before the line's end
EMBED_INLINE int cli_fits(const DocumentStoreCtx *args)
{
    return (args->position_position_pointer->line < args->lines_read->lines) &&
           (args->position_position_pointer->col <= args->lines_read->length[args->position_position_pointer->line]);
}

// a capacity that contains the text as `edit` leaves it, with capacity to grow, taken for `reserve`: 1, or 0 where the
// cells cannot meet it
EMBED_INLINE int stored_reserve_alloc(const DocumentStoreCtx *args)
{
    // a unit of the edit's text takes a byte at the least: its bytes bound the units it puts in
    const unsigned long long units = args->now_read->used + args->edit->length;
    const unsigned long long lines =
        (unsigned long long)args->now_read->lines +
        EMBED_CALL(stored_lines_in, DocumentStoreCtx, .text = args->edit->text, .size = args->edit->length);
    const unsigned long long most = (lines < CLI_DOC_LINES) ? lines : CLI_DOC_LINES;
    return EMBED_CALL(stored_capacity_enter, DocumentStoreCtx, .lines = args->reserve,
                      .units_max = units + (units / CLI_DOC_RESERVE_SHARE) + CLI_DOC_RESERVE_UNITS,
                      .lines_max = (unsigned int)(most + (most / CLI_DOC_RESERVE_SHARE) + CLI_DOC_RESERVE_LINES),
                      .what = "the lines an edit writes");
}

int cli_document_edit(const CliDocumentErrMsgCfg *args)
{
    const CliEdit *const *changes = args->changes;
    CliShape *shape = args->shape;
    CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one == NULL)
    {
        mmgr_line_put(args->err_msg, "no text is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    int fits = 1;
    int capacity = 1;
    for (unsigned int change = 0u; fits && capacity && (change < args->count_u32); change += 1u)
    {
        unsigned int at = args->edits[change];
        while (fits && capacity && (at > 0u))
        {
            const CliEdit *const edit = &changes[change][at - 1u];
            const int before = (edit->to.line < edit->from.line) ||
                               ((edit->to.line == edit->from.line) && (edit->to.col < edit->from.col));
            fits = EMBED_CALL(cli_fits, DocumentStoreCtx, .lines_read = &one->lines,
                              .position_position_pointer = &edit->from) &&
                   EMBED_CALL(cli_fits, DocumentStoreCtx, .lines_read = &one->lines,
                              .position_position_pointer = &edit->to) &&
                   !before;
            // an edit is written in the text's own capacity, and a capacity of its own is taken only where the result
            // outgrows it
            const int outgrown =
                fits && !EMBED_CALL(cli_edit_position, DocumentStoreCtx, .now = &one->lines, .edit = edit);
            CliLines reserve;
            capacity = outgrown ? EMBED_CALL(stored_reserve_alloc, DocumentStoreCtx, .now_read = &one->lines,
                                             .edit = edit, .reserve = &reserve)
                                : capacity;
            if (outgrown && capacity)
            {
                EMBED_CALL(cli_edit_write, DocumentStoreCtx, .one = one, .edit = edit, .reserve = &reserve);
            }
            at -= 1u;
        }
    }
    if (!fits || !capacity)
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
        mmgr_line_put(args->err_msg, fits ? "the lines an edit writes could not be taken for "
                                          : "an edit does not fit the text stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    *shape = EMBED_CALL(cli_document_lines_out.shape_get, CliDocumentLinesOutCfg, .lines = &one->lines);
    EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = one);
    one->changes += args->count_u32;
    one->regions_moved = 1;
    EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
    return 1;
}

int cli_document_histogram(const CliDocumentCfg *args)
{
    CliHistogram *histogram = args->histogram;
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one != NULL)
    {
        *histogram = one->lines.histogram;
    }
    return one != NULL;
}

int cli_document_has_head(const CliDocumentCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    const int has = (one != NULL) && one->head_known;
    return has;
}

int cli_document_changes(const CliDocumentOutKeyHeadInputCfg *args)
{
    CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one == NULL)
    {
        return 0;
    }
    if (args->head_given)
    {
        // the last commit's file laid into lines beside the text, in a capacity sized to it
        one->head_known = 1;
        one->head_has = args->head != NULL;
        EMBED_CALL(stored_capacity_exit, DocumentStoreCtx, .lines = &one->head);
        if ((args->head != NULL) &&
            EMBED_CALL(
                stored_capacity_enter, DocumentStoreCtx, .lines = &one->head, .units_max = args->head_size + 1ULL,
                .lines_max =
                    EMBED_CALL(stored_lines_in, DocumentStoreCtx, .text = args->head, .size = args->head_size) + 1u))
        {
            EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = args->head,
                       .size = args->head_size, .lines_out = &one->head);
        }
        else
        {
            one->head_known = args->head == NULL;
        }
    }
    if (!one->head_known || !one->head_has)
    {
        return 0;
    }
    return EMBED_CALL(cli_document_out_then.line_changes, CliDocumentOutThenCfg, .then = &one->head, .now = &one->lines,
                      .each = args->each, .context = args->context);
}

int cli_document_find(const CliDocumentOutNeedleCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one == NULL)
    {
        mmgr_line_put(args->err_msg, "no text is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    const int found =
        EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = &one->lines, .needle = args->needle,
                   .most = args->most, .out = args->out, .err_msg = args->err_msg);
    return found;
}
