// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_read.c: a JSON file read with the place of each of its values: spaces and comments, `//` and `/* */`, passed
// over; a string's escapes read; each fault reported with its span and the reading carried on past it, so one file
// gives all its faults at once; values nested deeper than 512 not read. Values, members and texts past what the read
// fits are not stored, and it is marked full
#include "../text/text.h"
#include "schema_internal.h"

#include <stdlib.h>

// A reader of a JSON file: the read it fills, the byte it stands at, and how deep it is
typedef struct
{
    CliSchemaRead *read;
    unsigned long long at;
    unsigned int depth;
} CliSchemaReader;
// the deepest values are nested that are read
#define CLI_SCHEMA_NESTED 512u

typedef struct
{
    const CliSchemaReader *reader_read;
    CliSchemaRead *read;
    const char *bytes;
    unsigned long long size;
    CliSchemaReader *reader;
    unsigned long long from;
    unsigned long long to;
    const char *before;
    const char *middle;
    const char *after;
    const char *stops;
    VerbaLinea *out;
    unsigned char kind;
    unsigned int *first;
} SchemaReadCtx;
// the byte the reader stands at, -1 past the text's end
EMBED_INLINE int read_peek(const SchemaReadCtx *args)
{
    return (args->reader_read->at < args->reader_read->read->size)
               ? (unsigned char)args->reader_read->read->text[args->reader_read->at]
               : -1;
}

// `size` bytes stored in the pool: where they start, CLI_SCHEMA_POOL where they do not fit
EMBED_INLINE unsigned int read_store(const SchemaReadCtx *args)
{
    const char *bytes = args->bytes;
    if ((args->read->used + args->size + 1ull) > args->read->pool_max)
    {
        args->read->full = 1;
        return CLI_SCHEMA_POOL;
    }
    const unsigned int at = args->read->used;
    memor.cpy(&(MemoriaCfg){.dst = args->read->pool + at, .src = bytes, .bytes = (size_t)args->size});
    args->read->pool[at + args->size] = '\0';
    args->read->used += (unsigned int)args->size + 1u;
    return at;
}

// a fault stored with its span, what it reports written as `before`, `size` bytes of `middle`, and `after`
EMBED_INLINE void read_error(const SchemaReadCtx *args)
{
    CliSchemaRead *const read = args->reader->read;
    static char s_err_buf[1u << 14u];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    mmgr_line_put(&err_msg, args->before);
    mmgr_line_put_n(&err_msg, args->middle, args->size);
    mmgr_line_put(&err_msg, args->after);
    const unsigned int at = mmgr_line_ok(&err_msg) ? EMBED_CALL(read_store, SchemaReadCtx, .read = read,
                                                                .bytes = s_err_buf, .size = err_msg.at)
                                                   : CLI_SCHEMA_POOL;
    if ((read->errors < CLI_SCHEMA_ERRORS) && (at != CLI_SCHEMA_POOL))
    {
        read->error[read->errors] = (CliSchemaError){args->from, args->to, at, (unsigned int)err_msg.at};
        read->errors += 1u;
    }
}

// spaces and comments passed
EMBED_INLINE void read_blank(const SchemaReadCtx *args)
{
    const char *const text = args->reader->read->text;
    const unsigned long long size = args->reader->read->size;
    for (;;)
    {
        args->reader->at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text,
                                      .at = args->reader->at, .size = size, .set = " \t\n\r\f");
        unsigned long long end = 0ull;
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text + args->reader->at,
                       .size = size - args->reader->at, .prefix = "//"))
        {
            args->reader->at =
                EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + args->reader->at,
                           .length_value = size - args->reader->at, .required = "\n", .size = 1u, .at_pointer = &end)
                    ? args->reader->at + end
                    : size;
        }
        else if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text + args->reader->at,
                            .size = size - args->reader->at, .prefix = "/*"))
        {
            args->reader->at = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + args->reader->at + 2u,
                                          .length_value = size - args->reader->at - 2u, .required = "*/", .size = 2u,
                                          .at_pointer = &end)
                                   ? args->reader->at + 2u + end + 2u
                                   : size;
        }
        else
        {
            return;
        }
    }
}

// the byte of the first of `stops` from the reader's place, the text's end where there is none, at least one past it
EMBED_INLINE unsigned long long read_stop(const SchemaReadCtx *args)
{
    const unsigned long long size = args->reader_read->read->size;
    unsigned long long end = args->reader_read->at;
    while ((end < size) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = args->stops, .cap = CLI_CELLS_POOL, .byte = args->reader_read->read->text[end]}) == NULL))
    {
        end += 1u;
    }
    end = (end > args->reader_read->at + 1u) ? end : args->reader_read->at + 1u;
    return (end < size) ? end : size;
}

// a string in double quotes, its escapes read, into `out`
static void read_string(const SchemaReadCtx *args)
{
    const char *const text = args->reader->read->text;
    const unsigned long long size = args->reader->read->size;
    const unsigned long long start = args->reader->at;
    args->reader->at += 1u;
    while (args->reader->at < size)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = size,
                                               .at = args->reader->at, .length = &length);
        if (letter == '"')
        {
            args->reader->at += 1u;
            return;
        }
        if (letter == '\n')
        {
            break;
        }
        args->reader->at += length;
        if (letter != '\\')
        {
            mmgr_line_put_n(args->out, text + args->reader->at - length, length);
            continue;
        }
        if (args->reader->at >= size)
        {
            break;
        }
        const unsigned int next = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = size,
                                             .at = args->reader->at, .length = &length);
        args->reader->at += length;
        static const char s_from[5] = {'n', 't', 'r', 'b', 'f'};
        static const char s_to[5] = {'\n', '\t', '\r', '\b', '\f'};
        const char *const escape =
            (next < 0x80u) ? memor.chr(&(MemoriaCfg){.src = s_from, .val = (int)next, .bytes = 5u}) : NULL;
        if (escape != NULL)
        {
            mmgr_line_ch(args->out, s_to[escape - s_from]);
        }
        else if (next == 'u')
        {
            char hex[5] = {0};
            const unsigned long long left = (args->reader->at + 4u <= size) ? 4u : size - args->reader->at;
            memor.cpy(&(MemoriaCfg){.dst = hex, .src = text + args->reader->at, .bytes = (size_t)left});
            char *end = NULL;
            const unsigned long value = strtoul(hex, &end, 16);
            const int spaced = (hex[0] == ' ') || ((hex[0] >= '\t') && (hex[0] <= '\r')) || (hex[0] == '-');
            if (!spaced && (left == 4u) && (end == hex + 4) && ((value < 0xd800u) || (value > 0xdfffu)))
            {
                EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = args->out,
                           .letter = (unsigned int)value);
            }
            args->reader->at += left;
        }
        else
        {
            mmgr_line_put_n(args->out, text + args->reader->at - length, length);
        }
    }
    EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = start, .to = args->reader->at,
               .before = "This string is not closed with `\"` on its line", .middle = "", .size = 0u, .after = "");
}

static unsigned int read_value(const SchemaReadCtx *args);

// a node made of `kind` from `from` to the reader's place: its place, CLI_SCHEMA_NONE where it does not fit
EMBED_INLINE unsigned int read_node(const SchemaReadCtx *args)
{
    unsigned char kind = args->kind;
    unsigned long long from = args->from;
    CliSchemaRead *const read = args->reader->read;
    if (read->nodes >= read->nodes_max)
    {
        read->full = 1;
        return CLI_SCHEMA_NONE;
    }
    CliSchemaNode *const node = &read->node[read->nodes];
    memor.set(&(MemoriaCfg){.dst = node, .val = 0, .bytes = sizeof(*node)});
    node->kind = kind;
    node->from = from;
    node->to = args->reader->at;
    node->first = CLI_SCHEMA_NONE;
    node->next = CLI_SCHEMA_NONE;
    read->nodes += 1u;
    return read->nodes - 1u;
}

// the members of an object read, linked from `*first`: their count
static unsigned int read_members(const SchemaReadCtx *args)
{
    unsigned int *first = args->first;
    CliSchemaRead *const read = args->reader->read;
    const char *const text = read->text;
    const unsigned long long open = args->reader->at;
    args->reader->at += 1u;
    unsigned int count = 0u;
    unsigned int last = CLI_SCHEMA_NONE;
    static char s_key[1u << 16u];
    for (;;)
    {
        read_blank(args);
        const int here = EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader);
        if (here == '}')
        {
            args->reader->at += 1u;
            return count;
        }
        if (here == -1)
        {
            EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = open, .to = open + 1u,
                       .before = "This `{` is not closed with `}`", .middle = "", .size = 0u, .after = "");
            return count;
        }
        if (here != '"')
        {
            const unsigned long long end =
                EMBED_CALL(read_stop, SchemaReadCtx, .reader_read = args->reader, .stops = ",}\n");
            EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = args->reader->at, .to = end,
                       .before = "A key is a string in double quotes", .middle = "", .size = 0u, .after = "");
            args->reader->at = end;
            args->reader->at += (EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader) == ',') ? 1u : 0u;
            continue;
        }
        const unsigned long long from = args->reader->at;
        VerbaLinea key = {s_key, sizeof(s_key), 0ull};
        EMBED_CALL(read_string, SchemaReadCtx, .reader = args->reader, .out = &key);
        const unsigned long long to = args->reader->at;
        const unsigned int key_at = EMBED_CALL(read_store, SchemaReadCtx, .read = read, .bytes = s_key, .size = key.at);
        const unsigned long long key_size = key.at;
        read_blank(args);
        unsigned int value = CLI_SCHEMA_NONE;
        if (EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader) == ':')
        {
            args->reader->at += 1u;
            value = read_value(args);
            if ((value == CLI_SCHEMA_NONE) && (EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader) != -1))
            {
                EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = from, .to = to, .before = "`",
                           .middle = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = read,
                                                .at = key_at),
                           .size = key_size, .after = "` is given no value");
            }
        }
        else
        {
            EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = from, .to = to, .before = "`",
                       .middle =
                           EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = read, .at = key_at),
                       .size = key_size, .after = "` is followed by `:` and its value");
        }
        for (unsigned int earlier = *first; earlier != CLI_SCHEMA_NONE; earlier = read->member[earlier].next)
        {
            const CliSchemaMember *const one = &read->member[earlier];
            if ((one->key_size == key_size) &&
                (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                           .read = read, .at = one->key_at),
                                         .other = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                             .read = read, .at = key_at),
                                         .bytes = (size_t)key_size}) == 0))
            {
                EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = from, .to = to, .before = "",
                           .middle = text + one->from, .size = one->to - one->from,
                           .after = " is a key of this object above it");
                break;
            }
        }
        if (read->members < read->members_max)
        {
            const unsigned int at = read->members;
            read->member[at] = (CliSchemaMember){key_at, (unsigned int)key_size, from, to, value, CLI_SCHEMA_NONE};
            read->members += 1u;
            if (last == CLI_SCHEMA_NONE)
            {
                *first = at;
            }
            else
            {
                read->member[last].next = at;
            }
            last = at;
            count += 1u;
        }
        else
        {
            read->full = 1;
        }
        read_blank(args);
        const int after = EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader);
        if (after == ',')
        {
            const unsigned long long comma = args->reader->at;
            args->reader->at += 1u;
            read_blank(args);
            if (EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader) == '}')
            {
                EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = comma, .to = comma + 1u,
                           .before = "No member follows this `,`", .middle = "", .size = 0u, .after = "");
            }
        }
        else if (after == '"')
        {
            EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = args->reader->at,
                       .to = args->reader->at + 1u, .before = "Members are parted by `,`", .middle = "", .size = 0u,
                       .after = "");
        }
        else if (after != '}')
        {
            EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = open, .to = open + 1u,
                       .before = "This `{` is not closed with `}`", .middle = "", .size = 0u, .after = "");
            return count;
        }
    }
}

// the items of an array read, linked from `*first`: their count
static unsigned int read_items(const SchemaReadCtx *args)
{
    unsigned int *first = args->first;
    CliSchemaRead *const read = args->reader->read;
    const unsigned long long open = args->reader->at;
    args->reader->at += 1u;
    unsigned int count = 0u;
    unsigned int last = CLI_SCHEMA_NONE;
    for (;;)
    {
        read_blank(args);
        const int here = EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader);
        if (here == ']')
        {
            args->reader->at += 1u;
            return count;
        }
        if (here == -1)
        {
            EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = open, .to = open + 1u,
                       .before = "This `[` is not closed with `]`", .middle = "", .size = 0u, .after = "");
            return count;
        }
        const unsigned long long before = args->reader->at;
        const unsigned int item = read_value(args);
        if (item != CLI_SCHEMA_NONE)
        {
            if (last == CLI_SCHEMA_NONE)
            {
                *first = item;
            }
            else
            {
                read->node[last].next = item;
            }
            last = item;
            count += 1u;
        }
        read_blank(args);
        const int after = EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader);
        if (after == ',')
        {
            const unsigned long long comma = args->reader->at;
            args->reader->at += 1u;
            read_blank(args);
            if (EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader) == ']')
            {
                EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = comma, .to = comma + 1u,
                           .before = "No item follows this `,`", .middle = "", .size = 0u, .after = "");
            }
        }
        else if ((after != ']') && (after != -1))
        {
            if (args->reader->at == before)
            {
                args->reader->at += 1u;
            }
            else
            {
                EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = args->reader->at,
                           .to = args->reader->at + 1u, .before = "Items are parted by `,`", .middle = "", .size = 0u,
                           .after = "");
            }
        }
    }
}

// a word, `true`, `false` or `null`, read into a node
EMBED_INLINE unsigned int read_word(const SchemaReadCtx *args)
{
    const char *const text = args->reader->read->text;
    args->reader->at =
        EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = args->reader->at,
                   .size = args->reader->read->size, .classes = CLI_TEXT_LETTERS, .set = "");
    const unsigned long long size = args->reader->at - args->from;
    const int is_true =
        (size == 4u) && (memor.cmp(&(MemoriaCfg){.src = text + args->from, .other = "true", .bytes = 4u}) == 0);
    const int is_false =
        (size == 5u) && (memor.cmp(&(MemoriaCfg){.src = text + args->from, .other = "false", .bytes = 5u}) == 0);
    if (!is_true && !is_false &&
        !((size == 4u) && (memor.cmp(&(MemoriaCfg){.src = text + args->from, .other = "null", .bytes = 4u}) == 0)))
    {
        EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = args->from, .to = args->reader->at,
                   .before = "`", .middle = text + args->from, .size = size,
                   .after =
                       "` is no value: a value is a string, a number, an object, an array, `true`, `false` or `null`");
    }
    const unsigned int node =
        EMBED_CALL(read_node, SchemaReadCtx, .reader = args->reader,
                   .kind = (is_true || is_false) ? CLI_SCHEMA_BOOL : CLI_SCHEMA_NULL, .from = args->from);
    if (node != CLI_SCHEMA_NONE)
    {
        args->reader->read->node[node].truth = is_true;
    }
    return node;
}

// a number read into a node, `null` where it is none
EMBED_INLINE unsigned int read_number(const SchemaReadCtx *args)
{
    const char *const text = args->reader->read->text;
    args->reader->at =
        EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = args->reader->at,
                   .size = args->reader->read->size, .classes = CLI_TEXT_DIGITS, .set = "+-.eE");
    char raw[512];
    const unsigned long long size = args->reader->at - args->from;
    double value = 0.0;
    int read = (size > 0u) && (size < sizeof(raw));
    if (read)
    {
        memor.cpy(&(MemoriaCfg){.dst = raw, .src = text + args->from, .bytes = (size_t)size});
        raw[size] = '\0';
        char *end = NULL;
        value = strtod(raw, &end);
        read = (end == raw + size);
    }
    if (!read)
    {
        EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = args->from, .to = args->reader->at,
                   .before = "`", .middle = text + args->from, .size = size, .after = "` is no number");
    }
    const unsigned int node = EMBED_CALL(read_node, SchemaReadCtx, .reader = args->reader,
                                         .kind = read ? CLI_SCHEMA_NUMBER : CLI_SCHEMA_NULL, .from = args->from);
    if (node != CLI_SCHEMA_NONE)
    {
        args->reader->read->node[node].number = value;
    }
    return node;
}

static unsigned int read_value(const SchemaReadCtx *args)
{
    read_blank(args);
    const unsigned long long from = args->reader->at;
    const int byte = EMBED_CALL(read_peek, SchemaReadCtx, .reader_read = args->reader);
    if (byte == -1)
    {
        return CLI_SCHEMA_NONE;
    }
    if (args->reader->depth > CLI_SCHEMA_NESTED)
    {
        EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = from, .to = from + 1u,
                   .before = "The values nest too deep to read", .middle = "", .size = 0u, .after = "");
        args->reader->at = args->reader->read->size;
        return CLI_SCHEMA_NONE;
    }
    if ((byte == '{') || (byte == '['))
    {
        unsigned int first = CLI_SCHEMA_NONE;
        args->reader->depth += 1u;
        const unsigned int count =
            (byte == '{') ? EMBED_CALL(read_members, SchemaReadCtx, .reader = args->reader, .first = &first)
                          : EMBED_CALL(read_items, SchemaReadCtx, .reader = args->reader, .first = &first);
        args->reader->depth -= 1u;
        const unsigned int node =
            EMBED_CALL(read_node, SchemaReadCtx, .reader = args->reader,
                       .kind = (byte == '{') ? CLI_SCHEMA_OBJECT : CLI_SCHEMA_ARRAY, .from = from);
        if (node != CLI_SCHEMA_NONE)
        {
            args->reader->read->node[node].first = first;
            args->reader->read->node[node].count = count;
        }
        return node;
    }
    if (byte == '"')
    {
        static char s_string[1u << 20u];
        VerbaLinea text = {s_string, sizeof(s_string), 0ull};
        EMBED_CALL(read_string, SchemaReadCtx, .reader = args->reader, .out = &text);
        const unsigned int node =
            EMBED_CALL(read_node, SchemaReadCtx, .reader = args->reader, .kind = CLI_SCHEMA_TEXT, .from = from);
        if (node != CLI_SCHEMA_NONE)
        {
            args->reader->read->node[node].text_at =
                EMBED_CALL(read_store, SchemaReadCtx, .read = args->reader->read, .bytes = s_string, .size = text.at);
            args->reader->read->node[node].text_size = (unsigned int)text.at;
        }
        return node;
    }
    if ((byte == 't') || (byte == 'f') || (byte == 'n'))
    {
        return EMBED_CALL(read_word, SchemaReadCtx, .reader = args->reader, .from = from);
    }
    if ((byte == '-') || ((byte >= '0') && (byte <= '9')))
    {
        return EMBED_CALL(read_number, SchemaReadCtx, .reader = args->reader, .from = from);
    }
    const unsigned long long end = EMBED_CALL(read_stop, SchemaReadCtx, .reader_read = args->reader, .stops = ",}]\n");
    EMBED_CALL(read_error, SchemaReadCtx, .reader = args->reader, .from = from, .to = end,
               .before = "A value stands here: a string, a number, an object, an array, `true`, `false` or `null`",
               .middle = "", .size = 0u, .after = "");
    args->reader->at = end;
    return CLI_SCHEMA_NONE;
}

// the most bytes of the error messages a read writes
#define CLI_SCHEMA_READ_MORE (64ull << 10u)

void cli_schema_read(const CliSchemaReadTextCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    cli_schema_read_text_internal.read_free(args);
    // a value takes two bytes of the text at the least with its comma, a member five, a text no more than its own bytes
    // and its end; the values a check makes stand past those the read makes, one for each value and member
    const unsigned long long values = (size / 2ull) + 2ull;
    const unsigned long long keyed = (size / 5ull) + 2ull;
    const unsigned long long nodes = values + keyed + CLI_SCHEMA_CHECK_NODES;
    const unsigned long long pool = (2ull * size) + CLI_SCHEMA_READ_MORE;
    args->read_out->nodes_max = (nodes < CLI_SCHEMA_NODES) ? (unsigned int)nodes : CLI_SCHEMA_NODES;
    args->read_out->members_max = (keyed < CLI_SCHEMA_MEMBERS) ? (unsigned int)keyed : CLI_SCHEMA_MEMBERS;
    args->read_out->pool_max = (pool < CLI_SCHEMA_POOL) ? (unsigned int)pool : CLI_SCHEMA_POOL;
    const unsigned long long node_bytes = (unsigned long long)args->read_out->nodes_max * sizeof(CliSchemaNode);
    const unsigned long long member_bytes = (unsigned long long)args->read_out->members_max * sizeof(CliSchemaMember);
    char *const cell =
        g_cli_cells->temporary_buf_alloc(node_bytes + member_bytes + args->read_out->pool_max, &g_cli_schema_cells);
    args->read_out->node = (CliSchemaNode *)(void *)cell;
    args->read_out->member = NULL;
    args->read_out->pool = NULL;
    if (cell != NULL)
    {
        args->read_out->member = (CliSchemaMember *)(void *)(cell + node_bytes);
        args->read_out->pool = cell + node_bytes + member_bytes;
    }
    else
    {
        args->read_out->nodes_max = 0u;
        args->read_out->members_max = 0u;
        args->read_out->pool_max = 0u;
    }
    args->read_out->text = text;
    args->read_out->size = size;
    args->read_out->nodes = 0u;
    args->read_out->members = 0u;
    args->read_out->errors = 0u;
    args->read_out->used = 0u;
    args->read_out->full = 0;
    CliSchemaReader reader = {args->read_out, 0ull, 0u};
    EMBED_CALL(read_blank, SchemaReadCtx, .reader = &reader);
    args->read_out->root = EMBED_CALL(read_value, SchemaReadCtx, .reader = &reader);
    EMBED_CALL(read_blank, SchemaReadCtx, .reader = &reader);
    if ((reader.at < size) && (args->read_out->root != CLI_SCHEMA_NONE))
    {
        EMBED_CALL(read_error, SchemaReadCtx, .reader = &reader, .from = reader.at,
                   .to = (reader.at + 1u < size) ? reader.at + 1u : size,
                   .before = "The file contains one value, and more stands after it", .middle = "", .size = 0u,
                   .after = "");
    }
}
