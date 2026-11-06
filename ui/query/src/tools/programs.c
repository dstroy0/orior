// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// programs.c: the programs the query needs, named by role and never by path: each role's name as a person reads it,
// and the programs as the PATH alone gives them, which the command line falls back to where no toolchain is chosen
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

// what the walk for MSVC's host compiler records: the folder it stands in, a level's names, and the newest cl found
typedef struct
{
    char folder[QUERY_PATH_MAX_LEN];
    char found[QUERY_PATH_MAX_LEN];
    unsigned int depth;
} QueryClWalk;
#if defined(_WIN32)
#define QUERY_PATH_SEPARATOR ';'
#define QUERY_FOLDER_SEPARATOR '\\'
#else
#define QUERY_PATH_SEPARATOR ':'
#define QUERY_FOLDER_SEPARATOR '/'
#endif

// the longest PATH the query reads
#define QUERY_RUN_PATH_MAX_LEN 32768u

const char *query_role_name(const QueryCfg *args)
{
    static const char *const s_names[QUERY_ROLE_COUNT] = {
        "the C compiler", "the C++ compiler", "nvcc",   "nvdisasm", "cuobjdump", "cl",
        "python",         "nvidia-smi",       "docker", "curl",     "tar"};
    return ((unsigned int)args->role < (unsigned int)QUERY_ROLE_COUNT) ? s_names[args->role] : "a program";
}

typedef struct
{
    const char *folder;
    unsigned long long folder_length;
    const char *name;
    char *path;
    size_t capacity;
    const char *const *names;
    unsigned int count;
    QueryClWalk *walk;
    unsigned int depth;
} ProgramsCtx;
// `name` as a program in `folder`, with `.exe` on Windows, where it is there, written into `path`: 1, or 0
EMBED_INLINE int query_in_folder(const ProgramsCtx *args)
{
    char *path = args->path;
    VerbaLinea line = {path, args->capacity, 0ULL};
    mmgr_line_put_n(&line, args->folder, args->folder_length);
    mmgr_line_ch(&line, QUERY_FOLDER_SEPARATOR);
    mmgr_line_put(&line, args->name);
    if ((mmgr_line_finish(&line) != 0ULL) &&
        EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        return 1;
    }
#if defined(_WIN32)
    line.at = 0ULL;
    mmgr_line_put_n(&line, args->folder, args->folder_length);
    mmgr_line_ch(&line, QUERY_FOLDER_SEPARATOR);
    mmgr_line_put(&line, args->name);
    mmgr_line_put(&line, ".exe");
    return (mmgr_line_finish(&line) != 0ULL) &&
           EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
#else
    return 0;
#endif
}

static const char *query_run_path_read(void *context)
{
    (void)context;
    static char s_path[QUERY_ENVIRONMENT_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH", .value_out = s_path,
                    .capacity = sizeof(s_path)))
    {
        s_path[0] = '\0';
    }
    return s_path;
}

// the first of `names`, `count` of them, found as a program in a folder of the PATH, written into `path`: 1, or 0
EMBED_INLINE int query_on_path_find(const ProgramsCtx *args)
{
    const char *const run_path = query_run_path_read(NULL);
    const char *folder = run_path;
    while (*folder != '\0')
    {
        const char *const end =
            cellul.chr(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES, .byte = QUERY_PATH_SEPARATOR});
        const unsigned long long length = (end != NULL)
                                              ? (unsigned long long)(end - folder)
                                              : cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES});
        for (unsigned int at = 0u; (length != 0ULL) && (at < args->count); at += 1u)
        {
            if (EMBED_CALL(query_in_folder, ProgramsCtx, .folder = folder, .folder_length = length,
                           .name = args->names[at], .path = args->path, .capacity = args->capacity))
            {
                return 1;
            }
        }
        folder += length + ((end != NULL) ? 1u : 0u);
    }
    return 0;
}

static void query_cl_walk(const CliEachNameIsFolderCfg *args);

// the folder `walk` stands in joined with `name`, walked one level down at `depth`, and stood back from
EMBED_INLINE void query_cl_enter(const ProgramsCtx *args)
{
    unsigned int depth = args->depth;
    const size_t existing = cellul.len(&(CatenaFinitaCfg){.src = args->walk->folder, .cap = CLI_POOL_BYTES});
    VerbaLinea line = {args->walk->folder, sizeof(args->walk->folder), existing};
    mmgr_line_ch(&line, QUERY_FOLDER_SEPARATOR);
    mmgr_line_put(&line, args->name);
    if (mmgr_line_finish(&line) != 0ULL)
    {
        const unsigned int saved_depth = args->walk->depth;
        args->walk->depth = depth;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->walk->folder,
                   .each = query_cl_walk, .context_bytess = args->walk);
        args->walk->depth = saved_depth;
    }
    args->walk->folder[existing] = '\0';
}

// The walk under a base: depth 0 a year or a version, the version containing `bin\Hostx64\x64\cl.exe` directly or the
// year an edition containing `VC\Tools\MSVC\<version>`; the found recorded where its path sorts last
static void query_cl_walk(const CliEachNameIsFolderCfg *args)
{
    void *context = args->context;
    const char *name = args->name;
    QueryClWalk *const walk = (QueryClWalk *)context;
    if (!args->is_folder)
    {
        return;
    }
    if ((walk->depth == 0u) || (walk->depth == 2u))
    {
        char cl[QUERY_PATH_MAX_LEN];
        VerbaLinea line = {cl, sizeof(cl), 0ULL};
        mmgr_line_put(&line, walk->folder);
        mmgr_line_ch(&line, QUERY_FOLDER_SEPARATOR);
        mmgr_line_put(&line, name);
        mmgr_line_put(&line, "\\bin\\Hostx64\\x64\\cl.exe");
        if ((mmgr_line_finish(&line) != 0ULL) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = cl) &&
            (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = cl, .other_pointer = walk->found) > 0))
        {
            memor.cpy(&(MemoriaCfg){.dst = walk->found,
                                    .src = cl,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_POOL_BYTES}) + 1u});
        }
    }
    if (walk->depth == 0u)
    {
        EMBED_CALL(query_cl_enter, ProgramsCtx, .walk = walk, .name = name, .depth = 1u);
    }
    else if (walk->depth == 1u)
    {
        const size_t existing = cellul.len(&(CatenaFinitaCfg){.src = walk->folder, .cap = CLI_POOL_BYTES});
        VerbaLinea line = {walk->folder, sizeof(walk->folder), existing};
        mmgr_line_ch(&line, QUERY_FOLDER_SEPARATOR);
        mmgr_line_put(&line, name);
        mmgr_line_put(&line, "\\VC\\Tools\\MSVC");
        if (mmgr_line_finish(&line) != 0ULL)
        {
            walk->depth = 2u;
            EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = walk->folder,
                       .each = query_cl_walk, .context_bytess = walk);
            walk->depth = 1u;
        }
        walk->folder[existing] = '\0';
    }
}

// MSVC's host compiler as nvcc wants it on Windows: the newest `cl` under Visual Studio's MSVC tools, the Build
// Tools' first, written into `path`: 1, or 0 where none is installed
EMBED_INLINE int query_cl_install(const ProgramsCtx *args)
{
    static const char *const s_bases[2] = {
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC",
        "C:\\Program Files\\Microsoft Visual Studio"};
    static QueryClWalk s_walk;
    s_walk.found[0] = '\0';
    for (unsigned int base = 0u; base < 2u; base += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_walk.folder,
                          .src = s_bases[base],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_bases[base], .cap = CLI_POOL_BYTES}) + 1u});
        s_walk.depth = 0u;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = s_walk.folder,
                   .each = query_cl_walk, .context_bytess = &s_walk);
    }
    if (s_walk.found[0] == '\0')
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->path,
                            .src = s_walk.found,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_walk.found, .cap = CLI_POOL_BYTES}) + 1u});
    return args->path[0] != '\0';
}

static int query_on_path_program(const QueryEachRolePathCapacityErrMsgCfg *args)
{
    QueryRole role = args->role;
    char *path = args->path;
    size_t capacity = args->capacity;
    static const char *const s_c[3] = {"cc", "gcc", "clang"};
    static const char *const s_cxx[3] = {"c++", "g++", "clang++"};
#if defined(_WIN32)
    static const char *const s_python[2] = {"python", "python3"};
#else
    static const char *const s_python[2] = {"python3", "python"};
#endif
    static const char *const s_alone[QUERY_ROLE_COUNT] = {NULL, NULL,         "nvcc",   NULL,   NULL, "cl",
                                                          NULL, "nvidia-smi", "docker", "curl", "tar"};
    int found = 0;
    if (role == QUERY_ROLE_C)
    {
        found =
            EMBED_CALL(query_on_path_find, ProgramsCtx, .names = s_c, .count = 3u, .path = path, .capacity = capacity);
    }
    else if (role == QUERY_ROLE_CXX)
    {
        found = EMBED_CALL(query_on_path_find, ProgramsCtx, .names = s_cxx, .count = 3u, .path = path,
                           .capacity = capacity);
    }
    else if (role == QUERY_ROLE_PYTHON)
    {
        found = EMBED_CALL(query_on_path_find, ProgramsCtx, .names = s_python, .count = 2u, .path = path,
                           .capacity = capacity);
    }
    else if ((role == QUERY_ROLE_NVDISASM) || (role == QUERY_ROLE_CUOBJDUMP))
    {
        // a program beside nvcc, in its folder
        char nvcc[QUERY_PATH_MAX_LEN];
        const char *const nvcc_name = "nvcc";
        if (EMBED_CALL(query_on_path_find, ProgramsCtx, .names = &nvcc_name, .count = 1u, .path = nvcc,
                       .capacity = sizeof(nvcc)))
        {
            const char *const slash =
                EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = nvcc,
                           .length_value = cellul.len(&(CatenaFinitaCfg){.src = nvcc, .cap = CLI_POOL_BYTES}),
                           .byte = QUERY_FOLDER_SEPARATOR);
            const unsigned long long folder = (slash != NULL) ? (unsigned long long)(slash - nvcc) : 0ULL;
            found =
                (folder != 0ULL) && EMBED_CALL(query_in_folder, ProgramsCtx, .folder = nvcc, .folder_length = folder,
                                               .name = (role == QUERY_ROLE_NVDISASM) ? "nvdisasm" : "cuobjdump",
                                               .path = path, .capacity = capacity);
        }
    }
    else if ((unsigned int)role < (unsigned int)QUERY_ROLE_COUNT)
    {
        found = EMBED_CALL(query_on_path_find, ProgramsCtx, .names = &s_alone[role], .count = 1u, .path = path,
                           .capacity = capacity);
        found = found || ((role == QUERY_ROLE_CL) &&
                          EMBED_CALL(query_cl_install, ProgramsCtx, .path = path, .capacity = capacity));
    }
    if (!found)
    {
        mmgr_line_put(args->err_msg, EMBED_CALL(query_table.role_name, QueryCfg, .role = role));
        mmgr_line_put(args->err_msg, " is not found on the PATH");
    }
    return found;
}

QueryPrograms query_on_path(void)
{
    const QueryPrograms programs = {query_on_path_program, query_run_path_read, NULL};
    return programs;
}
