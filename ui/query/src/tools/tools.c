// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tools.c: a tool built where its binary is missing or a source or a header of a folder it reads is newer, objects
// compiled as many at once as the launch queue lets, the file definitions linked into every program that names them
#include "tools_internal.h"

#include "../../../cli/src/text/text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// a tool's sources, each list sorted within a pattern, and their objects
QueryPaths g_query_c_sources;
QueryPaths g_query_cxx_sources;
QueryPaths g_query_tool_sources;
QueryPaths g_query_objects;

int query_paths_held(QueryPaths *paths, unsigned int count)
{
    if (count <= paths->room)
    {
        return 1;
    }
    char (*const larger)[QUERY_PATH_LONGEST] =
        cli_cell_taken_by(&g_query_cells, (unsigned long long)count * QUERY_PATH_LONGEST, "a list of paths");
    if (larger == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = paths->path, .bytes = (size_t)paths->count * QUERY_PATH_LONGEST});
    cli_cell_given_by(&g_query_cells, paths->path);
    paths->path = larger;
    paths->room = count;
    return 1;
}

int query_paths_room(QueryPaths *paths, unsigned int most)
{
    if (paths->count < paths->room)
    {
        return 1;
    }
    const unsigned int doubled = (paths->room == 0u) ? QUERY_PATHS_FIRST : (paths->room * 2u);
    return (paths->count < most) && query_paths_held(paths, (doubled < most) ? doubled : most);
}

void query_paths_given(QueryPaths *paths)
{
    cli_cell_given_by(&g_query_cells, paths->path);
    *paths = (QueryPaths){NULL, 0u, 0u};
}

// What a folder's walk for one pattern keeps: the folder, the extension wanted, the list it adds to, and where this
// pattern's files begin in it
typedef struct
{
    const char *folder;
    const char *extension;
    QueryPaths *found;
    unsigned int first;
} QuerySourceWalk;

// the base name `name` ends in `.extension`
static int query_extension_is(const char *name, const char *extension)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const size_t wanted = cellul.len(&(CatenaFinitaCfg){.src = extension, .cap = CLI_CELLS_POOL});
    return (length > (wanted + 1u)) && (name[length - wanted - 1u] == '.') &&
           cellul.eq(&(CatenaFinitaCfg){.src = name + length - wanted, .other = extension, .cap = wanted + 1u});
}

// 1 where the path `one` sorts after the path `other`, byte by byte, a path before every longer one it begins
static int query_path_after(const char *one, const char *other)
{
    // through `one`'s terminator: a path that begins the other ends first, and its terminator orders it first
    const size_t bytes = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = QUERY_PATH_LONGEST}) + 1u;
    return memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = bytes}) > 0;
}

static void query_source_walked(void *context, const char *name, int is_folder)
{
    QuerySourceWalk *const walk = (QuerySourceWalk *)context;
    QueryPaths *const found = walk->found;
    if (is_folder || !query_extension_is(name, walk->extension) || !query_paths_room(found, QUERY_SOURCES_MOST))
    {
        return;
    }
    CliLine line = {found->path[found->count], QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, walk->folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    // a path that does not fit is no source of the tree's
    if (cli_line_closed(&line) == 0ull)
    {
        return;
    }
    // kept sorted from where this pattern's files begin
    unsigned int at = found->count;
    while ((at > walk->first) && query_path_after(found->path[at - 1u], found->path[found->count]))
    {
        at -= 1u;
    }
    if (at != found->count)
    {
        char held[QUERY_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = held, .src = found->path[found->count], .bytes = QUERY_PATH_LONGEST});
        memor.move_up(&(MemoriaCfg){.dst = found->path[at + 1u],
                                    .src = found->path[at],
                                    .bytes = (size_t)(found->count - at) * QUERY_PATH_LONGEST});
        memor.cpy(&(MemoriaCfg){.dst = found->path[at], .src = held, .bytes = QUERY_PATH_LONGEST});
    }
    found->count += 1u;
}

unsigned int query_sources(const char *root, const char *const *patterns, QueryPaths *found)
{
    for (unsigned int at = 0u; patterns[at] != NULL; at += 1u)
    {
        const char *const pattern = patterns[at];
        const char *const slash =
            cli_text_last(pattern, cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL}), '/');
        if ((slash != NULL) && (slash[1] == '*') && (slash[2] == '.'))
        {
            char folder[QUERY_PATH_LONGEST];
            CliLine line = {folder, sizeof(folder), 0ull};
            cli_line_text(&line, root);
            cli_line_character(&line, '/');
            cli_line_counted(&line, pattern, (unsigned long long)(slash - pattern));
            if (cli_line_closed(&line) != 0ull)
            {
                QuerySourceWalk walk = {folder, slash + 3, found, found->count};
                query_folder_each(folder, query_source_walked, &walk);
            }
            continue;
        }
        if (!query_paths_room(found, QUERY_SOURCES_MOST))
        {
            break;
        }
        CliLine line = {found->path[found->count], QUERY_PATH_LONGEST, 0ull};
        cli_line_text(&line, root);
        cli_line_character(&line, '/');
        cli_line_text(&line, pattern);
        found->count += ((cli_line_closed(&line) != 0ull) && query_is_file(found->path[found->count])) ? 1u : 0u;
    }
    return found->count;
}

// the tool named, or NULL
const QueryTool *query_tool_named(const char *name)
{
    for (const QueryTool *tool = g_query_tools; tool->name != NULL; tool += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = tool->name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            return tool;
        }
    }
    return NULL;
}

void query_binary_name(const char *name, char *binary, size_t room)
{
    CliLine line = {binary, room, 0ull};
    cli_line_text(&line, name);
#if defined(_WIN32)
    cli_line_text(&line, ".exe");
#endif
    cli_line_closed(&line);
}

// 1 where `path`'s base name opens with none of `skip`
static int query_source_kept(const char *const *skip, const char *path)
{
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = QUERY_PATH_LONGEST}), '/');
    const char *const name = (slash != NULL) ? (slash + 1) : path;
    for (unsigned int at = 0u; skip[at] != NULL; at += 1u)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = skip[at], .cap = QUERY_PATH_LONGEST}))
        {
            return 0;
        }
    }
    return 1;
}

// Every source of `tool`: its C sources with the file definitions where it links them, then its C++ ones, those the
// tool skips left out, then its tools', each count written
void query_every_source(const char *root, const QueryTool *tool, unsigned int *c, unsigned int *cxx,
                        unsigned int *tools)
{
    query_every_source_given();
    for (unsigned int group = 0u; (group < QUERY_TOOL_GROUPS) && (tool->c[group] != NULL); group += 1u)
    {
        (void)query_sources(root, tool->c[group], &g_query_c_sources);
    }
    if (tool->file_defs)
    {
        (void)query_sources(root, g_query_file_defs, &g_query_c_sources);
    }
    *c = query_sources(root, g_query_mmgr, &g_query_c_sources);
    unsigned int found = 0u;
    for (unsigned int group = 0u; (group < QUERY_TOOL_GROUPS) && (tool->cxx[group] != NULL); group += 1u)
    {
        found = query_sources(root, tool->cxx[group], &g_query_cxx_sources);
    }
    char (*const cxx_paths)[QUERY_PATH_LONGEST] = g_query_cxx_sources.path;
    *cxx = 0u;
    for (unsigned int at = 0u; at < found; at += 1u)
    {
        if (query_source_kept(tool->skip, cxx_paths[at]))
        {
            memor.move_down(&(MemoriaCfg){.dst = cxx_paths[*cxx], .src = cxx_paths[at], .bytes = QUERY_PATH_LONGEST});
            *cxx += 1u;
        }
    }
    g_query_cxx_sources.count = *cxx;
    *tools = query_sources(root, tool->cxx_tool, &g_query_tool_sources);
}

void query_every_source_given(void)
{
    query_paths_given(&g_query_c_sources);
    query_paths_given(&g_query_cxx_sources);
    query_paths_given(&g_query_tool_sources);
    query_paths_given(&g_query_objects);
}

// What a folder's walk for its newest header keeps
typedef struct
{
    const char *folder;
    unsigned long long newest;
} QueryHeaderWalk;

static void query_header_walked(void *context, const char *name, int is_folder)
{
    QueryHeaderWalk *const walk = (QueryHeaderWalk *)context;
    if (is_folder || !query_extension_is(name, "h"))
    {
        return;
    }
    char path[QUERY_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, walk->folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    unsigned long long seconds = 0ull;
    if ((cli_line_closed(&line) != 0ull) && query_modified(path, &seconds) && (seconds > walk->newest))
    {
        walk->newest = seconds;
    }
}

// the newest .h in `folder`, one level deep, 0 where it holds none
static unsigned long long query_newest_header(const char *folder)
{
    QueryHeaderWalk walk = {folder, 0ull};
    query_folder_each(folder, query_header_walked, &walk);
    return walk.newest;
}

// the folder `path` stands in written into `folder`
static void query_folder_of(const char *path, char *folder)
{
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = QUERY_PATH_LONGEST}), '/');
    CliLine line = {folder, QUERY_PATH_LONGEST, 0ull};
    cli_line_counted(&line, path, (slash != NULL) ? (unsigned long long)(slash - path) : 0ull);
    cli_line_closed(&line);
}

// the newest .h across the folders a tool's sources read: those its `sources` stand in, those the tool names, the file
// definitions' where it links them, and its includes; 0 where none holds one
unsigned long long query_newest_header_of(const char *root, const QueryTool *tool, char (*sources)[QUERY_PATH_LONGEST],
                                          unsigned int count)
{
    unsigned long long newest = 0ull;
    char folder[QUERY_PATH_LONGEST];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        query_folder_of(sources[at], folder);
        const unsigned long long seconds = query_newest_header(folder);
        newest = (seconds > newest) ? seconds : newest;
    }
    const char *const *const named[3] = {tool->headers, tool->file_defs ? g_query_file_defs_headers : NULL,
                                         tool->includes};
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        for (unsigned int at = 0u; (named[list] != NULL) && (named[list][at] != NULL); at += 1u)
        {
            CliLine line = {folder, sizeof(folder), 0ull};
            cli_line_text(&line, root);
            cli_line_character(&line, '/');
            cli_line_text(&line, named[list][at]);
            const unsigned long long seconds = (cli_line_closed(&line) != 0ull) ? query_newest_header(folder) : 0ull;
            newest = (seconds > newest) ? seconds : newest;
        }
    }
    return newest;
}

// 1 where an object stands: it is there, and neither its source nor the newest header its tool reads is newer
int query_fresh(int source_known, unsigned long long source, int object_known, unsigned long long object,
                unsigned long long newest_header)
{
    return object_known && source_known && (source <= object) && (newest_header <= object);
}

// 1 where `binary` is missing or older than a source of `tool`, or a header of a folder its sources stand in or it
// names
int query_stale(const char *root, const QueryTool *tool, const char *binary)
{
    unsigned long long built = 0ull;
    if (!query_modified(binary, &built))
    {
        return 1;
    }
    unsigned int c = 0u;
    unsigned int cxx = 0u;
    unsigned int tools = 0u;
    query_every_source(root, tool, &c, &cxx, &tools);
    char (*const lists[3])[QUERY_PATH_LONGEST] = {g_query_c_sources.path, g_query_cxx_sources.path,
                                                  g_query_tool_sources.path};
    const unsigned int counts[3] = {c, cxx, tools};
    unsigned long long newest = 0ull;
    int stale = 0;
    for (unsigned int list = 0u; !stale && (list < 3u); list += 1u)
    {
        for (unsigned int at = 0u; !stale && (at < counts[list]); at += 1u)
        {
            unsigned long long seconds = 0ull;
            stale = query_modified(lists[list][at], &seconds) && (seconds > built);
        }
        const unsigned long long seconds = stale ? 0ull : query_newest_header_of(root, tool, lists[list], counts[list]);
        newest = (seconds > newest) ? seconds : newest;
    }
    query_every_source_given();
    return stale || (newest > built);
}

int query_tool_built(const char *root, const QueryPrograms *programs, const char *name, char *binary, size_t room,
                     CliLine *said)
{
    char out[QUERY_PATH_LONGEST];
    CliLine line = {out, sizeof(out), 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/build/engine");
    cli_line_closed(&line);
    return query_tool_built_into(root, programs, name, out, binary, room, said);
}

// the object `source` is compiled to under `objects`, named by its path from the top of the tree with its separators
// made `_`, so that two sources of one base name in two folders stand apart
void query_object_of(const char *root, const char *objects, const char *source, char *object)
{
    const size_t root_length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = QUERY_PATH_LONGEST});
    const char *under = (cellul.starts(&(CatenaFinitaCfg){.src = source, .other = root, .cap = QUERY_PATH_LONGEST}) &&
                         (source[root_length] == '/'))
                            ? (source + root_length + 1u)
                            : source;
    CliLine line = {object, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, objects);
    cli_line_character(&line, '/');
    const unsigned long long start = line.at;
    cli_line_text(&line, under);
    for (unsigned long long at = start; at < line.at; at += 1ull)
    {
        line.out[at] = ((line.out[at] == '/') || (line.out[at] == '\\')) ? '_' : line.out[at];
    }
    cli_line_text(&line, ".o");
    cli_line_closed(&line);
}
