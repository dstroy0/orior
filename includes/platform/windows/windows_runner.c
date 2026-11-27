// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_runner.c: the bash that runs the tree's scripts on Windows: Git's, found beside git itself, because the bash
// System32 offers is WSL's and reads none of these paths
#include "../../../ui/cli/src/runs/runner.h"
#include "../../../ui/cli/src/text/text.h"
#include "../../../ui/cli/src/toolchains/toolchains.h"

#if defined(_WIN32)

typedef struct
{
    const char *under;
    const char *name;
    char *path;
} WindowsRunnerCtx;

// a file in a folder joined, `under` and `name`, where it is a file: 1, its path into `path`
EMBED_INLINE int runner_found_in(const WindowsRunnerCtx *args)
{
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->under);
    mmgr_line_put(&line, args->name);
    return (mmgr_line_finish(&line) != 0ULL) &&
           EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->path);
}

int cli_runner_system_bash(const CliRunnerNameCfg *args)
{
    // the folders of the PATH every step runs with, each read off the cell the process holds them in
    const CliToolchainsFolders folders = cli_toolchains_run_folders();
    for (unsigned int at = 0u; at < folders.count; at += 1u)
    {
        char folder[CLI_PATH_MAX_LEN];
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = folders.cell,
                                                                     .offset = (embed_word)at * CLI_PATH_MAX_LEN,
                                                                     .dst = folder,
                                                                     .bytes = CLI_PATH_MAX_LEN});
        char git[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(runner_found_in, WindowsRunnerCtx, .under = folder, .name = "\\git.exe", .path = git))
        {
            continue;
        }
        // the folder above the one git is in: the folder cut at its last slash of either type, found a word at a time
        const unsigned long long past_slash =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = folder,
                       .at = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_PATH_MAX_LEN}), .set = "\\/");
        if (past_slash == 0ULL)
        {
            continue;
        }
        folder[past_slash - 1ULL] = '\0';
        if (EMBED_CALL(runner_found_in, WindowsRunnerCtx, .under = folder, .name = "\\bin\\bash.exe",
                       .path = args->path) ||
            EMBED_CALL(runner_found_in, WindowsRunnerCtx, .under = folder, .name = "\\usr\\bin\\bash.exe",
                       .path = args->path))
        {
            return 1;
        }
    }
    static const char *const s_bases[3] = {"ProgramFiles", "ProgramW6432", "LOCALAPPDATA"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char base[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = s_bases[at],
                       .value_out = base, .capacity = sizeof(base)) &&
            (EMBED_CALL(runner_found_in, WindowsRunnerCtx, .under = base, .name = "\\Git\\bin\\bash.exe",
                        .path = args->path) ||
             EMBED_CALL(runner_found_in, WindowsRunnerCtx, .under = base, .name = "\\Programs\\Git\\bin\\bash.exe",
                        .path = args->path)))
        {
            return 1;
        }
    }
    mmgr_line_put(args->err_msg, "no Git bash found: install Git for Windows, or name a bash in ORIOR_BASH");
    return 0;
}
#endif
