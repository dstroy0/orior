// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_folders.c: where Jupyter's kernels are looked for: the Pythons a notebook can run on, whether each holds
// ipykernel, and the folders Jupyter keeps its data in, in the order it reads them
#include "../programs/executables.h"
#include "jupyter_internal.h"

// `name` joined under `folder` into the listing's next folder, where one is left: the count after
static unsigned int jupyter_folder_put(char (*folders)[CLI_PATH_LONGEST], unsigned int count, const char *folder,
                                       const char *name)
{
    if (count >= CLI_KERNEL_FOLDERS)
    {
        return count;
    }
    cli_full(folder, name, folders[count]);
    return count + 1u;
}

// the prefix a Python's program stands in into `prefix`: its own folder on Windows, or the folder above its bin or
// Scripts folder. 1, or 0 where it has none
static int jupyter_python_prefix(const char *python, char *prefix)
{
    char folder[CLI_PATH_LONGEST];
    char above[CLI_PATH_LONGEST];
    if (!cli_path_parent(python, folder) || !cli_path_parent(folder, above))
    {
        return 0;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    size_t start = length;
    while ((start > 0u) && (folder[start - 1u] != '/') && (folder[start - 1u] != '\\'))
    {
        start -= 1u;
    }
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
    memor.cpy(&(MemoriaCfg){.dst = prefix, .src = up ? above : folder, .bytes = CLI_PATH_LONGEST});
    return 1;
}

// whether a folder of a Python's lib holds site-packages/ipykernel
typedef struct
{
    const char *lib;
    int holds;
} CliKernelLib;

static void jupyter_lib_entry(void *context, const char *name, int is_folder)
{
    (void)is_folder;
    CliKernelLib *const lib = (CliKernelLib *)context;
    char entry[CLI_PATH_LONGEST];
    char packages[CLI_PATH_LONGEST];
    char kernel[CLI_PATH_LONGEST];
    cli_full(lib->lib, name, entry);
    cli_full(entry, "site-packages", packages);
    cli_full(packages, "ipykernel", kernel);
    lib->holds = lib->holds || query_is_folder(kernel);
}

int cli_kernel_holds_ipykernel(const char *python)
{
    char prefix[CLI_PATH_LONGEST];
    if (!jupyter_python_prefix(python, prefix))
    {
        return 0;
    }
    char lib[CLI_PATH_LONGEST];
    char packages[CLI_PATH_LONGEST];
    char kernel[CLI_PATH_LONGEST];
    cli_full(prefix, "Lib", lib);
    cli_full(lib, "site-packages", packages);
    cli_full(packages, "ipykernel", kernel);
    if (query_is_folder(kernel))
    {
        return 1;
    }
    CliKernelLib found = {lib, 0};
    cli_full(prefix, "lib", lib);
    (void)query_folder_each(lib, jupyter_lib_entry, &found);
    return found.holds;
}

unsigned int cli_kernel_pythons(char (*pythons)[CLI_PATH_LONGEST], int *has_environment)
{
    static CliTreeEnvironment s_environment;
    unsigned int count = 0u;
    *has_environment = cli_toolchains_environment(&s_environment);
    if (*has_environment && s_environment.has_python)
    {
        memor.cpy(&(MemoriaCfg){.dst = pythons[count], .src = s_environment.python, .bytes = CLI_PATH_LONGEST});
        count += 1u;
    }
    char python[CLI_PATH_LONGEST];
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    if (cli_executables_program("python", "python", python, &said) &&
        ((count == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = pythons[0], .other = python, .cap = CLI_CELLS_POOL}))))
    {
        memor.cpy(&(MemoriaCfg){.dst = pythons[count], .src = python, .bytes = CLI_PATH_LONGEST});
        count += 1u;
    }
    return count;
}

unsigned int cli_kernel_data_folders(const char (*pythons)[CLI_PATH_LONGEST], unsigned int python_count,
                                     char (*folders)[CLI_PATH_LONGEST])
{
    unsigned int count = 0u;
    static char s_paths[CLI_PATH_TEXT];
    if (query_environment_read("JUPYTER_PATH", s_paths, sizeof(s_paths)))
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
            if (count < CLI_KERNEL_FOLDERS)
            {
                CliLine line = {folders[count], CLI_PATH_LONGEST, 0ull};
                for (const char *letter = from; letter < at; letter += 1)
                {
#if defined(_WIN32)
                    // Windows takes quotes out of a path list's entries
                    if (*letter == '"')
                    {
                        continue;
                    }
#endif
                    cli_line_character(&line, *letter);
                }
                cli_line_closed(&line);
                count += 1u;
            }
            if (*at == '\0')
            {
                break;
            }
            from = at + 1;
        }
    }
    char home[CLI_PATH_LONGEST];
    char under[CLI_PATH_LONGEST];
#if defined(_WIN32)
    if (query_environment_read("APPDATA", home, sizeof(home)))
    {
        count = jupyter_folder_put(folders, count, home, "jupyter");
    }
#elif defined(__APPLE__)
    if (query_environment_read("HOME", home, sizeof(home)))
    {
        cli_full(home, "Library", under);
        count = jupyter_folder_put(folders, count, under, "Jupyter");
    }
#else
    if (query_environment_read("XDG_DATA_HOME", home, sizeof(home)))
    {
        count = jupyter_folder_put(folders, count, home, "jupyter");
    }
    else if (query_environment_read("HOME", home, sizeof(home)))
    {
        char local[CLI_PATH_LONGEST];
        cli_full(home, ".local", local);
        cli_full(local, "share", under);
        count = jupyter_folder_put(folders, count, under, "jupyter");
    }
#endif
    for (unsigned int python = 0u; python < python_count; python += 1u)
    {
        char prefix[CLI_PATH_LONGEST];
        if (jupyter_python_prefix(pythons[python], prefix))
        {
            cli_full(prefix, "share", under);
            count = jupyter_folder_put(folders, count, under, "jupyter");
        }
    }
#if defined(_WIN32)
    if (query_environment_read("PROGRAMDATA", home, sizeof(home)))
    {
        count = jupyter_folder_put(folders, count, home, "jupyter");
    }
#else
    count = jupyter_folder_put(folders, count, "/usr/local/share", "jupyter");
    count = jupyter_folder_put(folders, count, "/usr/share", "jupyter");
#endif
    return count;
}
