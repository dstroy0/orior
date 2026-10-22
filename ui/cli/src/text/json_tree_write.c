// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_tree_write.c: a JSON tree written as serde_json writes it: on one line, or laid out a member or item to a line
// with a number of spaces to each level, every object's keys in order where asked
#include "json_tree.h"
#include "text.h"

// a byte JSON writes escaped, a quote, a backslash, a control, written so: 1, or 0 where the byte stands as it is
static int json_letter_escaped(CliLine *out, unsigned char letter)
{
    static const char s_hex[17] = "0123456789abcdef";
    const char *const escape = (letter == '"')     ? "\\\""
                               : (letter == '\\')  ? "\\\\"
                               : (letter == '\n')  ? "\\n"
                               : (letter == '\r')  ? "\\r"
                               : (letter == '\t')  ? "\\t"
                               : (letter == 0x08u) ? "\\b"
                               : (letter == 0x0cu) ? "\\f"
                                                   : NULL;
    if (escape != NULL)
    {
        cli_line_text(out, escape);
        return 1;
    }
    if (letter < 0x20u)
    {
        cli_line_text(out, "\\u00");
        cli_line_character(out, s_hex[letter >> 4u]);
        cli_line_character(out, s_hex[letter & 0x0fu]);
        return 1;
    }
    return 0;
}

void cli_json_quoted(CliLine *out, const char *text, unsigned long long length)
{
    cli_line_character(out, '"');
    // each run of bytes that stand as they are is written in one piece; the quote, the backslash and the controls
    // are the bytes escaped
    unsigned long long run = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const unsigned char letter = (unsigned char)text[at];
        if ((letter >= 0x20u) && (letter != '"') && (letter != '\\'))
        {
            continue;
        }
        cli_line_counted(out, text + run, at - run);
        (void)json_letter_escaped(out, letter);
        run = at + 1ull;
    }
    cli_line_counted(out, text + run, length - run);
    cli_line_character(out, '"');
}

// the bytes of the UTF-8 sequence that starts at `at`, or 0 where none whole and in its shortest form starts there
static unsigned long long cli_sequence(const unsigned char *text, unsigned long long length, unsigned long long at)
{
    const unsigned char lead = text[at];
    const unsigned long long bytes = (lead < 0x80u)                         ? 1ull
                                     : ((lead >= 0xc2u) && (lead <= 0xdfu)) ? 2ull
                                     : ((lead >= 0xe0u) && (lead <= 0xefu)) ? 3ull
                                     : ((lead >= 0xf0u) && (lead <= 0xf4u)) ? 4ull
                                                                            : 0ull;
    if ((bytes == 0ull) || ((at + bytes) > length))
    {
        return 0ull;
    }
    for (unsigned long long step = 1ull; step < bytes; step += 1ull)
    {
        if ((text[at + step] & 0xc0u) != 0x80u)
        {
            return 0ull;
        }
    }
    const unsigned char second = (bytes > 1ull) ? text[at + 1u] : 0x80u;
    // the overlong, the surrogates and the letters past U+10FFFF are no letters
    const int shortest = !((lead == 0xe0u) && (second < 0xa0u)) && !((lead == 0xedu) && (second >= 0xa0u)) &&
                         !((lead == 0xf0u) && (second < 0x90u)) && !((lead == 0xf4u) && (second >= 0x90u));
    return shortest ? bytes : 0ull;
}

unsigned long long cli_lossy_letter(const unsigned char *text, unsigned long long length, unsigned long long at,
                                    unsigned int *letter)
{
    const unsigned long long bytes = cli_sequence(text, length, at);
    if (bytes == 0ull)
    {
        *letter = 0xfffdu;
        return 1ull;
    }
    static const unsigned int s_lead_bits[5] = {0u, 0x7fu, 0x1fu, 0x0fu, 0x07u};
    unsigned int value = text[at] & s_lead_bits[bytes];
    for (unsigned long long step = 1ull; step < bytes; step += 1ull)
    {
        value = (value << 6u) | (text[at + step] & 0x3fu);
    }
    *letter = value;
    return bytes;
}

void cli_lossy(CliLine *out, const unsigned char *text, unsigned long long length)
{
    unsigned long long at = 0ull;
    while (at < length)
    {
        const unsigned long long bytes = cli_sequence(text, length, at);
        if (bytes == 0ull)
        {
            cli_line_text(out, "\xef\xbf\xbd");
            at += 1ull;
            continue;
        }
        // a letter's bytes, each a char of the line
        cli_line_counted(out, (const char *)(text + at), bytes);
        at += bytes;
    }
}

void cli_json_quoted_lossy(CliLine *out, const unsigned char *text, unsigned long long length)
{
    cli_line_character(out, '"');
    cli_json_escaped_lossy(out, text, length);
    cli_line_character(out, '"');
}

void cli_json_escaped_lossy(CliLine *out, const unsigned char *text, unsigned long long length)
{
    unsigned long long at = 0ull;
    while (at < length)
    {
        const unsigned long long bytes = cli_sequence(text, length, at);
        if (bytes == 0ull)
        {
            // a byte that starts no letter read as U+FFFD, as from_utf8_lossy reads it
            cli_line_text(out, "\xef\xbf\xbd");
            at += 1ull;
            continue;
        }
        if (!json_letter_escaped(out, text[at]))
        {
            cli_line_counted(out, (const char *)text + at, bytes);
        }
        at += bytes;
    }
}

// a line ended and the next begun `level` levels in, `indent` spaces to a level
static void cli_line_begun(CliLine *out, unsigned int indent, unsigned int level)
{
    cli_line_character(out, '\n');
    for (unsigned long long space = 0ull; space < ((unsigned long long)indent * level); space += 1ull)
    {
        cli_line_character(out, ' ');
    }
}

// the child of `parent` whose key comes next after `after`'s in order, the first where `after` is none:
// CLI_JSON_NONE where none does
static unsigned int cli_next_sorted(const CliJson *json, unsigned int parent, unsigned int after)
{
    unsigned int found = CLI_JSON_NONE;
    for (unsigned int child = json->node[parent].first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        const char *const key = json->text + json->node[child].key;
        const int past = (after == CLI_JSON_NONE) || (cli_text_order(key, json->text + json->node[after].key) > 0);
        if (past && ((found == CLI_JSON_NONE) || (cli_text_order(key, json->text + json->node[found].key) < 0)))
        {
            found = child;
        }
    }
    return found;
}

static void cli_written(const CliJson *json, unsigned int node, CliLine *out, unsigned int indent, int sorted,
                        unsigned int level)
{
    const CliJsonNode *const at = &json->node[node];
    switch (at->kind)
    {
    case CFG_JSON_STRING:
        cli_json_quoted(out, json->text + at->text, at->length);
        return;
    case CFG_JSON_NUMBER:
        cli_line_counted(out, json->text + at->text, at->length);
        return;
    case CFG_JSON_TRUE:
        cli_line_text(out, "true");
        return;
    case CFG_JSON_FALSE:
        cli_line_text(out, "false");
        return;
    case CFG_JSON_NULL:
        cli_line_text(out, "null");
        return;
    default:
        break;
    }
    const int object = at->kind == CFG_JSON_OBJECT;
    cli_line_character(out, object ? '{' : '[');
    if (at->first == CLI_JSON_NONE)
    {
        cli_line_character(out, object ? '}' : ']');
        return;
    }
    const int ordered = object && sorted;
    unsigned int child = ordered ? cli_next_sorted(json, node, CLI_JSON_NONE) : at->first;
    int first = 1;
    while (child != CLI_JSON_NONE)
    {
        if (!first)
        {
            cli_line_character(out, ',');
        }
        first = 0;
        if (indent != 0u)
        {
            cli_line_begun(out, indent, level + 1u);
        }
        if (object)
        {
            const char *const key = json->text + json->node[child].key;
            cli_json_quoted(out, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            cli_line_text(out, (indent != 0u) ? ": " : ":");
        }
        cli_written(json, child, out, indent, sorted, level + 1u);
        child = ordered ? cli_next_sorted(json, node, child) : json->node[child].next;
    }
    if (indent != 0u)
    {
        cli_line_begun(out, indent, level);
    }
    cli_line_character(out, object ? '}' : ']');
}

void cli_json_write(const CliJson *json, unsigned int node, CliLine *out, unsigned int indent, int sorted)
{
    if (node != CLI_JSON_NONE)
    {
        cli_written(json, node, out, indent, sorted, 0u);
    }
}
