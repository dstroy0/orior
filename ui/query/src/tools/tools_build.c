// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tools_build.c: a tool built into a folder where stale: every source an object under `<out>/objects/<name>`,
// compiled where it is missing or older than its source or the newest header the tool reads, the compilers run as
// many at once as the launch queue lets, and every object linked; a missing compiler said before anything is compiled
#include "../../../cli/src/text/text.h"
#include "tools_internal.h"

// the most include folders a tool names, MMgr's among them, and the most words one compile is given: the compiler,
// its flags, MMgr's settings, the includes and the source and object
#define QUERY_INCLUDES_MOST 32u

#define QUERY_COMPILE_WORDS (8u + 4u + QUERY_INCLUDES_MOST + 8u)

// what a compiler or the linker printed, kept to say why a build failed
#define QUERY_BUILD_SAID 65536u

// One compile: the compiler, its words, the program first and NULL after the last, and the source they compile
typedef struct
{
    const char *words[QUERY_COMPILE_WORDS];
    const char *source;
} QueryCompile;

static QueryCompile s_compiles[3u * QUERY_SOURCES_MOST];
static char s_includes[QUERY_INCLUDES_MOST][QUERY_PATH_LONGEST + 2u];
static const char *s_link[(3u * QUERY_SOURCES_MOST) + 16u];
static char s_build_said[QUERY_BUILD_SAID];

// the output of `process` read to its end into s_build_said, its length, and the process waited on: its exit code
static long long query_build_read(QueryProcess *process, unsigned long long *length)
{
    *length = 0ull;
    for (;;)
    {
        char piece[4096];
        const unsigned long long got = query_process_read(process, piece, sizeof(piece));
        if (got == 0ull)
        {
            break;
        }
        const unsigned long long room = sizeof(s_build_said) - 1u - *length;
        const unsigned long long kept = (got < room) ? got : room;
        memor.cpy(&(MemoriaCfg){.dst = s_build_said + *length, .src = piece, .bytes = (size_t)kept});
        *length += kept;
    }
    // what the compiler said, its trailing blank lines left off
    while ((*length != 0ull) && ((s_build_said[*length - 1u] == '\n') || (s_build_said[*length - 1u] == '\r')))
    {
        *length -= 1ull;
    }
    return query_process_wait(process);
}

// `failed`'s build failed: `build failed: <name> did not <what>` and what its compiler said, into `said`
static void query_build_failed(const char *name, const char *what, unsigned long long length, CliLine *said)
{
    cli_line_text(said, "build failed: ");
    cli_line_text(said, name);
    cli_line_text(said, " did not ");
    cli_line_text(said, what);
    cli_line_character(said, '\n');
    cli_line_text_most(said, s_build_said, length);
}

// Every compile of `compiles`, `count` of them, run QUERY_LAUNCHES_AT_ONCE at a time and the launch spacing between
// starts: 1, or 0 with the first that fails said
static int query_compiled_at_once(unsigned int count, CliLine *said)
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
            if (!query_process_start(&running[at - first], s_compiles[at].words, NULL, QUERY_OUTPUT_PIPED))
            {
                cli_line_text(said, s_compiles[at].words[0]);
                cli_line_text(said, ": it could not be started");
                return 0;
            }
            started += 1u;
        }
        int whole = 1;
        for (unsigned int at = first; at < past; at += 1u)
        {
            unsigned long long length = 0ull;
            const long long code = query_build_read(&running[at - first], &length);
            if (whole && (code != 0ll))
            {
                const char *const slash = cli_text_last(
                    s_compiles[at].source,
                    cellul.len(&(CatenaFinitaCfg){.src = s_compiles[at].source, .cap = CLI_CELLS_POOL}), '/');
                query_build_failed((slash != NULL) ? (slash + 1) : s_compiles[at].source, "compile", length, said);
                whole = 0;
            }
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
static void query_compile_added(unsigned int at, const char *compiler, const char *const *flags, unsigned int includes,
                                int as_cxx, const char *source, const char *object)
{
    QueryCompile *const compile = &s_compiles[at];
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

int query_tool_built_into(const char *root, const QueryPrograms *programs, const char *name, const char *out,
                          char *binary, size_t room, CliLine *said)
{
    const QueryTool *const tool = query_tool_named(name);
    if (tool == NULL)
    {
        cli_line_text(said, "no tool ");
        cli_line_text(said, name);
        return 0;
    }
    char base[QUERY_PATH_LONGEST];
    query_binary_name(name, base, sizeof(base));
    CliLine binary_line = {binary, room, 0ull};
    cli_line_text(&binary_line, out);
    cli_line_character(&binary_line, '/');
    cli_line_text(&binary_line, base);
    if ((cli_line_closed(&binary_line) == 0ull) || !query_stale(root, tool, binary))
    {
        return binary[0] != '\0';
    }
    static char s_c[QUERY_PATH_LONGEST];
    static char s_cxx[QUERY_PATH_LONGEST];
    if (!programs->program(programs->context, QUERY_ROLE_C, s_c, sizeof(s_c), said))
    {
        return 0;
    }
    unsigned int c = 0u;
    unsigned int cxx = 0u;
    unsigned int tools = 0u;
    query_every_source(root, tool, &c, &cxx, &tools);
    const int with_cxx = (cxx != 0u) || (tools != 0u);
    if (with_cxx && !programs->program(programs->context, QUERY_ROLE_CXX, s_cxx, sizeof(s_cxx), said))
    {
        return 0;
    }
    char objects[QUERY_PATH_LONGEST];
    CliLine objects_line = {objects, sizeof(objects), 0ull};
    cli_line_text(&objects_line, out);
    cli_line_text(&objects_line, "/objects/");
    cli_line_text(&objects_line, name);
    if ((cli_line_closed(&objects_line) == 0ull) || !query_folder_made(objects))
    {
        cli_line_text(said, objects);
        cli_line_text(said, " could not be made");
        return 0;
    }
    // MMgr's include folders first, then the tool's own
    unsigned int includes = 0u;
    const char *const *const folders[2] = {g_query_mmgr_includes, tool->includes};
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        for (unsigned int at = 0u; (folders[list][at] != NULL) && (includes < QUERY_INCLUDES_MOST); at += 1u)
        {
            CliLine line = {s_includes[includes], sizeof(s_includes[includes]), 0ull};
            cli_line_text(&line, "-I");
            cli_line_text(&line, root);
            cli_line_character(&line, '/');
            cli_line_text(&line, folders[list][at]);
            includes += (cli_line_closed(&line) != 0ull) ? 1u : 0u;
        }
    }
    char (*const lists[3])[QUERY_PATH_LONGEST] = {s_c_sources, s_cxx_sources, s_tool_sources};
    const unsigned int counts[3] = {c, cxx, tools};
    unsigned long long newest = 0ull;
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        const unsigned long long seconds = query_newest_header_of(root, tool, lists[list], counts[list]);
        newest = (seconds > newest) ? seconds : newest;
    }
    unsigned int compiles = 0u;
    unsigned int objected = 0u;
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        for (unsigned int at = 0u; at < counts[list]; at += 1u)
        {
            const char *const source = lists[list][at];
            query_object_of(root, objects, source, s_objects[objected]);
            unsigned long long source_time = 0ull;
            unsigned long long object_time = 0ull;
            const int source_known = query_modified(source, &source_time);
            const int object_known = query_modified(s_objects[objected], &object_time);
            if (!query_fresh(source_known, source_time, object_known, object_time, newest))
            {
                query_compile_added(compiles, (list == 0u) ? s_c : s_cxx,
                                    (list == 0u) ? tool->c_flags : tool->cxx_flags, includes, list == 1u, source,
                                    s_objects[objected]);
                compiles += 1u;
            }
            objected += 1u;
        }
    }
    query_environment_set("PATH", programs->run_path(programs->context));
    if (!query_compiled_at_once(compiles, said))
    {
        return 0;
    }
    unsigned int word = 0u;
    s_link[word++] = with_cxx ? s_cxx : s_c;
    s_link[word++] = "-o";
    s_link[word++] = binary;
    for (unsigned int at = 0u; at < objected; at += 1u)
    {
        s_link[word++] = s_objects[at];
    }
    for (unsigned int at = 0u; (tool->link[at] != NULL) && (word < (sizeof(s_link) / sizeof(s_link[0])) - 1u); at += 1u)
    {
        s_link[word++] = tool->link[at];
    }
    s_link[word] = NULL;
    QueryProcess linked;
    if (!query_process_start(&linked, s_link, NULL, QUERY_OUTPUT_PIPED))
    {
        cli_line_text(said, s_link[0]);
        cli_line_text(said, ": it could not be started");
        return 0;
    }
    unsigned long long length = 0ull;
    if (query_build_read(&linked, &length) != 0ll)
    {
        query_build_failed(name, "link", length, said);
        return 0;
    }
    return 1;
}
