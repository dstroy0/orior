// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_detached_run.c: a detached run run in a process of its own that outlives the server that started it: its job
// and its values read from its job file, every line it prints added to its output file as it comes, its start, each
// line and its end each a line of JSON, and the run stopped where its stop file is made. A run that cannot start
// reports why as a line of its errors and ends
#include "../runs/runner.h"
#include "serve_internal.h"

// how often the stop file is looked for, in milliseconds
#define CLI_SERVE_STOP_LOOKED 200u

// the detached run: its number, its files, and the run it started, under the lock its sink acquires
static unsigned long long s_run;
static char s_lines_file[CLI_PATH_MAX_LEN];
static char s_stop_file[CLI_PATH_MAX_LEN];
static unsigned long long s_inner;
static CliThread s_stopper;
static CliLock s_lock = CLI_LOCK_INIT;
static CliJson s_json;

void cli_serve_detached_file(const char *base, const char *kind, char *path)
{
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, base);
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, kind);
    (void)mmgr_line_finish(&line);
}

// `size` bytes of an output line appended to the output file as the line fills: 1, or 0 where they could not be
static int detached_run_put(void *context, const char *bytes, size_t size)
{
    (void)context;
    return cli_file_appended(s_lines_file, bytes, size, 0);
}

// what the run prints added to its output file as a line of its own, as the detached run's, handed to the file as it is
// written
static void detached_run_event(void *context, const CliRunEvent *event)
{
    (void)context;
    CliRunEvent copy = *event;
    copy.run = s_run;
    static const char *const s_kinds[3] = {"{\"start\":", "{\"line\":", "{\"end\":"};
    char piece[4096];
    cli_lock_take(&s_lock);
    VerbaLinea line = {piece, sizeof(piece), 0ull, detached_run_put, NULL, 0ull};
    mmgr_line_put(&line, s_kinds[copy.kind]);
    cli_run_event_written(&copy, &line);
    mmgr_line_put(&line, "}\n");
    (void)mmgr_line_flush(&line);
    cli_lock_give(&s_lock);
}

// why the run could not start given as a line of its errors and its end, the job's tree given back: the code to exit
// with
static int detached_run_failed(const char *why)
{
    cli_json_given(&s_json);
    CliRunEvent event = {CLI_RUN_EVENT_LINE, s_run, NULL, "stderr", why, 0, 0ll, 0, NULL, 0u, NULL, 0.0};
    detached_run_event(NULL, &event);
    event = (CliRunEvent){CLI_RUN_EVENT_END, s_run, NULL, NULL, NULL, 0, 0ll, 0, NULL, 0u, NULL, 0.0};
    detached_run_event(NULL, &event);
    return 1;
}

// the run stopped once its stop file is made
static void detached_run_stopper(void *context)
{
    (void)context;
    while (!query_is_file(s_stop_file))
    {
        query_paused(CLI_SERVE_STOP_LOOKED);
    }
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    (void)cli_runs_stop(s_inner, &ignored);
}

int cli_serve_detached_run(const char *base)
{
    const char *name = base;
    for (const char *at = base; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? at + 1 : name;
    }
    s_run = 0ull;
    int number = (name[0] != '\0');
    for (const char *at = name; number && (*at != '\0'); at += 1)
    {
        number = (*at >= '0') && (*at <= '9') && (s_run <= (~0ull - 9ull) / 10ull);
        s_run = number ? (s_run * 10ull) + (unsigned long long)(*at - '0') : s_run;
    }
    if (!number)
    {
        return 2;
    }
    cli_serve_detached_file(base, "output", s_lines_file);
    cli_serve_detached_file(base, "stop", s_stop_file);
    if (!cli_file_appended(s_lines_file, "", 0ull, 1))
    {
        return 2;
    }
    char pid_file[CLI_PATH_MAX_LEN];
    cli_serve_detached_file(base, "pid", pid_file);
    char pid[32];
    VerbaLinea pid_line = {pid, sizeof(pid), 0ull};
    mmgr_line_decimal(&pid_line, cli_process_own(), 1u);
    (void)query_file_written(pid_file, pid, pid_line.at);
    // the job read into a tree sized to it, retained while the run runs, its values the run's
    char job_file[CLI_PATH_MAX_LEN];
    cli_serve_detached_file(base, "job", job_file);
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int spec = cli_json_file_taken(&s_json, job_file, 0u, 0ull, &err_msg);
    if (spec == CLI_JSON_NONE)
    {
        (void)mmgr_line_finish(&err_msg);
        return detached_run_failed(why);
    }
    const char *const root = cli_serve_string_or(&(CliServeArgs){&s_json, spec}, "root", "");
    const char *const id = cli_serve_string_or(&(CliServeArgs){&s_json, spec}, "job", "");
    err_msg.at = 0ull;
    CliCatalog *const catalog = cli_serve_catalog_taken(root, &err_msg);
    if (catalog == NULL)
    {
        (void)mmgr_line_finish(&err_msg);
        return detached_run_failed(why);
    }
    const unsigned int found = cli_catalog_find(catalog, id);
    if (found == CLI_CATALOG_NONE)
    {
        cli_serve_catalog_given();
        err_msg.at = 0ull;
        mmgr_line_put(&err_msg, "no job ");
        mmgr_line_put(&err_msg, id);
        (void)mmgr_line_finish(&err_msg);
        return detached_run_failed(why);
    }
    const CliServeArgs args = {&s_json, spec};
    const CliRunSink sink = {detached_run_event, NULL};
    const int started = cli_runs_start(sink, root, catalog, found, cli_serve_values_of(&args), &s_inner, &err_msg);
    cli_serve_catalog_given();
    if (!started)
    {
        (void)mmgr_line_finish(&err_msg);
        return detached_run_failed(why);
    }
    s_stopper = (CliThread){detached_run_stopper, NULL};
    (void)cli_thread_started(&s_stopper);
    cli_runs_wait(s_inner);
    cli_json_given(&s_json);
    return 0;
}
