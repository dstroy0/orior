// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// programs.c: the programs the query needs, named by role and never by path: each role's name as a person reads it,
// and the programs as the PATH alone gives them, which the command line falls back to where no toolchain is chosen
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

#if defined(_WIN32)
#define QUERY_PATH_SEPARATOR ';'
#define QUERY_FOLDER_SEPARATOR '\\'
#else
#define QUERY_PATH_SEPARATOR ':'
#define QUERY_FOLDER_SEPARATOR '/'
#endif

// the longest PATH the query reads
#define QUERY_RUN_PATH_LONGEST 32768u

const char *query_role_name(QueryRole role)
{
    static const char *const s_names[QUERY_ROLE_COUNT] = {
        "the C compiler", "the C++ compiler", "nvcc",   "nvdisasm", "cuobjdump", "cl",
        "python",         "nvidia-smi",       "docker", "curl",     "tar"};
    return ((unsigned int)role < (unsigned int)QUERY_ROLE_COUNT) ? s_names[role] : "a program";
}

// `name` as a program in `folder`, with `.exe` on Windows, where it is there, written into `path`: 1, or 0
static int query_in_folder(const char *folder, unsigned long long folder_length, const char *name, char *path,
                           size_t room)
{
    CliLine line = {path, room, 0ull};
    cli_line_counted(&line, folder, folder_length);
    cli_line_character(&line, QUERY_FOLDER_SEPARATOR);
    cli_line_text(&line, name);
    if ((cli_line_closed(&line) != 0ull) && query_is_file(path))
    {
        return 1;
    }
#if defined(_WIN32)
    line.at = 0ull;
    cli_line_counted(&line, folder, folder_length);
    cli_line_character(&line, QUERY_FOLDER_SEPARATOR);
    cli_line_text(&line, name);
    cli_line_text(&line, ".exe");
    return (cli_line_closed(&line) != 0ull) && query_is_file(path);
#else
    return 0;
#endif
}

static const char *query_run_path_read(void *context)
{
    (void)context;
    static char s_path[QUERY_ENVIRONMENT_LONGEST];
    if (!query_environment_read("PATH", s_path, sizeof(s_path)))
    {
        s_path[0] = '\0';
    }
    return s_path;
}

// the first of `names`, `count` of them, found as a program in a folder of the PATH, written into `path`: 1, or 0
static int query_on_path_found(const char *const *names, unsigned int count, char *path, size_t room)
{
    const char *const run_path = query_run_path_read(NULL);
    const char *folder = run_path;
    while (*folder != '\0')
    {
        const char *const end =
            cellul.chr(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL, .byte = QUERY_PATH_SEPARATOR});
        const unsigned long long length = (end != NULL)
                                              ? (unsigned long long)(end - folder)
                                              : cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
        for (unsigned int at = 0u; (length != 0ull) && (at < count); at += 1u)
        {
            if (query_in_folder(folder, length, names[at], path, room))
            {
                return 1;
            }
        }
        folder += length + ((end != NULL) ? 1u : 0u);
    }
    return 0;
}

// what the walk for MSVC's host compiler keeps: the folder it stands in, a level's names, and the newest cl found
typedef struct
{
    char folder[QUERY_PATH_LONGEST];
    char found[QUERY_PATH_LONGEST];
    unsigned int depth;
} QueryClWalk;

static void query_cl_walked(void *context, const char *name, int is_folder);

// the folder `walk` stands in joined with `name`, walked one level down at `depth`, and stood back from
static void query_cl_entered(QueryClWalk *walk, const char *name, unsigned int depth)
{
    const size_t standing = cellul.len(&(CatenaFinitaCfg){.src = walk->folder, .cap = CLI_CELLS_POOL});
    CliLine line = {walk->folder, sizeof(walk->folder), standing};
    cli_line_character(&line, QUERY_FOLDER_SEPARATOR);
    cli_line_text(&line, name);
    if (cli_line_closed(&line) != 0ull)
    {
        const unsigned int held = walk->depth;
        walk->depth = depth;
        query_folder_each(walk->folder, query_cl_walked, walk);
        walk->depth = held;
    }
    walk->folder[standing] = '\0';
}

// The walk under a base: depth 0 a year or a version, the version holding `bin\Hostx64\x64\cl.exe` directly or the
// year an edition holding `VC\Tools\MSVC\<version>`; the found kept where its path sorts last
static void query_cl_walked(void *context, const char *name, int is_folder)
{
    QueryClWalk *const walk = (QueryClWalk *)context;
    if (!is_folder)
    {
        return;
    }
    if ((walk->depth == 0u) || (walk->depth == 2u))
    {
        char cl[QUERY_PATH_LONGEST];
        CliLine line = {cl, sizeof(cl), 0ull};
        cli_line_text(&line, walk->folder);
        cli_line_character(&line, QUERY_FOLDER_SEPARATOR);
        cli_line_text(&line, name);
        cli_line_text(&line, "\\bin\\Hostx64\\x64\\cl.exe");
        if ((cli_line_closed(&line) != 0ull) && query_is_file(cl) && (cli_text_order(cl, walk->found) > 0))
        {
            memor.cpy(&(MemoriaCfg){.dst = walk->found,
                                    .src = cl,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    if (walk->depth == 0u)
    {
        query_cl_entered(walk, name, 1u);
    }
    else if (walk->depth == 1u)
    {
        const size_t standing = cellul.len(&(CatenaFinitaCfg){.src = walk->folder, .cap = CLI_CELLS_POOL});
        CliLine line = {walk->folder, sizeof(walk->folder), standing};
        cli_line_character(&line, QUERY_FOLDER_SEPARATOR);
        cli_line_text(&line, name);
        cli_line_text(&line, "\\VC\\Tools\\MSVC");
        if (cli_line_closed(&line) != 0ull)
        {
            walk->depth = 2u;
            query_folder_each(walk->folder, query_cl_walked, walk);
            walk->depth = 1u;
        }
        walk->folder[standing] = '\0';
    }
}

// MSVC's host compiler as nvcc wants it on Windows: the newest `cl` under Visual Studio's MSVC tools, the Build
// Tools' first, written into `path`: 1, or 0 where none is installed
static int query_cl_installed(char *path, size_t room)
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
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_bases[base], .cap = CLI_CELLS_POOL}) + 1u});
        s_walk.depth = 0u;
        query_folder_each(s_walk.folder, query_cl_walked, &s_walk);
    }
    if (s_walk.found[0] == '\0')
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = path,
                            .src = s_walk.found,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_walk.found, .cap = CLI_CELLS_POOL}) + 1u});
    return path[0] != '\0';
}

static int query_on_path_program(void *context, QueryRole role, char *path, size_t room, CliLine *said)
{
    (void)context;
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
        found = query_on_path_found(s_c, 3u, path, room);
    }
    else if (role == QUERY_ROLE_CXX)
    {
        found = query_on_path_found(s_cxx, 3u, path, room);
    }
    else if (role == QUERY_ROLE_PYTHON)
    {
        found = query_on_path_found(s_python, 2u, path, room);
    }
    else if ((role == QUERY_ROLE_NVDISASM) || (role == QUERY_ROLE_CUOBJDUMP))
    {
        // a program beside nvcc, in its folder
        char nvcc[QUERY_PATH_LONGEST];
        const char *const nvcc_name = "nvcc";
        if (query_on_path_found(&nvcc_name, 1u, nvcc, sizeof(nvcc)))
        {
            const char *const slash = cli_text_last(
                nvcc, cellul.len(&(CatenaFinitaCfg){.src = nvcc, .cap = CLI_CELLS_POOL}), QUERY_FOLDER_SEPARATOR);
            const unsigned long long folder = (slash != NULL) ? (unsigned long long)(slash - nvcc) : 0ull;
            found = (folder != 0ull) &&
                    query_in_folder(nvcc, folder, (role == QUERY_ROLE_NVDISASM) ? "nvdisasm" : "cuobjdump", path, room);
        }
    }
    else if ((unsigned int)role < (unsigned int)QUERY_ROLE_COUNT)
    {
        found = query_on_path_found(&s_alone[role], 1u, path, room);
        found = found || ((role == QUERY_ROLE_CL) && query_cl_installed(path, room));
    }
    if (!found)
    {
        cli_line_text(said, query_role_name(role));
        cli_line_text(said, " is not found on the PATH");
    }
    return found;
}

QueryPrograms query_on_path(void)
{
    const QueryPrograms programs = {query_on_path_program, query_run_path_read, NULL};
    return programs;
}
