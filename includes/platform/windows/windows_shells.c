// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_shells.c: the reader's shell on Windows: Git's bash as a login shell where it is found, else the shell
// ComSpec names, else cmd.exe
#include "../../../ui/cli/src/runs/runner.h"
#include "../../../ui/cli/src/terminal/shells.h"

#if defined(_WIN32)

void cli_terminal_shell_system(const CliTerminalShellSystemCtx *args)
{
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = args->program, .err_msg = &unread))
    {
        // Git's bash starts a login shell in its home unless told to stay in the folder it starts in
        args->words[1] = "--login";
        args->words[2] = "-i";
        args->settings[*args->setting_count] = (PlatformSetting){"CHERE_INVOKING", "1"};
        *args->setting_count += 1u;
        return;
    }
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ComSpec",
                    .value_out = args->program, .capacity = CLI_PATH_MAX_LEN) ||
        (args->program[0] == '\0'))
    {
        args->words[0] = "cmd.exe";
    }
}
#endif
