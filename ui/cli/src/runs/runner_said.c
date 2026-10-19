// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_said.c: what a run says written as the page reads it, each field by its name
#include "../text/json_tree.h"
#include "runner.h"

// a text written as a JSON string, its bytes read as Rust's from_utf8_lossy reads them
static void said_text(const char *text, CliLine *out)
{
    cli_json_quoted_lossy(out, (const unsigned char *)text,
                          cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_said_written(const CliSaid *said, CliLine *out)
{
    cli_line_text(out, "{\"run\":");
    cli_line_decimal(out, said->run, 1u);
    if (said->kind == CLI_SAID_START)
    {
        cli_line_text(out, ",\"job\":");
        said_text((said->job != NULL) ? said->job : "", out);
        cli_line_character(out, '}');
        return;
    }
    if (said->kind == CLI_SAID_LINE)
    {
        cli_line_text(out, ",\"stream\":");
        said_text((said->stream != NULL) ? said->stream : "", out);
        cli_line_text(out, ",\"text\":");
        said_text((said->text != NULL) ? said->text : "", out);
        cli_line_text(out, ",\"ms\":");
        cli_json_double(out, said->ms);
        cli_line_character(out, '}');
        return;
    }
    cli_line_text(out, ",\"code\":");
    if (said->has_code)
    {
        cli_line_signed(out, said->code);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, said->stopped ? ",\"stopped\":true,\"views\":[" : ",\"stopped\":false,\"views\":[");
    for (unsigned int at = 0u; at < said->view_count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        said_text(said->views[at], out);
    }
    cli_line_text(out, "],\"made\":");
    if (said->made != NULL)
    {
        said_text(said->made, out);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"ms\":");
    cli_json_double(out, said->ms);
    cli_line_character(out, '}');
}
