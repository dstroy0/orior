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
static CliProcess s_listed[CLI_PROCESSES_MOST];
static unsigned int s_members[CLI_MEMORY_PROCESSES];
static unsigned int s_found[CLI_MEMORY_PROCESSES];
static CliProcessMemory s_measured[CLI_MEMORY_PROCESSES];
static CliMemoryReading s_reading;
static CliServerListed s_servers[CLI_SERVERS_MOST];
static CliServerListed s_naming[CLI_SERVERS_MOST];
static char s_needed[CLI_SERVERS_MOST][CLI_NAME_MAX_LEN];
static char s_saved[CLI_SERVERS_MOST][CLI_NAME_MAX_LEN];
static const char *s_stopping[CLI_SERVERS_MOST];

void cli_window_browser_known(unsigned int pid)
{
    s_browser = pid;
}

typedef struct
{
    unsigned int pid;
    unsigned long long after;
    unsigned int *found;
    unsigned int listed;
    unsigned int own;
    unsigned int root;
    const char *name;
    const char *tool;
    char (*tools)[CLI_NAME_MAX_LEN];
    unsigned int count;
} WindowCallMemoryCtx;
// the process `pid` added to those found, `found` of them, where it is not among them, the system says what it holds,
// and it started no earlier than `after`
EMBED_INLINE void memory_found_add(const WindowCallMemoryCtx *args)
{
    unsigned int pid = args->pid;
    unsigned int *found = args->found;
    for (unsigned int at = 0u; at < *found; at += 1u)
    {
        if (s_found[at] == pid)
        {
            return;
        }
    }
    CliProcessMemory measured;
    if ((*found < CLI_MEMORY_PROCESSES) &&
        EMBED_CALL(cli_pid_internal.process_memory, CliPidCtx, .pid = pid, .memory = &measured) &&
        (measured.started >= args->after))
    {
        s_found[*found] = pid;
        s_measured[*found] = measured;
        *found += 1u;
    }
}

// the parent of `pid` as the system listed it, `listed` of them, `pid` itself where it is not listed
EMBED_INLINE unsigned int memory_parent_get(const WindowCallMemoryCtx *args)
{
    for (unsigned int at = 0u; at < args->listed; at += 1u)
    {
        if (s_listed[at].id == args->pid)
        {
            return s_listed[at].parent;
        }
    }
    return args->pid;
}

// the process the window started `pid` from: `pid`'s parent and theirs on up, to the one whose parent is the window or
// is not listed
EMBED_INLINE unsigned int memory_started_from(const WindowCallMemoryCtx *args)
{
    unsigned int at = args->pid;
    for (unsigned int depth = 0u; depth < CLI_MEMORY_DEPTH; depth += 1u)
    {
        const unsigned int parent =
            EMBED_CALL(memory_parent_get, WindowCallMemoryCtx, .pid = at, .listed = args->listed);
        if ((at == args->own) || (parent == args->own) || (parent == at) || (parent <= 1u))
        {
            return at;
        }
        at = parent;
    }
    return at;
}

// the program the window started as `root` among the parts read, added where it is not: it, or NULL where every part is
// taken
EMBED_INLINE CliMemoryPart *memory_part_get(const WindowCallMemoryCtx *args)
{
    for (unsigned int at = 0u; at < s_reading.count; at += 1u)
    {
        if (s_reading.part[at].pid == args->root)
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
    part->pid = args->root;
    if (args->root == args->own)
    {
        VerbaLinea named = {part->name, sizeof(part->name), 0ull};
        mmgr_line_put(&named, "orior_gui");
        (void)mmgr_line_finish(&named);
    }
    else
    {
        (void)EMBED_CALL(cli_room_pid_internal.process_by_name, CliRoomPidCtx, .pid = args->root,
                         .name_out = part->name, .room_size = sizeof(part->name));
    }
    return part;
}

// whether a part is the web view's: its browser on Windows, and on Linux each WebKit process the window starts
EMBED_INLINE int memory_web_view(const WindowCallMemoryCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "msedgewebview2", .cap = CLI_CELLS_POOL}) ||
           (memor.cmp(&(MemoriaCfg){.src = args->name, .other = "WebKit", .bytes = 6ull}) == 0);
}

// each part a language server is named by its program, with its toolchain's id, and the web view's processes as the
// web view
static void memory_parts_by_name(void)
{
    const unsigned int servers =
        EMBED_CALL(cli_servers_table.list, CliServersCfg, .servers = s_naming, .room = CLI_SERVERS_MOST);
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
        if (EMBED_CALL(memory_web_view, WindowCallMemoryCtx, .name = part->name))
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
    EMBED_CALL(memory_found_add, WindowCallMemoryCtx, .pid = own, .after = 0ull, .found = &found);
    if (found == 0u)
    {
        return 0;
    }
    const unsigned int listed = EMBED_CALL(cli_room_processes_internal.processes_list, CliRoomProcessesCtx,
                                           .processes = s_listed, .room_u32 = CLI_PROCESSES_MOST);
    if (CLI_MEMORY_WALKED && (s_browser != 0u) && (s_browser != own))
    {
        EMBED_CALL(memory_found_add, WindowCallMemoryCtx, .pid = s_browser, .after = 0ull, .found = &found);
    }
    // a process whose number an ended parent's took before that parent started is no child of it
    for (unsigned int at = 0u; CLI_MEMORY_WALKED && (at < found); at += 1u)
    {
        for (unsigned int each = 0u; each < listed; each += 1u)
        {
            if ((s_listed[each].parent == s_found[at]) && (s_listed[each].id != s_found[at]))
            {
                EMBED_CALL(memory_found_add, WindowCallMemoryCtx, .pid = s_listed[each].id,
                           .after = s_measured[at].started, .found = &found);
            }
        }
    }
    // a process of the container whose parent has ended stands apart from the walk, and counts all the same
    const unsigned int members =
        EMBED_CALL(cli_ids_internal.container_members, CliIdsCtx, .ids = s_members, .room_u32 = CLI_MEMORY_PROCESSES);
    for (unsigned int at = 0u; at < members; at += 1u)
    {
        EMBED_CALL(memory_found_add, WindowCallMemoryCtx, .pid = s_members[at], .after = 0ull, .found = &found);
    }
    memor.set(&(MemoriaCfg){.dst = &s_reading, .val = 0, .bytes = sizeof(s_reading)});
    for (unsigned int at = 0u; at < found; at += 1u)
    {
        CliMemoryPart *const part = EMBED_CALL(memory_part_get, WindowCallMemoryCtx,
                                               .root = EMBED_CALL(memory_started_from, WindowCallMemoryCtx,
                                                                  .pid = s_found[at], .own = own, .listed = listed),
                                               .own = own);
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
    memory_parts_by_name();
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
static int memory_usage(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    if (!memory_read())
    {
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
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = part->name);
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
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = part->server);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, "]}");
    return 1;
}

// whether `tool` is among the `count` toolchains of `tools`
EMBED_INLINE int memory_tool_among(const WindowCallMemoryCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->tool, .other = args->tools[at], .cap = CLI_CELLS_POOL}))
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
static int memory_servers_unneeded_stop(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long budget = 0ull;
    unsigned int languages = 0u;
    const char **open = NULL;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "budget",
                    .value = &budget, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = arguments, .key = "open",
                    .most = CLI_MEMORY_LANGUAGES, .items_pointer = &open, .count = &languages, .err_msg = err_msg))
    {
        EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = open);
        return 0;
    }
    const char *const shown = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                         .key = "shown", .otherwise = NULL);
    unsigned int stopping = 0u;
    const unsigned int servers = cli_servers_checking() ? 0u
                                                        : EMBED_CALL(cli_servers_table.list, CliServersCfg,
                                                                     .servers = s_servers, .room = CLI_SERVERS_MOST);
    const unsigned int needed = EMBED_CALL(cli_servers_room.tools_for, CliServersRoomCfg, .languages = open,
                                           .count = languages, .tools = s_needed, .room = CLI_SERVERS_MOST);
    for (unsigned int at = 0u; at < servers; at += 1u)
    {
        if (!EMBED_CALL(memory_tool_among, WindowCallMemoryCtx, .tool = s_servers[at].tool, .tools = s_needed,
                        .count = needed))
        {
            s_stopping[stopping] = s_servers[at].tool;
            stopping += 1u;
        }
    }
    if ((servers > 0u) && (stopping == 0u) && memory_read() && (s_reading.working > budget))
    {
        const unsigned int saved = (shown != NULL)
                                       ? EMBED_CALL(cli_servers_room.tools_for, CliServersRoomCfg, .languages = &shown,
                                                    .count = 1u, .tools = s_saved, .room = CLI_SERVERS_MOST)
                                       : 0u;
        for (unsigned int at = 0u; (at < s_reading.count) && (stopping == 0u); at += 1u)
        {
            const char *const tool = s_reading.part[at].server;
            for (unsigned int server = 0u; (tool[0] != '\0') && (server < servers) && (stopping == 0u); server += 1u)
            {
                if (cellul.eq(
                        &(CatenaFinitaCfg){.src = s_servers[server].tool, .other = tool, .cap = CLI_CELLS_POOL}) &&
                    !EMBED_CALL(memory_tool_among, WindowCallMemoryCtx, .tool = tool, .tools = s_saved, .count = saved))
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
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = named);
    }
    mmgr_line_put(out, "],\"files\":");
    EMBED_CALL(cli_servers_out.stop, CliServersOutCfg, .tools_letters = s_stopping, .count = stopping, .out = out);
    mmgr_line_ch(out, '}');
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = open);
    return 1;
}

const CliServeCall g_cli_window_memory_calls[] = {
    {"memory_usage", memory_usage},
    {"servers_unneeded_stop", memory_servers_unneeded_stop},
};
const unsigned int g_cli_window_memory_count = sizeof(g_cli_window_memory_calls) / sizeof(g_cli_window_memory_calls[0]);
