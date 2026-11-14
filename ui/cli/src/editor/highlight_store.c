// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_store.c: the grammars stored, each a movable cell of its own under its key, given back when a grammar is
// put in its place; a grammar read, and lines colored by the grammar stored under a key. The lock is taken to find the
// grammar and to store one, never as lines are colored: a coloring copies the grammar it reads into a temporary cell
// of its own and works in a work state it claims by a mark, so as many colorings run at once as threads ask for them.
// The cells are the one cellblock's, counted to the grammars' own mark, which their report gives
#include "../command_line/callback_cfg.h"
#include "highlight_internal.h"

// The lines passed over: their state, the lines and their count, and whether the last ends the text; and where they
// go, the coloring written as JSON into `out`, or each line's leaves passed to `each`, the state the last ends in into
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
// the grammars stored at max, the list's one cell taken at that
#define CLI_GRAMMARS_MAX 1024u

// A grammar stored: its key, its movable cell and the cell's bytes, and its parts' counts. A coloring copies the cell
// into a temporary cell of its own and positions the parts out over the copy, so the cell is read through no pointer
typedef struct
{
    char key[CLI_NAME_MAX_LEN];
    int used;
    const CarcerMovableEntry *region;
    unsigned long long size;
    CliGrammar grammar;
} CliGrammarEntry;

// the grammars stored, in a movable cell of the entries it fits, and how many it fits; the grammar read, the lock the
// grammars are stored and read under, and what the grammars and the work states occupy in the cellblock
static const CarcerMovableEntry *s_grammars;
static unsigned int s_grammar_capacity;
static CliGrammar s_read;

void cli_highlight_cells_usage_write(const CliHighlightCfg *args)
{
    cli_pool_owner_usage_write(g_cli_grammars_owner, args->out);
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
    CliGrammarWorkState *work_state;
    VerbaLinea *err_msg;
    unsigned long long bound;
    unsigned int limit;
    unsigned char *cell;
    unsigned long long *at_pointer;
    CliHighlightWorkState *highlight_work_state_pointer;
    unsigned char **cell_pointer;
    const char *const *lines;
    unsigned int count;
    const CliColored *colored;
} HighlightStoreCtx;
// a stored grammar's cell copied into a temporary cell and its parts arranged over the copy into `grammar`: the copy,
// for the caller to give back, or NULL where it could not be taken or the cell could not be read
static unsigned char *cli_grammar_copied(const CliGrammarEntry *listed, CliGrammar *grammar);

// a part's bytes rounded up to a whole word
EMBED_INLINE unsigned long long cli_words_get(const HighlightStoreCtx *args)
{
    return (args->bytes + 7ULL) & ~7ULL;
}

// the place `at` bytes into a region, none where the region is none and only its size is counted
EMBED_INLINE void *cli_position(const HighlightStoreCtx *args)
{
    return (args->region != NULL) ? (void *)(args->region + args->at) : NULL;
}

// each part of a grammar arranged at `region` by its counts, in order, each on a word: the bytes they take
EMBED_INLINE unsigned long long cli_layout(const HighlightStoreCtx *args)
{
    unsigned long long at = 0ULL;
    args->grammar->node =
        (CliRegexpNode *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->nodes * sizeof(CliRegexpNode));
    args->grammar->item =
        (CliRegexpItem *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->items * sizeof(CliRegexpItem));
    args->grammar->part = (unsigned int *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->parts * sizeof(unsigned int));
    args->grammar->part_first =
        (CliRegexpFirst *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->parts * sizeof(CliRegexpFirst));
    args->grammar->rule =
        (CliHighlightRule *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->rules * sizeof(CliHighlightRule));
    args->grammar->token =
        (CliHighlightToken *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->tokens * sizeof(CliHighlightToken));
    args->grammar->cases =
        (CliHighlightCase *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->case_count * sizeof(CliHighlightCase));
    args->grammar->list_entries =
        (unsigned int *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->listing_count * sizeof(unsigned int));
    args->grammar->state =
        (CliHighlightList *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->states * sizeof(CliHighlightList));
    args->grammar->word_text =
        (unsigned int *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->word_count * sizeof(unsigned int));
    args->grammar->words =
        (CliHighlightList *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->word_lists * sizeof(CliHighlightList));
    args->grammar->name = (unsigned int *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->names * sizeof(unsigned int));
    args->grammar->text = (char *)EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->region, .at = at);
    at += EMBED_CALL(cli_words_get, HighlightStoreCtx, .bytes = args->grammar->text_size);
    return at;
}

// the grammar read copied into a region arranged at `region`, stored as `copy`
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
    memor.cpy(&(MemoriaCfg){.dst = copy->list_entries,
                            .src = args->read->list_entries,
                            .bytes = args->read->listing_count * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->state, .src = args->read->state, .bytes = args->read->states * sizeof(CliHighlightList)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->word_text, .src = args->read->word_text, .bytes = args->read->word_count * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){
        .dst = copy->words, .src = args->read->words, .bytes = args->read->word_lists * sizeof(CliHighlightList)});
    memor.cpy(
        &(MemoriaCfg){.dst = copy->name, .src = args->read->name, .bytes = args->read->names * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){.dst = copy->text, .src = args->read->text, .bytes = args->read->text_size});
}

// the entry `at` of the list read off its cell, and written into it
static CliGrammarEntry cli_grammar_listed(unsigned int at)
{
    CliGrammarEntry entry;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_grammars, .offset = (embed_word)at * sizeof(entry), .dst = &entry, .bytes = sizeof(entry)});
    return entry;
}
// the list of the grammars stored taken once at CLI_GRAMMARS_MAX entries, every place empty, where none is: 1, or
// 0 where the cellblock cannot meet it; under the lock
static int cli_grammars_taken(void)
{
    if (s_grammars != NULL)
    {
        return 1;
    }
    const unsigned long long bytes = (unsigned long long)CLI_GRAMMARS_MAX * sizeof(CliGrammarEntry);
    unsigned char *const zeroes = g_cli_guard->temporary_buf_alloc(bytes, g_cli_grammars_owner);
    const CarcerMovableEntry *const cell =
        (zeroes != NULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_grammars_owner) : NULL;
    if (cell == NULL)
    {
        g_cli_guard->persistent_buf_release(zeroes, g_cli_grammars_owner);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = zeroes, .val = 0, .bytes = (embed_word)bytes});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = zeroes, .bytes = bytes});
    g_cli_guard->persistent_buf_release(zeroes, g_cli_grammars_owner);
    s_grammars = cell;
    s_grammar_capacity = CLI_GRAMMARS_MAX;
    return 1;
}

// the grammar at `def` read into `capacity` and stored under `key` in a cell of its own, one stored there before put
// out of its place first: 1, or 0 with why given
EMBED_INLINE int cli_grammar_store(const HighlightStoreCtx *args)
{
    const CliJson *json = args->json;
    unsigned int def = args->def;
    CliGrammarWorkState *work_state = args->work_state;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_highlight_json_internal.read, CliHighlightJsonCtx, .json = json, .def = def,
                    .grammar_work_state_pointer = work_state, .grammar_out = &s_read, .err_msg = err_msg))
    {
        return 0;
    }
    CliGrammar positioned = s_read;
    const unsigned long long size = EMBED_CALL(cli_layout, HighlightStoreCtx, .grammar = &positioned, .region = NULL);
    if (!cli_grammars_taken())
    {
        mmgr_line_put(err_msg, "the grammars stored take more than impero supports");
        return 0;
    }
    unsigned int entry_at = s_grammar_capacity;
    unsigned int free_at = s_grammar_capacity;
    for (unsigned int at = 0u; at < s_grammar_capacity; at += 1u)
    {
        const CliGrammarEntry listed = cli_grammar_listed(at);
        entry_at = (listed.used &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = listed.key, .other = args->key, .cap = CLI_POOL_BYTES})))
                       ? at
                       : entry_at;
        free_at = (!listed.used && (free_at == s_grammar_capacity)) ? at : free_at;
    }
    if (entry_at != s_grammar_capacity)
    {
        CliGrammarEntry listed = cli_grammar_listed(entry_at);
        (void)g_cli_guard->movable_buf_release(listed.region, g_cli_grammars_owner);
        listed.used = 0;
        listed.region = NULL;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = s_grammars,
                                .offset = (embed_word)entry_at * sizeof(CliGrammarEntry),
                                .src = &listed,
                                .bytes = sizeof(CliGrammarEntry)});
        free_at = entry_at;
    }
    if (cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES}) >= CLI_NAME_MAX_LEN)
    {
        mmgr_line_put(err_msg, "a grammar's key is longer than impero supports");
        return 0;
    }
    if (free_at == s_grammar_capacity)
    {
        mmgr_line_put(err_msg, "the grammars stored take more than impero supports");
        return 0;
    }
    // the parts arranged in a temporary cell, then written whole into a movable cell sized to them
    unsigned char *const arranged = g_cli_guard->temporary_buf_alloc(size, g_cli_grammars_owner);
    const CarcerMovableEntry *const region =
        (arranged != NULL) ? g_cli_guard->movable_buf_alloc(size, g_cli_grammars_owner) : NULL;
    if (region == NULL)
    {
        g_cli_guard->persistent_buf_release(arranged, g_cli_grammars_owner);
        mmgr_line_put(err_msg, "the grammars stored take more than impero supports");
        return 0;
    }
    CliGrammarEntry grammar;
    memor.set(&(MemoriaCfg){.dst = &grammar, .val = 0, .bytes = sizeof(grammar)});
    memor.cpy(&(MemoriaCfg){.dst = grammar.key,
                            .src = args->key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES}) + 1u});
    grammar.region = region;
    grammar.size = size;
    EMBED_CALL(cli_copy, HighlightStoreCtx, .read = &s_read, .region = arranged, .copy = &grammar.grammar);
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = region, .src = arranged, .bytes = size});
    g_cli_guard->persistent_buf_release(arranged, g_cli_grammars_owner);
    // the entry retains the parts' counts; a reader positions the pointers out over its own copy
    grammar.grammar = positioned;
    grammar.used = 1;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_grammars,
                                                             .offset = (embed_word)free_at * sizeof(CliGrammarEntry),
                                                             .src = &grammar,
                                                             .bytes = sizeof(CliGrammarEntry)});
    return 1;
}

static unsigned char *cli_grammar_copied(const CliGrammarEntry *listed, CliGrammar *grammar)
{
    unsigned char *const copy = g_cli_guard->temporary_buf_alloc(listed->size, g_cli_grammars_owner);
    if (copy == NULL)
    {
        return NULL;
    }
    if (!g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = listed->region, .dst = copy, .bytes = listed->size}))
    {
        g_cli_guard->persistent_buf_release(copy, g_cli_grammars_owner);
        return NULL;
    }
    *grammar = listed->grammar;
    (void)EMBED_CALL(cli_layout, HighlightStoreCtx, .grammar = grammar, .region = copy);
    return copy;
}

// a part's count as the grammar's JSON bounds it, no more than the part's limit
EMBED_INLINE unsigned int cli_bound(const HighlightStoreCtx *args)
{
    // a count no more than the limit, which an unsigned int stores
    return (args->bound < args->limit) ? (unsigned int)args->bound : args->limit;
}

// a work state's part of `bytes` placed `*at` bytes into its cell, none where the cell is none and only the work
// state's size is counted, `*at` moved past it to the next word
EMBED_INLINE void *cli_work_state_part(const HighlightStoreCtx *args)
{
    unsigned long long *at = args->at_pointer;
    void *const part = EMBED_CALL(cli_position, HighlightStoreCtx, .region = args->cell, .at = *at);
    *at += cli_words_get(args);
    return part;
}

// each part of a read's work state placed in `cell` in order, at the counts the work state contains: the bytes they
// take
EMBED_INLINE unsigned long long cli_grammar_parts(const HighlightStoreCtx *args)
{
    unsigned long long at = 0ULL;
    args->work_state->node = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                        .bytes = args->work_state->nodes_max * sizeof(CliRegexpNode));
    args->work_state->item = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                        .bytes = args->work_state->items_max * sizeof(CliRegexpItem));
    args->work_state->part = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                        .bytes = args->work_state->parts_max * sizeof(unsigned int));
    args->work_state->part_first =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->work_state->parts_max * sizeof(CliRegexpFirst));
    args->work_state->rule = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                        .bytes = args->work_state->rules_max * sizeof(CliHighlightRule));
    args->work_state->token = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                         .bytes = args->work_state->tokens_max * sizeof(CliHighlightToken));
    args->work_state->cases = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                         .bytes = args->work_state->tokens_max * sizeof(CliHighlightCase));
    args->work_state->list_entries =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->work_state->listing_max * sizeof(unsigned int));
    args->work_state->state = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                         .bytes = args->work_state->states_max * sizeof(CliHighlightList));
    args->work_state->word_text =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->work_state->words_max * sizeof(unsigned int));
    args->work_state->words = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                         .bytes = args->work_state->word_lists_max * sizeof(CliHighlightList));
    args->work_state->name = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                        .bytes = args->work_state->names_max * sizeof(unsigned int));
    args->work_state->text = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                        .bytes = args->work_state->text_max);
    args->work_state->own = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                                       .bytes = args->work_state->own_max * sizeof(unsigned int));
    args->work_state->own_state =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->work_state->states_max * sizeof(CliHighlightList));
    args->work_state->pattern = EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell,
                                           .at_pointer = &at, .bytes = sizeof(CliRegexpWorkState));
    return at;
}

// The parts of a read's work state at the sizes the grammar's JSON bounds, its `nodes` and the `used` bytes of its
// texts: a pattern's nodes and parts no more than three for each of its characters and one more, its items no more than
// its characters; rules, own entries, states, words and their lists no more than the JSON's nodes, tokens two for each and
// names one more; the texts no more than four bytes for each byte and a terminator each; and the rules listed, which a
// state's includes repeat, at their limit. The work state is one cell, sized on a first pass over its parts and placed
// in on a second: the cell, for the caller to give back, or NULL where the cellblock cannot meet it
EMBED_INLINE unsigned char *cli_grammar_capacity(const HighlightStoreCtx *args)
{
    const unsigned long long nodes = args->json->nodes;
    const unsigned long long bytes = args->json->used;
    args->work_state->nodes_max =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = 3ULL * (bytes + nodes), .limit = CLI_GRAMMAR_NODES);
    args->work_state->items_max = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = bytes, .limit = CLI_GRAMMAR_ITEMS);
    args->work_state->parts_max =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = 3ULL * (bytes + nodes), .limit = CLI_GRAMMAR_PARTS);
    args->work_state->rules_max = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_RULES);
    args->work_state->tokens_max =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = 2ULL * nodes, .limit = CLI_GRAMMAR_TOKENS);
    args->work_state->listing_max = CLI_GRAMMAR_LISTING;
    args->work_state->states_max =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_STATES);
    args->work_state->words_max = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_WORDS);
    args->work_state->word_lists_max =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_WORD_LISTS);
    args->work_state->names_max =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes + 1ULL, .limit = CLI_GRAMMAR_NAMES);
    args->work_state->own_max = EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = nodes, .limit = CLI_GRAMMAR_OWN);
    args->work_state->text_max =
        EMBED_CALL(cli_bound, HighlightStoreCtx, .bound = (4ULL * bytes) + nodes + 1ULL, .limit = CLI_GRAMMAR_TEXT);
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(
        EMBED_CALL(cli_grammar_parts, HighlightStoreCtx, .work_state = args->work_state, .cell = NULL),
        g_cli_grammars_owner);
    if (cell != NULL)
    {
        (void)EMBED_CALL(cli_grammar_parts, HighlightStoreCtx, .work_state = args->work_state, .cell = cell);
    }
    return cell;
}

// each part of a coloring's work state placed in `cell` in order, at the counts the work state contains: the bytes they
// take
EMBED_INLINE unsigned long long cli_coloring_parts(const HighlightStoreCtx *args)
{
    unsigned long long at = 0ULL;
    args->highlight_work_state_pointer->characters =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->highlight_work_state_pointer->max * sizeof(unsigned int));
    args->highlight_work_state_pointer->cols =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (args->highlight_work_state_pointer->max + 1ULL) * sizeof(unsigned int));
    // a run for each character, one past the colored part and one for an empty line, and a mark for each state stacked and
    // one more
    args->highlight_work_state_pointer->run_col = EMBED_CALL(
        cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
        .bytes = (args->highlight_work_state_pointer->max + 3ULL + CLI_HIGHLIGHT_STACK) * sizeof(unsigned int));
    args->highlight_work_state_pointer->run_name = EMBED_CALL(
        cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
        .bytes = (args->highlight_work_state_pointer->max + 3ULL + CLI_HIGHLIGHT_STACK) * sizeof(unsigned int));
    args->highlight_work_state_pointer->stack =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (unsigned long long)CLI_HIGHLIGHT_STACK * CLI_NAME_MAX_LEN);
    args->highlight_work_state_pointer->picked =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (4ULL * args->highlight_work_state_pointer->max) + 1ULL);
    args->highlight_work_state_pointer->key =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = (4ULL * args->highlight_work_state_pointer->max) + 1ULL);
    args->highlight_work_state_pointer->endings =
        EMBED_CALL(cli_work_state_part, HighlightStoreCtx, .cell = args->cell, .at_pointer = &at,
                   .bytes = args->highlight_work_state_pointer->endings_size);
    return at;
}

// the work state of a coloring of `count` lines, taken in one cell as the coloring begins, as wide as the longest
// line's characters, a line's characters being no more than its bytes nor than a line is read to, sized on a first pass over
// its parts and placed in on a second: the work state, its cell into `*cell` for the coloring to give back as it ends,
// or NULL where the cellblock cannot meet it
EMBED_INLINE CliHighlightWorkState *cli_coloring_capacity(const HighlightStoreCtx *args)
{
    unsigned char **cell = args->cell_pointer;
    unsigned int count = args->count;
    unsigned long long max = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = args->lines[at], .cap = CLI_POOL_BYTES});
        max = (bytes > max) ? bytes : max;
    }
    args->highlight_work_state_pointer->max = (max < CLI_HIGHLIGHT_CHARACTERS) ? max : CLI_HIGHLIGHT_CHARACTERS;
    args->highlight_work_state_pointer->count = count;
    // each line's ending, and the byte the endings reserve back for their terminator
    args->highlight_work_state_pointer->endings_size =
        (args->highlight_work_state_pointer->count * CLI_HIGHLIGHT_ENDING) + 1ULL;
    *cell = g_cli_guard->temporary_buf_alloc(
        EMBED_CALL(cli_coloring_parts, HighlightStoreCtx,
                   .highlight_work_state_pointer = args->highlight_work_state_pointer, .cell = NULL),
        g_cli_grammars_owner);
    if (*cell == NULL)
    {
        return NULL;
    }
    (void)EMBED_CALL(cli_coloring_parts, HighlightStoreCtx,
                     .highlight_work_state_pointer = args->highlight_work_state_pointer, .cell = *cell);
    return args->highlight_work_state_pointer;
}

int cli_highlight_store(const CliHighlightKeyJsonCfg *args)
{
    CliGrammarWorkState work_state;
    unsigned char *const cell =
        EMBED_CALL(cli_grammar_capacity, HighlightStoreCtx, .work_state = &work_state, .json = args->json);
    const int stored =
        (cell != NULL) && EMBED_CALL(cli_grammar_store, HighlightStoreCtx, .key = args->key, .json = args->json,
                                     .def = args->def, .work_state = &work_state, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(cell, g_cli_grammars_owner);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "no capacity to read a grammar into");
    }
    return stored;
}

// the place of the grammar stored under `key`, s_grammar_capacity where none is; under the lock
EMBED_INLINE unsigned int cli_grammar_entry_get(const HighlightStoreCtx *args)
{
    unsigned int entry_at = 0u;
    while (entry_at < s_grammar_capacity)
    {
        const CliGrammarEntry listed = cli_grammar_listed(entry_at);
        if (listed.used &&
            (cellul.eq(&(CatenaFinitaCfg){.src = listed.key, .other = args->key, .cap = CLI_POOL_BYTES})))
        {
            break;
        }
        entry_at += 1u;
    }
    return entry_at;
}

// the lines colored with the grammar stored under `key`, as `colored` says where they go, the lock taken to find the
// grammar and copy it into a temporary cell, and the lines colored in a work state claimed with no lock
// taken: 1, or 0 with why given
static int cli_color(const HighlightStoreCtx *args)
{
    const unsigned int entry_at = cli_grammar_entry_get(args);
    const int has_entry = entry_at != s_grammar_capacity;
    unsigned char *region = NULL;
    CliGrammar grammar;
    if (has_entry)
    {
        const CliGrammarEntry listed = cli_grammar_listed(entry_at);
        region = cli_grammar_copied(&listed, &grammar);
    }
    if (!has_entry)
    {
        mmgr_line_put(args->err_msg, "no grammar is stored under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    if (region == NULL)
    {
        mmgr_line_put(args->err_msg, "the grammar's copy could not be taken");
        return 0;
    }
    CliHighlightWorkState reserved;
    unsigned char *cell = NULL;
    CliHighlightWorkState *const work_state =
        EMBED_CALL(cli_coloring_capacity, HighlightStoreCtx, .highlight_work_state_pointer = &reserved,
                   .cell_pointer = &cell, .lines = args->colored->lines, .count = args->colored->count);
    if ((work_state != NULL) && (args->colored->each != NULL))
    {
        EMBED_CALL(cli_highlight_capacity_state_internal.leaves_get, CliHighlightWorkStateStateCtx, .grammar = &grammar,
                   .work_state = work_state, .state = args->colored->state, .lines = args->colored->lines,
                   .count = args->colored->count, .ends = args->colored->ends, .each = args->colored->each,
                   .context = args->colored->context, .ended = args->colored->ended);
    }
    else if (work_state != NULL)
    {
        EMBED_CALL(cli_highlight_capacity_internal.lines, CliHighlightWorkStateCtx, .grammar = &grammar,
                   .work_state = work_state, .state = args->colored->state, .lines = args->colored->lines,
                   .count = args->colored->count, .out = args->colored->out);
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_grammars_owner);
    g_cli_guard->persistent_buf_release(region, g_cli_grammars_owner);
    if (work_state == NULL)
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

unsigned int cli_highlight_names_each(const CliHighlightKeyContextCfg *args)
{
    const unsigned int entry_at = EMBED_CALL(cli_grammar_entry_get, HighlightStoreCtx, .key = args->key);
    if (entry_at == s_grammar_capacity)
    {
        return 0u;
    }
    const CliGrammarEntry listed = cli_grammar_listed(entry_at);
    CliGrammar grammar;
    unsigned char *const region = cli_grammar_copied(&listed, &grammar);
    if (region == NULL)
    {
        return 0u;
    }
    for (unsigned int at = 0u; at < grammar.names; at += 1u)
    {
        EMBED_CALL(args->each_name_each, CliNameEachCfg, .context = args->context, .place = at,
                   .name = grammar.text + grammar.name[at]);
    }
    g_cli_guard->persistent_buf_release(region, g_cli_grammars_owner);
    return grammar.names;
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
        if ((cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_POOL_BYTES}) == args->length) &&
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
        const char *const word_at = args->grammar->text + args->grammar->word_text[args->list->first + middle];
        embed_iword order = memor.cmp(&(MemoriaCfg){.src = word_at, .other = args->word_text, .bytes = args->length});
        if (order == 0)
        {
            if (word_at[args->length] != '\0')
            {
                order = 1;
            }
            else
            {
                order = 0;
            }
        }
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
