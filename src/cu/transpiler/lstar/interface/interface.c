// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#if !defined(_WIN32)
// fork, exec, the process group, the clock and nanosleep are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

#define INTERFACE_MILLION 1000000ull
#define INTERFACE_THOUSAND 1000ull
// how often a POSIX interface asks whether its probe has ended
#define INTERFACE_POLL_MICROSECONDS 1000ull
// a probe's output is read back in pieces this long
#define INTERFACE_READ_BYTES 4096u
// the most a pipe's reader is waited on once the probe and every process it started have ended
#define INTERFACE_DRAIN_MILLISECONDS 5000ull

#define INTERFACE_CHECK(condition_, evacaddr_, error_, kind_)                                                               \
    engine_error_check((condition_), (kind_), ENGINE_MODULE_INTERFACE, (unsigned int)__LINE__, (const void *)(evacaddr_),   \
                       (error_))

// `read` bytes of the probe's output at `piece` kept in the answer, as many as its capacity holds past the `*total`
// before them, and counted whole
static void interface_output_kept(InterfaceAnswer *answer, unsigned long long *total, const char *piece,
                                  unsigned long long read)
{
    const unsigned long long kept_capacity =
        (answer->output_capacity != 0ull) ? (answer->output_capacity - 1ull) : 0ull;
    const unsigned long long kept = (*total < kept_capacity) ? (kept_capacity - *total) : 0ull;
    const unsigned long long copied = (kept < read) ? kept : read;
    if ((answer->output != NULL) && (copied != 0ull))
    {
        // a copy is at most one piece, which a size_t holds
        memcpy(answer->output + *total, piece, (size_t)copied);
    }
    *total += read;
}

// the answer's output ended by a zero byte inside its capacity, and its length whole
static void interface_output_ended(InterfaceAnswer *answer, unsigned long long total)
{
    if ((answer->output != NULL) && (answer->output_capacity != 0ull))
    {
        const unsigned long long end =
            (total < (answer->output_capacity - 1ull)) ? total : (answer->output_capacity - 1ull);
        // the end is inside the capacity, which the caller's buffer holds
        answer->output[(size_t)end] = '\0';
    }
    answer->output_bytes = total;
}

static unsigned long long interface_now(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    // a performance counter and its frequency are positive
    const unsigned long long ticks = (unsigned long long)counter.QuadPart;
    // a performance counter and its frequency are positive
    const unsigned long long rate = (unsigned long long)frequency.QuadPart;
    return ((ticks / rate) * INTERFACE_MILLION) + (((ticks % rate) * INTERFACE_MILLION) / rate);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // a monotonic clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * INTERFACE_MILLION) + ((unsigned long long)now.tv_nsec / 1000ull);
#endif
}

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

static long interface_probe_start(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
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
#else
// the rule a signal names
static InterfaceFault interface_fault_of(unsigned long long signal_number)
{
    switch (signal_number)
    {
    case SIGFPE:
        return INTERFACE_FAULT_ARITHMETIC;
    case SIGSEGV:
    case SIGBUS:
        return INTERFACE_FAULT_ADDRESS;
    case SIGILL:
        return INTERFACE_FAULT_INSTRUCTION;
    case SIGTRAP:
        return INTERFACE_FAULT_TRAP;
    case SIGABRT:
        return INTERFACE_FAULT_ABORT;
    default:
        return INTERFACE_FAULT_OTHER;
    }
}

static void interface_pause(unsigned long long microseconds)
{
    struct timespec pause;
    // whole seconds of a pause fit a time_t
    pause.tv_sec = (time_t)(microseconds / INTERFACE_MILLION);
    // the remainder is below a billion nanoseconds, which a long holds
    pause.tv_nsec = (long)((microseconds % INTERFACE_MILLION) * 1000ull);
    nanosleep(&pause, NULL);
}

// what the pipe holds now read into the answer: 1 once it is at its end, 0 where it holds no more for now
static int interface_pipe_read(int pipe_read, InterfaceAnswer *answer, unsigned long long *total)
{
    char piece[INTERFACE_READ_BYTES];
    ssize_t got = read(pipe_read, piece, sizeof(piece));
    while (got > 0)
    {
        // a read is at most one piece, which is not negative here
        interface_output_kept(answer, total, piece, (unsigned long long)got);
        got = read(pipe_read, piece, sizeof(piece));
    }
    return got == 0;
}

// the probe started in a process group of its own, its output the file or the pipe and its input empty, then run to
// its end or its limit, the pipe read as it is written. A pipe that closes on exec tells a probe that never started
// from one that exited: the probe writes its errno there where exec fails. 0 where it was waited on to its end; -1
// where the output file or the pipe could not be made or the probe could not be waited on.
//
// A pipe is made, then told to close on exec: a probe forked from another thread between the two would carry the pipe
// into its process and hold it open past this probe's end. So from the moment a probe's pipes are made to its fork, no
// other probe of this process is forked
static pthread_mutex_t s_interface_starting = PTHREAD_MUTEX_INITIALIZER;

static long interface_probe_start(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    int output = -1;
    int pipe_read = -1;
    pthread_mutex_lock(&s_interface_starting);
    if (probe->output_path != NULL)
    {
        output = open(probe->output_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    }
    else
    {
        // the probe is handed the write end on its standard output and error; the read end is the interface's, and
        // never waits
        int ends[2] = {-1, -1};
        if (pipe(ends) == 0)
        {
            fcntl(ends[0], F_SETFD, FD_CLOEXEC);
            fcntl(ends[1], F_SETFD, FD_CLOEXEC);
            fcntl(ends[0], F_SETFL, fcntl(ends[0], F_GETFL) | O_NONBLOCK);
            pipe_read = ends[0];
            output = ends[1];
        }
    }
    if (!INTERFACE_CHECK(output >= 0, probe, error, ENGINE_ERROR_RESOURCE))
    {
        pthread_mutex_unlock(&s_interface_starting);
        return -1L;
    }
    unsigned long long total = 0ull;
    const int input = open("/dev/null", O_RDONLY | O_CLOEXEC);
    int told[2] = {-1, -1};
    const int piped = pipe(told) == 0;
    if (piped)
    {
        fcntl(told[0], F_SETFD, FD_CLOEXEC);
        fcntl(told[1], F_SETFD, FD_CLOEXEC);
    }
    const unsigned long long began = interface_now();
    const pid_t made = piped ? fork() : -1;
    if (made != 0)
    {
        pthread_mutex_unlock(&s_interface_starting);
    }
    if (made == 0)
    {
        setpgid(0, 0);
        if (input >= 0)
        {
            dup2(input, 0);
        }
        dup2(output, 1);
        dup2(output, 2);
        execvp(probe->command[0], probe->command);
        const int failed = errno;
        const ssize_t said = write(told[1], &failed, sizeof(failed));
        (void)said;
        _exit(127);
    }
    close(output);
    if (input >= 0)
    {
        close(input);
    }
    if (piped)
    {
        close(told[1]);
    }
    if (made < 0)
    {
        if (piped)
        {
            close(told[0]);
        }
        if (pipe_read >= 0)
        {
            close(pipe_read);
        }
        answer->ending = INTERFACE_ENDING_NOT_STARTED;
        // errno is a positive code where fork or pipe failed
        answer->code = (unsigned long long)errno;
        return 0L;
    }
    // the group is set from both sides. A limit that runs out before the probe has set it still reaches it
    setpgid(made, made);
    int failed = 0;
    ssize_t heard = read(told[0], &failed, sizeof(failed));
    while ((heard < 0) && (errno == EINTR))
    {
        heard = read(told[0], &failed, sizeof(failed));
    }
    close(told[0]);
    int status = 0;
    int out_of_time = 0;
    pid_t reaped = waitpid(made, &status, WNOHANG);
    while ((reaped == 0) && !out_of_time)
    {
        if (pipe_read >= 0)
        {
            interface_pipe_read(pipe_read, answer, &total);
        }
        interface_pause(INTERFACE_POLL_MICROSECONDS);
        reaped = waitpid(made, &status, WNOHANG);
        out_of_time =
            (reaped == 0) && (probe->limit_microseconds != 0ull) && ((interface_now() - began) >= probe->limit_microseconds);
    }
    if (out_of_time)
    {
        kill(-made, SIGKILL);
        reaped = waitpid(made, &status, 0);
    }
    while ((reaped < 0) && (errno == EINTR))
    {
        reaped = waitpid(made, &status, 0);
    }
    answer->microseconds = interface_now() - began;
    // every process the probe started and left ends with it
    kill(-made, SIGKILL);
    // the rest of the pipe read to its end, once every process of the group that held its write end has ended; one a
    // process outside the group still holds is let go after INTERFACE_DRAIN_MILLISECONDS, its read so far kept
    if (pipe_read >= 0)
    {
        const unsigned long long drained_from = interface_now();
        while (!interface_pipe_read(pipe_read, answer, &total) &&
               ((interface_now() - drained_from) < (INTERFACE_DRAIN_MILLISECONDS * INTERFACE_THOUSAND)))
        {
            interface_pause(INTERFACE_POLL_MICROSECONDS);
        }
        close(pipe_read);
        interface_output_ended(answer, total);
    }
    if (!INTERFACE_CHECK(reaped == made, probe->output_path, error, ENGINE_ERROR_RESOURCE))
    {
        return -1L;
    }
    if (heard == (ssize_t)sizeof(failed))
    {
        answer->ending = INTERFACE_ENDING_NOT_STARTED;
        // the probe's errno is a positive code
        answer->code = (unsigned long long)failed;
        return 0L;
    }
    if (out_of_time)
    {
        answer->ending = INTERFACE_ENDING_OUT_OF_TIME;
        answer->code = SIGKILL;
        return 0L;
    }
    const int signaled = WIFSIGNALED(status);
    answer->ending = signaled ? INTERFACE_ENDING_SIGNALED : INTERFACE_ENDING_EXITED;
    // an exit status is 0 to 255 and a signal's number is positive
    answer->code = signaled ? (unsigned long long)WTERMSIG(status) : (unsigned long long)WEXITSTATUS(status);
    answer->fault = signaled ? interface_fault_of(answer->code) : INTERFACE_FAULT_NONE;
    return 0L;
}
#endif

// the probe's output read back: its length whole, and as much of it as the answer's capacity holds
static long interface_output_read(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    FILE *const file = fopen(probe->output_path, "rb");
    if (!INTERFACE_CHECK(file != NULL, probe->output_path, error, ENGINE_ERROR_RESOURCE))
    {
        return -1L;
    }
    char piece[INTERFACE_READ_BYTES];
    unsigned long long total = 0ull;
    size_t read = fread(piece, 1u, sizeof(piece), file);
    while (read != 0u)
    {
        interface_output_kept(answer, &total, piece, read);
        read = fread(piece, 1u, sizeof(piece), file);
    }
    const int complete = ferror(file) == 0;
    fclose(file);
    interface_output_ended(answer, total);
    return INTERFACE_CHECK(complete, probe->output_path, error, ENGINE_ERROR_RESOURCE) ? 0L : -1L;
}

long interface_probe_run(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    answer->ending = INTERFACE_ENDING_NOT_STARTED;
    answer->code = 0ull;
    answer->fault = INTERFACE_FAULT_NONE;
    answer->output_bytes = 0ull;
    answer->microseconds = 0ull;
    if ((answer->output != NULL) && (answer->output_capacity != 0ull))
    {
        answer->output[0] = '\0';
    }
    if (!INTERFACE_CHECK((probe->command != NULL) && (probe->command[0] != NULL), probe, error, ENGINE_ERROR_REQUEST))
    {
        return -1L;
    }
    if (interface_probe_start(probe, answer, error) != 0L)
    {
        return -1L;
    }
    // output that came through the pipe is in the answer already
    return (probe->output_path != NULL) ? interface_output_read(probe, answer, error) : 0L;
}
