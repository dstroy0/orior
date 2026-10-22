// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_install.c: a plugin of a catalog installed: its package fetched, its hash checked where the
// catalog gives one, its plugin.json held to the entry's id and version, and its files written into a folder of its
// own beside the one they take the place of, then moved into place
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

// the package at `from` read and checked against the entry, the lock held: its files' object, or CLI_JSON_NONE with
// why said
static unsigned int catalog_package(const char *from, const char *id, const char *version, const char *sha256,
                                    CliLine *said)
{
    char purpose[CLI_NAME_LONGEST + 16u];
    CliLine purpose_line = {purpose, sizeof(purpose), 0ull};
    cli_line_text(&purpose_line, "the plugin ");
    cli_line_text(&purpose_line, id);
    cli_line_closed(&purpose_line);
    unsigned long long size = 0ull;
    if (!cli_plugin_catalog_fetch(from, purpose, &size, said))
    {
        return CLI_JSON_NONE;
    }
    char digest[65];
    CliLine digest_line = {digest, sizeof(digest), 0ull};
    cli_sha256_text(g_cli_catalog->bytes, size, &digest_line);
    cli_line_closed(&digest_line);
    int same = (cellul.len(&(CatenaFinitaCfg){.src = sha256, .cap = CLI_CELLS_POOL}) == 64u);
    for (unsigned int at = 0u; same && (at < 64u); at += 1u)
    {
        const char letter = ((sha256[at] >= 'A') && (sha256[at] <= 'F')) ? (char)(sha256[at] - 'A' + 'a') : sha256[at];
        same = (letter == digest[at]);
    }
    if ((sha256[0] != '\0') && !same)
    {
        cli_line_text(said, from);
        cli_line_text(said, " is not the package the catalog names: its hash differs");
        return CLI_JSON_NONE;
    }
    char why[CLI_SAID_LONGEST];
    CliLine read_said = {why, sizeof(why), 0ull};
    const unsigned int package = cli_plugin_catalog_tree((const char *)g_cli_catalog->bytes, size, &read_said);
    const char *const kind =
        cli_json_string(&g_cli_catalog->json, cli_json_member(&g_cli_catalog->json, package, "orior"), NULL);
    const unsigned int files = cli_json_member(&g_cli_catalog->json, package, "files");
    if (package == CLI_JSON_NONE)
    {
        cli_line_text(said, from);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, read_said.at);
        return CLI_JSON_NONE;
    }
    if ((kind == NULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = CLI_PLUGIN_PACKAGE, .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, from);
        cli_line_text(said, " is no package of an orior plugin");
        return CLI_JSON_NONE;
    }
    if (!cli_json_is(&g_cli_catalog->json, files, CFG_JSON_OBJECT))
    {
        cli_plugin_catalog_invalid(from, &g_cli_catalog->json, files, "a map", said);
        return CLI_JSON_NONE;
    }
    for (unsigned int file = g_cli_catalog->json.node[files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        if (!cli_json_is(&g_cli_catalog->json, file, CFG_JSON_STRING))
        {
            cli_plugin_catalog_invalid(from, &g_cli_catalog->json, file, "a string", said);
            return CLI_JSON_NONE;
        }
    }
    unsigned long long own_size = 0ull;
    const char *const own_text =
        cli_json_string(&g_cli_catalog->json, cli_json_member(&g_cli_catalog->json, files, "plugin.json"), &own_size);
    if (own_text == NULL)
    {
        cli_line_text(said, from);
        cli_line_text(said, " holds no plugin.json");
        return CLI_JSON_NONE;
    }
    read_said.at = 0ull;
    CliJson own_json;
    cli_json_begun(&own_json, NULL, 0u, NULL, 0ull);
    const unsigned int own = cli_json_taken(&own_json, own_text, own_size, 0u, 0ull, &read_said);
    if (own == CLI_JSON_NONE)
    {
        cli_line_text(said, from);
        cli_line_text(said, ": plugin.json: ");
        cli_line_counted(said, why, read_said.at);
        return CLI_JSON_NONE;
    }
    const char *const own_id = cli_plugin_catalog_field(&own_json, own, "id");
    const char *const own_version = cli_plugin_catalog_field(&own_json, own, "version");
    const int matched = (cellul.eq(&(CatenaFinitaCfg){.src = own_id, .other = id, .cap = CLI_CELLS_POOL})) &&
                        (cellul.eq(&(CatenaFinitaCfg){.src = own_version, .other = version, .cap = CLI_CELLS_POOL}));
    if (!matched)
    {
        cli_line_text(said, from);
        cli_line_text(said, " holds ");
        cli_line_text(said, own_id);
        cli_line_character(said, ' ');
        cli_line_text(said, own_version);
        cli_line_text(said, ", and the catalog names ");
        cli_line_text(said, id);
        cli_line_character(said, ' ');
        cli_line_text(said, version);
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
            cli_line_text(said, from);
            cli_line_text(said, " names a file outside its plugin's folder");
            return CLI_JSON_NONE;
        }
    }
    return files;
}

// the files of the package written into `staged`, each folder above one made: 1, or 0 with why said
static int catalog_staged(const char *staged, unsigned int files, CliLine *said)
{
    for (unsigned int file = g_cli_catalog->json.node[files].first; file != CLI_JSON_NONE;
         file = g_cli_catalog->json.node[file].next)
    {
        char path[CLI_PATH_LONGEST];
        char parent[CLI_PATH_LONGEST];
        CliLine line = {path, sizeof(path), 0ull};
        cli_line_text(&line, staged);
        cli_line_character(&line, '/');
        cli_line_text(&line, g_cli_catalog->json.text + g_cli_catalog->json.node[file].key);
        const int fits = (cli_line_closed(&line) != 0ull);
        if (!fits || !cli_path_parent(path, parent) || !query_folder_made(parent))
        {
            cli_line_text(said, staged);
            cli_line_text(said, ": it could not be made");
            return 0;
        }
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(&g_cli_catalog->json, file, &size);
        if (!query_file_written(path, text, size))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

int cli_plugin_catalog_install_into(const char *plugins, const char *address, const char *entry,
                                    unsigned long long size, CliLine *out, CliLine *said)
{
    if (!cli_plugin_catalog_held(said))
    {
        return 0;
    }
    char why[CLI_SAID_LONGEST];
    CliLine read_said = {why, sizeof(why), 0ull};
    const unsigned int node = cli_plugin_catalog_tree(entry, size, &read_said);
    if (node == CLI_JSON_NONE)
    {
        cli_line_counted(said, why, read_said.at);
    }
    int done = (node != CLI_JSON_NONE) && cli_plugin_catalog_entry_read(&g_cli_catalog->json, node, "the entry", said);
    // the entry's texts are kept apart, since the package is read into the same tree
    char id[CLI_NAME_LONGEST];
    char version[CLI_NAME_LONGEST];
    char sha256[CLI_NAME_LONGEST];
    char from[CLI_PATH_LONGEST];
    CliLine id_line = {id, sizeof(id), 0ull};
    CliLine version_line = {version, sizeof(version), 0ull};
    CliLine sha_line = {sha256, sizeof(sha256), 0ull};
    CliLine from_line = {from, sizeof(from), 0ull};
    if (done)
    {
        cli_line_text(&id_line, cli_plugin_catalog_field(&g_cli_catalog->json, node, "id"));
        cli_line_text(&version_line, cli_plugin_catalog_field(&g_cli_catalog->json, node, "version"));
        cli_line_text(&sha_line, cli_plugin_catalog_field(&g_cli_catalog->json, node, "sha256"));
        cli_plugin_catalog_beside(address, cli_plugin_catalog_field(&g_cli_catalog->json, node, "package"), &from_line);
        // an entry may name no hash, so each text is held by whether it fit, not by its length
        done = cli_line_fits(&id_line) && cli_line_fits(&version_line) && cli_line_fits(&sha_line) &&
               cli_line_fits(&from_line);
        (void)cli_line_closed(&id_line);
        (void)cli_line_closed(&version_line);
        (void)cli_line_closed(&sha_line);
        (void)cli_line_closed(&from_line);
        if (!done)
        {
            cli_line_text(said, "the entry is larger than orior_cli holds");
        }
    }
    const unsigned int files = done ? catalog_package(from, id, version, sha256, said) : CLI_JSON_NONE;
    char folder[CLI_PATH_LONGEST];
    char staged[CLI_PATH_LONGEST];
    CliLine folder_line = {folder, sizeof(folder), 0ull};
    CliLine staged_line = {staged, sizeof(staged), 0ull};
    cli_line_text(&folder_line, plugins);
    cli_line_character(&folder_line, '/');
    cli_line_text(&folder_line, id);
    cli_line_text(&staged_line, plugins);
    cli_line_text(&staged_line, "/.");
    cli_line_text(&staged_line, id);
    cli_line_text(&staged_line, ".installing");
    done =
        (files != CLI_JSON_NONE) && (cli_line_closed(&folder_line) != 0ull) && (cli_line_closed(&staged_line) != 0ull);
    if ((files != CLI_JSON_NONE) && !done)
    {
        cli_line_text(said, plugins);
        cli_line_text(said, ": its path is too long");
    }
    if (done)
    {
        (void)query_removed(staged);
        done = catalog_staged(staged, files, said);
    }
    if (done)
    {
        (void)query_removed(folder);
        done = cli_moved(staged, folder);
        if (!done)
        {
            cli_line_text(said, folder);
            cli_line_text(said, ": it could not be made");
        }
    }
    if (done)
    {
        cli_line_text(out, folder);
    }
    cli_plugin_catalog_released();
    return done;
}

int cli_plugin_catalog_install(const char *address, const char *entry, unsigned long long size, CliLine *out,
                               CliLine *said)
{
    char plugins[CLI_PATH_LONGEST];
    if (!cli_home_plugins(plugins))
    {
        cli_line_text(said, "orior_cli has no folder of its own");
        return 0;
    }
    return cli_plugin_catalog_install_into(plugins, address, entry, size, out, said);
}
