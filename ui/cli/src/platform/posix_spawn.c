// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_spawn.c: a run's process started on Linux and macOS: forked, its settings made in the child before it runs the
// program, its output and errors piped back apart, and, detached, in a process group of its own, which a signal to the
// window's group does not reach and which a stop ends whole; or started in a pseudo-terminal of its own, a terminal's
// shell leading a session whose controlling terminal it is, its output read and its keys written through the leader
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
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

typedef struct
{
#if (!defined(_WIN32))
    const CliSetting *settings;
    unsigned int count;
    unsigned int priority;
    CliSpawned *spawned;
    const char *const *words;
    const char *folder;
    int detached;
    int stdin_written;
    VerbaLinea *err_msg;
    int leader;
    char *name;
    size_t capacity;
    pid_t child;
#endif
} PosixSpawnCtx;

// the settings made in this process's environment, as a child makes them before it runs its program
EMBED_INLINE void cli_spawn_settings(const PosixSpawnCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (args->settings[at].value != NULL)
        {
            setenv(args->settings[at].name, args->settings[at].value, 1);
        }
        else
        {
            unsetenv(args->settings[at].name);
        }
    }
}

int cli_spawn_share(const CliWordsErrMsgSettingsCtx *args)
{
    long long *code = args->code;
    const pid_t child = fork();
    if (child < 0)
    {
        mmgr_line_put(args->err_msg, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        EMBED_CALL(cli_spawn_settings, PosixSpawnCtx, .settings = args->settings, .count = args->count);
        // exec takes its words as it was declared; it writes to none of them
        execvp(args->words[0], (char *const *)args->words);
        _exit(127);
    }
    int status = 0;
    const pid_t waited = waitpid(child, &status, 0);
    *code = ((waited > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1ll;
    return 1;
}

// the child put at `priority` before it runs its program: a guest or an idle one niced, an idle one on the idle policy
// where the system has it; a run's process retains the level it was started at, as raising one takes a privilege orior
// does not request
EMBED_INLINE void cli_spawn_lower(const PosixSpawnCtx *args)
{
    if ((args->priority != CLI_PRIORITY_GUEST) && (args->priority != CLI_PRIORITY_IDLE))
    {
        return;
    }
    setpriority(PRIO_PROCESS, 0, (args->priority == CLI_PRIORITY_IDLE) ? 19 : 9);
#if defined(__linux__)
    if (args->priority == CLI_PRIORITY_IDLE)
    {
        const struct sched_param none = {0};
        sched_setscheduler(0, SCHED_IDLE, &none);
    }
#endif
}

// a process started, its standard input a pipe this process feeds where `stdin_written`, else /dev/null, at `priority`
static int cli_spawn_with(const PosixSpawnCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
    int out[2] = {-1, -1};
    int err[2] = {-1, -1};
    int in[2] = {-1, -1};
    if ((pipe(out) != 0) || (pipe(err) != 0) || (args->stdin_written && (pipe(in) != 0)))
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
        mmgr_line_put(args->err_msg, "no pipe could be made");
        return 0;
    }
    const pid_t child = fork();
    if (child < 0)
    {
        close(out[0]);
        close(out[1]);
        close(err[0]);
        close(err[1]);
        if (args->stdin_written)
        {
            close(in[0]);
            close(in[1]);
        }
        mmgr_line_put(args->err_msg, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        const int nothing = args->stdin_written ? in[0] : open("/dev/null", O_RDONLY);
        dup2(nothing, 0);
        dup2(out[1], 1);
        dup2(err[1], 2);
        close(out[0]);
        close(out[1]);
        close(err[0]);
        close(err[1]);
        if (args->stdin_written)
        {
            close(in[0]);
            close(in[1]);
        }
        if (args->detached)
        {
            setpgid(0, 0);
        }
        cli_spawn_lower(args);
        cli_spawn_settings(args);
        if ((args->folder != NULL) && (chdir(args->folder) != 0))
        {
            _exit(127);
        }
        // exec takes its words as it was declared; it writes to none of them
        execvp(args->words[0], (char *const *)args->words);
        _exit(127);
    }
    close(out[1]);
    close(err[1]);
    if (args->stdin_written)
    {
        close(in[0]);
    }
    // a process number and descriptors are not negative here, stored one past so 0 stands for none
    args->spawned->process = (unsigned long long)child;
    args->spawned->id = (unsigned long long)child;
    args->spawned->out = (unsigned long long)out[0] + 1ull;
    args->spawned->err = (unsigned long long)err[0] + 1ull;
    args->spawned->in = args->stdin_written ? ((unsigned long long)in[1] + 1ull) : 0ull;
    return 1;
}

int cli_spawn(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(cli_spawn_with, PosixSpawnCtx, .priority = CLI_PRIORITY_WORK, .spawned = args->spawned,
                      .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                      .detached = args->detached, .stdin_written = 0, .err_msg = args->err_msg);
}

int cli_spawn_as(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(cli_spawn_with, PosixSpawnCtx, .priority = args->priority, .spawned = args->spawned,
                      .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                      .detached = args->detached, .stdin_written = 0, .err_msg = args->err_msg);
}

int cli_spawn_feed(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(cli_words_err_msg_internal.spawn_piped_as, CliWordsErrMsgCtx, .priority = CLI_PRIORITY_WORK,
                      .spawned = args->spawned, .words = args->words, .folder = args->folder,
                      .settings = args->settings, .count = args->count, .detached = args->detached,
                      .err_msg = args->err_msg);
}

int cli_spawn_piped_as(const CliWordsErrMsgCtx *args)
{
    // a write to a pipe whose reader has gone fails, and does not end this process
    signal(SIGPIPE, SIG_IGN);
    return EMBED_CALL(cli_spawn_with, PosixSpawnCtx, .priority = args->priority, .spawned = args->spawned,
                      .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                      .detached = args->detached, .stdin_written = 1, .err_msg = args->err_msg);
}

// a terminal's columns or rows as the system counts them, two at the fewest
EMBED_INLINE unsigned short cli_terminal_extent(const PosixSpawnCtx *args)
{
    // a count saved between 2 and the most a short counts is what an unsigned short stores
    return (unsigned short)((args->count < 2u) ? 2u : ((args->count > 0x7fffu) ? 0x7fffu : args->count));
}

// the name of the terminal the pseudo-terminal `leader` leads into `name`, sized for `capacity`: 1, or 0 where the
// system names none; ptsname's own capacity is shared, and read under a lock where ptsname_r is not the system's
EMBED_INLINE int cli_terminal_by_name(const PosixSpawnCtx *args)
{
#if defined(__linux__)
    return ptsname_r(args->leader, args->name, args->capacity) == 0;
#else
    const char *const named = ptsname(args->leader);
    VerbaLinea line = {args->name, args->capacity, 0ull};
    mmgr_line_put(&line, (named != NULL) ? named : "");
    const int whole = (named != NULL) && (mmgr_line_finish(&line) != 0ull);
    return whole;
#endif
}

int cli_spawn_terminal(const CliWordsErrMsgSpawnedCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
    char follower[CLI_PATH_MAX_LEN];
    const int leader = posix_openpt(O_RDWR | O_NOCTTY);
    const int opened = (leader >= 0) && (grantpt(leader) == 0) && (unlockpt(leader) == 0) &&
                       EMBED_CALL(cli_terminal_by_name, PosixSpawnCtx, .leader = leader, .name = follower,
                                  .capacity = sizeof(follower));
    const int written = opened ? dup(leader) : -1;
    if (!opened || (written < 0))
    {
        if (leader >= 0)
        {
            close(leader);
        }
        mmgr_line_put(args->err_msg, "no pseudo-terminal could be made");
        return 0;
    }
    (void)fcntl(leader, F_SETFD, FD_CLOEXEC);
    (void)fcntl(written, F_SETFD, FD_CLOEXEC);
    const struct winsize size = {EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->rows),
                                 EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->cols), 0u, 0u};
    (void)ioctl(leader, TIOCSWINSZ, &size);
    const pid_t child = fork();
    if (child < 0)
    {
        close(leader);
        close(written);
        mmgr_line_put(args->err_msg, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        // a session of its own, whose controlling terminal the pseudo-terminal's follower is
        setsid();
        const int terminal = open(follower, O_RDWR);
        if (terminal < 0)
        {
            _exit(127);
        }
        (void)ioctl(terminal, TIOCSCTTY, 0);
        dup2(terminal, 0);
        dup2(terminal, 1);
        dup2(terminal, 2);
        if (terminal > 2)
        {
            close(terminal);
        }
        EMBED_CALL(cli_spawn_settings, PosixSpawnCtx, .settings = args->settings, .count = args->count);
        if ((args->folder != NULL) && (chdir(args->folder) != 0))
        {
            _exit(127);
        }
        // exec takes its words as it was declared; it writes to none of them
        execvp(args->words[0], (char *const *)args->words);
        _exit(127);
    }
    // a process number and descriptors are not negative here, stored one past so 0 stands for none
    args->spawned->process = (unsigned long long)child;
    args->spawned->id = (unsigned long long)child;
    args->spawned->out = (unsigned long long)leader + 1ull;
    args->spawned->in = (unsigned long long)written + 1ull;
    return 1;
}

int cli_spawn_terminal_resize(const CliColsCtx *args)
{
    const struct winsize size = {EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->rows),
                                 EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->cols), 0u, 0u};
    // a descriptor stored one past is one the system gave
    return (args->spawned_read->in != 0ull) && (ioctl((int)(args->spawned_read->in - 1ull), TIOCSWINSZ, &size) == 0);
}

void cli_spawn_terminal_hung_up(const CliColsCtx *args)
{
    // the shell leads a session and a group of its own, whose every process the terminal's hang-up reaches
    kill(-(pid_t)args->spawned_read->process, SIGHUP);
}

void cli_spawn_terminal_close(const CliSpawnedCtx *args)
{
    (void)args;
    // a reader of the leader sees its end once the shell and every process holding the follower end
}

int cli_spawn_write(const CliPipeHandleCtx *args)
{
    unsigned long long size = args->size;
    const unsigned char *at = (const unsigned char *)args->bytes;
    while ((size > 0ull) && (args->pipe_handle != 0ull))
    {
        const ssize_t written = write((int)(args->pipe_handle - 1ull), at, (size_t)size);
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

void cli_spawn_input_close(const CliSpawnedCtx *args)
{
    if (args->spawned->in != 0ull)
    {
        close((int)(args->spawned->in - 1ull));
        args->spawned->in = 0ull;
    }
}

unsigned long long cli_spawn_read(const CliCapacityPipeHandleCtx *args)
{
    if (args->pipe_handle == 0ull)
    {
        return 0ull;
    }
    const ssize_t read_count = read((int)(args->pipe_handle - 1ull), args->piece, (size_t)args->capacity);
    // a count read is not negative here
    return (read_count > 0) ? (unsigned long long)read_count : 0ull;
}

long long cli_spawn_wait(const CliSpawnedCtx *args)
{
    int status = 0;
    // the process number stored is the child's
    if (waitpid((pid_t)args->spawned->process, &status, 0) < 0)
    {
        return -1ll;
    }
    args->spawned->waited = 1ull;
    return WIFEXITED(status) ? (long long)WEXITSTATUS(status) : -1ll;
}

int cli_spawn_poll(const CliSpawnedCtx *args)
{
    long long *code = args->code;
    int status = 0;
    // the process number stored is the child's
    const pid_t done = waitpid((pid_t)args->spawned->process, &status, WNOHANG);
    if (done == 0)
    {
        return 0;
    }
    args->spawned->waited = 1ull;
    *code = ((done > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1ll;
    return 1;
}

// the children let go of before they ended, each waited on once it ends so that none is left a zombie: the most let
// go of at once
#define CLI_SPAWN_LET_GO_MAX 256u
static pid_t s_let_go[CLI_SPAWN_LET_GO_MAX];

// `child`, let go of before it ended, put among the children let go of where `child` is not 0, and every one of them
// that has ended waited on and its place freed
EMBED_INLINE void cli_spawn_let_go(const PosixSpawnCtx *args)
{
    int positioned = (args->child == 0);
    for (unsigned int at = 0u; at < CLI_SPAWN_LET_GO_MAX; at += 1u)
    {
        int status = 0;
        if ((s_let_go[at] != 0) && (waitpid(s_let_go[at], &status, WNOHANG) != 0))
        {
            s_let_go[at] = 0;
        }
        if (!positioned && (s_let_go[at] == 0))
        {
            s_let_go[at] = args->child;
            positioned = 1;
        }
    }
}

void cli_spawn_group_end(const CliGroupCtx *args)
{
    (void)args;
}

void cli_spawn_close(const CliSpawnedCtx *args)
{
    if (args->spawned->out != 0ull)
    {
        close((int)(args->spawned->out - 1ull));
    }
    if (args->spawned->err != 0ull)
    {
        close((int)(args->spawned->err - 1ull));
    }
    if (args->spawned->in != 0ull)
    {
        close((int)(args->spawned->in - 1ull));
    }
    // a child not yet waited on is waited on now where it has ended, and once it ends where it goes on
    int status = 0;
    const int going = (args->spawned->process != 0ull) && (args->spawned->waited == 0ull) &&
                      (waitpid((pid_t)args->spawned->process, &status, WNOHANG) == 0);
    // the process number stored is the child's
    EMBED_CALL(cli_spawn_let_go, PosixSpawnCtx, .child = going ? (pid_t)args->spawned->process : 0);
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
}

void cli_spawn_tree_end(const CliIdCtx *args)
{
    // the group the run's process leads, requested to end; an id of 0 names no run, and kill of group 0 would end the
    // caller's own group
    if (args->id_u64 != 0ull)
    {
        kill(-(pid_t)args->id_u64, SIGTERM);
    }
}

void cli_spawn_kill(const CliColsCtx *args)
{
    // the process number stored is the child's
    kill((pid_t)args->spawned_read->process, SIGKILL);
}

int cli_spawn_interrupt(const CliColsCtx *args)
{
    return kill((pid_t)args->spawned_read->process, SIGINT) == 0;
}

unsigned int cli_path_folders(const CliCapacityFoldersCtx *args)
{
    unsigned int capacity = args->capacity_u32;
    static char s_path[QUERY_ENVIRONMENT_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH", .value_out = s_path,
                    .capacity = sizeof(s_path)))
    {
        return 0u;
    }
    unsigned int count = 0u;
    const char *from = s_path;
    for (const char *at = s_path;; at += 1)
    {
        if ((at[0] == ':') || (at[0] == '\0'))
        {
            if ((at > from) && (count < capacity))
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
        mmgr_line_put(&line, (at != 0u) ? ":" : "");
        mmgr_line_put(&line, args->folders_callback[at]);
    }
    mmgr_line_finish(&line);
}
#endif
