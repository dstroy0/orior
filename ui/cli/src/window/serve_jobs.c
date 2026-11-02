// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_jobs.c: the tree's jobs as the window lists them, each with how many steps it runs, which the run's fuse burns
// through one at a time; and a job started on this machine. The lines a run prints are gathered and go to the page as
// one event every fifty milliseconds, so that a run printing thousands a second leaves the page's event queue open to
// every other event; the start goes before any line, so the page knows the run as its first line comes, and the end
// after the last of them
#include "../command_line/catalog.h"
#include "../runs/runner.h"
#include "serve_internal.h"

// A run's gathering, a cell taken as its run starts and given back as it ends: its place among the gatherings, and the
// lines it printed since they last went, a line containing a JSON array's opening and its items
typedef struct
{
    unsigned int place;
    VerbaLinea lines;
} CliServeGathering;
// the most jobs, params, steps, arguments, list items and bytes of text the catalog read for the page contains
#define CLI_SERVE_JOBS 1024u
#define CLI_SERVE_PARAMS 4096u
#define CLI_SERVE_STEPS 4096u
#define CLI_SERVE_ARGS 16384u
#define CLI_SERVE_LISTS 16384u
#define CLI_SERVE_CATALOG_TEXT (1u << 20u)
// the most runs started here whose lines are gathered at once; the bytes of the lines a gathering buffers before they
// go at once, and the bytes it is written in; and the bytes a line printed is written in
#define CLI_SERVE_GATHERING 64u
#define CLI_SERVE_GATHERED (1u << 20u)
#define CLI_SERVE_GATHERED_ROOM (CLI_SERVE_GATHERED + (1ull << 20u))
#define CLI_SERVE_EVENT_ROOM (1ull << 20u)
// how often the lines gathered go to the page, in milliseconds
#define CLI_SERVE_FLUSH 50u

// The tree's catalog and the values a job is started with, a cell taken with the catalog's lock and given back with it
typedef struct
{
    CliJob jobs[CLI_SERVE_JOBS];
    CliParam params[CLI_SERVE_PARAMS];
    CliStep steps[CLI_SERVE_STEPS];
    CliArg args[CLI_SERVE_ARGS];
    unsigned int lists[CLI_SERVE_LISTS];
    char text[CLI_SERVE_CATALOG_TEXT];
    CliCatalog catalog;
    CliValues values;
} CliServeCatalogRoom;

static CliServeCatalogRoom *s_catalog;
static CliLock s_catalog_lock = CLI_LOCK_INIT;

static CliServeGathering *s_gathering[CLI_SERVE_GATHERING];
static CliLock s_gathering_lock = CLI_LOCK_INIT;
static CliThread s_flusher;
static int s_flushing;

CliCatalog *cli_serve_catalog_take(const CliServeErrMsgRootCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_catalog_lock);
    // the catalog outlives the call that reads it: its cell is the shared cellblock's, never a lane's
    s_catalog = g_cli_cells->persistent_buf_alloc(sizeof(*s_catalog), &g_cli_serve_gathering_cells);
    if (s_catalog == NULL)
    {
        mmgr_line_put(args->err_msg, "the tree's catalog: the cells cannot meet it");
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_catalog_lock);
        return NULL;
    }
    CliServeCatalogRoom *const room = s_catalog;
    EMBED_CALL(cli_catalog_jobs_most.begin, CliCatalogJobsMostCfg, .catalog = &room->catalog, .jobs = room->jobs,
               .jobs_most = CLI_SERVE_JOBS, .params = room->params, .params_most = CLI_SERVE_PARAMS,
               .steps = room->steps, .steps_most = CLI_SERVE_STEPS, .request = room->args, .args_most = CLI_SERVE_ARGS,
               .lists = room->lists, .lists_most = CLI_SERVE_LISTS, .text_out = room->text,
               .text_most = CLI_SERVE_CATALOG_TEXT);
    EMBED_CALL(cli_catalog_root.read, CliCatalogRootCfg, .root = args->root_read, .catalog = &room->catalog);
    return &room->catalog;
}

void cli_serve_catalog_free(void)
{
    g_cli_cells->persistent_buf_release(s_catalog, &g_cli_serve_gathering_cells);
    s_catalog = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_catalog_lock);
}

CliValues *cli_serve_values_get(const CliServeErrMsgOutCtx *args)
{
    CliValues *const given = &s_catalog->values;
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = given);
    const unsigned int values =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = args->request, .key = "values");
    const CliJson *const json = args->request->json;
    for (unsigned int key = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = values, .kind = CFG_JSON_OBJECT)
                                ? json->node[values].first
                                : CLI_JSON_NONE;
         key != CLI_JSON_NONE; key = json->node[key].next)
    {
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = key, .kind = CFG_JSON_ARRAY)
                                    ? json->node[key].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const char *const value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                 .json_read = json, .node = one, .length_pointer = NULL);
            if (value != NULL)
            {
                EMBED_CALL(cli_catalog_key_values.values_put, CliCatalogKeyValuesCfg, .values_out = given,
                           .key = json->text + json->node[key].key, .value = value);
            }
        }
    }
    return given;
}

typedef struct
{
    CliServeGathering *one;
} ServeJobsCtx;
// the lines of a gathering sent to the page as run-lines, where it has any; under the gathering's lock
EMBED_INLINE void jobs_flush(const ServeJobsCtx *args)
{
    if (args->one->lines.at <= 1ull)
    {
        return;
    }
    mmgr_line_ch(&args->one->lines, ']');
    if (mmgr_line_ok(&args->one->lines))
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-lines", .body = args->one->lines.out,
                   .size = args->one->lines.at);
    }
    args->one->lines.at = 1ull;
}

// every gathering's lines sent to the page every fifty milliseconds
static void jobs_flusher(void *context)
{
    (void)context;
    for (;;)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = CLI_SERVE_FLUSH);
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_gathering_lock);
        for (unsigned int at = 0u; at < CLI_SERVE_GATHERING; at += 1u)
        {
            if (s_gathering[at] != NULL)
            {
                EMBED_CALL(jobs_flush, ServeJobsCtx, .one = s_gathering[at]);
            }
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_gathering_lock);
    }
}

// what a run prints, its gathering its context: its start sent at once, each line gathered, and its end sent after the
// lines gathered, its gathering given back
static void jobs_run_event(const CliEachEventCfg *args)
{
    const CliRunEvent *event = args->event;
    CliServeGathering *const one = args->context;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_gathering_lock);
    if ((event->kind == CLI_RUN_EVENT_LINE) && (one != NULL))
    {
        if (one->lines.at > CLI_SERVE_GATHERED)
        {
            EMBED_CALL(jobs_flush, ServeJobsCtx, .one = one);
        }
        // a line whose room cannot be met is left out, the lines before it retained
        const unsigned long long mark = one->lines.at;
        mmgr_line_put(&one->lines, (mark > 1ull) ? "," : "");
        EMBED_CALL(cli_runner_event.run_event_write, CliRunnerEventCfg, .event = event, .out = &one->lines);
        one->lines.at = mmgr_line_ok(&one->lines) ? one->lines.at : mark;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_gathering_lock);
        return;
    }
    // the line printed written after an array's opening, which a line printed where it has no gathering goes in alone
    char *const line_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_EVENT_ROOM, &g_cli_serve_gathering_cells);
    VerbaLinea line = mmgr_line_over(line_cell, CLI_SERVE_EVENT_ROOM);
    mmgr_line_ch(&line, '[');
    EMBED_CALL(cli_runner_event.run_event_write, CliRunnerEventCfg, .event = event, .out = &line);
    if (event->kind == CLI_RUN_EVENT_LINE)
    {
        mmgr_line_ch(&line, ']');
    }
    const int fits = mmgr_line_ok(&line);
    if ((event->kind == CLI_RUN_EVENT_START) && fits)
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-start", .body = line.out + 1u,
                   .size = line.at - 1u);
    }
    else if ((event->kind == CLI_RUN_EVENT_LINE) && fits)
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-lines", .body = line.out,
                   .size = line.at);
    }
    else if ((event->kind != CLI_RUN_EVENT_START) && (event->kind != CLI_RUN_EVENT_LINE))
    {
        if (one != NULL)
        {
            EMBED_CALL(jobs_flush, ServeJobsCtx, .one = one);
            s_gathering[one->place] = NULL;
            g_cli_cells->persistent_buf_release(one->lines.out, &g_cli_serve_gathering_cells);
            g_cli_cells->persistent_buf_release(one, &g_cli_serve_gathering_cells);
        }
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-end",
                   .body = fits ? line.out + 1u : "", .size = fits ? line.at - 1u : 0ull);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_gathering_lock);
    g_cli_cells->persistent_buf_release(line.out, &g_cli_serve_gathering_cells);
}

// a run's gathering given back where its run did not start, NULL given back as nothing
EMBED_INLINE void jobs_gathering_free(const ServeJobsCtx *args)
{
    if (args->one != NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_gathering_lock);
        s_gathering[args->one->place] = NULL;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_gathering_lock);
        g_cli_cells->persistent_buf_release(args->one->lines.out, &g_cli_serve_gathering_cells);
        g_cli_cells->persistent_buf_release(args->one, &g_cli_serve_gathering_cells);
    }
}

int cli_serve_job_start(const CliServeErrMsgOutCtx *args)
{
    const char *job = args->job;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_gathering_lock);
    unsigned int place = 0u;
    while ((place < CLI_SERVE_GATHERING) && (s_gathering[place] != NULL))
    {
        place += 1u;
    }
    // a run with no room left for its gathering, or whose lines' cell is refused, has its lines go at once, as one past
    // the places does
    CliServeGathering *one =
        (place < CLI_SERVE_GATHERING)
            ? g_cli_cells->persistent_buf_alloc(sizeof(CliServeGathering), &g_cli_serve_gathering_cells)
            : NULL;
    if (one != NULL)
    {
        one->place = place;
        char *const lines_cell =
            g_cli_cells->temporary_buf_alloc(CLI_SERVE_GATHERED_ROOM, &g_cli_serve_gathering_cells);
        one->lines = mmgr_line_over(lines_cell, CLI_SERVE_GATHERED_ROOM);
        if (lines_cell != NULL)
        {
            mmgr_line_ch(&one->lines, '[');
            s_gathering[place] = one;
        }
        else
        {
            g_cli_cells->persistent_buf_release(one, &g_cli_serve_gathering_cells);
            one = NULL;
        }
    }
    const int started = s_flushing;
    s_flushing = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_gathering_lock);
    if (!started)
    {
        s_flusher = (CliThread){jobs_flusher, NULL};
        (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_flusher);
    }
    // a place is free while fewer runs than the places allow at once; the run's lines go at once where none is
    const CliRunSink sink = {jobs_run_event, one};
    CliCatalog *const catalog = EMBED_CALL(cli_serve_err_msg_root_internal.catalog_take, CliServeErrMsgRootCtx,
                                           .root_read = root, .err_msg = err_msg);
    if (catalog == NULL)
    {
        EMBED_CALL(jobs_gathering_free, ServeJobsCtx, .one = one);
        return 0;
    }
    const unsigned int found = EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = catalog, .id = job);
    unsigned long long run = 0ull;
    const int done =
        (found != CLI_CATALOG_NONE) &&
        EMBED_CALL(cli_runner_err_msg.runs_start, CliRunnerErrMsgCfg, .sink = sink, .root = root, .catalog = catalog,
                   .job = found, .values = cli_serve_err_msg_out_internal.values_get(args), .run_number_pointer = &run,
                   .err_msg = err_msg);
    cli_serve_catalog_free();
    if (found == CLI_CATALOG_NONE)
    {
        mmgr_line_put(err_msg, "no job ");
        mmgr_line_put(err_msg, job);
    }
    if (!done)
    {
        EMBED_CALL(jobs_gathering_free, ServeJobsCtx, .one = one);
    }
    if (done)
    {
        mmgr_line_decimal(args->out, run, 1u);
    }
    return done;
}

int cli_serve_catalog_read(const CliServeErrMsgOutRequestCtx *args)
{
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    CliCatalog *const catalog = EMBED_CALL(cli_serve_err_msg_root_internal.catalog_take, CliServeErrMsgRootCtx,
                                           .root_read = root, .err_msg = args->err_msg);
    if (catalog == NULL)
    {
        return 0;
    }
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < catalog->jobs; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        const unsigned long long start = args->out->at;
        EMBED_CALL(cli_catalog_index.job_write, CliCatalogIndexCfg, .catalog_read = catalog, .index = at,
                   .out = args->out);
        if ((args->out->at > start) && (args->out->out[args->out->at - 1u] == '}'))
        {
            args->out->at -= 1u;
            mmgr_line_put(args->out, ",\"steps\":");
            mmgr_line_decimal(args->out, catalog->job[at].steps, 1u);
            mmgr_line_ch(args->out, '}');
        }
    }
    mmgr_line_ch(args->out, ']');
    cli_serve_catalog_free();
    return 1;
}
