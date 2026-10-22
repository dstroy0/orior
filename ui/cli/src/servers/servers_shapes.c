// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_shapes.c: a server's answer read, and what it says written in orior's own shapes: a place, a path, a hover
// as Markdown, a completion as the editor's item, a diagnostic and a location
#include "../command_line/cells.h"
#include "../text/measured.h"
#include "servers_internal.h"

void cli_servers_reading_held(CliServersReading *reading)
{
    cli_lock_take(&reading->lock);
    reading->answer = NULL;
    reading->answer_size = 0ull;
    cli_json_begun(&reading->json, NULL, 0u, NULL, 0ull);
}

// the reading's answer given back, and its pieces
static void reading_answer_given(CliServersReading *reading)
{
    cli_cell_given(reading->answer);
    reading->answer = NULL;
    reading->answer_size = 0ull;
    cli_pieces_given(&reading->pieces);
}

void cli_servers_reading_released(CliServersReading *reading)
{
    reading_answer_given(reading);
    cli_json_given(&reading->json);
    cli_lock_give(&reading->lock);
}

void cli_servers_answer_begun(CliServersReading *reading, CliLine *line)
{
    reading_answer_given(reading);
    cli_pieces_begun(&reading->pieces, line, reading->piece, sizeof(reading->piece));
}

int cli_servers_answer_joined(CliServersReading *reading, CliLine *line)
{
    reading->answer = cli_pieces_joined(&reading->pieces, line, "a server's answer", &reading->answer_size);
    reading->answer_size = (reading->answer != NULL) ? reading->answer_size : 0ull;
    return reading->answer != NULL;
}

unsigned int cli_servers_read(CliServersReading *reading, const char *text, unsigned long long size)
{
    cli_json_given(&reading->json);
    char quiet[256];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    return cli_json_taken(&reading->json, text, size, 0u, 0ull, &said);
}

unsigned int cli_servers_answer_read(CliServersReading *reading, CliLine *line)
{
    return cli_servers_answer_joined(reading, line) ? cli_servers_read(reading, reading->answer, reading->answer_size)
                                                    : CLI_JSON_NONE;
}

int cli_servers_asked(CliLsp *server, const char *method, CliPieces *pieces, CliLine *params, unsigned long long wait,
                      CliLine *result, CliLine *said)
{
    unsigned long long length = 0ull;
    char *const asked_with = cli_pieces_joined(pieces, params, "the params a server is asked with", &length);
    if (asked_with == NULL)
    {
        cli_line_text(said, "the asking could not be written: the cells cannot meet it");
    }
    const int asked = (asked_with != NULL) && cli_lsp_requested(server, method, asked_with, length, wait, result, said);
    cli_cell_given(asked_with);
    return asked;
}

int cli_servers_asked_at(CliLsp *server, const char *method, const char *path, unsigned int line, unsigned int col,
                         const char *added, unsigned long long wait, CliLine *result, CliLine *said)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    cli_lsp_at(path, line, col, added, &params);
    return cli_servers_asked(server, method, &pieces, &params, wait, result, said);
}

unsigned long long cli_servers_whole(const CliJson *json, unsigned int node, unsigned long long otherwise)
{
    if (!cli_json_is(json, node, CFG_JSON_NUMBER) || (json->node[node].length == 0ull))
    {
        return otherwise;
    }
    const char *const digits = json->text + json->node[node].text;
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < json->node[node].length; at += 1ull)
    {
        if ((digits[at] < '0') || (digits[at] > '9') || (value > (~0ull - 9ull) / 10ull))
        {
            return otherwise;
        }
        value = (value * 10ull) + (unsigned long long)(digits[at] - '0');
    }
    return value;
}

CliTextPlace cli_servers_place(const CliJson *json, unsigned int node)
{
    // a place past 2^32 units is cut to its low word, as the protocol's unsigned integer is read
    const CliTextPlace place = {(unsigned int)cli_servers_whole(json, cli_json_member(json, node, "line"), 0ull),
                                (unsigned int)cli_servers_whole(json, cli_json_member(json, node, "character"), 0ull)};
    return place;
}

void cli_servers_path_written(const char *tree, const char *path, CliLine *out)
{
    char written[CLI_PATH_LONGEST];
    CliLine shown = {written, sizeof(written), 0ull};
    cli_servers_tree_path(tree, path, &shown);
    cli_json_quoted_lossy(out, (const unsigned char *)written, cli_line_fits(&shown) ? shown.at : 0ull);
}

void cli_servers_markdown_of(const CliJson *json, unsigned int contents, CliLine *out)
{
    unsigned long long size = 0ull;
    const char *const text = cli_json_string(json, contents, &size);
    if (text != NULL)
    {
        cli_line_counted(out, text, size);
        return;
    }
    if (cli_json_is(json, contents, CFG_JSON_ARRAY))
    {
        // a part that writes nothing is left out with the break before it, so each is measured before it is written
        int written = 0;
        for (unsigned int part = json->node[contents].first; part != CLI_JSON_NONE; part = json->node[part].next)
        {
            char piece[64];
            CliLine measure;
            cli_measure_begun(&measure, piece, sizeof(piece));
            cli_servers_markdown_of(json, part, &measure);
            if (cli_measured(&measure) == 0ull)
            {
                continue;
            }
            cli_line_text(out, written ? "\n\n" : "");
            cli_servers_markdown_of(json, part, out);
            written = 1;
        }
        return;
    }
    unsigned long long language_size = 0ull;
    const char *const language = cli_json_string(json, cli_json_member(json, contents, "language"), &language_size);
    unsigned long long value_size = 0ull;
    const char *const value = cli_json_string(json, cli_json_member(json, contents, "value"), &value_size);
    if (value == NULL)
    {
        return;
    }
    if (language != NULL)
    {
        cli_line_text(out, "```");
        cli_line_counted(out, language, language_size);
        cli_line_character(out, '\n');
        cli_line_counted(out, value, value_size);
        cli_line_text(out, "\n```");
        return;
    }
    cli_line_counted(out, value, value_size);
}

// the kind the editor shows for the protocol's kind of a completion
static const char *shapes_kind_of(unsigned long long kind)
{
    switch (kind)
    {
    case 2ull:
    case 3ull:
    case 4ull:
        return "function";
    case 5ull:
    case 10ull:
        return "field";
    case 6ull:
        return "variable";
    case 7ull:
    case 8ull:
    case 13ull:
    case 22ull:
    case 25ull:
        return "type";
    case 9ull:
        return "module";
    case 12ull:
    case 20ull:
    case 21ull:
        return "constant";
    case 14ull:
        return "keyword";
    case 15ull:
        return "snippet";
    case 24ull:
        return "operator";
    default:
        return "word";
    }
}

// a string member of `one`, NULL where it has none
static const char *shapes_member(const CliJson *json, unsigned int one, const char *name, unsigned long long *size)
{
    return cli_json_string(json, cli_json_member(json, one, name), size);
}

void cli_servers_item_of(const CliJson *json, unsigned int one, CliCompletions *items)
{
    unsigned long long label_size = 0ull;
    const char *label = shapes_member(json, one, "label", &label_size);
    label = (label != NULL) ? label : "";
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(label, label_size, &from, &to);
    while ((to - from >= 3ull) &&
           (memor.cmp(&(MemoriaCfg){.src = label + from, .other = "\xe2\x80\xa2", .bytes = 3u}) == 0))
    {
        from += 3ull;
    }
    unsigned long long inner_from = 0ull;
    unsigned long long inner_to = 0ull;
    cli_text_trimmed(label + from, to - from, &inner_from, &inner_to);
    const char *const shown = label + from + inner_from;
    const unsigned long long shown_size = inner_to - inner_from;
    unsigned long long name_size = 0ull;
    const char *name = shapes_member(json, one, "filterText", &name_size);
    if (name == NULL)
    {
        name = shown;
        while ((name_size < shown_size) && (shown[name_size] != '(') && (shown[name_size] != ' '))
        {
            name_size += 1ull;
        }
    }
    unsigned long long insert_size = 0ull;
    const char *insert =
        cli_json_string(json, cli_json_member(json, cli_json_member(json, one, "textEdit"), "newText"), &insert_size);
    insert = (insert != NULL) ? insert : shapes_member(json, one, "insertText", &insert_size);
    if (insert == NULL)
    {
        insert = name;
        insert_size = name_size;
    }
    static char s_detail[CLI_COMPLETIONS_TEXT];
    CliLine detail = {s_detail, sizeof(s_detail), 0ull};
    unsigned long long said_size = 0ull;
    const char *const said = shapes_member(json, one, "detail", &said_size);
    if (said != NULL)
    {
        cli_line_counted(&detail, said, said_size);
        cli_line_character(&detail, ' ');
    }
    cli_line_counted(&detail, shown, shown_size);
    const unsigned long long detail_size = cli_line_fits(&detail) ? detail.at : 0ull;
    unsigned long long detail_from = 0ull;
    unsigned long long detail_to = 0ull;
    cli_text_trimmed(s_detail, detail_size, &detail_from, &detail_to);
    const char *const kind = shapes_kind_of(cli_servers_whole(json, cli_json_member(json, one, "kind"), 1ull));
    (void)cli_completion_put(items, name, name_size, kind, s_detail + detail_from, detail_to - detail_from, insert,
                             insert_size, 0);
}

// a string member of `one` written as a JSON string, empty where it has none
static void shapes_string_written(const CliJson *json, unsigned int one, const char *name, CliLine *out)
{
    unsigned long long size = 0ull;
    const char *const text = shapes_member(json, one, name, &size);
    cli_json_quoted(out, (text != NULL) ? text : "", size);
}

void cli_servers_diagnostic_of(const CliJson *json, unsigned int one, CliLine *out)
{
    const unsigned int range = cli_json_member(json, one, "range");
    cli_line_text(out, "{\"from\":");
    cli_text_place_written(cli_servers_place(json, cli_json_member(json, range, "start")), out);
    cli_line_text(out, ",\"to\":");
    cli_text_place_written(cli_servers_place(json, cli_json_member(json, range, "end")), out);
    cli_line_text(out, ",\"severity\":");
    // the severity is cut to its low byte, as the protocol's 1 to 4 always fits
    cli_line_decimal(out, cli_servers_whole(json, cli_json_member(json, one, "severity"), 1ull) & 0xffull, 1u);
    const unsigned int message = cli_json_member(json, one, "message");
    unsigned long long size = 0ull;
    const char *text = cli_json_string(json, message, &size);
    text = (text != NULL) ? text : shapes_member(json, message, "value", &size);
    cli_line_text(out, ",\"message\":");
    cli_json_quoted(out, (text != NULL) ? text : "", size);
    unsigned long long kind_size = 0ull;
    const char *const kind = shapes_member(json, message, "kind", &kind_size);
    const int markdown = (kind != NULL) && (kind_size == 8ull) &&
                         (memor.cmp(&(MemoriaCfg){.src = kind, .other = "markdown", .bytes = 8u}) == 0);
    cli_line_text(out, markdown ? ",\"markdown\":true,\"source\":" : ",\"markdown\":false,\"source\":");
    shapes_string_written(json, one, "source", out);
    cli_line_text(out, ",\"code\":");
    const unsigned int code = cli_json_member(json, one, "code");
    if (cli_json_is(json, code, CFG_JSON_NUMBER))
    {
        cli_json_quoted(out, json->text + json->node[code].text, json->node[code].length);
    }
    else
    {
        shapes_string_written(json, one, "code", out);
    }
    unsigned long long href_size = 0ull;
    const char *const href = shapes_member(json, cli_json_member(json, one, "codeDescription"), "href", &href_size);
    const int secure = (href != NULL) && cli_text_starts(href, href_size, "https://");
    cli_line_text(out, ",\"href\":");
    cli_json_quoted(out, secure ? href : "", secure ? href_size : 0ull);
    cli_line_character(out, '}');
}

int cli_servers_found_of(const CliJson *json, unsigned int location, const char *tree, CliLine *out)
{
    unsigned long long length = 0ull;
    const char *uri = shapes_member(json, location, "uri", &length);
    uri = (uri != NULL) ? uri : shapes_member(json, location, "targetUri", &length);
    char path[CLI_PATH_LONGEST];
    if ((uri == NULL) || !cli_lsp_path_of(uri, length, path))
    {
        return 0;
    }
    const unsigned int range = cli_json_member(json, location, "range");
    const unsigned int span =
        cli_json_is(json, range, CFG_JSON_OBJECT) ? range : cli_json_member(json, location, "targetSelectionRange");
    const CliTextPlace at = cli_servers_place(json, cli_json_member(json, span, "start"));
    cli_line_text(out, "{\"path\":");
    cli_servers_path_written(tree, path, out);
    cli_line_text(out, ",\"line\":");
    cli_line_decimal(out, at.line, 1u);
    cli_line_text(out, ",\"col\":");
    cli_line_decimal(out, at.col, 1u);
    cli_line_character(out, '}');
    return 1;
}
