// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_processes.c: the processes of the machine on Linux, the system's own list in /proc: each process's name from
// its comm and its line from its cmdline, the words apart by nuls, and a process that ends while the list is read left
// out; and a process's memory and its program's name from /proc
#if defined(__linux__)
// O_CLOEXEC is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#endif
#include "../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../ui/cli/src/text/json_tree.h"
#include "../../../../ui/cli/src/text/text.h"

#if defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct
{
    const char *path;
    char *out;
    unsigned long long capacity;
    const char *text;
    const char *field;
    unsigned int pid;
    const char *file;
} PosixProcessesCtx;

// the max bytes of a process's name and line read
#define CLI_PROCESS_TEXT (1u << 16u)

static char s_name[CLI_PROCESS_TEXT];
static char s_command[CLI_PROCESS_TEXT];

static char s_line[CLI_PROCESS_TEXT];

// the file at `path` read into `out`, which has room for `capacity`: the bytes read, -1 where the call cannot open it
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

// The list that the call reads: where each process goes
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
                 (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) <= 10u);
    const unsigned long long digits = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
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
                                .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = s_name[end - 1ULL]}) != NULL))
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
    unsigned long long word_text = 0ULL;
    for (unsigned long long at = 0ULL; at <= (unsigned long long)((line_size > 0LL) ? line_size : 0LL); at += 1ULL)
    {
        const int ends = (at == (unsigned long long)((line_size > 0LL) ? line_size : 0LL)) || (s_line[at] == '\0');
        if (ends && (at > word_text))
        {
            mmgr_line_put(&command, (command.at > 0ULL) ? " " : "");
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &command,
                       .text_bytes = (const unsigned char *)s_line + word_text, .length = at - word_text);
        }
        word_text = ends ? at + 1ULL : word_text;
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
// the kilobytes the line of `text` that starts with `field` gives, in bytes, 0 where no line gives them
EMBED_INLINE unsigned long long processes_kilobytes(const PosixProcessesCtx *args)
{
    const char *text = args->text;
    const char *at = cellul.find(
        &(CatenaFinitaCfg){.src = text, .cap = CLI_PROCESS_TEXT, .other = args->field, .other_cap = CLI_POOL_BYTES});
    if (at == NULL)
    {
        return 0ULL;
    }
    at += cellul.len(&(CatenaFinitaCfg){.src = args->field, .cap = CLI_POOL_BYTES});
    at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                     .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_POOL_BYTES}), .set = " \t");
    unsigned long long kilobytes = 0ULL;
    kilobytes = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_POOL_BYTES}), .end = NULL);
    return kilobytes * 1024ULL;
}

// the file `file` of the folder of the process `pid` in /proc, read into `out`, which has room for CLI_PROCESS_TEXT.
// A 0 goes after the bytes read. The return is 1, or 0 where the call cannot read it
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

// the processor time in the text of a process's stat file, in microseconds: its user and kernel ticks, the 14th and
// 15th fields. The count of fields starts after the last `)`, because the program's name can hold spaces
EMBED_INLINE unsigned long long processes_processor_us(const char *stat)
{
    const char *at =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = stat,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = stat, .cap = CLI_PROCESS_TEXT}), .byte = ')');
    if (at == NULL)
    {
        return 0ULL;
    }
    // the field after the `)` is the 3rd, the state
    unsigned int field = 2u;
    unsigned long long ticks = 0ULL;
    while ((at[0] != '\0') && (field < 15u))
    {
        at += 1;
        if (at[0] == ' ')
        {
            field += 1u;
            if ((field == 14u) || (field == 15u))
            {
                ticks += EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at + 1, .at = 0ULL,
                                    .size = cellul.len(&(CatenaFinitaCfg){.src = at + 1, .cap = CLI_POOL_BYTES}),
                                    .end = NULL);
            }
        }
    }
    const long per_second = sysconf(_SC_CLK_TCK);
    // the ticks a second is a small positive number where the system says it
    return (per_second > 0L) ? ((ticks * 1000000ULL) / (unsigned long long)per_second) : 0ULL;
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
        args->memory->processor_us =
            EMBED_CALL(processes_proc_read, PosixProcessesCtx, .pid = args->pid, .file = "/stat", .out = s_line)
                ? processes_processor_us(s_line)
                : 0ULL;
    }
    return read;
}

int cli_process_by_name(const CliCapacityPidCtx *args)
{
    const int read =
        EMBED_CALL(processes_proc_read, PosixProcessesCtx, .pid = args->pid, .file = "/comm", .out = s_name);
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = s_name, .cap = CLI_PROCESS_TEXT});
    while ((end > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = s_name[end - 1ULL]}) != NULL))
    {
        end -= 1ULL;
    }
    VerbaLinea written = {args->name_out, args->capacity_bytes, 0ULL};
    mmgr_line_put_n(&written, s_name, end);
    (void)mmgr_line_finish(&written);
    return read;
}
#endif
