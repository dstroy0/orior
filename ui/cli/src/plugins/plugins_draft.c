// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_draft.c: the plugin a few answers request: made from nothing, or from a plugin to start from with what the
// answers name changed
#include "../text/text.h"
#include "plugins.h"

// the most bytes of a pattern made; and the nodes and bytes of texts a draft makes besides the copies of its answers
// and the plugin it starts from, the members of a plugin and a grammar made from nothing among them
#define CLI_PATTERN_MAX_LEN 1024u
#define CLI_DRAFT_NODES_MORE 512u
#define CLI_DRAFT_TEXT_MORE (1ULL << 14u)

// the number's pattern a grammar made from nothing reads
static const char s_number[] = "0[xX][\\da-fA-F_]+|0[bB][01_]+|(?:\\d[\\d_]*)?\\.?\\d[\\d_]*(?:[eE][+-]?\\d+)?";

typedef struct
{
    const CliJson *json_read;
    unsigned int spec;
    const char *name;
    char *out;
    unsigned long long capacity;
    CliJson *json;
    unsigned int list;
    VerbaLinea *line;
    const char *text;
    const char *const *texts;
    unsigned int extra;
    unsigned int pattern;
    const char *token;
    const char *next;
    unsigned int quotes;
    unsigned int plugin;
    unsigned int template_node;
    int quoted;
    const char *from;
    VerbaLinea *err_msg;
} PluginsDraftCtx;
// the answer `name` of the spec as text, trimmed, into `out`, sized for `capacity`: its length
EMBED_INLINE unsigned long long cli_answer(const PluginsDraftCtx *args)
{
    const CliJson *json = args->json_read;
    const char *name = args->name;
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->spec, .name = name),
                   .length_pointer = &length);
    const char *from = (text != NULL) ? text : "";
    const char *to = from + length;
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
    return mmgr_line_finish(&line);
}

// the words of a list of the answers, trimmed, each once, in the order first given, made an array node
EMBED_INLINE unsigned int cli_words(const PluginsDraftCtx *args)
{
    CliJson *json = args->json;
    const unsigned int words =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = args->list, .kind = CFG_JSON_ARRAY)
                                 ? json->node[args->list].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        unsigned long long length = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = item, .length_pointer = &length);
        const char *from = (text != NULL) ? text : "";
        const char *to = from + length;
        from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                           .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
        to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                               .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
        const unsigned long long size = (unsigned long long)(to - from);
        int seen = size == 0ULL;
        for (unsigned int word_node = json->node[words].first; !seen && (word_node != CLI_JSON_NONE);
             word_node = json->node[word_node].next)
        {
            unsigned long long word_length = 0ULL;
            const char *const word_text =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = word_node, .length_pointer = &word_length);
            seen = (word_length == size) &&
                   (memor.cmp(&(MemoriaCfg){.src = word_text, .other = from, .bytes = size}) == 0);
        }
        if (!seen)
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = words,
                       .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init,
                                           CliJsonTreeTextLengthKindCfg, .json = json, .text = from, .length = size));
        }
    }
    return words;
}

// `text` as a pattern that matches only `text`, put after the line
EMBED_INLINE void cli_on_escape(const PluginsDraftCtx *args)
{
    const char *text = args->text;
    EMBED_CALL(cli_text_size_set.append_mark, CliTextSizeSetCfg, .out = args->line, .text = text,
               .set = "\\^$.|?*+()[]{}/", .mark = '\\');
}

// a string node of the line's text, the line let go
EMBED_INLINE unsigned int cli_line_init(const PluginsDraftCtx *args)
{
    CliJson *json = args->json;
    const unsigned long long length = mmgr_line_ok(args->line) ? args->line->at : 0ULL;
    return EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = json,
                      .text = args->line->out, .length = length);
}

EMBED_INLINE unsigned int cli_text_init(const PluginsDraftCtx *args)
{
    CliJson *json = args->json;
    const char *text = args->text;
    return EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = json,
                      .text = text, .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the strings given, NULL after the last, made a JSON array, the node `extra` after them where it is not none
EMBED_INLINE unsigned int cli_array(const PluginsDraftCtx *args)
{
    CliJson *json = args->json;
    const unsigned int array =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    for (unsigned int at = 0u; args->texts[at] != NULL; at += 1u)
    {
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = array,
                   .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = args->texts[at]));
    }
    if (args->extra != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = array,
                   .value = args->extra);
    }
    return array;
}

// [`pattern`, `token`, `next`], `next` left out where NULL
EMBED_INLINE unsigned int cli_rule(const PluginsDraftCtx *args)
{
    CliJson *json = args->json;
    const unsigned int rule =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rule,
               .value = args->pattern);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rule,
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = args->token));
    if (args->next != NULL)
    {
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rule,
                   .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = args->next));
    }
    return rule;
}

// The grammar the answers request, made from nothing
EMBED_INLINE unsigned int cli_grammar_get(const PluginsDraftCtx *args)
{
    CliJson *json = args->json;
    static const char *const s_lists[3][3] = {
        {"keywords", "@keywords", "keyword"}, {"types", "@types", "type"}, {"constants", "@constants", "predefined"}};
    char pattern[CLI_PATTERN_MAX_LEN];
    char state[CLI_NAME_MAX_LEN];
    const unsigned int grammar =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    const unsigned int cases =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned int list =
            EMBED_CALL(cli_words, PluginsDraftCtx, .json = json,
                       .list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->spec, .name = s_lists[at][0]));
        if (json->node[list].count != 0u)
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = grammar,
                       .name = s_lists[at][0], .value = list);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = cases,
                       .name = s_lists[at][1],
                       .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = s_lists[at][2]));
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = cases,
               .name = "@default",
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "identifier"));
    const unsigned int root =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    const unsigned int include =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = include,
               .name = "include",
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "@whitespace"));
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = root, .value = include);
    const unsigned int picked =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = picked,
               .name = "cases", .value = cases);
    const unsigned int named =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = named,
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "[A-Za-z_]\\w*"));
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = named, .value = picked);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = root, .value = named);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = root,
               .value =
                   EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                              .pattern = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = s_number),
                              .token = "number", .next = NULL));
    const unsigned int tokenizer =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    unsigned int index = 0u;
    for (unsigned int quote = json->node[args->quotes].first; quote != CLI_JSON_NONE; quote = json->node[quote].next)
    {
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = quote, .length_pointer = NULL);
        VerbaLinea named_state = {state, sizeof(state), 0ULL};
        mmgr_line_put(&named_state, "string");
        mmgr_line_decimal(&named_state, index, 1u);
        mmgr_line_finish(&named_state);
        char next[CLI_NAME_MAX_LEN];
        VerbaLinea next_line = {next, sizeof(next), 0ULL};
        mmgr_line_ch(&next_line, '@');
        mmgr_line_put(&next_line, state);
        mmgr_line_finish(&next_line);
        VerbaLinea escaped = {pattern, sizeof(pattern), 0ULL};
        EMBED_CALL(cli_on_escape, PluginsDraftCtx, .line = &escaped, .text = text);
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = root,
                   .value =
                       EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                  .pattern = EMBED_CALL(cli_line_init, PluginsDraftCtx, .json = json, .line = &escaped),
                                  .token = "string", .next = next));
        const unsigned int rules =
            EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
        VerbaLinea inner = {pattern, sizeof(pattern), 0ULL};
        mmgr_line_put(&inner, "(?:(?!");
        EMBED_CALL(cli_on_escape, PluginsDraftCtx, .line = &inner, .text = text);
        mmgr_line_put(&inner, "|\\\\).)+");
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rules,
                   .value =
                       EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                  .pattern = EMBED_CALL(cli_line_init, PluginsDraftCtx, .json = json, .line = &inner),
                                  .token = "string", .next = NULL));
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rules,
                   .value =
                       EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                  .pattern = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "\\\\."),
                                  .token = "string.escape", .next = NULL));
        VerbaLinea closing = {pattern, sizeof(pattern), 0ULL};
        EMBED_CALL(cli_on_escape, PluginsDraftCtx, .line = &closing, .text = text);
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rules,
                   .value =
                       EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                  .pattern = EMBED_CALL(cli_line_init, PluginsDraftCtx, .json = json, .line = &closing),
                                  .token = "string", .next = "@pop"));
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = tokenizer,
                   .name = state, .value = rules);
        index += 1u;
    }
    EMBED_CALL(
        cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = root,
        .value = EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                            .pattern = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "[{}()\\[\\]]"),
                            .token = "@brackets", .next = NULL));
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = root,
               .value = EMBED_CALL(
                   cli_rule, PluginsDraftCtx, .json = json,
                   .pattern = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "[<>=!~?:&|+\\-*/^%]+"),
                   .token = "operator", .next = NULL));
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = root,
               .value = EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                   .pattern = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "[;,.]"),
                                   .token = "delimiter", .next = NULL));
    const unsigned int whitespace =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = whitespace,
               .value = EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                   .pattern = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "\\s+"),
                                   .token = "", .next = NULL));
    const unsigned int block = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->spec, .name = "blockComment");
    const char *const open =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                      .array = block, .index = 0u),
                   .length_pointer = NULL);
    const char *const close =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                      .array = block, .index = 1u),
                   .length_pointer = NULL);
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = block,
                   .kind = CFG_JSON_ARRAY) &&
        (json->node[block].count == 2u) && (open != NULL) && (close != NULL))
    {
        VerbaLinea opening = {pattern, sizeof(pattern), 0ULL};
        EMBED_CALL(cli_on_escape, PluginsDraftCtx, .line = &opening, .text = open);
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = whitespace,
                   .value =
                       EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                  .pattern = EMBED_CALL(cli_line_init, PluginsDraftCtx, .json = json, .line = &opening),
                                  .token = "comment", .next = "@comment"));
        const unsigned int rules =
            EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
        VerbaLinea inner = {pattern, sizeof(pattern), 0ULL};
        mmgr_line_put(&inner, "(?:(?!");
        EMBED_CALL(cli_on_escape, PluginsDraftCtx, .line = &inner, .text = close);
        mmgr_line_put(&inner, ").)+");
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rules,
                   .value =
                       EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                  .pattern = EMBED_CALL(cli_line_init, PluginsDraftCtx, .json = json, .line = &inner),
                                  .token = "comment", .next = NULL));
        VerbaLinea closing = {pattern, sizeof(pattern), 0ULL};
        EMBED_CALL(cli_on_escape, PluginsDraftCtx, .line = &closing, .text = close);
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = rules,
                   .value =
                       EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                  .pattern = EMBED_CALL(cli_line_init, PluginsDraftCtx, .json = json, .line = &closing),
                                  .token = "comment", .next = "@pop"));
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = tokenizer,
                   .name = "comment", .value = rules);
    }
    char line_comment[CLI_NAME_MAX_LEN];
    if (EMBED_CALL(cli_answer, PluginsDraftCtx, .json_read = json, .spec = args->spec, .name = "lineComment",
                   .out = line_comment, .capacity = sizeof(line_comment)) != 0ULL)
    {
        VerbaLinea commented = {pattern, sizeof(pattern), 0ULL};
        EMBED_CALL(cli_on_escape, PluginsDraftCtx, .line = &commented, .text = line_comment);
        mmgr_line_put(&commented, ".*$");
        EMBED_CALL(
            cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = whitespace,
            .value = EMBED_CALL(cli_rule, PluginsDraftCtx, .json = json,
                                .pattern = EMBED_CALL(cli_line_init, PluginsDraftCtx, .json = json, .line = &commented),
                                .token = "comment", .next = NULL));
    }
    const unsigned int ordered =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = ordered,
               .name = "root", .value = root);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = ordered,
               .name = "whitespace", .value = whitespace);
    for (unsigned int each = json->node[tokenizer].first; each != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[each].next;
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = ordered,
                   .name = json->text + json->node[each].key, .value = each);
        each = next;
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = grammar,
               .name = "tokenizer", .value = ordered);
    return grammar;
}

// the comments the answers name: `line` where one is given, `block` where two are
EMBED_INLINE unsigned int cli_comments_get(const PluginsDraftCtx *args)
{
    CliJson *json = args->json;
    const unsigned int comments =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    char line[CLI_NAME_MAX_LEN];
    if (EMBED_CALL(cli_answer, PluginsDraftCtx, .json_read = json, .spec = args->spec, .name = "lineComment",
                   .out = line, .capacity = sizeof(line)) != 0ULL)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = comments,
                   .name = "line", .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = line));
    }
    const unsigned int block = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->spec, .name = "blockComment");
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = block,
                   .kind = CFG_JSON_ARRAY) &&
        (json->node[block].count == 2u))
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = comments,
                   .name = "block",
                   .value = EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json, .node = block));
    }
    return comments;
}

// The plugin of `id` to start from, the last found of that id, its text read into `json`: its node, or CLI_JSON_NONE
typedef struct
{
    const char *id;
    CliJson *json;
    unsigned int found;
    int seen;
} CliTemplate;

static void cli_template_each(const CliEachPluginCfg *args)
{
    CliTemplate *const template_of = (CliTemplate *)args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = template_of->id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    template_of->seen = 1;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    // the plugin's tokens in a cell of their count, given back once they are read into the tree
    const unsigned int count = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                          .text = args->plugin->text, .size = args->plugin->size) +
                               1u;
    CfgJsonToken *const tokens =
        g_cli_cells->temporary_buf_alloc((unsigned long long)count * sizeof(*tokens), &g_cli_plugins_cells);
    template_of->found =
        (tokens != NULL) ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                                      .json = template_of->json, .text = args->plugin->text, .size = args->plugin->size,
                                      .tokens = tokens, .tokens_max = count, .err_msg = &err_msg)
                         : CLI_JSON_NONE;
    g_cli_cells->persistent_buf_release(tokens, &g_cli_plugins_cells);
}

// The work state the plugin of `id` takes, the last found of that id: its tokens and its bytes
typedef struct
{
    const char *id;
    unsigned int nodes;
    unsigned long long text;
} CliTemplateCapacity;

static void cli_template_capacity_each(const CliEachPluginCfg *args)
{
    CliTemplateCapacity *const work_state = (CliTemplateCapacity *)args->context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = work_state->id, .cap = CLI_CELLS_POOL}))
    {
        work_state->nodes = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                       .text = args->plugin->text, .size = args->plugin->size) +
                            1u;
        work_state->text = args->plugin->size;
    }
}

unsigned int cli_plugins_draft_tree(const CliPluginsErrMsgJsonSpecCfg *args)
{
    unsigned int nodes = 0u;
    unsigned long long text = 0ULL;
    EMBED_CALL(cli_json_tree_node_nodes.json_extent, CliJsonTreeNodeNodesCfg, .json_read = args->json,
               .node = args->spec, .nodes_pointer = &nodes, .text_u64s = &text);
    char from[CLI_NAME_MAX_LEN];
    CliTemplateCapacity work_state = {from, 0u, 0ULL};
    if (EMBED_CALL(cli_answer, PluginsDraftCtx, .json_read = args->json, .spec = args->spec, .name = "from",
                   .out = from, .capacity = sizeof(from)) != 0ULL)
    {
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_template_capacity_each,
                   .context = &work_state);
    }
    // the answers twice over, as the draft copies their words, the plugin started from, and the members, patterns and
    // lists the draft makes besides
    if (!EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = args->draft,
                    .nodes_max = (2u * nodes) + work_state.nodes + CLI_DRAFT_NODES_MORE,
                    .text_max = (2ULL * text) + work_state.text + CLI_DRAFT_TEXT_MORE, .what = "a plugin's draft",
                    .owner = &g_cli_plugins_cells))
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet a plugin's draft");
        return CLI_JSON_NONE;
    }
    return EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .to = args->draft, .from = args->json,
                      .node = args->spec);
}

// the extensions the answers in `spec` give, each trimmed, its leading dots taken off and lower case, as an array in
// `json`: its node
EMBED_INLINE unsigned int cli_extensions_read(const PluginsDraftCtx *args)
{
    CliJson *const json = args->json;
    const unsigned int extensions =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    const unsigned int extensions_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = json, .object = args->spec, .name = "extensions");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = extensions_node, .kind = CFG_JSON_ARRAY)
                                 ? json->node[extensions_node].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = item, .length_pointer = &size);
        const char *at = (text != NULL) ? text : "";
        const char *end = at + size;
        at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = (unsigned long long)(end - at), .set = CLI_TEXT_WHITE);
        end = at + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = at,
                              .at = (unsigned long long)(end - at), .set = CLI_TEXT_WHITE);
        at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = (unsigned long long)(end - at), .set = ".");
        char lower[CLI_NAME_MAX_LEN];
        VerbaLinea line = {lower, sizeof(lower), 0ULL};
        for (const char *letter = at; letter < end; letter += 1)
        {
            mmgr_line_ch(&line, ((letter[0] >= 'A') && (letter[0] <= 'Z')) ? (char)(letter[0] + 32) : letter[0]);
        }
        if (mmgr_line_finish(&line) != 0ULL)
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = extensions,
                       .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = lower));
        }
    }
    return extensions;
}

// a new language's members set on `plugin` from the answers in `spec`: its comments, its pairs, its `quotes`, the
// indent after an open bracket, its grammar, and no hovers or snippets
EMBED_INLINE void cli_draft_new(const PluginsDraftCtx *args)
{
    CliJson *const json = args->json;
    static const char *const s_pairs[4] = {"()", "[]", "{}", NULL};
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
               .name = "comments",
               .value = EMBED_CALL(cli_comments_get, PluginsDraftCtx, .json = json, .spec = args->spec));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
               .name = "pairs",
               .value = EMBED_CALL(cli_array, PluginsDraftCtx, .json = json, .texts = s_pairs, .extra = CLI_JSON_NONE));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
               .name = "quotes", .value = args->quotes);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
               .name = "indentAfter",
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = json, .text = "[{[(]\\s*$"));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
               .name = "grammar",
               .value = EMBED_CALL(cli_grammar_get, PluginsDraftCtx, .json = json, .spec = args->spec,
                                   .quotes = args->quotes));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
               .name = "hovers",
               .value =
                   EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
               .name = "snippets",
               .value =
                   EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY));
}

// each member of the plugin started from, at `template_node`, set on `plugin` where `plugin` has none of its name
EMBED_INLINE void cli_members_merged(const PluginsDraftCtx *args)
{
    CliJson *const json = args->json;
    unsigned int member = json->node[args->template_node].first;
    while (member != CLI_JSON_NONE)
    {
        const unsigned int next = json->node[member].next;
        const char *const key = json->text + json->node[member].key;
        if (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                       .object = args->plugin, .name = key) == CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json,
                       .object = args->plugin, .name = key, .value = member);
        }
        member = next;
    }
}

// the keywords, types and constants the answers in `spec` give set on `plugin`'s grammar where it has one, each list
// that names any; the keywords as its words where the grammar has words
EMBED_INLINE void cli_grammar_lists(const PluginsDraftCtx *args)
{
    CliJson *const json = args->json;
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->plugin, .name = "grammar");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = grammar,
                    .kind = CFG_JSON_OBJECT))
    {
        return;
    }
    static const char *const s_lists[3] = {"keywords", "types", "constants"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned int list =
            EMBED_CALL(cli_words, PluginsDraftCtx, .json = json,
                       .list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->spec, .name = s_lists[at]));
        if (json->node[list].count == 0u)
        {
            continue;
        }
        const int words =
            (at == 0u) && (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = grammar, .name = "words") != CLI_JSON_NONE);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = grammar,
                   .name = words ? "words" : s_lists[at], .value = list);
    }
}

// `plugin` filled from the plugin `from` names: its members where `plugin` has none, the comments where the answers in
// `spec` give any, the `quotes` where they are `quoted`, and the grammar's lists: 1, or 0 with why given where no
// such plugin reads as an object
EMBED_INLINE int cli_draft_from_template(const PluginsDraftCtx *args)
{
    CliJson *const json = args->json;
    CliTemplate template_of = {args->from, json, CLI_JSON_NONE, 0};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_template_each, .context = &template_of);
    if (!template_of.seen || (template_of.found == CLI_JSON_NONE))
    {
        mmgr_line_put(args->err_msg, template_of.seen ? "plugin " : "no plugin ");
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg, template_of.seen ? " does not read" : " to start from");
        return 0;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                    .node = template_of.found, .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_put(args->err_msg, "plugin ");
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg, " is not an object");
        return 0;
    }
    EMBED_CALL(cli_members_merged, PluginsDraftCtx, .json = json, .plugin = args->plugin,
               .template_node = template_of.found);
    char line_comment[CLI_NAME_MAX_LEN];
    const unsigned int block = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->spec, .name = "blockComment");
    if ((EMBED_CALL(cli_answer, PluginsDraftCtx, .json_read = json, .spec = args->spec, .name = "lineComment",
                    .out = line_comment, .capacity = sizeof(line_comment)) != 0ULL) ||
        (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = block,
                    .kind = CFG_JSON_ARRAY) &&
         (json->node[block].count == 2u)))
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
                   .name = "comments",
                   .value = EMBED_CALL(cli_comments_get, PluginsDraftCtx, .json = json, .spec = args->spec));
    }
    if (args->quoted)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->plugin,
                   .name = "quotes", .value = args->quotes);
    }
    EMBED_CALL(cli_grammar_lists, PluginsDraftCtx, .json = json, .spec = args->spec, .plugin = args->plugin);
    return 1;
}

unsigned int cli_plugins_draft(const CliPluginsErrMsgJsonSpecKindCfg *args)
{
    char name[CLI_NAME_MAX_LEN];
    char supplied[CLI_NAME_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
    char from[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_answer, PluginsDraftCtx, .json_read = args->json_out, .spec = args->spec, .name = "name",
               .out = name, .capacity = sizeof(name));
    EMBED_CALL(cli_answer, PluginsDraftCtx, .json_read = args->json_out, .spec = args->spec, .name = "id",
               .out = supplied, .capacity = sizeof(supplied));
    const char *const raw_name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json_out,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = args->json_out, .object = args->spec, .name = "name"),
                   .length_pointer = NULL);
    const char *const raw_id =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json_out,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = args->json_out, .object = args->spec, .name = "id"),
                   .length_pointer = NULL);
    const char *const named_text = (raw_name != NULL) ? raw_name : "";
    const char *const id_text = (raw_id != NULL) ? raw_id : "";
    EMBED_CALL(cli_plugins_id_name.slug, CliPluginsIdNameCfg, .name = (supplied[0] == '\0') ? named_text : id_text,
               .id_out = id);
    if (id[0] == '\0')
    {
        mmgr_line_put(args->err_msg, "a plugin needs a name made of letters or digits");
        return CLI_JSON_NONE;
    }
    const unsigned int extensions =
        EMBED_CALL(cli_extensions_read, PluginsDraftCtx, .json = args->json_out, .spec = args->spec);
    if (args->json_out->node[extensions].count == 0u)
    {
        mmgr_line_put(args->err_msg, "a plugin needs an extension to open, such as --ext foo");
        return CLI_JSON_NONE;
    }
    const unsigned int quotes_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = args->json_out, .object = args->spec, .name = "quotes");
    const int quoted = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json_out,
                                  .node = quotes_node, .kind = CFG_JSON_ARRAY) &&
                       (args->json_out->node[quotes_node].count != 0u);
    static const char *const s_default_quotes[3] = {"\"", "'", NULL};
    const unsigned int quotes =
        quoted ? EMBED_CALL(cli_words, PluginsDraftCtx, .json = args->json_out, .list = quotes_node)
               : EMBED_CALL(cli_array, PluginsDraftCtx, .json = args->json_out, .texts = s_default_quotes,
                            .extra = CLI_JSON_NONE);
    const unsigned int plugin =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json_out, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json_out, .object = plugin,
               .name = "id", .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = args->json_out, .text = id));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json_out, .object = plugin,
               .name = "name",
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = args->json_out,
                                   .text = (name[0] != '\0') ? name : id));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json_out, .object = plugin,
               .name = "version",
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = args->json_out, .text = "1.0.0"));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json_out, .object = plugin,
               .name = "kind",
               .value = EMBED_CALL(cli_text_init, PluginsDraftCtx, .json = args->json_out, .text = "language"));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json_out, .object = plugin,
               .name = "extensions", .value = extensions);
    if (EMBED_CALL(cli_answer, PluginsDraftCtx, .json_read = args->json_out, .spec = args->spec, .name = "from",
                   .out = from, .capacity = sizeof(from)) == 0ULL)
    {
        EMBED_CALL(cli_draft_new, PluginsDraftCtx, .json = args->json_out, .spec = args->spec, .plugin = plugin,
                   .quotes = quotes);
    }
    else if (!EMBED_CALL(cli_draft_from_template, PluginsDraftCtx, .json = args->json_out, .spec = args->spec,
                         .plugin = plugin, .quotes = quotes, .quoted = quoted, .from = from, .err_msg = args->err_msg))
    {
        return CLI_JSON_NONE;
    }
    return args->json_out->full ? CLI_JSON_NONE : plugin;
}
