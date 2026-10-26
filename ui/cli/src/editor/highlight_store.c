// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_store.c: the grammars stored, each a cell of its own under its key, given back when a grammar is put in its
// place; a grammar read, and lines colored by the grammar stored under a key, each in a room of one cell given back
// before the call returns. The cells are the one cellblock's, counted to the grammars' own mark, which their report
// gives
#include "highlight_internal.h"

// the grammars the list of those stored is begun with, doubling as every place is taken
#define CLI_GRAMMARS_FIRST 16u

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
static CellOwner s_owner = MMGR_CELL_OWNER("cli_grammars");

void cli_highlight_cells_usage_written(VerbaLinea *out)
{
    cli_cells_owner_usage_written(&s_owner, out);
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

// the list of the grammars stored twice its room, or CLI_GRAMMARS_FIRST where it has none, the places after those it
// left empty: 1, or 0 where the cellblock cannot meet it; under the lock
static int cli_grammars_grown(void)
{
    const unsigned int room = (s_grammar_room == 0u) ? CLI_GRAMMARS_FIRST : (2u * s_grammar_room);
    CliGrammarEntry *const grown =
        g_cli_cells->persistent_buf_alloc((unsigned long long)room * sizeof(*grown), &s_owner);
    if (grown == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = grown, .val = 0, .bytes = (size_t)room * sizeof(*grown)});
    if (s_grammars != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_grammars, .bytes = (size_t)s_grammar_room * sizeof(*grown)});
    }
    g_cli_cells->persistent_buf_release(s_grammars, &s_owner);
    s_grammars = grown;
    s_grammar_room = room;
    return 1;
}

// the grammar at `def` read into `room` and stored under `key` in a cell of its own, the cell of one stored
// there before given back first: 1, or 0 with why given
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
        g_cli_cells->persistent_buf_release(s_grammars[entry_at].region, &s_owner);
        s_grammars[entry_at].used = 0;
        free_at = entry_at;
    }
    if (cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN)
    {
        mmgr_line_put(err_msg, "a grammar's key is longer than orior_cli supports");
        return 0;
    }
    // every place taken, the list doubled and the first place it adds taken
    if ((free_at == s_grammar_room) && !cli_grammars_grown())
    {
        mmgr_line_put(err_msg, "the grammars stored take more than orior_cli supports");
        return 0;
    }
    unsigned char *const region = g_cli_cells->persistent_buf_alloc(size, &s_owner);
    if (region == NULL)
    {
        mmgr_line_put(err_msg, "the grammars stored take more than orior_cli supports");
        return 0;
    }
    CliGrammarEntry *const grammar = &s_grammars[free_at];
    memor.cpy(&(MemoriaCfg){.dst = grammar->key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    grammar->region = region;
    grammar->size = size;
    cli_copied(&s_read, region, &grammar->grammar);
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
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(cli_grammar_parts(room, NULL), &s_owner);
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

// the coloring room retained, and its cell
static CliHighlightRoom s_coloring;
static unsigned char *s_coloring_cell;

// the coloring room for `count` lines: the retained one where it fits the longest line's letters, a line's letters
// being no more than its bytes nor than a line is read to, and the count; else one taken at twice both, the one
// retained before given back. The room is one cell, sized on a first pass over its parts and placed in on a second: the
// room, or NULL where the cellblock cannot meet it; under the lock
static CliHighlightRoom *cli_coloring_roomed(const char *const *lines, unsigned int count)
{
    unsigned long long most = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = lines[at], .cap = CLI_CELLS_POOL});
        most = (bytes > most) ? bytes : most;
    }
    most = (most < CLI_HIGHLIGHT_LETTERS) ? most : CLI_HIGHLIGHT_LETTERS;
    if ((s_coloring_cell != NULL) && (most <= s_coloring.most) && (count <= s_coloring.count))
    {
        return &s_coloring;
    }
    CliHighlightRoom room;
    room.most = (most * 2ull < CLI_HIGHLIGHT_LETTERS) ? most * 2ull : CLI_HIGHLIGHT_LETTERS;
    room.count = (unsigned long long)count * 2ull;
    // each line's ending, and the byte the endings reserve back for their terminator
    room.endings_size = (room.count * CLI_HIGHLIGHT_ENDING) + 1ull;
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(cli_coloring_parts(&room, NULL), &s_owner);
    if (cell == NULL)
    {
        return NULL;
    }
    (void)cli_coloring_parts(&room, cell);
    g_cli_cells->persistent_buf_release(s_coloring_cell, &s_owner);
    s_coloring_cell = cell;
    s_coloring = room;
    return &s_coloring;
}

int cli_highlight_store(const char *key, const CliJson *json, unsigned int def, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    CliGrammarRoom room;
    unsigned char *const cell = cli_grammar_roomed(&room, json);
    const int stored = (cell != NULL) && cli_grammar_stored(key, json, def, &room, err_msg);
    g_cli_cells->persistent_buf_release(cell, &s_owner);
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

// the lines colored with the grammar stored under `key`, as `colored` says where they go: 1, or 0 with why given
static int cli_colored(const char *key, const CliColored *colored, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    const unsigned int entry_at = cli_grammar_entry_of(key);
    int roomed = 0;
    const int has_entry = entry_at != s_grammar_room;
    if (has_entry)
    {
        CliHighlightRoom *const room = cli_coloring_roomed(colored->lines, colored->count);
        roomed = (room != NULL);
        if (roomed && (colored->each != NULL))
        {
            cli_highlight_leaves_of(&s_grammars[entry_at].grammar, room, colored->state, colored->lines,
                                    colored->count, colored->ends, colored->each, colored->context, colored->ended);
        }
        else if (roomed)
        {
            cli_highlight_lines(&s_grammars[entry_at].grammar, room, colored->state, colored->lines, colored->count,
                                colored->out);
        }
    }
    cli_lock_give(&s_lock);
    if (!has_entry)
    {
        mmgr_line_put(err_msg, "no grammar is stored under ");
        mmgr_line_put(err_msg, key);
        return 0;
    }
    if (!roomed)
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
        // the stored word is terminated and `word` contains no terminator within its length, so a compare over `length`
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
