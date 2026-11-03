// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_folders.c: where Jupyter's kernels are looked for: the Pythons a notebook can run on, whether each contains
// ipykernel, and the folders Jupyter stores its data in, in the order it reads them
#include "../command_line/callback_cfg.h"
#include "../programs/executables.h"
#include "../text/text.h"
#include "jupyter_internal.h"

typedef struct
{
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    const char *folder;
    const char *name;
    const char *python;
    char *prefix;
    const char *end;
} JupyterFoldersCtx;

// `name` joined under `folder` into the listing's next folder, where one is left: the count after
EMBED_INLINE unsigned int jupyter_folder_append(const JupyterFoldersCtx *args)
{
    if (args->count >= CLI_KERNEL_FOLDERS)
    {
        return args->count;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = args->name,
               .joined = args->folders[args->count]);
    return args->count + 1u;
}

// the path list's entry from `name` up to `end` into the listing's next folder, where one is left: the count after
EMBED_INLINE unsigned int jupyter_entry_append(const JupyterFoldersCtx *args)
{
    if (args->count >= CLI_KERNEL_FOLDERS)
    {
        return args->count;
    }
    VerbaLinea line = {args->folders[args->count], CLI_PATH_MAX_LEN, 0ULL};
    for (const char *letter = args->name; letter < args->end; letter += 1)
    {
#if defined(_WIN32)
        // Windows takes quotes out of a path list's entries
        if (*letter == '"')
        {
            continue;
        }
#endif
        mmgr_line_ch(&line, *letter);
    }
    mmgr_line_finish(&line);
    return args->count + 1u;
}

// the prefix a Python's program stands in into `prefix`: its own folder on Windows, or the folder above its bin or
// Scripts folder. 1, or 0 where it has none
EMBED_INLINE int jupyter_python_prefix(const JupyterFoldersCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->python, .out = folder) ||
        !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = folder, .out = above))
    {
        return 0;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    size_t start = length;
    start = (size_t)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = folder, .at = start,
                               .set = "/\\");
    char name[16];
    const size_t size = length - start;
    for (size_t at = 0u; (at < size) && (at < sizeof(name) - 1u); at += 1u)
    {
        const char letter = folder[start + at];
        name[at] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
    }
    name[(size < sizeof(name) - 1u) ? size : sizeof(name) - 1u] = '\0';
    const int up = (size < sizeof(name) - 1u) &&
                   ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "bin", .cap = CLI_CELLS_POOL})) ||
                    (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "scripts", .cap = CLI_CELLS_POOL})));
    memor.cpy(&(MemoriaCfg){.dst = args->prefix, .src = up ? above : folder, .bytes = CLI_PATH_MAX_LEN});
    return 1;
}

// whether a folder of a Python's lib contains site-packages/ipykernel
typedef struct
{
    const char *lib;
    int has_kernel;
} CliKernelLib;

static void jupyter_lib_entry(const CliEachNameIsFolderCfg *args)
{
    CliKernelLib *const lib = (CliKernelLib *)args->context;
    char entry[CLI_PATH_MAX_LEN];
    char packages[CLI_PATH_MAX_LEN];
    char kernel[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = lib->lib, .relative = args->name, .joined = entry);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = entry, .relative = "site-packages", .joined = packages);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = packages, .relative = "ipykernel", .joined = kernel);
    lib->has_kernel =
        lib->has_kernel || EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = kernel);
}

int cli_kernel_has_ipykernel(const CliJupyterCtx *args)
{
    char prefix[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(jupyter_python_prefix, JupyterFoldersCtx, .python = args->python, .prefix = prefix))
    {
        return 0;
    }
    char lib[CLI_PATH_MAX_LEN];
    char packages[CLI_PATH_MAX_LEN];
    char kernel[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = prefix, .relative = "Lib", .joined = lib);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = lib, .relative = "site-packages", .joined = packages);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = packages, .relative = "ipykernel", .joined = kernel);
    if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = kernel))
    {
        return 1;
    }
    CliKernelLib found = {lib, 0};
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = prefix, .relative = "lib", .joined = lib);
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = lib,
                     .each = jupyter_lib_entry, .context_bytess = &found);
    return found.has_kernel;
}

unsigned int cli_kernel_pythons(const CliJupyterPythonsCtx *args)
{
    int *has_environment = args->has_environment;
    static CliTreeEnvironment s_environment;
    unsigned int count = 0u;
    *has_environment =
        EMBED_CALL(cli_toolchains_table.environment, CliToolchainsCfg, .environment_out = &s_environment);
    if (*has_environment && s_environment.has_python)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->pythons[count], .src = s_environment.python, .bytes = CLI_PATH_MAX_LEN});
        count += 1u;
    }
    char python[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "python", .name = "python",
                   .path = python, .err_msg = &err_msg) &&
        ((count == 0u) ||
         (!cellul.eq(&(CatenaFinitaCfg){.src = args->pythons[0], .other = python, .cap = CLI_CELLS_POOL}))))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->pythons[count], .src = python, .bytes = CLI_PATH_MAX_LEN});
        count += 1u;
    }
    return count;
}

unsigned int cli_kernel_data_folders(const CliJupyterPythonsPythonCountCtx *args)
{
    char (*folders)[CLI_PATH_MAX_LEN] = args->folders;
    unsigned int count = 0u;
    static char s_paths[CLI_PATH_TEXT];
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "JUPYTER_PATH",
                   .value_out = s_paths, .capacity = sizeof(s_paths)))
    {
#if defined(_WIN32)
        const char between = ';';
#else
        const char between = ':';
#endif
        const char *from = s_paths;
        for (const char *at = s_paths;; at += 1)
        {
            if ((*at != between) && (*at != '\0'))
            {
                continue;
            }
            count = EMBED_CALL(jupyter_entry_append, JupyterFoldersCtx, .folders = folders, .count = count,
                               .name = from, .end = at);
            if (*at == '\0')
            {
                break;
            }
            from = at + 1;
        }
    }
    char home[CLI_PATH_MAX_LEN];
    char under[CLI_PATH_MAX_LEN];
#if defined(_WIN32)
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "APPDATA", .value_out = home,
                   .capacity = sizeof(home)))
    {
        count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count, .folder = home,
                           .name = "jupyter");
    }
#elif defined(__APPLE__)
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = home,
                   .capacity = sizeof(home)))
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "Library", .joined = under);
        count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count,
                           .folder = under, .name = "Jupyter");
    }
#else
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "XDG_DATA_HOME",
                   .value_out = home, .capacity = sizeof(home)))
    {
        count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count, .folder = home,
                           .name = "jupyter");
    }
    else if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME",
                        .value_out = home, .capacity = sizeof(home)))
    {
        char local[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = ".local", .joined = local);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = local, .relative = "share", .joined = under);
        count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count,
                           .folder = under, .name = "jupyter");
    }
#endif
    for (unsigned int python = 0u; python < args->python_count; python += 1u)
    {
        char prefix[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(jupyter_python_prefix, JupyterFoldersCtx, .python = args->pythons_callback[python],
                       .prefix = prefix))
        {
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = prefix, .relative = "share", .joined = under);
            count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count,
                               .folder = under, .name = "jupyter");
        }
    }
#if defined(_WIN32)
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PROGRAMDATA",
                   .value_out = home, .capacity = sizeof(home)))
    {
        count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count, .folder = home,
                           .name = "jupyter");
    }
#else
    count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count,
                       .folder = "/usr/local/share", .name = "jupyter");
    count = EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = folders, .count = count,
                       .folder = "/usr/share", .name = "jupyter");
#endif
    return count;
}
