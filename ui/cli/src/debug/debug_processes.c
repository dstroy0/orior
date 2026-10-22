// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_processes.c: the processes of the machine a session can attach to, by their names, this one and the system's
// idle process left out
#include "debug_internal.h"

// the most processes listed, and the most bytes their names and lines take
#define CLI_DEBUG_PROCESSES 16384u
#define CLI_DEBUG_PROCESS_TEXT (1ull << 23u)

// the processes a listing's room is begun with, and the bytes its text's is, each doubling as they are put
#define CLI_DEBUG_PROCESSES_FIRST 256u
#define CLI_DEBUG_PROCESS_TEXT_FIRST 16384ull

CliCellsTally g_cli_debug_processes_cells = CLI_CELLS_TALLY("cli_debug_processes");

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

// `cell` of `size` bytes moved into a cell of `larger`, given back: the larger, or NULL with `cell` kept where it is
// refused
static void *processes_moved(void *cell, unsigned long long size, unsigned long long larger, const char *what)
{
    char *const moved = cli_cell_taken_by(&g_cli_debug_processes_cells, larger, what);
    if (moved == NULL)
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = moved, .src = cell, .bytes = (size_t)size});
    cli_cell_given_by(&g_cli_debug_processes_cells, cell);
    return moved;
}

// a text kept in the listing's text: where it starts, or where an empty one does where it does not fit
static unsigned long long processes_kept(CliDebugListing *listing, const char *text)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    while ((listing->used + size > listing->text_room) && (listing->text_room < CLI_DEBUG_PROCESS_TEXT))
    {
        const unsigned long long doubled = listing->text_room * 2ull;
        const unsigned long long larger = (doubled < CLI_DEBUG_PROCESS_TEXT) ? doubled : CLI_DEBUG_PROCESS_TEXT;
        char *const moved = processes_moved(listing->text, listing->used, larger, "the processes' names and lines");
        if (moved == NULL)
        {
            return 0ull;
        }
        listing->text = moved;
        listing->text_room = larger;
    }
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
    if (listing->count == listing->room)
    {
        const unsigned int doubled = listing->room * 2u;
        const unsigned int larger = (doubled < CLI_DEBUG_PROCESSES) ? doubled : CLI_DEBUG_PROCESSES;
        CliDebugProcess *const moved =
            processes_moved(listing->process, (unsigned long long)listing->count * sizeof(CliDebugProcess),
                            (unsigned long long)larger * sizeof(CliDebugProcess), "the processes listed");
        if (moved == NULL)
        {
            return;
        }
        listing->process = moved;
        listing->room = larger;
    }
    listing->process[listing->count] =
        (CliDebugProcess){pid, processes_kept(listing, name), processes_kept(listing, command)};
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

void cli_debug_processes(CliLine *out)
{
    CliDebugListing listing = {
        NULL, 0u, CLI_DEBUG_PROCESSES_FIRST, NULL, 1ull, CLI_DEBUG_PROCESS_TEXT_FIRST, cli_process_own(), NULL};
    listing.process = cli_cell_taken_by(&g_cli_debug_processes_cells,
                                        CLI_DEBUG_PROCESSES_FIRST * sizeof(CliDebugProcess), "the processes listed");
    listing.text =
        cli_cell_taken_by(&g_cli_debug_processes_cells, CLI_DEBUG_PROCESS_TEXT_FIRST, "the processes' names and lines");
    if ((listing.process != NULL) && (listing.text != NULL))
    {
        listing.text[0] = '\0';
        cli_processes_each(processes_listed, &listing);
        listing.sorted = cli_cell_taken_by(&g_cli_debug_processes_cells,
                                           ((unsigned long long)listing.count + 1ull) * sizeof(CliDebugProcess),
                                           "the processes' sorting room");
    }
    const unsigned int count = (listing.sorted != NULL) ? listing.count : 0u;
    processes_sorted(&listing, 0u, count);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const name = listing.text + listing.process[at].name;
        const char *const command = listing.text + listing.process[at].command;
        cli_line_text(out, (at > 0u) ? ",{\"pid\":" : "{\"pid\":");
        cli_line_decimal(out, listing.process[at].pid, 1u);
        cli_line_text(out, ",\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)name,
                              cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"command\":");
        cli_json_quoted_lossy(out, (const unsigned char *)command,
                              cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL}));
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    cli_cell_given_by(&g_cli_debug_processes_cells, listing.sorted);
    cli_cell_given_by(&g_cli_debug_processes_cells, listing.text);
    cli_cell_given_by(&g_cli_debug_processes_cells, listing.process);
}
