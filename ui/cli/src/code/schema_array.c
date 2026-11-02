// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_array.c: an array checked: how many items it contains against `minItems` and `maxItems`; each item once where
// `uniqueItems` requires; each item against its place in `prefixItems`, or in draft 7's array `items`, and those past
// them against `items` or `additionalItems`, `false` allowing none; and how many fit `contains`, against `minContains`,
// one where it gives none, and `maxContains`
#include "schema_internal.h"

typedef struct
{
    CliSchemaCheck *check;
    unsigned long long from;
    const char *before;
    unsigned long long count;
    const char *middle;
    unsigned long long bound;
    const char *after;
    const char *code;
    unsigned int node;
    unsigned int map;
    const char *base;
    unsigned int depth;
} SchemaArrayCtx;

// a finding over an array's opening bracket: `before`, its count, `middle`, the bound and `after`
EMBED_INLINE void array_finding_put(const SchemaArrayCtx *args)
{
    unsigned long long from = args->from;
    const char *code = args->code;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_put(&msg, args->before);
    mmgr_line_decimal(&msg, args->count, 1u);
    mmgr_line_put(&msg, args->middle);
    mmgr_line_decimal(&msg, args->bound, 1u);
    mmgr_line_put(&msg, args->after);
    (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                     .lines = args->check->lines, .from = from, .to = from + 1u, .severity = CLI_BUILD_WARNING,
                     .code = code);
}

// each item that stands in the array above it, where the schema requires each once
EMBED_INLINE void array_unique(const SchemaArrayCtx *args)
{
    const CliSchemaRead *const read = args->check->read;
    for (unsigned int item = read->node[args->node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        for (unsigned int earlier = read->node[args->node].first; earlier != item; earlier = read->node[earlier].next)
        {
            if (EMBED_CALL(cli_schema_read_one_internal.same_values, CliSchemaReadOneCtx, .read = read, .one = earlier,
                           .other = item))
            {
                VerbaLinea msg =
                    EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
                mmgr_line_put(&msg, "This item is in the array above it, where the schema requires each once");
                (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                                 .message = &msg, .lines = args->check->lines, .from = read->node[item].from,
                                 .to = read->node[item].to, .severity = CLI_BUILD_WARNING, .code = "schema-unique");
                break;
            }
        }
    }
}

// each item checked against its place in the tuple, and those past it against the rest
EMBED_INLINE void array_items(const SchemaArrayCtx *args)
{
    CliSchemaCheck *check = args->check;
    unsigned int node = args->node;
    unsigned int map = args->map;
    const char *base = args->base;
    unsigned int depth = args->depth;
    const CliJson *const json = &check->store->json;
    const CliSchemaRead *const read = check->read;
    const unsigned int prefix =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = map, .name = "prefixItems");
    const unsigned int items =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = map, .name = "items");
    unsigned int tuple = CLI_JSON_NONE;
    if ((prefix != CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = json, .node = prefix, .kind = CFG_JSON_ARRAY))
    {
        tuple = prefix;
    }
    else if ((prefix == CLI_JSON_NONE) && (items != CLI_JSON_NONE) &&
             EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = items,
                        .kind = CFG_JSON_ARRAY))
    {
        tuple = items;
    }
    unsigned int rest = items;
    if (prefix == CLI_JSON_NONE)
    {
        rest = (tuple != CLI_JSON_NONE) ? EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                     .map = map, .name = "additionalItems")
                                        : items;
    }
    unsigned int place = (tuple != CLI_JSON_NONE) ? json->node[tuple].first : CLI_JSON_NONE;
    const CliSchemaKey none = {0, 0ull, 0ull};
    for (unsigned int item = read->node[node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        if (place != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = check, .node = item, .schema = place,
                       .base = base, .key_schema_key = none, .depth = depth + 1u);
            place = json->node[place].next;
            continue;
        }
        if (rest == CLI_JSON_NONE)
        {
            continue;
        }
        if (EMBED_CALL(cli_schema_json_node_internal.schema_false, CliSchemaJsonNodeCtx, .json = json, .node = rest))
        {
            VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
            mmgr_line_put(&msg, "The schema allows ");
            mmgr_line_decimal(&msg, (tuple != CLI_JSON_NONE) ? json->node[tuple].count : 0u, 1u);
            mmgr_line_put(&msg, " items here, and no more");
            (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                             .lines = check->lines, .from = read->node[item].from, .to = read->node[item].to,
                             .severity = CLI_BUILD_WARNING, .code = "schema-items");
            continue;
        }
        EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = check, .node = item, .schema = rest,
                   .base = base, .key_schema_key = none, .depth = depth + 1u);
    }
}

void cli_schema_array_check(const CliSchemaBaseMapCtx *args)
{
    const CliJson *const json = &args->check->store->json;
    const CliSchemaRead *const read = args->check->read;
    const unsigned long long from = read->node[args->node].from;
    const unsigned long long count = read->node[args->node].count;
    unsigned long long bound = 0ull;
    if (EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "minItems"),
                   .value_u64s = &bound) &&
        (count < bound))
    {
        EMBED_CALL(array_finding_put, SchemaArrayCtx, .check = args->check, .from = from,
                   .before = "This array contains ", .count = count, .middle = ", fewer than the ", .bound = bound,
                   .after = " items the schema requires", .code = "schema-items");
    }
    if (EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "maxItems"),
                   .value_u64s = &bound) &&
        (count > bound))
    {
        EMBED_CALL(array_finding_put, SchemaArrayCtx, .check = args->check, .from = from,
                   .before = "This array contains ", .count = count, .middle = ", more than the ", .bound = bound,
                   .after = " items the schema allows", .code = "schema-items");
    }
    if (EMBED_CALL(cli_schema_json_node_internal.schema_true, CliSchemaJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "uniqueItems")))
    {
        EMBED_CALL(array_unique, SchemaArrayCtx, .check = args->check, .node = args->node);
    }
    EMBED_CALL(array_items, SchemaArrayCtx, .check = args->check, .node = args->node, .map = args->map,
               .base = args->base, .depth = args->depth);
    const unsigned int contains =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map, .name = "contains");
    if (contains == CLI_JSON_NONE)
    {
        return;
    }
    unsigned long long matched = 0ull;
    for (unsigned int item = read->node[args->node].first; item != CLI_SCHEMA_NONE; item = read->node[item].next)
    {
        matched += EMBED_CALL(cli_schema_base_internal.fits, CliSchemaBaseCtx, .check = args->check, .node = item,
                              .schema = contains, .base = args->base, .depth = args->depth)
                       ? 1u
                       : 0u;
    }
    unsigned long long least = 1ull;
    (void)EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                     .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                        .name = "minContains"),
                     .value_u64s = &least);
    if (matched < least)
    {
        EMBED_CALL(array_finding_put, SchemaArrayCtx, .check = args->check, .from = from,
                   .before = "This array contains ", .count = matched,
                   .middle = " of the items the schema's `contains` requires, fewer than ", .bound = least, .after = "",
                   .code = "schema-contains");
    }
    if (EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "maxContains"),
                   .value_u64s = &bound) &&
        (matched > bound))
    {
        EMBED_CALL(array_finding_put, SchemaArrayCtx, .check = args->check, .from = from,
                   .before = "This array contains ", .count = matched,
                   .middle = " of the items the schema's `contains` requires, more than ", .bound = bound, .after = "",
                   .code = "schema-contains");
    }
}
