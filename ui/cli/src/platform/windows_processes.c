// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_processes.c: the processes of the machine as Windows lists them, read in one snapshot, and the line each was
// started with, read from the process where it lets that be read; and a process's memory, its start and its program's
// name
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <psapi.h>
#include <tlhelp32.h>

// what NtQueryInformationProcess is queried for: the line a process was started with; the most bytes it is read in; and
// the access a process is opened with to read it
#define CLI_PROCESS_COMMAND_LINE 60u
#define CLI_PROCESS_COMMAND_MOST (1u << 20u)
#define CLI_PROCESS_QUERY_LIMITED 0x1000u

typedef LONG(NTAPI *CliQueryProcess)(HANDLE process, ULONG kind, PVOID info, ULONG length, PULONG returned);

// the text the name is written in, under its lock
static char s_name[MAX_PATH * 4u];
static CliLock s_lock = CLI_LOCK_INIT;

// `count` UTF-16 units written as UTF-8 into `out`, sized for `room`, each unit that starts no whole letter as U+FFFD
static void processes_utf8(const wchar_t *text, int count, char *out, int room)
{
    const int written = (count > 0) ? WideCharToMultiByte(CP_UTF8, 0u, text, count, out, room - 1, NULL, NULL) : 0;
    out[(written > 0) ? written : 0] = '\0';
}

// the line process `pid` was started with, a counted string, its length in bytes first and the address of its text
// after, the text following in the same room, read into a cell of the bytes the system reports it takes: the line as
// UTF-8 in a cell of its own, given back with g_cli_cells->persistent_buf_release, or NULL where it cannot be read
static char *processes_command_of(CliQueryProcess query, DWORD pid)
{
    const HANDLE process = OpenProcess(CLI_PROCESS_QUERY_LIMITED, FALSE, pid);
    if ((process == NULL) || (query == NULL))
    {
        if (process != NULL)
        {
            CloseHandle(process);
        }
        return NULL;
    }
    char *command = NULL;
    ULONG needed = 0u;
    (void)query(process, CLI_PROCESS_COMMAND_LINE, NULL, 0u, &needed);
    const ULONG room = needed;
    // the room taken on a cell's boundary, aligned for the string's header
    unsigned char *const base = ((room > 2u * sizeof(void *)) && (room < CLI_PROCESS_COMMAND_MOST))
                                    ? g_cli_cells->persistent_buf_alloc(room, NULL)
                                    : NULL;
    if ((base != NULL) && (query(process, CLI_PROCESS_COMMAND_LINE, base, room, &needed) >= 0))
    {
        unsigned short bytes = 0u;
        const wchar_t *start = NULL;
        memor.cpy(&(MemoriaCfg){.dst = &bytes, .src = base, .bytes = sizeof(bytes)});
        memor.cpy(&(MemoriaCfg){.dst = (void *)&start, .src = base + sizeof(void *), .bytes = sizeof(start)});
        const size_t length = (size_t)bytes / 2u;
        const unsigned char *const end = base + room;
        // the text read lies within the room, as the system wrote it there, and each of its units is three bytes of
        // UTF-8 at the most, below an int's reach as the room is
        if ((start != NULL) && ((const unsigned char *)start >= base) &&
            ((const unsigned char *)(start + length) <= end))
        {
            command = g_cli_cells->persistent_buf_alloc((3ull * length) + 1ull, NULL);
        }
        if (command != NULL)
        {
            processes_utf8(start, (int)length, command, (int)((3u * length) + 1u));
        }
    }
    cli_cell_release(base, NULL);
    CloseHandle(process);
    return command;
}

void cli_processes_each(void (*each)(void *context, unsigned int pid, const char *name, const char *command),
                        void *context)
{
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    // the function's address taken as the type it has, as GetProcAddress gives every function's
    const CliQueryProcess query =
        (ntdll != NULL) ? (CliQueryProcess)(void (*)(void))GetProcAddress(ntdll, "NtQueryInformationProcess") : NULL;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0u);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }
    cli_lock_take(&s_lock);
    PROCESSENTRY32W entry;
    memor.set(&(MemoriaCfg){.dst = &entry, .val = 0, .bytes = sizeof(entry)});
    entry.dwSize = sizeof(entry);
    for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry))
    {
        int end = 0;
        while ((end < MAX_PATH) && (entry.szExeFile[end] != L'\0'))
        {
            end += 1;
        }
        processes_utf8(entry.szExeFile, end, s_name, (int)sizeof(s_name));
        char *const command = processes_command_of(query, entry.th32ProcessID);
        each(context, (unsigned int)entry.th32ProcessID, s_name, (command != NULL) ? command : "");
        cli_cell_release(command, NULL);
    }
    cli_lock_give(&s_lock);
    CloseHandle(snapshot);
}

int cli_process_running(unsigned int pid)
{
    const HANDLE process = OpenProcess(CLI_PROCESS_QUERY_LIMITED, FALSE, (DWORD)pid);
    if (process == NULL)
    {
        return 0;
    }
    DWORD code = 0u;
    const BOOL read = GetExitCodeProcess(process, &code);
    CloseHandle(process);
    return (read != 0) && (code == STILL_ACTIVE);
}

int cli_process_memory(unsigned int pid, CliProcessMemory *memory)
{
    const HANDLE process = OpenProcess(CLI_PROCESS_QUERY_LIMITED, FALSE, (DWORD)pid);
    if (process == NULL)
    {
        return 0;
    }
    PROCESS_MEMORY_COUNTERS_EX2 counters;
    memor.set(&(MemoriaCfg){.dst = &counters, .val = 0, .bytes = sizeof(counters)});
    counters.cb = sizeof(counters);
    // the counters' first part is the structure the call is declared to fill, and `cb` says how much more it holds
    const BOOL read = K32GetProcessMemoryInfo(process, (PROCESS_MEMORY_COUNTERS *)&counters, counters.cb);
    FILETIME made;
    FILETIME ended;
    FILETIME kernel;
    FILETIME user;
    const BOOL timed = GetProcessTimes(process, &made, &ended, &kernel, &user);
    CloseHandle(process);
    if (!read || !timed)
    {
        return 0;
    }
    memory->working = counters.PrivateWorkingSetSize;
    memory->commit = counters.PrivateUsage;
    memory->started = ((unsigned long long)made.dwHighDateTime << 32u) | made.dwLowDateTime;
    return 1;
}

int cli_process_named(unsigned int pid, char *name, size_t room)
{
    const HANDLE process = OpenProcess(CLI_PROCESS_QUERY_LIMITED, FALSE, (DWORD)pid);
    if (process == NULL)
    {
        return 0;
    }
    wchar_t path[MAX_PATH * 4u];
    DWORD length = MAX_PATH * 4u;
    const BOOL read = QueryFullProcessImageNameW(process, 0u, path, &length);
    CloseHandle(process);
    if (!read)
    {
        return 0;
    }
    DWORD start = length;
    while ((start > 0u) && (path[start - 1u] != L'\\') && (path[start - 1u] != L'/'))
    {
        start -= 1u;
    }
    DWORD end = length;
    if ((end - start > 4u) && (path[end - 4u] == L'.') && ((path[end - 3u] | 0x20u) == L'e') &&
        ((path[end - 2u] | 0x20u) == L'x') && ((path[end - 1u] | 0x20u) == L'e'))
    {
        end -= 4u;
    }
    // a name's units are fewer than a path's, which an int counts, and the room is far below what an int counts
    processes_utf8(path + start, (int)(end - start), name, (int)room);
    return 1;
}
#endif
