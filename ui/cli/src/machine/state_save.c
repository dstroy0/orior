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

typedef struct
{
    const char *folder;
    const char *name;
    char *out;
    const char *path;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
    CliJson *json;
    unsigned int object;
    unsigned long long length;
    const char *key;
} StateSaveCtx;
// `name` under `folder` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_in(const StateSaveCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

// `size` bytes of `text` written beside the file at `path` and moved over it, its folder made: 1, or 0 with why given
EMBED_INLINE int cli_atomic_write(const StateSaveCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long size = args->size;
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder) &&
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": it could not be made");
        return 0;
    }
    // the file's path with its extension put as `saving`
    char adjacent[CLI_PATH_MAX_LEN];
    const char *name = path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_end != 0ULL) ? (path + name_end) : name;
    const char *const dot =
        (name[0] != '\0')
            ? EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name + 1,
                         .length_value = cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}),
                         .byte = '.')
            : NULL;
    VerbaLinea line = {adjacent, sizeof(adjacent), 0ULL};
    mmgr_line_put_n(&line, path,
                    (dot != NULL) ? (unsigned long long)(dot - path)
                                  : cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ".saving");
    mmgr_line_finish(&line);
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = adjacent, .bytes = text,
                    .size = size) ||
        !EMBED_CALL(query_from_internal.file_rename, QueryFromCtx, .from = adjacent, .to = path))
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// an object written to `path` with its keys in order, as serde_json's pretty writer lays it out, a newline after
EMBED_INLINE int cli_map_write(const StateSaveCtx *args)
{
    CliJson *json = args->json;
    unsigned int object = args->object;
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = json, .object = object);
    // a file past the most read is one that never reads back
    const unsigned long long capacity = CLI_STATE_BYTES + 1ULL;
    char *const written = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_state_cells);
    VerbaLinea out = mmgr_line_over(written, capacity);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = object, .out = &out,
               .indent = 2u, .sorted = 1);
    mmgr_line_ch(&out, '\n');
    if (!mmgr_line_ok(&out))
    {
        g_cli_cells->persistent_buf_release(written, &g_cli_state_cells);
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it is larger than a stored file reads back, or its cell could not be taken");
        return 0;
    }
    const int whole = EMBED_CALL(cli_atomic_write, StateSaveCtx, .path = args->path, .text = out.out, .size = out.at,
                                 .err_msg = args->err_msg);
    g_cli_cells->persistent_buf_release(written, &g_cli_state_cells);
    return whole;
}

// an entry's text as it is written into `json`: its JSON where it is JSON other than a string that writes back the
// same, else the text itself as a string
EMBED_INLINE unsigned int cli_write(const StateSaveCtx *args)
{
    CliJson *json = args->json;
    const char *text = args->text;
    unsigned long long length = args->length;
    const unsigned int mark = json->nodes;
    const unsigned long long used = json->used;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    // the tokens the entry is read through, as many as it contains; and the entry written back, which matches it only
    // where it is as long as it, which a cell one longer fits at every match
    const unsigned int tokens_max =
        EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = text, .size = length) + 1u;
    CfgJsonToken *const tokens =
        g_cli_cells->temporary_buf_alloc((unsigned long long)tokens_max * sizeof(CfgJsonToken), &g_cli_state_cells);
    char *const back_text = g_cli_cells->temporary_buf_alloc(length + 2ULL, &g_cli_state_cells);
    const unsigned int value =
        ((tokens != NULL) && (back_text != NULL))
            ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = json,
                         .text = text, .size = length, .tokens = tokens, .tokens_max = tokens_max, .err_msg = &err_msg)
            : CLI_JSON_NONE;
    int same = 0;
    if ((value != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = json, .node = value, .kind = CFG_JSON_STRING))
    {
        VerbaLinea back = {back_text, length + 2ULL, 0ULL};
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = value,
                   .out = &back, .indent = 0u, .sorted = 0);
        same = mmgr_line_ok(&back) && (back.at == length) &&
               (memor.cmp(&(MemoriaCfg){.src = back_text, .other = text, .bytes = length}) == 0);
    }
    g_cli_cells->persistent_buf_release(back_text, &g_cli_state_cells);
    g_cli_cells->persistent_buf_release(tokens, &g_cli_state_cells);
    if (same)
    {
        return value;
    }
    // the nodes and texts the reading made taken back
    json->nodes = mark;
    json->used = used;
    return EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = json,
                      .text = text, .length = length);
}

// a theme's file name: its name with each letter a file name cannot contain, or should not, made `_`, `.` and spaces
// trimmed from its ends, `theme` where nothing is left, then `.json`, into `out`, sized for CLI_NAME_MAX_LEN
EMBED_INLINE void cli_theme_file(const StateSaveCtx *args)
{
    char *out = args->out;
    char safe[CLI_NAME_MAX_LEN];
    VerbaLinea line = {safe, sizeof(safe), 0ULL};
    EMBED_CALL(cli_text_classes.append_save, CliTextClassesCfg, .out = &line, .text = args->name,
               .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS | CLI_TEXT_END_ASCII, .set = " -_.", .other = '_');
    const unsigned long long length = mmgr_line_finish(&line);
    const char *from = safe;
    const char *to = safe + length;
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = ". ");
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = ". ");
    VerbaLinea file = {out, CLI_NAME_MAX_LEN, 0ULL};
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
EMBED_INLINE void cli_backup_file(const StateSaveCtx *args)
{
    unsigned long long hash = 0xcbf29ce484222325ULL;
    for (const char *at = args->key; at[0] != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ULL;
    }
    VerbaLinea line = {args->out, 22u, 0ULL};
    mmgr_line_hex(&line, hash, 16u);
    mmgr_line_put(&line, ".json");
    mmgr_line_finish(&line);
}

// each theme in the list written to a file of its own, and the files of themes no longer in it taken out
EMBED_INLINE int cli_themes_save(const StateSaveCtx *args)
{
    const char *folder = args->folder;
    const char *text = args->text;
    unsigned long long length = args->length;
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int themes =
        (text != NULL) ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                                    .json = &json, .text = text, .size = length, .nodes_more = 0u, .text_more = 0ULL,
                                    .err_msg = &unread, .owner = &g_cli_state_cells)
                       : CLI_JSON_NONE;
    // the files written, one a theme, in a cell of their count
    const unsigned int count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                          .node = themes, .kind = CFG_JSON_ARRAY)
                                   ? json.node[themes].count
                                   : 0u;
    char (*const saved_names)[CLI_NAME_MAX_LEN] =
        (count != 0u)
            ? g_cli_cells->temporary_buf_alloc((unsigned long long)count * CLI_NAME_MAX_LEN, &g_cli_state_cells)
            : NULL;
    // the one cell each theme is written in, of the most a stored file reads back
    const unsigned long long capacity = CLI_STATE_BYTES + 1ULL;
    char *const written = (count != 0u) ? g_cli_cells->temporary_buf_alloc(capacity, &g_cli_state_cells) : NULL;
    unsigned int names = 0u;
    for (unsigned int theme = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                         .node = themes, .kind = CFG_JSON_ARRAY)
                                  ? json.node[themes].first
                                  : CLI_JSON_NONE;
         theme != CLI_JSON_NONE; theme = json.node[theme].next)
    {
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = theme, .name = "name"),
                       .length_pointer = NULL);
        char base[CLI_NAME_MAX_LEN];
        EMBED_CALL(cli_theme_file, StateSaveCtx, .name = (named != NULL) ? named : "theme", .out = base);
        char name[CLI_NAME_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = name,
                                .src = base,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
        for (unsigned int inner_count = 2u;; inner_count += 1u)
        {
            int reserved = 0;
            unsigned int at = 0u;
            while (!reserved && (at < names))
            {
                reserved = cellul.eq(&(CatenaFinitaCfg){.src = saved_names[at], .other = name, .cap = CLI_CELLS_POOL});
                at += 1u;
            }
            if (!reserved)
            {
                break;
            }
            VerbaLinea counted = {name, sizeof(name), 0ULL};
            mmgr_line_put_n(&counted, base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) - 5u);
            mmgr_line_ch(&counted, ' ');
            mmgr_line_decimal(&counted, inner_count, 1u);
            mmgr_line_put(&counted, ".json");
            mmgr_line_finish(&counted);
        }
        VerbaLinea out = mmgr_line_over(written, capacity);
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = theme,
                   .out = &out, .indent = 2u, .sorted = 0);
        mmgr_line_ch(&out, '\n');
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_in, StateSaveCtx, .folder = folder, .name = name, .out = path);
        const int whole = mmgr_line_ok(&out) && EMBED_CALL(cli_atomic_write, StateSaveCtx, .path = path,
                                                           .text = out.out, .size = out.at, .err_msg = err_msg);
        if (!whole)
        {
            g_cli_cells->persistent_buf_release(written, &g_cli_state_cells);
            g_cli_cells->persistent_buf_release(saved_names, &g_cli_state_cells);
            EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
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
    g_cli_cells->persistent_buf_release(written, &g_cli_state_cells);
    CliJsonFiles files = {NULL, 0u};
    EMBED_CALL(cli_state_folder_internal.json_files, CliStateFolderCtx, .folder = folder, .files = &files);
    for (unsigned int at = 0u; at < files.count; at += 1u)
    {
        int saved_count = 0;
        unsigned int each = 0u;
        while (!saved_count && (each < names))
        {
            saved_count =
                cellul.eq(&(CatenaFinitaCfg){.src = saved_names[each], .other = files.name[at], .cap = CLI_CELLS_POOL});
            each += 1u;
        }
        if (!saved_count)
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_in, StateSaveCtx, .folder = folder, .name = files.name[at], .out = path);
            EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
        }
    }
    EMBED_CALL(cli_state_folder_internal.json_files_free, CliStateFolderCtx, .files = &files);
    g_cli_cells->persistent_buf_release(saved_names, &g_cli_state_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}

int cli_state_save(const CliStateFolderCfg *args)
{
    CliJson supplied;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &supplied,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int changed =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                   .json = &supplied, .text = args->changes, .size = args->length, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = args->err_msg, .owner = &g_cli_state_cells);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &supplied, .node = changed,
                    .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &supplied);
        return 0;
    }
    // what the changes put in the settings or the state: each a key and an entry's text, read as JSON or as a string:
    // a node for each 2 bytes of it at the most, and its bytes
    const unsigned int nodes_more = supplied.nodes + (unsigned int)(args->length / 2ULL) + 2u;
    const unsigned long long text_more = args->length + 2ULL;
    CliJson settings;
    CliJson state;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &settings,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &state,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    unsigned int settings_map = CLI_JSON_NONE;
    unsigned int state_map = CLI_JSON_NONE;
    char path[CLI_PATH_MAX_LEN];
    int done = 1;
    for (unsigned int change = supplied.node[changed].first; done && (change != CLI_JSON_NONE);
         change = supplied.node[change].next)
    {
        const char *const key = supplied.text + supplied.node[change].key;
        unsigned long long text_length = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &supplied, .node = change, .length_pointer = &text_length);
        const unsigned int place = EMBED_CALL(cli_state_internal.place, CliStateCtx, .key = key);
        if ((place == CLI_STATE_SETTINGS) || (place == CLI_STATE_FILE))
        {
            const int is_settings = place == CLI_STATE_SETTINGS;
            CliJson *const json = is_settings ? &settings : &state;
            unsigned int *const map = is_settings ? &settings_map : &state_map;
            if (*map == CLI_JSON_NONE)
            {
                EMBED_CALL(cli_in, StateSaveCtx, .folder = args->folder,
                           .name = is_settings ? "settings.json" : "state.json", .out = path);
                *map = EMBED_CALL(cli_state_json_internal.map_read, CliStateJsonCtx, .json = json, .path = path,
                                  .nodes_more = nodes_more, .text_more = text_more);
                // a file there holding more than an empty object that reads as holding nothing was not read whole: it
                // is too large for its cell, no cell was left for it, or its text is not a JSON object; a save over it
                // would drop every entry it holds, and is refused
                unsigned long long stored = 0ULL;
                if ((*map != CLI_JSON_NONE) && (json->node[*map].count == 0u) &&
                    EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path,
                               .size_pointer = &stored) &&
                    (stored > 3ULL))
                {
                    mmgr_line_put(args->err_msg, path);
                    mmgr_line_put(args->err_msg,
                                  ": its entries could not be read whole, and a save over them would lose them");
                    done = 0;
                    continue;
                }
            }
            if (*map == CLI_JSON_NONE)
            {
                mmgr_line_put(args->err_msg, path);
                mmgr_line_put(args->err_msg, ": the cells it is read into could not be taken");
                done = 0;
            }
            else if (text != NULL)
            {
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = *map,
                           .name = key,
                           .value =
                               EMBED_CALL(cli_write, StateSaveCtx, .json = json, .text = text, .length = text_length));
            }
            else
            {
                EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json,
                           .object = *map, .name = key);
            }
        }
        else if (place == CLI_STATE_THEMES)
        {
            char themes[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_in, StateSaveCtx, .folder = args->folder, .name = "themes", .out = themes);
            done = EMBED_CALL(cli_themes_save, StateSaveCtx, .folder = themes, .text = text, .length = text_length,
                              .err_msg = args->err_msg);
        }
        else
        {
            char file[24];
            char backups[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_backup_file, StateSaveCtx, .key = key, .out = file);
            EMBED_CALL(cli_in, StateSaveCtx, .folder = args->folder, .name = "backups", .out = backups);
            EMBED_CALL(cli_in, StateSaveCtx, .folder = backups, .name = file, .out = path);
            if (text != NULL)
            {
                // the backup: an object of two members, its key and its text, and their names
                CliJson backup;
                const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
                if (!EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &backup,
                                .nodes_max = 8u, .text_max = key_length + text_length + 16ULL,
                                .what = "a change not saved", .owner = &g_cli_state_cells))
                {
                    mmgr_line_put(args->err_msg, path);
                    mmgr_line_put(args->err_msg, ": the cells it is written from could not be taken");
                    done = 0;
                    continue;
                }
                const unsigned int object = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg,
                                                       .json = &backup, .kind = CFG_JSON_OBJECT);
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &backup,
                           .object = object, .name = "key",
                           .value =
                               EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                          .json = &backup, .text = key, .length = key_length));
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &backup,
                           .object = object, .name = "text",
                           .value =
                               EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                          .json = &backup, .text = text, .length = text_length));
                done = EMBED_CALL(cli_map_write, StateSaveCtx, .path = path, .json = &backup, .object = object,
                                  .err_msg = args->err_msg);
                EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &backup);
            }
            else
            {
                EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
            }
        }
    }
    if (done && (settings_map != CLI_JSON_NONE))
    {
        EMBED_CALL(cli_in, StateSaveCtx, .folder = args->folder, .name = "settings.json", .out = path);
        done = EMBED_CALL(cli_map_write, StateSaveCtx, .path = path, .json = &settings, .object = settings_map,
                          .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_in, StateSaveCtx, .folder = args->folder, .name = "state.json", .out = path);
    if (done && (state_map != CLI_JSON_NONE))
    {
        done = EMBED_CALL(cli_map_write, StateSaveCtx, .path = path, .json = &state, .object = state_map,
                          .err_msg = args->err_msg);
    }
    else if (done && !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        done = EMBED_CALL(cli_atomic_write, StateSaveCtx, .path = path, .text = "{}\n", .size = 3ULL,
                          .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &state);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &settings);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &supplied);
    return done;
}
