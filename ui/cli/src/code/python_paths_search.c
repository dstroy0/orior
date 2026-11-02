// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_search.c: the tree's Python scripts that touch `sys.path` read, and the folders they name in the tree
// counted, the most used first, the reading cached for thirty seconds
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "python_paths_internal.h"

// a file larger than this is not read; how long a tree's reading stands, in milliseconds; the most folders counted,
// and the most one script names
#define CLI_PY_LARGEST (1ull << 20u)
#define CLI_PY_FRESH_FOR 30000ull
#define CLI_PY_COUNTS_MAX 1024u
#define CLI_PY_SEEN 256u

// A folder the scripts name, and how many of them name it
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned int count;
} CliPyCount;

// The tree's reading: the tree, when it was read, the folders counted and how many, the tree's files, the folders
// one script names, and its text
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    unsigned long long at;
    int has_counts;
    CliPyCount counted[CLI_PY_COUNTS_MAX];
    unsigned int counts;
    char seen[CLI_PY_SEEN][CLI_PATH_MAX_LEN];
    unsigned int seens;
} CliPySearch;

static CliPySearch s_search;

typedef struct
{
    const char *file;
    const unsigned char *bytes;
    unsigned long long size;
    const char *folder;
    const char *root;
    const char *top;
} PythonPathsSearchCtx;
// whether a file of the tree is a script a search reads: one ending in .py in no folder of an environment, of what a
// build writes, or of a cache
EMBED_INLINE int cli_search(const PythonPathsSearchCtx *args)
{
    static const char *const s_not[8] = {"venv",          ".venv",       "env",  "node_modules",
                                         "site-packages", "__pycache__", ".tox", ".nox"};
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL});
    if ((length < 3ull) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->file + length - 3ull, .other = ".py", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    const char *part = args->file;
    for (const char *at = args->file;; at += 1)
    {
        if ((at[0] == '/') || (at[0] == '\0'))
        {
            // the part lies in the path, at or past its start
            const unsigned long long size = (unsigned long long)(at - part);
            for (unsigned int one = 0u; one < 8u; one += 1u)
            {
                if ((cellul.len(&(CatenaFinitaCfg){.src = s_not[one], .cap = CLI_CELLS_POOL}) == size) &&
                    (memor.cmp(&(MemoriaCfg){.src = part, .other = s_not[one], .bytes = size}) == 0))
                {
                    return 0;
                }
            }
            if (at[0] == '\0')
            {
                return 1;
            }
            part = at + 1;
        }
    }
}

// whether `size` bytes are UTF-8 whole and contain `sys.path`
EMBED_INLINE int cli_touches_path(const PythonPathsSearchCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long at = 0ull;
    while (at < args->size)
    {
        unsigned int letter = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = bytes,
                       .length = args->size, .at = at, .letter = &letter);
        if ((step == 1ull) && (bytes[at] >= 0x80u))
        {
            return 0;
        }
        at += step;
    }
    for (unsigned long long place = 0ull; (place + 8ull) <= args->size; place += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = bytes + place, .other = "sys.path", .bytes = 8u}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// a folder a script named counted, once for that script
EMBED_INLINE void cli_on_count(const PythonPathsSearchCtx *args)
{
    for (unsigned int at = 0u; at < s_search.seens; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_search.seen[at], .other = args->folder, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (s_search.seens < CLI_PY_SEEN)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_search.seen[s_search.seens],
                          .src = args->folder,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}) + 1u});
        s_search.seens += 1u;
    }
}

// the folders one script named, each counted
static void cli_seen_count(void)
{
    for (unsigned int one = 0u; one < s_search.seens; one += 1u)
    {
        unsigned int at = 0u;
        while ((at < s_search.counts) &&
               (!cellul.eq(&(CatenaFinitaCfg){
                   .src = s_search.counted[at].path, .other = s_search.seen[one], .cap = CLI_CELLS_POOL})))
        {
            at += 1u;
        }
        if ((at == s_search.counts) && (s_search.counts < CLI_PY_COUNTS_MAX))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_search.counted[at].path, .bytes = CLI_PATH_MAX_LEN, .src = s_search.seen[one]});
            s_search.counted[at].count = 0u;
            s_search.counts += 1u;
        }
        if (at < s_search.counts)
        {
            s_search.counted[at].count += 1u;
        }
    }
}

// the script at `file` of the tree read, and the folders in the tree other than `top` it names counted
EMBED_INLINE void cli_script_read(const PythonPathsSearchCtx *args)
{
    const char *file = args->file;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, args->root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, file);
    unsigned long long size = 0ull;
    if ((mmgr_line_finish(&line) == 0ull) ||
        !EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size) ||
        (size > CLI_PY_LARGEST))
    {
        return;
    }
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_python_cells);
    if (bytes == NULL)
    {
        return;
    }
    if (EMBED_CALL(cli_touches_path, PythonPathsSearchCtx, .bytes = bytes, .size = size))
    {
        EMBED_CALL(cli_python_paths_size_internal.py_paths_in, CliPythonPathsSizeCtx, .file = path,
                   .text = (const char *)bytes, .size = size);
        s_search.seens = 0u;
        for (unsigned int at = 0u; at < g_cli_py.founds; at += 1u)
        {
            char folder[CLI_PATH_MAX_LEN];
            const int full = EMBED_CALL(
                query_room_path_internal.path_full, QueryRoomPathCtx,
                .path = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = g_cli_py.found[at]),
                .full = folder, .room = sizeof(folder));
            if (full && EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = folder) &&
                EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = args->top,
                           .path = folder) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = args->top, .cap = CLI_CELLS_POOL})))
            {
                EMBED_CALL(cli_on_count, PythonPathsSearchCtx, .folder = folder);
            }
        }
        cli_py_reading_free();
        cli_seen_count();
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_python_cells);
}

unsigned int cli_python_paths(const CliPythonPathsCfg *args)
{
    const char *root = args->root;
    const int fresh = s_search.has_counts &&
                      (cellul.eq(&(CatenaFinitaCfg){.src = s_search.root, .other = root, .cap = CLI_CELLS_POOL})) &&
                      ((cli_clock_milliseconds() - s_search.at) < CLI_PY_FRESH_FOR);
    if (!fresh)
    {
        char top[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = root, .full = top,
                        .room = sizeof(top)))
        {
            memor.cpy(&(MemoriaCfg){.dst = top,
                                    .src = root,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        }
        s_search.counts = 0u;
        const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
        for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
        {
            const char *const file = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = index);
            if (EMBED_CALL(cli_search, PythonPathsSearchCtx, .file = file))
            {
                EMBED_CALL(cli_script_read, PythonPathsSearchCtx, .root = root, .top = top, .file = file);
            }
        }
        EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
        // the most used first, then by path
        for (unsigned int at = 1u; at < s_search.counts; at += 1u)
        {
            const CliPyCount pivot = s_search.counted[at];
            unsigned int place = at;
            while ((place > 0u) &&
                   ((s_search.counted[place - 1u].count < pivot.count) ||
                    ((s_search.counted[place - 1u].count == pivot.count) &&
                     (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_search.counted[place - 1u].path,
                                 .other_pointer = pivot.path) > 0))))
            {
                s_search.counted[place] = s_search.counted[place - 1u];
                place -= 1u;
            }
            s_search.counted[place] = pivot;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_search.root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        s_search.at = cli_clock_milliseconds();
        s_search.has_counts = 1;
    }
    const unsigned int count = (s_search.counts < args->most) ? s_search.counts : args->most;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(args->each, CliEachPathCfg, .context = args->context, .path = s_search.counted[at].path);
    }
    return count;
}
