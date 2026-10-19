// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_jobs.c: the tree's jobs at the command line: listed, each about written to the terminal's width and
// whole anywhere else; shown, its values and the line each step runs; run here, its steps sharing the terminal, its
// output streaming as it comes, and told to the window where a terminal of the window's started this, on a channel of
// its own under runs; and the tree's tests listed, a Python test as pytest names it, each with its line, or as JSON
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
    CliLine *const out = context;
    cli_line_text(out, (out->at > 1ull) ? "," : "");
    cli_test_written(test, out);
}

// a test said as its id and its line
static void jobs_test_said(void *context, const CliTest *test)
{
    unsigned int *const count = context;
    char line[CLI_PATH_LONGEST * 2u];
    CliLine said = {line, sizeof(line), 0ull};
    cli_line_text(&said, test->id);
    cli_line_text(&said, "  line ");
    cli_line_decimal(&said, test->line + 1u, 1u);
    cli_out_line(line, cli_line_fits(&said) ? said.at : 0ull);
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
        CliTestsListRoom *const room = cli_cell_taken(sizeof(*room), "a listing of tests");
        if (room == NULL)
        {
            cli_err("a listing of tests could not be taken");
            return CLI_NO_CODE;
        }
        CliLine found = {room->found, sizeof(room->found), 0ull};
        cli_line_character(&found, '[');
        cli_tests_found(root, jobs_test_written, &found);
        cli_line_character(&found, ']');
        cli_json_begun(&room->json, room->node, sizeof(room->node) / sizeof(room->node[0]), room->text,
                       sizeof(room->text));
        char quiet[256];
        CliLine ignored = {quiet, sizeof(quiet), 0ull};
        const unsigned int tree = cli_json_read(&room->json, room->found, found.at, room->token,
                                                sizeof(room->token) / sizeof(room->token[0]), &ignored);
        CliLine pretty = {room->pretty, sizeof(room->pretty), 0ull};
        if (tree != CLI_JSON_NONE)
        {
            cli_json_write(&room->json, tree, &pretty, 2u, 0);
        }
        cli_out_line(room->pretty, cli_line_fits(&pretty) ? pretty.at : 0ull);
        cli_cell_given(room);
        return 0;
    }
    unsigned int said = 0u;
    cli_tests_found(root, jobs_test_said, &said);
    if (said == 0u)
    {
        cli_err("the tree holds no test that pytest or unittest finds");
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
        if ((word != NULL) && !cli_holds_word(id, word) && !cli_holds_word(cli_catalog_at(catalog, job->title), word) &&
            !cli_holds_word(cli_catalog_at(catalog, job->about), word))
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
        cli_err("no job holds that word");
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
        CliLine line = {s_line, sizeof(s_line), 0ull};
        cli_line_text(&line, id);
        for (unsigned long long letters =
                 cli_text_letters(id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
             letters < pad; letters += 1ull)
        {
            cli_line_character(&line, ' ');
        }
        cli_line_text(&line, "  ");
        if (!terminal)
        {
            cli_line_text(&line, about);
        }
        else if (room >= 20ull)
        {
            cli_cut(about, cellul.len(&(CatenaFinitaCfg){.src = about, .cap = CLI_CELLS_POOL}), room, &line);
        }
        const unsigned long long size = cli_line_fits(&line) ? line.at : 0ull;
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
    CliLine out = {line, sizeof(line), 0ull};
    cli_line_text(&out, "  $ ");
    cli_line_text(&out, shown);
    cli_out_line(line, cli_line_fits(&out) ? out.at : 0ull);
}

void cli_job_show(const char *root, const CliCatalog *catalog, unsigned int job)
{
    const CliJob *const one = &catalog->job[job];
    const unsigned int most = cli_width();
    static char s_line[1u << 16u];
    cli_out(cli_catalog_at(catalog, one->id));
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, "  title  ");
    cli_line_text(&line, cli_catalog_at(catalog, one->title));
    cli_out_line(s_line, line.at);
    line.at = 0ull;
    cli_line_text(&line, "  file   ");
    cli_line_text(&line, cli_catalog_at(catalog, one->file));
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
            cli_line_text(&line, "  ");
            cli_line_text(&line, cli_catalog_at(catalog, param->key));
            cli_line_text(&line, "  ");
            cli_line_text(&line, param->kind);
            cli_line_text(&line, param->required ? "  required" : "");
            if (fallback[0] != '\0')
            {
                cli_line_text(&line, "  default ");
                cli_line_text(&line, fallback);
            }
            cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
            if (param->choices > 0u)
            {
                static char s_choices[1u << 16u];
                CliLine choices = {s_choices, sizeof(s_choices), 0ull};
                for (unsigned int choice = 0u; choice < param->choices; choice += 1u)
                {
                    cli_line_text(&choices, (choice > 0u) ? ", " : "");
                    cli_line_text(&choices, cli_catalog_at(catalog, catalog->list[param->first + choice]));
                }
                cli_wrap(s_choices, cli_line_fits(&choices) ? choices.at : 0ull, "      ", most, jobs_line_out, NULL);
            }
        }
    }
    cli_out("");
    cli_out("runs");
    static CliValues s_values;
    char why[4096];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_job_values(catalog, job, NULL, 0u, &s_values, &said) ||
        !cli_runner_shown(root, catalog, job, &s_values, jobs_shown, NULL, &said))
    {
        line.at = 0ull;
        cli_line_text(&line, "  ");
        cli_line_counted(&line, why, cli_line_fits(&said) ? said.at : 0ull);
        cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    }
}

// A run told to the window as it goes, on a channel of its own under runs, where a terminal of the window's started
// this process; elsewhere told to no one; and the code its end gave
typedef struct
{
    CliDoorClient *window;
    char channel[64];
    int has_code;
    long long code;
    int ended;
} CliLineRun;

static CliLineRun s_run;
static char s_told[1u << 20u];

void cli_line_told_begun(void)
{
    s_run.window = cli_door_from_environment();
    CliLine channel = {s_run.channel, sizeof(s_run.channel), 0ull};
    cli_line_text(&channel, "runs/");
    cli_line_decimal(&channel, cli_process_own(), 1u);
    (void)cli_line_closed(&channel);
}

void cli_line_told(const char *body, unsigned long long size)
{
    if (s_run.window != NULL)
    {
        char quiet[256];
        CliLine ignored = {quiet, sizeof(quiet), 0ull};
        unsigned long long heard = 0ull;
        (void)cli_door_said(s_run.window, s_run.channel, body, size, &heard, &ignored);
    }
}

// a line of the errors led by `lead`
static void jobs_err_led(const char *lead, const char *text)
{
    static char s_line[1u << 16u];
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, lead);
    cli_line_text(&line, text);
    cli_err_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
}

// what a run says: each line to the terminal and the window, and its end, the pages it wrote and the program it made
static void jobs_said(void *context, const CliSaid *said)
{
    (void)context;
    CliLine told = {s_told, sizeof(s_told), 0ull};
    // a blob the run's writer placed is the panel's, and the command line shows the console alone
    if (said->kind == CLI_SAID_START)
    {
        return;
    }
    if (said->kind == CLI_SAID_LINE)
    {
        cli_line_text(&told, "{\"line\":");
        cli_json_quoted_lossy(&told, (const unsigned char *)said->text,
                              cellul.len(&(CatenaFinitaCfg){.src = said->text, .cap = CLI_CELLS_POOL}));
        cli_line_text(&told, ",\"stream\":");
        cli_json_quoted(&told, said->stream,
                        cellul.len(&(CatenaFinitaCfg){.src = said->stream, .cap = CLI_CELLS_POOL}));
        cli_line_text(&told, ",\"ms\":");
        cli_json_double(&told, said->ms);
        cli_line_character(&told, '}');
        cli_line_told(s_told, told.at);
        if (cellul.eq(&(CatenaFinitaCfg){.src = said->stream, .other = "stdout", .cap = CLI_CELLS_POOL}))
        {
            cli_out(said->text);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = said->stream, .other = "command", .cap = CLI_CELLS_POOL}))
        {
            jobs_err_led("$ ", said->text);
        }
        else
        {
            cli_err(said->text);
        }
        return;
    }
    cli_line_text(&told, "{\"state\":\"ended\",\"code\":");
    if (said->has_code)
    {
        cli_line_signed(&told, said->code);
    }
    else
    {
        cli_line_text(&told, "null");
    }
    cli_line_text(&told, said->stopped ? ",\"stopped\":true,\"ms\":" : ",\"stopped\":false,\"ms\":");
    cli_json_double(&told, said->ms);
    cli_line_character(&told, '}');
    cli_line_told(s_told, told.at);
    for (unsigned int at = 0u; at < said->view_count; at += 1u)
    {
        jobs_err_led("page ", said->views[at]);
    }
    if (said->made != NULL)
    {
        jobs_err_led("made ", said->made);
    }
    s_run.has_code = said->has_code;
    s_run.code = said->code;
    s_run.ended = 1;
}

int cli_job_run(const char *root, const CliCatalog *catalog, unsigned int job, const char *const *words,
                unsigned int count)
{
    static CliValues s_values;
    char why[4096];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_job_values(catalog, job, words, count, &s_values, &said))
    {
        cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_WRONG;
    }
    cli_line_told_begun();
    s_run.ended = 0;
    s_run.has_code = 0;
    CliLine told = {s_told, sizeof(s_told), 0ull};
    cli_line_text(&told, "{\"state\":\"started\",\"job\":");
    const char *const id = cli_catalog_at(catalog, catalog->job[job].id);
    const char *const title = cli_catalog_at(catalog, catalog->job[job].title);
    cli_json_quoted(&told, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    cli_line_text(&told, ",\"title\":");
    cli_json_quoted(&told, title, cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_CELLS_POOL}));
    cli_line_text(&told, ",\"root\":");
    cli_json_quoted_lossy(&told, (const unsigned char *)root,
                          cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    cli_line_text(&told, ",\"words\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&told, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(&told, (const unsigned char *)words[at],
                              cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(&told, "]}");
    cli_line_told(s_told, told.at);
    cli_runs_attached();
    const CliRunSink sink = {jobs_said, NULL};
    unsigned long long run = 0ull;
    said.at = 0ull;
    if (!cli_runs_start(sink, root, catalog, job, &s_values, &run, &said))
    {
        cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_WRONG;
    }
    cli_runs_wait(run);
    // the code is cut to the system's exit code
    return (s_run.ended && s_run.has_code) ? (int)s_run.code : CLI_NO_CODE;
}
