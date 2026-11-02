// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_grammar.c: a language plugin's grammar stored for the colorer under `plugin:<id>`, read from the plugin.json
// of the plugin of that id that stands, the reader's in place of one that comes with orior, made whole from the tree's
// tables, and stored again only where the whole grammar changed. A language `<id>:<name>` is the plugin `<id>`'s for
// the folder `<name>`
#include "../command_line/cells.h"
#include "../editor/highlight.h"
#include "../text/text.h"
#include "plugins.h"

// the plugins whose grammars' stamps the list's room is taken with, and the bytes a whole grammar is written in
#define CLI_PLUGIN_GRAMMARS_MOST 1024u
#define CLI_PLUGIN_GRAMMAR_ROOM (4ull << 20u)

// A plugin whose grammar is stored: its id, and the stamp of the plugin.json it was read from
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    unsigned long long stamp;
} CliPluginGrammar;

// the plugins whose grammars are stored, in a cell of the plugins' at their most, and how many
static CliPluginGrammar *s_grammars;
static unsigned int s_grammar_room;
static unsigned int s_grammar_count;

// the list of the plugins whose grammars are stored, taken once at CLI_PLUGIN_GRAMMARS_MOST: 1, or 0 where it stands
// taken or the cells cannot meet it; under the lock
static int cli_grammars_init(void)
{
    CliPluginGrammar *const cell =
        (s_grammar_room == 0u) ? g_cli_cells->persistent_buf_alloc(
                                     (unsigned long long)CLI_PLUGIN_GRAMMARS_MOST * sizeof(*cell), &g_cli_plugins_cells)
                               : NULL;
    if (cell == NULL)
    {
        return 0;
    }
    s_grammars = cell;
    s_grammar_room = CLI_PLUGIN_GRAMMARS_MOST;
    return 1;
}

// The plugin sought: its id, and the folder its language stands for, empty for none; the tree its tables are read
// from; whether one of that id was found and its grammar made whole, the last, the one that stands, written into
// `whole`; and why where it was not
typedef struct
{
    const char *id;
    const char *name;
    const char *tree;
    int found;
    int made;
    VerbaLinea *whole;
    VerbaLinea *err_msg;
} CliGrammarSought;

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *key;
    const VerbaLinea *whole;
    VerbaLinea *err_msg;
} PluginsGrammarCtx;
// a text's stamp, FNV-1a over its bytes
EMBED_INLINE unsigned long long cli_grammar_stamp(const PluginsGrammarCtx *args)
{
    unsigned long long stamp = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        stamp = (stamp ^ (unsigned char)args->text[at]) * 0x100000001b3ull;
    }
    return stamp;
}

// a plugin of the id sought, its grammar made whole in the place of one made before: the last stands
static void cli_grammar_each(const CliEachPluginCfg *args)
{
    CliGrammarSought *const sought = (CliGrammarSought *)args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = sought->id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    sought->found = 1;
    sought->whole->at = 0ull;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = sought->err_msg, .owner = &g_cli_plugins_cells);
    const unsigned int grammar = (root != CLI_JSON_NONE)
                                     ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = root, .name = "grammar")
                                     : CLI_JSON_NONE;
    const int has_grammar = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                       .node = grammar, .kind = CFG_JSON_OBJECT);
    if ((root != CLI_JSON_NONE) && !has_grammar)
    {
        mmgr_line_put(sought->err_msg, "the plugin ");
        mmgr_line_put(sought->err_msg, sought->id);
        mmgr_line_put(sought->err_msg, " has no grammar");
    }
    sought->made =
        has_grammar &&
        EMBED_CALL(cli_plugins_err_msg_json.grammar_whole, CliPluginsErrMsgJsonCfg, .json = &json, .grammar = grammar,
                   .tree = sought->tree, .name = sought->name, .out = sought->whole, .err_msg = sought->err_msg) &&
        mmgr_line_ok(sought->whole);
    // the plugin's pairs written into its grammar as the brackets the grammar's `@brackets` names, before its last
    // brace
    const unsigned int pairs = (root != CLI_JSON_NONE)
                                   ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &json, .object = root, .name = "pairs")
                                   : CLI_JSON_NONE;
    if (sought->made &&
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = pairs,
                   .kind = CFG_JSON_ARRAY) &&
        (sought->whole->at > 2ull) && (sought->whole->out[sought->whole->at - 1ull] == '}'))
    {
        sought->whole->at -= 1ull;
        mmgr_line_put(sought->whole, ",\"brackets\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = pairs,
                   .out = sought->whole, .indent = 0u, .sorted = 0);
        mmgr_line_ch(sought->whole, '}');
        sought->made = mmgr_line_ok(sought->whole);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

// the whole grammar `whole` stored under `key`: 1, or 0 with why given
EMBED_INLINE int cli_grammar_whole_store(const PluginsGrammarCtx *args)
{
    const char *key = args->key;
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    const unsigned int grammar =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->whole->out, .size = args->whole->at, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = err_msg, .owner = &g_cli_plugins_cells);
    const int stored =
        (grammar != CLI_JSON_NONE) && EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = key,
                                                 .json = &json, .def = grammar, .err_msg = err_msg);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return stored;
}

int cli_plugins_grammar_store(const CliPluginsIdKeyCfg *args)
{
    VerbaLinea named = {args->key, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&named, "plugin:");
    mmgr_line_put(&named, args->id);
    // the plugin's id, and the folder its language stands for after a colon
    char plugin_id[CLI_NAME_MAX_LEN];
    VerbaLinea split = {plugin_id, sizeof(plugin_id), 0ull};
    const char *name = args->id;
    name += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = name, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .set = ":");
    // the colon lies at or after the id's first letter
    mmgr_line_put_n(&split, args->id, (unsigned long long)(name - args->id));
    name = (*name == ':') ? (name + 1) : name;
    if ((mmgr_line_finish(&named) == 0ull) || (mmgr_line_finish(&split) == 0ull))
    {
        mmgr_line_put(args->err_msg, "a plugin's id is longer than orior_cli supports");
        return 0;
    }
    char *const whole_cell = g_cli_cells->temporary_buf_alloc(CLI_PLUGIN_GRAMMAR_ROOM, &g_cli_plugins_cells);
    VerbaLinea whole = mmgr_line_over(whole_cell, CLI_PLUGIN_GRAMMAR_ROOM);
    if (whole_cell == NULL)
    {
        mmgr_line_put(args->err_msg, "a plugin's grammar could not be given a cell to be made whole in");
        return 0;
    }
    char tree[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_plugins_tree_table.tree, CliPluginsTreeCfg, .tree_out = tree))
    {
        tree[0] = '\0';
    }
    CliGrammarSought sought = {plugin_id, name, tree, 0, 0, &whole, args->err_msg};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_grammar_each, .context = &sought);
    const unsigned long long stamp =
        EMBED_CALL(cli_grammar_stamp, PluginsGrammarCtx, .text = whole.out, .size = whole.at);
    unsigned int at = 0u;
    while ((at < s_grammar_count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = s_grammars[at].id, .other = args->id, .cap = CLI_CELLS_POOL}))
    {
        at += 1u;
    }
    int stored = sought.made && (at < s_grammar_count) && (s_grammars[at].stamp == stamp);
    if (sought.made && !stored)
    {
        stored = EMBED_CALL(cli_grammar_whole_store, PluginsGrammarCtx, .key = args->key, .whole = &whole,
                            .err_msg = args->err_msg);
        // a grammar whose stamp the cells cannot hold is made whole again the next time it is sought
        if (stored && (at == s_grammar_count) && ((s_grammar_count < s_grammar_room) || cli_grammars_init()))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_grammars[at].id,
                              .src = args->id,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}) + 1u});
            s_grammar_count += 1u;
        }
        if (stored && (at < s_grammar_count))
        {
            s_grammars[at].stamp = stamp;
        }
    }
    g_cli_cells->persistent_buf_release(whole.out, &g_cli_plugins_cells);
    if (!sought.found)
    {
        mmgr_line_put(args->err_msg, "no plugin is named ");
        mmgr_line_put(args->err_msg, plugin_id);
    }
    return stored;
}
