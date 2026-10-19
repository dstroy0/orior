// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix.c: the query's calls on POSIX: a process forked and run, its output piped and waited on; the environment;
// folders walked, made and removed; a file's time and the clock; a path made full; and the terminal
#if !defined(_WIN32)
// fork, exec, the environment, folders, the clock and nanosleep are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
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

int query_process_start(QueryProcess *started, const char *const *words, const char *folder, int output)
{
    started->process = 0ull;
    started->output = 0ull;
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
        if ((folder != NULL) && (chdir(folder) != 0))
        {
            _exit(127);
        }
        // exec takes its words as it was declared; it writes to none of them
        execvp(words[0], (char *const *)words);
        _exit(127);
    }
    if (pipe_ends[1] >= 0)
    {
        close(pipe_ends[1]);
    }
    // a process number and a descriptor are not negative here, and an unsigned long long holds each
    started->process = (unsigned long long)child;
    started->output = (pipe_ends[0] >= 0) ? ((unsigned long long)pipe_ends[0] + 1ull) : 0ull;
    return 1;
}

unsigned long long query_process_read(const QueryProcess *process, char *piece, unsigned long long room)
{
    if (process->output == 0ull)
    {
        return 0ull;
    }
    // the descriptor is held one past itself, so that 0 stands for none
    const ssize_t got = read((int)(process->output - 1ull), piece, (size_t)room);
    return (got > 0) ? (unsigned long long)got : 0ull;
}

long long query_process_wait(QueryProcess *process)
{
    if (process->process == 0ull)
    {
        return -1ll;
    }
    int status = 0;
    // the process number came from fork, which a pid_t holds
    const pid_t waited = waitpid((pid_t)process->process, &status, 0);
    if (process->output != 0ull)
    {
        close((int)(process->output - 1ull));
    }
    process->process = 0ull;
    process->output = 0ull;
    return ((waited > 0) && WIFEXITED(status)) ? (long long)WEXITSTATUS(status) : -1ll;
}

int query_environment_set(const char *name, const char *value)
{
    return setenv(name, value, 1) == 0;
}

int query_environment_read(const char *name, char *value, size_t room)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (char **entry = environ; (entry != NULL) && (*entry != NULL); entry += 1)
    {
        if (cli_text_same(*entry, name, length) && ((*entry)[length] == '='))
        {
            const char *const held = *entry + length + 1;
            const size_t held_length = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL});
            if ((held_length == 0u) || (held_length >= room))
            {
                return 0;
            }
            memor.cpy(&(MemoriaCfg){.dst = value, .src = held, .bytes = held_length + 1u});
            return 1;
        }
    }
    return 0;
}

int query_environment_said(const char *name, CliLine *out)
{
    static char s_value[QUERY_ENVIRONMENT_LONGEST];
    static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
    (void)pthread_mutex_lock(&s_lock);
    const int read = query_environment_read(name, s_value, sizeof(s_value));
    if (read)
    {
        cli_line_text(out, s_value);
    }
    (void)pthread_mutex_unlock(&s_lock);
    return read && cli_line_fits(out);
}

int query_folder_each(const char *folder, void (*each)(void *context, const char *name, int is_folder), void *context)
{
    DIR *const walk = opendir(folder);
    if (walk == NULL)
    {
        return 0;
    }
    for (struct dirent *entry = readdir(walk); entry != NULL; entry = readdir(walk))
    {
        if ((cellul.eq(&(CatenaFinitaCfg){.src = entry->d_name, .other = ".", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = entry->d_name, .other = "..", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        char path[QUERY_PATH_LONGEST];
        CliLine line = {path, sizeof(path), 0ull};
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
        cli_line_text(&line, entry->d_name);
        each(context, entry->d_name, (cli_line_closed(&line) != 0ull) && query_is_folder(path));
    }
    closedir(walk);
    return 1;
}

int query_folder_made(const char *folder)
{
    char path[QUERY_PATH_LONGEST];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    if (length >= sizeof(path))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = path, .src = folder, .bytes = length + 1u});
    for (size_t at = 1u; at <= length; at += 1u)
    {
        if ((path[at] == '/') || (path[at] == '\0'))
        {
            const char held = path[at];
            path[at] = '\0';
            mkdir(path, 0755);
            path[at] = held;
        }
    }
    return query_is_folder(folder);
}

// what a folder's removal walks: the folder and whether every entry went
typedef struct
{
    const char *folder;
    int whole;
} QueryRemoval;

static void query_entry_removed(void *context, const char *name, int is_folder)
{
    QueryRemoval *const removal = (QueryRemoval *)context;
    char path[QUERY_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, removal->folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    (void)is_folder;
    removal->whole = (cli_line_closed(&line) != 0ull) && query_removed(path) && removal->whole;
}

int query_removed(const char *path)
{
    struct stat held;
    if (lstat(path, &held) != 0)
    {
        return 1;
    }
    if (!S_ISDIR(held.st_mode))
    {
        return unlink(path) == 0;
    }
    QueryRemoval removal = {path, 1};
    query_folder_each(path, query_entry_removed, &removal);
    return removal.whole && (rmdir(path) == 0);
}

int query_file_copied(const char *from, const char *to)
{
    const int source = open(from, O_RDONLY);
    if (source < 0)
    {
        return 0;
    }
    const int target = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

int query_file_renamed(const char *from, const char *to)
{
    // the file there let go, the name linked to the file moved, and its old name let go
    unlink(to);
    return (link(from, to) == 0) && (unlink(from) == 0);
}

int query_file_written(const char *path, const void *bytes, unsigned long long size)
{
    const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file < 0)
    {
        return 0;
    }
    unsigned long long put = 0ull;
    int whole = 1;
    while (whole && (put < size))
    {
        const ssize_t wrote = write(file, (const unsigned char *)bytes + put, (size_t)(size - put));
        whole = wrote > 0;
        // a write that put bytes put a positive count
        put += whole ? (unsigned long long)wrote : 0ull;
    }
    return (close(file) == 0) && whole;
}

int query_is_file(const char *path)
{
    struct stat held;
    return (stat(path, &held) == 0) && S_ISREG(held.st_mode);
}

int query_is_folder(const char *path)
{
    struct stat held;
    return (stat(path, &held) == 0) && S_ISDIR(held.st_mode);
}

int query_modified(const char *path, unsigned long long *seconds)
{
    struct stat held;
    if (stat(path, &held) != 0)
    {
        return 0;
    }
    // a time a file changed is after 1970 here
    *seconds = (held.st_mtime > 0) ? (unsigned long long)held.st_mtime : 0ull;
    return 1;
}

int query_modified_set(const char *path, unsigned long long seconds)
{
    struct timespec times[2];
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;
    // a time after 1970 that a time_t holds
    times[1].tv_sec = (time_t)seconds;
    times[1].tv_nsec = 0;
    return utimensat(AT_FDCWD, path, times, 0) == 0;
}

unsigned long long query_now(void)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    // the clock reads after 1970
    return (now.tv_sec > 0) ? (unsigned long long)now.tv_sec : 0ull;
}

void query_paused(unsigned int milliseconds)
{
    struct timespec pause = {(time_t)(milliseconds / 1000u), (long)(milliseconds % 1000u) * 1000000L};
    nanosleep(&pause, NULL);
}

int query_path_full(const char *path, char *full, size_t room)
{
    char resolved[PATH_MAX];
    if (realpath(path, resolved) != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = resolved,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = resolved, .cap = CLI_CELLS_POOL}) + 1u});
        return full[0] != '\0';
    }
    // a path not there yet is made full against the folder this process stands in
    if (path[0] == '/')
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        return full[0] != '\0';
    }
    char here[PATH_MAX];
    if (getcwd(here, sizeof(here)) == NULL)
    {
        return 0;
    }
    CliLine line = {full, room, 0ull};
    cli_line_text(&line, here);
    cli_line_character(&line, '/');
    cli_line_text(&line, path);
    return cli_line_closed(&line) != 0ull;
}

int query_terminal_is(void)
{
    return isatty(0);
}

unsigned long long query_terminal_line_read(char *line, size_t room)
{
    size_t length = 0u;
    unsigned long long whole = 0ull;
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
            whole += 1ull;
            if ((length + 1u) < room)
            {
                line[length] = letter;
                length += 1u;
            }
        }
    }
    line[length] = '\0';
    return whole;
}

void query_terminal_written(int errors, const char *text, unsigned long long length)
{
    unsigned long long put = 0ull;
    while (put < length)
    {
        const ssize_t wrote = write(errors ? 2 : 1, text + put, (size_t)(length - put));
        if (wrote <= 0)
        {
            return;
        }
        // a write that put bytes put a positive count
        put += (unsigned long long)wrote;
    }
}

#endif
