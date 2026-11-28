// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_windows_programs.c: a program started and held on Windows. Its environment block comes from this process's
// and the start's settings, and its output and errors are piped back apart. A detached program starts with no console
// inside a job of its own through the job list attribute, and so the end of the job ends every process that it
// starts. A terminal's shell starts in a pseudo-console of its own, its output read and its keys written through the
// console's pipes. Each start works in a cell of its own, and its child inherits only the handles that the start lists.
// A job holds the processor mask of a program and counts its processor time, and a suspended program waits on its
// first thread until it is resumed
#include "../platform_internal.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include "platform_windows.h"

// the max bytes of a command line and the max units of an environment block
#define PLATFORM_LINE_BYTES 32768u
#define PLATFORM_ENVIRONMENT_UNITS (1u << 17u)

// the max settings that a start names, and the units of a setting's name. The bytes of one entry, and the bytes of a
// start's attribute list
#define PLATFORM_SETTINGS 64u
#define PLATFORM_SETTING_NAME_UNITS 256u
#define PLATFORM_ENTRY_BYTES (8u * PLATFORM_PATH_BYTES)
#define PLATFORM_ATTRIBUTES 256u

// The work of one start: its command line, folder, environment block, the settings' names, its attribute list and the
// handles its child inherits. Each start takes a cell of its own, and so two starts on two threads share nothing
typedef struct
{
    char line[PLATFORM_LINE_BYTES];
    wchar_t wide[PLATFORM_LINE_BYTES];
    wchar_t folder[PLATFORM_WIDE_PATH];
    wchar_t block[PLATFORM_ENVIRONMENT_UNITS];
    wchar_t names[PLATFORM_SETTINGS][PLATFORM_SETTING_NAME_UNITS];
    wchar_t search[PLATFORM_LINE_BYTES];
    wchar_t found[PLATFORM_WIDE_PATH];
    char program[PLATFORM_PATH_BYTES];
    unsigned long long attributes[PLATFORM_ATTRIBUTES / sizeof(unsigned long long)];
    HANDLE inherited[4];
} PlatformStartCell;

// the UTF-8 text `text` into `wide` of `capacity` units as UTF-16, a byte that is no UTF-8 the replacement character:
// 1, or 0 where it does not fit. A command line and the environment are texts, and no byte of them refuses a start
static int platform_text_wide(const char *text, wchar_t *wide, unsigned int capacity)
{
    // a capacity of a buffer of this file is below what an int counts
    return MultiByteToWideChar(CP_UTF8, 0u, text, -1, wide, (int)capacity) != 0;
}

// `word` added to `line` quoted as the C runtime splits it back: in quotes where it is empty or holds a space, a tab or
// a quote. The line doubles the backslashes before a quote and before the closing quote
static void platform_word_quoted(VerbaLinea *line, const char *word)
{
    const size_t whole = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const int quoted = (whole == 0u) || (EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = word, .cap = whole,
                                                    .other = " \t\n\v\"") != whole);
    if (!quoted)
    {
        mmgr_line_put(line, word);
        return;
    }
    mmgr_line_ch(line, '"');
    size_t slashes = 0u;
    for (size_t at = 0u; at < whole;)
    {
        // the run up to the next backslash or quote, then the run of backslashes from there
        const size_t stop =
            at + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = word + at, .cap = whole - at, .other = "\\\"");
        mmgr_line_put_n(line, word + at, stop - at);
        const size_t run_end =
            stop + EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = word + stop, .cap = whole - stop, .other = "\\");
        mmgr_line_put_n(line, word + stop, run_end - stop);
        slashes = (run_end == whole) ? (run_end - stop) : 0u;
        at = run_end;
        if ((run_end < whole) && (word[run_end] == '"'))
        {
            for (size_t added = stop; added <= run_end; added += 1u)
            {
                mmgr_line_ch(line, '\\');
            }
            mmgr_line_ch(line, '"');
            at = run_end + 1u;
        }
    }
    for (size_t added = 0u; added < slashes; added += 1u)
    {
        mmgr_line_ch(line, '\\');
    }
    mmgr_line_ch(line, '"');
}

// 1 where the entry `name=value` at `entry` has the name `name`, whatever its case, as Windows names its variables
static int platform_entry_named(const wchar_t *entry, const wchar_t *name)
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

// The environment block of a start: this process's entries less those that the settings name, then each setting with
// a value. 1, or 0 where it does not fit
static int platform_environment_block(PlatformStartCell *cell, const PlatformStart *start)
{
    if (start->setting_count > PLATFORM_SETTINGS)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < start->setting_count; at += 1u)
    {
        if (!platform_text_wide(start->settings[at].name, cell->names[at], PLATFORM_SETTING_NAME_UNITS))
        {
            return 0;
        }
    }
    wchar_t *const own = GetEnvironmentStringsW();
    if (own == NULL)
    {
        return 0;
    }
    unsigned long long used = 0ULL;
    int fits = 1;
    for (const wchar_t *entry = own; fits && (entry[0] != L'\0'); entry += wcslen(entry) + 1u)
    {
        int named = 0;
        for (unsigned int at = 0u; !named && (at < start->setting_count); at += 1u)
        {
            named = platform_entry_named(entry, cell->names[at]);
        }
        const unsigned long long length = wcslen(entry) + 1ULL;
        fits = (used + length + 1ULL) < PLATFORM_ENVIRONMENT_UNITS;
        if (!named && fits)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = cell->block + used, .src = entry,
                       .bytes = (size_t)(length * sizeof(wchar_t)));
            used += length;
        }
    }
    FreeEnvironmentStringsW(own);
    for (unsigned int at = 0u; fits && (at < start->setting_count); at += 1u)
    {
        if (start->settings[at].value == NULL)
        {
            continue;
        }
        char entry[PLATFORM_ENTRY_BYTES];
        VerbaLinea line = mmgr_line_over(entry, sizeof(entry));
        mmgr_line_put(&line, start->settings[at].name);
        mmgr_line_ch(&line, '=');
        mmgr_line_put(&line, start->settings[at].value);
        fits = mmgr_line_finish(&line) != 0u;
        // the units left are below what an unsigned int counts
        fits = fits &&
               platform_text_wide(entry, cell->block + used, (unsigned int)(PLATFORM_ENVIRONMENT_UNITS - used - 1ULL));
        used += fits ? (unsigned long long)(wcslen(cell->block + used) + 1u) : 0ULL;
    }
    cell->block[used] = L'\0';
    return fits;
}

// The program of `start` found along PATH, as the shell that started this process finds it, into the cell's `program`.
// It answers the program found, or the program as named where it names a folder or PATH does not hold it.
// CreateProcess alone looks in the folders of the system first, where "bash" is WSL's
static const char *platform_program_found(PlatformStartCell *cell, const char *program)
{
    const size_t named = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = program, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if ((EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = program, .cap = named, .other = "\\/:") != named) ||
        !platform_text_wide(program, cell->wide, PLATFORM_LINE_BYTES))
    {
        return program;
    }
    const DWORD path_units = GetEnvironmentVariableW(L"PATH", cell->search, PLATFORM_LINE_BYTES);
    const DWORD found_units =
        ((path_units != 0u) && (path_units < PLATFORM_LINE_BYTES))
            ? SearchPathW(cell->search, cell->wide, L".exe", PLATFORM_WIDE_PATH, cell->found, NULL)
            : 0u;
    const int found = (found_units != 0u) && (found_units < PLATFORM_WIDE_PATH) &&
                      platform_narrow(cell->found, cell->program, sizeof(cell->program));
    return found ? cell->program : program;
}

// A start's cell taken, its command line, folder and environment block made: the cell, or NULL with why given and no
// cell kept
static PlatformStartCell *platform_start_prepared(const PlatformStart *start)
{
    PlatformStartCell *const cell = (PlatformStartCell *)platform_cell_taken(
        sizeof(PlatformStartCell), (unsigned int)platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (cell == NULL)
    {
        mmgr_line_put(start->err_msg, "no memory is left to start the command");
        return NULL;
    }
    const char *const program = start->found_on_path ? platform_program_found(cell, start->words[0]) : start->words[0];
    VerbaLinea line = mmgr_line_over(cell->line, sizeof(cell->line));
    for (unsigned int at = 0u; start->words[at] != NULL; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? " " : "");
        platform_word_quoted(&line, (at != 0u) ? start->words[at] : program);
    }
    if ((mmgr_line_finish(&line) == 0u) || !platform_text_wide(cell->line, cell->wide, PLATFORM_LINE_BYTES) ||
        ((start->folder != NULL) && !platform_wide(start->folder, cell->folder, PLATFORM_WIDE_PATH)) ||
        !platform_environment_block(cell, start))
    {
        mmgr_line_put(start->err_msg, "the command is longer than Windows starts");
        platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
        return NULL;
    }
    return cell;
}

// The cell's attribute list made with the handles that a child inherits and, where given, its job. 1 where the list
// holds both, else 0 with no list kept. The child inherits the listed handles and no other. A start on another thread
// then gives this child none of its pipe ends
static int platform_start_attributes(PlatformStartCell *cell, unsigned int inherited, HANDLE *job)
{
    const DWORD entries = (job != NULL) ? 2u : 1u;
    SIZE_T attributes_size = 0u;
    (void)InitializeProcThreadAttributeList(NULL, entries, 0u, &attributes_size);
    // Explicit cast takes the cell's words as the attribute list that they hold
    const LPPROC_THREAD_ATTRIBUTE_LIST attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)(void *)cell->attributes;
    if ((attributes_size > sizeof(cell->attributes)) ||
        !InitializeProcThreadAttributeList(attributes, entries, 0u, &attributes_size))
    {
        return 0;
    }
    const int listed = UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, cell->inherited,
                                                 inherited * sizeof(HANDLE), NULL, NULL) &&
                       ((job == NULL) || UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_JOB_LIST, job,
                                                                   sizeof(*job), NULL, NULL));
    if (!listed)
    {
        DeleteProcThreadAttributeList(attributes);
    }
    return listed;
}

// the error that Windows gave as the reason of a failed start
static void platform_start_refused(VerbaLinea *err_msg, DWORD error)
{
    mmgr_line_put(err_msg, "Windows reported ");
    mmgr_line_decimal(err_msg, error, 1u);
}

int platform_program_shared(const PlatformStart *start, long long *code)
{
    PlatformStartCell *const cell = platform_start_prepared(start);
    if (cell == NULL)
    {
        return 0;
    }
    const HANDLE standard[3] = {GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE),
                                GetStdHandle(STD_ERROR_HANDLE)};
    // The child inherits this process's standard handles and no other. The loop makes each one inheritable and lists
    // it once
    unsigned int inherited = 0u;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        int known = (standard[at] == NULL) || (standard[at] == INVALID_HANDLE_VALUE);
        for (unsigned int seen = 0u; !known && (seen < inherited); seen += 1u)
        {
            known = cell->inherited[seen] == standard[at];
        }
        if (!known && SetHandleInformation(standard[at], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
        {
            cell->inherited[inherited] = standard[at];
            inherited += 1u;
        }
    }
    const int listed = (inherited != 0u) && platform_start_attributes(cell, inherited, NULL);
    // Explicit cast takes the cell's words as the attribute list that they hold
    const LPPROC_THREAD_ATTRIBUTE_LIST attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)(void *)cell->attributes;
    STARTUPINFOEXW startup;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &startup, .val = 0, .bytes = sizeof(startup));
    startup.StartupInfo.cb = listed ? sizeof(startup) : sizeof(startup.StartupInfo);
    startup.lpAttributeList = listed ? attributes : NULL;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = standard[0];
    startup.StartupInfo.hStdOutput = standard[1];
    startup.StartupInfo.hStdError = standard[2];
    PROCESS_INFORMATION process;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &process, .val = 0, .bytes = sizeof(process));
    const DWORD flags = CREATE_UNICODE_ENVIRONMENT | (listed ? EXTENDED_STARTUPINFO_PRESENT : 0u);
    const BOOL made =
        CreateProcessW(NULL, cell->wide, NULL, NULL, listed, flags, cell->block, NULL, &startup.StartupInfo, &process);
    const DWORD error = GetLastError();
    if (listed)
    {
        DeleteProcThreadAttributeList(attributes);
    }
    platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
    if (!made)
    {
        platform_start_refused(start->err_msg, error);
        return 0;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 0u;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    // Explicit casts read an exit code as the signed number that a process returns
    *code = (long long)(int)exit_code;
    return 1;
}

// the priority class that a process of `priority` starts in
static DWORD platform_start_class(unsigned int priority)
{
    static const DWORD s_classes[4] = {0u, ABOVE_NORMAL_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS,
                                       IDLE_PRIORITY_CLASS};
    return s_classes[(priority < 4u) ? priority : PLATFORM_PRIORITY_WORK];
}

// a handle as the integer of a record, and back. A handle is a pointer's width, which an unsigned long long holds
static unsigned long long platform_handle_kept(HANDLE handle)
{
    // Explicit cast carries the handle as the integer that the record holds
    return (unsigned long long)(uintptr_t)handle;
}

static HANDLE platform_handle_of(unsigned long long kept)
{
    // Explicit cast reads the integer of the record back as the handle
    return (HANDLE)(uintptr_t)kept;
}

// every handle of `handles` that is a handle closed
static void platform_handles_closed(const HANDLE *handles, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((handles[at] != NULL) && (handles[at] != INVALID_HANDLE_VALUE))
        {
            CloseHandle(handles[at]);
        }
    }
}

// the null device opened for a child to inherit, to read where `written` is 0 and to write where it is 1. The
// handle, or NULL where the system refuses it
static HANDLE platform_nothing_opened(int written)
{
    SECURITY_ATTRIBUTES inherited = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    const HANDLE nothing =
        CreateFileW(L"NUL", written ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return (nothing != INVALID_HANDLE_VALUE) ? nothing : NULL;
}

// A pipe made with its end `kept` for this process and its end `given` for a child to inherit: 1, or 0 with neither
// end made. Where `kept_reads`, this process reads the pipe
static int platform_pipe_made(HANDLE *kept, HANDLE *given, int kept_reads)
{
    SECURITY_ATTRIBUTES inherited = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;
    if (!CreatePipe(&read_end, &write_end, &inherited, 0u))
    {
        return 0;
    }
    *kept = kept_reads ? read_end : write_end;
    *given = kept_reads ? write_end : read_end;
    // the child does not take the end that this process keeps
    SetHandleInformation(*kept, HANDLE_FLAG_INHERIT, 0u);
    return 1;
}

// a standard handle of this process made inheritable, for a child that writes where this process writes. The
// handle, or NULL where this process has none
static HANDLE platform_own_given(DWORD which)
{
    const HANDLE own = GetStdHandle(which);
    const int usable = (own != NULL) && (own != INVALID_HANDLE_VALUE) &&
                       SetHandleInformation(own, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    return usable ? own : NULL;
}

// The ends that a start makes. `given` holds the input, the output and the errors of the child. `kept` holds the
// input, the output and the errors that this process keeps, each NULL for none. `made` is 1 where a given end is
// this process's to close after the start
typedef struct
{
    HANDLE given[3];
    HANDLE kept[3];
    int made[3];
} PlatformEnds;

// the file at `path` made or replaced for a child to inherit and write: the handle, or NULL where the system refuses
// it
static HANDLE platform_output_file_made(const char *path)
{
    wchar_t wide[PLATFORM_WIDE_PATH];
    if ((path == NULL) || !platform_wide(path, wide, PLATFORM_WIDE_PATH))
    {
        return NULL;
    }
    SECURITY_ATTRIBUTES inherited = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    const HANDLE file = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
    return (file != INVALID_HANDLE_VALUE) ? file : NULL;
}

// the ends of `start` made as its output mode asks: 1, or 0 where the system refuses one
static int platform_ends_made(const PlatformStart *start, PlatformEnds *ends)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = ends, .val = 0, .bytes = sizeof(*ends));
    int whole = 1;
    if (start->input_piped)
    {
        whole = platform_pipe_made(&ends->kept[0], &ends->given[0], 0);
        ends->made[0] = whole;
    }
    else if (start->input_own)
    {
        ends->given[0] = platform_own_given(STD_INPUT_HANDLE);
    }
    else
    {
        whole = (ends->given[0] = platform_nothing_opened(0)) != NULL;
        ends->made[0] = whole;
    }
    const unsigned int output = start->output;
    if (output == PLATFORM_OUTPUT_FILE)
    {
        // the errors go to the one handle of the file, which the output's end closes
        whole = whole && ((ends->given[1] = platform_output_file_made(start->output_path)) != NULL);
        ends->made[1] = whole;
        ends->given[2] = ends->given[1];
        return whole;
    }
    if (output == PLATFORM_OUTPUT_OWN)
    {
        ends->given[1] = platform_own_given(STD_OUTPUT_HANDLE);
        ends->given[2] = platform_own_given(STD_ERROR_HANDLE);
        return whole;
    }
    if (output == PLATFORM_OUTPUT_NONE)
    {
        whole = whole && ((ends->given[1] = platform_nothing_opened(1)) != NULL);
        ends->made[1] = whole;
        whole = whole && ((ends->given[2] = platform_nothing_opened(1)) != NULL);
        ends->made[2] = whole;
        return whole;
    }
    whole = whole && platform_pipe_made(&ends->kept[1], &ends->given[1], 1);
    ends->made[1] = whole;
    if (output == PLATFORM_OUTPUT_JOINED)
    {
        // the errors go to the one pipe of the output, which the output's end closes
        ends->given[2] = ends->given[1];
        return whole;
    }
    whole = whole && ((output == PLATFORM_OUTPUT_ALONE) ? ((ends->given[2] = platform_nothing_opened(1)) != NULL)
                                                        : platform_pipe_made(&ends->kept[2], &ends->given[2], 1));
    ends->made[2] = whole;
    return whole;
}

// The handles of `ends` and the one handle that `start` names, each put once in the list of the handles that a child
// inherits. Their count
static unsigned int platform_inherited_listed(PlatformStartCell *cell, const PlatformEnds *ends,
                                              const PlatformStart *start)
{
    const HANDLE named[4] = {ends->given[0], ends->given[1], ends->given[2],
                             (HANDLE)(uintptr_t)start->inherited_handle};
    unsigned int listed = 0u;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        int known = (named[at] == NULL) || (named[at] == INVALID_HANDLE_VALUE);
        for (unsigned int seen = 0u; !known && (seen < listed); seen += 1u)
        {
            known = cell->inherited[seen] == named[at];
        }
        if (!known)
        {
            cell->inherited[listed] = named[at];
            listed += 1u;
        }
    }
    return listed;
}

// the given ends that the start made closed, and where `failed`, the kept ends too
static void platform_ends_closed(const PlatformEnds *ends, int failed)
{
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const int joined = (at == 2u) && (ends->given[2] == ends->given[1]);
        if (ends->made[at] && !joined && (ends->given[at] != NULL))
        {
            CloseHandle(ends->given[at]);
        }
        if (failed && (ends->kept[at] != NULL))
        {
            CloseHandle(ends->kept[at]);
        }
    }
}

// the stage and the code of a failed start put in the refusal of `start`, where it gives one
static void platform_refusal_given(const PlatformStart *start, unsigned int stage, unsigned long long code)
{
    if (start->refusal != NULL)
    {
        start->refusal->stage = stage;
        start->refusal->code = code;
    }
}

// The job of a start. A detached program heads a job that lets nothing break away. The job ends every process in it
// as its last handle closes, which the end of this process closes. A processor mask is a limit of the job, which counts
// the processor time of every process in it. The job, or NULL where the start asks for none or the system refuses one
static HANDLE platform_job_made(const PlatformStart *start)
{
    const int killed = start->detached && !start->outlives;
    if (start->outlives || (!killed && (start->processor_mask == 0ULL)))
    {
        return NULL;
    }
    const HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job == NULL)
    {
        return NULL;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &limits, .val = 0, .bytes = sizeof(limits));
    limits.BasicLimitInformation.LimitFlags = (killed ? (DWORD)JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE : 0u) |
                                              ((start->processor_mask != 0ULL) ? (DWORD)JOB_OBJECT_LIMIT_AFFINITY : 0u);
    // the mask covers the first 64 processors, which the machine word holds whole
    limits.BasicLimitInformation.Affinity = (ULONG_PTR)start->processor_mask;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
    {
        CloseHandle(job);
        return NULL;
    }
    return job;
}

// One CreateProcessW of the cell's command line with `flags`. The child inherits its own three ends and the one handle
// that the start names, and no other handle. Where the call gives `job`, the child starts inside it through the job
// list attribute. No moment then passes between its start and its place in the job. 1 where the program started, with
// the error of Windows kept for GetLastError where it did not
static int platform_process_created(PlatformStartCell *cell, const PlatformStart *start, const PlatformEnds *ends,
                                    HANDLE *job, DWORD flags, PROCESS_INFORMATION *process)
{
    const unsigned int handles_listed = platform_inherited_listed(cell, ends, start);
    if ((handles_listed == 0u) || !platform_start_attributes(cell, handles_listed, job))
    {
        return 0;
    }
    // Explicit cast takes the cell's words as the attribute list that they hold
    const LPPROC_THREAD_ATTRIBUTE_LIST attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)(void *)cell->attributes;
    STARTUPINFOEXW startup;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &startup, .val = 0, .bytes = sizeof(startup));
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = ends->given[0];
    startup.StartupInfo.hStdOutput = ends->given[1];
    startup.StartupInfo.hStdError = ends->given[2];
    const wchar_t *const folder = (start->folder != NULL) ? cell->folder : NULL;
    const BOOL made =
        CreateProcessW(NULL, cell->wide, NULL, NULL, TRUE, flags, cell->block, folder, &startup.StartupInfo, process);
    const DWORD error = GetLastError();
    DeleteProcThreadAttributeList(attributes);
    SetLastError(error);
    return made != 0;
}

int platform_program_started(const PlatformStart *start, PlatformProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .val = 0, .bytes = sizeof(*program));
    PlatformStartCell *const cell = platform_start_prepared(start);
    if (cell == NULL)
    {
        platform_refusal_given(start, PLATFORM_REFUSED_START, ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    PlatformEnds ends;
    if (!platform_ends_made(start, &ends))
    {
        const DWORD error = GetLastError();
        platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
        platform_ends_closed(&ends, 1);
        platform_refusal_given(start, PLATFORM_REFUSED_ENDS, error);
        platform_start_refused(start->err_msg, error);
        return 0;
    }
    HANDLE job = platform_job_made(start);
    const int windowless = start->detached || start->windowless;
    const DWORD flags = CREATE_UNICODE_ENVIRONMENT | platform_start_class(start->priority) |
                        (windowless ? CREATE_NO_WINDOW : 0u) | (start->suspended ? CREATE_SUSPENDED : 0u) |
                        EXTENDED_STARTUPINFO_PRESENT;
    // a fault in the program ends it with no dialog to wait on. The program inherits the error mode, which the call
    // sets back once it starts
    const UINT mode = start->fault_silent
                          ? SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX)
                          : 0u;
    PROCESS_INFORMATION process;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &process, .val = 0, .bytes = sizeof(process));
    // A program that outlives this process breaks away from its job. A job that lets nothing break away refuses that,
    // and then the program starts inside the job
    int made = start->outlives &&
               platform_process_created(cell, start, &ends, NULL, flags | CREATE_BREAKAWAY_FROM_JOB, &process);
    made = made || platform_process_created(cell, start, &ends, (job != NULL) ? &job : NULL, flags, &process);
    if (!made && (job != NULL) && !start->detached)
    {
        // a job that the system starts no program in leaves the program in none, on its processors all the same
        CloseHandle(job);
        job = NULL;
        made = platform_process_created(cell, start, &ends, NULL, flags, &process);
    }
    const DWORD error = GetLastError();
    if (start->fault_silent)
    {
        SetErrorMode(mode);
    }
    platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
    if ((job != NULL) && !made)
    {
        // a child that did not start leaves the job empty, and the call closes the job
        CloseHandle(job);
        job = NULL;
    }
    platform_ends_closed(&ends, !made);
    if (!made)
    {
        platform_refusal_given(start, PLATFORM_REFUSED_START, error);
        platform_start_refused(start->err_msg, error);
        return 0;
    }
    if ((job == NULL) && (start->processor_mask != 0ULL))
    {
        // the mask covers the first 64 processors, which the machine word holds whole
        SetProcessAffinityMask(process.hProcess, (DWORD_PTR)start->processor_mask);
    }
    if (start->suspended)
    {
        program->held = platform_handle_kept(process.hThread);
    }
    else
    {
        CloseHandle(process.hThread);
    }
    program->group = platform_handle_kept(job);
    program->process = platform_handle_kept(process.hProcess);
    program->id = process.dwProcessId;
    program->in = platform_handle_kept(ends.kept[0]);
    program->out = platform_handle_kept(ends.kept[1]);
    program->err = platform_handle_kept(ends.kept[2]);
    return 1;
}

void platform_program_resumed(PlatformProgram *program)
{
    if (program->held != 0ULL)
    {
        ResumeThread(platform_handle_of(program->held));
        CloseHandle(platform_handle_of(program->held));
        program->held = 0ULL;
    }
}

int platform_program_ended_within(const PlatformProgram *program, unsigned long long microseconds)
{
    // a wait of 49 days or more has no bound, and a shorter one's milliseconds, rounded up, fit a DWORD
    const int bounded = (microseconds / 1000ULL) < (unsigned long long)INFINITE;
    const DWORD wait = bounded ? (DWORD)((microseconds + 999ULL) / 1000ULL) : INFINITE;
    return WaitForSingleObject(platform_handle_of(program->process), wait) != WAIT_TIMEOUT;
}

int platform_program_ending(PlatformProgram *program, PlatformEnding *ending)
{
    const HANDLE process = platform_handle_of(program->process);
    DWORD code = 0u;
    const int read = (WaitForSingleObject(process, INFINITE) == WAIT_OBJECT_0) && GetExitCodeProcess(process, &code);
    program->waited = read ? 1ULL : program->waited;
    ending->signaled = 0;
    ending->code = code;
    return read;
}

void platform_program_group_killed(const PlatformProgram *program)
{
    if (program->group != 0ULL)
    {
        TerminateJobObject(platform_handle_of(program->group), 1u);
    }
    else
    {
        TerminateProcess(platform_handle_of(program->process), 1u);
    }
}

unsigned long long platform_program_tree_stopped(PlatformProgram *program, unsigned long long grace, unsigned int code)
{
    // a console program on Windows has no signal that it must heed: its job, or its process, ends at once
    if (program->group != 0ULL)
    {
        TerminateJobObject(platform_handle_of(program->group), code);
    }
    else
    {
        TerminateProcess(platform_handle_of(program->process), code);
    }
    // a grace's milliseconds fit a DWORD
    WaitForSingleObject(platform_handle_of(program->process), (DWORD)(grace / 1000ULL));
    return 1ULL;
}

int platform_program_processor_time(const PlatformProgram *program, unsigned long long *microseconds)
{
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &accounting, .val = 0, .bytes = sizeof(accounting));
    if ((program->group == 0ULL) ||
        !QueryInformationJobObject(platform_handle_of(program->group), JobObjectBasicAccountingInformation, &accounting,
                                   sizeof(accounting), NULL))
    {
        return 0;
    }
    // the job's user and kernel times are counts of 100 nanosecond ticks, never negative
    const unsigned long long user = (unsigned long long)accounting.TotalUserTime.QuadPart;
    const unsigned long long kernel = (unsigned long long)accounting.TotalKernelTime.QuadPart;
    *microseconds = (user + kernel) / 10ULL;
    return 1;
}

unsigned long long platform_program_read_ready(unsigned long long handle, void *piece, unsigned long long capacity,
                                               int *ended)
{
    DWORD waiting = 0u;
    *ended = 0;
    if ((handle == 0ULL) || !PeekNamedPipe(platform_handle_of(handle), NULL, 0u, NULL, &waiting, NULL))
    {
        // a pipe whose every writer closed answers the peek with an error, as its end
        *ended = 1;
        return 0ULL;
    }
    if (waiting == 0u)
    {
        return 0ULL;
    }
    // a piece is at max what waits, which a DWORD counts
    const DWORD chunk = (DWORD)((capacity < (unsigned long long)waiting) ? capacity : (unsigned long long)waiting);
    DWORD read = 0u;
    if (!ReadFile(platform_handle_of(handle), piece, chunk, &read, NULL))
    {
        *ended = 1;
        return 0ULL;
    }
    return read;
}

// a terminal's columns or rows as the console counts them, two at the fewest
static SHORT platform_terminal_extent(unsigned int count)
{
    // a count kept between 2 and the console's max is what a SHORT holds
    const unsigned int bounded = (count > 0x7fffu) ? 0x7fffu : count;
    return (SHORT)((count < 2u) ? 2u : bounded);
}

int platform_program_terminal(const PlatformStart *start, unsigned int cols, unsigned int rows,
                              PlatformProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .val = 0, .bytes = sizeof(*program));
    PlatformStartCell *const cell = platform_start_prepared(start);
    if (cell == NULL)
    {
        return 0;
    }
    HANDLE in_read = NULL;
    HANDLE in_write = NULL;
    HANDLE out_read = NULL;
    HANDLE out_write = NULL;
    HPCON console = NULL;
    const COORD size = {platform_terminal_extent(cols), platform_terminal_extent(rows)};
    int made = CreatePipe(&in_read, &in_write, NULL, 0u) && CreatePipe(&out_read, &out_write, NULL, 0u) &&
               SUCCEEDED(CreatePseudoConsole(size, in_read, out_write, 0u, &console));
    SIZE_T attributes_size = 0u;
    (void)InitializeProcThreadAttributeList(NULL, 1u, 0u, &attributes_size);
    // Explicit cast takes the cell's words as the attribute list that they hold
    const LPPROC_THREAD_ATTRIBUTE_LIST attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)(void *)cell->attributes;
    const int list_entries = made && (attributes_size <= sizeof(cell->attributes)) &&
                             InitializeProcThreadAttributeList(attributes, 1u, 0u, &attributes_size);
    made = list_entries && UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console,
                                                     sizeof(console), NULL, NULL);
    STARTUPINFOEXW startup;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &startup, .val = 0, .bytes = sizeof(startup));
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes;
    // The startup names none of the shell's standard handles, and the shell then takes the pseudo-console's. A child
    // that names none of its own takes its parent's where its parent redirects those, and writes past the terminal
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = INVALID_HANDLE_VALUE;
    startup.StartupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    startup.StartupInfo.hStdError = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION process;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &process, .val = 0, .bytes = sizeof(process));
    made = made && CreateProcessW(NULL, cell->wide, NULL, NULL, FALSE,
                                  EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, cell->block,
                                  (start->folder != NULL) ? cell->folder : NULL, &startup.StartupInfo, &process);
    const DWORD error = GetLastError();
    if (list_entries)
    {
        DeleteProcThreadAttributeList(attributes);
    }
    platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
    // the ends that the pseudo-console reads and writes are its own once the system makes it
    const HANDLE console_ends[2] = {in_read, out_write};
    platform_handles_closed(console_ends, 2u);
    if (!made)
    {
        const HANDLE own_ends[2] = {in_write, out_read};
        platform_handles_closed(own_ends, 2u);
        if (console != NULL)
        {
            ClosePseudoConsole(console);
        }
        platform_start_refused(start->err_msg, error);
        return 0;
    }
    CloseHandle(process.hThread);
    program->process = platform_handle_kept(process.hProcess);
    program->id = process.dwProcessId;
    program->out = platform_handle_kept(out_read);
    program->in = platform_handle_kept(in_write);
    // Explicit cast carries the pseudo-console's handle as the integer of the record
    program->group = (unsigned long long)(uintptr_t)console;
    return 1;
}

int platform_program_terminal_sized(const PlatformProgram *program, unsigned int cols, unsigned int rows)
{
    const COORD size = {platform_terminal_extent(cols), platform_terminal_extent(rows)};
    // Explicit cast reads the integer of the record back as the pseudo-console
    return (program->group != 0ULL) && SUCCEEDED(ResizePseudoConsole((HPCON)(uintptr_t)program->group, size));
}

void platform_program_terminal_hung_up(const PlatformProgram *program)
{
    TerminateProcess(platform_handle_of(program->process), 1u);
}

void platform_program_terminal_closed(PlatformProgram *program)
{
    if (program->group != 0ULL)
    {
        // Explicit cast reads the integer of the record back as the pseudo-console
        ClosePseudoConsole((HPCON)(uintptr_t)program->group);
    }
    program->group = 0ULL;
}

int platform_program_written(unsigned long long handle, const void *bytes, unsigned long long size)
{
    // Explicit cast reads the source as its bytes
    const unsigned char *at = (const unsigned char *)bytes;
    while ((size > 0ULL) && (handle != 0ULL))
    {
        DWORD written = 0u;
        // a piece is at max a gibibyte, which a DWORD counts
        const DWORD chunk = (DWORD)((size < (1ULL << 30u)) ? size : (1ULL << 30u));
        if (!WriteFile(platform_handle_of(handle), at, chunk, &written, NULL) || (written == 0u))
        {
            return 0;
        }
        at += written;
        size -= written;
    }
    return size == 0ULL;
}

void platform_program_input_closed(PlatformProgram *program)
{
    if (program->in != 0ULL)
    {
        CloseHandle(platform_handle_of(program->in));
        program->in = 0ULL;
    }
}

unsigned long long platform_program_read(unsigned long long handle, void *piece, unsigned long long capacity)
{
    DWORD read = 0u;
    // a piece is at max a gibibyte, which a DWORD counts
    const DWORD chunk = (DWORD)((capacity < (1ULL << 30u)) ? capacity : (1ULL << 30u));
    if ((handle == 0ULL) || !ReadFile(platform_handle_of(handle), piece, chunk, &read, NULL))
    {
        return 0ULL;
    }
    return read;
}

unsigned long long platform_program_waiting(unsigned long long handle)
{
    DWORD waiting = 0u;
    if ((handle == 0ULL) || !PeekNamedPipe(platform_handle_of(handle), NULL, 0u, NULL, &waiting, NULL))
    {
        return 0ULL;
    }
    return waiting;
}

long long platform_program_waited(PlatformProgram *program)
{
    const HANDLE process = platform_handle_of(program->process);
    WaitForSingleObject(process, INFINITE);
    DWORD code = 0u;
    GetExitCodeProcess(process, &code);
    program->waited = 1ULL;
    // Explicit casts read an exit code as the signed number that a process returns
    return (long long)(int)code;
}

int platform_program_polled(PlatformProgram *program, long long *code)
{
    const HANDLE process = platform_handle_of(program->process);
    if (WaitForSingleObject(process, 0u) != WAIT_OBJECT_0)
    {
        return 0;
    }
    DWORD exit_code = 0u;
    GetExitCodeProcess(process, &exit_code);
    program->waited = 1ULL;
    // Explicit casts read an exit code as the signed number that a process returns
    *code = (long long)(int)exit_code;
    return 1;
}

void platform_program_group_ended(unsigned long long group)
{
    if (group != 0ULL)
    {
        TerminateJobObject(platform_handle_of(group), 1u);
    }
}

void platform_program_closed(PlatformProgram *program)
{
    const HANDLE handles[6] = {platform_handle_of(program->process), platform_handle_of(program->out),
                               platform_handle_of(program->err),     platform_handle_of(program->group),
                               platform_handle_of(program->in),      platform_handle_of(program->held)};
    platform_handles_closed(handles, 6u);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .val = 0, .bytes = sizeof(*program));
}

void platform_program_killed(const PlatformProgram *program)
{
    TerminateProcess(platform_handle_of(program->process), 1u);
}

int platform_program_interrupted(const PlatformProgram *program)
{
    (void)program;
    return 0;
}

void platform_program_tree_ended(unsigned long long id)
{
    char number[24];
    VerbaLinea line = mmgr_line_over(number, sizeof(number));
    mmgr_line_decimal(&line, id, 1u);
    (void)mmgr_line_finish(&line);
    const char *const words[6] = {"taskkill", "/T", "/F", "/PID", number, NULL};
    char why[PLATFORM_PATH_BYTES];
    VerbaLinea err_msg = mmgr_line_over(why, sizeof(why));
    const PlatformStart start = {.words = words, .detached = 1, .err_msg = &err_msg};
    PlatformProgram killer;
    if (platform_program_started(&start, &killer))
    {
        (void)platform_program_waited(&killer);
        platform_program_closed(&killer);
    }
}
#endif
