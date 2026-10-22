// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_spawn.c: a run's process started on Windows: its environment block made from this process's and the run's
// settings, its output and errors piped back apart, and, detached, started suspended with no console, made the head of
// a job of its own and let run, so ending the job ends every process it starts
#include "../command_line/cli_internal.h"
#include "../text/text.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the most bytes of a command line and of an environment block
#define CLI_LINE_BYTES 32768u

#define CLI_ENVIRONMENT_UNITS (1u << 17u)

// `word` added to `line` quoted as the C runtime splits it back: in quotes where it is empty or holds a space, a tab
// or a quote, the backslashes before a quote and before the closing one doubled
static void cli_word_quoted(CliLine *line, const char *word)
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

// whether the entry `name=value` at `entry` is named `name`, whatever its case, as Windows names its variables
static int cli_named(const wchar_t *entry, const wchar_t *name)
{
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
static int cli_environment(const CliSetting *settings, unsigned int count, wchar_t *block, unsigned long long room)
{
    static wchar_t s_names[64][CLI_NAME_LONGEST];
    if (count > 64u)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (MultiByteToWideChar(CP_UTF8, 0, settings[at].name, -1, s_names[at], CLI_NAME_LONGEST) == 0)
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
        for (unsigned int at = 0u; !named && (at < count); at += 1u)
        {
            named = cli_named(entry, s_names[at]);
        }
        const unsigned long long length = wcslen(entry) + 1ull;
        fits = (used + length + 1ull) < room;
        if (!named && fits)
        {
            memor.cpy(&(MemoriaCfg){.dst = block + used, .src = entry, .bytes = length * sizeof(wchar_t)});
            used += length;
        }
    }
    FreeEnvironmentStringsW(own);
    for (unsigned int at = 0u; fits && (at < count); at += 1u)
    {
        if (settings[at].value == NULL)
        {
            continue;
        }
        char entry[CLI_PATH_LONGEST * 8u];
        CliLine line = {entry, sizeof(entry), 0ull};
        cli_line_text(&line, settings[at].name);
        cli_line_character(&line, '=');
        cli_line_text(&line, settings[at].value);
        fits = cli_line_closed(&line) != 0ull;
        // the room left is below what an int counts
        const int written =
            fits ? MultiByteToWideChar(CP_UTF8, 0, entry, -1, block + used, (int)(room - used - 1ull)) : 0;
        fits = written > 0;
        used += fits ? (unsigned long long)written : 0ull;
    }
    block[used] = L'\0';
    return fits;
}

// a process started, its standard input a pipe this process feeds where `fed`, else NUL
// The command line, its folder and its environment block a process is started with, made under the lock
static char s_line[CLI_LINE_BYTES];
static wchar_t s_wide[CLI_LINE_BYTES];
static wchar_t s_folder[CLI_PATH_LONGEST];
static wchar_t s_block[CLI_ENVIRONMENT_UNITS];
static CliLock s_lock = CLI_LOCK_INIT;

// the command line, the folder and the environment block made, the lock taken: 1, or 0 with why said and the lock
// given back
static int cli_spawn_prepared(const char *const *words, const char *folder, const CliSetting *settings,
                              unsigned int count, CliLine *said)
{
    cli_lock_take(&s_lock);
    CliLine line = {s_line, sizeof(s_line), 0ull};
    for (unsigned int at = 0u; words[at] != NULL; at += 1u)
    {
        cli_line_text(&line, (at != 0u) ? " " : "");
        cli_word_quoted(&line, words[at]);
    }
    if ((cli_line_closed(&line) == 0ull) ||
        (MultiByteToWideChar(CP_UTF8, 0, s_line, -1, s_wide, CLI_LINE_BYTES) == 0) ||
        ((folder != NULL) && (MultiByteToWideChar(CP_UTF8, 0, folder, -1, s_folder, CLI_PATH_LONGEST) == 0)) ||
        !cli_environment(settings, count, s_block, CLI_ENVIRONMENT_UNITS))
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "the command is longer than Windows starts");
        return 0;
    }
    return 1;
}

int cli_spawn_shared(const char *const *words, const CliSetting *settings, unsigned int count, long long *code,
                     CliLine *said)
{
    if (!cli_spawn_prepared(words, NULL, settings, count, said))
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
    cli_lock_give(&s_lock);
    if (!made)
    {
        cli_line_text(said, "Windows said ");
        cli_line_decimal(said, error, 1u);
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
static DWORD cli_spawn_class(unsigned int priority)
{
    return (priority == CLI_PRIORITY_RUN)     ? ABOVE_NORMAL_PRIORITY_CLASS
           : (priority == CLI_PRIORITY_GUEST) ? BELOW_NORMAL_PRIORITY_CLASS
           : (priority == CLI_PRIORITY_IDLE)  ? IDLE_PRIORITY_CLASS
                                              : 0u;
}

static int cli_spawn_with(unsigned int priority, CliSpawned *spawned, const char *const *words, const char *folder,
                          const CliSetting *settings, unsigned int count, int detached, int fed, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = spawned, .val = 0, .bytes = sizeof(*spawned)});
    if (!cli_spawn_prepared(words, folder, settings, count, said))
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
        fed ? CreatePipe(&input, &in_write, &inherited, 0u)
            : ((input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE);
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
    const DWORD flags = CREATE_UNICODE_ENVIRONMENT | cli_spawn_class(priority) |
                        (detached ? (CREATE_NO_WINDOW | CREATE_SUSPENDED) : 0u);
    const BOOL made = piped && CreateProcessW(NULL, s_wide, NULL, NULL, TRUE, flags, s_block,
                                              (folder != NULL) ? s_folder : NULL, &startup, &process);
    const DWORD error = GetLastError();
    cli_lock_give(&s_lock);
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
        cli_line_text(said, "Windows said ");
        cli_line_decimal(said, error, 1u);
        return 0;
    }
    if (detached)
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
        spawned->group = (unsigned long long)(uintptr_t)job;
        ResumeThread(process.hThread);
    }
    CloseHandle(process.hThread);
    // a handle is a pointer's width, which an unsigned long long holds
    spawned->process = (unsigned long long)(uintptr_t)process.hProcess;
    spawned->id = process.dwProcessId;
    spawned->out = (unsigned long long)(uintptr_t)out_read;
    spawned->err = (unsigned long long)(uintptr_t)err_read;
    spawned->in = (unsigned long long)(uintptr_t)in_write;
    return 1;
}

int cli_spawn(CliSpawned *spawned, const char *const *words, const char *folder, const CliSetting *settings,
              unsigned int count, int detached, CliLine *said)
{
    return cli_spawn_with(CLI_PRIORITY_WORK, spawned, words, folder, settings, count, detached, 0, said);
}

int cli_spawn_as(unsigned int priority, CliSpawned *spawned, const char *const *words, const char *folder,
                 const CliSetting *settings, unsigned int count, int detached, CliLine *said)
{
    return cli_spawn_with(priority, spawned, words, folder, settings, count, detached, 0, said);
}

int cli_spawn_fed(CliSpawned *spawned, const char *const *words, const char *folder, const CliSetting *settings,
                  unsigned int count, int detached, CliLine *said)
{
    return cli_spawn_with(CLI_PRIORITY_WORK, spawned, words, folder, settings, count, detached, 1, said);
}

int cli_spawn_fed_as(unsigned int priority, CliSpawned *spawned, const char *const *words, const char *folder,
                     const CliSetting *settings, unsigned int count, int detached, CliLine *said)
{
    return cli_spawn_with(priority, spawned, words, folder, settings, count, detached, 1, said);
}

int cli_spawn_written(unsigned long long pipe, const void *bytes, unsigned long long size)
{
    const unsigned char *at = (const unsigned char *)bytes;
    while ((size > 0ull) && (pipe != 0ull))
    {
        DWORD written = 0u;
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD asked = (DWORD)((size < (1ull << 30u)) ? size : (1ull << 30u));
        if (!WriteFile((HANDLE)(uintptr_t)pipe, at, asked, &written, NULL) || (written == 0u))
        {
            return 0;
        }
        at += written;
        size -= written;
    }
    return size == 0ull;
}

void cli_spawn_input_closed(CliSpawned *spawned)
{
    if (spawned->in != 0ull)
    {
        CloseHandle((HANDLE)(uintptr_t)spawned->in);
        spawned->in = 0ull;
    }
}

unsigned long long cli_spawn_read(unsigned long long pipe, char *piece, unsigned long long room)
{
    DWORD read = 0u;
    // a piece is at most a gibibyte, which a DWORD counts
    const DWORD asked = (DWORD)((room < (1ull << 30u)) ? room : (1ull << 30u));
    if ((pipe == 0ull) || !ReadFile((HANDLE)(uintptr_t)pipe, piece, asked, &read, NULL))
    {
        return 0ull;
    }
    return read;
}

long long cli_spawn_wait(CliSpawned *spawned)
{
    const HANDLE process = (HANDLE)(uintptr_t)spawned->process;
    WaitForSingleObject(process, INFINITE);
    DWORD code = 0u;
    GetExitCodeProcess(process, &code);
    // an exit code read as the signed number a process returns
    return (long long)(int)code;
}

int cli_spawn_poll(CliSpawned *spawned, long long *code)
{
    const HANDLE process = (HANDLE)(uintptr_t)spawned->process;
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

void cli_spawn_group_end(unsigned long long group)
{
    if (group != 0ull)
    {
        TerminateJobObject((HANDLE)(uintptr_t)group, 1u);
    }
}

void cli_spawn_closed(CliSpawned *spawned)
{
    const unsigned long long handles[5] = {spawned->process, spawned->out, spawned->err, spawned->group, spawned->in};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        if (handles[at] != 0ull)
        {
            CloseHandle((HANDLE)(uintptr_t)handles[at]);
        }
    }
    memor.set(&(MemoriaCfg){.dst = spawned, .val = 0, .bytes = sizeof(*spawned)});
}

void cli_spawn_killed(const CliSpawned *spawned)
{
    TerminateProcess((HANDLE)(uintptr_t)spawned->process, 1u);
}

int cli_spawn_interrupted(const CliSpawned *spawned)
{
    (void)spawned;
    return 0;
}

void cli_spawn_tree_end(unsigned long long id)
{
    char number[24];
    CliLine line = {number, sizeof(number), 0ull};
    cli_line_decimal(&line, id, 1u);
    cli_line_closed(&line);
    const char *const words[6] = {"taskkill", "/T", "/F", "/PID", number, NULL};
    CliSpawned killer;
    char unsaid[CLI_PATH_LONGEST];
    CliLine said = {unsaid, sizeof(unsaid), 0ull};
    if (cli_spawn(&killer, words, NULL, NULL, 0u, 1, &said))
    {
        cli_spawn_wait(&killer);
        cli_spawn_closed(&killer);
    }
}

unsigned int cli_path_folders(char (*folders)[CLI_PATH_LONGEST], unsigned int room)
{
    static char s_path[QUERY_ENVIRONMENT_LONGEST];
    if (!query_environment_read("PATH", s_path, sizeof(s_path)))
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
                CliLine line = {folders[count], CLI_PATH_LONGEST, 0ull};
                cli_line_counted(&line, from, (unsigned long long)(at - from));
                count += (cli_line_closed(&line) != 0ull) ? 1u : 0u;
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

void cli_path_joined(const char (*folders)[CLI_PATH_LONGEST], unsigned int count, char *path, unsigned long long size)
{
    CliLine line = {path, size, 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&line, (at != 0u) ? ";" : "");
        cli_line_text(&line, folders[at]);
    }
    cli_line_closed(&line);
}
#endif
