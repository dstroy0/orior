// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_processes.c: the processes of the machine a session can attach to, by their names, this one and the system's
// idle process left out
#include "../command_line/callback_cfg.h"
#include "debug_internal.h"

// A process listed: its number, and where its name and its line start in the text
typedef struct
{
    unsigned int pid;
    unsigned long long name;
    unsigned long long command;
} CliDebugProcess;
// One listing: the processes in a cell of `capacity`, their names and lines in a text cell of `text_capacity`, this
// process's number, and the capacity the processes are sorted through
typedef struct
{
    CliDebugProcess *process;
    unsigned int count;
    unsigned int capacity;
    char *text;
    unsigned long long used;
    unsigned long long text_capacity;
    unsigned int own;
    CliDebugProcess *sorted;
} CliDebugListing;
// the most processes listed, and the most bytes their names and lines take
#define CLI_DEBUG_PROCESSES 16384u
#define CLI_DEBUG_PROCESS_TEXT (1ULL << 23u)

// the processes a listing's capacity is taken with, and the bytes its text's is

typedef struct
{
    CliDebugListing *listing;
    const char *text;
    const CliDebugProcess *one;
    const CliDebugProcess *other;
    unsigned int from;
    unsigned int to;
} DebugProcessesCtx;
// a text stored in the listing's text: where it starts, or where an empty one does where it does not fit
EMBED_INLINE unsigned long long processes_text_store(const DebugProcessesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}) + 1ULL;
    if (args->listing->used + size > args->listing->text_capacity)
    {
        return 0ULL;
    }
    const unsigned long long at = args->listing->used;
    memor.cpy(&(MemoriaCfg){.dst = args->listing->text + at, .src = args->text, .bytes = size});
    args->listing->used += size;
    return at;
}

static void processes_list(const CliEachPidNameCommandCfg *args)
{
    CliDebugListing *const listing = args->context;
    if ((args->pid == 0u) || (args->pid == listing->own) || (listing->count >= CLI_DEBUG_PROCESSES))
    {
        return;
    }
    listing->process[listing->count] = (CliDebugProcess){
        args->pid, EMBED_CALL(processes_text_store, DebugProcessesCtx, .listing = listing, .text = args->name),
        EMBED_CALL(processes_text_store, DebugProcessesCtx, .listing = listing, .text = args->command)};
    listing->count += 1u;
}

// whether process `one` comes before `other`: by name, case aside, then by number
EMBED_INLINE int processes_before(const DebugProcessesCtx *args)
{
    const char *left = args->text + args->one->name;
    const char *right = args->text + args->other->name;
    for (;; left += 1, right += 1)
    {
        const char a = ((*left >= 'A') && (*left <= 'Z')) ? (char)(*left - 'A' + 'a') : *left;
        const char b = ((*right >= 'A') && (*right <= 'Z')) ? (char)(*right - 'A' + 'a') : *right;
        if ((a != b) || (a == '\0'))
        {
            return (a != b) ? ((unsigned char)a < (unsigned char)b) : (args->one->pid < args->other->pid);
        }
    }
}

// the processes from `from` to `to` sorted, by halves merged through the sorting capacity
static void processes_sort(const DebugProcessesCtx *args)
{
    if (args->to - args->from < 2u)
    {
        return;
    }
    const unsigned int middle = args->from + ((args->to - args->from) / 2u);
    EMBED_CALL(processes_sort, DebugProcessesCtx, .listing = args->listing, .from = args->from, .to = middle);
    EMBED_CALL(processes_sort, DebugProcessesCtx, .listing = args->listing, .from = middle, .to = args->to);
    CliDebugProcess *const process = args->listing->process;
    unsigned int left = args->from;
    unsigned int right = middle;
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        const int allocs_left =
            (left < middle) &&
            ((right >= args->to) || !EMBED_CALL(processes_before, DebugProcessesCtx, .text = args->listing->text,
                                                .one = &process[right], .other = &process[left]));
        args->listing->sorted[at] = allocs_left ? process[left] : process[right];
        left += allocs_left ? 1u : 0u;
        right += allocs_left ? 0u : 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = &process[args->from],
                            .src = &args->listing->sorted[args->from],
                            .bytes = (size_t)(args->to - args->from) * sizeof(process[0])});
}

void cli_debug_processes(const CliDebugOutErrMsgIdStepCfg *args)
{
    CliDebugListing listing = {NULL, 0u, CLI_DEBUG_PROCESSES, NULL, 1ULL, CLI_DEBUG_PROCESS_TEXT, cli_process_own(),
                               NULL};
    listing.process =
        g_cli_guard->temporary_buf_alloc(CLI_DEBUG_PROCESSES * sizeof(CliDebugProcess), g_cli_debug_processes_owner);
    listing.text = g_cli_guard->temporary_buf_alloc(CLI_DEBUG_PROCESS_TEXT, g_cli_debug_processes_owner);
    if ((listing.process != NULL) && (listing.text != NULL))
    {
        listing.text[0] = '\0';
        EMBED_CALL(cli_context_internal.processes_each, CliContextCtx, .each_callback = processes_list,
                   .context = &listing);
        listing.sorted = g_cli_guard->persistent_buf_alloc(
            ((unsigned long long)listing.count + 1ULL) * sizeof(CliDebugProcess), g_cli_debug_processes_owner);
    }
    const unsigned int count = (listing.sorted != NULL) ? listing.count : 0u;
    EMBED_CALL(processes_sort, DebugProcessesCtx, .listing = &listing, .from = 0u, .to = count);
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const name = listing.text + listing.process[at].name;
        const char *const command = listing.text + listing.process[at].command;
        mmgr_line_put(args->out, (at > 0u) ? ",{\"pid\":" : "{\"pid\":");
        mmgr_line_decimal(args->out, listing.process[at].pid, 1u);
        mmgr_line_put(args->out, ",\"name\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
        mmgr_line_put(args->out, ",\"command\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)command,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_POOL_BYTES}));
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
    g_cli_guard->persistent_buf_release(listing.sorted, g_cli_debug_processes_owner);
    g_cli_guard->persistent_buf_release(listing.text, g_cli_debug_processes_owner);
    g_cli_guard->persistent_buf_release(listing.process, g_cli_debug_processes_owner);
}
