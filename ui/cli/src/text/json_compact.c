// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_compact.c: JSON read and written again as serde_json writes a Value it read, with no tree: on one line, each
// string as serde_json escapes it, each number as the integer or the float serde_json reads it as
#include "json_tree.h"
#include "text.h"

#include <stdlib.h>

// the deepest arrays and objects nest, as serde_json's recursion limit has it
#define CLI_COMPACT_DEPTH 128u

// A reading: the text, its size, where it stands, and how deep it is
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    unsigned int depth;
} CliCompact;

static int cli_compact_value(CliCompact *read, CliLine *out);

// the whitespace JSON allows passed over
static void cli_compact_white(CliCompact *read)
{
    while ((read->at < read->size) && ((read->text[read->at] == ' ') || (read->text[read->at] == '\t') ||
                                       (read->text[read->at] == '\n') || (read->text[read->at] == '\r')))
    {
        read->at += 1ull;
    }
}

// four hex digits from `at`: their value, or more than 0xFFFF where they are not four hex digits
static unsigned int cli_compact_hex(const CliCompact *read, unsigned long long at)
{
    if (at + 4ull > read->size)
    {
        return 0x10000u;
    }
    unsigned int value = 0u;
    for (unsigned long long digit = at; digit < at + 4ull; digit += 1ull)
    {
        const char letter = read->text[digit];
        const unsigned int part = ((letter >= '0') && (letter <= '9'))   ? (unsigned int)(letter - '0')
                                  : ((letter >= 'a') && (letter <= 'f')) ? (unsigned int)(letter - 'a' + 10)
                                  : ((letter >= 'A') && (letter <= 'F')) ? (unsigned int)(letter - 'A' + 10)
                                                                         : 16u;
        if (part > 15u)
        {
            return 0x10000u;
        }
        value = (value << 4u) | part;
    }
    return value;
}

// a letter written as JSON writes it inside a string
static void cli_compact_letter(CliLine *out, unsigned int letter)
{
    char bytes[8];
    CliLine line = {bytes, sizeof(bytes), 0ull};
    cli_utf8_put(&line, letter);
    cli_json_escaped_lossy(out, (const unsigned char *)bytes, line.at);
}

// a string, the read at its opening quote, written again: 1, or 0 where it is not one serde_json reads
static int cli_compact_string(CliCompact *read, CliLine *out)
{
    read->at += 1ull;
    cli_line_character(out, '"');
    unsigned long long from = read->at;
    while (read->at < read->size)
    {
        const unsigned char letter = (unsigned char)read->text[read->at];
        if ((letter == '"') || (letter == '\\') || (letter < 0x20u))
        {
            cli_json_escaped_lossy(out, (const unsigned char *)read->text + from, read->at - from);
        }
        if (letter == '"')
        {
            read->at += 1ull;
            cli_line_character(out, '"');
            return 1;
        }
        if (letter < 0x20u)
        {
            return 0;
        }
        if (letter >= 0x80u)
        {
            unsigned int whole = 0u;
            const unsigned long long taken =
                cli_lossy_letter((const unsigned char *)read->text, read->size, read->at, &whole);
            if ((whole == 0xFFFDu) &&
                ((taken != 3ull) ||
                 (memor.cmp(&(MemoriaCfg){.src = read->text + read->at, .other = "\xEF\xBF\xBD", .bytes = 3u}) != 0)))
            {
                return 0;
            }
            read->at += taken;
            continue;
        }
        if (letter != '\\')
        {
            read->at += 1ull;
            continue;
        }
        if (read->at + 1ull >= read->size)
        {
            return 0;
        }
        const char escape = read->text[read->at + 1ull];
        const char *const plain = (escape == '"')    ? "\""
                                  : (escape == '\\') ? "\\"
                                  : (escape == '/')  ? "/"
                                  : (escape == 'b')  ? "\b"
                                  : (escape == 'f')  ? "\f"
                                  : (escape == 'n')  ? "\n"
                                  : (escape == 'r')  ? "\r"
                                  : (escape == 't')  ? "\t"
                                                     : NULL;
        if (plain != NULL)
        {
            cli_json_escaped_lossy(out, (const unsigned char *)plain, 1ull);
            read->at += 2ull;
            from = read->at;
            continue;
        }
        if (escape != 'u')
        {
            return 0;
        }
        unsigned int code = cli_compact_hex(read, read->at + 2ull);
        read->at += 6ull;
        if (code > 0xFFFFu)
        {
            return 0;
        }
        if ((code >= 0xD800u) && (code <= 0xDBFFu))
        {
            // a leading surrogate is whole only with a trailing one escaped after it
            const unsigned int trail = ((read->at + 1ull < read->size) && (read->text[read->at] == '\\') &&
                                        (read->text[read->at + 1ull] == 'u'))
                                           ? cli_compact_hex(read, read->at + 2ull)
                                           : 0x10000u;
            if ((trail < 0xDC00u) || (trail > 0xDFFFu))
            {
                return 0;
            }
            code = 0x10000u + ((code - 0xD800u) << 10u) + (trail - 0xDC00u);
            read->at += 6ull;
        }
        else if ((code >= 0xDC00u) && (code <= 0xDFFFu))
        {
            return 0;
        }
        cli_compact_letter(out, code);
        from = read->at;
    }
    return 0;
}

// the digits from `at`: their count
static unsigned long long cli_compact_digits(const CliCompact *read, unsigned long long at)
{
    unsigned long long count = 0ull;
    while ((at + count < read->size) && (read->text[at + count] >= '0') && (read->text[at + count] <= '9'))
    {
        count += 1ull;
    }
    return count;
}

// a number written again as serde_json reads it: an integer that fits 64 bits as that integer, -0 and any other as
// a float: 1, or 0 where it is not a JSON number or its float is out of range
static int cli_compact_number(CliCompact *read, CliLine *out)
{
    const unsigned long long start = read->at;
    const int negative = (read->text[read->at] == '-');
    read->at += negative ? 1ull : 0ull;
    const unsigned long long whole = cli_compact_digits(read, read->at);
    if ((whole == 0ull) || ((whole > 1ull) && (read->text[read->at] == '0')))
    {
        return 0;
    }
    const unsigned long long digits_at = read->at;
    read->at += whole;
    int integer = 1;
    if ((read->at < read->size) && (read->text[read->at] == '.'))
    {
        const unsigned long long part = cli_compact_digits(read, read->at + 1ull);
        if (part == 0ull)
        {
            return 0;
        }
        read->at += 1ull + part;
        integer = 0;
    }
    if ((read->at < read->size) && ((read->text[read->at] == 'e') || (read->text[read->at] == 'E')))
    {
        read->at += 1ull;
        read->at +=
            ((read->at < read->size) && ((read->text[read->at] == '+') || (read->text[read->at] == '-'))) ? 1ull : 0ull;
        const unsigned long long power = cli_compact_digits(read, read->at);
        if (power == 0ull)
        {
            return 0;
        }
        read->at += power;
        integer = 0;
    }
    unsigned long long value = 0ull;
    int fits = integer;
    for (unsigned long long at = digits_at; fits && (at < digits_at + whole); at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(read->text[at] - '0');
        fits = (value <= (18446744073709551615ull - digit) / 10ull);
        value = fits ? (value * 10ull) + digit : value;
    }
    if (fits && !negative)
    {
        cli_line_decimal(out, value, 1u);
        return 1;
    }
    if (fits && (value != 0ull) && (value <= 9223372036854775808ull))
    {
        cli_line_character(out, '-');
        cli_line_decimal(out, value, 1u);
        return 1;
    }
    char copy[1024];
    const unsigned long long length = read->at - start;
    if (length >= sizeof(copy))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = copy, .src = read->text + start, .bytes = length});
    copy[length] = '\0';
    const double number = strtod(copy, NULL);
    unsigned long long bits = 0ull;
    memor.cpy(&(MemoriaCfg){.dst = &bits, .src = &number, .bytes = sizeof(bits)});
    if (((bits >> 52u) & 0x7ffull) == 0x7ffull)
    {
        return 0;
    }
    cli_json_double(out, number);
    return 1;
}

// the members of an object or the items of an array, the read at its opening, written again
static int cli_compact_many(CliCompact *read, CliLine *out, int object)
{
    if (read->depth >= CLI_COMPACT_DEPTH)
    {
        return 0;
    }
    read->depth += 1u;
    const char close = object ? '}' : ']';
    cli_line_character(out, object ? '{' : '[');
    read->at += 1ull;
    cli_compact_white(read);
    int first = 1;
    while ((read->at < read->size) && (read->text[read->at] != close))
    {
        if (!first)
        {
            if (read->text[read->at] != ',')
            {
                return 0;
            }
            read->at += 1ull;
            cli_compact_white(read);
            cli_line_character(out, ',');
        }
        first = 0;
        if (object)
        {
            if ((read->at >= read->size) || (read->text[read->at] != '"') || !cli_compact_string(read, out))
            {
                return 0;
            }
            cli_compact_white(read);
            if ((read->at >= read->size) || (read->text[read->at] != ':'))
            {
                return 0;
            }
            read->at += 1ull;
            cli_line_character(out, ':');
        }
        if (!cli_compact_value(read, out))
        {
            return 0;
        }
        cli_compact_white(read);
    }
    if (read->at >= read->size)
    {
        return 0;
    }
    read->at += 1ull;
    read->depth -= 1u;
    cli_line_character(out, close);
    return 1;
}

static int cli_compact_value(CliCompact *read, CliLine *out)
{
    cli_compact_white(read);
    if (read->at >= read->size)
    {
        return 0;
    }
    static const char *const words[3] = {"true", "false", "null"};
    for (unsigned int word = 0u; word < 3u; word += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = words[word], .cap = CLI_CELLS_POOL});
        if ((read->at + length <= read->size) &&
            (memor.cmp(&(MemoriaCfg){.src = read->text + read->at, .other = words[word], .bytes = length}) == 0))
        {
            read->at += length;
            cli_line_text(out, words[word]);
            return 1;
        }
    }
    const char letter = read->text[read->at];
    if (letter == '"')
    {
        return cli_compact_string(read, out);
    }
    if ((letter == '{') || (letter == '['))
    {
        return cli_compact_many(read, out, letter == '{');
    }
    if ((letter == '-') || ((letter >= '0') && (letter <= '9')))
    {
        return cli_compact_number(read, out);
    }
    return 0;
}

int cli_json_compacted(const char *text, unsigned long long size, CliLine *out)
{
    const unsigned long long from = out->at;
    CliCompact read = {text, size, 0ull, 0u};
    const int whole = cli_compact_value(&read, out);
    cli_compact_white(&read);
    if (!whole || (read.at != size))
    {
        out->at = from;
        return 0;
    }
    return 1;
}

unsigned long long cli_json_value_end(const char *text, unsigned long long size, unsigned long long at)
{
    CliCompact read = {text, size, at, 0u};
    CliLine none = {NULL, 0ull, 0ull};
    cli_compact_white(&read);
    return cli_compact_value(&read, &none) ? read.at : at;
}

int cli_utf8_whole(const char *bytes, unsigned long long size)
{
    return cli_text_utf8_whole((const unsigned char *)bytes, size);
}
