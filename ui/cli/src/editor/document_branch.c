// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_branch.c: a stored text colored as a branch of the tree is walked, its grammar applied a line at a time, the
// runs of each line colored passed on and the states it is colored from and ends in saved with the line. The walk works
// out from a line, the view's first, the lines nearest it first, below it before above it at each distance. A run of
// lines below is entered in the state the line above it ends in; a run above starts from root, as far above as it
// reaches; and the lines after a run are colored again until one is entered in the state it was entered in before.
// Once every line is colored, a line entered in a state the line above does not end in is colored again the same way,
// until each line is entered in the state the line above ends in, root for the first. Only the lines colored go on.
// A paged text is walked the same way, its lines read through the pages as far as their index has read, once the page
// mirrors some of them; every line is colored and only the runs of those the page mirrors go on.
// As each line is colored the regions its grammar's roles open and close on it are saved with it, and once every line
// of a text is colored and its regions moved since, its folds go on: each region from the line it opens on to the last
// line the fold hides, a paged text's those that meet the lines the page mirrors.
// The lock is taken only to claim lines and to put in what a coloring found: each batch of lines is claimed under it,
// its lines and the state the first is entered in copied out, read and colored with the lock given back, in a work of
// the coloring's own, and put in under the lock again where the text did not move meanwhile. So the walk, the views
// and the reads color at once, each on its own thread, and every call that reads a text waits on none of them. The walk
// runs on as many threads as the machine has processors to spare, each walking one text at a time, and every coloring
// passes its colors on in the order they were put in
#include "../code/symbols.h"
#include "../code/symbols_internal.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "document_internal.h"
#include "highlight.h"
#include "pages.h"

// The words of a text in one cell of the temporary tier, for one call of cli_document_words_each: each one's start
// among `text` and its length
typedef struct
{
    unsigned int count;
    unsigned long long *at;
    unsigned int *length;
    char *text;
} CliDocumentWords;

// a place of the states' index: the hash of a state's text, and its number
typedef struct
{
    unsigned int hash;
    unsigned int place;
} BranchStatePlace;

// a word of a completion: the hash of its bytes, and its place among the words
typedef struct
{
    unsigned int hash;
    unsigned int word;
} BranchWordPlace;

// A walker:
//
// - `thread`: a thread of its own, which walks one text at a time
// - `walking`: whether it walks, under the lock
typedef struct
{
    CliThread thread;
    int walking;
} CliBranchWalker;
// The runs of runs of lines, passed on as the walk writes them:
//
// - `begun`: whether a `{"from", "runs"}` started
// - `written`: how many runs of lines it holds
// - `any`: whether the walk wrote a run of lines passed on with it before it
// - `out`: where the walk writes them
// - `columns`: the columns of each line whose runs go on, 0 for every one
// - `names`: how many names the tokens of the grammar take. That is also the place of the first name that a run which
//   counts depth takes by its depth. It is 0 for a grammar that names none.
// - `bytes`: the bytes of lines that it still colors, CLI_DOC_ALL_BYTES where it colors every line asked
// - `next`: the line after the last whose runs the walk wrote
// - `stale`: whether the text moved while it colored, and then it colors no more
// - `requested`: whether the lines whose runs go on are those that it asks, from `hand_from` to before `hand_to`. Else
//   they are those that the page holds as each goes in.
// - `high`: the line that it colors no further than, as it goes on past the lines asked
//
// Its columns read as those of the text as each line goes in, where it names them
typedef struct
{
    int begun;
    unsigned int written;
    int *any;
    VerbaLinea *out;
    unsigned long long columns;
    unsigned int names;
    unsigned long long bytes;
    unsigned int next;
    int stale;
    int requested;
    unsigned int hand_from;
    unsigned int hand_to;
    unsigned int high;
} CliBranchRange;
// the max names that a scope holds, and the bytes that their texts take
#define CLI_DOC_SCOPE_NAMES 128u
#define CLI_DOC_SCOPE_TEXT 4096u
// The scope that a line is in, as the walk reads it:
//
// - `depth`: the depth of the regions open
// - `pairs`: the depth of the pairs open that count it
// - `count`: how many names the regions declare that a use of the name takes its name from
// - for each name: `text_at`, the place of its text among the texts, and `size`, its bytes. `named` is the name that
//   the run of its declaration takes, and `belongs` the depth of the region that it belongs to. `waits` is whether it
//   waits for that region to open, as the parameter of a function waits for its body.
// - `used` and `text`: the bytes of the texts that it uses, and the texts
// - `place`: its place among the states of the text, CLI_DOC_NONE where the walk did not find it
// - `moved`: whether it moved since then, the only case where the walk finds it again
typedef struct
{
    unsigned int depth;
    unsigned int pairs;
    unsigned int count;
    unsigned int text_at[CLI_DOC_SCOPE_NAMES];
    unsigned int size[CLI_DOC_SCOPE_NAMES];
    unsigned int named[CLI_DOC_SCOPE_NAMES];
    unsigned int belongs[CLI_DOC_SCOPE_NAMES];
    unsigned char waits[CLI_DOC_SCOPE_NAMES];
    unsigned int used;
    char text[CLI_DOC_SCOPE_TEXT];
    unsigned int place;
    int moved;
} CliBranchScope;
// A bundle of lines as the walk colors it, with no lock taken:
//
// - `named`, `found` and their capacities: the work that the walk writes it into
// - `filled`: whether the work is full. The lines past the capacity go to the next bundle.
// - `scope`: the scope that the line colored next is in
// - `positioned`: whether the walk knows the place, among the states of the text, of the scope that the line before it
//   ended in
// - `names`, `roles` and `roles_count`: the names that the tokens of the grammar take, and the roles of each, read as
//   the walk claimed the lines
// - `used`: the words of what it found, written
// - `refused`: whether the cells refused the capacity for them
//
// What it finds on each line is a record of words. It is how many runs, and the regions of the line. Then the bytes of
// the state of the colorer that the line ends in, and the bytes of the scope that it ends in. The scope is CLI_DOC_NONE
// for a scope with no depth, pairs or names, and CLI_DOC_SCOPE_SAME for the one that the line before it ended in. Then
// come the columns of the runs, and their names as the walk names them. Last come the state of the colorer and the
// scope, each of the two from a word of its own
typedef struct
{
    unsigned int *named;
    unsigned long long named_capacity;
    unsigned int *found;
    unsigned long long found_capacity;
    int filled;
    CliBranchScope scope;
    int positioned;
    unsigned int names;
    const unsigned int *roles;
    unsigned int roles_count;
    unsigned long long used;
    int refused;
} CliBranchFinding;
// the constants of a coloring:
//
// - CLI_DOC_ROOT: the place of root among the states of a colored text, the first written
// - CLI_DOC_STEP: the max lines that a step of the walk colors in a run, and again after it
// - CLI_DOC_STEP_BYTES: the max bytes of lines that it colors. The last line that it colors can pass them, and its
//   colors go on in steps.
// - CLI_DOC_ALL_BYTES: the bytes of a coloring that colors every line asked
// - CLI_DOC_BATCH: the lines colored at once where a coloring stops at the first line entered as before
// - CLI_DOC_RUN: the lines colored at once where it does not stop, and CLI_DOC_STEP where its bytes reach no further
// - CLI_DOC_COLORS_CAPACITY: the capacity that the walk writes the colors of a coloring in
// - CLI_DOC_STATE: the longest text of a state
// - CLI_DOC_STATES_MAX: the max states that a text has
// - CLI_DOC_STATE_RUN: the places of the run of the newest states in its index
// - CLI_DOC_NONE: no line
#define CLI_DOC_ROOT 0u
#define CLI_DOC_STEP 256u
#define CLI_DOC_STEP_BYTES 16384ULL
#define CLI_DOC_ALL_BYTES 0xffffffffffffffffULL
#define CLI_DOC_BATCH 64u
#define CLI_DOC_RUN 4096u
#define CLI_DOC_COLORS_CAPACITY (1ULL << 20u)
#define CLI_DOC_STATE 8192u
#define CLI_DOC_STATES_MAX 8192u
#define CLI_DOC_STATE_RUN 256u
#define CLI_DOC_NONE 0xffffffffu

// what the walk saves of a line: how many runs it found, and the column that the line ends at. Then the columns of the
// runs, and their names
#define CLI_DOC_RECORD_HEAD 2u

// the max walkers. Then where the colors go, and whether walkers walk them out. Then how many can walk at once: the
// processors of the machine but one, and one at the fewest. Then the walkers, and past them the walk that a caller
// takes on its own thread. Last, the lock that the colors go on under, taken before the walk gives back the lock of the
// texts. The colors go in the order that they went in
#define CLI_DOC_WALKERS 8u
static CliDocColored s_colored;
static int s_threaded;
static unsigned int s_walkers_max;
static CliBranchWalker s_walkers[CLI_DOC_WALKERS + 1u];

// A cell of the temporary tier that a read over many lines takes once, and reads each line into. It is the cell, NULL
// for none, and its bytes. It grows by half again where a line needs more, and goes back as the read ends
typedef struct
{
    void *cell;
    unsigned long long room;
} BranchWork;

typedef struct
{
    const char *text;
    unsigned long long length;
    const CliStoredText *one_read;
    unsigned int at;
    unsigned int hash_u32;
    CliStoredText *one;
    const CliStoredLines *lines;
    unsigned int line;
    VerbaLinea *out;
    unsigned int from;
    unsigned int to;
    char *text_out;
    unsigned long long capacity;
    const char **line_characters;
    unsigned int first;
    unsigned int end;
    unsigned long long bytes;
    unsigned int *read;
    CliBranchRange *range;
    const CliLeaves *leaves;
    unsigned long long more;
    const CliBranchFinding *finding;
    unsigned int name;
    CliBranchScope *scope;
    unsigned int depth;
    int waiting;
    unsigned int size;
    unsigned int named;
    unsigned int belongs;
    int waits;
    const CliBranchScope *scope_read;
    unsigned long long size_u64;
    const char **from_characters;
    char after;
    unsigned int place;
    CliBranchFinding *finding_out;
    int stopping;
    unsigned int *entering;
    int *stopped;
    int yielding;
    int down;
    unsigned int low;
    unsigned int high;
    unsigned int *first_pointer;
    unsigned int *end_pointer;
    unsigned int *low_pointer;
    unsigned int *high_pointer;
    int *any_pointer;
    unsigned long long columns;
    int onward;
    int any;
    int *transferred;
    unsigned int opened;
    unsigned int close;
    unsigned int regions;
    unsigned int filled;
    CliFoldEach each;
    void *context;
    VerbaLinea *colors;
    CliBranchWalker *walker;
    int own;
    unsigned int passed_from;
    unsigned int passed_to;
    int made;
    const unsigned int *record;
    const CarcerMovableEntry *map_cell;
    unsigned int map_count;
    unsigned int *bars_pointer;
    unsigned int tab;
    CliDocumentWords *words;
    BranchWordPlace *word_order;
    unsigned long long word_at;
    const unsigned long long *number;
    unsigned long long cursor_line;
    unsigned long long cursor_col;
    unsigned long long *max_pointer;
    unsigned long long *units_pointer;
    VerbaLinea *entered;
    unsigned long long first_u64;
    unsigned long long last_u64;
    int mirrored;
    unsigned int roles;
    int renames;
    unsigned int names;
    unsigned int *found_bits;
    unsigned int *closes_pointer;
    unsigned int *unled_pointer;
    int mark;
    unsigned int line_number;
    unsigned int type;
    unsigned int word_value;
    unsigned int *place_out;
    unsigned long long *length_out;
    const CarcerMovableEntry *entry;
    unsigned long long at_u64;
    int deep;
    const char *grammar;
    const char *key;
    BranchWork *work;
    const unsigned short *units;
    unsigned long long count_u64;
    const void *copy_read;
} DocumentBranchCtx;

// the cell of the state at the place `at` among the text's states, NULL past them
EMBED_INLINE const CarcerMovableEntry *branch_state_cell(const DocumentBranchCtx *args)
{
    const CarcerMovableEntry *cell = NULL;
    if ((args->one_read->state_cells != NULL) && (args->at < args->one_read->state_count))
    {
        (void)g_platform_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = args->one_read->state_cells,
                                .offset = (unsigned long long)args->at * sizeof(const CarcerMovableEntry *),
                                .dst = &cell,
                                .bytes = sizeof(const CarcerMovableEntry *)});
    }
    return cell;
}

// the state at the place `at` among the states of the text, copied into `text_out`, CLI_DOC_STATE bytes, ended by a
// NUL. The return is its length, 0 with an empty text where no state is there
EMBED_INLINE unsigned long long branch_state_text(const DocumentBranchCtx *args)
{
    args->text_out[0] = '\0';
    const CarcerMovableEntry *const cell =
        EMBED_CALL(branch_state_cell, DocumentBranchCtx, .one_read = args->one_read, .at = args->at);
    if (cell == NULL)
    {
        return 0ULL;
    }
    const embed_word measured = g_platform_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = cell});
    const unsigned long long length = (measured < CLI_DOC_STATE) ? measured : (CLI_DOC_STATE - 1ULL);
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = args->text_out, .bytes = length});
    args->text_out[length] = '\0';
    return length;
}

// The work cell `work` made ready for `at_u64` bytes. The cell stays where it holds them. Else it goes back, and a new
// one takes its place with half the bytes again as room. Returns the cell, or NULL where the cells cannot meet it
EMBED_INLINE void *branch_work_room(const DocumentBranchCtx *args)
{
    BranchWork *const work = args->work;
    if ((work->cell == NULL) || (work->room < args->at_u64))
    {
        g_platform_guard->persistent_buf_release(work->cell, g_cli_editor_owner);
        const unsigned long long room = args->at_u64 + (args->at_u64 / 2ULL);
        work->cell = g_platform_guard->temporary_buf_alloc(room, g_cli_editor_owner);
        work->room = (work->cell != NULL) ? room : 0ULL;
    }
    return work->cell;
}

// The record of the runs saved at `place` read into a cell of the temporary tier. Where the caller gives the cell
// `work`, the record goes into it and the cell stays the caller's. Else the record goes into a cell sized to it that
// goes back with persistent_buf_release. Returns the cell, or NULL where the cells cannot meet it
EMBED_INLINE unsigned int *branch_record_alloc(const DocumentBranchCtx *args)
{
    unsigned int runs = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->one_read->run,
                                                            .offset = (unsigned long long)args->place * sizeof(runs),
                                                            .dst = &runs,
                                                            .bytes = sizeof(runs)});
    const unsigned long long words = CLI_DOC_RECORD_HEAD + (2ULL * runs);
    unsigned int *const record =
        (args->work != NULL) ? EMBED_CALL(branch_work_room, DocumentBranchCtx, .work = args->work,
                                          .at_u64 = words * sizeof(unsigned int))
                             : g_platform_guard->temporary_buf_alloc(words * sizeof(unsigned int), g_cli_editor_owner);
    if (record != NULL)
    {
        (void)g_platform_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = args->one_read->run,
                                .offset = (unsigned long long)args->place * sizeof(*record),
                                .dst = record,
                                .bytes = words * sizeof(*record)});
    }
    return record;
}

// The line `line_number` of `one_read`'s record read out of the runs' cell into one of the temporary tier. It goes
// into `work` or a cell of its own, as branch_record_alloc reads it. The record's place goes into `place_out` and the
// line's length into `length_out`. Returns the cell, or NULL where the line has no record or the cells cannot meet it
EMBED_INLINE unsigned int *branch_line_record(const DocumentBranchCtx *args)
{
    const CliStoredLines *const lines = &args->one_read->lines;
    unsigned int place = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->one_read->lines.words,
        .offset = (((unsigned long long)(CLI_DOC_WORD_RUNS)*args->one_read->lines.lines) + (args->line_number)) *
                  sizeof(place),
        .dst = &place,
        .bytes = sizeof(place)});
    *args->place_out = place;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = lines->lengths,
                                                            .offset = args->line_number * sizeof(*args->length_out),
                                                            .dst = args->length_out,
                                                            .bytes = sizeof(*args->length_out)});
    return (place != CLI_DOC_NO_RUNS) ? EMBED_CALL(branch_record_alloc, DocumentBranchCtx, .one_read = args->one_read,
                                                   .place = place, .work = args->work)
                                      : NULL;
}

// `bytes` of zeros written from `at` of the movable cell `cell`, a block at a time
EMBED_INLINE void branch_cell_zeroed(const DocumentBranchCtx *args)
{
    static const unsigned char s_zeros[4096];
    for (unsigned long long done = 0ULL; done < args->bytes; done += sizeof(s_zeros))
    {
        const unsigned long long step =
            ((args->bytes - done) < sizeof(s_zeros)) ? (args->bytes - done) : sizeof(s_zeros);
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = args->entry, .offset = args->at_u64 + done, .src = s_zeros, .bytes = step});
    }
}
// the hash of `length` bytes of `text`, FNV-1a over its bytes
EMBED_INLINE unsigned int branch_state_hash(const DocumentBranchCtx *args)
{
    unsigned int hash = 2166136261u;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        hash = (hash ^ (unsigned char)args->text[at]) * 16777619u;
    }
    return hash;
}

// whether the state at the place `at` among the text's states is `length` bytes of `text`
EMBED_INLINE int branch_state_is(const DocumentBranchCtx *args)
{
    const CarcerMovableEntry *const cell =
        EMBED_CALL(branch_state_cell, DocumentBranchCtx, .one_read = args->one_read, .at = args->at);
    // the call compares the length of the state first, so that a state that the text only starts differs from it
    return (cell != NULL) && (g_platform_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = cell}) == args->length) &&
           (g_platform_guard->movable_buf_compare(
                &(CarcerMovableCfg){.cell = cell, .src = args->text, .bytes = args->length + 1ULL}) == 0);
}

// the order of two places, by their hash: below 0, 0 or above 0
static int branch_place_order(const void *left, const void *right)
{
    const BranchStatePlace *const one = (const BranchStatePlace *)left;
    const BranchStatePlace *const other = (const BranchStatePlace *)right;
    return (one->hash < other->hash) ? -1 : ((one->hash > other->hash) ? 1 : 0);
}

// the place `at` of the index's order read
static BranchStatePlace branch_index_place(const CliStoredText *one, unsigned int at)
{
    BranchStatePlace place = {0u, 0u};
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one->state_index,
                                                            .offset = (unsigned long long)at * sizeof(BranchStatePlace),
                                                            .dst = &place,
                                                            .bytes = sizeof(place)});
    return place;
}

// the number of the state of `length` bytes of `text`, of the hash `hash`, CLI_DOC_NONE where the text has none. The
// call looks in the run first, then in the order from the first place of the hash, which a binary search finds
static unsigned int branch_index_found(const DocumentBranchCtx *args, unsigned int hash, unsigned long long length)
{
    const CliStoredText *const one = args->one;
    BranchStatePlace run[CLI_DOC_STATE_RUN];
    if (one->state_run != 0u)
    {
        (void)g_platform_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = one->state_index,
                                .offset = (unsigned long long)CLI_DOC_STATES_MAX * sizeof(BranchStatePlace),
                                .dst = run,
                                .bytes = (unsigned long long)one->state_run * sizeof(BranchStatePlace)});
    }
    for (unsigned int each = 0u; each < one->state_run; each += 1u)
    {
        if ((run[each].hash == hash) && EMBED_CALL(branch_state_is, DocumentBranchCtx, .one_read = one,
                                                   .at = run[each].place, .text = args->text, .length = length))
        {
            return run[each].place;
        }
    }
    unsigned int low = 0u;
    unsigned int high = one->state_ordered;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (branch_index_place(one, middle).hash < hash)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    for (unsigned int each = low; each < one->state_ordered; each += 1u)
    {
        const BranchStatePlace place = branch_index_place(one, each);
        if (place.hash != hash)
        {
            break;
        }
        if (EMBED_CALL(branch_state_is, DocumentBranchCtx, .one_read = one, .at = place.place, .text = args->text,
                       .length = length))
        {
            return place.place;
        }
    }
    return CLI_DOC_NONE;
}

// the run of the index, where it is full, sorted once and merged into the order from its end. The call does that
// through a cell of the temporary tier that holds the order and the run. The return is 1, or 0 where the cells cannot
// meet it
static int branch_index_merged(CliStoredText *one)
{
    if (one->state_run < CLI_DOC_STATE_RUN)
    {
        return 1;
    }
    const unsigned int count = one->state_ordered + one->state_run;
    BranchStatePlace *const order =
        g_platform_guard->temporary_buf_alloc((unsigned long long)count * sizeof(BranchStatePlace), g_cli_editor_owner);
    if (order == NULL)
    {
        return 0;
    }
    BranchStatePlace run[CLI_DOC_STATE_RUN];
    BranchStatePlace room[CLI_DOC_STATE_RUN];
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = one->state_index, .dst = order, .bytes = (unsigned long long)count * sizeof(BranchStatePlace)});
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = one->state_index,
                            .offset = (unsigned long long)CLI_DOC_STATES_MAX * sizeof(BranchStatePlace),
                            .dst = run,
                            .bytes = sizeof(run)});
    (void)memor.sort(&(MemoriaCfg){.dst = run,
                                   .scratch = room,
                                   .count = CLI_DOC_STATE_RUN,
                                   .bytes = sizeof(BranchStatePlace),
                                   .order = branch_place_order});
    unsigned int from_order = one->state_ordered;
    unsigned int from_run = CLI_DOC_STATE_RUN;
    unsigned int to = count;
    while (from_run != 0u)
    {
        const int order_first = (from_order != 0u) && (order[from_order - 1u].hash > run[from_run - 1u].hash);
        to -= 1u;
        if (order_first)
        {
            from_order -= 1u;
            order[to] = order[from_order];
        }
        else
        {
            from_run -= 1u;
            order[to] = run[from_run];
        }
    }
    (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = one->state_index, .src = order, .bytes = (unsigned long long)count * sizeof(BranchStatePlace)});
    g_platform_guard->persistent_buf_release(order, g_cli_editor_owner);
    one->state_ordered = count;
    one->state_run = 0u;
    return 1;
}

// the state at the place `at`, of the hash `hash`, put at the end of the index's run, which has room for it
EMBED_INLINE void branch_index_append(const DocumentBranchCtx *args)
{
    const BranchStatePlace place = {args->hash_u32, args->at};
    (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = args->one->state_index,
        .offset = ((unsigned long long)CLI_DOC_STATES_MAX + args->one->state_run) * sizeof(BranchStatePlace),
        .src = &place,
        .bytes = sizeof(place)});
    args->one->state_run += 1u;
}

// the index and the cells of the states, taken at their max where they are untaken. The return is 1, or 0 where the
// cells cannot meet them or the text holds CLI_DOC_STATES_MAX states
EMBED_INLINE int branch_index_init(const DocumentBranchCtx *args)
{
    if (args->one->state_indexed == 0u)
    {
        const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc(
            (CLI_DOC_STATES_MAX + CLI_DOC_STATE_RUN) * sizeof(BranchStatePlace), g_cli_editor_owner);
        if (cell == NULL)
        {
            return 0;
        }
        const CarcerMovableEntry *const cells =
            g_platform_guard->movable_buf_alloc(CLI_DOC_STATES_MAX * sizeof(const CarcerMovableEntry *), g_cli_editor_owner);
        if (cells == NULL)
        {
            (void)g_platform_guard->movable_buf_release(cell, g_cli_editor_owner);
            return 0;
        }
        args->one->state_index = cell;
        args->one->state_cells = cells;
        args->one->state_indexed = 1u;
        args->one->state_ordered = 0u;
        args->one->state_run = 0u;
    }
    return (args->one->state_count + 1u) <= CLI_DOC_STATES_MAX;
}

// the place of the state `text` among the states of the text, found by its hash, with the place found last tried
// first. The call writes it after them where it is not there. The return is the place of root where the cells cannot
// meet it, with the colors of the text refused
static unsigned int branch_state(const DocumentBranchCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    if ((args->one->state_count != 0u) && EMBED_CALL(branch_state_is, DocumentBranchCtx, .one_read = args->one,
                                                     .at = args->one->state_last, .text = args->text, .length = length))
    {
        return args->one->state_last;
    }
    if (!branch_index_init(args))
    {
        args->one->refused = 1;
        return CLI_DOC_ROOT;
    }
    const unsigned int hash = EMBED_CALL(branch_state_hash, DocumentBranchCtx, .text = args->text, .length = length);
    const unsigned int found = branch_index_found(args, hash, length);
    if (found != CLI_DOC_NONE)
    {
        args->one->state_last = found;
        return found;
    }
    if ((args->one->state_count >= CLI_DOC_STATES_MAX) || !branch_index_merged(args->one))
    {
        args->one->refused = 1;
        return CLI_DOC_ROOT;
    }
    // the state in a cell of its own, with the size of its text. Its identity goes after those of the states before
    // it, and its place is the number of states before it
    const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc(length + 1ULL, g_cli_editor_owner);
    if (cell == NULL)
    {
        args->one->refused = 1;
        return CLI_DOC_ROOT;
    }
    (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = args->text, .bytes = length + 1ULL});
    const unsigned int at = args->one->state_count;
    (void)g_platform_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = args->one->state_cells,
                            .offset = (unsigned long long)at * sizeof(const CarcerMovableEntry *),
                            .src = &cell,
                            .bytes = sizeof(const CarcerMovableEntry *)});
    args->one->state_count += 1u;
    EMBED_CALL(branch_index_append, DocumentBranchCtx, .one = args->one, .at = at, .hash_u32 = hash);
    args->one->state_last = at;
    return at;
}

void cli_document_states_released(const CliDocumentCtx *args)
{
    CliStoredText *const one = args->one;
    for (unsigned int at = 0u; at < one->state_count; at += 1u)
    {
        const CarcerMovableEntry *const cell =
            EMBED_CALL(branch_state_cell, DocumentBranchCtx, .one_read = one, .at = at);
        (void)g_platform_guard->movable_buf_release(cell, g_cli_editor_owner);
    }
    one->state_count = 0u;
    one->state_ordered = 0u;
    one->state_run = 0u;
}

// the `count_u64` units at `units` written as UTF-8 into `out`, a surrogate pair read as the one character it makes
EMBED_INLINE void branch_units_utf8(const DocumentBranchCtx *args)
{
    const unsigned short *const unit = args->units;
    const unsigned long long count = args->count_u64;
    unsigned long long place = 0ULL;
    while (place < count)
    {
        unsigned int character = unit[place];
        const int paired = (character >= 0xd800u) && (character < 0xdc00u) && ((place + 1ULL) < count) &&
                           (unit[place + 1ULL] >= 0xdc00u) && (unit[place + 1ULL] < 0xe000u);
        if (paired)
        {
            character = 0x10000u + ((character - 0xd800u) << 10u) + (unit[place + 1ULL] - 0xdc00u);
            place += 1ULL;
        }
        EMBED_CALL(cli_line_character_internal.utf8_append, CliLineLetterCtx, .line = args->out,
                   .character = character);
        place += 1ULL;
    }
}

// The line `line` of `lines` written as UTF-8 into `out`, a surrogate pair read as the one character it makes. Its
// units go into the work cell `work` where the caller gives one, and the cell stays the caller's. Else they go into a
// cell of their own that goes back before the call returns
EMBED_INLINE void branch_line_append(const DocumentBranchCtx *args)
{
    unsigned int line = args->line;
    unsigned long long count = 0ULL;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->lines->lengths, .offset = line * sizeof(count), .dst = &count, .bytes = sizeof(count)});
    unsigned short *const unit =
        (count == 0ULL)        ? NULL
        : (args->work != NULL) ? EMBED_CALL(branch_work_room, DocumentBranchCtx, .work = args->work,
                                            .at_u64 = count * sizeof(unsigned short))
                               : g_platform_guard->temporary_buf_alloc(count * sizeof(unsigned short), g_cli_editor_owner);
    if (unit == NULL)
    {
        return;
    }
    unsigned long long line_start = 0ULL;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->lines->starts,
                                                            .offset = line * sizeof(line_start),
                                                            .dst = &line_start,
                                                            .bytes = sizeof(line_start)});
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->lines->units,
                                                            .offset = line_start * sizeof(unsigned short),
                                                            .dst = unit,
                                                            .bytes = count * sizeof(unsigned short)});
    EMBED_CALL(branch_units_utf8, DocumentBranchCtx, .units = unit, .count_u64 = count, .out = args->out);
    if (args->work == NULL)
    {
        g_platform_guard->persistent_buf_release(unit, g_cli_editor_owner);
    }
}

// the lines from `from` to `to` of `lines` written as UTF-8 into `text`, `capacity` bytes, each ended by a NUL and each
// line's start put into `line`
EMBED_INLINE void branch_lines_write(const DocumentBranchCtx *args)
{
    VerbaLinea out = {args->text_out, args->capacity, 0ULL};
    // each line's units read into one work cell of the temporary tier, which goes back as the write ends
    BranchWork work = {NULL, 0ULL};
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        args->line_characters[at - args->from] = args->text_out + out.at;
        EMBED_CALL(branch_line_append, DocumentBranchCtx, .lines = args->lines, .line = at, .out = &out, .work = &work);
        mmgr_line_ch(&out, '\0');
    }
    g_platform_guard->persistent_buf_release(work.cell, g_cli_editor_owner);
}

// the lines from `first` to `end` of a text read whole, `lines`, measured. The call puts into `read` the line after
// the last that `bytes` of their units reach, one at the least. Then the bytes that they take as UTF-8, with each unit
// written as three bytes at max, and the NUL of each line. This runs under the lock
EMBED_INLINE unsigned long long branch_lines_measure(const DocumentBranchCtx *args)
{
    unsigned int *read = args->read;
    unsigned long long capacity = 1ULL;
    unsigned long long spent = 0ULL;
    unsigned int to = args->first;
    while ((to < args->end) && ((to == args->first) || (args->bytes == CLI_DOC_ALL_BYTES) || (spent < args->bytes)))
    {
        unsigned long long length = 0ULL;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = args->lines->lengths, .offset = to * sizeof(length), .dst = &length, .bytes = sizeof(length)});
        capacity += (3ULL * length) + 1ULL;
        spent += length + 1ULL;
        to += 1u;
    }
    *read = to;
    return capacity;
}

// the mark after which the state of a line carries the place of its scope, past the own state of the colorer. The
// scope goes among the states of the text once
#define CLI_DOC_SCOPE_MARK '\x1e'

// the constants of a found line:
//
// - CLI_DOC_FOUND_STATE: the words of capacity that the state and scope of a line get. Those of a line past them come
//   from the capacity of the lines after it.
// - CLI_DOC_SCOPE_SAME: the scope of a line, where it equals the scope that the line before it ended in
// - CLI_DOC_FOUND_HEAD: the words of a record before its runs
#define CLI_DOC_FOUND_STATE 64ULL
#define CLI_DOC_SCOPE_SAME 0xfffffffeu
#define CLI_DOC_FOUND_HEAD 5u

// the runs of a line colored in the range: the range marked as the one that colored it, and the line that it reached
EMBED_INLINE void branch_runs_append(const DocumentBranchCtx *args)
{
    *args->range->any = 1;
    args->range->next = args->line + 1u;
}

// the runs the line `line_number` of `one_read` holds in its record, the record's place in the runs' cell into
// `place_out`: 0 with CLI_DOC_NO_RUNS where it has none
EMBED_INLINE unsigned int branch_line_runs(const DocumentBranchCtx *args)
{
    unsigned int place = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->one_read->lines.words,
        .offset = (((unsigned long long)(CLI_DOC_WORD_RUNS)*args->one_read->lines.lines) + (args->line_number)) *
                  sizeof(place),
        .dst = &place,
        .bytes = sizeof(place)});
    *args->place_out = place;
    unsigned int line_runs = 0u;
    if (place != CLI_DOC_NO_RUNS)
    {
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->one_read->run,
                                                                .offset = (unsigned long long)place * sizeof(line_runs),
                                                                .dst = &line_runs,
                                                                .bytes = sizeof(line_runs)});
    }
    return line_runs;
}

// the words of the runs that the lines of `one` save, gathered in the order of their lines. They go into a cell with
// their size, and `more` words after them. The cell takes the place of the cell that they were in. The return is 1, or
// 0 where the cells cannot meet it, with the runs and their places as they were. The call takes both cells before the
// place of a line moves, thus a refusal changes nothing
EMBED_INLINE int branch_runs_capacity(const DocumentBranchCtx *args)
{
    const CliStoredLines *const lines = &args->one->lines;
    // the words the lines' records take, each read off its head
    unsigned long long saved = 0ULL;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        unsigned int at = 0u;
        const unsigned int line_runs = EMBED_CALL(branch_line_runs, DocumentBranchCtx, .one_read = args->one,
                                                  .line_number = line, .place_out = &at);
        if (at != CLI_DOC_NO_RUNS)
        {
            saved += CLI_DOC_RECORD_HEAD + (2ULL * line_runs);
        }
    }
    // the cell holds the runs and the words added twice over. Thus the next saves fill the room before the call
    // gathers the runs again
    const unsigned long long capacity = 2ULL * (saved + args->more);
    if (capacity > 0xffffffffULL)
    {
        return 0;
    }
    // the records gathered in their lines' order in a work cell, then a cell sized to them and the words added
    unsigned int *const work = g_platform_guard->temporary_buf_alloc(capacity * sizeof(*work), g_cli_editor_owner);
    if (work == NULL)
    {
        return 0;
    }
    const CarcerMovableEntry *const moved =
        (capacity != 0ULL) ? g_platform_guard->movable_buf_alloc(capacity * sizeof(*work), g_cli_editor_owner) : NULL;
    if ((moved == NULL) && (capacity != 0ULL))
    {
        g_platform_guard->persistent_buf_release(work, g_cli_editor_owner);
        return 0;
    }
    unsigned int used = 0u;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        unsigned int at = 0u;
        // a line's runs lie within the words saved, which an unsigned int counts
        const unsigned int line_runs = EMBED_CALL(branch_line_runs, DocumentBranchCtx, .one_read = args->one,
                                                  .line_number = line, .place_out = &at);
        if (at != CLI_DOC_NO_RUNS)
        {
            const unsigned int words = CLI_DOC_RECORD_HEAD + (2u * line_runs);
            (void)g_platform_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = args->one->run,
                                    .offset = (unsigned long long)at * sizeof(*work),
                                    .dst = work + used,
                                    .bytes = (unsigned long long)words * sizeof(*work)});
            {
                const unsigned int stored_value = used;
                (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = lines->words,
                    .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + line) * sizeof(stored_value),
                    .src = &stored_value,
                    .bytes = sizeof(stored_value)});
            }
            used += words;
        }
    }
    if (used != 0u)
    {
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = moved, .src = work, .bytes = (unsigned long long)used * sizeof(*work)});
    }
    g_platform_guard->persistent_buf_release(work, g_cli_editor_owner);
    (void)g_platform_guard->movable_buf_release(args->one->run, g_cli_editor_owner);
    args->one->run = moved;
    // a capacity checked above to be no more than an unsigned int counts
    args->one->run_capacity = (unsigned int)capacity;
    args->one->run_used = used;
    return 1;
}

// the runs of `leaves`, the line `line` of `one` as the walk named them, saved in place of those it saved, none where
// the cells cannot meet them
EMBED_INLINE void branch_runs_save(const DocumentBranchCtx *args)
{
    const CliStoredLines *const lines = &args->one->lines;
    {
        const unsigned int stored_value = CLI_DOC_NO_RUNS;
        (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + args->line) * sizeof(stored_value),
            .src = &stored_value,
            .bytes = sizeof(stored_value)});
    }
    const unsigned long long words = CLI_DOC_RECORD_HEAD + (2ULL * args->leaves->runs);
    if (((args->one->run_used + words) > args->one->run_capacity) &&
        !EMBED_CALL(branch_runs_capacity, DocumentBranchCtx, .one = args->one, .more = words))
    {
        return;
    }
    const unsigned long long at = (unsigned long long)args->one->run_used * sizeof(unsigned int);
    const unsigned int head[CLI_DOC_RECORD_HEAD] = {args->leaves->runs, args->leaves->end};
    (void)g_platform_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = args->one->run, .offset = at, .src = head, .bytes = sizeof(head)});
    (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = args->one->run,
                                                             .offset = at + sizeof(head),
                                                             .src = args->leaves->col,
                                                             .bytes = args->leaves->runs * sizeof(unsigned int)});
    (void)g_platform_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = args->one->run,
                            .offset = at + sizeof(head) + (args->leaves->runs * sizeof(unsigned int)),
                            .src = args->leaves->name,
                            .bytes = args->leaves->runs * sizeof(unsigned int)});
    {
        const unsigned int stored_value = args->one->run_used;
        (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + args->line) * sizeof(stored_value),
            .src = &stored_value,
            .bytes = sizeof(stored_value)});
    }
    // the words written lie within the capacity, which an unsigned int counts
    args->one->run_used += (unsigned int)words;
}

// whether the line `line` of `one` saves runs that it can hand on as they are. That is where the walk colored it, it
// saves runs, and the walk entered it in the state that the line above it ends in. Or the walk entered it from root,
// where the walk did not color that line
EMBED_INLINE int branch_runs_existing(const DocumentBranchCtx *args)
{
    const CliStoredLines *const lines = &args->one_read->lines;
    unsigned int word_1 = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = lines->words,
        .offset = (((unsigned long long)CLI_DOC_WORD_ENDED * lines->lines) + (args->line - 1u)) * sizeof(word_1),
        .dst = &word_1,
        .bytes = sizeof(word_1)});

    const unsigned int above = (args->line != 0u) ? word_1 : CLI_DOC_UNCOLORED;
    unsigned int begun = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = lines->words,
        .offset = (((unsigned long long)CLI_DOC_WORD_BEGUN * lines->lines) + args->line) * sizeof(begun),
        .dst = &begun,
        .bytes = sizeof(begun)});
    unsigned int line_runs = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = lines->words,
        .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + args->line) * sizeof(line_runs),
        .dst = &line_runs,
        .bytes = sizeof(line_runs)});
    return (begun != CLI_DOC_UNCOLORED) && (line_runs != CLI_DOC_NO_RUNS) &&
           ((above == CLI_DOC_UNCOLORED) ? (begun == CLI_DOC_ROOT) : (begun == above));
}

// one name of the grammar of the text at `context` its roles read and saved among the text's roles at its place
static void branch_role_read(const CliNameEachCfg *args)
{
    const char *name = args->name;
    CliStoredText *const one = (CliStoredText *)args->context;
    if (args->place < one->roles_capacity)
    {
        const unsigned int roles =
            CLI_DOC_ROLES_KNOWN | EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = name);
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = one->roles,
                                .offset = (unsigned long long)args->place * sizeof(roles),
                                .src = &roles,
                                .bytes = sizeof(roles)});
    }
}

// every name of the grammar of `one`, with its roles read from its segments and saved among the roles of the text by
// its place. Above them goes one more than how many names before it count the depth of a pair, once for the grammar.
// A coloring copies them as it claims its lines. This runs under the lock
EMBED_INLINE void branch_roles_read(const DocumentBranchCtx *args)
{
    const unsigned int names = (args->one->names != 0u) ? args->one->names : 1u;
    if (args->one->roles_complete || (names >= CLI_DOC_NONE))
    {
        return;
    }
    if (names != args->one->roles_capacity)
    {
        // the call reads the roles again over a cell with the size of the names counted
        (void)g_platform_guard->movable_buf_release(args->one->roles, g_cli_editor_owner);
        args->one->roles =
            g_platform_guard->movable_buf_alloc((unsigned long long)names * sizeof(unsigned int), g_cli_editor_owner);
        if (args->one->roles == NULL)
        {
            args->one->roles_capacity = 0u;
            return;
        }
        args->one->roles_capacity = names;
    }
    EMBED_CALL(branch_cell_zeroed, DocumentBranchCtx, .entry = args->one->roles, .at_u64 = 0ULL,
               .bytes = (unsigned long long)args->one->roles_capacity * sizeof(unsigned int));
    (void)EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = args->one->grammar,
                     .each_name_each = branch_role_read, .context = args->one);
    unsigned int rank = 0u;
    for (unsigned int place = 0u; place < args->one->roles_capacity; place += 1u)
    {
        // the rank saved plus 1, a name's place among no more than CLI_DOC_RANKS
        unsigned int roles = 0u;
        if (place < args->one->roles_capacity)
        {
            (void)g_platform_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = args->one->roles,
                                    .offset = (unsigned long long)(place) * sizeof(roles),
                                    .dst = &roles,
                                    .bytes = sizeof(roles)});
        }
        roles |= (rank < CLI_DOC_RANKS) ? ((rank + 1u) << CLI_DOC_RANK_AT) : 0u;
        (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = args->one->roles,
                                                                 .offset = (unsigned long long)place * sizeof(roles),
                                                                 .src = &roles,
                                                                 .bytes = sizeof(roles)});
        rank += ((roles & CLI_ROLE_PAIR) != 0u) ? 1u : 0u;
    }
    args->one->roles_complete = 1;
}

// the roles that the name at `name` among the names of `leaves` gives a run. They are those that the text saved as the
// walk claimed the lines of the bundle, else read from its segments
EMBED_INLINE unsigned int branch_roles(const DocumentBranchCtx *args)
{
    unsigned int name = args->name;
    return ((name < args->finding->roles_count) && ((args->finding->roles[name] & CLI_DOC_ROLES_KNOWN) != 0u))
               ? (args->finding->roles[name] & (CLI_DOC_ROLES_KNOWN - 1u))
               : EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg,
                            .name = args->leaves->text + args->leaves->name_at[name]);
}

// how many of the names of `leaves` before the name at `name` count the depth of a pair. It is as the text saved it as
// the walk claimed the lines of the bundle, else counted
EMBED_INLINE unsigned int branch_depth_rank(const DocumentBranchCtx *args)
{
    const unsigned int saved =
        (args->name < args->finding->roles_count) ? (args->finding->roles[args->name] >> CLI_DOC_RANK_AT) : 0u;
    if (saved != 0u)
    {
        return saved - 1u;
    }
    unsigned int rank = 0u;
    for (unsigned int before = 0u; before < args->name; before += 1u)
    {
        rank += ((EMBED_CALL(branch_roles, DocumentBranchCtx, .finding = args->finding, .leaves = args->leaves,
                             .name = before) &
                  CLI_ROLE_PAIR) != 0u)
                    ? 1u
                    : 0u;
    }
    return rank;
}

// a region that a run closes while none is open on its line. The first, where it is a `mark`, marks the line in
// `found_bits`. It counts among those that the line closes. While no leaf fills the line before it, it also counts
// among the `unled` closes
EMBED_INLINE void branch_region_unopened(const DocumentBranchCtx *args)
{
    unsigned int *const found = args->found_bits;
    unsigned int *const closes = args->closes_pointer;
    unsigned int *const unled = args->unled_pointer;
    *found |= ((*closes == 0u) && args->mark) ? CLI_DOC_REGION_MARK : 0u;
    *unled += (((*found & CLI_DOC_REGION_FILLED) == 0u) && (*unled < CLI_DOC_REGION_COUNT)) ? 1u : 0u;
    *closes += (*closes < CLI_DOC_REGION_COUNT) ? 1u : 0u;
}

// the regions of the leaves of one line, recorded as the regions of a line are. The call takes each run in turn. A
// region that it opens stays open until a run after it on the line closes it. One that it closes, which no run before
// it on the line opened, counts among those opened above
EMBED_INLINE unsigned int branch_regions_get(const DocumentBranchCtx *args)
{
    unsigned int opens = 0u;
    unsigned int closes = 0u;
    unsigned int unled = 0u;
    unsigned int found = CLI_DOC_REGION_CLOSERS;
    for (unsigned int run = 0u; run < args->leaves->runs; run += 1u)
    {
        const unsigned int name = args->leaves->name[run];
        const unsigned int end = ((run + 1u) < args->leaves->runs) ? args->leaves->col[run + 1u] : args->leaves->end;
        const int mark = end <= args->leaves->col[run];
        const unsigned int roles =
            EMBED_CALL(branch_roles, DocumentBranchCtx, .finding = args->finding, .leaves = args->leaves, .name = name);
        if ((roles & CLI_ROLE_REGION_OPENS) != 0u)
        {
            opens += (opens < CLI_DOC_REGION_COUNT) ? 1u : 0u;
        }
        else if (((roles & CLI_ROLE_REGION_CLOSES) != 0u) && (opens > 0u))
        {
            opens -= 1u;
        }
        else if ((roles & CLI_ROLE_REGION_CLOSES) != 0u)
        {
            EMBED_CALL(branch_region_unopened, DocumentBranchCtx, .found_bits = &found, .closes_pointer = &closes,
                       .unled_pointer = &unled, .mark = mark);
        }
        // a leaf is a run its grammar names of a character or more, the empty name the first
        const int leaf = (name != 0u) && !mark;
        found &= (leaf && ((roles & CLI_ROLE_CLOSING) == 0u)) ? ~CLI_DOC_REGION_CLOSERS : ~0u;
        found |= leaf ? CLI_DOC_REGION_FILLED : 0u;
    }
    found &= (closes != 0u) ? ~0u : ~CLI_DOC_REGION_CLOSERS;
    return found | opens | (closes << CLI_DOC_REGION_CLOSES_AT) | (unled << CLI_DOC_REGION_UNLED_AT);
}

// The names that the tokens of the grammar of a branch take, written as the page reads them. That is the names, then
// each name that counts the depth of a pair, followed by `.` and each depth. The walk names a run of it in that way.
// Then the place of the first of those, and how many of the names the call writes
typedef struct
{
    VerbaLinea *out;
    int depths;
    unsigned int written;
} CliBranchNames;

// one name of a grammar, passed over as the call counts its names
static void branch_name_pass(const CliNameEachCfg *args)
{
    (void)args;
}

// the grammar of the text under `key`, copied into `text_out`. The return is 1, or 0 where the store holds no text
// there
EMBED_INLINE int names_grammar_read(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->text_out, .src = one->grammar, .bytes = sizeof(one->grammar)});
    }
    return one != NULL;
}

unsigned int cli_document_names_count(const CliDocumentNamesCfg *args)
{
    char grammar[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(names_grammar_read, DocumentBranchCtx, .key = args->key, .text_out = grammar))
    {
        if (args->serial != NULL)
        {
            *args->serial = 0u;
        }
        return 0u;
    }
    return EMBED_CALL(cli_highlight_names.count, CliHighlightNamesCfg, .key = grammar, .serial = args->serial);
}

int cli_document_name_read(const CliDocumentNamesCfg *args)
{
    char grammar[CLI_NAME_MAX_LEN];
    return EMBED_CALL(names_grammar_read, DocumentBranchCtx, .key = args->key, .text_out = grammar) &&
           EMBED_CALL(cli_highlight_names.read, CliHighlightNamesCfg, .key = grammar, .name = args->name,
                      .out = args->out);
}

unsigned int cli_document_name_place(const CliDocumentNamesCfg *args)
{
    char grammar[CLI_NAME_MAX_LEN];
    return EMBED_CALL(names_grammar_read, DocumentBranchCtx, .key = args->key, .text_out = grammar)
               ? EMBED_CALL(cli_highlight_names.place, CliHighlightNamesCfg, .key = grammar, .text = args->text)
               : 0u;
}

// the names of `scope`, saved only where the region that each belongs to is at max `depth` deep. Where `waiting` is 1,
// the call also saves only those that do not wait for their region to open. The texts of those saved move down over
// those let go
EMBED_INLINE void branch_scope_leave(const DocumentBranchCtx *args)
{
    unsigned int saved = 0u;
    unsigned int used = 0u;
    for (unsigned int at = 0u; at < args->scope->count; at += 1u)
    {
        if ((args->scope->belongs[at] > args->depth) || (args->waiting && (args->scope->waits[at] != 0u)))
        {
            continue;
        }
        memor.move_down(&(MemoriaCfg){.dst = args->scope->text + used,
                                      .src = args->scope->text + args->scope->text_at[at],
                                      .bytes = args->scope->size[at]});
        args->scope->text_at[saved] = used;
        args->scope->size[saved] = args->scope->size[at];
        args->scope->named[saved] = args->scope->named[at];
        args->scope->belongs[saved] = args->scope->belongs[at];
        args->scope->waits[saved] = args->scope->waits[at];
        used += args->scope->size[saved];
        saved += 1u;
    }
    args->scope->moved = args->scope->moved || (saved != args->scope->count);
    args->scope->count = saved;
    args->scope->used = used;
}

// `size` bytes of `text` declared in `scope`, with a use of it named `named`. It belongs to the region `belongs` deep,
// and waits for it to open where `waits` is 1. The call passes over it where the scope holds no more
EMBED_INLINE void branch_scope_append(const DocumentBranchCtx *args)
{
    if ((args->scope->count == CLI_DOC_SCOPE_NAMES) || ((args->scope->used + args->size) > CLI_DOC_SCOPE_TEXT))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->scope->text + args->scope->used, .src = args->text, .bytes = args->size});
    args->scope->text_at[args->scope->count] = args->scope->used;
    args->scope->size[args->scope->count] = args->size;
    args->scope->named[args->scope->count] = args->named;
    args->scope->belongs[args->scope->count] = args->belongs;
    // `waiting` is 0 or 1
    args->scope->waits[args->scope->count] = (unsigned char)args->waits;
    args->scope->used += args->size;
    args->scope->count += 1u;
    args->scope->moved = 1;
}

// the name a use of `size` bytes of `text` takes in `scope`, its last declaration's that does not wait: CLI_DOC_NONE
// where none declares it
EMBED_INLINE unsigned int branch_scope_by_name(const DocumentBranchCtx *args)
{
    for (unsigned int at = args->scope_read->count; at > 0u; at -= 1u)
    {
        const unsigned int one = at - 1u;
        if ((args->scope_read->waits[one] == 0u) && (args->scope_read->size[one] == args->size_u64) &&
            (memor.cmp(&(MemoriaCfg){.src = args->scope_read->text + args->scope_read->text_at[one],
                                     .other = args->text,
                                     .bytes = args->size_u64}) == 0))
        {
            return args->scope_read->named[one];
        }
    }
    return CLI_DOC_NONE;
}

// `scope` written into `out`: its depth and the depth of its pairs as `depth.pairs`, then each name as
// `;named.belongs.waits.size:` and its text
EMBED_INLINE void branch_scope_write(const DocumentBranchCtx *args)
{
    mmgr_line_decimal(args->out, args->scope_read->depth, 1u);
    mmgr_line_ch(args->out, '.');
    mmgr_line_decimal(args->out, args->scope_read->pairs, 1u);
    for (unsigned int at = 0u; at < args->scope_read->count; at += 1u)
    {
        mmgr_line_ch(args->out, ';');
        mmgr_line_decimal(args->out, args->scope_read->named[at], 1u);
        mmgr_line_ch(args->out, '.');
        mmgr_line_decimal(args->out, args->scope_read->belongs[at], 1u);
        mmgr_line_ch(args->out, '.');
        mmgr_line_decimal(args->out, args->scope_read->waits[at], 1u);
        mmgr_line_ch(args->out, '.');
        mmgr_line_decimal(args->out, args->scope_read->size[at], 1u);
        mmgr_line_ch(args->out, ':');
        mmgr_line_put_n(args->out, args->scope_read->text + args->scope_read->text_at[at], args->scope_read->size[at]);
    }
}

// the number the digits from `*from` write, `*from` moved past them and past the character `after` where it follows
// them, never past the NUL that ends them
EMBED_INLINE unsigned int branch_digits(const DocumentBranchCtx *args)
{
    const char **from = args->from_characters;
    unsigned int value = 0u;
    unsigned long long end = 0ULL;
    // a state's digits write a place among the states, which an unsigned int counts
    value = (unsigned int)EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = *from, .at = 0ULL,
                                     .size = cellul.len(&(CatenaFinitaCfg){.src = *from, .cap = CLI_POOL_BYTES}),
                                     .end = &end);
    *from += end;
    *from += (((*from)[0] == args->after) && (args->after != '\0')) ? 1 : 0;
    return value;
}

// the scope written at the place `place` among the states of `one`, read into `scope`. The call reads as far as the
// text of each name is within it, and saves its place. It is empty where `place` is CLI_DOC_NONE or past the states
EMBED_INLINE void branch_scope_read(const DocumentBranchCtx *args)
{
    unsigned int place = args->place;
    args->scope->depth = 0u;
    args->scope->pairs = 0u;
    args->scope->count = 0u;
    args->scope->used = 0u;
    args->scope->place = CLI_DOC_NONE;
    args->scope->moved = 1;
    if ((place == CLI_DOC_NONE) || (place >= args->one_read->state_count))
    {
        return;
    }
    char text[CLI_DOC_STATE];
    (void)EMBED_CALL(branch_state_text, DocumentBranchCtx, .one_read = args->one_read, .at = place, .text_out = text);
    const char *at = text;
    args->scope->depth = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_characters = &at, .after = '.');
    args->scope->pairs = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_characters = &at, .after = '\0');
    while (at[0] == ';')
    {
        at += 1;
        const unsigned int named = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_characters = &at, .after = '.');
        const unsigned int belongs = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_characters = &at, .after = '.');
        const int waits = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_characters = &at, .after = '.') != 0u;
        const unsigned int size = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_characters = &at, .after = ':');
        if (cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_POOL_BYTES}) < size)
        {
            return;
        }
        EMBED_CALL(branch_scope_append, DocumentBranchCtx, .scope = args->scope, .text = at, .size = size,
                   .named = named, .belongs = belongs, .waits = waits);
        at += size;
    }
    args->scope->place = place;
    args->scope->moved = 0;
}

// the scope that the line `finding` colored last ends in, written into `out`. The call finds its place among the states
// of the text as it goes in. It writes it again only where it moved since the one before it. The return is its bytes.
// It is CLI_DOC_NONE for a scope with no depth, pairs or names, or one too large to write, and CLI_DOC_SCOPE_SAME for
// the one before it
EMBED_INLINE unsigned int branch_scope_find(const DocumentBranchCtx *args)
{
    CliBranchScope *const scope = &args->finding_out->scope;
    if ((scope->depth == 0u) && (scope->pairs == 0u) && (scope->count == 0u))
    {
        return CLI_DOC_NONE;
    }
    if (!scope->moved && args->finding_out->positioned)
    {
        return CLI_DOC_SCOPE_SAME;
    }
    EMBED_CALL(branch_scope_write, DocumentBranchCtx, .scope_read = scope, .out = args->out);
    const unsigned long long size = mmgr_line_finish(args->out);
    scope->moved = 0;
    args->finding_out->positioned = size != 0ULL;
    // a scope written within its capacity, which an unsigned int counts
    return (size != 0ULL) ? (unsigned int)size : CLI_DOC_NONE;
}

// the scope moved by a run whose roles are `roles`. The scope enters a region that the run opens, with the names that
// wait for it, or leaves a region that the run closes
EMBED_INLINE void branch_scope_region(const DocumentBranchCtx *args)
{
    CliBranchScope *const scope = args->scope;
    if ((args->roles & CLI_ROLE_REGION_OPENS) != 0u)
    {
        scope->depth += 1u;
        scope->moved = 1;
        for (unsigned int at = 0u; at < scope->count; at += 1u)
        {
            scope->waits[at] = (scope->belongs[at] == scope->depth) ? 0u : scope->waits[at];
        }
    }
    else if (((args->roles & CLI_ROLE_REGION_CLOSES) != 0u) && (scope->depth > 0u))
    {
        scope->depth -= 1u;
        scope->moved = 1;
        EMBED_CALL(branch_scope_leave, DocumentBranchCtx, .scope = scope, .depth = scope->depth, .waiting = 0);
    }
}

// a run named `named` whose roles are `roles`, with its `size` bytes at `text`. The call puts a parameter in the scope
// of the region that it opens, and a local in the scope that it is in. Where `renames`, it reads any other name
// through the scope. The return is the name that the run takes, `named` where the scope gives it none
EMBED_INLINE unsigned int branch_scope_named(const DocumentBranchCtx *args)
{
    CliBranchScope *const scope = args->scope;
    if ((args->roles & CLI_ROLE_PARAMETER) != 0u)
    {
        EMBED_CALL(branch_scope_append, DocumentBranchCtx, .scope = scope, .text = args->text, .size = args->size,
                   .named = args->named, .belongs = scope->depth + 1u, .waits = 1);
        return args->named;
    }
    if ((args->roles & CLI_ROLE_LOCAL) != 0u)
    {
        EMBED_CALL(branch_scope_append, DocumentBranchCtx, .scope = scope, .text = args->text, .size = args->size,
                   .named = args->named, .belongs = scope->depth, .waits = 0);
        return args->named;
    }
    if (!args->renames || ((args->roles & CLI_ROLE_NAME) == 0u))
    {
        return args->named;
    }
    const unsigned int declared = EMBED_CALL(branch_scope_by_name, DocumentBranchCtx, .scope_read = scope,
                                             .text = args->text, .size_u64 = args->size);
    return (declared != CLI_DOC_NONE) ? declared : args->named;
}

// the depth of the pairs, moved by a run named `name` whose roles are `roles`. The return is the name that a pair takes
// after the `names` that the language gives, by its rank and its depth. That is where `renames`, and the language ranks
// it. Else it is CLI_DOC_NONE. A pair that closes is at the depth of the one that it closes
EMBED_INLINE unsigned int branch_pair_named(const DocumentBranchCtx *args)
{
    CliBranchScope *const scope = args->scope;
    const int closes = (args->roles & CLI_ROLE_DEPTH_CLOSES) != 0u;
    const int opens = (args->roles & CLI_ROLE_DEPTH_OPENS) != 0u;
    scope->pairs -= (closes && (scope->pairs > 0u)) ? 1u : 0u;
    const unsigned int rank = ((closes || opens) && args->renames)
                                  ? EMBED_CALL(branch_depth_rank, DocumentBranchCtx, .finding = args->finding,
                                               .leaves = args->leaves, .name = args->name)
                                  : CLI_DOC_NONE;
    const unsigned int paired =
        (rank != CLI_DOC_NONE) ? args->names + (rank * CLI_HIGHLIGHT_DEPTHS) + (scope->pairs % CLI_HIGHLIGHT_DEPTHS)
                               : CLI_DOC_NONE;
    scope->pairs += opens ? 1u : 0u;
    scope->moved = scope->moved || closes || opens;
    return paired;
}

// the leaves of one line, walked through the scope of `finding`:
//
// - a region that its runs open deepens the scope, and the parameters of its function that wait for it open in it. A
//   region that they close lets go the names that belong to it.
// - the walk declares a local in the region that it is in, and a parameter in the one that the next region opens.
// - the end of a statement or a directive lets go the parameters that still wait, at once and at the end of the line.
// - each bare name, a use of the last declaration of it in the scope, takes the name of that declaration. One that the
//   walk reaches through the operator before it, with blanks between, does not.
// - a pair that it opens deepens the pairs of the scope, and one that it closes lets one go. Each run that opens or
//   closes one takes its name by the depth that it is at, from the name at `names` on, as `cli_document_names_write`
//   writes them.
//
// The return is the leaves, with the name of each run taken in that way, in the capacity of the work for them. It is
// the leaves as they came where the cells cannot meet that capacity
EMBED_INLINE CliLeaves branch_uses_by_name(const DocumentBranchCtx *args)
{
    CliLeaves named = *args->leaves;
    CliBranchScope *const scope = &args->finding_out->scope;
    const unsigned int names = args->finding_out->names;
    const int renaming = (args->leaves->runs != 0u) && (args->leaves->runs <= args->finding_out->named_capacity);
    unsigned int *const renamed = args->finding_out->named;
    const char *const line = args->leaves->line;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES});
    unsigned long long byte = 0ULL;
    unsigned int col = 0u;
    int ended = 0;
    int reached = 0;
    for (unsigned int run = 0u; run < args->leaves->runs; run += 1u)
    {
        const unsigned int name = args->leaves->name[run];
        const unsigned int roles = EMBED_CALL(branch_roles, DocumentBranchCtx, .finding = args->finding_out,
                                              .leaves = args->leaves, .name = name);
        const int through = reached;
        // the empty name, a blank's, is between an operator and the name it reaches
        reached = ((roles & CLI_ROLE_REACHES) != 0u) || (reached && (name == 0u));
        const unsigned long long from =
            EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = line, .size = size,
                       .col = args->leaves->col[run], .byte = &byte, .at = &col);
        const unsigned int end = ((run + 1u) < args->leaves->runs) ? args->leaves->col[run + 1u] : args->leaves->end;
        const unsigned long long to = EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = line,
                                                 .size = size, .col = end, .byte = &byte, .at = &col);
        // a run's bytes lie within its line, which an unsigned int counts
        const unsigned int bytes = (unsigned int)(to - from);
        EMBED_CALL(branch_scope_region, DocumentBranchCtx, .scope = scope, .roles = roles);
        const unsigned int as =
            EMBED_CALL(branch_scope_named, DocumentBranchCtx, .scope = scope, .roles = roles, .text = line + from,
                       .size = bytes, .named = name, .renames = renaming && !through);
        if ((roles & CLI_ROLE_ENDS) != 0u)
        {
            EMBED_CALL(branch_scope_leave, DocumentBranchCtx, .scope = scope, .depth = CLI_DOC_NONE, .waiting = 1);
            ended = 1;
        }
        const unsigned int paired = EMBED_CALL(branch_pair_named, DocumentBranchCtx, .scope = scope, .roles = roles,
                                               .renames = renaming && (names != 0u), .names = names,
                                               .finding = args->finding_out, .leaves = args->leaves, .name = name);
        if (renaming)
        {
            renamed[run] = (paired != CLI_DOC_NONE) ? paired : as;
        }
    }
    if (ended)
    {
        EMBED_CALL(branch_scope_leave, DocumentBranchCtx, .scope = scope, .depth = CLI_DOC_NONE, .waiting = 1);
    }
    named.name = renaming ? renamed : args->leaves->name;
    return named;
}

// the leaves of one line, colored with no lock taken. The call names its runs through the scope, and reads its regions.
// It writes the state that the colorer leaves it in, and the scope that it ends in, as a record of its own. That goes
// after what the bundle found on the lines before it. It writes nothing where the cells refused the capacity for one
// before it, or refuse it
static void branch_find(const CliLeavesEachCfg *args)
{
    const CliLeaves *leaves = args->leaves;
    CliBranchFinding *const finding = (CliBranchFinding *)args->context;
    if (finding->refused || finding->filled)
    {
        return;
    }
    const CliLeaves named =
        EMBED_CALL(branch_uses_by_name, DocumentBranchCtx, .finding_out = finding, .leaves = leaves);
    const unsigned int regions =
        EMBED_CALL(branch_regions_get, DocumentBranchCtx, .finding = finding, .leaves = leaves);
    char scoped[CLI_DOC_STATE];
    VerbaLinea scope = {scoped, sizeof(scoped), 0ULL};
    const unsigned int scope_size =
        EMBED_CALL(branch_scope_find, DocumentBranchCtx, .finding_out = finding, .out = &scope);
    const unsigned int scope_bytes =
        ((scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME)) ? scope_size : 0u;
    // the state of the colorer, no longer than the capacity that it goes in, which an unsigned int counts
    const unsigned int ended =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = leaves->ended, .cap = CLI_POOL_BYTES});
    const unsigned long long ended_words = (ended + 3ULL) / 4ULL;
    const unsigned long long words =
        CLI_DOC_FOUND_HEAD + (2ULL * named.runs) + ended_words + ((scope_bytes + 3ULL) / 4ULL);
    // the lines past the capacity are the next bundle's
    if ((finding->used + words) > finding->found_capacity)
    {
        finding->filled = 1;
        return;
    }
    unsigned int *const record = finding->found + finding->used;
    record[0] = named.runs;
    record[1] = regions;
    record[2] = ended;
    record[3] = scope_size;
    record[4] = leaves->end;
    unsigned int *const cols = record + CLI_DOC_FOUND_HEAD;
    memor.cpy(&(MemoriaCfg){.dst = cols, .src = named.col, .bytes = named.runs * sizeof(*cols)});
    memor.cpy(&(MemoriaCfg){.dst = cols + named.runs, .src = named.name, .bytes = named.runs * sizeof(*cols)});
    char *const text = (char *)(cols + (2u * named.runs));
    memor.cpy(&(MemoriaCfg){.dst = text, .src = leaves->ended, .bytes = ended});
    memor.cpy(&(MemoriaCfg){.dst = text + (4ULL * ended_words), .src = scoped, .bytes = scope_bytes});
    finding->used += words;
}

// whether a coloring under way claims the line `line` of `one`
EMBED_INLINE int branch_claim(const DocumentBranchCtx *args)
{
    for (unsigned int at = 0u; at < CLI_DOC_CLAIMS; at += 1u)
    {
        if ((args->line >= args->one_read->claim_from[at]) && (args->line < args->one_read->claim_to[at]))
        {
            return 1;
        }
    }
    return 0;
}

// whether a coloring under way claims any line of `one`
EMBED_INLINE int branch_claims_any(const DocumentBranchCtx *args)
{
    for (unsigned int at = 0u; at < CLI_DOC_CLAIMS; at += 1u)
    {
        if (args->one_read->claim_to[at] > args->one_read->claim_from[at])
        {
            return 1;
        }
    }
    return 0;
}

// the lines from `first` to before `end` of `one`, claimed by a coloring under way. The return is the place of the
// claim among those of the text, CLI_DOC_CLAIMS where every place is in use, with none claimed
EMBED_INLINE unsigned int branch_claim_append(const DocumentBranchCtx *args)
{
    for (unsigned int at = 0u; at < CLI_DOC_CLAIMS; at += 1u)
    {
        if (args->one->claim_from[at] == args->one->claim_to[at])
        {
            args->one->claim_from[at] = args->first;
            args->one->claim_to[at] = args->end;
            return at;
        }
    }
    return CLI_DOC_CLAIMS;
}

// the claim at `at` among those of `one` let go, and the walk woken for the lines it claimed, which it left to it
EMBED_INLINE void branch_claim_drop(const DocumentBranchCtx *args)
{
    CliStoredText *one = args->one;
    if (args->at < CLI_DOC_CLAIMS)
    {
        one->claim_from[args->at] = 0u;
        one->claim_to[args->at] = 0u;
    }
    EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
}

// the state that a line ends in, of the record that the bundle found for it at `record`, with the state of its colorer
// at `text`. It is the state of the colorer, with the place of its scope after the mark. The call reads the place where
// the record writes the scope, and else takes the one that `read` gives, and saves it back into `read`. It is the
// state of the colorer alone where the scope has no place, or the two do not fit. The return is the place of the state
// among those of the text
EMBED_INLINE unsigned int branch_state_ended(const DocumentBranchCtx *args)
{
    const unsigned int *const record = args->record;
    const char *const ended = args->text;
    const unsigned int scope_size = record[3];
    if ((scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME))
    {
        char scope[CLI_DOC_STATE];
        memor.cpy(&(MemoriaCfg){.dst = scope, .src = ended + (4u * ((record[2] + 3u) / 4u)), .bytes = scope_size});
        scope[scope_size] = '\0';
        *args->read = EMBED_CALL(branch_state, DocumentBranchCtx, .one = args->one, .text = scope);
    }
    char state[CLI_DOC_STATE];
    VerbaLinea out = {state, sizeof(state), 0ULL};
    mmgr_line_put_n(&out, ended, record[2]);
    const unsigned long long colorer = out.at;
    if ((scope_size != CLI_DOC_NONE) && (*args->read != CLI_DOC_NONE))
    {
        mmgr_line_ch(&out, CLI_DOC_SCOPE_MARK);
        mmgr_line_decimal(&out, *args->read, 1u);
    }
    if (mmgr_line_finish(&out) == 0ULL)
    {
        state[colorer] = '\0';
    }
    return EMBED_CALL(branch_state, DocumentBranchCtx, .one = args->one, .text = state);
}

// what `finding` found on the lines from `first`, put into `one` in turn. The walk enters the first line in the state
// `*entering`, and in the scope at the place `place` among the states of the text. For each line, the call puts:
//
// - the state that the walk entered it in, and the state that it ends in
// - its regions, and its runs saved: those of a paged text where the page holds the line or the range passes it on
// - the runs passed on, where the range passes it on, each no further than the columns of the text where the range
//   names them
//
// Where `stopping` is 1, the call stops at the first line that the walk entered in the state that it entered it in
// before, and sets `*stopped`. The return is the line after the last put. This runs under the lock
EMBED_INLINE unsigned int branch_found_append(const DocumentBranchCtx *args)
{
    unsigned int *entering = args->entering;
    int *stopped = args->stopped;
    const CliStoredLines *const lines = &args->one->lines;
    const unsigned int hand_from = args->range->requested ? args->range->hand_from : args->one->cached_from;
    const unsigned int hand_to = args->range->requested ? args->range->hand_to : args->one->cached_to;
    args->range->columns = (args->range->columns != 0ULL) ? args->one->columns : 0ULL;
    const unsigned int *record = args->finding->found;
    const unsigned int *const last = record + args->finding->used;
    // the words that the records of the bundle take at max, thus the call takes the cell of the runs once for the
    // bundle
    unsigned long long more = 0ULL;
    for (const unsigned int *counted = record; counted < last;)
    {
        const unsigned int runs = counted[0];
        const unsigned int scope_size = counted[3];
        const int written = (scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME);
        more += CLI_DOC_RECORD_HEAD + (2ULL * runs);
        counted +=
            CLI_DOC_FOUND_HEAD + (2u * runs) + ((counted[2] + 3u) / 4u) + (written ? ((scope_size + 3u) / 4u) : 0u);
    }
    if (((args->one->run_used + more) > args->one->run_capacity) &&
        !EMBED_CALL(branch_runs_capacity, DocumentBranchCtx, .one = args->one, .more = more))
    {
        args->one->refused = 1;
    }
    unsigned int line = args->first;
    unsigned int scoped = args->place;
    while ((record < last) && (line < lines->lines) && !args->one->refused)
    {
        unsigned int begun_word = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_BEGUN * lines->lines) + line) * sizeof(begun_word),
            .dst = &begun_word,
            .bytes = sizeof(begun_word)});
        if (args->stopping && (begun_word == *entering))
        {
            *stopped = 1;
            break;
        }
        const unsigned int runs = record[0];
        const unsigned int ended_words = (record[2] + 3u) / 4u;
        const unsigned int *const found_col = record + CLI_DOC_FOUND_HEAD;
        const char *const ended = (const char *)(found_col + (2u * runs));
        const unsigned int scope_size = record[3];
        const int written = (scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME);
        {
            const unsigned int stored_value = *entering;
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = lines->words,
                .offset = (((unsigned long long)CLI_DOC_WORD_BEGUN * lines->lines) + line) * sizeof(stored_value),
                .src = &stored_value,
                .bytes = sizeof(stored_value)});
        }
        const unsigned int ends = EMBED_CALL(branch_state_ended, DocumentBranchCtx, .one = args->one, .record = record,
                                             .text = ended, .read = &scoped);
        {
            const unsigned int stored_value = ends;
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = lines->words,
                .offset = (((unsigned long long)CLI_DOC_WORD_ENDED * lines->lines) + line) * sizeof(stored_value),
                .src = &stored_value,
                .bytes = sizeof(stored_value)});
        }
        unsigned int regions_word = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_REGIONS * lines->lines) + line) * sizeof(regions_word),
            .dst = &regions_word,
            .bytes = sizeof(regions_word)});
        args->one->regions_moved = args->one->regions_moved || (regions_word != record[1]);
        {
            const unsigned int stored_value = record[1];
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = lines->words,
                .offset = (((unsigned long long)CLI_DOC_WORD_REGIONS * lines->lines) + line) * sizeof(stored_value),
                .src = &stored_value,
                .bytes = sizeof(stored_value)});
        }
        const CliLeaves saved = {NULL, runs, found_col, found_col + runs, NULL, NULL, NULL, record[4]};
        const int transferred = (line >= hand_from) && (line < hand_to);
        const int cached = (line >= args->one->cached_from) && (line < args->one->cached_to);
        if (!args->one->paged || cached || transferred)
        {
            EMBED_CALL(branch_runs_save, DocumentBranchCtx, .one = args->one, .line = line, .leaves = &saved);
        }
        else
        {
            {
                const unsigned int stored_value = CLI_DOC_NO_RUNS;
                (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = lines->words,
                    .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + line) * sizeof(stored_value),
                    .src = &stored_value,
                    .bytes = sizeof(stored_value)});
            }
        }
        if (transferred)
        {
            EMBED_CALL(branch_runs_append, DocumentBranchCtx, .range = args->range, .line = line, .leaves = &saved);
        }
        *entering = ends;
        line += 1u;
        record += CLI_DOC_FOUND_HEAD + (2u * runs) + ended_words + (written ? ((scope_size + 3u) / 4u) : 0u);
    }
    return line;
}

// the state that the walk enters the line `first` in, that of the text at `entering`, copied into `text_out`,
// CLI_DOC_STATE bytes, cut at its mark. The call reads the scope at the place after the mark into that of the finding.
// The return is whether it copied the state whole
EMBED_INLINE int branch_state_entered(const DocumentBranchCtx *args)
{
    char *const state = args->text_out;
    CliBranchFinding *const finding = args->finding_out;
    const int stated = EMBED_CALL(branch_state_text, DocumentBranchCtx, .one_read = args->one_read,
                                  .at = *args->entering, .text_out = state) != 0ULL;
    const char *const mark =
        stated ? cellul.chr(&(CatenaFinitaCfg){.src = state, .cap = CLI_POOL_BYTES, .byte = CLI_DOC_SCOPE_MARK}) : NULL;
    const char *place = (mark != NULL) ? (mark + 1) : NULL;
    EMBED_CALL(branch_scope_read, DocumentBranchCtx, .one_read = args->one_read,
               .place = (place != NULL)
                            ? EMBED_CALL(branch_digits, DocumentBranchCtx, .from_characters = &place, .after = '\0')
                            : CLI_DOC_NONE,
               .scope = &finding->scope);
    finding->positioned = finding->scope.place != CLI_DOC_NONE;
    if (mark != NULL)
    {
        // the mark is within the state copied
        state[mark - state] = '\0';
    }
    return stated;
}

// the lines of `line_characters` from `first` to before `to` that the bytes of the range reach, counted, one at the
// least. The call takes the bytes spent from those of the range, where it counts them. It puts their bytes, each with
// its newline, into `units_pointer`, and those of the longest into `max_pointer`
EMBED_INLINE unsigned int branch_lines_spent(const DocumentBranchCtx *args)
{
    const char **const line = args->line_characters;
    CliBranchRange *const range = args->range;
    unsigned int spend = 0u;
    unsigned long long spent = 0ULL;
    while ((line != NULL) && ((args->first + spend) < args->to) &&
           ((spend == 0u) || (range->bytes == CLI_DOC_ALL_BYTES) || (spent < range->bytes)))
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = line[spend], .cap = CLI_POOL_BYTES});
        spent += (range->bytes != CLI_DOC_ALL_BYTES) ? (length + 1ULL) : 0ULL;
        *args->units_pointer += length + 1ULL;
        *args->max_pointer = (length > *args->max_pointer) ? length : *args->max_pointer;
        spend += 1u;
    }
    if (range->bytes != CLI_DOC_ALL_BYTES)
    {
        range->bytes -= (spent < range->bytes) ? spent : range->bytes;
    }
    return spend;
}

// one bundle of the lines from `first` to before `to` of `one`, colored from the state `*entering` that the walk enters
// the first in. That is no more than CLI_DOC_BATCH lines where `stopping` is 1, else CLI_DOC_RUN, or CLI_DOC_STEP where
// the bytes of the range reach no further. Of those, it is as many as its bytes reach, one at the least, with those
// spent taken from them.
//
// The call claims the lines under the lock, reads and colors them in `work` with the lock given back, and puts them in
// under it again. Its caller saves the text across it. Where the text moved meanwhile, the call puts none in, and marks
// the range stale. Where the grammar or the cells refused them, it puts none in, and marks the text refused. The return
// is the line after the last put. This runs under the lock
EMBED_INLINE unsigned int branch_batch_color(const DocumentBranchCtx *args)
{
    unsigned int to = args->to;
    unsigned int max = CLI_DOC_BATCH;
    if (!args->stopping)
    {
        max = (args->range->bytes != CLI_DOC_ALL_BYTES) ? CLI_DOC_STEP : CLI_DOC_RUN;
    }
    const unsigned int end = ((to - args->first) > max) ? (args->first + max) : to;
    // what the bundle reads with the lock given back, copied as the call claims its lines. It is the serial of the
    // text, its key, and the key of its grammar. Then whether it is a paged text, whether its last line ends it, and
    // how many lines it counts
    const unsigned long long serial = args->one->serial;
    char key[CLI_NAME_MAX_LEN];
    char grammar[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = key, .src = args->one->key, .bytes = sizeof(key)});
    memor.cpy(&(MemoriaCfg){.dst = grammar, .src = args->one->grammar, .bytes = sizeof(grammar)});
    const int paged = args->one->paged;
    const int whole = args->one->whole;
    const unsigned int count = args->one->lines.lines;
    CliBranchFinding finding;
    finding.named = NULL;
    finding.named_capacity = 0ULL;
    finding.found = NULL;
    finding.found_capacity = 0ULL;
    finding.filled = 0;
    finding.names = args->one->names;
    finding.used = 0ULL;
    finding.refused = 0;
    // the state that the walk enters the first line in, copied out. The call passes the own state of the colorer to
    // it, and reads the scope at the place after the mark into the bundle
    char state[CLI_DOC_STATE];
    const int stated = EMBED_CALL(branch_state_entered, DocumentBranchCtx, .one_read = args->one,
                                  .entering = args->entering, .text_out = state, .finding_out = &finding);
    // the roles of the grammar's names copied, read first where they were not
    branch_roles_read(args);
    const unsigned int roles = args->one->roles_complete ? args->one->roles_capacity : 0u;
    // where each line read starts, the roles and the UTF-8 of a text read whole, in one cell the bundle gives back
    unsigned int read = args->first;
    const unsigned long long text_capacity =
        paged ? 0ULL
              : EMBED_CALL(branch_lines_measure, DocumentBranchCtx, .lines = &args->one->lines, .first = args->first,
                           .end = end, .bytes = args->range->bytes, .read = &read);
    const unsigned long long starts_bytes = (unsigned long long)(end - args->first) * sizeof(const char *);
    const unsigned long long roles_bytes = (unsigned long long)roles * sizeof(unsigned int);
    char *const cell = g_platform_guard->temporary_buf_alloc(starts_bytes + roles_bytes + text_capacity, g_cli_editor_owner);
    const char **line = (const char **)(void *)cell;
    finding.roles = NULL;
    finding.roles_count = 0u;
    if (cell != NULL)
    {
        finding.roles = (const unsigned int *)(void *)(cell + starts_bytes);
        finding.roles_count = roles;
        if (roles_bytes != 0ULL)
        {
            (void)g_platform_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = args->one->roles, .dst = cell + starts_bytes, .bytes = roles_bytes});
        }
        if (!paged)
        {
            EMBED_CALL(branch_lines_write, DocumentBranchCtx, .lines = &args->one->lines, .from = args->first,
                       .to = read, .text_out = cell + starts_bytes + roles_bytes, .capacity = text_capacity,
                       .line_characters = line);
        }
    }
    const unsigned int claim =
        EMBED_CALL(branch_claim_append, DocumentBranchCtx, .one = args->one, .first = args->first, .end = end);
    char *reserved = NULL;
    if (paged && (cell != NULL))
    {
        unsigned long long inner_count = 0ULL;
        reserved = EMBED_CALL(cli_pages_from.lines_alloc, CliPagesFromCfg, .key = key, .from = args->first, .to = end,
                              .line = line, .count = &inner_count, .owner = g_cli_editor_owner);
        // the call reads no more lines than the caller asked, which an unsigned int counts
        read = args->first + (unsigned int)inner_count;
    }
    line = (paged && (reserved == NULL)) ? NULL : line;
    // the lines read colored as far as the range's bytes reach, one at the least
    unsigned long long total = 0ULL;
    unsigned long long longest = 0ULL;
    const unsigned int spend =
        EMBED_CALL(branch_lines_spent, DocumentBranchCtx, .line_characters = line, .first = args->first, .to = read,
                   .range = args->range, .units_pointer = &total, .max_pointer = &longest);
    // the names of the runs of a line, one run for each of its bytes at max, and the records. That is two words for
    // each run, the head of a record, and CLI_DOC_FOUND_STATE words of its state for each line. Then the words of one
    // state and scope whole. The lines past them go to the next bundle
    finding.named_capacity = longest + 2ULL;
    finding.found_capacity = (2ULL * total) + ((unsigned long long)spend * (CLI_DOC_FOUND_HEAD + CLI_DOC_FOUND_STATE)) +
                             (CLI_DOC_STATE / 2ULL);
    const unsigned long long named_bytes = finding.named_capacity * sizeof(unsigned int);
    char *const found_cell =
        (spend != 0u) ? g_platform_guard->temporary_buf_alloc(named_bytes + (finding.found_capacity * sizeof(unsigned int)),
                                                         g_cli_editor_owner)
                      : NULL;
    finding.named = (unsigned int *)(void *)found_cell;
    finding.found = (found_cell != NULL) ? (unsigned int *)(void *)(found_cell + named_bytes) : NULL;
    int colored = 0;
    if ((line != NULL) && (found_cell != NULL))
    {
        char ended[CLI_DOC_STATE];
        VerbaLinea last = {ended, sizeof(ended), 0ULL};
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        // the call colors the lines with the lock given back, thus the bundle reads only what it copied as it claimed
        // them
        cli_document_lock_give();
        colored =
            EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = grammar, .state = stated ? state : "root",
                       .lines = line, .count = spend, .ends = ((args->first + spend) == count) && whole,
                       .each = branch_find, .context = &finding, .ended = &last, .err_msg = &err_msg);
        cli_document_lock_take();
    }
    g_platform_guard->persistent_buf_release(reserved, g_cli_editor_owner);
    EMBED_CALL(branch_claim_drop, DocumentBranchCtx, .one = args->one, .at = claim);
    unsigned int put = args->first;
    if (args->one->let_go || (args->one->serial != serial))
    {
        args->range->stale = 1;
    }
    else if (!colored)
    {
        args->one->refused = 1;
    }
    else
    {
        put = EMBED_CALL(branch_found_append, DocumentBranchCtx, .one = args->one, .finding = &finding,
                         .first = args->first, .place = finding.scope.place, .stopping = args->stopping,
                         .entering = args->entering, .stopped = args->stopped, .range = args->range);
        args->one->refused = args->one->refused || finding.refused;
    }
    g_platform_guard->persistent_buf_release(found_cell, g_cli_editor_owner);
    g_platform_guard->persistent_buf_release(cell, g_cli_editor_owner);
    return put;
}

// the lines from `from` to `to` of `one`, colored. The walk enters the first in the state that the line above it ends
// in. It enters it from root where it is the first line, or where the walk did not color the line above. Where
// `stopping` is 1, the walk stops at the first line that it entered in the state that it entered it in before.
//
// It colors each line whole from left to right. The lines are those that the bytes of the range reach, the last maybe
// past them, with those spent taken from them. It colors one bundle at a time, in a work claimed for them.
//
// The call passes on in it the runs of those that the range passes on. Those of each line go no further than the
// columns of the range, where it names them. The return is the line after the last colored. This runs under the lock,
// which it gives back as it colors each bundle. Its caller saves the text across it
EMBED_INLINE unsigned int branch_color(const DocumentBranchCtx *args)
{
    unsigned int word_2 = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = (&args->one->lines)->words,
        .offset = (((unsigned long long)CLI_DOC_WORD_ENDED * (&args->one->lines)->lines) + (args->from - 1u)) *
                  sizeof(word_2),
        .dst = &word_2,
        .bytes = sizeof(word_2)});

    const unsigned int above = (args->from != 0u) ? word_2 : CLI_DOC_UNCOLORED;
    unsigned int entering = (above != CLI_DOC_UNCOLORED) ? above : CLI_DOC_ROOT;
    unsigned int line = args->from;
    int stopped = 0;
    while (!stopped && !args->one->refused && !args->range->stale && (line < args->to) && (args->range->bytes != 0ULL))
    {
        const unsigned int next =
            EMBED_CALL(branch_batch_color, DocumentBranchCtx, .one = args->one, .first = line, .to = args->to,
                       .stopping = args->stopping, .entering = &entering, .stopped = &stopped, .range = args->range);
        // a bundle that puts no line in ends the coloring
        stopped = stopped || (next == line);
        line = next;
    }
    return line;
}

// whether the line `line` of `one` is one to color. The walk did not color it, and where `yielding` is 1, as the walk
// yields, no coloring under way claims it
EMBED_INLINE int branch_uncolored(const DocumentBranchCtx *args)
{
    unsigned int begun = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->one_read->lines.words,
        .offset = (((unsigned long long)CLI_DOC_WORD_BEGUN * args->one_read->lines.lines) + args->line) * sizeof(begun),
        .dst = &begun,
        .bytes = sizeof(begun)});
    return (begun == CLI_DOC_UNCOLORED) && !(args->yielding && branch_claim(args));
}

// the run of lines of `one` to color about `at`, below the origin of the walk where `down` is 1. The call puts its
// first line, and the line after its last, into `first` and `end`. That is no more than CLI_DOC_STEP of them that reach
// from `at`, down or up. It is none outside the lines from `low` to before `high` that the walk colors. Where
// `yielding` is 1, it is none that a coloring under way claims. A run down whose line above ends in no known state
// reaches as far above as CLI_DOC_PREFIX lines to color reach
EMBED_INLINE void branch_run_get(const DocumentBranchCtx *args)
{
    unsigned int *first = args->first_pointer;
    unsigned int *end = args->end_pointer;
    unsigned int start = args->at;
    unsigned int stop = args->at + 1u;
    while (args->down && (stop < args->high) &&
           EMBED_CALL(branch_uncolored, DocumentBranchCtx, .one_read = args->one_read, .line = stop,
                      .yielding = args->yielding) &&
           ((stop - args->at) < CLI_DOC_STEP))
    {
        stop += 1u;
    }
    while (!args->down && (start > args->low) &&
           EMBED_CALL(branch_uncolored, DocumentBranchCtx, .one_read = args->one_read, .line = start - 1u,
                      .yielding = args->yielding) &&
           ((args->at + 1u - start) < CLI_DOC_STEP))
    {
        start -= 1u;
    }
    for (unsigned int lead = 0u; args->down && (start > args->low) &&
                                 EMBED_CALL(branch_uncolored, DocumentBranchCtx, .one_read = args->one_read,
                                            .line = start - 1u, .yielding = args->yielding) &&
                                 (lead < CLI_DOC_PREFIX);
         lead += 1u)
    {
        start -= 1u;
    }
    *first = start;
    *end = stop;
}

// the lines of `one` that the walk colors, from `*low` to before `*high`. For a paged text, those are the lines that
// the page holds alone, with the first of them entered from root. For any other text, they are every line
EMBED_INLINE void branch_bounds(const DocumentBranchCtx *args)
{
    unsigned int *low = args->low_pointer;
    unsigned int *high = args->high_pointer;
    const unsigned int lines = args->one_read->lines.lines;
    *high = (args->one_read->paged && (args->one_read->cached_to < lines)) ? args->one_read->cached_to : lines;
    if (args->one_read->paged)
    {
        if (args->one_read->cached_from < *high)
        {
            *low = args->one_read->cached_from;
        }
        else
        {
            *low = *high;
        }
    }
    else
    {
        *low = 0u;
    }
}

// a run of lines passed on in `out`, begun. The call writes their runs as `{"from", "runs"}`, after a comma where
// `*any` is not zero, and sets `*any` where they are. Each line goes no further than `columns` characters where that is
// not 0. There are no more lines than `bytes` reach, those that the page holds go on, and the call colors none past
// `high`
EMBED_INLINE CliBranchRange branch_range_begin(const DocumentBranchCtx *args)
{
    const CliBranchRange range = {
        0,  0u, args->any_pointer, args->out, args->columns, args->one_read->names, args->bytes, 0u, 0, 0,
        0u, 0u, args->high};
    return range;
}

// the lines from `first` to `end` of `one`, colored in `range`. The call colors the lines after them again until the
// walk enters one in the state that it entered it in before. That is no more than CLI_DOC_STEP of those, and none past
// the last of the range. Where its columns are not 0, the call leaves the lines after them to the walk, unless `onward`
// is 1. The own step of the walk colors them.
//
// This runs under the lock, which each bundle gives back as the call colors it. The call marks the range stale where
// the text moved
EMBED_INLINE void branch_range_color(const DocumentBranchCtx *args)
{
    const unsigned int line = EMBED_CALL(branch_color, DocumentBranchCtx, .one = args->one, .from = args->first,
                                         .to = args->end, .stopping = 0, .range = args->range);
    const unsigned int high = args->range->high;
    // the word of the line at the end, where the text has one
    unsigned int end_begun = CLI_DOC_UNCOLORED;
    if (args->end < args->one->lines.lines)
    {
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = args->one->lines.words,
            .offset =
                (((unsigned long long)CLI_DOC_WORD_BEGUN * args->one->lines.lines) + args->end) * sizeof(end_begun),
            .dst = &end_begun,
            .bytes = sizeof(end_begun)});
    }
    if ((args->onward || (args->range->columns == 0ULL)) && !args->range->stale && (line == args->end) &&
        (args->end < high) && (args->end < args->one->lines.lines) && (end_begun != CLI_DOC_UNCOLORED))
    {
        const unsigned int max = ((high - args->end) > CLI_DOC_STEP) ? (args->end + CLI_DOC_STEP) : high;
        (void)EMBED_CALL(branch_color, DocumentBranchCtx, .one = args->one, .from = args->end, .to = max, .stopping = 1,
                         .range = args->range);
    }
}

// the first line from `low` to before `high`, past `low` where it is not the first of the text, that no work claims.
// The walk entered it in another state than the one that the line above it ends in. The return is CLI_DOC_NONE where
// none is
EMBED_INLINE unsigned int branch_entered_astray(const DocumentBranchCtx *args)
{
    const CliStoredLines *const lines = &args->one_read->lines;
    for (unsigned int line = (args->low != 0u) ? (args->low + 1u) : 0u; line < args->high; line += 1u)
    {
        unsigned int begun = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_BEGUN * lines->lines) + line) * sizeof(begun),
            .dst = &begun,
            .bytes = sizeof(begun)});
        unsigned int word_3 = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_ENDED * lines->lines) + (line - 1u)) * sizeof(word_3),
            .dst = &word_3,
            .bytes = sizeof(word_3)});

        const unsigned int above = (line == 0u) ? CLI_DOC_ROOT : word_3;
        if ((begun != above) && !EMBED_CALL(branch_claim, DocumentBranchCtx, .one_read = args->one_read, .line = line))
        {
            return line;
        }
    }
    return CLI_DOC_NONE;
}

// whether the walk writes a telemetry line a step to the error stream, which the window turns on
static int s_telemetry;

// the telemetry line of the walk: `walk <key> colored <first> <end> origin <origin>`. It is the lines of the text under
// the key that the step colored, from `low` to before `high`. Then the line that the walk spread out from
EMBED_INLINE void branch_telemetry(const DocumentBranchCtx *args)
{
    char text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, "walk ");
    mmgr_line_put(&line, args->one_read->key);
    mmgr_line_put(&line, " colored ");
    mmgr_line_decimal(&line, args->low, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, args->high, 1u);
    mmgr_line_put(&line, " origin ");
    mmgr_line_decimal(&line, args->line, 1u);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
               .length = mmgr_line_finish(&line));
}

void cli_document_telemetry_on(void)
{
    s_telemetry = 1;
}

void cli_document_telemetry_off(void)
{
    s_telemetry = 0;
}

// one step of the walk of `one`. The call colors the run of lines to color nearest its origin. Where the walk colored
// or claimed every line, it colors again the first line whose start state differs from the end state above. It writes
// that into `out` as `{"key", "seq", "colors"}`, and sets `*passed` where it holds the runs of a line that the page
// holds. The return is 1 where it colored a line, else 0.
//
// The call counts the lines of a paged text again first, and walks it across the lines that the page holds alone. This
// runs under the lock, which it gives back as it colors. Its caller saves the text across it
EMBED_INLINE int branch_step(const DocumentBranchCtx *args)
{
    CliStoredText *one = args->one;
    one->refused =
        one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
    unsigned int low = 0u;
    unsigned int high = 0u;
    EMBED_CALL(branch_bounds, DocumentBranchCtx, .one_read = one, .low_pointer = &low, .high_pointer = &high);
    if ((one->grammar[0] == '\0') || one->refused || (high <= low) || (one->cached_to <= one->cached_from))
    {
        return 0;
    }
    // the line the walk spreads out from, cut to the lines it walks
    unsigned int origin = (one->origin < low) ? low : one->origin;
    origin = (origin < high) ? origin : (high - 1u);
    unsigned int at = CLI_DOC_NONE;
    int down = 1;
    unsigned int distance = 0u;
    while ((at == CLI_DOC_NONE) && (((origin + distance) < high) || (distance < (origin - low))))
    {
        if (((origin + distance) < high) &&
            EMBED_CALL(branch_uncolored, DocumentBranchCtx, .one_read = one, .line = origin + distance, .yielding = 1))
        {
            at = origin + distance;
        }
        else if ((distance < (origin - low)) && EMBED_CALL(branch_uncolored, DocumentBranchCtx, .one_read = one,
                                                           .line = origin - distance - 1u, .yielding = 1))
        {
            at = origin - distance - 1u;
            down = 0;
        }
        distance += 1u;
    }
    unsigned int first = CLI_DOC_NONE;
    unsigned int end = 0u;
    if (at != CLI_DOC_NONE)
    {
        EMBED_CALL(branch_run_get, DocumentBranchCtx, .one_read = one, .at = at, .down = down, .low = low, .high = high,
                   .yielding = 1, .first_pointer = &first, .end_pointer = &end);
    }
    if (at == CLI_DOC_NONE)
    {
        first = EMBED_CALL(branch_entered_astray, DocumentBranchCtx, .one_read = one, .low = low, .high = high);
        end = first + 1u;
    }
    if (first == CLI_DOC_NONE)
    {
        return 0;
    }
    CliBranchRange range =
        EMBED_CALL(branch_range_begin, DocumentBranchCtx, .one_read = one, .any_pointer = args->transferred,
                   .out = NULL, .columns = one->columns, .bytes = CLI_DOC_STEP_BYTES, .high = high);
    EMBED_CALL(branch_range_color, DocumentBranchCtx, .one = one, .first = first, .end = end, .onward = 1,
               .range = &range);
    if (s_telemetry)
    {
        EMBED_CALL(branch_telemetry, DocumentBranchCtx, .one_read = one, .low = first, .high = end, .line = origin);
    }
    return 1;
}

// the mark of a fold that shows on its first line the line it closes on, after what it hides
#define CLI_DOC_FOLD_DISPLAYED 0x80000000u

// the fold of the region opened on the line `opened` that the line `line` closes, the `close`th region that it closes.
// `regions` is the regions of the line, and `filled` the last line above it that holds a leaf. The return is the last
// line that it hides, plus 1, marked CLI_DOC_FOLD_DISPLAYED where it shows the line that it closes on. It is 0 where it
// hides no line.
//
// A mark of no characters closes a region on the line that it ends, with a leaf before it. Else it closes it on the
// last line above that holds one. A bracket closes it on the line before its own, where a leaf is before it, or another
// leaf on its line closes nothing. It closes it on its own line, shown, where its line holds closes alone
EMBED_INLINE unsigned int branch_fold_get(const DocumentBranchCtx *args)
{
    const int mark = (args->regions & CLI_DOC_REGION_MARK) != 0u;
    const int led = args->close >= ((args->regions >> CLI_DOC_REGION_UNLED_AT) & CLI_DOC_REGION_COUNT);
    const int displayed = !mark && !led && ((args->regions & CLI_DOC_REGION_CLOSERS) != 0u);
    unsigned int last;
    if (mark && led)
    {
        last = args->line;
    }
    else if (mark)
    {
        last = args->filled;
    }
    else if (displayed)
    {
        last = args->line;
    }
    else
    {
        last = args->line - 1u;
    }
    // a line displayed on the fold's first line is no line hidden
    const unsigned int hidden = displayed ? (last - 1u) : last;
    if ((last == CLI_DOC_NONE) || (hidden <= args->opened))
    {
        return 0u;
    }
    return (last + 1u) | (displayed ? CLI_DOC_FOLD_DISPLAYED : 0u);
}

// the folds of `one` found, each passed to `each` with its first line and the last line that it hides. Then whether it
// shows the line that it closes on after what it hides. Each fold is the widest of the regions that open on its first
// line, in the order of their first lines. For a paged text, they are those of the lines that the page mirrors alone.
// There are none where the cells cannot meet the capacity that the call finds them in. This runs under the lock of the
// texts
static void branch_folds_each(const DocumentBranchCtx *args)
{
    const CliStoredLines *const lines = &args->one_read->lines;
    unsigned int first = 0u;
    unsigned int end = 0u;
    EMBED_CALL(branch_bounds, DocumentBranchCtx, .one_read = args->one_read, .low_pointer = &first,
               .high_pointer = &end);
    const unsigned int saved = end - first;
    // the widest fold of each line and the regions open above a line, no more than the regions they open
    unsigned long long opening = 0ULL;
    for (unsigned int line = first; line < end; line += 1u)
    {
        unsigned int word_4 = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_REGIONS * lines->lines) + line) * sizeof(word_4),
            .dst = &word_4,
            .bytes = sizeof(word_4)});

        opening += word_4 & CLI_DOC_REGION_COUNT;
    }
    const unsigned long long widest_bytes = (unsigned long long)saved * sizeof(unsigned int);
    unsigned int *const widest =
        (saved != 0u)
            ? g_platform_guard->temporary_buf_alloc(widest_bytes + (opening * sizeof(unsigned int)), g_cli_editor_owner)
            : NULL;
    if (widest == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = widest, .val = 0, .bytes = widest_bytes});
    unsigned int *const opened_on = widest + saved;
    unsigned int open = 0u;
    unsigned int filled = CLI_DOC_NONE;
    for (unsigned int line = first; line < end; line += 1u)
    {
        unsigned int regions = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_REGIONS * lines->lines) + line) * sizeof(regions),
            .dst = &regions,
            .bytes = sizeof(regions)});
        const unsigned int closes = (regions >> CLI_DOC_REGION_CLOSES_AT) & CLI_DOC_REGION_COUNT;
        unsigned int close = 0u;
        while ((close < closes) && (open != 0u))
        {
            open -= 1u;
            const unsigned int opened = opened_on[open];
            const unsigned int fold = EMBED_CALL(branch_fold_get, DocumentBranchCtx, .opened = opened, .line = line,
                                                 .close = close, .regions = regions, .filled = filled);
            if ((fold & ~CLI_DOC_FOLD_DISPLAYED) > (widest[opened - first] & ~CLI_DOC_FOLD_DISPLAYED))
            {
                widest[opened - first] = fold;
            }
            close += 1u;
        }
        for (unsigned int opens = regions & CLI_DOC_REGION_COUNT; opens != 0u; opens -= 1u)
        {
            opened_on[open] = line;
            open += 1u;
        }
        filled = ((regions & CLI_DOC_REGION_FILLED) != 0u) ? line : filled;
    }
    for (unsigned int at = 0u; at < saved; at += 1u)
    {
        if (widest[at] != 0u)
        {
            EMBED_CALL(args->each, CliFoldEachCfg, .context = args->context, .first = first + at,
                       .last = (widest[at] & ~CLI_DOC_FOLD_DISPLAYED) - 1u,
                       .displayed = (widest[at] & CLI_DOC_FOLD_DISPLAYED) != 0u);
        }
    }
    g_platform_guard->persistent_buf_release(widest, g_cli_editor_owner);
}

static int folds_each_locked(const CliDocumentContextCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one != NULL)
    {
        EMBED_CALL(branch_folds_each, DocumentBranchCtx, .one_read = one, .each = args->each_fold_each,
                   .context = args->context);
    }
    return one != NULL;
}

int cli_document_folds_each(const CliDocumentContextCfg *args)
{
    cli_document_lock_take();
    const int answer = folds_each_locked(args);
    cli_document_lock_give();
    return answer;
}

static unsigned int openers_locked(const CliDocumentKeyLineCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    unsigned int end;
    if (one != NULL)
    {
        if (args->line < one->lines.lines)
        {
            end = args->line;
        }
        else
        {
            end = one->lines.lines;
        }
    }
    else
    {
        end = 0u;
    }
    // the regions open above the line, no more than those the lines above it open
    unsigned long long opening = 0ULL;
    for (unsigned int at = 0u; at < end; at += 1u)
    {
        unsigned int word_5 = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = (&one->lines)->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_REGIONS * (&one->lines)->lines) + at) * sizeof(word_5),
            .dst = &word_5,
            .bytes = sizeof(word_5)});

        opening += word_5 & CLI_DOC_REGION_COUNT;
    }
    unsigned int *const opened_on =
        (opening != 0ULL) ? g_platform_guard->temporary_buf_alloc(opening * sizeof(unsigned int), g_cli_editor_owner) : NULL;
    unsigned int open = 0u;
    for (unsigned int at = 0u; (opened_on != NULL) && (at < end); at += 1u)
    {
        unsigned int regions = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = (&one->lines)->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_REGIONS * (&one->lines)->lines) + at) * sizeof(regions),
            .dst = &regions,
            .bytes = sizeof(regions)});
        const unsigned int closes = (regions >> CLI_DOC_REGION_CLOSES_AT) & CLI_DOC_REGION_COUNT;
        open -= (closes < open) ? closes : open;
        for (unsigned int opens = regions & CLI_DOC_REGION_COUNT; opens != 0u; opens -= 1u)
        {
            opened_on[open] = at;
            open += 1u;
        }
    }
    const unsigned int count = (open < args->max_u32) ? open : args->max_u32;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        args->opener[at] = opened_on[open - count + at];
    }
    g_platform_guard->persistent_buf_release(opened_on, g_cli_editor_owner);
    return count;
}

unsigned int cli_document_openers(const CliDocumentKeyLineCfg *args)
{
    cli_document_lock_take();
    const unsigned int answer = openers_locked(args);
    cli_document_lock_give();
    return answer;
}

// the roles of the run that holds the unit `col` of the line `line` of the text under `key`. They are as the text saved
// them for the name of the run. The return is 0 where no text is under the key, the line saves no runs, or `col` is
// before its first run. The same is true where the call did not read the roles of the names yet
static unsigned int run_roles_locked(const CliDocumentKeyLineCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if ((one == NULL) || (args->line >= one->lines.lines) || !one->roles_complete)
    {
        return 0u;
    }
    unsigned int place = 0u;
    unsigned long long length = 0ULL;
    unsigned int *const record = EMBED_CALL(branch_line_record, DocumentBranchCtx, .one_read = one,
                                            .line_number = args->line, .place_out = &place, .length_out = &length);
    if (record == NULL)
    {
        return 0u;
    }
    const unsigned int runs = record[0];
    const unsigned int *const col = record + CLI_DOC_RECORD_HEAD;
    unsigned int name = CLI_DOC_NONE;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        name = (col[run] <= args->col) ? col[run + runs] : name;
    }
    g_platform_guard->persistent_buf_release(record, g_cli_editor_owner);
    unsigned int roles = 0u;
    if ((name != CLI_DOC_NONE) && (name < one->roles_capacity))
    {
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one->roles,
                                                                .offset = (unsigned long long)name * sizeof(roles),
                                                                .dst = &roles,
                                                                .bytes = sizeof(roles)});
    }
    return roles & (CLI_DOC_ROLES_KNOWN - 1u);
}

unsigned int cli_document_run_roles(const CliDocumentKeyLineCfg *args)
{
    cli_document_lock_take();
    const unsigned int answer = run_roles_locked(args);
    cli_document_lock_give();
    return answer;
}

// whether the folds of `one` go on. That is where its regions moved since they last went, and the walk colored every
// line of it. The step of the walk over it colored none, no coloring under way claims any, and the page holds lines
// of it
EMBED_INLINE int branch_folds_ready(const DocumentBranchCtx *args)
{
    return args->one_read->regions_moved && !args->one_read->refused &&
           (args->one_read->cached_to > args->one_read->cached_from) && (args->one_read->lines.lines != 0u) &&
           !branch_claims_any(args);
}

// the walk taken by `walker` step by step, until no text that it can walk has a line to color. Each step is on a text
// that no walker walks, and that the walk did not find colored whole since it last moved. The call tells whoever
// `colored` names, once a step colored a line that a reader reads. This runs on the own thread of a walker where `own`
// is 1, and the call marks it as one that walks no more as it ends
static void branch_walk(const DocumentBranchCtx *args)
{
    for (;;)
    {
        // each step runs under the lock of the texts, which the step gives back while the call colors the lines of a
        // bundle
        cli_document_lock_take();
        CliStoredText *one = cli_document_texts();
        while ((one != NULL) && (one->walked || one->resting))
        {
            one = one->next;
        }
        if (one == NULL)
        {
            args->walker->walking = args->own ? 0 : args->walker->walking;
            cli_document_lock_give();
            return;
        }
        int transferred = 0;
        one->walked = 1;
        EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
        int stepped = EMBED_CALL(branch_step, DocumentBranchCtx, .one = one, .transferred = &transferred);
        // the walk colored all of a text that the step colored none of. Its folds move on where its regions moved
        if (!stepped && EMBED_CALL(branch_folds_ready, DocumentBranchCtx, .one_read = one))
        {
            one->regions_moved = 0;
            stepped = 1;
            transferred = 1;
        }
        one->resting = !stepped;
        one->walked = 0;
        (void)EMBED_ATOMIC_ADD_32(&one->colors_serial, transferred ? 1u : 0u);
        EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
        const CliDocColored colored = s_colored;
        cli_document_lock_give();
        if (transferred && (colored != NULL))
        {
            colored();
        }
    }
}

static void branch_walk_run(void *context)
{
    EMBED_CALL(branch_walk, DocumentBranchCtx, .walker = (CliBranchWalker *)context, .own = 1);
}

void cli_document_wake(const CliDocumentCtx *args)
{
    if (args->one->let_go)
    {
        return;
    }
    args->one->resting = 0;
    // the walkers that walk, and the first free one. Then the texts that a walker walks now, and those that wait for
    // one
    unsigned int walking = 0u;
    unsigned int free_at = CLI_DOC_WALKERS;
    for (unsigned int at = 0u; at < CLI_DOC_WALKERS; at += 1u)
    {
        walking += s_walkers[at].walking ? 1u : 0u;
        free_at = (!s_walkers[at].walking && (free_at == CLI_DOC_WALKERS)) ? at : free_at;
    }
    unsigned int walked = 0u;
    unsigned int waiting = 0u;
    for (const CliStoredText *text = cli_document_texts(); text != NULL; text = text->next)
    {
        walked += text->walked ? 1u : 0u;
        waiting += (!text->walked && !text->resting) ? 1u : 0u;
    }
    // a walker started where more texts wait than the walkers between steps take up, and fewer walk than may
    const unsigned int between = (walking > walked) ? (walking - walked) : 0u;
    if (s_threaded && (free_at != CLI_DOC_WALKERS) && (walking < s_walkers_max) && (waiting > between))
    {
        CliBranchWalker *const walker = &s_walkers[free_at];
        walker->thread = (CliThread){branch_walk_run, walker, PLATFORM_PRIORITY_WORK};
        walker->walking = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &walker->thread);
    }
}

void cli_document_colored_wake(void)
{
    const CliDocColored colored = s_colored;
    if (colored != NULL)
    {
        colored();
    }
}

static void colors_to_locked(const CliDocumentColoredCfg *args)
{
    const unsigned int reserve = platform_processors() - 1u;
    s_colored = args->colored;
    s_threaded = args->threaded;
    if (reserve == 0u)
    {
        s_walkers_max = 1u;
    }
    else if (reserve < CLI_DOC_WALKERS)
    {
        s_walkers_max = reserve;
    }
    else
    {
        s_walkers_max = CLI_DOC_WALKERS;
    }
}

void cli_document_colors_to(const CliDocumentColoredCfg *args)
{
    cli_document_lock_take();
    colors_to_locked(args);
    cli_document_lock_give();
}

int cli_document_colored_by(const CliDocumentKeyGrammarCfg *args)
{
    int named = args->named;
    // the call counts the names that the tokens of the grammar take before it takes the lock of the texts
    const unsigned int names =
        (args->grammar[0] != '\0')
            ? EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = args->grammar,
                         .each_name_each = branch_name_pass, .context = NULL)
            : 0u;
    cli_document_lock_take();
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->grammar, .cap = CLI_POOL_BYTES});
    if ((one != NULL) && (length < CLI_NAME_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = one->grammar, .src = args->grammar, .bytes = length + 1ULL});
        EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = one);
        one->names = names;
        (void)EMBED_ATOMIC_ADD_32(&one->colors_serial, 1u);
        // the cells of the states go back, and the call empties the index. The cells of their identities and of the
        // index stay for the states to come
        EMBED_CALL(cli_document_internal.states_released, CliDocumentCtx, .one = one);
        one->refused = 0;
        one->named = named;
        (void)EMBED_CALL(branch_state, DocumentBranchCtx, .one = one, .text = "root");
        for (unsigned int line = 0u; line < one->lines.lines; line += 1u)
        {
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = one->lines.words,
                .offset = (((unsigned long long)(CLI_DOC_WORD_BEGUN)*one->lines.lines) + line) * sizeof(unsigned int),
                .src = &(unsigned int){CLI_DOC_UNCOLORED},
                .bytes = sizeof(unsigned int)});
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = one->lines.words,
                .offset = (((unsigned long long)(CLI_DOC_WORD_ENDED)*one->lines.lines) + line) * sizeof(unsigned int),
                .src = &(unsigned int){CLI_DOC_UNCOLORED},
                .bytes = sizeof(unsigned int)});
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = one->lines.words,
                .offset = (((unsigned long long)(CLI_DOC_WORD_REGIONS)*one->lines.lines) + line) * sizeof(unsigned int),
                .src = &(unsigned int){0u},
                .bytes = sizeof(unsigned int)});
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = one->lines.words,
                .offset = (((unsigned long long)(CLI_DOC_WORD_RUNS)*one->lines.lines) + line) * sizeof(unsigned int),
                .src = &(unsigned int){CLI_DOC_NO_RUNS},
                .bytes = sizeof(unsigned int)});
        }
        one->run_used = 0u;
        // the roles saved were the last grammar's names', read again for this one as its lines are first claimed
        one->roles_complete = 0;
        one->regions_moved = 1;
        EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
    }
    cli_document_lock_give();
    return one != NULL;
}

// the lines that the view mirrors, moved to those from `from` to before `to`. The lines of a paged text that the view
// lets go let their runs go with them. The walk colors again a line that the view comes to mirror and that saves no
// runs that exist, as it comes to it
EMBED_INLINE void branch_cache_moved(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one;
    const CliStoredLines *const lines = &one->lines;
    const unsigned int was_from = one->cached_from;
    const unsigned int was_to = (one->cached_to < lines->lines) ? one->cached_to : lines->lines;
    for (unsigned int line = was_from; one->paged && (line < was_to); line += 1u)
    {
        if ((line < args->from) || (line >= args->to))
        {
            {
                const unsigned int stored_value = CLI_DOC_NO_RUNS;
                (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = lines->words,
                    .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + line) * sizeof(stored_value),
                    .src = &stored_value,
                    .bytes = sizeof(stored_value)});
            }
        }
    }
    for (unsigned int line = args->from; line < args->to; line += 1u)
    {
        const int passed = (line >= was_from) && (line < was_to);
        if (!passed && !EMBED_CALL(branch_runs_existing, DocumentBranchCtx, .one_read = one, .line = line))
        {
            {
                const unsigned int stored_value = CLI_DOC_UNCOLORED;
                (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = lines->words,
                    .offset = (((unsigned long long)CLI_DOC_WORD_BEGUN * lines->lines) + line) * sizeof(stored_value),
                    .src = &stored_value,
                    .bytes = sizeof(stored_value)});
            }
        }
    }
}

static int cache_locked(const CliDocumentKeyFromCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one != NULL)
    {
        one->refused =
            one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
        const CliStoredLines *const lines = &one->lines;
        // the lines the view mirrors are among those the pages count, which an unsigned int counts
        const unsigned int cached_from = (args->from < lines->lines) ? (unsigned int)args->from : lines->lines;
        const unsigned int cached_to = (args->to < lines->lines) ? (unsigned int)args->to : lines->lines;
        EMBED_CALL(branch_cache_moved, DocumentBranchCtx, .one = one, .from = cached_from, .to = cached_to);
        // a paged text's folds are those of the lines the view mirrors
        one->regions_moved =
            one->regions_moved || (one->paged && ((cached_from != one->cached_from) || (cached_to != one->cached_to)));
        one->cached_from = cached_from;
        one->cached_to = cached_to;
        EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
    }
    return one != NULL;
}

int cli_document_cache(const CliDocumentKeyFromCfg *args)
{
    cli_document_lock_take();
    const int answer = cache_locked(args);
    cli_document_lock_give();
    return answer;
}

// the lines from `first` to before `end` of `one` walked where they save no runs that are, their columns cut at
// `columns`
EMBED_INLINE void branch_lines_walk(const DocumentBranchCtx *args)
{
    CliStoredText *const one = args->one;
    int any = 0;
    CliBranchRange range =
        EMBED_CALL(branch_range_begin, DocumentBranchCtx, .one_read = one, .any_pointer = &any, .out = NULL,
                   .columns = args->columns, .bytes = CLI_DOC_ALL_BYTES, .high = args->end);
    range.requested = 1;
    range.hand_from = args->first;
    range.hand_to = args->end;
    for (unsigned int line = args->first;
         (one->grammar[0] != '\0') && !one->refused && !range.stale && (line < args->end);)
    {
        if (EMBED_CALL(branch_runs_existing, DocumentBranchCtx, .one_read = one, .line = line))
        {
            line += 1u;
            continue;
        }
        unsigned int end = line + 1u;
        while ((end < args->end) && !EMBED_CALL(branch_runs_existing, DocumentBranchCtx, .one_read = one, .line = end))
        {
            end += 1u;
        }
        EMBED_CALL(branch_range_color, DocumentBranchCtx, .one = one, .first = line, .end = end, .onward = 0,
                   .range = &range);
        line = (range.next > end) ? range.next : end;
    }
}

// how many lines in order from `at` of the `count` at `number` follow one another
EMBED_INLINE unsigned long long branch_lines_following(const DocumentBranchCtx *args)
{
    unsigned long long end = args->word_at;
    while (((end + 1ULL) < args->capacity) && (args->number[end + 1ULL] == (args->number[end] + 1ULL)))
    {
        end += 1ULL;
    }
    return end + 1ULL;
}

// the `capacity` lines that `number` names, walked, with those that follow one another together. The call cuts each at
// `columns` where that is not 0, and widens the lines that the frame mirrors to them where `mirrored`. The return is
// the words that their runs take, counted from the records
EMBED_INLINE unsigned long long branch_runs_walked(const DocumentBranchCtx *args)
{
    CliStoredText *const one = args->one;
    const CliStoredLines *const lines = &one->lines;
    unsigned long long words = 0ULL;
    for (unsigned long long at = 0ULL; at < args->capacity;)
    {
        const unsigned long long end = EMBED_CALL(branch_lines_following, DocumentBranchCtx, .number = args->number,
                                                  .capacity = args->capacity, .word_at = at);
        // the lines of a text are among those it counts, which an unsigned int counts
        const unsigned int from = (args->number[at] < lines->lines) ? (unsigned int)args->number[at] : lines->lines;
        const unsigned int to =
            (args->number[end - 1ULL] < lines->lines) ? ((unsigned int)args->number[end - 1ULL] + 1u) : lines->lines;
        EMBED_CALL(branch_lines_walk, DocumentBranchCtx, .one = one, .first = from, .end = to,
                   .columns = args->columns);
        for (unsigned int line = from; line < to; line += 1u)
        {
            unsigned int place = 0u;
            (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = lines->words,
                .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + line) * sizeof(place),
                .dst = &place,
                .bytes = sizeof(place)});
            if (place != CLI_DOC_NO_RUNS)
            {
                unsigned int line_runs = 0u;
                (void)g_platform_guard->movable_buf_read(
                    &(CarcerMovableCfg){.cell = one->run,
                                        .offset = (unsigned long long)place * sizeof(line_runs),
                                        .dst = &line_runs,
                                        .bytes = sizeof(line_runs)});
                words += 2ULL * line_runs;
            }
        }
        if (args->mirrored)
        {
            const unsigned int cached_from = one->cached_from;
            const unsigned int cached_to = one->cached_to;
            const int meet = (from <= cached_to) && (to >= cached_from) && (cached_from < cached_to);
            one->cached_from = (meet && (cached_from < from)) ? cached_from : from;
            one->cached_to = (meet && (cached_to > to)) ? cached_to : to;
        }
        at = end;
    }
    return words;
}

static int runs_walk_locked(const CliDocumentRunsCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
    one->refused =
        one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
    (void)EMBED_CALL(branch_runs_walked, DocumentBranchCtx, .one = one, .number = args->number, .capacity = args->count,
                     .columns = args->mirrored ? one->columns : 0ULL, .mirrored = args->mirrored);
    EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    return 1;
}

int cli_document_runs_walk(const CliDocumentRunsCfg *args)
{
    // the walk colors a bundle with the lock given back, thus it starts and ends under the lock
    cli_document_lock_take();
    const int walked = runs_walk_locked(args);
    cli_document_lock_give();
    return walked;
}

// the place of the record of runs the line `line` saves in the text's runs cell: CLI_DOC_NO_RUNS where it saves none
EMBED_INLINE unsigned int branch_runs_place(const DocumentBranchCtx *args)
{
    const CliStoredLines *const lines = &args->one_read->lines;
    unsigned int place = CLI_DOC_NO_RUNS;
    if (args->line < lines->lines)
    {
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + args->line) * sizeof(place),
            .dst = &place,
            .bytes = sizeof(place)});
    }
    return place;
}

// how many runs the record at `place` holds, those that start at or past `columns` left off where it is not 0
EMBED_INLINE unsigned int branch_record_runs(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    unsigned int recorded = 0u;
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = one->run,
                            .offset = (unsigned long long)args->place * sizeof(recorded),
                            .dst = &recorded,
                            .bytes = sizeof(recorded)});
    const unsigned long long col_at = (unsigned long long)args->place + CLI_DOC_RECORD_HEAD;
    unsigned int runs = recorded;
    unsigned int last = 0u;
    while ((args->columns != 0ULL) && (runs > 1u))
    {
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = one->run, .offset = (col_at + runs - 1u) * sizeof(last), .dst = &last, .bytes = sizeof(last)});
        if (last < args->columns)
        {
            break;
        }
        runs -= 1u;
    }
    return runs;
}

// the line asked of the text `one_read`, or the count of its lines where it is past them. A line of a text is among
// the lines that it counts, which an unsigned int counts
EMBED_INLINE unsigned int branch_line_asked(const DocumentBranchCtx *args)
{
    const unsigned int lines = args->one_read->lines.lines;
    return (args->first_u64 < lines) ? (unsigned int)args->first_u64 : lines;
}

unsigned int cli_document_line_runs(const CliDocumentLineRunCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return 0u;
    }
    const unsigned int place =
        EMBED_CALL(branch_runs_place, DocumentBranchCtx, .one_read = one,
                   .line = EMBED_CALL(branch_line_asked, DocumentBranchCtx, .one_read = one, .first_u64 = args->line));
    return (place == CLI_DOC_NO_RUNS) ? 0u
                                      : EMBED_CALL(branch_record_runs, DocumentBranchCtx, .one_read = one,
                                                   .place = place, .columns = args->mirrored ? one->columns : 0ULL);
}

int cli_document_line_run(const CliDocumentLineRunCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return 0;
    }
    const unsigned int place =
        EMBED_CALL(branch_runs_place, DocumentBranchCtx, .one_read = one,
                   .line = EMBED_CALL(branch_line_asked, DocumentBranchCtx, .one_read = one, .first_u64 = args->line));
    if ((place == CLI_DOC_NO_RUNS) ||
        (args->run >= EMBED_CALL(branch_record_runs, DocumentBranchCtx, .one_read = one, .place = place,
                                 .columns = args->mirrored ? one->columns : 0ULL)))
    {
        return 0;
    }
    // a record holds its count, then its columns, then its names
    unsigned int recorded = 0u;
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one->run,
                                                            .offset = (unsigned long long)place * sizeof(recorded),
                                                            .dst = &recorded,
                                                            .bytes = sizeof(recorded)});
    const unsigned long long col_at = (unsigned long long)place + CLI_DOC_RECORD_HEAD;
    if (args->column != NULL)
    {
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one->run,
                                                                .offset = (col_at + args->run) * sizeof(unsigned int),
                                                                .dst = args->column,
                                                                .bytes = sizeof(unsigned int)});
    }
    if (args->name != NULL)
    {
        (void)g_platform_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = one->run,
                                .offset = (col_at + recorded + args->run) * sizeof(unsigned int),
                                .dst = args->name,
                                .bytes = sizeof(unsigned int)});
    }
    return 1;
}

// the bars of the line that a record saves, arranged on the map. A run with a name gets a bar, and the empty name a
// gap. The gap that opens the line is `tab` cells for each unit
EMBED_INLINE void branch_map_line(const DocumentBranchCtx *args)
{
    const unsigned int *const record = args->record;
    const unsigned int runs = record[0];
    const unsigned int *const col = record + CLI_DOC_RECORD_HEAD;
    const unsigned int *const name = col + runs;
    unsigned int cell = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        const unsigned int to = ((run + 1u) < runs) ? col[run + 1u] : record[1];
        const unsigned int width = to - col[run];
        if (name[run] == 0u)
        {
            cell += width * ((col[run] == 0u) ? args->tab : 1u);
        }
        else if (width != 0u)
        {
            const CliDocumentMapBar bar = {cell, width, name[run]};
            (void)g_platform_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = args->map_cell,
                                    .offset = CLI_DOCUMENT_MAP_BAR_AT(args->map_count, *args->bars_pointer),
                                    .src = &bar,
                                    .bytes = sizeof(bar)});
            *args->bars_pointer += 1u;
            cell += width;
        }
    }
}

const CarcerMovableEntry *cli_document_map_cell(const CliDocumentMapCfg *args)
{
    if ((args->map != NULL) && (*args->capacity >= args->bytes))
    {
        return args->map;
    }
    (void)g_platform_guard->movable_buf_release(args->map, args->owner);
    const unsigned long long room = args->bytes + (args->bytes / 2ULL);
    const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc(room, args->owner);
    *args->capacity = (cell != NULL) ? room : 0ULL;
    return cell;
}

static const CarcerMovableEntry *map_alloc_locked(const CliDocumentMapCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        // a key with no text gives the view's cell back, as a refused take does
        (void)g_platform_guard->movable_buf_release(args->map, args->owner);
        *args->capacity = 0ULL;
        return NULL;
    }
    EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
    one->refused =
        one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
    const CliStoredLines *const lines = &one->lines;
    // the lines asked are among those the text counts, which an unsigned int counts
    const unsigned int from = (args->first < lines->lines) ? (unsigned int)args->first : lines->lines;
    const unsigned int to = (args->last < lines->lines) ? ((unsigned int)args->last + 1u) : lines->lines;
    EMBED_CALL(branch_lines_walk, DocumentBranchCtx, .one = one, .first = from, .end = to, .columns = 0ULL);
    // a bar for each run at max and one for a line with none, counted off the records
    unsigned long long max = 0ULL;
    for (unsigned int line = from; line < to; line += 1u)
    {
        unsigned int place = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + line) * sizeof(place),
            .dst = &place,
            .bytes = sizeof(place)});
        unsigned int word_10 = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one->run,
                                                                .offset = (unsigned long long)place * sizeof(word_10),
                                                                .dst = &word_10,
                                                                .bytes = sizeof(word_10)});

        max += (place != CLI_DOC_NO_RUNS) ? word_10 : 1ULL;
    }
    const unsigned int count = to - from;
    // the map's lines for a view's frames go into the cell the view keeps, where it holds them
    const CarcerMovableEntry *const map =
        cli_document_map_cell(&(CliDocumentMapCfg){.owner = args->owner,
                                                   .map = args->map,
                                                   .capacity = args->capacity,
                                                   .bytes = CLI_DOCUMENT_MAP_BAR_AT(count, max)});
    if (map != NULL)
    {
        unsigned int bars = 0u;
        // each line's record read into one work cell of the temporary tier, which goes back as the map ends
        BranchWork work = {NULL, 0ULL};
        for (unsigned int at = 0u; at <= count; at += 1u)
        {
            (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = map, .offset = CLI_DOCUMENT_MAP_LINE_AT(at), .src = &bars, .bytes = sizeof(bars)});
            if (at == count)
            {
                break;
            }
            const unsigned int line = from + at;
            unsigned int place = 0u;
            unsigned long long length = 0ULL;
            unsigned int *const record =
                EMBED_CALL(branch_line_record, DocumentBranchCtx, .one_read = one, .line_number = line,
                           .place_out = &place, .length_out = &length, .work = &work);
            if (record != NULL)
            {
                EMBED_CALL(branch_map_line, DocumentBranchCtx, .map_cell = map, .map_count = count,
                           .bars_pointer = &bars, .tab = args->tab, .record = record);
            }
            else if ((place == CLI_DOC_NO_RUNS) && !one->paged && (length != 0ULL))
            {
                // the units of a line a map shows are far inside what an unsigned int represents
                const CliDocumentMapBar bar = {0u, (unsigned int)length, 0u};
                (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = map, .offset = CLI_DOCUMENT_MAP_BAR_AT(count, bars), .src = &bar, .bytes = sizeof(bar)});
                bars += 1u;
            }
        }
        g_platform_guard->persistent_buf_release(work.cell, g_cli_editor_owner);
        const CliDocumentMapHead head = {from, count, bars};
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = map, .offset = 0ULL, .src = &head, .bytes = sizeof(head)});
    }
    EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    return map;
}

const CarcerMovableEntry *cli_document_map_alloc(const CliDocumentMapCfg *args)
{
    cli_document_lock_take();
    const CarcerMovableEntry *const answer = map_alloc_locked(args);
    cli_document_lock_give();
    return answer;
}

// the order of two words, by their hash: below 0, 0 or above 0
static int branch_word_order(const void *left, const void *right)
{
    const BranchWordPlace *const one = (const BranchWordPlace *)left;
    const BranchWordPlace *const other = (const BranchWordPlace *)right;
    return (one->hash < other->hash) ? -1 : ((one->hash > other->hash) ? 1 : 0);
}

// 1 where the words `one` and `other` have the same bytes
static int branch_word_same(const CliDocumentWords *words, unsigned int one, unsigned int other)
{
    return (words->length[one] == words->length[other]) &&
           (memor.cmp(&(MemoriaCfg){.src = words->text + words->at[one],
                                    .other = words->text + words->at[other],
                                    .bytes = words->length[one]}) == 0);
}

// Each word put once. The call orders the words by their hash, with a stable sort, so that the first of a hash comes
// first. A word that an earlier word of its hash repeats takes the length 0. Then the words close up over those, in
// the order that they came. `order` and `room` hold the count of places of the words
static void branch_words_once(CliDocumentWords *words, BranchWordPlace *order, BranchWordPlace *room)
{
    for (unsigned int at = 0u; at < words->count; at += 1u)
    {
        order[at] = (BranchWordPlace){EMBED_CALL(branch_state_hash, DocumentBranchCtx,
                                                 .text = words->text + words->at[at], .length = words->length[at]),
                                      at};
    }
    (void)memor.sort(&(MemoriaCfg){.dst = order,
                                   .scratch = room,
                                   .count = words->count,
                                   .bytes = sizeof(BranchWordPlace),
                                   .order = branch_word_order});
    unsigned int first = 0u;
    while (first < words->count)
    {
        unsigned int end = first + 1u;
        while ((end < words->count) && (order[end].hash == order[first].hash))
        {
            end += 1u;
        }
        for (unsigned int later = first + 1u; later < end; later += 1u)
        {
            for (unsigned int earlier = first; earlier < later; earlier += 1u)
            {
                if ((words->length[order[earlier].word] != 0u) &&
                    branch_word_same(words, order[earlier].word, order[later].word))
                {
                    words->length[order[later].word] = 0u;
                    break;
                }
            }
        }
        first = end;
    }
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < words->count; at += 1u)
    {
        if (words->length[at] != 0u)
        {
            words->at[kept] = words->at[at];
            words->length[kept] = words->length[at];
            kept += 1u;
        }
    }
    words->count = kept;
}

// the units of the run `run` of a record where it is a word to give, 0 where it is not. That is where the roles of its
// name name something, and it has two units or more. The cursor must be neither in it nor at its end
EMBED_INLINE unsigned int branch_word_units(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const unsigned int *const record = args->record;
    const unsigned int runs = record[0];
    const unsigned int *const col = record + CLI_DOC_RECORD_HEAD;
    const unsigned int name = col[runs + args->name];
    unsigned int roles = 0u;
    if (name < one->roles_capacity)
    {
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one->roles,
                                                                .offset = (unsigned long long)(name) * sizeof(roles),
                                                                .dst = &roles,
                                                                .bytes = sizeof(roles)});
    }
    const unsigned int from = col[args->name];
    const unsigned int after = ((args->name + 1u) < runs) ? col[args->name + 1u] : record[1];
    const int typed = (args->cursor_line == args->line) && (from <= args->cursor_col) && (args->cursor_col <= after);
    return (((roles & CLI_ROLE_WORD) != 0u) && ((after - from) >= 2u) && !typed) ? (after - from) : 0u;
}

// the words of the lines of the text from `from` to before `to`, counted into `max_pointer`, with their units into
// `units_pointer`. The call leaves out the word typed at the cursor. A paged text gives none
EMBED_INLINE void branch_words_count(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const CliStoredLines *const lines = &one->lines;
    // each line's record read into one work cell of the temporary tier, which goes back as the count ends
    BranchWork work = {NULL, 0ULL};
    for (unsigned int line = args->from; !one->paged && (line < args->to); line += 1u)
    {
        unsigned int place = 0u;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = lines->words,
            .offset = (((unsigned long long)CLI_DOC_WORD_RUNS * lines->lines) + line) * sizeof(place),
            .dst = &place,
            .bytes = sizeof(place)});
        unsigned int *const record =
            (place != CLI_DOC_NO_RUNS)
                ? EMBED_CALL(branch_record_alloc, DocumentBranchCtx, .one_read = one, .place = place, .work = &work)
                : NULL;
        for (unsigned int run = 0u; (record != NULL) && (run < record[0]); run += 1u)
        {
            const unsigned int taken =
                EMBED_CALL(branch_word_units, DocumentBranchCtx, .one_read = one, .record = record, .name = run,
                           .line = line, .cursor_line = args->cursor_line, .cursor_col = args->cursor_col);
            *args->max_pointer += (taken != 0u) ? 1ULL : 0ULL;
            *args->units_pointer += taken;
        }
    }
    g_platform_guard->persistent_buf_release(work.cell, g_cli_editor_owner);
}

// the words of the lines of the text from `from` to before `to`, put in `words`. Each goes as the bytes of its units,
// three for each unit at the max. A word goes in as often as the lines give it. The call leaves out the word typed at
// the cursor
EMBED_INLINE void branch_words_fill(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const CliStoredLines *const lines = &one->lines;
    CliDocumentWords *const words = args->words;
    unsigned long long used = 0ULL;
    // each line's record and units read into two work cells of the temporary tier, which go back as the fill ends
    BranchWork record_work = {NULL, 0ULL};
    BranchWork unit_work = {NULL, 0ULL};
    for (unsigned int line = args->from; !one->paged && (line < args->to); line += 1u)
    {
        unsigned int place = 0u;
        unsigned long long length = 0ULL;
        unsigned int *const record =
            EMBED_CALL(branch_line_record, DocumentBranchCtx, .one_read = one, .line_number = line, .place_out = &place,
                       .length_out = &length, .work = &record_work);
        unsigned short *const unit = ((record != NULL) && (length != 0ULL))
                                         ? EMBED_CALL(branch_work_room, DocumentBranchCtx, .work = &unit_work,
                                                      .at_u64 = length * sizeof(unsigned short))
                                         : NULL;
        if (unit != NULL)
        {
            {
                unsigned long long line_start = 0ULL;
                (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = lines->starts,
                                                                        .offset = line * sizeof(line_start),
                                                                        .dst = &line_start,
                                                                        .bytes = sizeof(line_start)});
                (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = lines->units,
                                                                        .offset = line_start * sizeof(unsigned short),
                                                                        .dst = unit,
                                                                        .bytes = length * sizeof(unsigned short)});
            }
        }
        for (unsigned int run = 0u; (unit != NULL) && (run < record[0]); run += 1u)
        {
            const unsigned int taken =
                EMBED_CALL(branch_word_units, DocumentBranchCtx, .one_read = one, .record = record, .name = run,
                           .line = line, .cursor_line = args->cursor_line, .cursor_col = args->cursor_col);
            if (taken == 0u)
            {
                continue;
            }
            VerbaLinea written = {words->text + used, 3ULL * taken, 0ULL};
            EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &written,
                       .units = unit + record[CLI_DOC_RECORD_HEAD + run], .count = taken);
            words->at[words->count] = used;
            // a word's bytes are far inside what an unsigned int represents
            words->length[words->count] = (unsigned int)written.at;
            used += written.at;
            words->count += 1u;
        }
    }
    g_platform_guard->persistent_buf_release(unit_work.cell, g_cli_editor_owner);
    g_platform_guard->persistent_buf_release(record_work.cell, g_cli_editor_owner);
}

static CliDocumentWords *words_alloc_locked(const CliDocumentWordsCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return NULL;
    }
    EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
    const CliStoredLines *const lines = &one->lines;
    // the lines asked are among those the text counts, which an unsigned int counts
    const unsigned int from = (args->first < lines->lines) ? (unsigned int)args->first : lines->lines;
    const unsigned int to = (args->last < lines->lines) ? ((unsigned int)args->last + 1u) : lines->lines;
    // the words and their bytes at max, three a unit, counted off the records
    unsigned long long max = 0ULL;
    unsigned long long units = 0ULL;
    EMBED_CALL(branch_words_count, DocumentBranchCtx, .one_read = one, .from = from, .to = to,
               .cursor_line = args->cursor.line, .cursor_col = args->cursor.col, .max_pointer = &max,
               .units_pointer = &units);
    CliDocumentWords *const words = g_platform_guard->temporary_buf_alloc(
        sizeof(*words) + (max * (sizeof(unsigned long long) + sizeof(unsigned int))) + (3ULL * units) + 1ULL,
        g_cli_editor_owner);
    // the order of the words and the room its sort writes, the words' count of places each, one at the fewest
    const unsigned long long places = (max != 0ULL) ? max : 1ULL;
    BranchWordPlace *const order =
        (words != NULL) ? g_platform_guard->temporary_buf_alloc(2ULL * places * sizeof(BranchWordPlace), g_cli_editor_owner)
                        : NULL;
    if (words != NULL)
    {
        words->count = 0u;
        words->at = (unsigned long long *)(void *)(words + 1);
        words->length = (unsigned int *)(void *)(words->at + max);
        words->text = (char *)(void *)(words->length + max);
    }
    if (order != NULL)
    {
        EMBED_CALL(branch_words_fill, DocumentBranchCtx, .one_read = one, .from = from, .to = to, .words = words,
                   .cursor_line = args->cursor.line, .cursor_col = args->cursor.col);
        branch_words_once(words, order, order + places);
    }
    EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    g_platform_guard->persistent_buf_release(order, g_cli_editor_owner);
    return words;
}

void cli_document_words_each(const CliDocumentWordsCfg *args)
{
    CliDocumentWords *const words = words_alloc_locked(args);
    // the words go to the caller: the list is this call's own
    for (unsigned int at = 0u; (words != NULL) && (at < words->count); at += 1u)
    {
        EMBED_CALL(args->each, CliEachTextLengthCfg, .context = args->context, .text = words->text + words->at[at],
                   .length = words->length[at]);
    }
    g_platform_guard->persistent_buf_release(words, g_cli_editor_owner);
}

void cli_document_walk(void)
{
    EMBED_CALL(branch_walk, DocumentBranchCtx, .walker = &s_walkers[CLI_DOC_WALKERS], .own = 0);
}

static unsigned int walkers_locked(void)
{
    unsigned int walking = 0u;
    for (unsigned int at = 0u; at < CLI_DOC_WALKERS; at += 1u)
    {
        walking += s_walkers[at].walking ? 1u : 0u;
    }
    return walking;
}

unsigned int cli_document_walkers(void)
{
    cli_document_lock_take();
    const unsigned int answer = walkers_locked();
    cli_document_lock_give();
    return answer;
}

// A text read whole copied out of its cells under the lock, in three reads. Its lines' starts and lengths go into one
// cell of the temporary tier, then all its units. The count of lines goes into `size_pointer`. Returns the cell, or
// NULL where the key holds no text read whole or the cells cannot meet it
static void *text_units_locked(const CliDocumentKeyCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if ((one == NULL) || one->paged)
    {
        return NULL;
    }
    const CliStoredLines *const lines = &one->lines;
    const unsigned long long places = (unsigned long long)lines->lines * sizeof(unsigned long long);
    const unsigned long long units = ((lines->units != NULL) ? lines->used : 0ULL) * sizeof(unsigned short);
    char *const copy = g_platform_guard->temporary_buf_alloc((2ULL * places) + units + 1ULL, args->owner);
    if (copy == NULL)
    {
        return NULL;
    }
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = lines->starts, .dst = copy, .bytes = places});
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = lines->lengths, .dst = copy + places, .bytes = places});
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = lines->units, .dst = copy + (2ULL * places), .bytes = units});
    if (lines->units == NULL)
    {
        // a text whose cells hold no units writes each line empty
        memor.set(&(MemoriaCfg){.dst = copy + places, .val = 0, .bytes = places});
    }
    *args->size_pointer = lines->lines;
    return copy;
}

// The text the copy `copy_read` of text_units_locked holds, written as UTF-8 into a cell of the temporary tier. Its
// `count_u64` lines have a newline between each two and a NUL after the last. The caller gives the cell back. Its
// bytes go into `length_out`. Returns the cell, or NULL where the cells cannot meet it
EMBED_INLINE char *text_from_units(const DocumentBranchCtx *args)
{
    // the starts, lengths and units lie in that order, the units 2-byte aligned after two arrays of 8-byte words
    const unsigned long long lines = args->count_u64;
    const unsigned long long *const start = args->copy_read;
    const unsigned long long *const length = start + lines;
    const unsigned short *const unit = (const unsigned short *)(const void *)(length + lines);
    // each unit written as three bytes at max, each line's newline, and the NUL after the last
    unsigned long long capacity = 1ULL;
    for (unsigned long long line = 0ULL; line < lines; line += 1ULL)
    {
        capacity += (3ULL * length[line]) + 1ULL;
    }
    char *const reserved = g_platform_guard->temporary_buf_alloc(capacity, g_cli_editor_owner);
    if (reserved == NULL)
    {
        return NULL;
    }
    VerbaLinea text = {reserved, capacity, 0ULL};
    for (unsigned long long line = 0ULL; line < lines; line += 1ULL)
    {
        EMBED_CALL(branch_units_utf8, DocumentBranchCtx, .units = unit + start[line], .count_u64 = length[line],
                   .out = &text);
        mmgr_line_put(&text, ((line + 1ULL) < lines) ? "\n" : "");
    }
    reserved[text.at] = '\0';
    *args->length_out = text.at;
    return reserved;
}

int cli_document_text_each(const CliDocumentKeyCfg *args)
{
    unsigned long long size = 0ULL;
    unsigned long long lines = 0ULL;
    // the lock spans the copy of the cells alone; the text comes from the copy after the lock goes
    cli_document_lock_take();
    void *const copy =
        text_units_locked(&(CliDocumentKeyCfg){.key = args->key, .size_pointer = &lines, .owner = g_cli_editor_owner});
    cli_document_lock_give();
    char *const text = (copy != NULL) ? EMBED_CALL(text_from_units, DocumentBranchCtx, .copy_read = copy,
                                                   .count_u64 = lines, .length_out = &size)
                                      : NULL;
    g_platform_guard->persistent_buf_release(copy, g_cli_editor_owner);
    // the text goes to the caller: the cell is this call's own
    if (text != NULL)
    {
        EMBED_CALL(args->each, CliEachTextLengthCfg, .context = args->context, .text = text, .length = size);
    }
    g_platform_guard->persistent_buf_release(text, g_cli_editor_owner);
    return text != NULL;
}

static int file_put_locked(const CliDocumentOutCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if ((one == NULL) || one->paged)
    {
        mmgr_line_put(args->err_msg, "no text read whole is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    const CliStoredLines *const lines = &one->lines;
    const char *const ending = lines->crlf ? "\r\n" : "\n";
    // each line's units read into one work cell of the temporary tier, which goes back as the put ends
    BranchWork work = {NULL, 0ULL};
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        mmgr_line_put(args->out, (line != 0u) ? ending : "");
        EMBED_CALL(branch_line_append, DocumentBranchCtx, .lines = lines, .line = line, .out = args->out,
                   .work = &work);
    }
    g_platform_guard->persistent_buf_release(work.cell, g_cli_editor_owner);
    return mmgr_line_ok(args->out);
}

int cli_document_file_put(const CliDocumentOutCfg *args)
{
    cli_document_lock_take();
    const int answer = file_put_locked(args);
    cli_document_lock_give();
    return answer;
}

// the lines asked of a stored text, `first_u64` to `last_u64` cut to the text, into `first_pointer` and `end_pointer`,
// the last of them. The own state of the colorer that the walk enters the first in goes into `entered`, with the scope
// after its mark left out. The call writes the lines into `out`, with a newline between each two. For a paged text, it
// writes its key into `text_out`, CLI_NAME_MAX_LEN bytes. Then a reader reads its lines through the pages with the lock
// given back
EMBED_INLINE void branch_lines_stored(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const CliStoredLines *const lines = &one->lines;
    // the lines asked are below the text's last, which an unsigned int counts
    const unsigned int to = (args->last_u64 < lines->lines) ? (unsigned int)args->last_u64 : (lines->lines - 1u);
    const unsigned int from = (args->first_u64 <= to) ? (unsigned int)args->first_u64 : to;
    *args->first_pointer = from;
    *args->end_pointer = to;
    unsigned int word_7 = 0u;
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = lines->words,
                            .offset = (((unsigned long long)CLI_DOC_WORD_BEGUN * lines->lines) + from) * sizeof(word_7),
                            .dst = &word_7,
                            .bytes = sizeof(word_7)});

    const unsigned int begun = (lines->lines != 0u) ? word_7 : CLI_DOC_UNCOLORED;
    // the state that the walk enters the first line in, read from the cell of the states
    char state[CLI_DOC_STATE];
    const unsigned long long state_length =
        (begun != CLI_DOC_UNCOLORED)
            ? EMBED_CALL(branch_state_text, DocumentBranchCtx, .one_read = one, .at = begun, .text_out = state)
            : 0ULL;
    const char *const begun_in = (state_length != 0ULL) ? state : "root";
    const char *const mark =
        cellul.chr(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_POOL_BYTES, .byte = CLI_DOC_SCOPE_MARK});
    mmgr_line_put_n(args->entered, begun_in,
                    (mark != NULL) ? (unsigned long long)(mark - begun_in)
                                   : cellul.len(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_POOL_BYTES}));
    if (one->paged && (lines->lines != 0u))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->text_out, .src = one->key, .bytes = CLI_NAME_MAX_LEN});
    }
    for (unsigned int at = from; !one->paged && (lines->lines != 0u) && (at <= to); at += 1u)
    {
        EMBED_CALL(branch_line_append, DocumentBranchCtx, .lines = lines, .line = at, .out = args->out);
        mmgr_line_put(args->out, (at < to) ? "\n" : "");
    }
}

// the lines `from` to `to` of the paged text under the key `text` read through its pages and written into `out`, a
// newline between each two
EMBED_INLINE void branch_lines_paged(const DocumentBranchCtx *args)
{
    const unsigned long long required = (unsigned long long)(args->to + 1u - args->from);
    const char **const starts = g_platform_guard->temporary_buf_alloc(required * sizeof(const char *), g_cli_editor_owner);
    unsigned long long count = 0ULL;
    char *const reserved = (starts != NULL) ? EMBED_CALL(cli_pages_from.lines_alloc, CliPagesFromCfg, .key = args->text,
                                                         .from = args->from, .to = args->to + 1u, .line = starts,
                                                         .count = &count, .owner = g_cli_editor_owner)
                                            : NULL;
    for (unsigned long long at = 0ULL; (reserved != NULL) && (at < count); at += 1ULL)
    {
        mmgr_line_put(args->out, starts[at]);
        mmgr_line_put(args->out, ((args->from + at) < args->to) ? "\n" : "");
    }
    g_platform_guard->persistent_buf_release(reserved, g_cli_editor_owner);
    g_platform_guard->persistent_buf_release(starts, g_cli_editor_owner);
}

// the lines from `first` to `last` of the text under `key`, put after `text`, with the state that the walk enters the
// first in written into `state`. The return is 1, or 0 where no text is under `key`
static int lines_put_locked(const CliDocumentKeyCfg *args, VerbaLinea *text)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    VerbaLinea entered = {args->state, args->capacity, 0ULL};
    // a paged text's lines, read through the pages with the lock given back: its key, and the lines asked
    char paged[CLI_NAME_MAX_LEN];
    paged[0] = '\0';
    unsigned int from = 0u;
    unsigned int to = 0u;
    if (one != NULL)
    {
        EMBED_CALL(branch_lines_stored, DocumentBranchCtx, .one_read = one, .first_u64 = args->first,
                   .last_u64 = args->last, .first_pointer = &from, .end_pointer = &to, .entered = &entered,
                   .text_out = paged, .out = text);
    }
    if (paged[0] != '\0')
    {
        EMBED_CALL(branch_lines_paged, DocumentBranchCtx, .text = paged, .from = from, .to = to, .out = text);
    }
    if ((mmgr_line_finish(&entered) == 0ULL) && (args->capacity != 0ULL))
    {
        args->state[0] = '\0';
    }
    return one != NULL;
}

int cli_document_lines_read(const CliDocumentKeyCfg *args)
{
    cli_document_lock_take();
    const int found = lines_put_locked(args, args->out);
    cli_document_lock_give();
    return found && mmgr_line_ok(args->out);
}

static char *lines_alloc_locked(const CliDocumentKeyCfg *args)
{
    unsigned long long *size = args->size_pointer;
    PlatformText written;
    // The owner's place is its line of the owner table, which the platform's text names it by. Explicit cast takes
    // the line's count, which is below the table's bound
    platform_text_begun(&written, CLI_DOC_COLORS_CAPACITY, (unsigned int)(args->owner - mmgr_owner_table),
                        PLATFORM_TIER_TEMPORARY);
    const int made = written.cell != NULL;
    VerbaLinea *const text = &written.line;
    const int found = made && lines_put_locked(args, text);
    const int reserved = made && found && mmgr_line_ok(text);
    if (made && !reserved)
    {
        platform_text_ended(&written);
    }
    // the call passes the cell of the lines to the caller, who gives it back
    *size = reserved ? platform_text_size(&written) : 0ULL;
    return reserved ? written.cell : NULL;
}

int cli_document_lines_each(const CliDocumentKeyCfg *args)
{
    unsigned long long size = 0ULL;
    cli_document_lock_take();
    char *const text = EMBED_CALL(lines_alloc_locked, CliDocumentKeyCfg, .key = args->key, .first = args->first,
                                  .last = args->last, .size_pointer = &size, .state = args->state,
                                  .capacity = args->capacity, .owner = g_cli_editor_owner);
    cli_document_lock_give();
    // the lines go to the caller: the cell is this call's own
    if (text != NULL)
    {
        EMBED_CALL(args->each, CliEachTextLengthCfg, .context = args->context, .text = text, .length = size);
    }
    g_platform_guard->persistent_buf_release(text, g_cli_editor_owner);
    return text != NULL;
}

static int bounds_locked(const CliDocumentKeyLowCfg *args)
{
    unsigned long long *low = args->low;
    unsigned long long *high = args->high;
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    unsigned int from = 0u;
    unsigned int to = 0u;
    if (one != NULL)
    {
        EMBED_CALL(branch_bounds, DocumentBranchCtx, .one_read = one, .low_pointer = &from, .high_pointer = &to);
    }
    *low = from;
    *high = to;
    return one != NULL;
}

int cli_document_bounds(const CliDocumentKeyLowCfg *args)
{
    cli_document_lock_take();
    const int answer = bounds_locked(args);
    cli_document_lock_give();
    return answer;
}
