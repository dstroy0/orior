// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reply.c: a JSON reply read off its bytes in one pass with no tree made, as reply.h says
#include "reply.h"
#include "../../../../src/cu/includes/formats/cfg_json/cfg_json.h"

int cli_reply_held(const char *path, CliReply *reply)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    reply->text = (const char *)bytes;
    reply->size = (bytes != NULL) ? size : 0ull;
    return bytes != NULL;
}

void cli_reply_let_go(CliReply *reply)
{
    if (reply->text != NULL)
    {
        cli_file_let_go((const unsigned char *)reply->text, reply->size);
    }
    reply->text = NULL;
    reply->size = 0ull;
}

// the first byte at or after `at` that is no JSON white space: its place, the size where none is
static unsigned long long reply_dark(const CliReply *reply, unsigned long long at)
{
    while ((at < reply->size) && ((reply->text[at] == ' ') || (reply->text[at] == '\t') || (reply->text[at] == '\n') ||
                                  (reply->text[at] == '\r')))
    {
        at += 1ull;
    }
    return at;
}

// the closing quote of the string whose opening quote is at `at`: its place, the size where it has none. Each quote
// found is the end unless an odd run of backslashes stands before it
static unsigned long long reply_string_end(const CliReply *reply, unsigned long long at)
{
    unsigned long long from = at + 1ull;
    while (from < reply->size)
    {
        const char *const quote =
            memor.chr(&(MemoriaCfg){.src = reply->text + from, .val = '"', .bytes = (size_t)(reply->size - from)});
        if (quote == NULL)
        {
            return reply->size;
        }
        const unsigned long long place = (unsigned long long)(quote - reply->text);
        unsigned long long backslashes = 0ull;
        while ((place - backslashes > at + 1ull) && (reply->text[place - backslashes - 1ull] == '\\'))
        {
            backslashes += 1ull;
        }
        if ((backslashes % 2ull) == 0ull)
        {
            return place;
        }
        from = place + 1ull;
    }
    return reply->size;
}

unsigned long long cli_reply_value_end(const CliReply *reply, unsigned long long at)
{
    if (at >= reply->size)
    {
        return reply->size;
    }
    const char first = reply->text[at];
    if (first == '"')
    {
        const unsigned long long end = reply_string_end(reply, at);
        return (end < reply->size) ? end + 1ull : reply->size;
    }
    if ((first == '{') || (first == '['))
    {
        unsigned long long depth = 1ull;
        for (at += 1ull; at < reply->size; at += 1ull)
        {
            const char letter = reply->text[at];
            if (letter == '"')
            {
                at = reply_string_end(reply, at);
            }
            else if ((letter == '{') || (letter == '['))
            {
                depth += 1ull;
            }
            else if ((letter == '}') || (letter == ']'))
            {
                depth -= 1ull;
                if (depth == 0ull)
                {
                    return at + 1ull;
                }
            }
        }
        return reply->size;
    }
    while ((at < reply->size) && (reply->text[at] != ',') && (reply->text[at] != '}') && (reply->text[at] != ']') &&
           (reply->text[at] != ' ') && (reply->text[at] != '\t') && (reply->text[at] != '\n') &&
           (reply->text[at] != '\r'))
    {
        at += 1ull;
    }
    return at;
}

unsigned long long cli_reply_root(const CliReply *reply)
{
    const unsigned long long at = reply_dark(reply, 0ull);
    return (at < reply->size) ? at : CLI_REPLY_NONE;
}

unsigned long long cli_reply_member(const CliReply *reply, unsigned long long object, const char *key)
{
    if ((object == CLI_REPLY_NONE) || (object >= reply->size) || (reply->text[object] != '{'))
    {
        return CLI_REPLY_NONE;
    }
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    unsigned long long at = reply_dark(reply, object + 1ull);
    while ((at < reply->size) && (reply->text[at] == '"'))
    {
        const unsigned long long end = reply_string_end(reply, at);
        if (end >= reply->size)
        {
            return CLI_REPLY_NONE;
        }
        const int same =
            ((end - at - 1ull) == key_size) &&
            (memor.cmp(&(MemoriaCfg){.src = reply->text + at + 1ull, .other = key, .bytes = key_size}) == 0);
        at = reply_dark(reply, end + 1ull);
        if ((at >= reply->size) || (reply->text[at] != ':'))
        {
            return CLI_REPLY_NONE;
        }
        at = reply_dark(reply, at + 1ull);
        if (same)
        {
            return (at < reply->size) ? at : CLI_REPLY_NONE;
        }
        at = reply_dark(reply, cli_reply_value_end(reply, at));
        if ((at < reply->size) && (reply->text[at] == ','))
        {
            at = reply_dark(reply, at + 1ull);
        }
    }
    return CLI_REPLY_NONE;
}

unsigned long long cli_reply_first(const CliReply *reply, unsigned long long array)
{
    if ((array == CLI_REPLY_NONE) || (array >= reply->size) || (reply->text[array] != '['))
    {
        return CLI_REPLY_NONE;
    }
    const unsigned long long at = reply_dark(reply, array + 1ull);
    return ((at < reply->size) && (reply->text[at] != ']')) ? at : CLI_REPLY_NONE;
}

unsigned long long cli_reply_next(const CliReply *reply, unsigned long long item)
{
    if (item == CLI_REPLY_NONE)
    {
        return CLI_REPLY_NONE;
    }
    unsigned long long at = reply_dark(reply, cli_reply_value_end(reply, item));
    if ((at >= reply->size) || (reply->text[at] != ','))
    {
        return CLI_REPLY_NONE;
    }
    at = reply_dark(reply, at + 1ull);
    return ((at < reply->size) && (reply->text[at] != ']')) ? at : CLI_REPLY_NONE;
}

unsigned long long cli_reply_item(const CliReply *reply, unsigned long long array, unsigned long long index)
{
    unsigned long long at = cli_reply_first(reply, array);
    for (; (at != CLI_REPLY_NONE) && (index > 0ull); index -= 1ull)
    {
        at = cli_reply_next(reply, at);
    }
    return at;
}

unsigned long long cli_reply_string_span(const CliReply *reply, unsigned long long at)
{
    if ((at == CLI_REPLY_NONE) || (at >= reply->size) || (reply->text[at] != '"'))
    {
        return 0ull;
    }
    const unsigned long long end = reply_string_end(reply, at);
    return (end < reply->size) ? (end - at - 1ull) : 0ull;
}

int cli_reply_string(const CliReply *reply, unsigned long long at, char *out, unsigned long long capacity)
{
    if ((at == CLI_REPLY_NONE) || (at >= reply->size) || (reply->text[at] != '"'))
    {
        return 0;
    }
    const unsigned long long end = reply_string_end(reply, at);
    if (end >= reply->size)
    {
        return 0;
    }
    const CfgJsonToken token = {CFG_JSON_STRING, (size_t)(at + 1ull), (size_t)end, 0u, 0u};
    return cfg_json_string(reply->text, &token, out, (size_t)capacity);
}

unsigned long long cli_reply_whole(const CliReply *reply, unsigned long long at)
{
    if ((at == CLI_REPLY_NONE) || (at >= reply->size) || (reply->text[at] < '0') || (reply->text[at] > '9'))
    {
        return ~0ull;
    }
    unsigned long long value = 0ull;
    while ((at < reply->size) && (reply->text[at] >= '0') && (reply->text[at] <= '9'))
    {
        if (value > 0xffffffffull)
        {
            return ~0ull;
        }
        value = (value * 10ull) + (unsigned long long)(reply->text[at] - '0');
        at += 1ull;
    }
    return value;
}

int cli_reply_true(const CliReply *reply, unsigned long long at)
{
    return (at != CLI_REPLY_NONE) && (at + 4ull <= reply->size) &&
           (memor.cmp(&(MemoriaCfg){.src = reply->text + at, .other = "true", .bytes = 4u}) == 0);
}

int cli_reply_string_is(const CliReply *reply, unsigned long long at, const char *word)
{
    char text[CLI_NAME_LONGEST];
    return cli_reply_string(reply, at, text, sizeof(text)) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = word, .cap = CLI_CELLS_POOL}));
}
