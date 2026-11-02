// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_compact.c: JSON read and written again as serde_json writes a Value it read, with no tree: on one line, each
// string as serde_json escapes it, each number as the integer or the float serde_json reads it as
#include "json_tree.h"
#include "text.h"

#include <stdlib.h>

// A reading: the text, its size, where it stands, and how deep it is
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    unsigned int depth;
} CliCompact;
// the deepest arrays and objects nest, as serde_json's recursion limit has it
#define CLI_COMPACT_DEPTH 128u

typedef struct
{
    CliCompact *read;
    VerbaLinea *out;
    const CliCompact *read_read;
    unsigned long long at;
    unsigned int letter;
    int object;
} JsonCompactCtx;
static int cli_compact_value(const JsonCompactCtx *args);

// the whitespace JSON allows passed over
EMBED_INLINE void cli_compact_white(const JsonCompactCtx *args)
{
    args->read->at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->read->text,
                                .at = args->read->at, .size = args->read->size, .set = " \t\n\r");
}

// four hex digits from `at`: their value, or more than 0xFFFF where they are not four hex digits
EMBED_INLINE unsigned int cli_compact_hex(const JsonCompactCtx *args)
{
    if (args->at + 4ull > args->read_read->size)
    {
        return 0x10000u;
    }
    unsigned int value = 0u;
    for (unsigned long long digit = args->at; digit < args->at + 4ull; digit += 1ull)
    {
        const char letter = args->read_read->text[digit];
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
EMBED_INLINE void cli_compact_letter(const JsonCompactCtx *args)
{
    VerbaLinea *out = args->out;
    unsigned int letter = args->letter;
    char bytes[8];
    VerbaLinea line = {bytes, sizeof(bytes), 0ull};
    EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line, .letter = letter);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)bytes, .length = line.at);
}

// a string, the read at its opening quote, written again: 1, or 0 where it is not one serde_json reads
static int cli_compact_string(const JsonCompactCtx *args)
{
    VerbaLinea *out = args->out;
    args->read->at += 1ull;
    mmgr_line_ch(out, '"');
    unsigned long long from = args->read->at;
    while (args->read->at < args->read->size)
    {
        const unsigned char letter = (unsigned char)args->read->text[args->read->at];
        if ((letter == '"') || (letter == '\\') || (letter < 0x20u))
        {
            EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)args->read->text + from, .length = args->read->at - from);
        }
        if (letter == '"')
        {
            args->read->at += 1ull;
            mmgr_line_ch(out, '"');
            return 1;
        }
        if (letter < 0x20u)
        {
            return 0;
        }
        if (letter >= 0x80u)
        {
            unsigned int whole = 0u;
            const unsigned long long reserved =
                EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                           .text_bytes = (const unsigned char *)args->read->text, .length = args->read->size,
                           .at = args->read->at, .letter = &whole);
            if ((whole == 0xFFFDu) &&
                ((reserved != 3ull) ||
                 (memor.cmp(&(MemoriaCfg){
                      .src = args->read->text + args->read->at, .other = "\xEF\xBF\xBD", .bytes = 3u}) != 0)))
            {
                return 0;
            }
            args->read->at += reserved;
            continue;
        }
        if (letter != '\\')
        {
            args->read->at += 1ull;
            continue;
        }
        if (args->read->at + 1ull >= args->read->size)
        {
            return 0;
        }
        const char escape = args->read->text[args->read->at + 1ull];
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
            EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)plain, .length = 1ull);
            args->read->at += 2ull;
            from = args->read->at;
            continue;
        }
        if (escape != 'u')
        {
            return 0;
        }
        unsigned int code =
            EMBED_CALL(cli_compact_hex, JsonCompactCtx, .read_read = args->read, .at = args->read->at + 2ull);
        args->read->at += 6ull;
        if (code > 0xFFFFu)
        {
            return 0;
        }
        if ((code >= 0xD800u) && (code <= 0xDBFFu))
        {
            // a leading surrogate is whole only with a trailing one escaped after it
            const unsigned int trail =
                ((args->read->at + 1ull < args->read->size) && (args->read->text[args->read->at] == '\\') &&
                 (args->read->text[args->read->at + 1ull] == 'u'))
                    ? EMBED_CALL(cli_compact_hex, JsonCompactCtx, .read_read = args->read, .at = args->read->at + 2ull)
                    : 0x10000u;
            if ((trail < 0xDC00u) || (trail > 0xDFFFu))
            {
                return 0;
            }
            code = 0x10000u + ((code - 0xD800u) << 10u) + (trail - 0xDC00u);
            args->read->at += 6ull;
        }
        else if ((code >= 0xDC00u) && (code <= 0xDFFFu))
        {
            return 0;
        }
        EMBED_CALL(cli_compact_letter, JsonCompactCtx, .out = out, .letter = code);
        from = args->read->at;
    }
    return 0;
}

// the digits from `at`: their count
EMBED_INLINE unsigned long long cli_compact_digits(const JsonCompactCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long count = 0ull;
    count = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->read_read->text,
                       .at = at, .size = args->read_read->size, .classes = CLI_TEXT_DIGITS, .set = "") -
            at;
    return count;
}

// a number written again as serde_json reads it: an integer that fits 64 bits as that integer, -0 and any other as
// a float: 1, or 0 where it is not a JSON number or its float is out of range
EMBED_INLINE int cli_compact_number(const JsonCompactCtx *args)
{
    VerbaLinea *out = args->out;
    const unsigned long long start = args->read->at;
    const int negative = (args->read->text[args->read->at] == '-');
    args->read->at += negative ? 1ull : 0ull;
    const unsigned long long whole =
        EMBED_CALL(cli_compact_digits, JsonCompactCtx, .read_read = args->read, .at = args->read->at);
    if ((whole == 0ull) || ((whole > 1ull) && (args->read->text[args->read->at] == '0')))
    {
        return 0;
    }
    const unsigned long long digits_at = args->read->at;
    args->read->at += whole;
    int integer = 1;
    if ((args->read->at < args->read->size) && (args->read->text[args->read->at] == '.'))
    {
        const unsigned long long part =
            EMBED_CALL(cli_compact_digits, JsonCompactCtx, .read_read = args->read, .at = args->read->at + 1ull);
        if (part == 0ull)
        {
            return 0;
        }
        args->read->at += 1ull + part;
        integer = 0;
    }
    if ((args->read->at < args->read->size) &&
        ((args->read->text[args->read->at] == 'e') || (args->read->text[args->read->at] == 'E')))
    {
        args->read->at += 1ull;
        args->read->at += ((args->read->at < args->read->size) &&
                           ((args->read->text[args->read->at] == '+') || (args->read->text[args->read->at] == '-')))
                              ? 1ull
                              : 0ull;
        const unsigned long long power =
            EMBED_CALL(cli_compact_digits, JsonCompactCtx, .read_read = args->read, .at = args->read->at);
        if (power == 0ull)
        {
            return 0;
        }
        args->read->at += power;
        integer = 0;
    }
    unsigned long long value = 0ull;
    int fits = integer;
    for (unsigned long long at = digits_at; fits && (at < digits_at + whole); at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(args->read->text[at] - '0');
        fits = (value <= (18446744073709551615ull - digit) / 10ull);
        value = fits ? (value * 10ull) + digit : value;
    }
    if (fits && !negative)
    {
        mmgr_line_decimal(out, value, 1u);
        return 1;
    }
    if (fits && (value != 0ull) && (value <= 9223372036854775808ull))
    {
        mmgr_line_ch(out, '-');
        mmgr_line_decimal(out, value, 1u);
        return 1;
    }
    char copy[1024];
    const unsigned long long length = args->read->at - start;
    if (length >= sizeof(copy))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = copy, .src = args->read->text + start, .bytes = length});
    copy[length] = '\0';
    const double number = strtod(copy, NULL);
    unsigned long long bits = 0ull;
    memor.cpy(&(MemoriaCfg){.dst = &bits, .src = &number, .bytes = sizeof(bits)});
    if (((bits >> 52u) & 0x7ffull) == 0x7ffull)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out, .value_f64 = number);
    return 1;
}

// the members of an object or the items of an array, the read at its opening, written again
static int cli_compact_many(const JsonCompactCtx *args)
{
    if (args->read->depth >= CLI_COMPACT_DEPTH)
    {
        return 0;
    }
    args->read->depth += 1u;
    const char close = args->object ? '}' : ']';
    mmgr_line_ch(args->out, args->object ? '{' : '[');
    args->read->at += 1ull;
    cli_compact_white(args);
    int first = 1;
    while ((args->read->at < args->read->size) && (args->read->text[args->read->at] != close))
    {
        if (!first)
        {
            if (args->read->text[args->read->at] != ',')
            {
                return 0;
            }
            args->read->at += 1ull;
            cli_compact_white(args);
            mmgr_line_ch(args->out, ',');
        }
        first = 0;
        if (args->object)
        {
            if ((args->read->at >= args->read->size) || (args->read->text[args->read->at] != '"') ||
                !cli_compact_string(args))
            {
                return 0;
            }
            cli_compact_white(args);
            if ((args->read->at >= args->read->size) || (args->read->text[args->read->at] != ':'))
            {
                return 0;
            }
            args->read->at += 1ull;
            mmgr_line_ch(args->out, ':');
        }
        if (!cli_compact_value(args))
        {
            return 0;
        }
        cli_compact_white(args);
    }
    if (args->read->at >= args->read->size)
    {
        return 0;
    }
    args->read->at += 1ull;
    args->read->depth -= 1u;
    mmgr_line_ch(args->out, close);
    return 1;
}

static int cli_compact_value(const JsonCompactCtx *args)
{
    cli_compact_white(args);
    if (args->read->at >= args->read->size)
    {
        return 0;
    }
    static const char *const words[3] = {"true", "false", "null"};
    for (unsigned int word = 0u; word < 3u; word += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = words[word], .cap = CLI_CELLS_POOL});
        if ((args->read->at + length <= args->read->size) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = args->read->text + args->read->at, .other = words[word], .bytes = length}) == 0))
        {
            args->read->at += length;
            mmgr_line_put(args->out, words[word]);
            return 1;
        }
    }
    const char letter = args->read->text[args->read->at];
    if (letter == '"')
    {
        return cli_compact_string(args);
    }
    if ((letter == '{') || (letter == '['))
    {
        return EMBED_CALL(cli_compact_many, JsonCompactCtx, .read = args->read, .out = args->out,
                          .object = letter == '{');
    }
    if ((letter == '-') || ((letter >= '0') && (letter <= '9')))
    {
        return cli_compact_number(args);
    }
    return 0;
}

int cli_json_compact(const CliJsonTreeTextCfg *args)
{
    const unsigned long long from = args->out->at;
    CliCompact read = {args->text, args->size, 0ull, 0u};
    const int whole = EMBED_CALL(cli_compact_value, JsonCompactCtx, .read = &read, .out = args->out);
    EMBED_CALL(cli_compact_white, JsonCompactCtx, .read = &read);
    if (!whole || (read.at != args->size))
    {
        args->out->at = from;
        return 0;
    }
    return 1;
}

unsigned long long cli_json_value_end(const CliJsonTreeTextSizeCfg *args)
{
    CliCompact read = {args->text, args->size, args->at, 0u};
    VerbaLinea none = {NULL, 0ull, 0ull};
    EMBED_CALL(cli_compact_white, JsonCompactCtx, .read = &read);
    return EMBED_CALL(cli_compact_value, JsonCompactCtx, .read = &read, .out = &none) ? read.at : args->at;
}

int cli_utf8_valid(const CliJsonTreeSizeCfg *args)
{
    return EMBED_CALL(cli_text_size_kind.utf8_valid, CliTextSizeKindCfg,
                      .text_bytes = (const unsigned char *)args->bytes, .size = args->size);
}
