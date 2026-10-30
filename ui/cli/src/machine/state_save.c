// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// state_save.c: the window's changes stored in orior's own folder: each setting and each piece of state under its key
// in its file, each theme in a file of its own, and each change not saved in a file named by its key's hash; each file
// written beside itself and moved over, which leaves it whole where the write fails
#include "../command_line/cells.h"
#include "../text/text.h"
#include "state_internal.h"

// the files stored, one save at a time; each tree and text a save reads and writes is a cell of its own, sized to
// it
static CliLock s_lock = CLI_LOCK_INIT;

// `name` under `folder` into `out`, sized for CLI_PATH_MAX_LEN
static void cli_in(const char *folder, const char *name, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    mmgr_line_finish(&line);
}

// `size` bytes of `text` written beside the file at `path` and moved over it, its folder made: 1, or 0 with why given
static int cli_whole_written(const char *path, const char *text, unsigned long long size, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    if (cli_path_parent(path, folder) && !query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    // the file's path with its extension put as `saving`
    char beside[CLI_PATH_MAX_LEN];
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        (name[0] != '\0')
            ? cli_text_last(name + 1, cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}), '.')
            : NULL;
    VerbaLinea line = {beside, sizeof(beside), 0ull};
    mmgr_line_put_n(&line, path,
                    (dot != NULL) ? (unsigned long long)(dot - path)
                                  : cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ".saving");
    mmgr_line_finish(&line);
    if (!query_file_written(beside, text, size) || !query_file_renamed(beside, path))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// an object written to `path` with its keys in order, as serde_json's pretty writer lays it out, a newline after
static int cli_map_written(const char *path, CliJson *json, unsigned int object, VerbaLinea *err_msg)
{
    cli_json_sorted(json, object);
    CliStateWriting out;
    cli_state_writing_begun(&out);
    cli_json_write(json, object, &out.line, 2u, 1);
    mmgr_line_ch(&out.line, '\n');
    unsigned long long size = 0ull;
    char *const written = cli_state_writing_joined(&out, &size);
    if (written == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": the cells it is written in could not be taken");
        return 0;
    }
    const int whole = cli_whole_written(path, written, size, err_msg);
    cli_cell_release(written, NULL);
    return whole;
}

// an entry's text as it is written into `json`: its JSON where it is JSON other than a string that writes back the
// same, else the text itself as a string
static unsigned int cli_written(CliJson *json, const char *text, unsigned long long length)
{
    const unsigned int mark = json->nodes;
    const unsigned long long used = json->used;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    // the tokens the entry is read through, as many as it contains; and the entry written back, which matches it only
    // where it is as long as it, which a cell one longer fits at every match
    const unsigned int tokens_most = cli_json_tokens_counted(text, length) + 1u;
    CfgJsonToken *const tokens =
        cli_lane_alloc((unsigned long long)tokens_most * sizeof(CfgJsonToken), &g_cli_state_cells);
    char *const back_text = g_cli_cells->persistent_buf_alloc(length + 2ull, &g_cli_state_cells);
    const unsigned int value = ((tokens != NULL) && (back_text != NULL))
                                   ? cli_json_read(json, text, length, tokens, tokens_most, &err_msg)
                                   : CLI_JSON_NONE;
    int same = 0;
    if ((value != CLI_JSON_NONE) && !cli_json_is(json, value, CFG_JSON_STRING))
    {
        VerbaLinea back = {back_text, length + 2ull, 0ull};
        cli_json_write(json, value, &back, 0u, 0);
        same = mmgr_line_ok(&back) && (back.at == length) &&
               (memor.cmp(&(MemoriaCfg){.src = back_text, .other = text, .bytes = length}) == 0);
    }
    cli_cell_release(back_text, &g_cli_state_cells);
    cli_cell_release(tokens, &g_cli_state_cells);
    if (same)
    {
        return value;
    }
    // the nodes and texts the reading made taken back
    json->nodes = mark;
    json->used = used;
    return cli_json_string_made(json, text, length);
}

// a theme's file name: its name with each letter a file name cannot contain, or should not, made `_`, `.` and spaces
// trimmed from its ends, `theme` where nothing is left, then `.json`, into `out`, sized for CLI_NAME_MAX_LEN
static void cli_theme_file(const char *name, char *out)
{
    char safe[CLI_NAME_MAX_LEN];
    VerbaLinea line = {safe, sizeof(safe), 0ull};
    for (const char *at = name; at[0] != '\0'; at += 1)
    {
        const char letter = at[0];
        const int allowed = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                            ((letter >= '0') && (letter <= '9')) || (letter == ' ') || (letter == '-') ||
                            (letter == '_') || (letter == '.') || ((unsigned char)letter >= 0x80u);
        mmgr_line_ch(&line, allowed ? letter : '_');
    }
    const unsigned long long length = mmgr_line_finish(&line);
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
    VerbaLinea file = {out, CLI_NAME_MAX_LEN, 0ull};
    if (from == to)
    {
        mmgr_line_put(&file, "theme");
    }
    else
    {
        mmgr_line_put_n(&file, from, (unsigned long long)(to - from));
    }
    mmgr_line_put(&file, ".json");
    mmgr_line_finish(&file);
}

// a change not saved's file name: a hash of its key, the same for the same key, into `out`, sized for 22
static void cli_backup_file(const char *key, char *out)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = key; at[0] != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    VerbaLinea line = {out, 22u, 0ull};
    mmgr_line_hex(&line, hash, 16u);
    mmgr_line_put(&line, ".json");
    mmgr_line_finish(&line);
}

// each theme in the list written to a file of its own, and the files of themes no longer in it taken out
static int cli_themes_saved(const char *folder, const char *text, unsigned long long length, VerbaLinea *err_msg)
{
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int themes = (text != NULL) ? cli_json_taken(&json, text, length, 0u, 0ull, &unread) : CLI_JSON_NONE;
    // the files written, one a theme, in a cell of their count
    const unsigned int count = cli_json_is(&json, themes, CFG_JSON_ARRAY) ? json.node[themes].count : 0u;
    char (*const saved_names)[CLI_NAME_MAX_LEN] =
        (count != 0u)
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)count * CLI_NAME_MAX_LEN, &g_cli_state_cells)
            : NULL;
    unsigned int names = 0u;
    for (unsigned int theme = cli_json_is(&json, themes, CFG_JSON_ARRAY) ? json.node[themes].first : CLI_JSON_NONE;
         theme != CLI_JSON_NONE; theme = json.node[theme].next)
    {
        const char *const named = cli_json_string(&json, cli_json_member(&json, theme, "name"), NULL);
        char base[CLI_NAME_MAX_LEN];
        cli_theme_file((named != NULL) ? named : "theme", base);
        char name[CLI_NAME_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = name,
                                .src = base,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
        for (unsigned int count = 2u;; count += 1u)
        {
            int taken = 0;
            for (unsigned int at = 0u; !taken && (at < names); at += 1u)
            {
                taken = cellul.eq(&(CatenaFinitaCfg){.src = saved_names[at], .other = name, .cap = CLI_CELLS_POOL});
            }
            if (!taken)
            {
                break;
            }
            VerbaLinea counted = {name, sizeof(name), 0ull};
            mmgr_line_put_n(&counted, base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) - 5u);
            mmgr_line_ch(&counted, ' ');
            mmgr_line_decimal(&counted, count, 1u);
            mmgr_line_put(&counted, ".json");
            mmgr_line_finish(&counted);
        }
        CliStateWriting out;
        cli_state_writing_begun(&out);
        cli_json_write(&json, theme, &out.line, 2u, 0);
        mmgr_line_ch(&out.line, '\n');
        unsigned long long size = 0ull;
        char *const written = cli_state_writing_joined(&out, &size);
        char path[CLI_PATH_MAX_LEN];
        cli_in(folder, name, path);
        const int whole = (written != NULL) && cli_whole_written(path, written, size, err_msg);
        cli_cell_release(written, NULL);
        if (!whole)
        {
            cli_cell_release(saved_names, &g_cli_state_cells);
            cli_json_given(&json);
            return 0;
        }
        if (names < count)
        {
            memor.cpy(&(MemoriaCfg){.dst = saved_names[names],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            names += 1u;
        }
    }
    CliJsonFiles files = {NULL, 0u};
    cli_state_json_files(folder, &files);
    for (unsigned int at = 0u; at < files.count; at += 1u)
    {
        int saved_count = 0;
        for (unsigned int each = 0u; !saved_count && (each < names); each += 1u)
        {
            saved_count =
                cellul.eq(&(CatenaFinitaCfg){.src = saved_names[each], .other = files.name[at], .cap = CLI_CELLS_POOL});
        }
        if (!saved_count)
        {
            char path[CLI_PATH_MAX_LEN];
            cli_in(folder, files.name[at], path);
            query_removed(path);
        }
    }
    cli_state_json_files_given(&files);
    cli_cell_release(saved_names, &g_cli_state_cells);
    cli_json_given(&json);
    return 1;
}

int cli_state_save(const char *folder, const char *changes, unsigned long long length, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    CliJson given;
    cli_json_begun(&given, NULL, 0u, NULL, 0ull);
    const unsigned int changed = cli_json_taken(&given, changes, length, 0u, 0ull, err_msg);
    if (!cli_json_is(&given, changed, CFG_JSON_OBJECT))
    {
        cli_json_given(&given);
        cli_lock_give(&s_lock);
        return 0;
    }
    // what the changes put in the settings or the state: each a key and an entry's text, read as JSON or stored as a
    // string; the trees grow as the entries are put, from the changes' own nodes and bytes of room over
    const unsigned int nodes_more = given.nodes + 2u;
    const unsigned long long text_more = length + 2ull;
    CliJson settings;
    CliJson state;
    cli_json_begun(&settings, NULL, 0u, NULL, 0ull);
    cli_json_begun(&state, NULL, 0u, NULL, 0ull);
    unsigned int settings_map = CLI_JSON_NONE;
    unsigned int state_map = CLI_JSON_NONE;
    char path[CLI_PATH_MAX_LEN];
    int done = 1;
    for (unsigned int change = given.node[changed].first; done && (change != CLI_JSON_NONE);
         change = given.node[change].next)
    {
        const char *const key = given.text + given.node[change].key;
        unsigned long long text_length = 0ull;
        const char *const text = cli_json_string(&given, change, &text_length);
        const unsigned int place = cli_state_place(key);
        if ((place == CLI_STATE_SETTINGS) || (place == CLI_STATE_FILE))
        {
            const int is_settings = place == CLI_STATE_SETTINGS;
            CliJson *const json = is_settings ? &settings : &state;
            unsigned int *const map = is_settings ? &settings_map : &state_map;
            if (*map == CLI_JSON_NONE)
            {
                cli_in(folder, is_settings ? "settings.json" : "state.json", path);
                *map = cli_state_map_read(json, path, nodes_more, text_more);
                // a file there holding more than an empty object that reads as holding nothing was not read whole: it
                // is too large for its cell, no cell was left for it, or its text is not a JSON object; a save over it
                // would drop every entry it holds, and is refused
                unsigned long long stored = 0ull;
                if ((*map != CLI_JSON_NONE) && (json->node[*map].count == 0u) && cli_file_size(path, &stored) && (stored > 3ull))
                {
                    mmgr_line_put(err_msg, path);
                    mmgr_line_put(err_msg, ": its entries could not be read whole, and a save over them would lose them");
                    done = 0;
                    continue;
                }
            }
            if (*map == CLI_JSON_NONE)
            {
                mmgr_line_put(err_msg, path);
                mmgr_line_put(err_msg, ": the cells it is read into could not be taken");
                done = 0;
            }
            else if (text != NULL)
            {
                cli_json_set(json, *map, key, cli_written(json, text, text_length));
            }
            else
            {
                cli_json_remove(json, *map, key);
            }
        }
        else if (place == CLI_STATE_THEMES)
        {
            char themes[CLI_PATH_MAX_LEN];
            cli_in(folder, "themes", themes);
            done = cli_themes_saved(themes, text, text_length, err_msg);
        }
        else
        {
            char file[24];
            char backups[CLI_PATH_MAX_LEN];
            cli_backup_file(key, file);
            cli_in(folder, "backups", backups);
            cli_in(backups, file, path);
            if (text != NULL)
            {
                // the backup: an object of two members, its key and its text, and their names
                CliJson backup;
                const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
                if (!cli_json_reserved(&backup, 8u, key_length + text_length + 16ull, "a change not saved"))
                {
                    mmgr_line_put(err_msg, path);
                    mmgr_line_put(err_msg, ": the cells it is written from could not be taken");
                    done = 0;
                    continue;
                }
                const unsigned int object = cli_json_made(&backup, CFG_JSON_OBJECT);
                cli_json_set(&backup, object, "key", cli_json_string_made(&backup, key, key_length));
                cli_json_set(&backup, object, "text", cli_json_string_made(&backup, text, text_length));
                done = cli_map_written(path, &backup, object, err_msg);
                cli_json_given(&backup);
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
        done = cli_map_written(path, &settings, settings_map, err_msg);
    }
    cli_in(folder, "state.json", path);
    if (done && (state_map != CLI_JSON_NONE))
    {
        done = cli_map_written(path, &state, state_map, err_msg);
    }
    else if (done && !query_is_file(path))
    {
        done = cli_whole_written(path, "{}\n", 3ull, err_msg);
    }
    cli_json_given(&state);
    cli_json_given(&settings);
    cli_json_given(&given);
    cli_lock_give(&s_lock);
    return done;
}
