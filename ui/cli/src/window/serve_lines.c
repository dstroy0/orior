// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_lines.c: the window answered over the standard input and output, as `orior_cli serve` answers a window over a
// link: each line of input a call, `{id, name, args}`, answered on one of the server's call threads by a line `{id,
// ok}` or `{id, error}` once it is ready, a line read waiting for a thread to be free; what the server emits sent as
// lines `{event, body}` as it comes, each line written whole; and, once the input ends, the calls made before it
// answered, then every server and session ended
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "serve_internal.h"

// A call thread: whether it has a call; the line it was handed, a cell given back once the call is answered, and its
// size; and its thread
typedef struct
{
    int busy;
    char *line;
    unsigned long long size;
    CliThread thread;
} CliServeCaller;
// the call threads, the most bytes of a line of input, of an answer and of why a call failed
#define CLI_SERVE_CALLERS 8u
#define CLI_SERVE_LINE (16ull << 20u)
#define CLI_SERVE_ANSWER (16ull << 20u)
#define CLI_SERVE_ERR_MSG_MAX_LEN (1ull << 16u)

static CliServeCaller s_caller[CLI_SERVE_CALLERS];
static unsigned int s_started;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static CliLock s_output = CLI_LOCK_INIT;

static CliServeLink s_link;

typedef struct
{
    const char *line;
    unsigned long long size;
    CliServeCaller *caller;
    char *answer;
    char *why;
} ServeLinesCtx;
// `size` bytes of a line written whole to the link with its end
EMBED_INLINE void lines_send(const ServeLinesCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_output);
    EMBED_CALL(s_link.write, CliEachBytesSizeCfg, .context = s_link.context, .bytes = args->line, .size = args->size);
    EMBED_CALL(s_link.write, CliEachBytesSizeCfg, .context = s_link.context, .bytes = "\n", .size = 1ull);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_output);
}

// `size` bytes of a line handed to the link as the line fills
static int lines_link_put(void *context, const char *bytes, size_t size)
{
    (void)context;
    EMBED_CALL(s_link.write, CliEachBytesSizeCfg, .context = s_link.context, .bytes = bytes, .size = size);
    return 1;
}

// what the server emits sent as a line `{event, body}`, written to the link a piece at a time and whole, no other
// line written between its pieces
static void lines_event_send(void *context, const char *event, const char *body, unsigned long long size)
{
    (void)context;
    char piece[4096];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_output);
    VerbaLinea line = {piece, sizeof(piece), 0ull, lines_link_put, NULL, 0ull};
    mmgr_line_put(&line, "{\"event\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = event,
               .length = cellul.len(&(CatenaFinitaCfg){.src = event, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"body\":");
    mmgr_line_put_n(&line, body, size);
    mmgr_line_put(&line, "}\n");
    (void)mmgr_line_flush(&line);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_output);
}

// the call a caller has answered, by the line `{id, ok}` or `{id, error}`, made in `answer` and with why it failed in
// `why`, cells of CLI_SERVE_ANSWER and CLI_SERVE_ERR_MSG_MAX_LEN bytes or NULL where none could be taken
EMBED_INLINE void lines_answer(const ServeLinesCtx *args)
{
    unsigned long long id_at = 0ull;
    unsigned long long id_length = 0ull;
    unsigned long long name_at = 0ull;
    unsigned long long name_length = 0ull;
    unsigned long long args_at = 0ull;
    unsigned long long args_length = 0ull;
    const int has_id =
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->caller->line, .size_value = args->caller->size,
                   .name = "id", .start = &id_at, .length_pointer = &id_length);
    char name[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->caller->line, .size_value = args->caller->size,
                    .name = "name", .start = &name_at, .length_pointer = &name_length) ||
        !EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = args->caller->line + name_at, .length = name_length,
                    .out_room = name, .room = sizeof(name)))
    {
        name[0] = '\0';
    }
    const int has_args =
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->caller->line, .size_value = args->caller->size,
                   .name = "args", .start = &args_at, .length_pointer = &args_length);
    if ((args->answer == NULL) || (args->why == NULL))
    {
        static const char s_refused[] = "{\"id\":null,\"error\":\"the call's cells could not be taken\"}";
        EMBED_CALL(lines_send, ServeLinesCtx, .line = s_refused, .size = sizeof(s_refused) - 1u);
        return;
    }
    static const char s_head[] = "{\"id\":";
    VerbaLinea out = {args->answer, CLI_SERVE_ANSWER, 0ull};
    mmgr_line_put(&out, s_head);
    mmgr_line_put_n(&out, has_id ? args->caller->line + id_at : "null", has_id ? id_length : 4ull);
    mmgr_line_put(&out, ",\"ok\":");
    const unsigned long long head = out.at;
    VerbaLinea err_msg = {args->why, CLI_SERVE_ERR_MSG_MAX_LEN, 0ull};
    const int answered = EMBED_CALL(cli_serve_err_msg.call, CliServeErrMsgCfg, .name = name,
                                    .request = has_args ? args->caller->line + args_at : "",
                                    .size = has_args ? args_length : 0ull, .out = &out, .err_msg = &err_msg);
    if (answered && mmgr_line_ok(&out))
    {
        if (out.at == head)
        {
            mmgr_line_put(&out, "null");
        }
        mmgr_line_ch(&out, '}');
        EMBED_CALL(lines_send, ServeLinesCtx, .line = args->answer, .size = out.at);
        return;
    }
    out.at = 0ull;
    mmgr_line_put(&out, s_head);
    mmgr_line_put_n(&out, has_id ? args->caller->line + id_at : "null", has_id ? id_length : 4ull);
    mmgr_line_put(&out, ",\"error\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->why, .length = mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
    mmgr_line_ch(&out, '}');
    EMBED_CALL(lines_send, ServeLinesCtx, .line = args->answer, .size = out.at);
}

// a call thread: each call it is handed answered
static void lines_caller(void *context)
{
    CliServeCaller *const caller = context;
    for (;;)
    {
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
        while (caller->size == 0ull)
        {
            EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_wake, .lock = &s_lock);
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        char *const answer = g_cli_cells->temporary_buf_alloc(CLI_SERVE_ANSWER, &g_cli_window_cells);
        char *const why = g_cli_cells->temporary_buf_alloc(CLI_SERVE_ERR_MSG_MAX_LEN, &g_cli_window_cells);
        EMBED_CALL(lines_answer, ServeLinesCtx, .caller = caller, .answer = answer, .why = why);
        g_cli_cells->persistent_buf_release(why, &g_cli_window_cells);
        g_cli_cells->persistent_buf_release(answer, &g_cli_window_cells);
        // a call's line may carry a secret, and the pool leaves a cell as it is
        mmgr_zero_buf(caller->line, caller->size);
        g_cli_cells->persistent_buf_release(caller->line, &g_cli_window_cells);
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
        caller->size = 0ull;
        caller->busy = 0;
        EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_wake);
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    }
}

// a line handed to a free call thread, one started where none is free and fewer than CLI_SERVE_CALLERS run, else
// waited for
EMBED_INLINE void lines_dispatch(const ServeLinesCtx *args)
{
    const char *line = args->line;
    unsigned long long size = args->size;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    for (;;)
    {
        unsigned int at = 0u;
        while ((at < s_started) && s_caller[at].busy)
        {
            at += 1u;
        }
        if ((at == s_started) && (s_started < CLI_SERVE_CALLERS))
        {
            s_caller[at].thread = (CliThread){lines_caller, &s_caller[at]};
            s_started +=
                EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_caller[at].thread) ? 1u : 0u;
            continue;
        }
        if (at < s_started)
        {
            CliServeCaller *const caller = &s_caller[at];
            caller->line = g_cli_cells->persistent_buf_alloc(size, &g_cli_window_cells);
            if (caller->line == NULL)
            {
                break;
            }
            caller->busy = 1;
            memor.cpy(&(MemoriaCfg){.dst = caller->line, .src = line, .bytes = (size_t)size});
            caller->size = size;
            EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_wake);
            break;
        }
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_wake, .lock = &s_lock);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// a line of the standard input read into `line`, sized for `room`: its size, QRY_MEMBER_UNREAD once the input ends
static unsigned long long lines_terminal_read(const CliEachLineRoomCfg *args)
{
    char *line = args->line;
    unsigned long long room = args->room;
    return EMBED_CALL(query_room_line_internal.terminal_line_read, QueryRoomLineCtx, .line = line, .room = room);
}

// `size` bytes written whole to the standard output
static void lines_terminal_write(const CliEachBytesSizeCfg *args)
{
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = args->bytes,
               .length = args->size);
}

void cli_serve(const CliServeRootCfg *args)
{
    const CliServeLink terminal = {lines_terminal_read, lines_terminal_write, NULL};
    EMBED_CALL(cli_serve_root_link.over, CliServeRootLinkCfg, .root = args->root, .link = &terminal);
}

void cli_serve_over(const CliServeRootLinkCfg *args)
{
    s_link = *args->link;
    // a line of input is read whole or refused, and the room it is read into fits the most a line takes, a cell taken
    // while the server serves
    char *const input = g_cli_cells->temporary_buf_alloc(CLI_SERVE_LINE, &g_cli_window_cells);
    if (input == NULL)
    {
        static const char s_roomless[] = "{\"error\":\"the room a call is read in could not be taken\"}";
        lines_event_send(NULL, "refused", s_roomless, sizeof(s_roomless) - 1u);
        return;
    }
    char home[CLI_PATH_MAX_LEN];
    char detached_dir[CLI_PATH_MAX_LEN];
    const int has_home = EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home);
    if (has_home)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "runs", .joined = detached_dir);
    }
    EMBED_CALL(cli_serve_root_table.begin, CliServeRootCfg, .root = args->root,
               .detached_dir = has_home ? detached_dir : NULL);
    EMBED_CALL(cli_serve_emit.emit_set, CliServeEmitCfg, .emit = lines_event_send, .context = NULL);
    cli_serve_watch();
    char serving[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea body = {serving, sizeof(serving), 0ull};
    mmgr_line_put(&body, "{\"version\":\"" CLI_VERSION "\",\"root\":");
    char tree[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = tree))
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &body,
                   .text_bytes = (const unsigned char *)tree,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = tree, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(&body, "null");
    }
    mmgr_line_ch(&body, '}');
    lines_event_send(NULL, "serving", serving, body.at);
    for (;;)
    {
        const unsigned long long size = EMBED_CALL(s_link.read, CliEachLineRoomCfg, .context = s_link.context,
                                                   .line = input, .room = CLI_SERVE_LINE);
        if (size == QRY_MEMBER_UNREAD)
        {
            break;
        }
        // a line longer than the input fits is refused whole, and none of it is read as a call
        if (size >= CLI_SERVE_LINE)
        {
            static const char s_long[] = "{\"error\":\"a call longer than the server reads: pass its text through a "
                                         "file\"}";
            lines_event_send(NULL, "refused", s_long, sizeof(s_long) - 1u);
            continue;
        }
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = input, .size = size,
                   .from_u64s = &from, .to_u64s = &to);
        // a line that is no whole JSON value is passed over
        if ((to > from) && (EMBED_CALL(cli_json_tree_text_size.json_value_end, CliJsonTreeTextSizeCfg,
                                       .text = input + from, .size = to - from, .at = 0ull) == to - from))
        {
            EMBED_CALL(lines_dispatch, ServeLinesCtx, .line = input + from, .size = to - from);
        }
    }
    g_cli_cells->persistent_buf_release(input, &g_cli_window_cells);
    // the calls made before the input ended are answered before the server ends
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    for (unsigned int at = 0u; at < s_started; at += 1u)
    {
        while (s_caller[at].busy)
        {
            EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_wake, .lock = &s_lock);
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    cli_serve_stop_all();
}
