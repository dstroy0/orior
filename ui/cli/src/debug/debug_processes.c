// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_processes.c: the processes of the machine a session can attach to, by their names, this one and the system's
// idle process left out
#include "debug_internal.h"

// the most processes listed, and the most bytes their names and lines take
#define CLI_DEBUG_PROCESSES 16384u
#define CLI_DEBUG_PROCESS_TEXT (1ull << 23u)

// the processes a listing's room is taken with, and the bytes its text's is

// A process listed: its number, and where its name and its line start in the text
typedef struct
{
    unsigned int pid;
    unsigned long long name;
    unsigned long long command;
} CliDebugProcess;

// One listing: the processes in a cell of `room`, their names and lines in a text cell of `text_room`, this process's
// number, and the room the processes are sorted through
typedef struct
{
    CliDebugProcess *process;
    unsigned int count;
    unsigned int room;
    char *text;
    unsigned long long used;
    unsigned long long text_room;
    unsigned int own;
    CliDebugProcess *sorted;
} CliDebugListing;

// a text stored in the listing's text: where it starts, or where an empty one does where it does not fit
static unsigned long long processes_text_stored(CliDebugListing *listing, const char *text)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    if (listing->used + size > listing->text_room)
    {
        return 0ull;
    }
    const unsigned long long at = listing->used;
    memor.cpy(&(MemoriaCfg){.dst = listing->text + at, .src = text, .bytes = size});
    listing->used += size;
    return at;
}

static void processes_listed(void *context, unsigned int pid, const char *name, const char *command)
{
    CliDebugListing *const listing = context;
    if ((pid == 0u) || (pid == listing->own) || (listing->count >= CLI_DEBUG_PROCESSES))
    {
        return;
    }
    listing->process[listing->count] =
        (CliDebugProcess){pid, processes_text_stored(listing, name), processes_text_stored(listing, command)};
    listing->count += 1u;
}

// whether process `one` comes before `other`: by name, case aside, then by number
static int processes_before(const char *text, const CliDebugProcess *one, const CliDebugProcess *other)
{
    const char *left = text + one->name;
    const char *right = text + other->name;
    for (;; left += 1, right += 1)
    {
        const char a = ((*left >= 'A') && (*left <= 'Z')) ? (char)(*left - 'A' + 'a') : *left;
        const char b = ((*right >= 'A') && (*right <= 'Z')) ? (char)(*right - 'A' + 'a') : *right;
        if ((a != b) || (a == '\0'))
        {
            return (a != b) ? ((unsigned char)a < (unsigned char)b) : (one->pid < other->pid);
        }
    }
}

// the processes from `from` to `to` sorted, by halves merged through the sorting room
static void processes_sorted(CliDebugListing *listing, unsigned int from, unsigned int to)
{
    if (to - from < 2u)
    {
        return;
    }
    const unsigned int middle = from + ((to - from) / 2u);
    processes_sorted(listing, from, middle);
    processes_sorted(listing, middle, to);
    CliDebugProcess *const process = listing->process;
    unsigned int left = from;
    unsigned int right = middle;
    for (unsigned int at = from; at < to; at += 1u)
    {
        const int takes_left =
            (left < middle) && ((right >= to) || !processes_before(listing->text, &process[right], &process[left]));
        listing->sorted[at] = takes_left ? process[left] : process[right];
        left += takes_left ? 1u : 0u;
        right += takes_left ? 0u : 1u;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = &process[from], .src = &listing->sorted[from], .bytes = (size_t)(to - from) * sizeof(process[0])});
}

void cli_debug_processes(VerbaLinea *out)
{
    CliDebugListing listing = {NULL, 0u, CLI_DEBUG_PROCESSES, NULL, 1ull, CLI_DEBUG_PROCESS_TEXT, cli_process_own(),
                               NULL};
    listing.process =
        g_cli_cells->temporary_buf_alloc(CLI_DEBUG_PROCESSES * sizeof(CliDebugProcess), &g_cli_debug_processes_cells);
    listing.text = g_cli_cells->temporary_buf_alloc(CLI_DEBUG_PROCESS_TEXT, &g_cli_debug_processes_cells);
    if ((listing.process != NULL) && (listing.text != NULL))
    {
        listing.text[0] = '\0';
        cli_processes_each(processes_listed, &listing);
        listing.sorted = g_cli_cells->persistent_buf_alloc(
            ((unsigned long long)listing.count + 1ull) * sizeof(CliDebugProcess), &g_cli_debug_processes_cells);
    }
    const unsigned int count = (listing.sorted != NULL) ? listing.count : 0u;
    processes_sorted(&listing, 0u, count);
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const name = listing.text + listing.process[at].name;
        const char *const command = listing.text + listing.process[at].command;
        mmgr_line_put(out, (at > 0u) ? ",{\"pid\":" : "{\"pid\":");
        mmgr_line_decimal(out, listing.process[at].pid, 1u);
        mmgr_line_put(out, ",\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)name,
                              cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"command\":");
        cli_json_quoted_lossy(out, (const unsigned char *)command,
                              cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(listing.sorted, &g_cli_debug_processes_cells);
    g_cli_cells->persistent_buf_release(listing.text, &g_cli_debug_processes_cells);
    g_cli_cells->persistent_buf_release(listing.process, &g_cli_debug_processes_cells);
}
