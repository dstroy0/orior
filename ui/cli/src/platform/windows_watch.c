// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_watch.c: the tree's changes as Windows tells of them, through ReadDirectoryChangesW over the whole tree, on a
// thread of the watch's own that waits on the system's answer and on its stop together
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the buffer the system writes changes into, in 32-bit words: 64 KB, the most a folder shared over a network takes;
// where a record's name starts, after its offset to the next, its action and its name's length; and what is watched
#define CLI_WATCH_BUFFER (16u * 1024u)
#define CLI_WATCH_NAME_AT 12u
#define CLI_WATCH_FILTER                                                                                               \
    (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |                            \
     FILE_NOTIFY_CHANGE_LAST_WRITE)

// The watch: the tree's folder, the events of the system's answer and of the stop, who is told, the thread, whether
// it has ended, and the buffer
typedef struct
{
    HANDLE folder;
    HANDLE answered;
    HANDLE stopped;
    CliChangeTold told;
    void *context;
    CliThread thread;
    int running;
    int ended;
    CliLock lock;
    CliWake wake;
    DWORD buffer[CLI_WATCH_BUFFER];
} CliWatch;

static CliWatch s_watch = {.lock = CLI_LOCK_INIT, .wake = CLI_WAKE_INIT};

// the records the system wrote, `written` bytes, each told by its path from the tree's top and what happened to it
static void cli_records_told(DWORD written)
{
    unsigned long long at = 0ull;
    const unsigned char *const bytes = (const unsigned char *)s_watch.buffer;
    const unsigned long long size = (written < sizeof(s_watch.buffer)) ? written : sizeof(s_watch.buffer);
    while ((at + CLI_WATCH_NAME_AT) <= size)
    {
        DWORD next = 0u;
        DWORD action = 0u;
        DWORD length = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &next, .src = bytes + at, .bytes = 4u});
        memor.cpy(&(MemoriaCfg){.dst = &action, .src = bytes + at + 4u, .bytes = 4u});
        memor.cpy(&(MemoriaCfg){.dst = &length, .src = bytes + at + 8u, .bytes = 4u});
        if ((at + CLI_WATCH_NAME_AT + length) > size)
        {
            break;
        }
        char path[CLI_PATH_LONGEST * 3u];
        // the name lies within what the system wrote, on a two-byte boundary, half its bytes in units
        const int made =
            WideCharToMultiByte(CP_UTF8, 0u, (const wchar_t *)(const void *)(bytes + at + CLI_WATCH_NAME_AT),
                                (int)(length / 2u), path, (int)sizeof(path) - 1, NULL, NULL);
        path[(made > 0) ? made : 0] = '\0';
        for (char *letter = path; letter[0] != '\0'; letter += 1)
        {
            letter[0] = (letter[0] == '\\') ? '/' : letter[0];
        }
        const unsigned int kind =
            ((action == FILE_ACTION_ADDED) || (action == FILE_ACTION_RENAMED_NEW_NAME))     ? CLI_CHANGE_ADDED
            : ((action == FILE_ACTION_REMOVED) || (action == FILE_ACTION_RENAMED_OLD_NAME)) ? CLI_CHANGE_REMOVED
                                                                                            : CLI_CHANGE_MODIFIED;
        s_watch.told(s_watch.context, path, kind);
        if (next == 0u)
        {
            break;
        }
        at += next;
    }
}

// the system asked for the tree's changes again and again, each answer told, until the stop is set
static void cli_watch_read(void *context)
{
    (void)context;
    for (;;)
    {
        OVERLAPPED overlapped;
        memor.set(&(MemoriaCfg){.dst = &overlapped, .val = 0, .bytes = sizeof(overlapped)});
        overlapped.hEvent = s_watch.answered;
        ResetEvent(s_watch.answered);
        if (!ReadDirectoryChangesW(s_watch.folder, s_watch.buffer, (DWORD)sizeof(s_watch.buffer), TRUE,
                                   CLI_WATCH_FILTER, NULL, &overlapped, NULL))
        {
            break;
        }
        const HANDLE waits[2] = {s_watch.answered, s_watch.stopped};
        const DWORD waited = WaitForMultipleObjects(2u, waits, FALSE, INFINITE);
        DWORD written = 0u;
        if (waited != WAIT_OBJECT_0)
        {
            CancelIoEx(s_watch.folder, &overlapped);
            GetOverlappedResult(s_watch.folder, &overlapped, &written, TRUE);
            break;
        }
        if (!GetOverlappedResult(s_watch.folder, &overlapped, &written, FALSE))
        {
            break;
        }
        if (written == 0u)
        {
            s_watch.told(s_watch.context, "", CLI_CHANGE_LOST);
        }
        else
        {
            cli_records_told(written);
        }
    }
    CloseHandle(s_watch.folder);
    CloseHandle(s_watch.answered);
    cli_lock_take(&s_watch.lock);
    s_watch.ended = 1;
    cli_wake_all(&s_watch.wake);
    cli_lock_give(&s_watch.lock);
}

int cli_watch_system_start(const char *root, CliChangeTold told, void *context)
{
    cli_watch_system_stop();
    wchar_t wide[CLI_PATH_LONGEST];
    if (MultiByteToWideChar(CP_UTF8, 0, root, -1, wide, CLI_PATH_LONGEST) == 0)
    {
        return 0;
    }
    const HANDLE folder = CreateFileW(wide, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (folder == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    const HANDLE answered = CreateEventW(NULL, TRUE, FALSE, NULL);
    const HANDLE stopped = CreateEventW(NULL, TRUE, FALSE, NULL);
    if ((answered == NULL) || (stopped == NULL))
    {
        CloseHandle(folder);
        if (answered != NULL)
        {
            CloseHandle(answered);
        }
        if (stopped != NULL)
        {
            CloseHandle(stopped);
        }
        return 0;
    }
    s_watch.folder = folder;
    s_watch.answered = answered;
    s_watch.stopped = stopped;
    s_watch.told = told;
    s_watch.context = context;
    s_watch.ended = 0;
    s_watch.thread = (CliThread){cli_watch_read, NULL};
    if (!cli_thread_started(&s_watch.thread))
    {
        CloseHandle(folder);
        CloseHandle(answered);
        CloseHandle(stopped);
        return 0;
    }
    s_watch.running = 1;
    return 1;
}

void cli_watch_system_stop(void)
{
    if (!s_watch.running)
    {
        return;
    }
    SetEvent(s_watch.stopped);
    cli_lock_take(&s_watch.lock);
    while (!s_watch.ended)
    {
        cli_wake_wait(&s_watch.wake, &s_watch.lock);
    }
    cli_lock_give(&s_watch.lock);
    CloseHandle(s_watch.stopped);
    s_watch.running = 0;
}
#endif
