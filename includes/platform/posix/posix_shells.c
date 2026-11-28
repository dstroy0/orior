// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_shells.c: the reader's shell on Linux and macOS: the shell SHELL names, else bash, as a login shell
#include "../../../ui/cli/src/terminal/shells.h"

#if !defined(_WIN32)

void cli_terminal_shell_system(const CliTerminalShellSystemCtx *args)
{
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "SHELL",
                    .value_out = args->program, .capacity = CLI_PATH_MAX_LEN) ||
        (args->program[0] == '\0'))
    {
        args->words[0] = "bash";
    }
    args->words[1] = "-l";
}
#endif
