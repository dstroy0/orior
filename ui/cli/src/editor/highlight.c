// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight.c: a grammar read from its JSON into a work state, its patterns, tokens, states and words
#include "../text/text.h"
#include "highlight_internal.h"

// the most states an include reads through
#define CLI_GRAMMAR_DEPTH 64u

// an own entry that is an include and not a rule: the top bit set on its name's text
#define CLI_INCLUDE_MARK 0x80000000u

// the work state the grammar under way is read into, the one the store handed the read
static CliGrammarWorkState *s_read;

// 1 where what is read does not fit the work state
static int s_full;

// the grammar's brackets, each pair two letters, the one that opens and the one that closes; none where it has none;
// and the name the grammar's `bracketToken` gives a bracket's token, `delimiter.bracket` where it gives none
static unsigned int s_brackets;
static char s_bracket_name[CLI_NAME_MAX_LEN];

typedef struct
{
    const char *role;
    char *out;
    CliGrammar *grammar;
    const char *text;
    unsigned long long length;
    int lower;
    const char *name;
    unsigned int kind;
    unsigned int name_u32;
    unsigned int place;
    const char *key;
    unsigned long long size;
    const CliJson *json;
    unsigned int item;
    const char **pair;
    unsigned long long *first;
    unsigned long long *second;
    unsigned int value;
    unsigned int *token;
    VerbaLinea *err_msg;
    CliHighlightRule *rule;
    unsigned int *own;
    unsigned int state;
    unsigned int *seen;
    unsigned int depth;
    unsigned int def;
    const CliGrammar *grammar_read;
    unsigned int node;
    unsigned int indent;
} HighlightCtx;
// the name of a bracket's token, written into `out`, followed by `.` and `role` where `role` is not empty
EMBED_INLINE void cli_bracket_by_name(const HighlightCtx *args)
{
    VerbaLinea line = {args->out, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, s_bracket_name);
    if (args->role[0] != '\0')
    {
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, args->role);
    }
    (void)mmgr_line_finish(&line);
}

// `length` bytes of `text` put among the grammar's texts, lower case where `lower` is 1: where they start
EMBED_INLINE unsigned int cli_text(const HighlightCtx *args)
{
    const char *text = args->text;
    unsigned int letters[CLI_NAME_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = args->length, .letters = letters,
                   .capacity_u64 = CLI_NAME_MAX_LEN);
    VerbaLinea line = {args->grammar->text + args->grammar->text_size, s_read->text_max - args->grammar->text_size,
                       0ull};
    if (args->lower && (count < CLI_NAME_MAX_LEN))
    {
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                       .letter = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = letters[at]));
        }
    }
    else
    {
        mmgr_line_put_n(&line, text, args->length);
    }
    if (!mmgr_line_ok(&line))
    {
        s_full = 1;
        return 0u;
    }
    const unsigned int at = args->grammar->text_size;
    // the texts are below the work state's text_max, which an unsigned int counts
    args->grammar->text_size += (unsigned int)mmgr_line_finish(&line) + 1u;
    return at;
}

// The place of a token's name among the names, added where it is new; a bracket's name is the grammar's bracket name
EMBED_INLINE unsigned int cli_name_get(const HighlightCtx *args)
{
    const char *const named =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "@brackets", .cap = CLI_CELLS_POOL})) ? s_bracket_name
                                                                                                        : args->name;
    for (unsigned int at = 0u; at < args->grammar->names; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->grammar->text + args->grammar->name[at], .other = named, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    if (args->grammar->names >= s_read->names_max)
    {
        s_full = 1;
        return 0u;
    }
    args->grammar->name[args->grammar->names] =
        EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar, .text = named,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), .lower = 0);
    args->grammar->names += 1u;
    return args->grammar->names - 1u;
}

EMBED_INLINE unsigned int cli_token_init(const HighlightCtx *args)
{
    if (args->grammar->tokens >= s_read->tokens_max)
    {
        s_full = 1;
        return CLI_HIGHLIGHT_NONE;
    }
    const CliHighlightToken token = {args->kind, args->name_u32, 0u, 0u};
    args->grammar->token[args->grammar->tokens] = token;
    args->grammar->tokens += 1u;
    return args->grammar->tokens - 1u;
}

// a case put at `place` among the grammar's cases: `size` bytes of `key` naming `name`
EMBED_INLINE void cli_case_append(const HighlightCtx *args)
{
    const CliHighlightCase entry = {EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar, .text = args->key,
                                               .length = args->size, .lower = 0),
                                    EMBED_CALL(cli_token_init, HighlightCtx, .grammar = args->grammar,
                                               .kind = CLI_TOKEN_NAME, .name_u32 = cli_name_get(args)),
                                    CLI_HIGHLIGHT_NONE, 0};
    args->grammar->cases[args->place] = entry;
}

// the pair at `item` of the grammar's brackets, `*pair`, and the bytes its first letter takes, `*first`, and its
// second,
// `*second`: 1 where it is two letters that differ, else 0
EMBED_INLINE int cli_pair_split(const HighlightCtx *args)
{
    const CliJson *json = args->json;
    const char **pair = args->pair;
    unsigned long long *first = args->first;
    unsigned long long *second = args->second;
    unsigned long long size = 0ull;
    *pair = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = args->item, .length_pointer = &size);
    unsigned int opens = 0u;
    unsigned int closes = 0u;
    *first = (*pair != NULL)
                 ? EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                              .text_bytes = (const unsigned char *)*pair, .length = size, .at = 0ull, .letter = &opens)
                 : 0ull;
    *second = (*first < size) ? EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                           .text_bytes = (const unsigned char *)*pair, .length = size, .at = *first,
                                           .letter = &closes)
                              : 0ull;
    return ((*first + *second) == size) && (*second != 0ull) && (opens != closes);
}

// the token `@brackets` names: a bracket the grammar's brackets give as a pair's first letter the grammar's bracket
// name with `.open`, one they give as its second that name with `.close`, and any other that name, the name alone
// where no pair of two letters of its own gives one
EMBED_INLINE unsigned int cli_brackets_token(const HighlightCtx *args)
{
    const CliJson *json = args->json;
    char opens[CLI_NAME_MAX_LEN];
    char closes[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_bracket_by_name, HighlightCtx, .role = "open", .out = opens);
    EMBED_CALL(cli_bracket_by_name, HighlightCtx, .role = "close", .out = closes);
    const unsigned int list_entries = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                 .json_read = json, .node = s_brackets, .kind = CFG_JSON_ARRAY)
                                          ? json->node[s_brackets].first
                                          : CLI_JSON_NONE;
    unsigned int pairs = 0u;
    for (unsigned int item = list_entries; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *pair = NULL;
        unsigned long long first = 0ull;
        unsigned long long second = 0ull;
        pairs += EMBED_CALL(cli_pair_split, HighlightCtx, .json = json, .item = item, .pair = &pair, .first = &first,
                            .second = &second)
                     ? 1u
                     : 0u;
    }
    if (pairs == 0u)
    {
        return EMBED_CALL(cli_token_init, HighlightCtx, .grammar = args->grammar, .kind = CLI_TOKEN_NAME,
                          .name_u32 =
                              EMBED_CALL(cli_name_get, HighlightCtx, .grammar = args->grammar, .name = s_bracket_name));
    }
    const unsigned int made =
        EMBED_CALL(cli_token_init, HighlightCtx, .grammar = args->grammar, .kind = CLI_TOKEN_CASES, .name_u32 = 0u);
    const unsigned int count = (2u * pairs) + 1u;
    if ((made == CLI_HIGHLIGHT_NONE) || ((args->grammar->case_count + count) > s_read->tokens_max))
    {
        s_full = 1;
        return made;
    }
    const unsigned int first_case = args->grammar->case_count;
    args->grammar->case_count += count;
    args->grammar->token[made].first = first_case;
    args->grammar->token[made].count = count;
    unsigned int place = first_case;
    for (unsigned int item = list_entries; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *pair = NULL;
        unsigned long long first = 0ull;
        unsigned long long second = 0ull;
        if (!EMBED_CALL(cli_pair_split, HighlightCtx, .json = json, .item = item, .pair = &pair, .first = &first,
                        .second = &second))
        {
            continue;
        }
        EMBED_CALL(cli_case_append, HighlightCtx, .grammar = args->grammar, .place = place, .key = pair, .size = first,
                   .name = opens);
        EMBED_CALL(cli_case_append, HighlightCtx, .grammar = args->grammar, .place = place + 1u, .key = pair + first,
                   .size = second, .name = closes);
        place += 2u;
    }
    EMBED_CALL(cli_case_append, HighlightCtx, .grammar = args->grammar, .place = place, .key = "@default", .size = 8ull,
               .name = s_bracket_name);
    return made;
}

// the token a value writes into `token`, none for null: 1, or 0 with why given
static int cli_token_get(const HighlightCtx *args)
{
    const CliJson *json = args->json;
    unsigned int *token = args->token;
    *token = CLI_HIGHLIGHT_NONE;
    if ((args->value == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                     .json_read = json, .node = args->value, .kind = CFG_JSON_NULL))
    {
        return 1;
    }
    const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = args->value, .length_pointer = NULL);
    if ((name != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "@brackets", .cap = CLI_CELLS_POOL}))
    {
        *token = EMBED_CALL(cli_brackets_token, HighlightCtx, .grammar = args->grammar, .json = json);
        return 1;
    }
    if (name != NULL)
    {
        *token = EMBED_CALL(cli_token_init, HighlightCtx, .grammar = args->grammar, .kind = CLI_TOKEN_NAME,
                            .name_u32 = EMBED_CALL(cli_name_get, HighlightCtx, .grammar = args->grammar, .name = name));
        return 1;
    }
    const unsigned int cases = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->value, .name = "cases");
    const int grouped = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                   .node = args->value, .kind = CFG_JSON_ARRAY);
    if (!grouped && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                .node = cases, .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_put(args->err_msg, "a token that is neither a name, a list nor cases");
        return 0;
    }
    const unsigned int list = grouped ? args->value : cases;
    const unsigned int count = json->node[list].count;
    const unsigned int made = EMBED_CALL(cli_token_init, HighlightCtx, .grammar = args->grammar,
                                         .kind = grouped ? CLI_TOKEN_GROUPS : CLI_TOKEN_CASES, .name_u32 = 0u);
    if ((made == CLI_HIGHLIGHT_NONE) || ((args->grammar->case_count + count) > s_read->tokens_max))
    {
        s_full = 1;
        return 1;
    }
    // the range of the cases placed first, so the tokens under them go after it
    const unsigned int first = args->grammar->case_count;
    args->grammar->case_count += count;
    args->grammar->token[made].first = first;
    args->grammar->token[made].count = count;
    unsigned int place = first;
    for (unsigned int child = json->node[list].first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        unsigned int inner = CLI_HIGHLIGHT_NONE;
        if (!EMBED_CALL(cli_token_get, HighlightCtx, .grammar = args->grammar, .json = json, .value = child,
                        .token = &inner, .err_msg = args->err_msg))
        {
            return 0;
        }
        const char *const key = grouped ? NULL : (json->text + json->node[child].key);
        const unsigned int key_text =
            (key != NULL)
                ? EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar, .text = key,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}), .lower = 0)
                : CLI_HIGHLIGHT_NONE;
        const CliHighlightCase entry = {key_text,
                                        (inner != CLI_HIGHLIGHT_NONE)
                                            ? inner
                                            : EMBED_CALL(cli_token_init, HighlightCtx, .grammar = args->grammar,
                                                         .kind = CLI_TOKEN_NAME, .name_u32 = 0u),
                                        CLI_HIGHLIGHT_NONE, 0};
        args->grammar->cases[place] = entry;
        place += 1u;
    }
    *token = made;
    return 1;
}

// a rule's pattern read, its arrays put after the grammar's: 1, or 0 with why given
static int cli_pattern_get(const HighlightCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *err_msg = args->err_msg;
    const char *source = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                    .node = args->value, .length_pointer = NULL);
    const char *flags = "";
    if ((source == NULL) && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = args->value, .kind = CFG_JSON_OBJECT))
    {
        source = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = args->value, .name = "pattern"),
                            .length_pointer = NULL);
        if (source == NULL)
        {
            mmgr_line_put(err_msg, "a pattern with no source");
            return 0;
        }
        const char *const supplied =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->value, .name = "flags"),
                       .length_pointer = NULL);
        flags = (supplied != NULL) ? supplied : "";
    }
    else if (source == NULL)
    {
        mmgr_line_put(err_msg, "a pattern that is neither text nor a source and flags");
        return 0;
    }
    CliRegexp read;
    EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &read, .work_state = s_read->pattern);
    if (!EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &read, .source = source, .flags = flags,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (((args->grammar->nodes + read.nodes) > s_read->nodes_max) ||
        ((args->grammar->items + read.items) > s_read->items_max) ||
        ((args->grammar->parts + read.parts) > s_read->parts_max))
    {
        s_full = 1;
        return 1;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->grammar->node + args->grammar->nodes,
                            .src = read.node,
                            .bytes = read.nodes * sizeof(read.node[0])});
    memor.cpy(&(MemoriaCfg){.dst = args->grammar->item + args->grammar->items,
                            .src = read.item,
                            .bytes = read.items * sizeof(read.item[0])});
    memor.cpy(&(MemoriaCfg){.dst = args->grammar->part + args->grammar->parts,
                            .src = read.part,
                            .bytes = read.parts * sizeof(read.part[0])});
    memor.cpy(&(MemoriaCfg){.dst = args->grammar->part_first + args->grammar->parts,
                            .src = read.part_first,
                            .bytes = read.parts * sizeof(read.part_first[0])});
    args->rule->node_first = args->grammar->nodes;
    args->rule->nodes = read.nodes;
    args->rule->item_first = args->grammar->items;
    args->rule->items = read.items;
    args->rule->part_first = args->grammar->parts;
    args->rule->parts = read.parts;
    args->rule->root = read.root;
    args->rule->groups = read.groups;
    args->rule->fold = read.fold;
    args->rule->first = read.first;
    args->rule->anchored = read.anchored;
    args->grammar->nodes += read.nodes;
    args->grammar->items += read.items;
    args->grammar->parts += read.parts;
    return 1;
}

void cli_highlight_pattern(const CliHighlightCtx *args)
{
    args->pattern->node = args->grammar->node + args->rule->node_first;
    args->pattern->nodes = args->rule->nodes;
    args->pattern->item = args->grammar->item + args->rule->item_first;
    args->pattern->items = args->rule->items;
    args->pattern->part = args->grammar->part + args->rule->part_first;
    args->pattern->part_first = args->grammar->part_first + args->rule->part_first;
    args->pattern->parts = args->rule->parts;
    args->pattern->root = args->rule->root;
    args->pattern->groups = args->rule->groups;
    args->pattern->fold = args->rule->fold;
    args->pattern->first = args->rule->first;
    args->pattern->anchored = args->rule->anchored;
}

// one rule of a state read: an include, or a pattern with its token and the state it goes to. 1, or 0 with why given
EMBED_INLINE int cli_rule_read(const HighlightCtx *args)
{
    const CliJson *json = args->json;
    unsigned int *own = args->own;
    const char *const included =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->value, .name = "include"),
                   .length_pointer = NULL);
    if (*own >= s_read->own_max)
    {
        s_full = 1;
        return 1;
    }
    if (included != NULL)
    {
        const char *const name = (included[0] == '@') ? (included + 1) : included;
        s_read->own[*own] =
            EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar, .text = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .lower = 0) |
            CLI_INCLUDE_MARK;
        *own += 1u;
        return 1;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->value,
                    .kind = CFG_JSON_ARRAY))
    {
        mmgr_line_put(args->err_msg, "a rule that is no list");
        return 0;
    }
    const unsigned int pattern = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = args->value, .index = 0u);
    if (pattern == CLI_JSON_NONE)
    {
        mmgr_line_put(args->err_msg, "a rule with no pattern");
        return 0;
    }
    if (args->grammar->rules >= s_read->rules_max)
    {
        s_full = 1;
        return 1;
    }
    CliHighlightRule *const rule = &args->grammar->rule[args->grammar->rules];
    if (!EMBED_CALL(cli_pattern_get, HighlightCtx, .grammar = args->grammar, .json = json, .value = pattern,
                    .rule = rule, .err_msg = args->err_msg))
    {
        return 0;
    }
    const unsigned int action = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = args->value, .index = 1u);
    const int own_next = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                    .node = action, .kind = CFG_JSON_OBJECT) &&
                         (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                     .object = action, .name = "cases") == CLI_JSON_NONE);
    const unsigned int token = own_next ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = json, .object = action, .name = "token")
                                        : action;
    const char *const next =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = own_next ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = action, .name = "next")
                                    : EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                 .json_read = json, .array = args->value, .index = 2u),
                   .length_pointer = NULL);
    if (!EMBED_CALL(cli_token_get, HighlightCtx, .grammar = args->grammar, .json = json, .value = token,
                    .token = &rule->token, .err_msg = args->err_msg))
    {
        return 0;
    }
    rule->next =
        (next != NULL)
            ? EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar, .text = next,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = next, .cap = CLI_CELLS_POOL}), .lower = 0)
            : CLI_HIGHLIGHT_NONE;
    s_read->own[*own] = args->grammar->rules;
    *own += 1u;
    args->grammar->rules += 1u;
    return 1;
}

// A state's rules with each include read in its place, a state included within itself once
static void cli_on_expand(const HighlightCtx *args)
{
    const CliHighlightList *const own = &s_read->own_state[args->state];
    for (unsigned int at = own->first; at < (own->first + own->count); at += 1u)
    {
        const unsigned int entry = s_read->own[at];
        if ((entry & CLI_INCLUDE_MARK) == 0u)
        {
            if (args->grammar->listing_count >= s_read->listing_max)
            {
                s_full = 1;
                return;
            }
            args->grammar->list_entries[args->grammar->listing_count] = entry;
            args->grammar->listing_count += 1u;
            continue;
        }
        const char *const name = args->grammar->text + (entry & ~CLI_INCLUDE_MARK);
        int already = 0;
        for (unsigned int each = 0u; each < args->depth; each += 1u)
        {
            already =
                already || (cellul.eq(&(CatenaFinitaCfg){
                               .src = args->grammar->text + args->seen[each], .other = name, .cap = CLI_CELLS_POOL}));
        }
        unsigned int included = CLI_HIGHLIGHT_NONE;
        for (unsigned int each = 0u; each < args->grammar->states; each += 1u)
        {
            included =
                (cellul.eq(&(CatenaFinitaCfg){
                    .src = args->grammar->text + s_read->own_state[each].name, .other = name, .cap = CLI_CELLS_POOL}))
                    ? each
                    : included;
        }
        if (!already && (args->depth < CLI_GRAMMAR_DEPTH))
        {
            args->seen[args->depth] = entry & ~CLI_INCLUDE_MARK;
            if (included != CLI_HIGHLIGHT_NONE)
            {
                EMBED_CALL(cli_on_expand, HighlightCtx, .grammar = args->grammar, .state = included, .seen = args->seen,
                           .depth = args->depth + 1u);
            }
        }
    }
}

// the words of each member of the grammar that is a list, each once, lower case where the grammar ignores case, in
// order so a word is found by halves
EMBED_INLINE void cli_words_read(const HighlightCtx *args)
{
    const CliJson *json = args->json;
    for (unsigned int member = json->node[args->def].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        if ((json->node[member].kind != CFG_JSON_ARRAY) || (args->grammar->word_lists >= s_read->word_lists_max))
        {
            continue;
        }
        const char *const name = json->text + json->node[member].key;
        CliHighlightList *const list = &args->grammar->words[args->grammar->word_lists];
        list->name =
            EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar, .text = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .lower = 0);
        list->first = args->grammar->word_count;
        list->count = 0u;
        for (unsigned int item = json->node[member].first; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            unsigned long long length = 0ull;
            const char *const word = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = json, .node = item, .length_pointer = &length);
            if ((word == NULL) || (args->grammar->word_count >= s_read->words_max))
            {
                continue;
            }
            const unsigned int text = EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar, .text = word,
                                                 .length = length, .lower = args->grammar->ignore_case);
            // put in its place among the list's words, none twice
            unsigned int place = list->count;
            int twice = 0;
            while ((place > 0u) && !twice)
            {
                const int order = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                             .one = args->grammar->text + args->grammar->word[list->first + place - 1u],
                                             .other_pointer = args->grammar->text + text);
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
                args->grammar->word[list->first + at] = args->grammar->word[list->first + at - 1u];
            }
            args->grammar->word[list->first + place] = text;
            list->count += 1u;
            args->grammar->word_count += 1u;
        }
        args->grammar->word_lists += 1u;
    }
}

// the place among the grammar's states of the state the JSON string `node` names, none where it names none
EMBED_INLINE unsigned int cli_state_by_name(const HighlightCtx *args)
{
    const CliGrammar *grammar = args->grammar_read;
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned long long length = 0ull;
    const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = node, .length_pointer = &length);
    return (name != NULL) ? EMBED_CALL(cli_highlight_name_internal.state, CliHighlightNameCtx, .grammar = grammar,
                                       .name = name, .length = length)
                          : CLI_HIGHLIGHT_NONE;
}

// the grammar's `indent` at `indent` read, none where it has none: each state it runs with the name its close takes,
// the columns a tab reaches to the next multiple of, the pattern of a line it passes over as a rule of no state, and
// the states a line begun with on top begins a statement. 1, or 0 with why given
EMBED_INLINE int cli_indent_read(const HighlightCtx *args)
{
    CliGrammar *grammar = args->grammar;
    const CliJson *json = args->json;
    grammar->tab = 1u;
    grammar->skip = CLI_HIGHLIGHT_NONE;
    const unsigned int states = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->indent, .name = "states");
    const unsigned int named = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = states, .kind = CFG_JSON_OBJECT)
                                   ? json->node[states].first
                                   : CLI_JSON_NONE;
    for (unsigned int member = named; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        const char *const state = json->text + json->node[member].key;
        const char *const close = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                             .json_read = json, .node = member, .length_pointer = NULL);
        const unsigned int at =
            EMBED_CALL(cli_highlight_name_internal.state, CliHighlightNameCtx, .grammar = grammar, .name = state,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = state, .cap = CLI_CELLS_POOL}));
        if ((close != NULL) && (at != CLI_HIGHLIGHT_NONE))
        {
            grammar->state[at].close = EMBED_CALL(cli_name_get, HighlightCtx, .grammar = grammar, .name = close);
            grammar->indents = 1;
        }
    }
    const unsigned int begins = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->indent, .name = "begins");
    const unsigned int list_entries = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                 .json_read = json, .node = begins, .kind = CFG_JSON_ARRAY)
                                          ? json->node[begins].first
                                          : CLI_JSON_NONE;
    for (unsigned int item = list_entries; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const unsigned int at =
            EMBED_CALL(cli_state_by_name, HighlightCtx, .grammar_read = grammar, .json = json, .node = item);
        if (at != CLI_HIGHLIGHT_NONE)
        {
            grammar->state[at].begins = 1;
        }
    }
    const unsigned int tab = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->indent, .name = "tab");
    unsigned int columns = 0u;
    for (unsigned long long digit = 0ull;
         EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = tab,
                    .kind = CFG_JSON_NUMBER) &&
         (digit < json->node[tab].length) && (json->text[json->node[tab].text + digit] >= '0') &&
         (json->text[json->node[tab].text + digit] <= '9') && (columns < 1000u);
         digit += 1ull)
    {
        columns = (columns * 10u) + (unsigned int)(json->text[json->node[tab].text + digit] - '0');
    }
    grammar->tab = (columns != 0u) ? columns : 1u;
    const unsigned int skip = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->indent, .name = "skip");
    if (!grammar->indents || (skip == CLI_JSON_NONE))
    {
        return 1;
    }
    if (grammar->rules >= s_read->rules_max)
    {
        s_full = 1;
        return 1;
    }
    CliHighlightRule *const rule = &grammar->rule[grammar->rules];
    if (!EMBED_CALL(cli_pattern_get, HighlightCtx, .grammar = grammar, .json = json, .value = skip, .rule = rule,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    rule->token = CLI_HIGHLIGHT_NONE;
    rule->next = CLI_HIGHLIGHT_NONE;
    grammar->skip = grammar->rules;
    grammar->rules += 1u;
    return 1;
}

int cli_highlight_read(const CliHighlightJsonCtx *args)
{
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = args->def,
                    .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_put(args->err_msg, "a grammar that is no object");
        return 0;
    }
    s_read = args->grammar_work_state_pointer;
    memor.set(&(MemoriaCfg){.dst = args->grammar_out, .val = 0, .bytes = sizeof(*args->grammar_out)});
    args->grammar_out->node = args->grammar_work_state_pointer->node;
    args->grammar_out->item = args->grammar_work_state_pointer->item;
    args->grammar_out->part = args->grammar_work_state_pointer->part;
    args->grammar_out->part_first = args->grammar_work_state_pointer->part_first;
    args->grammar_out->rule = args->grammar_work_state_pointer->rule;
    args->grammar_out->token = args->grammar_work_state_pointer->token;
    args->grammar_out->cases = args->grammar_work_state_pointer->cases;
    args->grammar_out->list_entries = args->grammar_work_state_pointer->list_entries;
    args->grammar_out->state = args->grammar_work_state_pointer->state;
    args->grammar_out->word = args->grammar_work_state_pointer->word;
    args->grammar_out->words = args->grammar_work_state_pointer->words;
    args->grammar_out->name = args->grammar_work_state_pointer->name;
    args->grammar_out->text = args->grammar_work_state_pointer->text;
    s_full = 0;
    args->grammar_out->ignore_case =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->def, .name = "ignoreCase"),
                   .kind = CFG_JSON_TRUE);
    args->grammar_out->per_line =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->def, .name = "perLine"),
                   .kind = CFG_JSON_TRUE);
    s_brackets = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                            .object = args->def, .name = "brackets");
    unsigned long long bracket_size = 0ull;
    const char *const bracket_name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->def, .name = "bracketToken"),
                   .length_pointer = &bracket_size);
    VerbaLinea bracket_line = {s_bracket_name, sizeof(s_bracket_name), 0ull};
    if ((bracket_name != NULL) && (bracket_size != 0ull))
    {
        mmgr_line_put_n(&bracket_line, bracket_name, bracket_size);
    }
    else
    {
        mmgr_line_put(&bracket_line, "delimiter.bracket");
    }
    (void)mmgr_line_finish(&bracket_line);
    EMBED_CALL(cli_words_read, HighlightCtx, .grammar = args->grammar_out, .json = args->json, .def = args->def);
    const unsigned int tokenizer = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = args->def, .name = "tokenizer");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = tokenizer,
                    .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_put(args->err_msg, "a grammar with no tokenizer");
        return 0;
    }
    args->grammar_out->name[0] =
        EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar_out, .text = "", .length = 0u, .lower = 0);
    args->grammar_out->names = 1u;
    unsigned int own = 0u;
    for (unsigned int state = args->json->node[tokenizer].first; state != CLI_JSON_NONE;
         state = args->json->node[state].next)
    {
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = state,
                        .kind = CFG_JSON_ARRAY))
        {
            mmgr_line_put(args->err_msg, "a state that is no list");
            return 0;
        }
        if (args->grammar_out->states >= s_read->states_max)
        {
            s_full = 1;
            break;
        }
        const char *const name = args->json->text + args->json->node[state].key;
        CliHighlightList *const entry = &s_read->own_state[args->grammar_out->states];
        entry->name =
            EMBED_CALL(cli_text, HighlightCtx, .grammar = args->grammar_out, .text = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .lower = 0);
        entry->first = own;
        for (unsigned int rule = args->json->node[state].first; rule != CLI_JSON_NONE;
             rule = args->json->node[rule].next)
        {
            if (!EMBED_CALL(cli_rule_read, HighlightCtx, .grammar = args->grammar_out, .json = args->json,
                            .value = rule, .own = &own, .err_msg = args->err_msg))
            {
                return 0;
            }
        }
        entry->count = own - entry->first;
        args->grammar_out->states += 1u;
    }
    for (unsigned int state = 0u; state < args->grammar_out->states; state += 1u)
    {
        unsigned int seen[CLI_GRAMMAR_DEPTH];
        seen[0] = s_read->own_state[state].name;
        args->grammar_out->state[state].name = s_read->own_state[state].name;
        args->grammar_out->state[state].close = CLI_HIGHLIGHT_NONE;
        args->grammar_out->state[state].begins = 0;
        args->grammar_out->state[state].first = args->grammar_out->listing_count;
        EMBED_CALL(cli_on_expand, HighlightCtx, .grammar = args->grammar_out, .state = state, .seen = seen,
                   .depth = 1u);
        args->grammar_out->state[state].count =
            args->grammar_out->listing_count - args->grammar_out->state[state].first;
        // the letters a rule of the state that colors or changes the state starts at, each such rule's first letters
        // joined, and whether any takes nothing; a rule with the plain token and no next is a skip, which a stride over
        // its letters as plain is the same as, and it joins nothing
        CliRegexpFirst *const starts = &args->grammar_out->state[state].starts;
        starts->ascii[0] = 0ull;
        starts->ascii[1] = 0ull;
        starts->other = 0;
        starts->empty = 0;
        for (unsigned int each = 0u; each < args->grammar_out->state[state].count; each += 1u)
        {
            const CliHighlightRule *const rule =
                &args->grammar_out->rule[args->grammar_out->list_entries[args->grammar_out->state[state].first + each]];
            const CliHighlightToken *const token =
                (rule->token != CLI_HIGHLIGHT_NONE) ? &args->grammar_out->token[rule->token] : NULL;
            const int plain = (token == NULL) || ((token->kind == CLI_TOKEN_NAME) && (token->name == 0u));
            if (plain && (rule->next == CLI_HIGHLIGHT_NONE) && !rule->first.empty)
            {
                continue;
            }
            starts->ascii[0] |= rule->first.ascii[0];
            starts->ascii[1] |= rule->first.ascii[1];
            starts->other |= rule->first.other;
            starts->empty |= rule->first.empty;
        }
        // the ASCII letters among them as the table the colorer's stride reads, one byte a letter
        for (unsigned int byte = 0u; byte < 128u; byte += 1u)
        {
            args->grammar_out->state[state].starts_at[byte] =
                (unsigned char)((starts->ascii[byte / 64u] >> (byte % 64u)) & 1ull);
        }
    }
    if (!EMBED_CALL(cli_indent_read, HighlightCtx, .grammar = args->grammar_out, .json = args->json,
                    .indent = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->def, .name = "indent"),
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    // each case's text read once here: `@default` is the fallback, and a text starting with `@` names a word list
    for (unsigned int at = 0u; at < args->grammar_out->case_count; at += 1u)
    {
        CliHighlightCase *const entry = &args->grammar_out->cases[at];
        const char *const key = (entry->key != CLI_HIGHLIGHT_NONE) ? args->grammar_out->text + entry->key : "";
        entry->fallback = cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "@default", .cap = CLI_CELLS_POOL});
        entry->list = CLI_HIGHLIGHT_NONE;
        for (unsigned int list = 0u; !entry->fallback && (key[0] == '@') && (list < args->grammar_out->word_lists);
             list += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = args->grammar_out->text + args->grammar_out->words[list].name,
                                             .other = key + 1,
                                             .cap = CLI_CELLS_POOL}))
            {
                entry->list = list;
                break;
            }
        }
    }
    if (s_full)
    {
        mmgr_line_put(args->err_msg, "a grammar that contains more than a grammar is read into");
        return 0;
    }
    return 1;
}
