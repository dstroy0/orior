// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "cfg_json.h"

#include <string.h>

typedef struct
{
    const char *text;
    size_t length;
    size_t at;
    CfgJsonToken *tokens;
    unsigned int capacity;
    unsigned int count;
    const char *reason;
    int fractions;
} CfgJsonWalk;

static void cfg_json_space(CfgJsonWalk *walk)
{
    while ((walk->at < walk->length) && ((walk->text[walk->at] == ' ') || (walk->text[walk->at] == '\t') ||
                                         (walk->text[walk->at] == '\n') || (walk->text[walk->at] == '\r')))
    {
        walk->at += 1u;
    }
}

static unsigned int cfg_json_token(CfgJsonWalk *walk, CfgJsonKind kind, size_t start, size_t end)
{
    if (walk->count == walk->capacity)
    {
        walk->reason = "more tokens than the reader holds";
        return 0u;
    }
    const unsigned int slot = walk->count;
    walk->tokens[slot].kind = kind;
    walk->tokens[slot].start = start;
    walk->tokens[slot].end = end;
    walk->tokens[slot].count = 0u;
    walk->tokens[slot].next = slot + 1u;
    walk->count += 1u;
    return slot + 1u;
}

static int cfg_json_literal(CfgJsonWalk *walk, const char *defined, CfgJsonKind kind)
{
    const size_t size = strlen(defined);
    const int matches = ((walk->length - walk->at) >= size) && (memcmp(&walk->text[walk->at], defined, size) == 0);
    walk->reason = matches ? walk->reason : "a value the scheme does not know";
    const unsigned int made = matches ? cfg_json_token(walk, kind, walk->at, walk->at + size) : 0u;
    walk->at += matches ? size : 0u;
    return made != 0u;
}

static int cfg_json_quoted(CfgJsonWalk *walk)
{
    walk->at += 1u;
    const size_t start = walk->at;
    while ((walk->at < walk->length) && (walk->text[walk->at] != '"'))
    {
        const unsigned char byte = (unsigned char)walk->text[walk->at];
        if (byte < 0x20u)
        {
            walk->reason = "a control character inside a string";
            return 0;
        }
        walk->at += (byte == '\\') ? 2u : 1u;
    }
    if (walk->at >= walk->length)
    {
        walk->reason = "a string that never closes";
        return 0;
    }
    const unsigned int made = cfg_json_token(walk, CFG_JSON_STRING, start, walk->at);
    walk->at += 1u;
    return made != 0u;
}

// whether the letter at the walk's place is a digit
static int cfg_json_digit_at(const CfgJsonWalk *walk)
{
    return (walk->at < walk->length) && (walk->text[walk->at] >= '0') && (walk->text[walk->at] <= '9');
}

// the digits at the walk's place passed over: how many
static size_t cfg_json_digits(CfgJsonWalk *walk)
{
    const size_t from = walk->at;
    while (cfg_json_digit_at(walk))
    {
        walk->at += 1u;
    }
    return walk->at - from;
}

// a number as RFC 8259 writes one (section 6), a minus, then 0 or digits not opening with 0, where fractions are
// taken a fraction and an exponent after them each of one digit or more, and where they are not an integer alone
static int cfg_json_number(CfgJsonWalk *walk)
{
    const size_t start = walk->at;
    walk->at += (walk->text[walk->at] == '-') ? 1u : 0u;
    const size_t integer = walk->at;
    const size_t digits = cfg_json_digits(walk);
    if (digits == 0u)
    {
        walk->reason = "a number with no digits";
        return 0;
    }
    if ((walk->fractions != 0) && (digits > 1u) && (walk->text[integer] == '0'))
    {
        walk->reason = "a number whose digits open with 0";
        return 0;
    }
    const int fraction = (walk->at < walk->length) && (walk->text[walk->at] == '.');
    if (fraction && (walk->fractions != 0))
    {
        walk->at += 1u;
        if (cfg_json_digits(walk) == 0u)
        {
            walk->reason = "a fraction with no digits";
            return 0;
        }
    }
    const int exponent = (walk->at < walk->length) && ((walk->text[walk->at] == 'e') || (walk->text[walk->at] == 'E'));
    if (exponent && (walk->fractions != 0))
    {
        walk->at += 1u;
        walk->at +=
            ((walk->at < walk->length) && ((walk->text[walk->at] == '+') || (walk->text[walk->at] == '-'))) ? 1u : 0u;
        if (cfg_json_digits(walk) == 0u)
        {
            walk->reason = "an exponent with no digits";
            return 0;
        }
    }
    if ((walk->fractions == 0) && (fraction || exponent))
    {
        walk->reason = "a number that is not an integer; a .cfg takes integers only";
        return 0;
    }
    return cfg_json_token(walk, CFG_JSON_NUMBER, start, walk->at) != 0u;
}

static int cfg_json_walk(const char *text, size_t length, CfgJsonToken *tokens, unsigned int capacity,
                         CfgJsonParse *parse, int fractions)
{
    CfgJsonWalk walk = {text, length, 0u, tokens, capacity, 0u, NULL, fractions};
    unsigned int open[CFG_JSON_DEPTH];
    unsigned int depth = 0u;
    int expect_key = 0;
    int done = 0;
    cfg_json_space(&walk);
    while ((walk.reason == NULL) && !done)
    {
        cfg_json_space(&walk);
        if (walk.at >= walk.length)
        {
            walk.reason = "the text ends inside a value";
            break;
        }
        const char byte = walk.text[walk.at];
        const CfgJsonToken *const parent = (depth > 0u) ? &walk.tokens[open[depth - 1u]] : NULL;
        const int closes = (parent != NULL) && (((byte == '}') && (parent->kind == CFG_JSON_OBJECT)) ||
                                                ((byte == ']') && (parent->kind == CFG_JSON_ARRAY)));
        if (closes)
        {
            depth -= 1u;
            walk.tokens[open[depth]].end = walk.at + 1u;
            walk.tokens[open[depth]].next = walk.count;
            walk.at += 1u;
            done = (depth == 0u);
            expect_key = 0;
            cfg_json_space(&walk);
            if (!done && (walk.at < walk.length) && (walk.text[walk.at] == ','))
            {
                walk.at += 1u;
                expect_key = (walk.tokens[open[depth - 1u]].kind == CFG_JSON_OBJECT);
            }
            continue;
        }
        if (expect_key ||
            ((parent != NULL) && (parent->kind == CFG_JSON_OBJECT) && (walk.count == open[depth - 1u] + 1u)))
        {
            if (byte != '"')
            {
                walk.reason = "a member with no quoted name";
                break;
            }
            if (!cfg_json_quoted(&walk))
            {
                break;
            }
            walk.tokens[open[depth - 1u]].count += 1u;
            cfg_json_space(&walk);
            if ((walk.at >= walk.length) || (walk.text[walk.at] != ':'))
            {
                walk.reason = "a name with no colon after it";
                break;
            }
            walk.at += 1u;
            expect_key = 0;
            continue;
        }
        if ((parent != NULL) && (parent->kind == CFG_JSON_ARRAY))
        {
            walk.tokens[open[depth - 1u]].count += 1u;
        }
        if ((byte == '{') || (byte == '['))
        {
            if (depth == CFG_JSON_DEPTH)
            {
                walk.reason = "nesting deeper than a .cfg may go";
                break;
            }
            const unsigned int made =
                cfg_json_token(&walk, (byte == '{') ? CFG_JSON_OBJECT : CFG_JSON_ARRAY, walk.at, walk.at);
            if (made == 0u)
            {
                break;
            }
            open[depth] = made - 1u;
            depth += 1u;
            walk.at += 1u;
            expect_key = 0;
            continue;
        }
        const int made = (byte == '"')                                         ? cfg_json_quoted(&walk)
                         : ((byte == '-') || ((byte >= '0') && (byte <= '9'))) ? cfg_json_number(&walk)
                         : (byte == 't') ? cfg_json_literal(&walk, "true", CFG_JSON_TRUE)
                         : (byte == 'f') ? cfg_json_literal(&walk, "false", CFG_JSON_FALSE)
                         : (byte == 'n') ? cfg_json_literal(&walk, "null", CFG_JSON_NULL)
                                         : (walk.reason = "a value the scheme does not know", 0);
        if (!made)
        {
            break;
        }
        done = (depth == 0u);
        cfg_json_space(&walk);
        if (!done && (walk.at < walk.length) && (walk.text[walk.at] == ','))
        {
            walk.at += 1u;
            expect_key = (walk.tokens[open[depth - 1u]].kind == CFG_JSON_OBJECT);
        }
        else if (!done && (walk.at < walk.length) && (walk.text[walk.at] != '}') && (walk.text[walk.at] != ']'))
        {
            walk.reason = "a value with no comma or close after it";
        }
    }
    cfg_json_space(&walk);
    if ((walk.reason == NULL) && (walk.at != walk.length))
    {
        walk.reason = "text after the value";
    }
    size_t line = 1u;
    size_t column = 1u;
    for (size_t byte = 0u; (walk.reason != NULL) && (byte < walk.at) && (byte < walk.length); byte += 1u)
    {
        const int newline = (walk.text[byte] == '\n');
        line += (size_t)newline;
        column = newline ? 1u : (column + 1u);
    }
    parse->tokens = walk.count;
    parse->reason = walk.reason;
    parse->line = (walk.reason != NULL) ? line : 0u;
    parse->column = (walk.reason != NULL) ? column : 0u;
    return walk.reason == NULL;
}

int cfg_json_parse(const char *text, size_t length, CfgJsonToken *tokens, unsigned int capacity, CfgJsonParse *parse)
{
    return cfg_json_walk(text, length, tokens, capacity, parse, 0);
}

int cfg_json_parse_metadata(const char *text, size_t length, CfgJsonToken *tokens, unsigned int capacity,
                            CfgJsonParse *parse)
{
    return cfg_json_walk(text, length, tokens, capacity, parse, 1);
}

int cfg_json_names(const char *text, const CfgJsonToken *token, const char *name)
{
    const size_t size = strlen(name);
    return (token->kind == CFG_JSON_STRING) && ((token->end - token->start) == size) &&
           (memcmp(&text[token->start], name, size) == 0);
}

int cfg_json_unsigned(const char *text, const CfgJsonToken *token, unsigned long long *value)
{
    unsigned long long total = 0ULL;
    int fits = (token->kind == CFG_JSON_NUMBER) && (text[token->start] != '-');
    for (size_t at = token->start; fits && (at < token->end); at += 1u)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        fits = (total <= ((~0ULL - digit) / 10ULL));
        total = (total * 10ULL) + digit;
    }
    *value = total;
    return fits;
}

// the four hex digits at `at` before `end`: their value, or CFG_JSON_NO_LETTER where they are not four hex digits
#define CFG_JSON_NO_LETTER 0x110000u

static unsigned int cfg_json_hex4(const char *text, size_t at, size_t end)
{
    unsigned int value = 0u;
    for (size_t step = 0u; step < 4u; step += 1u)
    {
        const char digit = (at + step < end) ? text[at + step] : '\0';
        const unsigned int nibble = ((digit >= '0') && (digit <= '9'))   ? (unsigned int)(digit - '0')
                                    : ((digit >= 'a') && (digit <= 'f')) ? (unsigned int)(digit - 'a') + 10u
                                    : ((digit >= 'A') && (digit <= 'F')) ? (unsigned int)(digit - 'A') + 10u
                                                                         : 16u;
        if (nibble == 16u)
        {
            return CFG_JSON_NO_LETTER;
        }
        value = (value << 4u) | nibble;
    }
    return value;
}

// the escape at `at`, its backslash first: the letter it writes, and its length into `step`; CFG_JSON_NO_LETTER where
// it is not one JSON writes. A high surrogate followed by an escaped low one is the one letter the two write
static unsigned int cfg_json_escape(const char *text, size_t at, size_t end, size_t *step)
{
    const char next = (at + 1u < end) ? text[at + 1u] : '\0';
    *step = 2u;
    switch (next)
    {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case 'b':
        return 0x08u;
    case 'f':
        return 0x0cu;
    case '"':
    case '\\':
    case '/':
        return (unsigned char)next;
    case 'u': {
        *step = 6u;
        const unsigned int first = cfg_json_hex4(text, at + 2u, end);
        if ((first < 0xd800u) || (first > 0xdfffu) || (first == CFG_JSON_NO_LETTER))
        {
            return first;
        }
        const unsigned int second =
            ((first <= 0xdbffu) && (at + 7u < end) && (text[at + 6u] == '\\') && (text[at + 7u] == 'u'))
                ? cfg_json_hex4(text, at + 8u, end)
                : CFG_JSON_NO_LETTER;
        if ((second < 0xdc00u) || (second > 0xdfffu))
        {
            return CFG_JSON_NO_LETTER;
        }
        *step = 12u;
        return 0x10000u + ((first - 0xd800u) << 10u) + (second - 0xdc00u);
    }
    default:
        return CFG_JSON_NO_LETTER;
    }
}

int cfg_json_string(const char *text, const CfgJsonToken *token, char *out, size_t capacity)
{
    size_t written = 0u;
    int ok = (token->kind == CFG_JSON_STRING) && (capacity > 0u);
    size_t at = token->start;
    while (ok && (at < token->end))
    {
        if (text[at] != '\\')
        {
            ok = (written + 1u < capacity);
            out[written] = ok ? text[at] : out[written];
            written += (size_t)ok;
            at += 1u;
            continue;
        }
        size_t step = 0u;
        const unsigned int letter = cfg_json_escape(text, at, token->end, &step);
        // a letter written in UTF-8: one byte below 0x80, two below 0x800, three below 0x10000, four past
        const size_t bytes = (letter < 0x80u) ? 1u : (letter < 0x800u) ? 2u : (letter < 0x10000u) ? 3u : 4u;
        ok = (letter != 0u) && (letter != CFG_JSON_NO_LETTER) && (written + bytes < capacity);
        if (ok)
        {
            // each byte is the letter's bits in turn under its UTF-8 mark, which a char holds
            static const unsigned int s_marks[5] = {0x00u, 0x00u, 0xc0u, 0xe0u, 0xf0u};
            for (size_t place = bytes; place > 1u; place -= 1u)
            {
                out[written + place - 1u] = (char)(0x80u | ((letter >> (6u * (bytes - place))) & 0x3fu));
            }
            out[written] = (char)(s_marks[bytes] | (letter >> (6u * (bytes - 1u))));
            written += bytes;
        }
        at += step;
    }
    out[(written < capacity) ? written : 0u] = '\0';
    return ok;
}

unsigned int cfg_json_member(const char *text, const CfgJsonToken *tokens, unsigned int object, const char *name)
{
    unsigned int found = 0u;
    unsigned int key = object + 1u;
    for (unsigned int member = 0u; (tokens[object].kind == CFG_JSON_OBJECT) && (member < tokens[object].count);
         member += 1u)
    {
        const unsigned int value = key + 1u;
        found = cfg_json_names(text, &tokens[key], name) ? value : found;
        key = tokens[value].next;
    }
    return found;
}
