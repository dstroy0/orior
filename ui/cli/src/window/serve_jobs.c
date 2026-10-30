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

// the most jobs, params, steps, arguments, list items and bytes of text the catalog read for the page contains
#define CLI_SERVE_JOBS 4096u
#define CLI_SERVE_PARAMS 16384u
#define CLI_SERVE_STEPS 16384u
#define CLI_SERVE_ARGS 65536u
#define CLI_SERVE_LISTS 65536u
#define CLI_SERVE_CATALOG_TEXT (4u << 20u)
// the most runs started here whose lines are gathered at once; the bytes of the lines a gathering buffers before they
// go at once, and the bytes it is begun in, doubling as it fills; and the bytes a line printed is begun to be written
// in
#define CLI_SERVE_GATHERING 64u
#define CLI_SERVE_GATHERED (1u << 20u)
#define CLI_SERVE_GATHERED_FIRST 4096u
#define CLI_SERVE_EVENT_INIT_CAP 1024u
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

// A run's gathering, a cell taken as its run starts and given back as it ends: its place among the gatherings, and the
// lines it printed since they last went, a growing line containing a JSON array's opening and its items
typedef struct
{
    unsigned int place;
    VerbaLinea lines;
} CliServeGathering;

CellOwner g_cli_serve_gathering_cells = MMGR_CELL_OWNER("cli_serve_gathering");
static CliServeGathering *s_gathering[CLI_SERVE_GATHERING];
static CliLock s_gathering_lock = CLI_LOCK_INIT;
static CliThread s_flusher;
static int s_flushing;

CliCatalog *cli_serve_catalog_taken(const char *root, VerbaLinea *err_msg)
{
    cli_lock_take(&s_catalog_lock);
    s_catalog = cli_serve_cell(sizeof(*s_catalog), "the tree's catalog", err_msg);
    if (s_catalog == NULL)
    {
        cli_lock_give(&s_catalog_lock);
        return NULL;
    }
    CliServeCatalogRoom *const room = s_catalog;
    cli_catalog_begun(&room->catalog, room->jobs, CLI_SERVE_JOBS, room->params, CLI_SERVE_PARAMS, room->steps,
                      CLI_SERVE_STEPS, room->args, CLI_SERVE_ARGS, room->lists, CLI_SERVE_LISTS, room->text,
                      CLI_SERVE_CATALOG_TEXT);
    cli_catalog_read(root, &room->catalog);
    return &room->catalog;
}

void cli_serve_catalog_given(void)
{
    cli_cell_release(s_catalog, NULL);
    s_catalog = NULL;
    cli_lock_give(&s_catalog_lock);
}

CliValues *cli_serve_values_of(const CliServeArgs *args)
{
    CliValues *const given = &s_catalog->values;
    cli_values_begun(given);
    const unsigned int values = cli_serve_arg(args, "values");
    const CliJson *const json = args->json;
    for (unsigned int key = cli_json_is(json, values, CFG_JSON_OBJECT) ? json->node[values].first : CLI_JSON_NONE;
         key != CLI_JSON_NONE; key = json->node[key].next)
    {
        for (unsigned int one = cli_json_is(json, key, CFG_JSON_ARRAY) ? json->node[key].first : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const char *const value = cli_json_string(json, one, NULL);
            if (value != NULL)
            {
                cli_values_put(given, json->text + json->node[key].key, value);
            }
        }
    }
    return given;
}

// the lines of a gathering sent to the page as run-lines, where it has any; under the gathering's lock
static void jobs_flushed(CliServeGathering *one)
{
    if (one->lines.at <= 1ull)
    {
        return;
    }
    mmgr_line_ch(&one->lines, ']');
    if (mmgr_line_ok(&one->lines))
    {
        cli_serve_emitted("run-lines", one->lines.out, one->lines.at);
    }
    one->lines.at = 1ull;
}

// every gathering's lines sent to the page every fifty milliseconds
static void jobs_flusher(void *context)
{
    (void)context;
    for (;;)
    {
        query_paused(CLI_SERVE_FLUSH);
        cli_lock_take(&s_gathering_lock);
        for (unsigned int at = 0u; at < CLI_SERVE_GATHERING; at += 1u)
        {
            if (s_gathering[at] != NULL)
            {
                jobs_flushed(s_gathering[at]);
            }
        }
        cli_lock_give(&s_gathering_lock);
    }
}

// what a run prints, its gathering its context: its start sent at once, each line gathered, and its end sent after the
// lines gathered, its gathering given back
static void jobs_run_event(void *context, const CliRunEvent *event)
{
    CliServeGathering *const one = context;
    cli_lock_take(&s_gathering_lock);
    if ((event->kind == CLI_RUN_EVENT_LINE) && (one != NULL))
    {
        if (one->lines.at > CLI_SERVE_GATHERED)
        {
            jobs_flushed(one);
        }
        // a line whose room cannot be met is left out, the lines before it retained
        const unsigned long long mark = one->lines.at;
        mmgr_line_put(&one->lines, (mark > 1ull) ? "," : "");
        cli_run_event_written(event, &one->lines);
        one->lines.at = mmgr_line_ok(&one->lines) ? one->lines.at : mark;
        cli_lock_give(&s_gathering_lock);
        return;
    }
    // the line printed written after an array's opening, which a line printed where it has no gathering goes in alone
    VerbaLinea line = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&line, CLI_SERVE_EVENT_INIT_CAP);
    mmgr_line_ch(&line, '[');
    cli_run_event_written(event, &line);
    if (event->kind == CLI_RUN_EVENT_LINE)
    {
        mmgr_line_ch(&line, ']');
    }
    const int fits = mmgr_line_ok(&line);
    if ((event->kind == CLI_RUN_EVENT_START) && fits)
    {
        cli_serve_emitted("run-start", line.out + 1u, line.at - 1u);
    }
    else if ((event->kind == CLI_RUN_EVENT_LINE) && fits)
    {
        cli_serve_emitted("run-lines", line.out, line.at);
    }
    else if ((event->kind != CLI_RUN_EVENT_START) && (event->kind != CLI_RUN_EVENT_LINE))
    {
        if (one != NULL)
        {
            jobs_flushed(one);
            s_gathering[one->place] = NULL;
            cli_cell_release(one->lines.out, NULL);
            cli_cell_release(one, &g_cli_serve_gathering_cells);
        }
        cli_serve_emitted("run-end", fits ? line.out + 1u : "", fits ? line.at - 1u : 0ull);
    }
    cli_lock_give(&s_gathering_lock);
    cli_cell_release(line.out, NULL);
}

// a run's gathering given back where its run did not start, NULL given back as nothing
static void jobs_gathering_given(CliServeGathering *one)
{
    if (one != NULL)
    {
        cli_lock_take(&s_gathering_lock);
        s_gathering[one->place] = NULL;
        cli_lock_give(&s_gathering_lock);
        cli_cell_release(one->lines.out, NULL);
        cli_cell_release(one, &g_cli_serve_gathering_cells);
    }
}

int cli_serve_job_start(const char *job, const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_gathering_lock);
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
        one->lines = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
        if (cli_growing_line_made(&one->lines, CLI_SERVE_GATHERED_FIRST))
        {
            mmgr_line_ch(&one->lines, '[');
            s_gathering[place] = one;
        }
        else
        {
            cli_cell_release(one, &g_cli_serve_gathering_cells);
            one = NULL;
        }
    }
    const int started = s_flushing;
    s_flushing = 1;
    cli_lock_give(&s_gathering_lock);
    if (!started)
    {
        s_flusher = (CliThread){jobs_flusher, NULL};
        (void)cli_thread_started(&s_flusher);
    }
    // a place is free while fewer runs than the places allow at once; the run's lines go at once where none is
    const CliRunSink sink = {jobs_run_event, one};
    CliCatalog *const catalog = cli_serve_catalog_taken(root, err_msg);
    if (catalog == NULL)
    {
        jobs_gathering_given(one);
        return 0;
    }
    const unsigned int found = cli_catalog_find(catalog, job);
    unsigned long long run = 0ull;
    const int done = (found != CLI_CATALOG_NONE) &&
                     cli_runs_start(sink, root, catalog, found, cli_serve_values_of(args), &run, err_msg);
    cli_serve_catalog_given();
    if (found == CLI_CATALOG_NONE)
    {
        mmgr_line_put(err_msg, "no job ");
        mmgr_line_put(err_msg, job);
    }
    if (!done)
    {
        jobs_gathering_given(one);
    }
    if (done)
    {
        mmgr_line_decimal(out, run, 1u);
    }
    return done;
}

int cli_serve_catalog_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    CliCatalog *const catalog = cli_serve_catalog_taken(root, err_msg);
    if (catalog == NULL)
    {
        return 0;
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < catalog->jobs; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        const unsigned long long start = out->at;
        cli_catalog_job_written(catalog, at, out);
        if ((out->at > start) && (out->out[out->at - 1u] == '}'))
        {
            out->at -= 1u;
            mmgr_line_put(out, ",\"steps\":");
            mmgr_line_decimal(out, catalog->job[at].steps, 1u);
            mmgr_line_ch(out, '}');
        }
    }
    mmgr_line_ch(out, ']');
    cli_serve_catalog_given();
    return 1;
}
