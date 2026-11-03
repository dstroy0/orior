// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_processes.c: the processes of the machine: on Linux the system's own list in /proc, each process's name from
// its comm and its line from its cmdline, the words apart by nuls, and a process that ends while the list is read left
// out; elsewhere as ps lists them, each line its number, its name and the line it was started with; and a process's
// memory and its program's name, on Linux from /proc and on macOS as libproc gives them
#if !defined(_WIN32)
// O_CLOEXEC is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "../text/text.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <libproc.h>
#endif

typedef struct
{
#if (!defined(_WIN32)) && (defined(__linux__))
    const char *path;
    char *out;
    unsigned long long capacity;
    const char *text;
    const char *field;
    unsigned int pid;
    const char *file;
#endif
} PosixProcessesCtx;

// the most bytes of a process's name and line read
#define CLI_PROCESS_TEXT (1u << 16u)

static char s_name[CLI_PROCESS_TEXT];
static char s_command[CLI_PROCESS_TEXT];

#if defined(__linux__)
static char s_line[CLI_PROCESS_TEXT];

// the file at `path` read into `out`, sized for `capacity`: the bytes read, -1 where it cannot be opened
EMBED_INLINE long long processes_read(const PosixProcessesCtx *args)
{
    const int file = open(args->path, O_RDONLY | O_CLOEXEC);
    if (file < 0)
    {
        return -1LL;
    }
    unsigned long long used = 0ULL;
    for (;;)
    {
        const ssize_t count = read(file, args->out + used, args->capacity - used);
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
    void (*each)(const CliEachPidNameCommandCfg *args);
    void *context;
} CliProcessesRead;

// one entry of /proc read where its name is a process's number
static void processes_entry(const CliEachNameIsFolderCfg *args)
{
    const CliProcessesRead *const read_list = (const CliProcessesRead *)args->context;
    unsigned long long pid = 0ULL;
    int number = args->is_folder && (args->name[0] != '\0') &&
                 (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) <= 10u);
    const unsigned long long digits = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    number = number && (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->name,
                                   .at = 0ULL, .size = digits, .classes = CLI_TEXT_DIGITS, .set = "") == digits);
    pid = number ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->name, .at = 0ULL, .size = digits,
                              .end = NULL)
                 : 0ULL;
    if (!number || (pid > 0xffffffffULL))
    {
        return;
    }
    char path[64];
    VerbaLinea named = {path, sizeof(path), 0ULL};
    mmgr_line_put(&named, "/proc/");
    mmgr_line_put(&named, args->name);
    mmgr_line_put(&named, "/comm");
    mmgr_line_finish(&named);
    const long long name_size =
        EMBED_CALL(processes_read, PosixProcessesCtx, .path = path, .out = s_name, .capacity = sizeof(s_name) - 1u);
    // a size that is not negative fits an unsigned long long
    if ((name_size < 0LL) || ((unsigned long long)name_size >= sizeof(s_name)))
    {
        return;
    }
    unsigned long long end = (unsigned long long)name_size;
    while ((end > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = s_name[end - 1ULL]}) != NULL))
    {
        end -= 1ULL;
    }
    s_name[end] = '\0';
    named.at = 0ULL;
    mmgr_line_put(&named, "/proc/");
    mmgr_line_put(&named, args->name);
    mmgr_line_put(&named, "/cmdline");
    mmgr_line_finish(&named);
    const long long line_size =
        EMBED_CALL(processes_read, PosixProcessesCtx, .path = path, .out = s_line, .capacity = sizeof(s_line));
    VerbaLinea command = {s_command, sizeof(s_command), 0ULL};
    unsigned long long word = 0ULL;
    for (unsigned long long at = 0ULL; at <= (unsigned long long)((line_size > 0LL) ? line_size : 0LL); at += 1ULL)
    {
        const int ends = (at == (unsigned long long)((line_size > 0LL) ? line_size : 0LL)) || (s_line[at] == '\0');
        if (ends && (at > word))
        {
            mmgr_line_put(&command, (command.at > 0ULL) ? " " : "");
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &command,
                       .text_bytes = (const unsigned char *)s_line + word, .length = at - word);
        }
        word = ends ? at + 1ULL : word;
    }
    mmgr_line_finish(&command);
    EMBED_CALL(read_list->each, CliEachPidNameCommandCfg, .context = read_list->context, .pid = (unsigned int)pid,
               .name = s_name, .command = s_command);
}

void cli_processes_each(const CliContextCtx *args)
{
    CliProcessesRead read_list = {args->each_callback, args->context};
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = "/proc",
                     .each = processes_entry, .context_bytess = &read_list);
}
#else
void cli_processes_each(const CliContextCtx *args)
{
    const char *const words[4] = {"ps", "-eo", "pid=,comm=,args=", NULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u,
                    .run_result = run_result, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return;
    }
    for (unsigned long long start = 0ULL; start < run_result->out_size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = run_result->out,
                                                  .at = start, .size = run_result->out_size)
                                           .stop;
        // the line's words apart by spaces: its number, its name, then the words of its line joined by one space
        unsigned long long pid = 0ULL;
        unsigned int index = 0u;
        int number = 1;
        VerbaLinea name = {s_name, sizeof(s_name), 0ULL};
        VerbaLinea command = {s_command, sizeof(s_command), 0ULL};
        for (unsigned long long at = start; at < end;)
        {
            while ((at < end) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = run_result->out[at]}) != NULL))
            {
                at += 1ULL;
            }
            const unsigned long long first = at;
            while ((at < end) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = run_result->out[at]}) == NULL))
            {
                at += 1ULL;
            }
            if (at == first)
            {
                break;
            }
            for (unsigned long long digit = first; (index == 0u) && (digit < at); digit += 1ULL)
            {
                number = number && (run_result->out[digit] >= '0') && (run_result->out[digit] <= '9') &&
                         (at - first <= 10ULL);
                pid = (pid * 10ULL) + (unsigned long long)(run_result->out[digit] - '0');
            }
            if (index == 1u)
            {
                EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &name,
                           .text_bytes = (const unsigned char *)run_result->out + first, .length = at - first);
            }
            else if (index > 1u)
            {
                mmgr_line_put(&command, (command.at > 0ULL) ? " " : "");
                EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &command,
                           .text_bytes = (const unsigned char *)run_result->out + first, .length = at - first);
            }
            index += 1u;
        }
        mmgr_line_finish(&name);
        mmgr_line_finish(&command);
        if ((index >= 2u) && number && (pid <= 0xffffffffULL))
        {
            EMBED_CALL(args->each_callback, CliEachPidNameCommandCfg, .context = args->context,
                       .pid = (unsigned int)pid, .name = s_name, .command = s_command);
        }
        start = end + 1ULL;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
}
#endif
#endif

#if !defined(_WIN32)
int cli_process_running(const CliPidCtx *args)
{
    if (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = "/proc"))
    {
        return 1;
    }
    char path[64];
    VerbaLinea named = {path, sizeof(path), 0ULL};
    mmgr_line_put(&named, "/proc/");
    mmgr_line_decimal(&named, args->pid, 1u);
    (void)mmgr_line_finish(&named);
    return EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path);
}

#if defined(__linux__)
// the kilobytes the line of `text` that starts with `field` gives, in bytes, 0 where no line gives them
EMBED_INLINE unsigned long long processes_kilobytes(const PosixProcessesCtx *args)
{
    const char *text = args->text;
    const char *at = cellul.find(
        &(CatenaFinitaCfg){.src = text, .cap = CLI_PROCESS_TEXT, .other = args->field, .other_cap = CLI_CELLS_POOL});
    if (at == NULL)
    {
        return 0ULL;
    }
    at += cellul.len(&(CatenaFinitaCfg){.src = args->field, .cap = CLI_CELLS_POOL});
    at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                     .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}), .set = " \t");
    unsigned long long kilobytes = 0ULL;
    kilobytes = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}), .end = NULL);
    return kilobytes * 1024ULL;
}

// the file `file` of the process `pid`'s folder in /proc read into `out`, sized for CLI_PROCESS_TEXT, a 0 after what
// was read: 1, or 0 where it cannot be read
EMBED_INLINE int processes_proc_read(const PosixProcessesCtx *args)
{
    char path[64];
    VerbaLinea named = {path, sizeof(path), 0ULL};
    mmgr_line_put(&named, "/proc/");
    mmgr_line_decimal(&named, args->pid, 1u);
    mmgr_line_put(&named, args->file);
    (void)mmgr_line_finish(&named);
    const long long size = EMBED_CALL(processes_read, PosixProcessesCtx, .path = path, .out = args->out,
                                      .capacity = CLI_PROCESS_TEXT - 1u);
    // a size read is below the capacity, which an unsigned long long counts
    args->out[(size > 0LL) ? (unsigned long long)size : 0ULL] = '\0';
    return size >= 0LL;
}

int cli_process_memory(const CliPidCtx *args)
{
    const int read =
        EMBED_CALL(processes_proc_read, PosixProcessesCtx, .pid = args->pid, .file = "/smaps_rollup", .out = s_line) &&
        EMBED_CALL(processes_proc_read, PosixProcessesCtx, .pid = args->pid, .file = "/status", .out = s_command);
    if (read)
    {
        args->memory->working =
            EMBED_CALL(processes_kilobytes, PosixProcessesCtx, .text = s_line, .field = "Private_Clean:") +
            EMBED_CALL(processes_kilobytes, PosixProcessesCtx, .text = s_line, .field = "Private_Dirty:");
        args->memory->commit =
            EMBED_CALL(processes_kilobytes, PosixProcessesCtx, .text = s_command, .field = "RssAnon:") +
            EMBED_CALL(processes_kilobytes, PosixProcessesCtx, .text = s_command, .field = "VmSwap:");
        args->memory->started = 0ULL;
    }
    return read;
}

int cli_process_by_name(const CliCapacityPidCtx *args)
{
    const int read =
        EMBED_CALL(processes_proc_read, PosixProcessesCtx, .pid = args->pid, .file = "/comm", .out = s_name);
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = s_name, .cap = CLI_PROCESS_TEXT});
    while ((end > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = s_name[end - 1ULL]}) != NULL))
    {
        end -= 1ULL;
    }
    VerbaLinea written = {args->name_out, args->capacity_bytes, 0ULL};
    mmgr_line_put_n(&written, s_name, end);
    (void)mmgr_line_finish(&written);
    return read;
}
#elif defined(__APPLE__)
int cli_process_memory(const CliPidCtx *args)
{
    struct rusage_info_v4 info;
    // a process id the system gives is below what an int counts, and the call writes a rusage_info_v4 where it is asked
    // for RUSAGE_INFO_V4
    if (proc_pid_rusage((int)args->pid, RUSAGE_INFO_V4, (rusage_info_t *)&info) != 0)
    {
        return 0;
    }
    args->memory->working = info.ri_phys_footprint;
    args->memory->commit = info.ri_phys_footprint;
    args->memory->started = 0ULL;
    return 1;
}

int cli_process_by_name(const CliCapacityPidCtx *args)
{
    // a process id the system gives is below what an int counts, and a capacity is far below what a uint32_t counts
    return proc_name((int)args->pid, args->name_out, (uint32_t)args->capacity_bytes) > 0;
}
#else
int cli_process_memory(const CliPidCtx *args)
{
    (void)args;
    return 0;
}

int cli_process_by_name(const CliCapacityPidCtx *args)
{
    (void)args;
    return 0;
}
#endif
#endif
