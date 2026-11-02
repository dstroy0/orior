// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_report.c: the window's calls on reports: an error the window met filed on its own where the reporter lets
// errors file, and a panic of the window's filed the same, a bug report the reader wrote filed with the run's recent
// errors where they requested them, whether errors file on their own and the question of it, and a page of orior's
// repository opened
#include "../machine/report.h"
#include "window_internal.h"

// the bytes the recent errors a report takes are read in
#define CLI_WINDOW_REPORT_ROOM (2ull << 20u)

static CliLock s_lock = CLI_LOCK_INIT;

typedef struct
{
    const CliFiled *filed;
    VerbaLinea *out;
} WindowCallReportCtx;
// where a report went written as the page reads it, `{kind, url}`
EMBED_INLINE void report_filed_write(const WindowCallReportCtx *args)
{
    VerbaLinea *out = args->out;
    static const char *const s_kinds[3] = {"filed", "known", "page"};
    mmgr_line_put(out, "{\"kind\":\"");
    mmgr_line_put(out, (args->filed->kind < 3u) ? s_kinds[args->filed->kind] : "page");
    mmgr_line_put(out, "\",\"url\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = args->filed->url);
    mmgr_line_ch(out, '}');
}

// an error the window met filed on its own where the reporter lets errors file: where it went, or null
static int report_error(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const category = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "category", .err_msg = err_msg);
    const char *const message = (category != NULL)
                                    ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                 .request = arguments, .key = "message", .err_msg = err_msg)
                                    : NULL;
    const char *const detail = (message != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                              .request = arguments, .key = "detail", .err_msg = err_msg)
                                                 : NULL;
    if (detail == NULL)
    {
        return 0;
    }
    char root[CLI_PATH_MAX_LEN];
    const int has_root = EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root);
    static CliFiled s_filed;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (EMBED_CALL(cli_report_category.error, CliReportCategoryCfg, .category = category, .message = message,
                   .detail = detail, .root = has_root ? root : NULL, .filed = &s_filed))
    {
        EMBED_CALL(report_filed_write, WindowCallReportCtx, .filed = &s_filed, .out = out);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return 1;
}

// the most bytes of a panic's message and of its backtrace filed
#define CLI_WINDOW_PANIC_MESSAGE 4096u
#define CLI_WINDOW_PANIC_DETAIL (64u << 10u)

void cli_window_panicked(const char *message, unsigned long long message_size, const char *detail,
                         unsigned long long detail_size)
{
    static char s_message[CLI_WINDOW_PANIC_MESSAGE];
    static char s_detail[CLI_WINDOW_PANIC_DETAIL];
    static CliFiled s_panic_filed;
    char root[CLI_PATH_MAX_LEN];
    const int has_root = EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    // each cut to its room, a 0 after it
    VerbaLinea message_line = {s_message, sizeof(s_message), 0ull};
    mmgr_line_put_n(&message_line, message,
                    (message_size < sizeof(s_message) - 1u) ? message_size : sizeof(s_message) - 1u);
    (void)mmgr_line_finish(&message_line);
    VerbaLinea detail_line = {s_detail, sizeof(s_detail), 0ull};
    mmgr_line_put_n(&detail_line, detail, (detail_size < sizeof(s_detail) - 1u) ? detail_size : sizeof(s_detail) - 1u);
    (void)mmgr_line_finish(&detail_line);
    (void)EMBED_CALL(cli_report_category.error, CliReportCategoryCfg,
                     .category = EMBED_CALL(cli_report.panic_category, CliReportCfg, .trace = s_detail),
                     .message = s_message, .detail = s_detail, .root = has_root ? root : NULL, .filed = &s_panic_filed);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// a bug report the reader wrote filed, with the run's recent errors where they requested them: where it went
static int report_bug(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int report =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "report");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = arguments->json,
                    .node = report, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = arguments,
                   .node = report, .key = "report", .expected = "struct Report", .err_msg = err_msg);
        return 0;
    }
    const int with_errors = EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                       .key = "with_errors", .otherwise_i32 = 0);
    static const char *const s_fields[5] = {"category", "title", "what", "steps", "logs"};
    const char *field[5];
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = arguments->json, .object = report, .name = s_fields[at]),
                       .length_pointer = NULL);
        field[at] = (text != NULL) ? text : "";
    }
    static CliFiled s_filed;
    // the recent errors and the logs with them in lines of their own, given back once the report is filed
    VerbaLinea recent = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    VerbaLinea logs = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (with_errors)
    {
        char *const recent_cell = g_cli_cells->temporary_buf_alloc(CLI_WINDOW_REPORT_ROOM, &g_cli_window_cells);
        recent = mmgr_line_over(recent_cell, CLI_WINDOW_REPORT_ROOM);
        EMBED_CALL(cli_report_root_out.recent, CliReportRootOutCfg, .out = &recent);
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        unsigned long long errors_from = 0ull;
        unsigned long long errors_to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = field[4],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = field[4], .cap = CLI_CELLS_POOL}), .from_u64s = &from,
                   .to_u64s = &to);
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = recent.out,
                   .size = mmgr_line_ok(&recent) ? recent.at : 0ull, .from_u64s = &errors_from, .to_u64s = &errors_to);
        const unsigned long long logs_room = (to - from) + (errors_to - errors_from) + 4ull;
        char *const logs_cell = g_cli_cells->temporary_buf_alloc(logs_room, &g_cli_window_cells);
        logs = mmgr_line_over(logs_cell, logs_room);
        mmgr_line_put_n(&logs, field[4] + from, to - from);
        mmgr_line_put(&logs, ((to > from) && (errors_to > errors_from)) ? "\n\n" : "");
        mmgr_line_put_n(&logs, recent.out + errors_from, errors_to - errors_from);
        field[4] = (mmgr_line_finish(&logs) != 0ull) ? logs.out : field[4];
    }
    const CliReport given = {field[0], field[1], field[2], field[3], field[4]};
    char root[CLI_PATH_MAX_LEN];
    const int has_root = EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root);
    EMBED_CALL(cli_report_root.file, CliReportRootCfg, .report = &given, .root = has_root ? root : NULL,
               .filed = &s_filed);
    EMBED_CALL(report_filed_write, WindowCallReportCtx, .filed = &s_filed, .out = out);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    g_cli_cells->persistent_buf_release(logs.out, &g_cli_window_cells);
    g_cli_cells->persistent_buf_release(recent.out, &g_cli_window_cells);
    return 1;
}

// whether errors file on their own
static int report_auto(const CliServeAnswerCfg *args)
{
    mmgr_line_put(args->out, cli_report_auto() ? "true" : "false");
    return 1;
}

// errors filing on their own turned on or off
static int report_auto_set(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    int on = 0;
    return EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "on",
                      .value_i32s = &on, .err_msg = err_msg) &&
           EMBED_CALL(cli_report_on.set_auto, CliReportOnCfg, .on = on, .err_msg = err_msg);
}

// whether the question of whether errors file on their own needs no prompting, as the reporter has answered it or
// ORIOR_NO_REPORTS turned reports off for the run, and the question
static int report_prompt(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char value[16];
    const int prompted =
        cli_report_prompt() || EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx,
                                          .name = "ORIOR_NO_REPORTS", .value_out = value, .room = sizeof(value));
    mmgr_line_put(out, prompted ? "[true," : "[false,");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = CLI_REPORT_QUESTION);
    mmgr_line_ch(out, ']');
    return 1;
}

// a page of orior's repository opened in the browser
static int report_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const url = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "url", .err_msg = err_msg);
    return (url != NULL) &&
           EMBED_CALL(cli_report_err_msg.open_page, CliReportErrMsgCfg, .url = url, .err_msg = err_msg);
}

const CliServeCall g_cli_window_report_calls[] = {
    {"report_error", report_error},       {"report_bug", report_bug},         {"report_auto", report_auto},
    {"report_auto_set", report_auto_set}, {"report_prompted", report_prompt}, {"report_open", report_open},
};
const unsigned int g_cli_window_report_count = sizeof(g_cli_window_report_calls) / sizeof(g_cli_window_report_calls[0]);
