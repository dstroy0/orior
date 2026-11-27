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

unsigned int cli_kernel_folder_append(const CliJupyterSystemCtx *args)
{
    return EMBED_CALL(jupyter_folder_append, JupyterFoldersCtx, .folders = args->folders, .count = args->count,
                      .folder = args->folder, .name = args->name);
}

// the path list's entry from `name` up to `end` into the listing's next folder, where one is left: the count after
EMBED_INLINE unsigned int jupyter_entry_append(const JupyterFoldersCtx *args)
{
    if (args->count >= CLI_KERNEL_FOLDERS)
    {
        return args->count;
    }
    VerbaLinea line = {args->folders[args->count], CLI_PATH_MAX_LEN, 0ULL};
    // each run of the entry up to a quote, where the system takes quotes out of a path list's entries
    for (const char *from = args->name; from < args->end;)
    {
        const char *const quote =
            g_cli_system_facts.list_quoted
                ? memor.chr(&(MemoriaCfg){.src = from, .val = '"', .bytes = (embed_word)(args->end - from)})
                : NULL;
        const char *const stop = (quote != NULL) ? quote : args->end;
        mmgr_line_put_n(&line, from, (unsigned long long)(stop - from));
        from = (quote != NULL) ? (quote + 1) : args->end;
    }
    mmgr_line_finish(&line);
    return args->count + 1u;
}

// the prefix a Python's program is in into `prefix`: its own folder on Windows, or the folder above its bin or
// Scripts folder. 1, or 0 where it has none
EMBED_INLINE int jupyter_python_prefix(const JupyterFoldersCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->python, .out = folder) ||
        !EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = folder, .out = above))
    {
        return 0;
    }
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES});
    const embed_word start = (embed_word)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                    .text = folder, .at = length, .set = "/\\");
    // the folder's own name is bin or Scripts, in either case
    const char *const name = folder + start;
    const int up = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "bin", .cap = CLI_POOL_BYTES, .ci = 1}) ||
                   cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "scripts", .cap = CLI_POOL_BYTES, .ci = 1});
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
         (!cellul.eq(&(CatenaFinitaCfg){.src = args->pythons[0], .other = python, .cap = CLI_POOL_BYTES}))))
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
        const char between[2] = {g_cli_system_facts.list_separator, '\0'};
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_paths, .cap = CLI_POOL_BYTES});
        for (unsigned long long from = 0ULL; from <= size;)
        {
            // each entry of the path list, to the next separator or to the list's end
            const unsigned long long at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                     .text = s_paths, .at = from, .size = size, .set = between);
            count = EMBED_CALL(jupyter_entry_append, JupyterFoldersCtx, .folders = folders, .count = count,
                               .name = s_paths + from, .end = s_paths + at);
            from = at + 1ULL;
        }
    }
    char under[CLI_PATH_MAX_LEN];
    count = EMBED_CALL(cli_jupyter_system.reader_folders, CliJupyterSystemCtx, .folders = folders, .count = count);
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
    return EMBED_CALL(cli_jupyter_system.shared_folders, CliJupyterSystemCtx, .folders = folders, .count = count);
}
