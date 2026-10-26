// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_event.c: what a run reports written as the page reads it, each field by its name
#include "../text/json_tree.h"
#include "runner.h"

// a text written as a JSON string, its bytes read as Rust's from_utf8_lossy reads them
static void text_quoted(const char *text, VerbaLinea *out)
{
    cli_json_quoted_lossy(out, (const unsigned char *)text,
                          cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_run_event_written(const CliRunEvent *event, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"run\":");
    mmgr_line_decimal(out, event->run, 1u);
    if (event->kind == CLI_RUN_EVENT_START)
    {
        mmgr_line_put(out, ",\"job\":");
        text_quoted((event->job != NULL) ? event->job : "", out);
        mmgr_line_ch(out, '}');
        return;
    }
    if (event->kind == CLI_RUN_EVENT_LINE)
    {
        mmgr_line_put(out, ",\"stream\":");
        text_quoted((event->stream != NULL) ? event->stream : "", out);
        mmgr_line_put(out, ",\"text\":");
        text_quoted((event->text != NULL) ? event->text : "", out);
        mmgr_line_put(out, ",\"ms\":");
        cli_json_double(out, event->ms);
        mmgr_line_ch(out, '}');
        return;
    }
    mmgr_line_put(out, ",\"code\":");
    if (event->has_code)
    {
        mmgr_line_decimal_signed(out, event->code);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, event->stopped ? ",\"stopped\":true,\"views\":[" : ",\"stopped\":false,\"views\":[");
    for (unsigned int at = 0u; at < event->view_count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        text_quoted(event->views[at], out);
    }
    mmgr_line_put(out, "],\"made\":");
    if (event->made != NULL)
    {
        text_quoted(event->made, out);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"ms\":");
    cli_json_double(out, event->ms);
    mmgr_line_ch(out, '}');
}
