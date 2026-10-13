// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows.c: the probe on Windows: the rule a fault names, the command line, the program found, the pipe drained
// by a thread, and the probe started in a job of its own and run to its end or its limit
#include "interface_internal.h"

#if defined(_WIN32)
// the rule a Windows fault names: an unhandled exception ends its process with the exception's NTSTATUS as its code
static InterfaceFault interface_fault_of(unsigned long long code)
{
    switch (code)
    {
    // an integer division by zero, one that overflows, and the floating-point traps from denormal operand to
    // underflow
    case 0xC0000094ull:
    case 0xC0000095ull:
    case 0xC000008Dull:
    case 0xC000008Eull:
    case 0xC000008Full:
    case 0xC0000090ull:
    case 0xC0000091ull:
    case 0xC0000092ull:
    case 0xC0000093ull:
        return INTERFACE_FAULT_ARITHMETIC;
    // an access violation, and a page the system could not bring in
    case 0xC0000005ull:
    case 0xC0000006ull:
        return INTERFACE_FAULT_ADDRESS;
    // an illegal instruction, and a privileged one
    case 0xC000001Dull:
    case 0xC0000096ull:
        return INTERFACE_FAULT_INSTRUCTION;
    case 0xC00000FDull:
        return INTERFACE_FAULT_STACK;
    case 0x80000003ull:
        return INTERFACE_FAULT_TRAP;
    // the fail-fast a runtime ends itself with, abort among its callers
    case 0xC0000409ull:
        return INTERFACE_FAULT_ABORT;
    default:
        return INTERFACE_FAULT_OTHER;
    }
}

// the command's words as one command line, the program first, each quoted as the C runtime splits them back (as
// tessera_run_windows.c writes a job's)
static char *interface_command_line(const char *program, char *const *words)
{
    size_t capacity = 1u + (2u * strlen(program)) + 3u;
    for (size_t at = 1u; words[at] != NULL; at += 1u)
    {
        capacity += (2u * strlen(words[at])) + 3u;
    }
    char *const line = (char *)malloc(capacity);
    if (line == NULL)
    {
        return NULL;
    }
    size_t length = 0u;
    for (size_t at = 0u; words[at] != NULL; at += 1u)
    {
        const char *const word = (at == 0u) ? program : words[at];
        if (at != 0u)
        {
            line[length] = ' ';
            length += 1u;
        }
        const int quoted = (word[0] == '\0') || (strpbrk(word, " \t\n\v\"") != NULL);
        if (!quoted)
        {
            memcpy(line + length, word, strlen(word));
            length += strlen(word);
            continue;
        }
        line[length] = '"';
        length += 1u;
        size_t slashes = 0u;
        for (const char *walk = word; *walk != '\0'; walk += 1)
        {
            // the backslashes before a quote are doubled, and the quote is escaped by one more
            const size_t doubled = (*walk == '"') ? (slashes + 1u) : 0u;
            for (size_t added = 0u; added < doubled; added += 1u)
            {
                line[length] = '\\';
                length += 1u;
            }
            slashes = (*walk == '\\') ? (slashes + 1u) : 0u;
            line[length] = *walk;
            length += 1u;
        }
        // the backslashes before the closing quote are doubled
        for (size_t added = 0u; added < slashes; added += 1u)
        {
            line[length] = '\\';
            length += 1u;
        }
        line[length] = '"';
        length += 1u;
    }
    line[length] = '\0';
    return line;
}

// a program named with no folder, found along PATH; the program as named where PATH does not hold it
static const char *interface_program_found(const char *program, char *found, size_t capacity)
{
    const char *const path = getenv("PATH");
    if ((strpbrk(program, "\\/:") != NULL) || (path == NULL))
    {
        return program;
    }
    // the capacity is a buffer size this file holds, which a DWORD counts on every Windows target
    const DWORD length = SearchPathA(path, program, ".exe", (DWORD)capacity, found, NULL);
    return ((length != 0u) && (length < capacity)) ? found : program;
}

// the pipe a probe's output comes back through, drained by a thread of the interface's while the probe runs: a
// probe that writes more than the pipe holds never waits on it
typedef struct
{
    HANDLE pipe;
    InterfaceAnswer *answer;
    unsigned long long total;
} InterfacePipe;

static DWORD WINAPI interface_pipe_drained(LPVOID argument)
{
    InterfacePipe *const piped = (InterfacePipe *)argument;
    char piece[INTERFACE_READ_BYTES];
    DWORD read = 0u;
    while (ReadFile(piped->pipe, piece, sizeof(piece), &read, NULL) && (read != 0u))
    {
        interface_output_kept(piped->answer, &piped->total, piece, read);
    }
    return 0u;
}

// the probe started suspended in a job of its own that ends every process in it when the interface lets it go, then run
// to its end or its limit, its output the file or the pipe. 0 where it was waited on to its end; -1 where the output
// file or the pipe could not be made
//
// A started process inherits every inheritable handle its starter holds at that moment, its own and every other
// probe's. Probes started from threads at once would each hold the others' pipes open, and a pipe a probe that has
// ended still holds open in another never reaches its end. So from the moment a probe's inheritable handles are made
// to the moment the interface lets its own copies go, no other probe of this process is started
static SRWLOCK s_interface_starting = SRWLOCK_INIT;

long interface_probe_start(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    SECURITY_ATTRIBUTES inherited;
    memset(&inherited, 0, sizeof(inherited));
    inherited.nLength = sizeof(inherited);
    inherited.bInheritHandle = TRUE;
    HANDLE output = INVALID_HANDLE_VALUE;
    HANDLE pipe_read = NULL;
    AcquireSRWLockExclusive(&s_interface_starting);
    if (probe->output_path != NULL)
    {
        output = CreateFileA(probe->output_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (!INTERFACE_CHECK(output != INVALID_HANDLE_VALUE, probe->output_path, error, ENGINE_ERROR_RESOURCE))
        {
            ReleaseSRWLockExclusive(&s_interface_starting);
            return -1L;
        }
    }
    else
    {
        // the probe is handed the pipe's write end, and the interface keeps the read end to itself
        if (!INTERFACE_CHECK(CreatePipe(&pipe_read, &output, &inherited, 0u), probe, error, ENGINE_ERROR_RESOURCE))
        {
            ReleaseSRWLockExclusive(&s_interface_starting);
            return -1L;
        }
        SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0u);
    }
    const HANDLE input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited, OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL, NULL);
    char found[ENGINE_PATH_CAPACITY];
    char *const line = interface_command_line(interface_program_found(probe->command[0], found, sizeof(found)), probe->command);
    const HANDLE job = CreateJobObjectA(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    memset(&limits, 0, sizeof(limits));
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    const int limited =
        (job != NULL) && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    STARTUPINFOA startup;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = output;
    startup.hStdError = output;
    PROCESS_INFORMATION started;
    memset(&started, 0, sizeof(started));
    // a fault in the probe ends it with no dialog to wait on: the error mode is inherited, and set back once it starts
    const UINT mode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    const unsigned long long began = interface_now();
    const int made = (line != NULL) && CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                                                      NULL, NULL, &startup, &started);
    // the reason a probe did not start, read before any call after can set another
    const DWORD start_error = made ? 0u : GetLastError();
    SetErrorMode(mode);
    free(line);
    CloseHandle(output);
    if (input != INVALID_HANDLE_VALUE)
    {
        CloseHandle(input);
    }
    ReleaseSRWLockExclusive(&s_interface_starting);
    if (!made)
    {
        if (job != NULL)
        {
            CloseHandle(job);
        }
        if (pipe_read != NULL)
        {
            CloseHandle(pipe_read);
        }
        answer->ending = INTERFACE_ENDING_NOT_STARTED;
        answer->code = start_error;
        return 0L;
    }
    // a job the probe cannot join leaves only the probe itself to end at its limit
    const int joined = limited && AssignProcessToJobObject(job, started.hProcess);
    InterfacePipe piped = {pipe_read, answer, 0ull};
    const HANDLE reader = (pipe_read != NULL) ? CreateThread(NULL, 0u, interface_pipe_drained, &piped, 0u, NULL) : NULL;
    if ((pipe_read != NULL) && (reader == NULL))
    {
        // a pipe nothing drains would hold the probe once it fills: the probe is ended before it runs
        TerminateProcess(started.hProcess, 1u);
        CloseHandle(started.hThread);
        CloseHandle(started.hProcess);
        CloseHandle(pipe_read);
        if (job != NULL)
        {
            CloseHandle(job);
        }
        INTERFACE_CHECK(0, probe, error, ENGINE_ERROR_RESOURCE);
        return -1L;
    }
    ResumeThread(started.hThread);
    CloseHandle(started.hThread);
    // a limit's milliseconds, rounded up, fit a DWORD below INFINITE for any limit under 49 days
    const DWORD wait = (probe->limit_microseconds == 0ull)
                           ? INFINITE
                           : (DWORD)((probe->limit_microseconds + (INTERFACE_THOUSAND - 1ull)) / INTERFACE_THOUSAND);
    const int out_of_time = WaitForSingleObject(started.hProcess, wait) == WAIT_TIMEOUT;
    if (out_of_time && joined)
    {
        TerminateJobObject(job, 1u);
    }
    else if (out_of_time)
    {
        TerminateProcess(started.hProcess, 1u);
    }
    WaitForSingleObject(started.hProcess, INFINITE);
    answer->microseconds = interface_now() - began;
    DWORD code = 0u;
    GetExitCodeProcess(started.hProcess, &code);
    CloseHandle(started.hProcess);
    // closing the job ends every process the probe started and left
    if (job != NULL)
    {
        CloseHandle(job);
    }
    // the pipe's reader ends at the pipe's end, once every process that held its write end has ended; one a process
    // outside the job still holds is let go after INTERFACE_DRAIN_MILLISECONDS, its read so far kept
    if (reader != NULL)
    {
        if (WaitForSingleObject(reader, (DWORD)INTERFACE_DRAIN_MILLISECONDS) == WAIT_TIMEOUT)
        {
            CancelSynchronousIo(reader);
            WaitForSingleObject(reader, INFINITE);
        }
        CloseHandle(reader);
        CloseHandle(pipe_read);
        interface_output_ended(answer, piped.total);
    }
    const int faulted = (code & 0x80000000u) != 0u;
    answer->code = code;
    answer->ending = out_of_time ? INTERFACE_ENDING_OUT_OF_TIME : (faulted ? INTERFACE_ENDING_FAULTED : INTERFACE_ENDING_EXITED);
    answer->fault = (answer->ending == INTERFACE_ENDING_FAULTED) ? interface_fault_of(code) : INTERFACE_FAULT_NONE;
    return 0L;
}
#endif
