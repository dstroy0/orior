// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_macos_programs.c: the calls of the programs that macOS alone has (../platform_posix.h). A pipe is set to
// close on exec after it is made, a child keeps its processors and its policy, a wait goes in steps, a pseudo-terminal
// comes from util.h, a tree's stop goes to the program's group, and the processor time of a tree is not read
#if defined(__APPLE__)
// openpty and login_tty are BSD's, outside strict C11
#define _DARWIN_C_SOURCE
#endif
#include "../platform_posix.h"

#if defined(__APPLE__)
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <util.h>

int platform_pipe_made(int ends[2])
{
    const int made = pipe(ends) == 0;
    if (made)
    {
        (void)fcntl(ends[0], F_SETFD, FD_CLOEXEC);
        (void)fcntl(ends[1], F_SETFD, FD_CLOEXEC);
    }
    else
    {
        ends[0] = -1;
        ends[1] = -1;
    }
    return made;
}

void platform_child_placed(const PlatformStart *start)
{
    // macOS sets no processors of a process and has no idle policy, and its nice value is the level of the child
    (void)start;
}

int platform_child_watched(pid_t child, unsigned long long microseconds)
{
    // macOS gives no descriptor of a process, and the caller waits in steps
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

unsigned long long platform_program_tree_stopped(PlatformProgram *program, unsigned long long grace, unsigned int code)
{
    (void)code;
    // the process number kept is the child's
    const pid_t child = (pid_t)program->process;
    // the group that the program heads asked to end, and ended by force where it runs on after the grace
    kill(-child, SIGTERM);
    kill(child, SIGTERM);
    if (platform_program_ended_within(program, grace))
    {
        return 0ULL;
    }
    platform_program_group_killed(program);
    return 1ULL;
}

int platform_program_processor_time(const PlatformProgram *program, unsigned long long *microseconds)
{
    // macOS lists no tree of a program to read, and the call reads no time
    (void)program;
    (void)microseconds;
    return 0;
}
#endif
