// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reply.c: a JSON reply read off its bytes in one pass with no tree made, as reply.h describes
#include "reply.h"
#include "../../../../src/cu/includes/formats/cfg_json/cfg_json.h"
#include "../command_line/pool.h"
#include "text.h"

int cli_reply_read(const CliReplyPathCfg *args)
{
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = g_cli_text_owner);
    args->reply_out->text = (const char *)bytes;
    args->reply_out->size = size;
    return bytes != NULL;
}

void cli_reply_let_go(const CliReplyPathCfg *args)
{
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)args->reply_out->text),
                                        (g_cli_text_owner));
    args->reply_out->text = NULL;
    args->reply_out->size = 0ULL;
}

typedef struct
{
    const CliReply *reply;
    unsigned long long at;
} ReplyCtx;
// the first byte at or after `at` that is no JSON white space: its place, the size where none is
EMBED_INLINE unsigned long long reply_dark(const ReplyCtx *args)
{
    unsigned long long at = args->at;
    at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->reply->text, .at = at,
                    .size = args->reply->size, .set = " \t\n\r");
    return at;
}

// the closing quote of the string whose opening quote is at `at`: its place, the size where it has none. Each quote
// found is the end unless an odd run of backslashes stands before it
EMBED_INLINE unsigned long long reply_string_end(const ReplyCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long from = at + 1ULL;
    while (from < args->reply->size)
    {
        const char *const quote = memor.chr(
            &(MemoriaCfg){.src = args->reply->text + from, .val = '"', .bytes = (size_t)(args->reply->size - from)});
        if (quote == NULL)
        {
            return args->reply->size;
        }
        const unsigned long long place = (unsigned long long)(quote - args->reply->text);
        unsigned long long backslashes = 0ULL;
        backslashes = (place - (at + 1ULL)) - EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                                         .text = args->reply->text + at + 1ULL,
                                                         .at = place - (at + 1ULL), .set = "\\");
        if ((backslashes % 2ULL) == 0ULL)
        {
            return place;
        }
        from = place + 1ULL;
    }
    return args->reply->size;
}

unsigned long long cli_reply_value_end(const CliReplyAtCfg *args)
{
    unsigned long long at = args->at;
    if (at >= args->reply->size)
    {
        return args->reply->size;
    }
    const char first = args->reply->text[at];
    if (first == '"')
    {
        const unsigned long long end = EMBED_CALL(reply_string_end, ReplyCtx, .reply = args->reply, .at = at);
        return (end < args->reply->size) ? end + 1ULL : args->reply->size;
    }
    if ((first == '{') || (first == '['))
    {
        unsigned long long depth = 1ULL;
        for (at += 1ULL; at < args->reply->size; at += 1ULL)
        {
            const char letter = args->reply->text[at];
            const int closes = (letter == '}') || (letter == ']');
            depth += ((letter == '{') || (letter == '[')) ? 1ULL : 0ULL;
            depth -= closes ? 1ULL : 0ULL;
            if (closes && (depth == 0ULL))
            {
                return at + 1ULL;
            }
            if (letter == '"')
            {
                at = EMBED_CALL(reply_string_end, ReplyCtx, .reply = args->reply, .at = at);
            }
        }
        return args->reply->size;
    }
    at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->reply->text, .at = at,
                    .size = args->reply->size, .set = ",}] \t\n\r");
    return at;
}

unsigned long long cli_reply_root(const CliReplyCfg *args)
{
    const unsigned long long at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply, .at = 0ULL);
    return (at < args->reply->size) ? at : CLI_REPLY_NONE;
}

unsigned long long cli_reply_member(const CliReplyObjectCfg *args)
{
    if ((args->object == CLI_REPLY_NONE) || (args->object >= args->reply->size) ||
        (args->reply->text[args->object] != '{'))
    {
        return CLI_REPLY_NONE;
    }
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    unsigned long long at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply, .at = args->object + 1ULL);
    while ((at < args->reply->size) && (args->reply->text[at] == '"'))
    {
        const unsigned long long end = EMBED_CALL(reply_string_end, ReplyCtx, .reply = args->reply, .at = at);
        if (end >= args->reply->size)
        {
            return CLI_REPLY_NONE;
        }
        const int same =
            ((end - at - 1ULL) == key_size) &&
            (memor.cmp(&(MemoriaCfg){.src = args->reply->text + at + 1ULL, .other = args->key, .bytes = key_size}) ==
             0);
        at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply, .at = end + 1ULL);
        if ((at >= args->reply->size) || (args->reply->text[at] != ':'))
        {
            return CLI_REPLY_NONE;
        }
        at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply, .at = at + 1ULL);
        if (same)
        {
            return (at < args->reply->size) ? at : CLI_REPLY_NONE;
        }
        at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply,
                        .at = EMBED_CALL(cli_reply_at.value_end, CliReplyAtCfg, .reply = args->reply, .at = at));
        if ((at < args->reply->size) && (args->reply->text[at] == ','))
        {
            at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply, .at = at + 1ULL);
        }
    }
    return CLI_REPLY_NONE;
}

unsigned long long cli_reply_first(const CliReplyArrayCfg *args)
{
    if ((args->array == CLI_REPLY_NONE) || (args->array >= args->reply->size) ||
        (args->reply->text[args->array] != '['))
    {
        return CLI_REPLY_NONE;
    }
    const unsigned long long at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply, .at = args->array + 1ULL);
    return ((at < args->reply->size) && (args->reply->text[at] != ']')) ? at : CLI_REPLY_NONE;
}

unsigned long long cli_reply_next(const CliReplyCfg *args)
{
    if (args->item == CLI_REPLY_NONE)
    {
        return CLI_REPLY_NONE;
    }
    unsigned long long at =
        EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply,
                   .at = EMBED_CALL(cli_reply_at.value_end, CliReplyAtCfg, .reply = args->reply, .at = args->item));
    if ((at >= args->reply->size) || (args->reply->text[at] != ','))
    {
        return CLI_REPLY_NONE;
    }
    at = EMBED_CALL(reply_dark, ReplyCtx, .reply = args->reply, .at = at + 1ULL);
    return ((at < args->reply->size) && (args->reply->text[at] != ']')) ? at : CLI_REPLY_NONE;
}

unsigned long long cli_reply_item(const CliReplyArrayCfg *args)
{
    unsigned long long index = args->index;
    unsigned long long at = cli_reply_array.first(args);
    while ((at != CLI_REPLY_NONE) && (index > 0ULL))
    {
        at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->reply, .item = at);
        index -= 1ULL;
    }
    return at;
}

unsigned long long cli_reply_string_span(const CliReplyAtCfg *args)
{
    if ((args->at == CLI_REPLY_NONE) || (args->at >= args->reply->size) || (args->reply->text[args->at] != '"'))
    {
        return 0ULL;
    }
    const unsigned long long end = EMBED_CALL(reply_string_end, ReplyCtx, .reply = args->reply, .at = args->at);
    return (end < args->reply->size) ? (end - args->at - 1ULL) : 0ULL;
}

int cli_reply_string(const CliReplyAtOutCfg *args)
{
    if ((args->at == CLI_REPLY_NONE) || (args->at >= args->reply->size) || (args->reply->text[args->at] != '"'))
    {
        return 0;
    }
    const unsigned long long end = EMBED_CALL(reply_string_end, ReplyCtx, .reply = args->reply, .at = args->at);
    if (end >= args->reply->size)
    {
        return 0;
    }
    const CfgJsonToken token = {CFG_JSON_STRING, (size_t)(args->at + 1ULL), (size_t)end, 0u, 0u};
    return cfg_json_string(args->reply->text, &token, args->out, (size_t)args->capacity);
}

unsigned long long cli_reply_integer(const CliReplyAtCfg *args)
{
    if ((args->at == CLI_REPLY_NONE) || (args->at >= args->reply->size) || (args->reply->text[args->at] < '0') ||
        (args->reply->text[args->at] > '9'))
    {
        return ~0ULL;
    }
    // a number whose digits but the last fit 32 bits is read, and any longer is not
    const unsigned long long value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->reply->text,
                                                .at = args->at, .size = args->reply->size, .end = NULL);
    return (value > ((0xffffffffULL * 10ULL) + 9ULL)) ? ~0ULL : value;
}

int cli_reply_true(const CliReplyAtCfg *args)
{
    return (args->at != CLI_REPLY_NONE) && (args->at + 4ULL <= args->reply->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->reply->text + args->at, .other = "true", .bytes = 4u}) == 0);
}

int cli_reply_string_is(const CliReplyAtCfg *args)
{
    char text[CLI_NAME_MAX_LEN];
    return EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->reply, .at = args->at, .out = text,
                      .capacity = sizeof(text)) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = args->word_text, .cap = CLI_POOL_BYTES}));
}
