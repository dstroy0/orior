// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_grammar.c: a language plugin's grammar stored for the colorer under `plugin:<id>`, read from the plugin.json
// of the plugin of that id that is, the reader's in place of one that comes with orior, made whole from the tree's
// tables, and stored again only where the whole grammar changed. A language `<id>:<name>` is the plugin `<id>`'s for
// the folder `<name>`
#include "../command_line/pool.h"
#include "../editor/highlight.h"
#include "../text/text.h"
#include "plugins.h"

// the plugins whose grammars are stored at max, and the bytes a whole grammar is written in
#define CLI_PLUGIN_GRAMMARS_MAX 1024u
#define CLI_PLUGIN_GRAMMAR_CAPACITY (4ULL << 20u)

// A plugin whose grammar is stored: its id, and the stamp of the plugin.json it was read from. Each is in a movable
// cell of its own: the stamp first, then the id with its 0
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    unsigned long long stamp;
} CliPluginGrammar;
#define CLI_PLUGIN_GRAMMAR_ID_AT sizeof(unsigned long long)

// the plugins whose grammars are stored, each cell's token, and how many are
static const CarcerMovableEntry *s_grammars[CLI_PLUGIN_GRAMMARS_MAX];
static unsigned int s_grammar_count;

// the plugin `at` of the list read off its cell
static CliPluginGrammar grammar_listed(unsigned int at)
{
    CliPluginGrammar listed = {{0}, 0ULL};
    const unsigned long long bytes = g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = s_grammars[at]});
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_grammars[at], .offset = 0ULL, .dst = &listed.stamp, .bytes = sizeof(listed.stamp)});
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_grammars[at],
                                                            .offset = CLI_PLUGIN_GRAMMAR_ID_AT,
                                                            .dst = listed.id,
                                                            .bytes = bytes - CLI_PLUGIN_GRAMMAR_ID_AT});
    return listed;
}

// the plugin `id` listed after the others with `stamp`, in a cell sized to the two: 1, or 0 where the list is at
// its max or the cells cannot meet it; under the lock
static int cli_grammar_listed_put(const char *id, unsigned long long stamp)
{
    const unsigned long long id_bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_NAME_MAX_LEN - 1u}) + 1ULL;
    const CarcerMovableEntry *const cell =
        (s_grammar_count < CLI_PLUGIN_GRAMMARS_MAX)
            ? g_cli_guard->movable_buf_alloc(CLI_PLUGIN_GRAMMAR_ID_AT + id_bytes, g_cli_plugins_owner)
            : NULL;
    if (cell == NULL)
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = &stamp, .bytes = sizeof(stamp)});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = CLI_PLUGIN_GRAMMAR_ID_AT, .src = id, .bytes = id_bytes});
    s_grammars[s_grammar_count] = cell;
    s_grammar_count += 1u;
    return 1;
}

// The plugin sought: its id, and the folder its language is for, empty for none; the tree its tables are read
// from; whether one of that id was found and its grammar made whole, the last, the one that is, written into
// `whole`; and why where it was not
typedef struct
{
    const char *id;
    const char *name;
    const char *tree;
    int found;
    int made;
    CliGrownLine *whole;
    VerbaLinea *err_msg;
} CliGrammarNeedle;

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *key;
    const CliGrownLine *whole;
    VerbaLinea *err_msg;
} PluginsGrammarCtx;
// a text's stamp, FNV-1a over its bytes
EMBED_INLINE unsigned long long cli_grammar_stamp(const PluginsGrammarCtx *args)
{
    unsigned long long stamp = 0xcbf29ce484222325ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        stamp = (stamp ^ (unsigned char)args->text[at]) * 0x100000001b3ULL;
    }
    return stamp;
}

// a plugin of the id sought, its grammar made whole in the place of one made before: the last is
static void cli_grammar_each(const CliEachPluginCfg *args)
{
    CliGrammarNeedle *const needle = (CliGrammarNeedle *)args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = needle->id, .cap = CLI_POOL_BYTES}))
    {
        return;
    }
    needle->found = 1;
    cli_grown_reset(needle->whole);
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = needle->err_msg, .owner = g_cli_plugins_owner);
    const unsigned int grammar = (root != CLI_JSON_NONE)
                                     ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = root, .name = "grammar")
                                     : CLI_JSON_NONE;
    const int has_grammar = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                                       .node = grammar, .type = CFG_JSON_OBJECT);
    if ((root != CLI_JSON_NONE) && !has_grammar)
    {
        mmgr_line_put(needle->err_msg, "the plugin ");
        mmgr_line_put(needle->err_msg, needle->id);
        mmgr_line_put(needle->err_msg, " has no grammar");
    }
    needle->made = has_grammar &&
                   EMBED_CALL(cli_plugins_err_msg_json.grammar_resolved, CliPluginsErrMsgJsonCfg, .json = &json,
                              .grammar = grammar, .tree = needle->tree, .name = needle->name,
                              .out = &needle->whole->line, .err_msg = needle->err_msg) &&
                   mmgr_line_ok(&needle->whole->line);
    // the plugin's pairs written into its grammar as the brackets the grammar's `@brackets` names, before its last
    // brace
    const unsigned int pairs = (root != CLI_JSON_NONE)
                                   ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &json, .object = root, .name = "pairs")
                                   : CLI_JSON_NONE;
    if (needle->made &&
        EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json, .node = pairs,
                   .type = CFG_JSON_ARRAY) &&
        (cli_grown_size(needle->whole) > 2ULL) && (needle->whole->cell[cli_grown_size(needle->whole) - 1ULL] == '}'))
    {
        // the closing brace written as the comma before the brackets, the brackets after it, and a brace closes again
        needle->whole->cell[cli_grown_size(needle->whole) - 1ULL] = ',';
        mmgr_line_put(&needle->whole->line, "\"brackets\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = pairs,
                   .out = &needle->whole->line, .indent = 0u, .sorted = 0);
        mmgr_line_ch(&needle->whole->line, '}');
        needle->made = mmgr_line_ok(&needle->whole->line);
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

// the whole grammar `whole` stored under `key`: 1, or 0 with why given
EMBED_INLINE int cli_grammar_resolved_store(const PluginsGrammarCtx *args)
{
    const char *key = args->key;
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int grammar =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->whole->cell, .size = cli_grown_size(args->whole), .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = err_msg, .owner = g_cli_plugins_owner);
    const int stored =
        (grammar != CLI_JSON_NONE) && EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = key,
                                                 .json = &json, .def = grammar, .err_msg = err_msg);
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return stored;
}

// One store at a time: the stores' tables, the highlighter's read scratch and the grammars stored are one writer's,
// behind this gate
static CliGate s_store_gate = CLI_GATE_INIT;

static int plugins_grammar_stored(const CliPluginsIdKeyCfg *args);

int cli_plugins_grammar_store(const CliPluginsIdKeyCfg *args)
{
    cli_gate_shut(&s_store_gate);
    const int stored = plugins_grammar_stored(args);
    cli_gate_opened(&s_store_gate);
    return stored;
}

// the store itself, with the stores closed to every other thread
static int plugins_grammar_stored(const CliPluginsIdKeyCfg *args)
{
    VerbaLinea named = {args->key, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&named, "plugin:");
    mmgr_line_put(&named, args->id);
    // the plugin's id, and the folder its language is for after a colon
    char plugin_id[CLI_NAME_MAX_LEN];
    VerbaLinea split = {plugin_id, sizeof(plugin_id), 0ULL};
    const char *name = args->id;
    name += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = name, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .set = ":");
    // the colon lies at or after the id's first character
    mmgr_line_put_n(&split, args->id, (unsigned long long)(name - args->id));
    name = (*name == ':') ? (name + 1) : name;
    if ((mmgr_line_finish(&named) == 0ULL) || (mmgr_line_finish(&split) == 0ULL))
    {
        mmgr_line_put(args->err_msg, "a plugin's id is longer than impero supports");
        return 0;
    }
    CliGrownLine whole;
    cli_grown_begin(&whole, CLI_PLUGIN_GRAMMAR_CAPACITY, g_cli_plugins_owner);
    if (whole.cell == NULL)
    {
        mmgr_line_put(args->err_msg, "a plugin's grammar could not be given a cell to be made whole in");
        return 0;
    }
    char tree[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_plugins_tree_table.tree, CliPluginsTreeCfg, .tree_out = tree))
    {
        tree[0] = '\0';
    }
    CliGrammarNeedle needle = {plugin_id, name, tree, 0, 0, &whole, args->err_msg};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_grammar_each, .context = &needle);
    const unsigned long long stamp =
        EMBED_CALL(cli_grammar_stamp, PluginsGrammarCtx, .text = whole.cell, .size = cli_grown_size(&whole));
    unsigned int at = 0u;
    CliPluginGrammar listed = {{0}, 0ULL};
    while (at < s_grammar_count)
    {
        listed = grammar_listed(at);
        if (cellul.eq(&(CatenaFinitaCfg){.src = listed.id, .other = args->id, .cap = CLI_POOL_BYTES}))
        {
            break;
        }
        at += 1u;
    }
    int stored = needle.made && (at < s_grammar_count) && (listed.stamp == stamp);
    if (needle.made && !stored)
    {
        stored = EMBED_CALL(cli_grammar_resolved_store, PluginsGrammarCtx, .key = args->key, .whole = &whole,
                            .err_msg = args->err_msg);
        // a grammar whose stamp the cells cannot hold is made whole again the next time it is sought
        if (stored && (at == s_grammar_count))
        {
            (void)cli_grammar_listed_put(args->id, stamp);
        }
        else if (stored)
        {
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = s_grammars[at], .offset = 0ULL, .src = &stamp, .bytes = sizeof(stamp)});
        }
    }
    cli_grown_end(&whole);
    if (!needle.found)
    {
        mmgr_line_put(args->err_msg, "no plugin is named ");
        mmgr_line_put(args->err_msg, plugin_id);
    }
    return stored;
}
