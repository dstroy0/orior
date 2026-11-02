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
    VerbaLinea step;
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
    int asked;
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
// what it found written; and whether the cells refused the room for them. What it finds on each line is a record of
// words: how many runs, the line's regions, the bytes of the colorer's state the line ends in, and the bytes of the
// scope it ends in, CLI_DOC_NONE for a scope with no depth, pairs or names, CLI_DOC_SCOPE_SAME for the one the line
// before it ended in; then the runs' columns, their names as the walk names them, the colorer's state and the scope,
// each of the two from a word of its own
typedef struct
{
    unsigned int *named;
    unsigned long long named_room;
    unsigned int *found;
    unsigned long long found_room;
    int filled;
    CliBranchScope scope;
    int placed;
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
// the room a step's colors are written in, a state's longest text, the room a text's states hold past them and the
// places of its index they are found by, the bytes a coloring's room is written in, the share of what a text's room for
// its states or its runs is taken for that it holds past it, and no line
#define CLI_DOC_ROOT 0u
#define CLI_DOC_STEP 256u
#define CLI_DOC_STEP_BYTES 16384ull
#define CLI_DOC_ALL_BYTES 0xffffffffffffffffull
#define CLI_DOC_BATCH 64u
#define CLI_DOC_RUN 4096u
#define CLI_DOC_COLORS_ROOM (1ull << 20u)
#define CLI_DOC_STATE 8192u
#define CLI_DOC_STATES_ROOM (256u << 10u)
#define CLI_DOC_INDEX_PLACES 16384u
#define CLI_DOC_ROOM_SHARE 8ull
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
static unsigned int s_walkers_most;
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
    unsigned long long room;
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
    int *handed;
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
} DocumentBranchCtx;
// the hash of `length` bytes of `text`, FNV-1a over its bytes
EMBED_INLINE unsigned int branch_state_hash(const DocumentBranchCtx *args)
{
    unsigned int hash = 2166136261u;
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
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
EMBED_INLINE void branch_index_put(const DocumentBranchCtx *args)
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
            g_cli_cells->persistent_buf_alloc(CLI_DOC_INDEX_PLACES * sizeof(*cell), &g_cli_editor_cells);
        if (cell == NULL)
        {
            return 0;
        }
        memor.set(&(MemoriaCfg){.dst = cell, .val = 0, .bytes = CLI_DOC_INDEX_PLACES * sizeof(*cell)});
        args->one->state_index = cell;
        args->one->state_indexed = CLI_DOC_INDEX_PLACES;
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
    const unsigned long long needed = (unsigned long long)args->one->state_used + length + 1ull;
    if (args->one->state_room == 0u)
    {
        args->one->state = g_cli_cells->persistent_buf_alloc(CLI_DOC_STATES_ROOM, &g_cli_editor_cells);
        args->one->state_room = (args->one->state != NULL) ? CLI_DOC_STATES_ROOM : 0u;
    }
    if (needed > args->one->state_room)
    {
        args->one->refused = 1;
        return CLI_DOC_ROOT;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = args->one->state + args->one->state_used, .src = args->text, .bytes = length + 1ull});
    const unsigned int at = args->one->state_used;
    // the states take no more than their room
    args->one->state_used = (unsigned int)needed;
    EMBED_CALL(branch_index_put, DocumentBranchCtx, .one = args->one, .at = at);
    args->one->state_count += 1u;
    args->one->state_last = at;
    return at;
}

// the line `line` of `lines` written as UTF-8 into `out`, a surrogate pair read as the one letter it makes
EMBED_INLINE void branch_line_put(const DocumentBranchCtx *args)
{
    unsigned int line = args->line;
    const unsigned short *const unit = args->lines->unit + args->lines->start[line];
    const unsigned long long count = args->lines->length[line];
    for (unsigned long long place = 0ull; place < count; place += 1ull)
    {
        unsigned int letter = unit[place];
        const int paired = (letter >= 0xd800u) && (letter < 0xdc00u) && ((place + 1ull) < count) &&
                           (unit[place + 1ull] >= 0xdc00u) && (unit[place + 1ull] < 0xe000u);
        if (paired)
        {
            letter = 0x10000u + ((letter - 0xd800u) << 10u) + (unit[place + 1ull] - 0xdc00u);
            place += 1ull;
        }
        EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = args->out, .letter = letter);
    }
}

// the lines from `from` to `to` of `lines` written as UTF-8 into `text`, `room` bytes, each ended by a NUL and each
// line's start put into `line`
EMBED_INLINE void branch_lines_write(const DocumentBranchCtx *args)
{
    VerbaLinea out = {args->text_out, args->room, 0ull};
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        args->line_letters[at - args->from] = args->text_out + out.at;
        EMBED_CALL(branch_line_put, DocumentBranchCtx, .lines = args->lines, .line = at, .out = &out);
        mmgr_line_ch(&out, '\0');
    }
}

// the lines from `first` to `end` of a text read whole, `lines`, measured: the line after the last that `bytes` of
// their units reach, one at the least, into `read`, and the bytes they take as UTF-8, each unit written as three bytes
// at the most and each line's NUL; under the lock
EMBED_INLINE unsigned long long branch_lines_measure(const DocumentBranchCtx *args)
{
    unsigned int *read = args->read;
    unsigned long long room = 1ull;
    unsigned long long spent = 0ull;
    unsigned int to = args->first;
    while ((to < args->end) && ((to == args->first) || (args->bytes == CLI_DOC_ALL_BYTES) || (spent < args->bytes)))
    {
        room += (3ull * args->lines->length[to]) + 1ull;
        spent += args->lines->length[to] + 1ull;
        to += 1u;
    }
    *read = to;
    return room;
}

// the mark a line's state carries the place of the scope it stands in after, past the colorer's own state, the scope
// written among the text's states once; the most names a scope holds and the bytes their texts take
#define CLI_DOC_SCOPE_MARK '\x1e'

// a line's scope the one the line before it ended in, the words of a record before its runs, and the words a line's
// state and scope are given room for, those of a line past them taken from the room of the lines after it
#define CLI_DOC_FOUND_STATE 64ull
#define CLI_DOC_SCOPE_SAME 0xfffffffeu
#define CLI_DOC_FOUND_HEAD 4u

// a run of lines handed on in `range` ended where one is begun
EMBED_INLINE void branch_range_end(const DocumentBranchCtx *args)
{
    if (args->range->begun)
    {
        mmgr_line_put(args->range->out, "]}");
        args->range->begun = 0;
    }
}

// the runs of the line `line` written into the run of lines handed on, `[column, name, column, name, ...]`, those that
// start at or past the range's columns left out where it names them, the first saved; a run of lines begun at it as
// `{"from", "runs"}` where it is the first or the line before it was not the last written, the one before ended
EMBED_INLINE void branch_runs_put(const DocumentBranchCtx *args)
{
    const CliLeaves *leaves = args->leaves;
    CliLeaves shown = *leaves;
    while ((args->range->columns != 0ull) && (shown.runs > 1u) && (shown.col[shown.runs - 1u] >= args->range->columns))
    {
        shown.runs -= 1u;
    }
    VerbaLinea *const out = args->range->out;
    if (args->range->begun && (args->line != args->range->next))
    {
        mmgr_line_put(out, "]}");
        args->range->begun = 0;
    }
    if (!args->range->begun)
    {
        mmgr_line_put(out, *args->range->any ? ",{\"from\":" : "{\"from\":");
        mmgr_line_decimal(out, args->line, 1u);
        mmgr_line_put(out, ",\"runs\":[");
        args->range->begun = 1;
        args->range->written = 0u;
        *args->range->any = 1;
    }
    mmgr_line_put(out, (args->range->written != 0u) ? "," : "");
    EMBED_CALL(cli_highlight.runs_write, CliHighlightCfg, .leaves = &shown, .out = out);
    args->range->written += 1u;
    args->range->next = args->line + 1u;
}

// the words a runs' room holds past the runs it is taken for, as many again as their share CLI_DOC_ROOM_SHARE holds
#define CLI_DOC_RUNS_SPARE 4096ull

// the words of the runs the lines of `one` save, and a room the cells give them taken afresh, those runs moved into
// it in the order of their lines, and it holds them with `more` words after them and a share of them past those: 1, or
// 0 where the cells cannot meet it, the room as it was
EMBED_INLINE int branch_runs_room(const DocumentBranchCtx *args)
{
    CliLines *const lines = &args->one->lines;
    unsigned long long saved = 0ull;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        saved += (lines->runs[line] != CLI_DOC_NO_RUNS) ? (1ull + (2ull * args->one->run[lines->runs[line]])) : 0ull;
    }
    const unsigned long long wanted = saved + args->more;
    const unsigned long long room = wanted + (wanted / CLI_DOC_ROOM_SHARE) + CLI_DOC_RUNS_SPARE;
    unsigned int *const moved =
        (room <= 0xffffffffull) ? g_cli_cells->persistent_buf_alloc(room * sizeof(*moved), &g_cli_editor_cells) : NULL;
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
            const unsigned int words = 1u + (2u * args->one->run[at]);
            memor.cpy(&(MemoriaCfg){.dst = moved + used, .src = args->one->run + at, .bytes = words * sizeof(*moved)});
            lines->runs[line] = used;
            used += words;
        }
    }
    g_cli_cells->persistent_buf_release(args->one->run, &g_cli_editor_cells);
    args->one->run = moved;
    // a room checked above to be no more than an unsigned int counts
    args->one->run_room = (unsigned int)room;
    args->one->run_used = used;
    return 1;
}

// the runs of `leaves`, the line `line` of `one` as the walk named them, saved in place of those it saved, none where
// the cells cannot meet them
EMBED_INLINE void branch_runs_save(const DocumentBranchCtx *args)
{
    CliLines *const lines = &args->one->lines;
    lines->runs[args->line] = CLI_DOC_NO_RUNS;
    const unsigned long long words = 1ull + (2ull * args->leaves->runs);
    if (((args->one->run_used + words) > args->one->run_room) &&
        !EMBED_CALL(branch_runs_room, DocumentBranchCtx, .one = args->one, .more = words))
    {
        return;
    }
    unsigned int *const record = args->one->run + args->one->run_used;
    record[0] = args->leaves->runs;
    memor.cpy(
        &(MemoriaCfg){.dst = record + 1, .src = args->leaves->col, .bytes = args->leaves->runs * sizeof(*record)});
    memor.cpy(&(MemoriaCfg){.dst = record + 1 + args->leaves->runs,
                            .src = args->leaves->name,
                            .bytes = args->leaves->runs * sizeof(*record)});
    lines->runs[args->line] = args->one->run_used;
    // the words written lie within the room, which an unsigned int counts
    args->one->run_used += (unsigned int)words;
}

// whether the line `line` of `one` saves runs it may hand on as they are: it is colored, saves runs, and was entered
// in the state the line above it ends in, or from root where that line is not colored
EMBED_INLINE int branch_runs_standing(const DocumentBranchCtx *args)
{
    const CliLines *const lines = &args->one_read->lines;
    const unsigned int above = (args->line != 0u) ? lines->ended[args->line - 1u] : CLI_DOC_UNCOLORED;
    return (lines->begun[args->line] != CLI_DOC_UNCOLORED) && (lines->runs[args->line] != CLI_DOC_NO_RUNS) &&
           ((above == CLI_DOC_UNCOLORED) ? (lines->begun[args->line] == CLI_DOC_ROOT)
                                         : (lines->begun[args->line] == above));
}

// the runs the line `line` of `one` saves written through `range`
EMBED_INLINE void branch_runs_dispatch(const DocumentBranchCtx *args)
{
    const unsigned int *const record = args->one_read->run + args->one_read->lines.runs[args->line];
    const CliLeaves saved = {NULL, record[0], record + 1, record + 1 + record[0], NULL, NULL, NULL, 0u};
    EMBED_CALL(branch_runs_put, DocumentBranchCtx, .range = args->range, .line = args->line, .leaves = &saved);
}

// the roles that count the depth of a pair
#define CLI_DOC_PAIR_ROLES (CLI_ROLE_DEPTH_OPENS | CLI_ROLE_DEPTH_CLOSES)

// one name of the grammar of the text at `context` its roles read and saved among the text's roles at its place
static void branch_role_read(const CliNameEachCfg *args)
{
    const char *name = args->name;
    CliStoredText *const one = (CliStoredText *)args->context;
    if (args->place < one->roles_room)
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
    if (args->one->roles_whole || (names >= CLI_DOC_NONE))
    {
        return;
    }
    if (names > args->one->roles_room)
    {
        // the roles are read anew over a cell of the names counted
        g_cli_cells->persistent_buf_release(args->one->roles, &g_cli_editor_cells);
        args->one->roles = g_cli_cells->persistent_buf_alloc((unsigned long long)names * sizeof(*args->one->roles),
                                                             &g_cli_editor_cells);
        if (args->one->roles == NULL)
        {
            args->one->roles_room = 0u;
            return;
        }
        args->one->roles_room = names;
    }
    memor.set(&(MemoriaCfg){.dst = args->one->roles,
                            .val = 0,
                            .bytes = (unsigned long long)args->one->roles_room * sizeof(*args->one->roles)});
    (void)EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = args->one->grammar,
                     .each_name_each = branch_role_read, .context = args->one);
    unsigned int rank = 0u;
    for (unsigned int place = 0u; place < args->one->roles_room; place += 1u)
    {
        // the rank saved plus 1, a name's place among no more than CLI_DOC_RANKS
        args->one->roles[place] |= (rank < CLI_DOC_RANKS) ? ((rank + 1u) << CLI_DOC_RANK_AT) : 0u;
        rank += ((args->one->roles[place] & CLI_DOC_PAIR_ROLES) != 0u) ? 1u : 0u;
    }
    args->one->roles_whole = 1;
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
            found |= ((closes == 0u) && mark) ? CLI_DOC_REGION_MARK : 0u;
            unled += (((found & CLI_DOC_REGION_FILLED) == 0u) && (unled < CLI_DOC_REGION_COUNT)) ? 1u : 0u;
            closes += (closes < CLI_DOC_REGION_COUNT) ? 1u : 0u;
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

// one name of a grammar written, or where the names' depths are written, written once for each depth where it counts
// the depth of a pair
static void branch_name_write(const CliNameEachCfg *args)
{
    const char *name = args->name;
    CliBranchNames *const names = (CliBranchNames *)args->context;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (!names->depths)
    {
        mmgr_line_put(names->out, (names->written != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = names->out, .text = name,
                   .length = size);
        names->written += 1u;
        return;
    }
    if ((EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = name) & CLI_DOC_PAIR_ROLES) == 0u)
    {
        return;
    }
    for (unsigned int depth = 0u; depth < CLI_DOC_DEPTHS; depth += 1u)
    {
        char deep[CLI_NAME_MAX_LEN + 8u];
        VerbaLinea text = {deep, sizeof(deep), 0ull};
        mmgr_line_put_n(&text, name, size);
        mmgr_line_ch(&text, '.');
        mmgr_line_decimal(&text, depth, 1u);
        const unsigned long long length = mmgr_line_finish(&text);
        mmgr_line_ch(names->out, ',');
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = names->out, .text = deep,
                   .length = length);
    }
}

// one name of a grammar passed over, as its names are counted
static void branch_name_pass(const CliNameEachCfg *args)
{
    (void)args;
}

void cli_document_names_write(const CliDocumentOutGrammarCfg *args)
{
    CliBranchNames names = {args->out, 0, 0u};
    mmgr_line_ch(args->out, '[');
    if ((args->grammar[0] == '\0') ||
        (EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = args->grammar,
                    .each_name_each = branch_name_write, .context = &names) == 0u))
    {
        mmgr_line_put(args->out, "\"\"]");
        return;
    }
    names.depths = 1;
    (void)EMBED_CALL(cli_highlight_key_context.names_each, CliHighlightKeyContextCfg, .key = args->grammar,
                     .each_name_each = branch_name_write, .context = &names);
    mmgr_line_ch(args->out, ']');
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
EMBED_INLINE void branch_scope_put(const DocumentBranchCtx *args)
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
    unsigned long long end = 0ull;
    // a state's digits write a place among the states, which an unsigned int counts
    value = (unsigned int)EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = *from, .at = 0ull,
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
        EMBED_CALL(branch_scope_put, DocumentBranchCtx, .scope = args->scope, .text = at, .size = size, .named = named,
                   .belongs = belongs, .waits = waits);
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
    if (!scope->moved && args->finding_out->placed)
    {
        return CLI_DOC_SCOPE_SAME;
    }
    EMBED_CALL(branch_scope_write, DocumentBranchCtx, .scope_read = scope, .out = args->out);
    const unsigned long long size = mmgr_line_finish(args->out);
    scope->moved = 0;
    args->finding_out->placed = size != 0ull;
    // a scope written within its room, which an unsigned int counts
    return (size != 0ull) ? (unsigned int)size : CLI_DOC_NONE;
}

// one line's leaves walked through the scope of `finding`: a region its runs open deepens it and its function's
// parameters waiting
// for it open in it, one they close lets go the names that belong to it, a local is declared in the region it stands
// in and a parameter in the one the next region opens, a statement's end or a directive lets go the parameters still
// waiting, at once and at the line's end; each bare name a use of the last declaration of it in the scope takes that
// declaration's name, but one reached through the operator before it, blanks between; and a pair it opens deepens the
// scope's pairs and one it closes lets one go, each run that opens or closes one named by the depth it stands at, the
// first of its names' `names` on, as `cli_document_names_write` writes them. The leaves with each run's name so
// taken, in the work's room for them, the leaves as they came where the cells cannot meet that room
EMBED_INLINE CliLeaves branch_uses_by_name(const DocumentBranchCtx *args)
{
    CliLeaves named = *args->leaves;
    CliBranchScope *const scope = &args->finding_out->scope;
    const unsigned int names = args->finding_out->names;
    const int renaming = (args->leaves->runs != 0u) && (args->leaves->runs <= args->finding_out->named_room);
    unsigned int *const renamed = args->finding_out->named;
    const char *const line = args->leaves->line;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
    unsigned long long byte = 0ull;
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
        if (renaming)
        {
            renamed[run] = name;
        }
        if ((roles & CLI_ROLE_REGION_OPENS) != 0u)
        {
            scope->depth += 1u;
            scope->moved = 1;
            for (unsigned int at = 0u; at < scope->count; at += 1u)
            {
                scope->waits[at] = (scope->belongs[at] == scope->depth) ? 0u : scope->waits[at];
            }
        }
        else if (((roles & CLI_ROLE_REGION_CLOSES) != 0u) && (scope->depth > 0u))
        {
            scope->depth -= 1u;
            scope->moved = 1;
            EMBED_CALL(branch_scope_leave, DocumentBranchCtx, .scope = scope, .depth = scope->depth, .waiting = 0);
        }
        if ((roles & CLI_ROLE_PARAMETER) != 0u)
        {
            EMBED_CALL(branch_scope_put, DocumentBranchCtx, .scope = scope, .text = line + from, .size = bytes,
                       .named = name, .belongs = scope->depth + 1u, .waits = 1);
        }
        else if ((roles & CLI_ROLE_LOCAL) != 0u)
        {
            EMBED_CALL(branch_scope_put, DocumentBranchCtx, .scope = scope, .text = line + from, .size = bytes,
                       .named = name, .belongs = scope->depth, .waits = 0);
        }
        else if (renaming && !through && ((roles & CLI_ROLE_NAME) != 0u))
        {
            const unsigned int declared = EMBED_CALL(branch_scope_by_name, DocumentBranchCtx, .scope_read = scope,
                                                     .text = line + from, .size_u64 = bytes);
            renamed[run] = (declared != CLI_DOC_NONE) ? declared : name;
        }
        if ((roles & CLI_ROLE_ENDS) != 0u)
        {
            EMBED_CALL(branch_scope_leave, DocumentBranchCtx, .scope = scope, .depth = CLI_DOC_NONE, .waiting = 1);
            ended = 1;
        }
        // a pair that closes stands at the depth of the one it closes
        const int closes = (roles & CLI_ROLE_DEPTH_CLOSES) != 0u;
        const int opens = (roles & CLI_ROLE_DEPTH_OPENS) != 0u;
        scope->pairs -= (closes && (scope->pairs > 0u)) ? 1u : 0u;
        const unsigned int rank = ((closes || opens) && renaming && (names != 0u))
                                      ? EMBED_CALL(branch_depth_rank, DocumentBranchCtx, .finding = args->finding_out,
                                                   .leaves = args->leaves, .name = name)
                                      : CLI_DOC_NONE;
        if (rank != CLI_DOC_NONE)
        {
            renamed[run] = names + (rank * CLI_DOC_DEPTHS) + (scope->pairs % CLI_DOC_DEPTHS);
        }
        scope->pairs += opens ? 1u : 0u;
        scope->moved = scope->moved || closes || opens;
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
// its own; nothing where the cells refused the room for one before it or refuse it
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
    VerbaLinea scope = {scoped, sizeof(scoped), 0ull};
    const unsigned int scope_size =
        EMBED_CALL(branch_scope_find, DocumentBranchCtx, .finding_out = finding, .out = &scope);
    const unsigned int scope_bytes =
        ((scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME)) ? scope_size : 0u;
    // the colorer's state, no longer than the room it is written in, which an unsigned int counts
    const unsigned int ended =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = leaves->ended, .cap = CLI_CELLS_POOL});
    const unsigned long long ended_words = (ended + 3ull) / 4ull;
    const unsigned long long words =
        CLI_DOC_FOUND_HEAD + (2ull * named.runs) + ended_words + ((scope_bytes + 3ull) / 4ull);
    // the lines past the room are the next bundle's
    if ((finding->used + words) > finding->found_room)
    {
        finding->filled = 1;
        return;
    }
    unsigned int *const record = finding->found + finding->used;
    record[0] = named.runs;
    record[1] = regions;
    record[2] = ended;
    record[3] = scope_size;
    unsigned int *const cols = record + CLI_DOC_FOUND_HEAD;
    memor.cpy(&(MemoriaCfg){.dst = cols, .src = named.col, .bytes = named.runs * sizeof(*cols)});
    memor.cpy(&(MemoriaCfg){.dst = cols + named.runs, .src = named.name, .bytes = named.runs * sizeof(*cols)});
    char *const text = (char *)(cols + (2u * named.runs));
    memor.cpy(&(MemoriaCfg){.dst = text, .src = leaves->ended, .bytes = ended});
    memor.cpy(&(MemoriaCfg){.dst = text + (4ull * ended_words), .src = scoped, .bytes = scope_bytes});
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
EMBED_INLINE unsigned int branch_claim_put(const DocumentBranchCtx *args)
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
EMBED_INLINE unsigned int branch_found_put(const DocumentBranchCtx *args)
{
    unsigned int *entering = args->entering;
    int *stopped = args->stopped;
    CliLines *const lines = &args->one->lines;
    const unsigned int hand_from = args->range->asked ? args->range->hand_from : args->one->cached_from;
    const unsigned int hand_to = args->range->asked ? args->range->hand_to : args->one->cached_to;
    args->range->columns = (args->range->columns != 0ull) ? args->one->columns : 0ull;
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
        const char *const ended = (const char *)(record + CLI_DOC_FOUND_HEAD + (2u * runs));
        const unsigned int scope_size = record[3];
        const int written = (scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME);
        if (written)
        {
            char scope[CLI_DOC_STATE];
            memor.cpy(&(MemoriaCfg){.dst = scope, .src = ended + (4u * ended_words), .bytes = scope_size});
            scope[scope_size] = '\0';
            scoped = EMBED_CALL(branch_state, DocumentBranchCtx, .one = args->one, .text = scope);
        }
        // the state the line ends in the colorer's with the place of its scope after the mark, the colorer's alone
        // where the scope has no place or the two do not fit
        char state[CLI_DOC_STATE];
        VerbaLinea out = {state, sizeof(state), 0ull};
        mmgr_line_put_n(&out, ended, record[2]);
        const unsigned long long colorer = out.at;
        if ((scope_size != CLI_DOC_NONE) && (scoped != CLI_DOC_NONE))
        {
            mmgr_line_ch(&out, CLI_DOC_SCOPE_MARK);
            mmgr_line_decimal(&out, scoped, 1u);
        }
        if (mmgr_line_finish(&out) == 0ull)
        {
            state[colorer] = '\0';
        }
        lines->begun[line] = *entering;
        lines->ended[line] = EMBED_CALL(branch_state, DocumentBranchCtx, .one = args->one, .text = state);
        args->one->regions_moved = args->one->regions_moved || (lines->regions[line] != record[1]);
        lines->regions[line] = record[1];
        const CliLeaves saved = {
            NULL, runs, record + CLI_DOC_FOUND_HEAD, record + CLI_DOC_FOUND_HEAD + runs, NULL, NULL, NULL, 0u};
        const int handed = (line >= hand_from) && (line < hand_to);
        const int cached = (line >= args->one->cached_from) && (line < args->one->cached_to);
        if (!args->one->paged || cached || handed)
        {
            EMBED_CALL(branch_runs_save, DocumentBranchCtx, .one = args->one, .line = line, .leaves = &saved);
        }
        else
        {
            lines->runs[line] = CLI_DOC_NO_RUNS;
        }
        if (handed)
        {
            EMBED_CALL(branch_runs_put, DocumentBranchCtx, .range = args->range, .line = line, .leaves = &saved);
        }
        *entering = lines->ended[line];
        line += 1u;
        record += CLI_DOC_FOUND_HEAD + (2u * runs) + ended_words + (written ? ((scope_size + 3u) / 4u) : 0u);
    }
    return line;
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
    const unsigned int most =
        args->stopping ? CLI_DOC_BATCH : ((args->range->bytes != CLI_DOC_ALL_BYTES) ? CLI_DOC_STEP : CLI_DOC_RUN);
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
    finding.named_room = 0ull;
    finding.found = NULL;
    finding.found_room = 0ull;
    finding.filled = 0;
    finding.names = args->one->names;
    finding.used = 0ull;
    finding.refused = 0;
    // the state the first line is entered in, copied out, the colorer's own handed to it and the scope at the place
    // after the mark read into the bundle
    char state[CLI_DOC_STATE];
    VerbaLinea entered = {state, sizeof(state), 0ull};
    mmgr_line_put(&entered, args->one->state + *args->entering);
    const int stated = mmgr_line_finish(&entered) != 0ull;
    const char *const mark =
        stated ? cellul.chr(&(CatenaFinitaCfg){.src = state, .cap = CLI_CELLS_POOL, .byte = CLI_DOC_SCOPE_MARK}) : NULL;
    const char *place = (mark != NULL) ? (mark + 1) : NULL;
    EMBED_CALL(branch_scope_read, DocumentBranchCtx, .one_read = args->one,
               .place = (place != NULL)
                            ? EMBED_CALL(branch_digits, DocumentBranchCtx, .from_letters = &place, .after = '\0')
                            : CLI_DOC_NONE,
               .scope = &finding.scope);
    finding.placed = finding.scope.place != CLI_DOC_NONE;
    if (mark != NULL)
    {
        // the mark stands within the state copied
        state[mark - state] = '\0';
    }
    // the roles of the grammar's names copied, read first where they were not
    branch_roles_read(args);
    const unsigned int roles = args->one->roles_whole ? args->one->roles_room : 0u;
    // where each line read starts, the roles and the UTF-8 of a text read whole, in one cell the bundle gives back
    unsigned int read = args->first;
    const unsigned long long text_room =
        paged ? 0ull
              : EMBED_CALL(branch_lines_measure, DocumentBranchCtx, .lines = &args->one->lines, .first = args->first,
                           .end = end, .bytes = args->range->bytes, .read = &read);
    const unsigned long long starts_bytes = (unsigned long long)(end - args->first) * sizeof(const char *);
    const unsigned long long roles_bytes = (unsigned long long)roles * sizeof(unsigned int);
    char *const cell = g_cli_cells->temporary_buf_alloc(starts_bytes + roles_bytes + text_room, &g_cli_editor_cells);
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
                       .to = read, .text_out = cell + starts_bytes + roles_bytes, .room = text_room,
                       .line_letters = line);
        }
    }
    const unsigned int claim =
        EMBED_CALL(branch_claim_put, DocumentBranchCtx, .one = args->one, .first = args->first, .end = end);
    char *taken = NULL;
    if (paged && (cell != NULL))
    {
        unsigned long long count = 0ull;
        taken = EMBED_CALL(cli_pages_from.lines_take, CliPagesFromCfg, .key = key, .from = args->first, .to = end,
                           .line = line, .count = &count, .owner = &g_cli_editor_cells);
        // no more lines are read than were asked, which an unsigned int counts
        read = args->first + (unsigned int)count;
    }
    line = (paged && (taken == NULL)) ? NULL : line;
    // the lines read colored as far as the range's bytes reach, one at the least
    unsigned int spend = 0u;
    unsigned long long spent = 0ull;
    unsigned long long total = 0ull;
    unsigned long long longest = 0ull;
    while ((line != NULL) && ((args->first + spend) < read) &&
           ((spend == 0u) || (args->range->bytes == CLI_DOC_ALL_BYTES) || (spent < args->range->bytes)))
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = line[spend], .cap = CLI_CELLS_POOL});
        spent += (args->range->bytes != CLI_DOC_ALL_BYTES) ? (length + 1ull) : 0ull;
        total += length + 1ull;
        longest = (length > longest) ? length : longest;
        spend += 1u;
    }
    if (args->range->bytes != CLI_DOC_ALL_BYTES)
    {
        args->range->bytes -= (spent < args->range->bytes) ? spent : args->range->bytes;
    }
    // the names of a line's runs, one run for each of its bytes at the most, and the records: two words for each run, a
    // record's head and CLI_DOC_FOUND_STATE words of its state for each line, and the words of one state and scope
    // whole. The lines past them are the next bundle's
    finding.named_room = longest + 2ull;
    finding.found_room = (2ull * total) + ((unsigned long long)spend * (CLI_DOC_FOUND_HEAD + CLI_DOC_FOUND_STATE)) +
                         (CLI_DOC_STATE / 2ull);
    const unsigned long long named_bytes = finding.named_room * sizeof(unsigned int);
    char *const found_cell =
        (spend != 0u) ? g_cli_cells->temporary_buf_alloc(named_bytes + (finding.found_room * sizeof(unsigned int)),
                                                         &g_cli_editor_cells)
                      : NULL;
    finding.named = (unsigned int *)(void *)found_cell;
    finding.found = (found_cell != NULL) ? (unsigned int *)(void *)(found_cell + named_bytes) : NULL;
    int colored = 0;
    if ((line != NULL) && (found_cell != NULL))
    {
        char ended[CLI_DOC_STATE];
        VerbaLinea last = {ended, sizeof(ended), 0ull};
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        colored =
            EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = grammar, .state = stated ? state : "root",
                       .lines = line, .count = spend, .ends = ((args->first + spend) == count) && whole,
                       .each = branch_find, .context = &finding, .ended = &last, .err_msg = &err_msg);
    }
    g_cli_cells->persistent_buf_release(taken, &g_cli_editor_cells);
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
        put = EMBED_CALL(branch_found_put, DocumentBranchCtx, .one = args->one, .finding = &finding,
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
    while (!stopped && !args->one->refused && !args->range->stale && (line < args->to) && (args->range->bytes != 0ull))
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
// run down whose line above ends in no known state reaching as far above as CLI_DOC_LEAD lines to color reach
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
                                 (lead < CLI_DOC_LEAD);
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
    *low = args->one_read->paged ? ((args->one_read->cached_from < *high) ? args->one_read->cached_from : *high) : 0u;
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
    if ((args->onward || (args->range->columns == 0ull)) && !args->range->stale && (line == args->end) &&
        (args->end < high) && (args->end < args->one->lines.lines) &&
        (args->one->lines.begun[args->end] != CLI_DOC_UNCOLORED))
    {
        const unsigned int most = ((high - args->end) > CLI_DOC_STEP) ? (args->end + CLI_DOC_STEP) : high;
        (void)EMBED_CALL(branch_color, DocumentBranchCtx, .one = args->one, .from = args->end, .to = most,
                         .stopping = 1, .range = args->range);
    }
}

// colors handed on begun: `{"key":` the text's key `,"seq":` the changes written to it `,"colors":`
EMBED_INLINE void branch_colors_begin(const DocumentBranchCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->one_read->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->one_read->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"seq\":");
    mmgr_line_decimal(out, args->one_read->changes, 1u);
    mmgr_line_put(out, ",\"colors\":");
}

// colors handed on ended: `,"names":` the names the text's grammar's tokens take where `any` colors go and they were
// not handed on since it was colored by the grammar, the first colors handed on carrying them, and `}`; under the lock
// the colors are handed on after, and no colors go before the names they are named by
EMBED_INLINE void branch_colors_end(const DocumentBranchCtx *args)
{
    VerbaLinea *out = args->out;
    if (args->any && !args->one->named)
    {
        mmgr_line_put(out, ",\"names\":");
        EMBED_CALL(cli_document_out_grammar.names_write, CliDocumentOutGrammarCfg, .grammar = args->one->grammar,
                   .out = out);
        args->one->named = 1;
    }
    mmgr_line_ch(out, '}');
}

// one step of the walk of `one`: the run of lines to color nearest its origin colored, or where every line is colored
// or claimed, the first entered in a state the line above does not end in colored again, written into `out` as
// `{"key", "seq", "colors"}`, `*handed` set where it holds the runs of a line the page holds: 1 where it colored a
// line, else 0. A paged text's lines are counted again first, and it is walked across the lines the page holds alone.
// Under the lock, which it gives back as it colors, the text saved across it by its caller
EMBED_INLINE int branch_step(const DocumentBranchCtx *args)
{
    CliStoredText *one = args->one;
    const CliLines *const lines = &one->lines;
    one->refused =
        one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
    unsigned int low = 0u;
    unsigned int high = 0u;
    EMBED_CALL(branch_bounds, DocumentBranchCtx, .one_read = one, .low_pointer = &low, .high_pointer = &high);
    if ((one->grammar[0] == '\0') || one->refused || (high <= low) || (one->cached_to <= one->cached_from))
    {
        return 0;
    }
    const unsigned int origin = (one->origin < low) ? low : ((one->origin < high) ? one->origin : (high - 1u));
    unsigned int at = CLI_DOC_NONE;
    int down = 1;
    for (unsigned int distance = 0u;
         (at == CLI_DOC_NONE) && (((origin + distance) < high) || (distance < (origin - low))); distance += 1u)
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
    }
    unsigned int first = CLI_DOC_NONE;
    unsigned int end = 0u;
    if (at != CLI_DOC_NONE)
    {
        EMBED_CALL(branch_run_get, DocumentBranchCtx, .one_read = one, .at = at, .down = down, .low = low, .high = high,
                   .yielding = 1, .first_pointer = &first, .end_pointer = &end);
    }
    // the first line of the lines walked entered in whatever state it was entered in, where it is not the text's first
    for (unsigned int line = (low != 0u) ? (low + 1u) : 0u;
         (at == CLI_DOC_NONE) && (first == CLI_DOC_NONE) && (line < high); line += 1u)
    {
        if ((lines->begun[line] != ((line == 0u) ? CLI_DOC_ROOT : lines->ended[line - 1u])) &&
            !EMBED_CALL(branch_claim, DocumentBranchCtx, .one_read = one, .line = line))
        {
            first = line;
            end = line + 1u;
        }
    }
    if (first == CLI_DOC_NONE)
    {
        return 0;
    }
    EMBED_CALL(branch_colors_begin, DocumentBranchCtx, .one_read = one, .out = args->out);
    mmgr_line_ch(args->out, '[');
    CliBranchRange range =
        EMBED_CALL(branch_range_begin, DocumentBranchCtx, .one_read = one, .any_pointer = args->handed,
                   .out = args->out, .columns = one->columns, .bytes = CLI_DOC_STEP_BYTES, .high = high);
    EMBED_CALL(branch_range_color, DocumentBranchCtx, .one = one, .first = first, .end = end, .onward = 1,
               .range = &range);
    EMBED_CALL(branch_range_end, DocumentBranchCtx, .range = &range);
    mmgr_line_ch(args->out, ']');
    EMBED_CALL(branch_colors_end, DocumentBranchCtx, .one = one, .any = *args->handed, .out = args->out);
    return 1;
}

// the mark of a fold that shows on its first line the line it closes on, after what it hides
#define CLI_DOC_FOLD_SHOWN 0x80000000u

// the fold of the region opened on the line `opened` that the line `line` closes, the `close`th region it closes, the
// line's regions `regions`, the last line above it that holds a leaf `filled`: the last line it hides, plus 1, marked
// CLI_DOC_FOLD_SHOWN where it shows the line it closes on, or 0 where it hides no line. A mark of no letters closes a
// region on the line it ends with a leaf before it, else on the last line above that holds one; a bracket closes it on
// the line before its own where a leaf stands before it or another leaf on its line closes nothing, and on its own,
// shown, where its line holds closes alone
EMBED_INLINE unsigned int branch_fold_get(const DocumentBranchCtx *args)
{
    const int mark = (args->regions & CLI_DOC_REGION_MARK) != 0u;
    const int led = args->close >= ((args->regions >> CLI_DOC_REGION_UNLED_AT) & CLI_DOC_REGION_COUNT);
    const int shown = !mark && !led && ((args->regions & CLI_DOC_REGION_CLOSERS) != 0u);
    const unsigned int last = (mark && led) ? args->line : mark ? args->filled : shown ? args->line : (args->line - 1u);
    // a line shown on the fold's first line is no line hidden
    const unsigned int hidden = shown ? (last - 1u) : last;
    if ((last == CLI_DOC_NONE) || (hidden <= args->opened))
    {
        return 0u;
    }
    return (last + 1u) | (shown ? CLI_DOC_FOLD_SHOWN : 0u);
}

// The regions open above a line as its folds are found: the line each opens on, the innermost last, in the room the
// walk saves for them, how many, and whether the cells refused it
// the folds of `one` found, each handed to `each` with its first line, the last line it hides and whether it shows the
// line it closes on after what it hides: each fold the widest of the regions that open on its first line, in the order
// of their first lines; a paged text's those of the lines the page mirrors alone, and none where the cells cannot meet
// the room they are found in. Under the texts' lock
static void branch_folds_each(const DocumentBranchCtx *args)
{
    const CliLines *const lines = &args->one_read->lines;
    unsigned int first = 0u;
    unsigned int end = 0u;
    EMBED_CALL(branch_bounds, DocumentBranchCtx, .one_read = args->one_read, .low_pointer = &first,
               .high_pointer = &end);
    const unsigned int saved = end - first;
    // the widest fold of each line and the regions open above a line, no more than the regions they open
    unsigned long long opening = 0ull;
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
        for (unsigned int close = 0u; (close < closes) && (open != 0u); close += 1u)
        {
            open -= 1u;
            const unsigned int opened = opened_on[open];
            const unsigned int fold = EMBED_CALL(branch_fold_get, DocumentBranchCtx, .opened = opened, .line = line,
                                                 .close = close, .regions = regions, .filled = filled);
            if ((fold & ~CLI_DOC_FOLD_SHOWN) > (widest[opened - first] & ~CLI_DOC_FOLD_SHOWN))
            {
                widest[opened - first] = fold;
            }
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
                       .last = (widest[at] & ~CLI_DOC_FOLD_SHOWN) - 1u,
                       .shown = (widest[at] & CLI_DOC_FOLD_SHOWN) != 0u);
        }
    }
    g_cli_cells->persistent_buf_release(widest, &g_cli_editor_cells);
}

// a fold written into the line `context`, `[first, last, shown]` after a comma where one was written before it
static void branch_fold_write(const CliFoldEachCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->out[out->at - 1ull] == '[') ? "[" : ",[");
    mmgr_line_decimal(out, args->first, 1u);
    mmgr_line_ch(out, ',');
    mmgr_line_decimal(out, args->last, 1u);
    mmgr_line_put(out, args->shown ? ",1]" : ",0]");
}

// the folds of `one` written into `out` as `{"key", "seq", "folds": [[first, last, shown], ...]}`, `seq` the changes
// written to it, each as branch_folds_each finds them. Its regions marked as handed on
EMBED_INLINE void branch_folds_write(const DocumentBranchCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->one->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->one->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"seq\":");
    mmgr_line_decimal(out, args->one->changes, 1u);
    mmgr_line_put(out, ",\"folds\":[");
    EMBED_CALL(branch_folds_each, DocumentBranchCtx, .one_read = args->one, .each = branch_fold_write, .context = out);
    mmgr_line_put(out, "]}");
    args->one->regions_moved = 0;
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
    const unsigned int end = (one != NULL) ? ((args->line < one->lines.lines) ? args->line : one->lines.lines) : 0u;
    // the regions open above the line, no more than those the lines above it open
    unsigned long long opening = 0ull;
    for (unsigned int at = 0u; at < end; at += 1u)
    {
        opening += one->lines.regions[at] & CLI_DOC_REGION_COUNT;
    }
    unsigned int *const opened_on =
        (opening != 0ull) ? g_cli_cells->temporary_buf_alloc(opening * sizeof(unsigned int), &g_cli_editor_cells)
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
    const unsigned int count = (open < args->most_u32) ? open : args->most_u32;
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

// colors handed on to where they go, `colors` where `any` line's runs or folds were written into it: the lock they are
// handed on under taken before the texts' lock is given back, and they go in the order they were put in; the texts'
// lock given back either way
EMBED_INLINE void branch_handed_out(const DocumentBranchCtx *args)
{
    const CliDocColored colored = s_colored;
    const int handing = args->any && (colored != NULL) && mmgr_line_ok(args->colors);
    if (handing)
    {
    }
    if (handing)
    {
        EMBED_CALL(colored, CliDocColoredCfg, .body = args->colors->out, .size = args->colors->at);
    }
}

// the walk taken by `walker` step by step until no text it may walk has a line to color: each step on a text no other
// walker walks and the walk did not find colored whole since it last moved, the text saved across the lock given back
// as it colors, and the step's colors handed on once it is; on a walker's own thread where `own` is 1, marked as
// walking no more as it ends
static void branch_walk(const DocumentBranchCtx *args)
{
    for (;;)
    {
        // the line a step's colors are written into, taken as the walk begins and emptied for each step
        if (args->walker->step.out == NULL)
        {
            char *const step_cell = g_cli_cells->temporary_buf_alloc(CLI_DOC_COLORS_ROOM, &g_cli_editor_cells);
            args->walker->step = mmgr_line_over(step_cell, CLI_DOC_COLORS_ROOM);
        }
        const int made = args->walker->step.out != NULL;
        args->walker->step.at = 0u;
        CliStoredText *one = made ? cli_document_texts() : NULL;
        while ((one != NULL) && (one->walked || one->resting))
        {
            one = one->next;
        }
        int handed = 0;
        if (one != NULL)
        {
            one->walked = 1;
            EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
            int stepped =
                EMBED_CALL(branch_step, DocumentBranchCtx, .one = one, .out = &args->walker->step, .handed = &handed);
            // a text the step colored none of is colored whole: its folds go on where its regions moved
            if (!stepped && EMBED_CALL(branch_folds_ready, DocumentBranchCtx, .one_read = one))
            {
                EMBED_CALL(branch_folds_write, DocumentBranchCtx, .one = one, .out = &args->walker->step);
                stepped = 1;
                handed = 1;
            }
            one->resting = !stepped;
            one->walked = 0;
            EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
        }
        else
        {
            // the step's line given back as the walk ends, before the walker is marked free to walk again
            g_cli_cells->persistent_buf_release(args->walker->step.out, &g_cli_editor_cells);
            args->walker->step.out = NULL;
            args->walker->walking = args->own ? 0 : args->walker->walking;
            return;
        }
        EMBED_CALL(branch_handed_out, DocumentBranchCtx, .any = handed, .colors = &args->walker->step);
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
    if (s_threaded && (free_at != CLI_DOC_WALKERS) && (walking < s_walkers_most) && (waiting > between))
    {
        CliBranchWalker *const walker = &s_walkers[free_at];
        walker->thread = (CliThread){branch_walk_run, walker, CLI_PRIORITY_WORK};
        walker->walking = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &walker->thread);
    }
}

void cli_document_colors_to(const CliDocumentColoredCfg *args)
{
    const unsigned int spare = cli_processors() - 1u;
    s_colored = args->colored;
    s_threaded = args->threaded;
    s_walkers_most = (spare == 0u) ? 1u : ((spare < CLI_DOC_WALKERS) ? spare : CLI_DOC_WALKERS);
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
        memor.cpy(&(MemoriaCfg){.dst = one->grammar, .src = args->grammar, .bytes = length + 1ull});
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
        one->roles_whole = 0;
        one->regions_moved = 1;
        EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
    }
    return one != NULL;
}

int cli_document_view(const CliDocumentOutKeyFirstCfg *args)
{
    char *const colors_cell =
        (args->out == NULL) ? g_cli_cells->temporary_buf_alloc(CLI_DOC_COLORS_ROOM, &g_cli_editor_cells) : NULL;
    VerbaLinea colors = mmgr_line_over(colors_cell, CLI_DOC_COLORS_ROOM);
    const int made = colors_cell != NULL;
    VerbaLinea *const into = (args->out != NULL) ? args->out : &colors;
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    const int found = one != NULL;
    int any = 0;
    if ((one != NULL) && ((args->out != NULL) || made))
    {
        EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
        one->refused =
            one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
        const CliLines *const lines = &one->lines;
        // a view past the text's last line is read at its last
        one->origin =
            (args->first < lines->lines) ? (unsigned int)args->first : ((lines->lines != 0u) ? lines->lines - 1u : 0u);
        if (args->out == NULL)
        {
            EMBED_CALL(branch_colors_begin, DocumentBranchCtx, .one_read = one, .out = into);
        }
        mmgr_line_ch(into, '[');
        // a paged text's view colored within the lines the page holds alone
        unsigned int low = 0u;
        unsigned int high = 0u;
        EMBED_CALL(branch_bounds, DocumentBranchCtx, .one_read = one, .low_pointer = &low, .high_pointer = &high);
        CliBranchRange range =
            EMBED_CALL(branch_range_begin, DocumentBranchCtx, .one_read = one, .any_pointer = &any, .out = into,
                       .columns = one->columns, .bytes = CLI_DOC_ALL_BYTES, .high = high);
        for (unsigned long long line = (args->first > low) ? args->first : low;
             (one->grammar[0] != '\0') && !one->refused && !range.stale && (line <= args->last) && (line < high);)
        {
            // the view's lines stand below the text's last, which an unsigned int counts
            const unsigned int at = (unsigned int)line;
            // a line colored is passed over; one the walk claims is colored here as well, the view waiting on no
            // coloring of another
            if (lines->begun[at] != CLI_DOC_UNCOLORED)
            {
                line += 1ull;
                continue;
            }
            unsigned int run_first = 0u;
            unsigned int run_end = 0u;
            EMBED_CALL(branch_run_get, DocumentBranchCtx, .one_read = one, .at = at, .down = 1, .low = low,
                       .high = high, .yielding = 0, .first_pointer = &run_first, .end_pointer = &run_end);
            // the view's lines alone, those below it left to the walk, which colors them on a thread of its own; a
            // line past the view's last stands below the run's end, which an unsigned int counts
            run_end = ((unsigned long long)run_end > (args->last + 1ull)) ? (unsigned int)(args->last + 1ull) : run_end;
            EMBED_CALL(branch_range_color, DocumentBranchCtx, .one = one, .first = run_first, .end = run_end,
                       .onward = 0, .range = &range);
            line = run_end;
        }
        EMBED_CALL(branch_range_end, DocumentBranchCtx, .range = &range);
        mmgr_line_ch(into, ']');
        if (args->out == NULL)
        {
            EMBED_CALL(branch_colors_end, DocumentBranchCtx, .one = one, .any = any, .out = into);
        }
        EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
        EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    }
    EMBED_CALL(branch_handed_out, DocumentBranchCtx, .any = any && (args->out == NULL), .colors = &colors);
    if (made)
    {
        g_cli_cells->persistent_buf_release(colors.out, &g_cli_editor_cells);
    }
    return found;
}

// the lines from `from` to before `to` of `one`, those from `passed_from` to before `passed_to` passed over, handed on
// again into `out` as the walk hands its colors, `{"key", "seq", "colors"}`: the runs each saves where they stand, and
// each that saves none that stand not colored, the walk or a view coloring it and handing it on as it comes to it, as
// every line is where `out` is NULL: 1 where any line's runs were written, else 0
EMBED_INLINE int branch_runs_rehanded(const DocumentBranchCtx *args)
{
    int any = 0;
    if (args->out != NULL)
    {
        EMBED_CALL(branch_colors_begin, DocumentBranchCtx, .one_read = args->one, .out = args->out);
        mmgr_line_ch(args->out, '[');
    }
    CliBranchRange range =
        EMBED_CALL(branch_range_begin, DocumentBranchCtx, .one_read = args->one, .any_pointer = &any, .out = args->out,
                   .columns = args->one->columns, .bytes = CLI_DOC_ALL_BYTES, .high = args->to);
    for (unsigned int line = args->from; line < args->to; line += 1u)
    {
        if ((line >= args->passed_from) && (line < args->passed_to))
        {
            continue;
        }
        if ((args->out != NULL) &&
            EMBED_CALL(branch_runs_standing, DocumentBranchCtx, .one_read = args->one, .line = line))
        {
            EMBED_CALL(branch_runs_dispatch, DocumentBranchCtx, .one_read = args->one, .line = line, .range = &range);
        }
        else
        {
            args->one->lines.begun[line] = CLI_DOC_UNCOLORED;
        }
    }
    if (args->out != NULL)
    {
        EMBED_CALL(branch_range_end, DocumentBranchCtx, .range = &range);
        mmgr_line_ch(args->out, ']');
        EMBED_CALL(branch_colors_end, DocumentBranchCtx, .one = args->one, .any = any, .out = args->out);
    }
    return any;
}

// colors handed on again written into `colors`, handed to where the colors go as the lock is given back, and its cell
// given back where `made` is 1
EMBED_INLINE void branch_rehanded_send(const DocumentBranchCtx *args)
{
    branch_handed_out(args);
    if (args->made)
    {
        g_cli_cells->persistent_buf_release(args->colors->out, &g_cli_editor_cells);
    }
}

int cli_document_cache(const CliDocumentKeyFromCfg *args)
{
    char *const colors_cell = g_cli_cells->temporary_buf_alloc(CLI_DOC_COLORS_ROOM, &g_cli_editor_cells);
    VerbaLinea colors = mmgr_line_over(colors_cell, CLI_DOC_COLORS_ROOM);
    const int made = colors.out != NULL;
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    int any = 0;
    if (one != NULL)
    {
        one->refused =
            one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
        CliLines *const lines = &one->lines;
        // the lines the page mirrors stand among those the pages count, which an unsigned int counts
        const unsigned int cached_from = (args->from < lines->lines) ? (unsigned int)args->from : lines->lines;
        const unsigned int cached_to = (args->to < lines->lines) ? (unsigned int)args->to : lines->lines;
        const unsigned int was_from = one->cached_from;
        const unsigned int was_to = (one->cached_to < lines->lines) ? one->cached_to : lines->lines;
        // a paged text's lines the page lets go let their runs go with them
        for (unsigned int line = was_from; one->paged && (line < was_to); line += 1u)
        {
            lines->runs[line] = ((line < cached_from) || (line >= cached_to)) ? CLI_DOC_NO_RUNS : lines->runs[line];
        }
        // a line the page comes to mirror handed the runs it saves, or colored again where it saves none that stand,
        // the state it ends in saved: the line below it is entered in the state it was
        any = EMBED_CALL(branch_runs_rehanded, DocumentBranchCtx, .one = one, .from = cached_from, .to = cached_to,
                         .passed_from = was_from, .passed_to = was_to, .out = made ? &colors : NULL);
        // a paged text's folds are those of the lines the page mirrors
        one->regions_moved =
            one->regions_moved || (one->paged && ((cached_from != one->cached_from) || (cached_to != one->cached_to)));
        one->cached_from = cached_from;
        one->cached_to = cached_to;
        EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
    }
    EMBED_CALL(branch_rehanded_send, DocumentBranchCtx, .any = any, .colors = &colors, .made = made);
    return one != NULL;
}

int cli_document_columns(const CliDocumentKeyColumnsCfg *args)
{
    unsigned long long columns = args->columns;
    char *const colors_cell = g_cli_cells->temporary_buf_alloc(CLI_DOC_COLORS_ROOM, &g_cli_editor_cells);
    VerbaLinea colors = mmgr_line_over(colors_cell, CLI_DOC_COLORS_ROOM);
    const int made = colors.out != NULL;
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    int any = 0;
    if (one != NULL)
    {
        // a view drawing farther than the lines' runs were handed has the lines the page mirrors handed again as far;
        // one drawing less leaves them as they are
        const int farther = (one->columns != 0ull) && ((columns == 0ull) || (columns > one->columns));
        one->columns = (farther || (one->columns == 0ull)) ? columns : one->columns;
        if (farther)
        {
            const unsigned int cached_to = (one->cached_to < one->lines.lines) ? one->cached_to : one->lines.lines;
            any = EMBED_CALL(branch_runs_rehanded, DocumentBranchCtx, .one = one, .from = one->cached_from,
                             .to = cached_to, .passed_from = 0u, .passed_to = 0u, .out = made ? &colors : NULL);
            EMBED_CALL(cli_document_internal.wake, CliDocumentCtx, .one = one);
        }
    }
    EMBED_CALL(branch_rehanded_send, DocumentBranchCtx, .any = any, .colors = &colors, .made = made);
    return one != NULL;
}

int cli_document_runs_write(const CliDocumentOutKeyFirstCfg *args)
{
    CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    const int found = one != NULL;
    int any = 0;
    mmgr_line_ch(args->out, '[');
    if (one != NULL)
    {
        EMBED_CALL(cli_document_internal.holder_add, CliDocumentCtx, .one = one);
        one->refused =
            one->refused || (one->paged && !EMBED_CALL(cli_document_internal.paged_count, CliDocumentCtx, .one = one));
        const CliLines *const lines = &one->lines;
        // the lines asked stand among those the text counts, which an unsigned int counts
        const unsigned int from = (args->first < lines->lines) ? (unsigned int)args->first : lines->lines;
        const unsigned int to = (args->last < lines->lines) ? ((unsigned int)args->last + 1u) : lines->lines;
        // each line's runs the walk saved where they stand, and the lines that save none colored, none past the last
        // asked, their runs written whatever lines the page mirrors
        const unsigned long long columns = args->mirrored ? one->columns : 0ull;
        CliBranchRange range = EMBED_CALL(branch_range_begin, DocumentBranchCtx, .one_read = one, .any_pointer = &any,
                                          .out = args->out, .columns = columns, .bytes = CLI_DOC_ALL_BYTES, .high = to);
        range.asked = 1;
        range.hand_from = from;
        range.hand_to = to;
        for (unsigned int line = from; (one->grammar[0] != '\0') && !one->refused && !range.stale && (line < to);)
        {
            if (EMBED_CALL(branch_runs_standing, DocumentBranchCtx, .one_read = one, .line = line))
            {
                EMBED_CALL(branch_runs_dispatch, DocumentBranchCtx, .one_read = one, .line = line, .range = &range);
                line += 1u;
                continue;
            }
            unsigned int end = line + 1u;
            while ((end < to) && !EMBED_CALL(branch_runs_standing, DocumentBranchCtx, .one_read = one, .line = end))
            {
                end += 1u;
            }
            EMBED_CALL(branch_range_color, DocumentBranchCtx, .one = one, .first = line, .end = end, .onward = 0,
                       .range = &range);
            // the lines after them colored again with them written with them
            line = (range.next > end) ? range.next : end;
        }
        EMBED_CALL(branch_range_end, DocumentBranchCtx, .range = &range);
        // the lines the page mirrors take the lines asked in, joined to them where the two meet and in their place
        // where they do not
        if (args->mirrored)
        {
            const unsigned int cached_from = one->cached_from;
            const unsigned int cached_to = one->cached_to;
            const int meet = (from <= cached_to) && (to >= cached_from) && (cached_from < cached_to);
            one->cached_from = (meet && (cached_from < from)) ? cached_from : from;
            one->cached_to = (meet && (cached_to > to)) ? cached_to : to;
        }
        EMBED_CALL(cli_document_internal.holder_end, CliDocumentCtx, .one = one);
    }
    mmgr_line_ch(args->out, ']');
    return found;
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

char *cli_document_text_take(const CliDocumentKeyCfg *args)
{
    unsigned long long *size = args->size_pointer;
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    char *taken = NULL;
    if ((one != NULL) && !one->paged)
    {
        const CliLines *const lines = &one->lines;
        // each unit written as three bytes at the most, each line's newline, and the NUL after the last
        unsigned long long room = 1ull;
        for (unsigned int line = 0u; line < lines->lines; line += 1u)
        {
            room += (3ull * lines->length[line]) + 1ull;
        }
        taken = g_cli_cells->persistent_buf_alloc(room, args->owner);
        if (taken != NULL)
        {
            VerbaLinea text = {taken, room, 0ull};
            for (unsigned int line = 0u; line < lines->lines; line += 1u)
            {
                EMBED_CALL(branch_line_put, DocumentBranchCtx, .lines = lines, .line = line, .out = &text);
                mmgr_line_put(&text, ((line + 1u) < lines->lines) ? "\n" : "");
            }
            taken[text.at] = '\0';
            *size = text.at;
        }
    }
    return taken;
}

char *cli_document_lines_take(const CliDocumentKeyCfg *args)
{
    unsigned long long *size = args->size_pointer;
    char *const written_cell = g_cli_cells->temporary_buf_alloc(CLI_DOC_COLORS_ROOM, args->owner);
    VerbaLinea written = mmgr_line_over(written_cell, CLI_DOC_COLORS_ROOM);
    const int made = written.out != NULL;
    VerbaLinea *const text = &written;
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    const int found = one != NULL;
    VerbaLinea entered = {args->state, args->room, 0ull};
    // a paged text's lines, read through the pages with the lock given back: its key, and the lines asked
    char paged[CLI_NAME_MAX_LEN];
    paged[0] = '\0';
    unsigned int from = 0u;
    unsigned int to = 0u;
    if (made && (one != NULL))
    {
        const CliLines *const lines = &one->lines;
        // the lines asked stand below the text's last, which an unsigned int counts
        to = (args->last < lines->lines) ? (unsigned int)args->last : (lines->lines - 1u);
        from = (args->first <= to) ? (unsigned int)args->first : to;
        const unsigned int begun = (lines->lines != 0u) ? lines->begun[from] : CLI_DOC_UNCOLORED;
        // the colorer's own state the first is entered in, the scope after the mark left out
        const char *const begun_in =
            ((begun != CLI_DOC_UNCOLORED) && (one->state != NULL)) ? (one->state + begun) : "root";
        const char *const mark =
            cellul.chr(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_CELLS_POOL, .byte = CLI_DOC_SCOPE_MARK});
        mmgr_line_put_n(&entered, begun_in,
                        (mark != NULL) ? (unsigned long long)(mark - begun_in)
                                       : cellul.len(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_CELLS_POOL}));
        if (one->paged && (lines->lines != 0u))
        {
            memor.cpy(&(MemoriaCfg){.dst = paged, .src = one->key, .bytes = sizeof(paged)});
        }
        for (unsigned int at = from; !one->paged && (lines->lines != 0u) && (at <= to); at += 1u)
        {
            EMBED_CALL(branch_line_put, DocumentBranchCtx, .lines = lines, .line = at, .out = text);
            mmgr_line_put(text, (at < to) ? "\n" : "");
        }
    }
    if (paged[0] != '\0')
    {
        const unsigned long long wanted = (unsigned long long)(to + 1u - from);
        const char **const starts =
            g_cli_cells->temporary_buf_alloc(wanted * sizeof(const char *), &g_cli_editor_cells);
        unsigned long long count = 0ull;
        char *const taken =
            (starts != NULL) ? EMBED_CALL(cli_pages_from.lines_take, CliPagesFromCfg, .key = paged, .from = from,
                                          .to = to + 1u, .line = starts, .count = &count, .owner = &g_cli_editor_cells)
                             : NULL;
        for (unsigned long long at = 0ull; (taken != NULL) && (at < count); at += 1ull)
        {
            mmgr_line_put(text, starts[at]);
            mmgr_line_put(text, ((from + at) < to) ? "\n" : "");
        }
        g_cli_cells->persistent_buf_release(taken, &g_cli_editor_cells);
        g_cli_cells->persistent_buf_release(starts, &g_cli_editor_cells);
    }
    if ((mmgr_line_finish(&entered) == 0ull) && (args->room != 0ull))
    {
        args->state[0] = '\0';
    }
    const int taken = made && found && mmgr_line_ok(text);
    if (made && !taken)
    {
        g_cli_cells->persistent_buf_release(written.out, args->owner);
    }
    *size = taken ? text->at : 0ull;
    return taken ? written.out : NULL;
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

int cli_document_text_write(const CliDocumentOutKeyCfg *args)
{
    unsigned long long size = 0ull;
    char *const text = EMBED_CALL(cli_document_key.lines_take, CliDocumentKeyCfg, .key = args->key,
                                  .first = args->first, .last = args->last, .size_pointer = &size, .state = args->state,
                                  .room = args->room, .owner = &g_cli_editor_cells);
    if (text != NULL)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = text,
                   .length = size);
        g_cli_cells->persistent_buf_release(text, &g_cli_editor_cells);
    }
    return text != NULL;
}
