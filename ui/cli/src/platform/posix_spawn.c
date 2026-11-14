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
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#include <utmp.h>
#endif

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
    embed_word capacity;
    pid_t child;
    const char *entry;
#endif
} PosixSpawnCtx;

// the entries a child's environment holds at most, and the bytes its settings' pairs take at most
#define CLI_SPAWN_ENVIRONMENT 4096u
#define CLI_SPAWN_PAIRS (64u << 10u)

extern char **environ;

// whether the environment entry `entry`, name=value, is named by one of the settings
EMBED_INLINE int cli_spawn_entry_named(const PosixSpawnCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const name = args->settings[at].name;
        const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
        if ((args->entry[length] == '=') &&
            cellul.starts(
                &(CatenaFinitaCfg){.src = args->entry, .cap = CLI_POOL_BYTES, .other = name, .other_cap = length}))
        {
            return 1;
        }
    }
    return 0;
}

// the environment a child runs its program in, made in the child before it runs: this process's entries without
// those the settings name, then each setting with a value as name=value; an entry past the bounds is dropped
EMBED_INLINE void cli_spawn_settings(const PosixSpawnCtx *args)
{
    static char *s_entries[CLI_SPAWN_ENVIRONMENT + 1u];
    static char s_pairs[CLI_SPAWN_PAIRS];
    unsigned int entries = 0u;
    for (char **entry = environ; (*entry != NULL) && (entries < CLI_SPAWN_ENVIRONMENT); entry += 1)
    {
        if (!EMBED_CALL(cli_spawn_entry_named, PosixSpawnCtx, .settings = args->settings, .count = args->count,
                        .entry = *entry))
        {
            s_entries[entries] = *entry;
            entries += 1u;
        }
    }
    VerbaLinea pairs = {s_pairs, sizeof(s_pairs), 0ULL, NULL, NULL, 0ULL};
    for (unsigned int at = 0u; (at < args->count) && (entries < CLI_SPAWN_ENVIRONMENT); at += 1u)
    {
        if (args->settings[at].value == NULL)
        {
            continue;
        }
        const unsigned long long start = pairs.at;
        mmgr_line_put(&pairs, args->settings[at].name);
        mmgr_line_ch(&pairs, '=');
        mmgr_line_put(&pairs, args->settings[at].value);
        mmgr_line_ch(&pairs, '\0');
        if (!mmgr_line_ok(&pairs))
        {
            break;
        }
        s_entries[entries] = s_pairs + start;
        entries += 1u;
    }
    s_entries[entries] = NULL;
    environ = s_entries;
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
        execvp(args->words[0], (char *const *)(embed_word)args->words);
        _exit(127);
    }
    int status = 0;
    const pid_t waited = waitpid(child, &status, 0);
    *code = ((waited > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1LL;
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
        const int ends[6] = {out[0], err[0], in[0], out[1], err[1], in[1]};
        for (unsigned int end = 0u; end < 6u; end += 1u)
        {
            if (ends[end] >= 0)
            {
                close(ends[end]);
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
        if (nothing < 0)
        {
            _exit(127);
        }
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
        execvp(args->words[0], (char *const *)(embed_word)args->words);
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
    args->spawned->out = (unsigned long long)out[0] + 1ULL;
    args->spawned->err = (unsigned long long)err[0] + 1ULL;
    args->spawned->in = args->stdin_written ? ((unsigned long long)in[1] + 1ULL) : 0ULL;
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
    const unsigned int bounded = (args->count > 0x7fffu) ? 0x7fffu : args->count;
    return (unsigned short)((args->count < 2u) ? 2u : bounded);
}

int cli_spawn_terminal(const CliWordsErrMsgSpawnedCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
    // the system makes the pair and sizes the follower, as its own terminals do
    struct winsize size = {EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->rows),
                           EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->cols), 0u, 0u};
    int leader = -1;
    int follower = -1;
    if (openpty(&leader, &follower, NULL, NULL, &size) != 0)
    {
        mmgr_line_put(args->err_msg, "no pseudo-terminal could be made");
        return 0;
    }
    const int written = dup(leader);
    if (written < 0)
    {
        close(leader);
        close(follower);
        mmgr_line_put(args->err_msg, "no pseudo-terminal could be made");
        return 0;
    }
    (void)fcntl(leader, F_SETFD, FD_CLOEXEC);
    (void)fcntl(written, F_SETFD, FD_CLOEXEC);
    const pid_t child = fork();
    if (child < 0)
    {
        close(leader);
        close(written);
        close(follower);
        mmgr_line_put(args->err_msg, "no process could be made");
        return 0;
    }
    if (child == 0)
    {
        // a session of its own, whose controlling terminal the follower is, on its standard descriptors
        if (login_tty(follower) != 0)
        {
            _exit(127);
        }
        EMBED_CALL(cli_spawn_settings, PosixSpawnCtx, .settings = args->settings, .count = args->count);
        if ((args->folder != NULL) && (chdir(args->folder) != 0))
        {
            _exit(127);
        }
        // exec takes its words as it was declared; it writes to none of them
        execvp(args->words[0], (char *const *)(embed_word)args->words);
        _exit(127);
    }
    // the follower is the child's alone: a leader read sees its end once every holder of the follower ends
    close(follower);
    // a process number and descriptors are not negative here, stored one past so 0 stands for none
    args->spawned->process = (unsigned long long)child;
    args->spawned->id = (unsigned long long)child;
    args->spawned->out = (unsigned long long)leader + 1ULL;
    args->spawned->in = (unsigned long long)written + 1ULL;
    return 1;
}

int cli_spawn_terminal_resize(const CliColsCtx *args)
{
    const struct winsize size = {EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->rows),
                                 EMBED_CALL(cli_terminal_extent, PosixSpawnCtx, .count = args->cols), 0u, 0u};
    // a descriptor stored one past is one the system gave
    return (args->spawned_read->in != 0ULL) && (ioctl((int)(args->spawned_read->in - 1ULL), TIOCSWINSZ, &size) == 0);
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
    while ((size > 0ULL) && (args->pipe_handle != 0ULL))
    {
        const ssize_t written = write((int)(args->pipe_handle - 1ULL), at, (embed_word)size);
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

void cli_spawn_input_close(const CliSpawnedCtx *args)
{
    if (args->spawned->in != 0ULL)
    {
        close((int)(args->spawned->in - 1ULL));
        args->spawned->in = 0ULL;
    }
}

unsigned long long cli_spawn_read(const CliCapacityPipeHandleCtx *args)
{
    if (args->pipe_handle == 0ULL)
    {
        return 0ULL;
    }
    const ssize_t read_count = read((int)(args->pipe_handle - 1ULL), args->piece, (embed_word)args->capacity);
    // a count read is not negative here
    return (read_count > 0) ? (unsigned long long)read_count : 0ULL;
}

// the count answered where the system's byte count stands at 0 and a read still returns at once: a pseudo-terminal
// leader on macOS counts no waiting bytes, and the read answers how many there are
#define CLI_SPAWN_READ_READY 65536ULL

unsigned long long cli_spawn_waiting(const CliCapacityPipeHandleCtx *args)
{
    int waiting = 0;
    // a descriptor stored one past is one the system gave, and a count the system reports is not negative
    if ((args->pipe_handle == 0ULL) || (ioctl((int)(args->pipe_handle - 1ULL), FIONREAD, &waiting) != 0) ||
        (waiting < 0))
    {
        return 0ULL;
    }
    if (waiting == 0)
    {
        const int descriptor = (int)(args->pipe_handle - 1ULL);
        fd_set readable;
        struct timeval none = {0, 0};
        FD_ZERO(&readable);
        FD_SET(descriptor, &readable);
        if ((descriptor < FD_SETSIZE) && (select(descriptor + 1, &readable, NULL, NULL, &none) > 0))
        {
            return CLI_SPAWN_READ_READY;
        }
    }
    return (unsigned long long)waiting;
}

long long cli_spawn_wait(const CliSpawnedCtx *args)
{
    int status = 0;
    // the process number stored is the child's
    if (waitpid((pid_t)args->spawned->process, &status, 0) < 0)
    {
        return -1LL;
    }
    args->spawned->waited = 1ULL;
    return WIFEXITED(status) ? (long long)WEXITSTATUS(status) : -1LL;
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
    args->spawned->waited = 1ULL;
    *code = ((done > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1LL;
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
    if (args->spawned->out != 0ULL)
    {
        close((int)(args->spawned->out - 1ULL));
    }
    if (args->spawned->err != 0ULL)
    {
        close((int)(args->spawned->err - 1ULL));
    }
    if (args->spawned->in != 0ULL)
    {
        close((int)(args->spawned->in - 1ULL));
    }
    // a child not yet waited on is waited on now where it has ended, and once it ends where it goes on
    int status = 0;
    const int going = (args->spawned->process != 0ULL) && (args->spawned->waited == 0ULL) &&
                      (waitpid((pid_t)args->spawned->process, &status, WNOHANG) == 0);
    // the process number stored is the child's
    EMBED_CALL(cli_spawn_let_go, PosixSpawnCtx, .child = going ? (pid_t)args->spawned->process : 0);
    memor.set(&(MemoriaCfg){.dst = args->spawned, .val = 0, .bytes = sizeof(*args->spawned)});
}

void cli_spawn_tree_end(const CliIdCtx *args)
{
    // the group the run's process leads, requested to end; an id of 0 names no run, and kill of group 0 would end the
    // caller's own group
    if (args->id_u64 != 0ULL)
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
                VerbaLinea line = {args->folders[count], CLI_PATH_MAX_LEN, 0ULL};
                mmgr_line_put_n(&line, from, (unsigned long long)(at - from));
                count += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
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
    VerbaLinea line = {args->path_out, args->size, 0ULL};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? ":" : "");
        mmgr_line_put(&line, args->folders_callback[at]);
    }
    mmgr_line_finish(&line);
}
#endif
