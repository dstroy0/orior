// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_executables.c: what Linux and macOS answer of a build: Meson needs no Visual Studio here, .NET installs where
// /etc/dotnet/install_location names and in the usual folders, and an exit code says nothing of a start
#include "../../../ui/cli/src/programs/executables_internal.h"

#if !defined(_WIN32)

int cli_executables_vsenv(void)
{
    return 0;
}

// the folder /etc/dotnet/install_location names, then the usual folders
unsigned int cli_executables_dotnet_installed(const CliExecutablesSystemPositionsCtx *args)
{
    unsigned int count = 0u;
    char text[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_executables_capacity_internal.text_read, CliExecutablesCapacityCtx,
                   .path = "/etc/dotnet/install_location", .bytes = text, .capacity = sizeof(text)))
    {
        const char *from = text;
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES});
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = from,
                   .length = length, .out = args->positions[count], .capacity = CLI_PATH_MAX_LEN);
        count += 1u;
    }
    static const char *const s_usual[3] = {"/usr/share/dotnet", "/usr/lib/dotnet", "/usr/local/share/dotnet"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->positions[count],
                          .src = s_usual[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_usual[at], .cap = CLI_POOL_BYTES}) + 1u});
        count += 1u;
    }
    return count;
}

const char *cli_executables_unstarted(const CliExecutablesRootCfg *args)
{
    (void)args;
    return NULL;
}
#endif
