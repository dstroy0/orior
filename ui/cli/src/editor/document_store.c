// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_store.c: the branches here, by key: stored whole as a branch is read, written by the page's edits, marked
// against the last commit's file, and searched. Each text's lines live in MMgr's movable cells sized to the text: the
// units, each line's start and length, and each line's four words. An edit reads the text into a work cell of the
// temporary tier sized to the result, writes the result there and takes new cells sized to it, the old given back; a
// read copies the span it works on out through the entry. The last commit's file is stored the same way. A paged text
// is a branch too, its lines in the pages and only their words here, carried through each edit the pages write
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "document_internal.h"
#include "pages.h"

// the max units and lines of a text stored
#define CLI_DOC_UNITS (1u << 22u)
#define CLI_DOC_LINES (1u << 18u)

// the gate the texts are read and written under; the first of the texts stored, and the last serial a text took, both
// under the gate
static CliGate s_texts_gate = CLI_GATE_INIT;
static CliStoredText *s_stored;
static unsigned long long s_serial;

void cli_document_lock_take(void)
{
    cli_gate_shut(&s_texts_gate);
}

void cli_document_lock_give(void)
{
    cli_gate_opened(&s_texts_gate);
}

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
    unsigned long long lines_max;
    unsigned long long count;
    const char *key;
    CliStoredText *one;
    CliShape *shape;
    const CliEdit *edit;
    CliStoredLines *stored;
    const CliStoredLines *stored_read;
    const CliLines *lines_read;
    const CliPosition *position_position_pointer;
    CliLines *now;
    unsigned long long more_units;
    unsigned long long more_lines;
    unsigned short *unit;
    unsigned long long from;
    unsigned long long to;
    unsigned long long *place;
    unsigned int *state;
    void (*each)(const CliEachHunkCfg *args);
    void *context;
} DocumentStoreCtx;

// how many lines `size` bytes of `text` make, one more than its newlines
EMBED_INLINE unsigned long long stored_lines_in(const DocumentStoreCtx *args)
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
    return count;
}

// A work cell of the temporary tier arranged as `lines`: `units_max` units, then each line's start and length, then
// each line's four words, `lines_max` lines of each; `lines` empty where the cells cannot meet it: 1, or 0
EMBED_INLINE int stored_work_enter(const DocumentStoreCtx *args)
{
    CliLines *lines = args->lines;
    const unsigned long long units = (args->units_max < CLI_DOC_UNITS) ? args->units_max : CLI_DOC_UNITS;
    const unsigned long long count = (args->lines_max < CLI_DOC_LINES) ? args->lines_max : CLI_DOC_LINES;
    // the units' bytes rounded up to a boundary of the starts after them
    const unsigned long long unit_bytes = ((units * sizeof(unsigned short)) + 7ULL) & ~7ULL;
    const unsigned long long bytes =
        unit_bytes + (2ULL * count * sizeof(unsigned long long)) + (CLI_DOC_WORDS * count * sizeof(unsigned int));
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_editor_owner);
    *lines = (CliLines){NULL, 0ULL, 0ULL, NULL, NULL, 0u, 0u, 0};
    if (cell == NULL)
    {
        return 0;
    }
    unsigned long long *const start = (unsigned long long *)(void *)(cell + unit_bytes);
    unsigned int *const begun = (unsigned int *)(void *)(start + (2ULL * count));
    // the lines a work cell holds are bounded by CLI_DOC_LINES, which an unsigned int counts
    *lines = (CliLines){(unsigned short *)(void *)cell, units, 0ULL, start, start + count, (unsigned int)count, 0u, 0};
    lines->begun = begun;
    lines->ended = begun + count;
    lines->regions = begun + (2ULL * count);
    lines->runs = begun + (3ULL * count);
    return 1;
}

// the work cell given back, `lines` left empty
EMBED_INLINE void stored_work_exit(const DocumentStoreCtx *args)
{
    g_cli_guard->persistent_buf_release(args->lines->unit, g_cli_editor_owner);
    *args->lines = (CliLines){NULL, 0ULL, 0ULL, NULL, NULL, 0u, 0u, 0};
}

// the movable cells of `stored` given back, `stored` left empty
EMBED_INLINE void stored_cells_release(const DocumentStoreCtx *args)
{
    CliStoredLines *stored = args->stored;
    (void)g_cli_guard->movable_buf_release(stored->units, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(stored->starts, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(stored->lengths, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(stored->words, g_cli_editor_owner);
    memor.set(&(MemoriaCfg){.dst = stored, .val = 0, .bytes = sizeof(*stored)});
}

// `lines` stored in movable cells sized to them, in place of what `stored` held, its units left out where the work
// holds none: 1, or 0 with `stored` empty where the cells cannot meet them
EMBED_INLINE int stored_cells_take(const DocumentStoreCtx *args)
{
    CliStoredLines *stored = args->stored;
    const CliLines *lines = args->lines_read;
    stored_cells_release(args);
    const unsigned long long count = lines->lines;
    const unsigned long long unit_bytes = lines->used * sizeof(unsigned short);
    const unsigned long long place_bytes = count * sizeof(unsigned long long);
    const unsigned long long word_bytes = CLI_DOC_WORDS * count * sizeof(unsigned int);
    stored->units = ((lines->unit != NULL) && (unit_bytes != 0ULL))
                        ? g_cli_guard->movable_buf_alloc(unit_bytes, g_cli_editor_owner)
                        : NULL;
    stored->starts = (count != 0ULL) ? g_cli_guard->movable_buf_alloc(place_bytes, g_cli_editor_owner) : NULL;
    stored->lengths = (count != 0ULL) ? g_cli_guard->movable_buf_alloc(place_bytes, g_cli_editor_owner) : NULL;
    stored->words = (count != 0ULL) ? g_cli_guard->movable_buf_alloc(word_bytes, g_cli_editor_owner) : NULL;
    const int units_met = (stored->units != NULL) || (lines->unit == NULL) || (unit_bytes == 0ULL);
    const int lines_met =
        (count == 0ULL) || ((stored->starts != NULL) && (stored->lengths != NULL) && (stored->words != NULL));
    if (!units_met || !lines_met)
    {
        stored_cells_release(args);
        return 0;
    }
    if (stored->units != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored->units, .src = lines->unit, .bytes = unit_bytes});
    }
    if (count != 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored->starts, .src = lines->start, .bytes = place_bytes});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored->lengths, .src = lines->length, .bytes = place_bytes});
        const unsigned long long array = count * sizeof(unsigned int);
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored->words, .offset = 0ULL, .src = lines->begun, .bytes = array});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored->words, .offset = array, .src = lines->ended, .bytes = array});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored->words, .offset = 2ULL * array, .src = lines->regions, .bytes = array});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored->words, .offset = 3ULL * array, .src = lines->runs, .bytes = array});
    }
    stored->used = (lines->unit != NULL) ? lines->used : 0ULL;
    stored->lines = lines->lines;
    stored->crlf = lines->crlf;
    return 1;
}

// `stored` read whole into a work cell as `lines`, with capacity for `more_units` units and `more_lines` lines past
// what it holds: 1, or 0 where the cells cannot meet it
EMBED_INLINE int stored_copy_enter(const DocumentStoreCtx *args)
{
    const CliStoredLines *stored = args->stored_read;
    CliLines *lines = args->lines;
    if (!EMBED_CALL(stored_work_enter, DocumentStoreCtx, .lines = lines, .units_max = stored->used + args->more_units,
                    .lines_max = (unsigned long long)stored->lines + args->more_lines))
    {
        return 0;
    }
    if (stored->units != NULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = stored->units, .dst = lines->unit, .bytes = stored->used * sizeof(unsigned short)});
    }
    if (stored->lines != 0u)
    {
        const unsigned long long place_bytes = (unsigned long long)stored->lines * sizeof(unsigned long long);
        const unsigned long long array = (unsigned long long)stored->lines * sizeof(unsigned int);
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = stored->starts, .dst = lines->start, .bytes = place_bytes});
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = stored->lengths, .dst = lines->length, .bytes = place_bytes});
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = stored->words, .offset = 0ULL, .dst = lines->begun, .bytes = array});
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = stored->words, .offset = array, .dst = lines->ended, .bytes = array});
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = stored->words, .offset = 2ULL * array, .dst = lines->regions, .bytes = array});
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = stored->words, .offset = 3ULL * array, .dst = lines->runs, .bytes = array});
    }
    lines->used = stored->used;
    lines->lines = stored->lines;
    lines->crlf = stored->crlf;
    return 1;
}

// a paged text's words taken for `count` lines, those it holds carried over and each line past them not colored: 1, or
// 0 where the cells cannot meet it, the words as they were
static int stored_states_grow(const DocumentStoreCtx *args)
{
    CliStoredLines *stored = args->stored;
    if (args->count > CLI_DOC_LINES)
    {
        return 0;
    }
    if (args->count <= stored->lines)
    {
        return 1;
    }
    CliLines work;
    if (!EMBED_CALL(stored_copy_enter, DocumentStoreCtx, .stored_read = stored, .lines = &work,
                    .more_lines = args->count - stored->lines))
    {
        return 0;
    }
    for (unsigned long long line = work.lines; line < args->count; line += 1ULL)
    {
        work.begun[line] = CLI_DOC_UNCOLORED;
        work.ended[line] = CLI_DOC_UNCOLORED;
        work.regions[line] = 0u;
        work.runs[line] = CLI_DOC_NO_RUNS;
    }
    // the count is bounded by CLI_DOC_LINES, which an unsigned int counts
    work.lines = (unsigned int)args->count;
    // a paged text's words alone are stored: the work cell starts at its units, none of which are taken
    unsigned short *const cell = work.unit;
    work.unit = NULL;
    const int taken = EMBED_CALL(stored_cells_take, DocumentStoreCtx, .stored = stored, .lines_read = &work);
    g_cli_guard->persistent_buf_release(cell, g_cli_editor_owner);
    return taken;
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
           EMBED_CALL(stored_states_grow, DocumentStoreCtx, .stored = &args->one->lines, .count = count);
}

CliStoredText *cli_document_on_store(const CliDocumentKeyCtx *args)
{
    for (CliStoredText *one = s_stored; one != NULL; one = one->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = args->key, .cap = CLI_POOL_BYTES}))
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
    // a stored text's record carries the claims and the readers' count its threads pin it by, so the records are
    // fixed outside the pool, one place for each text stored at once, where they punch no hole in the persistent tier
    static CliStoredText s_places[CLI_STORED_MAX];
    CliStoredText *one = NULL;
    for (unsigned int at = 0u; (one == NULL) && (at < CLI_STORED_MAX); at += 1u)
    {
        one = (s_places[at].reserved == 0) ? &s_places[at] : NULL;
    }
    if (one == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    one->reserved = 1;
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1u});
    one->next = s_stored;
    s_stored = one;
    return one;
}

// the changes saved off the diff let go, the next read finding them anew
EMBED_INLINE void hunks_forgotten(const DocumentStoreCtx *args)
{
    if (args->one->hunks_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(args->one->hunks_cell, g_cli_editor_owner);
    }
    args->one->hunks_cell = NULL;
    args->one->hunks = 0u;
    args->one->hunks_known = 0;
}

// a text's cells given back, the text's own last
EMBED_INLINE void stored_cells_free(const DocumentStoreCtx *args)
{
    EMBED_CALL(stored_cells_release, DocumentStoreCtx, .stored = &args->one->lines);
    EMBED_CALL(stored_cells_release, DocumentStoreCtx, .stored = &args->one->head);
    EMBED_CALL(hunks_forgotten, DocumentStoreCtx, .one = args->one);
    (void)g_cli_guard->movable_buf_release(args->one->state, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(args->one->state_index, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(args->one->roles, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(args->one->run, g_cli_editor_owner);
    args->one->reserved = 0;
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

// `size` bytes of `text` positioned as lines in a work cell sized to them and stored in `stored`, in place of what it held:
// 1, or 0 where the cells cannot meet them
EMBED_INLINE int stored_text_take(const DocumentStoreCtx *args)
{
    CliLines work;
    // a unit of the text takes a byte at the least, and every line is counted
    if (!EMBED_CALL(stored_work_enter, DocumentStoreCtx, .lines = &work, .units_max = args->size,
                    .lines_max = EMBED_CALL(stored_lines_in, DocumentStoreCtx, .text = args->text, .size = args->size)))
    {
        return 0;
    }
    EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = args->text, .size = args->size,
               .lines_out = &work);
    const int taken =
        !work.full && EMBED_CALL(stored_cells_take, DocumentStoreCtx, .stored = args->stored, .lines_read = &work);
    EMBED_CALL(stored_work_exit, DocumentStoreCtx, .lines = &work);
    return taken;
}

// `size` bytes of `text` made the lines of the text stored: 1, or 0 where the cells cannot meet it, the text let go
EMBED_INLINE int stored_lines_init(const DocumentStoreCtx *args)
{
    CliStoredText *one = args->one;
    if (!EMBED_CALL(stored_text_take, DocumentStoreCtx, .stored = &one->lines, .text = args->text, .size = args->size))
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
        return 0;
    }
    EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = one);
    one->head_known = 0;
    one->changes = 0ULL;
    one->paged = 0;
    one->whole = 1;
    one->regions_moved = 1;
    one->cached_from = 0u;
    one->cached_to = CLI_DOC_CACHED_ALL;
    *args->shape = (CliShape){one->lines.lines, one->lines.used};
    return 1;
}

// the text stored under `key` where it is read whole, NULL where none is or it is a paged text's branch
EMBED_INLINE CliStoredText *stored_full_text_get(const DocumentStoreCtx *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    return ((one != NULL) && !one->paged) ? one : NULL;
}

static int page_locked(const CliDocumentKeyFromCfg *args)
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
        EMBED_CALL(stored_cells_release, DocumentStoreCtx, .stored = &one->lines);
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
        (one != NULL) && EMBED_CALL(stored_states_grow, DocumentStoreCtx, .stored = &one->lines, .count = count);
    if ((one != NULL) && !grown)
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
    }
    return grown;
}

int cli_document_page(const CliDocumentKeyFromCfg *args)
{
    cli_document_lock_take();
    const int answer = page_locked(args);
    cli_document_lock_give();
    return answer;
}

// the words of a paged text's lines carried through one edit the pages wrote, in the work `lines`: the lines from its
// first to its last taken out and those its text writes put in their place, not colored, and the lines after moved
// with them. The work holds the lines the edit adds: 1, or 0 where the edit lies past the lines
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
    if (count > lines->lines_max)
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
    // the count is no more than the work's capacity, which an unsigned int counts
    lines->lines = (unsigned int)count;
    return 1;
}

// the lines the changes `changes` add at max, `count` of them with `edits[at]` edits each
EMBED_INLINE unsigned long long stored_edits_lines(const CliEdit *const *changes, const unsigned int *edits,
                                                   unsigned int count)
{
    unsigned long long more = 0ULL;
    for (unsigned int change = 0u; change < count; change += 1u)
    {
        for (unsigned int at = 0u; at < edits[change]; at += 1u)
        {
            more += EMBED_CALL(stored_lines_in, DocumentStoreCtx, .text = changes[change][at].text,
                               .size = changes[change][at].length);
        }
    }
    return more;
}

static int paged_edit_locked(const CliDocumentOutCfg *args)
{
    const CliEdit *const *changes = args->changes;
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    CliStoredText *const branch = ((one != NULL) && one->paged) ? one : NULL;
    // the words counted as far as the pages are before the edits, which are placed against those lines
    int carried = (branch == NULL) || EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = branch);
    const int written =
        EMBED_CALL(cli_pages_out_err_msg.edit, CliPagesOutErrMsgCfg, .key = args->key, .changes = changes,
                   .edits = args->edits, .count_u32 = args->count_u32, .out = args->out, .err_msg = args->err_msg);
    if (written && carried && (branch != NULL))
    {
        // the words carried in a work copy sized for the lines the edits add, then stored again sized to them
        CliLines work;
        carried = EMBED_CALL(stored_copy_enter, DocumentStoreCtx, .stored_read = &branch->lines, .lines = &work,
                             .more_lines = stored_edits_lines(changes, args->edits, args->count_u32));
        for (unsigned int change = 0u; carried && (change < args->count_u32); change += 1u)
        {
            unsigned int at = args->edits[change];
            while (carried && (at > 0u))
            {
                carried =
                    EMBED_CALL(stored_states_edit, DocumentStoreCtx, .lines = &work, .edit = &changes[change][at - 1u]);
                at -= 1u;
            }
        }
        if (work.unit != NULL)
        {
            // a paged text's words alone are stored: the work cell starts at its units, none of which are taken
            unsigned short *const cell = work.unit;
            work.unit = NULL;
            carried = carried &&
                      EMBED_CALL(stored_cells_take, DocumentStoreCtx, .stored = &branch->lines, .lines_read = &work);
            g_cli_guard->persistent_buf_release(cell, g_cli_editor_owner);
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

int cli_document_paged_edit(const CliDocumentOutCfg *args)
{
    cli_document_lock_take();
    const int answer = paged_edit_locked(args);
    cli_document_lock_give();
    return answer;
}

static int store_locked(const CliDocumentTextCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_stored_init, DocumentStoreCtx, .key = args->key);
    const int stored_ok = (one != NULL) && EMBED_CALL(stored_lines_init, DocumentStoreCtx, .one = one,
                                                      .text = args->text, .size = args->size, .shape = args->shape);
    return stored_ok;
}

int cli_document_store(const CliDocumentTextCfg *args)
{
    cli_document_lock_take();
    const int answer = store_locked(args);
    cli_document_lock_give();
    return answer;
}

static void release_locked(const CliDocumentCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one != NULL)
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
    }
}

void cli_document_release(const CliDocumentCfg *args)
{
    cli_document_lock_take();
    release_locked(args);
    cli_document_lock_give();
}

static int crlf_locked(const CliDocumentCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    return (one != NULL) && one->lines.crlf;
}

int cli_document_crlf(const CliDocumentCfg *args)
{
    cli_document_lock_take();
    const int answer = crlf_locked(args);
    cli_document_lock_give();
    return answer;
}

static int shape_locked(const CliDocumentTextCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one != NULL)
    {
        *args->shape = (CliShape){one->lines.lines, one->lines.used};
    }
    return one != NULL;
}

int cli_document_shape(const CliDocumentTextCfg *args)
{
    cli_document_lock_take();
    const int answer = shape_locked(args);
    cli_document_lock_give();
    return answer;
}

// the units of the line `line` of `stored` copied into `dst`, `capacity` of them at max: the line's units, 0 where
// the text holds no such line
EMBED_INLINE unsigned long long stored_line_read(const CliStoredLines *stored, unsigned long long line,
                                                 unsigned short *dst, unsigned long long capacity)
{
    if ((stored->units == NULL) || (line >= stored->lines))
    {
        return 0ULL;
    }
    unsigned long long length = 0ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = stored->lengths, .offset = line * sizeof(length), .dst = &length, .bytes = sizeof(length)});
    const unsigned long long copied = (length < capacity) ? length : capacity;
    if (copied != 0ULL)
    {
        {
            unsigned long long line_start = 0ULL;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = stored->starts,
                                                                    .offset = line * sizeof(line_start),
                                                                    .dst = &line_start,
                                                                    .bytes = sizeof(line_start)});
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = stored->units,
                                                                    .offset = line_start * sizeof(unsigned short),
                                                                    .dst = dst,
                                                                    .bytes = copied * sizeof(unsigned short)});
        }
    }
    return length;
}

static unsigned long long line_units_locked(const CliDocumentLineUnitsCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    const unsigned long long count =
        (one != NULL) ? stored_line_read(&one->lines, args->line, args->dst, args->capacity) : 0ULL;
    *args->count = count;
    return ((one != NULL) && (args->line < one->lines.lines)) ? 1ULL : 0ULL;
}

unsigned long long cli_document_line_units(const CliDocumentLineUnitsCfg *args)
{
    cli_document_lock_take();
    const unsigned long long answer = line_units_locked(args);
    cli_document_lock_give();
    return answer;
}

static unsigned long long head_line_units_locked(const CliDocumentLineUnitsCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    const int has = (one != NULL) && one->head_has;
    const unsigned long long count = has ? stored_line_read(&one->head, args->line, args->dst, args->capacity) : 0ULL;
    *args->count = count;
    return (has && (args->line < one->head.lines)) ? 1ULL : 0ULL;
}

unsigned long long cli_document_head_line_units(const CliDocumentLineUnitsCfg *args)
{
    cli_document_lock_take();
    const unsigned long long answer = head_line_units_locked(args);
    cli_document_lock_give();
    return answer;
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

// one edit written over the work's lines: the units after the edit moved to where its text ends and its text put
// between, split at each "\n", each line it writes not colored and each line after it carried with its states: 1, or 0
// with the work as it was where the result does not fit its capacity
EMBED_INLINE int cli_edit_position(const DocumentStoreCtx *args)
{
    // the units and the line breaks the edit's text puts in, counted on a first pass over it
    unsigned long long units = 0ULL;
    unsigned long long breaks = 0ULL;
    for (unsigned long long at = 0ULL; at < args->edit->length;)
    {
        unsigned int character = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg,
                         .text_bytes = (const unsigned char *)args->edit->text, .length = args->edit->length, .at = at,
                         .character = &character);
        breaks += (character == '\n') ? 1ULL : 0ULL;
        if (character == '\n')
        {
            units += 0ULL;
        }
        else if (character > 0xffffu)
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
    const unsigned long long tail = args->now->length[args->edit->to.line] - args->edit->to.col;
    EMBED_CALL(cli_units_move, DocumentStoreCtx, .unit = args->now->unit, .from = to, .to = from + units,
               .count = args->now->used - to);
    // the lines after the edit moved to be after the lines it writes, each starting as many units on
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
        unsigned int character = '\n';
        if (at < args->edit->length)
        {
            at += EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg,
                             .text_bytes = (const unsigned char *)args->edit->text, .length = args->edit->length,
                             .at = at, .character = &character);
        }
        else
        {
            at += 1ULL;
        }
        if (character == '\n')
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
        // each unit of a character is below 2^16
        const unsigned short first =
            (unsigned short)((character > 0xffffu) ? (0xd800u + ((character - 0x10000u) >> 10u)) : character);
        args->now->unit[put] = first;
        put += 1ULL;
        if (character > 0xffffu)
        {
            args->now->unit[put] = (unsigned short)(0xdc00u + ((character - 0x10000u) & 0x3ffu));
            put += 1ULL;
        }
    }
    args->now->used = used;
    // the lines counted above to be no more than the capacity's, which an unsigned int counts
    args->now->lines = (unsigned int)lines;
    return 1;
}

// whether a place is in the text: on a line it contains, at or before the line's end
EMBED_INLINE int cli_fits(const DocumentStoreCtx *args)
{
    return (args->position_position_pointer->line < args->lines_read->lines) &&
           (args->position_position_pointer->col <= args->lines_read->length[args->position_position_pointer->line]);
}

static int edit_locked(const CliDocumentErrMsgCfg *args)
{
    const CliEdit *const *changes = args->changes;
    CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one == NULL)
    {
        mmgr_line_put(args->err_msg, "no text is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    // the text in a work copy with capacity for every edit's units and lines; a unit of an edit's text takes a byte of
    // it at the least
    unsigned long long more_units = 0ULL;
    for (unsigned int change = 0u; change < args->count_u32; change += 1u)
    {
        for (unsigned int at = 0u; at < args->edits[change]; at += 1u)
        {
            more_units += changes[change][at].length;
        }
    }
    CliLines work;
    int capacity =
        EMBED_CALL(stored_copy_enter, DocumentStoreCtx, .stored_read = &one->lines, .lines = &work,
                   .more_units = more_units, .more_lines = stored_edits_lines(changes, args->edits, args->count_u32));
    int fits = capacity;
    for (unsigned int change = 0u; fits && capacity && (change < args->count_u32); change += 1u)
    {
        unsigned int at = args->edits[change];
        while (fits && capacity && (at > 0u))
        {
            const CliEdit *const edit = &changes[change][at - 1u];
            const int before = (edit->to.line < edit->from.line) ||
                               ((edit->to.line == edit->from.line) && (edit->to.col < edit->from.col));
            fits =
                EMBED_CALL(cli_fits, DocumentStoreCtx, .lines_read = &work, .position_position_pointer = &edit->from) &&
                EMBED_CALL(cli_fits, DocumentStoreCtx, .lines_read = &work, .position_position_pointer = &edit->to) &&
                !before;
            capacity = !fits || EMBED_CALL(cli_edit_position, DocumentStoreCtx, .now = &work, .edit = edit);
            at -= 1u;
        }
    }
    const int taken =
        fits && capacity && EMBED_CALL(stored_cells_take, DocumentStoreCtx, .stored = &one->lines, .lines_read = &work);
    if (work.unit != NULL)
    {
        EMBED_CALL(stored_work_exit, DocumentStoreCtx, .lines = &work);
    }
    if (!taken)
    {
        EMBED_CALL(cli_stored_let_go, DocumentStoreCtx, .one = one);
        mmgr_line_put(args->err_msg, fits ? "the lines an edit writes could not be taken for "
                                          : "an edit does not fit the text stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    *args->shape = (CliShape){one->lines.lines, one->lines.used};
    EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = one);
    one->changes += args->count_u32;
    one->regions_moved = 1;
    EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
    return 1;
}

int cli_document_edit(const CliDocumentErrMsgCfg *args)
{
    cli_document_lock_take();
    const int answer = edit_locked(args);
    cli_document_lock_give();
    return answer;
}

static int has_head_locked(const CliDocumentCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    return (one != NULL) && one->head_known;
}

int cli_document_has_head(const CliDocumentCfg *args)
{
    cli_document_lock_take();
    const int answer = has_head_locked(args);
    cli_document_lock_give();
    return answer;
}

// The changes the diff passes on as they are positioned for the save: the reader they are passed on to with its context, the
// words of this call they are positioned in, how many changes, and how many the words hold
typedef struct
{
    void (*each)(const CliEachHunkCfg *args);
    void *context;
    unsigned int *word;
    unsigned int count;
    unsigned int max;
} HunksLaying;

// a change the diff found passed on to the reader and positioned among the words where they have room
static void hunk_positioned(const CliEachHunkCfg *args)
{
    HunksLaying *const positioning = args->context;
    positioning->each(&(CliEachHunkCfg){positioning->context, args->now_from, args->now_to, args->then_from, args->then_to});
    if (positioning->count < positioning->max)
    {
        unsigned int *const word = positioning->word + ((unsigned long long)positioning->count * CLI_DOC_HUNK_WORDS);
        word[0] = args->now_from;
        word[1] = args->now_to;
        word[2] = args->then_from;
        word[3] = args->then_to;
    }
    positioning->count += 1u;
}

// the changes saved off the diff read out of their cell into a cell of this call and passed to `each` in order
EMBED_INLINE void hunks_replayed(const DocumentStoreCtx *args)
{
    const CliStoredText *const one = args->one;
    if (one->hunks == 0u)
    {
        return;
    }
    const unsigned long long bytes = (unsigned long long)one->hunks * CLI_DOC_HUNK_WORDS * sizeof(unsigned int);
    unsigned int *const word = g_cli_guard->temporary_buf_alloc(bytes, g_cli_editor_owner);
    if (word == NULL)
    {
        return;
    }
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = one->hunks_cell, .offset = 0u, .dst = word, .bytes = bytes});
    for (unsigned int at = 0u; at < one->hunks; at += 1u)
    {
        const unsigned int *const change = word + ((unsigned long long)at * CLI_DOC_HUNK_WORDS);
        args->each(&(CliEachHunkCfg){args->context, change[0], change[1], change[2], change[3]});
    }
    g_cli_guard->persistent_buf_release(word, g_cli_editor_owner);
}

// the diff of the head against the lines run, each change passed to `each` as it is found, and the changes saved in a
// movable cell sized to them at the text's serial where they all fit the words of this call: 1 where the diff ran
EMBED_INLINE int hunks_found(const DocumentStoreCtx *args)
{
    CliStoredText *const one = args->one;
    EMBED_CALL(hunks_forgotten, DocumentStoreCtx, .one = one);
    // the two texts copied out for the diff, which reads them whole; a change takes a line of one text at the least,
    // so the words hold as many changes as the two have lines
    CliLines then;
    CliLines now;
    const int copied = EMBED_CALL(stored_copy_enter, DocumentStoreCtx, .stored_read = &one->head, .lines = &then) &&
                       EMBED_CALL(stored_copy_enter, DocumentStoreCtx, .stored_read = &one->lines, .lines = &now);
    // the lines of both texts are far below 2^32
    const unsigned int max = copied ? (unsigned int)(then.lines + now.lines) : 0u;
    unsigned int *const word =
        (max != 0u) ? g_cli_guard->temporary_buf_alloc(
                           (unsigned long long)max * CLI_DOC_HUNK_WORDS * sizeof(unsigned int), g_cli_editor_owner)
                     : NULL;
    HunksLaying positioning = {args->each, args->context, word, 0u, (word != NULL) ? max : 0u};
    const int changed = copied && EMBED_CALL(cli_document_out_then.line_changes, CliDocumentOutThenCfg, .then = &then,
                                             .now = &now, .each = hunk_positioned, .context = &positioning);
    if (changed && (positioning.count <= positioning.max))
    {
        const unsigned long long bytes = (unsigned long long)positioning.count * CLI_DOC_HUNK_WORDS * sizeof(unsigned int);
        one->hunks_cell = (bytes != 0ULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_editor_owner) : NULL;
        if ((bytes == 0ULL) || (one->hunks_cell != NULL))
        {
            if (one->hunks_cell != NULL)
            {
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = one->hunks_cell, .offset = 0u, .src = word, .bytes = bytes});
            }
            one->hunks = positioning.count;
            one->hunks_serial = one->serial;
            one->hunks_known = 1;
        }
    }
    g_cli_guard->persistent_buf_release(word, g_cli_editor_owner);
    if (now.unit != NULL)
    {
        EMBED_CALL(stored_work_exit, DocumentStoreCtx, .lines = &now);
    }
    if (then.unit != NULL)
    {
        EMBED_CALL(stored_work_exit, DocumentStoreCtx, .lines = &then);
    }
    return changed;
}

static int changes_locked(const CliDocumentOutKeyHeadInputCfg *args)
{
    CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one == NULL)
    {
        return 0;
    }
    if (args->head_given)
    {
        // the last commit's file positioned into lines beside the text, in cells sized to it, the changes saved against the
        // file before let go
        one->head_known = 1;
        one->head_has = args->head != NULL;
        EMBED_CALL(stored_cells_release, DocumentStoreCtx, .stored = &one->head);
        EMBED_CALL(hunks_forgotten, DocumentStoreCtx, .one = one);
        if ((args->head != NULL) && !EMBED_CALL(stored_text_take, DocumentStoreCtx, .stored = &one->head,
                                                .text = args->head, .size = args->head_size))
        {
            one->head_known = 0;
            one->head_has = 0;
        }
    }
    if (!one->head_known || !one->head_has)
    {
        return 0;
    }
    // the changes saved at this serial are the diff's answer; moved, the diff runs again
    if (one->hunks_known && (one->hunks_serial == one->serial))
    {
        EMBED_CALL(hunks_replayed, DocumentStoreCtx, .one = one, .each = args->each, .context = args->context);
        return 1;
    }
    return EMBED_CALL(hunks_found, DocumentStoreCtx, .one = one, .each = args->each, .context = args->context);
}

int cli_document_changes(const CliDocumentOutKeyHeadInputCfg *args)
{
    cli_document_lock_take();
    const int answer = changes_locked(args);
    cli_document_lock_give();
    return answer;
}

static int find_locked(const CliDocumentOutNeedleCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(stored_full_text_get, DocumentStoreCtx, .key = args->key);
    if (one == NULL)
    {
        mmgr_line_put(args->err_msg, "no text is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    CliLines work;
    if (!EMBED_CALL(stored_copy_enter, DocumentStoreCtx, .stored_read = &one->lines, .lines = &work))
    {
        mmgr_line_put(args->err_msg, "the text could not be searched: the cells cannot meet it");
        return 0;
    }
    const int found =
        EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = &work, .needle = args->needle,
                   .max = args->max, .out = args->out, .err_msg = args->err_msg);
    EMBED_CALL(stored_work_exit, DocumentStoreCtx, .lines = &work);
    return found;
}

int cli_document_find(const CliDocumentOutNeedleCfg *args)
{
    cli_document_lock_take();
    const int answer = find_locked(args);
    cli_document_lock_give();
    return answer;
}
