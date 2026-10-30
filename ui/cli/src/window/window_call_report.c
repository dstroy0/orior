// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_report.c: the window's calls on reports: an error the window met filed on its own where the reporter lets
// errors file, and a panic of the window's filed the same, a bug report the reader wrote filed with the run's recent
// errors where they requested them, whether errors file on their own and the question of it, and a page of orior's
// repository opened
#include "../machine/report.h"
#include "window_internal.h"

// the bytes the recent errors a report takes are begun to be read in, doubling as they are
#define CLI_WINDOW_REPORT_FIRST 4096u

static CliLock s_lock = CLI_LOCK_INIT;

// where a report went written as the page reads it, `{kind, url}`
static void report_filed_written(const CliFiled *filed, VerbaLinea *out)
{
    static const char *const s_kinds[3] = {"filed", "known", "page"};
    mmgr_line_put(out, "{\"kind\":\"");
    mmgr_line_put(out, (filed->kind < 3u) ? s_kinds[filed->kind] : "page");
    mmgr_line_put(out, "\",\"url\":");
    cli_window_quoted(out, filed->url);
    mmgr_line_ch(out, '}');
}

// an error the window met filed on its own where the reporter lets errors file: where it went, or null
static int report_error(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const category = cli_serve_string(args, "category", err_msg);
    const char *const message = (category != NULL) ? cli_serve_string(args, "message", err_msg) : NULL;
    const char *const detail = (message != NULL) ? cli_serve_string(args, "detail", err_msg) : NULL;
    if (detail == NULL)
    {
        return 0;
    }
    char root[CLI_PATH_MAX_LEN];
    const int has_root = cli_serve_root_now(root);
    static CliFiled s_filed;
    cli_lock_take(&s_lock);
    if (cli_report_error(category, message, detail, has_root ? root : NULL, &s_filed))
    {
        report_filed_written(&s_filed, out);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    cli_lock_give(&s_lock);
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
    const int has_root = cli_serve_root_now(root);
    cli_lock_take(&s_lock);
    // each cut to its room, a 0 after it
    VerbaLinea message_line = {s_message, sizeof(s_message), 0ull};
    mmgr_line_put_n(&message_line, message,
                    (message_size < sizeof(s_message) - 1u) ? message_size : sizeof(s_message) - 1u);
    (void)mmgr_line_finish(&message_line);
    VerbaLinea detail_line = {s_detail, sizeof(s_detail), 0ull};
    mmgr_line_put_n(&detail_line, detail, (detail_size < sizeof(s_detail) - 1u) ? detail_size : sizeof(s_detail) - 1u);
    (void)mmgr_line_finish(&detail_line);
    (void)cli_report_error(cli_report_panic_category(s_detail), s_message, s_detail, has_root ? root : NULL,
                           &s_panic_filed);
    cli_lock_give(&s_lock);
}

// a bug report the reader wrote filed, with the run's recent errors where they requested them: where it went
static int report_bug(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const unsigned int report = cli_serve_arg(args, "report");
    if (!cli_json_is(args->json, report, CFG_JSON_OBJECT))
    {
        cli_serve_refused(args, report, "report", "struct Report", err_msg);
        return 0;
    }
    const int with_errors = cli_serve_bool_or(args, "with_errors", 0);
    static const char *const s_fields[5] = {"category", "title", "what", "steps", "logs"};
    const char *field[5];
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        const char *const text = cli_json_string(args->json, cli_json_member(args->json, report, s_fields[at]), NULL);
        field[at] = (text != NULL) ? text : "";
    }
    static CliFiled s_filed;
    // the recent errors and the logs with them in growing lines, given back once the report is filed
    VerbaLinea recent = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    VerbaLinea logs = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    cli_lock_take(&s_lock);
    if (with_errors)
    {
        (void)cli_growing_line_temporary(&recent, CLI_WINDOW_REPORT_FIRST, &g_cli_window_cells);
        cli_report_recent(&recent);
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        unsigned long long errors_from = 0ull;
        unsigned long long errors_to = 0ull;
        cli_text_trimmed(field[4], cellul.len(&(CatenaFinitaCfg){.src = field[4], .cap = CLI_CELLS_POOL}), &from, &to);
        cli_text_trimmed(recent.out, mmgr_line_ok(&recent) ? recent.at : 0ull, &errors_from, &errors_to);
        (void)cli_growing_line_temporary(&logs, (to - from) + (errors_to - errors_from) + 4ull, &g_cli_window_cells);
        mmgr_line_put_n(&logs, field[4] + from, to - from);
        mmgr_line_put(&logs, ((to > from) && (errors_to > errors_from)) ? "\n\n" : "");
        mmgr_line_put_n(&logs, recent.out + errors_from, errors_to - errors_from);
        field[4] = (mmgr_line_finish(&logs) != 0ull) ? logs.out : field[4];
    }
    const CliReport given = {field[0], field[1], field[2], field[3], field[4]};
    char root[CLI_PATH_MAX_LEN];
    const int has_root = cli_serve_root_now(root);
    cli_report_file(&given, has_root ? root : NULL, &s_filed);
    report_filed_written(&s_filed, out);
    cli_lock_give(&s_lock);
    g_cli_cells->persistent_buf_release(logs.out, &g_cli_window_cells);
    g_cli_cells->persistent_buf_release(recent.out, &g_cli_window_cells);
    return 1;
}

// whether errors file on their own
static int report_auto(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    mmgr_line_put(out, cli_report_auto() ? "true" : "false");
    return 1;
}

// errors filing on their own turned on or off
static int report_auto_set(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    int on = 0;
    return cli_serve_bool(args, "on", &on, err_msg) && cli_report_set_auto(on, err_msg);
}

// whether the question of whether errors file on their own needs no prompting, as the reporter has answered it or
// ORIOR_NO_REPORTS turned reports off for the run, and the question
static int report_prompted(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char value[16];
    const int prompted = cli_report_prompted() || query_environment_read("ORIOR_NO_REPORTS", value, sizeof(value));
    mmgr_line_put(out, prompted ? "[true," : "[false,");
    cli_window_quoted(out, CLI_REPORT_QUESTION);
    mmgr_line_ch(out, ']');
    return 1;
}

// a page of orior's repository opened in the browser
static int report_open(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    const char *const url = cli_serve_string(args, "url", err_msg);
    return (url != NULL) && cli_report_open_page(url, err_msg);
}

const CliServeCall g_cli_window_report_calls[] = {
    {"report_error", report_error},       {"report_bug", report_bug},           {"report_auto", report_auto},
    {"report_auto_set", report_auto_set}, {"report_prompted", report_prompted}, {"report_open", report_open},
};
const unsigned int g_cli_window_report_count = sizeof(g_cli_window_report_calls) / sizeof(g_cli_window_report_calls[0]);
