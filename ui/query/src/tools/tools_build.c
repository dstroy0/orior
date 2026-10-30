// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tools_build.c: a tool built into a folder where stale: every source an object under `<out>/objects/<name>`,
// compiled where it is missing or older than its source or the newest header the tool reads, the compilers run as
// many at once as the launch queue lets, and every object linked; a missing compiler reported before anything is
// compiled
#include "../../../cli/src/text/text.h"
#include "tools_internal.h"

// the most include folders a tool names, MMgr's among them, and the most words one compile is given: the compiler,
// its flags, MMgr's settings, the includes and the source and object
#define QUERY_INCLUDES_MOST 32u

#define QUERY_COMPILE_WORDS (8u + 4u + QUERY_INCLUDES_MOST + 8u)

// the most of what a compiler or the linker printed retained to explain why a build failed, and the bytes it is begun
// to be stored in, doubling as it is
#define QUERY_BUILD_OUTPUT_MAX_LEN 65536u
#define QUERY_BUILD_OUTPUT_INIT_CAP 4096u

// One compile: the compiler, its words, the program first and NULL after the last, and the source they compile
typedef struct
{
    const char *words[QUERY_COMPILE_WORDS];
    const char *source;
} QueryCompile;

static char s_includes[QUERY_INCLUDES_MOST][QUERY_PATH_MAX_LEN + 2u];

// the output of `process` read to its end, QUERY_BUILD_OUTPUT_MAX_LEN bytes of it at the most, into `output`, a growing
// line, its trailing blank lines left off and closed, and the process waited on: its exit code
static long long query_build_read(QueryProcess *process, VerbaLinea *output)
{
    (void)cli_growing_line_made(output, QUERY_BUILD_OUTPUT_INIT_CAP, &g_query_cells);
    for (;;)
    {
        char piece[4096];
        const unsigned long long got = query_process_read(process, piece, sizeof(piece));
        if (got == 0ull)
        {
            break;
        }
        const unsigned long long room =
            (output->at < QUERY_BUILD_OUTPUT_MAX_LEN) ? (QUERY_BUILD_OUTPUT_MAX_LEN - output->at) : 0ull;
        mmgr_line_put_n(output, piece, (got < room) ? got : room);
    }
    // what the compiler printed, its trailing blank lines left off
    while (mmgr_line_ok(output) && (output->at != 0ull) &&
           ((output->out[output->at - 1u] == '\n') || (output->out[output->at - 1u] == '\r')))
    {
        output->at -= 1ull;
    }
    (void)mmgr_line_finish(output);
    return query_process_wait(process);
}

// `failed`'s build failed: `build failed: <name> did not <what>` and what its compiler printed, `output`, into
// `err_msg`, and `output`'s cell given back
static void query_build_failed(const char *name, const char *what, VerbaLinea *output, VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, "build failed: ");
    mmgr_line_put(err_msg, name);
    mmgr_line_put(err_msg, " did not ");
    mmgr_line_put(err_msg, what);
    mmgr_line_ch(err_msg, '\n');
    mmgr_line_put_n(err_msg, output->out, mmgr_line_ok(output) ? output->at : 0ull);
    g_cli_cells->persistent_buf_release(output->out, &g_query_cells);
}

// Every compile of `compiles`, `count` of them, run QUERY_LAUNCHES_AT_ONCE at a time and the launch spacing between
// starts: 1, or 0 with the first that fails reported
static int query_compiled_at_once(const QueryCompile *compiles, unsigned int count, VerbaLinea *err_msg)
{
    unsigned int started = 0u;
    for (unsigned int first = 0u; first < count; first += QUERY_LAUNCHES_AT_ONCE)
    {
        const unsigned int past = ((count - first) < QUERY_LAUNCHES_AT_ONCE) ? count : (first + QUERY_LAUNCHES_AT_ONCE);
        QueryProcess running[QUERY_LAUNCHES_AT_ONCE];
        for (unsigned int at = first; at < past; at += 1u)
        {
            if (started != 0u)
            {
                query_paused(QUERY_LAUNCH_SPACING_MILLISECONDS);
            }
            if (!query_process_start(&running[at - first], compiles[at].words, NULL, QUERY_OUTPUT_PIPED))
            {
                mmgr_line_put(err_msg, compiles[at].words[0]);
                mmgr_line_put(err_msg, ": it could not be started");
                return 0;
            }
            started += 1u;
        }
        int whole = 1;
        for (unsigned int at = first; at < past; at += 1u)
        {
            VerbaLinea output;
            const long long code = query_build_read(&running[at - first], &output);
            if (whole && (code != 0ll))
            {
                const char *const slash = cli_text_last(
                    compiles[at].source,
                    cellul.len(&(CatenaFinitaCfg){.src = compiles[at].source, .cap = CLI_CELLS_POOL}), '/');
                query_build_failed((slash != NULL) ? (slash + 1) : compiles[at].source, "compile", &output, err_msg);
                whole = 0;
                continue;
            }
            g_cli_cells->persistent_buf_release(output.out, &g_query_cells);
        }
        if (!whole)
        {
            return 0;
        }
    }
    return 1;
}

// the compile of `source` into `object` with `compiler`, `flags`, the includes and, where `as_cxx`, `-x c++`, added
// to the compiles: its place
static void query_compile_added(QueryCompile *compile, const char *compiler, const char *const *flags,
                                unsigned int includes, int as_cxx, const char *source, const char *object)
{
    unsigned int word = 0u;
    compile->words[word++] = compiler;
    for (unsigned int flag = 0u; (flags[flag] != NULL) && (word < 8u); flag += 1u)
    {
        compile->words[word++] = flags[flag];
    }
    for (unsigned int setting = 0u; g_query_mmgr_settings[setting] != NULL; setting += 1u)
    {
        compile->words[word++] = g_query_mmgr_settings[setting];
    }
    for (unsigned int include = 0u; include < includes; include += 1u)
    {
        compile->words[word++] = s_includes[include];
    }
    if (as_cxx)
    {
        compile->words[word++] = "-x";
        compile->words[word++] = "c++";
    }
    compile->words[word++] = "-c";
    compile->words[word++] = source;
    compile->words[word++] = "-o";
    compile->words[word++] = object;
    compile->words[word] = NULL;
    compile->source = source;
}

// the stale tool `tool` compiled and linked into `binary` as query_tool_built_into builds it, its C compiler
// `c_compiler`, and its C++ compiler found into `cxx_compiler`, sized for QUERY_PATH_MAX_LEN, where a source needs
// one: 1, or 0 with why given
static int query_tool_compiled(const char *root, const QueryPrograms *programs, const char *name, const QueryTool *tool,
                               const char *out, const char *binary, const char *c_compiler, char *cxx_compiler,
                               VerbaLinea *err_msg);

int query_tool_built_into(const char *root, const QueryPrograms *programs, const char *name, const char *out,
                          char *binary, size_t room, VerbaLinea *err_msg)
{
    const QueryTool *const tool = query_tool_named(name);
    if (tool == NULL)
    {
        mmgr_line_put(err_msg, "no tool ");
        mmgr_line_put(err_msg, name);
        return 0;
    }
    char base[QUERY_PATH_MAX_LEN];
    query_binary_name(name, base, sizeof(base));
    VerbaLinea binary_line = {binary, room, 0ull};
    mmgr_line_put(&binary_line, out);
    mmgr_line_ch(&binary_line, '/');
    mmgr_line_put(&binary_line, base);
    if ((mmgr_line_finish(&binary_line) == 0ull) || !query_stale(root, tool, binary))
    {
        return binary[0] != '\0';
    }
    static char s_c[QUERY_PATH_MAX_LEN];
    static char s_cxx[QUERY_PATH_MAX_LEN];
    if (!programs->program(programs->context, QUERY_ROLE_C, s_c, sizeof(s_c), err_msg))
    {
        return 0;
    }
    const int built = query_tool_compiled(root, programs, name, tool, out, binary, s_c, s_cxx, err_msg);
    query_every_source_given();
    return built;
}

static int query_tool_compiled(const char *root, const QueryPrograms *programs, const char *name, const QueryTool *tool,
                               const char *out, const char *binary, const char *c_compiler, char *cxx_compiler,
                               VerbaLinea *err_msg)
{
    unsigned int c = 0u;
    unsigned int cxx = 0u;
    unsigned int tools = 0u;
    query_every_source(root, tool, &c, &cxx, &tools);
    const int with_cxx = (cxx != 0u) || (tools != 0u);
    if (with_cxx && !programs->program(programs->context, QUERY_ROLE_CXX, cxx_compiler, QUERY_PATH_MAX_LEN, err_msg))
    {
        return 0;
    }
    char objects[QUERY_PATH_MAX_LEN];
    VerbaLinea objects_line = {objects, sizeof(objects), 0ull};
    mmgr_line_put(&objects_line, out);
    mmgr_line_put(&objects_line, "/objects/");
    mmgr_line_put(&objects_line, name);
    if ((mmgr_line_finish(&objects_line) == 0ull) || !query_folder_made(objects))
    {
        mmgr_line_put(err_msg, objects);
        mmgr_line_put(err_msg, " could not be made");
        return 0;
    }
    // MMgr's include folders first, then the tool's own
    unsigned int includes = 0u;
    const char *const *const folders[2] = {g_query_mmgr_includes, tool->includes};
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        for (unsigned int at = 0u; (folders[list][at] != NULL) && (includes < QUERY_INCLUDES_MOST); at += 1u)
        {
            VerbaLinea line = {s_includes[includes], sizeof(s_includes[includes]), 0ull};
            mmgr_line_put(&line, "-I");
            mmgr_line_put(&line, root);
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, folders[list][at]);
            includes += (mmgr_line_finish(&line) != 0ull) ? 1u : 0u;
        }
    }
    char (*const lists[3])[QUERY_PATH_MAX_LEN] = {g_query_c_sources.path, g_query_cxx_sources.path,
                                                  g_query_tool_sources.path};
    const unsigned int counts[3] = {c, cxx, tools};
    unsigned long long newest = 0ull;
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        const unsigned long long seconds = query_newest_header_of(root, tool, lists[list], counts[list]);
        newest = (seconds > newest) ? seconds : newest;
    }
    // the objects' room taken for every source before one is named, as each compile reserves its object's place
    if (!query_paths_reserved(&g_query_objects, c + cxx + tools))
    {
        mmgr_line_put(err_msg, "the objects of ");
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " could not be given a cell");
        return 0;
    }
    // a compile for each source and the link's words, the compiler, its output, each object, the tool's own words and
    // the NULL after them, in one cell of their count
    unsigned int link_words = 0u;
    while (tool->link[link_words] != NULL)
    {
        link_words += 1u;
    }
    const unsigned long long compile_bytes = (c + cxx + tools + 1ull) * sizeof(QueryCompile);
    char *const cell = g_cli_cells->persistent_buf_alloc(
        compile_bytes + ((c + cxx + tools + link_words + 4ull) * sizeof(const char *)), &g_query_cells);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "the compiles of ");
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " could not be given a cell");
        return 0;
    }
    // the compiles on the cell's boundary, the link's words after them
    QueryCompile *const compile_list = (QueryCompile *)(void *)cell;
    const char **const link = (const char **)(void *)(cell + compile_bytes);
    unsigned int compiles = 0u;
    unsigned int objected = 0u;
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        for (unsigned int at = 0u; at < counts[list]; at += 1u)
        {
            const char *const source = lists[list][at];
            char *const object = g_query_objects.path[objected];
            query_object_of(root, objects, source, object);
            g_query_objects.count += 1u;
            unsigned long long source_time = 0ull;
            unsigned long long object_time = 0ull;
            const int source_known = query_modified(source, &source_time);
            const int object_known = query_modified(object, &object_time);
            if (!query_fresh(source_known, source_time, object_known, object_time, newest))
            {
                query_compile_added(&compile_list[compiles], (list == 0u) ? c_compiler : cxx_compiler,
                                    (list == 0u) ? tool->c_flags : tool->cxx_flags, includes, list == 1u, source,
                                    object);
                compiles += 1u;
            }
            objected += 1u;
        }
    }
    query_environment_set("PATH", programs->run_path(programs->context));
    if (!query_compiled_at_once(compile_list, compiles, err_msg))
    {
        g_cli_cells->persistent_buf_release(cell, &g_query_cells);
        return 0;
    }
    unsigned int word = 0u;
    link[word++] = with_cxx ? cxx_compiler : c_compiler;
    link[word++] = "-o";
    link[word++] = binary;
    for (unsigned int at = 0u; at < objected; at += 1u)
    {
        link[word++] = g_query_objects.path[at];
    }
    for (unsigned int at = 0u; at < link_words; at += 1u)
    {
        link[word++] = tool->link[at];
    }
    link[word] = NULL;
    QueryProcess linked;
    int built = query_process_start(&linked, link, NULL, QUERY_OUTPUT_PIPED);
    if (!built)
    {
        mmgr_line_put(err_msg, link[0]);
        mmgr_line_put(err_msg, ": it could not be started");
    }
    VerbaLinea output;
    if (built && (query_build_read(&linked, &output) != 0ll))
    {
        query_build_failed(name, "link", &output, err_msg);
        built = 0;
    }
    else if (built)
    {
        g_cli_cells->persistent_buf_release(output.out, &g_query_cells);
    }
    g_cli_cells->persistent_buf_release(cell, &g_query_cells);
    return built;
}
