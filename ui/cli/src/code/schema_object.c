// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_object.c: an object checked: how many members it holds against `minProperties` and `maxProperties`; the keys
// `required` asks for; what `dependencies`, `dependentRequired` and `dependentSchemas` ask of a key it holds; each key
// against `propertyNames`; and each member's value against `properties` and `patternProperties`, and where neither
// gives it, against `additionalProperties`, `false` allowing none and offering the keys near it. A key's own value is
// checked as a string that stands where the key does
#include "schema_internal.h"

// whether the object holds the key `name`
static int object_has(const CliSchemaRead *read, unsigned int node, const char *name, unsigned long long size)
{
    for (unsigned int at = read->node[node].first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        if ((member->key_size == size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = cli_schema_text(read, member->key_at), .other = name, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the names of `names`, a schema's array, the object does not hold, each between backticks, written into `out`: their
// count
static unsigned int object_missing(const CliSchemaCheck *check, unsigned int node, unsigned int names, CliLine *out)
{
    const CliJson *const json = &check->store->json;
    unsigned int count = 0u;
    for (unsigned int one = json->node[names].first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long size = 0ull;
        const char *const name = cli_json_string(json, one, &size);
        if ((name == NULL) || object_has(check->read, node, name, size))
        {
            continue;
        }
        cli_line_text(out, (count > 0u) ? ", `" : "`");
        cli_line_counted(out, name, size);
        cli_line_character(out, '`');
        count += 1u;
    }
    return count;
}

// a count of members past a bound said over `from` to `to`
static void object_count_said(CliSchemaCheck *check, unsigned long long from, unsigned long long to,
                              unsigned long long count, const char *middle, unsigned long long bound, const char *after)
{
    CliLine said = cli_build_said(check->found);
    cli_line_text(&said, "This object holds ");
    cli_line_decimal(&said, count, 1u);
    cli_line_text(&said, middle);
    cli_line_decimal(&said, bound, 1u);
    cli_line_text(&said, after);
    (void)cli_build_finding(check->found, &said, check->lines, from, to, CLI_BUILD_WARNING, "schema-properties");
}

// what the dependency keywords ask of each key the object holds
static void object_dependent(CliSchemaCheck *check, unsigned int node, unsigned int map, const char *base,
                             CliSchemaKey key, unsigned int depth)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaRead *const read = check->read;
    static const char *const s_words[3] = {"dependencies", "dependentRequired", "dependentSchemas"};
    for (unsigned int word = 0u; word < 3u; word += 1u)
    {
        const unsigned int all = cli_schema_get(json, map, s_words[word]);
        for (unsigned int needs = cli_json_is(json, all, CFG_JSON_OBJECT) ? json->node[all].first : CLI_JSON_NONE;
             needs != CLI_JSON_NONE; needs = json->node[needs].next)
        {
            const char *const name = json->text + json->node[needs].key;
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
            unsigned int member = CLI_SCHEMA_NONE;
            for (unsigned int at = read->node[node].first; (member == CLI_SCHEMA_NONE) && (at != CLI_SCHEMA_NONE);
                 at = read->member[at].next)
            {
                const CliSchemaMember *const one = &read->member[at];
                member = ((one->key_size == size) &&
                          (memor.cmp(&(MemoriaCfg){
                               .src = cli_schema_text(read, one->key_at), .other = name, .bytes = (size_t)size}) == 0))
                             ? at
                             : member;
            }
            if (member == CLI_SCHEMA_NONE)
            {
                continue;
            }
            const CliSchemaMember *const one = &read->member[member];
            if (!cli_json_is(json, needs, CFG_JSON_ARRAY))
            {
                cli_schema_node(check, node, needs, base, key, depth + 1u);
                continue;
            }
            CliLine said = cli_build_said(check->found);
            cli_line_counted(&said, check->text + one->from, one->to - one->from);
            cli_line_text(&said, " asks for ");
            if (object_missing(check, node, needs, &said) > 0u)
            {
                cli_line_text(&said, " beside it, which this object has not");
                (void)cli_build_finding(check->found, &said, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                        "schema-required");
            }
        }
    }
}

// a member no schema gives the object: a finding offering the keys near it where `additionalProperties` is `false`,
// and otherwise its value checked against it
static void object_additional(CliSchemaCheck *check, unsigned int member, unsigned int properties,
                              unsigned int additional, const char *base, unsigned int depth)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaMember *const one = &check->read->member[member];
    if (additional == CLI_JSON_NONE)
    {
        return;
    }
    if (!cli_schema_false(json, additional))
    {
        if (one->value != CLI_SCHEMA_NONE)
        {
            const CliSchemaKey key = {1, one->from, one->to};
            cli_schema_node(check, one->value, additional, base, key, depth + 1u);
        }
        return;
    }
    CliLine said = cli_build_said(check->found);
    cli_line_counted(&said, check->text + one->from, one->to - one->from);
    cli_line_text(&said, " is no property the schema gives this object");
    const unsigned int finding =
        cli_build_finding(check->found, &said, check->lines, one->from, one->to, CLI_BUILD_WARNING, "schema-property");
    CliBuildNear near;
    cli_build_near_begun(&near, cli_schema_text(check->read, one->key_at), one->key_size);
    for (unsigned int at = cli_json_is(json, properties, CFG_JSON_OBJECT) ? json->node[properties].first
                                                                          : CLI_JSON_NONE;
         at != CLI_JSON_NONE; at = json->node[at].next)
    {
        const char *const name = json->text + json->node[at].key;
        cli_build_near_offered(&near, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        CliLine title = cli_build_said(check->found);
        cli_line_text(&title, "Change to \"");
        cli_line_counted(&title, near.near[at], near.near_size[at]);
        cli_line_character(&title, '"');
        static char s_quoted[1u << 13u];
        CliLine quoted = {s_quoted, sizeof(s_quoted), 0ull};
        cli_json_quoted(&quoted, near.near[at], near.near_size[at]);
        cli_build_edit_fix(check->found, finding, &title, check->lines, one->from, one->to, s_quoted, quoted.at);
    }
}

// a key checked against `propertyNames`, as a string standing where the key does
static void object_name(CliSchemaCheck *check, unsigned int member, unsigned int names, const char *base,
                        unsigned int depth)
{
    CliSchemaRead *const read = check->read;
    if (read->nodes == read->nodes_most)
    {
        return;
    }
    const CliSchemaMember *const one = &read->member[member];
    const unsigned int at = read->nodes;
    read->node[at] = (CliSchemaNode){
        CLI_SCHEMA_TEXT, 0, 0.0, one->from, one->to, one->key_at, one->key_size, CLI_SCHEMA_NONE, 0u, CLI_SCHEMA_NONE};
    read->nodes += 1u;
    const CliSchemaKey none = {0, 0ull, 0ull};
    cli_schema_node(check, at, names, base, none, depth + 1u);
    read->nodes = at;
}

void cli_schema_object_checked(CliSchemaCheck *check, unsigned int node, unsigned int map, const char *base,
                               CliSchemaKey key, unsigned int depth)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaRead *const read = check->read;
    const unsigned long long from = key.held ? key.from : read->node[node].from;
    const unsigned long long to = key.held ? key.to : read->node[node].from + 1u;
    const unsigned long long count = read->node[node].count;
    unsigned long long bound = 0ull;
    if (cli_schema_u64(json, cli_schema_get(json, map, "minProperties"), &bound) && (count < bound))
    {
        object_count_said(check, from, to, count, ", fewer than the ", bound, " properties the schema asks for");
    }
    if (cli_schema_u64(json, cli_schema_get(json, map, "maxProperties"), &bound) && (count > bound))
    {
        object_count_said(check, from, to, count, ", more than the ", bound, " properties the schema allows");
    }
    const unsigned int required = cli_schema_get(json, map, "required");
    if ((required != CLI_JSON_NONE) && cli_json_is(json, required, CFG_JSON_ARRAY))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "This object has no ");
        if (object_missing(check, node, required, &said) > 0u)
        {
            cli_line_text(&said, ", which the schema asks for");
            (void)cli_build_finding(check->found, &said, check->lines, from, to, CLI_BUILD_WARNING, "schema-required");
        }
    }
    object_dependent(check, node, map, base, key, depth);
    const unsigned int properties = cli_schema_get(json, map, "properties");
    const unsigned int patterns = cli_schema_get(json, map, "patternProperties");
    const unsigned int additional = cli_schema_get(json, map, "additionalProperties");
    const unsigned int names = cli_schema_get(json, map, "propertyNames");
    for (unsigned int member = read->node[node].first; member != CLI_SCHEMA_NONE; member = read->member[member].next)
    {
        const CliSchemaMember *const one = &read->member[member];
        const char *const name = cli_schema_text(read, one->key_at);
        const CliSchemaKey here = {1, one->from, one->to};
        if (names != CLI_JSON_NONE)
        {
            object_name(check, member, names, base, depth);
        }
        int matched = 0;
        const unsigned int schema = cli_schema_get(json, properties, name);
        if (schema != CLI_JSON_NONE)
        {
            matched = 1;
            if (one->value != CLI_SCHEMA_NONE)
            {
                cli_schema_node(check, one->value, schema, base, here, depth + 1u);
            }
        }
        for (unsigned int at = cli_json_is(json, patterns, CFG_JSON_OBJECT) ? json->node[patterns].first
                                                                            : CLI_JSON_NONE;
             at != CLI_JSON_NONE; at = json->node[at].next)
        {
            const char *const pattern = json->text + json->node[at].key;
            if (cli_schema_matches(pattern, cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL}), name,
                                   one->key_size) == 1)
            {
                matched = 1;
                if (one->value != CLI_SCHEMA_NONE)
                {
                    cli_schema_node(check, one->value, at, base, here, depth + 1u);
                }
            }
        }
        if (!matched)
        {
            object_additional(check, member, properties, additional, base, depth);
        }
    }
}
