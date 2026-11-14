// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_tree_write.c: a JSON tree written as serde_json writes it: on one line, or arranged a member or item to a line
// with a number of spaces to each level, every object's keys in order where requested
#include "json_tree.h"
#include "text.h"

typedef struct
{
    VerbaLinea *out;
    unsigned char character;
    const unsigned char *text;
    unsigned long long length;
    unsigned long long at;
    unsigned long long from;
    unsigned long long to;
    unsigned int indent;
    unsigned int level;
    const CliJson *json;
    unsigned int parent;
    unsigned int after;
    unsigned int node;
    int sorted;
} JsonTreeWriteCtx;

// a byte JSON writes escaped, a quote, a backslash, a control, written so: 1, or 0 where the byte is as it is
EMBED_INLINE int json_character_escape(const JsonTreeWriteCtx *args)
{
    static const char s_hex[17] = "0123456789abcdef";
    const char *escape;
    switch (args->character)
    {
    case '"': {
        escape = "\\\"";
        break;
    }
    case '\\': {
        escape = "\\\\";
        break;
    }
    case '\n': {
        escape = "\\n";
        break;
    }
    case '\r': {
        escape = "\\r";
        break;
    }
    case '\t': {
        escape = "\\t";
        break;
    }
    case 0x08u: {
        escape = "\\b";
        break;
    }
    case 0x0cu: {
        escape = "\\f";
        break;
    }
    default: {
        escape = NULL;
        break;
    }
    }
    if (escape != NULL)
    {
        mmgr_line_put(args->out, escape);
        return 1;
    }
    if (args->character < 0x20u)
    {
        mmgr_line_put(args->out, "\\u00");
        mmgr_line_ch(args->out, s_hex[args->character >> 4u]);
        mmgr_line_ch(args->out, s_hex[args->character & 0x0fu]);
        return 1;
    }
    return 0;
}

void cli_json_quote(const CliJsonTreeTextLengthCfg *args)
{
    mmgr_line_ch(args->out, '"');
    // each run of bytes that are unchanged is written in one piece; the quote, the backslash and the controls
    // are the bytes escaped
    unsigned long long run = 0ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const unsigned char character = (unsigned char)args->text[at];
        if ((character >= 0x20u) && (character != '"') && (character != '\\'))
        {
            continue;
        }
        mmgr_line_put_n(args->out, args->text + run, at - run);
        (void)EMBED_CALL(json_character_escape, JsonTreeWriteCtx, .out = args->out, .character = character);
        run = at + 1ULL;
    }
    mmgr_line_put_n(args->out, args->text + run, args->length - run);
    mmgr_line_ch(args->out, '"');
}

// the bytes of the UTF-8 sequence that starts at `at`, or 0 where none whole and in its shortest form starts there
EMBED_INLINE unsigned long long cli_sequence(const JsonTreeWriteCtx *args)
{
    const unsigned char lead = args->text[args->at];
    unsigned long long bytes;
    if (lead < 0x80u)
    {
        bytes = 1ULL;
    }
    else if ((lead >= 0xc2u) && (lead <= 0xdfu))
    {
        bytes = 2ULL;
    }
    else if ((lead >= 0xe0u) && (lead <= 0xefu))
    {
        bytes = 3ULL;
    }
    else if ((lead >= 0xf0u) && (lead <= 0xf4u))
    {
        bytes = 4ULL;
    }
    else
    {
        bytes = 0ULL;
    }
    if ((bytes == 0ULL) || ((args->at + bytes) > args->length))
    {
        return 0ULL;
    }
    for (unsigned long long step = 1ULL; step < bytes; step += 1ULL)
    {
        if ((args->text[args->at + step] & 0xc0u) != 0x80u)
        {
            return 0ULL;
        }
    }
    const unsigned char second = (bytes > 1ULL) ? args->text[args->at + 1u] : 0x80u;
    // the overlong, the surrogates and the characters past U+10FFFF are no characters
    const int shortest = !((lead == 0xe0u) && (second < 0xa0u)) && !((lead == 0xedu) && (second >= 0xa0u)) &&
                         !((lead == 0xf0u) && (second < 0x90u)) && !((lead == 0xf4u) && (second >= 0x90u));
    return shortest ? bytes : 0ULL;
}

unsigned long long cli_lossy_character(const CliJsonTreeLengthCfg *args)
{
    unsigned int *character = args->character;
    const unsigned long long bytes =
        EMBED_CALL(cli_sequence, JsonTreeWriteCtx, .text = args->text_bytes, .length = args->length, .at = args->at);
    if (bytes == 0ULL)
    {
        *character = 0xfffdu;
        return 1ULL;
    }
    static const unsigned int s_prefix_bits[5] = {0u, 0x7fu, 0x1fu, 0x0fu, 0x07u};
    unsigned int value = args->text_bytes[args->at] & s_prefix_bits[bytes];
    for (unsigned long long step = 1ULL; step < bytes; step += 1ULL)
    {
        value = (value << 6u) | (args->text_bytes[args->at + step] & 0x3fu);
    }
    *character = value;
    return bytes;
}

// the whole characters of `text` from `from` to before `to` put on `out` together where they fit or the line passes its
// bytes on; else as many whole characters as fit and the line filled, as a character at a time leaves it
EMBED_INLINE void cli_lossy_run_append(const JsonTreeWriteCtx *args)
{
    // a byte of the line is saved back for the terminator
    const unsigned long long capacity =
        (args->out->at < args->out->capacity) ? (args->out->capacity - args->out->at - 1ULL) : 0ULL;
    unsigned long long fits = args->to;
    if ((args->out->flush == NULL) && ((args->to - args->from) > capacity))
    {
        // the characters that fit end where the next begins: a byte that continues a character begins none
        fits = args->from + capacity;
        while ((fits > args->from) && ((args->text[fits] & 0xc0u) == 0x80u))
        {
            fits -= 1ULL;
        }
    }
    mmgr_line_put_n(args->out, (const char *)(args->text + args->from), fits - args->from);
    if (fits < args->to)
    {
        // the line is full and its text ends on the last whole character: no part of the next is written
        if (args->out->at < args->out->capacity)
        {
            args->out->out[args->out->at] = '\0';
        }
        args->out->at = args->out->capacity;
    }
}

void cli_lossy(const CliJsonTreeLengthOutCfg *args)
{
    // the characters from `run` on put together once a byte that starts none, written as U+FFFD, or the end comes
    unsigned long long run = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->length)
    {
        if (args->text_bytes[at] < 0x80u)
        {
            at += 1ULL;
            continue;
        }
        const unsigned long long bytes =
            EMBED_CALL(cli_sequence, JsonTreeWriteCtx, .text = args->text_bytes, .length = args->length, .at = at);
        if (bytes == 0ULL)
        {
            EMBED_CALL(cli_lossy_run_append, JsonTreeWriteCtx, .out = args->out, .text = args->text_bytes, .from = run,
                       .to = at);
            EMBED_CALL(cli_lossy_run_append, JsonTreeWriteCtx, .out = args->out,
                       .text = (const unsigned char *)"\xef\xbf\xbd", .from = 0ULL, .to = 3ULL);
            at += 1ULL;
            run = at;
            continue;
        }
        at += bytes;
    }
    EMBED_CALL(cli_lossy_run_append, JsonTreeWriteCtx, .out = args->out, .text = args->text_bytes, .from = run,
               .to = args->length);
}

void cli_json_quoted_lossy(const CliJsonTreeLengthOutCfg *args)
{
    mmgr_line_ch(args->out, '"');
    cli_json_tree_length_out.json_escaped_lossy(args);
    mmgr_line_ch(args->out, '"');
}

void cli_json_escaped_lossy(const CliJsonTreeLengthOutCfg *args)
{
    unsigned long long at = 0ULL;
    while (at < args->length)
    {
        const unsigned long long bytes =
            EMBED_CALL(cli_sequence, JsonTreeWriteCtx, .text = args->text_bytes, .length = args->length, .at = at);
        if (bytes == 0ULL)
        {
            // a byte that starts no character read as U+FFFD, as from_utf8_lossy reads it
            mmgr_line_put(args->out, "\xef\xbf\xbd");
            at += 1ULL;
            continue;
        }
        if (!EMBED_CALL(json_character_escape, JsonTreeWriteCtx, .out = args->out, .character = args->text_bytes[at]))
        {
            mmgr_line_put_n(args->out, (const char *)args->text_bytes + at, bytes);
        }
        at += bytes;
    }
}

// a line ended and the next begun `level` levels in, `indent` spaces to a level
EMBED_INLINE void cli_line_begin(const JsonTreeWriteCtx *args)
{
    mmgr_line_ch(args->out, '\n');
    for (unsigned long long space = 0ULL; space < ((unsigned long long)args->indent * args->level); space += 1ULL)
    {
        mmgr_line_ch(args->out, ' ');
    }
}

// the child of `parent` whose key comes next after `after`'s in order, the first where `after` is none:
// CLI_JSON_NONE where none does
EMBED_INLINE unsigned int cli_next_sort(const JsonTreeWriteCtx *args)
{
    unsigned int found = CLI_JSON_NONE;
    for (unsigned int child = args->json->node[args->parent].first; child != CLI_JSON_NONE;
         child = args->json->node[child].next)
    {
        const char *const key = args->json->text + args->json->node[child].key;
        const int end = (args->after == CLI_JSON_NONE) ||
                        (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = key,
                                    .other_pointer = args->json->text + args->json->node[args->after].key) > 0);
        if (end && ((found == CLI_JSON_NONE) ||
                    (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = key,
                                .other_pointer = args->json->text + args->json->node[found].key) < 0)))
        {
            found = child;
        }
    }
    return found;
}

static void cli_write(const JsonTreeWriteCtx *args)
{
    VerbaLinea *out = args->out;
    const CliJsonNode *const at = &args->json->node[args->node];
    switch (at->type)
    {
    case CFG_JSON_STRING:
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = args->json->text + at->text, .length = at->length);
        return;
    case CFG_JSON_NUMBER:
        mmgr_line_put_n(out, args->json->text + at->text, at->length);
        return;
    case CFG_JSON_TRUE:
        mmgr_line_put(out, "true");
        return;
    case CFG_JSON_FALSE:
        mmgr_line_put(out, "false");
        return;
    case CFG_JSON_NULL:
        mmgr_line_put(out, "null");
        return;
    default:
        break;
    }
    const int object = at->type == CFG_JSON_OBJECT;
    mmgr_line_ch(out, object ? '{' : '[');
    if (at->first == CLI_JSON_NONE)
    {
        mmgr_line_ch(out, object ? '}' : ']');
        return;
    }
    const int ordered = object && args->sorted;
    unsigned int child = ordered ? EMBED_CALL(cli_next_sort, JsonTreeWriteCtx, .json = args->json, .parent = args->node,
                                              .after = CLI_JSON_NONE)
                                 : at->first;
    int first = 1;
    while (child != CLI_JSON_NONE)
    {
        if (!first)
        {
            mmgr_line_ch(out, ',');
        }
        first = 0;
        if (args->indent != 0u)
        {
            EMBED_CALL(cli_line_begin, JsonTreeWriteCtx, .out = out, .indent = args->indent, .level = args->level + 1u);
        }
        if (object)
        {
            const char *const key = args->json->text + args->json->node[child].key;
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}));
            mmgr_line_put(out, (args->indent != 0u) ? ": " : ":");
        }
        EMBED_CALL(cli_write, JsonTreeWriteCtx, .json = args->json, .node = child, .out = out, .indent = args->indent,
                   .sorted = args->sorted, .level = args->level + 1u);
        child = ordered ? EMBED_CALL(cli_next_sort, JsonTreeWriteCtx, .json = args->json, .parent = args->node,
                                     .after = child)
                        : args->json->node[child].next;
    }
    if (args->indent != 0u)
    {
        EMBED_CALL(cli_line_begin, JsonTreeWriteCtx, .out = out, .indent = args->indent, .level = args->level);
    }
    mmgr_line_ch(out, object ? '}' : ']');
}

void cli_json_write(const CliJsonTreeNodeOutCfg *args)
{
    if (args->node != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_write, JsonTreeWriteCtx, .json = args->json_read, .node = args->node, .out = args->out,
                   .indent = args->indent, .sorted = args->sorted, .level = 0u);
    }
}

// the deepest level a pretty writing's capacity counts
#define CLI_JSON_PRETTY_DEPTH 16ULL

unsigned long long cli_json_write_capacity(const CliJsonTreeNodeOutCfg *args)
{
    // a text's byte is escaped to at max 6, and a node adds at max its quotes, its key's colon, its comma and a
    // literal
    return (args->json_read->used * 6ULL) + ((unsigned long long)args->json_read->nodes * 16ULL) + 16ULL;
}

unsigned long long cli_json_pretty_capacity(const CliJsonTreeNodeOutCfg *args)
{
    // a node adds its line's end, its key's space and its level's spaces
    return cli_json_tree_node_out.json_write_capacity(args) +
           ((unsigned long long)args->json_read->nodes *
            (((unsigned long long)args->indent * CLI_JSON_PRETTY_DEPTH) + 2ULL));
}
