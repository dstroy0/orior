// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_store.c: the grammars stored, each a cell of its own under its key, given back when a grammar is put in its
// place; a grammar read, and lines colored by the grammar stored under a key. The lock is taken to find the grammar and
// to store one, never as lines are colored: a coloring holds the grammar it reads by a count in the grammar's cell and
// works in a room it claims by a mark, so as many colorings run at once as threads ask for them. The cells are the one
// cellblock's, counted to the grammars' own mark, which their report gives
#include "../command_line/callback_cfg.h"
#include "highlight_internal.h"

#include <stdatomic.h>

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

void cli_highlight_cells_usage_write(const CliHighlightCfg *args)
{
    EMBED_CALL(cli_cells_table.owner_usage_write, CliCellsCfg, .owner = &g_cli_grammars_cells, .out = args->out);
}

typedef struct
{
    unsigned char *region;
    unsigned long long bytes;
    unsigned long long at;
    CliGrammar *grammar;
    const CliGrammar *read;
    CliGrammar *copy;
    const char *key;
    const CliJson *json;
    unsigned int def;
    CliGrammarRoom *room;
    VerbaLinea *err_msg;
    unsigned long long bound;
    unsigned int limit;
    unsigned char *cell;
    unsigned long long *at_pointer;
    CliHighlightRoom *room_highlight_room_pointer;
    unsigned char **cell_pointer;
    const char *const *lines;
    unsigned int count;
    const CliColored *colored;
} HighlightStoreCtx;
// the head of the grammar whose cell is `region`
EMBED_INLINE CliGrammarHead *cli_grammar_head(const HighlightStoreCtx *args)
{
    return (CliGrammarHead *)(void *)args->region;
}

// the grammar whose cell is `region` put out of its place: its cell given back at once where no coloring reads it, else
// by the last to end; under the lock
EMBED_INLINE void cli_grammar_put_out(const HighlightStoreCtx *args)
{
    const unsigned long long readers =
        atomic_fetch_or_explicit(&cli_grammar_head(args)->readers, CLI_GRAMMAR_PUT_OUT, memory_order_acq_rel);
    if (readers == 0ull)
    {
        g_cli_cells->persistent_buf_release(args->region, &g_cli_grammars_cells);
    }
}

// a coloring's reading of the grammar whose cell is `region` ended, the cell given back where it was the last to read
// one put out of its place
EMBED_INLINE void cli_grammar_let_go(const HighlightStoreCtx *args)
{
    const unsigned long long readers =
        atomic_fetch_sub_explicit(&cli_grammar_head(args)->readers, 1ull, memory_order_acq_rel);
    if (readers == (CLI_GRAMMAR_PUT_OUT | 1ull))
    {
        g_cli_cells->persistent_buf_release(args->region, &g_cli_grammars_cells);
    }
}

// a part's bytes rounded up to a whole word
EMBED_INLINE unsigned long long cli_words_get(const HighlightStoreCtx *args)
{
    return (args->bytes + 7ull) & ~7ull;
}

// the place `at` bytes into a region, none where the region is none and only its size is counted
EMBED_INLINE void *cli_place(const HighlightStoreCtx *args)
{
    return (args->region != NULL) ? (void *)(args->region + args->at) : NULL;
}

// each part of a grammar laid out at `region` by its counts, in order, each on a word: the bytes they take
EMBED_INLINE unsigned long long cli_layout(const HighlightStoreCtx *args)
{
    unsigned long long at = 0ull;
    args->grammar->node = (CliRegexpNode *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->nodes * sizeof(CliRegexpNode));
    args->grammar->item = (CliRegexpItem *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->items * sizeof(CliRegexpItem));
    args->grammar->part = (unsigned int *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->parts * sizeof(unsigned int));
    args->grammar->part_first =
        (CliRegexpFirst *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->parts * sizeof(CliRegexpFirst));
    args->grammar->rule =
        (CliHighlightRule *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->rules * sizeof(CliHighlightRule));
    args->grammar->token =
        (CliHighlightToken *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->tokens * sizeof(CliHighlightToken));
    args->grammar->cases =
        (CliHighlightCase *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->case_count * sizeof(CliHighlightCase));
    args->grammar->listed = (unsigned int *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->listed_count * sizeof(unsigned int));
    args->grammar->state =
        (CliHighlightList *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->states * sizeof(CliHighlightList));
    args->grammar->word = (unsigned int *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->word_count * sizeof(unsigned int));
    args->grammar->words =
        (CliHighlightList *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->word_lists * sizeof(CliHighlightList));
    args->grammar->name = (unsigned int *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->names * sizeof(unsigned int));
    args->grammar->text = (char *)EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->text_size);
    return at;
}

// the grammar read copied into a region laid out at `region`, stored as `copy`
EMBED_INLINE void cli_copy(const HighlightStoreCtx *args)
{
    CliGrammar *copy = args->copy;
    *copy = *args->read;
    EMBED_CALL(cli_layout, HighlightStoreCtx, .grammar = copy, .region = args->region);
    memor.cpy(
        &(MemoriaCfg){.dst = copy->node, .src = args->read->node, .bytes = args->read->nodes * sizeof(CliRegexpNode)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->item, .src = args->read->item, .bytes = args->read->items * sizeof(CliRegexpItem)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->part, .src = args->read->part, .bytes = args->read->parts * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->part_first, .src = args->read->part_first, .bytes = args->read->parts * sizeof(CliRegexpFirst)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->rule, .src = args->read->rule, .bytes = args->read->rules * sizeof(CliHighlightRule)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->token, .src = args->read->token, .bytes = args->read->tokens * sizeof(CliHighlightToken)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->cases, .src = args->read->cases, .bytes = args->read->case_count * sizeof(CliHighlightCase)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->listed, .src = args->read->listed, .bytes = args->read->listed_count * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->state, .src = args->read->state, .bytes = args->read->states * sizeof(CliHighlightList)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->word, .src = args->read->word, .bytes = args->read->word_count * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->words, .src = args->read->words, .bytes = args->read->word_lists * sizeof(CliHighlightList)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->name, .src = args->read->name, .bytes = args->read->names * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){.dst = copy->text, .src = args->read->text, .bytes = args->read->text_size});
}

// the list of the grammars stored taken at its most, every place empty, where it stands untaken: 1, or 0 where it
// stands taken or the cellblock cannot meet it; under the lock
static int cli_grammars_init(void)
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
EMBED_INLINE int cli_grammar_store(const HighlightStoreCtx *args)
{
    const CliJson *json = args->json;
    unsigned int def = args->def;
    CliGrammarRoom *room = args->room;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_highlight_json_internal.read, CliHighlightJsonCtx, .json = json, .def = def,
                    .room_grammar_room_pointer = room, .grammar_out = &s_read, .err_msg = err_msg))
    {
        return 0;
    }
    CliGrammar laid = s_read;
    const unsigned long long size = EMBED_CALL(cli_layout, HighlightStoreCtx, .grammar = &laid, .region = NULL);
    unsigned int entry_at = s_grammar_room;
    unsigned int free_at = s_grammar_room;
    for (unsigned int at = 0u; at < s_grammar_room; at += 1u)
    {
        entry_at =
            (s_grammars[at].used &&
             (cellul.eq(&(CatenaFinitaCfg){.src = s_grammars[at].key, .other = args->key, .cap = CLI_CELLS_POOL})))
                ? at
                : entry_at;
        free_at = (!s_grammars[at].used && (free_at == s_grammar_room)) ? at : free_at;
    }
    if (entry_at != s_grammar_room)
    {
        EMBED_CALL(cli_grammar_put_out, HighlightStoreCtx, .region = s_grammars[entry_at].region);
        s_grammars[entry_at].used = 0;
        free_at = entry_at;
    }
    if (cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN)
    {
        mmgr_line_put(err_msg, "a grammar's key is longer than orior_cli supports");
        return 0;
    }
    // no place free: the list is taken where it stands untaken
    if ((free_at == s_grammar_room) && !cli_grammars_init())
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
    atomic_init(&EMBED_CALL(cli_grammar_head, HighlightStoreCtx, .region = region)->readers, 0ull);
    CliGrammarEntry *const grammar = &s_grammars[free_at];
    memor.cpy(&(MemoriaCfg){.dst = grammar->key,
                            .src = args->key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1u});
    grammar->region = region;
    grammar->size = size;
    EMBED_CALL(cli_copy, HighlightStoreCtx, .read = &s_read, .region = region + CLI_GRAMMAR_HEAD,
               .copy = &grammar->grammar);
    grammar->used = 1;
    return 1;
}

// a part's count as the grammar's JSON bounds it, no more than the part's limit
EMBED_INLINE unsigned int cli_bound(const HighlightStoreCtx *args)
{
    // a count no more than the limit, which an unsigned int stores
    return (args->bound < args->limit) ? (unsigned int)args->bound : args->limit;
}

// a room's part of `bytes` placed `*at` bytes into its cell, none where the cell is none and only the room's size is
// counted, `*at` moved past it to the next word
EMBED_INLINE void *cli_room_part(const HighlightStoreCtx *args)
{
    unsigned long long *at = args->at_pointer;
    void *const part = EMBED_CALL(cli_place, HighlightStoreCtx, .region = args->cell, .at = *at);
    *at += cli_words_get(args);
    return part;
}

// each part of a read's room placed in `cell` in order, at the counts the room contains: the bytes they take
EMBED_INLINE unsigned long long cli_grammar_parts(const HighlightStoreCtx *args)
{
    unsigned long long at = 0ull;
    args->room->node = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                  .bytes = args->room->nodes_most * sizeof(CliRegexpNode));
    args->room->item = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                  .bytes = args->room->items_most * sizeof(CliRegexpItem));
    args->room->part = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                  .bytes = args->room->parts_most * sizeof(unsigned int));
    args->room->part_first = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                        .bytes = args->room->parts_most * sizeof(CliRegexpFirst));
    args->room->rule = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                  .bytes = args->room->rules_most * sizeof(CliHighlightRule));
    args->room->token = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                   .bytes = args->room->tokens_most * sizeof(CliHighlightToken));
    args->room->cases = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                   .bytes = args->room->tokens_most * sizeof(CliHighlightCase));
    args->room->listed = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                    .bytes = args->room->listed_most * sizeof(unsigned int));
    args->room->state = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                   .bytes = args->room->states_most * sizeof(CliHighlightList));
    args->room->word = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                  .bytes = args->room->words_most * sizeof(unsigned int));
    args->room->words = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                   .bytes = args->room->word_lists_most * sizeof(CliHighlightList));
    args->room->name = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                  .bytes = args->room->names_most * sizeof(unsigned int));
    args->room->text = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                  .bytes = args->room->text_most);
    args->room->own = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                 .bytes = args->room->own_most * sizeof(unsigned int));
    args->room->own_state = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                       .bytes = args->room->states_most * sizeof(CliHighlightList));
    args->room->pattern = EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                     .bytes = sizeof(CliRegexpRoom));
    return at;
}

// The parts of a read's room at the sizes the grammar's JSON bounds, its `nodes` and the `used` bytes of its texts: a
// pattern's nodes and parts no more than three for each of its letters and one more, its items no more than its
// letters; rules, own entries, states, words and their lists no more than the JSON's nodes, tokens two for each and
// names one more; the texts no more than four bytes for each byte and a terminator each; and the rules listed, which a
// state's includes repeat, at their limit. The room is one cell, sized on a first pass over its parts and placed in on
// a second: the cell, for the caller to give back, or NULL where the cellblock cannot meet it
EMBED_INLINE unsigned char *cli_grammar_room(const HighlightStoreCtx *args)
{
    const unsigned long long nodes = args->json->nodes;
    const unsigned long long bytes = args->json->used;
    args->room->nodes_most =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = 3ull * (bytes + nodes), .limit = CLI_GRAMMAR_NODES);
    args->room->items_most = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = bytes, .limit = CLI_GRAMMAR_ITEMS);
    args->room->parts_most =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = 3ull * (bytes + nodes), .limit = CLI_GRAMMAR_PARTS);
    args->room->rules_most = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_RULES);
    args->room->tokens_most =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = 2ull * nodes, .limit = CLI_GRAMMAR_TOKENS);
    args->room->listed_most = CLI_GRAMMAR_LISTED;
    args->room->states_most = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_STATES);
    args->room->words_most = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_WORDS);
    args->room->word_lists_most =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_WORD_LISTS);
    args->room->names_most =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes + 1ull, .limit = CLI_GRAMMAR_NAMES);
    args->room->own_most = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_OWN);
    args->room->text_most =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = (4ull * bytes) + nodes + 1ull, .limit = CLI_GRAMMAR_TEXT);
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(
        EMBED_CALL(cli_grammar_parts, HighlightStoreCtx, .room = args->room, .cell = NULL), &g_cli_grammars_cells);
    if (cell != NULL)
    {
        (void)EMBED_CALL(cli_grammar_parts, HighlightStoreCtx, .room = args->room, .cell = cell);
    }
    return cell;
}

// each part of a coloring's room placed in `cell` in order, at the counts the room contains: the bytes they take
EMBED_INLINE unsigned long long cli_coloring_parts(const HighlightStoreCtx *args)
{
    unsigned long long at = 0ull;
    args->room_highlight_room_pointer->letters =
        EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->room_highlight_room_pointer->most * sizeof(unsigned int));
    args->room_highlight_room_pointer->cols =
        EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (args->room_highlight_room_pointer->most + 1ull) * sizeof(unsigned int));
    // a run for each letter, one past the colored part and one for an empty line, and a mark for each state stacked and
    // one more
    args->room_highlight_room_pointer->run_col = EMBED_CALL(
        cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
        .bytes = (args->room_highlight_room_pointer->most + 3ull + CLI_HIGHLIGHT_STACK) * sizeof(unsigned int));
    args->room_highlight_room_pointer->run_name = EMBED_CALL(
        cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
        .bytes = (args->room_highlight_room_pointer->most + 3ull + CLI_HIGHLIGHT_STACK) * sizeof(unsigned int));
    args->room_highlight_room_pointer->stack =
        EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (unsigned long long)CLI_HIGHLIGHT_STACK * CLI_NAME_MAX_LEN);
    args->room_highlight_room_pointer->picked =
        EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (4ull * args->room_highlight_room_pointer->most) + 1ull);
    args->room_highlight_room_pointer->key =
        EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (4ull * args->room_highlight_room_pointer->most) + 1ull);
    args->room_highlight_room_pointer->endings =
        EMBED_CALL(cli_room_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->room_highlight_room_pointer->endings_size);
    return at;
}

// the room of a coloring of `count` lines, taken in one cell as the coloring begins, as wide as the longest line's
// letters, a line's letters being no more than its bytes nor than a line is read to, sized on a first pass over its
// parts and placed in on a second: the room, its cell into `*cell` for the coloring to give back as it ends, or NULL
// where the cellblock cannot meet it
EMBED_INLINE CliHighlightRoom *cli_coloring_room(const HighlightStoreCtx *args)
{
    unsigned char **cell = args->cell_pointer;
    unsigned int count = args->count;
    unsigned long long most = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = args->lines[at], .cap = CLI_CELLS_POOL});
        most = (bytes > most) ? bytes : most;
    }
    args->room_highlight_room_pointer->most = (most < CLI_HIGHLIGHT_LETTERS) ? most : CLI_HIGHLIGHT_LETTERS;
    args->room_highlight_room_pointer->count = count;
    // each line's ending, and the byte the endings reserve back for their terminator
    args->room_highlight_room_pointer->endings_size =
        (args->room_highlight_room_pointer->count * CLI_HIGHLIGHT_ENDING) + 1ull;
    *cell = g_cli_cells->temporary_buf_alloc(
        EMBED_CALL(cli_coloring_parts, HighlightStoreCtx,
                   .room_highlight_room_pointer = args->room_highlight_room_pointer, .cell = NULL),
        &g_cli_grammars_cells);
    if (*cell == NULL)
    {
        return NULL;
    }
    (void)EMBED_CALL(cli_coloring_parts, HighlightStoreCtx,
                     .room_highlight_room_pointer = args->room_highlight_room_pointer, .cell = *cell);
    return args->room_highlight_room_pointer;
}

int cli_highlight_store(const CliHighlightKeyJsonCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    CliGrammarRoom room;
    unsigned char *const cell = EMBED_CALL(cli_grammar_room, HighlightStoreCtx, .room = &room, .json = args->json);
    const int stored =
        (cell != NULL) && EMBED_CALL(cli_grammar_store, HighlightStoreCtx, .key = args->key, .json = args->json,
                                     .def = args->def, .room = &room, .err_msg = args->err_msg);
    g_cli_cells->persistent_buf_release(cell, &g_cli_grammars_cells);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "no room to read a grammar into");
    }
    return stored;
}

// the place of the grammar stored under `key`, s_grammar_room where none is; under the lock
EMBED_INLINE unsigned int cli_grammar_entry_get(const HighlightStoreCtx *args)
{
    unsigned int entry_at = 0u;
    while (
        (entry_at < s_grammar_room) &&
        !(s_grammars[entry_at].used &&
          (cellul.eq(&(CatenaFinitaCfg){.src = s_grammars[entry_at].key, .other = args->key, .cap = CLI_CELLS_POOL}))))
    {
        entry_at += 1u;
    }
    return entry_at;
}

// the lines colored with the grammar stored under `key`, as `colored` says where they go, the lock taken to find the
// grammar and count the coloring among its readers alone, and the lines colored in a room claimed with no lock taken:
// 1, or 0 with why given
static int cli_color(const HighlightStoreCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned int entry_at = cli_grammar_entry_get(args);
    const int has_entry = entry_at != s_grammar_room;
    unsigned char *const region = has_entry ? s_grammars[entry_at].region : NULL;
    CliGrammar grammar;
    if (has_entry)
    {
        grammar = s_grammars[entry_at].grammar;
        (void)atomic_fetch_add_explicit(&EMBED_CALL(cli_grammar_head, HighlightStoreCtx, .region = region)->readers,
                                        1ull, memory_order_relaxed);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (!has_entry)
    {
        mmgr_line_put(args->err_msg, "no grammar is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    CliHighlightRoom taken;
    unsigned char *cell = NULL;
    CliHighlightRoom *const room =
        EMBED_CALL(cli_coloring_room, HighlightStoreCtx, .room_highlight_room_pointer = &taken, .cell_pointer = &cell,
                   .lines = args->colored->lines, .count = args->colored->count);
    if ((room != NULL) && (args->colored->each != NULL))
    {
        EMBED_CALL(cli_highlight_room_state_internal.leaves_get, CliHighlightRoomStateCtx, .grammar = &grammar,
                   .room = room, .state = args->colored->state, .lines = args->colored->lines,
                   .count = args->colored->count, .ends = args->colored->ends, .each = args->colored->each,
                   .context = args->colored->context, .ended = args->colored->ended);
    }
    else if (room != NULL)
    {
        EMBED_CALL(cli_highlight_room_internal.lines, CliHighlightRoomCtx, .grammar = &grammar, .room = room,
                   .state = args->colored->state, .lines = args->colored->lines, .count = args->colored->count,
                   .out = args->colored->out);
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_grammars_cells);
    EMBED_CALL(cli_grammar_let_go, HighlightStoreCtx, .region = region);
    if (room == NULL)
    {
        mmgr_line_put(args->err_msg, "the coloring could not be taken");
        return 0;
    }
    return 1;
}

int cli_highlight_color(const CliHighlightKeyErrMsgCfg *args)
{
    const CliColored colored = {args->state, args->lines, args->count, 0, args->out, NULL, NULL, NULL};
    return EMBED_CALL(cli_color, HighlightStoreCtx, .key = args->key, .colored = &colored, .err_msg = args->err_msg);
}

int cli_highlight_leaves(const CliHighlightKeyCfg *args)
{
    const CliColored colored = {args->state, args->lines, args->count,   args->ends,
                                NULL,        args->each,  args->context, args->ended};
    return EMBED_CALL(cli_color, HighlightStoreCtx, .key = args->key, .colored = &colored, .err_msg = args->err_msg);
}

int cli_highlight_names_write(const CliHighlightKeyErrMsgCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned int entry_at = EMBED_CALL(cli_grammar_entry_get, HighlightStoreCtx, .key = args->key);
    const int has_entry = entry_at != s_grammar_room;
    if (has_entry)
    {
        const CliGrammar *const grammar = &s_grammars[entry_at].grammar;
        mmgr_line_ch(args->out, '[');
        for (unsigned int at = 0u; at < grammar->names; at += 1u)
        {
            const char *const name = grammar->text + grammar->name[at];
            mmgr_line_put(args->out, (at != 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(args->out, ']');
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return has_entry;
}

unsigned int cli_highlight_names_each(const CliHighlightKeyContextCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned int entry_at = EMBED_CALL(cli_grammar_entry_get, HighlightStoreCtx, .key = args->key);
    const CliGrammar *const grammar = (entry_at != s_grammar_room) ? &s_grammars[entry_at].grammar : NULL;
    const unsigned int names = (grammar != NULL) ? grammar->names : 0u;
    for (unsigned int at = 0u; at < names; at += 1u)
    {
        EMBED_CALL(args->each_name_each, CliNameEachCfg, .context = args->context, .place = at,
                   .name = grammar->text + grammar->name[at]);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return names;
}

void cli_highlight_runs_write(const CliHighlightCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int run = 0u; run < args->leaves->runs; run += 1u)
    {
        mmgr_line_put(args->out, (run != 0u) ? "," : "");
        mmgr_line_decimal(args->out, args->leaves->col[run], 1u);
        mmgr_line_ch(args->out, ',');
        mmgr_line_decimal(args->out, args->leaves->name[run], 1u);
    }
    mmgr_line_ch(args->out, ']');
}

unsigned int cli_highlight_state(const CliHighlightNameCtx *args)
{
    for (unsigned int at = 0u; at < args->grammar->states; at += 1u)
    {
        const char *const named = args->grammar->text + args->grammar->state[at].name;
        if ((cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = named, .other = args->name, .bytes = args->length}) == 0))
        {
            return at;
        }
    }
    return CLI_HIGHLIGHT_NONE;
}

int cli_highlight_has_word(const CliHighlightLengthCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->list->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        // the stored word is terminated and `word` contains no terminator within its length: a compare over `length`
        // bytes orders a shorter stored word by its terminator, and a longer one by the byte after `length`
        const char *const word_at = args->grammar->text + args->grammar->word[args->list->first + middle];
        int order = memor.cmp(&(MemoriaCfg){.src = word_at, .other = args->word, .bytes = args->length});
        order = (order != 0) ? order : ((word_at[args->length] != '\0') ? 1 : 0);
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
