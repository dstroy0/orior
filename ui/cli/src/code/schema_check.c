// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_check.c: a value checked against a schema: `true` allows it and `false` none; `$id` sets the base its
// references are read from; `$ref` is followed; `deprecated` is noted on the key; `type`, `enum` and `const` are
// checked to; the value's own bounds by its kind; `allOf` each, `anyOf` one and `oneOf` exactly one, the nearest
// choice's findings given where none fits; `not`; and `if` with `then` or `else`. A trial's findings are those that are
// no note. Values are checked CLI_SCHEMA_DEEPEST deep at most; the bases a check reads from are stored on one stack of
// CLI_SCHEMA_BASES bytes, a base past it read as the one before
#include "../text/text.h"
#include "schema_internal.h"

// the most bytes of the bases a check reads from at once
#define CLI_SCHEMA_BASES (1u << 18u)

static char s_bases[CLI_SCHEMA_BASES];
static unsigned int s_bases_used;

typedef struct
{
    const CliSchemaCheck *check_read;
    unsigned int node;
    CliSchemaKey key;
    unsigned long long *from;
    unsigned long long *to;
    CliBuildFindings *found;
    unsigned int count;
    unsigned int fixes;
    CliSchemaCheck *check;
    unsigned int choices;
    const char *base;
    unsigned int depth;
    int exactly;
    unsigned int map;
    const CliJson *json;
} SchemaCheckCtx;
// the span a finding about a value stands on: its key's where it has one, its own where it has none
EMBED_INLINE void check_span(const SchemaCheckCtx *args)
{
    unsigned long long *from = args->from;
    unsigned long long *to = args->to;
    *from = args->key.has_key ? args->key.from : args->check_read->read->node[args->node].from;
    *to = args->key.has_key ? args->key.to : args->check_read->read->node[args->node].to;
}

// the findings from `count` and the fixes from `fixes` that are notes taken out, the rest retained in their order
EMBED_INLINE void check_notes_drop(const SchemaCheckCtx *args)
{
    unsigned int count = args->count;
    unsigned int fixes = args->fixes;
    unsigned int remaining = count;
    static unsigned int s_moved[CLI_BUILD_FINDINGS];
    for (unsigned int at = count; at < args->found->count; at += 1u)
    {
        s_moved[at] = CLI_BUILD_NONE;
        if (args->found->finding[at].severity != CLI_BUILD_NOTE)
        {
            args->found->finding[remaining] = args->found->finding[at];
            s_moved[at] = remaining;
            remaining += 1u;
        }
    }
    unsigned int fix_count = fixes;
    for (unsigned int at = fixes; at < args->found->fixes; at += 1u)
    {
        const unsigned int finding = args->found->fix[at].finding;
        const unsigned int moved = (finding >= count) ? s_moved[finding] : finding;
        if (moved != CLI_BUILD_NONE)
        {
            args->found->fix[fix_count] = args->found->fix[at];
            args->found->fix[fix_count].finding = moved;
            fix_count += 1u;
        }
    }
    args->found->count = remaining;
    args->found->fixes = fix_count;
}

unsigned int cli_schema_trial(const CliSchemaBaseNodeCtx *args)
{
    CliBuildFindings *const found = args->check->found;
    const unsigned int count = found->count;
    const unsigned int fixes = found->fixes;
    const unsigned int used = found->used;
    const CliSchemaKey none = {0, 0ULL, 0ULL};
    EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = args->check, .node = args->node,
               .schema = args->schema, .base = args->base, .key_schema_key = none, .depth = args->depth + 1u);
    unsigned int tried = 0u;
    for (unsigned int at = count; at < found->count; at += 1u)
    {
        tried += (found->finding[at].severity != CLI_BUILD_NOTE) ? 1u : 0u;
    }
    if (args->record)
    {
        EMBED_CALL(check_notes_drop, SchemaCheckCtx, .found = found, .count = count, .fixes = fixes);
        return tried;
    }
    found->count = count;
    found->fixes = fixes;
    found->used = used;
    return tried;
}

int cli_schema_fits(const CliSchemaBaseCtx *args)
{
    return EMBED_CALL(cli_schema_base_node_internal.trial, CliSchemaBaseNodeCtx, .check = args->check,
                      .node = args->node, .schema = args->schema, .base = args->base, .depth = args->depth,
                      .record = 0) == 0u;
}

// `anyOf` and `oneOf`: the value fits one of the choices, or exactly one; where it fits none, the findings of the
// choice it comes nearest are given
EMBED_INLINE void check_choose(const SchemaCheckCtx *args)
{
    CliSchemaCheck *check = args->check;
    unsigned int node = args->node;
    const char *base = args->base;
    unsigned int depth = args->depth;
    const CliJson *const json = &check->store->json;
    unsigned int fitting = 0u;
    unsigned int nearest = CLI_JSON_NONE;
    unsigned int min = 0u;
    for (unsigned int choice = json->node[args->choices].first; choice != CLI_JSON_NONE;
         choice = json->node[choice].next)
    {
        const unsigned int tried =
            EMBED_CALL(cli_schema_base_node_internal.trial, CliSchemaBaseNodeCtx, .check = check, .node = node,
                       .schema = choice, .base = base, .depth = depth, .record = 0);
        fitting += (tried == 0u) ? 1u : 0u;
        if ((nearest == CLI_JSON_NONE) || (tried < min))
        {
            nearest = choice;
            min = tried;
        }
    }
    if ((fitting == 0u) && (nearest != CLI_JSON_NONE))
    {
        (void)EMBED_CALL(cli_schema_base_node_internal.trial, CliSchemaBaseNodeCtx, .check = check, .node = node,
                         .schema = nearest, .base = base, .depth = depth, .record = 1);
    }
    else if (args->exactly && (fitting > 1u))
    {
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(check_span, SchemaCheckCtx, .check_read = check, .node = node, .key = args->key, .from = &from,
                   .to = &to);
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&msg, "This value fits ");
        mmgr_line_decimal(&msg, fitting, 1u);
        mmgr_line_put(&msg, " of the shapes the schema allows, where it requires exactly one");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                         .lines = check->lines, .from = from, .to = to, .severity = CLI_BUILD_WARNING,
                         .code = "schema-one-of");
    }
}

// the value checked against the schema's `type`: 1 where it fits, 0 where a finding was made
EMBED_INLINE int check_type(const SchemaCheckCtx *args)
{
    unsigned int node = args->node;
    unsigned int map = args->map;
    const CliJson *const json = &args->check->store->json;
    const unsigned int types =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = map, .name = "type");
    if (types == CLI_JSON_NONE)
    {
        return 1;
    }
    const char *const kind =
        EMBED_CALL(cli_schema_internal.kind, CliSchemaCtx, .node_schema_node_pointer = &args->check->read->node[node]);
    const unsigned int single = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = types, .kind = CFG_JSON_STRING)
                                    ? types
                                    : CLI_JSON_NONE;
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = types, .kind = CFG_JSON_ARRAY)
                                   ? json->node[types].first
                                   : single;
    int any = 0;
    int fits = 0;
    for (unsigned int one = first; one != CLI_JSON_NONE;
         one = (single != CLI_JSON_NONE) ? CLI_JSON_NONE : json->node[one].next)
    {
        unsigned long long size = 0ULL;
        const char *const allowed = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                               .json_read = json, .node = one, .length_pointer = &size);
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
    const CliSchemaNode *const value = &args->check->read->node[node];
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_put(&msg, "A value of type ");
    EMBED_CALL(cli_schema_size_out_internal.kind_write, CliSchemaSizeOutCtx, .kind = kind,
               .size = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}), .out = &msg);
    mmgr_line_put(&msg, " stands here, where the schema requires ");
    int first_listing = 1;
    for (unsigned int one = first; one != CLI_JSON_NONE;
         one = (single != CLI_JSON_NONE) ? CLI_JSON_NONE : json->node[one].next)
    {
        unsigned long long size = 0ULL;
        const char *const allowed = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                               .json_read = json, .node = one, .length_pointer = &size);
        if (allowed != NULL)
        {
            mmgr_line_put(&msg, first_listing ? "" : " or ");
            EMBED_CALL(cli_schema_size_out_internal.kind_write, CliSchemaSizeOutCtx, .kind = allowed, .size = size,
                       .out = &msg);
            first_listing = 0;
        }
    }
    (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                     .lines = args->check->lines, .from = value->from, .to = value->to, .severity = CLI_BUILD_WARNING,
                     .code = "schema-type");
    return 0;
}

// the value checked against the schema's `enum` and `const`
EMBED_INLINE void check_values(const SchemaCheckCtx *args)
{
    unsigned int node = args->node;
    unsigned int map = args->map;
    const CliJson *const json = &args->check->store->json;
    const CliSchemaNode *const value = &args->check->read->node[node];
    const unsigned int choices =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = map, .name = "enum");
    if ((choices != CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                 .json_read = json, .node = choices, .kind = CFG_JSON_ARRAY))
    {
        int any = 0;
        for (unsigned int one = json->node[choices].first; !any && (one != CLI_JSON_NONE); one = json->node[one].next)
        {
            any = EMBED_CALL(cli_schema_read_json_internal.same, CliSchemaReadJsonCtx, .read = args->check->read,
                             .node = node, .json = json, .value = one);
        }
        if (!any)
        {
            VerbaLinea msg =
                EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
            EMBED_CALL(cli_schema_read_node_internal.displayed_value, CliSchemaReadNodeCtx, .read = args->check->read,
                       .node = node, .out = &msg);
            mmgr_line_put(&msg, " is none of the values the schema allows: ");
            unsigned int displayed = 0u;
            for (unsigned int one = json->node[choices].first; (one != CLI_JSON_NONE) && (displayed < 12u);
                 one = json->node[one].next)
            {
                mmgr_line_put(&msg, (displayed > 0u) ? ", " : "");
                EMBED_CALL(cli_schema_json_value_internal.show, CliSchemaJsonValueCtx, .json = json, .value = one,
                           .out = &msg);
                displayed += 1u;
            }
            const unsigned int finding =
                EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                           .message = &msg, .lines = args->check->lines, .from = value->from, .to = value->to,
                           .severity = CLI_BUILD_WARNING, .code = "schema-enum");
            if (value->kind == CLI_SCHEMA_TEXT)
            {
                CliBuildAdjacent adjacent;
                EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent,
                           .name = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                              .read = args->check->read, .at = value->text_at),
                           .size = value->text_size);
                for (unsigned int one = json->node[choices].first; one != CLI_JSON_NONE; one = json->node[one].next)
                {
                    unsigned long long size = 0ULL;
                    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                        .json_read = json, .node = one, .length_pointer = &size);
                    if (text != NULL)
                    {
                        EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg,
                                   .adjacent = &adjacent, .one = text, .size = size);
                    }
                }
                for (unsigned int at = 0u; at < adjacent.count; at += 1u)
                {
                    VerbaLinea title =
                        EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
                    mmgr_line_put(&title, "Change to \"");
                    mmgr_line_put_n(&title, adjacent.adjacent[at], adjacent.adjacent_size[at]);
                    mmgr_line_ch(&title, '"');
                    static char s_quoted[1u << 13u];
                    VerbaLinea quoted = {s_quoted, sizeof(s_quoted), 0ULL};
                    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &quoted,
                               .text = adjacent.adjacent[at], .length = adjacent.adjacent_size[at]);
                    EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = args->check->found,
                               .finding = finding, .title = &title, .lines = args->check->lines, .from = value->from,
                               .to = value->to, .text = s_quoted, .size = quoted.at);
                }
            }
        }
    }
    const unsigned int required =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = map, .name = "const");
    if ((required != CLI_JSON_NONE) &&
        !EMBED_CALL(cli_schema_read_json_internal.same, CliSchemaReadJsonCtx, .read = args->check->read, .node = node,
                    .json = json, .value = required))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "The schema requires ");
        EMBED_CALL(cli_schema_json_value_internal.show, CliSchemaJsonValueCtx, .json = json, .value = required,
                   .out = &msg);
        mmgr_line_put(&msg, " here");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = value->from, .to = value->to,
                         .severity = CLI_BUILD_WARNING, .code = "schema-const");
    }
}

// the base a schema's `$id` sets, stored on the stack of bases: it, or `base` where it sets none
EMBED_INLINE const char *check_base(const SchemaCheckCtx *args)
{
    const CliJson *json = args->json;
    unsigned int map = args->map;
    unsigned long long size = 0ULL;
    const char *const id = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json, .map = map,
                                      .name = "$id", .size_pointer = &size);
    unsigned long long found = 0ULL;
    if ((id == NULL) || !EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = id, .length_value = size,
                                    .required = "://", .size = 3u, .at_pointer = &found))
    {
        return args->base;
    }
    size = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = id, .at = size, .set = "#");
    if (size + 1u > CLI_SCHEMA_BASES - s_bases_used)
    {
        return args->base;
    }
    char *const base_at = s_bases + s_bases_used;
    memor.cpy(&(MemoriaCfg){.dst = base_at, .src = id, .bytes = (size_t)size});
    base_at[size] = '\0';
    s_bases_used += (unsigned int)size + 1u;
    return base_at;
}

void cli_schema_node(const CliSchemaBaseCtx *args)
{
    const CliJson *const json = &args->check->store->json;
    if ((args->depth > CLI_SCHEMA_DEEPEST) || (args->schema == CLI_SCHEMA_NONE) ||
        EMBED_CALL(cli_schema_json_node_internal.schema_true, CliSchemaJsonNodeCtx, .json = json, .node = args->schema))
    {
        return;
    }
    if (EMBED_CALL(cli_schema_json_node_internal.schema_false, CliSchemaJsonNodeCtx, .json = json,
                   .node = args->schema))
    {
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(check_span, SchemaCheckCtx, .check_read = args->check, .node = args->node,
                   .key = args->key_schema_key, .from = &from, .to = &to);
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "The schema allows no value here");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = from, .to = to,
                         .severity = CLI_BUILD_WARNING, .code = "schema");
        return;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->schema,
                    .kind = CFG_JSON_OBJECT))
    {
        return;
    }
    const unsigned int used = s_bases_used;
    const char *const here =
        EMBED_CALL(check_base, SchemaCheckCtx, .json = json, .map = args->schema, .base = args->base);
    unsigned long long size = 0ULL;
    const char *const reference = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json,
                                             .map = args->schema, .name = "$ref", .size_pointer = &size);
    if ((reference != NULL) && (CLI_PATH_MAX_LEN <= CLI_SCHEMA_BASES - s_bases_used))
    {
        char *const doc = s_bases + s_bases_used;
        const unsigned int found =
            EMBED_CALL(cli_schema_size_internal.resolve, CliSchemaSizeCtx, .store = args->check->store, .base = here,
                       .reference = reference, .size = size, .key = doc);
        if (found != CLI_SCHEMA_NONE)
        {
            const unsigned int before = s_bases_used;
            s_bases_used += (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = doc, .cap = CLI_CELLS_POOL}) + 1u;
            EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = args->check, .node = args->node,
                       .schema = found, .base = doc, .key_schema_key = args->key_schema_key, .depth = args->depth + 1u);
            s_bases_used = before;
        }
    }
    if (EMBED_CALL(cli_schema_json_node_internal.schema_true, CliSchemaJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema,
                                      .name = "deprecated")) &&
        args->key_schema_key.has_key)
    {
        unsigned long long message_size = 0ULL;
        const char *const message =
            EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json, .map = args->schema,
                       .name = "deprecationMessage", .size_pointer = &message_size);
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put_n(&msg, args->check->text + args->key_schema_key.from,
                        args->key_schema_key.to - args->key_schema_key.from);
        mmgr_line_put(&msg, " is deprecated");
        if (message != NULL)
        {
            mmgr_line_put(&msg, ": ");
            mmgr_line_put_n(&msg, message, message_size);
        }
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = args->key_schema_key.from,
                         .to = args->key_schema_key.to, .severity = CLI_BUILD_NOTE, .code = "schema-deprecated");
    }
    if (EMBED_CALL(check_type, SchemaCheckCtx, .check = args->check, .node = args->node, .map = args->schema))
    {
        EMBED_CALL(check_values, SchemaCheckCtx, .check = args->check, .node = args->node, .map = args->schema);
        const unsigned char kind = args->check->read->node[args->node].kind;
        if (kind == CLI_SCHEMA_NUMBER)
        {
            EMBED_CALL(cli_schema_base_map_internal.number_check, CliSchemaBaseMapCtx, .check = args->check,
                       .node = args->node, .map = args->schema);
        }
        else if (kind == CLI_SCHEMA_TEXT)
        {
            EMBED_CALL(cli_schema_base_map_internal.text_check, CliSchemaBaseMapCtx, .check = args->check,
                       .node = args->node, .map = args->schema);
        }
        else if (kind == CLI_SCHEMA_ARRAY)
        {
            EMBED_CALL(cli_schema_base_map_internal.array_check, CliSchemaBaseMapCtx, .check = args->check,
                       .node = args->node, .map = args->schema, .base = here, .depth = args->depth);
        }
        else if (kind == CLI_SCHEMA_OBJECT)
        {
            EMBED_CALL(cli_schema_base_map_internal.object_check, CliSchemaBaseMapCtx, .check = args->check,
                       .node = args->node, .map = args->schema, .base = here, .key_schema_key = args->key_schema_key,
                       .depth = args->depth);
        }
        const unsigned int all = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                            .map = args->schema, .name = "allOf");
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = all, .kind = CFG_JSON_ARRAY)
                                    ? json->node[all].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = args->check, .node = args->node,
                       .schema = one, .base = here, .key_schema_key = args->key_schema_key, .depth = args->depth + 1u);
        }
        static const char *const s_words[2] = {"anyOf", "oneOf"};
        for (unsigned int word = 0u; word < 2u; word += 1u)
        {
            const unsigned int choices = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                    .map = args->schema, .name = s_words[word]);
            if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = choices,
                           .kind = CFG_JSON_ARRAY))
            {
                EMBED_CALL(check_choose, SchemaCheckCtx, .check = args->check, .node = args->node, .choices = choices,
                           .base = here, .key = args->key_schema_key, .depth = args->depth, .exactly = word == 1u);
            }
        }
        const unsigned int not_one = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                .map = args->schema, .name = "not");
        if ((not_one != CLI_JSON_NONE) &&
            EMBED_CALL(cli_schema_base_internal.fits, CliSchemaBaseCtx, .check = args->check, .node = args->node,
                       .schema = not_one, .base = here, .depth = args->depth))
        {
            const CliSchemaNode *const value = &args->check->read->node[args->node];
            VerbaLinea msg =
                EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
            mmgr_line_put(&msg, "The schema rules this value out");
            (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                             .message = &msg, .lines = args->check->lines, .from = value->from, .to = value->to,
                             .severity = CLI_BUILD_WARNING, .code = "schema-not");
        }
        const unsigned int condition =
            EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema, .name = "if");
        if (condition != CLI_JSON_NONE)
        {
            const unsigned int branch = EMBED_CALL(
                cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema,
                .name = EMBED_CALL(cli_schema_base_internal.fits, CliSchemaBaseCtx, .check = args->check,
                                   .node = args->node, .schema = condition, .base = here, .depth = args->depth)
                            ? "then"
                            : "else");
            if (branch != CLI_JSON_NONE)
            {
                EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = args->check, .node = args->node,
                           .schema = branch, .base = here, .key_schema_key = args->key_schema_key,
                           .depth = args->depth + 1u);
            }
        }
    }
    s_bases_used = used;
}
