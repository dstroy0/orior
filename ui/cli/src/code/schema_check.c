// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_check.c: a value checked against a schema: `true` allows it and `false` none; `$id` sets the base its
// references are read from; `$ref` is followed; `deprecated` is noted on the key; `type`, `enum` and `const` are held
// to; the value's own bounds by its kind; `allOf` each, `anyOf` one and `oneOf` exactly one, the nearest choice's
// findings given where none fits; `not`; and `if` with `then` or `else`. A trial's findings are those that are no note.
// Values are checked CLI_SCHEMA_DEEPEST deep at most; the bases a check reads from are kept on one stack of
// CLI_SCHEMA_BASES bytes, a base past it read as the one before
#include "schema_internal.h"

// the most bytes of the bases a check reads from at once
#define CLI_SCHEMA_BASES (1u << 18u)

static char s_bases[CLI_SCHEMA_BASES];
static unsigned int s_bases_used;

// the span a finding about a value stands on: its key's where it has one, its own where it has none
static void check_span(const CliSchemaCheck *check, unsigned int node, CliSchemaKey key, unsigned long long *from,
                       unsigned long long *to)
{
    *from = key.held ? key.from : check->read->node[node].from;
    *to = key.held ? key.to : check->read->node[node].to;
}

// the findings from `count` and the fixes from `fixes` that are notes taken out, the rest kept in their order
static void check_notes_dropped(CliBuildFindings *found, unsigned int count, unsigned int fixes)
{
    unsigned int kept = count;
    static unsigned int s_moved[CLI_BUILD_FINDINGS];
    for (unsigned int at = count; at < found->count; at += 1u)
    {
        s_moved[at] = CLI_BUILD_NONE;
        if (found->finding[at].severity != CLI_BUILD_NOTE)
        {
            found->finding[kept] = found->finding[at];
            s_moved[at] = kept;
            kept += 1u;
        }
    }
    unsigned int fix_kept = fixes;
    for (unsigned int at = fixes; at < found->fixes; at += 1u)
    {
        const unsigned int finding = found->fix[at].finding;
        const unsigned int moved = (finding >= count) ? s_moved[finding] : finding;
        if (moved != CLI_BUILD_NONE)
        {
            found->fix[fix_kept] = found->fix[at];
            found->fix[fix_kept].finding = moved;
            fix_kept += 1u;
        }
    }
    found->count = kept;
    found->fixes = fix_kept;
}

unsigned int cli_schema_trial(CliSchemaCheck *check, unsigned int node, unsigned int schema, const char *base,
                              unsigned int depth, int keep)
{
    CliBuildFindings *const found = check->found;
    const unsigned int count = found->count;
    const unsigned int fixes = found->fixes;
    const unsigned int used = found->used;
    const CliSchemaKey none = {0, 0ull, 0ull};
    cli_schema_node(check, node, schema, base, none, depth + 1u);
    unsigned int tried = 0u;
    for (unsigned int at = count; at < found->count; at += 1u)
    {
        tried += (found->finding[at].severity != CLI_BUILD_NOTE) ? 1u : 0u;
    }
    if (keep)
    {
        check_notes_dropped(found, count, fixes);
        return tried;
    }
    found->count = count;
    found->fixes = fixes;
    found->used = used;
    return tried;
}

int cli_schema_fits(CliSchemaCheck *check, unsigned int node, unsigned int schema, const char *base, unsigned int depth)
{
    return cli_schema_trial(check, node, schema, base, depth, 0) == 0u;
}

// `anyOf` and `oneOf`: the value fits one of the choices, or exactly one; where it fits none, the findings of the
// choice it comes nearest are given
static void check_choose(CliSchemaCheck *check, unsigned int node, unsigned int choices, const char *base,
                         CliSchemaKey key, unsigned int depth, int exactly)
{
    const CliJson *const json = &check->store->json;
    unsigned int fitting = 0u;
    unsigned int nearest = CLI_JSON_NONE;
    unsigned int least = 0u;
    for (unsigned int choice = json->node[choices].first; choice != CLI_JSON_NONE; choice = json->node[choice].next)
    {
        const unsigned int tried = cli_schema_trial(check, node, choice, base, depth, 0);
        fitting += (tried == 0u) ? 1u : 0u;
        if ((nearest == CLI_JSON_NONE) || (tried < least))
        {
            nearest = choice;
            least = tried;
        }
    }
    if ((fitting == 0u) && (nearest != CLI_JSON_NONE))
    {
        (void)cli_schema_trial(check, node, nearest, base, depth, 1);
    }
    else if (exactly && (fitting > 1u))
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        check_span(check, node, key, &from, &to);
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "This value fits ");
        cli_line_decimal(&said, fitting, 1u);
        cli_line_text(&said, " of the shapes the schema allows, where it asks for exactly one");
        (void)cli_build_finding(check->found, &said, check->lines, from, to, CLI_BUILD_WARNING, "schema-one-of");
    }
}

// the value checked against the schema's `type`: 1 where it fits, 0 where a finding was made
static int check_type(CliSchemaCheck *check, unsigned int node, unsigned int map)
{
    const CliJson *const json = &check->store->json;
    const unsigned int types = cli_schema_get(json, map, "type");
    if (types == CLI_JSON_NONE)
    {
        return 1;
    }
    const char *const kind = cli_schema_kind(&check->read->node[node]);
    const unsigned int single = cli_json_is(json, types, CFG_JSON_STRING) ? types : CLI_JSON_NONE;
    const unsigned int first = cli_json_is(json, types, CFG_JSON_ARRAY) ? json->node[types].first : single;
    int any = 0;
    int fits = 0;
    for (unsigned int one = first; one != CLI_JSON_NONE;
         one = (single != CLI_JSON_NONE) ? CLI_JSON_NONE : json->node[one].next)
    {
        unsigned long long size = 0ull;
        const char *const allowed = cli_json_string(json, one, &size);
        if (allowed == NULL)
        {
            continue;
        }
        any = 1;
        fits |= ((cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) == size) &&
                 (memor.cmp(&(MemoriaCfg){.src = allowed, .other = kind, .bytes = (size_t)size}) == 0)) ||
                ((size == 6u) && (memor.cmp(&(MemoriaCfg){.src = allowed, .other = "number", .bytes = 6u}) == 0) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "integer", .cap = CLI_CELLS_POOL})));
    }
    if (!any || fits)
    {
        return 1;
    }
    const CliSchemaNode *const value = &check->read->node[node];
    CliLine said = cli_build_said(check->found);
    cli_line_text(&said, "A value of type ");
    cli_schema_kind_said(kind, cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}), &said);
    cli_line_text(&said, " stands here, where the schema asks for ");
    int first_said = 1;
    for (unsigned int one = first; one != CLI_JSON_NONE;
         one = (single != CLI_JSON_NONE) ? CLI_JSON_NONE : json->node[one].next)
    {
        unsigned long long size = 0ull;
        const char *const allowed = cli_json_string(json, one, &size);
        if (allowed != NULL)
        {
            cli_line_text(&said, first_said ? "" : " or ");
            cli_schema_kind_said(allowed, size, &said);
            first_said = 0;
        }
    }
    (void)cli_build_finding(check->found, &said, check->lines, value->from, value->to, CLI_BUILD_WARNING,
                            "schema-type");
    return 0;
}

// the value checked against the schema's `enum` and `const`
static void check_values(CliSchemaCheck *check, unsigned int node, unsigned int map)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaNode *const value = &check->read->node[node];
    const unsigned int choices = cli_schema_get(json, map, "enum");
    if ((choices != CLI_JSON_NONE) && cli_json_is(json, choices, CFG_JSON_ARRAY))
    {
        int any = 0;
        for (unsigned int one = json->node[choices].first; !any && (one != CLI_JSON_NONE); one = json->node[one].next)
        {
            any = cli_schema_same(check->read, node, json, one);
        }
        if (!any)
        {
            CliLine said = cli_build_said(check->found);
            cli_schema_shown_value(check->read, node, &said);
            cli_line_text(&said, " is none of the values the schema allows: ");
            unsigned int shown = 0u;
            for (unsigned int one = json->node[choices].first; (one != CLI_JSON_NONE) && (shown < 12u);
                 one = json->node[one].next)
            {
                cli_line_text(&said, (shown > 0u) ? ", " : "");
                cli_schema_shown(json, one, &said);
                shown += 1u;
            }
            const unsigned int finding = cli_build_finding(check->found, &said, check->lines, value->from, value->to,
                                                           CLI_BUILD_WARNING, "schema-enum");
            if (value->kind == CLI_SCHEMA_TEXT)
            {
                CliBuildNear near;
                cli_build_near_begun(&near, cli_schema_text(check->read, value->text_at), value->text_size);
                for (unsigned int one = json->node[choices].first; one != CLI_JSON_NONE; one = json->node[one].next)
                {
                    unsigned long long size = 0ull;
                    const char *const text = cli_json_string(json, one, &size);
                    if (text != NULL)
                    {
                        cli_build_near_offered(&near, text, size);
                    }
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
                    cli_build_edit_fix(check->found, finding, &title, check->lines, value->from, value->to, s_quoted,
                                       quoted.at);
                }
            }
        }
    }
    const unsigned int wanted = cli_schema_get(json, map, "const");
    if ((wanted != CLI_JSON_NONE) && !cli_schema_same(check->read, node, json, wanted))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "The schema asks for ");
        cli_schema_shown(json, wanted, &said);
        cli_line_text(&said, " here");
        (void)cli_build_finding(check->found, &said, check->lines, value->from, value->to, CLI_BUILD_WARNING,
                                "schema-const");
    }
}

// the base a schema's `$id` sets, kept on the stack of bases: it, or `base` where it sets none
static const char *check_base(const CliJson *json, unsigned int map, const char *base)
{
    unsigned long long size = 0ull;
    const char *const id = cli_schema_string(json, map, "$id", &size);
    unsigned long long found = 0ull;
    if ((id == NULL) || !cli_text_find(id, size, "://", 3u, &found))
    {
        return base;
    }
    while ((size > 0u) && (id[size - 1u] == '#'))
    {
        size -= 1u;
    }
    if (size + 1u > CLI_SCHEMA_BASES - s_bases_used)
    {
        return base;
    }
    char *const kept = s_bases + s_bases_used;
    memor.cpy(&(MemoriaCfg){.dst = kept, .src = id, .bytes = (size_t)size});
    kept[size] = '\0';
    s_bases_used += (unsigned int)size + 1u;
    return kept;
}

void cli_schema_node(CliSchemaCheck *check, unsigned int node, unsigned int schema, const char *base, CliSchemaKey key,
                     unsigned int depth)
{
    const CliJson *const json = &check->store->json;
    if ((depth > CLI_SCHEMA_DEEPEST) || (schema == CLI_SCHEMA_NONE) || cli_schema_true(json, schema))
    {
        return;
    }
    if (cli_schema_false(json, schema))
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        check_span(check, node, key, &from, &to);
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "The schema allows no value here");
        (void)cli_build_finding(check->found, &said, check->lines, from, to, CLI_BUILD_WARNING, "schema");
        return;
    }
    if (!cli_json_is(json, schema, CFG_JSON_OBJECT))
    {
        return;
    }
    const unsigned int used = s_bases_used;
    const char *const here = check_base(json, schema, base);
    unsigned long long size = 0ull;
    const char *const reference = cli_schema_string(json, schema, "$ref", &size);
    if ((reference != NULL) && (CLI_PATH_LONGEST <= CLI_SCHEMA_BASES - s_bases_used))
    {
        char *const doc = s_bases + s_bases_used;
        const unsigned int found = cli_schema_resolve(check->store, here, reference, size, doc);
        if (found != CLI_SCHEMA_NONE)
        {
            const unsigned int before = s_bases_used;
            s_bases_used += (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = doc, .cap = CLI_CELLS_POOL}) + 1u;
            cli_schema_node(check, node, found, doc, key, depth + 1u);
            s_bases_used = before;
        }
    }
    if (cli_schema_true(json, cli_schema_get(json, schema, "deprecated")) && key.held)
    {
        unsigned long long said_size = 0ull;
        const char *const message = cli_schema_string(json, schema, "deprecationMessage", &said_size);
        CliLine said = cli_build_said(check->found);
        cli_line_counted(&said, check->text + key.from, key.to - key.from);
        cli_line_text(&said, " is deprecated");
        if (message != NULL)
        {
            cli_line_text(&said, ": ");
            cli_line_counted(&said, message, said_size);
        }
        (void)cli_build_finding(check->found, &said, check->lines, key.from, key.to, CLI_BUILD_NOTE,
                                "schema-deprecated");
    }
    if (check_type(check, node, schema))
    {
        check_values(check, node, schema);
        const unsigned char kind = check->read->node[node].kind;
        if (kind == CLI_SCHEMA_NUMBER)
        {
            cli_schema_number_checked(check, node, schema);
        }
        else if (kind == CLI_SCHEMA_TEXT)
        {
            cli_schema_text_checked(check, node, schema);
        }
        else if (kind == CLI_SCHEMA_ARRAY)
        {
            cli_schema_array_checked(check, node, schema, here, depth);
        }
        else if (kind == CLI_SCHEMA_OBJECT)
        {
            cli_schema_object_checked(check, node, schema, here, key, depth);
        }
        const unsigned int all = cli_schema_get(json, schema, "allOf");
        for (unsigned int one = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            cli_schema_node(check, node, one, here, key, depth + 1u);
        }
        static const char *const s_words[2] = {"anyOf", "oneOf"};
        for (unsigned int word = 0u; word < 2u; word += 1u)
        {
            const unsigned int choices = cli_schema_get(json, schema, s_words[word]);
            if (cli_json_is(json, choices, CFG_JSON_ARRAY))
            {
                check_choose(check, node, choices, here, key, depth, word == 1u);
            }
        }
        const unsigned int not_one = cli_schema_get(json, schema, "not");
        if ((not_one != CLI_JSON_NONE) && cli_schema_fits(check, node, not_one, here, depth))
        {
            const CliSchemaNode *const value = &check->read->node[node];
            CliLine said = cli_build_said(check->found);
            cli_line_text(&said, "The schema rules this value out");
            (void)cli_build_finding(check->found, &said, check->lines, value->from, value->to, CLI_BUILD_WARNING,
                                    "schema-not");
        }
        const unsigned int condition = cli_schema_get(json, schema, "if");
        if (condition != CLI_JSON_NONE)
        {
            const unsigned int branch =
                cli_schema_get(json, schema, cli_schema_fits(check, node, condition, here, depth) ? "then" : "else");
            if (branch != CLI_JSON_NONE)
            {
                cli_schema_node(check, node, branch, here, key, depth + 1u);
            }
        }
    }
    s_bases_used = used;
}
