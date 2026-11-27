// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_processes.c: whether a process runs on the POSIX systems, as /proc lists it where the system has /proc. The
// list of the machine's processes, a process's memory and its program's name are the system's
// (linux/linux_processes.c, macos/macos_processes.c)
#include "../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../ui/cli/src/text/text.h"

#if !defined(_WIN32)
int cli_process_running(const CliPidCtx *args)
{
    if (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = "/proc"))
    {
        return 1;
    }
    char path[64];
    VerbaLinea named = {path, sizeof(path), 0ULL};
    mmgr_line_put(&named, "/proc/");
    mmgr_line_decimal(&named, args->pid, 1u);
    (void)mmgr_line_finish(&named);
    return EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path);
}
#endif
