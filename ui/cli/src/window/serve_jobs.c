// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_jobs.c: the tree's jobs as the window lists them, each with how many steps it runs, which the run's fuse burns
// through one at a time; and a job started on this machine. The lines a run says are gathered and go to the page as
// one event every fifty milliseconds, so that a run saying thousands a second leaves the page's event queue open to
// every other event; the start goes before any line, so the page knows the run as its first line comes, and the end
// after the last of them
#include "../command_line/catalog.h"
#include "../runs/runner.h"
#include "serve_internal.h"

// the most jobs, params, steps, arguments, list items and bytes of text the catalog read for the page holds
#define CLI_SERVE_JOBS 4096u
#define CLI_SERVE_PARAMS 16384u
#define CLI_SERVE_STEPS 16384u
#define CLI_SERVE_ARGS 65536u
#define CLI_SERVE_LISTS 65536u
#define CLI_SERVE_CATALOG_TEXT (4u << 20u)
// the most runs started here whose lines are gathered at once, and the bytes of the lines each gathers
#define CLI_SERVE_GATHERING 64u
#define CLI_SERVE_GATHERED (1u << 20u)
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
// lines it said since they last went, as a JSON array's items
typedef struct
{
    unsigned int place;
    char lines[CLI_SERVE_GATHERED];
    unsigned long long at;
} CliServeGathering;

static CliServeGathering *s_gathering[CLI_SERVE_GATHERING];
static CliLock s_gathering_lock = CLI_LOCK_INIT;
static CliThread s_flusher;
static int s_flushing;
static char s_flushed[CLI_SERVE_GATHERED + 2u];

CliCatalog *cli_serve_catalog_taken(const char *root, CliLine *said)
{
    cli_lock_take(&s_catalog_lock);
    s_catalog = cli_serve_cell(sizeof(*s_catalog), "the tree's catalog", said);
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
    cli_cell_given(s_catalog);
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

// the lines of a gathering sent to the page as run-lines, where it holds any; under the gathering's lock
static void jobs_flushed(CliServeGathering *one)
{
    if (one->at == 0ull)
    {
        return;
    }
    s_flushed[0] = '[';
    memor.cpy(&(MemoriaCfg){.dst = s_flushed + 1u, .src = one->lines, .bytes = (size_t)one->at});
    s_flushed[one->at + 1u] = ']';
    cli_serve_emitted("run-lines", s_flushed, one->at + 2u);
    one->at = 0ull;
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

// what a run says, its gathering its context: its start sent at once, each line gathered, and its end sent after the
// lines gathered, its gathering given back
static void jobs_said(void *context, const CliSaid *said)
{
    CliServeGathering *const one = context;
    static char s_line[CLI_SERVE_GATHERED];
    cli_lock_take(&s_gathering_lock);
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_said_written(said, &line);
    if (said->kind == CLI_SAID_START)
    {
        cli_serve_emitted("run-start", s_line, line.at);
    }
    else if ((said->kind == CLI_SAID_LINE) && (one == NULL))
    {
        s_flushed[0] = '[';
        memor.cpy(&(MemoriaCfg){.dst = s_flushed + 1u, .src = s_line, .bytes = (size_t)line.at});
        s_flushed[line.at + 1u] = ']';
        cli_serve_emitted("run-lines", s_flushed, line.at + 2u);
    }
    else if (said->kind == CLI_SAID_LINE)
    {
        if (one->at + line.at + 1u > sizeof(one->lines))
        {
            jobs_flushed(one);
        }
        if (cli_line_fits(&line) && (one->at + line.at + 1u <= sizeof(one->lines)))
        {
            if (one->at > 0ull)
            {
                one->lines[one->at] = ',';
                one->at += 1u;
            }
            memor.cpy(&(MemoriaCfg){.dst = one->lines + one->at, .src = s_line, .bytes = (size_t)line.at});
            one->at += line.at;
        }
    }
    else
    {
        if (one != NULL)
        {
            jobs_flushed(one);
            s_gathering[one->place] = NULL;
            cli_cell_given(one);
        }
        cli_serve_emitted("run-end", s_line, line.at);
    }
    cli_lock_give(&s_gathering_lock);
}

// a run's gathering given back where its run did not start, NULL given back as nothing
static void jobs_gathering_given(CliServeGathering *one)
{
    if (one != NULL)
    {
        cli_lock_take(&s_gathering_lock);
        s_gathering[one->place] = NULL;
        cli_lock_give(&s_gathering_lock);
        cli_cell_given(one);
    }
}

int cli_serve_job_start(const char *job, const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_lock_take(&s_gathering_lock);
    unsigned int place = 0u;
    while ((place < CLI_SERVE_GATHERING) && (s_gathering[place] != NULL))
    {
        place += 1u;
    }
    // a run with no room left for its gathering has its lines go at once, as one past the places does
    CliServeGathering *const one =
        (place < CLI_SERVE_GATHERING) ? cli_cell_taken(sizeof(CliServeGathering), "a run's gathering") : NULL;
    if (one != NULL)
    {
        one->place = place;
        one->at = 0ull;
        s_gathering[place] = one;
    }
    const int started = s_flushing;
    s_flushing = 1;
    cli_lock_give(&s_gathering_lock);
    if (!started)
    {
        s_flusher = (CliThread){jobs_flusher, NULL};
        (void)cli_thread_started(&s_flusher);
    }
    // a place is free while fewer runs than the places say at once; the run's lines go at once where none is
    const CliRunSink sink = {jobs_said, one};
    CliCatalog *const catalog = cli_serve_catalog_taken(root, said);
    if (catalog == NULL)
    {
        jobs_gathering_given(one);
        return 0;
    }
    const unsigned int found = cli_catalog_find(catalog, job);
    unsigned long long run = 0ull;
    const int done = (found != CLI_CATALOG_NONE) &&
                     cli_runs_start(sink, root, catalog, found, cli_serve_values_of(args), &run, said);
    cli_serve_catalog_given();
    if (found == CLI_CATALOG_NONE)
    {
        cli_line_text(said, "no job ");
        cli_line_text(said, job);
    }
    if (!done)
    {
        jobs_gathering_given(one);
    }
    if (done)
    {
        cli_line_decimal(out, run, 1u);
    }
    return done;
}

int cli_serve_catalog_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    CliCatalog *const catalog = cli_serve_catalog_taken(root, said);
    if (catalog == NULL)
    {
        return 0;
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < catalog->jobs; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        const unsigned long long start = out->at;
        cli_catalog_job_written(catalog, at, out);
        if ((out->at > start) && (out->out[out->at - 1u] == '}'))
        {
            out->at -= 1u;
            cli_line_text(out, ",\"steps\":");
            cli_line_decimal(out, catalog->job[at].steps, 1u);
            cli_line_character(out, '}');
        }
    }
    cli_line_character(out, ']');
    cli_serve_catalog_given();
    return 1;
}
