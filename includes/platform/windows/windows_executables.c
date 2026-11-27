// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_executables.c: what Windows answers of a build: whether Meson sets Visual Studio's environment up, where
// .NET installs for every user, and why a program a build made did not start
#include "../../../ui/cli/src/programs/executables_internal.h"
#include "../../../ui/cli/src/toolchains/toolchains.h"

#if defined(_WIN32)

// Where MSVC's cl is on the PATH and nothing set the environment it compiles in, Meson takes cl and cannot compile.
// Meson sets the environment up only where no cl is on the PATH, and its setup and compile run without cl's folders
int cli_executables_vsenv(void)
{
    char include[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "INCLUDE",
                   .value_out = include, .capacity = sizeof(include)))
    {
        return 0;
    }
    static const char *const s_cl[1] = {"cl"};
    char path[CLI_PATH_MAX_LEN];
    return EMBED_CALL(cli_toolchains_names.on_path, CliToolchainsNamesCfg, .names = s_cl, .count = 1u, .program = path);
}

// the dotnet folder under ProgramFiles
unsigned int cli_executables_dotnet_installed(const CliExecutablesSystemPositionsCtx *args)
{
    char files[CLI_PATH_MAX_LEN];
    return (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ProgramFiles",
                       .value_out = files, .capacity = sizeof(files)) &&
            EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = files,
                       .name = "dotnet", .out = args->positions[0]))
               ? 1u
               : 0u;
}

const char *cli_executables_unstarted(const CliExecutablesRootCfg *args)
{
    // the exit code read as the unsigned number Windows writes it as
    const unsigned int status = args->has_code ? (unsigned int)args->code : 0u;
    if (status == 0xC0000135u)
    {
        return "a DLL it loads was found nowhere (0xC0000135)";
    }
    if (status == 0xC000007Bu)
    {
        return "a DLL it loads is built for another processor, or is damaged (0xC000007B)";
    }
    if (status == 0xC0000139u)
    {
        return "a DLL it loads lacks a function it calls, another version than the one it was built with (0xC0000139)";
    }
    return NULL;
}
#endif
