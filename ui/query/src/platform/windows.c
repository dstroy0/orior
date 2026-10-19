// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows.c: the query's calls on Windows: a process started, its output piped and waited on; the environment; folders
// walked, made and removed; a file's time and the clock; a path made full; and the terminal
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <stdint.h>

// the longest command line a process is started with, its zero counted
#define QUERY_COMMAND_LONGEST 32768u

// the hundreds of nanoseconds from 1601 to 1970, which the system's file times count from
#define QUERY_EPOCH_TICKS 116444736000000000ull

#define QUERY_TICKS_A_SECOND 10000000ull

// `word` added to `line` quoted as the C runtime splits it back: in quotes where it is empty or holds a space, a tab
// or a quote, the backslashes before a quote and before the closing one doubled
static void query_word_quoted(CliLine *line, const char *word)
{
    const int quoted = (word[0] == '\0') || (word[cli_text_upto(word, " \t\n\v\"")] != '\0');
    if (!quoted)
    {
        cli_line_text(line, word);
        return;
    }
    cli_line_character(line, '"');
    unsigned int slashes = 0u;
    for (const char *walk = word; *walk != '\0'; walk += 1)
    {
        const unsigned int doubled = (*walk == '"') ? (slashes + 1u) : 0u;
        for (unsigned int added = 0u; added < doubled; added += 1u)
        {
            cli_line_character(line, '\\');
        }
        slashes = (*walk == '\\') ? (slashes + 1u) : 0u;
        cli_line_character(line, *walk);
    }
    for (unsigned int added = 0u; added < slashes; added += 1u)
    {
        cli_line_character(line, '\\');
    }
    cli_line_character(line, '"');
}

int query_process_start(QueryProcess *started, const char *const *words, const char *folder, int output)
{
    started->process = 0ull;
    started->output = 0ull;
    static char s_line[QUERY_COMMAND_LONGEST];
    CliLine line = {s_line, sizeof(s_line), 0ull};
    for (unsigned int at = 0u; words[at] != NULL; at += 1u)
    {
        if (at != 0u)
        {
            cli_line_character(&line, ' ');
        }
        query_word_quoted(&line, words[at]);
    }
    if (cli_line_closed(&line) == 0ull)
    {
        return 0;
    }
    SECURITY_ATTRIBUTES inherited = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE read_end = INVALID_HANDLE_VALUE;
    HANDLE write_end = INVALID_HANDLE_VALUE;
    if ((output == QUERY_OUTPUT_PIPED) || (output == QUERY_OUTPUT_PIPED_ALONE))
    {
        if (!CreatePipe(&read_end, &write_end, &inherited, 0u))
        {
            return 0;
        }
        // the end this process reads is not handed to the child
        SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0u);
    }
    else if (output == QUERY_OUTPUT_NONE)
    {
        write_end = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    }
    const HANDLE input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited, OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL, NULL);
    const HANDLE nowhere = (output == QUERY_OUTPUT_PIPED_ALONE)
                               ? CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited,
                                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
                               : INVALID_HANDLE_VALUE;
    STARTUPINFOA startup;
    memor.set(&(MemoriaCfg){.dst = &startup, .val = 0, .bytes = sizeof(startup)});
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = (output == QUERY_OUTPUT_OWN) ? GetStdHandle(STD_OUTPUT_HANDLE) : write_end;
    startup.hStdError = (output == QUERY_OUTPUT_OWN)           ? GetStdHandle(STD_ERROR_HANDLE)
                        : (output == QUERY_OUTPUT_PIPED_ALONE) ? nowhere
                                                               : write_end;
    PROCESS_INFORMATION process;
    memor.set(&(MemoriaCfg){.dst = &process, .val = 0, .bytes = sizeof(process)});
    const BOOL made =
        CreateProcessA(NULL, s_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, folder, &startup, &process);
    if (input != INVALID_HANDLE_VALUE)
    {
        CloseHandle(input);
    }
    if (write_end != INVALID_HANDLE_VALUE)
    {
        CloseHandle(write_end);
    }
    if (nowhere != INVALID_HANDLE_VALUE)
    {
        CloseHandle(nowhere);
    }
    if (!made)
    {
        if (read_end != INVALID_HANDLE_VALUE)
        {
            CloseHandle(read_end);
        }
        return 0;
    }
    CloseHandle(process.hThread);
    // a handle is a pointer's width, which an unsigned long long holds
    started->process = (unsigned long long)(uintptr_t)process.hProcess;
    started->output = (read_end != INVALID_HANDLE_VALUE) ? (unsigned long long)(uintptr_t)read_end : 0ull;
    return 1;
}

unsigned long long query_process_read(const QueryProcess *process, char *piece, unsigned long long room)
{
    if (process->output == 0ull)
    {
        return 0ull;
    }
    // a piece is at most a gibibyte, which a DWORD counts
    const DWORD most = (DWORD)((room < (1ull << 30u)) ? room : (1ull << 30u));
    DWORD got = 0u;
    if (!ReadFile((HANDLE)(uintptr_t)process->output, piece, most, &got, NULL))
    {
        return 0ull;
    }
    return got;
}

long long query_process_wait(QueryProcess *process)
{
    if (process->process == 0ull)
    {
        return -1ll;
    }
    const HANDLE handle = (HANDLE)(uintptr_t)process->process;
    WaitForSingleObject(handle, INFINITE);
    DWORD code = 0u;
    const BOOL read = GetExitCodeProcess(handle, &code);
    CloseHandle(handle);
    if (process->output != 0ull)
    {
        CloseHandle((HANDLE)(uintptr_t)process->output);
    }
    process->process = 0ull;
    process->output = 0ull;
    // an exit code is a DWORD, which a long long holds
    return read ? (long long)code : -1ll;
}

// the most letters of a variable's name or value in Windows' wide letters, its zero among them
#define QUERY_ENVIRONMENT_WIDE 32768u

int query_environment_set(const char *name, const char *value)
{
    static wchar_t s_name[QUERY_ENVIRONMENT_WIDE];
    static wchar_t s_value[QUERY_ENVIRONMENT_WIDE];
    static SRWLOCK s_lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&s_lock);
    const int named = (MultiByteToWideChar(CP_UTF8, 0, name, -1, s_name, (int)QUERY_ENVIRONMENT_WIDE) != 0) &&
                      (MultiByteToWideChar(CP_UTF8, 0, value, -1, s_value, (int)QUERY_ENVIRONMENT_WIDE) != 0);
    const int set = named && (SetEnvironmentVariableW(s_name, s_value) != 0);
    ReleaseSRWLockExclusive(&s_lock);
    return set;
}

int query_environment_read(const char *name, char *value, size_t room)
{
    static wchar_t s_name[QUERY_ENVIRONMENT_WIDE];
    static wchar_t s_value[QUERY_ENVIRONMENT_WIDE];
    static SRWLOCK s_lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&s_lock);
    const DWORD length = (MultiByteToWideChar(CP_UTF8, 0, name, -1, s_name, (int)QUERY_ENVIRONMENT_WIDE) != 0)
                             ? GetEnvironmentVariableW(s_name, s_value, QUERY_ENVIRONMENT_WIDE)
                             : 0u;
    // a room is far below what an int counts
    const int read = (length != 0u) && (length < QUERY_ENVIRONMENT_WIDE) &&
                     (WideCharToMultiByte(CP_UTF8, 0, s_value, -1, value, (int)room, NULL, NULL) != 0);
    ReleaseSRWLockExclusive(&s_lock);
    return read;
}

int query_environment_said(const char *name, CliLine *out)
{
    static char s_value[QUERY_ENVIRONMENT_LONGEST];
    static SRWLOCK s_lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&s_lock);
    const int read = query_environment_read(name, s_value, sizeof(s_value));
    if (read)
    {
        cli_line_text(out, s_value);
    }
    ReleaseSRWLockExclusive(&s_lock);
    return read && cli_line_fits(out);
}

int query_folder_each(const char *folder, void (*each)(void *context, const char *name, int is_folder), void *context)
{
    char pattern[QUERY_PATH_LONGEST + 4u];
    CliLine line = {pattern, sizeof(pattern), 0ull};
    cli_line_text(&line, folder);
    cli_line_text(&line, "\\*");
    if (cli_line_closed(&line) == 0ull)
    {
        return 0;
    }
    WIN32_FIND_DATAA found;
    const HANDLE walk = FindFirstFileA(pattern, &found);
    if (walk == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    do
    {
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = found.cFileName, .other = ".", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = found.cFileName, .other = "..", .cap = CLI_CELLS_POOL})))
        {
            each(context, found.cFileName, (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u);
        }
    } while (FindNextFileA(walk, &found));
    FindClose(walk);
    return 1;
}

int query_folder_made(const char *folder)
{
    char path[QUERY_PATH_LONGEST];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    if (length >= sizeof(path))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = path, .src = folder, .bytes = length + 1u});
    for (size_t at = 1u; at <= length; at += 1u)
    {
        if ((path[at] == '/') || (path[at] == '\\') || (path[at] == '\0'))
        {
            const char held = path[at];
            path[at] = '\0';
            // a drive's own name, `C:`, is no folder to make
            if (!((at == 2u) && (path[1] == ':')))
            {
                CreateDirectoryA(path, NULL);
            }
            path[at] = held;
        }
    }
    return query_is_folder(folder);
}

// what a folder's removal walks: the folder and whether every entry went
typedef struct
{
    const char *folder;
    int whole;
} QueryRemoval;

static void query_entry_removed(void *context, const char *name, int is_folder)
{
    QueryRemoval *const removal = (QueryRemoval *)context;
    char path[QUERY_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, removal->folder);
    cli_line_character(&line, '\\');
    cli_line_text(&line, name);
    (void)is_folder;
    removal->whole = (cli_line_closed(&line) != 0ull) && query_removed(path) && removal->whole;
}

int query_removed(const char *path)
{
    const DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return 1;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u)
    {
        return DeleteFileA(path) != 0;
    }
    QueryRemoval removal = {path, 1};
    query_folder_each(path, query_entry_removed, &removal);
    return removal.whole && (RemoveDirectoryA(path) != 0);
}

int query_file_copied(const char *from, const char *to)
{
    return CopyFileA(from, to, FALSE) != 0;
}

int query_file_renamed(const char *from, const char *to)
{
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
}

int query_file_written(const char *path, const void *bytes, unsigned long long size)
{
    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    unsigned long long put = 0ull;
    int whole = 1;
    while (whole && (put < size))
    {
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((size - put) < (1ull << 30u)) ? (size - put) : (1ull << 30u));
        DWORD wrote = 0u;
        whole = WriteFile(file, (const unsigned char *)bytes + put, piece, &wrote, NULL) && (wrote == piece);
        put += wrote;
    }
    return CloseHandle(file) && whole;
}

int query_is_file(const char *path)
{
    const DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u);
}

int query_is_folder(const char *path)
{
    const DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u);
}

// a file time as seconds since 1970, 0 for one before it
static unsigned long long query_seconds_of(FILETIME time)
{
    const unsigned long long ticks = ((unsigned long long)time.dwHighDateTime << 32u) | time.dwLowDateTime;
    return (ticks > QUERY_EPOCH_TICKS) ? ((ticks - QUERY_EPOCH_TICKS) / QUERY_TICKS_A_SECOND) : 0ull;
}

int query_modified(const char *path, unsigned long long *seconds)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
    {
        return 0;
    }
    *seconds = query_seconds_of(data.ftLastWriteTime);
    return 1;
}

int query_modified_set(const char *path, unsigned long long seconds)
{
    const HANDLE file = CreateFileA(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    const unsigned long long ticks = (seconds * QUERY_TICKS_A_SECOND) + QUERY_EPOCH_TICKS;
    FILETIME time;
    // a FILETIME is the ticks' two halves
    time.dwLowDateTime = (DWORD)(ticks & 0xffffffffull);
    time.dwHighDateTime = (DWORD)(ticks >> 32u);
    const int set = SetFileTime(file, NULL, NULL, &time) != 0;
    CloseHandle(file);
    return set;
}

unsigned long long query_now(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    return query_seconds_of(now);
}

void query_paused(unsigned int milliseconds)
{
    Sleep(milliseconds);
}

int query_path_full(const char *path, char *full, size_t room)
{
    // a room is far below what a DWORD counts
    const DWORD length = GetFullPathNameA(path, (DWORD)room, full, NULL);
    if ((length == 0u) || (length >= room))
    {
        return 0;
    }
    if (cellul.starts(&(CatenaFinitaCfg){.src = full, .other = "\\\\?\\", .cap = CLI_CELLS_POOL}))
    {
        memor.move_down(
            &(MemoriaCfg){.dst = full,
                          .src = full + 4,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = full + 4, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return 1;
}

int query_terminal_is(void)
{
    return GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_CHAR;
}

unsigned long long query_terminal_line_read(char *line, size_t room)
{
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    size_t length = 0u;
    unsigned long long whole = 0ull;
    int read_any = 0;
    for (;;)
    {
        char letter = '\0';
        DWORD got = 0u;
        if (!ReadFile(input, &letter, 1u, &got, NULL) || (got == 0u))
        {
            if (!read_any)
            {
                return QRY_MEMBER_UNREAD;
            }
            break;
        }
        read_any = 1;
        if (letter == '\n')
        {
            break;
        }
        if (letter != '\r')
        {
            whole += 1ull;
            if ((length + 1u) < room)
            {
                line[length] = letter;
                length += 1u;
            }
        }
    }
    line[length] = '\0';
    return whole;
}

void query_terminal_written(int errors, const char *text, unsigned long long length)
{
    const HANDLE output = GetStdHandle(errors ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    unsigned long long put = 0ull;
    while (put < length)
    {
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((length - put) < (1ull << 30u)) ? (length - put) : (1ull << 30u));
        DWORD wrote = 0u;
        if (!WriteFile(output, text + put, piece, &wrote, NULL) || (wrote == 0u))
        {
            return;
        }
        put += wrote;
    }
}

#endif
