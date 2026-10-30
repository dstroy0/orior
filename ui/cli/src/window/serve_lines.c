// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_lines.c: the window answered over the standard input and output, as `orior_cli serve` answers a window over a
// link: each line of input a call, `{id, name, args}`, answered on one of the server's call threads by a line `{id,
// ok}` or `{id, error}` once it is ready, a line read waiting for a thread to be free; what the server emits sent as
// lines `{event, body}` as it comes, each line written whole; and, once the input ends, the calls made before it
// answered, then every server and session ended
#include "../command_line/cells.h"
#include "serve_internal.h"

// the call threads, the most bytes of a line of input, of an answer and of why a call failed
#define CLI_SERVE_CALLERS 8u
#define CLI_SERVE_LINE (16ull << 20u)
#define CLI_SERVE_ANSWER (16ull << 20u)
#define CLI_SERVE_ERR_MSG_MAX_LEN (1ull << 16u)

// A call thread: whether it has a call; the line it was handed, a cell given back once the call is answered, and its
// size; and its thread
typedef struct
{
    int busy;
    char *line;
    unsigned long long size;
    CliThread thread;
} CliServeCaller;

static CliServeCaller s_caller[CLI_SERVE_CALLERS];
static unsigned int s_started;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static CliLock s_output = CLI_LOCK_INIT;

static CliServeLink s_link;

// `size` bytes of a line written whole to the link with its end
static void lines_sent(const char *line, unsigned long long size)
{
    cli_lock_take(&s_output);
    s_link.write(s_link.context, line, size);
    s_link.write(s_link.context, "\n", 1ull);
    cli_lock_give(&s_output);
}

// `size` bytes of a line handed to the link as the line fills
static int lines_link_put(void *context, const char *bytes, size_t size)
{
    (void)context;
    s_link.write(s_link.context, bytes, size);
    return 1;
}

// what the server emits sent as a line `{event, body}`, written to the link a piece at a time and whole, no other
// line written between its pieces
static void lines_event_sent(void *context, const char *event, const char *body, unsigned long long size)
{
    (void)context;
    char piece[4096];
    cli_lock_take(&s_output);
    VerbaLinea line = {piece, sizeof(piece), 0ull, lines_link_put, NULL, 0ull};
    mmgr_line_put(&line, "{\"event\":");
    cli_json_quoted(&line, event, cellul.len(&(CatenaFinitaCfg){.src = event, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"body\":");
    mmgr_line_put_n(&line, body, size);
    mmgr_line_put(&line, "}\n");
    (void)mmgr_line_flush(&line);
    cli_lock_give(&s_output);
}

// the call a caller has answered, by the line `{id, ok}` or `{id, error}`, made in `answer` and with why it failed in
// `why`, cells of CLI_SERVE_ANSWER and CLI_SERVE_ERR_MSG_MAX_LEN bytes or NULL where none could be taken
static void lines_answered(CliServeCaller *caller, char *answer, char *why)
{
    unsigned long long id_at = 0ull;
    unsigned long long id_length = 0ull;
    unsigned long long name_at = 0ull;
    unsigned long long name_length = 0ull;
    unsigned long long args_at = 0ull;
    unsigned long long args_length = 0ull;
    const int has_id = cli_lsp_member(caller->line, caller->size, "id", &id_at, &id_length);
    char name[CLI_NAME_MAX_LEN];
    if (!cli_lsp_member(caller->line, caller->size, "name", &name_at, &name_length) ||
        !cli_lsp_string(caller->line + name_at, name_length, name, sizeof(name)))
    {
        name[0] = '\0';
    }
    const int has_args = cli_lsp_member(caller->line, caller->size, "args", &args_at, &args_length);
    if ((answer == NULL) || (why == NULL))
    {
        static const char s_refused[] = "{\"id\":null,\"error\":\"the call's cells could not be taken\"}";
        lines_sent(s_refused, sizeof(s_refused) - 1u);
        return;
    }
    static const char s_head[] = "{\"id\":";
    VerbaLinea out = {answer, CLI_SERVE_ANSWER, 0ull};
    mmgr_line_put(&out, s_head);
    mmgr_line_put_n(&out, has_id ? caller->line + id_at : "null", has_id ? id_length : 4ull);
    mmgr_line_put(&out, ",\"ok\":");
    const unsigned long long head = out.at;
    VerbaLinea err_msg = {why, CLI_SERVE_ERR_MSG_MAX_LEN, 0ull};
    const int answered =
        cli_serve_call(name, has_args ? caller->line + args_at : "", has_args ? args_length : 0ull, &out, &err_msg);
    if (answered && mmgr_line_ok(&out))
    {
        if (out.at == head)
        {
            mmgr_line_put(&out, "null");
        }
        mmgr_line_ch(&out, '}');
        lines_sent(answer, out.at);
        return;
    }
    out.at = 0ull;
    mmgr_line_put(&out, s_head);
    mmgr_line_put_n(&out, has_id ? caller->line + id_at : "null", has_id ? id_length : 4ull);
    mmgr_line_put(&out, ",\"error\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
    mmgr_line_ch(&out, '}');
    lines_sent(answer, out.at);
}

// a call thread: each call it is handed answered
static void lines_caller(void *context)
{
    CliServeCaller *const caller = context;
    for (;;)
    {
        cli_lock_take(&s_lock);
        while (caller->size == 0ull)
        {
            cli_wake_wait(&s_wake, &s_lock);
        }
        cli_lock_give(&s_lock);
        char *const answer = cli_lane_alloc(CLI_SERVE_ANSWER, &g_cli_window_cells);
        char *const why = cli_lane_alloc(CLI_SERVE_ERR_MSG_MAX_LEN, &g_cli_window_cells);
        lines_answered(caller, answer, why);
        cli_cell_release(why, &g_cli_window_cells);
        cli_cell_release(answer, &g_cli_window_cells);
        cli_cell_release(caller->line, &g_cli_window_cells);
        cli_lock_take(&s_lock);
        caller->size = 0ull;
        caller->busy = 0;
        cli_wake_all(&s_wake);
        cli_lock_give(&s_lock);
    }
}

// a line handed to a free call thread, one started where none is free and fewer than CLI_SERVE_CALLERS run, else
// waited for
static void lines_handed(const char *line, unsigned long long size)
{
    cli_lock_take(&s_lock);
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
            s_started += cli_thread_started(&s_caller[at].thread) ? 1u : 0u;
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
            cli_wake_all(&s_wake);
            break;
        }
        cli_wake_wait(&s_wake, &s_lock);
    }
    cli_lock_give(&s_lock);
}

// a line of the standard input read into `line`, sized for `room`: its size, QRY_MEMBER_UNREAD once the input ends
static unsigned long long lines_terminal_read(void *context, char *line, unsigned long long room)
{
    (void)context;
    return query_terminal_line_read(line, room);
}

// `size` bytes written whole to the standard output
static void lines_terminal_written(void *context, const char *bytes, unsigned long long size)
{
    (void)context;
    query_terminal_written(0, bytes, size);
}

void cli_serve(const char *root)
{
    const CliServeLink terminal = {lines_terminal_read, lines_terminal_written, NULL};
    cli_serve_over(root, &terminal);
}

void cli_serve_over(const char *root, const CliServeLink *link)
{
    s_link = *link;
    // a line of input is read whole or refused, and the room it is read into fits the most a line takes, a cell taken
    // while the server serves
    char *const input = cli_lane_alloc(CLI_SERVE_LINE, &g_cli_window_cells);
    if (input == NULL)
    {
        static const char s_roomless[] = "{\"error\":\"the room a call is read in could not be taken\"}";
        lines_event_sent(NULL, "refused", s_roomless, sizeof(s_roomless) - 1u);
        return;
    }
    char home[CLI_PATH_MAX_LEN];
    char detached_dir[CLI_PATH_MAX_LEN];
    const int has_home = cli_home_folder(home);
    if (has_home)
    {
        cli_full(home, "runs", detached_dir);
    }
    cli_serve_begun(root, has_home ? detached_dir : NULL);
    cli_serve_emit_set(lines_event_sent, NULL);
    cli_serve_watch();
    char serving[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea body = {serving, sizeof(serving), 0ull};
    mmgr_line_put(&body, "{\"version\":\"" CLI_VERSION "\",\"root\":");
    char tree[CLI_PATH_MAX_LEN];
    if (cli_serve_root_now(tree))
    {
        cli_json_quoted_lossy(&body, (const unsigned char *)tree,
                              cellul.len(&(CatenaFinitaCfg){.src = tree, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(&body, "null");
    }
    mmgr_line_ch(&body, '}');
    lines_event_sent(NULL, "serving", serving, body.at);
    for (;;)
    {
        const unsigned long long size = s_link.read(s_link.context, input, CLI_SERVE_LINE);
        if (size == QRY_MEMBER_UNREAD)
        {
            break;
        }
        // a line longer than the input fits is refused whole, and none of it is read as a call
        if (size >= CLI_SERVE_LINE)
        {
            static const char s_long[] = "{\"error\":\"a call longer than the server reads: pass its text through a "
                                         "file\"}";
            lines_event_sent(NULL, "refused", s_long, sizeof(s_long) - 1u);
            continue;
        }
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(input, size, &from, &to);
        // a line that is no whole JSON value is passed over
        if ((to > from) && (cli_json_value_end(input + from, to - from, 0ull) == to - from))
        {
            lines_handed(input + from, to - from);
        }
    }
    cli_cell_release(input, &g_cli_window_cells);
    // the calls made before the input ended are answered before the server ends
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < s_started; at += 1u)
    {
        while (s_caller[at].busy)
        {
            cli_wake_wait(&s_wake, &s_lock);
        }
    }
    cli_lock_give(&s_lock);
    cli_serve_stop_all();
}
