// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_object.c: an object checked: how many members it contains against `minProperties` and `maxProperties`; the
// keys `required` requires; what `dependencies`, `dependentRequired` and `dependentSchemas` require of a key it
// contains; each key against `propertyNames`; and each member's value against `properties` and `patternProperties`, and
// where neither gives it, against `additionalProperties`, `false` allowing none and offering the keys near it. A key's
// own value is checked as a string that is where the key does
#include "schema_internal.h"

typedef struct
{
    const CliSchemaRead *read;
    unsigned int node;
    const char *name;
    unsigned long long size;
    const CliSchemaCheck *check_read;
    unsigned int names;
    VerbaLinea *out;
    CliSchemaCheck *check;
    unsigned long long from;
    unsigned long long to;
    unsigned long long count;
    const char *middle;
    unsigned long long bound;
    const char *after;
    unsigned int map;
    const char *base;
    CliSchemaKey key;
    unsigned int depth;
    unsigned int member;
    unsigned int properties;
    unsigned int additional;
} SchemaObjectCtx;

// whether the object contains the key `name`
EMBED_INLINE int object_has(const SchemaObjectCtx *args)
{
    const CliSchemaRead *read = args->read;
    for (unsigned int at = read->node[args->node].first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        if ((member->key_size == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                       .read = read, .at = member->key_at),
                                     .other = args->name,
                                     .bytes = (embed_word)args->size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the names of `names`, a schema's array, the object does not contain, each between backticks, written into `out`:
// their count
EMBED_INLINE unsigned int object_missing(const SchemaObjectCtx *args)
{
    unsigned int node = args->node;
    const CliJson *const json = &args->check_read->store->json;
    unsigned int count = 0u;
    for (unsigned int one = json->node[args->names].first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long size = 0ULL;
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = one, .length_pointer = &size);
        if ((name == NULL) || EMBED_CALL(object_has, SchemaObjectCtx, .read = args->check_read->read, .node = node,
                                         .name = name, .size = size))
        {
            continue;
        }
        mmgr_line_put(args->out, (count > 0u) ? ", `" : "`");
        mmgr_line_put_n(args->out, name, size);
        mmgr_line_ch(args->out, '`');
        count += 1u;
    }
    return count;
}

// a count of members past a bound reported over `from` to `to`
EMBED_INLINE void object_count_finding_append(const SchemaObjectCtx *args)
{
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_put(&msg, "This object contains ");
    mmgr_line_decimal(&msg, args->count, 1u);
    mmgr_line_put(&msg, args->middle);
    mmgr_line_decimal(&msg, args->bound, 1u);
    mmgr_line_put(&msg, args->after);
    (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                     .lines = args->check->lines, .from = from, .to = to, .severity = CLI_BUILD_WARNING,
                     .code = "schema-properties");
}

// what the dependency keywords require of each key the object contains
EMBED_INLINE void object_dependent(const SchemaObjectCtx *args)
{
    CliSchemaCheck *check = args->check;
    unsigned int node = args->node;
    unsigned int map = args->map;
    const char *base = args->base;
    CliSchemaKey key = args->key;
    unsigned int depth = args->depth;
    const CliJson *const json = &check->store->json;
    const CliSchemaRead *const read = check->read;
    static const char *const s_words[3] = {"dependencies", "dependentRequired", "dependentSchemas"};
    for (unsigned int word_text = 0u; word_text < 3u; word_text += 1u)
    {
        const unsigned int all = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = map,
                                            .name = s_words[word_text]);
        for (unsigned int needs = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                             .node = all, .type = CFG_JSON_OBJECT)
                                      ? json->node[all].first
                                      : CLI_JSON_NONE;
             needs != CLI_JSON_NONE; needs = json->node[needs].next)
        {
            const char *const name = json->text + json->node[needs].key;
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
            unsigned int member = CLI_SCHEMA_NONE;
            for (unsigned int at = read->node[node].first; (member == CLI_SCHEMA_NONE) && (at != CLI_SCHEMA_NONE);
                 at = read->member[at].next)
            {
                const CliSchemaMember *const one = &read->member[at];
                member =
                    ((one->key_size == size) &&
                     (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                                .read = read, .at = one->key_at),
                                              .other = name,
                                              .bytes = (embed_word)size}) == 0))
                        ? at
                        : member;
            }
            if (member == CLI_SCHEMA_NONE)
            {
                continue;
            }
            const CliSchemaMember *const one = &read->member[member];
            if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = needs,
                            .type = CFG_JSON_ARRAY))
            {
                EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = check, .node = node,
                           .schema = needs, .base = base, .key_schema_key = key, .depth = depth + 1u);
                continue;
            }
            VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
            mmgr_line_put_n(&msg, check->text + one->from, one->to - one->from);
            mmgr_line_put(&msg, " requires ");
            if (EMBED_CALL(object_missing, SchemaObjectCtx, .check_read = check, .node = node, .names = needs,
                           .out = &msg) > 0u)
            {
                mmgr_line_put(&msg, " beside it, which this object has not");
                (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found,
                                 .message = &msg, .lines = check->lines, .from = one->from, .to = one->to,
                                 .severity = CLI_BUILD_WARNING, .code = "schema-required");
            }
        }
    }
}

// a member no schema gives the object: a finding offering the keys near it where `additionalProperties` is `false`,
// and otherwise its value checked against it
EMBED_INLINE void object_additional(const SchemaObjectCtx *args)
{
    CliSchemaCheck *check = args->check;
    const char *base = args->base;
    unsigned int depth = args->depth;
    const CliJson *const json = &check->store->json;
    const CliSchemaMember *const one = &check->read->member[args->member];
    if (args->additional == CLI_JSON_NONE)
    {
        return;
    }
    if (!EMBED_CALL(cli_schema_json_node_internal.schema_false, CliSchemaJsonNodeCtx, .json = json,
                    .node = args->additional))
    {
        if (one->value != CLI_SCHEMA_NONE)
        {
            const CliSchemaKey key = {1, one->from, one->to};
            EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = check, .node = one->value,
                       .schema = args->additional, .base = base, .key_schema_key = key, .depth = depth + 1u);
        }
        return;
    }
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
    mmgr_line_put_n(&msg, check->text + one->from, one->to - one->from);
    mmgr_line_put(&msg, " is no property the schema gives this object");
    const unsigned int finding = EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found,
                                            .message = &msg, .lines = check->lines, .from = one->from, .to = one->to,
                                            .severity = CLI_BUILD_WARNING, .code = "schema-property");
    CliBuildAdjacent adjacent;
    EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent,
               .name = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = check->read,
                                  .at = one->key_at),
               .size = one->key_size);
    for (unsigned int at = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                      .node = args->properties, .type = CFG_JSON_OBJECT)
                               ? json->node[args->properties].first
                               : CLI_JSON_NONE;
         at != CLI_JSON_NONE; at = json->node[at].next)
    {
        const char *const name = json->text + json->node[at].key;
        EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                   .one = name, .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    }
    for (unsigned int at = 0u; at < adjacent.count; at += 1u)
    {
        VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&title, "Change to \"");
        mmgr_line_put_n(&title, adjacent.adjacent[at], adjacent.adjacent_size[at]);
        mmgr_line_ch(&title, '"');
        static char s_quoted[1u << 13u];
        VerbaLinea quoted = {s_quoted, sizeof(s_quoted), 0ULL};
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &quoted,
                   .text = adjacent.adjacent[at], .length = adjacent.adjacent_size[at]);
        EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = check->found, .finding = finding,
                   .title = &title, .lines = check->lines, .from = one->from, .to = one->to, .text = s_quoted,
                   .size = quoted.at);
    }
}

// a key checked against `propertyNames`, as a string located where the key does
EMBED_INLINE void object_name(const SchemaObjectCtx *args)
{
    CliSchemaCheck *check = args->check;
    const char *base = args->base;
    unsigned int depth = args->depth;
    CliSchemaRead *const read = check->read;
    if (read->nodes == read->nodes_max)
    {
        return;
    }
    const CliSchemaMember *const one = &read->member[args->member];
    const unsigned int at = read->nodes;
    read->node[at] = (CliSchemaNode){
        CLI_SCHEMA_TEXT, 0, 0.0, one->from, one->to, one->key_at, one->key_size, CLI_SCHEMA_NONE, 0u, CLI_SCHEMA_NONE};
    read->nodes += 1u;
    const CliSchemaKey none = {0, 0ULL, 0ULL};
    EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = check, .node = at, .schema = args->names,
               .base = base, .key_schema_key = none, .depth = depth + 1u);
    read->nodes = at;
}

void cli_schema_object_check(const CliSchemaBaseMapCtx *args)
{
    const CliJson *const json = &args->check->store->json;
    const CliSchemaRead *const read = args->check->read;
    const unsigned long long from =
        args->key_schema_key.has_key ? args->key_schema_key.from : read->node[args->node].from;
    const unsigned long long to =
        args->key_schema_key.has_key ? args->key_schema_key.to : read->node[args->node].from + 1u;
    const unsigned long long count = read->node[args->node].count;
    unsigned long long bound = 0ULL;
    if (EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "minProperties"),
                   .value_u64s = &bound) &&
        (count < bound))
    {
        EMBED_CALL(object_count_finding_append, SchemaObjectCtx, .check = args->check, .from = from, .to = to,
                   .count = count, .middle = ", fewer than the ", .bound = bound,
                   .after = " properties the schema requires");
    }
    if (EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "maxProperties"),
                   .value_u64s = &bound) &&
        (count > bound))
    {
        EMBED_CALL(object_count_finding_append, SchemaObjectCtx, .check = args->check, .from = from, .to = to,
                   .count = count, .middle = ", more than the ", .bound = bound,
                   .after = " properties the schema allows");
    }
    const unsigned int required =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map, .name = "required");
    if ((required != CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                  .json_read = json, .node = required, .type = CFG_JSON_ARRAY))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "This object has no ");
        if (EMBED_CALL(object_missing, SchemaObjectCtx, .check_read = args->check, .node = args->node,
                       .names = required, .out = &msg) > 0u)
        {
            mmgr_line_put(&msg, ", which the schema requires");
            (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                             .message = &msg, .lines = args->check->lines, .from = from, .to = to,
                             .severity = CLI_BUILD_WARNING, .code = "schema-required");
        }
    }
    EMBED_CALL(object_dependent, SchemaObjectCtx, .check = args->check, .node = args->node, .map = args->map,
               .base = args->base, .key = args->key_schema_key, .depth = args->depth);
    const unsigned int properties = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                               .map = args->map, .name = "properties");
    const unsigned int patterns = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                             .map = args->map, .name = "patternProperties");
    const unsigned int additional = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                               .map = args->map, .name = "additionalProperties");
    const unsigned int names = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                          .map = args->map, .name = "propertyNames");
    for (unsigned int member = read->node[args->node].first; member != CLI_SCHEMA_NONE;
         member = read->member[member].next)
    {
        const CliSchemaMember *const one = &read->member[member];
        const char *const name =
            EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = read, .at = one->key_at);
        const CliSchemaKey here = {1, one->from, one->to};
        if (names != CLI_JSON_NONE)
        {
            EMBED_CALL(object_name, SchemaObjectCtx, .check = args->check, .member = member, .names = names,
                       .base = args->base, .depth = args->depth);
        }
        int matched = 0;
        const unsigned int schema =
            EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = properties, .name = name);
        if (schema != CLI_JSON_NONE)
        {
            matched = 1;
            if (one->value != CLI_SCHEMA_NONE)
            {
                EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = args->check, .node = one->value,
                           .schema = schema, .base = args->base, .key_schema_key = here, .depth = args->depth + 1u);
            }
        }
        for (unsigned int at = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                          .node = patterns, .type = CFG_JSON_OBJECT)
                                   ? json->node[patterns].first
                                   : CLI_JSON_NONE;
             at != CLI_JSON_NONE; at = json->node[at].next)
        {
            const char *const pattern = json->text + json->node[at].key;
            if (EMBED_CALL(cli_schema_text_internal.matches, CliSchemaTextCtx, .pattern = pattern,
                           .pattern_size = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_POOL_BYTES}),
                           .text = name, .size = one->key_size) != 1)
            {
                continue;
            }
            matched = 1;
            if (one->value != CLI_SCHEMA_NONE)
            {
                EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = args->check, .node = one->value,
                           .schema = at, .base = args->base, .key_schema_key = here, .depth = args->depth + 1u);
            }
        }
        if (!matched)
        {
            EMBED_CALL(object_additional, SchemaObjectCtx, .check = args->check, .member = member,
                       .properties = properties, .additional = additional, .base = args->base, .depth = args->depth);
        }
    }
}
