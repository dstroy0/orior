// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tools.c: a tool built where its binary is missing or a source or a header of a folder it reads is newer, objects
// compiled as many at once as the launch queue lets, the file definitions linked into every program that names them
#include "../../../cli/src/command_line/callback_cfg.h"
#include "tools_internal.h"

#include "../../../cli/src/text/text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// a tool's sources, each list sorted within a pattern, and their objects
QueryPaths g_query_c_sources;
QueryPaths g_query_cxx_sources;
QueryPaths g_query_tool_sources;
QueryPaths g_query_objects;

int query_paths_alloc(const QueryPathsCtx *args)
{
    g_cli_guard->persistent_buf_release(args->paths->path, g_query_owner);
    *args->paths = (QueryPaths){NULL, 0u, 0u};
    args->paths->path =
        g_cli_guard->persistent_buf_alloc((unsigned long long)args->count * QUERY_PATH_MAX_LEN, g_query_owner);
    args->paths->capacity = (args->paths->path != NULL) ? args->count : 0u;
    return args->paths->path != NULL;
}

// What a folder's walk for one pattern records: the folder, the extension wanted, the list it adds to, and where this
// pattern's files begin in it
typedef struct
{
    const char *folder;
    const char *extension;
    QueryPaths *found;
    unsigned int first;
} QuerySourceWalk;

typedef struct
{
    const char *name;
    const char *extension;
    const char *one;
    const char *other;
    const char *const *skip;
    const char *path;
    const char *folder;
    char *folder_out;
} ToolsCtx;
// the base name `name` ends in `.extension`
EMBED_INLINE int query_extension_is(const ToolsCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    const size_t required = cellul.len(&(CatenaFinitaCfg){.src = args->extension, .cap = CLI_POOL_BYTES});
    return (length > (required + 1u)) && (args->name[length - required - 1u] == '.') &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = args->name + length - required, .other = args->extension, .cap = required + 1u});
}

// 1 where the path `one` sorts after the path `other`, byte by byte, a path before every longer one it begins
EMBED_INLINE int query_path_after(const ToolsCtx *args)
{
    const char *other = args->other;
    // through `one`'s terminator: a path that begins the other ends first, and its terminator orders it first
    const size_t bytes = cellul.len(&(CatenaFinitaCfg){.src = args->one, .cap = QUERY_PATH_MAX_LEN}) + 1u;
    return memor.cmp(&(MemoriaCfg){.src = args->one, .other = other, .bytes = bytes}) > 0;
}

static void query_source_walk(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    QuerySourceWalk *const walk = (QuerySourceWalk *)args->context;
    QueryPaths *const found = walk->found;
    if (args->is_folder || !EMBED_CALL(query_extension_is, ToolsCtx, .name = name, .extension = walk->extension) ||
        (found->count >= found->capacity))
    {
        return;
    }
    VerbaLinea line = {found->path[found->count], QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    // a path that does not fit is no source of the tree's
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    // stored sorted from where this pattern's files begin
    unsigned int at = found->count;
    while ((at > walk->first) &&
           EMBED_CALL(query_path_after, ToolsCtx, .one = found->path[at - 1u], .other = found->path[found->count]))
    {
        at -= 1u;
    }
    if (at != found->count)
    {
        char saved_path[QUERY_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = saved_path, .src = found->path[found->count], .bytes = QUERY_PATH_MAX_LEN});
        memor.move_up(&(MemoriaCfg){.dst = found->path[at + 1u],
                                    .src = found->path[at],
                                    .bytes = (size_t)(found->count - at) * QUERY_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = found->path[at], .src = saved_path, .bytes = QUERY_PATH_MAX_LEN});
    }
    found->count += 1u;
}

unsigned int query_sources(const QueryToolsRootCtx *args)
{
    for (unsigned int at = 0u; args->patterns[at] != NULL; at += 1u)
    {
        const char *const pattern = args->patterns[at];
        const char *const slash = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = pattern,
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_POOL_BYTES}), .byte = '/');
        if ((slash != NULL) && (slash[1] == '*') && (slash[2] == '.'))
        {
            char folder[QUERY_PATH_MAX_LEN];
            VerbaLinea line = {folder, sizeof(folder), 0ULL};
            mmgr_line_put(&line, args->root);
            mmgr_line_ch(&line, '/');
            mmgr_line_put_n(&line, pattern, (unsigned long long)(slash - pattern));
            if (mmgr_line_finish(&line) != 0ULL)
            {
                QuerySourceWalk walk = {folder, slash + 3, args->found, args->found->count};
                EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                           .each = query_source_walk, .context_bytess = &walk);
            }
            continue;
        }
        if (args->found->count >= args->found->capacity)
        {
            break;
        }
        VerbaLinea line = {args->found->path[args->found->count], QUERY_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, args->root);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, pattern);
        args->found->count +=
            ((mmgr_line_finish(&line) != 0ULL) && EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx,
                                                             .path = args->found->path[args->found->count]))
                ? 1u
                : 0u;
    }
    return args->found->count;
}

// the tool named, or NULL
const QueryTool *query_tool_by_name(const QueryToolsCtx *args)
{
    for (const QueryTool *tool = g_query_tools; tool->name != NULL; tool += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = tool->name, .other = args->name, .cap = CLI_POOL_BYTES}))
        {
            return tool;
        }
    }
    return NULL;
}

void query_binary_name(const QueryNameCapacityCtx *args)
{
    VerbaLinea line = {args->binary, args->capacity, 0ULL};
    mmgr_line_put(&line, args->name);
#if defined(_WIN32)
    mmgr_line_put(&line, ".exe");
#endif
    mmgr_line_finish(&line);
}

// 1 where `path`'s base name opens with none of `skip`
EMBED_INLINE int query_source_include(const ToolsCtx *args)
{
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = QUERY_PATH_MAX_LEN}), .byte = '/');
    const char *const name = (slash != NULL) ? (slash + 1) : args->path;
    for (unsigned int at = 0u; args->skip[at] != NULL; at += 1u)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = args->skip[at], .cap = QUERY_PATH_MAX_LEN}))
        {
            return 0;
        }
    }
    return 1;
}

// Every source of `tool`: its C sources with the file definitions where it links them, then its C++ ones, those the
// tool skips left out, then its tools', each count written
void query_every_source(const QueryToolsRootToolCCtx *args)
{
    unsigned int *c = args->c;
    unsigned int *cxx = args->cxx;
    unsigned int *tools = args->tools;
    g_cli_guard->persistent_buf_release(g_query_c_sources.path, g_query_owner);
    *(&g_query_c_sources) = (QueryPaths){NULL, 0u, 0u};
    g_cli_guard->persistent_buf_release(g_query_cxx_sources.path, g_query_owner);
    *(&g_query_cxx_sources) = (QueryPaths){NULL, 0u, 0u};
    g_cli_guard->persistent_buf_release(g_query_tool_sources.path, g_query_owner);
    *(&g_query_tool_sources) = (QueryPaths){NULL, 0u, 0u};
    g_cli_guard->persistent_buf_release(g_query_objects.path, g_query_owner);
    *(&g_query_objects) = (QueryPaths){NULL, 0u, 0u};
    (void)EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &g_query_c_sources,
                     .count = QUERY_SOURCES_MAX);
    (void)EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &g_query_cxx_sources,
                     .count = QUERY_SOURCES_MAX);
    (void)EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &g_query_tool_sources,
                     .count = QUERY_SOURCES_MAX);
    for (unsigned int group = 0u; (group < QUERY_TOOL_GROUPS) && (args->tool->c[group] != NULL); group += 1u)
    {
        (void)EMBED_CALL(query_tools_root_internal.sources, QueryToolsRootCtx, .root = args->root,
                         .patterns = args->tool->c[group], .found = &g_query_c_sources);
    }
    if (args->tool->file_defs)
    {
        (void)EMBED_CALL(query_tools_root_internal.sources, QueryToolsRootCtx, .root = args->root,
                         .patterns = g_query_file_defs, .found = &g_query_c_sources);
    }
    *c = EMBED_CALL(query_tools_root_internal.sources, QueryToolsRootCtx, .root = args->root, .patterns = g_query_mmgr,
                    .found = &g_query_c_sources);
    unsigned int found = 0u;
    for (unsigned int group = 0u; (group < QUERY_TOOL_GROUPS) && (args->tool->cxx[group] != NULL); group += 1u)
    {
        found = EMBED_CALL(query_tools_root_internal.sources, QueryToolsRootCtx, .root = args->root,
                           .patterns = args->tool->cxx[group], .found = &g_query_cxx_sources);
    }
    char (*const cxx_paths)[QUERY_PATH_MAX_LEN] = g_query_cxx_sources.path;
    *cxx = 0u;
    for (unsigned int at = 0u; at < found; at += 1u)
    {
        if (EMBED_CALL(query_source_include, ToolsCtx, .skip = args->tool->skip, .path = cxx_paths[at]))
        {
            memor.move_down(&(MemoriaCfg){.dst = cxx_paths[*cxx], .src = cxx_paths[at], .bytes = QUERY_PATH_MAX_LEN});
            *cxx += 1u;
        }
    }
    g_query_cxx_sources.count = *cxx;
    *tools = EMBED_CALL(query_tools_root_internal.sources, QueryToolsRootCtx, .root = args->root,
                        .patterns = args->tool->cxx_tool, .found = &g_query_tool_sources);
}

// What a folder's walk for its newest header records
typedef struct
{
    const char *folder;
    unsigned long long newest;
} QueryHeaderWalk;

static void query_header_walk(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    QueryHeaderWalk *const walk = (QueryHeaderWalk *)args->context;
    if (args->is_folder || !EMBED_CALL(query_extension_is, ToolsCtx, .name = name, .extension = "h"))
    {
        return;
    }
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    unsigned long long seconds = 0ULL;
    if ((mmgr_line_finish(&line) != 0ULL) &&
        EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = path, .seconds = &seconds) &&
        (seconds > walk->newest))
    {
        walk->newest = seconds;
    }
}

// the newest .h in `folder`, one level deep, 0 where it contains none
EMBED_INLINE unsigned long long query_newest_header(const ToolsCtx *args)
{
    QueryHeaderWalk walk = {args->folder, 0ULL};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_header_walk, .context_bytess = &walk);
    return walk.newest;
}

// the folder `path` is in written into `folder`
EMBED_INLINE void query_folder_get(const ToolsCtx *args)
{
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = QUERY_PATH_MAX_LEN}), .byte = '/');
    VerbaLinea line = {args->folder_out, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, args->path, (slash != NULL) ? (unsigned long long)(slash - args->path) : 0ULL);
    mmgr_line_finish(&line);
}

// the newest .h across the folders a tool's sources read: those its `sources` are in, those the tool names, the file
// definitions' where it links them, and its includes; 0 where none contains one
unsigned long long query_newest_header_get(const QueryToolsRootToolSourcesCtx *args)
{
    unsigned long long newest = 0ULL;
    char folder[QUERY_PATH_MAX_LEN];
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(query_folder_get, ToolsCtx, .path = args->sources[at], .folder_out = folder);
        const unsigned long long seconds = EMBED_CALL(query_newest_header, ToolsCtx, .folder = folder);
        newest = (seconds > newest) ? seconds : newest;
    }
    const char *const *const named[3] = {args->tool->headers, args->tool->file_defs ? g_query_file_defs_headers : NULL,
                                         args->tool->includes};
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        for (unsigned int at = 0u; (named[list] != NULL) && (named[list][at] != NULL); at += 1u)
        {
            VerbaLinea line = {folder, sizeof(folder), 0ULL};
            mmgr_line_put(&line, args->root);
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, named[list][at]);
            const unsigned long long seconds =
                (mmgr_line_finish(&line) != 0ULL) ? EMBED_CALL(query_newest_header, ToolsCtx, .folder = folder) : 0ULL;
            newest = (seconds > newest) ? seconds : newest;
        }
    }
    return newest;
}

// 1 where an object is: it is there, and neither its source nor the newest header its tool reads is newer
int query_fresh(const QueryToolsSourceKnownCtx *args)
{
    return args->object_known && args->source_known && (args->source <= args->object) &&
           (args->newest_header <= args->object);
}

// 1 where `binary` is missing or older than a source of `tool`, or a header of a folder its sources are in or it
// names
int query_stale(const QueryToolsRootToolCtx *args)
{
    unsigned long long built = 0ULL;
    if (!EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = args->binary, .seconds = &built))
    {
        return 1;
    }
    unsigned int c = 0u;
    unsigned int cxx = 0u;
    unsigned int tools = 0u;
    EMBED_CALL(query_tools_root_tool_c_internal.every_source, QueryToolsRootToolCCtx, .root = args->root,
               .tool = args->tool, .c = &c, .cxx = &cxx, .tools = &tools);
    char (*const lists[3])[QUERY_PATH_MAX_LEN] = {g_query_c_sources.path, g_query_cxx_sources.path,
                                                  g_query_tool_sources.path};
    const unsigned int counts[3] = {c, cxx, tools};
    unsigned long long newest = 0ULL;
    int stale = 0;
    unsigned int list = 0u;
    while (!stale && (list < 3u))
    {
        unsigned int at = 0u;
        while (!stale && (at < counts[list]))
        {
            unsigned long long seconds = 0ULL;
            stale = EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = lists[list][at],
                               .seconds = &seconds) &&
                    (seconds > built);
            at += 1u;
        }
        const unsigned long long seconds =
            stale ? 0ULL
                  : EMBED_CALL(query_tools_root_tool_sources_internal.newest_header_get, QueryToolsRootToolSourcesCtx,
                               .root = args->root, .tool = args->tool, .sources = lists[list], .count = counts[list]);
        newest = (seconds > newest) ? seconds : newest;
        list += 1u;
    }
    g_cli_guard->persistent_buf_release(g_query_c_sources.path, g_query_owner);
    *(&g_query_c_sources) = (QueryPaths){NULL, 0u, 0u};
    g_cli_guard->persistent_buf_release(g_query_cxx_sources.path, g_query_owner);
    *(&g_query_cxx_sources) = (QueryPaths){NULL, 0u, 0u};
    g_cli_guard->persistent_buf_release(g_query_tool_sources.path, g_query_owner);
    *(&g_query_tool_sources) = (QueryPaths){NULL, 0u, 0u};
    g_cli_guard->persistent_buf_release(g_query_objects.path, g_query_owner);
    *(&g_query_objects) = (QueryPaths){NULL, 0u, 0u};
    return stale || (newest > built);
}

int query_tool_build(const QueryNameCapacityCtx *args)
{
    char out[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {out, sizeof(out), 0ULL};
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "/build/engine");
    mmgr_line_finish(&line);
    return EMBED_CALL(query_name_capacity_internal.tool_built_into, QueryNameCapacityCtx, .root = args->root,
                      .programs = args->programs, .name = args->name, .out_text = out, .binary = args->binary,
                      .capacity = args->capacity, .err_msg = args->err_msg);
}

// the object `source` is compiled to under `objects`, named by its path from the top of the tree with its separators
// made `_`, so that two sources of one base name in two folders are apart
void query_object_get(const QueryToolsRootObjectsCtx *args)
{
    const size_t root_length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = QUERY_PATH_MAX_LEN});
    const char *under =
        (cellul.starts(&(CatenaFinitaCfg){.src = args->source_text, .other = args->root, .cap = QUERY_PATH_MAX_LEN}) &&
         (args->source_text[root_length] == '/'))
            ? (args->source_text + root_length + 1u)
            : args->source_text;
    VerbaLinea line = {args->object_capacity, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->objects);
    mmgr_line_ch(&line, '/');
    const unsigned long long start = line.at;
    mmgr_line_put(&line, under);
    for (unsigned long long at = start; at < line.at; at += 1ULL)
    {
        line.out[at] = ((line.out[at] == '/') || (line.out[at] == '\\')) ? '_' : line.out[at];
    }
    mmgr_line_put(&line, ".o");
    mmgr_line_finish(&line);
}
