// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_install.c: a plugin of a catalog installed: its package fetched, its hash checked where the
// catalog gives one, its plugin.json checked against the entry's id and version, and its files written into a folder of
// its own beside the one they take the place of, then moved into place
#include "../text/digest.h"
#include "plugin_catalog_internal.h"

#include "../text/text.h"

typedef struct
{
    const char *name;
    const char *from;
    const char *id;
    const char *version;
    const char *sha256;
    VerbaLinea *err_msg;
    const char *staged;
    unsigned int files;
} PluginCatalogInstallCtx;

// whether a file's name in a package stays inside its plugin's folder: no part of it empty, `.` or `..`
EMBED_INLINE int catalog_inside(const PluginCatalogInstallCtx *args)
{
    if (args->name[0] == '\0')
    {
        return 0;
    }
    for (const char *part = args->name;; part += 1)
    {
        const size_t length = EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = part, .set = "/\\");
        if ((length == 0u) || ((length == 1u) && (part[0] == '.')) ||
            ((length == 2u) && (part[0] == '.') && (part[1] == '.')))
        {
            return 0;
        }
        part += length;
        if (*part == '\0')
        {
            return 1;
        }
    }
}

// the package at `from` read and checked against the entry, the lock acquired: its files' object, or CLI_JSON_NONE with
// why given
EMBED_INLINE unsigned int catalog_package(const PluginCatalogInstallCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char purpose[CLI_NAME_MAX_LEN + 16u];
    VerbaLinea purpose_line = {purpose, sizeof(purpose), 0ull};
    mmgr_line_put(&purpose_line, "the plugin ");
    mmgr_line_put(&purpose_line, args->id);
    mmgr_line_finish(&purpose_line);
    unsigned long long size = 0ull;
    if (!EMBED_CALL(cli_plugin_catalog_address_internal.fetch, CliPluginCatalogAddressCtx, .address = args->from,
                    .purpose = purpose, .size = &size, .err_msg = err_msg))
    {
        return CLI_JSON_NONE;
    }
    char digest[65];
    VerbaLinea digest_line = {digest, sizeof(digest), 0ull};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = g_cli_catalog->bytes, .size = size,
               .text = &digest_line);
    mmgr_line_finish(&digest_line);
    int same = (cellul.len(&(CatenaFinitaCfg){.src = args->sha256, .cap = CLI_CELLS_POOL}) == 64u);
    for (unsigned int at = 0u; same && (at < 64u); at += 1u)
    {
        const char letter = ((args->sha256[at] >= 'A') && (args->sha256[at] <= 'F'))
                                ? (char)(args->sha256[at] - 'A' + 'a')
                                : args->sha256[at];
        same = (letter == digest[at]);
    }
    if ((args->sha256[0] != '\0') && !same)
    {
        mmgr_line_put(err_msg, args->from);
        mmgr_line_put(err_msg, " is not the package the catalog names: its hash differs");
        return CLI_JSON_NONE;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {why, sizeof(why), 0ull};
    const unsigned int package =
        EMBED_CALL(cli_plugin_catalog_internal.tree, CliPluginCatalogCtx, .text = (const char *)g_cli_catalog->bytes,
                   .size_value = size, .err_msg = &read_err_msg);
    const char *const kind =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &g_cli_catalog->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &g_cli_catalog->json, .object = package, .name = "orior"),
                   .length_pointer = NULL);
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &g_cli_catalog->json, .object = package, .name = "files");
    if (package == CLI_JSON_NONE)
    {
        mmgr_line_put(err_msg, args->from);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, read_err_msg.at);
        return CLI_JSON_NONE;
    }
    if ((kind == NULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = CLI_PLUGIN_PACKAGE, .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(err_msg, args->from);
        mmgr_line_put(err_msg, " is no package of an orior plugin");
        return CLI_JSON_NONE;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &g_cli_catalog->json,
                    .node = files, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->from,
                   .json = &g_cli_catalog->json, .node = files, .wanted = "a map", .err_msg = err_msg);
        return CLI_JSON_NONE;
    }
    for (unsigned int file = g_cli_catalog->json.node[files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &g_cli_catalog->json,
                        .node = file, .kind = CFG_JSON_STRING))
        {
            EMBED_CALL(cli_plugin_catalog_where_internal.invalid, CliPluginCatalogWhereCtx, .where = args->from,
                       .json = &g_cli_catalog->json, .node = file, .wanted = "a string", .err_msg = err_msg);
            return CLI_JSON_NONE;
        }
    }
    unsigned long long own_size = 0ull;
    const char *const own_text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &g_cli_catalog->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &g_cli_catalog->json, .object = files, .name = "plugin.json"),
                   .length_pointer = &own_size);
    if (own_text == NULL)
    {
        mmgr_line_put(err_msg, args->from);
        mmgr_line_put(err_msg, " contains no plugin.json");
        return CLI_JSON_NONE;
    }
    read_err_msg.at = 0ull;
    CliJson own_json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &own_json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    const unsigned int own = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                        .json = &own_json, .text = own_text, .size = own_size, .nodes_more = 0u,
                                        .text_more = 0ull, .err_msg = &read_err_msg, .owner = &g_cli_plugins_cells);
    if (own == CLI_JSON_NONE)
    {
        mmgr_line_put(err_msg, args->from);
        mmgr_line_put(err_msg, ": plugin.json: ");
        mmgr_line_put_n(err_msg, why, read_err_msg.at);
        return CLI_JSON_NONE;
    }
    const char *const own_id = EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg, .json = &own_json,
                                          .entry_u32 = own, .name = "id");
    const char *const own_version = EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg,
                                               .json = &own_json, .entry_u32 = own, .name = "version");
    const int matched =
        (cellul.eq(&(CatenaFinitaCfg){.src = own_id, .other = args->id, .cap = CLI_CELLS_POOL})) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = own_version, .other = args->version, .cap = CLI_CELLS_POOL}));
    if (!matched)
    {
        mmgr_line_put(err_msg, args->from);
        mmgr_line_put(err_msg, " contains ");
        mmgr_line_put(err_msg, own_id);
        mmgr_line_ch(err_msg, ' ');
        mmgr_line_put(err_msg, own_version);
        mmgr_line_put(err_msg, ", and the catalog names ");
        mmgr_line_put(err_msg, args->id);
        mmgr_line_ch(err_msg, ' ');
        mmgr_line_put(err_msg, args->version);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &own_json);
    if (!matched)
    {
        return CLI_JSON_NONE;
    }
    for (unsigned int file = g_cli_catalog->json.node[files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        if (!EMBED_CALL(catalog_inside, PluginCatalogInstallCtx,
                        .name = g_cli_catalog->json.text + g_cli_catalog->json.node[file].key))
        {
            mmgr_line_put(err_msg, args->from);
            mmgr_line_put(err_msg, " names a file outside its plugin's folder");
            return CLI_JSON_NONE;
        }
    }
    return files;
}

// the files of the package written into `staged`, each folder above one made: 1, or 0 with why given
EMBED_INLINE int catalog_stage(const PluginCatalogInstallCtx *args)
{
    for (unsigned int file = g_cli_catalog->json.node[args->files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        char path[CLI_PATH_MAX_LEN];
        char parent[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ull};
        mmgr_line_put(&line, args->staged);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, g_cli_catalog->json.text + g_cli_catalog->json.node[file].key);
        const int fits = (mmgr_line_finish(&line) != 0ull);
        if (!fits ||
            !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = parent) ||
            !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent))
        {
            mmgr_line_put(args->err_msg, args->staged);
            mmgr_line_put(args->err_msg, ": it could not be made");
            return 0;
        }
        unsigned long long size = 0ull;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &g_cli_catalog->json, .node = file, .length_pointer = &size);
        if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                        .size = size))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

int cli_plugin_catalog_install_into(const CliPluginCatalogAddressCfg *args)
{
    if (!EMBED_CALL(cli_plugin_catalog_internal.lock, CliPluginCatalogCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {why, sizeof(why), 0ull};
    const unsigned int node = EMBED_CALL(cli_plugin_catalog_internal.tree, CliPluginCatalogCtx, .text = args->entry,
                                         .size_value = args->size, .err_msg = &read_err_msg);
    if (node == CLI_JSON_NONE)
    {
        mmgr_line_put_n(args->err_msg, why, read_err_msg.at);
    }
    int done = (node != CLI_JSON_NONE) && EMBED_CALL(cli_plugin_catalog_err_msg.entry_read, CliPluginCatalogErrMsgCfg,
                                                     .json = &g_cli_catalog->json, .entry_u32 = node,
                                                     .where = "the entry", .err_msg = args->err_msg);
    // the entry's texts are stored apart, since the package is read into the same tree
    char id[CLI_NAME_MAX_LEN];
    char version[CLI_NAME_MAX_LEN];
    char sha256[CLI_NAME_MAX_LEN];
    char from[CLI_PATH_MAX_LEN];
    VerbaLinea id_line = {id, sizeof(id), 0ull};
    VerbaLinea version_line = {version, sizeof(version), 0ull};
    VerbaLinea sha_line = {sha256, sizeof(sha256), 0ull};
    VerbaLinea from_line = {from, sizeof(from), 0ull};
    if (done)
    {
        mmgr_line_put(&id_line, EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg,
                                           .json = &g_cli_catalog->json, .entry_u32 = node, .name = "id"));
        mmgr_line_put(&version_line, EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg,
                                                .json = &g_cli_catalog->json, .entry_u32 = node, .name = "version"));
        mmgr_line_put(&sha_line, EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg,
                                            .json = &g_cli_catalog->json, .entry_u32 = node, .name = "sha256"));
        EMBED_CALL(cli_plugin_catalog.beside, CliPluginCatalogCfg, .base = args->address,
                   .address = EMBED_CALL(cli_plugin_catalog_json.field, CliPluginCatalogJsonCfg,
                                         .json = &g_cli_catalog->json, .entry_u32 = node, .name = "package"),
                   .out = &from_line);
        // an entry may name no hash: each text is accepted by whether it fit, not by its length
        done = mmgr_line_ok(&id_line) && mmgr_line_ok(&version_line) && mmgr_line_ok(&sha_line) &&
               mmgr_line_ok(&from_line);
        (void)mmgr_line_finish(&id_line);
        (void)mmgr_line_finish(&version_line);
        (void)mmgr_line_finish(&sha_line);
        (void)mmgr_line_finish(&from_line);
        if (!done)
        {
            mmgr_line_put(args->err_msg, "the entry is larger than orior_cli supports");
        }
    }
    const unsigned int files = done ? EMBED_CALL(catalog_package, PluginCatalogInstallCtx, .from = from, .id = id,
                                                 .version = version, .sha256 = sha256, .err_msg = args->err_msg)
                                    : CLI_JSON_NONE;
    char folder[CLI_PATH_MAX_LEN];
    char staged[CLI_PATH_MAX_LEN];
    VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
    VerbaLinea staged_line = {staged, sizeof(staged), 0ull};
    mmgr_line_put(&folder_line, args->plugins);
    mmgr_line_ch(&folder_line, '/');
    mmgr_line_put(&folder_line, id);
    mmgr_line_put(&staged_line, args->plugins);
    mmgr_line_put(&staged_line, "/.");
    mmgr_line_put(&staged_line, id);
    mmgr_line_put(&staged_line, ".installing");
    done = (files != CLI_JSON_NONE) && (mmgr_line_finish(&folder_line) != 0ull) &&
           (mmgr_line_finish(&staged_line) != 0ull);
    if ((files != CLI_JSON_NONE) && !done)
    {
        mmgr_line_put(args->err_msg, args->plugins);
        mmgr_line_put(args->err_msg, ": its path is too long");
    }
    if (done)
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = staged);
        done = EMBED_CALL(catalog_stage, PluginCatalogInstallCtx, .staged = staged, .files = files,
                          .err_msg = args->err_msg);
    }
    if (done)
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = folder);
        done = EMBED_CALL(cli_err_msg_from_internal.move, CliErrMsgFromCtx, .from_text = staged, .to = folder);
        if (!done)
        {
            mmgr_line_put(args->err_msg, folder);
            mmgr_line_put(args->err_msg, ": it could not be made");
        }
    }
    if (done)
    {
        mmgr_line_put(args->out, folder);
    }
    cli_plugin_catalog_release();
    return done;
}

int cli_plugin_catalog_install(const CliPluginCatalogAddressCfg *args)
{
    char plugins[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = plugins))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    return EMBED_CALL(cli_plugin_catalog_address.install_into, CliPluginCatalogAddressCfg, .plugins = plugins,
                      .address = args->address, .entry = args->entry, .size = args->size, .out = args->out,
                      .err_msg = args->err_msg);
}
