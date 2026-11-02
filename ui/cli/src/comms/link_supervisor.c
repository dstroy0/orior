// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_supervisor.c: the link made: the server started on the machine and waited on to report it serves, the link
// maintained on a thread of its own and made again each time it drops until it is ended, the calls on the tree's side
// written to the server and waited on, and the server ended
#include "../command_line/cells.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "link_internal.h"

typedef struct
{
    VerbaLinea *out;
    const char *why;
    unsigned long long size;
    VerbaLinea *why_verba_linea_pointer;
    unsigned long long until;
    unsigned long long pipe;
    unsigned long long id;
    const char *name;
    const char *request;
} LinkSupervisorCtx;

// the machine, as the window names it, into `out`
EMBED_INLINE void link_machine_write(const LinkSupervisorCtx *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = &g_cli_link.address, .out = out);
}

// the server's process ended and let go, on the store's thread: ended under both locks, so that no call writes to
// it and nothing ends it again, then waited on with its errors' reader, and let go under both locks
static void link_server_stop(void)
{
    g_cli_link.joined = 0;
    g_cli_link.stopping = 1;
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &g_cli_link.spawned);
    EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &g_cli_link.spawned);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &g_cli_link.spawned);
    while (g_cli_link.reading_errors)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &g_cli_link_changed);
    }
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &g_cli_link.spawned);
    g_cli_link.running = 0;
    g_cli_link.stopping = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_link_changed);
}

// the link dropped: its server let go of, and every call waiting answered with the drop and why, `size` bytes of it
EMBED_INLINE void link_on_drop(const LinkSupervisorCtx *args)
{
    link_server_stop();
    for (unsigned int at = 0u; at < CLI_LINK_CALLS; at += 1u)
    {
        CliLinkCall *const call = &g_cli_link.calls[at];
        if (call->used && !call->done)
        {
            call->done = 2;
            mmgr_line_put(call->err_msg, "the link to ");
            EMBED_CALL(link_machine_write, LinkSupervisorCtx, .out = call->err_msg);
            mmgr_line_put(call->err_msg, " dropped");
            if (args->size > 0u)
            {
                mmgr_line_put(call->err_msg, ": ");
                mmgr_line_put_n(call->err_msg, args->why, args->size);
            }
        }
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_link_changed);
}

// whether the line the server wrote first reports it serves
EMBED_INLINE int link_serving(const LinkSupervisorCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char event[16];
    VerbaLinea none = {NULL, 0ull, 0ull};
    return EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = g_cli_link_line, .size = size,
                      .out = &none) &&
           EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = g_cli_link_line, .size_value = size, .name = "event",
                      .start = &start, .length_pointer = &length) &&
           EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = g_cli_link_line + start, .length = length,
                      .out_room = event, .room = sizeof(event)) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "serving", .cap = CLI_CELLS_POOL}));
}

// the server started on the machine and waited on to report it serves: 1, or 0 with why given
EMBED_INLINE int link_join(const LinkSupervisorCtx *args)
{
    if (!g_cli_link.has_start)
    {
        VerbaLinea start = {g_cli_link.start, sizeof(g_cli_link.start), 0ull};
        if (!EMBED_CALL(cli_link_err_msg.prepare, CliLinkErrMsgCfg, .address = &g_cli_link.address,
                        .stated = cli_link_state, .context = NULL, .start = &start,
                        .err_msg = args->why_verba_linea_pointer))
        {
            return 0;
        }
        if (mmgr_line_finish(&start) == 0ull)
        {
            mmgr_line_put(args->why_verba_linea_pointer,
                          "the line that starts orior's server is longer than orior_cli runs");
            return 0;
        }
        g_cli_link.has_start = 1;
    }
    static CliWords s_words;
    EMBED_CALL(cli_link_address_table.runs, CliLinkAddressCfg, .address = &g_cli_link.address, .line = g_cli_link.start,
               .terminal = 0, .words = &s_words);
    g_cli_link.stderr_tail_size = 0ull;
    if (s_words.full ||
        !EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &g_cli_link.spawned,
                    .words = s_words.word, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1,
                    .err_msg = args->why_verba_linea_pointer))
    {
        mmgr_line_put(args->why_verba_linea_pointer,
                      s_words.full ? "the line that starts orior's server is longer than orior_cli runs" : "");
        return 0;
    }
    g_cli_link.running = 1;
    g_cli_link.reading_errors = 1;
    g_cli_link.errors = (CliThread){cli_link_errors_read, NULL};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &g_cli_link.errors))
    {
        g_cli_link.reading_errors = 0;
    }
    g_cli_link.piece_at = 0ull;
    g_cli_link.piece_end = 0ull;
    unsigned long long size = 0ull;
    int whole = 0;
    if (!EMBED_CALL(cli_link_size_whole_internal.line, CliLinkSizeWholeCtx, .size_pointer = &size, .whole = &whole) ||
        !whole || !EMBED_CALL(link_serving, LinkSupervisorCtx, .size = size))
    {
        link_server_stop();
        // a server that will not start is sent again on the next try
        g_cli_link.has_start = 0;
        const unsigned long long from = args->why_verba_linea_pointer->at;
        EMBED_CALL(cli_link_internal.stderr_write, CliLinkCtx, .out = args->why_verba_linea_pointer);
        if (args->why_verba_linea_pointer->at == from)
        {
            mmgr_line_put(args->why_verba_linea_pointer, "orior's server on ");
            EMBED_CALL(link_machine_write, LinkSupervisorCtx, .out = args->why_verba_linea_pointer);
            mmgr_line_put(args->why_verba_linea_pointer, " did not start");
        }
        return 0;
    }
    g_cli_link.joined = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_link_changed);
    return 1;
}

// the link made, and made again each time it drops, until it is ended
static void link_on_join(void *context)
{
    (void)context;
    static char s_why[CLI_ERR_MSG_MAX_LEN + CLI_LINK_STDERR_TAIL_LEN];
    unsigned long long wait = 1000ull;
    int once = 0;
    // the server's line a cell for as long as the store runs; with no room for it the link fails at once
    g_cli_link_line = g_cli_cells->persistent_buf_alloc(CLI_LINK_LINE, &g_cli_comms_cells);
    if (g_cli_link_line == NULL)
    {
        static const char s_untaken[] = "the line the link's server writes could not be taken";
        EMBED_CALL(cli_link_state_internal.state_set, CliLinkStateCtx, .state = "failed", .detail = s_untaken,
                   .size = sizeof(s_untaken) - 1u);
    }
    while (g_cli_link_line != NULL)
    {
        const int ended = g_cli_link.ended;
        if (ended)
        {
            break;
        }
        EMBED_CALL(cli_link_state_internal.state_set, CliLinkStateCtx, .state = once ? "joining-again" : "joining",
                   .detail = "", .size = 0ull);
        VerbaLinea why = {s_why, sizeof(s_why), 0ull};
        if (EMBED_CALL(link_join, LinkSupervisorCtx, .why_verba_linea_pointer = &why))
        {
            once = 1;
            wait = 1000ull;
            EMBED_CALL(cli_link_state_internal.state_set, CliLinkStateCtx, .state = "joined", .detail = "",
                       .size = 0ull);
            unsigned long long size = 0ull;
            int whole = 0;
            while (EMBED_CALL(cli_link_size_whole_internal.line, CliLinkSizeWholeCtx, .size_pointer = &size,
                              .whole = &whole))
            {
                EMBED_CALL(cli_link_size_internal.recv, CliLinkSizeCtx, .size = size, .whole_value = whole);
            }
            EMBED_CALL(cli_link_internal.stderr_write, CliLinkCtx, .out = &why);
            EMBED_CALL(link_on_drop, LinkSupervisorCtx, .why = s_why, .size = why.at);
            const int over = g_cli_link.ended;
            if (over)
            {
                break;
            }
            EMBED_CALL(cli_link_state_internal.state_set, CliLinkStateCtx, .state = "dropped", .detail = s_why,
                       .size = why.at);
            continue;
        }
        EMBED_CALL(cli_link_state_internal.state_set, CliLinkStateCtx, .state = "failed", .detail = s_why,
                   .size = why.at);
        if (!g_cli_link.ended)
        {
            (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &g_cli_link_changed,
                             .milliseconds_u64 = wait);
        }
        wait = (wait * 2u < CLI_LINK_JOIN_MOST) ? (wait * 2u) : CLI_LINK_JOIN_MOST;
    }
    g_cli_cells->persistent_buf_release(g_cli_link_line, &g_cli_comms_cells);
    g_cli_link_line = NULL;
    g_cli_link.open = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_link_changed);
}

int cli_link_open(const CliLinkErrMsgAddressCfg *args)
{
    const CliLinkAddress *address = args->address;
    CliLinkEventFn emit = args->emit;
    void *context = args->context;
    if (g_cli_link.open)
    {
        mmgr_line_put(args->err_msg, "a link is open already");
        return 0;
    }
    g_cli_link.address = *address;
    g_cli_link.emit = emit;
    g_cli_link.context = context;
    g_cli_link.open = 1;
    g_cli_link.ended = 0;
    g_cli_link.joined = 0;
    g_cli_link.has_start = 0;
    g_cli_link.standing_size = 0ull;
    g_cli_link.next = 1ull;
    memor.set(&(MemoriaCfg){.dst = g_cli_link.calls, .val = 0, .bytes = sizeof(g_cli_link.calls)});
    g_cli_link.store = (CliThread){link_on_join, NULL};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &g_cli_link.store))
    {
        g_cli_link.open = 0;
        mmgr_line_put(args->err_msg, "the link's thread could not start");
        return 0;
    }
    return 1;
}

int cli_link_address(const CliLinkTextCfg *args)
{
    CliLinkAddress *address = args->address_out;
    const int open = g_cli_link.open;
    if (open)
    {
        *address = g_cli_link.address;
    }
    return open;
}

void cli_link_standing(const CliLinkOutPathCfg *args)
{
    if (g_cli_link.standing_size > 0u)
    {
        mmgr_line_put_n(args->out, g_cli_link.standing, g_cli_link.standing_size);
    }
    else
    {
        char text[CLI_PATH_MAX_LEN * 3u];
        VerbaLinea line = {text, sizeof(text), 0ull};
        mmgr_line_put(args->out, "{\"state\":\"joining\",\"machine\":");
        EMBED_CALL(link_machine_write, LinkSupervisorCtx, .out = &line);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = text,
                   .length = line.at);
        mmgr_line_put(args->out, ",\"address\":");
        line.at = 0ull;
        EMBED_CALL(cli_link_out.address_text, CliLinkOutCfg, .address = &g_cli_link.address, .out = &line);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = text,
                   .length = line.at);
        mmgr_line_put(args->out, ",\"message\":\"\"}");
    }
}

// a place among the calls taken for a call, waiting while all are taken, the lock acquired: the place, or NULL where
// the link ended or was not made within `until`
EMBED_INLINE CliLinkCall *link_call_take(const LinkSupervisorCtx *args)
{
    for (;;)
    {
        const unsigned long long now = cli_clock_milliseconds();
        if (g_cli_link.ended || (now >= args->until))
        {
            return NULL;
        }
        for (unsigned int at = 0u; g_cli_link.joined && (at < CLI_LINK_CALLS); at += 1u)
        {
            if (!g_cli_link.calls[at].used)
            {
                return &g_cli_link.calls[at];
            }
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &g_cli_link_changed,
                         .milliseconds_u64 = args->until - now);
    }
}

// a call's line written to the server: 1, or 0 where the server stopped reading
EMBED_INLINE int link_call_write(const LinkSupervisorCtx *args)
{
    unsigned long long size = args->size;
    char head[CLI_NAME_MAX_LEN * 8u];
    VerbaLinea line = {head, sizeof(head), 0ull};
    mmgr_line_put(&line, "{\"id\":");
    mmgr_line_decimal(&line, args->id, 1u);
    mmgr_line_put(&line, ",\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"args\":");
    return mmgr_line_ok(&line) &&
           EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = args->pipe, .bytes = head,
                      .size = line.at) &&
           EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = args->pipe,
                      .bytes = (size > 0u) ? args->request : "null", .size = (size > 0u) ? size : 4u) &&
           EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = args->pipe, .bytes = "}\n",
                      .size = 2u);
}

int cli_link_call(const CliLinkSizeOutCfg *args)
{
    const unsigned long long until = cli_clock_milliseconds() + CLI_LINK_JOINING;
    CliLinkCall *const call = EMBED_CALL(link_call_take, LinkSupervisorCtx, .until = until);
    if (call == NULL)
    {
        EMBED_CALL(link_machine_write, LinkSupervisorCtx, .out = args->err_msg);
        mmgr_line_put(args->err_msg, " is not reached");
        return 0;
    }
    *call = (CliLinkCall){1, 0, g_cli_link.next, args->out, args->err_msg};
    g_cli_link.next += 1ull;
    // the server's input is written under its own lock, taken before the link's, and only while the link is joined
    const int joined = g_cli_link.joined && !call->done;
    const unsigned long long pipe = g_cli_link.spawned.in;
    const int written = joined && EMBED_CALL(link_call_write, LinkSupervisorCtx, .pipe = pipe, .id = call->id,
                                             .name = args->name, .request = args->request, .size = args->size);
    if (!written && !call->done)
    {
        call->done = 2;
        mmgr_line_put(args->err_msg, "the link to ");
        EMBED_CALL(link_machine_write, LinkSupervisorCtx, .out = args->err_msg);
        mmgr_line_put(args->err_msg, " dropped");
    }
    while (!call->done)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &g_cli_link_changed);
    }
    const int answered = (call->done == 1);
    call->used = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_link_changed);
    return answered;
}

// the server ended where it runs and is not being stopped already, both locks acquired
static void link_server_end(void)
{
    if (g_cli_link.running && !g_cli_link.stopping)
    {
        g_cli_link.joined = 0;
        EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &g_cli_link.spawned);
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &g_cli_link.spawned);
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_link_changed);
}

void cli_link_end(void)
{
    g_cli_link.ended = 1;
    link_server_end();
}

void cli_link_drop_now(void)
{
    link_server_end();
}
