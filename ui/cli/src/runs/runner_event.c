// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_event.c: what a run reports written as the page reads it, each field by its name
#include "../text/json_tree.h"
#include "runner.h"

typedef struct
{
    const char *text;
    VerbaLinea *out;
} RunnerEventCtx;

// a text written as a JSON string, its bytes read as Rust's from_utf8_lossy reads them
EMBED_INLINE void text_quote(const RunnerEventCtx *args)
{
    const char *text = args->text;
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_run_event_write(const CliRunnerEventCfg *args)
{
    mmgr_line_put(args->out, "{\"run\":");
    mmgr_line_decimal(args->out, args->event->run, 1u);
    if (args->event->kind == CLI_RUN_EVENT_START)
    {
        mmgr_line_put(args->out, ",\"job\":");
        EMBED_CALL(text_quote, RunnerEventCtx, .text = (args->event->job != NULL) ? args->event->job : "",
                   .out = args->out);
        mmgr_line_ch(args->out, '}');
        return;
    }
    if (args->event->kind == CLI_RUN_EVENT_LINE)
    {
        mmgr_line_put(args->out, ",\"stream\":");
        EMBED_CALL(text_quote, RunnerEventCtx, .text = (args->event->stream != NULL) ? args->event->stream : "",
                   .out = args->out);
        mmgr_line_put(args->out, ",\"text\":");
        EMBED_CALL(text_quote, RunnerEventCtx, .text = (args->event->text != NULL) ? args->event->text : "",
                   .out = args->out);
        mmgr_line_put(args->out, ",\"ms\":");
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = args->out,
                   .value_f64 = args->event->ms);
        mmgr_line_ch(args->out, '}');
        return;
    }
    mmgr_line_put(args->out, ",\"code\":");
    if (args->event->has_code)
    {
        mmgr_line_decimal_signed(args->out, args->event->code);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, args->event->stopped ? ",\"stopped\":true,\"views\":[" : ",\"stopped\":false,\"views\":[");
    for (unsigned int at = 0u; at < args->event->view_count; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        EMBED_CALL(text_quote, RunnerEventCtx, .text = args->event->views[at], .out = args->out);
    }
    mmgr_line_put(args->out, "],\"made\":");
    if (args->event->made != NULL)
    {
        EMBED_CALL(text_quote, RunnerEventCtx, .text = args->event->made, .out = args->out);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"ms\":");
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = args->out,
               .value_f64 = args->event->ms);
    mmgr_line_ch(args->out, '}');
}
