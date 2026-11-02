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
#include "../command_line/cells.h"
#include "plugin_catalog_internal.h"

#include "../machine/net.h"
#include "../requests/requests.h"
#include "plugins.h"

#include "../text/text.h"

CliCatalogRoom *g_cli_catalog;
static CliCatalogRoom s_catalog_room;

// an entry's fields that are texts, and those that are lists of texts, in the order serde writes them
static const char *const s_texts[6] = {"id", "name", "version", "about", "package", "sha256"};
static const char *const s_lists[3] = {"reads", "writes", "reaches"};

int cli_plugin_catalog_lock(const CliPluginCatalogCtx *args)
{
    (void)args;
    g_cli_catalog = &s_catalog_room;
    g_cli_catalog->bytes = NULL;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &g_cli_catalog->json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    return 1;
}

void cli_plugin_catalog_release(void)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &g_cli_catalog->json);
    g_cli_cells->persistent_buf_release(g_cli_catalog->bytes, &g_cli_plugins_cells);
    g_cli_catalog->bytes = NULL;
    g_cli_catalog = NULL;
}

unsigned int cli_plugin_catalog_tree(const CliPluginCatalogCtx *args)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &g_cli_catalog->json);
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg,
                      .json = &g_cli_catalog->json, .text = args->text, .size = args->size_value, .nodes_more = 0u,
                      .text_more = 0ull, .err_msg = args->err_msg, .owner = &g_cli_plugins_cells);
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
// whether `text` begins with `prefix`
EMBED_INLINE int catalog_begins(const PluginCatalogCtx *args)
{
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->text, .other_pointer = args->prefix,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = args->prefix, .cap = CLI_CELLS_POOL}));
}

// the bytes at `address`, a web address or a file of this machine, requested for `purpose`, read into
// g_cli_catalog->bytes, their count into `size`: 1, or 0 with why given
int cli_plugin_catalog_fetch(const CliPluginCatalogAddressCtx *args)
{
    const char *address = args->address;
    unsigned long long *size = args->size;
    VerbaLinea *err_msg = args->err_msg;
    g_cli_cells->persistent_buf_release(g_cli_catalog->bytes, &g_cli_plugins_cells);
    g_cli_catalog->bytes = NULL;
    if (EMBED_CALL(catalog_begins, PluginCatalogCtx, .text = address, .prefix = "http://") ||
        EMBED_CALL(catalog_begins, PluginCatalogCtx, .text = address, .prefix = "https://"))
    {
        if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = address, .purpose = args->purpose,
                        .err_msg = err_msg))
        {
            return 0;
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea failed = {why, sizeof(why), 0ull};
        char reason[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea reason_line = {reason, sizeof(reason), 0ull};
        // a cell of the most a catalog holds, its body's bytes stored as its text
        const unsigned long long room = CLI_CATALOG_BYTES + 1ull;
        unsigned char *const joined = g_cli_cells->temporary_buf_alloc(room, &g_cli_plugins_cells);
        VerbaLinea body = mmgr_line_over((char *)joined, room);
        unsigned int status = 0u;
        int cut = 0;
        const int got =
            EMBED_CALL(cli_requests_address_table.get, CliRequestsAddressCfg, .address = address, .status = &status,
                       .reason = &reason_line, .cut = &cut, .body = &body, .err_msg = &failed);
        if (!got)
        {
            g_cli_cells->persistent_buf_release(joined, &g_cli_plugins_cells);
            mmgr_line_put(err_msg, address);
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, why, failed.at);
            return 0;
        }
        if ((status < 200u) || (status >= 300u))
        {
            g_cli_cells->persistent_buf_release(joined, &g_cli_plugins_cells);
            mmgr_line_put(err_msg, address);
            mmgr_line_put(err_msg, ": ");
            mmgr_line_decimal(err_msg, status, 1u);
            mmgr_line_ch(err_msg, ' ');
            mmgr_line_put_n(err_msg, reason, reason_line.at);
            return 0;
        }
        if (cut || !mmgr_line_ok(&body))
        {
            g_cli_cells->persistent_buf_release(joined, &g_cli_plugins_cells);
            mmgr_line_put(err_msg, address);
            mmgr_line_put(err_msg, " is larger than orior_cli reads");
            return 0;
        }
        g_cli_catalog->bytes = joined;
        *size = body.at;
        return 1;
    }
    const char *const path =
        EMBED_CALL(catalog_begins, PluginCatalogCtx, .text = address, .prefix = "file://") ? (address + 7) : address;
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                   .size_pointer = &mapped_size, .owner = &g_cli_plugins_cells);
    // a file of no bytes is stored in a cell of one, and every read has a cell
    unsigned char *const bytes =
        ((mapped != NULL) && (mapped_size <= CLI_CATALOG_BYTES))
            ? g_cli_cells->temporary_buf_alloc((mapped_size != 0ull) ? mapped_size : 1ull, &g_cli_plugins_cells)
            : NULL;
    if (bytes != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = bytes, .src = mapped, .bytes = (size_t)mapped_size});
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_plugins_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    g_cli_catalog->bytes = bytes;
    *size = mapped_size;
    return 1;
}

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

void cli_plugin_catalog_beside(const CliPluginCatalogCfg *args)
{
    if ((cellul.find(&(CatenaFinitaCfg){
             .src = args->address, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL) ||
        EMBED_CALL(catalog_absolute, PluginCatalogCtx, .path = args->address))
    {
        mmgr_line_put(args->out, args->address);
        return;
    }
    if (cellul.find(&(CatenaFinitaCfg){
            .src = args->base, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL)
    {
        const char *const cut = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = args->base,
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}), .byte = '/');
        if (cut != NULL)
        {
            mmgr_line_put_n(args->out, args->base, (unsigned long long)(cut - args->base));
            mmgr_line_ch(args->out, '/');
        }
        mmgr_line_put(args->out, args->address);
        return;
    }
    char parent[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->base, .out = parent))
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = parent, .cap = CLI_CELLS_POOL});
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

// the name serde gives a value's kind where it is not the kind wanted
EMBED_INLINE const char *catalog_kind(const PluginCatalogCtx *args)
{
    static const char *const kinds[7] = {"map", "sequence", "string", "number", "boolean", "boolean", "null"};
    return (args->node == CLI_JSON_NONE) ? "null" : kinds[args->json->node[args->node].kind];
}

void cli_plugin_catalog_invalid(const CliPluginCatalogWhereCtx *args)
{
    mmgr_line_put(args->err_msg, args->where);
    mmgr_line_put(args->err_msg, ": invalid type: ");
    mmgr_line_put(args->err_msg, EMBED_CALL(catalog_kind, PluginCatalogCtx, .json = args->json, .node = args->node));
    mmgr_line_put(args->err_msg, ", expected ");
    mmgr_line_put(args->err_msg, args->wanted);
}

int cli_plugin_catalog_entry_read(const CliPluginCatalogErrMsgCfg *args)
{
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                    .node = args->entry_u32, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                   .json = args->json, .node = args->entry_u32, .wanted = "struct Entry", .err_msg = args->err_msg);
        return 0;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        const unsigned int field = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = args->entry_u32, .name = s_texts[at]);
        if ((field != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                    .json_read = args->json, .node = field, .kind = CFG_JSON_STRING))
        {
            EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                       .json = args->json, .node = field, .wanted = "a string", .err_msg = args->err_msg);
            return 0;
        }
    }
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned int field = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = args->entry_u32, .name = s_lists[at]);
        if ((field != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                    .json_read = args->json, .node = field, .kind = CFG_JSON_ARRAY))
        {
            EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                       .json = args->json, .node = field, .wanted = "a sequence", .err_msg = args->err_msg);
            return 0;
        }
        for (unsigned int item = 0u;
             (field != CLI_JSON_NONE) &&
             (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                         .array = field, .index = item) != CLI_JSON_NONE);
             item += 1u)
        {
            if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = args->json, .array = field, .index = item),
                            .kind = CFG_JSON_STRING))
            {
                EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->where,
                           .json = args->json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = args->json, .array = field, .index = item),
                           .wanted = "a string", .err_msg = args->err_msg);
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

// an entry written as serde writes it, a field it does not give written empty
EMBED_INLINE void catalog_entry_write(const PluginCatalogCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        mmgr_line_put(out, (at == 0u) ? "{\"" : ",\"");
        mmgr_line_put(out, s_texts[at]);
        mmgr_line_put(out, "\":");
        unsigned long long length = 0ull;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->entry, .name = s_texts[at]),
                       .length_pointer = &length);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = (text != NULL) ? text : "", .length = length);
    }
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        mmgr_line_put(out, ",\"");
        mmgr_line_put(out, s_lists[at]);
        mmgr_line_put(out, "\":");
        const unsigned int field = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->entry, .name = s_lists[at]);
        if (field == CLI_JSON_NONE)
        {
            mmgr_line_put(out, "[]");
        }
        else
        {
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = field,
                       .out = out, .indent = 0u, .sorted = 0);
        }
    }
    mmgr_line_ch(out, '}');
}

// whether an entry is one to list: an id that is its own slug, and a package
EMBED_INLINE int catalog_list(const PluginCatalogCtx *args)
{
    const CliJson *json = args->json;
    unsigned int entry = args->entry;
    const char *const id = EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg, .json = json,
                                      .entry_u32 = entry, .name = "id");
    char slug[CLI_NAME_MAX_LEN];
    if ((id[0] == '\0') || (cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) >= sizeof(slug)))
    {
        return 0;
    }
    EMBED_CALL(cli_plugins_id_name.slug, CliPluginsIdNameCfg, .name = id, .id_out = slug);
    return (cellul.eq(&(CatenaFinitaCfg){.src = slug, .other = id, .cap = CLI_CELLS_POOL})) &&
           (EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg, .json = json, .entry_u32 = entry,
                       .name = "package")[0] != '\0');
}

int cli_plugin_catalog_read(const CliPluginCatalogAddressCfg *args)
{
    if (!EMBED_CALL(cli_plugin_catalog_internal.lock, CliPluginCatalogCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    int read =
        EMBED_CALL(cli_plugin_catalog_address_internal.fetch, CliPluginCatalogAddressCtx, .address = args->address,
                   .purpose = "the plugin catalog", .size = &size, .err_msg = args->err_msg);
    unsigned int catalog = CLI_JSON_NONE;
    if (read)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea read_err_msg = {why, sizeof(why), 0ull};
        catalog = EMBED_CALL(cli_plugin_catalog_internal.tree, CliPluginCatalogCtx,
                             .text = (const char *)g_cli_catalog->bytes, .size_value = size, .err_msg = &read_err_msg);
        read = (catalog != CLI_JSON_NONE);
        if (!read)
        {
            mmgr_line_put(args->err_msg, args->address);
            mmgr_line_put(args->err_msg, ": ");
            mmgr_line_put_n(args->err_msg, why, read_err_msg.at);
        }
    }
    const char *const kind =
        read ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                          .json_read = &g_cli_catalog->json,
                          .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = &g_cli_catalog->json, .object = catalog, .name = "orior"),
                          .length_pointer = NULL)
             : NULL;
    if (read && ((kind == NULL) ||
                 (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = CLI_PLUGIN_CATALOG, .cap = CLI_CELLS_POOL}))))
    {
        mmgr_line_put(args->err_msg, args->address);
        mmgr_line_put(args->err_msg, " is no catalog of orior's plugins");
        read = 0;
    }
    const unsigned int plugins =
        read ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &g_cli_catalog->json,
                          .object = catalog, .name = "plugins")
             : CLI_JSON_NONE;
    if (read && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &g_cli_catalog->json,
                            .node = plugins, .kind = CFG_JSON_ARRAY))
    {
        EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->address,
                   .json = &g_cli_catalog->json, .node = plugins, .wanted = "a sequence", .err_msg = args->err_msg);
        read = 0;
    }
    for (unsigned int at = 0u;
         read && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                             .json_read = &g_cli_catalog->json, .array = plugins, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        read =
            EMBED_CALL(cli_plugin_catalog_err_msg.entry_read, CliPluginCatalogErrMsgCfg, .json = &g_cli_catalog->json,
                       .entry_u32 = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = &g_cli_catalog->json, .array = plugins, .index = at),
                       .where = args->address, .err_msg = args->err_msg);
    }
    if (read)
    {
        mmgr_line_ch(args->out, '[');
        int first = 1;
        for (unsigned int at = 0u;
             EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                        .json_read = &g_cli_catalog->json, .array = plugins, .index = at) != CLI_JSON_NONE;
             at += 1u)
        {
            const unsigned int entry = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                  .json_read = &g_cli_catalog->json, .array = plugins, .index = at);
            if (EMBED_CALL(catalog_list, PluginCatalogCtx, .json = &g_cli_catalog->json, .entry = entry))
            {
                mmgr_line_put(args->out, first ? "" : ",");
                EMBED_CALL(catalog_entry_write, PluginCatalogCtx, .json = &g_cli_catalog->json, .entry = entry,
                           .out = args->out);
                first = 0;
            }
        }
        mmgr_line_ch(args->out, ']');
    }
    cli_plugin_catalog_release();
    return read;
}
