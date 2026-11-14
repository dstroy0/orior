// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_compact.c: JSON read and written again as serde_json writes a Value it read, with no tree: on one line, each
// string as serde_json escapes it, each number as the integer or the float serde_json reads it as
#include "json_tree.h"
#include "text.h"

// A reading: the text, its size, where it is, and how deep it is
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
    unsigned int character;
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
    if (args->at + 4ULL > args->read_read->size)
    {
        return 0x10000u;
    }
    unsigned long long end = 0ULL;
    const unsigned long long read = EMBED_CALL(cli_text_at_size.hex, CliTextAtSizeCfg, .text = args->read_read->text,
                                               .at = args->at, .size = args->at + 4ULL, .end = &end);
    // Explicit cast narrows four digits' value, below 0x10000, to the answer's width
    const unsigned int value = (end == args->at + 4ULL) ? (unsigned int)read : 0x10000u;
    return value;
}

// a character written as JSON writes it inside a string
EMBED_INLINE void cli_compact_character(const JsonCompactCtx *args)
{
    VerbaLinea *out = args->out;
    unsigned int character = args->character;
    char bytes[8];
    VerbaLinea line = {bytes, sizeof(bytes), 0ULL};
    EMBED_CALL(cli_line_character_internal.utf8_append, CliLineLetterCtx, .line = &line, .character = character);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)bytes, .length = line.at);
}

// a string, the read at its opening quote, written again: 1, or 0 where it is not one serde_json reads
static int cli_compact_string(const JsonCompactCtx *args)
{
    VerbaLinea *out = args->out;
    args->read->at += 1ULL;
    mmgr_line_ch(out, '"');
    unsigned long long from = args->read->at;
    while (args->read->at < args->read->size)
    {
        const unsigned char character = (unsigned char)args->read->text[args->read->at];
        if ((character == '"') || (character == '\\') || (character < 0x20u))
        {
            EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)args->read->text + from, .length = args->read->at - from);
        }
        if (character == '"')
        {
            args->read->at += 1ULL;
            mmgr_line_ch(out, '"');
            return 1;
        }
        if (character < 0x20u)
        {
            return 0;
        }
        if (character >= 0x80u)
        {
            unsigned int whole = 0u;
            const unsigned long long reserved =
                EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg,
                           .text_bytes = (const unsigned char *)args->read->text, .length = args->read->size,
                           .at = args->read->at, .character = &whole);
            if ((whole == 0xFFFDu) &&
                ((reserved != 3ULL) ||
                 (memor.cmp(&(MemoriaCfg){
                      .src = args->read->text + args->read->at, .other = "\xEF\xBF\xBD", .bytes = 3u}) != 0)))
            {
                return 0;
            }
            args->read->at += reserved;
            continue;
        }
        if (character != '\\')
        {
            args->read->at += 1ULL;
            continue;
        }
        if (args->read->at + 1ULL >= args->read->size)
        {
            return 0;
        }
        const char escape = args->read->text[args->read->at + 1ULL];
        const char *plain;
        switch (escape)
        {
        case '"': {
            plain = "\"";
            break;
        }
        case '\\': {
            plain = "\\";
            break;
        }
        case '/': {
            plain = "/";
            break;
        }
        case 'b': {
            plain = "\b";
            break;
        }
        case 'f': {
            plain = "\f";
            break;
        }
        case 'n': {
            plain = "\n";
            break;
        }
        case 'r': {
            plain = "\r";
            break;
        }
        case 't': {
            plain = "\t";
            break;
        }
        default: {
            plain = NULL;
            break;
        }
        }
        if (plain != NULL)
        {
            EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)plain, .length = 1ULL);
            args->read->at += 2ULL;
            from = args->read->at;
            continue;
        }
        if (escape != 'u')
        {
            return 0;
        }
        unsigned int code =
            EMBED_CALL(cli_compact_hex, JsonCompactCtx, .read_read = args->read, .at = args->read->at + 2ULL);
        args->read->at += 6ULL;
        if (code > 0xFFFFu)
        {
            return 0;
        }
        if ((code >= 0xD800u) && (code <= 0xDBFFu))
        {
            // a leading surrogate is whole only with a trailing one escaped after it
            const unsigned int trail =
                ((args->read->at + 1ULL < args->read->size) && (args->read->text[args->read->at] == '\\') &&
                 (args->read->text[args->read->at + 1ULL] == 'u'))
                    ? EMBED_CALL(cli_compact_hex, JsonCompactCtx, .read_read = args->read, .at = args->read->at + 2ULL)
                    : 0x10000u;
            if ((trail < 0xDC00u) || (trail > 0xDFFFu))
            {
                return 0;
            }
            code = 0x10000u + ((code - 0xD800u) << 10u) + (trail - 0xDC00u);
            args->read->at += 6ULL;
        }
        else if ((code >= 0xDC00u) && (code <= 0xDFFFu))
        {
            return 0;
        }
        EMBED_CALL(cli_compact_character, JsonCompactCtx, .out = out, .character = code);
        from = args->read->at;
    }
    return 0;
}

// the digits from `at`: their count
EMBED_INLINE unsigned long long cli_compact_digits(const JsonCompactCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long count = 0ULL;
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
    args->read->at += negative ? 1ULL : 0ULL;
    const unsigned long long whole =
        EMBED_CALL(cli_compact_digits, JsonCompactCtx, .read_read = args->read, .at = args->read->at);
    if ((whole == 0ULL) || ((whole > 1ULL) && (args->read->text[args->read->at] == '0')))
    {
        return 0;
    }
    const unsigned long long digits_at = args->read->at;
    args->read->at += whole;
    int integer = 1;
    if ((args->read->at < args->read->size) && (args->read->text[args->read->at] == '.'))
    {
        const unsigned long long part =
            EMBED_CALL(cli_compact_digits, JsonCompactCtx, .read_read = args->read, .at = args->read->at + 1ULL);
        if (part == 0ULL)
        {
            return 0;
        }
        args->read->at += 1ULL + part;
        integer = 0;
    }
    if ((args->read->at < args->read->size) &&
        ((args->read->text[args->read->at] == 'e') || (args->read->text[args->read->at] == 'E')))
    {
        args->read->at += 1ULL;
        args->read->at += ((args->read->at < args->read->size) &&
                           ((args->read->text[args->read->at] == '+') || (args->read->text[args->read->at] == '-')))
                              ? 1ULL
                              : 0ULL;
        const unsigned long long power =
            EMBED_CALL(cli_compact_digits, JsonCompactCtx, .read_read = args->read, .at = args->read->at);
        if (power == 0ULL)
        {
            return 0;
        }
        args->read->at += power;
        integer = 0;
    }
    unsigned long long value = 0ULL;
    int fits = integer;
    {
        unsigned long long at = digits_at;
        while (fits && (at < digits_at + whole))
        {
            const unsigned long long digit = (unsigned long long)(args->read->text[at] - '0');
            fits = (value <= (18446744073709551615ULL - digit) / 10ULL);
            value = fits ? (value * 10ULL) + digit : value;
            at += 1ULL;
        }
    }
    if (fits && !negative)
    {
        mmgr_line_decimal(out, value, 1u);
        return 1;
    }
    if (fits && (value != 0ULL) && (value <= 9223372036854775808ULL))
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
    const double number = cellul.to_double(&(TransfiguroCfg){.src = copy});
    unsigned long long bits = 0ULL;
    memor.cpy(&(MemoriaCfg){.dst = &bits, .src = &number, .bytes = sizeof(bits)});
    if (((bits >> 52u) & 0x7ffULL) == 0x7ffULL)
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
    args->read->at += 1ULL;
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
            args->read->at += 1ULL;
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
            args->read->at += 1ULL;
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
    args->read->at += 1ULL;
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
    for (unsigned int word_text = 0u; word_text < 3u; word_text += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = words[word_text], .cap = CLI_POOL_BYTES});
        if ((args->read->at + length <= args->read->size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->read->text + args->read->at, .other = words[word_text], .bytes = length}) == 0))
        {
            args->read->at += length;
            mmgr_line_put(args->out, words[word_text]);
            return 1;
        }
    }
    const char character = args->read->text[args->read->at];
    if (character == '"')
    {
        return cli_compact_string(args);
    }
    if ((character == '{') || (character == '['))
    {
        return EMBED_CALL(cli_compact_many, JsonCompactCtx, .read = args->read, .out = args->out,
                          .object = character == '{');
    }
    if ((character == '-') || ((character >= '0') && (character <= '9')))
    {
        return cli_compact_number(args);
    }
    return 0;
}

int cli_json_compact(const CliJsonTreeTextCfg *args)
{
    const unsigned long long from = args->out->at;
    CliCompact read = {args->text, args->size, 0ULL, 0u};
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
    VerbaLinea none = {NULL, 0ULL, 0ULL};
    EMBED_CALL(cli_compact_white, JsonCompactCtx, .read = &read);
    return EMBED_CALL(cli_compact_value, JsonCompactCtx, .read = &read, .out = &none) ? read.at : args->at;
}

int cli_utf8_valid(const CliJsonTreeSizeCfg *args)
{
    return EMBED_CALL(cli_text_size_type.utf8_valid, CliTextSizeTypeCfg,
                      .text_bytes = (const unsigned char *)args->bytes, .size = args->size);
}
