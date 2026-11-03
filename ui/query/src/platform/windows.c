// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows.c: the query's calls on Windows: a process started, its output piped and waited on; the environment; folders
// walked, made and removed; a file's time and the clock; a path made full; and the terminal
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <stdint.h>

// the longest command line a process is started with, its zero counted
#define QUERY_COMMAND_MAX_LEN 32768u

// the hundreds of nanoseconds from 1601 to 1970, which the system's file times count from
#define QUERY_EPOCH_TICKS 116444736000000000ull

#define QUERY_TICKS_A_SECOND 10000000ull

typedef struct
{
#if (defined(_WIN32))
    VerbaLinea *line;
    const char *word;
    FILETIME time;
#endif
} WindowsCtx;
// `word` added to `line` quoted as the C runtime splits it back: in quotes where it is empty or contains a space, a tab
// or a quote, the backslashes before a quote and before the closing one doubled
EMBED_INLINE void query_word_quote(const WindowsCtx *args)
{
    const int quoted =
        (args->word[0] == '\0') ||
        (args->word[EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = args->word, .set = " \t\n\v\"")] !=
         '\0');
    if (!quoted)
    {
        mmgr_line_put(args->line, args->word);
        return;
    }
    mmgr_line_ch(args->line, '"');
    unsigned int slashes = 0u;
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at < whole;)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->word, .at = at, .size = whole, .set = "\\\"");
        mmgr_line_put_n(args->line, args->word + at, (size_t)(stop - at));
        const unsigned long long run_end = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                      .text = args->word, .at = stop, .size = whole, .set = "\\");
        mmgr_line_put_n(args->line, args->word + stop, (size_t)(run_end - stop));
        // a slash's count is below the path's bytes, which an unsigned int stores
        slashes = (run_end == whole) ? (unsigned int)(run_end - stop) : 0u;
        at = run_end;
        if ((run_end < whole) && (args->word[run_end] == '"'))
        {
            for (unsigned long long added = stop; added <= run_end; added += 1ull)
            {
                mmgr_line_ch(args->line, '\\');
            }
            mmgr_line_ch(args->line, '"');
            at = run_end + 1ull;
        }
    }
    for (unsigned int added = 0u; added < slashes; added += 1u)
    {
        mmgr_line_ch(args->line, '\\');
    }
    mmgr_line_ch(args->line, '"');
}

int query_process_start(const QueryStartedCtx *args)
{
    int output = args->output;
    args->started->process = 0ull;
    args->started->output = 0ull;
    static char s_line[QUERY_COMMAND_MAX_LEN];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    for (unsigned int at = 0u; args->words[at] != NULL; at += 1u)
    {
        if (at != 0u)
        {
            mmgr_line_ch(&line, ' ');
        }
        EMBED_CALL(query_word_quote, WindowsCtx, .line = &line, .word = args->words[at]);
    }
    if (mmgr_line_finish(&line) == 0ull)
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
    if (output == QUERY_OUTPUT_OWN)
    {
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
    else if (output == QUERY_OUTPUT_PIPED_ALONE)
    {
        startup.hStdError = nowhere;
    }
    else
    {
        startup.hStdError = write_end;
    }
    PROCESS_INFORMATION process;
    memor.set(&(MemoriaCfg){.dst = &process, .val = 0, .bytes = sizeof(process)});
    const BOOL made =
        CreateProcessA(NULL, s_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, args->folder, &startup, &process);
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
    // a handle is a pointer's width, which an unsigned long long represents
    args->started->process = (unsigned long long)(uintptr_t)process.hProcess;
    args->started->output = (read_end != INVALID_HANDLE_VALUE) ? (unsigned long long)(uintptr_t)read_end : 0ull;
    return 1;
}

unsigned long long query_process_read(const QueryProcessPieceCtx *args)
{
    if (args->process->output == 0ull)
    {
        return 0ull;
    }
    // a piece is at most a gibibyte, which a DWORD counts
    const DWORD most = (DWORD)((args->capacity_u64 < (1ull << 30u)) ? args->capacity_u64 : (1ull << 30u));
    DWORD got = 0u;
    if (!ReadFile((HANDLE)(uintptr_t)args->process->output, args->piece, most, &got, NULL))
    {
        return 0ull;
    }
    return got;
}

long long query_process_wait(const QueryProcessCtx *args)
{
    QueryProcess *process = args->process_out;
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
    // an exit code is a DWORD, which a long long represents
    return read ? (long long)code : -1ll;
}

// the most letters of a variable's name or value in Windows' wide letters, its zero among them
#define QUERY_ENVIRONMENT_WIDE 32768u

int query_environment_set(const QueryNameCtx *args)
{
    wchar_t s_name[QUERY_ENVIRONMENT_WIDE];
    wchar_t s_value[QUERY_ENVIRONMENT_WIDE];
    const int named = (MultiByteToWideChar(CP_UTF8, 0, args->name, -1, s_name, (int)QUERY_ENVIRONMENT_WIDE) != 0) &&
                      (MultiByteToWideChar(CP_UTF8, 0, args->value, -1, s_value, (int)QUERY_ENVIRONMENT_WIDE) != 0);
    const int set = named && (SetEnvironmentVariableW(s_name, s_value) != 0);
    return set;
}

int query_environment_read(const QueryNameValueCtx *args)
{
    wchar_t s_name[QUERY_ENVIRONMENT_WIDE];
    wchar_t s_value[QUERY_ENVIRONMENT_WIDE];
    const DWORD length = (MultiByteToWideChar(CP_UTF8, 0, args->name, -1, s_name, (int)QUERY_ENVIRONMENT_WIDE) != 0)
                             ? GetEnvironmentVariableW(s_name, s_value, QUERY_ENVIRONMENT_WIDE)
                             : 0u;
    // a capacity is far below what an int counts
    const int read =
        (length != 0u) && (length < QUERY_ENVIRONMENT_WIDE) &&
        (WideCharToMultiByte(CP_UTF8, 0, s_value, -1, args->value_out, (int)args->capacity, NULL, NULL) != 0);
    return read;
}

int query_environment_write(const QueryNameOutCtx *args)
{
    char s_value[QUERY_ENVIRONMENT_MAX_LEN];
    const int read = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = args->name,
                                .value_out = s_value, .capacity = sizeof(s_value));
    if (read)
    {
        mmgr_line_put(args->out, s_value);
    }
    return read && mmgr_line_ok(args->out);
}

int query_folder_each(const QueryFolderEachCtx *args)
{
    char pattern[QUERY_PATH_MAX_LEN + 4u];
    VerbaLinea line = {pattern, sizeof(pattern), 0ull};
    mmgr_line_put(&line, args->folder);
    mmgr_line_put(&line, "\\*");
    if (mmgr_line_finish(&line) == 0ull)
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
            EMBED_CALL(args->each, CliEachNameIsFolderCfg, .context = args->context_bytess, .name = found.cFileName,
                       .is_folder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u);
        }
    } while (FindNextFileA(walk, &found));
    FindClose(walk);
    return 1;
}

int query_folder_init(const QueryFolderContextCtx *args)
{
    char path[QUERY_PATH_MAX_LEN];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL});
    if (length >= sizeof(path))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = path, .src = args->folder, .bytes = length + 1u});
    for (size_t at = 1u; at <= length; at += 1u)
    {
        if ((path[at] == '/') || (path[at] == '\\') || (path[at] == '\0'))
        {
            const char saved_char = path[at];
            path[at] = '\0';
            // a drive's own name, `C:`, is no folder to make
            if (!((at == 2u) && (path[1] == ':')))
            {
                CreateDirectoryA(path, NULL);
            }
            path[at] = saved_char;
        }
    }
    return EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->folder);
}

// what a folder's removal walks: the folder and whether every entry went
typedef struct
{
    const char *folder;
    int whole;
} QueryRemoval;

static void query_entry_remove(const CliEachNameIsFolderCfg *args)
{
    QueryRemoval *const removal = (QueryRemoval *)args->context;
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, removal->folder);
    mmgr_line_ch(&line, '\\');
    mmgr_line_put(&line, args->name);
    removal->whole = (mmgr_line_finish(&line) != 0ull) &&
                     EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path) &&
                     removal->whole;
}

int query_remove(const QueryPathSecondsCtx *args)
{
    const DWORD attributes = GetFileAttributesA(args->path);
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return 1;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u)
    {
        return DeleteFileA(args->path) != 0;
    }
    QueryRemoval removal = {args->path, 1};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->path,
               .each = query_entry_remove, .context_bytess = &removal);
    return removal.whole && (RemoveDirectoryA(args->path) != 0);
}

int query_file_copy(const QueryFromCtx *args)
{
    return CopyFileA(args->from, args->to, FALSE) != 0;
}

int query_file_rename(const QueryFromCtx *args)
{
    return MoveFileExA(args->from, args->to, MOVEFILE_REPLACE_EXISTING) != 0;
}

int query_file_write(const QueryPathBytesSizeCtx *args)
{
    const HANDLE file = CreateFileA(args->path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    unsigned long long put = 0ull;
    int whole = 1;
    while (whole && (put < args->size))
    {
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((args->size - put) < (1ull << 30u)) ? (args->size - put) : (1ull << 30u));
        DWORD wrote = 0u;
        whole = WriteFile(file, (const unsigned char *)args->bytes + put, piece, &wrote, NULL) && (wrote == piece);
        put += wrote;
    }
    return CloseHandle(file) && whole;
}

int query_is_file(const QueryPathSecondsCtx *args)
{
    const DWORD attributes = GetFileAttributesA(args->path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u);
}

int query_is_folder(const QueryPathSecondsCtx *args)
{
    const DWORD attributes = GetFileAttributesA(args->path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u);
}

// a file time as seconds since 1970, 0 for one before it
EMBED_INLINE unsigned long long query_seconds_get(const WindowsCtx *args)
{
    const unsigned long long ticks = ((unsigned long long)args->time.dwHighDateTime << 32u) | args->time.dwLowDateTime;
    return (ticks > QUERY_EPOCH_TICKS) ? ((ticks - QUERY_EPOCH_TICKS) / QUERY_TICKS_A_SECOND) : 0ull;
}

int query_modify(const QueryPathSecondsCtx *args)
{
    unsigned long long *seconds = args->seconds;
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(args->path, GetFileExInfoStandard, &data))
    {
        return 0;
    }
    *seconds = EMBED_CALL(query_seconds_get, WindowsCtx, .time = data.ftLastWriteTime);
    return 1;
}

int query_modified_set(const QueryPathSecondsKindCtx *args)
{
    const HANDLE file = CreateFileA(args->path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    const unsigned long long ticks = (args->seconds_value * QUERY_TICKS_A_SECOND) + QUERY_EPOCH_TICKS;
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
    return EMBED_CALL(query_seconds_get, WindowsCtx, .time = now);
}

void query_pause(const QueryMillisecondsCtx *args)
{
    Sleep(args->milliseconds);
}

int query_path_full(const QueryCapacityPathCtx *args)
{
    // a capacity is far below what a DWORD counts
    const DWORD length = GetFullPathNameA(args->path, (DWORD)args->capacity, args->full, NULL);
    if ((length == 0u) || (length >= args->capacity))
    {
        return 0;
    }
    if (cellul.starts(&(CatenaFinitaCfg){.src = args->full, .other = "\\\\?\\", .cap = CLI_CELLS_POOL}))
    {
        memor.move_down(
            &(MemoriaCfg){.dst = args->full,
                          .src = args->full + 4,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->full + 4, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return 1;
}

int query_terminal_is(void)
{
    return GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_CHAR;
}

unsigned long long query_terminal_line_read(const QueryCapacityLineCtx *args)
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
            if ((length + 1u) < args->capacity)
            {
                args->line[length] = letter;
                length += 1u;
            }
        }
    }
    args->line[length] = '\0';
    return whole;
}

void query_terminal_write(const QueryErrorsCtx *args)
{
    const HANDLE output = GetStdHandle(args->errors ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    unsigned long long put = 0ull;
    while (put < args->length)
    {
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((args->length - put) < (1ull << 30u)) ? (args->length - put) : (1ull << 30u));
        DWORD wrote = 0u;
        if (!WriteFile(output, args->text + put, piece, &wrote, NULL) || (wrote == 0u))
        {
            return;
        }
        put += wrote;
    }
}

#endif
