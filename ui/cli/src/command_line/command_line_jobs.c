// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_jobs.c: the tree's jobs at the command line: listed, each about written to the terminal's width and
// whole anywhere else; shown, its values and the line each step runs; run here, its steps sharing the terminal, its
// output streaming as it comes, and reported to the window where a terminal of the window's started this, on a channel
// of its own under runs; and the tree's tests listed, a Python test as pytest names it, each with its line, or as JSON
#include "../comms/door.h"
#include "../runs/runner.h"
#include "../tests/testing.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "cells.h"
#include "command_line_internal.h"

// a test written after those written
static void jobs_test_written(void *context, const CliTest *test)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->at > 1ull) ? "," : "");
    cli_test_written(test, out);
}

// a test reported as its id and its line
static void jobs_test_printed(void *context, const CliTest *test)
{
    unsigned int *const count = context;
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea out = {line, sizeof(line), 0ull};
    mmgr_line_put(&out, test->id);
    mmgr_line_put(&out, "  line ");
    mmgr_line_decimal(&out, test->line + 1u, 1u);
    cli_out_line(line, mmgr_line_ok(&out) ? out.at : 0ull);
    *count += 1u;
}

int cli_tests_list(const char *root, const char *const *words, unsigned int count)
{
    int json = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        json |= (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--json", .cap = CLI_CELLS_POOL}));
    }
    if (json)
    {
        // the tests found, their tree and the tree written again, a cell taken for the listing
        typedef struct
        {
            char found[1u << 22u];
            char pretty[1u << 23u];
            CliJsonNode node[1u << 17u];
            char text[1u << 22u];
            CfgJsonToken token[1u << 17u];
            CliJson json;
        } CliTestsListRoom;
        CliTestsListRoom *const room = g_cli_cells->temporary_buf_alloc(sizeof(*room), &g_cli_command_line_cells);
        if (room == NULL)
        {
            cli_err("a listing of tests could not be taken");
            return CLI_NO_CODE;
        }
        VerbaLinea found = {room->found, sizeof(room->found), 0ull};
        mmgr_line_ch(&found, '[');
        cli_tests_found(root, jobs_test_written, &found);
        mmgr_line_ch(&found, ']');
        cli_json_begun(&room->json, room->node, sizeof(room->node) / sizeof(room->node[0]), room->text,
                       sizeof(room->text));
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        const unsigned int tree = cli_json_read(&room->json, room->found, found.at, room->token,
                                                sizeof(room->token) / sizeof(room->token[0]), &ignored);
        VerbaLinea pretty = {room->pretty, sizeof(room->pretty), 0ull};
        if (tree != CLI_JSON_NONE)
        {
            cli_json_write(&room->json, tree, &pretty, 2u, 0);
        }
        cli_out_line(room->pretty, mmgr_line_ok(&pretty) ? pretty.at : 0ull);
        g_cli_cells->persistent_buf_release(room, &g_cli_command_line_cells);
        return 0;
    }
    unsigned int listed = 0u;
    cli_tests_found(root, jobs_test_printed, &listed);
    if (listed == 0u)
    {
        cli_err("the tree contains no test that pytest or unittest finds");
        return CLI_NO_CODE;
    }
    return 0;
}

int cli_jobs_list(const char *root, const char *word)
{
    const CliCatalog *const catalog = cli_command_line_catalog(root);
    static unsigned int s_listed[CLI_LINE_JOBS];
    unsigned int count = 0u;
    unsigned long long pad = 0ull;
    for (unsigned int at = 0u; (at < catalog->jobs) && (count < CLI_LINE_JOBS); at += 1u)
    {
        const CliJob *const job = &catalog->job[at];
        const char *const id = cli_catalog_at(catalog, job->id);
        if ((word != NULL) && !cli_contains_word(id, word) &&
            !cli_contains_word(cli_catalog_at(catalog, job->title), word) &&
            !cli_contains_word(cli_catalog_at(catalog, job->about), word))
        {
            continue;
        }
        s_listed[count] = at;
        count += 1u;
        const unsigned long long letters =
            cli_text_letters(id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
        pad = (letters > pad) ? letters : pad;
    }
    if (count == 0u)
    {
        cli_err("no job contains that word");
        return CLI_NO_CODE;
    }
    pad = (pad < 48ull) ? pad : 48ull;
    const unsigned long long width = cli_width();
    const unsigned long long room = (width > pad + 2ull) ? width - (pad + 2ull) : 0ull;
    // written to a terminal, each about is cut to the line; written anywhere else, it is whole
    const int terminal = cli_output_terminal();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliJob *const job = &catalog->job[s_listed[at]];
        const char *const id = cli_catalog_at(catalog, job->id);
        const char *const about = cli_catalog_at(catalog, job->about);
        static char s_line[1u << 16u];
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&line, id);
        for (unsigned long long letters =
                 cli_text_letters(id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
             letters < pad; letters += 1ull)
        {
            mmgr_line_ch(&line, ' ');
        }
        mmgr_line_put(&line, "  ");
        if (!terminal)
        {
            mmgr_line_put(&line, about);
        }
        else if (room >= 20ull)
        {
            cli_cut(about, cellul.len(&(CatenaFinitaCfg){.src = about, .cap = CLI_CELLS_POOL}), room, &line);
        }
        const unsigned long long size = mmgr_line_ok(&line) ? line.at : 0ull;
        cli_out_line(s_line, cli_text_trimmed_end(s_line, size));
    }
    return 0;
}

// a line of a shown job written
static void jobs_line_out(void *context, const char *line, unsigned long long size)
{
    (void)context;
    cli_out_line(line, size);
}

// a step's line written as the job's runs show it
static void jobs_shown(void *context, const char *shown)
{
    (void)context;
    char line[1u << 16u];
    VerbaLinea out = {line, sizeof(line), 0ull};
    mmgr_line_put(&out, "  $ ");
    mmgr_line_put(&out, shown);
    cli_out_line(line, mmgr_line_ok(&out) ? out.at : 0ull);
}

void cli_job_show(const char *root, const CliCatalog *catalog, unsigned int job)
{
    const CliJob *const one = &catalog->job[job];
    const unsigned int most = cli_width();
    static char s_line[1u << 16u];
    cli_out(cli_catalog_at(catalog, one->id));
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "  title  ");
    mmgr_line_put(&line, cli_catalog_at(catalog, one->title));
    cli_out_line(s_line, line.at);
    line.at = 0ull;
    mmgr_line_put(&line, "  file   ");
    mmgr_line_put(&line, cli_catalog_at(catalog, one->file));
    cli_out_line(s_line, line.at);
    const char *const about = cli_catalog_at(catalog, one->about);
    if (about[0] != '\0')
    {
        cli_out("");
        cli_wrap(about, cellul.len(&(CatenaFinitaCfg){.src = about, .cap = CLI_CELLS_POOL}), "  ", most, jobs_line_out,
                 NULL);
    }
    if (one->params > 0u)
    {
        cli_out("");
        cli_out("values");
        for (unsigned int at = 0u; at < one->params; at += 1u)
        {
            const CliParam *const param = &catalog->param[one->param_first + at];
            const char *const fallback = cli_catalog_at(catalog, param->fallback);
            line.at = 0ull;
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, cli_catalog_at(catalog, param->key));
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, param->kind);
            mmgr_line_put(&line, param->required ? "  required" : "");
            if (fallback[0] != '\0')
            {
                mmgr_line_put(&line, "  default ");
                mmgr_line_put(&line, fallback);
            }
            cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
            if (param->choices > 0u)
            {
                static char s_choices[1u << 16u];
                VerbaLinea choices = {s_choices, sizeof(s_choices), 0ull};
                for (unsigned int choice = 0u; choice < param->choices; choice += 1u)
                {
                    mmgr_line_put(&choices, (choice > 0u) ? ", " : "");
                    mmgr_line_put(&choices, cli_catalog_at(catalog, catalog->list[param->first + choice]));
                }
                cli_wrap(s_choices, mmgr_line_ok(&choices) ? choices.at : 0ull, "      ", most, jobs_line_out, NULL);
            }
        }
    }
    cli_out("");
    cli_out("runs");
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliValues *const values = g_cli_cells->temporary_buf_alloc(sizeof(*values), &g_cli_command_line_cells);
    if (values == NULL)
    {
        mmgr_line_put(&err_msg, "the cells cannot meet the values a job is shown with");
    }
    const int shown = (values != NULL) && cli_job_values(catalog, job, NULL, 0u, values, &err_msg) &&
                      cli_runner_shown(root, catalog, job, values, jobs_shown, NULL, &err_msg);
    g_cli_cells->persistent_buf_release(values, &g_cli_command_line_cells);
    if (!shown)
    {
        line.at = 0ull;
        mmgr_line_put(&line, "  ");
        mmgr_line_put_n(&line, why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
    }
}

// A run reported to the window as it goes, on a channel of its own under runs, where a terminal of the window's started
// this process; elsewhere reported to no one; and the code its end gave
typedef struct
{
    CliDoorClient *window;
    char channel[64];
    int has_code;
    long long code;
    int ended;
} CliLineRun;

static CliLineRun s_run;

// the bytes what a run sends the window is begun to be written in, doubling as it fills
#define CLI_JOBS_STATE_INIT_CAP 1024u

void cli_window_channel_begun(void)
{
    s_run.window = cli_door_from_environment();
    VerbaLinea channel = {s_run.channel, sizeof(s_run.channel), 0ull};
    mmgr_line_put(&channel, "runs/");
    mmgr_line_decimal(&channel, cli_process_own(), 1u);
    (void)mmgr_line_finish(&channel);
}

void cli_window_channel_sent(const char *body, unsigned long long size)
{
    if (s_run.window != NULL)
    {
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        unsigned long long reply_size = 0ull;
        (void)cli_door_send(s_run.window, s_run.channel, body, size, &reply_size, &ignored);
    }
}

// `state`, a growing line, sent to the window where it fits, and given back
static void jobs_state_sent(VerbaLinea *state)
{
    if (mmgr_line_ok(state))
    {
        cli_window_channel_sent(state->out, state->at);
    }
    g_cli_cells->persistent_buf_release(state->out, &g_cli_command_line_cells);
}

// a line of the errors led by `lead`
static void jobs_err_led(const char *lead, const char *text)
{
    static char s_line[1u << 16u];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, lead);
    mmgr_line_put(&line, text);
    cli_err_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
}

// what a run prints: each line to the terminal and the window, and its end, the pages it wrote and the program it made
static void jobs_run_event(void *context, const CliRunEvent *event)
{
    (void)context;
    // a blob the run's writer placed is the panel's, and the command line shows the console alone
    if (event->kind == CLI_RUN_EVENT_START)
    {
        return;
    }
    VerbaLinea state = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&state, CLI_JOBS_STATE_INIT_CAP, &g_cli_command_line_cells);
    if (event->kind == CLI_RUN_EVENT_LINE)
    {
        mmgr_line_put(&state, "{\"line\":");
        cli_json_quoted_lossy(&state, (const unsigned char *)event->text,
                              cellul.len(&(CatenaFinitaCfg){.src = event->text, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(&state, ",\"stream\":");
        cli_json_quoted(&state, event->stream,
                        cellul.len(&(CatenaFinitaCfg){.src = event->stream, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(&state, ",\"ms\":");
        cli_json_double(&state, event->ms);
        mmgr_line_ch(&state, '}');
        jobs_state_sent(&state);
        if (cellul.eq(&(CatenaFinitaCfg){.src = event->stream, .other = "stdout", .cap = CLI_CELLS_POOL}))
        {
            cli_out(event->text);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = event->stream, .other = "command", .cap = CLI_CELLS_POOL}))
        {
            jobs_err_led("$ ", event->text);
        }
        else
        {
            cli_err(event->text);
        }
        return;
    }
    mmgr_line_put(&state, "{\"state\":\"ended\",\"code\":");
    if (event->has_code)
    {
        mmgr_line_decimal_signed(&state, event->code);
    }
    else
    {
        mmgr_line_put(&state, "null");
    }
    mmgr_line_put(&state, event->stopped ? ",\"stopped\":true,\"ms\":" : ",\"stopped\":false,\"ms\":");
    cli_json_double(&state, event->ms);
    mmgr_line_ch(&state, '}');
    jobs_state_sent(&state);
    for (unsigned int at = 0u; at < event->view_count; at += 1u)
    {
        jobs_err_led("page ", event->views[at]);
    }
    if (event->made != NULL)
    {
        jobs_err_led("made ", event->made);
    }
    s_run.has_code = event->has_code;
    s_run.code = event->code;
    s_run.ended = 1;
}

int cli_job_run(const char *root, const CliCatalog *catalog, unsigned int job, const char *const *words,
                unsigned int count)
{
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliValues *const values = g_cli_cells->temporary_buf_alloc(sizeof(*values), &g_cli_command_line_cells);
    if ((values == NULL) || !cli_job_values(catalog, job, words, count, values, &err_msg))
    {
        mmgr_line_put(&err_msg, (values == NULL) ? "the cells cannot meet the values a job is run with" : "");
        cli_err_line(why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        g_cli_cells->persistent_buf_release(values, &g_cli_command_line_cells);
        return CLI_WRONG;
    }
    cli_window_channel_begun();
    s_run.ended = 0;
    s_run.has_code = 0;
    VerbaLinea state = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&state, CLI_JOBS_STATE_INIT_CAP, &g_cli_command_line_cells);
    mmgr_line_put(&state, "{\"state\":\"started\",\"job\":");
    const char *const id = cli_catalog_at(catalog, catalog->job[job].id);
    const char *const title = cli_catalog_at(catalog, catalog->job[job].title);
    cli_json_quoted(&state, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"title\":");
    cli_json_quoted(&state, title, cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"root\":");
    cli_json_quoted_lossy(&state, (const unsigned char *)root,
                          cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"words\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&state, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(&state, (const unsigned char *)words[at],
                              cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(&state, "]}");
    jobs_state_sent(&state);
    cli_runs_attached();
    const CliRunSink sink = {jobs_run_event, NULL};
    unsigned long long run = 0ull;
    err_msg.at = 0ull;
    const int started = cli_runs_start(sink, root, catalog, job, values, &run, &err_msg);
    g_cli_cells->persistent_buf_release(values, &g_cli_command_line_cells);
    if (!started)
    {
        cli_err_line(why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_WRONG;
    }
    cli_runs_wait(run);
    // the code is cut to the system's exit code
    return (s_run.ended && s_run.has_code) ? (int)s_run.code : CLI_NO_CODE;
}
