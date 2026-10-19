// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_read.c: a JSON file read with the place of each of its values: spaces and comments, `//` and `/* */`, passed
// over; a string's escapes read; each fault said with its span and the reading carried on past it, so one file gives
// all its faults at once; values nested deeper than 512 not read. Values, members and texts past what the read holds
// are not kept, and it is marked full
#include "schema_internal.h"

#include <stdlib.h>

// the deepest values are nested that are read
#define CLI_SCHEMA_NESTED 512u

// A reader of a JSON file: the read it fills, the byte it stands at, and how deep it is
typedef struct
{
    CliSchemaRead *read;
    unsigned long long at;
    unsigned int depth;
} CliSchemaReader;

// the byte the reader stands at, -1 past the text's end
static int read_peek(const CliSchemaReader *reader)
{
    return (reader->at < reader->read->size) ? (unsigned char)reader->read->text[reader->at] : -1;
}

// `size` bytes kept in the pool: where they start, CLI_SCHEMA_POOL where they do not fit
static unsigned int read_kept(CliSchemaRead *read, const char *bytes, unsigned long long size)
{
    if (size + 1u > CLI_SCHEMA_POOL - read->used)
    {
        read->full = 1;
        return CLI_SCHEMA_POOL;
    }
    const unsigned int at = read->used;
    memor.cpy(&(MemoriaCfg){.dst = read->pool + at, .src = bytes, .bytes = (size_t)size});
    read->pool[at + size] = '\0';
    read->used += (unsigned int)size + 1u;
    return at;
}

// a fault kept with its span, what it says written as `before`, `size` bytes of `middle`, and `after`
static void read_error(CliSchemaReader *reader, unsigned long long from, unsigned long long to, const char *before,
                       const char *middle, unsigned long long size, const char *after)
{
    CliSchemaRead *const read = reader->read;
    static char s_said[1u << 14u];
    CliLine said = {s_said, sizeof(s_said), 0ull};
    cli_line_text(&said, before);
    cli_line_counted(&said, middle, size);
    cli_line_text(&said, after);
    const unsigned int at = cli_line_fits(&said) ? read_kept(read, s_said, said.at) : CLI_SCHEMA_POOL;
    if ((read->errors < CLI_SCHEMA_ERRORS) && (at != CLI_SCHEMA_POOL))
    {
        read->error[read->errors] = (CliSchemaError){from, to, at, (unsigned int)said.at};
        read->errors += 1u;
    }
}

// spaces and comments passed
static void read_blank(CliSchemaReader *reader)
{
    const char *const text = reader->read->text;
    const unsigned long long size = reader->read->size;
    for (;;)
    {
        while ((reader->at < size) &&
               ((text[reader->at] == ' ') || (text[reader->at] == '\t') || (text[reader->at] == '\n') ||
                (text[reader->at] == '\r') || (text[reader->at] == '\f')))
        {
            reader->at += 1u;
        }
        unsigned long long end = 0ull;
        if (cli_text_starts(text + reader->at, size - reader->at, "//"))
        {
            reader->at = cli_text_find(text + reader->at, size - reader->at, "\n", 1u, &end) ? reader->at + end : size;
        }
        else if (cli_text_starts(text + reader->at, size - reader->at, "/*"))
        {
            reader->at = cli_text_find(text + reader->at + 2u, size - reader->at - 2u, "*/", 2u, &end)
                             ? reader->at + 2u + end + 2u
                             : size;
        }
        else
        {
            return;
        }
    }
}

// the byte of the first of `stops` from the reader's place, the text's end where there is none, at least one past it
static unsigned long long read_stop(const CliSchemaReader *reader, const char *stops)
{
    const unsigned long long size = reader->read->size;
    unsigned long long end = reader->at;
    while ((end < size) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = stops, .cap = CLI_CELLS_POOL, .byte = reader->read->text[end]}) == NULL))
    {
        end += 1u;
    }
    end = (end > reader->at + 1u) ? end : reader->at + 1u;
    return (end < size) ? end : size;
}

// a string in double quotes, its escapes read, into `out`
static void read_string(CliSchemaReader *reader, CliLine *out)
{
    const char *const text = reader->read->text;
    const unsigned long long size = reader->read->size;
    const unsigned long long start = reader->at;
    reader->at += 1u;
    while (reader->at < size)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(text, size, reader->at, &length);
        if (letter == '"')
        {
            reader->at += 1u;
            return;
        }
        if (letter == '\n')
        {
            break;
        }
        reader->at += length;
        if (letter != '\\')
        {
            cli_line_counted(out, text + reader->at - length, length);
            continue;
        }
        if (reader->at >= size)
        {
            break;
        }
        const unsigned int next = cli_text_letter(text, size, reader->at, &length);
        reader->at += length;
        static const char s_from[5] = {'n', 't', 'r', 'b', 'f'};
        static const char s_to[5] = {'\n', '\t', '\r', '\b', '\f'};
        const char *const escape =
            (next < 0x80u) ? memor.chr(&(MemoriaCfg){.src = s_from, .val = (int)next, .bytes = 5u}) : NULL;
        if (escape != NULL)
        {
            cli_line_character(out, s_to[escape - s_from]);
        }
        else if (next == 'u')
        {
            char hex[5] = {0};
            const unsigned long long left = (reader->at + 4u <= size) ? 4u : size - reader->at;
            memor.cpy(&(MemoriaCfg){.dst = hex, .src = text + reader->at, .bytes = (size_t)left});
            char *end = NULL;
            const unsigned long value = strtoul(hex, &end, 16);
            const int spaced = (hex[0] == ' ') || ((hex[0] >= '\t') && (hex[0] <= '\r')) || (hex[0] == '-');
            if (!spaced && (left == 4u) && (end == hex + 4) && ((value < 0xd800u) || (value > 0xdfffu)))
            {
                cli_utf8_put(out, (unsigned int)value);
            }
            reader->at += left;
        }
        else
        {
            cli_line_counted(out, text + reader->at - length, length);
        }
    }
    read_error(reader, start, reader->at, "This string is not closed with `\"` on its line", "", 0u, "");
}

static unsigned int read_value(CliSchemaReader *reader);

// a node made of `kind` from `from` to the reader's place: its place, CLI_SCHEMA_NONE where it does not fit
static unsigned int read_node(CliSchemaReader *reader, unsigned char kind, unsigned long long from)
{
    CliSchemaRead *const read = reader->read;
    if (read->nodes == CLI_SCHEMA_NODES)
    {
        read->full = 1;
        return CLI_SCHEMA_NONE;
    }
    CliSchemaNode *const node = &read->node[read->nodes];
    memor.set(&(MemoriaCfg){.dst = node, .val = 0, .bytes = sizeof(*node)});
    node->kind = kind;
    node->from = from;
    node->to = reader->at;
    node->first = CLI_SCHEMA_NONE;
    node->next = CLI_SCHEMA_NONE;
    read->nodes += 1u;
    return read->nodes - 1u;
}

// the members of an object read, linked from `*first`: their count
static unsigned int read_members(CliSchemaReader *reader, unsigned int *first)
{
    CliSchemaRead *const read = reader->read;
    const char *const text = read->text;
    const unsigned long long open = reader->at;
    reader->at += 1u;
    unsigned int count = 0u;
    unsigned int last = CLI_SCHEMA_NONE;
    static char s_key[1u << 16u];
    for (;;)
    {
        read_blank(reader);
        const int here = read_peek(reader);
        if (here == '}')
        {
            reader->at += 1u;
            return count;
        }
        if (here == -1)
        {
            read_error(reader, open, open + 1u, "This `{` is not closed with `}`", "", 0u, "");
            return count;
        }
        if (here != '"')
        {
            const unsigned long long end = read_stop(reader, ",}\n");
            read_error(reader, reader->at, end, "A key is a string in double quotes", "", 0u, "");
            reader->at = end;
            reader->at += (read_peek(reader) == ',') ? 1u : 0u;
            continue;
        }
        const unsigned long long from = reader->at;
        CliLine key = {s_key, sizeof(s_key), 0ull};
        read_string(reader, &key);
        const unsigned long long to = reader->at;
        const unsigned int key_at = read_kept(read, s_key, key.at);
        const unsigned long long key_size = key.at;
        read_blank(reader);
        unsigned int value = CLI_SCHEMA_NONE;
        if (read_peek(reader) == ':')
        {
            reader->at += 1u;
            value = read_value(reader);
            if ((value == CLI_SCHEMA_NONE) && (read_peek(reader) != -1))
            {
                read_error(reader, from, to, "`", cli_schema_text(read, key_at), key_size, "` is given no value");
            }
        }
        else
        {
            read_error(reader, from, to, "`", cli_schema_text(read, key_at), key_size,
                       "` is followed by `:` and its value");
        }
        for (unsigned int earlier = *first; earlier != CLI_SCHEMA_NONE; earlier = read->member[earlier].next)
        {
            const CliSchemaMember *const one = &read->member[earlier];
            if ((one->key_size == key_size) && (memor.cmp(&(MemoriaCfg){.src = cli_schema_text(read, one->key_at),
                                                                        .other = cli_schema_text(read, key_at),
                                                                        .bytes = (size_t)key_size}) == 0))
            {
                read_error(reader, from, to, "", text + one->from, one->to - one->from,
                           " is a key of this object above it");
                break;
            }
        }
        if (read->members < CLI_SCHEMA_MEMBERS)
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
        read_blank(reader);
        const int after = read_peek(reader);
        if (after == ',')
        {
            const unsigned long long comma = reader->at;
            reader->at += 1u;
            read_blank(reader);
            if (read_peek(reader) == '}')
            {
                read_error(reader, comma, comma + 1u, "No member follows this `,`", "", 0u, "");
            }
        }
        else if (after == '"')
        {
            read_error(reader, reader->at, reader->at + 1u, "Members are parted by `,`", "", 0u, "");
        }
        else if (after != '}')
        {
            read_error(reader, open, open + 1u, "This `{` is not closed with `}`", "", 0u, "");
            return count;
        }
    }
}

// the items of an array read, linked from `*first`: their count
static unsigned int read_items(CliSchemaReader *reader, unsigned int *first)
{
    CliSchemaRead *const read = reader->read;
    const unsigned long long open = reader->at;
    reader->at += 1u;
    unsigned int count = 0u;
    unsigned int last = CLI_SCHEMA_NONE;
    for (;;)
    {
        read_blank(reader);
        const int here = read_peek(reader);
        if (here == ']')
        {
            reader->at += 1u;
            return count;
        }
        if (here == -1)
        {
            read_error(reader, open, open + 1u, "This `[` is not closed with `]`", "", 0u, "");
            return count;
        }
        const unsigned long long before = reader->at;
        const unsigned int item = read_value(reader);
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
        read_blank(reader);
        const int after = read_peek(reader);
        if (after == ',')
        {
            const unsigned long long comma = reader->at;
            reader->at += 1u;
            read_blank(reader);
            if (read_peek(reader) == ']')
            {
                read_error(reader, comma, comma + 1u, "No item follows this `,`", "", 0u, "");
            }
        }
        else if ((after != ']') && (after != -1))
        {
            if (reader->at == before)
            {
                reader->at += 1u;
            }
            else
            {
                read_error(reader, reader->at, reader->at + 1u, "Items are parted by `,`", "", 0u, "");
            }
        }
    }
}

// a word, `true`, `false` or `null`, read into a node
static unsigned int read_word(CliSchemaReader *reader, unsigned long long from)
{
    const char *const text = reader->read->text;
    while ((reader->at < reader->read->size) && (((text[reader->at] >= 'a') && (text[reader->at] <= 'z')) ||
                                                 ((text[reader->at] >= 'A') && (text[reader->at] <= 'Z'))))
    {
        reader->at += 1u;
    }
    const unsigned long long size = reader->at - from;
    const int is_true =
        (size == 4u) && (memor.cmp(&(MemoriaCfg){.src = text + from, .other = "true", .bytes = 4u}) == 0);
    const int is_false =
        (size == 5u) && (memor.cmp(&(MemoriaCfg){.src = text + from, .other = "false", .bytes = 5u}) == 0);
    if (!is_true && !is_false &&
        !((size == 4u) && (memor.cmp(&(MemoriaCfg){.src = text + from, .other = "null", .bytes = 4u}) == 0)))
    {
        read_error(reader, from, reader->at, "`", text + from, size,
                   "` is no value: a value is a string, a number, an object, an array, `true`, `false` or `null`");
    }
    const unsigned int node = read_node(reader, (is_true || is_false) ? CLI_SCHEMA_BOOL : CLI_SCHEMA_NULL, from);
    if (node != CLI_SCHEMA_NONE)
    {
        reader->read->node[node].truth = is_true;
    }
    return node;
}

// a number read into a node, `null` where it is none
static unsigned int read_number(CliSchemaReader *reader, unsigned long long from)
{
    const char *const text = reader->read->text;
    while ((reader->at < reader->read->size) &&
           (((text[reader->at] >= '0') && (text[reader->at] <= '9')) ||
            (cellul.chr(&(CatenaFinitaCfg){.src = "+-.eE", .cap = CLI_CELLS_POOL, .byte = text[reader->at]}) != NULL)))
    {
        reader->at += 1u;
    }
    char raw[512];
    const unsigned long long size = reader->at - from;
    double value = 0.0;
    int read = (size > 0u) && (size < sizeof(raw));
    if (read)
    {
        memor.cpy(&(MemoriaCfg){.dst = raw, .src = text + from, .bytes = (size_t)size});
        raw[size] = '\0';
        char *end = NULL;
        value = strtod(raw, &end);
        read = (end == raw + size);
    }
    if (!read)
    {
        read_error(reader, from, reader->at, "`", text + from, size, "` is no number");
    }
    const unsigned int node = read_node(reader, read ? CLI_SCHEMA_NUMBER : CLI_SCHEMA_NULL, from);
    if (node != CLI_SCHEMA_NONE)
    {
        reader->read->node[node].number = value;
    }
    return node;
}

static unsigned int read_value(CliSchemaReader *reader)
{
    read_blank(reader);
    const unsigned long long from = reader->at;
    const int byte = read_peek(reader);
    if (byte == -1)
    {
        return CLI_SCHEMA_NONE;
    }
    if (reader->depth > CLI_SCHEMA_NESTED)
    {
        read_error(reader, from, from + 1u, "The values nest too deep to read", "", 0u, "");
        reader->at = reader->read->size;
        return CLI_SCHEMA_NONE;
    }
    if ((byte == '{') || (byte == '['))
    {
        unsigned int first = CLI_SCHEMA_NONE;
        reader->depth += 1u;
        const unsigned int count = (byte == '{') ? read_members(reader, &first) : read_items(reader, &first);
        reader->depth -= 1u;
        const unsigned int node = read_node(reader, (byte == '{') ? CLI_SCHEMA_OBJECT : CLI_SCHEMA_ARRAY, from);
        if (node != CLI_SCHEMA_NONE)
        {
            reader->read->node[node].first = first;
            reader->read->node[node].count = count;
        }
        return node;
    }
    if (byte == '"')
    {
        static char s_string[1u << 20u];
        CliLine text = {s_string, sizeof(s_string), 0ull};
        read_string(reader, &text);
        const unsigned int node = read_node(reader, CLI_SCHEMA_TEXT, from);
        if (node != CLI_SCHEMA_NONE)
        {
            reader->read->node[node].text_at = read_kept(reader->read, s_string, text.at);
            reader->read->node[node].text_size = (unsigned int)text.at;
        }
        return node;
    }
    if ((byte == 't') || (byte == 'f') || (byte == 'n'))
    {
        return read_word(reader, from);
    }
    if ((byte == '-') || ((byte >= '0') && (byte <= '9')))
    {
        return read_number(reader, from);
    }
    const unsigned long long end = read_stop(reader, ",}]\n");
    read_error(reader, from, end,
               "A value stands here: a string, a number, an object, an array, `true`, `false` or `null`", "", 0u, "");
    reader->at = end;
    return CLI_SCHEMA_NONE;
}

void cli_schema_read(const char *text, unsigned long long size, CliSchemaRead *read)
{
    read->text = text;
    read->size = size;
    read->nodes = 0u;
    read->members = 0u;
    read->errors = 0u;
    read->used = 0u;
    read->full = 0;
    CliSchemaReader reader = {read, 0ull, 0u};
    read_blank(&reader);
    read->root = read_value(&reader);
    read_blank(&reader);
    if ((reader.at < size) && (read->root != CLI_SCHEMA_NONE))
    {
        read_error(&reader, reader.at, (reader.at + 1u < size) ? reader.at + 1u : size,
                   "The file holds one value, and more stands after it", "", 0u, "");
    }
}
