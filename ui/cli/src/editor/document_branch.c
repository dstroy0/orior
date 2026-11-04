// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_branch.c: a stored text colored as a branch of the tree is walked, its grammar applied a line at a time, the
// runs of each line colored handed on and the states it is colored from and ends in saved with the line. The walk works
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
// hands its colors on in the order they were put in
#include "../code/symbols.h"
#include "../code/symbols_internal.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "document_internal.h"
#include "highlight.h"
#include "pages.h"

#include <stdatomic.h>

// A walker: a thread of its own walking one text at a time, what it runs, the line its steps' colors are written into,
// taken once and emptied for each step, and whether it walks, under the lock
typedef struct
{
    CliThread thread;
    int walking;
} CliBranchWalker;
// The runs of runs of lines handed on as they are written: whether a `{"from", "runs"}` is begun, how many lines' runs
// it holds, whether a run of lines handed on with it was written before it, where they are written, the columns of
// each line whose runs are handed on, 0 for every one, how many names the grammar's tokens take, the place of the
// first a run that counts depth takes by its depth, 0 for a grammar that names none, the bytes of lines it colors yet,
// CLI_DOC_ALL_BYTES where it colors every line asked, and the line after the last whose runs were written; whether the
// text moved while it colored, and it colors no more; whether the lines whose runs are handed on are those it asks,
// from `hand_from` to before `hand_to`, else those the page holds as each is put in; and the line it colors no further
// than as it goes on past the lines asked. Its columns read as the text's as each line is put in, where it names them
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
#define CLI_DOC_SCOPE_NAMES 128u
#define CLI_DOC_SCOPE_TEXT 4096u
// The scope a line stands in as the walk reads it: the depth of the regions open, the depth of the pairs open that
// count it, and the names declared in the regions that a use of the name is named as, each its text's place among the
// texts and its bytes, the name its declaration's run takes, the depth of the region it belongs to, and whether it
// waits for that region to open, as a function's parameter waits for its body; how many, and the texts' bytes used; and
// its place among the text's states, found again only where it moved since, CLI_DOC_NONE where it was not found
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
// A bundle of lines as it is colored with no lock taken: the work it is written into; the scope the line colored next
// stands in, and whether the place among the text's states of the scope the line before it ended in is known; the names
// the grammar's tokens take, and the roles of each, read as the lines were claimed, `roles_count` of them; the words of
// what it found written; and whether the cells refused the capacity for them. What it finds on each line is a record of
// words: how many runs, the line's regions, the bytes of the colorer's state the line ends in, and the bytes of the
// scope it ends in, CLI_DOC_NONE for a scope with no depth, pairs or names, CLI_DOC_SCOPE_SAME for the one the line
// before it ended in; then the runs' columns, their names as the walk names them, the colorer's state and the scope,
// each of the two from a word of its own
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
// root's place among a colored text's states, the first written; the most lines a step of the walk colors in a run, and
// again after it, and the most bytes of lines it colors, the last line it colors maybe past them, and its colors are
// handed on in steps, and the bytes of a coloring that colors every line asked; the lines colored at once where a
// coloring stops at the first entered as before, and where it does not, CLI_DOC_STEP where its bytes reach no further;
// the capacity a step's colors are written in, a state's longest text, the capacity a text's states hold past them and
// the places of its index they are found by, the bytes a coloring's capacity is written in, the share of what a text's
// capacity for its states or its runs is taken for that it holds past it, and no line
#define CLI_DOC_ROOT 0u
#define CLI_DOC_STEP 256u
#define CLI_DOC_STEP_BYTES 16384ULL
#define CLI_DOC_ALL_BYTES 0xffffffffffffffffULL
#define CLI_DOC_BATCH 64u
#define CLI_DOC_RUN 4096u
#define CLI_DOC_COLORS_CAPACITY (1ULL << 20u)
#define CLI_DOC_STATE 8192u
#define CLI_DOC_STATES_CAPACITY (256u << 10u)
#define CLI_DOC_INDEX_POSITIONS 16384u
#define CLI_DOC_CAPACITY_SHARE 8ULL
#define CLI_DOC_NONE 0xffffffffu

// the depths of pairs a name each, a deeper pair taking the name of its depth less as many
#define CLI_DOC_DEPTHS 16u

// the most walkers; where the colors go, whether walkers walk them out, and how many may walk at once, the machine's
// processors but one, one at the fewest; the walkers, and past them the walk a caller takes on its own thread; and the
// lock the colors are handed on under, taken before the texts' lock is given back, and they go in the order they were
// put in
#define CLI_DOC_WALKERS 8u
static CliDocColored s_colored;
static int s_threaded;
static unsigned int s_walkers_max;
static CliBranchWalker s_walkers[CLI_DOC_WALKERS + 1u];

typedef struct
{
    const char *text;
    unsigned long long length;
    const CliStoredText *one_read;
    unsigned int at;
    CliStoredText *one;
    const CliLines *lines;
    unsigned int line;
    VerbaLinea *out;
    unsigned int from;
    unsigned int to;
    char *text_out;
    unsigned long long capacity;
    const char **line_letters;
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
    const char **from_letters;
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
    CliDocumentMap *map;
    unsigned int tab;
    CliDocumentWords *words;
    unsigned int (*word_table)[2];
    unsigned long long word_at;
    const unsigned long long *number;
    unsigned long long cursor_line;
    unsigned long long cursor_col;
    unsigned long long *most_pointer;
    unsigned long long *units_pointer;
    VerbaLinea *entered;
    unsigned long long first_u64;
    unsigned long long last_u64;
    int mirrored;
    CliDocumentRuns *runs_out;
    unsigned int roles;
    int renames;
    unsigned int names;
    unsigned int *found_bits;
    unsigned int *closes_pointer;
    unsigned int *unled_pointer;
    int mark;
} DocumentBranchCtx;
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
    return ((args->at + args->length) < args->one_read->state_used) &&
           (args->one_read->state[args->at + args->length] == '\0') &&
           (memor.cmp(&(MemoriaCfg){
                .src = args->one_read->state + args->at, .other = args->text, .bytes = args->length}) == 0);
}

// the state at the place `at` put at the first place of the index free from its hash's
EMBED_INLINE void branch_index_append(const DocumentBranchCtx *args)
{
    const unsigned long long length =
        cellul.len(&(CatenaFinitaCfg){.src = args->one->state + args->at, .cap = CLI_CELLS_POOL});
    unsigned int entry =
        EMBED_CALL(branch_state_hash, DocumentBranchCtx, .text = args->one->state + args->at, .length = length) &
        (args->one->state_indexed - 1u);
    while (args->one->state_index[entry] != 0u)
    {
        entry = (entry + 1u) & (args->one->state_indexed - 1u);
    }
    args->one->state_index[entry] = args->at + 1u;
}

// the index taken at its most where it stands untaken: 1, or 0 where the cells cannot meet it or half of it holds
// states
EMBED_INLINE int branch_index_init(const DocumentBranchCtx *args)
{
    if (args->one->state_indexed == 0u)
    {
        unsigned int *const cell =
            g_cli_cells->persistent_buf_alloc(CLI_DOC_INDEX_POSITIONS * sizeof(*cell), &g_cli_editor_cells);
        if (cell == NULL)
        {
            return 0;
        }
        memor.set(&(MemoriaCfg){.dst = cell, .val = 0, .bytes = CLI_DOC_INDEX_POSITIONS * sizeof(*cell)});
        args->one->state_index = cell;
        args->one->state_indexed = CLI_DOC_INDEX_POSITIONS;
    }
    return (2u * (args->one->state_count + 1u)) <= args->one->state_indexed;
}

// the place of the state `text` among the text's states, found by its hash, the place found last tried first, and
// written after them where it is not there: root's place where the cells cannot meet it, the text's colors refused
static unsigned int branch_state(const DocumentBranchCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
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
    for (unsigned int entry = EMBED_CALL(branch_state_hash, DocumentBranchCtx, .text = args->text, .length = length) &
                              (args->one->state_indexed - 1u);
         args->one->state_index[entry] != 0u; entry = (entry + 1u) & (args->one->state_indexed - 1u))
    {
        if (EMBED_CALL(branch_state_is, DocumentBranchCtx, .one_read = args->one,
                       .at = args->one->state_index[entry] - 1u, .text = args->text, .length = length))
        {
            args->one->state_last = args->one->state_index[entry] - 1u;
            return args->one->state_last;
        }
    }
    const unsigned long long needed = (unsigned long long)args->one->state_used + length + 1ULL;
    if (args->one->state_capacity == 0u)
    {
        args->one->state = g_cli_cells->persistent_buf_alloc(CLI_DOC_STATES_CAPACITY, &g_cli_editor_cells);
        args->one->state_capacity = (args->one->state != NULL) ? CLI_DOC_STATES_CAPACITY : 0u;
    }
    if (needed > args->one->state_capacity)
    {
        args->one->refused = 1;
        return CLI_DOC_ROOT;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = args->one->state + args->one->state_used, .src = args->text, .bytes = length + 1ULL});
    const unsigned int at = args->one->state_used;
    // the states take no more than their capacity
    args->one->state_used = (unsigned int)needed;
    EMBED_CALL(branch_index_append, DocumentBranchCtx, .one = args->one, .at = at);
    args->one->state_count += 1u;
    args->one->state_last = at;
    return at;
}

// the line `line` of `lines` written as UTF-8 into `out`, a surrogate pair read as the one letter it makes
EMBED_INLINE void branch_line_append(const DocumentBranchCtx *args)
{
    unsigned int line = args->line;
    const unsigned short *const unit = args->lines->unit + args->lines->start[line];
    const unsigned long long count = args->lines->length[line];
    unsigned long long place = 0ULL;
    while (place < count)
    {
        unsigned int letter = unit[place];
        const int paired = (letter >= 0xd800u) && (letter < 0xdc00u) && ((place + 1ULL) < count) &&
                           (unit[place + 1ULL] >= 0xdc00u) && (unit[place + 1ULL] < 0xe000u);
        if (paired)
        {
            letter = 0x10000u + ((letter - 0xd800u) << 10u) + (unit[place + 1ULL] - 0xdc00u);
            place += 1ULL;
        }
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = args->out, .letter = letter);
        place += 1ULL;
    }
}

// the lines from `from` to `to` of `lines` written as UTF-8 into `text`, `capacity` bytes, each ended by a NUL and each
// line's start put into `line`
EMBED_INLINE void branch_lines_write(const DocumentBranchCtx *args)
{
    VerbaLinea out = {args->text_out, args->capacity, 0ULL};
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        args->line_letters[at - args->from] = args->text_out + out.at;
        EMBED_CALL(branch_line_append, DocumentBranchCtx, .lines = args->lines, .line = at, .out = &out);
        mmgr_line_ch(&out, '\0');
    }
}

// the lines from `first` to `end` of a text read whole, `lines`, measured: the line after the last that `bytes` of
// their units reach, one at the least, into `read`, and the bytes they take as UTF-8, each unit written as three bytes
// at the most and each line's NUL; under the lock
EMBED_INLINE unsigned long long branch_lines_measure(const DocumentBranchCtx *args)
{
    unsigned int *read = args->read;
    unsigned long long capacity = 1ULL;
    unsigned long long spent = 0ULL;
    unsigned int to = args->first;
    while ((to < args->end) && ((to == args->first) || (args->bytes == CLI_DOC_ALL_BYTES) || (spent < args->bytes)))
    {
        capacity += (3ULL * args->lines->length[to]) + 1ULL;
        spent += args->lines->length[to] + 1ULL;
        to += 1u;
    }
    *read = to;
    return capacity;
}

// the mark a line's state carries the place of the scope it stands in after, past the colorer's own state, the scope
// written among the text's states once; the most names a scope holds and the bytes their texts take
#define CLI_DOC_SCOPE_MARK '\x1e'

// a line's scope the one the line before it ended in, the words of a record before its runs, and the words a line's
// state and scope are given capacity for, those of a line past them taken from the capacity of the lines after it
#define CLI_DOC_FOUND_STATE 64ULL
#define CLI_DOC_SCOPE_SAME 0xfffffffeu
#define CLI_DOC_FOUND_HEAD 5u

// a line's runs colored in the range: the range marked as having colored it, and the line it reached
EMBED_INLINE void branch_runs_append(const DocumentBranchCtx *args)
{
    *args->range->any = 1;
    args->range->next = args->line + 1u;
}

// the words a runs' capacity holds past the runs it is taken for, as many again as their share CLI_DOC_CAPACITY_SHARE
// holds
#define CLI_DOC_RUNS_RESERVE 4096ULL

// what the walk saves of a line: how many runs it found and the column the line ends at, then the runs' columns and
// their names
#define CLI_DOC_RECORD_HEAD 2u
#define CLI_DOC_RECORD_WORDS(record_) (CLI_DOC_RECORD_HEAD + (2u * (record_)[0]))

// the words of the runs the lines of `one` save, and a capacity the cells give them taken afresh, those runs moved into
// it in the order of their lines, and it holds them with `more` words after them and a share of them past those: 1, or
// 0 where the cells cannot meet it, the capacity as it was
EMBED_INLINE int branch_runs_capacity(const DocumentBranchCtx *args)
{
    CliLines *const lines = &args->one->lines;
    unsigned long long saved = 0ULL;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        saved +=
            (lines->runs[line] != CLI_DOC_NO_RUNS) ? CLI_DOC_RECORD_WORDS(args->one->run + lines->runs[line]) : 0ULL;
    }
    const unsigned long long required = saved + args->more;
    const unsigned long long capacity = required + (required / CLI_DOC_CAPACITY_SHARE) + CLI_DOC_RUNS_RESERVE;
    unsigned int *const moved = (capacity <= 0xffffffffULL)
                                    ? g_cli_cells->persistent_buf_alloc(capacity * sizeof(*moved), &g_cli_editor_cells)
                                    : NULL;
    if (moved == NULL)
    {
        return 0;
    }
    unsigned int used = 0u;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        const unsigned int at = lines->runs[line];
        if (at != CLI_DOC_NO_RUNS)
        {
            // a line's runs lie within the words saved, which an unsigned int counts
            const unsigned int words = CLI_DOC_RECORD_WORDS(args->one->run + at);
            memor.cpy(&(MemoriaCfg){.dst = moved + used, .src = args->one->run + at, .bytes = words * sizeof(*moved)});
            lines->runs[line] = used;
            used += words;
        }
    }
    g_cli_cells->persistent_buf_release(args->one->run, &g_cli_editor_cells);
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
    CliLines *const lines = &args->one->lines;
    lines->runs[args->line] = CLI_DOC_NO_RUNS;
    const unsigned long long words = CLI_DOC_RECORD_HEAD + (2ULL * args->leaves->runs);
    if (((args->one->run_used + words) > args->one->run_capacity) &&
        !EMBED_CALL(branch_runs_capacity, DocumentBranchCtx, .one = args->one, .more = words))
    {
        return;
    }
    unsigned int *const record = args->one->run + args->one->run_used;
    record[0] = args->leaves->runs;
    record[1] = args->leaves->end;
    unsigned int *const col = record + CLI_DOC_RECORD_HEAD;
    memor.cpy(&(MemoriaCfg){.dst = col, .src = args->leaves->col, .bytes = args->leaves->runs * sizeof(*record)});
    memor.cpy(&(MemoriaCfg){
        .dst = col + args->leaves->runs, .src = args->leaves->name, .bytes = args->leaves->runs * sizeof(*record)});
    lines->runs[args->line] = args->one->run_used;
    // the words written lie within the capacity, which an unsigned int counts
    args->one->run_used += (unsigned int)words;
}

// whether the line `line` of `one` saves runs it may hand on as they are: it is colored, saves runs, and was entered
// in the state the line above it ends in, or from root where that line is not colored
EMBED_INLINE int branch_runs_existing(const DocumentBranchCtx *args)
{
    const CliLines *const lines = &args->one_read->lines;
    const unsigned int above = (args->line != 0u) ? lines->ended[args->line - 1u] : CLI_DOC_UNCOLORED;
    return (lines->begun[args->line] != CLI_DOC_UNCOLORED) && (lines->runs[args->line] != CLI_DOC_NO_RUNS) &&
           ((above == CLI_DOC_UNCOLORED) ? (lines->begun[args->line] == CLI_DOC_ROOT)
                                         : (lines->begun[args->line] == above));
}

// the roles that count the depth of a pair
#define CLI_DOC_PAIR_ROLES (CLI_ROLE_DEPTH_OPENS | CLI_ROLE_DEPTH_CLOSES)

// one name of the grammar of the text at `context` its roles read and saved among the text's roles at its place
static void branch_role_read(const CliNameEachCfg *args)
{
    const char *name = args->name;
    CliStoredText *const one = (CliStoredText *)args->context;
    if (args->place < one->roles_capacity)
    {
        one->roles[args->place] =
            CLI_DOC_ROLES_KNOWN | EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = name);
    }
}

// every name of the grammar of `one` its roles read off its segments and saved among the text's roles by its place,
// and above them one more than how many names before it count the depth of a pair, once for the grammar, and a
// coloring copies them as it claims its lines; under the lock
EMBED_INLINE void branch_roles_read(const DocumentBranchCtx *args)
{
    const unsigned int names = (args->one->names != 0u) ? args->one->names : 1u;
    if (args->one->roles_complete || (names >= CLI_DOC_NONE))
    {
        return;
    }
    if (names > args->one->roles_capacity)
    {
        // the roles are read anew over a cell of the names counted
        g_cli_cells->persistent_buf_release(args->one->roles, &g_cli_editor_cells);
        args->one->roles = g_cli_cells->persistent_buf_alloc((unsigned long long)names * sizeof(*args->one->roles),
                                                             &g_cli_editor_cells);
        if (args->one->roles == NULL)
        {
            args->one->roles_capacity = 0u;
            return;
        }
        args->one->roles_capacity = names;
    }
    memor.set(&(MemoriaCfg){.dst = args->one->roles,
                            .val = 0,
                            .bytes = (unsigned long long)args->one->roles_capacity * sizeof(*args->one->roles)});
    (void)EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = args->one->grammar,
                     .each_name_each = branch_role_read, .context = args->one);
    unsigned int rank = 0u;
    for (unsigned int place = 0u; place < args->one->roles_capacity; place += 1u)
    {
        // the rank saved plus 1, a name's place among no more than CLI_DOC_RANKS
        args->one->roles[place] |= (rank < CLI_DOC_RANKS) ? ((rank + 1u) << CLI_DOC_RANK_AT) : 0u;
        rank += ((args->one->roles[place] & CLI_DOC_PAIR_ROLES) != 0u) ? 1u : 0u;
    }
    args->one->roles_complete = 1;
}

// the roles the name at `name` among the names of `leaves` gives a run: those the text saved as the bundle's lines were
// claimed, else read off its segments
EMBED_INLINE unsigned int branch_roles(const DocumentBranchCtx *args)
{
    unsigned int name = args->name;
    return ((name < args->finding->roles_count) && ((args->finding->roles[name] & CLI_DOC_ROLES_KNOWN) != 0u))
               ? (args->finding->roles[name] & (CLI_DOC_ROLES_KNOWN - 1u))
               : EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg,
                            .name = args->leaves->text + args->leaves->name_at[name]);
}

// how many of the names of `leaves` before the name at `name` count the depth of a pair: as the text saved it as the
// bundle's lines were claimed, else counted
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
                  CLI_DOC_PAIR_ROLES) != 0u)
                    ? 1u
                    : 0u;
    }
    return rank;
}

// the regions of one line's leaves recorded as a line's regions are: each run in turn, a region it opens left open
// until a run after it on the line closes it, and one it closes that no run before it on the line opened counted among
// those opened above
// a region a run closes while none is open on its line: the first, where it is a `mark`, marks the line in
// `found_bits`, and it counts among those the line closes and, while no leaf fills the line before it, among those it
// closes unled
EMBED_INLINE void branch_region_unopened(const DocumentBranchCtx *args)
{
    unsigned int *const found = args->found_bits;
    unsigned int *const closes = args->closes_pointer;
    unsigned int *const unled = args->unled_pointer;
    *found |= ((*closes == 0u) && args->mark) ? CLI_DOC_REGION_MARK : 0u;
    *unled += (((*found & CLI_DOC_REGION_FILLED) == 0u) && (*unled < CLI_DOC_REGION_COUNT)) ? 1u : 0u;
    *closes += (*closes < CLI_DOC_REGION_COUNT) ? 1u : 0u;
}

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
        // a leaf is a run its grammar names of a letter or more, the empty name the first
        const int leaf = (name != 0u) && !mark;
        found &= (leaf && ((roles & CLI_ROLE_CLOSING) == 0u)) ? ~CLI_DOC_REGION_CLOSERS : ~0u;
        found |= leaf ? CLI_DOC_REGION_FILLED : 0u;
    }
    found &= (closes != 0u) ? ~0u : ~CLI_DOC_REGION_CLOSERS;
    return found | opens | (closes << CLI_DOC_REGION_CLOSES_AT) | (unled << CLI_DOC_REGION_UNLED_AT);
}

// The names a branch's grammar's tokens take written as the page reads them: the names, then each name that counts the
// depth of a pair followed by `.` and each depth, as the walk names a run of it; the place of the first of those, and
// how many of the names are written
typedef struct
{
    VerbaLinea *out;
    int depths;
    unsigned int written;
} CliBranchNames;

// one name of a grammar passed over, as its names are counted
static void branch_name_pass(const CliNameEachCfg *args)
{
    (void)args;
}

// the names of a text's tokens as they are counted and laid: whether the depths' names come now, how many and their
// bytes so far, and where they are laid, NULL while they are counted
typedef struct
{
    int depths;
    unsigned int count;
    unsigned long long bytes;
    CliDocumentNames *names;
    char *text;
} CliBranchNamesLaid;

// one name counted or laid, `depth` after it where it is a depth's
static void branch_name_lay(CliBranchNamesLaid *laid, const char *name, unsigned long long size, int deep,
                            unsigned int depth)
{
    char suffix[16];
    VerbaLinea line = {suffix, sizeof(suffix), 0ULL};
    if (deep)
    {
        mmgr_line_ch(&line, '.');
        mmgr_line_decimal(&line, depth, 1u);
    }
    const unsigned long long suffix_size = mmgr_line_finish(&line);
    if (laid->names != NULL)
    {
        char *const place = laid->text + laid->bytes;
        memor.cpy(&(MemoriaCfg){.dst = place, .src = name, .bytes = size});
        memor.cpy(&(MemoriaCfg){.dst = place + size, .src = suffix, .bytes = suffix_size});
        place[size + suffix_size] = '\0';
        laid->names->name[laid->count] = place;
    }
    laid->bytes += size + suffix_size + 1ULL;
    laid->count += 1u;
}

// one name of a text's grammar counted or laid; in the second pass one for each depth of a name that counts a pair's
static void branch_name_laid(const CliNameEachCfg *args)
{
    const char *name = args->name;
    CliBranchNamesLaid *const laid = (CliBranchNamesLaid *)args->context;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (!laid->depths)
    {
        branch_name_lay(laid, name, size, 0, 0u);
        return;
    }
    if ((EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = name) & CLI_DOC_PAIR_ROLES) != 0u)
    {
        for (unsigned int depth = 0u; depth < CLI_DOC_DEPTHS; depth += 1u)
        {
            branch_name_lay(laid, name, size, 1, depth);
        }
    }
}

// both passes over the grammar's names, the empty name alone where it gives none
static void branch_names_lay(CliBranchNamesLaid *laid, const char *grammar)
{
    laid->depths = 0;
    if ((grammar[0] == '\0') || (EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg,
                                            .key = grammar, .each_name_each = branch_name_laid, .context = laid) == 0u))
    {
        branch_name_lay(laid, "", 0ULL, 0, 0u);
        return;
    }
    laid->depths = 1;
    (void)EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = grammar,
                     .each_name_each = branch_name_laid, .context = laid);
}

CliDocumentNames *cli_document_names_alloc(const CliDocumentNamesCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return NULL;
    }
    CliBranchNamesLaid counted = {0, 0u, 0ULL, NULL, NULL};
    branch_names_lay(&counted, one->grammar);
    CliDocumentNames *const names = g_cli_cells->persistent_buf_alloc(
        sizeof(*names) + (counted.count * sizeof(*names->name)) + counted.bytes, args->owner);
    if (names != NULL)
    {
        names->name = (const char **)(void *)(names + 1);
        CliBranchNamesLaid laid = {0, 0u, 0ULL, names, (char *)(void *)(names->name + counted.count)};
        branch_names_lay(&laid, one->grammar);
        names->count = laid.count;
    }
    return names;
}

// the names of `scope` saved only where the region each belongs to is at most `depth` deep and, where `waiting` is 1,
// none waits for its region to open, the texts of those saved moved down over those let go
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

// `size` bytes of `text` declared in `scope`, a use of it named `named`, belonging to the region `belongs` deep and
// waiting for it to open where `waits` is 1; passed over where the scope holds no more
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
    // waiting is 0 or 1
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

// the number the digits from `*from` write, `*from` moved past them and past the letter `after` where it follows them,
// never past the NUL that ends them
EMBED_INLINE unsigned int branch_digits(const DocumentBranchCtx *args)
{
    const char **from = args->from_letters;
    unsigned int value = 0u;
    unsigned long long end = 0ULL;
    // a state's digits write a place among the states, which an unsigned int counts
    value = (unsigned int)EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = *from, .at = 0ULL,
                                     .size = cellul.len(&(CatenaFinitaCfg){.src = *from, .cap = CLI_CELLS_POOL}),
                                     .end = &end);
    *from += end;
    *from += (((*from)[0] == args->after) && (args->after != '\0')) ? 1 : 0;
    return value;
}

// the scope written at the place `place` among the states of `one` read into `scope`, as far as each name's text
// stands within it, its place saved; empty where `place` is CLI_DOC_NONE or past the states
EMBED_INLINE void branch_scope_read(const DocumentBranchCtx *args)
{
    unsigned int place = args->place;
    args->scope->depth = 0u;
    args->scope->pairs = 0u;
    args->scope->count = 0u;
    args->scope->used = 0u;
    args->scope->place = CLI_DOC_NONE;
    args->scope->moved = 1;
    if ((place == CLI_DOC_NONE) || (place >= args->one_read->state_used))
    {
        return;
    }
    const char *at = args->one_read->state + place;
    args->scope->depth = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &at, .after = '.');
    args->scope->pairs = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &at, .after = '\0');
    while (at[0] == ';')
    {
        at += 1;
        const unsigned int named = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &at, .after = '.');
        const unsigned int belongs = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &at, .after = '.');
        const int waits = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &at, .after = '.') != 0u;
        const unsigned int size = EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &at, .after = ':');
        if (cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}) < size)
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

// the scope the line `finding` colored last ends in, written into `out` where its place among the text's states is to
// be found as it is put in, written again only where it moved since the one before it was: its bytes, CLI_DOC_NONE for
// a scope with no depth, pairs or names or one too large to write, and CLI_DOC_SCOPE_SAME for the one before it
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

// one line's leaves walked through the scope of `finding`: a region its runs open deepens it and its function's
// parameters waiting
// for it open in it, one they close lets go the names that belong to it, a local is declared in the region it stands
// in and a parameter in the one the next region opens, a statement's end or a directive lets go the parameters still
// waiting, at once and at the line's end; each bare name a use of the last declaration of it in the scope takes that
// declaration's name, but one reached through the operator before it, blanks between; and a pair it opens deepens the
// scope's pairs and one it closes lets one go, each run that opens or closes one named by the depth it stands at, the
// first of its names' `names` on, as `cli_document_names_write` writes them. The leaves with each run's name so
// taken, in the work's capacity for them, the leaves as they came where the cells cannot meet that capacity
// the scope moved by a run whose roles are `roles`: a region it opens entered, the names that wait for it entering
// with it, or a region it closes left
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

// a run named `named` whose roles are `roles`, its `size` bytes at `text`: a parameter put in the scope of the region
// it opens, a local in the scope it stands in, and where `renames`, any other name read through the scope: the name
// the run takes, `named` where the scope gives it none
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

// the depth of the pairs moved by a run named `name` whose roles are `roles`: the name a pair takes after the `names`
// the language gives, by its rank and its depth, where `renames` and the language ranks it, else CLI_DOC_NONE. A pair
// that closes stands at the depth of the one it closes
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
        (rank != CLI_DOC_NONE) ? args->names + (rank * CLI_DOC_DEPTHS) + (scope->pairs % CLI_DOC_DEPTHS) : CLI_DOC_NONE;
    scope->pairs += opens ? 1u : 0u;
    scope->moved = scope->moved || closes || opens;
    return paired;
}

EMBED_INLINE CliLeaves branch_uses_by_name(const DocumentBranchCtx *args)
{
    CliLeaves named = *args->leaves;
    CliBranchScope *const scope = &args->finding_out->scope;
    const unsigned int names = args->finding_out->names;
    const int renaming = (args->leaves->runs != 0u) && (args->leaves->runs <= args->finding_out->named_capacity);
    unsigned int *const renamed = args->finding_out->named;
    const char *const line = args->leaves->line;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
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
        // the empty name, a blank's, stands between an operator and the name it reaches
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

// one line's leaves, colored with no lock taken: its runs named through the scope, its regions read, and the state the
// colorer leaves it in and the scope it ends in written after what the bundle found on the lines before it, a record of
// its own; nothing where the cells refused the capacity for one before it or refuse it
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
    // the colorer's state, no longer than the capacity it is written in, which an unsigned int counts
    const unsigned int ended =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = leaves->ended, .cap = CLI_CELLS_POOL});
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

// the lines from `first` to before `end` of `one` claimed by a coloring under way: the claim's place among those of the
// text, CLI_DOC_CLAIMS where every place is taken with none claimed
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

// what `finding` found on the lines from `first` put into `one` in turn, from the state `*entering` the first is
// entered in and the place `place` among the text's states of the scope it is entered in: each line's state it is
// entered in and the state it ends in, its regions, its runs saved, a paged text's where the page holds the line or the
// range hands it on, and handed on where the range hands it on, each no further than the text's columns where the range
// names them; where `stopping` is 1, stopped at the first line entered in the state it was entered in before,
// `*stopped` set. The line after the last put; under the lock
// the state a line ends in, of the record the bundle found for it at `record`, its colorer's state at `text`: the
// colorer's with the place of its scope after the mark, the place read where the record writes the scope and else the
// one `read` gives, saved back into `read`; the colorer's alone where the scope has no place or the two do not fit. The
// state's place among the text's
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

EMBED_INLINE unsigned int branch_found_append(const DocumentBranchCtx *args)
{
    unsigned int *entering = args->entering;
    int *stopped = args->stopped;
    CliLines *const lines = &args->one->lines;
    const unsigned int hand_from = args->range->requested ? args->range->hand_from : args->one->cached_from;
    const unsigned int hand_to = args->range->requested ? args->range->hand_to : args->one->cached_to;
    args->range->columns = (args->range->columns != 0ULL) ? args->one->columns : 0ULL;
    const unsigned int *record = args->finding->found;
    const unsigned int *const last = record + args->finding->used;
    unsigned int line = args->first;
    unsigned int scoped = args->place;
    while ((record < last) && (line < lines->lines) && !args->one->refused)
    {
        if (args->stopping && (lines->begun[line] == *entering))
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
        lines->begun[line] = *entering;
        lines->ended[line] = EMBED_CALL(branch_state_ended, DocumentBranchCtx, .one = args->one, .record = record,
                                        .text = ended, .read = &scoped);
        args->one->regions_moved = args->one->regions_moved || (lines->regions[line] != record[1]);
        lines->regions[line] = record[1];
        const CliLeaves saved = {NULL, runs, found_col, found_col + runs, NULL, NULL, NULL, record[4]};
        const int transferred = (line >= hand_from) && (line < hand_to);
        const int cached = (line >= args->one->cached_from) && (line < args->one->cached_to);
        if (!args->one->paged || cached || transferred)
        {
            EMBED_CALL(branch_runs_save, DocumentBranchCtx, .one = args->one, .line = line, .leaves = &saved);
        }
        else
        {
            lines->runs[line] = CLI_DOC_NO_RUNS;
        }
        if (transferred)
        {
            EMBED_CALL(branch_runs_append, DocumentBranchCtx, .range = args->range, .line = line, .leaves = &saved);
        }
        *entering = lines->ended[line];
        line += 1u;
        record += CLI_DOC_FOUND_HEAD + (2u * runs) + ended_words + (written ? ((scope_size + 3u) / 4u) : 0u);
    }
    return line;
}

// the state the line `first` is entered in, the text's at `entering`, copied into `text_out`, CLI_DOC_STATE bytes, cut
// at its mark, and the scope at the place after the mark read into the finding's: whether the state was copied whole
EMBED_INLINE int branch_state_entered(const DocumentBranchCtx *args)
{
    char *const state = args->text_out;
    CliBranchFinding *const finding = args->finding_out;
    VerbaLinea entered = {state, CLI_DOC_STATE, 0ULL};
    mmgr_line_put(&entered, args->one_read->state + *args->entering);
    const int stated = mmgr_line_finish(&entered) != 0ULL;
    const char *const mark =
        stated ? cellul.chr(&(CatenaFinitaCfg){.src = state, .cap = CLI_CELLS_POOL, .byte = CLI_DOC_SCOPE_MARK}) : NULL;
    const char *place = (mark != NULL) ? (mark + 1) : NULL;
    EMBED_CALL(branch_scope_read, DocumentBranchCtx, .one_read = args->one_read,
               .place = (place != NULL)
                            ? EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &place, .after = '\0')
                            : CLI_DOC_NONE,
               .scope = &finding->scope);
    finding->positioned = finding->scope.place != CLI_DOC_NONE;
    if (mark != NULL)
    {
        // the mark stands within the state copied
        state[mark - state] = '\0';
    }
    return stated;
}

// the lines of `line_letters` from `first` to before `to` the range's bytes reach counted, one at the least, the bytes
// spent taken from the range's where it counts them; their bytes, each with its newline, into `units_pointer`, and
// the longest's into `most_pointer`
EMBED_INLINE unsigned int branch_lines_spent(const DocumentBranchCtx *args)
{
    const char **const line = args->line_letters;
    CliBranchRange *const range = args->range;
    unsigned int spend = 0u;
    unsigned long long spent = 0ULL;
    while ((line != NULL) && ((args->first + spend) < args->to) &&
           ((spend == 0u) || (range->bytes == CLI_DOC_ALL_BYTES) || (spent < range->bytes)))
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = line[spend], .cap = CLI_CELLS_POOL});
        spent += (range->bytes != CLI_DOC_ALL_BYTES) ? (length + 1ULL) : 0ULL;
        *args->units_pointer += length + 1ULL;
        *args->most_pointer = (length > *args->most_pointer) ? length : *args->most_pointer;
        spend += 1u;
    }
    if (range->bytes != CLI_DOC_ALL_BYTES)
    {
        range->bytes -= (spent < range->bytes) ? spent : range->bytes;
    }
    return spend;
}

// one bundle of the lines from `first` to before `to` of `one` colored from the state `*entering` the first is entered
// in: no more than CLI_DOC_BATCH lines where `stopping` is 1, else CLI_DOC_RUN, CLI_DOC_STEP where the range's bytes
// reach no further, and of those as many as its bytes reach, one at the least, those spent taken from them. The lines
// are claimed under the lock, read and colored in `work` with it given back, and put in under it again, the text saved
// across it by its caller; where the text moved meanwhile none is put in and the range is marked stale, and where the
// grammar or the cells refused them none is and the text is marked refused. The line after the last put; under the lock
EMBED_INLINE unsigned int branch_batch_color(const DocumentBranchCtx *args)
{
    unsigned int to = args->to;
    unsigned int most = CLI_DOC_BATCH;
    if (!args->stopping)
    {
        most = (args->range->bytes != CLI_DOC_ALL_BYTES) ? CLI_DOC_STEP : CLI_DOC_RUN;
    }
    const unsigned int end = ((to - args->first) > most) ? (args->first + most) : to;
    // what the bundle reads with the lock given back, copied as its lines are claimed: the text's serial, its key and
    // its grammar's, whether it is paged and whether its last line ends it, and how many lines it counts
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
    // the state the first line is entered in, copied out, the colorer's own handed to it and the scope at the place
    // after the mark read into the bundle
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
    char *const cell =
        g_cli_cells->temporary_buf_alloc(starts_bytes + roles_bytes + text_capacity, &g_cli_editor_cells);
    const char **line = (const char **)(void *)cell;
    finding.roles = NULL;
    finding.roles_count = 0u;
    if (cell != NULL)
    {
        finding.roles = (const unsigned int *)(void *)(cell + starts_bytes);
        finding.roles_count = roles;
        memor.cpy(&(MemoriaCfg){.dst = cell + starts_bytes, .src = args->one->roles, .bytes = roles_bytes});
        if (!paged)
        {
            EMBED_CALL(branch_lines_write, DocumentBranchCtx, .lines = &args->one->lines, .from = args->first,
                       .to = read, .text_out = cell + starts_bytes + roles_bytes, .capacity = text_capacity,
                       .line_letters = line);
        }
    }
    const unsigned int claim =
        EMBED_CALL(branch_claim_append, DocumentBranchCtx, .one = args->one, .first = args->first, .end = end);
    char *reserved = NULL;
    if (paged && (cell != NULL))
    {
        unsigned long long inner_count = 0ULL;
        reserved = EMBED_CALL(cli_pages_from.lines_alloc, CliPagesFromCfg, .key = key, .from = args->first, .to = end,
                              .line = line, .count = &inner_count, .owner = &g_cli_editor_cells);
        // no more lines are read than were asked, which an unsigned int counts
        read = args->first + (unsigned int)inner_count;
    }
    line = (paged && (reserved == NULL)) ? NULL : line;
    // the lines read colored as far as the range's bytes reach, one at the least
    unsigned long long total = 0ULL;
    unsigned long long longest = 0ULL;
    const unsigned int spend =
        EMBED_CALL(branch_lines_spent, DocumentBranchCtx, .line_letters = line, .first = args->first, .to = read,
                   .range = args->range, .units_pointer = &total, .most_pointer = &longest);
    // the names of a line's runs, one run for each of its bytes at the most, and the records: two words for each run, a
    // record's head and CLI_DOC_FOUND_STATE words of its state for each line, and the words of one state and scope
    // whole. The lines past them are the next bundle's
    finding.named_capacity = longest + 2ULL;
    finding.found_capacity = (2ULL * total) + ((unsigned long long)spend * (CLI_DOC_FOUND_HEAD + CLI_DOC_FOUND_STATE)) +
                             (CLI_DOC_STATE / 2ULL);
    const unsigned long long named_bytes = finding.named_capacity * sizeof(unsigned int);
    char *const found_cell =
        (spend != 0u) ? g_cli_cells->temporary_buf_alloc(named_bytes + (finding.found_capacity * sizeof(unsigned int)),
                                                         &g_cli_editor_cells)
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
        colored =
            EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = grammar, .state = stated ? state : "root",
                       .lines = line, .count = spend, .ends = ((args->first + spend) == count) && whole,
                       .each = branch_find, .context = &finding, .ended = &last, .err_msg = &err_msg);
    }
    g_cli_cells->persistent_buf_release(reserved, &g_cli_editor_cells);
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
    g_cli_cells->persistent_buf_release(found_cell, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(cell, &g_cli_editor_cells);
    return put;
}

// the lines from `from` to `to` of `one` colored, the first entered in the state the line above it ends in, root where
// it is the first or the line above is not colored; where `stopping` is 1, stopped at the first entered in the state it
// was entered in before, each line colored whole from left to right, and the lines those the range's bytes reach, the
// last maybe past them, those spent taken from them, a bundle at a time in a work claimed for them. The runs of those
// the range hands on handed on in it, each line's no further than the range's columns where it names them: the line
// after the last colored. Under the lock, which it gives back as each bundle is colored, the text saved across it by
// its caller
EMBED_INLINE unsigned int branch_color(const DocumentBranchCtx *args)
{
    const unsigned int above = (args->from != 0u) ? args->one->lines.ended[args->from - 1u] : CLI_DOC_UNCOLORED;
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

// whether the line `line` of `one` is one to color: it is not colored, and where `yielding` is 1, as the walk yields,
// no coloring under way claims it
EMBED_INLINE int branch_uncolored(const DocumentBranchCtx *args)
{
    return (args->one_read->lines.begun[args->line] == CLI_DOC_UNCOLORED) && !(args->yielding && branch_claim(args));
}

// the run of lines of `one` to color about `at`, below the walk's origin where `down` is 1: its first and the line
// after its last into `first` and `end`, no more than CLI_DOC_STEP of them reaching from `at`, down or up, none outside
// the lines from `low` to before `high` the walk colors, and where `yielding` is 1 none a coloring under way claims; a
// run down whose line above ends in no known state reaching as far above as CLI_DOC_PREFIX lines to color reach
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

// the lines of `one` the walk colors, from `*low` to before `*high`: a paged text's those the page holds alone, the
// first of them entered from root, and every line of any other
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

// a run of lines handed on in `out` begun: their runs written as `{"from", "runs"}`, after a comma where `*any` is set,
// and `*any` set where they are, each line's no further than `columns` letters where it is not 0, no more lines than
// `bytes` reach, those the page holds handed on, and none colored past `high`
EMBED_INLINE CliBranchRange branch_range_begin(const DocumentBranchCtx *args)
{
    const CliBranchRange range = {
        0,  0u, args->any_pointer, args->out, args->columns, args->one_read->names, args->bytes, 0u, 0, 0,
        0u, 0u, args->high};
    return range;
}

// the lines from `first` to `end` of `one` colored in `range` and the lines after them colored again until one is
// entered in the state it was entered in before, no more than CLI_DOC_STEP of those and none past the range's last;
// where its columns are not 0, the lines after them left to the walk unless `onward` is 1, as the walk's own step
// colors them. Under the lock, which each bundle gives back as it is colored, the range marked stale where the text
// moved
EMBED_INLINE void branch_range_color(const DocumentBranchCtx *args)
{
    const unsigned int line = EMBED_CALL(branch_color, DocumentBranchCtx, .one = args->one, .from = args->first,
                                         .to = args->end, .stopping = 0, .range = args->range);
    const unsigned int high = args->range->high;
    if ((args->onward || (args->range->columns == 0ULL)) && !args->range->stale && (line == args->end) &&
        (args->end < high) && (args->end < args->one->lines.lines) &&
        (args->one->lines.begun[args->end] != CLI_DOC_UNCOLORED))
    {
        const unsigned int most = ((high - args->end) > CLI_DOC_STEP) ? (args->end + CLI_DOC_STEP) : high;
        (void)EMBED_CALL(branch_color, DocumentBranchCtx, .one = args->one, .from = args->end, .to = most,
                         .stopping = 1, .range = args->range);
    }
}

// one step of the walk of `one`: the run of lines to color nearest its origin colored, or where every line is colored
// or claimed, the first entered in a state the line above does not end in colored again, written into `out` as
// `{"key", "seq", "colors"}`, `*handed` set where it holds the runs of a line the page holds: 1 where it colored a
// line, else 0. A paged text's lines are counted again first, and it is walked across the lines the page holds alone.
// Under the lock, which it gives back as it colors, the text saved across it by its caller
// the first line from `low` to before `high`, past `low` where it is not the text's first, entered in another state
// than the one the line above it ends in and claimed by no work: CLI_DOC_NONE where none is
EMBED_INLINE unsigned int branch_entered_astray(const DocumentBranchCtx *args)
{
    const CliLines *const lines = &args->one_read->lines;
    for (unsigned int line = (args->low != 0u) ? (args->low + 1u) : 0u; line < args->high; line += 1u)
    {
        if ((lines->begun[line] != ((line == 0u) ? CLI_DOC_ROOT : lines->ended[line - 1u])) &&
            !EMBED_CALL(branch_claim, DocumentBranchCtx, .one_read = args->one_read, .line = line))
        {
            return line;
        }
    }
    return CLI_DOC_NONE;
}

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
    return 1;
}

// the mark of a fold that shows on its first line the line it closes on, after what it hides
#define CLI_DOC_FOLD_DISPLAYED 0x80000000u

// the fold of the region opened on the line `opened` that the line `line` closes, the `close`th region it closes, the
// line's regions `regions`, the last line above it that holds a leaf `filled`: the last line it hides, plus 1, marked
// CLI_DOC_FOLD_DISPLAYED where it shows the line it closes on, or 0 where it hides no line. A mark of no letters closes
// a region on the line it ends with a leaf before it, else on the last line above that holds one; a bracket closes it
// on the line before its own where a leaf stands before it or another leaf on its line closes nothing, and on its own,
// shown, where its line holds closes alone
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
    // a line shown on the fold's first line is no line hidden
    const unsigned int hidden = displayed ? (last - 1u) : last;
    if ((last == CLI_DOC_NONE) || (hidden <= args->opened))
    {
        return 0u;
    }
    return (last + 1u) | (displayed ? CLI_DOC_FOLD_DISPLAYED : 0u);
}

// The regions open above a line as its folds are found: the line each opens on, the innermost last, in the capacity the
// walk saves for them, how many, and whether the cells refused it
// the folds of `one` found, each handed to `each` with its first line, the last line it hides and whether it shows the
// line it closes on after what it hides: each fold the widest of the regions that open on its first line, in the order
// of their first lines; a paged text's those of the lines the page mirrors alone, and none where the cells cannot meet
// the capacity they are found in. Under the texts' lock
static void branch_folds_each(const DocumentBranchCtx *args)
{
    const CliLines *const lines = &args->one_read->lines;
    unsigned int first = 0u;
    unsigned int end = 0u;
    EMBED_CALL(branch_bounds, DocumentBranchCtx, .one_read = args->one_read, .low_pointer = &first,
               .high_pointer = &end);
    const unsigned int saved = end - first;
    // the widest fold of each line and the regions open above a line, no more than the regions they open
    unsigned long long opening = 0ULL;
    for (unsigned int line = first; line < end; line += 1u)
    {
        opening += lines->regions[line] & CLI_DOC_REGION_COUNT;
    }
    const unsigned long long widest_bytes = (unsigned long long)saved * sizeof(unsigned int);
    unsigned int *const widest =
        (saved != 0u)
            ? g_cli_cells->temporary_buf_alloc(widest_bytes + (opening * sizeof(unsigned int)), &g_cli_editor_cells)
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
        const unsigned int regions = lines->regions[line];
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
    g_cli_cells->persistent_buf_release(widest, &g_cli_editor_cells);
}

int cli_document_folds_each(const CliDocumentContextCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one != NULL)
    {
        EMBED_CALL(branch_folds_each, DocumentBranchCtx, .one_read = one, .each = args->each_fold_each,
                   .context = args->context);
    }
    return one != NULL;
}

unsigned int cli_document_openers(const CliDocumentKeyLineCfg *args)
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
        opening += one->lines.regions[at] & CLI_DOC_REGION_COUNT;
    }
    unsigned int *const opened_on =
        (opening != 0ULL) ? g_cli_cells->temporary_buf_alloc(opening * sizeof(unsigned int), &g_cli_editor_cells)
                          : NULL;
    unsigned int open = 0u;
    for (unsigned int at = 0u; (opened_on != NULL) && (at < end); at += 1u)
    {
        const unsigned int regions = one->lines.regions[at];
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
    g_cli_cells->persistent_buf_release(opened_on, &g_cli_editor_cells);
    return count;
}

// whether the folds of `one` go on: its regions moved since they last went, every line of it colored, the walk's step
// over it having colored none and no coloring under way claiming any, and the page holding lines of it
EMBED_INLINE int branch_folds_ready(const DocumentBranchCtx *args)
{
    return args->one_read->regions_moved && !args->one_read->refused &&
           (args->one_read->cached_to > args->one_read->cached_from) && (args->one_read->lines.lines != 0u) &&
           !branch_claims_any(args);
}

// the walk taken by `walker` step by step until no text it may walk has a line to color: each step on a text no
// walker walks and the walk did not find colored whole since it last moved, whoever `colored` names told once a step
// colored a line that is read; on a walker's own thread where `own` is 1, marked as walking no more as it ends
static void branch_walk(const DocumentBranchCtx *args)
{
    for (;;)
    {
        CliStoredText *one = cli_document_texts();
        while ((one != NULL) && (one->walked || one->resting))
        {
            one = one->next;
        }
        if (one == NULL)
        {
            args->walker->walking = args->own ? 0 : args->walker->walking;
            return;
        }
        int transferred = 0;
        one->walked = 1;
        EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
        int stepped = EMBED_CALL(branch_step, DocumentBranchCtx, .one = one, .transferred = &transferred);
        // a text the step colored none of is colored whole: its folds move on where its regions moved
        if (!stepped && EMBED_CALL(branch_folds_ready, DocumentBranchCtx, .one_read = one))
        {
            one->regions_moved = 0;
            stepped = 1;
            transferred = 1;
        }
        one->resting = !stepped;
        one->walked = 0;
        EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
        const CliDocColored colored = s_colored;
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
    // the walkers walking and the first free, the texts a walker walks now, and those that wait for one
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
        walker->thread = (CliThread){branch_walk_run, walker, CLI_PRIORITY_WORK};
        walker->walking = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &walker->thread);
    }
}

void cli_document_colors_to(const CliDocumentColoredCfg *args)
{
    const unsigned int reserve = cli_processors() - 1u;
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

int cli_document_colored_by(const CliDocumentKeyGrammarCfg *args)
{
    int named = args->named;
    // the names the grammar's tokens take counted before the texts' lock is taken
    const unsigned int names =
        (args->grammar[0] != '\0')
            ? EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = args->grammar,
                         .each_name_each = branch_name_pass, .context = NULL)
            : 0u;
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->grammar, .cap = CLI_CELLS_POOL});
    if ((one != NULL) && (length < CLI_NAME_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = one->grammar, .src = args->grammar, .bytes = length + 1ULL});
        EMBED_CALL(cli_document_internal.serial_move, CliDocumentCtx, .one = one);
        one->names = names;
        one->state_used = 0u;
        one->state_count = 0u;
        if (one->state_indexed != 0u)
        {
            memor.set(&(MemoriaCfg){
                .dst = one->state_index, .val = 0, .bytes = one->state_indexed * sizeof(*one->state_index)});
        }
        one->refused = 0;
        one->named = named;
        (void)EMBED_CALL(branch_state, DocumentBranchCtx, .one = one, .text = "root");
        for (unsigned int line = 0u; line < one->lines.lines; line += 1u)
        {
            one->lines.begun[line] = CLI_DOC_UNCOLORED;
            one->lines.ended[line] = CLI_DOC_UNCOLORED;
            one->lines.regions[line] = 0u;
            one->lines.runs[line] = CLI_DOC_NO_RUNS;
        }
        one->run_used = 0u;
        // the roles saved were the last grammar's names', read again for this one as its lines are first claimed
        one->roles_complete = 0;
        one->regions_moved = 1;
        EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
    }
    return one != NULL;
}

// the lines the view mirrors moved to those from `from` to before `to`: a paged text's lines the view lets go let
// their runs go with them, and a line the view comes to mirror that saves no runs that stand is colored again, the
// walk coming to it
EMBED_INLINE void branch_cache_moved(const DocumentBranchCtx *args)
{
    CliStoredText *const one = args->one;
    CliLines *const lines = &one->lines;
    const unsigned int was_from = one->cached_from;
    const unsigned int was_to = (one->cached_to < lines->lines) ? one->cached_to : lines->lines;
    for (unsigned int line = was_from; one->paged && (line < was_to); line += 1u)
    {
        lines->runs[line] = ((line < args->from) || (line >= args->to)) ? CLI_DOC_NO_RUNS : lines->runs[line];
    }
    for (unsigned int line = args->from; line < args->to; line += 1u)
    {
        const int passed = (line >= was_from) && (line < was_to);
        if (!passed && !EMBED_CALL(branch_runs_existing, DocumentBranchCtx, .one_read = one, .line = line))
        {
            lines->begun[line] = CLI_DOC_UNCOLORED;
        }
    }
}

int cli_document_cache(const CliDocumentKeyFromCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one != NULL)
    {
        one->refused =
            one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
        const CliLines *const lines = &one->lines;
        // the lines the view mirrors stand among those the pages count, which an unsigned int counts
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

// the lines from `first` to before `end` of `one` walked where they save no runs that stand, their columns cut at
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

// the `capacity` lines `number` names walked, those that follow one another together, each cut at `columns` where it
// is not 0, the lines the frame mirrors widened to them where `mirrored`: the words their runs take, counted off the
// records
EMBED_INLINE unsigned long long branch_runs_walked(const DocumentBranchCtx *args)
{
    CliStoredText *const one = args->one;
    const CliLines *const lines = &one->lines;
    unsigned long long words = 0ULL;
    for (unsigned long long at = 0ULL; at < args->capacity;)
    {
        const unsigned long long end = EMBED_CALL(branch_lines_following, DocumentBranchCtx, .number = args->number,
                                                  .capacity = args->capacity, .word_at = at);
        // the lines of a text stand among those it counts, which an unsigned int counts
        const unsigned int from = (args->number[at] < lines->lines) ? (unsigned int)args->number[at] : lines->lines;
        const unsigned int to =
            (args->number[end - 1ULL] < lines->lines) ? ((unsigned int)args->number[end - 1ULL] + 1u) : lines->lines;
        EMBED_CALL(branch_lines_walk, DocumentBranchCtx, .one = one, .first = from, .end = to,
                   .columns = args->columns);
        for (unsigned int line = from; line < to; line += 1u)
        {
            words += (lines->runs[line] != CLI_DOC_NO_RUNS) ? (2ULL * one->run[lines->runs[line]]) : 0ULL;
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

// the runs of the `capacity` lines `number` names put in `runs_out`, each line's columns, then its names, those at or
// past `columns` left off where it is not 0
EMBED_INLINE void branch_runs_put(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const CliLines *const lines = &one->lines;
    CliDocumentRuns *const read = args->runs_out;
    unsigned int used = 0u;
    for (unsigned long long at = 0ULL; at < args->capacity; at += 1ULL)
    {
        read->line[at] = (CliDocumentLineRuns){used, 0u};
        const unsigned long long line = args->number[at];
        if ((line >= lines->lines) || (lines->runs[line] == CLI_DOC_NO_RUNS))
        {
            continue;
        }
        const unsigned int *const record = one->run + lines->runs[line];
        const unsigned int *const col = record + CLI_DOC_RECORD_HEAD;
        unsigned int runs = record[0];
        while ((args->columns != 0ULL) && (runs > 1u) && (col[runs - 1u] >= args->columns))
        {
            runs -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = read->word_text + used, .src = col, .bytes = runs * sizeof(*col)});
        memor.cpy(
            &(MemoriaCfg){.dst = read->word_text + used + runs, .src = col + record[0], .bytes = runs * sizeof(*col)});
        read->line[at].runs = runs;
        used += 2u * runs;
    }
}

CliDocumentRuns *cli_document_runs_alloc(const CliDocumentRunsCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return NULL;
    }
    EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
    one->refused =
        one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
    const unsigned long long columns = args->mirrored ? one->columns : 0ULL;
    const unsigned long long words =
        EMBED_CALL(branch_runs_walked, DocumentBranchCtx, .one = one, .number = args->number, .capacity = args->count,
                   .columns = columns, .mirrored = args->mirrored);
    const unsigned long long line_bytes = args->count * sizeof(CliDocumentLineRuns);
    CliDocumentRuns *const read =
        g_cli_cells->persistent_buf_alloc(sizeof(*read) + line_bytes + (words * sizeof(unsigned int)), args->owner);
    if (read != NULL)
    {
        read->line = (CliDocumentLineRuns *)(void *)(read + 1);
        read->word_text = (unsigned int *)(void *)(read->line + args->count);
        EMBED_CALL(branch_runs_put, DocumentBranchCtx, .one_read = one, .runs_out = read, .number = args->number,
                   .capacity = args->count, .columns = columns);
    }
    EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    return read;
}

// the bars of the line a record saves laid on the map: a run with a name a bar, the empty name a gap, and the gap that
// opens the line `tab` cells a unit
EMBED_INLINE void branch_map_line(const DocumentBranchCtx *args)
{
    CliDocumentMap *const map = args->map;
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
            map->bar[map->bars] = (CliDocumentMapBar){cell, width, name[run]};
            map->bars += 1u;
            cell += width;
        }
    }
}

CliDocumentMap *cli_document_map_alloc(const CliDocumentMapCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return NULL;
    }
    EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
    one->refused =
        one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
    const CliLines *const lines = &one->lines;
    // the lines asked stand among those the text counts, which an unsigned int counts
    const unsigned int from = (args->first < lines->lines) ? (unsigned int)args->first : lines->lines;
    const unsigned int to = (args->last < lines->lines) ? ((unsigned int)args->last + 1u) : lines->lines;
    EMBED_CALL(branch_lines_walk, DocumentBranchCtx, .one = one, .first = from, .end = to, .columns = 0ULL);
    // a bar for each run at the most and one for a line with none, counted off the records
    unsigned long long most = 0ULL;
    for (unsigned int line = from; line < to; line += 1u)
    {
        most += (lines->runs[line] != CLI_DOC_NO_RUNS) ? one->run[lines->runs[line]] : 1ULL;
    }
    const unsigned int count = to - from;
    const unsigned long long line_bytes = (count + 1ULL) * sizeof(unsigned int);
    CliDocumentMap *const map =
        g_cli_cells->persistent_buf_alloc(sizeof(*map) + line_bytes + (most * sizeof(CliDocumentMapBar)), args->owner);
    if (map != NULL)
    {
        map->from = from;
        map->count = count;
        map->bars = 0u;
        map->line_at = (unsigned int *)(void *)(map + 1);
        map->bar = (CliDocumentMapBar *)(void *)(map->line_at + count + 1u);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            map->line_at[at] = map->bars;
            const unsigned int line = from + at;
            if (lines->runs[line] != CLI_DOC_NO_RUNS)
            {
                EMBED_CALL(branch_map_line, DocumentBranchCtx, .map = map, .tab = args->tab,
                           .record = one->run + lines->runs[line]);
            }
            else if (!one->paged && (lines->length[line] != 0ULL))
            {
                // the units of a line a map shows are far inside what an unsigned int represents
                map->bar[map->bars] = (CliDocumentMapBar){0u, (unsigned int)lines->length[line], 0u};
                map->bars += 1u;
            }
        }
        map->line_at[count] = map->bars;
    }
    EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    return map;
}

// whether the `length` bytes at `word_at` among the words' text stand in the table of `capacity` entries, a power of
// two, put there where they do not: 1 where they stand, each entry the word's start plus one and its length, 0 where
// free
EMBED_INLINE int branch_word_seen(const DocumentBranchCtx *args)
{
    const char *const text = args->words->text;
    // the table's entries are a power of two an unsigned int counts
    const unsigned int mask_bits = (unsigned int)args->capacity - 1u;
    unsigned int entry =
        EMBED_CALL(branch_state_hash, DocumentBranchCtx, .text = text + args->word_at, .length = args->length) &
        mask_bits;
    for (;;)
    {
        unsigned int *const standing = args->word_table[entry];
        if (standing[0] == 0u)
        {
            // a place among the words' text and a word's length are far inside what an unsigned int represents
            standing[0] = (unsigned int)args->word_at + 1u;
            standing[1] = (unsigned int)args->length;
            return 0;
        }
        if ((standing[1] == args->length) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = text + standing[0] - 1u, .other = text + args->word_at, .bytes = args->length}) == 0))
        {
            return 1;
        }
        entry = (entry + 1u) & mask_bits;
    }
}

// the units of the run `run` of a record where it is a word to give, 0 where it is not: its name's roles name
// something, it has two units or more and the cursor stands neither in it nor at its end
EMBED_INLINE unsigned int branch_word_units(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const unsigned int *const record = args->record;
    const unsigned int runs = record[0];
    const unsigned int *const col = record + CLI_DOC_RECORD_HEAD;
    const unsigned int name = col[runs + args->name];
    const unsigned int roles = (name < one->roles_capacity) ? one->roles[name] : 0u;
    const unsigned int from = col[args->name];
    const unsigned int after = ((args->name + 1u) < runs) ? col[args->name + 1u] : record[1];
    const int typed = (args->cursor_line == args->line) && (from <= args->cursor_col) && (args->cursor_col <= after);
    return (((roles & CLI_ROLE_WORD) != 0u) && ((after - from) >= 2u) && !typed) ? (after - from) : 0u;
}

// the words of the text's lines from `from` to before `to` counted into `most_pointer`, and their units into
// `units_pointer`, the word typed at the cursor left out; a paged text gives none
EMBED_INLINE void branch_words_count(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const CliLines *const lines = &one->lines;
    for (unsigned int line = args->from; !one->paged && (line < args->to); line += 1u)
    {
        const unsigned int place = lines->runs[line];
        for (unsigned int run = 0u; (place != CLI_DOC_NO_RUNS) && (run < one->run[place]); run += 1u)
        {
            const unsigned int taken =
                EMBED_CALL(branch_word_units, DocumentBranchCtx, .one_read = one, .record = one->run + place,
                           .name = run, .line = line, .cursor_line = args->cursor_line, .cursor_col = args->cursor_col);
            *args->most_pointer += (taken != 0u) ? 1ULL : 0ULL;
            *args->units_pointer += taken;
        }
    }
}

// the words of the text's lines from `from` to before `to` put in `words`, each its units' bytes, three a unit at the
// most, and each once, `word_table` of `capacity` entries telling those put; the word typed at the cursor left out
EMBED_INLINE void branch_words_fill(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const CliLines *const lines = &one->lines;
    CliDocumentWords *const words = args->words;
    unsigned long long used = 0ULL;
    for (unsigned int line = args->from; !one->paged && (line < args->to); line += 1u)
    {
        const unsigned int place = lines->runs[line];
        for (unsigned int run = 0u; (place != CLI_DOC_NO_RUNS) && (run < one->run[place]); run += 1u)
        {
            const unsigned int *const record = one->run + place;
            const unsigned int taken =
                EMBED_CALL(branch_word_units, DocumentBranchCtx, .one_read = one, .record = record, .name = run,
                           .line = line, .cursor_line = args->cursor_line, .cursor_col = args->cursor_col);
            if (taken == 0u)
            {
                continue;
            }
            VerbaLinea written = {words->text + used, 3ULL * taken, 0ULL};
            EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &written,
                       .units = lines->unit + lines->start[line] + record[CLI_DOC_RECORD_HEAD + run], .count = taken);
            if (!EMBED_CALL(branch_word_seen, DocumentBranchCtx, .words = words, .word_table = args->word_table,
                            .capacity = args->capacity, .word_at = used, .length = written.at))
            {
                words->at[words->count] = used;
                // a word's bytes are far inside what an unsigned int represents
                words->length[words->count] = (unsigned int)written.at;
                used += written.at;
                words->count += 1u;
            }
        }
    }
}

CliDocumentWords *cli_document_words_alloc(const CliDocumentWordsCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if (one == NULL)
    {
        return NULL;
    }
    EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
    const CliLines *const lines = &one->lines;
    // the lines asked stand among those the text counts, which an unsigned int counts
    const unsigned int from = (args->first < lines->lines) ? (unsigned int)args->first : lines->lines;
    const unsigned int to = (args->last < lines->lines) ? ((unsigned int)args->last + 1u) : lines->lines;
    // the words and their bytes at the most, three a unit, counted off the records
    unsigned long long most = 0ULL;
    unsigned long long units = 0ULL;
    EMBED_CALL(branch_words_count, DocumentBranchCtx, .one_read = one, .from = from, .to = to,
               .cursor_line = args->cursor.line, .cursor_col = args->cursor.col, .most_pointer = &most,
               .units_pointer = &units);
    CliDocumentWords *const words = g_cli_cells->persistent_buf_alloc(
        sizeof(*words) + (most * (sizeof(unsigned long long) + sizeof(unsigned int))) + (3ULL * units) + 1ULL,
        args->owner);
    unsigned long long capacity = 16ULL;
    while (capacity < (2ULL * most))
    {
        capacity *= 2ULL;
    }
    unsigned int (*const table)[2] =
        (words != NULL) ? g_cli_cells->temporary_buf_alloc(capacity * sizeof(*table), args->owner) : NULL;
    if (words != NULL)
    {
        words->count = 0u;
        words->at = (unsigned long long *)(void *)(words + 1);
        words->length = (unsigned int *)(void *)(words->at + most);
        words->text = (char *)(void *)(words->length + most);
    }
    if (table != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = table, .val = 0, .bytes = capacity * sizeof(*table)});
        EMBED_CALL(branch_words_fill, DocumentBranchCtx, .one_read = one, .from = from, .to = to, .words = words,
                   .word_table = table, .capacity = capacity, .cursor_line = args->cursor.line,
                   .cursor_col = args->cursor.col);
    }
    EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    g_cli_cells->persistent_buf_release(table, args->owner);
    return words;
}

void cli_document_walk(void)
{
    EMBED_CALL(branch_walk, DocumentBranchCtx, .walker = &s_walkers[CLI_DOC_WALKERS], .own = 0);
}

unsigned int cli_document_walkers(void)
{
    unsigned int walking = 0u;
    for (unsigned int at = 0u; at < CLI_DOC_WALKERS; at += 1u)
    {
        walking += s_walkers[at].walking ? 1u : 0u;
    }
    return walking;
}

char *cli_document_text_alloc(const CliDocumentKeyCfg *args)
{
    unsigned long long *size = args->size_pointer;
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    char *reserved = NULL;
    if ((one != NULL) && !one->paged)
    {
        const CliLines *const lines = &one->lines;
        // each unit written as three bytes at the most, each line's newline, and the NUL after the last
        unsigned long long capacity = 1ULL;
        for (unsigned int line = 0u; line < lines->lines; line += 1u)
        {
            capacity += (3ULL * lines->length[line]) + 1ULL;
        }
        reserved = g_cli_cells->persistent_buf_alloc(capacity, args->owner);
        if (reserved != NULL)
        {
            VerbaLinea text = {reserved, capacity, 0ULL};
            for (unsigned int line = 0u; line < lines->lines; line += 1u)
            {
                EMBED_CALL(branch_line_append, DocumentBranchCtx, .lines = lines, .line = line, .out = &text);
                mmgr_line_put(&text, ((line + 1u) < lines->lines) ? "\n" : "");
            }
            reserved[text.at] = '\0';
            *size = text.at;
        }
    }
    return reserved;
}

int cli_document_file_put(const CliDocumentOutCfg *args)
{
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    if ((one == NULL) || one->paged)
    {
        mmgr_line_put(args->err_msg, "no text read whole is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    const CliLines *const lines = &one->lines;
    const char *const ending = lines->crlf ? "\r\n" : "\n";
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        mmgr_line_put(args->out, (line != 0u) ? ending : "");
        EMBED_CALL(branch_line_append, DocumentBranchCtx, .lines = lines, .line = line, .out = args->out);
    }
    return mmgr_line_ok(args->out);
}

// the lines asked of a stored text, `first_u64` to `last_u64` cut to the text, into `first_pointer` and `end_pointer`,
// the last of them; the colorer's own state the first is entered in, the scope after its mark left out, into
// `entered`; and the lines written into `out`, a newline between each two, or where the text is paged its key into
// `text_out`, CLI_NAME_MAX_LEN bytes, for its lines to be read through the pages with the lock given back
EMBED_INLINE void branch_lines_stored(const DocumentBranchCtx *args)
{
    const CliStoredText *const one = args->one_read;
    const CliLines *const lines = &one->lines;
    // the lines asked stand below the text's last, which an unsigned int counts
    const unsigned int to = (args->last_u64 < lines->lines) ? (unsigned int)args->last_u64 : (lines->lines - 1u);
    const unsigned int from = (args->first_u64 <= to) ? (unsigned int)args->first_u64 : to;
    *args->first_pointer = from;
    *args->end_pointer = to;
    const unsigned int begun = (lines->lines != 0u) ? lines->begun[from] : CLI_DOC_UNCOLORED;
    const char *const begun_in = ((begun != CLI_DOC_UNCOLORED) && (one->state != NULL)) ? (one->state + begun) : "root";
    const char *const mark =
        cellul.chr(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_CELLS_POOL, .byte = CLI_DOC_SCOPE_MARK});
    mmgr_line_put_n(args->entered, begun_in,
                    (mark != NULL) ? (unsigned long long)(mark - begun_in)
                                   : cellul.len(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_CELLS_POOL}));
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
    const char **const starts = g_cli_cells->temporary_buf_alloc(required * sizeof(const char *), &g_cli_editor_cells);
    unsigned long long count = 0ULL;
    char *const reserved = (starts != NULL) ? EMBED_CALL(cli_pages_from.lines_alloc, CliPagesFromCfg, .key = args->text,
                                                         .from = args->from, .to = args->to + 1u, .line = starts,
                                                         .count = &count, .owner = &g_cli_editor_cells)
                                            : NULL;
    for (unsigned long long at = 0ULL; (reserved != NULL) && (at < count); at += 1ULL)
    {
        mmgr_line_put(args->out, starts[at]);
        mmgr_line_put(args->out, ((args->from + at) < args->to) ? "\n" : "");
    }
    g_cli_cells->persistent_buf_release(reserved, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(starts, &g_cli_editor_cells);
}

char *cli_document_lines_alloc(const CliDocumentKeyCfg *args)
{
    unsigned long long *size = args->size_pointer;
    char *const written_cell = g_cli_cells->temporary_buf_alloc(CLI_DOC_COLORS_CAPACITY, args->owner);
    VerbaLinea written = mmgr_line_over(written_cell, CLI_DOC_COLORS_CAPACITY);
    const int made = written.out != NULL;
    VerbaLinea *const text = &written;
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    const int found = one != NULL;
    VerbaLinea entered = {args->state, args->capacity, 0ULL};
    // a paged text's lines, read through the pages with the lock given back: its key, and the lines asked
    char paged[CLI_NAME_MAX_LEN];
    paged[0] = '\0';
    unsigned int from = 0u;
    unsigned int to = 0u;
    if (made && (one != NULL))
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
    const int reserved = made && found && mmgr_line_ok(text);
    if (made && !reserved)
    {
        g_cli_cells->persistent_buf_release(written.out, args->owner);
    }
    *size = reserved ? text->at : 0ULL;
    return reserved ? written.out : NULL;
}

int cli_document_bounds(const CliDocumentKeyLowCfg *args)
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
