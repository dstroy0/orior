// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_draft.c: the plugin a few answers ask for: made from nothing, or from a plugin to start from with what the
// answers name changed
#include "plugins.h"

// the most bytes of a pattern made; and the nodes and bytes of texts a draft makes besides the copies of its answers
// and the plugin it starts from, the members of a plugin and a grammar made from nothing among them
#define CLI_PATTERN_LONGEST 1024u
#define CLI_DRAFT_NODES_MORE 512u
#define CLI_DRAFT_TEXT_MORE (1ull << 14u)

// the number's pattern a grammar made from nothing reads
static const char s_number[] = "0[xX][\\da-fA-F_]+|0[bB][01_]+|(?:\\d[\\d_]*)?\\.?\\d[\\d_]*(?:[eE][+-]?\\d+)?";

// the answer `name` of the spec as text, trimmed, into `out`, which holds `room`: its length
static unsigned long long cli_answer(const CliJson *json, unsigned int spec, const char *name, char *out,
                                     unsigned long long room)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, cli_json_member(json, spec, name), &length);
    const char *from = (text != NULL) ? text : "";
    const char *to = from + ((text != NULL) ? length : 0ull);
    while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
    {
        to -= 1;
    }
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, from, (unsigned long long)(to - from));
    return cli_line_closed(&line);
}

// the words of a list of the answers, trimmed, each once, in the order first given, made an array node
static unsigned int cli_words(CliJson *json, unsigned int list)
{
    const unsigned int words = cli_json_made(json, CFG_JSON_ARRAY);
    for (unsigned int item = cli_json_is(json, list, CFG_JSON_ARRAY) ? json->node[list].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(json, item, &length);
        const char *from = (text != NULL) ? text : "";
        const char *to = from + ((text != NULL) ? length : 0ull);
        while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
        {
            from += 1;
        }
        while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
        {
            to -= 1;
        }
        const unsigned long long size = (unsigned long long)(to - from);
        int seen = size == 0ull;
        for (unsigned int kept = json->node[words].first; !seen && (kept != CLI_JSON_NONE);
             kept = json->node[kept].next)
        {
            unsigned long long held_length = 0ull;
            const char *const held = cli_json_string(json, kept, &held_length);
            seen = (held_length == size) && (memor.cmp(&(MemoriaCfg){.src = held, .other = from, .bytes = size}) == 0);
        }
        if (!seen)
        {
            cli_json_append(json, words, cli_json_string_made(json, from, size));
        }
    }
    return words;
}

// `text` as a pattern that matches only `text`, put after the line
static void cli_escaped(CliLine *line, const char *text)
{
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        if (cellul.chr(&(CatenaFinitaCfg){.src = "\\^$.|?*+()[]{}/", .cap = CLI_CELLS_POOL, .byte = at[0]}) != NULL)
        {
            cli_line_character(line, '\\');
        }
        cli_line_character(line, at[0]);
    }
}

// a string node of the line's text, the line let go
static unsigned int cli_line_made(CliJson *json, CliLine *line)
{
    const unsigned long long length = cli_line_fits(line) ? line->at : 0ull;
    return cli_json_string_made(json, line->out, length);
}

static unsigned int cli_text_made(CliJson *json, const char *text)
{
    return cli_json_string_made(json, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// an array of the strings given, NULL after the last, the node `extra` after them where it is not none
static unsigned int cli_array(CliJson *json, const char *const *texts, unsigned int extra)
{
    const unsigned int array = cli_json_made(json, CFG_JSON_ARRAY);
    for (unsigned int at = 0u; texts[at] != NULL; at += 1u)
    {
        cli_json_append(json, array, cli_text_made(json, texts[at]));
    }
    if (extra != CLI_JSON_NONE)
    {
        cli_json_append(json, array, extra);
    }
    return array;
}

// [`pattern`, `token`, `next`], `next` left out where NULL
static unsigned int cli_rule(CliJson *json, unsigned int pattern, const char *token, const char *next)
{
    const unsigned int rule = cli_json_made(json, CFG_JSON_ARRAY);
    cli_json_append(json, rule, pattern);
    cli_json_append(json, rule, cli_text_made(json, token));
    if (next != NULL)
    {
        cli_json_append(json, rule, cli_text_made(json, next));
    }
    return rule;
}

// The grammar the answers ask for, made from nothing
static unsigned int cli_grammar_of(CliJson *json, unsigned int spec, unsigned int quotes)
{
    static const char *const s_lists[3][3] = {
        {"keywords", "@keywords", "keyword"}, {"types", "@types", "type"}, {"constants", "@constants", "predefined"}};
    char pattern[CLI_PATTERN_LONGEST];
    char state[CLI_NAME_LONGEST];
    const unsigned int grammar = cli_json_made(json, CFG_JSON_OBJECT);
    const unsigned int cases = cli_json_made(json, CFG_JSON_OBJECT);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned int list = cli_words(json, cli_json_member(json, spec, s_lists[at][0]));
        if (json->node[list].count != 0u)
        {
            cli_json_set(json, grammar, s_lists[at][0], list);
            cli_json_set(json, cases, s_lists[at][1], cli_text_made(json, s_lists[at][2]));
        }
    }
    cli_json_set(json, cases, "@default", cli_text_made(json, "identifier"));
    const unsigned int root = cli_json_made(json, CFG_JSON_ARRAY);
    const unsigned int include = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, include, "include", cli_text_made(json, "@whitespace"));
    cli_json_append(json, root, include);
    const unsigned int picked = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, picked, "cases", cases);
    const unsigned int named = cli_json_made(json, CFG_JSON_ARRAY);
    cli_json_append(json, named, cli_text_made(json, "[A-Za-z_]\\w*"));
    cli_json_append(json, named, picked);
    cli_json_append(json, root, named);
    cli_json_append(json, root, cli_rule(json, cli_text_made(json, s_number), "number", NULL));
    const unsigned int tokenizer = cli_json_made(json, CFG_JSON_OBJECT);
    unsigned int index = 0u;
    for (unsigned int quote = json->node[quotes].first; quote != CLI_JSON_NONE; quote = json->node[quote].next)
    {
        const char *const text = cli_json_string(json, quote, NULL);
        CliLine named_state = {state, sizeof(state), 0ull};
        cli_line_text(&named_state, "string");
        cli_line_decimal(&named_state, index, 1u);
        cli_line_closed(&named_state);
        char next[CLI_NAME_LONGEST];
        CliLine next_line = {next, sizeof(next), 0ull};
        cli_line_character(&next_line, '@');
        cli_line_text(&next_line, state);
        cli_line_closed(&next_line);
        CliLine escaped = {pattern, sizeof(pattern), 0ull};
        cli_escaped(&escaped, text);
        cli_json_append(json, root, cli_rule(json, cli_line_made(json, &escaped), "string", next));
        const unsigned int rules = cli_json_made(json, CFG_JSON_ARRAY);
        CliLine inner = {pattern, sizeof(pattern), 0ull};
        cli_line_text(&inner, "(?:(?!");
        cli_escaped(&inner, text);
        cli_line_text(&inner, "|\\\\).)+");
        cli_json_append(json, rules, cli_rule(json, cli_line_made(json, &inner), "string", NULL));
        cli_json_append(json, rules, cli_rule(json, cli_text_made(json, "\\\\."), "string.escape", NULL));
        CliLine closing = {pattern, sizeof(pattern), 0ull};
        cli_escaped(&closing, text);
        cli_json_append(json, rules, cli_rule(json, cli_line_made(json, &closing), "string", "@pop"));
        cli_json_set(json, tokenizer, state, rules);
        index += 1u;
    }
    cli_json_append(json, root, cli_rule(json, cli_text_made(json, "[{}()\\[\\]]"), "@brackets", NULL));
    cli_json_append(json, root, cli_rule(json, cli_text_made(json, "[<>=!~?:&|+\\-*/^%]+"), "operator", NULL));
    cli_json_append(json, root, cli_rule(json, cli_text_made(json, "[;,.]"), "delimiter", NULL));
    const unsigned int whitespace = cli_json_made(json, CFG_JSON_ARRAY);
    cli_json_append(json, whitespace, cli_rule(json, cli_text_made(json, "\\s+"), "", NULL));
    const unsigned int block = cli_json_member(json, spec, "blockComment");
    const char *const open = cli_json_string(json, cli_json_item(json, block, 0u), NULL);
    const char *const close = cli_json_string(json, cli_json_item(json, block, 1u), NULL);
    if (cli_json_is(json, block, CFG_JSON_ARRAY) && (json->node[block].count == 2u) && (open != NULL) &&
        (close != NULL))
    {
        CliLine opening = {pattern, sizeof(pattern), 0ull};
        cli_escaped(&opening, open);
        cli_json_append(json, whitespace, cli_rule(json, cli_line_made(json, &opening), "comment", "@comment"));
        const unsigned int rules = cli_json_made(json, CFG_JSON_ARRAY);
        CliLine inner = {pattern, sizeof(pattern), 0ull};
        cli_line_text(&inner, "(?:(?!");
        cli_escaped(&inner, close);
        cli_line_text(&inner, ").)+");
        cli_json_append(json, rules, cli_rule(json, cli_line_made(json, &inner), "comment", NULL));
        CliLine closing = {pattern, sizeof(pattern), 0ull};
        cli_escaped(&closing, close);
        cli_json_append(json, rules, cli_rule(json, cli_line_made(json, &closing), "comment", "@pop"));
        cli_json_set(json, tokenizer, "comment", rules);
    }
    char line_comment[CLI_NAME_LONGEST];
    if (cli_answer(json, spec, "lineComment", line_comment, sizeof(line_comment)) != 0ull)
    {
        CliLine commented = {pattern, sizeof(pattern), 0ull};
        cli_escaped(&commented, line_comment);
        cli_line_text(&commented, ".*$");
        cli_json_append(json, whitespace, cli_rule(json, cli_line_made(json, &commented), "comment", NULL));
    }
    const unsigned int ordered = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, ordered, "root", root);
    cli_json_set(json, ordered, "whitespace", whitespace);
    for (unsigned int each = json->node[tokenizer].first; each != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[each].next;
        cli_json_set(json, ordered, json->text + json->node[each].key, each);
        each = next;
    }
    cli_json_set(json, grammar, "tokenizer", ordered);
    return grammar;
}

// the comments the answers name: `line` where one is given, `block` where two are
static unsigned int cli_comments_of(CliJson *json, unsigned int spec)
{
    const unsigned int comments = cli_json_made(json, CFG_JSON_OBJECT);
    char line[CLI_NAME_LONGEST];
    if (cli_answer(json, spec, "lineComment", line, sizeof(line)) != 0ull)
    {
        cli_json_set(json, comments, "line", cli_text_made(json, line));
    }
    const unsigned int block = cli_json_member(json, spec, "blockComment");
    if (cli_json_is(json, block, CFG_JSON_ARRAY) && (json->node[block].count == 2u))
    {
        cli_json_set(json, comments, "block", cli_json_copy(json, block));
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

static void cli_template_each(void *context, const CliPlugin *plugin)
{
    CliTemplate *const template_of = (CliTemplate *)context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = template_of->id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    template_of->seen = 1;
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    // the plugin's tokens in a cell of their count, given back once they are read into the tree
    const unsigned int count = cli_json_tokens_counted(plugin->text, plugin->size) + 1u;
    CfgJsonToken *const tokens =
        cli_cell_taken_by(&g_cli_plugins_cells, (unsigned long long)count * sizeof(*tokens), "a plugin's tokens");
    template_of->found = (tokens != NULL)
                             ? cli_json_read(template_of->json, plugin->text, plugin->size, tokens, count, &said)
                             : CLI_JSON_NONE;
    cli_cell_given_by(&g_cli_plugins_cells, tokens);
}

// The room the plugin of `id` takes, the last found of that id: its tokens and its bytes
typedef struct
{
    const char *id;
    unsigned int nodes;
    unsigned long long text;
} CliTemplateRoom;

static void cli_template_room_each(void *context, const CliPlugin *plugin)
{
    CliTemplateRoom *const room = (CliTemplateRoom *)context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = room->id, .cap = CLI_CELLS_POOL}))
    {
        room->nodes = cli_json_tokens_counted(plugin->text, plugin->size) + 1u;
        room->text = plugin->size;
    }
}

unsigned int cli_plugins_draft_tree(CliJson *draft, const CliJson *json, unsigned int spec, CliLine *said)
{
    unsigned int nodes = 0u;
    unsigned long long text = 0ull;
    cli_json_extent(json, spec, &nodes, &text);
    char from[CLI_NAME_LONGEST];
    CliTemplateRoom room = {from, 0u, 0ull};
    if (cli_answer(json, spec, "from", from, sizeof(from)) != 0ull)
    {
        cli_plugins_each(cli_template_room_each, &room);
    }
    // the answers twice over, as the draft copies their words, the plugin started from, and the members, patterns and
    // lists the draft makes besides
    if (!cli_json_held(draft, (2u * nodes) + room.nodes + CLI_DRAFT_NODES_MORE,
                       (2ull * text) + room.text + CLI_DRAFT_TEXT_MORE, "a plugin's draft"))
    {
        cli_line_text(said, "the cells cannot meet a plugin's draft");
        return CLI_JSON_NONE;
    }
    return cli_json_copy_from(draft, json, spec);
}

unsigned int cli_plugins_draft(CliJson *json, unsigned int spec, CliLine *said)
{
    char name[CLI_NAME_LONGEST];
    char given[CLI_NAME_LONGEST];
    char id[CLI_NAME_LONGEST];
    char from[CLI_NAME_LONGEST];
    cli_answer(json, spec, "name", name, sizeof(name));
    cli_answer(json, spec, "id", given, sizeof(given));
    const char *const raw_name = cli_json_string(json, cli_json_member(json, spec, "name"), NULL);
    const char *const raw_id = cli_json_string(json, cli_json_member(json, spec, "id"), NULL);
    cli_plugins_slug((given[0] == '\0') ? ((raw_name != NULL) ? raw_name : "") : ((raw_id != NULL) ? raw_id : ""), id);
    if (id[0] == '\0')
    {
        cli_line_text(said, "a plugin needs a name made of letters or digits");
        return CLI_JSON_NONE;
    }
    const unsigned int extensions = cli_json_made(json, CFG_JSON_ARRAY);
    const unsigned int asked = cli_json_member(json, spec, "extensions");
    for (unsigned int item = cli_json_is(json, asked, CFG_JSON_ARRAY) ? json->node[asked].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, item, &size);
        const char *at = (text != NULL) ? text : "";
        const char *end = at + ((text != NULL) ? size : 0ull);
        while ((at < end) && ((at[0] == ' ') || ((at[0] >= '\t') && (at[0] <= '\r'))))
        {
            at += 1;
        }
        while ((end > at) && ((end[-1] == ' ') || ((end[-1] >= '\t') && (end[-1] <= '\r'))))
        {
            end -= 1;
        }
        while ((at < end) && (at[0] == '.'))
        {
            at += 1;
        }
        char lower[CLI_NAME_LONGEST];
        CliLine line = {lower, sizeof(lower), 0ull};
        for (const char *letter = at; letter < end; letter += 1)
        {
            cli_line_character(&line, ((letter[0] >= 'A') && (letter[0] <= 'Z')) ? (char)(letter[0] + 32) : letter[0]);
        }
        if (cli_line_closed(&line) != 0ull)
        {
            cli_json_append(json, extensions, cli_text_made(json, lower));
        }
    }
    if (json->node[extensions].count == 0u)
    {
        cli_line_text(said, "a plugin needs an extension to open, such as --ext foo");
        return CLI_JSON_NONE;
    }
    const unsigned int asked_quotes = cli_json_member(json, spec, "quotes");
    const int quoted = cli_json_is(json, asked_quotes, CFG_JSON_ARRAY) && (json->node[asked_quotes].count != 0u);
    static const char *const s_default_quotes[3] = {"\"", "'", NULL};
    const unsigned int quotes =
        quoted ? cli_words(json, asked_quotes) : cli_array(json, s_default_quotes, CLI_JSON_NONE);
    const unsigned int plugin = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, plugin, "id", cli_text_made(json, id));
    cli_json_set(json, plugin, "name", cli_text_made(json, (name[0] != '\0') ? name : id));
    cli_json_set(json, plugin, "version", cli_text_made(json, "1.0.0"));
    cli_json_set(json, plugin, "kind", cli_text_made(json, "language"));
    cli_json_set(json, plugin, "extensions", extensions);
    if (cli_answer(json, spec, "from", from, sizeof(from)) == 0ull)
    {
        static const char *const s_pairs[4] = {"()", "[]", "{}", NULL};
        cli_json_set(json, plugin, "comments", cli_comments_of(json, spec));
        cli_json_set(json, plugin, "pairs", cli_array(json, s_pairs, CLI_JSON_NONE));
        cli_json_set(json, plugin, "quotes", quotes);
        cli_json_set(json, plugin, "indentAfter", cli_text_made(json, "[{[(]\\s*$"));
        cli_json_set(json, plugin, "grammar", cli_grammar_of(json, spec, quotes));
        cli_json_set(json, plugin, "hovers", cli_json_made(json, CFG_JSON_OBJECT));
        cli_json_set(json, plugin, "snippets", cli_json_made(json, CFG_JSON_ARRAY));
        return json->full ? CLI_JSON_NONE : plugin;
    }
    CliTemplate template_of = {from, json, CLI_JSON_NONE, 0};
    cli_plugins_each(cli_template_each, &template_of);
    if (!template_of.seen || (template_of.found == CLI_JSON_NONE))
    {
        cli_line_text(said, template_of.seen ? "plugin " : "no plugin ");
        cli_line_text(said, from);
        cli_line_text(said, template_of.seen ? " does not read" : " to start from");
        return CLI_JSON_NONE;
    }
    if (!cli_json_is(json, template_of.found, CFG_JSON_OBJECT))
    {
        cli_line_text(said, "plugin ");
        cli_line_text(said, from);
        cli_line_text(said, " is not an object");
        return CLI_JSON_NONE;
    }
    for (unsigned int member = json->node[template_of.found].first; member != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[member].next;
        const char *const key = json->text + json->node[member].key;
        if (cli_json_member(json, plugin, key) == CLI_JSON_NONE)
        {
            cli_json_set(json, plugin, key, member);
        }
        member = next;
    }
    char line_comment[CLI_NAME_LONGEST];
    const unsigned int block = cli_json_member(json, spec, "blockComment");
    if ((cli_answer(json, spec, "lineComment", line_comment, sizeof(line_comment)) != 0ull) ||
        (cli_json_is(json, block, CFG_JSON_ARRAY) && (json->node[block].count == 2u)))
    {
        cli_json_set(json, plugin, "comments", cli_comments_of(json, spec));
    }
    if (quoted)
    {
        cli_json_set(json, plugin, "quotes", quotes);
    }
    const unsigned int grammar = cli_json_member(json, plugin, "grammar");
    static const char *const s_lists[3] = {"keywords", "types", "constants"};
    for (unsigned int at = 0u; cli_json_is(json, grammar, CFG_JSON_OBJECT) && (at < 3u); at += 1u)
    {
        const unsigned int list = cli_words(json, cli_json_member(json, spec, s_lists[at]));
        if (json->node[list].count == 0u)
        {
            continue;
        }
        const int words = (at == 0u) && (cli_json_member(json, grammar, "words") != CLI_JSON_NONE);
        cli_json_set(json, grammar, words ? "words" : s_lists[at], list);
    }
    return json->full ? CLI_JSON_NONE : plugin;
}
