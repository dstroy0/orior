// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix.c: the query's calls on POSIX: a process forked and run, its output piped and waited on; the environment;
// folders walked, made and removed; a file's time and the clock; a path made full; and the terminal
#if !defined(_WIN32)
// fork, exec, the environment, folders, the clock and nanosleep are POSIX, outside strict C11, and realpath is an
// X/Open extension glibc declares under _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../query_internal.h"

#if !defined(_WIN32)
#include "../../../cli/src/text/text.h"
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

int query_process_start(const QueryStartedCtx *args)
{
    int output = args->output;
    args->started->process = 0ULL;
    args->started->output = 0ULL;
    int pipe_ends[2] = {-1, -1};
    if (((output == QUERY_OUTPUT_PIPED) || (output == QUERY_OUTPUT_PIPED_ALONE)) && (pipe(pipe_ends) != 0))
    {
        return 0;
    }
    const pid_t child = fork();
    if (child < 0)
    {
        if (pipe_ends[0] >= 0)
        {
            close(pipe_ends[0]);
            close(pipe_ends[1]);
        }
        return 0;
    }
    if (child == 0)
    {
        const int nothing = open("/dev/null", O_RDWR);
        if (nothing < 0)
        {
            _exit(127);
        }
        dup2(nothing, 0);
        if (output == QUERY_OUTPUT_PIPED)
        {
            dup2(pipe_ends[1], 1);
            dup2(pipe_ends[1], 2);
            close(pipe_ends[0]);
            close(pipe_ends[1]);
        }
        else if (output == QUERY_OUTPUT_PIPED_ALONE)
        {
            dup2(pipe_ends[1], 1);
            dup2(nothing, 2);
            close(pipe_ends[0]);
            close(pipe_ends[1]);
        }
        else if (output == QUERY_OUTPUT_NONE)
        {
            dup2(nothing, 1);
            dup2(nothing, 2);
        }
        if ((args->folder != NULL) && (chdir(args->folder) != 0))
        {
            _exit(127);
        }
        // exec takes its words as it was declared; it writes to none of them
        execvp(args->words[0], (char *const *)(uintptr_t)args->words);
        _exit(127);
    }
    if (pipe_ends[1] >= 0)
    {
        close(pipe_ends[1]);
    }
    // a process number and a descriptor are not negative here, and an unsigned long long represents each
    args->started->process = (unsigned long long)child;
    args->started->output = (pipe_ends[0] >= 0) ? ((unsigned long long)pipe_ends[0] + 1ULL) : 0ULL;
    return 1;
}

unsigned long long query_process_read(const QueryProcessPieceCtx *args)
{
    if (args->process->output == 0ULL)
    {
        return 0ULL;
    }
    // the descriptor is stored one past itself, so that 0 stands for none
    const ssize_t got = read((int)(args->process->output - 1ULL), args->piece, (size_t)args->capacity_u64);
    return (got > 0) ? (unsigned long long)got : 0ULL;
}

long long query_process_wait(const QueryProcessCtx *args)
{
    QueryProcess *process = args->process_out;
    if (process->process == 0ULL)
    {
        return -1LL;
    }
    int status = 0;
    // the process number came from fork, which a pid_t represents
    const pid_t waited = waitpid((pid_t)process->process, &status, 0);
    if (process->output != 0ULL)
    {
        close((int)(process->output - 1ULL));
    }
    process->process = 0ULL;
    process->output = 0ULL;
    return ((waited > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1LL;
}

int query_environment_set(const QueryNameCtx *args)
{
    return setenv(args->name, args->value, 1) == 0;
}

int query_environment_read(const QueryNameValueCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    for (char **entry = environ; (entry != NULL) && (*entry != NULL); entry += 1)
    {
        if (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = *entry, .other_pointer = args->name,
                       .size = length) &&
            ((*entry)[length] == '='))
        {
            const char *const value_src = *entry + length + 1;
            const size_t value_length = cellul.len(&(CatenaFinitaCfg){.src = value_src, .cap = CLI_CELLS_POOL});
            if ((value_length == 0u) || (value_length >= args->capacity))
            {
                return 0;
            }
            memor.cpy(&(MemoriaCfg){.dst = args->value_out, .src = value_src, .bytes = value_length + 1u});
            return 1;
        }
    }
    return 0;
}

int query_environment_write(const QueryNameOutCtx *args)
{
    char s_value[QUERY_ENVIRONMENT_MAX_LEN];
    const int read = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = args->name,
                                .value_out = s_value, .capacity = sizeof(s_value));
    if (read)
    {
        mmgr_line_put(args->out, s_value);
    }
    return read && mmgr_line_ok(args->out);
}

int query_folder_each(const QueryFolderEachCtx *args)
{
    DIR *const walk = opendir(args->folder);
    if (walk == NULL)
    {
        return 0;
    }
    for (const struct dirent *entry = readdir(walk); entry != NULL; entry = readdir(walk))
    {
        if ((cellul.eq(&(CatenaFinitaCfg){.src = entry->d_name, .other = ".", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = entry->d_name, .other = "..", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        char path[QUERY_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ULL};
        mmgr_line_put(&line, args->folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, entry->d_name);
        EMBED_CALL(args->each, CliEachNameIsFolderCfg, .context = args->context_bytess, .name = entry->d_name,
                   .is_folder = (mmgr_line_finish(&line) != 0ULL) &&
                                EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path));
    }
    closedir(walk);
    return 1;
}

int query_folder_init(const QueryFolderContextCtx *args)
{
    char path[QUERY_PATH_MAX_LEN];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL});
    if (length >= sizeof(path))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = path, .src = args->folder, .bytes = length + 1u});
    for (size_t at = 1u; at <= length; at += 1u)
    {
        if ((path[at] == '/') || (path[at] == '\0'))
        {
            const char saved_char = path[at];
            path[at] = '\0';
            mkdir(path, 0750);
            path[at] = saved_char;
        }
    }
    return EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->folder);
}

// what a folder's removal walks: the folder and whether every entry went
typedef struct
{
    const char *folder;
    int whole;
} QueryRemoval;

static void query_entry_remove(const CliEachNameIsFolderCfg *args)
{
    QueryRemoval *const removal = (QueryRemoval *)args->context;
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, removal->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    removal->whole = (mmgr_line_finish(&line) != 0ULL) &&
                     EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path) &&
                     removal->whole;
}

int query_remove(const QueryPathSecondsCtx *args)
{
    struct stat st;
    if (lstat(args->path, &st) != 0)
    {
        return 1;
    }
    if (!S_ISDIR(st.st_mode))
    {
        return unlink(args->path) == 0;
    }
    QueryRemoval removal = {args->path, 1};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->path,
               .each = query_entry_remove, .context_bytess = &removal);
    return removal.whole && (rmdir(args->path) == 0);
}

int query_file_copy(const QueryFromCtx *args)
{
    const int source = open(args->from, O_RDONLY);
    if (source < 0)
    {
        return 0;
    }
    const int target = open(args->to, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    int whole = target >= 0;
    char piece[65536];
    ssize_t got = whole ? read(source, piece, sizeof(piece)) : 0;
    while (whole && (got > 0))
    {
        // a read that gave bytes gave a positive count, which a write puts whole or fails
        whole = write(target, piece, (size_t)got) == got;
        got = read(source, piece, sizeof(piece));
    }
    close(source);
    if (target >= 0)
    {
        close(target);
    }
    return whole && (got == 0);
}

int query_file_rename(const QueryFromCtx *args)
{
    // the file there let go, the name linked to the file moved, and its old name let go
    unlink(args->to);
    return (link(args->from, args->to) == 0) && (unlink(args->from) == 0);
}

int query_file_write(const QueryPathBytesSizeCtx *args)
{
    const int file = open(args->path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (file < 0)
    {
        return 0;
    }
    unsigned long long put = 0ULL;
    int whole = 1;
    while (whole && (put < args->size))
    {
        const ssize_t wrote = write(file, (const unsigned char *)args->bytes + put, (size_t)(args->size - put));
        whole = wrote > 0;
        // a write that put bytes put a positive count
        put += whole ? (unsigned long long)wrote : 0ULL;
    }
    return (close(file) == 0) && whole;
}

int query_is_file(const QueryPathSecondsCtx *args)
{
    struct stat st;
    return (stat(args->path, &st) == 0) && S_ISREG(st.st_mode);
}

int query_is_folder(const QueryPathSecondsCtx *args)
{
    struct stat st;
    return (stat(args->path, &st) == 0) && S_ISDIR(st.st_mode);
}

int query_modify(const QueryPathSecondsCtx *args)
{
    unsigned long long *seconds = args->seconds;
    struct stat st;
    if (stat(args->path, &st) != 0)
    {
        return 0;
    }
    // a time a file changed is after 1970 here
    *seconds = (st.st_mtime > 0) ? (unsigned long long)st.st_mtime : 0ULL;
    return 1;
}

int query_modified_set(const QueryPathSecondsKindCtx *args)
{
    struct timespec times[2];
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;
    // a time after 1970 that a time_t represents
    times[1].tv_sec = (time_t)args->seconds_value;
    times[1].tv_nsec = 0;
    return utimensat(AT_FDCWD, args->path, times, 0) == 0;
}

unsigned long long query_now(void)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    // the clock reads after 1970
    return (now.tv_sec > 0) ? (unsigned long long)now.tv_sec : 0ULL;
}

void query_pause(const QueryMillisecondsCtx *args)
{
    struct timespec pause = {(time_t)(args->milliseconds / 1000u), (long)(args->milliseconds % 1000u) * 1000000L};
    nanosleep(&pause, NULL);
}

int query_path_full(const QueryCapacityPathCtx *args)
{
    char resolved[PATH_MAX];
    if (realpath(args->path, resolved) != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->full,
                                .src = resolved,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = resolved, .cap = CLI_CELLS_POOL}) + 1u});
        return args->full[0] != '\0';
    }
    // a path not there yet is made full against the folder this process stands in
    if (args->path[0] == '/')
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->full,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
        return args->full[0] != '\0';
    }
    char here[PATH_MAX];
    if (getcwd(here, sizeof(here)) == NULL)
    {
        return 0;
    }
    VerbaLinea line = {args->full, args->capacity, 0ULL};
    mmgr_line_put(&line, here);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->path);
    return mmgr_line_finish(&line) != 0ULL;
}

int query_terminal_is(void)
{
    return isatty(0);
}

unsigned long long query_terminal_line_read(const QueryCapacityLineCtx *args)
{
    size_t length = 0u;
    unsigned long long whole = 0ULL;
    int read_any = 0;
    for (;;)
    {
        char letter = '\0';
        if (read(0, &letter, 1u) != 1)
        {
            if (!read_any)
            {
                return QRY_MEMBER_UNREAD;
            }
            break;
        }
        read_any = 1;
        if (letter == '\n')
        {
            break;
        }
        if (letter != '\r')
        {
            whole += 1ULL;
            if ((length + 1u) < args->capacity)
            {
                args->line[length] = letter;
                length += 1u;
            }
        }
    }
    args->line[length] = '\0';
    return whole;
}

void query_terminal_write(const QueryErrorsCtx *args)
{
    unsigned long long put = 0ULL;
    while (put < args->length)
    {
        const ssize_t wrote = write(args->errors ? 2 : 1, args->text + put, (size_t)(args->length - put));
        if (wrote <= 0)
        {
            return;
        }
        // a write that put bytes put a positive count
        put += (unsigned long long)wrote;
    }
}

#endif
