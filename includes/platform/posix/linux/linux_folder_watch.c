// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_folder_watch.c: the watch of a folder on Linux (../../folder_watch.h). An inotify watches the folder and each
// folder under it, and an eventfd carries the stop
#include "../../folder_watch.h"

#if defined(__linux__)
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>

// a descriptor of the record, which the record holds as it is: a descriptor that the system gives is not negative
static int folder_watch_descriptor(unsigned long long kept)
{
    // Explicit cast reads the integer of the record back as the descriptor that the system gave
    return (int)kept;
}

void folder_watch_folder_put(CliFolderWatch *watch, const char *path)
{
    (void)inotify_add_watch(folder_watch_descriptor(watch->notified), path,
                            IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DONT_FOLLOW);
}

// a wait of `timeout_ms` on the inotify of `watch` and on its stop, -1 for no end. 1 where changes came, each read
// off the inotify. 0 where the time ran out. -1 where the stop came or the wait failed
static int folder_watch_waited(CliFolderWatch *watch, int timeout_ms)
{
    const int inotify = folder_watch_descriptor(watch->notified);
    struct pollfd waits[2] = {{inotify, POLLIN, 0}, {folder_watch_descriptor(watch->stopped), POLLIN, 0}};
    const int ready = poll(waits, 2u, timeout_ms);
    if ((ready < 0) || ((waits[1].revents & POLLIN) != 0))
    {
        return -1;
    }
    if (ready == 0)
    {
        return 0;
    }
    char answer[4096];
    while (read(inotify, answer, sizeof(answer)) > 0)
    {
    }
    return 1;
}

// Each change waited for, then each change after it until CLI_FOLDER_WATCH_QUIET_MS pass with none. A folder made
// since the last walk goes under the watch, and then the call. The stop ends the thread
void folder_watch_run(void *context)
{
    CliFolderWatch *const watch = (CliFolderWatch *)context;
    while (folder_watch_waited(watch, -1) == 1)
    {
        int waited = 1;
        while (waited == 1)
        {
            waited = folder_watch_waited(watch, (int)CLI_FOLDER_WATCH_QUIET_MS);
        }
        if (waited != 0)
        {
            break;
        }
        (void)folder_watch_walked(watch);
        watch->changed();
    }
    close(folder_watch_descriptor(watch->notified));
    folder_watch_ended(watch);
}

// the inotify on the folder of `watch` and each folder under it, and the stop's event, made: 1, or 0 where the system
// refuses
int folder_watch_opened(CliFolderWatch *watch)
{
    const int inotify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (inotify < 0)
    {
        return 0;
    }
    const int stopped = eventfd(0u, EFD_CLOEXEC);
    if (stopped < 0)
    {
        close(inotify);
        return 0;
    }
    // a descriptor that the system gives is not negative, which an unsigned long long holds
    watch->notified = (unsigned long long)inotify;
    watch->stopped = (unsigned long long)stopped;
    (void)folder_watch_walked(watch);
    return 1;
}

void folder_watch_stop_sent(CliFolderWatch *watch)
{
    const unsigned long long one = 1ULL;
    (void)write(folder_watch_descriptor(watch->stopped), &one, sizeof(one));
}

void folder_watch_closed(CliFolderWatch *watch)
{
    close(folder_watch_descriptor(watch->stopped));
}
#endif
