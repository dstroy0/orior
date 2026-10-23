// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_spawn.c: a run's process started on Linux and macOS: forked, its settings made in the child before it runs the
// program, its output and errors piped back apart, and, detached, in a process group of its own, which a signal to the
// window's group does not reach and which a stop ends whole
#if !defined(_WIN32)
// fork, setenv, setpgid and kill are POSIX, outside strict C11, and SCHED_IDLE is GNU's
#define _GNU_SOURCE
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// the settings made in this process's environment, as a child makes them before it runs its program
static void cli_spawn_settings(const CliSetting *settings, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (settings[at].value != NULL)
        {
            setenv(settings[at].name, settings[at].value, 1);
        }
        else
        {
            unsetenv(settings[at].name);
        }
    }
}

int cli_spawn_shared(const char *const *words, const CliSetting *settings, unsigned int count, long long *code,
                     CliLine *said)
{
    const pid_t child = fork();
    if (child < 0)
    {
        cli_line_text(said, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        cli_spawn_settings(settings, count);
        // exec takes its words as it was declared; it writes to none of them
        execvp(words[0], (char *const *)words);
        _exit(127);
    }
    int status = 0;
    const pid_t waited = waitpid(child, &status, 0);
    *code = ((waited > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1ll;
    return 1;
}

// the child put at `priority` before it runs its program: a guest or an idle one niced, an idle one on the idle policy
// where the system has it; a run's process keeps the level it was started at, as raising one takes a privilege orior
// does not ask for
static void cli_spawn_lowered(unsigned int priority)
{
    if ((priority != CLI_PRIORITY_GUEST) && (priority != CLI_PRIORITY_IDLE))
    {
        return;
    }
    setpriority(PRIO_PROCESS, 0, (priority == CLI_PRIORITY_IDLE) ? 19 : 9);
#if defined(__linux__)
    if (priority == CLI_PRIORITY_IDLE)
    {
        const struct sched_param none = {0};
        sched_setscheduler(0, SCHED_IDLE, &none);
    }
#endif
}

// a process started, its standard input a pipe this process feeds where `fed`, else /dev/null, at `priority`
static int cli_spawn_with(unsigned int priority, CliSpawned *spawned, const char *const *words, const char *folder,
                          const CliSetting *settings, unsigned int count, int detached, int fed, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = spawned, .val = 0, .bytes = sizeof(*spawned)});
    int out[2] = {-1, -1};
    int err[2] = {-1, -1};
    int in[2] = {-1, -1};
    if ((pipe(out) != 0) || (pipe(err) != 0) || (fed && (pipe(in) != 0)))
    {
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            const int ends[3] = {out[at], err[at], in[at]};
            for (unsigned int end = 0u; end < 3u; end += 1u)
            {
                if (ends[end] >= 0)
                {
                    close(ends[end]);
                }
            }
        }
        cli_line_text(said, "no pipe could be made");
        return 0;
    }
    const pid_t child = fork();
    if (child < 0)
    {
        close(out[0]);
        close(out[1]);
        close(err[0]);
        close(err[1]);
        if (fed)
        {
            close(in[0]);
            close(in[1]);
        }
        cli_line_text(said, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        const int nothing = fed ? in[0] : open("/dev/null", O_RDONLY);
        dup2(nothing, 0);
        dup2(out[1], 1);
        dup2(err[1], 2);
        close(out[0]);
        close(out[1]);
        close(err[0]);
        close(err[1]);
        if (fed)
        {
            close(in[0]);
            close(in[1]);
        }
        if (detached)
        {
            setpgid(0, 0);
        }
        cli_spawn_lowered(priority);
        cli_spawn_settings(settings, count);
        if ((folder != NULL) && (chdir(folder) != 0))
        {
            _exit(127);
        }
        // exec takes its words as it was declared; it writes to none of them
        execvp(words[0], (char *const *)words);
        _exit(127);
    }
    close(out[1]);
    close(err[1]);
    if (fed)
    {
        close(in[0]);
    }
    // a process number and descriptors are not negative here, kept one past so 0 stands for none
    spawned->process = (unsigned long long)child;
    spawned->id = (unsigned long long)child;
    spawned->out = (unsigned long long)out[0] + 1ull;
    spawned->err = (unsigned long long)err[0] + 1ull;
    spawned->in = fed ? ((unsigned long long)in[1] + 1ull) : 0ull;
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
    return cli_spawn_fed_as(CLI_PRIORITY_WORK, spawned, words, folder, settings, count, detached, said);
}

int cli_spawn_fed_as(unsigned int priority, CliSpawned *spawned, const char *const *words, const char *folder,
                     const CliSetting *settings, unsigned int count, int detached, CliLine *said)
{
    // a write to a pipe whose reader has gone fails, and does not end this process
    signal(SIGPIPE, SIG_IGN);
    return cli_spawn_with(priority, spawned, words, folder, settings, count, detached, 1, said);
}

int cli_spawn_written(unsigned long long pipe_held, const void *bytes, unsigned long long size)
{
    const unsigned char *at = (const unsigned char *)bytes;
    while ((size > 0ull) && (pipe_held != 0ull))
    {
        const ssize_t written = write((int)(pipe_held - 1ull), at, (size_t)size);
        if (written <= 0)
        {
            return 0;
        }
        // a count written is positive here
        at += written;
        size -= (unsigned long long)written;
    }
    return size == 0ull;
}

void cli_spawn_input_closed(CliSpawned *spawned)
{
    if (spawned->in != 0ull)
    {
        close((int)(spawned->in - 1ull));
        spawned->in = 0ull;
    }
}

unsigned long long cli_spawn_read(unsigned long long pipe_held, char *piece, unsigned long long room)
{
    if (pipe_held == 0ull)
    {
        return 0ull;
    }
    const ssize_t read_count = read((int)(pipe_held - 1ull), piece, (size_t)room);
    // a count read is not negative here
    return (read_count > 0) ? (unsigned long long)read_count : 0ull;
}

long long cli_spawn_wait(CliSpawned *spawned)
{
    int status = 0;
    // the process number kept is the child's
    if (waitpid((pid_t)spawned->process, &status, 0) < 0)
    {
        return -1ll;
    }
    return WIFEXITED(status) ? (long long)WEXITSTATUS(status) : -1ll;
}

int cli_spawn_poll(CliSpawned *spawned, long long *code)
{
    int status = 0;
    // the process number kept is the child's
    const pid_t done = waitpid((pid_t)spawned->process, &status, WNOHANG);
    if (done == 0)
    {
        return 0;
    }
    *code = ((done > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1ll;
    return 1;
}

void cli_spawn_group_end(unsigned long long group)
{
    (void)group;
}

void cli_spawn_closed(CliSpawned *spawned)
{
    if (spawned->out != 0ull)
    {
        close((int)(spawned->out - 1ull));
    }
    if (spawned->err != 0ull)
    {
        close((int)(spawned->err - 1ull));
    }
    if (spawned->in != 0ull)
    {
        close((int)(spawned->in - 1ull));
    }
    memor.set(&(MemoriaCfg){.dst = spawned, .val = 0, .bytes = sizeof(*spawned)});
}

void cli_spawn_tree_end(unsigned long long id)
{
    // the group the run's process leads, asked to end; an id of 0 names no run, and kill of group 0 would end the
    // caller's own group
    if (id != 0ull)
    {
        kill(-(pid_t)id, SIGTERM);
    }
}

void cli_spawn_killed(const CliSpawned *spawned)
{
    // the process number kept is the child's
    kill((pid_t)spawned->process, SIGKILL);
}

int cli_spawn_interrupted(const CliSpawned *spawned)
{
    return kill((pid_t)spawned->process, SIGINT) == 0;
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
        if ((at[0] == ':') || (at[0] == '\0'))
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
        cli_line_text(&line, (at != 0u) ? ":" : "");
        cli_line_text(&line, folders[at]);
    }
    cli_line_closed(&line);
}
#endif
