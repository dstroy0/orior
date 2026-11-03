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
#define QUERY_INCLUDES_MAX 32u
#define QUERY_COMPILE_WORDS (8u + 4u + QUERY_INCLUDES_MAX + 8u)
// One compile: the compiler, its words, the program first and NULL after the last, and the source they compile
typedef struct
{
    const char *words[QUERY_COMPILE_WORDS];
    const char *source;
} QueryCompile;

// the most of what a compiler or the linker printed retained to explain why a build failed, and the bytes it is taken
// to be stored in
#define QUERY_BUILD_OUTPUT_MAX_LEN 65536u

static char s_includes[QUERY_INCLUDES_MAX][QUERY_PATH_MAX_LEN + 2u];

typedef struct
{
    QueryProcess *process;
    VerbaLinea *output;
    const char *name;
    const char *what;
    VerbaLinea *err_msg;
    const QueryCompile *compiles;
    unsigned int count;
    QueryCompile *compile;
    const char *compiler;
    const char *const *flags;
    unsigned int includes;
    int as_cxx;
    const char *source;
    const char *object;
    const char *root;
    const QueryPrograms *programs;
    const QueryTool *tool;
    const char *out;
    const char *binary;
    const char *c_compiler;
    char *cxx_compiler;
} ToolsBuildCtx;
// the output of `process` read to its end, QUERY_BUILD_OUTPUT_MAX_LEN bytes of it at the most, into `output`, a line,
// its trailing blank lines left off and closed, and the process waited on: its exit code
EMBED_INLINE long long query_build_read(const ToolsBuildCtx *args)
{
    QueryProcess *process = args->process;
    VerbaLinea *output = args->output;
    char *const cell = g_cli_cells->temporary_buf_alloc(QUERY_BUILD_OUTPUT_MAX_LEN + 1ULL, &g_query_cells);
    *output = mmgr_line_over(cell, QUERY_BUILD_OUTPUT_MAX_LEN + 1ULL);
    for (;;)
    {
        char piece[4096];
        const unsigned long long got = EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx,
                                                  .process = process, .piece = piece, .capacity_u64 = sizeof(piece));
        if (got == 0ULL)
        {
            break;
        }
        const unsigned long long capacity =
            (output->at < QUERY_BUILD_OUTPUT_MAX_LEN) ? (QUERY_BUILD_OUTPUT_MAX_LEN - output->at) : 0ULL;
        mmgr_line_put_n(output, piece, (got < capacity) ? got : capacity);
    }
    // what the compiler printed, its trailing blank lines left off
    if (mmgr_line_ok(output))
    {
        output->at = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = output->out,
                                .at = output->at, .set = "\n\r");
    }
    (void)mmgr_line_finish(output);
    return EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = process);
}

// `failed`'s build failed: `build failed: <name> did not <what>` and what its compiler printed, `output`, into
// `err_msg`, and `output`'s cell given back
EMBED_INLINE void query_build_fail(const ToolsBuildCtx *args)
{
    mmgr_line_put(args->err_msg, "build failed: ");
    mmgr_line_put(args->err_msg, args->name);
    mmgr_line_put(args->err_msg, " did not ");
    mmgr_line_put(args->err_msg, args->what);
    mmgr_line_ch(args->err_msg, '\n');
    mmgr_line_put_n(args->err_msg, args->output->out, mmgr_line_ok(args->output) ? args->output->at : 0ULL);
    g_cli_cells->persistent_buf_release(args->output->out, &g_query_cells);
}

// Every compile of `compiles`, `count` of them, run QUERY_LAUNCHES_AT_ONCE at a time and the launch spacing between
// starts: 1, or 0 with the first that fails reported
EMBED_INLINE int query_compiled_at_once(const ToolsBuildCtx *args)
{
    unsigned int started = 0u;
    for (unsigned int first = 0u; first < args->count; first += QUERY_LAUNCHES_AT_ONCE)
    {
        const unsigned int end =
            ((args->count - first) < QUERY_LAUNCHES_AT_ONCE) ? args->count : (first + QUERY_LAUNCHES_AT_ONCE);
        QueryProcess running[QUERY_LAUNCHES_AT_ONCE];
        for (unsigned int at = first; at < end; at += 1u)
        {
            if (started != 0u)
            {
                EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx,
                           .milliseconds = QUERY_LAUNCH_SPACING_MILLISECONDS);
            }
            if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &running[at - first],
                            .words = args->compiles[at].words, .folder = NULL, .output = QUERY_OUTPUT_PIPED))
            {
                mmgr_line_put(args->err_msg, args->compiles[at].words[0]);
                mmgr_line_put(args->err_msg, ": it could not be started");
                return 0;
            }
            started += 1u;
        }
        int whole = 1;
        for (unsigned int at = first; at < end; at += 1u)
        {
            VerbaLinea output;
            const long long code =
                EMBED_CALL(query_build_read, ToolsBuildCtx, .process = &running[at - first], .output = &output);
            if (whole && (code != 0LL))
            {
                const char *const slash =
                    EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->compiles[at].source,
                               .length_value = cellul.len(
                                   &(CatenaFinitaCfg){.src = args->compiles[at].source, .cap = CLI_CELLS_POOL}),
                               .byte = '/');
                EMBED_CALL(query_build_fail, ToolsBuildCtx,
                           .name = (slash != NULL) ? (slash + 1) : args->compiles[at].source, .what = "compile",
                           .output = &output, .err_msg = args->err_msg);
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
EMBED_INLINE void query_compile_add(const ToolsBuildCtx *args)
{
    const char *source = args->source;
    unsigned int word = 0u;
    args->compile->words[word++] = args->compiler;
    for (unsigned int flag = 0u; (args->flags[flag] != NULL) && (word < 8u); flag += 1u)
    {
        args->compile->words[word++] = args->flags[flag];
    }
    for (unsigned int setting = 0u; g_query_mmgr_settings[setting] != NULL; setting += 1u)
    {
        args->compile->words[word++] = g_query_mmgr_settings[setting];
    }
    for (unsigned int include = 0u; include < args->includes; include += 1u)
    {
        args->compile->words[word++] = s_includes[include];
    }
    if (args->as_cxx)
    {
        args->compile->words[word++] = "-x";
        args->compile->words[word++] = "c++";
    }
    args->compile->words[word++] = "-c";
    args->compile->words[word++] = source;
    args->compile->words[word++] = "-o";
    args->compile->words[word++] = args->object;
    args->compile->words[word] = NULL;
    args->compile->source = source;
}

// the stale tool `tool` compiled and linked into `binary` as query_tool_built_into builds it, its C compiler
// `c_compiler`, and its C++ compiler found into `cxx_compiler`, sized for QUERY_PATH_MAX_LEN, where a source needs
// one: 1, or 0 with why given
EMBED_INLINE int query_tool_compile(const ToolsBuildCtx *args);

int query_tool_built_into(const QueryNameCapacityCtx *args)
{
    const QueryTool *const tool = EMBED_CALL(query_tools_internal.tool_by_name, QueryToolsCtx, .name = args->name);
    if (tool == NULL)
    {
        mmgr_line_put(args->err_msg, "no tool ");
        mmgr_line_put(args->err_msg, args->name);
        return 0;
    }
    char base[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_name_capacity_internal.binary_name, QueryNameCapacityCtx, .name = args->name, .binary = base,
               .capacity = sizeof(base));
    VerbaLinea binary_line = {args->binary, args->capacity, 0ULL};
    mmgr_line_put(&binary_line, args->out_text);
    mmgr_line_ch(&binary_line, '/');
    mmgr_line_put(&binary_line, base);
    if ((mmgr_line_finish(&binary_line) == 0ULL) ||
        !EMBED_CALL(query_tools_root_tool_internal.stale, QueryToolsRootToolCtx, .root = args->root, .tool = tool,
                    .binary = args->binary))
    {
        return args->binary[0] != '\0';
    }
    static char s_c[QUERY_PATH_MAX_LEN];
    static char s_cxx[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(args->programs->program, QueryEachRolePathCapacityErrMsgCfg, .context = args->programs->context,
                    .role = QUERY_ROLE_C, .path = s_c, .capacity = sizeof(s_c), .err_msg = args->err_msg))
    {
        return 0;
    }
    const int built = EMBED_CALL(query_tool_compile, ToolsBuildCtx, .root = args->root, .programs = args->programs,
                                 .name = args->name, .tool = tool, .out = args->out_text, .binary = args->binary,
                                 .c_compiler = s_c, .cxx_compiler = s_cxx, .err_msg = args->err_msg);
    query_every_source_free();
    return built;
}

EMBED_INLINE int query_tool_compile(const ToolsBuildCtx *args)
{
    const char *root = args->root;
    const char *name = args->name;
    const QueryTool *tool = args->tool;
    unsigned int c = 0u;
    unsigned int cxx = 0u;
    unsigned int tools = 0u;
    EMBED_CALL(query_tools_root_tool_c_internal.every_source, QueryToolsRootToolCCtx, .root = root, .tool = tool,
               .c = &c, .cxx = &cxx, .tools = &tools);
    const int with_cxx = (cxx != 0u) || (tools != 0u);
    if (with_cxx && !EMBED_CALL(args->programs->program, QueryEachRolePathCapacityErrMsgCfg,
                                .context = args->programs->context, .role = QUERY_ROLE_CXX, .path = args->cxx_compiler,
                                .capacity = QUERY_PATH_MAX_LEN, .err_msg = args->err_msg))
    {
        return 0;
    }
    char objects[QUERY_PATH_MAX_LEN];
    VerbaLinea objects_line = {objects, sizeof(objects), 0ULL};
    mmgr_line_put(&objects_line, args->out);
    mmgr_line_put(&objects_line, "/objects/");
    mmgr_line_put(&objects_line, name);
    if ((mmgr_line_finish(&objects_line) == 0ULL) ||
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = objects))
    {
        mmgr_line_put(args->err_msg, objects);
        mmgr_line_put(args->err_msg, " could not be made");
        return 0;
    }
    // MMgr's include folders first, then the tool's own
    unsigned int includes = 0u;
    const char *const *const folders[2] = {g_query_mmgr_includes, tool->includes};
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        for (unsigned int at = 0u; (folders[list][at] != NULL) && (includes < QUERY_INCLUDES_MAX); at += 1u)
        {
            VerbaLinea line = {s_includes[includes], sizeof(s_includes[includes]), 0ULL};
            mmgr_line_put(&line, "-I");
            mmgr_line_put(&line, root);
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, folders[list][at]);
            includes += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
        }
    }
    char (*const lists[3])[QUERY_PATH_MAX_LEN] = {g_query_c_sources.path, g_query_cxx_sources.path,
                                                  g_query_tool_sources.path};
    const unsigned int counts[3] = {c, cxx, tools};
    unsigned long long newest = 0ULL;
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        const unsigned long long seconds =
            EMBED_CALL(query_tools_root_tool_sources_internal.newest_header_get, QueryToolsRootToolSourcesCtx,
                       .root = root, .tool = tool, .sources = lists[list], .count = counts[list]);
        newest = (seconds > newest) ? seconds : newest;
    }
    // the objects' capacity taken for every source before one is named, as each compile reserves its object's place
    if (!EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &g_query_objects,
                    .count = c + cxx + tools))
    {
        mmgr_line_put(args->err_msg, "the objects of ");
        mmgr_line_put(args->err_msg, name);
        mmgr_line_put(args->err_msg, " could not be given a cell");
        return 0;
    }
    // a compile for each source and the link's words, the compiler, its output, each object, the tool's own words and
    // the NULL after them, in one cell of their count
    unsigned int link_words = 0u;
    while (tool->link[link_words] != NULL)
    {
        link_words += 1u;
    }
    const unsigned long long compile_bytes = (c + cxx + tools + 1ULL) * sizeof(QueryCompile);
    char *const cell = g_cli_cells->persistent_buf_alloc(
        compile_bytes + ((c + cxx + tools + link_words + 4ULL) * sizeof(const char *)), &g_query_cells);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "the compiles of ");
        mmgr_line_put(args->err_msg, name);
        mmgr_line_put(args->err_msg, " could not be given a cell");
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
            EMBED_CALL(query_tools_root_objects_internal.object_get, QueryToolsRootObjectsCtx, .root = root,
                       .objects = objects, .source_text = source, .object_capacity = object);
            g_query_objects.count += 1u;
            unsigned long long source_time = 0ULL;
            unsigned long long object_time = 0ULL;
            const int source_known = EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = source,
                                                .seconds = &source_time);
            const int object_known = EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = object,
                                                .seconds = &object_time);
            if (!EMBED_CALL(query_tools_source_known_internal.fresh, QueryToolsSourceKnownCtx,
                            .source_known = source_known, .source = source_time, .object_known = object_known,
                            .object = object_time, .newest_header = newest))
            {
                EMBED_CALL(query_compile_add, ToolsBuildCtx, .compile = &compile_list[compiles],
                           .compiler = (list == 0u) ? args->c_compiler : args->cxx_compiler,
                           .flags = (list == 0u) ? tool->c_flags : tool->cxx_flags, .includes = includes,
                           .as_cxx = list == 1u, .source = source, .object = object);
                compiles += 1u;
            }
            objected += 1u;
        }
    }
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "PATH",
               .value = args->programs->run_path(args->programs->context));
    if (!EMBED_CALL(query_compiled_at_once, ToolsBuildCtx, .compiles = compile_list, .count = compiles,
                    .err_msg = args->err_msg))
    {
        g_cli_cells->persistent_buf_release(cell, &g_query_cells);
        return 0;
    }
    unsigned int word = 0u;
    link[word++] = with_cxx ? args->cxx_compiler : args->c_compiler;
    link[word++] = "-o";
    link[word++] = args->binary;
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
    int built = EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &linked, .words = link,
                           .folder = NULL, .output = QUERY_OUTPUT_PIPED);
    if (!built)
    {
        mmgr_line_put(args->err_msg, link[0]);
        mmgr_line_put(args->err_msg, ": it could not be started");
    }
    VerbaLinea output;
    if (built && (EMBED_CALL(query_build_read, ToolsBuildCtx, .process = &linked, .output = &output) != 0LL))
    {
        EMBED_CALL(query_build_fail, ToolsBuildCtx, .name = name, .what = "link", .output = &output,
                   .err_msg = args->err_msg);
        built = 0;
    }
    else if (built)
    {
        g_cli_cells->persistent_buf_release(output.out, &g_query_cells);
    }
    g_cli_cells->persistent_buf_release(cell, &g_query_cells);
    return built;
}
