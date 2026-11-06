// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_path.c: the tree whose own tools come first, its environment, the PATH's folders as orior_cli reads them,
// the PATH every step runs with, and a folder added to the reader's own PATH
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "toolchains_internal.h"

// how long the login shell is given to report its PATH, in milliseconds
#define CLI_SHELL_PATIENCE 10000ULL

#if defined(_WIN32)
#define CLI_SEPARATOR ';'
#else
#define CLI_SEPARATOR ':'
#endif

// the tree open, its environment, and the PATH the reader's login shell sets, read once on a thread of its own the
// first time the PATH's folders are requested and none until it is read: a program opened from the desktop starts with
// less of a PATH than a terminal gives, and a slow shell profile blocks nothing
static char s_tree[CLI_PATH_MAX_LEN];
static int s_has_tree;
static CliTreeEnvironment s_environment;
static int s_has_environment;
static char s_shell_path[CLI_PATH_TEXT];
static int s_shell_queried;
static int s_shell_read;
static CliThread s_shell_thread;

void cli_toolchains_set_tree(const CliToolchainsRootCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = s_tree,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    s_has_tree = 1;
}

unsigned int cli_toolchains_tree_tools(const CliToolchainsCountCfg *args)
{
    static const char *const s_folders[CLI_TREE_TOOLS] = {"/node_modules/.bin", "/vendor/bin"};
    char root[CLI_PATH_MAX_LEN];
    const int has = s_has_tree;
    memor.cpy(&(MemoriaCfg){.dst = root,
                            .src = s_tree,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_tree, .cap = CLI_POOL_BYTES}) + 1u});
    unsigned int count = 0u;
    for (unsigned int at = 0u; has && (at < CLI_TREE_TOOLS) && (count < args->capacity); at += 1u)
    {
        VerbaLinea line = {args->folders[count], CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, root);
        mmgr_line_put(&line, s_folders[at]);
        count += ((mmgr_line_finish(&line) != 0ULL) &&
                  EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->folders[count]))
                     ? 1u
                     : 0u;
    }
    return count;
}

void cli_toolchains_set_environment(const CliToolchainsEnvironmentCfg *args)
{
    s_has_environment = args->environment != NULL;
    if (args->environment != NULL)
    {
        s_environment = *args->environment;
    }
}

int cli_toolchains_environment(const CliToolchainsCfg *args)
{
    CliTreeEnvironment *environment = args->environment_out;
    const int has = s_has_environment;
    if (has)
    {
        *environment = s_environment;
    }
    return has;
}

// The PATH `shell` sets as a login shell, or none where it reports none within the patience: 1, or 0
int cli_toolchains_path_of_shell(const CliToolchainsShellCtx *args)
{
    const char *const words[5] = {args->shell, "-l", "-c", "printf '%s' \"$PATH\"", NULL};
    CliSpawned spawned;
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = words,
                    .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg))
    {
        return 0;
    }
    const unsigned long long began = cli_clock_milliseconds();
    long long code = 0LL;
    while (!EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &spawned, .code = &code))
    {
        if ((cli_clock_milliseconds() - began) >= CLI_SHELL_PATIENCE)
        {
            EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = spawned.id);
            EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
            EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
            return 0;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
    unsigned long long read = 0ULL;
    for (;;)
    {
        const unsigned long long got =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = spawned.out, .piece = args->path + read, .capacity = args->size - read - 1ULL);
        if (got == 0ULL)
        {
            break;
        }
        read += got;
    }
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    read = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->path, .at = read,
                      .set = CLI_TEXT_WHITE);
    args->path[read] = '\0';
    return read != 0ULL;
}

static void cli_shell_read_state(void *context)
{
    const char *const shell = (const char *)context;
    char *const text = g_cli_guard->temporary_buf_alloc(CLI_PATH_TEXT, g_cli_toolchains_owner);
    const int read = (text != NULL) && EMBED_CALL(cli_toolchains_shell_internal.path_of_shell, CliToolchainsShellCtx,
                                                  .shell = shell, .path = text, .size = CLI_PATH_TEXT);
    if (read)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_shell_path,
                                .src = text,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}) + 1u});
        s_shell_read = 1;
    }
    g_cli_guard->persistent_buf_release(text, g_cli_toolchains_owner);
}

typedef struct
{
    char *path;
    unsigned long long size;
    const char *text;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
    unsigned int list_entries;
    const char (*from)[CLI_PATH_MAX_LEN];
} ToolchainsPathCtx;
// the login shell's PATH into `path`: 1 once it has been read, its reading begun the first time it is requested
EMBED_INLINE int cli_shell_path(const ToolchainsPathCtx *args)
{
#if defined(_WIN32)
    return 0;
#else
    static char s_shell[CLI_PATH_MAX_LEN];
    const int query = !s_shell_queried;
    s_shell_queried = 1;
    const int read = s_shell_read;
    if (read)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->path,
                          .src = s_shell_path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_shell_path, .cap = CLI_POOL_BYTES}) + 1u});
    }
    if (query && EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "SHELL",
                            .value_out = s_shell, .capacity = sizeof(s_shell)))
    {
        s_shell_thread.run = cli_shell_read_state;
        s_shell_thread.context = s_shell;
        EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_shell_thread);
    }
    return read;
#endif
}

// each folder of a PATH's text put after the folders, each once by its key: the count after
EMBED_INLINE unsigned int cli_split(const ToolchainsPathCtx *args)
{
    unsigned int count = args->count;
    const char *from = args->text;
    for (const char *at = args->text;; at += 1)
    {
        if ((at[0] != CLI_SEPARATOR) && (at[0] != '\0'))
        {
            continue;
        }
        char folder[CLI_PATH_MAX_LEN];
        VerbaLinea line = {folder, sizeof(folder), 0ULL};
        mmgr_line_put_n(&line, from, (unsigned long long)(at - from));
        if ((at > from) && (mmgr_line_finish(&line) != 0ULL))
        {
            char key[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = folder, .key = key);
            int seen = 0;
            unsigned int each = 0u;
            while (!seen && (each < count))
            {
                char path_buf[CLI_PATH_MAX_LEN];
                EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = args->folders[each],
                           .key = path_buf);
                seen = cellul.eq(&(CatenaFinitaCfg){.src = path_buf, .other = key, .cap = CLI_POOL_BYTES});
                each += 1u;
            }
            if (!seen && (count < args->capacity))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = args->folders[count],
                                  .src = folder,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}) + 1u});
                count += 1u;
            }
        }
        if (at[0] == '\0')
        {
            break;
        }
        from = at + 1;
    }
    return count;
}

// the system's and the reader's PATH as the registry stores them now, put after the folders: the count after
EMBED_INLINE unsigned int cli_registry_folders(const ToolchainsPathCtx *args)
{
    unsigned int count = args->count;
    unsigned int capacity = args->capacity;
    // the PATH as the registry stores it and as it reads expanded, a cell of the two
    char *const registry_buf = g_cli_guard->temporary_buf_alloc(2ULL * CLI_PATH_TEXT, g_cli_toolchains_owner);
    if (registry_buf != NULL)
    {
        char *const expanded = registry_buf + CLI_PATH_TEXT;
        if (EMBED_CALL(cli_out_internal.registry_machine_path, CliOutCtx, .out = registry_buf,
                       .capacity = CLI_PATH_TEXT))
        {
            EMBED_CALL(cli_out_internal.registry_expand, CliOutCtx, .text = registry_buf, .out = expanded,
                       .capacity = CLI_PATH_TEXT);
            count = EMBED_CALL(cli_split, ToolchainsPathCtx, .text = expanded, .folders = args->folders, .count = count,
                               .capacity = capacity);
        }
        if (EMBED_CALL(cli_out_capacity_internal.registry_user_value, CliOutCapacityCtx, .name = "Path",
                       .out = registry_buf, .capacity = CLI_PATH_TEXT))
        {
            EMBED_CALL(cli_out_internal.registry_expand, CliOutCtx, .text = registry_buf, .out = expanded,
                       .capacity = CLI_PATH_TEXT);
            count = EMBED_CALL(cli_split, ToolchainsPathCtx, .text = expanded, .folders = args->folders, .count = count,
                               .capacity = capacity);
        }
    }
    g_cli_guard->persistent_buf_release(registry_buf, g_cli_toolchains_owner);
    return count;
}

unsigned int cli_toolchains_path_folders(const CliToolchainsCountCfg *args)
{
    char (*folders)[CLI_PATH_MAX_LEN] = args->folders;
    unsigned int capacity = args->capacity;
    char *const text = g_cli_guard->temporary_buf_alloc(CLI_PATH_TEXT, g_cli_toolchains_owner);
    unsigned int count =
        EMBED_CALL(cli_toolchains_count.tree_tools, CliToolchainsCountCfg, .folders = folders, .capacity = capacity);
    count =
        EMBED_CALL(cli_registry_folders, ToolchainsPathCtx, .folders = folders, .count = count, .capacity = capacity);
    if ((text != NULL) && EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH",
                                     .value_out = text, .capacity = CLI_PATH_TEXT))
    {
        count = EMBED_CALL(cli_split, ToolchainsPathCtx, .text = text, .folders = folders, .count = count,
                           .capacity = capacity);
    }
    if ((text != NULL) && EMBED_CALL(cli_shell_path, ToolchainsPathCtx, .path = text, .size = CLI_PATH_TEXT))
    {
        count = EMBED_CALL(cli_split, ToolchainsPathCtx, .text = text, .folders = folders, .count = count,
                           .capacity = capacity);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_toolchains_owner);
    return count;
}

// the folders a list the separator parts names at the most: one for each separator, and one more
EMBED_INLINE unsigned int cli_parts(const ToolchainsPathCtx *args)
{
    const char *text = args->text;
    unsigned int parts = 1u;
    parts += (unsigned int)EMBED_CALL(cli_text_size_byte.count, CliTextSizeByteCfg, .text = text,
                                      .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}),
                                      .byte = CLI_SEPARATOR);
    return parts;
}

char (*cli_toolchains_path_allocated(unsigned int *count, const CellOwner *owner)) [CLI_PATH_MAX_LEN] {
    // the most folders the tree's tools, the registry's two paths, the PATH and the shell's PATH name, counted from
    // their texts as cli_toolchains_path_folders reads them
    char *const text = g_cli_guard->persistent_buf_alloc(2ULL * CLI_PATH_TEXT, g_cli_toolchains_owner);
    char *const expanded = (text != NULL) ? text + CLI_PATH_TEXT : NULL;
    if (text == NULL)
    {
        *count = 0u;
        return NULL;
    }
    unsigned int most = CLI_TREE_TOOLS;
    if (EMBED_CALL(cli_out_internal.registry_machine_path, CliOutCtx, .out = text, .capacity = CLI_PATH_TEXT))
    {
        EMBED_CALL(cli_out_internal.registry_expand, CliOutCtx, .text = text, .out = expanded,
                   .capacity = CLI_PATH_TEXT);
        most += EMBED_CALL(cli_parts, ToolchainsPathCtx, .text = expanded);
    }
    if (EMBED_CALL(cli_out_capacity_internal.registry_user_value, CliOutCapacityCtx, .name = "Path", .out = text,
                   .capacity = CLI_PATH_TEXT))
    {
        EMBED_CALL(cli_out_internal.registry_expand, CliOutCtx, .text = text, .out = expanded,
                   .capacity = CLI_PATH_TEXT);
        most += EMBED_CALL(cli_parts, ToolchainsPathCtx, .text = expanded);
    }
    most += EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH", .value_out = text,
                       .capacity = CLI_PATH_TEXT)
                ? EMBED_CALL(cli_parts, ToolchainsPathCtx, .text = text)
                : 0u;
    most += EMBED_CALL(cli_shell_path, ToolchainsPathCtx, .path = text, .size = CLI_PATH_TEXT)
                ? EMBED_CALL(cli_parts, ToolchainsPathCtx, .text = text)
                : 0u;
    g_cli_guard->persistent_buf_release(text, g_cli_toolchains_owner);
    char (*const folders)[CLI_PATH_MAX_LEN] =
        g_cli_guard->persistent_buf_alloc((unsigned long long)most * CLI_PATH_MAX_LEN, owner);
    *count = (folders != NULL) ? EMBED_CALL(cli_toolchains_count.path_folders, CliToolchainsCountCfg,
                                            .folders = folders, .capacity = most)
                               : 0u;
    return folders;
}

int cli_toolchains_on_path(const CliToolchainsNamesCfg *args)
{
    unsigned int folders = 0u;
    char (*const path)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&folders, g_cli_toolchains_owner);
    int found = 0;
    unsigned int at = 0u;
    while (!found && (at < folders))
    {
        found = EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = path[at], .names = args->names,
                           .count = args->count, .path = args->program);
        at += 1u;
    }
    g_cli_guard->persistent_buf_release(path, g_cli_toolchains_owner);
    return found;
}

unsigned int cli_toolchains_system_path(const CliToolchainsCountCfg *args)
{
    char (*folders)[CLI_PATH_MAX_LEN] = args->folders;
    unsigned int capacity = args->capacity;
#if defined(_WIN32)
    return EMBED_CALL(cli_registry_folders, ToolchainsPathCtx, .folders = folders, .count = 0u, .capacity = capacity);
#else
    char *const text = g_cli_guard->temporary_buf_alloc(CLI_PATH_TEXT, g_cli_toolchains_owner);
    const unsigned int count =
        ((text != NULL) && EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH",
                                      .value_out = text, .capacity = CLI_PATH_TEXT))
            ? EMBED_CALL(cli_split, ToolchainsPathCtx, .text = text, .folders = folders, .count = 0u,
                         .capacity = capacity)
            : 0u;
    g_cli_guard->persistent_buf_release(text, g_cli_toolchains_owner);
    return count;
#endif
}

// The folders the reader gave for toolchains, put after the folders where each is a folder, or only counted where
// `folders` is NULL
typedef struct
{
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliChosenFolders;

static void cli_chosen_each(const CliEachIdFolderCfg *args)
{
    CliChosenFolders *const chosen = (CliChosenFolders *)args->context;
    if (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->folder))
    {
        return;
    }
    if (chosen->folders == NULL)
    {
        chosen->count += 1u;
        return;
    }
    if (chosen->count < chosen->capacity)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = chosen->folders[chosen->count],
                          .src = args->folder,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES}) + 1u});
        chosen->count += 1u;
    }
}

// `count` folders of `from` put after the folders, `list_entries` of them, which are sized for `capacity`: the count
// after
EMBED_INLINE unsigned int cli_folders_append(const ToolchainsPathCtx *args)
{
    unsigned int list_entries = args->list_entries;
    for (unsigned int at = 0u; (at < args->count) && (list_entries < args->capacity); at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->folders[list_entries],
                          .src = args->from[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->from[at], .cap = CLI_POOL_BYTES}) + 1u});
        list_entries += 1u;
    }
    return list_entries;
}

void cli_toolchains_run_path_with(const CliToolchainsCountPathCfg *args)
{
    unsigned int count = args->count;
    unsigned int path_count = 0u;
    char (*const path_folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&path_count, g_cli_toolchains_owner);
    CliTreeEnvironment environment;
    const unsigned int bins =
        EMBED_CALL(cli_toolchains_table.environment, CliToolchainsCfg, .environment_out = &environment)
            ? environment.bins
            : 0u;
    CliChosenFolders counted = {NULL, 0u, 0u};
    EMBED_CALL(cli_toolchains_context.chosen_each, CliToolchainsContextCfg, .each_callback = cli_chosen_each,
               .context = &counted);
    // the folders ahead, the environment's, the tree's own tools', those the reader chose and the PATH's, in a cell
    // of their count
    const unsigned int most = count + bins + CLI_TREE_TOOLS + counted.count + path_count;
    char (*const folders)[CLI_PATH_MAX_LEN] =
        g_cli_guard->persistent_buf_alloc((unsigned long long)most * CLI_PATH_MAX_LEN, g_cli_toolchains_owner);
    if (folders == NULL)
    {
        g_cli_guard->persistent_buf_release(path_folders, g_cli_toolchains_owner);
        EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH",
                   .value_out = args->path, .capacity = args->size);
        return;
    }
    unsigned int list_entries = EMBED_CALL(cli_folders_append, ToolchainsPathCtx, .folders = folders,
                                           .list_entries = 0u, .capacity = most, .from = args->ahead, .count = count);
    list_entries =
        EMBED_CALL(cli_folders_append, ToolchainsPathCtx, .folders = folders, .list_entries = list_entries,
                   .capacity = most, .from = (const char (*)[CLI_PATH_MAX_LEN])environment.bin, .count = bins);
    list_entries += EMBED_CALL(cli_toolchains_count.tree_tools, CliToolchainsCountCfg,
                               .folders = folders + list_entries, .capacity = most - list_entries);
    CliChosenFolders chosen = {folders, list_entries, most};
    EMBED_CALL(cli_toolchains_context.chosen_each, CliToolchainsContextCfg, .each_callback = cli_chosen_each,
               .context = &chosen);
    list_entries =
        EMBED_CALL(cli_folders_append, ToolchainsPathCtx, .folders = folders, .list_entries = chosen.count,
                   .capacity = most, .from = (const char (*)[CLI_PATH_MAX_LEN])path_folders, .count = path_count);
    g_cli_guard->persistent_buf_release(path_folders, g_cli_toolchains_owner);
    // each folder once by its key, in order
    VerbaLinea line = {args->path, args->size, 0ULL};
    unsigned int folder_count = 0u;
    for (unsigned int at = 0u; at < list_entries; at += 1u)
    {
        char key[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = folders[at], .key = key);
        int seen = folders[at][0] == '\0';
        unsigned int before = 0u;
        while (!seen && (before < at))
        {
            char path_buf[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = folders[before], .key = path_buf);
            seen = cellul.eq(&(CatenaFinitaCfg){.src = path_buf, .other = key, .cap = CLI_POOL_BYTES});
            before += 1u;
        }
        if (!seen)
        {
            if (folder_count != 0u)
            {
                mmgr_line_ch(&line, CLI_SEPARATOR);
            }
            mmgr_line_put(&line, folders[at]);
            folder_count += 1u;
        }
    }
    g_cli_guard->persistent_buf_release(folders, g_cli_toolchains_owner);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH",
                   .value_out = args->path, .capacity = args->size);
    }
}

void cli_toolchains_run_path(const CliToolchainsCountPathCfg *args)
{
    EMBED_CALL(cli_toolchains_count_path.run_path_with, CliToolchainsCountPathCfg, .ahead = NULL, .count = 0u,
               .path = args->path, .size = args->size);
}

unsigned int cli_toolchains_folders_for(const CliToolchainsJsonCountCfg *args)
{
    const CliJson *json = args->json;
    unsigned int count = args->count;
    const unsigned int patterns =
        EMBED_CALL(cli_toolchains_json_map.for_system, CliToolchainsJsonMapCfg, .json = json, .map = args->by_system);
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = patterns, .kind = CFG_JSON_ARRAY)
                                 ? json->node[patterns].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const pattern = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                               .json_read = json, .node = item, .length_pointer = NULL);
        if (pattern != NULL)
        {
            count = EMBED_CALL(cli_toolchains_count.folders_get, CliToolchainsCountCfg, .pattern = pattern,
                               .folders = args->folders, .count = count, .capacity = args->capacity);
        }
    }
    return count;
}

int cli_toolchains_join(const CliToolchainsFolderCfg *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = args->folder_read, .key = key);
    const char *from = args->path_read;
    for (const char *at = args->path_read;; at += 1)
    {
        if ((at[0] != CLI_SEPARATOR) && (at[0] != '\0'))
        {
            continue;
        }
        char dir[CLI_PATH_MAX_LEN];
        char path_buf[CLI_PATH_MAX_LEN];
        VerbaLinea line = {dir, sizeof(dir), 0ULL};
        mmgr_line_put_n(&line, from, (unsigned long long)(at - from));
        if ((at > from) && (mmgr_line_finish(&line) != 0ULL))
        {
            EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = dir, .key = path_buf);
            if (cellul.eq(&(CatenaFinitaCfg){.src = path_buf, .other = key, .cap = CLI_POOL_BYTES}))
            {
                return 0;
            }
        }
        if (at[0] == '\0')
        {
            break;
        }
        from = at + 1;
    }
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path_read, .cap = CLI_POOL_BYTES});
    while ((length != 0ULL) && (args->path_read[length - 1u] == CLI_SEPARATOR))
    {
        length -= 1ULL;
    }
    VerbaLinea line = {args->out_capacity, args->size, 0ULL};
    mmgr_line_put_n(&line, args->path_read, length);
    if (length != 0ULL)
    {
        mmgr_line_ch(&line, CLI_SEPARATOR);
    }
    mmgr_line_put(&line, args->folder_read);
    return mmgr_line_finish(&line) != 0ULL;
}
