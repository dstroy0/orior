// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog.c: plugins others publish, found in a catalog and installed from File, Plugins. A catalog is a JSON
// file at an address the reader names, on the web or on this machine:
//
//   { "orior": "plugin catalog", "plugins": [ { "id", "name", "version", "about", "package",
//     "sha256", "reads", "writes", "reaches" }, ... ] }
//
// and each plugin's package a JSON file, at `package`, an address of its own or one from the catalog's, containing the
// plugin's files by name:
//
//   { "orior": "plugin", "files": { "plugin.json": "...", "plugin.js": "..." } }
//
// A package whose catalog gives its `sha256` is installed only where its bytes hash to it, and one whose plugin.json
// names another id or version than its catalog's entry is not installed. What a plugin reads, writes and reaches on
// the network is what its own plugin.json declares (plugins.h); the window restricts it to that. Nothing is read from a
// catalog or installed but when the reader requests it, and a plugin is updated only by installing it again
#include "../command_line/pool.h"
#include "plugin_catalog_internal.h"

#include "../machine/net.h"
#include "../requests/requests.h"
#include "plugins.h"

#include "../text/text.h"

CliCatalogWorkState *g_cli_catalog;
static CliCatalogWorkState s_catalog_work_state;

// an entry's fields that are texts, and those that are lists of texts, in the order serde writes them
static const char *const s_texts[6] = {"id", "name", "version", "about", "package", "sha256"};
static const char *const s_lists[3] = {"reads", "writes", "reaches"};

int cli_plugin_catalog_lock(const CliPluginCatalogCtx *args)
{
    (void)args;
    g_cli_catalog = &s_catalog_work_state;
    g_cli_catalog->bytes = NULL;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &g_cli_catalog->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    return 1;
}

unsigned int cli_plugin_catalog_tree(const CliPluginCatalogCtx *args)
{
    g_cli_guard->persistent_buf_release(g_cli_catalog->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_catalog->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_catalog->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                      .json = &g_cli_catalog->json, .text = args->text, .size = args->size_value, .nodes_more = 0u,
                      .text_more = 0ull, .err_msg = args->err_msg, .owner = g_cli_plugins_owner);
}

typedef struct
{
    const char *text;
    const char *prefix;
    const char *path;
    const CliJson *json;
    unsigned int node;
    unsigned int entry;
    VerbaLinea *out;
} PluginCatalogCtx;
// whether `path` is whole on its own, as Rust's Path::is_absolute has it on this system
EMBED_INLINE int catalog_absolute(const PluginCatalogCtx *args)
{
#if defined(_WIN32)
    const int drive =
        (((args->path[0] >= 'a') && (args->path[0] <= 'z')) || ((args->path[0] >= 'A') && (args->path[0] <= 'Z'))) &&
        (args->path[1] == ':') && ((args->path[2] == '\\') || (args->path[2] == '/'));
    const int shared =
        ((args->path[0] == '\\') || (args->path[0] == '/')) && ((args->path[1] == '\\') || (args->path[1] == '/'));
    return drive || shared;
#else
    return args->path[0] == '/';
#endif
}

void cli_plugin_catalog_adjacent(const CliPluginCatalogCfg *args)
{
    if ((cellul.find(&(CatenaFinitaCfg){
             .src = args->address, .cap = CLI_POOL_BYTES, .other = "://", .other_cap = CLI_POOL_BYTES}) != NULL) ||
        EMBED_CALL(catalog_absolute, PluginCatalogCtx, .path = args->address))
    {
        mmgr_line_put(args->out, args->address);
        return;
    }
    if (cellul.find(&(CatenaFinitaCfg){
            .src = args->base, .cap = CLI_POOL_BYTES, .other = "://", .other_cap = CLI_POOL_BYTES}) != NULL)
    {
        const char *const cut = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = args->base,
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_POOL_BYTES}), .byte = '/');
        if (cut != NULL)
        {
            mmgr_line_put_n(args->out, args->base, (unsigned long long)(cut - args->base));
            mmgr_line_ch(args->out, '/');
        }
        mmgr_line_put(args->out, args->address);
        return;
    }
    char parent[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->base, .out = parent))
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = parent, .cap = CLI_POOL_BYTES});
        mmgr_line_put(args->out, parent);
        if ((length > 0u) && (parent[length - 1u] != '/') && (parent[length - 1u] != '\\'))
        {
#if defined(_WIN32)
            mmgr_line_ch(args->out, '\\');
#else
            mmgr_line_ch(args->out, '/');
#endif
        }
    }
    mmgr_line_put(args->out, args->address);
}

// the name serde gives a value's type where it is not the type wanted
EMBED_INLINE const char *catalog_type(const PluginCatalogCtx *args)
{
    static const char *const types[7] = {"map", "sequence", "string", "number", "boolean", "boolean", "null"};
    return (args->node == CLI_JSON_NONE) ? "null" : types[args->json->node[args->node].type];
}

void cli_plugin_catalog_invalid(const CliPluginCatalogWhereCtx *args)
{
    mmgr_line_put(args->err_msg, args->where);
    mmgr_line_put(args->err_msg, ": invalid type: ");
    mmgr_line_put(args->err_msg, EMBED_CALL(catalog_type, PluginCatalogCtx, .json = args->json, .node = args->node));
    mmgr_line_put(args->err_msg, ", expected ");
    mmgr_line_put(args->err_msg, args->required);
}

int cli_plugin_catalog_entry_read(const CliPluginCatalogErrMsgCfg *args)
{
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json,
                    .node = args->entry_u32, .type = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                   .json = args->json, .node = args->entry_u32, .required = "struct Entry", .err_msg = args->err_msg);
        return 0;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        const unsigned int field = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = args->entry_u32, .name = s_texts[at]);
        if ((field != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                    .json_read = args->json, .node = field, .type = CFG_JSON_STRING))
        {
            EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                       .json = args->json, .node = field, .required = "a string", .err_msg = args->err_msg);
            return 0;
        }
    }
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned int field = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = args->entry_u32, .name = s_lists[at]);
        if ((field != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                    .json_read = args->json, .node = field, .type = CFG_JSON_ARRAY))
        {
            EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                       .json = args->json, .node = field, .required = "a sequence", .err_msg = args->err_msg);
            return 0;
        }
        for (unsigned int item = 0u;
             (field != CLI_JSON_NONE) &&
             (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                         .array = field, .index = item) != CLI_JSON_NONE);
             item += 1u)
        {
            if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json,
                            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = args->json, .array = field, .index = item),
                            .type = CFG_JSON_STRING))
            {
                EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                           .json = args->json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = args->json, .array = field, .index = item),
                           .required = "a string", .err_msg = args->err_msg);
                return 0;
            }
        }
    }
    return 1;
}

const char *cli_plugin_catalog_field(const CliPluginCatalogJsonCfg *args)
{
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->entry_u32, .name = args->name),
                   .length_pointer = NULL);
    return (text != NULL) ? text : "";
}
