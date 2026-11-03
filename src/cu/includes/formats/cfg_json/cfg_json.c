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
    unsigned int open[CFG_JSON_DEPTH];
    unsigned int depth;
    int expect_key;
    int done;
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
    if ((walk->count == walk->capacity) || (walk->tokens == NULL))
    {
        walk->reason = "more tokens than the reader supports";
        return 0u;
    }
    const unsigned int at = walk->count;
    walk->tokens[at].kind = kind;
    walk->tokens[at].start = start;
    walk->tokens[at].end = end;
    walk->tokens[at].count = 0u;
    walk->tokens[at].next = at + 1u;
    walk->count += 1u;
    return at + 1u;
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

// the walk past a value or a close: done where nothing stays open; else past a comma, a key expected next inside an
// object, or before a close; anything else a value with no comma or close after it
static void cfg_json_after(CfgJsonWalk *walk)
{
    walk->done = (walk->depth == 0u);
    cfg_json_space(walk);
    if (walk->done || (walk->at >= walk->length))
    {
        return;
    }
    if (walk->text[walk->at] == ',')
    {
        walk->at += 1u;
        walk->expect_key = (walk->tokens[walk->open[walk->depth - 1u]].kind == CFG_JSON_OBJECT);
    }
    else if ((walk->text[walk->at] != '}') && (walk->text[walk->at] != ']'))
    {
        walk->reason = "a value with no comma or close after it";
    }
}

// the object or array open closed at the walk's place: its end and the token after it set, and the walk past it
static void cfg_json_closed(CfgJsonWalk *walk)
{
    walk->depth -= 1u;
    const unsigned int closed = walk->open[walk->depth];
    walk->tokens[closed].end = walk->at + 1u;
    walk->tokens[closed].next = walk->count;
    walk->at += 1u;
    walk->expect_key = 0;
    cfg_json_after(walk);
}

// a member's name read at the walk's place, `byte` there, counted in its object, and its colon passed; why it fails
// into `reason`
static void cfg_json_named(CfgJsonWalk *walk, char byte)
{
    if (byte != '"')
    {
        walk->reason = "a member with no quoted name";
        return;
    }
    if (!cfg_json_quoted(walk))
    {
        return;
    }
    walk->tokens[walk->open[walk->depth - 1u]].count += 1u;
    cfg_json_space(walk);
    if ((walk->at >= walk->length) || (walk->text[walk->at] != ':'))
    {
        walk->reason = "a name with no colon after it";
        return;
    }
    walk->at += 1u;
    walk->expect_key = 0;
}

// an object or an array opened at the walk's place, `byte` there; why it fails into `reason`
static void cfg_json_opened(CfgJsonWalk *walk, char byte)
{
    if (walk->depth == CFG_JSON_DEPTH)
    {
        walk->reason = "nesting deeper than a .cfg may go";
        return;
    }
    const unsigned int made =
        cfg_json_token(walk, (byte == '{') ? CFG_JSON_OBJECT : CFG_JSON_ARRAY, walk->at, walk->at);
    if (made == 0u)
    {
        return;
    }
    walk->open[walk->depth] = made - 1u;
    walk->depth += 1u;
    walk->at += 1u;
    walk->expect_key = 0;
}

// a value that opens no object or array read at the walk's place, `byte` there: a string, a number or a literal; 1, or
// 0 with why it fails in `reason`
static int cfg_json_value(CfgJsonWalk *walk, char byte)
{
    if (byte == '"')
    {
        return cfg_json_quoted(walk);
    }
    if ((byte == '-') || ((byte >= '0') && (byte <= '9')))
    {
        return cfg_json_number(walk);
    }
    if (byte == 't')
    {
        return cfg_json_literal(walk, "true", CFG_JSON_TRUE);
    }
    if (byte == 'f')
    {
        return cfg_json_literal(walk, "false", CFG_JSON_FALSE);
    }
    if (byte == 'n')
    {
        return cfg_json_literal(walk, "null", CFG_JSON_NULL);
    }
    walk->reason = "a value the scheme does not know";
    return 0;
}

// one step of the walk at its place, which holds a byte: the close of what stands open, a member's name, an opening or
// a value; why it fails into `reason`
static void cfg_json_step(CfgJsonWalk *walk)
{
    const char byte = walk->text[walk->at];
    const CfgJsonToken *const parent = (walk->depth > 0u) ? &walk->tokens[walk->open[walk->depth - 1u]] : NULL;
    const int closes = (parent != NULL) && (((byte == '}') && (parent->kind == CFG_JSON_OBJECT)) ||
                                            ((byte == ']') && (parent->kind == CFG_JSON_ARRAY)));
    if (closes)
    {
        cfg_json_closed(walk);
        return;
    }
    if (walk->expect_key ||
        ((parent != NULL) && (parent->kind == CFG_JSON_OBJECT) && (walk->count == walk->open[walk->depth - 1u] + 1u)))
    {
        cfg_json_named(walk, byte);
        return;
    }
    if ((parent != NULL) && (parent->kind == CFG_JSON_ARRAY))
    {
        walk->tokens[walk->open[walk->depth - 1u]].count += 1u;
    }
    if ((byte == '{') || (byte == '['))
    {
        cfg_json_opened(walk, byte);
        return;
    }
    if (cfg_json_value(walk, byte))
    {
        cfg_json_after(walk);
    }
}

static int cfg_json_walk(const char *text, size_t length, CfgJsonToken *tokens, unsigned int capacity,
                         CfgJsonParse *parse, int fractions)
{
    CfgJsonWalk walk = {text, length, 0u, tokens, capacity, 0u, NULL, fractions, {0u}, 0u, 0, 0};
    cfg_json_space(&walk);
    while ((walk.reason == NULL) && !walk.done)
    {
        cfg_json_space(&walk);
        if (walk.at >= walk.length)
        {
            walk.reason = "the text ends inside a value";
        }
        else
        {
            cfg_json_step(&walk);
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

// a hex digit's value; 16 where `digit` is none
static unsigned int cfg_json_nibble(char digit)
{
    if ((digit >= '0') && (digit <= '9'))
    {
        return (unsigned int)(digit - '0');
    }
    if ((digit >= 'a') && (digit <= 'f'))
    {
        return (unsigned int)(digit - 'a') + 10u;
    }
    if ((digit >= 'A') && (digit <= 'F'))
    {
        return (unsigned int)(digit - 'A') + 10u;
    }
    return 16u;
}

static unsigned int cfg_json_hex4(const char *text, size_t at, size_t end)
{
    unsigned int value = 0u;
    for (size_t step = 0u; step < 4u; step += 1u)
    {
        const char digit = (at + step < end) ? text[at + step] : '\0';
        const unsigned int nibble = cfg_json_nibble(digit);
        if (nibble == 16u)
        {
            return CFG_JSON_NO_LETTER;
        }
        value = (value << 4u) | nibble;
    }
    return value;
}

// the escape at `at`, its backslash first: the letter it writes, and its length into `step`; CFG_JSON_NO_LETTER where
// it is not one JSON writes. A high surrogate followed by an escaped low one is the single letter the two write
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

// the bytes a letter takes in UTF-8: one below 0x80, two below 0x800, three below 0x10000, four past
static size_t cfg_json_utf8_bytes(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return 1u;
    }
    if (letter < 0x800u)
    {
        return 2u;
    }
    return (letter < 0x10000u) ? 3u : 4u;
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
        const size_t bytes = cfg_json_utf8_bytes(letter);
        ok = (letter != 0u) && (letter != CFG_JSON_NO_LETTER) && (written + bytes < capacity);
        if (ok)
        {
            // each byte is the letter's bits in turn under its UTF-8 mark, which a char represents
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
