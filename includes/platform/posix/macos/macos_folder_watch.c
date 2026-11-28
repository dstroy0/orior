// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_folder_watch.c: the watch of a folder on macOS (../../folder_watch.h). The watch reads the folder every
// CLI_FOLDER_WATCH_READ_MS and calls where the stamp of its files changed, and the wake of the watch carries the stop
#include "../../folder_watch.h"

#if defined(__APPLE__)

void folder_watch_folder_put(CliFolderWatch *watch, const char *path)
{
    // a reading of the folder covers each folder under it
    (void)watch;
    (void)path;
}

// The folder read every CLI_FOLDER_WATCH_READ_MS, and the call where the stamp of its files changed since the last
// reading. The stop ends the thread
void folder_watch_run(void *context)
{
    CliFolderWatch *const watch = (CliFolderWatch *)context;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &watch->wake);
        if (watch->stop)
        {
            break;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_seen_for, CliWakeCtx, .wake = &watch->wake, .seen = seen,
                         .milliseconds_u64 = CLI_FOLDER_WATCH_READ_MS);
        if (watch->stop)
        {
            break;
        }
        const unsigned long long stamp = folder_watch_walked(watch);
        if (stamp != watch->stamp)
        {
            watch->stamp = stamp;
            watch->changed();
        }
    }
    folder_watch_ended(watch);
}

// the first stamp of the folder of `watch`: 1
int folder_watch_opened(CliFolderWatch *watch)
{
    watch->stamp = folder_watch_walked(watch);
    return 1;
}

void folder_watch_stop_sent(CliFolderWatch *watch)
{
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &watch->wake);
}

void folder_watch_closed(CliFolderWatch *watch)
{
    (void)watch;
}
#endif
