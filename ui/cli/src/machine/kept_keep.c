// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept_keep.c: the window's changes kept in orior's own folder: each setting and each piece of state under its key in
// its file, each theme in a file of its own, and each change not saved in a file named by its key's hash; each file
// written beside itself and moved over, which leaves it whole where the write fails
#include "../command_line/cells.h"
#include "../text/text.h"
#include "kept_internal.h"

// The files being kept and what is written, a cell taken with the lock and given back with it: with the tokens the
// changes, the themes and an entry are read through, and an entry written back
typedef struct
{
    CfgJsonToken given_tokens[CLI_KEPT_NODES];
    CfgJsonToken theme_tokens[CLI_KEPT_NODES];
    CfgJsonToken entry_tokens[CLI_KEPT_NODES];
    char back[CLI_KEPT_BYTES];
    CliJsonNode change_nodes[CLI_KEPT_NODES];
    char change_text[CLI_KEPT_BYTES];
    CliJsonNode settings_nodes[CLI_KEPT_NODES];
    char settings_text[CLI_KEPT_BYTES];
    CliJsonNode state_nodes[CLI_KEPT_NODES];
    char state_text[CLI_KEPT_BYTES];
    CliJsonNode file_nodes[CLI_KEPT_NODES];
    char file_text[CLI_KEPT_BYTES];
    char written[CLI_KEPT_BYTES];
    CliJsonFiles files;
    char names[CLI_KEPT_FILES][CLI_NAME_LONGEST];
} CliKeeping;

static CliKeeping *s_keeping;
static CliLock s_lock = CLI_LOCK_INIT;

// `name` under `folder` into `out`, which holds CLI_PATH_LONGEST
static void cli_in(const char *folder, const char *name, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

// `size` bytes of `text` written beside the file at `path` and moved over it, its folder made: 1, or 0 with why said
static int cli_whole_written(const char *path, const char *text, unsigned long long size, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (cli_path_parent(path, folder) && !query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    // the file's path with its extension put as `saving`
    char beside[CLI_PATH_LONGEST];
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        (name[0] != '\0')
            ? cli_text_last(name + 1, cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}), '.')
            : NULL;
    CliLine line = {beside, sizeof(beside), 0ull};
    cli_line_counted(&line, path,
                     (dot != NULL) ? (unsigned long long)(dot - path)
                                   : cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, ".saving");
    cli_line_closed(&line);
    if (!query_file_written(beside, text, size) || !query_file_renamed(beside, path))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

// an object written to `path` with its keys in order, as serde_json's pretty writer lays it out, a newline after
static int cli_map_written(const char *path, CliJson *json, unsigned int object, CliLine *said)
{
    cli_json_sorted(json, object);
    CliLine out = {s_keeping->written, sizeof(s_keeping->written), 0ull};
    cli_json_write(json, object, &out, 2u, 1);
    cli_line_character(&out, '\n');
    if (!cli_line_fits(&out))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it is larger than orior writes");
        return 0;
    }
    return cli_whole_written(path, s_keeping->written, out.at, said);
}

// an entry's text as it is written into `json`: its JSON where it is JSON other than a string that writes back the
// same, else the text itself as a string
static unsigned int cli_written(CliJson *json, const char *text, unsigned long long length)
{
    const unsigned int mark = json->nodes;
    const unsigned long long used = json->used;
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int value = cli_json_read(json, text, length, s_keeping->entry_tokens, CLI_KEPT_NODES, &said);
    if ((value != CLI_JSON_NONE) && !cli_json_is(json, value, CFG_JSON_STRING))
    {
        CliLine back = {s_keeping->back, sizeof(s_keeping->back), 0ull};
        cli_json_write(json, value, &back, 0u, 0);
        if (cli_line_fits(&back) && (back.at == length) &&
            (memor.cmp(&(MemoriaCfg){.src = s_keeping->back, .other = text, .bytes = length}) == 0))
        {
            return value;
        }
    }
    // the nodes and texts the reading made taken back
    json->nodes = mark;
    json->used = used;
    return cli_json_string_made(json, text, length);
}

// a theme's file name: its name with each letter a file name cannot hold, or should not, made `_`, `.` and spaces
// trimmed from its ends, `theme` where nothing is left, then `.json`, into `out`, which holds CLI_NAME_LONGEST
static void cli_theme_file(const char *name, char *out)
{
    char safe[CLI_NAME_LONGEST];
    CliLine line = {safe, sizeof(safe), 0ull};
    for (const char *at = name; at[0] != '\0'; at += 1)
    {
        const char letter = at[0];
        const int kept = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                         ((letter >= '0') && (letter <= '9')) || (letter == ' ') || (letter == '-') ||
                         (letter == '_') || (letter == '.') || ((unsigned char)letter >= 0x80u);
        cli_line_character(&line, kept ? letter : '_');
    }
    const unsigned long long length = cli_line_closed(&line);
    const char *from = safe;
    const char *to = safe + length;
    while ((from < to) && ((from[0] == '.') || (from[0] == ' ')))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == '.') || (to[-1] == ' ')))
    {
        to -= 1;
    }
    CliLine file = {out, CLI_NAME_LONGEST, 0ull};
    if (from == to)
    {
        cli_line_text(&file, "theme");
    }
    else
    {
        cli_line_counted(&file, from, (unsigned long long)(to - from));
    }
    cli_line_text(&file, ".json");
    cli_line_closed(&file);
}

// a change not saved's file name: a hash of its key, the same for the same key, into `out`, which holds 22
static void cli_backup_file(const char *key, char *out)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = key; at[0] != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    CliLine line = {out, 22u, 0ull};
    cli_line_hex(&line, hash, 16u);
    cli_line_text(&line, ".json");
    cli_line_closed(&line);
}

// each theme in the list written to a file of its own, and the files of themes no longer in it taken out
static int cli_themes_kept(const char *folder, const char *text, unsigned long long length, CliLine *said)
{
    CliJson json;
    cli_json_begun(&json, s_keeping->file_nodes, CLI_KEPT_NODES, s_keeping->file_text, CLI_KEPT_BYTES);
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int themes =
        (text != NULL) ? cli_json_read(&json, text, length, s_keeping->theme_tokens, CLI_KEPT_NODES, &unread)
                       : CLI_JSON_NONE;
    unsigned int names = 0u;
    for (unsigned int theme = cli_json_is(&json, themes, CFG_JSON_ARRAY) ? json.node[themes].first : CLI_JSON_NONE;
         theme != CLI_JSON_NONE; theme = json.node[theme].next)
    {
        const char *const named = cli_json_string(&json, cli_json_member(&json, theme, "name"), NULL);
        char base[CLI_NAME_LONGEST];
        cli_theme_file((named != NULL) ? named : "theme", base);
        char name[CLI_NAME_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = name,
                                .src = base,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
        for (unsigned int count = 2u;; count += 1u)
        {
            int taken = 0;
            for (unsigned int at = 0u; !taken && (at < names); at += 1u)
            {
                taken =
                    cellul.eq(&(CatenaFinitaCfg){.src = s_keeping->names[at], .other = name, .cap = CLI_CELLS_POOL});
            }
            if (!taken)
            {
                break;
            }
            CliLine counted = {name, sizeof(name), 0ull};
            cli_line_counted(&counted, base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) - 5u);
            cli_line_character(&counted, ' ');
            cli_line_decimal(&counted, count, 1u);
            cli_line_text(&counted, ".json");
            cli_line_closed(&counted);
        }
        CliLine out = {s_keeping->written, sizeof(s_keeping->written), 0ull};
        cli_json_write(&json, theme, &out, 2u, 0);
        cli_line_character(&out, '\n');
        char path[CLI_PATH_LONGEST];
        cli_in(folder, name, path);
        if (!cli_line_fits(&out) || !cli_whole_written(path, s_keeping->written, out.at, said))
        {
            return 0;
        }
        if (names < CLI_KEPT_FILES)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_keeping->names[names],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            names += 1u;
        }
    }
    cli_kept_json_files(folder, &s_keeping->files);
    for (unsigned int at = 0u; at < s_keeping->files.count; at += 1u)
    {
        int kept = 0;
        for (unsigned int each = 0u; !kept && (each < names); each += 1u)
        {
            kept = cellul.eq(&(CatenaFinitaCfg){
                .src = s_keeping->names[each], .other = s_keeping->files.name[at], .cap = CLI_CELLS_POOL});
        }
        if (!kept)
        {
            char path[CLI_PATH_LONGEST];
            cli_in(folder, s_keeping->files.name[at], path);
            query_removed(path);
        }
    }
    return 1;
}

// the files kept given back, and the lock with them
static void keep_released(void)
{
    cli_cell_given(s_keeping);
    s_keeping = NULL;
    cli_lock_give(&s_lock);
}

int cli_kept_keep(const char *folder, const char *changes, unsigned long long length, CliLine *said)
{
    cli_lock_take(&s_lock);
    s_keeping = cli_cell_taken(sizeof(*s_keeping), "the changes kept");
    if (s_keeping == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "the changes kept could not be taken");
        return 0;
    }
    CliJson given;
    cli_json_begun(&given, s_keeping->change_nodes, CLI_KEPT_NODES, s_keeping->change_text, CLI_KEPT_BYTES);
    const unsigned int changed = cli_json_read(&given, changes, length, s_keeping->given_tokens, CLI_KEPT_NODES, said);
    if (!cli_json_is(&given, changed, CFG_JSON_OBJECT))
    {
        keep_released();
        return 0;
    }
    CliJson settings;
    CliJson state;
    unsigned int settings_map = CLI_JSON_NONE;
    unsigned int state_map = CLI_JSON_NONE;
    char path[CLI_PATH_LONGEST];
    int done = 1;
    for (unsigned int change = given.node[changed].first; done && (change != CLI_JSON_NONE);
         change = given.node[change].next)
    {
        const char *const key = given.text + given.node[change].key;
        unsigned long long text_length = 0ull;
        const char *const text = cli_json_string(&given, change, &text_length);
        const unsigned int place = cli_kept_place(key);
        if ((place == CLI_KEPT_SETTINGS) || (place == CLI_KEPT_STATE))
        {
            const int is_settings = place == CLI_KEPT_SETTINGS;
            CliJson *const json = is_settings ? &settings : &state;
            unsigned int *const map = is_settings ? &settings_map : &state_map;
            if (*map == CLI_JSON_NONE)
            {
                cli_in(folder, is_settings ? "settings.json" : "state.json", path);
                *map = cli_kept_map_read(json, is_settings ? s_keeping->settings_nodes : s_keeping->state_nodes,
                                         is_settings ? s_keeping->settings_text : s_keeping->state_text, path);
            }
            if (text != NULL)
            {
                cli_json_set(json, *map, key, cli_written(json, text, text_length));
            }
            else
            {
                cli_json_remove(json, *map, key);
            }
        }
        else if (place == CLI_KEPT_THEMES)
        {
            char themes[CLI_PATH_LONGEST];
            cli_in(folder, "themes", themes);
            done = cli_themes_kept(themes, text, text_length, said);
        }
        else
        {
            char file[24];
            char backups[CLI_PATH_LONGEST];
            cli_backup_file(key, file);
            cli_in(folder, "backups", backups);
            cli_in(backups, file, path);
            if (text != NULL)
            {
                CliJson backup;
                cli_json_begun(&backup, s_keeping->file_nodes, CLI_KEPT_NODES, s_keeping->file_text, CLI_KEPT_BYTES);
                const unsigned int object = cli_json_made(&backup, CFG_JSON_OBJECT);
                cli_json_set(&backup, object, "key",
                             cli_json_string_made(&backup, key,
                                                  cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL})));
                cli_json_set(&backup, object, "text", cli_json_string_made(&backup, text, text_length));
                done = cli_map_written(path, &backup, object, said);
            }
            else
            {
                query_removed(path);
            }
        }
    }
    if (done && (settings_map != CLI_JSON_NONE))
    {
        cli_in(folder, "settings.json", path);
        done = cli_map_written(path, &settings, settings_map, said);
    }
    cli_in(folder, "state.json", path);
    if (done && (state_map != CLI_JSON_NONE))
    {
        done = cli_map_written(path, &state, state_map, said);
    }
    else if (done && !query_is_file(path))
    {
        done = cli_whole_written(path, "{}\n", 3ull, said);
    }
    keep_released();
    return done;
}
