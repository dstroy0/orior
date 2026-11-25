// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_settings.c: the settings of each plugin, its plugin.json read once into a tree and kept in memory. Every
// reader of a plugin gets the kept tree, a setting is reached by its dotted key, and a change of a setting changes the
// kept tree in place (plugins.h)
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "plugins.h"

// One plugin as the table keeps it. The record holds the plugin's id, whether it is the reader's, and its folder. It
// holds the place of its text among the plugins that come with orior, or the cell of the reader's text. It also holds
// the size of the text, the tree and the root of the tree. The tree's cell is NULL where the text is no JSON
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    int user;
    char folder[CLI_PATH_MAX_LEN];
    unsigned int bundled;
    const CarcerMovableEntry *text;
    unsigned long long size;
    CliJsonStored tree;
    unsigned int root;
} PluginsKept;

// The head of the table's cell: the count of plugins after it
typedef struct
{
    unsigned int count;
} PluginsKeptHead;

// the offset in the table's cell of the plugin `at`
#define PLUGINS_KEPT_AT(at) (sizeof(PluginsKeptHead) + ((unsigned long long)(at) * sizeof(PluginsKept)))

// The table as the build fills it: a cell of the temporary tier of `capacity` plugins, `count` of them kept
typedef struct
{
    PluginsKept *kept;
    unsigned int count;
    unsigned int capacity;
} PluginsBuilt;

typedef struct
{
    const CarcerMovableEntry *table;
    unsigned int at;
    PluginsKept *kept;
    CliJson *json;
    char **text_out;
    const char *id;
    void (*each)(const CliEachPluginCfg *args);
    void *context;
} PluginsSettingsCtx;

// The token of the table's cell, the integer of its address. The gate keeps a read of the table apart from a build or
// a change of it. A reader holds the gate only while it copies one plugin out, and never while it reads the plugin
static embed_atomic_word s_kept;
static CliGate s_kept_gate = CLI_GATE_INIT;

// each plugin of the files counted
static void plugins_count(const CliEachPluginCfg *args)
{
    PluginsBuilt *const built = (PluginsBuilt *)args->context;
    built->capacity += 1u;
}

// the place among the plugins that come with orior of the one whose text is `text`, or the count of them where none is
EMBED_INLINE unsigned int plugins_bundled_place(const char *text)
{
    unsigned int at = 0u;
    while ((at < g_cli_bundled_count) && ((const char *)g_cli_bundled[at].text != text))
    {
        at += 1u;
    }
    return at;
}

// a text copied into `out`, sized for `capacity` bytes, with its end
EMBED_INLINE void plugins_text_copy(char *out, unsigned long long capacity, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = capacity - 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = out, .src = text, .bytes = length});
    out[length] = '\0';
}

// one plugin of the files kept: its names, its text where it is the reader's, and its text read into a stored tree
static void plugins_keep(const CliEachPluginCfg *args)
{
    PluginsBuilt *const built = (PluginsBuilt *)args->context;
    if (built->count >= built->capacity)
    {
        return;
    }
    PluginsKept *const kept = &built->kept[built->count];
    memor.set(&(MemoriaCfg){.dst = kept, .val = 0, .bytes = sizeof(*kept)});
    plugins_text_copy(kept->id, sizeof(kept->id), args->plugin->id);
    kept->user = args->plugin->folder != NULL;
    plugins_text_copy(kept->folder, sizeof(kept->folder), kept->user ? args->plugin->folder : "");
    kept->bundled = kept->user ? g_cli_bundled_count : plugins_bundled_place(args->plugin->text);
    kept->size = args->plugin->size;
    kept->root = CLI_JSON_NONE;
    if (kept->user)
    {
        // the reader's text goes into a cell of its own with its end, and the module does not read its file again
        kept->text = g_cli_guard->movable_buf_alloc(args->plugin->size + 1ULL, g_cli_plugins_owner);
        if (kept->text == NULL)
        {
            return;
        }
        const char end = '\0';
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = kept->text, .offset = 0ULL, .src = args->plugin->text, .bytes = args->plugin->size});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = kept->text, .offset = args->plugin->size, .src = &end, .bytes = 1ULL});
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &err_msg, .owner = g_cli_plugins_owner);
    if ((root != CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_stored.json_stored, CliJsonTreeStoredCfg,
                                              .json_read = &json, .stored = &kept->tree, .owner = g_cli_plugins_owner))
    {
        kept->root = root;
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    built->count += 1u;
}

// the cells of one kept plugin given back
EMBED_INLINE void plugins_kept_release(PluginsKept *kept)
{
    cli_json_stored_release(&(CliJsonStoredCfg){.stored = &kept->tree});
    if (kept->text != NULL)
    {
        (void)g_cli_guard->movable_buf_release(kept->text, g_cli_plugins_owner);
        kept->text = NULL;
    }
}

// every plugin of the files kept in a new table's cell: the token of the cell, or NULL where the pool refuses a cell
static const CarcerMovableEntry *plugins_kept_build(void)
{
    PluginsBuilt built = {NULL, 0u, 0u};
    cli_plugins_files_each(&(CliPluginsEachCfg){.each = plugins_count, .context = &built});
    built.kept = (built.capacity != 0u)
                     ? g_cli_guard->temporary_buf_alloc((unsigned long long)built.capacity * sizeof(PluginsKept),
                                                        g_cli_plugins_owner)
                     : NULL;
    if ((built.kept == NULL) && (built.capacity != 0u))
    {
        return NULL;
    }
    cli_plugins_files_each(&(CliPluginsEachCfg){.each = plugins_keep, .context = &built});
    const CarcerMovableEntry *const table =
        g_cli_guard->movable_buf_alloc(PLUGINS_KEPT_AT(built.count), g_cli_plugins_owner);
    if (table == NULL)
    {
        for (unsigned int at = 0u; at < built.count; at += 1u)
        {
            plugins_kept_release(&built.kept[at]);
        }
        g_cli_guard->persistent_buf_release(built.kept, g_cli_plugins_owner);
        return NULL;
    }
    const PluginsKeptHead head = {built.count};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = table, .offset = 0ULL, .src = &head, .bytes = sizeof(head)});
    if (built.count != 0u)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = table,
                                .offset = PLUGINS_KEPT_AT(0u),
                                .src = built.kept,
                                .bytes = (unsigned long long)built.count * sizeof(PluginsKept)});
    }
    g_cli_guard->persistent_buf_release(built.kept, g_cli_plugins_owner);
    return table;
}

// the count of plugins of the table `table`, 0 where the table is none
EMBED_INLINE unsigned int plugins_kept_count(const CarcerMovableEntry *table)
{
    PluginsKeptHead head = {0u};
    if (table != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = table, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    }
    return head.count;
}

// the table `table` given back: each kept plugin's cells, then the table's own cell
EMBED_INLINE void plugins_table_release(const CarcerMovableEntry *table)
{
    const unsigned int count = plugins_kept_count(table);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        PluginsKept kept;
        if (g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = table, .offset = PLUGINS_KEPT_AT(at), .dst = &kept, .bytes = sizeof(kept)}))
        {
            plugins_kept_release(&kept);
        }
    }
    if (table != NULL)
    {
        (void)g_cli_guard->movable_buf_release(table, g_cli_plugins_owner);
    }
}

// the token of the table, with the gate shut: the call builds the table where no call built it before
EMBED_INLINE const CarcerMovableEntry *plugins_table_held(void)
{
    if (EMBED_CALL(embed_atomic_word_load, EmbedAtomicWordLoadCfg, .word = &s_kept) == 0u)
    {
        // the token goes through the atomic word as the integer of its address
        EMBED_CALL(embed_atomic_word_store, EmbedAtomicWordStoreCfg, .word = &s_kept,
                   .value = (embed_atomic_word_value)(embed_word)plugins_kept_build());
    }
    return (const CarcerMovableEntry *)(embed_word)EMBED_CALL(embed_atomic_word_load, EmbedAtomicWordLoadCfg,
                                                              .word = &s_kept);
}

// The plugin `at` of the table copied out, with the gate shut. Its record goes into `kept`, and its tree into `json`, a
// tree in a cell of the call's own. The reader's text goes into a cell of the call's own at `text_out`. 1, or 0 where
// the table has no plugin `at` or the pool refuses a cell
EMBED_INLINE int plugins_kept_copy(const PluginsSettingsCtx *args)
{
    PluginsKept *const kept = args->kept;
    if ((args->at >= plugins_kept_count(args->table)) ||
        !g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = args->table, .offset = PLUGINS_KEPT_AT(args->at), .dst = kept, .bytes = sizeof(*kept)}))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if ((kept->tree.cell != NULL) && !EMBED_CALL(cli_json_tree_stored.json_pinned, CliJsonTreeStoredCfg,
                                                 .stored_read = &kept->tree, .json = args->json))
    {
        return 0;
    }
    *args->text_out = NULL;
    if (kept->user)
    {
        *args->text_out = g_cli_guard->temporary_buf_alloc(kept->size + 1ULL, g_cli_plugins_owner);
        if ((*args->text_out == NULL) ||
            !g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = kept->text, .offset = 0ULL, .dst = *args->text_out, .bytes = kept->size + 1ULL}))
        {
            g_cli_guard->persistent_buf_release(*args->text_out, g_cli_plugins_owner);
            g_cli_guard->persistent_buf_release(args->json->node, kept->tree.owner);
            return 0;
        }
    }
    return 1;
}

// The plugin `at` of the table passed to `each`, with its tree. The call copies the plugin out with the gate shut, and
// `each` reads the copy with the gate open. 1, or 0 where the table has no plugin `at`
EMBED_INLINE int plugins_kept_pass(const PluginsSettingsCtx *args)
{
    PluginsKept kept;
    CliJson json;
    char *text = NULL;
    cli_gate_shut(&s_kept_gate);
    const int copied = EMBED_CALL(plugins_kept_copy, PluginsSettingsCtx, .table = plugins_table_held(), .at = args->at,
                                  .kept = &kept, .json = &json, .text_out = &text);
    cli_gate_opened(&s_kept_gate);
    if (!copied)
    {
        return 0;
    }
    const char *const body =
        kept.user ? text : ((kept.bundled < g_cli_bundled_count) ? (const char *)g_cli_bundled[kept.bundled].text : "");
    const CliPlugin plugin = {kept.id,
                              kept.user ? "user" : "bundled",
                              kept.user ? kept.folder : NULL,
                              body,
                              kept.size,
                              (kept.tree.cell != NULL) ? &json : NULL,
                              kept.root};
    args->each(&(CliEachPluginCfg){.context = args->context, .plugin = &plugin});
    if (kept.tree.cell != NULL)
    {
        g_cli_guard->persistent_buf_release(json.node, kept.tree.owner);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_plugins_owner);
    return 1;
}

void cli_plugins_settings_each(const CliPluginsEachCfg *args)
{
    unsigned int at = 0u;
    while (EMBED_CALL(plugins_kept_pass, PluginsSettingsCtx, .at = at, .each = args->each, .context = args->context))
    {
        at += 1u;
    }
}

// the place of the last plugin of the id `id` in the table `table`, or the count of the table's plugins where none is
EMBED_INLINE unsigned int plugins_kept_last(const PluginsSettingsCtx *args)
{
    const unsigned int count = plugins_kept_count(args->table);
    const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_NAME_MAX_LEN}) + 1ULL;
    unsigned int found = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                .cell = args->table, .offset = PLUGINS_KEPT_AT(at), .src = args->id, .bytes = bytes}) == 0)
        {
            found = at;
        }
    }
    return found;
}

int cli_plugins_settings_of(const CliPluginsSettingsOfCfg *args)
{
    cli_gate_shut(&s_kept_gate);
    const CarcerMovableEntry *const table = plugins_table_held();
    const unsigned int at = EMBED_CALL(plugins_kept_last, PluginsSettingsCtx, .table = table, .id = args->id);
    const int known = at < plugins_kept_count(table);
    cli_gate_opened(&s_kept_gate);
    return known &&
           EMBED_CALL(plugins_kept_pass, PluginsSettingsCtx, .at = at, .each = args->each, .context = args->context);
}

// the length of the member name `name` where the key `key` starts with it and ends there or at a dot, else 0
EMBED_INLINE unsigned long long plugins_key_fits(const char *key, const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_NAME_MAX_LEN});
    if ((length == 0ULL) || !cellul.starts(&(CatenaFinitaCfg){.src = key, .other = name, .cap = CLI_NAME_MAX_LEN}) ||
        ((key[length] != '\0') && (key[length] != '.')))
    {
        return 0ULL;
    }
    return length;
}

unsigned int cli_plugins_setting(const CliPluginsSettingCfg *args)
{
    const CliJson *const json = args->json;
    unsigned int node = args->node;
    const char *key = args->key;
    while ((key[0] != '\0') && (node != CLI_JSON_NONE))
    {
        unsigned long long taken = 0ULL;
        unsigned int next = CLI_JSON_NONE;
        if (json->node[node].type == CFG_JSON_OBJECT)
        {
            // the longest member name the key starts with: a name can carry dots
            for (unsigned int member = json->node[node].first; member != CLI_JSON_NONE;
                 member = json->node[member].next)
            {
                const unsigned long long length = plugins_key_fits(key, json->text + json->node[member].key);
                if (length > taken)
                {
                    taken = length;
                    next = member;
                }
            }
        }
        else if (json->node[node].type == CFG_JSON_ARRAY)
        {
            unsigned int index = 0u;
            while ((key[taken] >= '0') && (key[taken] <= '9'))
            {
                index = (index * 10u) + (unsigned int)(key[taken] - '0');
                taken += 1ULL;
            }
            next = ((taken != 0ULL) && ((key[taken] == '\0') || (key[taken] == '.')))
                       ? EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                    .array = node, .index = index)
                       : CLI_JSON_NONE;
        }
        if (next == CLI_JSON_NONE)
        {
            return CLI_JSON_NONE;
        }
        node = next;
        key += taken;
        key += (key[0] == '.') ? 1u : 0u;
    }
    return node;
}

// The change of one setting, with the gate shut. The call copies the plugin's tree into a tree with room for the new
// value and puts the value. It then stores the tree in place of the kept one. 1, or 0 where a step fails
EMBED_INLINE int plugins_setting_put(const CliPluginsSettingSetCfg *args)
{
    const CarcerMovableEntry *const table = plugins_table_held();
    const unsigned int at = EMBED_CALL(plugins_kept_last, PluginsSettingsCtx, .table = table, .id = args->id);
    PluginsKept kept;
    CliJson from;
    char *text = NULL;
    if (!EMBED_CALL(plugins_kept_copy, PluginsSettingsCtx, .table = table, .at = at, .kept = &kept, .json = &from,
                    .text_out = &text))
    {
        return 0;
    }
    g_cli_guard->persistent_buf_release(text, g_cli_plugins_owner);
    if (kept.tree.cell == NULL)
    {
        return 0;
    }
    const unsigned long long value_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_PLUGIN_TEXT_CAPACITY});
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN});
    CliJson to;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &to, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    int put = EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &to,
                         .nodes_max = from.nodes + 2u, .text_max = from.used + value_length + name_length + 2ULL,
                         .what = "a plugin's settings", .owner = g_cli_plugins_owner);
    const unsigned int root = put ? EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .node = kept.root,
                                               .to = &to, .from = &from)
                                  : CLI_JSON_NONE;
    g_cli_guard->persistent_buf_release(from.node, kept.tree.owner);
    const unsigned int object = (root != CLI_JSON_NONE)
                                    ? EMBED_CALL(cli_plugins_setting_table.setting, CliPluginsSettingCfg, .json = &to,
                                                 .node = root, .key = args->object)
                                    : CLI_JSON_NONE;
    put = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &to, .node = object,
                     .type = CFG_JSON_OBJECT);
    const unsigned int value =
        put ? EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg, .json = &to,
                         .text = args->value, .length = value_length)
            : CLI_JSON_NONE;
    put = (value != CLI_JSON_NONE);
    if (put)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &to, .object = object,
                   .name = args->name, .value = value);
        put = !to.full && EMBED_CALL(cli_json_tree_stored.json_stored, CliJsonTreeStoredCfg, .json_read = &to,
                                     .stored = &kept.tree, .owner = g_cli_plugins_owner);
    }
    if (put)
    {
        // the new tree's cell and root go into the table in place of the kept ones
        kept.root = root;
        put = g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = table, .offset = PLUGINS_KEPT_AT(at), .src = &kept, .bytes = sizeof(kept)});
    }
    g_cli_guard->persistent_buf_release(to.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), to.node));
    return put;
}

int cli_plugins_setting_set(const CliPluginsSettingSetCfg *args)
{
    cli_gate_shut(&s_kept_gate);
    const int put = plugins_setting_put(args);
    cli_gate_opened(&s_kept_gate);
    return put;
}

void cli_plugins_settings_changed(void)
{
    // the build reads the files with the gate open, and only the change of the token shuts the gate
    const CarcerMovableEntry *const table = plugins_kept_build();
    cli_gate_shut(&s_kept_gate);
    // the token goes through the atomic word as the integer of its address
    const CarcerMovableEntry *const before = (const CarcerMovableEntry *)(embed_word)EMBED_CALL(
        embed_atomic_word_exchange, EmbedAtomicWordExchangeCfg, .word = &s_kept,
        .value = (embed_atomic_word_value)(embed_word)table);
    plugins_table_release(before);
    cli_gate_opened(&s_kept_gate);
}

void cli_plugins_settings_end(void)
{
    cli_gate_shut(&s_kept_gate);
    const CarcerMovableEntry *const before = (const CarcerMovableEntry *)(embed_word)EMBED_CALL(
        embed_atomic_word_exchange, EmbedAtomicWordExchangeCfg, .word = &s_kept, .value = (embed_atomic_word_value)0u);
    plugins_table_release(before);
    cli_gate_opened(&s_kept_gate);
}
