// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_processes.c: the processes of the machine: on Linux the system's own list in /proc, each process's name from
// its comm and its line from its cmdline, the words apart by nuls, and a process that ends while the list is read left
// out; elsewhere as ps lists them, each line its number, its name and the line it was started with; and a process's
// memory and its program's name, on Linux from /proc and on macOS as libproc gives them
#if !defined(_WIN32)
// O_CLOEXEC is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../text/json_tree.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <libproc.h>
#endif

// the most bytes of a process's name and line read
#define CLI_PROCESS_TEXT (1u << 16u)

static char s_name[CLI_PROCESS_TEXT];
static char s_command[CLI_PROCESS_TEXT];
static CliLock s_lock = CLI_LOCK_INIT;

#if defined(__linux__)
static char s_line[CLI_PROCESS_TEXT];

// the file at `path` read into `out`, sized for `room`: the bytes read, -1 where it cannot be opened
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
    VerbaLinea named = {path, sizeof(path), 0ull};
    mmgr_line_put(&named, "/proc/");
    mmgr_line_put(&named, entry);
    mmgr_line_put(&named, "/comm");
    mmgr_line_finish(&named);
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
    mmgr_line_put(&named, "/proc/");
    mmgr_line_put(&named, entry);
    mmgr_line_put(&named, "/cmdline");
    mmgr_line_finish(&named);
    const long long line_size = processes_read(path, s_line, sizeof(s_line));
    VerbaLinea command = {s_command, sizeof(s_command), 0ull};
    unsigned long long word = 0ull;
    for (unsigned long long at = 0ull; at <= (unsigned long long)((line_size > 0ll) ? line_size : 0ll); at += 1ull)
    {
        const int ends = (at == (unsigned long long)((line_size > 0ll) ? line_size : 0ll)) || (s_line[at] == '\0');
        if (ends && (at > word))
        {
            mmgr_line_put(&command, (command.at > 0ull) ? " " : "");
            cli_lossy(&command, (const unsigned char *)s_line + word, at - word);
        }
        word = ends ? at + 1ull : word;
    }
    mmgr_line_finish(&command);
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
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    if (!cli_ran(words, NULL, NULL, 0u, ran, &err_msg))
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
        VerbaLinea name = {s_name, sizeof(s_name), 0ull};
        VerbaLinea command = {s_command, sizeof(s_command), 0ull};
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
                mmgr_line_put(&command, (command.at > 0ull) ? " " : "");
                cli_lossy(&command, (const unsigned char *)ran->out + first, at - first);
            }
            index += 1u;
        }
        mmgr_line_finish(&name);
        mmgr_line_finish(&command);
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
    VerbaLinea named = {path, sizeof(path), 0ull};
    mmgr_line_put(&named, "/proc/");
    mmgr_line_decimal(&named, pid, 1u);
    (void)mmgr_line_finish(&named);
    return query_is_folder(path);
}

#if defined(__linux__)
// the kilobytes the line of `text` that starts with `field` gives, in bytes, 0 where no line gives them
static unsigned long long processes_kilobytes(const char *text, const char *field)
{
    const char *at = cellul.find(
        &(CatenaFinitaCfg){.src = text, .cap = CLI_PROCESS_TEXT, .other = field, .other_cap = CLI_CELLS_POOL});
    if (at == NULL)
    {
        return 0ull;
    }
    at += cellul.len(&(CatenaFinitaCfg){.src = field, .cap = CLI_CELLS_POOL});
    while ((*at == ' ') || (*at == '\t'))
    {
        at += 1;
    }
    unsigned long long kilobytes = 0ull;
    while ((*at >= '0') && (*at <= '9'))
    {
        kilobytes = (kilobytes * 10ull) + (unsigned long long)(*at - '0');
        at += 1;
    }
    return kilobytes * 1024ull;
}

// the file `file` of the process `pid`'s folder in /proc read into `out`, sized for CLI_PROCESS_TEXT, a 0 after what
// was read: 1, or 0 where it cannot be read
static int processes_proc_read(unsigned int pid, const char *file, char *out)
{
    char path[64];
    VerbaLinea named = {path, sizeof(path), 0ull};
    mmgr_line_put(&named, "/proc/");
    mmgr_line_decimal(&named, pid, 1u);
    mmgr_line_put(&named, file);
    (void)mmgr_line_finish(&named);
    const long long size = processes_read(path, out, CLI_PROCESS_TEXT - 1u);
    // a size read is below the room, which an unsigned long long counts
    out[(size > 0ll) ? (unsigned long long)size : 0ull] = '\0';
    return size >= 0ll;
}

int cli_process_memory(unsigned int pid, CliProcessMemory *memory)
{
    cli_lock_take(&s_lock);
    const int read =
        processes_proc_read(pid, "/smaps_rollup", s_line) && processes_proc_read(pid, "/status", s_command);
    if (read)
    {
        memory->working = processes_kilobytes(s_line, "Private_Clean:") + processes_kilobytes(s_line, "Private_Dirty:");
        memory->commit = processes_kilobytes(s_command, "RssAnon:") + processes_kilobytes(s_command, "VmSwap:");
        memory->started = 0ull;
    }
    cli_lock_give(&s_lock);
    return read;
}

int cli_process_named(unsigned int pid, char *name, size_t room)
{
    cli_lock_take(&s_lock);
    const int read = processes_proc_read(pid, "/comm", s_name);
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = s_name, .cap = CLI_PROCESS_TEXT});
    while ((end > 0ull) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = s_name[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    VerbaLinea written = {name, room, 0ull};
    mmgr_line_put_n(&written, s_name, end);
    (void)mmgr_line_finish(&written);
    cli_lock_give(&s_lock);
    return read;
}
#elif defined(__APPLE__)
int cli_process_memory(unsigned int pid, CliProcessMemory *memory)
{
    struct rusage_info_v4 info;
    // a process id the system gives is below what an int counts, and the call writes a rusage_info_v4 where it is asked
    // for RUSAGE_INFO_V4
    if (proc_pid_rusage((int)pid, RUSAGE_INFO_V4, (rusage_info_t *)&info) != 0)
    {
        return 0;
    }
    memory->working = info.ri_phys_footprint;
    memory->commit = info.ri_phys_footprint;
    memory->started = 0ull;
    return 1;
}

int cli_process_named(unsigned int pid, char *name, size_t room)
{
    // a process id the system gives is below what an int counts, and a room is far below what a uint32_t counts
    return proc_name((int)pid, name, (uint32_t)room) > 0;
}
#else
int cli_process_memory(unsigned int pid, CliProcessMemory *memory)
{
    (void)pid;
    (void)memory;
    return 0;
}

int cli_process_named(unsigned int pid, char *name, size_t room)
{
    (void)pid;
    (void)name;
    (void)room;
    return 0;
}
#endif
#endif
