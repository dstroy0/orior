// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_array.c: an array checked: how many items it holds against `minItems` and `maxItems`; each item once where
// `uniqueItems` asks; each item against its place in `prefixItems`, or in draft 7's array `items`, and those past them
// against `items` or `additionalItems`, `false` allowing none; and how many fit `contains`, against `minContains`, one
// where it gives none, and `maxContains`
#include "schema_internal.h"

// a finding over an array's opening bracket: `before`, its count, `middle`, the bound and `after`
static void array_said(CliSchemaCheck *check, unsigned long long from, const char *before, unsigned long long count,
                       const char *middle, unsigned long long bound, const char *after, const char *code)
{
    CliLine said = cli_build_said(check->found);
    cli_line_text(&said, before);
    cli_line_decimal(&said, count, 1u);
    cli_line_text(&said, middle);
    cli_line_decimal(&said, bound, 1u);
    cli_line_text(&said, after);
    (void)cli_build_finding(check->found, &said, check->lines, from, from + 1u, CLI_BUILD_WARNING, code);
}

// each item that stands in the array above it, where the schema asks for each once
static void array_unique(CliSchemaCheck *check, unsigned int node)
{
    const CliSchemaRead *const read = check->read;
    for (unsigned int item = read->node[node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        for (unsigned int earlier = read->node[node].first; earlier != item; earlier = read->node[earlier].next)
        {
            if (cli_schema_same_values(read, earlier, item))
            {
                CliLine said = cli_build_said(check->found);
                cli_line_text(&said, "This item is in the array above it, where the schema asks for each once");
                (void)cli_build_finding(check->found, &said, check->lines, read->node[item].from, read->node[item].to,
                                        CLI_BUILD_WARNING, "schema-unique");
                break;
            }
        }
    }
}

// each item checked against its place in the tuple, and those past it against the rest
static void array_items(CliSchemaCheck *check, unsigned int node, unsigned int map, const char *base,
                        unsigned int depth)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaRead *const read = check->read;
    const unsigned int prefix = cli_schema_get(json, map, "prefixItems");
    const unsigned int items = cli_schema_get(json, map, "items");
    unsigned int tuple = CLI_JSON_NONE;
    if ((prefix != CLI_JSON_NONE) && cli_json_is(json, prefix, CFG_JSON_ARRAY))
    {
        tuple = prefix;
    }
    else if ((prefix == CLI_JSON_NONE) && (items != CLI_JSON_NONE) && cli_json_is(json, items, CFG_JSON_ARRAY))
    {
        tuple = items;
    }
    unsigned int rest = items;
    if (prefix == CLI_JSON_NONE)
    {
        rest = (tuple != CLI_JSON_NONE) ? cli_schema_get(json, map, "additionalItems") : items;
    }
    unsigned int place = (tuple != CLI_JSON_NONE) ? json->node[tuple].first : CLI_JSON_NONE;
    const CliSchemaKey none = {0, 0ull, 0ull};
    for (unsigned int item = read->node[node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        if (place != CLI_JSON_NONE)
        {
            cli_schema_node(check, item, place, base, none, depth + 1u);
            place = json->node[place].next;
            continue;
        }
        if (rest == CLI_JSON_NONE)
        {
            continue;
        }
        if (cli_schema_false(json, rest))
        {
            CliLine said = cli_build_said(check->found);
            cli_line_text(&said, "The schema allows ");
            cli_line_decimal(&said, (tuple != CLI_JSON_NONE) ? json->node[tuple].count : 0u, 1u);
            cli_line_text(&said, " items here, and no more");
            (void)cli_build_finding(check->found, &said, check->lines, read->node[item].from, read->node[item].to,
                                    CLI_BUILD_WARNING, "schema-items");
            continue;
        }
        cli_schema_node(check, item, rest, base, none, depth + 1u);
    }
}

void cli_schema_array_checked(CliSchemaCheck *check, unsigned int node, unsigned int map, const char *base,
                              unsigned int depth)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaRead *const read = check->read;
    const unsigned long long from = read->node[node].from;
    const unsigned long long count = read->node[node].count;
    unsigned long long bound = 0ull;
    if (cli_schema_u64(json, cli_schema_get(json, map, "minItems"), &bound) && (count < bound))
    {
        array_said(check, from, "This array holds ", count, ", fewer than the ", bound, " items the schema asks for",
                   "schema-items");
    }
    if (cli_schema_u64(json, cli_schema_get(json, map, "maxItems"), &bound) && (count > bound))
    {
        array_said(check, from, "This array holds ", count, ", more than the ", bound, " items the schema allows",
                   "schema-items");
    }
    if (cli_schema_true(json, cli_schema_get(json, map, "uniqueItems")))
    {
        array_unique(check, node);
    }
    array_items(check, node, map, base, depth);
    const unsigned int contains = cli_schema_get(json, map, "contains");
    if (contains == CLI_JSON_NONE)
    {
        return;
    }
    unsigned long long held = 0ull;
    for (unsigned int item = read->node[node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        held += cli_schema_fits(check, item, contains, base, depth) ? 1u : 0u;
    }
    unsigned long long least = 1ull;
    (void)cli_schema_u64(json, cli_schema_get(json, map, "minContains"), &least);
    if (held < least)
    {
        array_said(check, from, "This array holds ", held,
                   " of the items the schema's `contains` asks for, fewer than ", least, "", "schema-contains");
    }
    if (cli_schema_u64(json, cli_schema_get(json, map, "maxContains"), &bound) && (held > bound))
    {
        array_said(check, from, "This array holds ", held, " of the items the schema's `contains` asks for, more than ",
                   bound, "", "schema-contains");
    }
}
