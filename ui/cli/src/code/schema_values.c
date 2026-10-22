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
// begun to be written in before it is cut short, doubling as it is written
#define CLI_SCHEMA_MATCHED (1u << 20u)
#define CLI_SCHEMA_PATTERN (1u << 14u)
#define CLI_SCHEMA_SHOWN_FIRST 256u

const char *cli_schema_text(const CliSchemaRead *read, unsigned int at)
{
    return (at < read->used) ? read->pool + at : "";
}

const char *cli_schema_kind(const CliSchemaNode *node)
{
    static const char *const s_kinds[7] = {"", "object", "array", "string", "number", "boolean", "null"};
    if ((node->kind == CLI_SCHEMA_NUMBER) && isfinite(node->number) && (floor(node->number) == node->number))
    {
        return "integer";
    }
    return s_kinds[node->kind];
}

unsigned int cli_schema_get(const CliJson *json, unsigned int map, const char *name)
{
    return ((map != CLI_JSON_NONE) && cli_json_is(json, map, CFG_JSON_OBJECT)) ? cli_json_member(json, map, name)
                                                                               : CLI_JSON_NONE;
}

const char *cli_schema_string(const CliJson *json, unsigned int map, const char *name, unsigned long long *size)
{
    return cli_json_string(json, cli_schema_get(json, map, name), size);
}

int cli_schema_true(const CliJson *json, unsigned int node)
{
    return (node != CLI_JSON_NONE) && cli_json_is(json, node, CFG_JSON_TRUE);
}

int cli_schema_false(const CliJson *json, unsigned int node)
{
    return (node != CLI_JSON_NONE) && cli_json_is(json, node, CFG_JSON_FALSE);
}

int cli_schema_number(const CliJson *json, unsigned int node, double *value)
{
    if ((node == CLI_JSON_NONE) || !cli_json_is(json, node, CFG_JSON_NUMBER))
    {
        return 0;
    }
    char raw[512];
    const CliJsonNode *const one = &json->node[node];
    const unsigned long long size = (one->length < sizeof(raw)) ? one->length : sizeof(raw) - 1u;
    memor.cpy(&(MemoriaCfg){.dst = raw, .src = json->text + one->text, .bytes = (size_t)size});
    raw[size] = '\0';
    *value = strtod(raw, NULL);
    return 1;
}

int cli_schema_u64(const CliJson *json, unsigned int node, unsigned long long *value)
{
    if ((node == CLI_JSON_NONE) || !cli_json_is(json, node, CFG_JSON_NUMBER))
    {
        return 0;
    }
    const CliJsonNode *const one = &json->node[node];
    const char *const text = json->text + one->text;
    unsigned long long read = 0ull;
    for (unsigned long long at = 0ull; at < one->length; at += 1u)
    {
        if ((text[at] < '0') || (text[at] > '9') || (read > (~0ull - 9u) / 10u))
        {
            return 0;
        }
        read = (read * 10u) + (unsigned long long)(text[at] - '0');
    }
    *value = read;
    return one->length > 0u;
}

// the last member of the file's object `object` keyed as `key` that holds a value, CLI_SCHEMA_NONE where none does
static unsigned int values_member(const CliSchemaRead *read, unsigned int object, const char *key,
                                  unsigned long long size)
{
    unsigned int found = CLI_SCHEMA_NONE;
    for (unsigned int at = read->node[object].first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        if ((member->value != CLI_SCHEMA_NONE) && (member->key_size == size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = cli_schema_text(read, member->key_at), .other = key, .bytes = (size_t)size}) == 0))
        {
            found = at;
        }
    }
    return found;
}

// how many keys of the file's object `object` hold values, each counted once
static unsigned int values_keys(const CliSchemaRead *read, unsigned int object)
{
    unsigned int count = 0u;
    for (unsigned int at = read->node[object].first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        count += ((member->value != CLI_SCHEMA_NONE) &&
                  (values_member(read, object, cli_schema_text(read, member->key_at), member->key_size) == at))
                     ? 1u
                     : 0u;
    }
    return count;
}

// the kind of a file's value as JSON's values compare, a number that is not finite read as null
static unsigned char values_kind(const CliSchemaNode *node)
{
    return ((node->kind == CLI_SCHEMA_NUMBER) && !isfinite(node->number)) ? CLI_SCHEMA_NULL : node->kind;
}

int cli_schema_same(const CliSchemaRead *read, unsigned int node, const CliJson *json, unsigned int value)
{
    const CliSchemaNode *const one = &read->node[node];
    const CfgJsonKind kind = json->node[value].kind;
    const unsigned char own = values_kind(one);
    if (own == CLI_SCHEMA_NUMBER)
    {
        double other = 0.0;
        return cli_schema_number(json, value, &other) && (one->number == other);
    }
    if (own == CLI_SCHEMA_TEXT)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, value, &size);
        return (text != NULL) && (size == one->text_size) &&
               (memor.cmp(&(MemoriaCfg){
                    .src = text, .other = cli_schema_text(read, one->text_at), .bytes = (size_t)size}) == 0);
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
        if ((kind != CFG_JSON_ARRAY) || (json->node[value].count != one->count))
        {
            return 0;
        }
        unsigned int theirs = json->node[value].first;
        for (unsigned int at = one->first; at != CLI_SCHEMA_NONE; at = read->node[at].next)
        {
            if ((theirs == CLI_JSON_NONE) || !cli_schema_same(read, at, json, theirs))
            {
                return 0;
            }
            theirs = json->node[theirs].next;
        }
        return 1;
    }
    if ((kind != CFG_JSON_OBJECT) || (values_keys(read, node) != json->node[value].count))
    {
        return 0;
    }
    for (unsigned int at = json->node[value].first; at != CLI_JSON_NONE; at = json->node[at].next)
    {
        const char *const name = json->text + json->node[at].key;
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        const unsigned int member = values_member(read, node, name, size);
        if ((member == CLI_SCHEMA_NONE) || !cli_schema_same(read, read->member[member].value, json, at))
        {
            return 0;
        }
    }
    return 1;
}

int cli_schema_same_values(const CliSchemaRead *read, unsigned int one, unsigned int other)
{
    const CliSchemaNode *const left = &read->node[one];
    const CliSchemaNode *const right = &read->node[other];
    const unsigned char kind = values_kind(left);
    if (kind != values_kind(right))
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
               (memor.cmp(&(MemoriaCfg){.src = cli_schema_text(read, left->text_at),
                                        .other = cli_schema_text(read, right->text_at),
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
        for (unsigned int at = left->first; at != CLI_SCHEMA_NONE; at = read->node[at].next)
        {
            if ((theirs == CLI_SCHEMA_NONE) || !cli_schema_same_values(read, at, theirs))
            {
                return 0;
            }
            theirs = read->node[theirs].next;
        }
        return 1;
    }
    if (values_keys(read, one) != values_keys(read, other))
    {
        return 0;
    }
    for (unsigned int at = left->first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        const char *const key = cli_schema_text(read, member->key_at);
        if ((member->value == CLI_SCHEMA_NONE) || (values_member(read, one, key, member->key_size) != at))
        {
            continue;
        }
        const unsigned int match = values_member(read, other, key, member->key_size);
        if ((match == CLI_SCHEMA_NONE) || !cli_schema_same_values(read, member->value, read->member[match].value))
        {
            return 0;
        }
    }
    return 1;
}

// `size` bytes of a value written between backticks, cut short past forty letters
static void values_cut(const char *text, unsigned long long size, CliLine *out)
{
    cli_line_character(out, '`');
    unsigned long long letters = 0ull;
    unsigned long long at = 0ull;
    while ((at < size) && (letters < 40u))
    {
        unsigned long long length = 0ull;
        (void)cli_text_letter(text, size, at, &length);
        at += length;
        letters += 1u;
    }
    cli_line_counted(out, text, at);
    cli_line_text(out, (at < size) ? "\xe2\x80\xa6`" : "`");
}

void cli_schema_shown(const CliJson *json, unsigned int value, CliLine *out)
{
    CliLine written;
    (void)cli_line_growing(&written, CLI_SCHEMA_SHOWN_FIRST, "a value written to be shown");
    cli_json_write(json, value, &written, 0u, 1);
    values_cut(written.out, cli_line_fits(&written) ? written.at : 0u, out);
    cli_cell_given(written.out);
}

// a file's value written as serde_json writes the value it reads it as, an object's keys in order, the last of a key
static void values_written(const CliSchemaRead *read, unsigned int node, CliLine *out)
{
    const CliSchemaNode *const one = &read->node[node];
    const unsigned char kind = values_kind(one);
    if (kind == CLI_SCHEMA_NUMBER)
    {
        cli_json_double(out, one->number);
    }
    else if (kind == CLI_SCHEMA_TEXT)
    {
        cli_json_quoted(out, cli_schema_text(read, one->text_at), one->text_size);
    }
    else if (kind == CLI_SCHEMA_BOOL)
    {
        cli_line_text(out, one->truth ? "true" : "false");
    }
    else if (kind == CLI_SCHEMA_NULL)
    {
        cli_line_text(out, "null");
    }
    else if (kind == CLI_SCHEMA_ARRAY)
    {
        cli_line_character(out, '[');
        for (unsigned int at = one->first; at != CLI_SCHEMA_NONE; at = read->node[at].next)
        {
            cli_line_text(out, (at != one->first) ? "," : "");
            values_written(read, at, out);
        }
        cli_line_character(out, ']');
    }
    else
    {
        cli_line_character(out, '{');
        const char *previous = NULL;
        unsigned int previous_size = 0u;
        for (int first = 1;; first = 0)
        {
            unsigned int next = CLI_SCHEMA_NONE;
            for (unsigned int at = one->first; at != CLI_SCHEMA_NONE; at = read->member[at].next)
            {
                const CliSchemaMember *const member = &read->member[at];
                const char *const key = cli_schema_text(read, member->key_at);
                if ((member->value == CLI_SCHEMA_NONE) || (values_member(read, node, key, member->key_size) != at))
                {
                    continue;
                }
                const int after = (previous == NULL) || (cli_text_order(key, previous) > 0);
                const int before = (next == CLI_SCHEMA_NONE) ||
                                   (cli_text_order(key, cli_schema_text(read, read->member[next].key_at)) < 0);
                next = (after && before) ? at : next;
            }
            if (next == CLI_SCHEMA_NONE)
            {
                break;
            }
            const CliSchemaMember *const member = &read->member[next];
            previous = cli_schema_text(read, member->key_at);
            previous_size = member->key_size;
            cli_line_text(out, first ? "" : ",");
            cli_json_quoted(out, previous, previous_size);
            cli_line_character(out, ':');
            values_written(read, member->value, out);
        }
        cli_line_character(out, '}');
    }
}

void cli_schema_shown_value(const CliSchemaRead *read, unsigned int node, CliLine *out)
{
    CliLine written;
    (void)cli_line_growing(&written, CLI_SCHEMA_SHOWN_FIRST, "a value written to be shown");
    values_written(read, node, &written);
    values_cut(written.out, cli_line_fits(&written) ? written.at : 0u, out);
    cli_cell_given(written.out);
}

void cli_schema_number_shown(double value, CliLine *out)
{
    char raw[64];
    CliLine line = {raw, sizeof(raw), 0ull};
    cli_json_double(&line, value);
    unsigned long long size = line.at;
    while ((size >= 2u) && (memor.cmp(&(MemoriaCfg){.src = raw + size - 2u, .other = ".0", .bytes = 2u}) == 0))
    {
        size -= 2u;
    }
    cli_line_counted(out, raw, size);
}

void cli_schema_kind_said(const char *kind, unsigned long long size, CliLine *out)
{
    const int an = ((size == 6u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "object", .bytes = 6u}) == 0)) ||
                   ((size == 5u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "array", .bytes = 5u}) == 0)) ||
                   ((size == 7u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "integer", .bytes = 7u}) == 0));
    if ((size == 4u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "null", .bytes = 4u}) == 0))
    {
        cli_line_text(out, "`null`");
        return;
    }
    cli_line_text(out, an ? "an " : "a ");
    cli_line_counted(out, kind, size);
}

int cli_schema_matches(const char *pattern, unsigned long long pattern_size, const char *text, unsigned long long size)
{
    static CliRegexp s_regexp;
    static CliRegexpRoom s_room;
    static CliRegexpSpan s_groups[CLI_REGEXP_GROUPS + 1u];
    static char s_said[512];
    static CliLock s_matches_lock = CLI_LOCK_INIT;
    if (pattern_size >= CLI_SCHEMA_PATTERN)
    {
        return -1;
    }
    // the string's letters, each at least one of its bytes, and the pattern with a terminator after it
    const unsigned long long room = (size < CLI_SCHEMA_MATCHED) ? size : CLI_SCHEMA_MATCHED;
    unsigned int *const letters =
        cli_cell_taken_by(&g_cli_schema_cells, (room + 1ull) * sizeof(*letters), "the letters a pattern is matched");
    char *const source = cli_cell_taken_by(&g_cli_schema_cells, pattern_size + 1ull, "a pattern of a schema");
    int found = -1;
    if ((letters != NULL) && (source != NULL))
    {
        cli_lock_take(&s_matches_lock);
        memor.cpy(&(MemoriaCfg){.dst = source, .src = pattern, .bytes = (size_t)pattern_size});
        source[pattern_size] = '\0';
        cli_regexp_roomed(&s_regexp, &s_room);
        CliLine said = {s_said, sizeof(s_said), 0ull};
        if (cli_regexp_read(&s_regexp, source, "u", &said))
        {
            const unsigned long long length = cli_regexp_letters(text, size, letters, room);
            found = 0;
            for (unsigned long long at = 0ull; !found && (at <= length); at += 1u)
            {
                found = cli_regexp_match_into(&s_regexp, letters, length, at, s_groups);
            }
        }
        cli_lock_give(&s_matches_lock);
    }
    cli_cell_given_by(&g_cli_schema_cells, source);
    cli_cell_given_by(&g_cli_schema_cells, letters);
    return found;
}
