// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept.c: where each of the window's entries is kept, and every entry read back from orior's own folder
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "kept_internal.h"
#include "settings.h"

// the settings among the window's entries: a key named here, or one starting with a name here that ends in `.`
static const char *const s_settings[46] = {"scheme",          "zoom",           "opacity.",
                                           "fonts",           "theme.",         "crumbs",
                                           "column",          "sticky",         "brackets",
                                           "autosave",        "trim",           "final-newline",
                                           "format-on-save",  "panes.auto",     "patterns.",
                                           "search.sets",     "plugins.off",    "plugins.allowed",
                                           "plugins.catalog", "macros",         "vim",
                                           "margin",          "whitespace",     "comments-hidden",
                                           "past-ends",       "diff-space",     "diff-structure",
                                           "memory-budget",   "cores-kept",     "smooth-scroll",
                                           "flick-scroll",    "type-hints",     "parameter-hints",
                                           "shape-search",    "docstring-form", "checkers",
                                           "environment.",    "continuation",   "operator-next-line",
                                           "doc-margin",      "tests.parallel", "titlebar",
                                           "signing.",        "keys",           "network",
                                           "language"};

// the entry that holds every theme of the reader's, and the start of each change not saved
static const char s_themes[] = "themes";

static const char s_backup[] = "backup.";

unsigned int cli_kept_place(const char *key)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_themes, .cap = CLI_CELLS_POOL}))
    {
        return CLI_KEPT_THEMES;
    }
    if (cli_text_same(key, s_backup, sizeof(s_backup) - 1u))
    {
        return CLI_KEPT_BACKUP;
    }
    for (unsigned int at = 0u; at < 46u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_settings[at], .cap = CLI_CELLS_POOL});
        const int prefix = s_settings[at][length - 1ull] == '.';
        if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_settings[at], .cap = CLI_CELLS_POOL})) ||
            (prefix && cli_text_same(key, s_settings[at], length)))
        {
            return CLI_KEPT_SETTINGS;
        }
    }
    return CLI_KEPT_STATE;
}

int cli_kept_is_carried(const char *key)
{
    const unsigned int place = cli_kept_place(key);
    return (place == CLI_KEPT_SETTINGS) || (place == CLI_KEPT_THEMES);
}

void cli_kept_read_back(const CliJson *json, unsigned int value, CliLine *out)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, value, &length);
    if (text != NULL)
    {
        cli_line_counted(out, text, length);
    }
    else
    {
        cli_json_write(json, value, out, 0u, 0);
    }
}

unsigned int cli_kept_map_read(CliJson *json, CliJsonNode *nodes, char *text, const char *path)
{
    // the file, it made plain and the tokens it is read through, a cell taken for the reading; a file with no room
    // left to read it in reads as one that is not there
    typedef struct
    {
        char file[CLI_KEPT_BYTES];
        char plain[CLI_KEPT_BYTES];
        CfgJsonToken tokens[CLI_KEPT_NODES];
    } CliKeptMapRoom;
    cli_json_begun(json, nodes, CLI_KEPT_NODES, text, CLI_KEPT_BYTES);
    CliKeptMapRoom *const room = cli_cell_taken(sizeof(*room), "a kept file's reading");
    const unsigned long long size = (room != NULL)
                                        ? qry_member_file_read(path, (unsigned char *)room->file, sizeof(room->file))
                                        : QRY_MEMBER_UNREAD;
    unsigned int root = CLI_JSON_NONE;
    if (size != QRY_MEMBER_UNREAD)
    {
        CliLine plain = {room->plain, sizeof(room->plain), 0ull};
        cli_plain_json(room->file, size, &plain);
        char why[CLI_NAME_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        root = cli_line_fits(&plain) ? cli_json_read(json, room->plain, plain.at, room->tokens, CLI_KEPT_NODES, &said)
                                     : CLI_JSON_NONE;
    }
    cli_cell_given(room);
    return cli_json_is(json, root, CFG_JSON_OBJECT) ? root : cli_json_made(json, CFG_JSON_OBJECT);
}

// a JSON file's name put among a folder's, in order
static void cli_json_file_each(void *context, const char *name, int is_folder)
{
    CliJsonFiles *const files = (CliJsonFiles *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length < 5ull) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 5ull, .other = ".json", .cap = CLI_CELLS_POOL})) ||
        (files->count >= CLI_KEPT_FILES))
    {
        return;
    }
    unsigned int place = files->count;
    while ((place > 0u) && (cli_text_order(files->name[place - 1u], name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = files->name[place],
            .src = files->name[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->name[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = files->name[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    files->count += 1u;
}

void cli_kept_json_files(const char *folder, CliJsonFiles *files)
{
    files->count = 0u;
    query_folder_each(folder, cli_json_file_each, files);
}

// `name` under `folder` into `out`, which holds CLI_PATH_LONGEST
static void cli_in(const char *folder, const char *name, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

// The entries read, a tree of their own, and each file read in turn, a cell taken with the lock and given back with
// it: with a theme's bytes and the tokens it is read through
typedef struct
{
    char theme[CLI_KEPT_BYTES];
    CfgJsonToken tokens[CLI_KEPT_NODES];
    CliJsonNode entry_nodes[CLI_KEPT_NODES];
    char entry_text[CLI_KEPT_BYTES];
    CliJsonNode file_nodes[CLI_KEPT_NODES];
    char file_text[CLI_KEPT_BYTES];
    char value[CLI_KEPT_BYTES];
    CliJsonFiles files;
} CliKeptRead;

static CliKeptRead *s_read;
static CliLock s_read_lock = CLI_LOCK_INIT;

// an entry put in the entries' object as its text, in the place of one it had
static void cli_entry_put(CliJson *entries, unsigned int object, const char *key, const char *text,
                          unsigned long long length)
{
    cli_json_set(entries, object, key, cli_json_string_made(entries, text, length));
}

int cli_kept_read(const char *folder, CliLine *out)
{
    char path[CLI_PATH_LONGEST];
    cli_in(folder, "state.json", path);
    if (!query_is_file(path))
    {
        return 0;
    }
    cli_lock_take(&s_read_lock);
    s_read = cli_cell_taken(sizeof(*s_read), "the changes kept read");
    if (s_read == NULL)
    {
        cli_lock_give(&s_read_lock);
        return 0;
    }
    CliJson entries;
    cli_json_begun(&entries, s_read->entry_nodes, CLI_KEPT_NODES, s_read->entry_text, CLI_KEPT_BYTES);
    const unsigned int object = cli_json_made(&entries, CFG_JSON_OBJECT);
    static const char *const s_files[2] = {"settings.json", "state.json"};
    for (unsigned int file = 0u; file < 2u; file += 1u)
    {
        CliJson map;
        cli_in(folder, s_files[file], path);
        const unsigned int read = cli_kept_map_read(&map, s_read->file_nodes, s_read->file_text, path);
        for (unsigned int member = map.node[read].first; member != CLI_JSON_NONE; member = map.node[member].next)
        {
            const char *const key = map.text + map.node[member].key;
            if ((file == 0u) && cli_settings_is_file_key(key))
            {
                continue;
            }
            CliLine value = {s_read->value, sizeof(s_read->value), 0ull};
            cli_kept_read_back(&map, member, &value);
            cli_entry_put(&entries, object, key, s_read->value, cli_line_fits(&value) ? value.at : 0ull);
        }
    }
    // the themes, each file's object, as one JSON array
    char themes_folder[CLI_PATH_LONGEST];
    cli_in(folder, "themes", themes_folder);
    cli_kept_json_files(themes_folder, &s_read->files);
    CliLine themes = {s_read->value, sizeof(s_read->value), 0ull};
    unsigned int themed = 0u;
    for (unsigned int at = 0u; at < s_read->files.count; at += 1u)
    {
        CliJson map;
        cli_in(themes_folder, s_read->files.name[at], path);
        char why[CLI_NAME_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        cli_json_begun(&map, s_read->file_nodes, CLI_KEPT_NODES, s_read->file_text, CLI_KEPT_BYTES);
        const unsigned long long size =
            qry_member_file_read(path, (unsigned char *)s_read->theme, sizeof(s_read->theme));
        const unsigned int theme = (size != QRY_MEMBER_UNREAD)
                                       ? cli_json_read(&map, s_read->theme, size, s_read->tokens, CLI_KEPT_NODES, &said)
                                       : CLI_JSON_NONE;
        if (theme == CLI_JSON_NONE)
        {
            continue;
        }
        cli_line_character(&themes, (themed == 0u) ? '[' : ',');
        cli_json_write(&map, theme, &themes, 0u, 0);
        themed += 1u;
    }
    if (themed != 0u)
    {
        cli_line_character(&themes, ']');
        cli_entry_put(&entries, object, s_themes, s_read->value, cli_line_fits(&themes) ? themes.at : 0ull);
    }
    // each change not saved: its key and its text
    char backups[CLI_PATH_LONGEST];
    cli_in(folder, "backups", backups);
    cli_kept_json_files(backups, &s_read->files);
    for (unsigned int at = 0u; at < s_read->files.count; at += 1u)
    {
        CliJson map;
        cli_in(backups, s_read->files.name[at], path);
        const unsigned int backup = cli_kept_map_read(&map, s_read->file_nodes, s_read->file_text, path);
        const char *const key = cli_json_string(&map, cli_json_member(&map, backup, "key"), NULL);
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(&map, cli_json_member(&map, backup, "text"), &length);
        if ((key != NULL) && (text != NULL))
        {
            cli_entry_put(&entries, object, key, text, length);
        }
    }
    cli_json_sorted(&entries, object);
    cli_json_write(&entries, object, out, 0u, 1);
    cli_cell_given(s_read);
    s_read = NULL;
    cli_lock_give(&s_read_lock);
    return 1;
}
