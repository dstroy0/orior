// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db.c: what a look for the tree's compilation database reads with: a file and when it changed, the folders and
// files of a name below a build, a CMake reply's newest index, a command line's words, and a JSON file read
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// the most folders a search below a build tracks to look in
#define CLI_COMPILE_PENDING 4096u

int cli_compile_stamp(const CliCompileDbPathStampedCtx *args)
{
    unsigned long long size = 0ULL;
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->path) ||
        !EMBED_CALL(cli_path_size_internal.file_stamp, CliPathSizeCtx, .path = args->path, .size_pointer = &size,
                    .changed = &args->stamped->time))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->stamped->path,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    return 1;
}

void cli_compile_found_alloc(const CliCompileDbFoundTopCtx *args)
{
    CliFoundFolders *found = args->found;
    char (*const paths)[CLI_PATH_MAX_LEN] =
        g_cli_guard->temporary_buf_alloc((unsigned long long)CLI_COMPILE_FOUND * CLI_PATH_MAX_LEN, g_cli_compile_owner);
    *found = (CliFoundFolders){paths, 0u, (paths != NULL) ? CLI_COMPILE_FOUND : 0u};
}

// A folder still to look in below a build, and its depth
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned int depth;
} CliBelowFolder;

// A search below a build: the folders still to look in, in a cell of `capacity`, and the folder looked in and its depth
typedef struct
{
    CliBelowFolder *pending;
    unsigned int count;
    unsigned int capacity;
    const char *folder;
    unsigned int folder_depth;
} CliBelow;

static void cli_below_each(const CliEachNameIsFolderCfg *args)
{
    CliBelow *const below = args->context;
    if (args->is_folder && (args->name[0] != '.') && (below->count < below->capacity))
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = below->folder, .relative = args->name,
                   .joined = below->pending[below->count].path);
        below->pending[below->count].depth = below->folder_depth + 1u;
        below->count += 1u;
    }
}

typedef struct
{
    const char *top;
    int (*each)(const CliEachFolderDepthCfg *args);
    void *context;
    CliFoundFolders *found;
    const char *path;
} CompileDbCtx;
// the folders below `top` walked, each handed to `each` with its depth: whether to look below it
EMBED_INLINE void cli_walked_below(const CompileDbCtx *args)
{
    CliBelow below = {NULL, 0u, CLI_COMPILE_PENDING, NULL, 0u};
    below.pending = g_cli_guard->temporary_buf_alloc((unsigned long long)CLI_COMPILE_PENDING * sizeof(CliBelowFolder),
                                                     g_cli_compile_owner);
    if (below.pending == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = below.pending[0].path,
                            .src = args->top,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->top, .cap = CLI_POOL_BYTES}) + 1u});
    below.pending[0].depth = 0u;
    below.count = 1u;
    while (below.count > 0u)
    {
        below.count -= 1u;
        char folder[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = below.pending[below.count].path, .bytes = sizeof(folder)});
        const unsigned int depth = below.pending[below.count].depth;
        if (!EMBED_CALL(args->each, CliEachFolderDepthCfg, .context = args->context, .folder = folder,
                        .depth = depth) ||
            (depth >= CLI_COMPILE_DEPTH))
        {
            continue;
        }
        below.folder = folder;
        below.folder_depth = depth;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = cli_below_each,
                   .context_bytess = &below);
    }
    g_cli_guard->persistent_buf_release(below.pending, g_cli_compile_owner);
}

// A search for folders ending in a tail, or for files of a name
typedef struct
{
    const char *const *tail;
    unsigned int count;
    const char *name;
    CliFoundFolders *found;
} CliNamedSearch;

EMBED_INLINE void cli_found_append(const CompileDbCtx *args)
{
    if (args->found->count < args->found->capacity)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->found->path[args->found->count],
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
        args->found->count += 1u;
    }
}

static int cli_tail_each(const CliEachFolderDepthCfg *args)
{
    const CliNamedSearch *const search = (const CliNamedSearch *)args->context;
    char below[CLI_PATH_MAX_LEN];
    char next[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = below,
                            .src = args->folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES}) + 1u});
    for (unsigned int at = 0u; at < search->count; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = below, .relative = search->tail[at], .joined = next);
        memor.cpy(&(MemoriaCfg){.dst = below,
                                .src = next,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = next, .cap = CLI_POOL_BYTES}) + 1u});
    }
    if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = below))
    {
        EMBED_CALL(cli_found_append, CompileDbCtx, .found = search->found, .path = below);
        return 0;
    }
    return 1;
}

void cli_compile_folders_by_name(const CliCompileDbFoundCtx *args)
{
    unsigned int count = args->count;
    args->found->count = 0u;
    CliNamedSearch search = {args->tail, count, NULL, args->found};
    EMBED_CALL(cli_walked_below, CompileDbCtx, .top = args->top, .each = cli_tail_each, .context = &search);
}

static int cli_name_each(const CliEachFolderDepthCfg *args)
{
    const CliNamedSearch *const search = (const CliNamedSearch *)args->context;
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = search->name, .joined = file);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = file))
    {
        EMBED_CALL(cli_found_append, CompileDbCtx, .found = search->found, .path = file);
    }
    return 1;
}

void cli_compile_files_by_name(const CliCompileDbFoundTopCtx *args)
{
    args->found->count = 0u;
    CliNamedSearch search = {NULL, 0u, args->name, args->found};
    EMBED_CALL(cli_walked_below, CompileDbCtx, .top = args->top, .each = cli_name_each, .context = &search);
}

// The newest index file of a reply folder as its entries are read
typedef struct
{
    const char *reply;
    char *index;
    int found;
} CliNewest;

static void cli_index_each(const CliEachNameIsFolderCfg *args)
{
    CliNewest *const newest = (CliNewest *)args->context;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = newest->reply, .relative = args->name, .joined = path);
    if (args->is_folder ||
        (!cellul.starts(&(CatenaFinitaCfg){.src = args->name, .other = "index-", .cap = CLI_POOL_BYTES})) ||
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        return;
    }
    if (!newest->found ||
        (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = path, .other_pointer = newest->index) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = newest->index,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u});
        newest->found = 1;
    }
}

int cli_compile_newest_index(const CliCompileDbReplyCtx *args)
{
    CliNewest newest = {args->reply, args->index, 0};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->reply,
               .each = cli_index_each, .context_bytess = &newest);
    return newest.found;
}

int cli_compile_white(const CliCompileDbCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

void cli_compile_words_each(const CliCompileDbTextCtx *args)
{
    static char s_word[1u << 16u];
    unsigned long long size = 0ULL;
    int quoted = 0;
    int any = 0;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const char letter = args->text[at];
        if (letter == '"')
        {
            quoted = !quoted;
            any = 1;
        }
        else if (EMBED_CALL(cli_compile_db_internal.compile_white, CliCompileDbCtx, .letter = letter) && !quoted)
        {
            if (any)
            {
                EMBED_CALL(args->each, CliEachWordSizeCfg, .context = args->context, .word_text = s_word, .size = size);
                size = 0ULL;
                any = 0;
            }
        }
        else
        {
            if (size < (sizeof(s_word) - 1ULL))
            {
                s_word[size] = letter;
                size += 1ULL;
            }
            any = 1;
        }
    }
    if (any)
    {
        EMBED_CALL(args->each, CliEachWordSizeCfg, .context = args->context, .word_text = s_word, .size = size);
    }
}

// The first word of a command line as the words are read
typedef struct
{
    char *first;
    int found;
} CliFirst;

static void cli_first_each(const CliEachWordSizeCfg *args)
{
    CliFirst *const first = (CliFirst *)args->context;
    if (!first->found && (args->size < CLI_PATH_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = first->first, .src = args->word_text, .bytes = args->size});
        first->first[args->size] = '\0';
        first->found = 1;
    }
}

int cli_compile_first_word(const CliCompileDbTextLengthCtx *args)
{
    CliFirst found = {args->first, 0};
    EMBED_CALL(cli_compile_db_text_internal.compile_words_each, CliCompileDbTextCtx, .text = args->text,
               .length = args->length, .each = cli_first_each, .context = &found);
    return found.found;
}

unsigned int cli_compile_json_read(const CliCompileDbPathCtx *args)
{
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = g_cli_compile_owner);
    if (bytes == NULL)
    {
        return CLI_JSON_NONE;
    }
    const unsigned int tokens_max = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                               .text = (const char *)bytes, .size = size) +
                                    1u;
    CfgJsonToken *const tokens =
        g_cli_guard->temporary_buf_alloc((unsigned long long)tokens_max * sizeof(CfgJsonToken), g_cli_compile_owner);
    unsigned int root = CLI_JSON_NONE;
    if ((tokens != NULL) && EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg, .json = args->json,
                                       .nodes = tokens_max, .text_u64 = size))
    {
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea unread = {why, sizeof(why), 0ULL};
        root = EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = args->json,
                          .text = (const char *)bytes, .size = size, .tokens = tokens, .tokens_max = tokens_max,
                          .err_msg = &unread);
    }
    g_cli_guard->persistent_buf_release(tokens, g_cli_compile_owner);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_compile_owner));
    return root;
}
