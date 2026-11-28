// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_posix_programs.c: a program started and held on the POSIX systems. The child forks, makes its settings and
// then runs the program, and its output and errors come back on two pipes. A detached child heads a process group of
// its own, which a signal to the group of this process does not reach and which a stop ends whole. A terminal's shell
// starts in a pseudo-terminal of its own, leads a session whose controlling terminal it is, and is read and written
// through the leader. A suspended child waits on a pipe until it is resumed, and an exec that fails writes its errno to
// a pipe that closes on exec. The pipes, the processors of a child, its watch, its pseudo-terminal and the tree of a
// program are the system's own (linux/platform_linux_programs.c, macos/platform_macos_programs.c)
#if !defined(_WIN32)
// fork, setenv, setpgid, kill and waitid are POSIX, outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "platform_posix.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

// the entries that a child's environment holds at max, and the bytes that its settings' pairs take at max
#define PLATFORM_ENVIRONMENT_ENTRIES 4096u
#define PLATFORM_SETTING_PAIRS (64u << 10u)

// the environment of the process, which a child changes before it runs its program
extern char **environ;

// 1 where the environment entry `entry`, name=value, has the name of one of the settings of `start`
static int platform_entry_named(const PlatformStart *start, const char *entry)
{
    for (unsigned int at = 0u; at < start->setting_count; at += 1u)
    {
        const char *const name = start->settings[at].name;
        const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        if ((entry[length] == '=') &&
            EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = entry, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = name,
                       .other_cap = length))
        {
            return 1;
        }
    }
    return 0;
}

// The environment that a child runs its program in, made in the child before it runs. It holds the entries of this
// process less those that the settings name, then each setting with a value as name=value. The call drops an entry
// past the bounds. The tables are static: only the child of a fork writes them, and it runs nothing else
static void platform_settings_made(const PlatformStart *start)
{
    static char *s_entries[PLATFORM_ENVIRONMENT_ENTRIES + 1u];
    static char s_pairs[PLATFORM_SETTING_PAIRS];
    unsigned int entries = 0u;
    for (char **entry = environ; (*entry != NULL) && (entries < PLATFORM_ENVIRONMENT_ENTRIES); entry += 1)
    {
        if (!platform_entry_named(start, *entry))
        {
            s_entries[entries] = *entry;
            entries += 1u;
        }
    }
    VerbaLinea pairs = mmgr_line_over(s_pairs, sizeof(s_pairs));
    for (unsigned int at = 0u; (at < start->setting_count) && (entries < PLATFORM_ENVIRONMENT_ENTRIES); at += 1u)
    {
        if (start->settings[at].value == NULL)
        {
            continue;
        }
        const size_t begun = pairs.at;
        mmgr_line_put(&pairs, start->settings[at].name);
        mmgr_line_ch(&pairs, '=');
        mmgr_line_put(&pairs, start->settings[at].value);
        mmgr_line_ch(&pairs, '\0');
        if (!mmgr_line_ok(&pairs))
        {
            break;
        }
        s_entries[entries] = s_pairs + begun;
        entries += 1u;
    }
    s_entries[entries] = NULL;
    environ = s_entries;
}

// the exit code of a status that waitpid gave, or -1 where a signal ended the process
static long long platform_status_code(int status)
{
    return WIFEXITED(status) ? (long long)WEXITSTATUS(status) : -1LL;
}

int platform_program_shared(const PlatformStart *start, long long *code)
{
    const pid_t child = fork();
    if (child < 0)
    {
        mmgr_line_put(start->err_msg, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        platform_settings_made(start);
        // exec takes its words as its declaration has them, and it writes to none of them
        execvp(start->words[0], (char *const *)(uintptr_t)start->words);
        _exit(127);
    }
    int status = 0;
    const pid_t waited = waitpid(child, &status, 0);
    *code = (waited > 0) ? platform_status_code(status) : -1LL;
    return 1;
}

// The child put at the priority of `start` before it runs its program. The call nices a guest or an idle child. A run's
// child keeps the level that it starts at, because a raise takes a privilege that this process does not ask for. The
// system then puts it on its processors and its policy (platform_child_placed)
static void platform_child_lowered(const PlatformStart *start)
{
    if ((start->priority != PLATFORM_PRIORITY_GUEST) && (start->priority != PLATFORM_PRIORITY_IDLE))
    {
        return;
    }
    const int target = (start->priority == PLATFORM_PRIORITY_IDLE) ? 19 : 10;
    errno = 0;
    const int current = getpriority(PRIO_PROCESS, 0);
    // a nicer level already set stays, as under a parent that set it so
    if ((errno != 0) || (current < target))
    {
        setpriority(PRIO_PROCESS, 0, target);
    }
}

// every descriptor of `ends` that is open closed
static void platform_ends_closed(const int *ends, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (ends[at] >= 0)
        {
            close(ends[at]);
        }
    }
}

// a descriptor as the integer of a record, one past it so that 0 is for none. A descriptor that the system gives is
// not negative
static unsigned long long platform_end_kept(int descriptor)
{
    return (unsigned long long)descriptor + 1ULL;
}

static int platform_end_of(unsigned long long kept)
{
    // Explicit cast reads the integer of the record back as the descriptor that the system gave
    return (int)(kept - 1ULL);
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

// The descriptors of one start. `out`, `err` and `in` are the pipes of the output, the errors and the input. `file` is
// the output file. `go` is the pipe that a suspended child waits on, and `report` is the pipe that carries the errno of
// an exec that failed. Each end is -1 for none
typedef struct
{
    int out[2];
    int err[2];
    int in[2];
    int go[2];
    int report[2];
    int file;
} PlatformEnds;

// every descriptor of `ends` closed
static void platform_all_ends_closed(const PlatformEnds *ends)
{
    const int every[11] = {ends->out[0], ends->out[1], ends->err[0],    ends->err[1],    ends->in[0], ends->in[1],
                           ends->go[0],  ends->go[1],  ends->report[0], ends->report[1], ends->file};
    platform_ends_closed(every, 11u);
}

// the descriptors of `start` made as its modes ask: 1, or 0 with the errno of the one that failed in `code`
static int platform_ends_made(const PlatformStart *start, PlatformEnds *ends, int *code)
{
    const PlatformEnds none = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, -1};
    *ends = none;
    const unsigned int output = start->output;
    const int out_piped =
        (output == PLATFORM_OUTPUT_APART) || (output == PLATFORM_OUTPUT_JOINED) || (output == PLATFORM_OUTPUT_ALONE);
    int whole = !out_piped || platform_pipe_made(ends->out);
    whole = whole && ((output != PLATFORM_OUTPUT_APART) || platform_pipe_made(ends->err));
    whole = whole && (!start->input_piped || platform_pipe_made(ends->in));
    whole = whole && (!start->suspended || platform_pipe_made(ends->go));
    whole = whole && ((start->refusal == NULL) || start->suspended || platform_pipe_made(ends->report));
    if (whole && (output == PLATFORM_OUTPUT_FILE))
    {
        ends->file = (start->output_path != NULL)
                         ? open(start->output_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644)
                         : -1;
        whole = ends->file >= 0;
    }
    *code = whole ? 0 : errno;
    return whole;
}

// The child of a fork made ready, then its program run: its standard descriptors, its group, its priority, its
// processors, its environment and its folder. It waits on `go` first where the start suspends it. It does not return
static void platform_child_run(const PlatformStart *start, const PlatformEnds *ends)
{
    const int nothing = open("/dev/null", O_RDWR);
    if (nothing < 0)
    {
        _exit(127);
    }
    const unsigned int output = start->output;
    if (start->input_piped || !start->input_own)
    {
        dup2(start->input_piped ? ends->in[0] : nothing, 0);
    }
    if (output == PLATFORM_OUTPUT_FILE)
    {
        dup2(ends->file, 1);
        dup2(ends->file, 2);
    }
    else if (output != PLATFORM_OUTPUT_OWN)
    {
        // the output goes to its pipe or to nothing, and the errors to their own pipe, to the output's or to nothing
        dup2((ends->out[1] >= 0) ? ends->out[1] : nothing, 1);
        const int joined = (output == PLATFORM_OUTPUT_JOINED) ? ends->out[1] : nothing;
        dup2((ends->err[1] >= 0) ? ends->err[1] : joined, 2);
    }
    if (nothing > 2)
    {
        close(nothing);
    }
    if (start->detached || start->outlives)
    {
        setpgid(0, 0);
    }
    if (ends->go[0] >= 0)
    {
        // the parent holds the one end that writes, and so the read meets the end of the pipe once the parent ends
        close(ends->go[1]);
        char word = 0;
        // a parent that ends before it lets the child go leaves nothing to read, and the child runs nothing
        if (read(ends->go[0], &word, 1u) != 1)
        {
            _exit(127);
        }
    }
    platform_child_placed(start);
    platform_child_lowered(start);
    platform_settings_made(start);
    if ((start->folder != NULL) && (chdir(start->folder) != 0))
    {
        _exit(127);
    }
    // exec takes its words as its declaration has them, and it writes to none of them
    execvp(start->words[0], (char *const *)(uintptr_t)start->words);
    if (ends->report[1] >= 0)
    {
        const int failed = errno;
        const ssize_t written = write(ends->report[1], &failed, sizeof(failed));
        (void)written;
    }
    _exit(127);
}

// The report of the exec of `child` read. 1 where its program runs, as the pipe closes on exec with nothing written. 0
// with the errno of the exec in `code` where it failed. The call waits here for a child whose exec failed
static int platform_exec_reported(pid_t child, int report, int *code)
{
    int failed = 0;
    ssize_t received = read(report, &failed, sizeof(failed));
    while ((received < 0) && (errno == EINTR))
    {
        received = read(report, &failed, sizeof(failed));
    }
    if (received != (ssize_t)sizeof(failed))
    {
        return 1;
    }
    int status = 0;
    while ((waitpid(child, &status, 0) < 0) && (errno == EINTR))
    {
    }
    *code = failed;
    return 0;
}

int platform_program_started(const PlatformStart *start, PlatformProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .val = 0, .bytes = sizeof(*program));
    if (start->confined)
    {
        // a confined start needs the glue that hands each start of a child to this process, and none runs yet
        platform_refusal_given(start, PLATFORM_REFUSED_START, (unsigned long long)ENOSYS);
        mmgr_line_put(start->err_msg, "this system cannot confine the program's starts");
        return 0;
    }
    if (start->input_piped)
    {
        // a write to a pipe whose reader ended fails, and does not end this process
        signal(SIGPIPE, SIG_IGN);
    }
    PlatformEnds ends;
    int code = 0;
    if (!platform_ends_made(start, &ends, &code))
    {
        platform_all_ends_closed(&ends);
        platform_refusal_given(start, PLATFORM_REFUSED_ENDS, (unsigned long long)code);
        mmgr_line_put(start->err_msg, "no pipe or output file could be made");
        return 0;
    }
    const pid_t child = fork();
    if (child < 0)
    {
        code = errno;
        platform_all_ends_closed(&ends);
        platform_refusal_given(start, PLATFORM_REFUSED_START, (unsigned long long)code);
        mmgr_line_put(start->err_msg, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        platform_child_run(start, &ends);
    }
    if (start->detached || start->outlives)
    {
        // both sides set the group, and a signal to the group then reaches the child before it set the group
        (void)setpgid(child, child);
    }
    const int child_ends[6] = {ends.out[1], ends.err[1], ends.in[0], ends.go[0], ends.report[1], ends.file};
    platform_ends_closed(child_ends, 6u);
    if ((ends.report[0] >= 0) && !platform_exec_reported(child, ends.report[0], &code))
    {
        const int own_ends[4] = {ends.out[0], ends.err[0], ends.in[1], ends.report[0]};
        platform_ends_closed(own_ends, 4u);
        platform_refusal_given(start, PLATFORM_REFUSED_START, (unsigned long long)code);
        mmgr_line_put(start->err_msg, "the program could not be run");
        return 0;
    }
    if (ends.report[0] >= 0)
    {
        close(ends.report[0]);
    }
    // a process number that fork gives the parent is positive
    program->process = (unsigned long long)child;
    program->id = (unsigned long long)child;
    program->out = (ends.out[0] >= 0) ? platform_end_kept(ends.out[0]) : 0ULL;
    program->err = (ends.err[0] >= 0) ? platform_end_kept(ends.err[0]) : 0ULL;
    program->in = (ends.in[1] >= 0) ? platform_end_kept(ends.in[1]) : 0ULL;
    program->held = (ends.go[1] >= 0) ? platform_end_kept(ends.go[1]) : 0ULL;
    return 1;
}

void platform_program_resumed(PlatformProgram *program)
{
    if (program->held != 0ULL)
    {
        const char word = 1;
        const ssize_t written = write(platform_end_of(program->held), &word, 1u);
        (void)written;
        close(platform_end_of(program->held));
        program->held = 0ULL;
    }
}

// a terminal's columns or rows as the system counts them, two at the fewest
static unsigned short platform_terminal_extent(unsigned int count)
{
    // a count kept between 2 and the max that a short counts is what an unsigned short holds
    const unsigned int bounded = (count > 0x7fffu) ? 0x7fffu : count;
    return (unsigned short)((count < 2u) ? 2u : bounded);
}

int platform_program_terminal(const PlatformStart *start, unsigned int cols, unsigned int rows,
                              PlatformProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .val = 0, .bytes = sizeof(*program));
    // the system makes the pair and sizes the follower, as its own terminals do
    struct winsize size = {platform_terminal_extent(rows), platform_terminal_extent(cols), 0u, 0u};
    int leader = -1;
    int follower = -1;
    if (!platform_terminal_pair(&leader, &follower, &size))
    {
        mmgr_line_put(start->err_msg, "no pseudo-terminal could be made");
        return 0;
    }
    const int written = dup(leader);
    if (written < 0)
    {
        const int ends[2] = {leader, follower};
        platform_ends_closed(ends, 2u);
        mmgr_line_put(start->err_msg, "no pseudo-terminal could be made");
        return 0;
    }
    (void)fcntl(leader, F_SETFD, FD_CLOEXEC);
    (void)fcntl(written, F_SETFD, FD_CLOEXEC);
    const pid_t child = fork();
    if (child < 0)
    {
        const int ends[3] = {leader, written, follower};
        platform_ends_closed(ends, 3u);
        mmgr_line_put(start->err_msg, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        // a session of its own, with the follower as the terminal that controls it, on its standard descriptors
        if (!platform_terminal_led(follower))
        {
            _exit(127);
        }
        platform_settings_made(start);
        if ((start->folder != NULL) && (chdir(start->folder) != 0))
        {
            _exit(127);
        }
        // exec takes its words as its declaration has them, and it writes to none of them
        execvp(start->words[0], (char *const *)(uintptr_t)start->words);
        _exit(127);
    }
    // the follower is the child's alone: a read of the leader meets its end once every holder of the follower ends
    close(follower);
    // a process number that fork gives the parent is positive
    program->process = (unsigned long long)child;
    program->id = (unsigned long long)child;
    program->out = platform_end_kept(leader);
    program->in = platform_end_kept(written);
    return 1;
}

int platform_program_terminal_sized(const PlatformProgram *program, unsigned int cols, unsigned int rows)
{
    const struct winsize size = {platform_terminal_extent(rows), platform_terminal_extent(cols), 0u, 0u};
    return (program->in != 0ULL) && (ioctl(platform_end_of(program->in), TIOCSWINSZ, &size) == 0);
}

void platform_program_terminal_hung_up(const PlatformProgram *program)
{
    // the shell leads a session and a group of its own, and the hang-up reaches every process of it
    kill(-(pid_t)program->process, SIGHUP);
}

void platform_program_terminal_closed(PlatformProgram *program)
{
    // a reader of the leader meets its end once the shell and every process that holds the follower end
    (void)program;
}

int platform_program_written(unsigned long long handle, const void *bytes, unsigned long long size)
{
    // Explicit cast reads the source as its bytes
    const unsigned char *at = (const unsigned char *)bytes;
    while ((size > 0ULL) && (handle != 0ULL))
    {
        const ssize_t written = write(platform_end_of(handle), at, (size_t)size);
        if (written <= 0)
        {
            return 0;
        }
        // a count written is positive here
        at += written;
        size -= (unsigned long long)written;
    }
    return size == 0ULL;
}

void platform_program_input_closed(PlatformProgram *program)
{
    if (program->in != 0ULL)
    {
        close(platform_end_of(program->in));
        program->in = 0ULL;
    }
}

unsigned long long platform_program_read(unsigned long long handle, void *piece, unsigned long long capacity)
{
    if (handle == 0ULL)
    {
        return 0ULL;
    }
    const ssize_t read_count = read(platform_end_of(handle), piece, (size_t)capacity);
    // a count read is not negative here
    return (read_count > 0) ? (unsigned long long)read_count : 0ULL;
}

// the count that a call answers where the system counts no bytes that wait, and a read still returns at once. A
// pseudo-terminal leader on macOS counts none, and the read answers how many there are
#define PLATFORM_READ_READY 65536ULL

unsigned long long platform_program_waiting(unsigned long long handle)
{
    int waiting = 0;
    if ((handle == 0ULL) || (ioctl(platform_end_of(handle), FIONREAD, &waiting) != 0) || (waiting < 0))
    {
        return 0ULL;
    }
    if (waiting == 0)
    {
        const int descriptor = platform_end_of(handle);
        fd_set readable;
        struct timeval none = {0, 0};
        FD_ZERO(&readable);
        FD_SET(descriptor, &readable);
        if ((descriptor < FD_SETSIZE) && (select(descriptor + 1, &readable, NULL, NULL, &none) > 0))
        {
            return PLATFORM_READ_READY;
        }
    }
    // a count that the system reports is not negative here
    return (unsigned long long)waiting;
}

long long platform_program_waited(PlatformProgram *program)
{
    int status = 0;
    // the process number kept is the child's
    if (waitpid((pid_t)program->process, &status, 0) < 0)
    {
        return -1LL;
    }
    program->waited = 1ULL;
    return platform_status_code(status);
}

int platform_program_polled(PlatformProgram *program, long long *code)
{
    int status = 0;
    // the process number kept is the child's
    const pid_t done = waitpid((pid_t)program->process, &status, WNOHANG);
    if (done == 0)
    {
        return 0;
    }
    program->waited = 1ULL;
    *code = (done > 0) ? platform_status_code(status) : -1LL;
    return 1;
}

// the children let go of before they ended. The call waits for each one once it ends, and then none of them is a
// zombie. The max let go of at once
#define PLATFORM_LET_GO_MAX 256u
static pid_t s_let_go[PLATFORM_LET_GO_MAX];

// `child`, let go of before it ended, put among the children let go of where `child` is not 0. The call waits for
// every one of them that ended, and frees its place
static void platform_children_let_go(pid_t child)
{
    int placed = (child == 0);
    for (unsigned int at = 0u; at < PLATFORM_LET_GO_MAX; at += 1u)
    {
        int status = 0;
        if ((s_let_go[at] != 0) && (waitpid(s_let_go[at], &status, WNOHANG) != 0))
        {
            s_let_go[at] = 0;
        }
        if (!placed && (s_let_go[at] == 0))
        {
            s_let_go[at] = child;
            placed = 1;
        }
    }
}

void platform_program_group_ended(unsigned long long group)
{
    (void)group;
}

void platform_program_closed(PlatformProgram *program)
{
    // a suspended child whose pipe closes here meets its end, and runs nothing
    const unsigned long long ends[4] = {program->out, program->err, program->in, program->held};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (ends[at] != 0ULL)
        {
            close(platform_end_of(ends[at]));
        }
    }
    // where no wait read a child yet, the call waits for it now where it ended, and once it ends where it goes on
    int status = 0;
    const int going = (program->process != 0ULL) && (program->waited == 0ULL) &&
                      (waitpid((pid_t)program->process, &status, WNOHANG) == 0);
    // the process number kept is the child's
    platform_children_let_go(going ? (pid_t)program->process : 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .val = 0, .bytes = sizeof(*program));
}

void platform_program_killed(const PlatformProgram *program)
{
    // the process number kept is the child's
    kill((pid_t)program->process, SIGKILL);
}

int platform_program_interrupted(const PlatformProgram *program)
{
    // the process number kept is the child's
    return kill((pid_t)program->process, SIGINT) == 0;
}

void platform_program_tree_ended(unsigned long long id)
{
    // The call asks the group that the process leads to end. An id of 0 names no program, and a kill of group 0 ends
    // the group of the caller
    if (id != 0ULL)
    {
        // the id kept is the number of the child, which leads its group
        kill(-(pid_t)id, SIGTERM);
    }
}

// 1 where the child `child` ended, which stays unreaped, or where the system knows no such child
static int platform_child_has_ended(pid_t child)
{
    siginfo_t ended;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &ended, .val = 0, .bytes = sizeof(ended));
    // a process number that fork gave is positive, which an id_t holds
    const int result = waitid(P_PID, (id_t)child, &ended, WEXITED | WNOHANG | WNOWAIT);
    return ((result == 0) && (ended.si_pid == child)) || ((result != 0) && (errno != EINTR));
}

// the step of a wait that a system with no descriptor of a process takes between two looks at it
#define PLATFORM_WAIT_STEP_MICROSECONDS 10000ULL

int platform_program_ended_within(const PlatformProgram *program, unsigned long long microseconds)
{
    // the process number kept is the child's
    const pid_t child = (pid_t)program->process;
    if ((program->waited != 0ULL) || platform_child_has_ended(child))
    {
        return 1;
    }
    if (platform_child_watched(child, microseconds))
    {
        return platform_child_has_ended(child);
    }
    const unsigned long long began = platform_clock_ns();
    int ended = 0;
    while (!ended && (((platform_clock_ns() - began) / 1000ULL) < microseconds))
    {
        platform_paused((unsigned int)(PLATFORM_WAIT_STEP_MICROSECONDS / 1000ULL));
        ended = platform_child_has_ended(child);
    }
    return ended;
}

int platform_program_ending(PlatformProgram *program, PlatformEnding *ending)
{
    int status = 0;
    // the process number kept is the child's
    const pid_t child = (pid_t)program->process;
    pid_t reaped = waitpid(child, &status, 0);
    while ((reaped < 0) && (errno == EINTR))
    {
        reaped = waitpid(child, &status, 0);
    }
    if (reaped != child)
    {
        return 0;
    }
    program->waited = 1ULL;
    ending->signaled = WIFSIGNALED(status);
    // an exit status is 0 to 255 and a signal's number is positive
    ending->code = ending->signaled ? (unsigned long long)WTERMSIG(status)
                                    : (WIFEXITED(status) ? (unsigned long long)WEXITSTATUS(status) : 0ULL);
    return 1;
}

void platform_program_group_killed(const PlatformProgram *program)
{
    // the process number kept is the child's, and its group's where it heads one
    const pid_t child = (pid_t)program->process;
    kill(-child, SIGKILL);
    if (program->waited == 0ULL)
    {
        kill(child, SIGKILL);
    }
}

unsigned long long platform_program_read_ready(unsigned long long handle, void *piece, unsigned long long capacity,
                                               int *ended)
{
    *ended = (handle == 0ULL);
    if (handle == 0ULL)
    {
        return 0ULL;
    }
    struct pollfd ready;
    ready.fd = platform_end_of(handle);
    ready.events = POLLIN;
    ready.revents = 0;
    if ((poll(&ready, 1u, 0) <= 0) || ((ready.revents & (POLLIN | POLLHUP | POLLERR)) == 0))
    {
        return 0ULL;
    }
    const ssize_t got = read(ready.fd, piece, (size_t)capacity);
    if (got > 0)
    {
        // a count read is positive here
        return (unsigned long long)got;
    }
    // a read of 0 is the end of the pipe, and so is an error that is no interrupt
    *ended = (got == 0) || ((errno != EINTR) && (errno != EAGAIN));
    return 0ULL;
}
#endif
