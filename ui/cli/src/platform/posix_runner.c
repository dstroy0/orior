// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_runner.c: the bash that runs the tree's scripts on Linux and macOS: the bash on the PATH
#include "../runs/runner.h"

#if !defined(_WIN32)

int cli_runner_system_bash(const CliRunnerNameCfg *args)
{
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, "bash");
    return mmgr_line_finish(&line) != 0ULL;
}
#endif
