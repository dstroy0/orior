// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_memory.c: how much RAM the window holds: its own process, every process it started and the web view's
// browser with the processes under it, each counted with the process the window started it from, a language server by
// its program and its toolchain's id and the web view's browser as the web view, and orior's own share apart from the
// tools it started, its own process and the web view's; and the language servers the open tabs
// do not need stopped, and where the window still holds more than its budget the server that holds the most and does
// not serve the tab shown. Every reading is made in buffers of this file's own, under its lock
#include "../servers/servers.h"
#include "window_internal.h"

// the most processes counted, the most programs they are counted under, and the most languages of the open tabs
#define CLI_MEMORY_PROCESSES 4096u
#define CLI_MEMORY_PARTS 256u
#define CLI_MEMORY_LANGUAGES 256u

// the most parents walked from a process up to the one the window started it from
#define CLI_MEMORY_DEPTH 64u

// whether the processes under the window and its browser are found by walking the system's list down from each, as on
// Windows, where the web view's browser stands outside the window's job; elsewhere every process under the window is a
// member of its container
#if defined(_WIN32)
#define CLI_MEMORY_WALKED 1
#else
#define CLI_MEMORY_WALKED 0
#endif

// One program the window started, with every process it started in turn: its process, its name, how many processes,
// what they hold in RAM and what they have reserved, and for a language server its toolchain's id, empty for none
typedef struct
{
    unsigned int pid;
    char name[CLI_NAME_MAX_LEN];
    unsigned int processes;
    unsigned long long working;
    unsigned long long commit;
    char server[CLI_NAME_MAX_LEN];
} CliMemoryPart;

// What the window holds: the whole; orior's own share of it, the window's process and the web view's, its browser with
// every renderer, GPU and toolkit process under it; and the programs it is counted under, the most first
typedef struct
{
    unsigned long long working;
    unsigned long long commit;
    unsigned long long own_working;
    unsigned long long own_commit;
    unsigned int count;
    CliMemoryPart part[CLI_MEMORY_PARTS];
} CliMemoryReading;

// the web view's browser process, 0 until the window names it
static unsigned int s_browser;
static CliLock s_lock = CLI_LOCK_INIT;
static CliProcess s_listed[CLI_PROCESSES_MOST];
static unsigned int s_members[CLI_MEMORY_PROCESSES];
static unsigned int s_found[CLI_MEMORY_PROCESSES];
static CliProcessMemory s_measured[CLI_MEMORY_PROCESSES];
static CliMemoryReading s_reading;
static CliServerListed s_servers[CLI_SERVERS_MOST];
static CliServerListed s_naming[CLI_SERVERS_MOST];
static char s_needed[CLI_SERVERS_MOST][CLI_NAME_MAX_LEN];
static char s_kept[CLI_SERVERS_MOST][CLI_NAME_MAX_LEN];
static const char *s_stopping[CLI_SERVERS_MOST];

void cli_window_browser_known(unsigned int pid)
{
    cli_lock_take(&s_lock);
    s_browser = pid;
    cli_lock_give(&s_lock);
}

// the process `pid` added to those found, `found` of them, where it is not among them, the system says what it holds,
// and it started no earlier than `after`
static void memory_found_add(unsigned int pid, unsigned long long after, unsigned int *found)
{
    for (unsigned int at = 0u; at < *found; at += 1u)
    {
        if (s_found[at] == pid)
        {
            return;
        }
    }
    CliProcessMemory measured;
    if ((*found < CLI_MEMORY_PROCESSES) && cli_process_memory(pid, &measured) && (measured.started >= after))
    {
        s_found[*found] = pid;
        s_measured[*found] = measured;
        *found += 1u;
    }
}

// the parent of `pid` as the system listed it, `listed` of them, `pid` itself where it is not listed
static unsigned int memory_parent_of(unsigned int pid, unsigned int listed)
{
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        if (s_listed[at].id == pid)
        {
            return s_listed[at].parent;
        }
    }
    return pid;
}

// the process the window started `pid` from: `pid`'s parent and theirs on up, to the one whose parent is the window or
// is not listed
static unsigned int memory_started_from(unsigned int pid, unsigned int own, unsigned int listed)
{
    unsigned int at = pid;
    for (unsigned int depth = 0u; depth < CLI_MEMORY_DEPTH; depth += 1u)
    {
        const unsigned int parent = memory_parent_of(at, listed);
        if ((at == own) || (parent == own) || (parent == at) || (parent <= 1u))
        {
            return at;
        }
        at = parent;
    }
    return at;
}

// the program the window started as `root` among the parts read, added where it is not: it, or NULL where every part is
// taken
static CliMemoryPart *memory_part_of(unsigned int root, unsigned int own)
{
    for (unsigned int at = 0u; at < s_reading.count; at += 1u)
    {
        if (s_reading.part[at].pid == root)
        {
            return &s_reading.part[at];
        }
    }
    if (s_reading.count == CLI_MEMORY_PARTS)
    {
        return NULL;
    }
    CliMemoryPart *const part = &s_reading.part[s_reading.count];
    s_reading.count += 1u;
    memor.set(&(MemoriaCfg){.dst = part, .val = 0, .bytes = sizeof(*part)});
    part->pid = root;
    if (root == own)
    {
        VerbaLinea named = {part->name, sizeof(part->name), 0ull};
        mmgr_line_put(&named, "orior_gui");
        (void)mmgr_line_finish(&named);
    }
    else
    {
        (void)cli_process_named(root, part->name, sizeof(part->name));
    }
    return part;
}

// whether a part is the web view's: its browser on Windows, and on Linux each WebKit process the window starts
static int memory_web_view(const char *name)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "msedgewebview2", .cap = CLI_CELLS_POOL}) ||
           (memor.cmp(&(MemoriaCfg){.src = name, .other = "WebKit", .bytes = 6ull}) == 0);
}

// each part a language server is named by its program, with its toolchain's id, and the web view's processes as the
// web view
static void memory_parts_named(void)
{
    const unsigned int servers = cli_servers_listed(s_naming, CLI_SERVERS_MOST);
    for (unsigned int at = 0u; at < s_reading.count; at += 1u)
    {
        CliMemoryPart *const part = &s_reading.part[at];
        VerbaLinea named = {part->name, sizeof(part->name), 0ull};
        for (unsigned int server = 0u; server < servers; server += 1u)
        {
            if (s_naming[server].pid == part->pid)
            {
                named.at = 0ull;
                mmgr_line_put(&named, s_naming[server].program);
                (void)mmgr_line_finish(&named);
                VerbaLinea tool = {part->server, sizeof(part->server), 0ull};
                mmgr_line_put(&tool, s_naming[server].tool);
                (void)mmgr_line_finish(&tool);
            }
        }
        if (memory_web_view(part->name))
        {
            named.at = 0ull;
            mmgr_line_put(&named, "web view");
            (void)mmgr_line_finish(&named);
        }
    }
}

// what the window holds read into s_reading, its parts named, the most first: 1, or 0 where the system does not say
// what the window's own process holds
static int memory_read(void)
{
    const unsigned int own = cli_process_own();
    unsigned int found = 0u;
    memory_found_add(own, 0ull, &found);
    if (found == 0u)
    {
        return 0;
    }
    const unsigned int listed = cli_processes_listed(s_listed, CLI_PROCESSES_MOST);
    if (CLI_MEMORY_WALKED && (s_browser != 0u) && (s_browser != own))
    {
        memory_found_add(s_browser, 0ull, &found);
    }
    // a process whose number an ended parent's took before that parent started is no child of it
    for (unsigned int at = 0u; CLI_MEMORY_WALKED && (at < found); at += 1u)
    {
        for (unsigned int each = 0u; each < listed; each += 1u)
        {
            if ((s_listed[each].parent == s_found[at]) && (s_listed[each].id != s_found[at]))
            {
                memory_found_add(s_listed[each].id, s_measured[at].started, &found);
            }
        }
    }
    // a process of the container whose parent has ended stands apart from the walk, and counts all the same
    const unsigned int members = cli_container_members(s_members, CLI_MEMORY_PROCESSES);
    for (unsigned int at = 0u; at < members; at += 1u)
    {
        memory_found_add(s_members[at], 0ull, &found);
    }
    memor.set(&(MemoriaCfg){.dst = &s_reading, .val = 0, .bytes = sizeof(s_reading)});
    for (unsigned int at = 0u; at < found; at += 1u)
    {
        CliMemoryPart *const part = memory_part_of(memory_started_from(s_found[at], own, listed), own);
        if (part != NULL)
        {
            part->processes += 1u;
            part->working += s_measured[at].working;
            part->commit += s_measured[at].commit;
        }
        s_reading.working += s_measured[at].working;
        s_reading.commit += s_measured[at].commit;
    }
    // the parts put in order, the one that holds the most first
    for (unsigned int at = 1u; at < s_reading.count; at += 1u)
    {
        const CliMemoryPart moving = s_reading.part[at];
        unsigned int to = at;
        while ((to > 0u) && (s_reading.part[to - 1u].working < moving.working))
        {
            s_reading.part[to] = s_reading.part[to - 1u];
            to -= 1u;
        }
        s_reading.part[to] = moving;
    }
    memory_parts_named();
    for (unsigned int at = 0u; at < s_reading.count; at += 1u)
    {
        const CliMemoryPart *const part = &s_reading.part[at];
        if ((part->pid == own) ||
            cellul.eq(&(CatenaFinitaCfg){.src = part->name, .other = "web view", .cap = CLI_CELLS_POOL}))
        {
            s_reading.own_working += part->working;
            s_reading.own_commit += part->commit;
        }
    }
    return 1;
}

// what the window holds, `{working, commit, orior, parts}`, `orior` orior's own share `{working, commit}`, each part
// `{name, pid, processes, working, commit, server}`, the server null for a part that is no language server; null where
// the system does not say
static int memory_usage(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_lock_take(&s_lock);
    if (!memory_read())
    {
        cli_lock_give(&s_lock);
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_put(out, "{\"working\":");
    mmgr_line_decimal(out, s_reading.working, 1u);
    mmgr_line_put(out, ",\"commit\":");
    mmgr_line_decimal(out, s_reading.commit, 1u);
    mmgr_line_put(out, ",\"orior\":{\"working\":");
    mmgr_line_decimal(out, s_reading.own_working, 1u);
    mmgr_line_put(out, ",\"commit\":");
    mmgr_line_decimal(out, s_reading.own_commit, 1u);
    mmgr_line_put(out, "},\"parts\":[");
    for (unsigned int at = 0u; at < s_reading.count; at += 1u)
    {
        const CliMemoryPart *const part = &s_reading.part[at];
        mmgr_line_put(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        cli_window_quoted(out, part->name);
        mmgr_line_put(out, ",\"pid\":");
        mmgr_line_decimal(out, part->pid, 1u);
        mmgr_line_put(out, ",\"processes\":");
        mmgr_line_decimal(out, part->processes, 1u);
        mmgr_line_put(out, ",\"working\":");
        mmgr_line_decimal(out, part->working, 1u);
        mmgr_line_put(out, ",\"commit\":");
        mmgr_line_decimal(out, part->commit, 1u);
        mmgr_line_put(out, ",\"server\":");
        if (part->server[0] != '\0')
        {
            cli_window_quoted(out, part->server);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, "]}");
    cli_lock_give(&s_lock);
    return 1;
}

// whether `tool` is among the `count` toolchains of `tools`
static int memory_tool_among(const char *tool, char (*tools)[CLI_NAME_MAX_LEN], unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = tool, .other = tools[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the language servers no language of the open tabs `open` needs stopped; where none is and the window holds more than
// `budget` bytes, the server that holds the most and does not serve `shown`, the language of the tab shown: what was
// stopped, `{stopped, files}`, each server stopped by its program and the files they had open. While the tree's check
// goes on it needs every server, and none is stopped
static int memory_servers_unneeded_stop(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long budget = 0ull;
    unsigned int languages = 0u;
    const char **open = NULL;
    if (!cli_serve_whole(args, "budget", &budget, err_msg) ||
        !cli_serve_strings_taken(args, "open", CLI_MEMORY_LANGUAGES, &open, &languages, err_msg))
    {
        cli_serve_strings_given(open);
        return 0;
    }
    const char *const shown = cli_serve_string_or(args, "shown", NULL);
    cli_lock_take(&s_lock);
    unsigned int stopping = 0u;
    const unsigned int servers = cli_servers_checking() ? 0u : cli_servers_listed(s_servers, CLI_SERVERS_MOST);
    const unsigned int needed = cli_servers_tools_for(open, languages, s_needed, CLI_SERVERS_MOST);
    for (unsigned int at = 0u; at < servers; at += 1u)
    {
        if (!memory_tool_among(s_servers[at].tool, s_needed, needed))
        {
            s_stopping[stopping] = s_servers[at].tool;
            stopping += 1u;
        }
    }
    if ((servers > 0u) && (stopping == 0u) && memory_read() && (s_reading.working > budget))
    {
        const unsigned int kept = (shown != NULL) ? cli_servers_tools_for(&shown, 1u, s_kept, CLI_SERVERS_MOST) : 0u;
        for (unsigned int at = 0u; (at < s_reading.count) && (stopping == 0u); at += 1u)
        {
            const char *const tool = s_reading.part[at].server;
            for (unsigned int server = 0u; (tool[0] != '\0') && (server < servers) && (stopping == 0u); server += 1u)
            {
                if (cellul.eq(
                        &(CatenaFinitaCfg){.src = s_servers[server].tool, .other = tool, .cap = CLI_CELLS_POOL}) &&
                    !memory_tool_among(tool, s_kept, kept))
                {
                    s_stopping[0] = s_servers[server].tool;
                    stopping = 1u;
                }
            }
        }
    }
    mmgr_line_put(out, "{\"stopped\":[");
    for (unsigned int at = 0u; at < stopping; at += 1u)
    {
        const char *named = s_stopping[at];
        for (unsigned int server = 0u; server < servers; server += 1u)
        {
            named = (s_servers[server].tool == s_stopping[at]) ? s_servers[server].program : named;
        }
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, named);
    }
    mmgr_line_put(out, "],\"files\":");
    cli_servers_stop(s_stopping, stopping, out);
    mmgr_line_ch(out, '}');
    cli_lock_give(&s_lock);
    cli_serve_strings_given(open);
    return 1;
}

const CliServeCall g_cli_window_memory_calls[] = {
    {"memory_usage", memory_usage},
    {"servers_unneeded_stop", memory_servers_unneeded_stop},
};
const unsigned int g_cli_window_memory_count = sizeof(g_cli_window_memory_calls) / sizeof(g_cli_window_memory_calls[0]);
