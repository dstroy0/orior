// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_processes.c: the processes of the machine as Windows lists them, read in one snapshot, and the line each was
// started with, read from the process where it lets that be read
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <tlhelp32.h>

// what NtQueryInformationProcess is asked for: the line a process was started with; the most bytes it is read in; and
// the access a process is opened with to read it
#define CLI_PROCESS_COMMAND_LINE 60u
#define CLI_PROCESS_COMMAND_MOST (1u << 20u)
#define CLI_PROCESS_QUERY_LIMITED 0x1000u

typedef LONG(NTAPI *CliQueryProcess)(HANDLE process, ULONG kind, PVOID info, ULONG length, PULONG returned);

// the room the line is read in, as long as the most it may be and aligned for the string's header, and the texts the
// name and the line are written in, under their lock
static unsigned long long s_line[CLI_PROCESS_COMMAND_MOST / 8u];
static char s_name[MAX_PATH * 4u];
static char s_command[CLI_PROCESS_COMMAND_MOST];
static CliLock s_lock = CLI_LOCK_INIT;

// `count` UTF-16 units written as UTF-8 into `out`, which holds `room`, each unit that starts no whole letter as U+FFFD
static void processes_utf8(const wchar_t *text, int count, char *out, int room)
{
    const int written = (count > 0) ? WideCharToMultiByte(CP_UTF8, 0u, text, count, out, room - 1, NULL, NULL) : 0;
    out[(written > 0) ? written : 0] = '\0';
}

// the line process `pid` was started with, a counted string, its length in bytes first and the address of its text
// after, the text following in the same room, into the command text: empty where it cannot be read
static void processes_command_of(CliQueryProcess query, DWORD pid)
{
    s_command[0] = '\0';
    const HANDLE process = OpenProcess(CLI_PROCESS_QUERY_LIMITED, FALSE, pid);
    if ((process == NULL) || (query == NULL))
    {
        if (process != NULL)
        {
            CloseHandle(process);
        }
        return;
    }
    ULONG needed = 0u;
    (void)query(process, CLI_PROCESS_COMMAND_LINE, NULL, 0u, &needed);
    if ((needed > 2u * sizeof(void *)) && (needed < CLI_PROCESS_COMMAND_MOST) &&
        (query(process, CLI_PROCESS_COMMAND_LINE, s_line, needed, &needed) >= 0))
    {
        const unsigned char *const base = (const unsigned char *)s_line;
        unsigned short bytes = 0u;
        const wchar_t *start = NULL;
        memor.cpy(&(MemoriaCfg){.dst = &bytes, .src = base, .bytes = sizeof(bytes)});
        memor.cpy(&(MemoriaCfg){.dst = (void *)&start, .src = base + sizeof(void *), .bytes = sizeof(start)});
        const size_t length = (size_t)bytes / 2u;
        const unsigned char *const end = base + sizeof(s_line);
        // the text read lies within the room, as the system wrote it there
        if ((start != NULL) && ((const unsigned char *)start >= base) &&
            ((const unsigned char *)(start + length) <= end))
        {
            processes_utf8(start, (int)length, s_command, (int)sizeof(s_command));
        }
    }
    CloseHandle(process);
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
        processes_command_of(query, entry.th32ProcessID);
        each(context, (unsigned int)entry.th32ProcessID, s_name, s_command);
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
#endif
