// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_install.c: a plugin of a catalog installed: its package fetched, its hash checked where the
// catalog gives one, its plugin.json checked against the entry's id and version, and its files written into a folder of
// its own beside the one they take the place of, then moved into place
#include "../text/digest.h"
#include "plugin_catalog_internal.h"

#include "../text/text.h"

// whether a file's name in a package stays inside its plugin's folder: no part of it empty, `.` or `..`
static int catalog_inside(const char *name)
{
    if (name[0] == '\0')
    {
        return 0;
    }
    for (const char *part = name;; part += 1)
    {
        const size_t length = cli_text_upto(part, "/\\");
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
static unsigned int catalog_package(const char *from, const char *id, const char *version, const char *sha256,
                                    VerbaLinea *err_msg)
{
    char purpose[CLI_NAME_MAX_LEN + 16u];
    VerbaLinea purpose_line = {purpose, sizeof(purpose), 0ull};
    mmgr_line_put(&purpose_line, "the plugin ");
    mmgr_line_put(&purpose_line, id);
    mmgr_line_finish(&purpose_line);
    unsigned long long size = 0ull;
    if (!cli_plugin_catalog_fetch(from, purpose, &size, err_msg))
    {
        return CLI_JSON_NONE;
    }
    char digest[65];
    VerbaLinea digest_line = {digest, sizeof(digest), 0ull};
    cli_sha256_text(g_cli_catalog->bytes, size, &digest_line);
    mmgr_line_finish(&digest_line);
    int same = (cellul.len(&(CatenaFinitaCfg){.src = sha256, .cap = CLI_CELLS_POOL}) == 64u);
    for (unsigned int at = 0u; same && (at < 64u); at += 1u)
    {
        const char letter = ((sha256[at] >= 'A') && (sha256[at] <= 'F')) ? (char)(sha256[at] - 'A' + 'a') : sha256[at];
        same = (letter == digest[at]);
    }
    if ((sha256[0] != '\0') && !same)
    {
        mmgr_line_put(err_msg, from);
        mmgr_line_put(err_msg, " is not the package the catalog names: its hash differs");
        return CLI_JSON_NONE;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {why, sizeof(why), 0ull};
    const unsigned int package = cli_plugin_catalog_tree((const char *)g_cli_catalog->bytes, size, &read_err_msg);
    const char *const kind =
        cli_json_string(&g_cli_catalog->json, cli_json_member(&g_cli_catalog->json, package, "orior"), NULL);
    const unsigned int files = cli_json_member(&g_cli_catalog->json, package, "files");
    if (package == CLI_JSON_NONE)
    {
        mmgr_line_put(err_msg, from);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, read_err_msg.at);
        return CLI_JSON_NONE;
    }
    if ((kind == NULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = CLI_PLUGIN_PACKAGE, .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(err_msg, from);
        mmgr_line_put(err_msg, " is no package of an orior plugin");
        return CLI_JSON_NONE;
    }
    if (!cli_json_is(&g_cli_catalog->json, files, CFG_JSON_OBJECT))
    {
        cli_plugin_catalog_invalid(from, &g_cli_catalog->json, files, "a map", err_msg);
        return CLI_JSON_NONE;
    }
    for (unsigned int file = g_cli_catalog->json.node[files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        if (!cli_json_is(&g_cli_catalog->json, file, CFG_JSON_STRING))
        {
            cli_plugin_catalog_invalid(from, &g_cli_catalog->json, file, "a string", err_msg);
            return CLI_JSON_NONE;
        }
    }
    unsigned long long own_size = 0ull;
    const char *const own_text =
        cli_json_string(&g_cli_catalog->json, cli_json_member(&g_cli_catalog->json, files, "plugin.json"), &own_size);
    if (own_text == NULL)
    {
        mmgr_line_put(err_msg, from);
        mmgr_line_put(err_msg, " contains no plugin.json");
        return CLI_JSON_NONE;
    }
    read_err_msg.at = 0ull;
    CliJson own_json;
    cli_json_begun(&own_json, NULL, 0u, NULL, 0ull);
    const unsigned int own =
        cli_json_taken(&own_json, own_text, own_size, 0u, 0ull, &read_err_msg, &g_cli_plugins_cells);
    if (own == CLI_JSON_NONE)
    {
        mmgr_line_put(err_msg, from);
        mmgr_line_put(err_msg, ": plugin.json: ");
        mmgr_line_put_n(err_msg, why, read_err_msg.at);
        return CLI_JSON_NONE;
    }
    const char *const own_id = cli_plugin_catalog_field(&own_json, own, "id");
    const char *const own_version = cli_plugin_catalog_field(&own_json, own, "version");
    const int matched = (cellul.eq(&(CatenaFinitaCfg){.src = own_id, .other = id, .cap = CLI_CELLS_POOL})) &&
                        (cellul.eq(&(CatenaFinitaCfg){.src = own_version, .other = version, .cap = CLI_CELLS_POOL}));
    if (!matched)
    {
        mmgr_line_put(err_msg, from);
        mmgr_line_put(err_msg, " contains ");
        mmgr_line_put(err_msg, own_id);
        mmgr_line_ch(err_msg, ' ');
        mmgr_line_put(err_msg, own_version);
        mmgr_line_put(err_msg, ", and the catalog names ");
        mmgr_line_put(err_msg, id);
        mmgr_line_ch(err_msg, ' ');
        mmgr_line_put(err_msg, version);
    }
    cli_json_given(&own_json);
    if (!matched)
    {
        return CLI_JSON_NONE;
    }
    for (unsigned int file = g_cli_catalog->json.node[files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        if (!catalog_inside(g_cli_catalog->json.text + g_cli_catalog->json.node[file].key))
        {
            mmgr_line_put(err_msg, from);
            mmgr_line_put(err_msg, " names a file outside its plugin's folder");
            return CLI_JSON_NONE;
        }
    }
    return files;
}

// the files of the package written into `staged`, each folder above one made: 1, or 0 with why given
static int catalog_staged(const char *staged, unsigned int files, VerbaLinea *err_msg)
{
    for (unsigned int file = g_cli_catalog->json.node[files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        char path[CLI_PATH_MAX_LEN];
        char parent[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ull};
        mmgr_line_put(&line, staged);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, g_cli_catalog->json.text + g_cli_catalog->json.node[file].key);
        const int fits = (mmgr_line_finish(&line) != 0ull);
        if (!fits || !cli_path_parent(path, parent) || !query_folder_made(parent))
        {
            mmgr_line_put(err_msg, staged);
            mmgr_line_put(err_msg, ": it could not be made");
            return 0;
        }
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(&g_cli_catalog->json, file, &size);
        if (!query_file_written(path, text, size))
        {
            mmgr_line_put(err_msg, path);
            mmgr_line_put(err_msg, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

int cli_plugin_catalog_install_into(const char *plugins, const char *address, const char *entry,
                                    unsigned long long size, VerbaLinea *out, VerbaLinea *err_msg)
{
    if (!cli_plugin_catalog_locked(err_msg))
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {why, sizeof(why), 0ull};
    const unsigned int node = cli_plugin_catalog_tree(entry, size, &read_err_msg);
    if (node == CLI_JSON_NONE)
    {
        mmgr_line_put_n(err_msg, why, read_err_msg.at);
    }
    int done =
        (node != CLI_JSON_NONE) && cli_plugin_catalog_entry_read(&g_cli_catalog->json, node, "the entry", err_msg);
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
        mmgr_line_put(&id_line, cli_plugin_catalog_field(&g_cli_catalog->json, node, "id"));
        mmgr_line_put(&version_line, cli_plugin_catalog_field(&g_cli_catalog->json, node, "version"));
        mmgr_line_put(&sha_line, cli_plugin_catalog_field(&g_cli_catalog->json, node, "sha256"));
        cli_plugin_catalog_beside(address, cli_plugin_catalog_field(&g_cli_catalog->json, node, "package"), &from_line);
        // an entry may name no hash: each text is accepted by whether it fit, not by its length
        done = mmgr_line_ok(&id_line) && mmgr_line_ok(&version_line) && mmgr_line_ok(&sha_line) &&
               mmgr_line_ok(&from_line);
        (void)mmgr_line_finish(&id_line);
        (void)mmgr_line_finish(&version_line);
        (void)mmgr_line_finish(&sha_line);
        (void)mmgr_line_finish(&from_line);
        if (!done)
        {
            mmgr_line_put(err_msg, "the entry is larger than orior_cli supports");
        }
    }
    const unsigned int files = done ? catalog_package(from, id, version, sha256, err_msg) : CLI_JSON_NONE;
    char folder[CLI_PATH_MAX_LEN];
    char staged[CLI_PATH_MAX_LEN];
    VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
    VerbaLinea staged_line = {staged, sizeof(staged), 0ull};
    mmgr_line_put(&folder_line, plugins);
    mmgr_line_ch(&folder_line, '/');
    mmgr_line_put(&folder_line, id);
    mmgr_line_put(&staged_line, plugins);
    mmgr_line_put(&staged_line, "/.");
    mmgr_line_put(&staged_line, id);
    mmgr_line_put(&staged_line, ".installing");
    done = (files != CLI_JSON_NONE) && (mmgr_line_finish(&folder_line) != 0ull) &&
           (mmgr_line_finish(&staged_line) != 0ull);
    if ((files != CLI_JSON_NONE) && !done)
    {
        mmgr_line_put(err_msg, plugins);
        mmgr_line_put(err_msg, ": its path is too long");
    }
    if (done)
    {
        (void)query_removed(staged);
        done = catalog_staged(staged, files, err_msg);
    }
    if (done)
    {
        (void)query_removed(folder);
        done = cli_moved(staged, folder);
        if (!done)
        {
            mmgr_line_put(err_msg, folder);
            mmgr_line_put(err_msg, ": it could not be made");
        }
    }
    if (done)
    {
        mmgr_line_put(out, folder);
    }
    cli_plugin_catalog_released();
    return done;
}

int cli_plugin_catalog_install(const char *address, const char *entry, unsigned long long size, VerbaLinea *out,
                               VerbaLinea *err_msg)
{
    char plugins[CLI_PATH_MAX_LEN];
    if (!cli_home_plugins(plugins))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    return cli_plugin_catalog_install_into(plugins, address, entry, size, out, err_msg);
}
