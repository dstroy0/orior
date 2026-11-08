// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp.c: an IPP message made, encoded and decoded, and its groups and attributes read
#include "ipp.h"

void cli_ipp_begin(const CliIppMessageCodeCfg *args)
{
    unsigned int code = args->code;
    unsigned int request_id = args->request_id;
    g_cli_guard->persistent_buf_release(args->message->group, g_cli_ipp_owner);
    *args->message = (CliIppMessage){0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0ULL, 0};
    args->message->code = code;
    args->message->request_id = request_id;
    const unsigned long long group_bytes = (unsigned long long)CLI_IPP_GROUPS * sizeof(CliIppGroup);
    const unsigned long long attribute_bytes = (unsigned long long)CLI_IPP_ATTRIBUTES * sizeof(CliIppAttribute);
    const unsigned long long value_bytes = (unsigned long long)CLI_IPP_VALUES * sizeof(CliIppValue);
    char *const cell =
        g_cli_guard->persistent_buf_alloc(group_bytes + attribute_bytes + value_bytes + CLI_IPP_BYTES, g_cli_ipp_owner);
    if (cell == NULL)
    {
        args->message->full = 1;
        return;
    }
    args->message->group = (CliIppGroup *)(void *)cell;
    args->message->group_capacity = CLI_IPP_GROUPS;
    args->message->attribute = (CliIppAttribute *)(void *)(cell + group_bytes);
    args->message->attribute_capacity = CLI_IPP_ATTRIBUTES;
    args->message->value = (CliIppValue *)(void *)(cell + group_bytes + attribute_bytes);
    args->message->value_capacity = CLI_IPP_VALUES;
    args->message->bytes = (unsigned char *)(cell + group_bytes + attribute_bytes + value_bytes);
    args->message->byte_capacity = CLI_IPP_BYTES;
}

typedef struct
{
    CliIppMessage *message;
    const void *bytes;
    unsigned long long length;
    const void *name;
    const CliIppValue *value;
    unsigned long long value_u64;
    unsigned int count;
    VerbaLinea *out;
    const unsigned char *bytes_bytes;
    unsigned long long size;
    unsigned long long *at;
    unsigned long long count_u64;
    const unsigned char *raw;
} IppCtx;
// `length` bytes stored among the message's: where they start
EMBED_INLINE unsigned int cli_bytes_store(const IppCtx *args)
{
    const void *bytes = args->bytes;
    if ((args->message->used + args->length) > args->message->byte_capacity)
    {
        args->message->full = 1;
        return 0u;
    }
    const unsigned int at = args->message->used;
    memor.cpy(&(MemoriaCfg){.dst = args->message->bytes + at, .src = bytes, .bytes = args->length});
    // the bytes are below CLI_IPP_BYTES, which an unsigned int counts
    args->message->used += (unsigned int)args->length;
    return at;
}

void cli_ipp_group(const CliIppMessageTagCfg *args)
{
    if (args->message->groups >= args->message->group_capacity)
    {
        args->message->full = 1;
        return;
    }
    const CliIppGroup group = {args->tag, args->message->attributes, 0u};
    args->message->group[args->message->groups] = group;
    args->message->groups += 1u;
}

// an attribute of `length` bytes of `name` written in the last group
EMBED_INLINE void cli_attribute_count(const IppCtx *args)
{
    if ((args->message->groups == 0u) || (args->message->attributes >= args->message->attribute_capacity))
    {
        args->message->full = 1;
        return;
    }
    // a name is at most what the encoding's 16 bits count
    const CliIppAttribute attribute = {
        EMBED_CALL(cli_bytes_store, IppCtx, .message = args->message, .bytes = args->name, .length = args->length),
        (unsigned int)args->length, args->message->values, 0u};
    args->message->attribute[args->message->attributes] = attribute;
    args->message->attributes += 1u;
    args->message->group[args->message->groups - 1u].count += 1u;
}

void cli_ipp_attribute(const CliIppCfg *args)
{
    EMBED_CALL(cli_attribute_count, IppCtx, .message = args->message, .name = args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}));
}

// a value put after the last attribute's
EMBED_INLINE void cli_value(const IppCtx *args)
{
    if ((args->message->attributes == 0u) || (args->message->values >= args->message->value_capacity))
    {
        args->message->full = 1;
        return;
    }
    args->message->value[args->message->values] = *args->value;
    args->message->values += 1u;
    args->message->attribute[args->message->attributes - 1u].count += 1u;
}

void cli_ipp_number(const CliIppMessageTypeCfg *args)
{
    const CliIppValue made = {args->type, (args->type == CLI_IPP_ENUM) ? 0x23u : 0x21u, args->value, 0, 0u, 0u};
    EMBED_CALL(cli_value, IppCtx, .message = args->message, .value = &made);
}

void cli_ipp_truth(const CliIppMessageTypeCfg *args)
{
    const CliIppValue made = {CLI_IPP_BOOLEAN, 0x22u, args->value ? 1 : 0, 0, 0u, 0u};
    EMBED_CALL(cli_value, IppCtx, .message = args->message, .value = &made);
}

void cli_ipp_range(const CliIppMessageLowCfg *args)
{
    const CliIppValue made = {CLI_IPP_RANGE, 0x33u, args->low, args->high, 0u, 0u};
    EMBED_CALL(cli_value, IppCtx, .message = args->message, .value = &made);
}

void cli_ipp_text(const CliIppMessageTagCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    // a text is at most what the encoding's 16 bits count
    const CliIppValue made = {
        CLI_IPP_TEXT,
        args->tag,
        0,
        0,
        EMBED_CALL(cli_bytes_store, IppCtx, .message = args->message, .bytes = args->text, .length = length),
        (unsigned int)length};
    EMBED_CALL(cli_value, IppCtx, .message = args->message, .value = &made);
}

// a number's bytes, the high first
EMBED_INLINE void cli_bytes_get(const IppCtx *args)
{
    for (unsigned int byte = args->count; byte > 0u; byte -= 1u)
    {
        // each byte of the number in turn
        mmgr_line_ch(args->out, (char)(unsigned char)((args->value_u64 >> (8u * (byte - 1u))) & 0xffu));
    }
}

// the length and bytes of `value`, whose message's bytes start at `bytes_bytes`, written to `out`
EMBED_INLINE void cli_value_encode(const IppCtx *args)
{
    const CliIppValue *const value = args->value;
    // a number's bits are written as the 32 bits they are
    switch (value->type)
    {
    case CLI_IPP_INTEGER:
    case CLI_IPP_ENUM:
        EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = 4u, .count = 2u, .out = args->out);
        EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = (unsigned int)value->low, .count = 4u, .out = args->out);
        break;
    case CLI_IPP_BOOLEAN:
        EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = 1u, .count = 2u, .out = args->out);
        mmgr_line_ch(args->out, (char)(value->low ? 1 : 0));
        break;
    case CLI_IPP_RANGE:
        EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = 8u, .count = 2u, .out = args->out);
        EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = (unsigned int)value->low, .count = 4u, .out = args->out);
        EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = (unsigned int)value->high, .count = 4u, .out = args->out);
        break;
    default:
        EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = value->length, .count = 2u, .out = args->out);
        mmgr_line_put_n(args->out, (const char *)(args->bytes_bytes + value->bytes), value->length);
        break;
    }
}

void cli_ipp_encode(const CliIppMessageOutVerbaLineaCfg *args)
{
    mmgr_line_ch(args->out_verba_linea_pointer, 2);
    mmgr_line_ch(args->out_verba_linea_pointer, 0);
    EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = args->message_read->code, .count = 2u,
               .out = args->out_verba_linea_pointer);
    EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = args->message_read->request_id, .count = 4u,
               .out = args->out_verba_linea_pointer);
    for (unsigned int group_at = 0u; group_at < args->message_read->groups; group_at += 1u)
    {
        const CliIppGroup *const group = &args->message_read->group[group_at];
        // a tag is a byte
        mmgr_line_ch(args->out_verba_linea_pointer, (char)(unsigned char)group->tag);
        for (unsigned int each = 0u; each < group->count; each += 1u)
        {
            const CliIppAttribute *const attribute = &args->message_read->attribute[group->first + each];
            for (unsigned int index = 0u; index < attribute->count; index += 1u)
            {
                const CliIppValue *const value = &args->message_read->value[attribute->first + index];
                mmgr_line_ch(args->out_verba_linea_pointer, (char)(unsigned char)value->tag);
                const unsigned int named = (index == 0u) ? attribute->name_length : 0u;
                EMBED_CALL(cli_bytes_get, IppCtx, .value_u64 = named, .count = 2u,
                           .out = args->out_verba_linea_pointer);
                mmgr_line_put_n(args->out_verba_linea_pointer,
                                (const char *)(args->message_read->bytes + attribute->name), named);
                EMBED_CALL(cli_value_encode, IppCtx, .value = value, .bytes_bytes = args->message_read->bytes,
                           .out = args->out_verba_linea_pointer);
            }
        }
    }
    mmgr_line_ch(args->out_verba_linea_pointer, (char)CLI_IPP_END);
}

// `count` bytes taken from `at`: where they start, or NULL where the answer is cut short
EMBED_INLINE const unsigned char *cli_alloc(const IppCtx *args)
{
    unsigned long long *at = args->at;
    if ((*at + args->count_u64) > args->size)
    {
        return NULL;
    }
    const unsigned char *const reserved = args->bytes_bytes + *at;
    *at += args->count_u64;
    return reserved;
}

EMBED_INLINE unsigned long long cli_number(const IppCtx *args)
{
    unsigned long long value = 0ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        value = (value << 8u) | args->raw[at];
    }
    return value;
}

// four bytes read as the signed number they write, 0 where they are fewer
EMBED_INLINE int cli_sign(const IppCtx *args)
{
    // the 32 bits read as the signed number they write
    return (args->length >= 4ULL) ? (int)(unsigned int)EMBED_CALL(cli_number, IppCtx, .raw = args->raw, .count = 4u)
                                  : 0;
}

int cli_ipp_decode(const CliIppMessageCfg *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long at = 2ULL;
    const unsigned char *const code =
        EMBED_CALL(cli_alloc, IppCtx, .bytes_bytes = bytes, .size = args->size, .at = &at, .count_u64 = 2ULL);
    const unsigned char *const request =
        EMBED_CALL(cli_alloc, IppCtx, .bytes_bytes = bytes, .size = args->size, .at = &at, .count_u64 = 4ULL);
    if ((code == NULL) || (request == NULL))
    {
        mmgr_line_put(args->err_msg, "the printer's answer is cut short");
        return 0;
    }
    // a status is 16 bits and an id 32
    EMBED_CALL(cli_ipp_message_code.begin, CliIppMessageCodeCfg, .message = args->message,
               .code = (unsigned int)EMBED_CALL(cli_number, IppCtx, .raw = code, .count = 2u),
               .request_id = (unsigned int)EMBED_CALL(cli_number, IppCtx, .raw = request, .count = 4u));
    for (;;)
    {
        const unsigned char *const tag =
            EMBED_CALL(cli_alloc, IppCtx, .bytes_bytes = bytes, .size = args->size, .at = &at, .count_u64 = 1ULL);
        if (tag == NULL)
        {
            mmgr_line_put(args->err_msg, "the printer's answer is cut short");
            return 0;
        }
        if (tag[0] == CLI_IPP_END)
        {
            break;
        }
        if (tag[0] < 0x10u)
        {
            EMBED_CALL(cli_ipp_message_tag.group, CliIppMessageTagCfg, .message = args->message, .tag = tag[0]);
            continue;
        }
        const unsigned char *const name_size =
            EMBED_CALL(cli_alloc, IppCtx, .bytes_bytes = bytes, .size = args->size, .at = &at, .count_u64 = 2ULL);
        const unsigned char *const name =
            (name_size != NULL) ? EMBED_CALL(cli_alloc, IppCtx, .bytes_bytes = bytes, .size = args->size, .at = &at,
                                             .count_u64 = EMBED_CALL(cli_number, IppCtx, .raw = name_size, .count = 2u))
                                : NULL;
        const unsigned char *const value_size =
            (name != NULL)
                ? EMBED_CALL(cli_alloc, IppCtx, .bytes_bytes = bytes, .size = args->size, .at = &at, .count_u64 = 2ULL)
                : NULL;
        const unsigned long long length =
            (value_size != NULL) ? EMBED_CALL(cli_number, IppCtx, .raw = value_size, .count = 2u) : 0ULL;
        const unsigned char *const raw = (value_size != NULL)
                                             ? EMBED_CALL(cli_alloc, IppCtx, .bytes_bytes = bytes, .size = args->size,
                                                          .at = &at, .count_u64 = length)
                                             : NULL;
        if (raw == NULL)
        {
            mmgr_line_put(args->err_msg, "the printer's answer is cut short");
            return 0;
        }
        if (args->message->groups == 0u)
        {
            mmgr_line_put(args->err_msg, "the printer's answer has an attribute in no group");
            return 0;
        }
        // a value with no name is one more of the attribute before it, and passed over where its group has none
        const unsigned long long name_length = EMBED_CALL(cli_number, IppCtx, .raw = name_size, .count = 2u);
        if (name_length != 0ULL)
        {
            EMBED_CALL(cli_attribute_count, IppCtx, .message = args->message, .name = name, .length = name_length);
        }
        else if (args->message->group[args->message->groups - 1u].count == 0u)
        {
            continue;
        }
        CliIppValue value = {CLI_IPP_OTHER, tag[0], 0, 0, 0u, 0u};
        switch (tag[0])
        {
        case 0x21u:
            value.type = CLI_IPP_INTEGER;
            value.low = EMBED_CALL(cli_sign, IppCtx, .raw = raw, .length = length);
            break;
        case 0x22u:
            value.type = CLI_IPP_BOOLEAN;
            value.low = (length >= 1ULL) && (raw[0] == 1u);
            break;
        case 0x23u:
            value.type = CLI_IPP_ENUM;
            value.low = EMBED_CALL(cli_sign, IppCtx, .raw = raw, .length = length);
            break;
        case 0x33u:
            value.type = CLI_IPP_RANGE;
            value.low = EMBED_CALL(cli_sign, IppCtx, .raw = raw, .length = length);
            value.high = (length >= 8ULL) ? EMBED_CALL(cli_sign, IppCtx, .raw = raw + 4u, .length = 4u) : 0;
            break;
        default:
            value.type = ((tag[0] >= 0x41u) && (tag[0] <= 0x49u)) ? CLI_IPP_TEXT : CLI_IPP_OTHER;
            value.bytes = EMBED_CALL(cli_bytes_store, IppCtx, .message = args->message, .bytes = raw, .length = length);
            // a value is at most what the encoding's 16 bits count
            value.length = (unsigned int)length;
            break;
        }
        EMBED_CALL(cli_value, IppCtx, .message = args->message, .value = &value);
    }
    args->message->data = bytes + at;
    args->message->data_length = args->size - at;
    if (args->message->full)
    {
        mmgr_line_put(args->err_msg, "the printer's answer contains more than orior_cli reads");
        return 0;
    }
    return 1;
}

const CliIppGroup *cli_ipp_group_get(const CliIppTagCfg *args)
{
    for (unsigned int at = args->after; at < args->message_read->groups; at += 1u)
    {
        if (args->message_read->group[at].tag == args->tag)
        {
            return &args->message_read->group[at];
        }
    }
    return NULL;
}

const CliIppAttribute *cli_ipp_get(const CliIppNameCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    for (unsigned int each = 0u; (args->group != NULL) && (each < args->group->count); each += 1u)
    {
        const CliIppAttribute *const attribute = &args->message_read->attribute[args->group->first + each];
        if ((attribute->name_length == length) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->message_read->bytes + attribute->name, .other = args->name, .bytes = length}) == 0))
        {
            return attribute;
        }
    }
    return NULL;
}

int cli_ipp_value_text(const CliIppMessageValueCfg *args)
{
    if (args->value_ipp_value_pointer->type != CLI_IPP_TEXT)
    {
        return 0;
    }
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put_n(&line, (const char *)(args->message_read->bytes + args->value_ipp_value_pointer->bytes),
                    args->value_ipp_value_pointer->length);
    mmgr_line_finish(&line);
    return 1;
}

int cli_ipp_first_text(const CliIppNameCfg *args)
{
    const CliIppAttribute *const attribute = cli_ipp_name.get(args);
    return (attribute != NULL) && (attribute->count != 0u) &&
           EMBED_CALL(cli_ipp_message_value.value_text, CliIppMessageValueCfg, .message_read = args->message_read,
                      .value_ipp_value_pointer = &args->message_read->value[attribute->first], .out = args->out,
                      .capacity = args->capacity);
}

int cli_ipp_first_number(const CliIppNameCfg *args)
{
    int *number = args->number;
    const CliIppAttribute *const attribute = cli_ipp_name.get(args);
    if ((attribute == NULL) || (attribute->count == 0u))
    {
        return 0;
    }
    const CliIppValue *const value = &args->message_read->value[attribute->first];
    *number = value->low;
    return (value->type == CLI_IPP_INTEGER) || (value->type == CLI_IPP_ENUM);
}
