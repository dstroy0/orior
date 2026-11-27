// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_container.c: the command line's calls on Linux: the system's processes listed from /proc, this program's
// path and the words it was started with from /proc/self, and a file copied with the reader's rights raised by
// pkexec
#if defined(__linux__)
// O_CLOEXEC and readlink are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../ui/cli/src/comms/container.h"
#include "../../../../ui/cli/src/text/text.h"
#include "../../../parsers/line_read.h"
#include "../posix_systems.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>

// what the walk over /proc records: the processes found, their count and capacity
typedef struct
{
    CliProcess *processes;
    unsigned int count;
    unsigned int capacity;
} CliProcWalk;

// one entry of /proc read: where it is a process's folder, its id and its parent recorded
static void cli_proc_walk(const CliEachNameIsFolderCfg *args)
{
    CliProcWalk *const walk = (CliProcWalk *)args->context;
    // Explicit cast reads the byte as its code point
    if (!args->is_folder || !(ascii.in(&(AsciiCfg){.type = MMGR_ASCII_NUM, .byte = (uint8_t)args->name[0]})) ||
        (walk->count >= walk->capacity))
    {
        return;
    }
    char path[64];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, "/proc/");
    mmgr_line_put(&line, args->name);
    mmgr_line_put(&line, "/stat");
    char stat[1024];
    // the call reads the file's bytes as bytes into text that it reads as characters
    const unsigned long long size = (mmgr_line_finish(&line) != 0ULL)
                                        ? qry_member_file_read(path, (unsigned char *)stat, sizeof(stat) - 1u)
                                        : QRY_MEMBER_UNREAD;
    // QRY_MEMBER_UNREAD is above every size
    if ((size == 0ULL) || (size >= sizeof(stat)))
    {
        return;
    }
    stat[size] = '\0';
    const char *const close =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = stat,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = stat, .cap = CLI_POOL_BYTES}), .byte = ')');
    if (close == NULL)
    {
        return;
    }
    // past the parenthesis: the state, then the parent
    const char *at = close + 1;
    at += EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = at, .set = " ");
    at += EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = at, .set = " ");
    at += EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = at, .set = " ");
    unsigned long long parent = 0ULL;
    unsigned long long id = 0ULL;
    const char *id_at = args->name;
    if (!line_number(&at, 0, &parent) || !line_number(&id_at, 0, &id))
    {
        return;
    }
    // a process id fits an unsigned int
    walk->processes[walk->count].id = (unsigned int)id;
    walk->processes[walk->count].parent = (unsigned int)parent;
    walk->count += 1u;
}

unsigned int cli_processes_list(const CliCapacityProcessesCtx *args)
{
    // each numbered folder of /proc, its stat's fourth field the parent, after the program's name in parentheses
    // which may contain spaces of its own
    CliProcWalk walk = {args->processes, 0u, args->capacity_u32};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = "/proc", .each = cli_proc_walk,
               .context_bytess = &walk);
    return walk.count;
}

int cli_program_own(const CliPathCapacityCtx *args)
{
    // readlink ends nothing with a 0, and the capacity's last byte stays the 0 written here
    memor.set(&(MemoriaCfg){.dst = args->path_out, .val = 0, .bytes = args->capacity_bytes});
    return readlink("/proc/self/exe", args->path_out, args->capacity_bytes - 1u) > 0;
}

// the max bytes of the words that this program started with, read where its main gives none
#define CLI_ARGUMENTS_TEXT (1u << 16u)

unsigned int cli_arguments_own_read(const char **words, unsigned int capacity)
{
    unsigned int reserved = 0u;
    static char s_text[CLI_ARGUMENTS_TEXT];
    const int file = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    unsigned long long used = 0ULL;
    while ((file >= 0) && (used + 1ULL < sizeof(s_text)))
    {
        const unsigned long long capacity = sizeof(s_text) - 1u - used;
        const ssize_t count = read(file, s_text + used, (embed_word)capacity);
        // a count that is positive fits an unsigned long long
        if ((count <= 0) || ((unsigned long long)count > capacity))
        {
            break;
        }
        used += (unsigned long long)count;
    }
    if (file >= 0)
    {
        close(file);
    }
    s_text[used] = '\0';
    // the program's own name first, passed over
    unsigned long long at = cellul.len(&(CatenaFinitaCfg){.src = s_text, .cap = sizeof(s_text)}) + 1ULL;
    while ((at < used) && (reserved < capacity))
    {
        words[reserved] = s_text + at;
        reserved += 1u;
        at += cellul.len(&(CatenaFinitaCfg){.src = s_text + at, .cap = sizeof(s_text)}) + 1ULL;
    }
    return reserved;
}

int cli_raised_platform(const CliErrMsgFromCtx *args)
{
    const char *const words[6] = {"pkexec", "/bin/cp", "--", args->from_text, args->to, NULL};
    PlatformProgram child;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &child, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_OWN))
    {
        mmgr_line_put(args->err_msg, "pkexec: it could not be started");
        return 0;
    }
    const long long code = EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &child);
    if ((code == 126LL) || (code == 127LL))
    {
        mmgr_line_put(args->err_msg, "the reader did not give the rights");
        return 0;
    }
    if (code != 0LL)
    {
        mmgr_line_put(args->err_msg, "pkexec did not write ");
        mmgr_line_put(args->err_msg, args->to);
        return 0;
    }
    return 1;
}
#endif
