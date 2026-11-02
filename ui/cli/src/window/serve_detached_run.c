// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_detached_run.c: a detached run run in a process of its own that outlives the server that started it: its job
// and its values read from its job file, every line it prints added to its output file as it comes, its start, each
// line and its end each a line of JSON, and the run stopped where its stop file is made. A run that cannot start
// reports why as a line of its errors and ends
#include "../runs/runner.h"
#include "../text/text.h"
#include "serve_internal.h"

// how often the stop file is looked for, in milliseconds
#define CLI_SERVE_STOP_LOOKED 200u

// the detached run: its number, its files, and the run it started, under the lock its sink acquires
static unsigned long long s_run;
static char s_lines_file[CLI_PATH_MAX_LEN];
static char s_stop_file[CLI_PATH_MAX_LEN];
static unsigned long long s_inner;
static CliThread s_stopper;
static CliJson s_json;

void cli_serve_detached_file(const CliServeBaseCtx *args)
{
    VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->base);
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, args->kind);
    (void)mmgr_line_finish(&line);
}

// `size` bytes of an output line appended to the output file as the line fills: 1, or 0 where they could not be
static int detached_run_put(void *context, const char *bytes, size_t size)
{
    (void)context;
    return EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = s_lines_file, .bytes = bytes,
                      .size = size, .fresh = 0);
}

// what the run prints added to its output file as a line of its own, as the detached run's, handed to the file as it is
// written
static void detached_run_event(const CliEachEventCfg *args)
{
    const CliRunEvent *event = args->event;
    CliRunEvent copy = *event;
    copy.run = s_run;
    static const char *const s_kinds[3] = {"{\"start\":", "{\"line\":", "{\"end\":"};
    char piece[4096];
    VerbaLinea line = {piece, sizeof(piece), 0ull, detached_run_put, NULL, 0ull};
    mmgr_line_put(&line, s_kinds[copy.kind]);
    EMBED_CALL(cli_runner_event.run_event_write, CliRunnerEventCfg, .event = &copy, .out = &line);
    mmgr_line_put(&line, "}\n");
    (void)mmgr_line_flush(&line);
}

typedef struct
{
    const char *why;
} ServeDetachedRunCtx;
// why the run could not start given as a line of its errors and its end, the job's tree given back: the code to exit
// with
EMBED_INLINE int detached_run_fail(const ServeDetachedRunCtx *args)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    CliRunEvent event = {CLI_RUN_EVENT_LINE, s_run, NULL, "stderr", args->why, 0, 0ll, 0, NULL, 0u, NULL, 0.0};
    EMBED_CALL(detached_run_event, CliEachEventCfg, .context = NULL, .event = &event);
    event = (CliRunEvent){CLI_RUN_EVENT_END, s_run, NULL, NULL, NULL, 0, 0ll, 0, NULL, 0u, NULL, 0.0};
    EMBED_CALL(detached_run_event, CliEachEventCfg, .context = NULL, .event = &event);
    return 1;
}

// the run stopped once its stop file is made
static void detached_run_stopper(void *context)
{
    (void)context;
    while (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = s_stop_file))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = CLI_SERVE_STOP_LOOKED);
    }
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    (void)EMBED_CALL(cli_runner_err_msg_run_number.runs_stop, CliRunnerErrMsgRunNumberCfg, .run_number = s_inner,
                     .err_msg = &ignored);
}

int cli_serve_detached_run(const CliServeCfg *args)
{
    const char *name = args->base;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->base,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->base + name_past) : name;
    s_run = 0ull;
    int number = (name[0] != '\0');
    const unsigned long long digits = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    number = number && (digits <= 19ull) &&
             (EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = name, .at = 0ull,
                         .size = digits, .classes = CLI_TEXT_DIGITS, .set = "") == digits);
    s_run = number ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = name, .at = 0ull, .size = digits,
                                .end = NULL)
                   : s_run;
    if (!number)
    {
        return 2;
    }
    EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = args->base, .kind = "output",
               .path_out = s_lines_file);
    EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = args->base, .kind = "stop",
               .path_out = s_stop_file);
    if (!EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = s_lines_file, .bytes = "",
                    .size = 0ull, .fresh = 1))
    {
        return 2;
    }
    char pid_file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = args->base, .kind = "pid",
               .path_out = pid_file);
    char pid[32];
    VerbaLinea pid_line = {pid, sizeof(pid), 0ull};
    mmgr_line_decimal(&pid_line, cli_process_own(), 1u);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = pid_file, .bytes = pid,
                     .size = pid_line.at);
    // the job read into a tree sized to it, retained while the run runs, its values the run's
    char job_file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = args->base, .kind = "job",
               .path_out = job_file);
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int spec =
        EMBED_CALL(cli_json_tree_err_msg.json_file_take, CliJsonTreeErrMsgCfg, .json = &s_json, .path = job_file,
                   .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_window_cells);
    if (spec == CLI_JSON_NONE)
    {
        (void)mmgr_line_finish(&err_msg);
        return EMBED_CALL(detached_run_fail, ServeDetachedRunCtx, .why = why);
    }
    const char *const root = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                        .request = &(CliServeArgs){&s_json, spec}, .key = "root", .otherwise = "");
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                      .request = &(CliServeArgs){&s_json, spec}, .key = "job", .otherwise = "");
    err_msg.at = 0ull;
    CliCatalog *const catalog = EMBED_CALL(cli_serve_err_msg_root_internal.catalog_take, CliServeErrMsgRootCtx,
                                           .root_read = root, .err_msg = &err_msg);
    if (catalog == NULL)
    {
        (void)mmgr_line_finish(&err_msg);
        return EMBED_CALL(detached_run_fail, ServeDetachedRunCtx, .why = why);
    }
    const unsigned int found = EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = catalog, .id = id);
    if (found == CLI_CATALOG_NONE)
    {
        cli_serve_catalog_free();
        err_msg.at = 0ull;
        mmgr_line_put(&err_msg, "no job ");
        mmgr_line_put(&err_msg, id);
        (void)mmgr_line_finish(&err_msg);
        return EMBED_CALL(detached_run_fail, ServeDetachedRunCtx, .why = why);
    }
    const CliServeArgs arguments = {&s_json, spec};
    const CliRunSink sink = {detached_run_event, NULL};
    const int started = EMBED_CALL(
        cli_runner_err_msg.runs_start, CliRunnerErrMsgCfg, .sink = sink, .root = root, .catalog = catalog, .job = found,
        .values = EMBED_CALL(cli_serve_err_msg_out_internal.values_get, CliServeErrMsgOutCtx, .request = &arguments),
        .run_number_pointer = &s_inner, .err_msg = &err_msg);
    cli_serve_catalog_free();
    if (!started)
    {
        (void)mmgr_line_finish(&err_msg);
        return EMBED_CALL(detached_run_fail, ServeDetachedRunCtx, .why = why);
    }
    s_stopper = (CliThread){detached_run_stopper, NULL};
    (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_stopper);
    EMBED_CALL(cli_runner_err_msg_run_number.runs_wait, CliRunnerErrMsgRunNumberCfg, .run_number = s_inner);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    return 0;
}
