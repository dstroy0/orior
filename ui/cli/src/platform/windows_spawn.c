// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_spawn.c: a run's process started on Windows: its environment block made from this process's and the run's
// settings, its output and errors piped back apart, and, detached, started suspended with no console, made the head of
// a job of its own and let run, so ending the job ends every process it starts; or started in a pseudo-console of its
// own, a terminal's shell, its output read and its keys written through the console's pipes
#include "../command_line/cli_internal.h"
#include "../text/text.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the most bytes of a command line and of an environment block
#define CLI_LINE_BYTES 32768u

#define CLI_ENVIRONMENT_UNITS (1u << 17u)

typedef struct
{
#if (defined(_WIN32))
    VerbaLinea *line;
    const char *word;
    const wchar_t *entry;
    const wchar_t *name;
    const CliSetting *settings;
    unsigned int count;
    wchar_t *block;
    unsigned long long room;
    const char *const *words;
    const char *folder;
    VerbaLinea *err_msg;
    unsigned int priority;
    CliSpawned *spawned;
    int detached;
    int fed;
#endif
} WindowsSpawnCtx;
// `word` added to `line` quoted as the C runtime splits it back: in quotes where it is empty or contains a space, a tab
// or a quote, the backslashes before a quote and before the closing one doubled
EMBED_INLINE void cli_word_quote(const WindowsSpawnCtx *args)
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
        const unsigned long long run_end = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
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

// whether the entry `name=value` at `entry` is named `name`, whatever its case, as Windows names its variables
EMBED_INLINE int cli_by_name(const WindowsSpawnCtx *args)
{
    const wchar_t *entry = args->entry;
    const wchar_t *name = args->name;
    for (; name[0] != L'\0'; name += 1, entry += 1)
    {
        const wchar_t left = ((entry[0] >= L'a') && (entry[0] <= L'z')) ? (wchar_t)(entry[0] - 32) : entry[0];
        const wchar_t right = ((name[0] >= L'a') && (name[0] <= L'z')) ? (wchar_t)(name[0] - 32) : name[0];
        if (left != right)
        {
            return 0;
        }
    }
    return entry[0] == L'=';
}

// The environment block: this process's entries less those the settings name, then each setting given a value
EMBED_INLINE int cli_environment(const WindowsSpawnCtx *args)
{
    static wchar_t s_names[64][CLI_NAME_MAX_LEN];
    if (args->count > 64u)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (MultiByteToWideChar(CP_UTF8, 0, args->settings[at].name, -1, s_names[at], CLI_NAME_MAX_LEN) == 0)
        {
            return 0;
        }
    }
    wchar_t *const own = GetEnvironmentStringsW();
    if (own == NULL)
    {
        return 0;
    }
    unsigned long long used = 0ull;
    int fits = 1;
    for (const wchar_t *entry = own; fits && (entry[0] != L'\0'); entry += wcslen(entry) + 1u)
    {
        int named = 0;
        for (unsigned int at = 0u; !named && (at < args->count); at += 1u)
        {
            named = EMBED_CALL(cli_by_name, WindowsSpawnCtx, .entry = entry, .name = s_names[at]);
        }
        const unsigned long long length = wcslen(entry) + 1ull;
        fits = (used + length + 1ull) < args->room;
        if (!named && fits)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->block + used, .src = entry, .bytes = length * sizeof(wchar_t)});
            used += length;
        }
    }
    FreeEnvironmentStringsW(own);
    for (unsigned int at = 0u; fits && (at < args->count); at += 1u)
    {
        if (args->settings[at].value == NULL)
        {
            continue;
        }
        char entry[CLI_PATH_MAX_LEN * 8u];
        VerbaLinea line = {entry, sizeof(entry), 0ull};
        mmgr_line_put(&line, args->settings[at].name);
        mmgr_line_ch(&line, '=');
        mmgr_line_put(&line, args->settings[at].value);
        fits = mmgr_line_finish(&line) != 0ull;
        // the room left is below what an int counts
        const int written =
            fits ? MultiByteToWideChar(CP_UTF8, 0, entry, -1, args->block + used, (int)(args->room - used - 1ull)) : 0;
        fits = written > 0;
        used += fits ? (unsigned long long)written : 0ull;
    }
    args->block[used] = L'\0';
    return fits;
}

// a process started, its standard input a pipe this process feeds where `fed`, else NUL
// The command line, its folder and its environment block a process is started with, made under the lock
static char s_line[CLI_LINE_BYTES];
static wchar_t s_wide[CLI_LINE_BYTES];
static wchar_t s_folder[CLI_PATH_MAX_LEN];
static wchar_t s_block[CLI_ENVIRONMENT_UNITS];
static CliLock s_lock = CLI_LOCK_INIT;

// the command line, the folder and the environment block made, the lock taken: 1, or 0 with why given and the lock
// given back
EMBED_INLINE int cli_spawn_prepare(const WindowsSpawnCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    for (unsigned int at = 0u; args->words[at] != NULL; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? " " : "");
        EMBED_CALL(cli_word_quote, WindowsSpawnCtx, .line = &line, .word = args->words[at]);
    }
    if ((mmgr_line_finish(&line) == 0ull) ||
        (MultiByteToWideChar(CP_UTF8, 0, s_line, -1, s_wide, CLI_LINE_BYTES) == 0) ||
        ((args->folder != NULL) &&
         (MultiByteToWideChar(CP_UTF8, 0, args->folder, -1, s_folder, CLI_PATH_MAX_LEN) == 0)) ||
        !EMBED_CALL(cli_environment, WindowsSpawnCtx, .settings = args->settings, .count = args->count,
                    .block = s_block, .room = CLI_ENVIRONMENT_UNITS))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        mmgr_line_put(args->err_msg, "the command is longer than Windows starts");
        return 0;
    }
    return 1;
}

int cli_spawn_share(const CliWordsErrMsgSettingsCtx *args)
{
    long long *code = args->code;
    if (!EMBED_CALL(cli_spawn_prepare, WindowsSpawnCtx, .words = args->words, .folder = NULL,
                    .settings = args->settings, .count = args->count, .err_msg = args->err_msg))
    {
        return 0;
    }
    STARTUPINFOW startup;
    memor.set(&(MemoriaCfg){.dst = &startup, .val = 0, .bytes = sizeof(startup)});
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process;
    memor.set(&(MemoriaCfg){.dst = &process, .val = 0, .bytes = sizeof(process)});
    const BOOL made =
        CreateProcessW(NULL, s_wide, NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT, s_block, NULL, &startup, &process);
    const DWORD error = GetLastError();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (!made)
    {
        mmgr_line_put(args->err_msg, "Windows reported ");
        mmgr_line_decimal(args->err_msg, error, 1u);
        return 0;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 0u;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    // an exit code read as the signed number a process returns
    *code = (long long)(int)exit_code;
    return 1;
}

// the priority class a process of `priority` is made in
EMBED_INLINE DWORD cli_spawn_class(const WindowsSpawnCtx *args)
{
    return (args->priority == CLI_PRIORITY_RUN)     ? ABOVE_NORMAL_PRIORITY_CLASS
           : (args->priority == CLI_PRIORITY_GUEST) ? BELOW_NORMAL_PRIORITY_CLASS
           : (args->priority == CLI_PRIORITY_IDLE)  ? IDLE_PRIORITY_CLASS
                                                    : 0u;
}

static int cli_spawn_with(const WindowsSpawnCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
    if (!cli_spawn_prepare(args))
    {
        return 0;
    }
    SECURITY_ATTRIBUTES inherited = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE out_read = NULL;
    HANDLE out_write = NULL;
    HANDLE err_read = NULL;
    HANDLE err_write = NULL;
    HANDLE in_write = NULL;
    HANDLE input = NULL;
    const int input_made =
        args->fed ? CreatePipe(&input, &in_write, &inherited, 0u)
                  : ((input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE);
    const int piped = input_made && CreatePipe(&out_read, &out_write, &inherited, 0u) &&
                      CreatePipe(&err_read, &err_write, &inherited, 0u);
    if (piped)
    {
        // the ends this process reads and writes are not handed to the child
        SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0u);
        SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0u);
        if (in_write != NULL)
        {
            SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0u);
        }
    }
    STARTUPINFOW startup;
    memor.set(&(MemoriaCfg){.dst = &startup, .val = 0, .bytes = sizeof(startup)});
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = out_write;
    startup.hStdError = err_write;
    PROCESS_INFORMATION process;
    memor.set(&(MemoriaCfg){.dst = &process, .val = 0, .bytes = sizeof(process)});
    const DWORD flags = CREATE_UNICODE_ENVIRONMENT | cli_spawn_class(args) |
                        (args->detached ? (CREATE_NO_WINDOW | CREATE_SUSPENDED) : 0u);
    const BOOL made = piped && CreateProcessW(NULL, s_wide, NULL, NULL, TRUE, flags, s_block,
                                              (args->folder != NULL) ? s_folder : NULL, &startup, &process);
    const DWORD error = GetLastError();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const HANDLE closed = (at == 0u) ? input : (at == 1u) ? out_write : err_write;
        if ((closed != NULL) && (closed != INVALID_HANDLE_VALUE))
        {
            CloseHandle(closed);
        }
    }
    if (!made)
    {
        if (out_read != NULL)
        {
            CloseHandle(out_read);
        }
        if (err_read != NULL)
        {
            CloseHandle(err_read);
        }
        if (in_write != NULL)
        {
            CloseHandle(in_write);
        }
        mmgr_line_put(args->err_msg, "Windows reported ");
        mmgr_line_decimal(args->err_msg, error, 1u);
        return 0;
    }
    if (args->detached)
    {
        // a job which lets nothing break away and ends every process in it as its last handle closes, which orior's own
        // end closes with it, so a child outlives orior in no outer job; a job the child does not join is closed
        HANDLE job = CreateJobObjectW(NULL, NULL);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
        memor.set(&(MemoriaCfg){.dst = &limits, .val = 0, .bytes = sizeof(limits)});
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if ((job != NULL) &&
            (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
             !AssignProcessToJobObject(job, process.hProcess)))
        {
            CloseHandle(job);
            job = NULL;
        }
        args->spawned->group = (unsigned long long)(uintptr_t)job;
        ResumeThread(process.hThread);
    }
    CloseHandle(process.hThread);
    // a handle is a pointer's width, which an unsigned long long stores
    args->spawned->process = (unsigned long long)(uintptr_t)process.hProcess;
    args->spawned->id = process.dwProcessId;
    args->spawned->out = (unsigned long long)(uintptr_t)out_read;
    args->spawned->err = (unsigned long long)(uintptr_t)err_read;
    args->spawned->in = (unsigned long long)(uintptr_t)in_write;
    return 1;
}

int cli_spawn(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(cli_spawn_with, WindowsSpawnCtx, .priority = CLI_PRIORITY_WORK, .spawned = args->spawned,
                      .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                      .detached = args->detached, .fed = 0, .err_msg = args->err_msg);
}

int cli_spawn_as(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(cli_spawn_with, WindowsSpawnCtx, .priority = args->priority, .spawned = args->spawned,
                      .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                      .detached = args->detached, .fed = 0, .err_msg = args->err_msg);
}

int cli_spawn_feed(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(cli_spawn_with, WindowsSpawnCtx, .priority = CLI_PRIORITY_WORK, .spawned = args->spawned,
                      .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                      .detached = args->detached, .fed = 1, .err_msg = args->err_msg);
}

int cli_spawn_fed_as(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(cli_spawn_with, WindowsSpawnCtx, .priority = args->priority, .spawned = args->spawned,
                      .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                      .detached = args->detached, .fed = 1, .err_msg = args->err_msg);
}

// the bytes a pseudo-console's attribute list is made in, more than the system asks for one attribute
#define CLI_TERMINAL_ATTRIBUTES 256u

// a terminal's columns or rows as the console counts them, two at the fewest
EMBED_INLINE SHORT cli_terminal_extent(const WindowsSpawnCtx *args)
{
    // a count saved between 2 and the console's most is what a SHORT stores
    return (SHORT)((args->count < 2u) ? 2u : ((args->count > 0x7fffu) ? 0x7fffu : args->count));
}

int cli_spawn_terminal(const CliWordsErrMsgSpawnedCtx *args)
{
    // the attribute list's room, aligned for the pointers the system writes into it
    static unsigned long long s_attributes[CLI_TERMINAL_ATTRIBUTES / sizeof(unsigned long long)];
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
    if (!EMBED_CALL(cli_spawn_prepare, WindowsSpawnCtx, .words = args->words, .folder = args->folder,
                    .settings = args->settings, .count = args->count, .err_msg = args->err_msg))
    {
        return 0;
    }
    HANDLE in_read = NULL;
    HANDLE in_write = NULL;
    HANDLE out_read = NULL;
    HANDLE out_write = NULL;
    HPCON console = NULL;
    const COORD size = {EMBED_CALL(cli_terminal_extent, WindowsSpawnCtx, .count = args->cols),
                        EMBED_CALL(cli_terminal_extent, WindowsSpawnCtx, .count = args->rows)};
    int made = CreatePipe(&in_read, &in_write, NULL, 0u) && CreatePipe(&out_read, &out_write, NULL, 0u) &&
               SUCCEEDED(CreatePseudoConsole(size, in_read, out_write, 0u, &console));
    SIZE_T attributes_size = 0u;
    (void)InitializeProcThreadAttributeList(NULL, 1u, 0u, &attributes_size);
    const LPPROC_THREAD_ATTRIBUTE_LIST attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)(void *)s_attributes;
    const int listed = made && (attributes_size <= sizeof(s_attributes)) &&
                       InitializeProcThreadAttributeList(attributes, 1u, 0u, &attributes_size);
    made = listed && UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console,
                                               sizeof(console), NULL, NULL);
    STARTUPINFOEXW startup;
    memor.set(&(MemoriaCfg){.dst = &startup, .val = 0, .bytes = sizeof(startup)});
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes;
    // the shell's standard handles named as none, so it takes the pseudo-console's: a child named none of its own takes
    // its parent's where those are redirected, and writes past the terminal
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = INVALID_HANDLE_VALUE;
    startup.StartupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    startup.StartupInfo.hStdError = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION process;
    memor.set(&(MemoriaCfg){.dst = &process, .val = 0, .bytes = sizeof(process)});
    made = made &&
           CreateProcessW(NULL, s_wide, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
                          s_block, (args->folder != NULL) ? s_folder : NULL, &startup.StartupInfo, &process);
    const DWORD error = GetLastError();
    if (listed)
    {
        DeleteProcThreadAttributeList(attributes);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    // the ends the pseudo-console reads and writes are its own once it is made
    const HANDLE console_ends[2] = {in_read, out_write};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (console_ends[at] != NULL)
        {
            CloseHandle(console_ends[at]);
        }
    }
    if (!made)
    {
        const HANDLE own_ends[2] = {in_write, out_read};
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            if (own_ends[at] != NULL)
            {
                CloseHandle(own_ends[at]);
            }
        }
        if (console != NULL)
        {
            ClosePseudoConsole(console);
        }
        mmgr_line_put(args->err_msg, "Windows reported ");
        mmgr_line_decimal(args->err_msg, error, 1u);
        return 0;
    }
    CloseHandle(process.hThread);
    // a handle is a pointer's width, which an unsigned long long stores
    args->spawned->process = (unsigned long long)(uintptr_t)process.hProcess;
    args->spawned->id = process.dwProcessId;
    args->spawned->out = (unsigned long long)(uintptr_t)out_read;
    args->spawned->in = (unsigned long long)(uintptr_t)in_write;
    args->spawned->group = (unsigned long long)(uintptr_t)console;
    return 1;
}

int cli_spawn_terminal_resize(const CliColsCtx *args)
{
    const COORD size = {EMBED_CALL(cli_terminal_extent, WindowsSpawnCtx, .count = args->cols),
                        EMBED_CALL(cli_terminal_extent, WindowsSpawnCtx, .count = args->rows)};
    return (args->spawned_read->group != 0ull) &&
           SUCCEEDED(ResizePseudoConsole((HPCON)(uintptr_t)args->spawned_read->group, size));
}

void cli_spawn_terminal_hung_up(const CliColsCtx *args)
{
    TerminateProcess((HANDLE)(uintptr_t)args->spawned_read->process, 1u);
}

void cli_spawn_terminal_close(const CliSpawnedCtx *args)
{
    if (args->spawned->group != 0ull)
    {
        ClosePseudoConsole((HPCON)(uintptr_t)args->spawned->group);
    }
    args->spawned->group = 0ull;
}

int cli_spawn_write(const CliPipeHandleCtx *args)
{
    unsigned long long size = args->size;
    const unsigned char *at = (const unsigned char *)args->bytes;
    while ((size > 0ull) && (args->pipe_handle != 0ull))
    {
        DWORD written = 0u;
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD chunk = (DWORD)((size < (1ull << 30u)) ? size : (1ull << 30u));
        if (!WriteFile((HANDLE)(uintptr_t)args->pipe_handle, at, chunk, &written, NULL) || (written == 0u))
        {
            return 0;
        }
        at += written;
        size -= written;
    }
    return size == 0ull;
}

void cli_spawn_input_close(const CliSpawnedCtx *args)
{
    if (args->spawned->in != 0ull)
    {
        CloseHandle((HANDLE)(uintptr_t)args->spawned->in);
        args->spawned->in = 0ull;
    }
}

unsigned long long cli_spawn_read(const CliRoomPipeHandleCtx *args)
{
    DWORD read = 0u;
    // a piece is at most a gibibyte, which a DWORD counts
    const DWORD chunk = (DWORD)((args->room < (1ull << 30u)) ? args->room : (1ull << 30u));
    if ((args->pipe_handle == 0ull) || !ReadFile((HANDLE)(uintptr_t)args->pipe_handle, args->piece, chunk, &read, NULL))
    {
        return 0ull;
    }
    return read;
}

long long cli_spawn_wait(const CliSpawnedCtx *args)
{
    const HANDLE process = (HANDLE)(uintptr_t)args->spawned->process;
    WaitForSingleObject(process, INFINITE);
    DWORD code = 0u;
    GetExitCodeProcess(process, &code);
    // an exit code read as the signed number a process returns
    return (long long)(int)code;
}

int cli_spawn_poll(const CliSpawnedCtx *args)
{
    long long *code = args->code;
    const HANDLE process = (HANDLE)(uintptr_t)args->spawned->process;
    if (WaitForSingleObject(process, 0u) != WAIT_OBJECT_0)
    {
        return 0;
    }
    DWORD exit_code = 0u;
    GetExitCodeProcess(process, &exit_code);
    // an exit code read as the signed number a process returns
    *code = (long long)(int)exit_code;
    return 1;
}

void cli_spawn_group_end(const CliGroupCtx *args)
{
    if (args->group != 0ull)
    {
        TerminateJobObject((HANDLE)(uintptr_t)args->group, 1u);
    }
}

void cli_spawn_close(const CliSpawnedCtx *args)
{
    const unsigned long long handles[5] = {args->spawned->process, args->spawned->out, args->spawned->err,
                                           args->spawned->group, args->spawned->in};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        if (handles[at] != 0ull)
        {
            CloseHandle((HANDLE)(uintptr_t)handles[at]);
        }
    }
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
}

void cli_spawn_kill(const CliColsCtx *args)
{
    TerminateProcess((HANDLE)(uintptr_t)args->spawned_read->process, 1u);
}

int cli_spawn_interrupt(const CliColsCtx *args)
{
    (void)args;
    return 0;
}

void cli_spawn_tree_end(const CliIdCtx *args)
{
    char number[24];
    VerbaLinea line = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&line, args->id_u64, 1u);
    mmgr_line_finish(&line);
    const char *const words[6] = {"taskkill", "/T", "/F", "/PID", number, NULL};
    CliSpawned killer;
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ull};
    if (EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &killer, .words = words,
                   .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &killer);
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &killer);
    }
}

unsigned int cli_path_folders(const CliRoomFoldersCtx *args)
{
    unsigned int room = args->room_u32;
    static char s_path[QUERY_ENVIRONMENT_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH", .value_out = s_path,
                    .room = sizeof(s_path)))
    {
        return 0u;
    }
    unsigned int count = 0u;
    const char *from = s_path;
    for (const char *at = s_path;; at += 1)
    {
        if ((at[0] == ';') || (at[0] == '\0'))
        {
            if ((at > from) && (count < room))
            {
                VerbaLinea line = {args->folders[count], CLI_PATH_MAX_LEN, 0ull};
                mmgr_line_put_n(&line, from, (unsigned long long)(at - from));
                count += (mmgr_line_finish(&line) != 0ull) ? 1u : 0u;
            }
            if (at[0] == '\0')
            {
                break;
            }
            from = at + 1;
        }
    }
    return count;
}

void cli_path_join(const CliPathCountSizeCtx *args)
{
    VerbaLinea line = {args->path_out, args->size, 0ull};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? ";" : "");
        mmgr_line_put(&line, args->folders_callback[at]);
    }
    mmgr_line_finish(&line);
}
#endif
