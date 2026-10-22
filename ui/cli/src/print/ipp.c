// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp.c: an IPP message made, encoded and decoded, and its groups and attributes read
#include "ipp.h"

void cli_ipp_begun(CliIppMessage *message, unsigned int code, unsigned int request_id)
{
    message->code = code;
    message->request_id = request_id;
    message->groups = 0u;
    message->attributes = 0u;
    message->values = 0u;
    message->used = 0u;
    message->data = NULL;
    message->data_length = 0ull;
    message->full = 0;
}

// the groups, attributes and values, and the bytes, a message's rooms are begun with, each doubling as it is put
#define CLI_IPP_FIRST 16u
#define CLI_IPP_BYTES_FIRST 1024u

CliCellsTally g_cli_ipp_cells = CLI_CELLS_TALLY("cli_ipp");

void cli_ipp_given(CliIppMessage *message)
{
    cli_cell_given_by(&g_cli_ipp_cells, message->group);
    cli_cell_given_by(&g_cli_ipp_cells, message->attribute);
    cli_cell_given_by(&g_cli_ipp_cells, message->value);
    cli_cell_given_by(&g_cli_ipp_cells, message->bytes);
    *message = (CliIppMessage){0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0ull, 0};
}

// `list`, `count` items of `each` bytes in a room of `*room`, made to hold `wanted` in all, `most` at the most, its
// room begun at `first`: the list, moved into a cell of doubled room where it is too small; or NULL with the message
// full where `wanted` is past `most` or the cell is refused
static void *cli_room(CliIppMessage *message, void *list, unsigned int count, unsigned int *room,
                      unsigned long long each, unsigned long long wanted, unsigned int most, unsigned int first,
                      const char *what)
{
    if (wanted <= *room)
    {
        return list;
    }
    unsigned long long grown = (*room == 0u) ? first : *room;
    while (grown < wanted)
    {
        grown *= 2ull;
    }
    grown = (grown < most) ? grown : most;
    void *const larger = (wanted <= most) ? cli_cell_taken_by(&g_cli_ipp_cells, grown * each, what) : NULL;
    if (larger == NULL)
    {
        message->full = 1;
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = list, .bytes = (size_t)(count * each)});
    cli_cell_given_by(&g_cli_ipp_cells, list);
    // a room no more than `most`, which an unsigned int holds
    *room = (unsigned int)grown;
    return larger;
}

// `length` bytes kept among the message's: where they start
static unsigned int cli_kept(CliIppMessage *message, const void *bytes, unsigned long long length)
{
    unsigned char *const kept =
        cli_room(message, message->bytes, message->used, &message->byte_room, 1ull, message->used + length,
                 CLI_IPP_BYTES, CLI_IPP_BYTES_FIRST, "an IPP message's bytes");
    if (kept == NULL)
    {
        return 0u;
    }
    message->bytes = kept;
    const unsigned int at = message->used;
    memor.cpy(&(MemoriaCfg){.dst = message->bytes + at, .src = bytes, .bytes = length});
    // the bytes are below CLI_IPP_BYTES, which an unsigned int counts
    message->used += (unsigned int)length;
    return at;
}

void cli_ipp_group(CliIppMessage *message, unsigned int tag)
{
    CliIppGroup *const groups =
        cli_room(message, message->group, message->groups, &message->group_room, sizeof(CliIppGroup),
                 message->groups + 1ull, CLI_IPP_GROUPS, CLI_IPP_FIRST, "an IPP message's groups");
    if (groups == NULL)
    {
        return;
    }
    message->group = groups;
    const CliIppGroup group = {tag, message->attributes, 0u};
    message->group[message->groups] = group;
    message->groups += 1u;
}

// an attribute of `length` bytes of `name` begun in the last group
static void cli_attribute_counted(CliIppMessage *message, const void *name, unsigned long long length)
{
    CliIppAttribute *const attributes =
        (message->groups != 0u) ? cli_room(message, message->attribute, message->attributes, &message->attribute_room,
                                           sizeof(CliIppAttribute), message->attributes + 1ull, CLI_IPP_ATTRIBUTES,
                                           CLI_IPP_FIRST, "an IPP message's attributes")
                                : NULL;
    if (attributes == NULL)
    {
        message->full = 1;
        return;
    }
    message->attribute = attributes;
    // a name is at most what the encoding's 16 bits count
    const CliIppAttribute attribute = {cli_kept(message, name, length), (unsigned int)length, message->values, 0u};
    message->attribute[message->attributes] = attribute;
    message->attributes += 1u;
    message->group[message->groups - 1u].count += 1u;
}

void cli_ipp_attribute(CliIppMessage *message, const char *name)
{
    cli_attribute_counted(message, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
}

// a value put after the last attribute's
static void cli_value(CliIppMessage *message, const CliIppValue *value)
{
    CliIppValue *const values =
        (message->attributes != 0u)
            ? cli_room(message, message->value, message->values, &message->value_room, sizeof(CliIppValue),
                       message->values + 1ull, CLI_IPP_VALUES, CLI_IPP_FIRST, "an IPP message's values")
            : NULL;
    if (values == NULL)
    {
        message->full = 1;
        return;
    }
    message->value = values;
    message->value[message->values] = *value;
    message->values += 1u;
    message->attribute[message->attributes - 1u].count += 1u;
}

void cli_ipp_number(CliIppMessage *message, unsigned int kind, int value)
{
    const CliIppValue made = {kind, (kind == CLI_IPP_ENUM) ? 0x23u : 0x21u, value, 0, 0u, 0u};
    cli_value(message, &made);
}

void cli_ipp_truth(CliIppMessage *message, int value)
{
    const CliIppValue made = {CLI_IPP_BOOLEAN, 0x22u, value ? 1 : 0, 0, 0u, 0u};
    cli_value(message, &made);
}

void cli_ipp_range(CliIppMessage *message, int low, int high)
{
    const CliIppValue made = {CLI_IPP_RANGE, 0x33u, low, high, 0u, 0u};
    cli_value(message, &made);
}

void cli_ipp_text(CliIppMessage *message, unsigned int tag, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    // a text is at most what the encoding's 16 bits count
    const CliIppValue made = {CLI_IPP_TEXT, tag, 0, 0, cli_kept(message, text, length), (unsigned int)length};
    cli_value(message, &made);
}

// a number's bytes, the high first
static void cli_bytes_of(unsigned long long value, unsigned int count, CliLine *out)
{
    for (unsigned int byte = count; byte > 0u; byte -= 1u)
    {
        // each byte of the number in turn
        cli_line_character(out, (char)(unsigned char)((value >> (8u * (byte - 1u))) & 0xffu));
    }
}

void cli_ipp_encode(const CliIppMessage *message, CliLine *out)
{
    cli_line_character(out, 2);
    cli_line_character(out, 0);
    cli_bytes_of(message->code, 2u, out);
    cli_bytes_of(message->request_id, 4u, out);
    for (unsigned int group_at = 0u; group_at < message->groups; group_at += 1u)
    {
        const CliIppGroup *const group = &message->group[group_at];
        // a tag is a byte
        cli_line_character(out, (char)(unsigned char)group->tag);
        for (unsigned int each = 0u; each < group->count; each += 1u)
        {
            const CliIppAttribute *const attribute = &message->attribute[group->first + each];
            for (unsigned int index = 0u; index < attribute->count; index += 1u)
            {
                const CliIppValue *const value = &message->value[attribute->first + index];
                cli_line_character(out, (char)(unsigned char)value->tag);
                const unsigned int named = (index == 0u) ? attribute->name_length : 0u;
                cli_bytes_of(named, 2u, out);
                cli_line_counted(out, (const char *)(message->bytes + attribute->name), named);
                // a number's bits are written as the 32 bits they are
                switch (value->kind)
                {
                case CLI_IPP_INTEGER:
                case CLI_IPP_ENUM:
                    cli_bytes_of(4u, 2u, out);
                    cli_bytes_of((unsigned int)value->low, 4u, out);
                    break;
                case CLI_IPP_BOOLEAN:
                    cli_bytes_of(1u, 2u, out);
                    cli_line_character(out, (char)(value->low ? 1 : 0));
                    break;
                case CLI_IPP_RANGE:
                    cli_bytes_of(8u, 2u, out);
                    cli_bytes_of((unsigned int)value->low, 4u, out);
                    cli_bytes_of((unsigned int)value->high, 4u, out);
                    break;
                default:
                    cli_bytes_of(value->length, 2u, out);
                    cli_line_counted(out, (const char *)(message->bytes + value->bytes), value->length);
                    break;
                }
            }
        }
    }
    cli_line_character(out, (char)CLI_IPP_END);
}

// `count` bytes taken from `at`: where they start, or NULL where the answer is cut short
static const unsigned char *cli_take(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                                     unsigned long long count)
{
    if ((*at + count) > size)
    {
        return NULL;
    }
    const unsigned char *const taken = bytes + *at;
    *at += count;
    return taken;
}

static unsigned long long cli_number(const unsigned char *raw, unsigned int count)
{
    unsigned long long value = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        value = (value << 8u) | raw[at];
    }
    return value;
}

// four bytes read as the signed number they write, 0 where they are fewer
static int cli_signed(const unsigned char *raw, unsigned long long length)
{
    // the 32 bits read as the signed number they write
    return (length >= 4ull) ? (int)(unsigned int)cli_number(raw, 4u) : 0;
}

int cli_ipp_decode(const unsigned char *bytes, unsigned long long size, CliIppMessage *message, CliLine *said)
{
    unsigned long long at = 2ull;
    const unsigned char *const code = cli_take(bytes, size, &at, 2ull);
    const unsigned char *const request = cli_take(bytes, size, &at, 4ull);
    if ((code == NULL) || (request == NULL))
    {
        cli_line_text(said, "the printer's answer is cut short");
        return 0;
    }
    // a status is 16 bits and an id 32
    cli_ipp_begun(message, (unsigned int)cli_number(code, 2u), (unsigned int)cli_number(request, 4u));
    for (;;)
    {
        const unsigned char *const tag = cli_take(bytes, size, &at, 1ull);
        if (tag == NULL)
        {
            cli_line_text(said, "the printer's answer is cut short");
            return 0;
        }
        if (tag[0] == CLI_IPP_END)
        {
            break;
        }
        if (tag[0] < 0x10u)
        {
            cli_ipp_group(message, tag[0]);
            continue;
        }
        const unsigned char *const name_size = cli_take(bytes, size, &at, 2ull);
        const unsigned char *const name =
            (name_size != NULL) ? cli_take(bytes, size, &at, cli_number(name_size, 2u)) : NULL;
        const unsigned char *const value_size = (name != NULL) ? cli_take(bytes, size, &at, 2ull) : NULL;
        const unsigned long long length = (value_size != NULL) ? cli_number(value_size, 2u) : 0ull;
        const unsigned char *const raw = (value_size != NULL) ? cli_take(bytes, size, &at, length) : NULL;
        if (raw == NULL)
        {
            cli_line_text(said, "the printer's answer is cut short");
            return 0;
        }
        if (message->groups == 0u)
        {
            cli_line_text(said, "the printer's answer has an attribute in no group");
            return 0;
        }
        // a value with no name is one more of the attribute before it, and passed over where its group has none
        const unsigned long long name_length = cli_number(name_size, 2u);
        if (name_length != 0ull)
        {
            cli_attribute_counted(message, name, name_length);
        }
        else if (message->group[message->groups - 1u].count == 0u)
        {
            continue;
        }
        CliIppValue value = {CLI_IPP_OTHER, tag[0], 0, 0, 0u, 0u};
        switch (tag[0])
        {
        case 0x21u:
            value.kind = CLI_IPP_INTEGER;
            value.low = cli_signed(raw, length);
            break;
        case 0x22u:
            value.kind = CLI_IPP_BOOLEAN;
            value.low = (length >= 1ull) && (raw[0] == 1u);
            break;
        case 0x23u:
            value.kind = CLI_IPP_ENUM;
            value.low = cli_signed(raw, length);
            break;
        case 0x33u:
            value.kind = CLI_IPP_RANGE;
            value.low = cli_signed(raw, length);
            value.high = (length >= 8ull) ? cli_signed(raw + 4u, 4u) : 0;
            break;
        default:
            value.kind = ((tag[0] >= 0x41u) && (tag[0] <= 0x49u)) ? CLI_IPP_TEXT : CLI_IPP_OTHER;
            value.bytes = cli_kept(message, raw, length);
            // a value is at most what the encoding's 16 bits count
            value.length = (unsigned int)length;
            break;
        }
        cli_value(message, &value);
    }
    message->data = bytes + at;
    message->data_length = size - at;
    if (message->full)
    {
        cli_line_text(said, "the printer's answer holds more than orior_cli reads");
        return 0;
    }
    return 1;
}

const CliIppGroup *cli_ipp_group_of(const CliIppMessage *message, unsigned int tag, unsigned int after)
{
    for (unsigned int at = after; at < message->groups; at += 1u)
    {
        if (message->group[at].tag == tag)
        {
            return &message->group[at];
        }
    }
    return NULL;
}

const CliIppAttribute *cli_ipp_get(const CliIppMessage *message, const CliIppGroup *group, const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (unsigned int each = 0u; (group != NULL) && (each < group->count); each += 1u)
    {
        const CliIppAttribute *const attribute = &message->attribute[group->first + each];
        if ((attribute->name_length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = message->bytes + attribute->name, .other = name, .bytes = length}) == 0))
        {
            return attribute;
        }
    }
    return NULL;
}

int cli_ipp_value_text(const CliIppMessage *message, const CliIppValue *value, char *out, unsigned long long room)
{
    if (value->kind != CLI_IPP_TEXT)
    {
        return 0;
    }
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, (const char *)(message->bytes + value->bytes), value->length);
    cli_line_closed(&line);
    return 1;
}

int cli_ipp_first_text(const CliIppMessage *message, const CliIppGroup *group, const char *name, char *out,
                       unsigned long long room)
{
    const CliIppAttribute *const attribute = cli_ipp_get(message, group, name);
    return (attribute != NULL) && (attribute->count != 0u) &&
           cli_ipp_value_text(message, &message->value[attribute->first], out, room);
}

int cli_ipp_first_number(const CliIppMessage *message, const CliIppGroup *group, const char *name, int *number)
{
    const CliIppAttribute *const attribute = cli_ipp_get(message, group, name);
    if ((attribute == NULL) || (attribute->count == 0u))
    {
        return 0;
    }
    const CliIppValue *const value = &message->value[attribute->first];
    *number = value->low;
    return (value->kind == CLI_IPP_INTEGER) || (value->kind == CLI_IPP_ENUM);
}
