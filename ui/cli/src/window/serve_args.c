// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_args.c: a call's arguments, each found by its own name or by the page's, as the page writes names of more than
// one word, `with_errors` as `withErrors`; one of the wrong kind refused as serde refuses it, its name before why
#include "../command_line/cells.h"
#include "serve_internal.h"

void *cli_serve_cell(unsigned long long size, const char *what, CliLine *said)
{
    void *const cell = cli_cell_taken(size, what);
    if (cell == NULL)
    {
        cli_line_text(said, what);
        cli_line_text(said, ": the cells cannot meet it");
    }
    return cell;
}

CliServeFiles *cli_serve_files_listed(const char *root, CliLine *said)
{
    const CliFiles *const files = cli_files_held(root);
    if (files == NULL)
    {
        cli_line_text(said, "the tree's files: the cells cannot meet it");
        return NULL;
    }
    CliServeFiles *const listed = cli_serve_cell(
        sizeof(CliServeFiles) + ((unsigned long long)files->count * sizeof(const char *)), "the tree's files", said);
    if (listed == NULL)
    {
        cli_files_let(files);
        return NULL;
    }
    // the pointers lie in the cell past the list, which the cell's alignment and the list's size keep aligned
    *listed = (CliServeFiles){files, files->count, (const char **)(void *)(listed + 1)};
    for (unsigned int at = 0u; at < listed->count; at += 1u)
    {
        listed->paths[at] = cli_files_at(files, at);
    }
    return listed;
}

void cli_serve_files_given(CliServeFiles *listed)
{
    if (listed != NULL)
    {
        cli_files_let(listed->files);
        cli_cell_given(listed);
    }
}

// the page's name for `key`: each letter after a `_` in upper case, the `_` left out, into `out`, which holds `room`
static void args_camel(const char *key, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    int upper = 0;
    for (const char *at = key; *at != '\0'; at += 1)
    {
        if (*at == '_')
        {
            upper = 1;
            continue;
        }
        const char one = (upper && (*at >= 'a') && (*at <= 'z')) ? (char)(*at - 'a' + 'A') : *at;
        cli_line_character(&line, one);
        upper = 0;
    }
    (void)cli_line_closed(&line);
}

unsigned int cli_serve_arg(const CliServeArgs *args, const char *key)
{
    const unsigned int found = cli_json_member(args->json, args->object, key);
    if (found != CLI_JSON_NONE)
    {
        return found;
    }
    char camel[CLI_NAME_LONGEST];
    args_camel(key, camel, sizeof(camel));
    return cli_json_member(args->json, args->object, camel);
}

// why an argument of the wrong kind is refused, as serde says it: its name, what it is, and what was expected
void cli_serve_refused(const CliServeArgs *args, unsigned int node, const char *key, const char *expected,
                       CliLine *said)
{
    static const char *const s_kinds[7] = {"map", "sequence", "string", "number", "boolean `true`", "boolean `false`",
                                           "null"};
    cli_line_text(said, key);
    cli_line_text(said, ": invalid type: ");
    const CfgJsonKind kind = (node == CLI_JSON_NONE) ? CFG_JSON_NULL : args->json->node[node].kind;
    cli_line_text(said, s_kinds[kind]);
    if (kind == CFG_JSON_STRING)
    {
        cli_line_character(said, ' ');
        cli_json_quoted(said, args->json->text + args->json->node[node].text, args->json->node[node].length);
    }
    else if (kind == CFG_JSON_NUMBER)
    {
        cli_line_character(said, ' ');
        cli_line_counted(said, args->json->text + args->json->node[node].text, args->json->node[node].length);
    }
    cli_line_text(said, ", expected ");
    cli_line_text(said, expected);
}

const char *cli_serve_string(const CliServeArgs *args, const char *key, CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    const char *const text = cli_json_string(args->json, node, NULL);
    if (text == NULL)
    {
        cli_serve_refused(args, node, key, "a string", said);
    }
    return text;
}

const char *cli_serve_string_or(const CliServeArgs *args, const char *key, const char *otherwise)
{
    const char *const text = cli_json_string(args->json, cli_serve_arg(args, key), NULL);
    return (text != NULL) ? text : otherwise;
}

int cli_serve_whole(const CliServeArgs *args, const char *key, unsigned long long *value, CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    const unsigned long long none = ~0ull;
    *value = cli_servers_whole(args->json, node, none);
    if (*value == none)
    {
        cli_serve_refused(args, node, key, "u64", said);
        return 0;
    }
    return 1;
}

unsigned long long cli_serve_whole_or(const CliServeArgs *args, const char *key, unsigned long long otherwise)
{
    return cli_servers_whole(args->json, cli_serve_arg(args, key), otherwise);
}

int cli_serve_signed(const CliServeArgs *args, const char *key, long long *value, CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    if (!cli_json_is(args->json, node, CFG_JSON_NUMBER) || (args->json->node[node].length == 0ull))
    {
        cli_serve_refused(args, node, key, "i64", said);
        return 0;
    }
    const char *const digits = args->json->text + args->json->node[node].text;
    const unsigned long long length = args->json->node[node].length;
    const int below = (digits[0] == '-');
    unsigned long long magnitude = 0ull;
    for (unsigned long long at = below ? 1ull : 0ull; at < length; at += 1ull)
    {
        if ((digits[at] < '0') || (digits[at] > '9') || (magnitude > 922337203685477580ull))
        {
            cli_serve_refused(args, node, key, "i64", said);
            return 0;
        }
        magnitude = (magnitude * 10ull) + (unsigned long long)(digits[at] - '0');
    }
    // the magnitude is at most 2^63 - 1 and is negated within the signed range
    *value = below ? -(long long)magnitude : (long long)magnitude;
    return 1;
}

int cli_serve_bool_or(const CliServeArgs *args, const char *key, int otherwise)
{
    const unsigned int node = cli_serve_arg(args, key);
    if (cli_json_is(args->json, node, CFG_JSON_TRUE))
    {
        return 1;
    }
    return cli_json_is(args->json, node, CFG_JSON_FALSE) ? 0 : otherwise;
}

int cli_serve_bool(const CliServeArgs *args, const char *key, int *value, CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    if (!cli_json_is(args->json, node, CFG_JSON_TRUE) && !cli_json_is(args->json, node, CFG_JSON_FALSE))
    {
        cli_serve_refused(args, node, key, "a boolean", said);
        return 0;
    }
    *value = cli_json_is(args->json, node, CFG_JSON_TRUE);
    return 1;
}

int cli_serve_strings(const CliServeArgs *args, const char *key, const char **items, unsigned int room,
                      unsigned int *count, CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    *count = 0u;
    if (!cli_json_is(args->json, node, CFG_JSON_ARRAY))
    {
        cli_serve_refused(args, node, key, "a sequence", said);
        return 0;
    }
    for (unsigned int item = args->json->node[node].first; item != CLI_JSON_NONE; item = args->json->node[item].next)
    {
        const char *const text = cli_json_string(args->json, item, NULL);
        if (text == NULL)
        {
            cli_serve_refused(args, item, key, "a string", said);
            return 0;
        }
        if (*count < room)
        {
            items[*count] = text;
            *count += 1u;
        }
    }
    return 1;
}

int cli_serve_strings_or(const CliServeArgs *args, const char *key, const char **items, unsigned int room,
                         unsigned int *count, CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    *count = 0u;
    if ((node == CLI_JSON_NONE) || cli_json_is(args->json, node, CFG_JSON_NULL))
    {
        return 1;
    }
    return cli_serve_strings(args, key, items, room, count, said);
}

int cli_serve_tree(char *root, CliLine *said)
{
    return cli_serve_root(root, said);
}

int cli_serve_at(const CliServeArgs *args, char *root, char *full, CliLine *said)
{
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_root(root, said))
    {
        return 0;
    }
    cli_full(root, path, full);
    return 1;
}

int cli_serve_place(const CliServeArgs *args, const char *key, CliTextPlace *place, CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    const CliServeArgs inner = {args->json, node};
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if (!cli_json_is(args->json, node, CFG_JSON_OBJECT))
    {
        cli_serve_refused(args, node, key, "struct Place", said);
        return 0;
    }
    if (!cli_serve_whole(&inner, "line", &line, said) || !cli_serve_whole(&inner, "col", &col, said))
    {
        return 0;
    }
    // a place is cut to its low words, as the protocol counts them
    *place = (CliTextPlace){(unsigned int)line, (unsigned int)col};
    return 1;
}
