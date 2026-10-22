// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog.c: plugins others publish, found in a catalog and installed from File, Plugins. A catalog is a JSON
// file at an address the reader names, on the web or on this machine:
//
//   { "orior": "plugin catalog", "plugins": [ { "id", "name", "version", "about", "package",
//     "sha256", "reads", "writes", "reaches" }, ... ] }
//
// and each plugin's package a JSON file, at `package`, an address of its own or one from the catalog's, holding the
// plugin's files by name:
//
//   { "orior": "plugin", "files": { "plugin.json": "...", "plugin.js": "..." } }
//
// A package whose catalog gives its `sha256` is installed only where its bytes hash to it, and one whose plugin.json
// names another id or version than its catalog's entry is not installed. What a plugin reads, writes and reaches on
// the network is what its own plugin.json says (plugins.h); the window holds it to that. Nothing is read from a
// catalog or installed but when the reader asks, and a plugin is updated only by installing it again
#include "plugin_catalog_internal.h"

#include "../machine/net.h"
#include "../requests/requests.h"
#include "plugins.h"

#include "../text/pieces.h"
#include "../text/text.h"

// the bytes of a piece a fetched body is written in before the piece is held in a cell of its own
#define CLI_PLUGIN_CATALOG_PIECE 4096u

CliCatalogRoom *g_cli_catalog;
static CliCatalogRoom s_catalog_room;
static CliLock s_catalog_lock = CLI_LOCK_INIT;

// an entry's fields that are texts, and those that are lists of texts, in the order serde writes them
static const char *const s_texts[6] = {"id", "name", "version", "about", "package", "sha256"};
static const char *const s_lists[3] = {"reads", "writes", "reaches"};

int cli_plugin_catalog_held(CliLine *said)
{
    (void)said;
    cli_lock_take(&s_catalog_lock);
    g_cli_catalog = &s_catalog_room;
    g_cli_catalog->bytes = NULL;
    cli_json_begun(&g_cli_catalog->json, NULL, 0u, NULL, 0ull);
    return 1;
}

void cli_plugin_catalog_released(void)
{
    cli_json_given(&g_cli_catalog->json);
    cli_cell_given(g_cli_catalog->bytes);
    g_cli_catalog->bytes = NULL;
    g_cli_catalog = NULL;
    cli_lock_give(&s_catalog_lock);
}

unsigned int cli_plugin_catalog_tree(const char *text, unsigned long long size, CliLine *said)
{
    cli_json_given(&g_cli_catalog->json);
    return cli_json_taken(&g_cli_catalog->json, text, size, 0u, 0ull, said);
}

// whether `text` begins with `prefix`
static int catalog_begins(const char *text, const char *prefix)
{
    return cli_text_same(text, prefix, cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL}));
}

// the bytes at `address`, a web address or a file of this machine, asked for `purpose`, read into g_cli_catalog->bytes,
// their count into `size`: 1, or 0 with why said
int cli_plugin_catalog_fetch(const char *address, const char *purpose, unsigned long long *size, CliLine *said)
{
    cli_cell_given(g_cli_catalog->bytes);
    g_cli_catalog->bytes = NULL;
    if (catalog_begins(address, "http://") || catalog_begins(address, "https://"))
    {
        if (!cli_net_reach(address, purpose, said))
        {
            return 0;
        }
        char why[CLI_SAID_LONGEST];
        CliLine failed = {why, sizeof(why), 0ull};
        char reason[CLI_SAID_LONGEST];
        CliLine reason_line = {reason, sizeof(reason), 0ull};
        CliPieces pieces;
        CliLine body;
        char piece[CLI_PLUGIN_CATALOG_PIECE];
        cli_pieces_begun(&pieces, &body, piece, sizeof(piece));
        unsigned int status = 0u;
        int cut = 0;
        const int got = cli_requests_get(address, &status, &reason_line, &cut, &body, &failed);
        unsigned long long joined_size = 0ull;
        // a body's bytes are held as its text
        unsigned char *const joined =
            (unsigned char *)cli_pieces_joined(&pieces, &body, "the bytes a plugin catalog fetched", &joined_size);
        if (!got)
        {
            cli_cell_given(joined);
            cli_line_text(said, address);
            cli_line_text(said, ": ");
            cli_line_counted(said, why, failed.at);
            return 0;
        }
        if ((status < 200u) || (status >= 300u))
        {
            cli_cell_given(joined);
            cli_line_text(said, address);
            cli_line_text(said, ": ");
            cli_line_decimal(said, status, 1u);
            cli_line_character(said, ' ');
            cli_line_counted(said, reason, reason_line.at);
            return 0;
        }
        if (cut || (joined == NULL) || (joined_size > CLI_CATALOG_BYTES))
        {
            cli_cell_given(joined);
            cli_line_text(said, address);
            cli_line_text(said, " is larger than orior_cli reads");
            return 0;
        }
        g_cli_catalog->bytes = joined;
        *size = joined_size;
        return 1;
    }
    const char *const path = catalog_begins(address, "file://") ? (address + 7) : address;
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = cli_file_held(path, &mapped_size);
    // a file of no bytes is held in a cell of one, so every read has a cell
    unsigned char *const bytes =
        ((mapped != NULL) && (mapped_size <= CLI_CATALOG_BYTES))
            ? cli_cell_taken((mapped_size != 0ull) ? mapped_size : 1ull, "the bytes a plugin catalog read")
            : NULL;
    if (bytes != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = bytes, .src = mapped, .bytes = (size_t)mapped_size});
    }
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, mapped_size);
    }
    if (bytes == NULL)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    g_cli_catalog->bytes = bytes;
    *size = mapped_size;
    return 1;
}

// whether `path` is whole on its own, as Rust's Path::is_absolute has it on this system
static int catalog_absolute(const char *path)
{
#if defined(_WIN32)
    const int drive = (((path[0] >= 'a') && (path[0] <= 'z')) || ((path[0] >= 'A') && (path[0] <= 'Z'))) &&
                      (path[1] == ':') && ((path[2] == '\\') || (path[2] == '/'));
    const int shared = ((path[0] == '\\') || (path[0] == '/')) && ((path[1] == '\\') || (path[1] == '/'));
    return drive || shared;
#else
    return path[0] == '/';
#endif
}

void cli_plugin_catalog_beside(const char *base, const char *address, CliLine *out)
{
    if ((cellul.find(&(CatenaFinitaCfg){
             .src = address, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL) ||
        catalog_absolute(address))
    {
        cli_line_text(out, address);
        return;
    }
    if (cellul.find(&(CatenaFinitaCfg){
            .src = base, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL)
    {
        const char *const cut =
            cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '/');
        if (cut != NULL)
        {
            cli_line_counted(out, base, (unsigned long long)(cut - base));
            cli_line_character(out, '/');
        }
        cli_line_text(out, address);
        return;
    }
    char parent[CLI_PATH_LONGEST];
    if (cli_path_parent(base, parent))
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = parent, .cap = CLI_CELLS_POOL});
        cli_line_text(out, parent);
        if ((length > 0u) && (parent[length - 1u] != '/') && (parent[length - 1u] != '\\'))
        {
#if defined(_WIN32)
            cli_line_character(out, '\\');
#else
            cli_line_character(out, '/');
#endif
        }
    }
    cli_line_text(out, address);
}

// the name serde gives a value's kind where it is not the kind wanted
static const char *catalog_kind(const CliJson *json, unsigned int node)
{
    static const char *const kinds[7] = {"map", "sequence", "string", "number", "boolean", "boolean", "null"};
    return (node == CLI_JSON_NONE) ? "null" : kinds[json->node[node].kind];
}

void cli_plugin_catalog_invalid(const char *where, const CliJson *json, unsigned int node, const char *wanted,
                                CliLine *said)
{
    cli_line_text(said, where);
    cli_line_text(said, ": invalid type: ");
    cli_line_text(said, catalog_kind(json, node));
    cli_line_text(said, ", expected ");
    cli_line_text(said, wanted);
}

int cli_plugin_catalog_entry_read(const CliJson *json, unsigned int entry, const char *where, CliLine *said)
{
    if (!cli_json_is(json, entry, CFG_JSON_OBJECT))
    {
        cli_plugin_catalog_invalid(where, json, entry, "struct Entry", said);
        return 0;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        const unsigned int field = cli_json_member(json, entry, s_texts[at]);
        if ((field != CLI_JSON_NONE) && !cli_json_is(json, field, CFG_JSON_STRING))
        {
            cli_plugin_catalog_invalid(where, json, field, "a string", said);
            return 0;
        }
    }
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned int field = cli_json_member(json, entry, s_lists[at]);
        if ((field != CLI_JSON_NONE) && !cli_json_is(json, field, CFG_JSON_ARRAY))
        {
            cli_plugin_catalog_invalid(where, json, field, "a sequence", said);
            return 0;
        }
        for (unsigned int item = 0u; (field != CLI_JSON_NONE) && (cli_json_item(json, field, item) != CLI_JSON_NONE);
             item += 1u)
        {
            if (!cli_json_is(json, cli_json_item(json, field, item), CFG_JSON_STRING))
            {
                cli_plugin_catalog_invalid(where, json, cli_json_item(json, field, item), "a string", said);
                return 0;
            }
        }
    }
    return 1;
}

const char *cli_plugin_catalog_field(const CliJson *json, unsigned int entry, const char *name)
{
    const char *const text = cli_json_string(json, cli_json_member(json, entry, name), NULL);
    return (text != NULL) ? text : "";
}

// an entry written as serde writes it, a field it does not give written empty
static void catalog_entry_written(const CliJson *json, unsigned int entry, CliLine *out)
{
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        cli_line_text(out, (at == 0u) ? "{\"" : ",\"");
        cli_line_text(out, s_texts[at]);
        cli_line_text(out, "\":");
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(json, cli_json_member(json, entry, s_texts[at]), &length);
        cli_json_quoted(out, (text != NULL) ? text : "", (text != NULL) ? length : 0ull);
    }
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_line_text(out, ",\"");
        cli_line_text(out, s_lists[at]);
        cli_line_text(out, "\":");
        const unsigned int field = cli_json_member(json, entry, s_lists[at]);
        if (field == CLI_JSON_NONE)
        {
            cli_line_text(out, "[]");
        }
        else
        {
            cli_json_write(json, field, out, 0u, 0);
        }
    }
    cli_line_character(out, '}');
}

// whether an entry is one to list: an id that is its own slug, and a package
static int catalog_listed(const CliJson *json, unsigned int entry)
{
    const char *const id = cli_plugin_catalog_field(json, entry, "id");
    char slug[CLI_NAME_LONGEST];
    if ((id[0] == '\0') || (cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) >= sizeof(slug)))
    {
        return 0;
    }
    cli_plugins_slug(id, slug);
    return (cellul.eq(&(CatenaFinitaCfg){.src = slug, .other = id, .cap = CLI_CELLS_POOL})) &&
           (cli_plugin_catalog_field(json, entry, "package")[0] != '\0');
}

int cli_plugin_catalog_read(const char *address, CliLine *out, CliLine *said)
{
    if (!cli_plugin_catalog_held(said))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    int read = cli_plugin_catalog_fetch(address, "the plugin catalog", &size, said);
    unsigned int catalog = CLI_JSON_NONE;
    if (read)
    {
        char why[CLI_SAID_LONGEST];
        CliLine read_said = {why, sizeof(why), 0ull};
        catalog = cli_plugin_catalog_tree((const char *)g_cli_catalog->bytes, size, &read_said);
        read = (catalog != CLI_JSON_NONE);
        if (!read)
        {
            cli_line_text(said, address);
            cli_line_text(said, ": ");
            cli_line_counted(said, why, read_said.at);
        }
    }
    const char *const kind =
        read ? cli_json_string(&g_cli_catalog->json, cli_json_member(&g_cli_catalog->json, catalog, "orior"), NULL)
             : NULL;
    if (read && ((kind == NULL) ||
                 (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = CLI_PLUGIN_CATALOG, .cap = CLI_CELLS_POOL}))))
    {
        cli_line_text(said, address);
        cli_line_text(said, " is no catalog of orior's plugins");
        read = 0;
    }
    const unsigned int plugins = read ? cli_json_member(&g_cli_catalog->json, catalog, "plugins") : CLI_JSON_NONE;
    if (read && !cli_json_is(&g_cli_catalog->json, plugins, CFG_JSON_ARRAY))
    {
        cli_plugin_catalog_invalid(address, &g_cli_catalog->json, plugins, "a sequence", said);
        read = 0;
    }
    for (unsigned int at = 0u; read && (cli_json_item(&g_cli_catalog->json, plugins, at) != CLI_JSON_NONE); at += 1u)
    {
        read = cli_plugin_catalog_entry_read(&g_cli_catalog->json, cli_json_item(&g_cli_catalog->json, plugins, at),
                                             address, said);
    }
    if (read)
    {
        cli_line_character(out, '[');
        int first = 1;
        for (unsigned int at = 0u; cli_json_item(&g_cli_catalog->json, plugins, at) != CLI_JSON_NONE; at += 1u)
        {
            const unsigned int entry = cli_json_item(&g_cli_catalog->json, plugins, at);
            if (catalog_listed(&g_cli_catalog->json, entry))
            {
                cli_line_text(out, first ? "" : ",");
                catalog_entry_written(&g_cli_catalog->json, entry, out);
                first = 0;
            }
        }
        cli_line_character(out, ']');
    }
    cli_plugin_catalog_released();
    return read;
}
