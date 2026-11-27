// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_folder_watch.c: the watch of a folder on Windows (../folder_watch.h). A change notice on the folder reports
// each change under it, and an event carries the stop
#include "../folder_watch.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include "platform_windows.h"

// a handle of the system as the integer of the record, and back. A handle is a pointer's width, which an unsigned long
// long holds
static unsigned long long folder_watch_kept(HANDLE handle)
{
    // Explicit cast carries the handle as the integer that the record holds
    return (unsigned long long)(uintptr_t)handle;
}

static HANDLE folder_watch_handle(unsigned long long kept)
{
    // Explicit cast reads the integer of the record back as the handle
    return (HANDLE)(uintptr_t)kept;
}

void folder_watch_folder_put(CliFolderWatch *watch, const char *path)
{
    // the change notice of the top folder reports every folder under it
    (void)watch;
    (void)path;
}

// Each notice waited for, then each notice after it until CLI_FOLDER_WATCH_QUIET_MS pass with none, then the call.
// The stop ends the thread
void folder_watch_run(void *context)
{
    CliFolderWatch *const watch = (CliFolderWatch *)context;
    const HANDLE waits[2] = {folder_watch_handle(watch->notified), folder_watch_handle(watch->stopped)};
    while (WaitForMultipleObjects(2u, waits, FALSE, INFINITE) == WAIT_OBJECT_0)
    {
        DWORD waited = WAIT_OBJECT_0;
        while ((waited == WAIT_OBJECT_0) && FindNextChangeNotification(waits[0]))
        {
            waited = WaitForMultipleObjects(2u, waits, FALSE, CLI_FOLDER_WATCH_QUIET_MS);
        }
        if (waited != WAIT_TIMEOUT)
        {
            break;
        }
        watch->changed();
    }
    FindCloseChangeNotification(waits[0]);
    folder_watch_ended(watch);
}

// the system's notice on the folder of `watch` and the stop's event made: 1, or 0 where the system refuses
int folder_watch_opened(CliFolderWatch *watch)
{
    wchar_t wide[PLATFORM_WIDE_PATH];
    if (!platform_wide(watch->folder, wide, PLATFORM_WIDE_PATH))
    {
        return 0;
    }
    const HANDLE notified = FindFirstChangeNotificationW(wide, TRUE,
                                                         FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                                             FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (notified == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    const HANDLE stopped = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (stopped == NULL)
    {
        FindCloseChangeNotification(notified);
        return 0;
    }
    watch->notified = folder_watch_kept(notified);
    watch->stopped = folder_watch_kept(stopped);
    return 1;
}

void folder_watch_stop_sent(CliFolderWatch *watch)
{
    SetEvent(folder_watch_handle(watch->stopped));
}

void folder_watch_closed(CliFolderWatch *watch)
{
    CloseHandle(folder_watch_handle(watch->stopped));
}
#endif
