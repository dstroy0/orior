// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_processes.c: the processes of the machine a session can attach to, by their names, this one and the system's
// idle process left out
#include "debug_internal.h"

// the most processes listed, and the bytes their names and lines take
#define CLI_DEBUG_PROCESSES 16384u
#define CLI_DEBUG_PROCESS_TEXT (1u << 23u)

// A process listed: its number, and where its name and its line start in the text
typedef struct
{
    unsigned int pid;
    unsigned long long name;
    unsigned long long command;
} CliDebugProcess;

// The processes listed, their texts, the room they are sorted through, and the lock they are held by
static CliDebugProcess s_processes[CLI_DEBUG_PROCESSES];
static CliDebugProcess s_sorted[CLI_DEBUG_PROCESSES];
static unsigned int s_count;
static char s_text[CLI_DEBUG_PROCESS_TEXT];
static unsigned long long s_used;
static unsigned int s_own;
static CliLock s_lock = CLI_LOCK_INIT;

// a text kept in the processes' text: where it starts, or where an empty one does where it does not fit
static unsigned long long processes_kept(const char *text)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    if (s_used + size > sizeof(s_text))
    {
        return 0ull;
    }
    const unsigned long long at = s_used;
    memor.cpy(&(MemoriaCfg){.dst = s_text + at, .src = text, .bytes = size});
    s_used += size;
    return at;
}

static void processes_listed(void *context, unsigned int pid, const char *name, const char *command)
{
    (void)context;
    if ((pid == 0u) || (pid == s_own) || (s_count >= CLI_DEBUG_PROCESSES))
    {
        return;
    }
    s_processes[s_count] = (CliDebugProcess){pid, processes_kept(name), processes_kept(command)};
    s_count += 1u;
}

// whether process `one` comes before `other`: by name, case aside, then by number
static int processes_before(const CliDebugProcess *one, const CliDebugProcess *other)
{
    const char *left = s_text + one->name;
    const char *right = s_text + other->name;
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
static void processes_sorted(unsigned int from, unsigned int to)
{
    if (to - from < 2u)
    {
        return;
    }
    const unsigned int middle = from + ((to - from) / 2u);
    processes_sorted(from, middle);
    processes_sorted(middle, to);
    unsigned int left = from;
    unsigned int right = middle;
    for (unsigned int at = from; at < to; at += 1u)
    {
        const int takes_left =
            (left < middle) && ((right >= to) || !processes_before(&s_processes[right], &s_processes[left]));
        s_sorted[at] = takes_left ? s_processes[left] : s_processes[right];
        left += takes_left ? 1u : 0u;
        right += takes_left ? 0u : 1u;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = &s_processes[from], .src = &s_sorted[from], .bytes = (size_t)(to - from) * sizeof(s_processes[0])});
}

void cli_debug_processes(CliLine *out)
{
    cli_lock_take(&s_lock);
    s_count = 0u;
    s_used = 1ull;
    s_text[0] = '\0';
    s_own = cli_process_own();
    cli_processes_each(processes_listed, NULL);
    processes_sorted(0u, s_count);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        const char *const name = s_text + s_processes[at].name;
        const char *const command = s_text + s_processes[at].command;
        cli_line_text(out, (at > 0u) ? ",{\"pid\":" : "{\"pid\":");
        cli_line_decimal(out, s_processes[at].pid, 1u);
        cli_line_text(out, ",\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)name,
                              cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"command\":");
        cli_json_quoted_lossy(out, (const unsigned char *)command,
                              cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL}));
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
}
