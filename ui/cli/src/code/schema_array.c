// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_array.c: an array checked: how many items it contains against `minItems` and `maxItems`; each item once where
// `uniqueItems` requires; each item against its place in `prefixItems`, or in draft 7's array `items`, and those past
// them against `items` or `additionalItems`, `false` allowing none; and how many fit `contains`, against `minContains`,
// one where it gives none, and `maxContains`
#include "schema_internal.h"

// a finding over an array's opening bracket: `before`, its count, `middle`, the bound and `after`
static void array_finding_put(CliSchemaCheck *check, unsigned long long from, const char *before,
                              unsigned long long count, const char *middle, unsigned long long bound, const char *after,
                              const char *code)
{
    VerbaLinea msg = cli_build_msg_line(check->found);
    mmgr_line_put(&msg, before);
    mmgr_line_decimal(&msg, count, 1u);
    mmgr_line_put(&msg, middle);
    mmgr_line_decimal(&msg, bound, 1u);
    mmgr_line_put(&msg, after);
    (void)cli_build_finding(check->found, &msg, check->lines, from, from + 1u, CLI_BUILD_WARNING, code);
}

// each item that stands in the array above it, where the schema requires each once
static void array_unique(CliSchemaCheck *check, unsigned int node)
{
    const CliSchemaRead *const read = check->read;
    for (unsigned int item = read->node[node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        for (unsigned int earlier = read->node[node].first; earlier != item; earlier = read->node[earlier].next)
        {
            if (cli_schema_same_values(read, earlier, item))
            {
                VerbaLinea msg = cli_build_msg_line(check->found);
                mmgr_line_put(&msg, "This item is in the array above it, where the schema requires each once");
                (void)cli_build_finding(check->found, &msg, check->lines, read->node[item].from, read->node[item].to,
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
            VerbaLinea msg = cli_build_msg_line(check->found);
            mmgr_line_put(&msg, "The schema allows ");
            mmgr_line_decimal(&msg, (tuple != CLI_JSON_NONE) ? json->node[tuple].count : 0u, 1u);
            mmgr_line_put(&msg, " items here, and no more");
            (void)cli_build_finding(check->found, &msg, check->lines, read->node[item].from, read->node[item].to,
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
        array_finding_put(check, from, "This array contains ", count, ", fewer than the ", bound,
                          " items the schema requires", "schema-items");
    }
    if (cli_schema_u64(json, cli_schema_get(json, map, "maxItems"), &bound) && (count > bound))
    {
        array_finding_put(check, from, "This array contains ", count, ", more than the ", bound,
                          " items the schema allows", "schema-items");
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
    unsigned long long matched = 0ull;
    for (unsigned int item = read->node[node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        matched += cli_schema_fits(check, item, contains, base, depth) ? 1u : 0u;
    }
    unsigned long long least = 1ull;
    (void)cli_schema_u64(json, cli_schema_get(json, map, "minContains"), &least);
    if (matched < least)
    {
        array_finding_put(check, from, "This array contains ", matched,
                          " of the items the schema's `contains` requires, fewer than ", least, "", "schema-contains");
    }
    if (cli_schema_u64(json, cli_schema_get(json, map, "maxContains"), &bound) && (matched > bound))
    {
        array_finding_put(check, from, "This array contains ", matched,
                          " of the items the schema's `contains` requires, more than ", bound, "", "schema-contains");
    }
}
