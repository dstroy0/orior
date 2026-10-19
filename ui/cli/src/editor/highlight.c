// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight.c: a grammar read from its JSON into a room, its patterns, tokens, states and words
#include "../text/text.h"
#include "highlight_internal.h"

// the most states an include reads through
#define CLI_GRAMMAR_DEPTH 64u

// an own entry that is an include and not a rule: the top bit set on its name's text
#define CLI_INCLUDE_MARK 0x80000000u

// the room the grammar under way is read into, the one the keeper handed the read
static CliGrammarRoom *s_read;

// 1 where what is read does not fit the room
static int s_full;

// `length` bytes of `text` put among the grammar's texts, lower case where `lower` is 1: where they start
static unsigned int cli_text(CliGrammar *grammar, const char *text, unsigned long long length, int lower)
{
    unsigned int letters[CLI_NAME_LONGEST];
    const unsigned long long count = cli_regexp_letters(text, length, letters, CLI_NAME_LONGEST);
    CliLine line = {grammar->text + grammar->text_size, s_read->text_most - grammar->text_size, 0ull};
    if (lower && (count < CLI_NAME_LONGEST))
    {
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            cli_utf8_put(&line, cli_regexp_lower(letters[at]));
        }
    }
    else
    {
        cli_line_counted(&line, text, length);
    }
    if (!cli_line_fits(&line))
    {
        s_full = 1;
        return 0u;
    }
    const unsigned int at = grammar->text_size;
    // the texts are below the room's text_most, which an unsigned int counts
    grammar->text_size += (unsigned int)cli_line_closed(&line) + 1u;
    return at;
}

// The place of a token's name among the names, added where it is new; a bracket's name is `delimiter.bracket`
static unsigned int cli_name_of(CliGrammar *grammar, const char *name)
{
    const char *const named = (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "@brackets", .cap = CLI_CELLS_POOL}))
                                  ? "delimiter.bracket"
                                  : name;
    for (unsigned int at = 0u; at < grammar->names; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = grammar->text + grammar->name[at], .other = named, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    if (grammar->names >= s_read->names_most)
    {
        s_full = 1;
        return 0u;
    }
    grammar->name[grammar->names] =
        cli_text(grammar, named, cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), 0);
    grammar->names += 1u;
    return grammar->names - 1u;
}

static unsigned int cli_token_made(CliGrammar *grammar, unsigned int kind, unsigned int name)
{
    if (grammar->tokens >= s_read->tokens_most)
    {
        s_full = 1;
        return CLI_HIGHLIGHT_NONE;
    }
    const CliHighlightToken token = {kind, name, 0u, 0u};
    grammar->token[grammar->tokens] = token;
    grammar->tokens += 1u;
    return grammar->tokens - 1u;
}

// the token a value writes into `token`, none for null: 1, or 0 with why said
static int cli_token_of(CliGrammar *grammar, const CliJson *json, unsigned int value, unsigned int *token,
                        CliLine *said)
{
    *token = CLI_HIGHLIGHT_NONE;
    if ((value == CLI_JSON_NONE) || cli_json_is(json, value, CFG_JSON_NULL))
    {
        return 1;
    }
    const char *const name = cli_json_string(json, value, NULL);
    if (name != NULL)
    {
        *token = cli_token_made(grammar, CLI_TOKEN_NAME, cli_name_of(grammar, name));
        return 1;
    }
    const unsigned int cases = cli_json_member(json, value, "cases");
    const int grouped = cli_json_is(json, value, CFG_JSON_ARRAY);
    if (!grouped && !cli_json_is(json, cases, CFG_JSON_OBJECT))
    {
        cli_line_text(said, "a token that is neither a name, a list nor cases");
        return 0;
    }
    const unsigned int list = grouped ? value : cases;
    const unsigned int count = json->node[list].count;
    const unsigned int made = cli_token_made(grammar, grouped ? CLI_TOKEN_GROUPS : CLI_TOKEN_CASES, 0u);
    if ((made == CLI_HIGHLIGHT_NONE) || ((grammar->case_count + count) > s_read->tokens_most))
    {
        s_full = 1;
        return 1;
    }
    // the range of the cases held first, so the tokens under them go after it
    const unsigned int first = grammar->case_count;
    grammar->case_count += count;
    grammar->token[made].first = first;
    grammar->token[made].count = count;
    unsigned int place = first;
    for (unsigned int child = json->node[list].first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        unsigned int inner = CLI_HIGHLIGHT_NONE;
        if (!cli_token_of(grammar, json, child, &inner, said))
        {
            return 0;
        }
        const char *const key = grouped ? NULL : (json->text + json->node[child].key);
        const unsigned int key_text =
            (key != NULL) ? cli_text(grammar, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}), 0)
                          : CLI_HIGHLIGHT_NONE;
        const CliHighlightCase entry = {
            key_text, (inner != CLI_HIGHLIGHT_NONE) ? inner : cli_token_made(grammar, CLI_TOKEN_NAME, 0u)};
        grammar->cases[place] = entry;
        place += 1u;
    }
    *token = made;
    return 1;
}

// a rule's pattern read, its arrays put after the grammar's: 1, or 0 with why said
static int cli_pattern_of(CliGrammar *grammar, const CliJson *json, unsigned int value, CliHighlightRule *rule,
                          CliLine *said)
{
    const char *source = cli_json_string(json, value, NULL);
    const char *flags = "";
    if ((source == NULL) && cli_json_is(json, value, CFG_JSON_OBJECT))
    {
        source = cli_json_string(json, cli_json_member(json, value, "pattern"), NULL);
        if (source == NULL)
        {
            cli_line_text(said, "a pattern with no source");
            return 0;
        }
        const char *const given = cli_json_string(json, cli_json_member(json, value, "flags"), NULL);
        flags = (given != NULL) ? given : "";
    }
    else if (source == NULL)
    {
        cli_line_text(said, "a pattern that is neither text nor a source and flags");
        return 0;
    }
    CliRegexp read;
    cli_regexp_roomed(&read, s_read->pattern);
    if (!cli_regexp_read(&read, source, flags, said))
    {
        return 0;
    }
    if (((grammar->nodes + read.nodes) > s_read->nodes_most) || ((grammar->items + read.items) > s_read->items_most) ||
        ((grammar->parts + read.parts) > s_read->parts_most))
    {
        s_full = 1;
        return 1;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = grammar->node + grammar->nodes, .src = read.node, .bytes = read.nodes * sizeof(read.node[0])});
    memor.cpy(&(MemoriaCfg){
        .dst = grammar->item + grammar->items, .src = read.item, .bytes = read.items * sizeof(read.item[0])});
    memor.cpy(&(MemoriaCfg){
        .dst = grammar->part + grammar->parts, .src = read.part, .bytes = read.parts * sizeof(read.part[0])});
    memor.cpy(&(MemoriaCfg){.dst = grammar->part_first + grammar->parts,
                            .src = read.part_first,
                            .bytes = read.parts * sizeof(read.part_first[0])});
    rule->node_first = grammar->nodes;
    rule->nodes = read.nodes;
    rule->item_first = grammar->items;
    rule->items = read.items;
    rule->part_first = grammar->parts;
    rule->parts = read.parts;
    rule->root = read.root;
    rule->groups = read.groups;
    rule->fold = read.fold;
    rule->first = read.first;
    grammar->nodes += read.nodes;
    grammar->items += read.items;
    grammar->parts += read.parts;
    return 1;
}

void cli_highlight_pattern(const CliGrammar *grammar, const CliHighlightRule *rule, CliRegexp *pattern)
{
    pattern->node = grammar->node + rule->node_first;
    pattern->nodes = rule->nodes;
    pattern->item = grammar->item + rule->item_first;
    pattern->items = rule->items;
    pattern->part = grammar->part + rule->part_first;
    pattern->part_first = grammar->part_first + rule->part_first;
    pattern->parts = rule->parts;
    pattern->root = rule->root;
    pattern->groups = rule->groups;
    pattern->fold = rule->fold;
    pattern->first = rule->first;
}

// one rule of a state read: an include, or a pattern with its token and the state it goes to. 1, or 0 with why said
static int cli_rule_read(CliGrammar *grammar, const CliJson *json, unsigned int value, unsigned int *own, CliLine *said)
{
    const char *const included = cli_json_string(json, cli_json_member(json, value, "include"), NULL);
    if (*own >= s_read->own_most)
    {
        s_full = 1;
        return 1;
    }
    if (included != NULL)
    {
        const char *const name = (included[0] == '@') ? (included + 1) : included;
        s_read->own[*own] =
            cli_text(grammar, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), 0) |
            CLI_INCLUDE_MARK;
        *own += 1u;
        return 1;
    }
    if (!cli_json_is(json, value, CFG_JSON_ARRAY))
    {
        cli_line_text(said, "a rule that is no list");
        return 0;
    }
    const unsigned int pattern = cli_json_item(json, value, 0u);
    if (pattern == CLI_JSON_NONE)
    {
        cli_line_text(said, "a rule with no pattern");
        return 0;
    }
    if (grammar->rules >= s_read->rules_most)
    {
        s_full = 1;
        return 1;
    }
    CliHighlightRule *const rule = &grammar->rule[grammar->rules];
    if (!cli_pattern_of(grammar, json, pattern, rule, said))
    {
        return 0;
    }
    const unsigned int action = cli_json_item(json, value, 1u);
    const int own_next =
        cli_json_is(json, action, CFG_JSON_OBJECT) && (cli_json_member(json, action, "cases") == CLI_JSON_NONE);
    const unsigned int token = own_next ? cli_json_member(json, action, "token") : action;
    const char *const next =
        cli_json_string(json, own_next ? cli_json_member(json, action, "next") : cli_json_item(json, value, 2u), NULL);
    if (!cli_token_of(grammar, json, token, &rule->token, said))
    {
        return 0;
    }
    rule->next = (next != NULL)
                     ? cli_text(grammar, next, cellul.len(&(CatenaFinitaCfg){.src = next, .cap = CLI_CELLS_POOL}), 0)
                     : CLI_HIGHLIGHT_NONE;
    s_read->own[*own] = grammar->rules;
    *own += 1u;
    grammar->rules += 1u;
    return 1;
}

// A state's rules with each include read in its place, a state included within itself once
static void cli_expanded(CliGrammar *grammar, unsigned int state, unsigned int *seen, unsigned int depth)
{
    const CliHighlightList *const own = &s_read->own_state[state];
    for (unsigned int at = own->first; at < (own->first + own->count); at += 1u)
    {
        const unsigned int entry = s_read->own[at];
        if ((entry & CLI_INCLUDE_MARK) == 0u)
        {
            if (grammar->listed_count >= s_read->listed_most)
            {
                s_full = 1;
                return;
            }
            grammar->listed[grammar->listed_count] = entry;
            grammar->listed_count += 1u;
            continue;
        }
        const char *const name = grammar->text + (entry & ~CLI_INCLUDE_MARK);
        int already = 0;
        for (unsigned int each = 0u; each < depth; each += 1u)
        {
            already = already || (cellul.eq(&(CatenaFinitaCfg){
                                     .src = grammar->text + seen[each], .other = name, .cap = CLI_CELLS_POOL}));
        }
        unsigned int included = CLI_HIGHLIGHT_NONE;
        for (unsigned int each = 0u; each < grammar->states; each += 1u)
        {
            included = (cellul.eq(&(CatenaFinitaCfg){
                           .src = grammar->text + s_read->own_state[each].name, .other = name, .cap = CLI_CELLS_POOL}))
                           ? each
                           : included;
        }
        if (!already && (depth < CLI_GRAMMAR_DEPTH))
        {
            seen[depth] = entry & ~CLI_INCLUDE_MARK;
            if (included != CLI_HIGHLIGHT_NONE)
            {
                cli_expanded(grammar, included, seen, depth + 1u);
            }
        }
    }
}

// the words of each member of the grammar that is a list, each once, lower case where the grammar ignores case, in
// order so a word is found by halves
static void cli_words_read(CliGrammar *grammar, const CliJson *json, unsigned int def)
{
    for (unsigned int member = json->node[def].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        if ((json->node[member].kind != CFG_JSON_ARRAY) || (grammar->word_lists >= s_read->word_lists_most))
        {
            continue;
        }
        const char *const name = json->text + json->node[member].key;
        CliHighlightList *const list = &grammar->words[grammar->word_lists];
        list->name = cli_text(grammar, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), 0);
        list->first = grammar->word_count;
        list->count = 0u;
        for (unsigned int item = json->node[member].first; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            unsigned long long length = 0ull;
            const char *const word = cli_json_string(json, item, &length);
            if ((word == NULL) || (grammar->word_count >= s_read->words_most))
            {
                continue;
            }
            const unsigned int text = cli_text(grammar, word, length, grammar->ignore_case);
            // put in its place among the list's words, none twice
            unsigned int place = list->count;
            int twice = 0;
            while ((place > 0u) && !twice)
            {
                const int order =
                    cli_text_order(grammar->text + grammar->word[list->first + place - 1u], grammar->text + text);
                twice = order == 0;
                if (order <= 0)
                {
                    break;
                }
                place -= 1u;
            }
            if (twice)
            {
                continue;
            }
            for (unsigned int at = list->count; at > place; at -= 1u)
            {
                grammar->word[list->first + at] = grammar->word[list->first + at - 1u];
            }
            grammar->word[list->first + place] = text;
            list->count += 1u;
            grammar->word_count += 1u;
        }
        grammar->word_lists += 1u;
    }
}

int cli_highlight_read(const CliJson *json, unsigned int def, CliGrammarRoom *room, CliGrammar *grammar, CliLine *said)
{
    if (!cli_json_is(json, def, CFG_JSON_OBJECT))
    {
        cli_line_text(said, "a grammar that is no object");
        return 0;
    }
    s_read = room;
    memor.set(&(MemoriaCfg){.dst = grammar, .val = 0, .bytes = sizeof(*grammar)});
    grammar->node = room->node;
    grammar->item = room->item;
    grammar->part = room->part;
    grammar->part_first = room->part_first;
    grammar->rule = room->rule;
    grammar->token = room->token;
    grammar->cases = room->cases;
    grammar->listed = room->listed;
    grammar->state = room->state;
    grammar->word = room->word;
    grammar->words = room->words;
    grammar->name = room->name;
    grammar->text = room->text;
    s_full = 0;
    grammar->ignore_case = cli_json_is(json, cli_json_member(json, def, "ignoreCase"), CFG_JSON_TRUE);
    grammar->per_line = cli_json_is(json, cli_json_member(json, def, "perLine"), CFG_JSON_TRUE);
    cli_words_read(grammar, json, def);
    const unsigned int tokenizer = cli_json_member(json, def, "tokenizer");
    if (!cli_json_is(json, tokenizer, CFG_JSON_OBJECT))
    {
        cli_line_text(said, "a grammar with no tokenizer");
        return 0;
    }
    grammar->name[0] = cli_text(grammar, "", 0u, 0);
    grammar->names = 1u;
    unsigned int own = 0u;
    for (unsigned int state = json->node[tokenizer].first; state != CLI_JSON_NONE; state = json->node[state].next)
    {
        if (!cli_json_is(json, state, CFG_JSON_ARRAY))
        {
            cli_line_text(said, "a state that is no list");
            return 0;
        }
        if (grammar->states >= s_read->states_most)
        {
            s_full = 1;
            break;
        }
        const char *const name = json->text + json->node[state].key;
        CliHighlightList *const entry = &s_read->own_state[grammar->states];
        entry->name = cli_text(grammar, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), 0);
        entry->first = own;
        for (unsigned int rule = json->node[state].first; rule != CLI_JSON_NONE; rule = json->node[rule].next)
        {
            if (!cli_rule_read(grammar, json, rule, &own, said))
            {
                return 0;
            }
        }
        entry->count = own - entry->first;
        grammar->states += 1u;
    }
    for (unsigned int state = 0u; state < grammar->states; state += 1u)
    {
        unsigned int seen[CLI_GRAMMAR_DEPTH];
        seen[0] = s_read->own_state[state].name;
        grammar->state[state].name = s_read->own_state[state].name;
        grammar->state[state].first = grammar->listed_count;
        cli_expanded(grammar, state, seen, 1u);
        grammar->state[state].count = grammar->listed_count - grammar->state[state].first;
    }
    if (s_full)
    {
        cli_line_text(said, "a grammar that holds more than a grammar is read into");
        return 0;
    }
    return 1;
}
