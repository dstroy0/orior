// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_processes.c: the processes of the machine on macOS, as ps lists them, each line its number, its name and the
// line it was started with; and a process's memory and its program's name as libproc gives them
#if defined(__APPLE__)
// libproc and the absolute time units are the system's, outside strict C11
#define _DARWIN_C_SOURCE
#endif
#include "../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../ui/cli/src/text/json_tree.h"
#include "../../../../ui/cli/src/text/text.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

#if defined(__APPLE__)
#include <libproc.h>
#include <mach/mach_time.h>
#include <unistd.h>

// the max bytes of a process's name and line read
#define CLI_PROCESS_TEXT (1u << 16u)

static char s_name[CLI_PROCESS_TEXT];
static char s_command[CLI_PROCESS_TEXT];

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
                                      .src = " \t\r\f\v", .cap = CLI_POOL_BYTES, .byte = run_result->out[at]}) != NULL))
            {
                at += 1ULL;
            }
            const unsigned long long first = at;
            while ((at < end) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = " \t\r\f\v", .cap = CLI_POOL_BYTES, .byte = run_result->out[at]}) == NULL))
            {
                at += 1ULL;
            }
            if (at == first)
            {
                break;
            }
            for (unsigned long long digit = first; (index == 0u) && (digit < at); digit += 1ULL)
            {
                // Explicit cast reads the byte as its code point
                number = number &&
                         ascii.in(&(AsciiCfg){.type = MMGR_ASCII_NUM, .byte = (uint8_t)run_result->out[digit]}) &&
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
int cli_process_memory(const CliPidCtx *args)
{
    struct rusage_info_v4 info;
    // a process id the system gives is below what an int counts, and the call writes a rusage_info_v4 where a caller
    // asks it for RUSAGE_INFO_V4
    if (proc_pid_rusage((int)args->pid, RUSAGE_INFO_V4, (rusage_info_t *)&info) != 0)
    {
        return 0;
    }
    args->memory->working = info.ri_phys_footprint;
    args->memory->commit = info.ri_phys_footprint;
    args->memory->started = 0ULL;
    // the user and system times count the machine's absolute time units, and the timebase turns them to nanoseconds
    mach_timebase_info_data_t timebase = {1u, 1u};
    (void)mach_timebase_info(&timebase);
    const unsigned long long units = info.ri_user_time + info.ri_system_time;
    args->memory->processor_us =
        (timebase.denom != 0u) ? (((units / 1000ULL) * timebase.numer) / timebase.denom) : 0ULL;
    return 1;
}

int cli_process_by_name(const CliCapacityPidCtx *args)
{
    // a process id the system gives is below what an int counts, and a capacity is far below what a embed_u32 counts
    return proc_name((int)args->pid, args->name_out, (embed_u32)args->capacity_bytes) > 0;
}
#endif
