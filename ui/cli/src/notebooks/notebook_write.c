// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_write.c: a notebook written back as the file it was read from, and a notebook's JSON written without its
// outputs
#include "notebook_internal.h"

// a percent script's text: its header, then each cell's mark and its lines, a blank line before each mark
static void cli_script_write(const CliJson *json, const char *path, unsigned int book, CliLine *out)
{
    const char *const comment = cli_notebook_comment(path);
    const unsigned long long begun = out->at;
    const char *const header = cli_json_string(json, cli_json_member(json, book, "header"), NULL);
    cli_line_text(out, (header != NULL) ? header : "");
    const unsigned int cells = cli_json_member(json, book, "cells");
    for (unsigned int cell = cli_json_is(json, cells, CFG_JSON_ARRAY) ? json->node[cells].first : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = json->node[cell].next)
    {
        const unsigned long long written = cli_line_fits(out) ? (out->at - begun) : 0ull;
        if ((written != 0ull) && !cli_notebook_ends(out->out + begun, written, "\n\n"))
        {
            cli_line_character(out, '\n');
        }
        const unsigned int metadata = cli_json_member(json, cell, "metadata");
        const char *const title = cli_json_string(json, cli_json_member(json, metadata, "title"), NULL);
        const char *const named = cli_json_string(json, cli_json_member(json, cell, "cell_type"), NULL);
        const char *const kind = (named != NULL) ? named : "code";
        const int markdown = cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "markdown", .cap = CLI_CELLS_POOL});
        cli_line_text(out, comment);
        cli_line_text(out, " %%");
        if ((title != NULL) && (title[0] != '\0'))
        {
            cli_line_character(out, ' ');
            cli_line_text(out, title);
        }
        if (markdown || (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "raw", .cap = CLI_CELLS_POOL})))
        {
            cli_line_text(out, " [");
            cli_line_text(out, kind);
            cli_line_character(out, ']');
        }
        cli_line_character(out, '\n');
        unsigned long long length = 0ull;
        const char *const source = cli_notebook_source(json, cell, &length);
        const char *at = source;
        while (at < (source + length))
        {
            const char *stop = NULL;
            const char *const next = cli_notebook_line(at, source + length, &stop);
            if (markdown)
            {
                cli_line_text(out, comment);
                if (stop > at)
                {
                    cli_line_character(out, ' ');
                }
            }
            cli_line_counted(out, at, (unsigned long long)(stop - at));
            cli_line_character(out, '\n');
            at = next;
        }
    }
}

// the list of lines a text node keeps, where it is one
static unsigned int cli_lines_again(CliJson *json, unsigned int value)
{
    const unsigned int joined = cli_notebook_joined(json, value);
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, joined, &length);
    return (text != NULL) ? cli_notebook_lines_of(json, text, length) : CLI_JSON_NONE;
}

void cli_notebook_write(CliJson *json, const char *path, unsigned int book, CliLine *out)
{
    const char *const format = cli_json_string(json, cli_json_member(json, book, "format"), NULL);
    if ((format != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = format, .other = "percent", .cap = CLI_CELLS_POOL})))
    {
        cli_script_write(json, path, book, out);
        return;
    }
    const unsigned int copy = cli_json_copy(json, book);
    cli_json_remove(json, copy, "format");
    const unsigned int cells = cli_json_member(json, copy, "cells");
    for (unsigned int cell = cli_json_is(json, cells, CFG_JSON_ARRAY) ? json->node[cells].first : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = json->node[cell].next)
    {
        if (json->node[cell].kind != CFG_JSON_OBJECT)
        {
            continue;
        }
        cli_json_set(json, cell, "source", cli_lines_again(json, cli_json_member(json, cell, "source")));
        const unsigned int outputs = cli_json_member(json, cell, "outputs");
        for (unsigned int output = cli_json_is(json, outputs, CFG_JSON_ARRAY) ? json->node[outputs].first
                                                                              : CLI_JSON_NONE;
             output != CLI_JSON_NONE; output = json->node[output].next)
        {
            const unsigned int text = cli_json_member(json, output, "text");
            if (text != CLI_JSON_NONE)
            {
                cli_json_set(json, output, "text", cli_lines_again(json, text));
            }
            const unsigned int data = cli_json_member(json, output, "data");
            for (unsigned int kind = cli_json_is(json, data, CFG_JSON_OBJECT) ? json->node[data].first : CLI_JSON_NONE;
                 kind != CLI_JSON_NONE; kind = json->node[kind].next)
            {
                const char *const name = json->text + json->node[kind].key;
                if ((json->node[kind].kind == CFG_JSON_STRING) &&
                    !cli_notebook_ends(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}),
                                       "json") &&
                    (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "text/", .cap = CLI_CELLS_POOL})))
                {
                    cli_json_set(json, data, name, cli_lines_again(json, kind));
                }
            }
        }
    }
    cli_json_write(json, copy, out, 1u, 1);
    cli_line_character(out, '\n');
}

int cli_notebook_without_outputs(CliJson *json, const char *text, unsigned long long size, CfgJsonToken *tokens,
                                 unsigned int tokens_most, CliLine *out, CliLine *said)
{
    const unsigned int book = cli_json_read(json, text, size, tokens, tokens_most, said);
    if (book == CLI_JSON_NONE)
    {
        return 0;
    }
    const unsigned int cells = cli_json_member(json, book, "cells");
    for (unsigned int cell = cli_json_is(json, cells, CFG_JSON_ARRAY) ? json->node[cells].first : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = json->node[cell].next)
    {
        const char *const kind = cli_json_string(json, cli_json_member(json, cell, "cell_type"), NULL);
        if ((kind != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "code", .cap = CLI_CELLS_POOL})))
        {
            cli_json_set(json, cell, "outputs", cli_json_made(json, CFG_JSON_ARRAY));
            cli_json_set(json, cell, "execution_count", cli_json_made(json, CFG_JSON_NULL));
        }
        if (json->node[cell].kind == CFG_JSON_OBJECT)
        {
            cli_json_set(json, cell, "source", cli_notebook_joined(json, cli_json_member(json, cell, "source")));
        }
    }
    cli_json_set(json, book, "format", cli_json_string_made(json, "ipynb", 5u));
    cli_notebook_write(json, "x.ipynb", book, out);
    if (json->full)
    {
        cli_line_text(said, "the notebook holds more than the tree it is read into keeps");
        return 0;
    }
    return 1;
}
