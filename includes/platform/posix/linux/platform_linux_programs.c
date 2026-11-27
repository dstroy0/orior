// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_linux_programs.c: the calls of the programs that Linux alone has (../platform_posix.h). A pipe closes on
// exec from the call that makes it, a child takes its processors and the idle policy, a wait watches a descriptor of
// the process, a pseudo-terminal comes from pty.h, and the tree of a program is read from /proc for its processor time
// and its stop
#if defined(__linux__)
// pipe2, the affinity mask, the idle policy, the descriptor of a process and openpty are the system's, outside strict
// C11
#define _GNU_SOURCE
#endif
#include "../platform_posix.h"
#include "cellularum_laboro/cellularum_laboro.h"

#if defined(__linux__)
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <sched.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utmp.h>

int platform_pipe_made(int ends[2])
{
    const int made = pipe2(ends, O_CLOEXEC) == 0;
    if (!made)
    {
        ends[0] = -1;
        ends[1] = -1;
    }
    return made;
}

void platform_child_placed(const PlatformStart *start)
{
    if (start->processor_mask != 0ULL)
    {
        cpu_set_t set;
        CPU_ZERO(&set);
        for (unsigned int processor = 0u; processor < 64u; processor += 1u)
        {
            if (((start->processor_mask >> processor) & 1ULL) != 0ULL)
            {
                CPU_SET(processor, &set);
            }
        }
        (void)sched_setaffinity(0, sizeof(set), &set);
    }
    if (start->priority == PLATFORM_PRIORITY_IDLE)
    {
        const struct sched_param none = {0};
        (void)sched_setscheduler(0, SCHED_IDLE, &none);
    }
}

int platform_child_watched(pid_t child, unsigned long long microseconds)
{
#if defined(SYS_pidfd_open)
    // a descriptor number fits an int
    const int watch = (int)syscall(SYS_pidfd_open, child, 0u);
    if (watch >= 0)
    {
        struct pollfd ready;
        ready.fd = watch;
        ready.events = POLLIN;
        ready.revents = 0;
        // a wait of 24 days or more has no bound, and a shorter one's milliseconds, rounded up, fit an int
        const int bounded = (microseconds / 1000ULL) < 0x7fffffffULL;
        (void)poll(&ready, 1u, bounded ? (int)((microseconds + 999ULL) / 1000ULL) : -1);
        close(watch);
        return 1;
    }
#endif
    (void)child;
    (void)microseconds;
    return 0;
}

int platform_terminal_pair(int *leader, int *follower, struct winsize *size)
{
    return openpty(leader, follower, NULL, NULL, size) == 0;
}

int platform_terminal_led(int follower)
{
    return login_tty(follower) == 0;
}

// A process that /proc lists: its number, its parent and its state. Then the ticks of the processor that it and the
// children that it waited for used. Then 1 where it is in the tree of the program
typedef struct
{
    unsigned long long pid;
    unsigned long long parent;
    unsigned long long ticks;
    char state;
    int in_tree;
} PlatformTreeProcess;

// the white space that the read of /proc's stat skips before each field
#define PLATFORM_STAT_SPACE " \t\n\v\f\r"

// The fields of /proc's stat after the name: the state, the parent, nine fields skipped, then four counts of ticks.
// The answer is the count of fields read, 6 where all of them came, as the read of a format counts them
static int platform_stat_fields(const char *at, PlatformTreeProcess *process, unsigned long long ticks[4])
{
    at += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = PLATFORM_STAT_SPACE);
    if (*at == '\0')
    {
        return 0;
    }
    process->state = *at;
    at += 1;
    const char *end = at;
    process->parent = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = at, .end = &end, .base = 10u);
    if (end == at)
    {
        return 1;
    }
    at = end;
    for (unsigned int skipped = 0u; skipped < 9u; skipped += 1u)
    {
        at += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = PLATFORM_STAT_SPACE);
        const size_t word = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                       .other = PLATFORM_STAT_SPACE);
        if (word == 0u)
        {
            return 2;
        }
        at += word;
    }
    for (unsigned int tick = 0u; tick < 4u; tick += 1u)
    {
        end = at;
        ticks[tick] = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = at, .end = &end, .base = 10u);
        if (end == at)
        {
            // Explicit cast narrows a count of at max 4 to the int of the answer
            return 2 + (int)tick;
        }
        at = end;
    }
    return 6;
}

// the process `pid` read from /proc: 1, or 0 where /proc does not list it whole
static int platform_tree_process_read(unsigned long long pid, PlatformTreeProcess *process)
{
    char path[64];
    VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
    mmgr_line_put(&path_line, "/proc/");
    mmgr_line_decimal(&path_line, pid, 1u);
    mmgr_line_put(&path_line, "/stat");
    (void)mmgr_line_finish(&path_line);
    PlatformFile *const stat = platform_file_open(path, "r");
    if (stat == NULL)
    {
        return 0;
    }
    char line[1024];
    const int read = platform_file_text_read(line, sizeof(line), stat) != NULL;
    platform_file_close(stat);
    // The name may hold spaces and parentheses. The fields come after its last ')'. They are the state, the parent and
    // nine skipped. Then come the user and system ticks of the process and those of the children that it waited for
    const char *const after_name = read ? EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = line,
                                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ')')
                                        : NULL;
    unsigned long long ticks[4] = {0ULL, 0ULL, 0ULL, 0ULL};
    if ((after_name == NULL) || (platform_stat_fields(after_name + 1, process, ticks) != 6))
    {
        return 0;
    }
    process->pid = pid;
    process->ticks = ticks[0] + ticks[1] + ticks[2] + ticks[3];
    process->in_tree = 0;
    return 1;
}

// what one listing of /proc gathers: the processes read, the room for them, their count, and 0 once a growth failed
typedef struct
{
    PlatformTreeProcess *processes;
    unsigned long long capacity;
    unsigned long long count;
    int whole;
} PlatformTreeListing;

// one entry of /proc: the process that it names read where its name is a process number
static void platform_tree_entry(void *context, const char *name, int folder)
{
    (void)folder;
    // Explicit cast takes the context that platform_tree_read gave as the listing that it holds
    PlatformTreeListing *const listing = (PlatformTreeListing *)context;
    const char *end = NULL;
    const unsigned long long pid = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = name, .end = &end, .base = 10);
    PlatformTreeProcess read;
    if (!listing->whole || (end == name) || (*end != '\0') || !platform_tree_process_read(pid, &read))
    {
        return;
    }
    if (listing->count == listing->capacity)
    {
        listing->capacity = (listing->capacity == 0ULL) ? 256ULL : (listing->capacity * 2ULL);
        // Explicit cast takes the grown cell as the records that it holds
        PlatformTreeProcess *const grown = (PlatformTreeProcess *)platform_cell_grown(
            listing->processes, listing->count * sizeof(PlatformTreeProcess),
            listing->capacity * sizeof(PlatformTreeProcess), (unsigned int)platform_place_g_program_owner, 0);
        listing->whole = grown != NULL;
        listing->processes = listing->whole ? grown : listing->processes;
    }
    if (listing->whole)
    {
        listing->processes[listing->count] = read;
        listing->count += 1ULL;
    }
}

// Every process that /proc lists, the tree of `root` marked: the root itself, then each whose parent is in the tree,
// until a pass adds none. The records and their count, or NULL where a read of /proc fails [RETURNS OWNERSHIP]
static PlatformTreeProcess *platform_tree_read(pid_t root, unsigned long long *count)
{
    PlatformTreeListing listing = {NULL, 0ULL, 0ULL, 1};
    *count = 0ULL;
    const int listed = platform_folder_listed("/proc", platform_tree_entry, &listing);
    if (!listed || !listing.whole || (listing.processes == NULL))
    {
        platform_cell_released(listing.processes, (unsigned int)platform_place_g_program_owner);
        return NULL;
    }
    PlatformTreeProcess *const processes = listing.processes;
    for (unsigned long long at = 0ULL; at < listing.count; at += 1ULL)
    {
        // a child's process number is positive, and it converts exactly
        processes[at].in_tree = processes[at].pid == (unsigned long long)root;
    }
    int added = 1;
    while (added)
    {
        added = 0;
        for (unsigned long long at = 0ULL; at < listing.count; at += 1ULL)
        {
            for (unsigned long long parent = 0ULL; !processes[at].in_tree && (parent < listing.count); parent += 1ULL)
            {
                processes[at].in_tree = processes[parent].in_tree && (processes[parent].pid == processes[at].parent);
                added = added || processes[at].in_tree;
            }
        }
    }
    *count = listing.count;
    return processes;
}

// the processes of the tree that still live, not ended and not left for a reap, each sent `signal_number` where it
// is not 0: their count
static unsigned long long platform_tree_living(const PlatformTreeProcess *processes, unsigned long long count,
                                               int signal_number)
{
    unsigned long long living = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        PlatformTreeProcess now;
        const int alive =
            processes[at].in_tree && platform_tree_process_read(processes[at].pid, &now) && (now.state != 'Z');
        if (alive && (signal_number != 0))
        {
            // a process number that /proc lists is positive, which a pid_t holds
            kill((pid_t)processes[at].pid, signal_number);
        }
        living += alive ? 1ULL : 0ULL;
    }
    return living;
}

// the step between two looks at the tree of a program that the call asks to end
#define PLATFORM_TREE_STEP_MICROSECONDS 100000ULL

unsigned long long platform_program_tree_stopped(PlatformProgram *program, unsigned long long grace, unsigned int code)
{
    (void)code;
    // the process number kept is the child's
    const pid_t child = (pid_t)program->process;
    unsigned long long count = 0ULL;
    PlatformTreeProcess *const processes = platform_tree_read(child, &count);
    if (processes == NULL)
    {
        kill(child, SIGKILL);
        return 1ULL;
    }
    unsigned long long living = platform_tree_living(processes, count, SIGTERM);
    const unsigned long long asked_at = platform_clock_ns();
    while ((living != 0ULL) && (((platform_clock_ns() - asked_at) / 1000ULL) < grace))
    {
        platform_paused((unsigned int)(PLATFORM_TREE_STEP_MICROSECONDS / 1000ULL));
        living = platform_tree_living(processes, count, 0);
    }
    const unsigned long long forced = (living != 0ULL) ? platform_tree_living(processes, count, SIGKILL) : 0ULL;
    platform_cell_released(processes, (unsigned int)platform_place_g_program_owner);
    return forced;
}

int platform_program_processor_time(const PlatformProgram *program, unsigned long long *microseconds)
{
    unsigned long long count = 0ULL;
    // the process number kept is the child's
    PlatformTreeProcess *const processes = platform_tree_read((pid_t)program->process, &count);
    if (processes == NULL)
    {
        return 0;
    }
    unsigned long long ticks = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        ticks += processes[at].in_tree ? processes[at].ticks : 0ULL;
    }
    platform_cell_released(processes, (unsigned int)platform_place_g_program_owner);
    const long hertz = sysconf(_SC_CLK_TCK);
    // the ticks of the clock in a second are positive where it answers
    *microseconds = (hertz > 0L) ? ((ticks * 1000000ULL) / (unsigned long long)hertz) : 0ULL;
    return hertz > 0L;
}
#endif
