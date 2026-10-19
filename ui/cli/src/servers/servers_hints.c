// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_hints.c: what a server can do, as its capabilities name it; the spans Expand Selection steps through, from
// the server's selection ranges; and the hints a server writes in the text and not into it, each its place, its text,
// its kind, 1 a type, 2 a parameter's name and 0 any other, and whether a space stands before it and after it
#include "../inspect/inspect.h"
#include "servers_internal.h"

// a server's capabilities read into a tree taken to their size, under their lock
static CliJson s_capable;
static CliLock s_capable_lock = CLI_LOCK_INIT;

// what the server answers read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

// the capability `name` of the server read, with the capabilities' lock taken: its node
static unsigned int hints_capability(CliLsp *server, const char *name)
{
    unsigned long long length = 0ull;
    const char *const capabilities = cli_lsp_capabilities(server, &length);
    cli_lock_take(&s_capable_lock);
    char quiet[256];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    const unsigned int root = cli_json_taken(&s_capable, capabilities, length, 0u, 0ull, &said);
    return cli_json_member(&s_capable, root, name);
}

// the capabilities' tree given back with their lock
static void hints_capability_given(void)
{
    cli_json_given(&s_capable);
    cli_lock_give(&s_capable_lock);
}

int cli_servers_capable(CliLsp *server, const char *name)
{
    const unsigned int member = hints_capability(server, name);
    const int capable = (member != CLI_JSON_NONE) && !cli_json_is(&s_capable, member, CFG_JSON_NULL) &&
                        !cli_json_is(&s_capable, member, CFG_JSON_FALSE);
    hints_capability_given();
    return capable;
}

int cli_servers_capable_true(CliLsp *server, const char *name, const char *inner)
{
    const unsigned int member = hints_capability(server, name);
    const int capable = cli_json_is(&s_capable, cli_json_member(&s_capable, member, inner), CFG_JSON_TRUE);
    hints_capability_given();
    return capable;
}

// the params of a request about the file `uri` begun, its object left open
static void hints_params_begun(const char *uri, CliLine *params)
{
    cli_line_text(params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    cli_line_character(params, '}');
}

int cli_servers_selection(const char *path, CliTextPlace from, CliTextPlace to, CliLine *out, CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if ((server == NULL) || !cli_servers_capable(server, "selectionRangeProvider"))
    {
        if (server != NULL)
        {
            cli_lsp_given(server);
        }
        cli_line_text(out, "[]");
        return 1;
    }
    cli_servers_reading_held(&s_reading);
    char uri[CLI_PATH_LONGEST];
    cli_servers_uri_of(path, uri);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    hints_params_begun(uri, &params);
    cli_line_text(&params, ",\"positions\":[{\"line\":");
    cli_line_decimal(&params, from.line, 1u);
    cli_line_text(&params, ",\"character\":");
    cli_line_decimal(&params, from.col, 1u);
    cli_line_text(&params, "}]}");
    CliLine result;
    cli_servers_answer_begun(&s_reading, &result);
    const int asked =
        cli_servers_asked(server, "textDocument/selectionRange", &pieces, &params, CLI_SERVERS_ASKING, &result, said);
    cli_lsp_given(server);
    if (asked)
    {
        const CliJson *const json = &s_reading.json;
        const unsigned int first = cli_json_item(json, cli_servers_answer_read(&s_reading, &result), 0u);
        // the chain counted, then its spans and those the span asked for lies in, each in a cell of that count
        unsigned int chain = 0u;
        for (unsigned int at = first; cli_json_is(json, at, CFG_JSON_OBJECT); at = cli_json_member(json, at, "parent"))
        {
            chain += 1u;
        }
        CliInspectSpan *const spans =
            (chain > 0u) ? cli_cell_taken(2ull * chain * sizeof(CliInspectSpan), "a selection's spans") : NULL;
        unsigned int held = 0u;
        for (unsigned int at = (spans != NULL) ? first : CLI_JSON_NONE; cli_json_is(json, at, CFG_JSON_OBJECT);
             at = cli_json_member(json, at, "parent"))
        {
            const unsigned int range = cli_json_member(json, at, "range");
            const CliTextPlace start = cli_servers_place(json, cli_json_member(json, range, "start"));
            const CliTextPlace end = cli_servers_place(json, cli_json_member(json, range, "end"));
            spans[held] = (CliInspectSpan){{start.line, start.col}, {end.line, end.col}};
            held += 1u;
        }
        const unsigned int count = (spans != NULL)
                                       ? cli_inspect_ranges_at(spans, held, (CliInspectPlace){from.line, from.col},
                                                               (CliInspectPlace){to.line, to.col}, spans + chain, chain)
                                       : 0u;
        cli_line_character(out, '[');
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            const CliInspectSpan *const span = &spans[chain + one];
            cli_line_text(out, (one > 0u) ? ",[" : "[");
            cli_text_place_written((CliTextPlace){span->from.line, span->from.col}, out);
            cli_line_character(out, ',');
            cli_text_place_written((CliTextPlace){span->to.line, span->to.col}, out);
            cli_line_character(out, ']');
        }
        cli_line_character(out, ']');
        cli_cell_given(spans);
    }
    cli_servers_reading_released(&s_reading);
    return asked;
}

// a hint's label written as a JSON string, trimmed: its text, or the values of its parts joined; 0 where it is empty
static int hints_label_written(const CliJson *json, unsigned int label, CliLine *out)
{
    static char s_label[1u << 16u];
    CliLine joined = {s_label, sizeof(s_label), 0ull};
    unsigned long long size = 0ull;
    const char *const text = cli_json_string(json, label, &size);
    if (text != NULL)
    {
        cli_line_counted(&joined, text, size);
    }
    for (unsigned int part = cli_json_is(json, label, CFG_JSON_ARRAY) ? json->node[label].first : CLI_JSON_NONE;
         part != CLI_JSON_NONE; part = json->node[part].next)
    {
        const char *const value = cli_json_string(json, cli_json_member(json, part, "value"), &size);
        if (value != NULL)
        {
            cli_line_counted(&joined, value, size);
        }
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(s_label, cli_line_fits(&joined) ? joined.at : 0ull, &from, &to);
    if (to == from)
    {
        return 0;
    }
    cli_json_quoted(out, s_label + from, to - from);
    return 1;
}

int cli_servers_hints(const char *path, unsigned int from, unsigned int to, CliLine *out, CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if ((server == NULL) || !cli_servers_capable(server, "inlayHintProvider"))
    {
        if (server != NULL)
        {
            cli_lsp_given(server);
        }
        cli_line_text(out, "[]");
        return 1;
    }
    cli_servers_reading_held(&s_reading);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine held;
    cli_pieces_begun(&pieces, &held, piece, sizeof(piece));
    const int has = cli_lsp_text(server, path, &held);
    unsigned long long joined_size = 0ull;
    char *const text = cli_pieces_joined(&pieces, &held, "the text a server holds", &joined_size);
    const unsigned long long size = (has && (text != NULL)) ? joined_size : 0ull;
    unsigned long long lines = 1ull;
    unsigned long long last = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if (text[at] == '\n')
        {
            lines += 1ull;
            last = at + 1ull;
        }
    }
    // the range ends at the start of the line after `to`, or at the end of the text where `to` is its last line
    char uri[CLI_PATH_LONGEST];
    cli_servers_uri_of(path, uri);
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    hints_params_begun(uri, &params);
    cli_line_text(&params, ",\"range\":{\"start\":{\"line\":");
    cli_line_decimal(&params, from, 1u);
    cli_line_text(&params, ",\"character\":0},\"end\":{\"line\":");
    const int inside = ((unsigned long long)to + 1ull) < lines;
    cli_line_decimal(&params, inside ? (unsigned long long)to + 1ull : lines - 1ull, 1u);
    cli_line_text(&params, ",\"character\":");
    cli_line_decimal(&params, inside ? 0ull : cli_text_units((size > 0ull) ? text + last : "", size - last), 1u);
    cli_line_text(&params, "}}}");
    cli_cell_given(text);
    CliLine result;
    cli_servers_answer_begun(&s_reading, &result);
    const int asked =
        cli_servers_asked(server, "textDocument/inlayHint", &pieces, &params, CLI_SERVERS_ASKING, &result, said);
    cli_lsp_given(server);
    if (asked)
    {
        const CliJson *const json = &s_reading.json;
        const unsigned int all = cli_servers_answer_read(&s_reading, &result);
        cli_line_character(out, '[');
        int written = 0;
        for (unsigned int one = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const unsigned long long before = out->at;
            const CliTextPlace at = cli_servers_place(json, cli_json_member(json, one, "position"));
            cli_line_text(out, written ? ",{\"line\":" : "{\"line\":");
            cli_line_decimal(out, at.line, 1u);
            cli_line_text(out, ",\"col\":");
            cli_line_decimal(out, at.col, 1u);
            cli_line_text(out, ",\"label\":");
            if (!hints_label_written(json, cli_json_member(json, one, "label"), out))
            {
                out->at = before;
                continue;
            }
            cli_line_text(out, ",\"kind\":");
            // the kind is cut to its low byte, as the protocol's 1 and 2 always fit
            cli_line_decimal(out, cli_servers_whole(json, cli_json_member(json, one, "kind"), 0ull) & 0xffull, 1u);
            const int left = cli_json_is(json, cli_json_member(json, one, "paddingLeft"), CFG_JSON_TRUE);
            const int right = cli_json_is(json, cli_json_member(json, one, "paddingRight"), CFG_JSON_TRUE);
            cli_line_text(out, left ? ",\"left\":true" : ",\"left\":false");
            cli_line_text(out, right ? ",\"right\":true}" : ",\"right\":false}");
            written = 1;
        }
        cli_line_character(out, ']');
    }
    cli_servers_reading_released(&s_reading);
    return asked;
}
