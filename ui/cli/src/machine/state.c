// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// state.c: where each of the window's entries is stored, and every entry read back from orior's own folder
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "settings.h"
#include "state_internal.h"

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
                                           "memory-budget",   "cores-reserved", "smooth-scroll",
                                           "flick-scroll",    "type-hints",     "parameter-hints",
                                           "shape-search",    "docstring-form", "checkers",
                                           "environment.",    "continuation",   "operator-next-line",
                                           "doc-margin",      "tests.parallel", "titlebar",
                                           "signing.",        "keys",           "network",
                                           "language"};

// the entry that contains every theme of the reader's, and the start of each change not saved
static const char s_themes[] = "themes";

static const char s_backup[] = "backup.";

unsigned int cli_state_place(const char *key)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_themes, .cap = CLI_CELLS_POOL}))
    {
        return CLI_STATE_THEMES;
    }
    if (cli_text_same(key, s_backup, sizeof(s_backup) - 1u))
    {
        return CLI_STATE_BACKUP;
    }
    for (unsigned int at = 0u; at < 46u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_settings[at], .cap = CLI_CELLS_POOL});
        const int prefix = s_settings[at][length - 1ull] == '.';
        if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_settings[at], .cap = CLI_CELLS_POOL})) ||
            (prefix && cli_text_same(key, s_settings[at], length)))
        {
            return CLI_STATE_SETTINGS;
        }
    }
    return CLI_STATE_FILE;
}

int cli_state_is_carried(const char *key)
{
    const unsigned int place = cli_state_place(key);
    return (place == CLI_STATE_SETTINGS) || (place == CLI_STATE_THEMES);
}

void cli_state_read_back(const CliJson *json, unsigned int value, VerbaLinea *out)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, value, &length);
    if (text != NULL)
    {
        mmgr_line_put_n(out, text, length);
    }
    else
    {
        cli_json_write(json, value, out, 0u, 0);
    }
}

CellOwner g_cli_state_cells = MMGR_CELL_OWNER("cli_state");

unsigned int cli_state_map_read(CliJson *json, const char *path, unsigned int nodes_more, unsigned long long text_more)
{
    // the file mapped and made plain in a cell no larger than it; a file larger than CLI_STATE_BYTES, or one with no
    // cell left to read it in, reads as one that is not there
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_read(path, &size);
    char *const plain_text = ((bytes != NULL) && (size <= CLI_STATE_BYTES))
                                 ? cli_lane_alloc(size + 1ull, &g_cli_state_cells)
                                 : NULL;
    unsigned int root = CLI_JSON_NONE;
    if (plain_text != NULL)
    {
        VerbaLinea plain = {plain_text, size + 1ull, 0ull};
        // the file's bytes are its text
        cli_plain_json((const char *)bytes, size, &plain);
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        root = mmgr_line_ok(&plain) ? cli_json_taken(json, plain_text, plain.at, nodes_more + 1u, text_more, &err_msg)
                                    : CLI_JSON_NONE;
    }
    cli_cell_release(plain_text, &g_cli_state_cells);
    if (bytes != NULL)
    {
        cli_file_let_go(bytes, size);
    }
    if (cli_json_is(json, root, CFG_JSON_OBJECT))
    {
        // the tree grows as entries are put, so the room over is a first size and not a bound
        json->grows = 1;
        return root;
    }
    // an empty object in a tree of its own, with the room over requested to begin with
    cli_json_given(json);
    return cli_json_growing(json, nodes_more + 1u, text_more, "an empty stored file's tree")
               ? cli_json_made(json, CFG_JSON_OBJECT)
               : CLI_JSON_NONE;
}

void cli_state_writing_begun(CliStateWriting *writing)
{
    cli_pieces_begun(&writing->pieces, &writing->line, writing->piece, sizeof(writing->piece));
}

char *cli_state_writing_joined(CliStateWriting *writing, unsigned long long *size)
{
    *size = 0ull;
    return cli_pieces_joined(&writing->pieces, &writing->line, "a stored text written", size);
}

// A folder's walk for its JSON files: the files, and the count of those it includes, read before they are put
typedef struct
{
    CliJsonFiles *files;
    unsigned int room;
} CliJsonFilesWalk;

// whether a folder's entry is a JSON file
static int cli_json_file_included(const char *name, int is_folder)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return !is_folder && (length >= 5ull) && (length < CLI_NAME_MAX_LEN) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 5ull, .other = ".json", .cap = CLI_CELLS_POOL}));
}

static void cli_json_file_counted(void *context, const char *name, int is_folder)
{
    CliJsonFilesWalk *const walk = (CliJsonFilesWalk *)context;
    walk->room += (cli_json_file_included(name, is_folder) && (walk->room < CLI_STATE_FILES)) ? 1u : 0u;
}

// a JSON file's name put among a folder's, in order
static void cli_json_file_each(void *context, const char *name, int is_folder)
{
    CliJsonFilesWalk *const walk = (CliJsonFilesWalk *)context;
    CliJsonFiles *const files = walk->files;
    if (!cli_json_file_included(name, is_folder) || (files->count >= walk->room))
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

void cli_state_json_files(const char *folder, CliJsonFiles *files)
{
    files->count = 0u;
    CliJsonFilesWalk walk = {files, 0u};
    query_folder_each(folder, cli_json_file_counted, &walk);
    files->name =
        (walk.room != 0u)
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)walk.room * CLI_NAME_MAX_LEN, &g_cli_state_cells)
            : NULL;
    walk.room = (files->name != NULL) ? walk.room : 0u;
    query_folder_each(folder, cli_json_file_each, &walk);
}

void cli_state_json_files_given(CliJsonFiles *files)
{
    cli_cell_release(files->name, &g_cli_state_cells);
    files->name = NULL;
    files->count = 0u;
}

// `name` under `folder` into `out`, sized for CLI_PATH_MAX_LEN
static void cli_in(const char *folder, const char *name, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    mmgr_line_finish(&line);
}

// an entry put in the entries' object as its text, in the place of one it had
static void cli_entry_put(CliJson *entries, unsigned int object, const char *key, const char *text,
                          unsigned long long length)
{
    cli_json_set(entries, object, key, cli_json_string_made(entries, text, length));
}

// the bytes of the file at `path`, 0 where it is not there
static unsigned long long cli_state_size(const char *path)
{
    unsigned long long size = 0ull;
    return cli_file_size(path, &size) ? size : 0ull;
}

// the bytes of the JSON files of `folder` together
static unsigned long long cli_state_files_size(const char *folder, const CliJsonFiles *files)
{
    unsigned long long size = 0ull;
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        cli_in(folder, files->name[at], path);
        size += cli_state_size(path);
    }
    return size;
}

int cli_state_read(const char *folder, VerbaLinea *out)
{
    char path[CLI_PATH_MAX_LEN];
    cli_in(folder, "state.json", path);
    if (!query_is_file(path))
    {
        return 0;
    }
    static const char *const s_files[2] = {"settings.json", "state.json"};
    char themes_folder[CLI_PATH_MAX_LEN];
    char backups[CLI_PATH_MAX_LEN];
    cli_in(folder, "themes", themes_folder);
    cli_in(folder, "backups", backups);
    CliJsonFiles themes_files = {NULL, 0u};
    CliJsonFiles backup_files = {NULL, 0u};
    cli_state_json_files(themes_folder, &themes_files);
    cli_state_json_files(backups, &backup_files);
    // every entry read is a key and a text of the files read, so the entries' tree contains no more nodes than their
    // bytes, and no more text than twice them, a value written back in place of its text
    unsigned long long bytes =
        cli_state_files_size(themes_folder, &themes_files) + cli_state_files_size(backups, &backup_files);
    for (unsigned int file = 0u; file < 2u; file += 1u)
    {
        cli_in(folder, s_files[file], path);
        bytes += cli_state_size(path);
    }
    bytes = (bytes < (CLI_STATE_BYTES * 4ull)) ? bytes : (CLI_STATE_BYTES * 4ull);
    CliJson entries;
    if (!cli_json_reserved(&entries, (unsigned int)bytes + 16u, (2ull * bytes) + 64ull, "the changes stored read"))
    {
        cli_state_json_files_given(&backup_files);
        cli_state_json_files_given(&themes_files);
        return 0;
    }
    const unsigned int object = cli_json_made(&entries, CFG_JSON_OBJECT);
    for (unsigned int file = 0u; file < 2u; file += 1u)
    {
        CliJson map;
        cli_in(folder, s_files[file], path);
        const unsigned int read = cli_state_map_read(&map, path, 0u, 0ull);
        for (unsigned int member = (read != CLI_JSON_NONE) ? map.node[read].first : CLI_JSON_NONE;
             member != CLI_JSON_NONE; member = map.node[member].next)
        {
            const char *const key = map.text + map.node[member].key;
            if ((file == 0u) && cli_settings_is_file_key(key))
            {
                continue;
            }
            CliStateWriting value;
            cli_state_writing_begun(&value);
            cli_state_read_back(&map, member, &value.line);
            unsigned long long size = 0ull;
            char *const joined = cli_state_writing_joined(&value, &size);
            cli_entry_put(&entries, object, key, (joined != NULL) ? joined : "", size);
            cli_cell_release(joined, NULL);
        }
        cli_json_given(&map);
    }
    // the themes, each file's object, as one JSON array
    CliStateWriting themes;
    cli_state_writing_begun(&themes);
    unsigned int themed = 0u;
    for (unsigned int at = 0u; at < themes_files.count; at += 1u)
    {
        CliJson map;
        cli_in(themes_folder, themes_files.name[at], path);
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        cli_json_begun(&map, NULL, 0u, NULL, 0ull);
        const unsigned int theme = (cli_state_size(path) <= CLI_STATE_BYTES)
                                       ? cli_json_file_taken(&map, path, 0u, 0ull, &err_msg)
                                       : CLI_JSON_NONE;
        if (theme != CLI_JSON_NONE)
        {
            mmgr_line_ch(&themes.line, (themed == 0u) ? '[' : ',');
            cli_json_write(&map, theme, &themes.line, 0u, 0);
            themed += 1u;
        }
        cli_json_given(&map);
    }
    mmgr_line_ch(&themes.line, ']');
    unsigned long long themes_size = 0ull;
    char *const themes_text = cli_state_writing_joined(&themes, &themes_size);
    if (themed != 0u)
    {
        cli_entry_put(&entries, object, s_themes, (themes_text != NULL) ? themes_text : "", themes_size);
    }
    cli_cell_release(themes_text, NULL);
    // each change not saved: its key and its text
    for (unsigned int at = 0u; at < backup_files.count; at += 1u)
    {
        CliJson map;
        cli_in(backups, backup_files.name[at], path);
        const unsigned int backup = cli_state_map_read(&map, path, 0u, 0ull);
        const char *const key = cli_json_string(&map, cli_json_member(&map, backup, "key"), NULL);
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(&map, cli_json_member(&map, backup, "text"), &length);
        if ((key != NULL) && (text != NULL))
        {
            cli_entry_put(&entries, object, key, text, length);
        }
        cli_json_given(&map);
    }
    cli_state_json_files_given(&backup_files);
    cli_state_json_files_given(&themes_files);
    cli_json_sorted(&entries, object);
    cli_json_write(&entries, object, out, 0u, 1);
    cli_json_given(&entries);
    return 1;
}
