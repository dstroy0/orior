// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix.c: the probe on POSIX: the rule a signal names, the pause, the pipe read, and the probe forked in a
// process group of its own and run to its end or its limit
#include "interface_internal.h"

#if !defined(_WIN32)
#include <semaphore.h>

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

// The launch queue (interface.h): two semaphores every process on the host opens by name, the places for the children
// running at once and the one start under way, which its taker holds for the spacing after the start is made. A
// process the system gives no named semaphore queues nothing
static sem_t *s_launches = SEM_FAILED;
static sem_t *s_launching = SEM_FAILED;
static pthread_once_t s_queue_opened = PTHREAD_ONCE_INIT;

static void interface_queue_open(void)
{
    s_launches = sem_open("/orior_launches", O_CREAT, 0600, INTERFACE_LAUNCHES_AT_ONCE);
    s_launching = sem_open("/orior_launching", O_CREAT, 0600, 1u);
}

void interface_launch_taken(void)
{
    pthread_once(&s_queue_opened, interface_queue_open);
    if (s_launches != SEM_FAILED)
    {
        while ((sem_wait(s_launches) != 0) && (errno == EINTR))
        {
        }
    }
    if (s_launching != SEM_FAILED)
    {
        while ((sem_wait(s_launching) != 0) && (errno == EINTR))
        {
        }
    }
}

void interface_launch_started(void)
{
    interface_pause((unsigned long long)INTERFACE_LAUNCH_SPACING_MILLISECONDS * INTERFACE_THOUSAND);
    if (s_launching != SEM_FAILED)
    {
        sem_post(s_launching);
    }
}

void interface_launch_given(void)
{
    if (s_launches != SEM_FAILED)
    {
        sem_post(s_launches);
    }
}

long interface_probe_start(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    int output = -1;
    int pipe_read = -1;
    interface_launch_taken();
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
        interface_launch_started();
        interface_launch_given();
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
        interface_launch_started();
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
        interface_launch_given();
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
    // the probe has ended, however it ended: its place in the queue is given back
    interface_launch_given();
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

long interface_output_file_read(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    const int file = open(probe->output_path, O_RDONLY | O_CLOEXEC);
    if (!INTERFACE_CHECK(file >= 0, probe->output_path, error, ENGINE_ERROR_RESOURCE))
    {
        return -1L;
    }
    char piece[INTERFACE_READ_BYTES];
    unsigned long long total = 0ull;
    ssize_t got = read(file, piece, sizeof(piece));
    while (got > 0)
    {
        // a read that gave bytes gave a positive count
        interface_output_kept(answer, &total, piece, (unsigned long long)got);
        got = read(file, piece, sizeof(piece));
    }
    close(file);
    interface_output_ended(answer, total);
    return INTERFACE_CHECK(got == 0, probe->output_path, error, ENGINE_ERROR_RESOURCE) ? 0L : -1L;
}
