// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_offers.c: what a server offers written in orior's own shapes: a call's signature, each parameter by its
// span in the signature's text in UTF-16 units, and a quick fix or a refactoring with the server's own form of it,
// which is given back to carry it out; and whether a span touches another
#include "servers_internal.h"

// the most parameters of a signature written
#define CLI_SERVERS_PARAMS 256u

// what a server says of a signature or a parameter, from either of the shapes the protocol allows, its letters as
// they are
static void offers_documentation_of(const CliJson *json, unsigned int value, CliLine *out)
{
    unsigned long long size = 0ull;
    const char *text = cli_json_string(json, value, &size);
    text = (text != NULL) ? text : cli_json_string(json, cli_json_member(json, value, "value"), &size);
    if (text != NULL)
    {
        cli_line_counted(out, text, size);
    }
}

// the span of the parameter `param` in the signature's text `label`, `size` bytes, into `from` and `to`: 1, or 0
// where the parameter's text is not in it or its span is not whole
static int offers_param_span(const CliJson *json, unsigned int param, const char *label, unsigned long long size,
                             unsigned long long *from, unsigned long long *to)
{
    const unsigned int named = cli_json_member(json, param, "label");
    unsigned long long part_size = 0ull;
    const char *const part = cli_json_string(json, named, &part_size);
    if (part != NULL)
    {
        unsigned long long at = 0ull;
        if (!cli_text_find(label, size, part, part_size, &at))
        {
            return 0;
        }
        *from = cli_text_units(label, at);
        *to = *from + cli_text_units(part, part_size);
        return 1;
    }
    const unsigned int first = cli_json_item(json, named, 0u);
    const unsigned int second = cli_json_item(json, named, 1u);
    const unsigned long long none = ~0ull;
    *from = cli_servers_whole(json, first, none);
    *to = cli_servers_whole(json, second, none);
    // each end is cut to its low word, as the protocol's unsigned integer is read
    *from &= 0xffffffffull;
    *to &= 0xffffffffull;
    return cli_json_is(json, named, CFG_JSON_ARRAY) && (cli_servers_whole(json, first, none) != none) &&
           (cli_servers_whole(json, second, none) != none);
}

void cli_servers_signature_of(const CliJson *json, unsigned int help, CliLine *out)
{
    const unsigned int all = cli_json_member(json, help, "signatures");
    const unsigned long long chosen = cli_servers_whole(json, cli_json_member(json, help, "activeSignature"), 0ull);
    unsigned int one = (chosen < 0xffffffffull) ? cli_json_item(json, all, (unsigned int)chosen) : CLI_JSON_NONE;
    one = (one != CLI_JSON_NONE) ? one : cli_json_item(json, all, 0u);
    unsigned long long size = 0ull;
    const char *const label = cli_json_string(json, cli_json_member(json, one, "label"), &size);
    if (label == NULL)
    {
        cli_line_text(out, "null");
        return;
    }
    cli_line_text(out, "{\"label\":");
    cli_json_quoted(out, label, size);
    cli_line_text(out, ",\"params\":[");
    unsigned long long params = 0ull;
    const unsigned int parameters = cli_json_member(json, one, "parameters");
    for (unsigned int param = cli_json_is(json, parameters, CFG_JSON_ARRAY) ? json->node[parameters].first
                                                                            : CLI_JSON_NONE;
         (param != CLI_JSON_NONE) && (params < CLI_SERVERS_PARAMS); param = json->node[param].next)
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        if (offers_param_span(json, param, label, size, &from, &to))
        {
            cli_line_text(out, (params > 0ull) ? ",[" : "[");
            cli_line_decimal(out, from, 1u);
            cli_line_character(out, ',');
            cli_line_decimal(out, to, 1u);
            cli_line_character(out, ']');
            params += 1ull;
        }
    }
    const unsigned long long none = ~0ull;
    unsigned long long active = cli_servers_whole(json, cli_json_member(json, one, "activeParameter"), none);
    active = (active != none) ? active : cli_servers_whole(json, cli_json_member(json, help, "activeParameter"), none);
    cli_line_text(out, "],\"active\":");
    if (active < params)
    {
        cli_line_decimal(out, active, 1u);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"doc\":\"");
    static char s_doc[1u << 16u];
    CliLine doc = {s_doc, sizeof(s_doc), 0ull};
    offers_documentation_of(json, cli_json_member(json, one, "documentation"), &doc);
    cli_json_escaped_lossy(out, (const unsigned char *)s_doc, cli_line_fits(&doc) ? doc.at : 0ull);
    cli_line_text(out, "\",\"count\":");
    cli_line_decimal(out, cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].count : 0u, 1u);
    cli_line_character(out, '}');
}

int cli_servers_action_of(const CliJson *json, unsigned int one, CliLine *out)
{
    unsigned long long size = 0ull;
    const char *const title = cli_json_string(json, cli_json_member(json, one, "title"), &size);
    if (title == NULL)
    {
        return 0;
    }
    cli_line_text(out, "{\"title\":");
    cli_json_quoted(out, title, size);
    unsigned long long kind_size = 0ull;
    const char *const kind = cli_json_string(json, cli_json_member(json, one, "kind"), &kind_size);
    cli_line_text(out, ",\"kind\":");
    cli_json_quoted(out, (kind != NULL) ? kind : "", kind_size);
    const int preferred = cli_json_is(json, cli_json_member(json, one, "isPreferred"), CFG_JSON_TRUE);
    cli_line_text(out, preferred ? ",\"preferred\":true,\"disabled\":" : ",\"preferred\":false,\"disabled\":");
    unsigned long long reason_size = 0ull;
    const char *const reason =
        cli_json_string(json, cli_json_member(json, cli_json_member(json, one, "disabled"), "reason"), &reason_size);
    if (reason != NULL)
    {
        cli_json_quoted(out, reason, reason_size);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"raw\":");
    cli_json_write(json, one, out, 0u, 0);
    cli_line_character(out, '}');
    return 1;
}

// whether the place `one` stands before `other`
static int offers_before(CliTextPlace one, CliTextPlace other)
{
    return (one.line < other.line) || ((one.line == other.line) && (one.col < other.col));
}

int cli_servers_overlaps(CliTextPlace from, CliTextPlace to, const CliJson *json, unsigned int range)
{
    const CliTextPlace start = cli_servers_place(json, cli_json_member(json, range, "start"));
    const CliTextPlace end = cli_servers_place(json, cli_json_member(json, range, "end"));
    return !offers_before(end, from) && !offers_before(to, start);
}
