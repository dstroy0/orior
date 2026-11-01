// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_store.c: the grammars stored, each a cell of its own under its key, given back when a grammar is put in its
// place; a grammar read, and lines colored by the grammar stored under a key. The lock is taken to find the grammar and
// to store one, never as lines are colored: a coloring holds the grammar it reads by a count in the grammar's cell and
// works in a room it claims by a mark, so as many colorings run at once as threads ask for them. The cells are the one
// cellblock's, counted to the grammars' own mark, which their report gives
#include "highlight_internal.h"

#include <stdatomic.h>

// the grammars the list of those stored is taken with
#define CLI_GRAMMARS_MOST 1024u

// A grammar's cell begins with its head: how many colorings read the grammar, raised under the lock as each finds it
// and lowered with no lock taken as each ends, and above them the mark that the grammar was put out of its place by
// another stored under its key, the cell given back by the last coloring to end, or at once where none reads it. The
// grammar's parts follow the head, which takes a whole word
#define CLI_GRAMMAR_PUT_OUT (1ull << 63u)
typedef struct
{
    _Atomic unsigned long long readers;
} CliGrammarHead;
#define CLI_GRAMMAR_HEAD ((sizeof(CliGrammarHead) + 7ull) & ~7ull)

// A grammar stored: its key, its cell and the cell's bytes, and its parts there
typedef struct
{
    char key[CLI_NAME_MAX_LEN];
    int used;
    unsigned char *region;
    unsigned long long size;
    CliGrammar grammar;
} CliGrammarEntry;

// the grammars stored, the grammar read, the lock the grammars are stored and read under, and what the grammars and the
// rooms occupy in the cellblock
static CliGrammarEntry *s_grammars;
static unsigned int s_grammar_room;
static CliGrammar s_read;
static CliLock s_lock = CLI_LOCK_INIT;

void cli_highlight_cells_usage_written(VerbaLinea *out)
{
    cli_cells_owner_usage_written(&g_cli_grammars_cells, out);
}

// the head of the grammar whose cell is `region`
static CliGrammarHead *cli_grammar_head(unsigned char *region)
{
    return (CliGrammarHead *)(void *)region;
}

// the grammar whose cell is `region` put out of its place: its cell given back at once where no coloring reads it, else
// by the last to end; under the lock
static void cli_grammar_put_out(unsigned char *region)
{
    const unsigned long long readers =
        atomic_fetch_or_explicit(&cli_grammar_head(region)->readers, CLI_GRAMMAR_PUT_OUT, memory_order_acq_rel);
    if (readers == 0ull)
    {
        g_cli_cells->persistent_buf_release(region, &g_cli_grammars_cells);
    }
}

// a coloring's reading of the grammar whose cell is `region` ended, the cell given back where it was the last to read
// one put out of its place
static void cli_grammar_let_go(unsigned char *region)
{
    const unsigned long long readers =
        atomic_fetch_sub_explicit(&cli_grammar_head(region)->readers, 1ull, memory_order_acq_rel);
    if (readers == (CLI_GRAMMAR_PUT_OUT | 1ull))
    {
        g_cli_cells->persistent_buf_release(region, &g_cli_grammars_cells);
    }
}

// a part's bytes rounded up to a whole word
static unsigned long long cli_words_of(unsigned long long bytes)
{
    return (bytes + 7ull) & ~7ull;
}

// the place `at` bytes into a region, none where the region is none and only its size is counted
static void *cli_place(unsigned char *region, unsigned long long at)
{
    return (region != NULL) ? (void *)(region + at) : NULL;
}

// each part of a grammar laid out at `region` by its counts, in order, each on a word: the bytes they take
static unsigned long long cli_laid(CliGrammar *grammar, unsigned char *region)
{
    unsigned long long at = 0ull;
    grammar->node = (CliRegexpNode *)cli_place(region, at);
    at += cli_words_of(grammar->nodes * sizeof(CliRegexpNode));
    grammar->item = (CliRegexpItem *)cli_place(region, at);
    at += cli_words_of(grammar->items * sizeof(CliRegexpItem));
    grammar->part = (unsigned int *)cli_place(region, at);
    at += cli_words_of(grammar->parts * sizeof(unsigned int));
    grammar->part_first = (CliRegexpFirst *)cli_place(region, at);
    at += cli_words_of(grammar->parts * sizeof(CliRegexpFirst));
    grammar->rule = (CliHighlightRule *)cli_place(region, at);
    at += cli_words_of(grammar->rules * sizeof(CliHighlightRule));
    grammar->token = (CliHighlightToken *)cli_place(region, at);
    at += cli_words_of(grammar->tokens * sizeof(CliHighlightToken));
    grammar->cases = (CliHighlightCase *)cli_place(region, at);
    at += cli_words_of(grammar->case_count * sizeof(CliHighlightCase));
    grammar->listed = (unsigned int *)cli_place(region, at);
    at += cli_words_of(grammar->listed_count * sizeof(unsigned int));
    grammar->state = (CliHighlightList *)cli_place(region, at);
    at += cli_words_of(grammar->states * sizeof(CliHighlightList));
    grammar->word = (unsigned int *)cli_place(region, at);
    at += cli_words_of(grammar->word_count * sizeof(unsigned int));
    grammar->words = (CliHighlightList *)cli_place(region, at);
    at += cli_words_of(grammar->word_lists * sizeof(CliHighlightList));
    grammar->name = (unsigned int *)cli_place(region, at);
    at += cli_words_of(grammar->names * sizeof(unsigned int));
    grammar->text = (char *)cli_place(region, at);
    at += cli_words_of(grammar->text_size);
    return at;
}

// the grammar read copied into a region laid out at `region`, stored as `copy`
static void cli_copied(const CliGrammar *read, unsigned char *region, CliGrammar *copy)
{
    *copy = *read;
    cli_laid(copy, region);
    memor.cpy(&(MemoriaCfg){.dst = copy->node, .src = read->node, .bytes = read->nodes * sizeof(CliRegexpNode)});
    memor.cpy(&(MemoriaCfg){.dst = copy->item, .src = read->item, .bytes = read->items * sizeof(CliRegexpItem)});
    memor.cpy(&(MemoriaCfg){.dst = copy->part, .src = read->part, .bytes = read->parts * sizeof(unsigned int)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->part_first, .src = read->part_first, .bytes = read->parts * sizeof(CliRegexpFirst)});
    memor.cpy(&(MemoriaCfg){.dst = copy->rule, .src = read->rule, .bytes = read->rules * sizeof(CliHighlightRule)});
    memor.cpy(&(MemoriaCfg){.dst = copy->token, .src = read->token, .bytes = read->tokens * sizeof(CliHighlightToken)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->cases, .src = read->cases, .bytes = read->case_count * sizeof(CliHighlightCase)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->listed, .src = read->listed, .bytes = read->listed_count * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){.dst = copy->state, .src = read->state, .bytes = read->states * sizeof(CliHighlightList)});
    memor.cpy(&(MemoriaCfg){.dst = copy->word, .src = read->word, .bytes = read->word_count * sizeof(unsigned int)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->words, .src = read->words, .bytes = read->word_lists * sizeof(CliHighlightList)});
    memor.cpy(&(MemoriaCfg){.dst = copy->name, .src = read->name, .bytes = read->names * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){.dst = copy->text, .src = read->text, .bytes = read->text_size});
}

// the list of the grammars stored taken at its most, every place empty, where it stands untaken: 1, or 0 where it
// stands taken or the cellblock cannot meet it; under the lock
static int cli_grammars_made(void)
{
    CliGrammarEntry *const cell =
        (s_grammar_room == 0u) ? g_cli_cells->persistent_buf_alloc(
                                     (unsigned long long)CLI_GRAMMARS_MOST * sizeof(*cell), &g_cli_grammars_cells)
                               : NULL;
    if (cell == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = cell, .val = 0, .bytes = (size_t)CLI_GRAMMARS_MOST * sizeof(*cell)});
    s_grammars = cell;
    s_grammar_room = CLI_GRAMMARS_MOST;
    return 1;
}

// the grammar at `def` read into `room` and stored under `key` in a cell of its own, one stored there before put out of
// its place first: 1, or 0 with why given
static int cli_grammar_stored(const char *key, const CliJson *json, unsigned int def, CliGrammarRoom *room,
                              VerbaLinea *err_msg)
{
    if (!cli_highlight_read(json, def, room, &s_read, err_msg))
    {
        return 0;
    }
    CliGrammar laid = s_read;
    const unsigned long long size = cli_laid(&laid, NULL);
    unsigned int entry_at = s_grammar_room;
    unsigned int free_at = s_grammar_room;
    for (unsigned int at = 0u; at < s_grammar_room; at += 1u)
    {
        entry_at = (s_grammars[at].used &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = s_grammars[at].key, .other = key, .cap = CLI_CELLS_POOL})))
                       ? at
                       : entry_at;
        free_at = (!s_grammars[at].used && (free_at == s_grammar_room)) ? at : free_at;
    }
    if (entry_at != s_grammar_room)
    {
        cli_grammar_put_out(s_grammars[entry_at].region);
        s_grammars[entry_at].used = 0;
        free_at = entry_at;
    }
    if (cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN)
    {
        mmgr_line_put(err_msg, "a grammar's key is longer than orior_cli supports");
        return 0;
    }
    // no place free: the list is taken where it stands untaken
    if ((free_at == s_grammar_room) && !cli_grammars_made())
    {
        mmgr_line_put(err_msg, "the grammars stored take more than orior_cli supports");
        return 0;
    }
    unsigned char *const region = g_cli_cells->persistent_buf_alloc(CLI_GRAMMAR_HEAD + size, &g_cli_grammars_cells);
    if (region == NULL)
    {
        mmgr_line_put(err_msg, "the grammars stored take more than orior_cli supports");
        return 0;
    }
    atomic_init(&cli_grammar_head(region)->readers, 0ull);
    CliGrammarEntry *const grammar = &s_grammars[free_at];
    memor.cpy(&(MemoriaCfg){.dst = grammar->key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    grammar->region = region;
    grammar->size = size;
    cli_copied(&s_read, region + CLI_GRAMMAR_HEAD, &grammar->grammar);
    grammar->used = 1;
    return 1;
}

// a part's count as the grammar's JSON bounds it, no more than the part's limit
static unsigned int cli_bounded(unsigned long long bound, unsigned int limit)
{
    // a count no more than the limit, which an unsigned int stores
    return (bound < limit) ? (unsigned int)bound : limit;
}

// a room's part of `bytes` placed `*at` bytes into its cell, none where the cell is none and only the room's size is
// counted, `*at` moved past it to the next word
static void *cli_room_part(unsigned char *cell, unsigned long long *at, unsigned long long bytes)
{
    void *const part = cli_place(cell, *at);
    *at += cli_words_of(bytes);
    return part;
}

// each part of a read's room placed in `cell` in order, at the counts the room contains: the bytes they take
static unsigned long long cli_grammar_parts(CliGrammarRoom *room, unsigned char *cell)
{
    unsigned long long at = 0ull;
    room->node = cli_room_part(cell, &at, room->nodes_most * sizeof(CliRegexpNode));
    room->item = cli_room_part(cell, &at, room->items_most * sizeof(CliRegexpItem));
    room->part = cli_room_part(cell, &at, room->parts_most * sizeof(unsigned int));
    room->part_first = cli_room_part(cell, &at, room->parts_most * sizeof(CliRegexpFirst));
    room->rule = cli_room_part(cell, &at, room->rules_most * sizeof(CliHighlightRule));
    room->token = cli_room_part(cell, &at, room->tokens_most * sizeof(CliHighlightToken));
    room->cases = cli_room_part(cell, &at, room->tokens_most * sizeof(CliHighlightCase));
    room->listed = cli_room_part(cell, &at, room->listed_most * sizeof(unsigned int));
    room->state = cli_room_part(cell, &at, room->states_most * sizeof(CliHighlightList));
    room->word = cli_room_part(cell, &at, room->words_most * sizeof(unsigned int));
    room->words = cli_room_part(cell, &at, room->word_lists_most * sizeof(CliHighlightList));
    room->name = cli_room_part(cell, &at, room->names_most * sizeof(unsigned int));
    room->text = cli_room_part(cell, &at, room->text_most);
    room->own = cli_room_part(cell, &at, room->own_most * sizeof(unsigned int));
    room->own_state = cli_room_part(cell, &at, room->states_most * sizeof(CliHighlightList));
    room->pattern = cli_room_part(cell, &at, sizeof(CliRegexpRoom));
    return at;
}

// The parts of a read's room at the sizes the grammar's JSON bounds, its `nodes` and the `used` bytes of its texts: a
// pattern's nodes and parts no more than three for each of its letters and one more, its items no more than its
// letters; rules, own entries, states, words and their lists no more than the JSON's nodes, tokens two for each and
// names one more; the texts no more than four bytes for each byte and a terminator each; and the rules listed, which a
// state's includes repeat, at their limit. The room is one cell, sized on a first pass over its parts and placed in on
// a second: the cell, for the caller to give back, or NULL where the cellblock cannot meet it
static unsigned char *cli_grammar_roomed(CliGrammarRoom *room, const CliJson *json)
{
    const unsigned long long nodes = json->nodes;
    const unsigned long long bytes = json->used;
    room->nodes_most = cli_bounded(3ull * (bytes + nodes), CLI_GRAMMAR_NODES);
    room->items_most = cli_bounded(bytes, CLI_GRAMMAR_ITEMS);
    room->parts_most = cli_bounded(3ull * (bytes + nodes), CLI_GRAMMAR_PARTS);
    room->rules_most = cli_bounded(nodes, CLI_GRAMMAR_RULES);
    room->tokens_most = cli_bounded(2ull * nodes, CLI_GRAMMAR_TOKENS);
    room->listed_most = CLI_GRAMMAR_LISTED;
    room->states_most = cli_bounded(nodes, CLI_GRAMMAR_STATES);
    room->words_most = cli_bounded(nodes, CLI_GRAMMAR_WORDS);
    room->word_lists_most = cli_bounded(nodes, CLI_GRAMMAR_WORD_LISTS);
    room->names_most = cli_bounded(nodes + 1ull, CLI_GRAMMAR_NAMES);
    room->own_most = cli_bounded(nodes, CLI_GRAMMAR_OWN);
    room->text_most = cli_bounded((4ull * bytes) + nodes + 1ull, CLI_GRAMMAR_TEXT);
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(cli_grammar_parts(room, NULL), &g_cli_grammars_cells);
    if (cell != NULL)
    {
        (void)cli_grammar_parts(room, cell);
    }
    return cell;
}

// each part of a coloring's room placed in `cell` in order, at the counts the room contains: the bytes they take
static unsigned long long cli_coloring_parts(CliHighlightRoom *room, unsigned char *cell)
{
    unsigned long long at = 0ull;
    room->letters = cli_room_part(cell, &at, room->most * sizeof(unsigned int));
    room->cols = cli_room_part(cell, &at, (room->most + 1ull) * sizeof(unsigned int));
    // a run for each letter, one past the colored part and one for an empty line, and a mark for each state stacked and
    // one more
    room->run_col = cli_room_part(cell, &at, (room->most + 3ull + CLI_HIGHLIGHT_STACK) * sizeof(unsigned int));
    room->run_name = cli_room_part(cell, &at, (room->most + 3ull + CLI_HIGHLIGHT_STACK) * sizeof(unsigned int));
    room->stack = cli_room_part(cell, &at, (unsigned long long)CLI_HIGHLIGHT_STACK * CLI_NAME_MAX_LEN);
    room->picked = cli_room_part(cell, &at, (4ull * room->most) + 1ull);
    room->key = cli_room_part(cell, &at, (4ull * room->most) + 1ull);
    room->endings = cli_room_part(cell, &at, room->endings_size);
    return at;
}

// the room of a coloring of `count` lines, taken in one cell as the coloring begins, as wide as the longest line's
// letters, a line's letters being no more than its bytes nor than a line is read to, sized on a first pass over its
// parts and placed in on a second: the room, its cell into `*cell` for the coloring to give back as it ends, or NULL
// where the cellblock cannot meet it
static CliHighlightRoom *cli_coloring_roomed(CliHighlightRoom *room, unsigned char **cell, const char *const *lines,
                                             unsigned int count)
{
    unsigned long long most = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = lines[at], .cap = CLI_CELLS_POOL});
        most = (bytes > most) ? bytes : most;
    }
    room->most = (most < CLI_HIGHLIGHT_LETTERS) ? most : CLI_HIGHLIGHT_LETTERS;
    room->count = count;
    // each line's ending, and the byte the endings reserve back for their terminator
    room->endings_size = (room->count * CLI_HIGHLIGHT_ENDING) + 1ull;
    *cell = g_cli_cells->temporary_buf_alloc(cli_coloring_parts(room, NULL), &g_cli_grammars_cells);
    if (*cell == NULL)
    {
        return NULL;
    }
    (void)cli_coloring_parts(room, *cell);
    return room;
}

int cli_highlight_store(const char *key, const CliJson *json, unsigned int def, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    CliGrammarRoom room;
    unsigned char *const cell = cli_grammar_roomed(&room, json);
    const int stored = (cell != NULL) && cli_grammar_stored(key, json, def, &room, err_msg);
    g_cli_cells->persistent_buf_release(cell, &g_cli_grammars_cells);
    cli_lock_give(&s_lock);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "no room to read a grammar into");
    }
    return stored;
}

// The lines handed over: their state, the lines and their count, and whether the last ends the text; and where they
// go, the coloring written as JSON into `out`, or each line's leaves handed to `each`, the state the last ends in into
// `ended`
typedef struct
{
    const char *state;
    const char *const *lines;
    unsigned int count;
    int ends;
    VerbaLinea *out;
    CliLeavesEach each;
    void *context;
    VerbaLinea *ended;
} CliColored;

// the place of the grammar stored under `key`, s_grammar_room where none is; under the lock
static unsigned int cli_grammar_entry_of(const char *key)
{
    unsigned int entry_at = 0u;
    while ((entry_at < s_grammar_room) &&
           !(s_grammars[entry_at].used &&
             (cellul.eq(&(CatenaFinitaCfg){.src = s_grammars[entry_at].key, .other = key, .cap = CLI_CELLS_POOL}))))
    {
        entry_at += 1u;
    }
    return entry_at;
}

// the lines colored with the grammar stored under `key`, as `colored` says where they go, the lock taken to find the
// grammar and count the coloring among its readers alone, and the lines colored in a room claimed with no lock taken:
// 1, or 0 with why given
static int cli_colored(const char *key, const CliColored *colored, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    const unsigned int entry_at = cli_grammar_entry_of(key);
    const int has_entry = entry_at != s_grammar_room;
    unsigned char *const region = has_entry ? s_grammars[entry_at].region : NULL;
    CliGrammar grammar;
    if (has_entry)
    {
        grammar = s_grammars[entry_at].grammar;
        (void)atomic_fetch_add_explicit(&cli_grammar_head(region)->readers, 1ull, memory_order_relaxed);
    }
    cli_lock_give(&s_lock);
    if (!has_entry)
    {
        mmgr_line_put(err_msg, "no grammar is stored under ");
        mmgr_line_put(err_msg, key);
        return 0;
    }
    CliHighlightRoom taken;
    unsigned char *cell = NULL;
    CliHighlightRoom *const room = cli_coloring_roomed(&taken, &cell, colored->lines, colored->count);
    if ((room != NULL) && (colored->each != NULL))
    {
        cli_highlight_leaves_of(&grammar, room, colored->state, colored->lines, colored->count, colored->ends,
                                colored->each, colored->context, colored->ended);
    }
    else if (room != NULL)
    {
        cli_highlight_lines(&grammar, room, colored->state, colored->lines, colored->count, colored->out);
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_grammars_cells);
    cli_grammar_let_go(region);
    if (room == NULL)
    {
        mmgr_line_put(err_msg, "the coloring could not be taken");
        return 0;
    }
    return 1;
}

int cli_highlight_color(const char *key, const char *state, const char *const *lines, unsigned int count,
                        VerbaLinea *out, VerbaLinea *err_msg)
{
    const CliColored colored = {state, lines, count, 0, out, NULL, NULL, NULL};
    return cli_colored(key, &colored, err_msg);
}

int cli_highlight_leaves(const char *key, const char *state, const char *const *lines, unsigned int count, int ends,
                         CliLeavesEach each, void *context, VerbaLinea *ended, VerbaLinea *err_msg)
{
    const CliColored colored = {state, lines, count, ends, NULL, each, context, ended};
    return cli_colored(key, &colored, err_msg);
}

int cli_highlight_names_written(const char *key, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    const unsigned int entry_at = cli_grammar_entry_of(key);
    const int has_entry = entry_at != s_grammar_room;
    if (has_entry)
    {
        const CliGrammar *const grammar = &s_grammars[entry_at].grammar;
        mmgr_line_ch(out, '[');
        for (unsigned int at = 0u; at < grammar->names; at += 1u)
        {
            const char *const name = grammar->text + grammar->name[at];
            mmgr_line_put(out, (at != 0u) ? "," : "");
            cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(out, ']');
    }
    cli_lock_give(&s_lock);
    return has_entry;
}

unsigned int cli_highlight_names_each(const char *key, CliNameEach each, void *context)
{
    cli_lock_take(&s_lock);
    const unsigned int entry_at = cli_grammar_entry_of(key);
    const CliGrammar *const grammar = (entry_at != s_grammar_room) ? &s_grammars[entry_at].grammar : NULL;
    const unsigned int names = (grammar != NULL) ? grammar->names : 0u;
    for (unsigned int at = 0u; at < names; at += 1u)
    {
        each(context, at, grammar->text + grammar->name[at]);
    }
    cli_lock_give(&s_lock);
    return names;
}

void cli_highlight_runs_written(const CliLeaves *leaves, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int run = 0u; run < leaves->runs; run += 1u)
    {
        mmgr_line_put(out, (run != 0u) ? "," : "");
        mmgr_line_decimal(out, leaves->col[run], 1u);
        mmgr_line_ch(out, ',');
        mmgr_line_decimal(out, leaves->name[run], 1u);
    }
    mmgr_line_ch(out, ']');
}

unsigned int cli_highlight_state(const CliGrammar *grammar, const char *name, unsigned long long length)
{
    for (unsigned int at = 0u; at < grammar->states; at += 1u)
    {
        const char *const named = grammar->text + grammar->state[at].name;
        if ((cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = named, .other = name, .bytes = length}) == 0))
        {
            return at;
        }
    }
    return CLI_HIGHLIGHT_NONE;
}

int cli_highlight_has_word(const CliGrammar *grammar, const CliHighlightList *list, const char *word,
                           unsigned long long length)
{
    unsigned int low = 0u;
    unsigned int high = list->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        // the stored word is terminated and `word` contains no terminator within its length: a compare over `length`
        // bytes orders a shorter stored word by its terminator, and a longer one by the byte after `length`
        const char *const word_at = grammar->text + grammar->word[list->first + middle];
        int order = memor.cmp(&(MemoriaCfg){.src = word_at, .other = word, .bytes = length});
        order = (order != 0) ? order : ((word_at[length] != '\0') ? 1 : 0);
        if (order == 0)
        {
            return 1;
        }
        if (order < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return 0;
}
