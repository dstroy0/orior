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
#include "callback_cfg.h"
#include "cells.h"
#include "command_line_internal.h"

// a test written after those written
static void jobs_test_write(const CliTestEachCfg *args)
{
    const CliTest *test = args->test;
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 1ull) ? "," : "");
    EMBED_CALL(cli_testing.test_write, CliTestingCfg, .test = test, .out = out);
}

// a test reported as its id and its line
static void jobs_test_print(const CliTestEachCfg *args)
{
    unsigned int *const count = args->context;
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea out = {line, sizeof(line), 0ull};
    mmgr_line_put(&out, args->test->id);
    mmgr_line_put(&out, "  line ");
    mmgr_line_decimal(&out, args->test->line + 1u, 1u);
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = line,
               .size = mmgr_line_ok(&out) ? out.at : 0ull);
    *count += 1u;
}

int cli_tests_list(const CliCommandLineRootCtx *args)
{
    const char *root = args->root;
    int json = 0;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        json |= (cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--json", .cap = CLI_CELLS_POOL}));
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
            EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                       .text = "a listing of tests could not be taken");
            return CLI_NO_CODE;
        }
        VerbaLinea found = {room->found, sizeof(room->found), 0ull};
        mmgr_line_ch(&found, '[');
        EMBED_CALL(cli_testing_root.tests_find, CliTestingRootCfg, .root = root, .each = jobs_test_write,
                   .context = &found);
        mmgr_line_ch(&found, ']');
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &room->json,
                   .node_json_node_pointer = room->node, .nodes_most = sizeof(room->node) / sizeof(room->node[0]),
                   .text_out = room->text, .text_most = sizeof(room->text));
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        const unsigned int tree =
            EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &room->json,
                       .text = room->found, .size = found.at, .tokens = room->token,
                       .tokens_most = sizeof(room->token) / sizeof(room->token[0]), .err_msg = &ignored);
        VerbaLinea pretty = {room->pretty, sizeof(room->pretty), 0ull};
        if (tree != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &room->json, .node = tree,
                       .out = &pretty, .indent = 2u, .sorted = 0);
        }
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = room->pretty,
                   .size = mmgr_line_ok(&pretty) ? pretty.at : 0ull);
        g_cli_cells->persistent_buf_release(room, &g_cli_command_line_cells);
        return 0;
    }
    unsigned int listed = 0u;
    EMBED_CALL(cli_testing_root.tests_find, CliTestingRootCfg, .root = root, .each = jobs_test_print,
               .context = &listed);
    if (listed == 0u)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "the tree contains no test that pytest or unittest finds");
        return CLI_NO_CODE;
    }
    return 0;
}

int cli_jobs_list(const CliCommandLineWordCtx *args)
{
    const CliCatalog *const catalog = cli_command_line_word_internal.catalog(args);
    static unsigned int s_listed[CLI_LINE_JOBS];
    unsigned int count = 0u;
    unsigned long long pad = 0ull;
    for (unsigned int at = 0u; (at < catalog->jobs) && (count < CLI_LINE_JOBS); at += 1u)
    {
        const CliJob *const job = &catalog->job[at];
        const char *const id =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->id);
        if ((args->word != NULL) &&
            !EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx, .text = id, .word = args->word) &&
            !EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                        .text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                           .text_u32 = job->title),
                        .word = args->word) &&
            !EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                        .text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                           .text_u32 = job->about),
                        .word = args->word))
        {
            continue;
        }
        s_listed[count] = at;
        count += 1u;
        const unsigned long long letters =
            EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = id,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
        pad = (letters > pad) ? letters : pad;
    }
    if (count == 0u)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "no job contains that word");
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
        const char *const id =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->id);
        const char *const about =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->about);
        static char s_line[1u << 16u];
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&line, id);
        for (unsigned long long letters =
                 EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = id,
                            .size = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
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
            EMBED_CALL(cli_command_line_text_size_internal.cut, CliCommandLineTextSizeCtx, .text = about,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = about, .cap = CLI_CELLS_POOL}), .most = room,
                       .out = &line);
        }
        const unsigned long long size = mmgr_line_ok(&line) ? line.at : 0ull;
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size =
                       EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = s_line, .size = size));
    }
    return 0;
}

// a line of a shown job written
static void jobs_line_out(const CliEachLineSizeCfg *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = args->line,
               .size = size);
}

// a step's line written as the job's runs show it
static void jobs_show(const CliEachShownCfg *args)
{
    char line[1u << 16u];
    VerbaLinea out = {line, sizeof(line), 0ull};
    mmgr_line_put(&out, "  $ ");
    mmgr_line_put(&out, args->shown);
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = line,
               .size = mmgr_line_ok(&out) ? out.at : 0ull);
}

void cli_job_show(const CliCommandLineRootCtx *args)
{
    const CliCatalog *catalog = args->catalog;
    const CliJob *const one = &catalog->job[args->job];
    const unsigned int most = cli_width();
    static char s_line[1u << 16u];
    EMBED_CALL(
        cli_command_line_internal.out, CliCommandLineCtx,
        .text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = one->id));
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "  title  ");
    mmgr_line_put(&line, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                    .text_u32 = one->title));
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = line.at);
    line.at = 0ull;
    mmgr_line_put(&line, "  file   ");
    mmgr_line_put(&line, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                    .text_u32 = one->file));
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = line.at);
    const char *const about =
        EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = one->about);
    if (about[0] != '\0')
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "");
        EMBED_CALL(cli_command_line_text_internal.wrap, CliCommandLineTextCtx, .text = about,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = about, .cap = CLI_CELLS_POOL}), .lead = "  ",
                   .most = most, .each = jobs_line_out, .context = NULL);
    }
    if (one->params > 0u)
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "");
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "values");
        for (unsigned int at = 0u; at < one->params; at += 1u)
        {
            const CliParam *const param = &catalog->param[one->param_first + at];
            const char *const fallback = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                    .catalog_read = catalog, .text_u32 = param->fallback);
            line.at = 0ull;
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                            .text_u32 = param->key));
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, param->kind);
            mmgr_line_put(&line, param->required ? "  required" : "");
            if (fallback[0] != '\0')
            {
                mmgr_line_put(&line, "  default ");
                mmgr_line_put(&line, fallback);
            }
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
                       .size = mmgr_line_ok(&line) ? line.at : 0ull);
            if (param->choices > 0u)
            {
                static char s_choices[1u << 16u];
                VerbaLinea choices = {s_choices, sizeof(s_choices), 0ull};
                for (unsigned int choice = 0u; choice < param->choices; choice += 1u)
                {
                    mmgr_line_put(&choices, (choice > 0u) ? ", " : "");
                    mmgr_line_put(&choices,
                                  EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                             .text_u32 = catalog->list[param->first + choice]));
                }
                EMBED_CALL(cli_command_line_text_internal.wrap, CliCommandLineTextCtx, .text = s_choices,
                           .size = mmgr_line_ok(&choices) ? choices.at : 0ull, .lead = "      ", .most = most,
                           .each = jobs_line_out, .context = NULL);
            }
        }
    }
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "");
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "runs");
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliValues *const values = g_cli_cells->temporary_buf_alloc(sizeof(*values), &g_cli_command_line_cells);
    if (values == NULL)
    {
        mmgr_line_put(&err_msg, "the cells cannot meet the values a job is shown with");
    }
    const int shown =
        (values != NULL) &&
        EMBED_CALL(cli_command_line_catalog_internal.job_values, CliCommandLineCatalogCtx, .catalog = catalog,
                   .job = args->job, .words = NULL, .count = 0u, .values = values, .err_msg = &err_msg) &&
        EMBED_CALL(cli_runner_err_msg_root.show, CliRunnerErrMsgRootCfg, .root = args->root, .catalog = catalog,
                   .job = args->job, .values = values, .each = jobs_show, .context = NULL, .err_msg = &err_msg);
    g_cli_cells->persistent_buf_release(values, &g_cli_command_line_cells);
    if (!shown)
    {
        line.at = 0ull;
        mmgr_line_put(&line, "  ");
        mmgr_line_put_n(&line, why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ull);
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

// the bytes what a run sends the window is written in
#define CLI_JOBS_STATE_MORE 256ull
#define CLI_JOBS_STATE_ROOM (1ull << 20u)

void cli_window_channel_begin(void)
{
    s_run.window = cli_door_from_environment();
    VerbaLinea channel = {s_run.channel, sizeof(s_run.channel), 0ull};
    mmgr_line_put(&channel, "runs/");
    mmgr_line_decimal(&channel, cli_process_own(), 1u);
    (void)mmgr_line_finish(&channel);
}

void cli_window_channel_send(const CliCommandLineSizeCtx *args)
{
    const char *body = args->body;
    unsigned long long size = args->size;
    if (s_run.window != NULL)
    {
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        unsigned long long reply_size = 0ull;
        (void)EMBED_CALL(cli_door_err_msg.send, CliDoorErrMsgCfg, .client = s_run.window, .channel = s_run.channel,
                         .body = body, .size = size, .delivered = &reply_size, .err_msg = &ignored);
    }
}

typedef struct
{
    VerbaLinea *state;
    const char *lead;
    const char *text;
} CommandLineJobsCtx;
// `state`, a line, sent to the window where it fits, and given back
EMBED_INLINE void jobs_state_send(const CommandLineJobsCtx *args)
{
    if (mmgr_line_ok(args->state))
    {
        EMBED_CALL(cli_command_line_size_internal.window_channel_send, CliCommandLineSizeCtx, .body = args->state->out,
                   .size = args->state->at);
    }
    g_cli_cells->persistent_buf_release(args->state->out, &g_cli_command_line_cells);
}

// a line of the errors led by `lead`
EMBED_INLINE void jobs_err_lead(const CommandLineJobsCtx *args)
{
    const char *text = args->text;
    static char s_line[1u << 16u];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, args->lead);
    mmgr_line_put(&line, text);
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = mmgr_line_ok(&line) ? line.at : 0ull);
}

// what a run prints: each line to the terminal and the window, and its end, the pages it wrote and the program it made
static void jobs_run_event(const CliEachEventCfg *args)
{
    // a blob the run's writer placed is the panel's, and the command line shows the console alone
    if (args->event->kind == CLI_RUN_EVENT_START)
    {
        return;
    }
    // a line's byte is written as at most 6, and the words about it are few
    const unsigned long long text_length =
        (args->event->kind == CLI_RUN_EVENT_LINE)
            ? cellul.len(&(CatenaFinitaCfg){.src = args->event->text, .cap = CLI_CELLS_POOL})
            : 0ull;
    const unsigned long long state_room = (text_length * 6ull) + CLI_JOBS_STATE_MORE;
    char *const state_cell = g_cli_cells->temporary_buf_alloc(state_room, &g_cli_command_line_cells);
    VerbaLinea state = mmgr_line_over(state_cell, state_room);
    if (args->event->kind == CLI_RUN_EVENT_LINE)
    {
        mmgr_line_put(&state, "{\"line\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &state,
                   .text_bytes = (const unsigned char *)args->event->text, .length = text_length);
        mmgr_line_put(&state, ",\"stream\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &state,
                   .text = args->event->stream,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->event->stream, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(&state, ",\"ms\":");
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &state,
                   .value_f64 = args->event->ms);
        mmgr_line_ch(&state, '}');
        EMBED_CALL(jobs_state_send, CommandLineJobsCtx, .state = &state);
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->event->stream, .other = "stdout", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = args->event->text);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = args->event->stream, .other = "command", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(jobs_err_lead, CommandLineJobsCtx, .lead = "$ ", .text = args->event->text);
        }
        else
        {
            EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = args->event->text);
        }
        return;
    }
    mmgr_line_put(&state, "{\"state\":\"ended\",\"code\":");
    if (args->event->has_code)
    {
        mmgr_line_decimal_signed(&state, args->event->code);
    }
    else
    {
        mmgr_line_put(&state, "null");
    }
    mmgr_line_put(&state, args->event->stopped ? ",\"stopped\":true,\"ms\":" : ",\"stopped\":false,\"ms\":");
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &state,
               .value_f64 = args->event->ms);
    mmgr_line_ch(&state, '}');
    EMBED_CALL(jobs_state_send, CommandLineJobsCtx, .state = &state);
    for (unsigned int at = 0u; at < args->event->view_count; at += 1u)
    {
        EMBED_CALL(jobs_err_lead, CommandLineJobsCtx, .lead = "page ", .text = args->event->views[at]);
    }
    if (args->event->made != NULL)
    {
        EMBED_CALL(jobs_err_lead, CommandLineJobsCtx, .lead = "made ", .text = args->event->made);
    }
    s_run.has_code = args->event->has_code;
    s_run.code = args->event->code;
    s_run.ended = 1;
}

int cli_job_run(const CliCommandLineRootCtx *args)
{
    const char *root = args->root;
    const CliCatalog *catalog = args->catalog;
    unsigned int job = args->job;
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliValues *const values = g_cli_cells->temporary_buf_alloc(sizeof(*values), &g_cli_command_line_cells);
    if ((values == NULL) ||
        !EMBED_CALL(cli_command_line_catalog_internal.job_values, CliCommandLineCatalogCtx, .catalog = catalog,
                    .job = job, .words = args->words, .count = args->count, .values = values, .err_msg = &err_msg))
    {
        mmgr_line_put(&err_msg, (values == NULL) ? "the cells cannot meet the values a job is run with" : "");
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        g_cli_cells->persistent_buf_release(values, &g_cli_command_line_cells);
        return CLI_WRONG;
    }
    cli_window_channel_begin();
    s_run.ended = 0;
    s_run.has_code = 0;
    char *const state_cell = g_cli_cells->temporary_buf_alloc(CLI_JOBS_STATE_ROOM, &g_cli_command_line_cells);
    VerbaLinea state = mmgr_line_over(state_cell, CLI_JOBS_STATE_ROOM);
    mmgr_line_put(&state, "{\"state\":\"started\",\"job\":");
    const char *const id = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                      .text_u32 = catalog->job[job].id);
    const char *const title = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                         .text_u32 = catalog->job[job].title);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &state, .text = id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"title\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &state, .text = title,
               .length = cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"root\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &state,
               .text_bytes = (const unsigned char *)root,
               .length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"words\":[");
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&state, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &state,
                   .text_bytes = (const unsigned char *)args->words[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->words[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(&state, "]}");
    EMBED_CALL(jobs_state_send, CommandLineJobsCtx, .state = &state);
    cli_runs_attach();
    const CliRunSink sink = {jobs_run_event, NULL};
    unsigned long long run = 0ull;
    err_msg.at = 0ull;
    const int started =
        EMBED_CALL(cli_runner_err_msg.runs_start, CliRunnerErrMsgCfg, .sink = sink, .root = root, .catalog = catalog,
                   .job = job, .values = values, .run_number_pointer = &run, .err_msg = &err_msg);
    g_cli_cells->persistent_buf_release(values, &g_cli_command_line_cells);
    if (!started)
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_WRONG;
    }
    EMBED_CALL(cli_runner_err_msg_run_number.runs_wait, CliRunnerErrMsgRunNumberCfg, .run_number = run);
    // the code is cut to the system's exit code
    return (s_run.ended && s_run.has_code) ? (int)s_run.code : CLI_NO_CODE;
}
