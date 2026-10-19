// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_processes.c: the processes of the machine: on Linux the system's own list in /proc, each process's name from
// its comm and its line from its cmdline, the words apart by nuls, and a process that ends while the list is read left
// out; elsewhere as ps lists them, each line its number, its name and the line it was started with
#if !defined(_WIN32)
// O_CLOEXEC is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../text/json_tree.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>

// the most bytes of a process's name and line read
#define CLI_PROCESS_TEXT (1u << 16u)

static char s_name[CLI_PROCESS_TEXT];
static char s_command[CLI_PROCESS_TEXT];
static CliLock s_lock = CLI_LOCK_INIT;

#if defined(__linux__)
static char s_line[CLI_PROCESS_TEXT];

// the file at `path` read into `out`, which holds `room`: the bytes read, -1 where it cannot be opened
static long long processes_read(const char *path, char *out, unsigned long long room)
{
    const int file = open(path, O_RDONLY | O_CLOEXEC);
    if (file < 0)
    {
        return -1ll;
    }
    unsigned long long used = 0ull;
    for (;;)
    {
        const ssize_t count = read(file, out + used, room - used);
        if (count <= 0)
        {
            break;
        }
        used += (unsigned long long)count;
    }
    close(file);
    return (long long)used;
}

// The list being read: where each process goes
typedef struct
{
    void (*each)(void *context, unsigned int pid, const char *name, const char *command);
    void *context;
} CliProcessesRead;

// one entry of /proc read where its name is a process's number
static void processes_entry(void *context, const char *entry, int is_folder)
{
    const CliProcessesRead *const read_list = (const CliProcessesRead *)context;
    unsigned long long pid = 0ull;
    int number =
        is_folder && (entry[0] != '\0') && (cellul.len(&(CatenaFinitaCfg){.src = entry, .cap = CLI_CELLS_POOL}) <= 10u);
    for (const char *digit = entry; number && (*digit != '\0'); digit += 1)
    {
        number = (*digit >= '0') && (*digit <= '9');
        pid = (pid * 10ull) + (unsigned long long)(*digit - '0');
    }
    if (!number || (pid > 0xffffffffull))
    {
        return;
    }
    char path[64];
    CliLine named = {path, sizeof(path), 0ull};
    cli_line_text(&named, "/proc/");
    cli_line_text(&named, entry);
    cli_line_text(&named, "/comm");
    cli_line_closed(&named);
    const long long name_size = processes_read(path, s_name, sizeof(s_name) - 1u);
    if (name_size < 0ll)
    {
        return;
    }
    unsigned long long end = (unsigned long long)name_size;
    while ((end > 0ull) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = s_name[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    s_name[end] = '\0';
    named.at = 0ull;
    cli_line_text(&named, "/proc/");
    cli_line_text(&named, entry);
    cli_line_text(&named, "/cmdline");
    cli_line_closed(&named);
    const long long line_size = processes_read(path, s_line, sizeof(s_line));
    CliLine command = {s_command, sizeof(s_command), 0ull};
    unsigned long long word = 0ull;
    for (unsigned long long at = 0ull; at <= (unsigned long long)((line_size > 0ll) ? line_size : 0ll); at += 1ull)
    {
        const int ends = (at == (unsigned long long)((line_size > 0ll) ? line_size : 0ll)) || (s_line[at] == '\0');
        if (ends && (at > word))
        {
            cli_line_text(&command, (command.at > 0ull) ? " " : "");
            cli_lossy(&command, (const unsigned char *)s_line + word, at - word);
        }
        word = ends ? at + 1ull : word;
    }
    cli_line_closed(&command);
    read_list->each(read_list->context, (unsigned int)pid, s_name, s_command);
}

void cli_processes_each(void (*each)(void *context, unsigned int pid, const char *name, const char *command),
                        void *context)
{
    CliProcessesRead read_list = {each, context};
    cli_lock_take(&s_lock);
    (void)query_folder_each("/proc", processes_entry, &read_list);
    cli_lock_give(&s_lock);
}
#else
void cli_processes_each(void (*each)(void *context, unsigned int pid, const char *name, const char *command),
                        void *context)
{
    const char *const words[4] = {"ps", "-eo", "pid=,comm=,args=", NULL};
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    if (!cli_ran(words, NULL, NULL, 0u, ran, &said))
    {
        cli_ran_given(ran);
        return;
    }
    cli_lock_take(&s_lock);
    for (unsigned long long start = 0ull; start < ran->out_size;)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = ran->out + start, .val = '\n', .bytes = ran->out_size - start});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - ran->out) : ran->out_size;
        // the line's words apart by spaces: its number, its name, then the words of its line joined by one space
        unsigned long long pid = 0ull;
        unsigned int index = 0u;
        int number = 1;
        CliLine name = {s_name, sizeof(s_name), 0ull};
        CliLine command = {s_command, sizeof(s_command), 0ull};
        for (unsigned long long at = start; at < end;)
        {
            while ((at < end) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = ran->out[at]}) != NULL))
            {
                at += 1ull;
            }
            const unsigned long long first = at;
            while ((at < end) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = ran->out[at]}) == NULL))
            {
                at += 1ull;
            }
            if (at == first)
            {
                break;
            }
            for (unsigned long long digit = first; (index == 0u) && (digit < at); digit += 1ull)
            {
                number = number && (ran->out[digit] >= '0') && (ran->out[digit] <= '9') && (at - first <= 10ull);
                pid = (pid * 10ull) + (unsigned long long)(ran->out[digit] - '0');
            }
            if (index == 1u)
            {
                cli_lossy(&name, (const unsigned char *)ran->out + first, at - first);
            }
            else if (index > 1u)
            {
                cli_line_text(&command, (command.at > 0ull) ? " " : "");
                cli_lossy(&command, (const unsigned char *)ran->out + first, at - first);
            }
            index += 1u;
        }
        cli_line_closed(&name);
        cli_line_closed(&command);
        if ((index >= 2u) && number && (pid <= 0xffffffffull))
        {
            each(context, (unsigned int)pid, s_name, s_command);
        }
        start = end + 1ull;
    }
    cli_lock_give(&s_lock);
    cli_ran_given(ran);
}
#endif
#endif

#if !defined(_WIN32)
int cli_process_running(unsigned int pid)
{
    if (!query_is_folder("/proc"))
    {
        return 1;
    }
    char path[64];
    CliLine named = {path, sizeof(path), 0ull};
    cli_line_text(&named, "/proc/");
    cli_line_decimal(&named, pid, 1u);
    (void)cli_line_closed(&named);
    return query_is_folder(path);
}
#endif
