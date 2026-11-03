// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_values.c: a schema's values read: its members, strings, booleans and numbers; two values compared as JSON's
// values are, a number by its value; values shown in a message as serde_json writes them, an object's keys in order,
// cut short past forty letters; a pattern matched as ECMAScript reads one; and a number and a string checked against
// the keywords that bound them
#include "../text/regexp.h"
#include "schema_internal.h"

#include "../text/text.h"
#include <math.h>
#include <stdlib.h>

// the most letters of a string a pattern is matched against, the most bytes of a pattern, and the bytes a value is
// written in before it is cut short
#define CLI_SCHEMA_MATCHED (1u << 20u)
#define CLI_SCHEMA_PATTERN (1u << 14u)
#define CLI_SCHEMA_DISPLAYED_CAPACITY (1ULL << 20u)

const char *cli_schema_text(const CliSchemaReadAtCtx *args)
{
    return (args->at < args->read->used) ? args->read->pool + args->at : "";
}

const char *cli_schema_kind(const CliSchemaCtx *args)
{
    static const char *const s_kinds[7] = {"", "object", "array", "string", "number", "boolean", "null"};
    if ((args->node_schema_node_pointer->kind == CLI_SCHEMA_NUMBER) &&
        isfinite(args->node_schema_node_pointer->number) &&
        (floor(args->node_schema_node_pointer->number) == args->node_schema_node_pointer->number))
    {
        return "integer";
    }
    return s_kinds[args->node_schema_node_pointer->kind];
}

unsigned int cli_schema_get(const CliSchemaJsonCtx *args)
{
    return ((args->map != CLI_JSON_NONE) &&
            EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                       .node = args->map, .kind = CFG_JSON_OBJECT))
               ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                            .object = args->map, .name = args->name)
               : CLI_JSON_NONE;
}

const char *cli_schema_string(const CliSchemaJsonCtx *args)
{
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                      .node = cli_schema_json_internal.get(args), .length_pointer = args->size_pointer);
}

int cli_schema_true(const CliSchemaJsonNodeCtx *args)
{
    return (args->node != CLI_JSON_NONE) &&
           EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                      .node = args->node, .kind = CFG_JSON_TRUE);
}

int cli_schema_false(const CliSchemaJsonNodeCtx *args)
{
    return (args->node != CLI_JSON_NONE) &&
           EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                      .node = args->node, .kind = CFG_JSON_FALSE);
}

int cli_schema_number(const CliSchemaJsonNodeCtx *args)
{
    double *value = args->value_f64s;
    if ((args->node == CLI_JSON_NONE) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                    .node = args->node, .kind = CFG_JSON_NUMBER))
    {
        return 0;
    }
    char raw[512];
    const CliJsonNode *const one = &args->json->node[args->node];
    const unsigned long long size = (one->length < sizeof(raw)) ? one->length : sizeof(raw) - 1u;
    memor.cpy(&(MemoriaCfg){.dst = raw, .src = args->json->text + one->text, .bytes = (size_t)size});
    raw[size] = '\0';
    *value = strtod(raw, NULL);
    return 1;
}

int cli_schema_u64(const CliSchemaJsonNodeValueU64sCtx *args)
{
    unsigned long long *value = args->value_u64s;
    if ((args->node == CLI_JSON_NONE) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                    .node = args->node, .kind = CFG_JSON_NUMBER))
    {
        return 0;
    }
    const CliJsonNode *const one = &args->json->node[args->node];
    const char *const text = args->json->text + one->text;
    unsigned long long read = 0ULL;
    for (unsigned long long at = 0ULL; at < one->length; at += 1u)
    {
        if ((text[at] < '0') || (text[at] > '9') || (read > (~0ULL - 9u) / 10u))
        {
            return 0;
        }
        read = (read * 10u) + (unsigned long long)(text[at] - '0');
    }
    *value = read;
    return one->length > 0u;
}

typedef struct
{
    const CliSchemaRead *read;
    unsigned int object;
    const char *key;
    unsigned long long size;
    const CliSchemaNode *node;
    const char *text;
    VerbaLinea *out;
    unsigned int node_u32;
} SchemaValuesCtx;
// the last member of the file's object `object` keyed as `key` that contains a value, CLI_SCHEMA_NONE where none does
EMBED_INLINE unsigned int values_member(const SchemaValuesCtx *args)
{
    const CliSchemaRead *read = args->read;
    unsigned int found = CLI_SCHEMA_NONE;
    for (unsigned int at = read->node[args->object].first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        if ((member->value != CLI_SCHEMA_NONE) && (member->key_size == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                       .read = read, .at = member->key_at),
                                     .other = args->key,
                                     .bytes = (size_t)args->size}) == 0))
        {
            found = at;
        }
    }
    return found;
}

// how many keys of the file's object `object` contain values, each counted once
EMBED_INLINE unsigned int values_keys(const SchemaValuesCtx *args)
{
    const CliSchemaRead *read = args->read;
    unsigned int count = 0u;
    for (unsigned int at = read->node[args->object].first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        count += ((member->value != CLI_SCHEMA_NONE) &&
                  (EMBED_CALL(values_member, SchemaValuesCtx, .read = read, .object = args->object,
                              .key = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = read,
                                                .at = member->key_at),
                              .size = member->key_size) == at))
                     ? 1u
                     : 0u;
    }
    return count;
}

// the kind of a file's value as JSON's values compare, a number that is not finite read as null
EMBED_INLINE unsigned char values_kind(const SchemaValuesCtx *args)
{
    return ((args->node->kind == CLI_SCHEMA_NUMBER) && !isfinite(args->node->number)) ? CLI_SCHEMA_NULL
                                                                                      : args->node->kind;
}

int cli_schema_same(const CliSchemaReadJsonCtx *args)
{
    const CliSchemaNode *const one = &args->read->node[args->node];
    const CfgJsonKind kind = args->json->node[args->value].kind;
    const unsigned char own = EMBED_CALL(values_kind, SchemaValuesCtx, .node = one);
    if (own == CLI_SCHEMA_NUMBER)
    {
        double other = 0.0;
        return EMBED_CALL(cli_schema_json_node_internal.number, CliSchemaJsonNodeCtx, .json = args->json,
                          .node = args->value, .value_f64s = &other) &&
               (one->number == other);
    }
    if (own == CLI_SCHEMA_TEXT)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = args->json, .node = args->value, .length_pointer = &size);
        return (text != NULL) && (size == one->text_size) &&
               (memor.cmp(&(MemoriaCfg){.src = text,
                                        .other = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                            .read = args->read, .at = one->text_at),
                                        .bytes = (size_t)size}) == 0);
    }
    if (own == CLI_SCHEMA_BOOL)
    {
        return (kind == (one->truth ? CFG_JSON_TRUE : CFG_JSON_FALSE));
    }
    if (own == CLI_SCHEMA_NULL)
    {
        return kind == CFG_JSON_NULL;
    }
    if (own == CLI_SCHEMA_ARRAY)
    {
        if ((kind != CFG_JSON_ARRAY) || (args->json->node[args->value].count != one->count))
        {
            return 0;
        }
        unsigned int theirs = args->json->node[args->value].first;
        for (unsigned int at = one->first; at != CLI_SCHEMA_NONE; at = args->read->node[at].next)
        {
            if ((theirs == CLI_JSON_NONE) ||
                !EMBED_CALL(cli_schema_read_json_internal.same, CliSchemaReadJsonCtx, .read = args->read, .node = at,
                            .json = args->json, .value = theirs))
            {
                return 0;
            }
            theirs = args->json->node[theirs].next;
        }
        return 1;
    }
    if ((kind != CFG_JSON_OBJECT) || (EMBED_CALL(values_keys, SchemaValuesCtx, .read = args->read,
                                                 .object = args->node) != args->json->node[args->value].count))
    {
        return 0;
    }
    for (unsigned int at = args->json->node[args->value].first; at != CLI_JSON_NONE; at = args->json->node[at].next)
    {
        const char *const name = args->json->text + args->json->node[at].key;
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        const unsigned int member = EMBED_CALL(values_member, SchemaValuesCtx, .read = args->read, .object = args->node,
                                               .key = name, .size = size);
        if ((member == CLI_SCHEMA_NONE) ||
            !EMBED_CALL(cli_schema_read_json_internal.same, CliSchemaReadJsonCtx, .read = args->read,
                        .node = args->read->member[member].value, .json = args->json, .value = at))
        {
            return 0;
        }
    }
    return 1;
}

int cli_schema_same_values(const CliSchemaReadOneCtx *args)
{
    unsigned int other = args->other;
    const CliSchemaNode *const left = &args->read->node[args->one];
    const CliSchemaNode *const right = &args->read->node[other];
    const unsigned char kind = EMBED_CALL(values_kind, SchemaValuesCtx, .node = left);
    if (kind != EMBED_CALL(values_kind, SchemaValuesCtx, .node = right))
    {
        return 0;
    }
    if (kind == CLI_SCHEMA_NUMBER)
    {
        return left->number == right->number;
    }
    if (kind == CLI_SCHEMA_TEXT)
    {
        return (left->text_size == right->text_size) &&
               (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                          .read = args->read, .at = left->text_at),
                                        .other = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                            .read = args->read, .at = right->text_at),
                                        .bytes = left->text_size}) == 0);
    }
    if (kind == CLI_SCHEMA_BOOL)
    {
        return left->truth == right->truth;
    }
    if (kind == CLI_SCHEMA_NULL)
    {
        return 1;
    }
    if (kind == CLI_SCHEMA_ARRAY)
    {
        if (left->count != right->count)
        {
            return 0;
        }
        unsigned int theirs = right->first;
        for (unsigned int at = left->first; at != CLI_SCHEMA_NONE; at = args->read->node[at].next)
        {
            if ((theirs == CLI_SCHEMA_NONE) ||
                !EMBED_CALL(cli_schema_read_one_internal.same_values, CliSchemaReadOneCtx, .read = args->read,
                            .one = at, .other = theirs))
            {
                return 0;
            }
            theirs = args->read->node[theirs].next;
        }
        return 1;
    }
    if (EMBED_CALL(values_keys, SchemaValuesCtx, .read = args->read, .object = args->one) !=
        EMBED_CALL(values_keys, SchemaValuesCtx, .read = args->read, .object = other))
    {
        return 0;
    }
    for (unsigned int at = left->first; at != CLI_SCHEMA_NONE; at = args->read->member[at].next)
    {
        const CliSchemaMember *const member = &args->read->member[at];
        const char *const key =
            EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = args->read, .at = member->key_at);
        if ((member->value == CLI_SCHEMA_NONE) ||
            (EMBED_CALL(values_member, SchemaValuesCtx, .read = args->read, .object = args->one, .key = key,
                        .size = member->key_size) != at))
        {
            continue;
        }
        const unsigned int match = EMBED_CALL(values_member, SchemaValuesCtx, .read = args->read, .object = other,
                                              .key = key, .size = member->key_size);
        if ((match == CLI_SCHEMA_NONE) ||
            !EMBED_CALL(cli_schema_read_one_internal.same_values, CliSchemaReadOneCtx, .read = args->read,
                        .one = member->value, .other = args->read->member[match].value))
        {
            return 0;
        }
    }
    return 1;
}

// `size` bytes of a value written between backticks, cut short past forty letters
EMBED_INLINE void values_cut(const SchemaValuesCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    mmgr_line_ch(args->out, '`');
    unsigned long long letters = 0ULL;
    unsigned long long at = 0ULL;
    while ((at < size) && (letters < 40u))
    {
        unsigned long long length = 0ULL;
        (void)EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = size, .at = at, .length = &length);
        at += length;
        letters += 1u;
    }
    mmgr_line_put_n(args->out, text, at);
    mmgr_line_put(args->out, (at < size) ? "\xe2\x80\xa6`" : "`");
}

void cli_schema_show(const CliSchemaJsonValueCtx *args)
{
    char *const written_cell = g_cli_cells->temporary_buf_alloc(CLI_SCHEMA_DISPLAYED_CAPACITY, &g_cli_schema_cells);
    VerbaLinea written = mmgr_line_over(written_cell, CLI_SCHEMA_DISPLAYED_CAPACITY);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json, .node = args->value,
               .out = &written, .indent = 0u, .sorted = 1);
    EMBED_CALL(values_cut, SchemaValuesCtx, .text = written.out, .size = mmgr_line_ok(&written) ? written.at : 0u,
               .out = args->out);
    g_cli_cells->persistent_buf_release(written.out, &g_cli_schema_cells);
}

// a file's value written as serde_json writes the value it reads it as, an object's keys in order, the last of a key
static void values_write(const SchemaValuesCtx *args)
{
    const CliSchemaRead *read = args->read;
    VerbaLinea *out = args->out;
    const CliSchemaNode *const one = &read->node[args->node_u32];
    const unsigned char kind = EMBED_CALL(values_kind, SchemaValuesCtx, .node = one);
    switch (kind)
    {
    case CLI_SCHEMA_NUMBER: {
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out, .value_f64 = one->number);
        break;
    }
    case CLI_SCHEMA_TEXT: {
        EMBED_CALL(
            cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
            .text = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = read, .at = one->text_at),
            .length = one->text_size);
        break;
    }
    case CLI_SCHEMA_BOOL: {
        mmgr_line_put(out, one->truth ? "true" : "false");
        break;
    }
    case CLI_SCHEMA_NULL: {
        mmgr_line_put(out, "null");
        break;
    }
    case CLI_SCHEMA_ARRAY: {
        mmgr_line_ch(out, '[');
        for (unsigned int at = one->first; at != CLI_SCHEMA_NONE; at = read->node[at].next)
        {
            mmgr_line_put(out, (at != one->first) ? "," : "");
            EMBED_CALL(values_write, SchemaValuesCtx, .read = read, .node_u32 = at, .out = out);
        }
        mmgr_line_ch(out, ']');
        break;
    }
    default: {
        mmgr_line_ch(out, '{');
        const char *previous = NULL;
        unsigned int previous_size = 0u;
        for (int first = 1;; first = 0)
        {
            unsigned int next = CLI_SCHEMA_NONE;
            for (unsigned int at = one->first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
            {
                const CliSchemaMember *const member = &read->member[at];
                const char *const key = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = read,
                                                   .at = member->key_at);
                if ((member->value == CLI_SCHEMA_NONE) ||
                    (EMBED_CALL(values_member, SchemaValuesCtx, .read = read, .object = args->node_u32, .key = key,
                                .size = member->key_size) != at))
                {
                    continue;
                }
                const int after = (previous == NULL) || (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                                    .one = key, .other_pointer = previous) > 0);
                const int before =
                    (next == CLI_SCHEMA_NONE) ||
                    (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = key,
                                .other_pointer = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                            .read = read, .at = read->member[next].key_at)) < 0);
                next = (after && before) ? at : next;
            }
            if (next == CLI_SCHEMA_NONE)
            {
                break;
            }
            const CliSchemaMember *const member = &read->member[next];
            previous =
                EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = read, .at = member->key_at);
            previous_size = member->key_size;
            mmgr_line_put(out, first ? "" : ",");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = previous,
                       .length = previous_size);
            mmgr_line_ch(out, ':');
            EMBED_CALL(values_write, SchemaValuesCtx, .read = read, .node_u32 = member->value, .out = out);
        }
        mmgr_line_ch(out, '}');
        break;
    }
    }
}

void cli_schema_displayed_value(const CliSchemaReadNodeCtx *args)
{
    char *const written_cell = g_cli_cells->temporary_buf_alloc(CLI_SCHEMA_DISPLAYED_CAPACITY, &g_cli_schema_cells);
    VerbaLinea written = mmgr_line_over(written_cell, CLI_SCHEMA_DISPLAYED_CAPACITY);
    EMBED_CALL(values_write, SchemaValuesCtx, .read = args->read, .node_u32 = args->node, .out = &written);
    EMBED_CALL(values_cut, SchemaValuesCtx, .text = written.out, .size = mmgr_line_ok(&written) ? written.at : 0u,
               .out = args->out);
    g_cli_cells->persistent_buf_release(written.out, &g_cli_schema_cells);
}

void cli_schema_number_show(const CliSchemaOutCtx *args)
{
    char raw[64];
    VerbaLinea line = {raw, sizeof(raw), 0ULL};
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &line, .value_f64 = args->value_f64);
    unsigned long long size = line.at;
    while ((size >= 2u) && (memor.cmp(&(MemoriaCfg){.src = raw + size - 2u, .other = ".0", .bytes = 2u}) == 0))
    {
        size -= 2u;
    }
    mmgr_line_put_n(args->out, raw, size);
}

void cli_schema_kind_write(const CliSchemaSizeOutCtx *args)
{
    const int an =
        ((args->size == 6u) && (memor.cmp(&(MemoriaCfg){.src = args->kind, .other = "object", .bytes = 6u}) == 0)) ||
        ((args->size == 5u) && (memor.cmp(&(MemoriaCfg){.src = args->kind, .other = "array", .bytes = 5u}) == 0)) ||
        ((args->size == 7u) && (memor.cmp(&(MemoriaCfg){.src = args->kind, .other = "integer", .bytes = 7u}) == 0));
    if ((args->size == 4u) && (memor.cmp(&(MemoriaCfg){.src = args->kind, .other = "null", .bytes = 4u}) == 0))
    {
        mmgr_line_put(args->out, "`null`");
        return;
    }
    mmgr_line_put(args->out, an ? "an " : "a ");
    mmgr_line_put_n(args->out, args->kind, args->size);
}

int cli_schema_matches(const CliSchemaTextCtx *args)
{
    static CliRegexp s_regexp;
    static CliRegexpWorkState s_work_state;
    static CliRegexpSpan s_groups[CLI_REGEXP_GROUPS + 1u];
    static char s_err_buf[512];
    if (args->pattern_size >= CLI_SCHEMA_PATTERN)
    {
        return -1;
    }
    // the string's letters, each at least one of its bytes, and the pattern with a terminator after it
    const unsigned long long capacity = (args->size < CLI_SCHEMA_MATCHED) ? args->size : CLI_SCHEMA_MATCHED;
    unsigned int *const letters =
        g_cli_cells->temporary_buf_alloc((capacity + 1ULL) * sizeof(*letters), &g_cli_schema_cells);
    char *const source = g_cli_cells->persistent_buf_alloc(args->pattern_size + 1ULL, &g_cli_schema_cells);
    int found = -1;
    if ((letters != NULL) && (source != NULL))
    {
        memor.cpy(&(MemoriaCfg){.dst = source, .src = args->pattern, .bytes = (size_t)args->pattern_size});
        source[args->pattern_size] = '\0';
        EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &s_regexp,
                   .work_state = &s_work_state);
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
        if (EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &s_regexp, .source = source,
                       .flags = "u", .err_msg = &err_msg))
        {
            const unsigned long long length =
                EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->text, .size = args->size,
                           .letters = letters, .capacity_u64 = capacity);
            found = 0;
            unsigned long long at = 0ULL;
            while (!found && (at <= length))
            {
                found = EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &s_regexp,
                                   .text_u32s = letters, .length = length, .at = at, .groups = s_groups);
                at += 1u;
            }
        }
    }
    g_cli_cells->persistent_buf_release(source, &g_cli_schema_cells);
    g_cli_cells->persistent_buf_release(letters, &g_cli_schema_cells);
    return found;
}
