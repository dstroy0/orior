// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_search.c: the tree's Python scripts that touch `sys.path` read, and the folders they name in the tree
// counted, the max used first, the reading cached for thirty seconds
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "python_paths_internal.h"

// The bytes past which the search reads no file, and how long a tree's read stays fresh, in milliseconds. Then the
// bytes a list's cell starts at
#define CLI_PY_LARGEST (1ULL << 20u)
#define CLI_PY_FRESH_FOR 30000ULL
#define CLI_PY_CELL_FIRST (8ULL * CLI_PATH_MAX_LEN)

// A folder the scripts name, and how many of them name it: one record of the counted cell
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned int count;
} CliPyCount;

// A list in a movable cell the process holds: the cell and its bytes, grown as the list outgrows it
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long bytes;
} CliPyCell;

// The tree's read: the tree, and the millisecond of its read. It holds how many folders the read counts, in their cell
// as records, and how many one script names, in their cell as paths
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    unsigned long long at;
    int has_counts;
    CliPyCell counted;
    unsigned int counts;
    CliPyCell seen;
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
    CliPyCell *store;
    unsigned long long needed;
    unsigned int index;
    const CliPyCount *record;
} PythonPathsSearchCtx;

// The list's cell grown to hold `needed` bytes where it holds less. It doubles from its bytes, or takes the need
// where that is more. The records so far go across through a page of the stack, and the old cell goes back. Returns
// 1, or 0 where the cells cannot meet it and the list stays as it is
EMBED_INLINE int cli_py_cell_grown(const PythonPathsSearchCtx *args)
{
    CliPyCell *const store = args->store;
    if (args->needed <= store->bytes)
    {
        return 1;
    }
    const unsigned long long doubled = (store->bytes != 0ULL) ? (store->bytes * 2ULL) : CLI_PY_CELL_FIRST;
    const unsigned long long bytes = (doubled < args->needed) ? args->needed : doubled;
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(bytes, g_cli_python_owner);
    if (cell == NULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < store->bytes; at += CLI_PATH_MAX_LEN)
    {
        char page[CLI_PATH_MAX_LEN];
        const unsigned long long piece =
            ((store->bytes - at) < CLI_PATH_MAX_LEN) ? (store->bytes - at) : (unsigned long long)CLI_PATH_MAX_LEN;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = store->cell, .offset = (embed_word)at, .dst = page, .bytes = (embed_word)piece});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = (embed_word)at, .src = page, .bytes = (embed_word)piece});
    }
    if (store->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(store->cell, g_cli_python_owner);
    }
    store->cell = cell;
    store->bytes = bytes;
    return 1;
}

// whether the path `index` paths into the seen cell is `folder`, compared in the cell
EMBED_INLINE int cli_py_seen_is(const PythonPathsSearchCtx *args)
{
    return g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
               .cell = s_search.seen.cell,
               .offset = (embed_word)args->index * CLI_PATH_MAX_LEN,
               .src = args->folder,
               .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_PATH_MAX_LEN}) + 1ULL}) == 0;
}

// Whether a file of the tree is a script the search reads. It ends in .py and lies in no folder of an environment,
// of what a build writes, or of a cache
EMBED_INLINE int cli_search(const PythonPathsSearchCtx *args)
{
    static const char *const s_not[8] = {"venv",          ".venv",       "env",  "node_modules",
                                         "site-packages", "__pycache__", ".tox", ".nox"};
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES});
    if ((length < 3ULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->file + length - 3ULL, .other = ".py", .cap = CLI_POOL_BYTES})))
    {
        return 0;
    }
    const char *part = args->file;
    for (const char *at = args->file;; at += 1)
    {
        if ((at[0] != '/') && (at[0] != '\0'))
        {
            continue;
        }
        // the part lies in the path, at or past its start
        const unsigned long long size = (unsigned long long)(at - part);
        for (unsigned int one = 0u; one < 8u; one += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = s_not[one], .cap = CLI_POOL_BYTES}) == size) &&
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

// whether `size` bytes are UTF-8 whole and contain `sys.path`
EMBED_INLINE int cli_touches_path(const PythonPathsSearchCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        unsigned int character = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = bytes,
                       .length = args->size, .at = at, .character = &character);
        if ((step == 1ULL) && (bytes[at] >= 0x80u))
        {
            return 0;
        }
        at += step;
    }
    for (unsigned long long place = 0ULL; (place + 8ULL) <= args->size; place += 1ULL)
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
        if (EMBED_CALL(cli_py_seen_is, PythonPathsSearchCtx, .index = at, .folder = args->folder))
        {
            return;
        }
    }
    // the folder put after the paths seen, the cell grown where it is full
    if (EMBED_CALL(cli_py_cell_grown, PythonPathsSearchCtx, .store = &s_search.seen,
                   .needed = ((unsigned long long)s_search.seens + 1ULL) * CLI_PATH_MAX_LEN))
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_search.seen.cell,
            .offset = (embed_word)s_search.seens * CLI_PATH_MAX_LEN,
            .src = args->folder,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_PATH_MAX_LEN}) + 1ULL});
        s_search.seens += 1u;
    }
}

// the folders one script named, each counted
static void cli_seen_count(void)
{
    for (unsigned int one = 0u; one < s_search.seens; one += 1u)
    {
        // the path seen read out of its cell, then found among the records by a compare in theirs
        CliPyCount record = {.path = {0}, .count = 0u};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_search.seen.cell,
                                                                .offset = (embed_word)one * CLI_PATH_MAX_LEN,
                                                                .dst = record.path,
                                                                .bytes = CLI_PATH_MAX_LEN});
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = record.path, .cap = CLI_PATH_MAX_LEN});
        unsigned int at = 0u;
        while ((at < s_search.counts) &&
               (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = s_search.counted.cell,
                                                                     .offset = (embed_word)at * sizeof(CliPyCount),
                                                                     .src = record.path,
                                                                     .bytes = length + 1ULL}) != 0))
        {
            at += 1u;
        }
        if ((at == s_search.counts) &&
            EMBED_CALL(cli_py_cell_grown, PythonPathsSearchCtx, .store = &s_search.counted,
                       .needed = ((unsigned long long)s_search.counts + 1ULL) * sizeof(CliPyCount)))
        {
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_search.counted.cell,
                                                                     .offset = (embed_word)at * sizeof(CliPyCount),
                                                                     .src = &record,
                                                                     .bytes = sizeof(CliPyCount)});
            s_search.counts += 1u;
        }
        if (at < s_search.counts)
        {
            CliPyCount found;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_search.counted.cell,
                                                                    .offset = (embed_word)at * sizeof(found),
                                                                    .dst = &found,
                                                                    .bytes = sizeof(found)});
            found.count += 1u;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_search.counted.cell,
                                                                     .offset = (embed_word)at * sizeof(CliPyCount),
                                                                     .src = &found,
                                                                     .bytes = sizeof(CliPyCount)});
        }
    }
}

// the script at `file` of the tree read, and the folders in the tree other than `top` it names counted
EMBED_INLINE void cli_script_read(const PythonPathsSearchCtx *args)
{
    const char *file = args->file;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, file);
    unsigned long long size = 0ULL;
    if ((mmgr_line_finish(&line) == 0ULL) ||
        !EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size) ||
        (size > CLI_PY_LARGEST))
    {
        return;
    }
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = g_cli_python_owner);
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
                query_capacity_path_internal.path_full, QueryCapacityPathCtx,
                .path = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = g_cli_py.found[at]),
                .full = folder, .capacity = sizeof(folder));
            if (full && EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = folder) &&
                EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = args->top,
                           .path = folder) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = args->top, .cap = CLI_POOL_BYTES})))
            {
                EMBED_CALL(cli_on_count, PythonPathsSearchCtx, .folder = folder);
            }
        }
        cli_py_read_state_free();
        cli_seen_count();
    }
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, g_cli_python_owner);
}

// each of the tree's files the search reads, its scripts read, with the root and the top the context holds
static void python_files_searched(const CliFilesEachCfg *view)
{
    const PythonPathsSearchCtx *const args = view->context;
    const CliPicked *const files = view->files;
    for (unsigned int index = 0u; index < files->count; index += 1u)
    {
        char file_read[CLI_PATH_MAX_LEN];
        const char *const file =
            EMBED_CALL(cli_files_view_read.at, CliFilesViewCfg, .view = files, .index = index, .path_out = file_read);
        if (EMBED_CALL(cli_search, PythonPathsSearchCtx, .file = file))
        {
            EMBED_CALL(cli_script_read, PythonPathsSearchCtx, .root = args->root, .top = args->top, .file = file);
        }
    }
}

unsigned int cli_python_paths(const CliPythonPathsCfg *args)
{
    const char *root = args->root;
    const int fresh = s_search.has_counts &&
                      (cellul.eq(&(CatenaFinitaCfg){.src = s_search.root, .other = root, .cap = CLI_POOL_BYTES})) &&
                      ((cli_clock_milliseconds() - s_search.at) < CLI_PY_FRESH_FOR);
    if (!fresh)
    {
        char top[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = root, .full = top,
                        .capacity = sizeof(top)))
        {
            memor.cpy(&(MemoriaCfg){.dst = top,
                                    .src = root,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_POOL_BYTES}) + 1u});
        }
        s_search.counts = 0u;
        PythonPathsSearchCtx searched = {.root = root, .top = top};
        (void)EMBED_CALL(cli_files_root_paths.each, CliFilesRootPathsCfg, .root = root, .each = python_files_searched,
                         .context = &searched);
        // The max used first, then by path. The sort takes each record out of the cell, and moves the ones before it
        // up a record while they order after it. It writes the record where they stop
        for (unsigned int at = 1u; at < s_search.counts; at += 1u)
        {
            CliPyCount pivot;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_search.counted.cell,
                                                                    .offset = (embed_word)at * sizeof(pivot),
                                                                    .dst = &pivot,
                                                                    .bytes = sizeof(pivot)});
            unsigned int place = at;
            while (place > 0u)
            {
                CliPyCount before;
                (void)g_cli_guard->movable_buf_read(
                    &(CarcerMovableCfg){.cell = s_search.counted.cell,
                                        .offset = (embed_word)(place - 1u) * sizeof(before),
                                        .dst = &before,
                                        .bytes = sizeof(before)});
                const int after =
                    (before.count < pivot.count) || ((before.count == pivot.count) &&
                                                     (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                                 .one = before.path, .other_pointer = pivot.path) > 0));
                if (!after)
                {
                    break;
                }
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = s_search.counted.cell,
                                        .offset = (embed_word)place * sizeof(CliPyCount),
                                        .src = &before,
                                        .bytes = sizeof(CliPyCount)});
                place -= 1u;
            }
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_search.counted.cell,
                                                                     .offset = (embed_word)place * sizeof(CliPyCount),
                                                                     .src = &pivot,
                                                                     .bytes = sizeof(CliPyCount)});
        }
        memor.cpy(&(MemoriaCfg){.dst = s_search.root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_POOL_BYTES}) + 1u});
        s_search.at = cli_clock_milliseconds();
        s_search.has_counts = 1;
    }
    const unsigned int count = (s_search.counts < args->max) ? s_search.counts : args->max;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        CliPyCount record;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_search.counted.cell,
                                                                .offset = (embed_word)at * sizeof(record),
                                                                .dst = &record,
                                                                .bytes = sizeof(record)});
        EMBED_CALL(args->each, CliEachPathCfg, .context = args->context, .path = record.path);
    }
    return count;
}
