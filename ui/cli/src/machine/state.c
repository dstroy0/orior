// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// state.c: where each of the window's entries is stored, and every entry read back from orior's own folder
#include "../command_line/callback_cfg.h"
#include "../command_line/commands.h"
#include "../command_line/pool.h"
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "settings.h"
#include "state_internal.h"

// the entry that contains every theme of the reader's, and the start of each change not saved
static const char s_themes[] = "themes";

static const char s_backup[] = "backup.";

unsigned int cli_state_position(const CliStateCtx *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = s_themes, .cap = CLI_POOL_BYTES}))
    {
        return CLI_STATE_THEMES;
    }
    if (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->key, .other_pointer = s_backup,
                   .size = sizeof(s_backup) - 1u))
    {
        return CLI_STATE_BACKUP;
    }
    CliItem node;
    return (cli_setting_node(&(CliCommandsMenuCfg){.word_text = args->key, .item = &node}) && node.setting)
               ? CLI_STATE_SETTINGS
               : CLI_STATE_FILE;
}

int cli_state_is_synced(const CliStateCfg *args)
{
    const unsigned int place = EMBED_CALL(cli_state_internal.place, CliStateCtx, .key = args->key);
    return (place == CLI_STATE_SETTINGS) || (place == CLI_STATE_THEMES);
}

void cli_state_read_back(const CliStateJsonCfg *args)
{
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->json, .node = args->value, .length_pointer = &length);
    if (text != NULL)
    {
        mmgr_line_put_n(args->out, text, length);
    }
    else
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json,
                   .node = args->value, .out = args->out, .indent = 0u, .sorted = 0);
    }
}

unsigned int cli_state_map_read(const CliStateJsonCtx *args)
{
    // the file mapped and made plain in a cell no larger than it; a file larger than CLI_STATE_BYTES, or one with no
    // cell left to read it in, reads as one that is not there
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = g_cli_state_owner);
    char *const plain_text = ((bytes != NULL) && (size <= CLI_STATE_BYTES))
                                 ? g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_state_owner)
                                 : NULL;
    unsigned int root = CLI_JSON_NONE;
    if (plain_text != NULL)
    {
        VerbaLinea plain = {plain_text, size + 1ULL, 0ULL};
        // the file's bytes are its text
        EMBED_CALL(cli_devcontainer_text.plain_json, CliDevcontainerTextCfg, .text = (const char *)bytes, .size = size,
                   .plain = &plain);
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        root = mmgr_line_ok(&plain)
                   ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                .json = args->json, .text = plain_text, .size = plain.at,
                                .nodes_more = args->nodes_more + 1u, .text_more = args->text_more, .err_msg = &err_msg,
                                .owner = g_cli_state_owner)
                   : CLI_JSON_NONE;
    }
    g_cli_guard->persistent_buf_release(plain_text, g_cli_state_owner);
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_state_owner));
    if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json, .node = root,
                   .type = CFG_JSON_OBJECT))
    {
        return root;
    }
    // an empty object in a tree of its own, with the capacity over requested
    g_cli_guard->persistent_buf_release(args->json->node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), args->json->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = args->json,
                      .nodes_max = args->nodes_more + 1u, .text_max = args->text_more,
                      .what = "an empty stored file's tree", .owner = g_cli_state_owner)
               ? EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = args->json,
                            .type = CFG_JSON_OBJECT)
               : CLI_JSON_NONE;
}

// A folder's walk for its JSON files: the files, and the count of those it includes, read before they are put
typedef struct
{
    CliJsonFiles *files;
    unsigned int capacity;
} CliJsonFilesWalk;

typedef struct
{
    const char *name;
    int is_folder;
    const char *folder;
    char *out;
    CliJson *entries;
    unsigned int object;
    const char *key;
    const char *text;
    unsigned long long length;
    const char *path;
    const CliJsonFiles *files;
} StateCtx;
// whether a folder's entry is a JSON file
EMBED_INLINE int cli_json_file_include(const StateCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    return !args->is_folder && (length >= 5ULL) && (length < CLI_NAME_MAX_LEN) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 5ULL, .other = ".json", .cap = CLI_POOL_BYTES}));
}

static void cli_json_file_count(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    int is_folder = args->is_folder;
    CliJsonFilesWalk *const walk = (CliJsonFilesWalk *)args->context;
    walk->capacity += (EMBED_CALL(cli_json_file_include, StateCtx, .name = name, .is_folder = is_folder) &&
                       (walk->capacity < CLI_STATE_FILES))
                          ? 1u
                          : 0u;
}

// a JSON file's name put among a folder's, in order
static void cli_json_file_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    int is_folder = args->is_folder;
    CliJsonFilesWalk *const walk = (CliJsonFilesWalk *)args->context;
    CliJsonFiles *const files = walk->files;
    if (!EMBED_CALL(cli_json_file_include, StateCtx, .name = name, .is_folder = is_folder) ||
        (files->count >= walk->capacity))
    {
        return;
    }
    unsigned int place = files->count;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = files->name[place - 1u],
                                       .other_pointer = name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = files->name[place],
            .src = files->name[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->name[place - 1u], .cap = CLI_POOL_BYTES}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = files->name[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}) + 1u});
    files->count += 1u;
}

void cli_state_json_files(const CliStateFolderCtx *args)
{
    args->files->count = 0u;
    CliJsonFilesWalk walk = {args->files, 0u};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = cli_json_file_count, .context_bytess = &walk);
    args->files->name =
        (walk.capacity != 0u)
            ? g_cli_guard->temporary_buf_alloc((unsigned long long)walk.capacity * CLI_NAME_MAX_LEN, g_cli_state_owner)
            : NULL;
    walk.capacity = (args->files->name != NULL) ? walk.capacity : 0u;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = cli_json_file_each, .context_bytess = &walk);
}

// `name` under `folder` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_in(const StateCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

// an entry put in the entries' object as its text, in the place of one it had
EMBED_INLINE void cli_entry_append(const StateCtx *args)
{
    unsigned int object = args->object;
    const char *text = args->text;
    unsigned long long length = args->length;
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->entries, .object = object,
               .name = args->key,
               .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                   .json = args->entries, .text = text, .length = length));
}

// the bytes of the file at `path`, 0 where it is not there
EMBED_INLINE unsigned long long cli_state_size(const StateCtx *args)
{
    const char *path = args->path;
    unsigned long long size = 0ULL;
    return EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size) ? size
                                                                                                             : 0ULL;
}

// the bytes of the JSON files of `folder` together
EMBED_INLINE unsigned long long cli_state_files_size(const StateCtx *args)
{
    unsigned long long size = 0ULL;
    for (unsigned int at = 0u; at < args->files->count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_in, StateCtx, .folder = args->folder, .name = args->files->name[at], .out = path);
        size += EMBED_CALL(cli_state_size, StateCtx, .path = path);
    }
    return size;
}

int cli_state_read(const CliStateOutCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_in, StateCtx, .folder = args->folder, .name = "state.json", .out = path);
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        return 0;
    }
    static const char *const s_files[2] = {"settings.json", "state.json"};
    char themes_folder[CLI_PATH_MAX_LEN];
    char backups[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_in, StateCtx, .folder = args->folder, .name = "themes", .out = themes_folder);
    EMBED_CALL(cli_in, StateCtx, .folder = args->folder, .name = "backups", .out = backups);
    CliJsonFiles themes_files = {NULL, 0u};
    CliJsonFiles backup_files = {NULL, 0u};
    EMBED_CALL(cli_state_folder_internal.json_files, CliStateFolderCtx, .folder = themes_folder,
               .files = &themes_files);
    EMBED_CALL(cli_state_folder_internal.json_files, CliStateFolderCtx, .folder = backups, .files = &backup_files);
    // every entry read is a key and a text of the files read: the entries' tree contains at max as many nodes as
    // their bytes, and at max twice them in text, a value written back in place of its text
    unsigned long long bytes =
        EMBED_CALL(cli_state_files_size, StateCtx, .folder = themes_folder, .files = &themes_files) +
        EMBED_CALL(cli_state_files_size, StateCtx, .folder = backups, .files = &backup_files);
    for (unsigned int file = 0u; file < 2u; file += 1u)
    {
        EMBED_CALL(cli_in, StateCtx, .folder = args->folder, .name = s_files[file], .out = path);
        bytes += EMBED_CALL(cli_state_size, StateCtx, .path = path);
    }
    bytes = (bytes < (CLI_STATE_BYTES * 4ULL)) ? bytes : (CLI_STATE_BYTES * 4ULL);
    CliJson entries;
    if (!EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &entries,
                    .nodes_max = (unsigned int)bytes + 16u, .text_max = (2ULL * bytes) + 64ULL,
                    .what = "the changes stored read", .owner = g_cli_state_owner))
    {
        g_cli_guard->persistent_buf_release(backup_files.name, g_cli_state_owner);
        backup_files.name = NULL;
        backup_files.count = 0u;
        g_cli_guard->persistent_buf_release(themes_files.name, g_cli_state_owner);
        themes_files.name = NULL;
        themes_files.count = 0u;
        return 0;
    }
    const unsigned int object =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = &entries, .type = CFG_JSON_OBJECT);
    for (unsigned int file = 0u; file < 2u; file += 1u)
    {
        CliJson map;
        EMBED_CALL(cli_in, StateCtx, .folder = args->folder, .name = s_files[file], .out = path);
        const unsigned int read = EMBED_CALL(cli_state_json_internal.map_read, CliStateJsonCtx, .json = &map,
                                             .path = path, .nodes_more = 0u, .text_more = 0ULL);
        // the one cell each member of the file is written back in, of the max the file's tree writes to
        const unsigned long long capacity =
            EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = &map);
        char *const value_text = g_cli_guard->temporary_buf_alloc(capacity, g_cli_state_owner);
        for (unsigned int member = (read != CLI_JSON_NONE) ? map.node[read].first : CLI_JSON_NONE;
             member != CLI_JSON_NONE; member = map.node[member].next)
        {
            const char *const key = map.text + map.node[member].key;
            if ((file == 0u) && EMBED_CALL(cli_settings_key.is_file_key, CliSettingsKeyCfg, .key = key))
            {
                continue;
            }
            VerbaLinea value = mmgr_line_over(value_text, capacity);
            EMBED_CALL(cli_state_json.read_back, CliStateJsonCfg, .json = &map, .value = member, .out = &value);
            const int whole = mmgr_line_ok(&value);
            EMBED_CALL(cli_entry_append, StateCtx, .entries = &entries, .object = object, .key = key,
                       .text = whole ? value.out : "", .length = whole ? value.at : 0ULL);
        }
        g_cli_guard->persistent_buf_release(value_text, g_cli_state_owner);
        g_cli_guard->persistent_buf_release(map.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), map.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&map),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    }
    // the themes, each file's object, as one JSON array in a cell of the max they write to: a tree written on one line
    // is at max the bytes of the file it reads, each with the array's mark before it
    const unsigned long long themes_capacity =
        (EMBED_CALL(cli_state_files_size, StateCtx, .folder = themes_folder, .files = &themes_files) * 2ULL) +
        (themes_files.count * 2ULL) + 16ULL;
    char *const themes_text = g_cli_guard->temporary_buf_alloc(themes_capacity, g_cli_state_owner);
    VerbaLinea themes = mmgr_line_over(themes_text, themes_capacity);
    unsigned int themed = 0u;
    for (unsigned int at = 0u; at < themes_files.count; at += 1u)
    {
        CliJson map;
        EMBED_CALL(cli_in, StateCtx, .folder = themes_folder, .name = themes_files.name[at], .out = path);
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &map,
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        const unsigned int theme =
            (EMBED_CALL(cli_state_size, StateCtx, .path = path) <= CLI_STATE_BYTES)
                ? EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &map, .path = path,
                             .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_state_owner)
                : CLI_JSON_NONE;
        if (theme != CLI_JSON_NONE)
        {
            mmgr_line_ch(&themes, (themed == 0u) ? '[' : ',');
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &map, .node = theme,
                       .out = &themes, .indent = 0u, .sorted = 0);
            themed += 1u;
        }
        g_cli_guard->persistent_buf_release(map.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), map.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&map),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    }
    mmgr_line_ch(&themes, ']');
    if (themed != 0u)
    {
        const int whole = mmgr_line_ok(&themes);
        EMBED_CALL(cli_entry_append, StateCtx, .entries = &entries, .object = object, .key = s_themes,
                   .text = whole ? themes.out : "", .length = whole ? themes.at : 0ULL);
    }
    g_cli_guard->persistent_buf_release(themes_text, g_cli_state_owner);
    // each change not saved: its key and its text
    for (unsigned int at = 0u; at < backup_files.count; at += 1u)
    {
        CliJson map;
        EMBED_CALL(cli_in, StateCtx, .folder = backups, .name = backup_files.name[at], .out = path);
        const unsigned int backup = EMBED_CALL(cli_state_json_internal.map_read, CliStateJsonCtx, .json = &map,
                                               .path = path, .nodes_more = 0u, .text_more = 0ULL);
        const char *const key =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &map,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &map,
                                          .object = backup, .name = "key"),
                       .length_pointer = NULL);
        unsigned long long length = 0ULL;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &map,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &map,
                                          .object = backup, .name = "text"),
                       .length_pointer = &length);
        if ((key != NULL) && (text != NULL))
        {
            EMBED_CALL(cli_entry_append, StateCtx, .entries = &entries, .object = object, .key = key, .text = text,
                       .length = length);
        }
        g_cli_guard->persistent_buf_release(map.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), map.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&map),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    }
    g_cli_guard->persistent_buf_release(backup_files.name, g_cli_state_owner);
    backup_files.name = NULL;
    backup_files.count = 0u;
    g_cli_guard->persistent_buf_release(themes_files.name, g_cli_state_owner);
    themes_files.name = NULL;
    themes_files.count = 0u;
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = &entries, .object = object);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &entries, .node = object,
               .out = args->out, .indent = 0u, .sorted = 1);
    g_cli_guard->persistent_buf_release(entries.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), entries.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&entries),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return 1;
}

// the max bytes the entries of orior's folder grow to as they are read
#define CLI_STATE_ENTRIES_TEXT (6ULL << 20u)

CliStateEntries cli_state_entries_alloc(const CliStateEntriesCfg *args)
{
    CliStateEntries entries = {NULL, args->owner, 0u};
    CliGrownLine read;
    cli_grown_begin(&read, CLI_STATE_ENTRIES_TEXT, g_cli_state_owner);
    if ((read.cell == NULL) ||
        !EMBED_CALL(cli_state_out.read, CliStateOutCfg, .folder = args->folder, .out = &read.line) ||
        !mmgr_line_ok(&read.line))
    {
        cli_grown_end(&read);
        return entries;
    }
    char why[256];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    CliJson json;
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = read.cell, .size = cli_grown_size(&read), .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &ignored, .owner = g_cli_state_owner);
    cli_grown_end(&read);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json, .node = root,
                    .type = CFG_JSON_OBJECT))
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        return entries;
    }
    // the entries and their texts counted, then written into one movable cell, the entries first and the texts after
    unsigned int count = 0u;
    unsigned long long bytes = 0ULL;
    for (unsigned int member = json.node[root].first; member != CLI_JSON_NONE; member = json.node[member].next)
    {
        unsigned long long length = 0ULL;
        (void)EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = member, .length_pointer = &length);
        bytes += length + 1ULL +
                 cellul.len(&(CatenaFinitaCfg){.src = json.text + json.node[member].key, .cap = CLI_POOL_BYTES}) + 1ULL;
        count += 1u;
    }
    entries.cell = g_cli_guard->movable_buf_alloc(CLI_STATE_ENTRY_AT(count) + bytes + 1ULL, args->owner);
    if (entries.cell != NULL)
    {
        const char zero = '\0';
        unsigned long long place = CLI_STATE_ENTRY_AT(count);
        for (unsigned int member = json.node[root].first; member != CLI_JSON_NONE; member = json.node[member].next)
        {
            const char *const key = json.text + json.node[member].key;
            const unsigned long long key_size =
                cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1ULL;
            unsigned long long length = 0ULL;
            const char *const value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                 .json_read = &json, .node = member, .length_pointer = &length);
            CliStateEntry entry = {place, place + key_size};
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = entries.cell, .offset = place, .src = key, .bytes = key_size});
            place += key_size;
            if ((value != NULL) && (length > 0ULL))
            {
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = entries.cell, .offset = place, .src = value, .bytes = length});
            }
            place += (value != NULL) ? length : 0ULL;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = entries.cell, .offset = place, .src = &zero, .bytes = 1ULL});
            place += 1ULL;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = entries.cell,
                                                                     .offset = CLI_STATE_ENTRY_AT(entries.count),
                                                                     .src = &entry,
                                                                     .bytes = sizeof(entry)});
            entries.count += 1u;
        }
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return entries;
}

void cli_state_entries_release(CliStateEntries *entries)
{
    if (entries->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(entries->cell, entries->owner);
    }
    entries->cell = NULL;
    entries->count = 0u;
}

// the entry `at` read out of the entries' cell
static CliStateEntry state_entry(const CliStateEntries *entries, unsigned int at)
{
    CliStateEntry entry = {0ULL, 0ULL};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = entries->cell, .offset = CLI_STATE_ENTRY_AT(at), .dst = &entry, .bytes = sizeof(entry)});
    return entry;
}

// the text at `text_at` of the entries' cell read into `out`, `capacity` bytes, a 0 after it: `out`, or NULL where
// it does not fit
static const char *state_text(const CliStateEntries *entries, unsigned long long text_at, char *out,
                              unsigned long long capacity)
{
    const unsigned long long length = g_cli_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = entries->cell, .offset = text_at, .bytes = (embed_word)capacity});
    if (length >= capacity)
    {
        return NULL;
    }
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = entries->cell, .offset = text_at, .dst = out, .bytes = (embed_word)length});
    out[length] = '\0';
    return out;
}

// the place among the entries of the one with `key`, the count where none has it
static unsigned int state_entry_find(const CliStateEntries *entries, const char *key)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES});
    for (unsigned int at = 0u; (entries != NULL) && (entries->cell != NULL) && (at < entries->count); at += 1u)
    {
        const CliStateEntry entry = state_entry(entries, at);
        if (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                .cell = entries->cell, .offset = entry.key_at, .src = key, .bytes = (embed_word)(length + 1ULL)}) == 0)
        {
            return at;
        }
    }
    return (entries != NULL) ? entries->count : 0u;
}

const char *cli_state_entry_value(const CliStateEntries *entries, const char *key, char *out,
                                  unsigned long long capacity)
{
    const unsigned int at = state_entry_find(entries, key);
    if ((entries == NULL) || (at >= entries->count))
    {
        return NULL;
    }
    return state_text(entries, state_entry(entries, at).value_at, out, capacity);
}

unsigned long long cli_state_entry_size(const CliStateEntries *entries, const char *key)
{
    const unsigned int at = state_entry_find(entries, key);
    if ((entries == NULL) || (at >= entries->count))
    {
        return CLI_STATE_NONE;
    }
    return g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){
        .cell = entries->cell, .offset = state_entry(entries, at).value_at, .bytes = CLI_STATE_ENTRIES_TEXT});
}

const char *cli_state_entry_key(const CliStateEntries *entries, unsigned int at, char *out, unsigned long long capacity)
{
    if ((entries == NULL) || (entries->cell == NULL) || (at >= entries->count))
    {
        return NULL;
    }
    return state_text(entries, state_entry(entries, at).key_at, out, capacity);
}

const char *cli_state_entry_text(const CliStateEntries *entries, unsigned int at, char *out,
                                 unsigned long long capacity)
{
    if ((entries == NULL) || (entries->cell == NULL) || (at >= entries->count))
    {
        return NULL;
    }
    return state_text(entries, state_entry(entries, at).value_at, out, capacity);
}

int cli_state_value_save(const CliStateValueCfg *args)
{
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    const unsigned long long value_size = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_POOL_BYTES});
    // each byte escaped to six at max, and the marks about them
    const unsigned long long capacity = ((key_size + value_size) * 6ULL) + 16ULL;
    char *const changes_cell = g_cli_guard->temporary_buf_alloc(capacity, g_cli_state_owner);
    VerbaLinea changes = mmgr_line_over(changes_cell, capacity);
    mmgr_line_ch(&changes, '{');
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &changes, .text = args->key,
               .length = key_size);
    mmgr_line_ch(&changes, ':');
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &changes, .text = args->value,
               .length = value_size);
    mmgr_line_ch(&changes, '}');
    const int saved =
        mmgr_line_ok(&changes) && EMBED_CALL(cli_state_folder.save, CliStateFolderCfg, .folder = args->folder,
                                             .changes = changes.out, .length = changes.at, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(changes_cell, g_cli_state_owner);
    return saved;
}
