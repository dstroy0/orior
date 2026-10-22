// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_report.c: the window's calls on reports: an error the window met filed on its own where the reporter lets
// errors file, a bug report the reader wrote filed with the run's recent errors where they asked for them, whether
// errors file on their own and the question of it, and a page of orior's repository opened
#include "../machine/report.h"
#include "window_internal.h"

// the bytes the recent errors a report takes are begun to be read in, doubling as they are
#define CLI_WINDOW_REPORT_FIRST 4096u

static CliLock s_lock = CLI_LOCK_INIT;

// where a report went written as the page reads it, `{kind, url}`
static void report_filed_written(const CliFiled *filed, CliLine *out)
{
    static const char *const s_kinds[3] = {"filed", "known", "page"};
    cli_line_text(out, "{\"kind\":\"");
    cli_line_text(out, (filed->kind < 3u) ? s_kinds[filed->kind] : "page");
    cli_line_text(out, "\",\"url\":");
    cli_window_quoted(out, filed->url);
    cli_line_character(out, '}');
}

// an error the window met filed on its own where the reporter lets errors file: where it went, or null
static int report_error(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const category = cli_serve_string(args, "category", said);
    const char *const message = (category != NULL) ? cli_serve_string(args, "message", said) : NULL;
    const char *const detail = (message != NULL) ? cli_serve_string(args, "detail", said) : NULL;
    if (detail == NULL)
    {
        return 0;
    }
    char root[CLI_PATH_LONGEST];
    const int has_root = cli_serve_root_now(root);
    static CliFiled s_filed;
    cli_lock_take(&s_lock);
    if (cli_report_error(category, message, detail, has_root ? root : NULL, &s_filed))
    {
        report_filed_written(&s_filed, out);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_lock_give(&s_lock);
    return 1;
}

// a panic of the window's filed as report_error files an error, its category by where its backtrace `detail` first
// enters orior's own code
static int report_panic(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const message = cli_serve_string(args, "message", said);
    const char *const detail = (message != NULL) ? cli_serve_string(args, "detail", said) : NULL;
    if (detail == NULL)
    {
        return 0;
    }
    char root[CLI_PATH_LONGEST];
    const int has_root = cli_serve_root_now(root);
    static CliFiled s_panic_filed;
    cli_lock_take(&s_lock);
    if (cli_report_error(cli_report_panic_category(detail), message, detail, has_root ? root : NULL, &s_panic_filed))
    {
        report_filed_written(&s_panic_filed, out);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_lock_give(&s_lock);
    return 1;
}

// a bug report the reader wrote filed, with the run's recent errors where they asked for them: where it went
static int report_bug(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const unsigned int report = cli_serve_arg(args, "report");
    if (!cli_json_is(args->json, report, CFG_JSON_OBJECT))
    {
        cli_serve_refused(args, report, "report", "struct Report", said);
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
    CliLine recent = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    CliLine logs = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    cli_lock_take(&s_lock);
    if (with_errors)
    {
        (void)cli_line_growing(&recent, CLI_WINDOW_REPORT_FIRST, "the recent errors a report takes");
        cli_report_recent(&recent);
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        unsigned long long errors_from = 0ull;
        unsigned long long errors_to = 0ull;
        cli_text_trimmed(field[4], cellul.len(&(CatenaFinitaCfg){.src = field[4], .cap = CLI_CELLS_POOL}), &from, &to);
        cli_text_trimmed(recent.out, cli_line_fits(&recent) ? recent.at : 0ull, &errors_from, &errors_to);
        (void)cli_line_growing(&logs, (to - from) + (errors_to - errors_from) + 4ull, "the logs a report takes");
        cli_line_counted(&logs, field[4] + from, to - from);
        cli_line_text(&logs, ((to > from) && (errors_to > errors_from)) ? "\n\n" : "");
        cli_line_counted(&logs, recent.out + errors_from, errors_to - errors_from);
        field[4] = (cli_line_closed(&logs) != 0ull) ? logs.out : field[4];
    }
    const CliReport given = {field[0], field[1], field[2], field[3], field[4]};
    char root[CLI_PATH_LONGEST];
    const int has_root = cli_serve_root_now(root);
    cli_report_file(&given, has_root ? root : NULL, &s_filed);
    report_filed_written(&s_filed, out);
    cli_lock_give(&s_lock);
    cli_cell_given(logs.out);
    cli_cell_given(recent.out);
    return 1;
}

// whether errors file on their own
static int report_auto(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_line_text(out, cli_report_auto() ? "true" : "false");
    return 1;
}

// errors filing on their own turned on or off
static int report_auto_set(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    int on = 0;
    return cli_serve_bool(args, "on", &on, said) && cli_report_set_auto(on, said);
}

// whether the question of whether errors file on their own needs no asking, as the reporter has answered it or
// ORIOR_NO_REPORTS turned reports off for the run, and the question
static int report_asked(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    char value[16];
    const int asked = cli_report_asked() || query_environment_read("ORIOR_NO_REPORTS", value, sizeof(value));
    cli_line_text(out, asked ? "[true," : "[false,");
    cli_window_quoted(out, CLI_REPORT_QUESTION);
    cli_line_character(out, ']');
    return 1;
}

// a page of orior's repository opened in the browser
static int report_open(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char *const url = cli_serve_string(args, "url", said);
    return (url != NULL) && cli_report_open_page(url, said);
}

const CliServeCall g_cli_window_report_calls[] = {
    {"report_error", report_error}, {"report_panic", report_panic},       {"report_bug", report_bug},
    {"report_auto", report_auto},   {"report_auto_set", report_auto_set}, {"report_asked", report_asked},
    {"report_open", report_open},
};
const unsigned int g_cli_window_report_count = sizeof(g_cli_window_report_calls) / sizeof(g_cli_window_report_calls[0]);
