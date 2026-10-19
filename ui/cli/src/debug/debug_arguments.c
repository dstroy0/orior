// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_arguments.c: what the requests to an adapter are made of and read by: numbers, truths and texts read from a
// tree, the files a session takes, bytes from base64, a launch's arguments with their names filled in, the arguments
// that set stepping, and breakpoints as the protocol writes them
#include "debug_internal.h"

// the bytes of a string filled in, and the lock the room it is filled in is held by
#define CLI_DEBUG_FILLED (1u << 16u)

static char s_filled[2][CLI_DEBUG_FILLED];
static CliLock s_filled_lock = CLI_LOCK_INIT;

// the spaces a text is trimmed of
static const char s_spaces[] = " \t\n\r\f\v";

int cli_debug_signed(const CliJson *json, unsigned int node, long long *value)
{
    const char *const text = cli_json_is(json, node, CFG_JSON_NUMBER) ? json->text + json->node[node].text : NULL;
    const unsigned long long length = (text != NULL) ? json->node[node].length : 0ull;
    const int negative = (length > 0ull) && (text[0] == '-');
    const unsigned long long first = negative ? 1ull : 0ull;
    unsigned long long whole = 0ull;
    int read = (length > first) && (length - first <= 19ull);
    for (unsigned long long at = first; read && (at < length); at += 1ull)
    {
        read = (text[at] >= '0') && (text[at] <= '9');
        whole = (whole * 10ull) + (unsigned long long)(text[at] - '0');
    }
    read = read && (whole <= (negative ? 9223372036854775808ull : 9223372036854775807ull));
    // the least number is its own negative's bits, as two's complement writes it
    *value = read ? (negative ? (long long)(0ull - whole) : (long long)whole) : 0ll;
    return read;
}

int cli_debug_unsigned(const CliJson *json, unsigned int node, unsigned long long *value)
{
    const char *const text = cli_json_is(json, node, CFG_JSON_NUMBER) ? json->text + json->node[node].text : NULL;
    const unsigned long long length = (text != NULL) ? json->node[node].length : 0ull;
    unsigned long long whole = 0ull;
    int read = (length > 0ull) && (length <= 20ull);
    for (unsigned long long at = 0ull; read && (at < length); at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        read = (text[at] >= '0') && (text[at] <= '9') && (whole <= (18446744073709551615ull - digit) / 10ull);
        whole = (whole * 10ull) + digit;
    }
    *value = read ? whole : 0ull;
    return read;
}

int cli_debug_true(const CliJson *json, unsigned int node)
{
    return cli_json_is(json, node, CFG_JSON_TRUE);
}

const char *cli_debug_text(const CliJson *json, unsigned int node, const char *fallback)
{
    const char *const text = cli_json_string(json, node, NULL);
    return (text != NULL) ? text : fallback;
}

// whether `text` ends with `ending`, case aside
static int arguments_ends_with(const char *text, const char *ending)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = ending, .cap = CLI_CELLS_POOL});
    for (size_t at = 0u; (size <= length) && (at < size); at += 1u)
    {
        char one = text[length - size + at];
        one = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
        if (one != ending[at])
        {
            return 0;
        }
    }
    return size <= length;
}

int cli_debug_takes(const char *language, const char *file)
{
    const int python = arguments_ends_with(file, ".py") || arguments_ends_with(file, ".pyw");
    return (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL})) == python;
}

unsigned long long cli_debug_base64(const char *text, unsigned long long size, unsigned char *out,
                                    unsigned long long room)
{
    unsigned long long count = 0ull;
    unsigned int held = 0u;
    unsigned int bits = 0u;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        const char one = text[at];
        const int value = ((one >= 'A') && (one <= 'Z'))   ? (one - 'A')
                          : ((one >= 'a') && (one <= 'z')) ? (one - 'a' + 26)
                          : ((one >= '0') && (one <= '9')) ? (one - '0' + 52)
                          : ((one == '+') || (one == '-')) ? 62
                          : ((one == '/') || (one == '_')) ? 63
                                                           : -1;
        if (value < 0)
        {
            continue;
        }
        held = (held << 6u) | (unsigned int)value;
        bits += 6u;
        if (bits >= 8u)
        {
            bits -= 8u;
            if (count < room)
            {
                out[count] = (unsigned char)((held >> bits) & 0xffu);
                count += 1ull;
            }
            held &= (1u << bits) - 1u;
        }
    }
    return count;
}

// a string filled in: each "{name}" of `names` replaced by its `withs`, one name after another, into the filled room
static const char *arguments_string_filled(const char *text, unsigned long long length, const char *const *names,
                                           const char *const *withs, unsigned int count, unsigned long long *size)
{
    CliLine first = {s_filled[0], sizeof(s_filled[0]), 0ull};
    cli_line_counted(&first, text, length);
    unsigned int from = 0u;
    unsigned long long at_end = first.at;
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        char mark[CLI_NAME_LONGEST];
        CliLine marked = {mark, sizeof(mark), 0ull};
        cli_line_character(&marked, '{');
        cli_line_text(&marked, names[one]);
        cli_line_character(&marked, '}');
        const unsigned long long mark_size = marked.at;
        CliLine next = {s_filled[1u - from], sizeof(s_filled[0]), 0ull};
        const char *const was = s_filled[from];
        for (unsigned long long at = 0ull; at < at_end;)
        {
            if ((at + mark_size <= at_end) &&
                (memor.cmp(&(MemoriaCfg){.src = was + at, .other = mark, .bytes = mark_size}) == 0))
            {
                cli_line_text(&next, withs[one]);
                at += mark_size;
            }
            else
            {
                cli_line_character(&next, was[at]);
                at += 1ull;
            }
        }
        at_end = next.at;
        from = 1u - from;
    }
    *size = at_end;
    return s_filled[from];
}

// whether a string is digits alone, its number into `value`: a number past 64 bits stays a string
static int arguments_digits(const char *text, unsigned long long length, unsigned long long *value)
{
    unsigned long long whole = 0ull;
    int digits = (length > 0ull);
    for (unsigned long long at = 0ull; digits && (at < length); at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        digits = (text[at] >= '0') && (text[at] <= '9') && (whole <= (18446744073709551615ull - digit) / 10ull);
        whole = (whole * 10ull) + digit;
    }
    *value = whole;
    return digits;
}

static unsigned int arguments_filled(CliJson *to, const CliJson *json, unsigned int from, const char *const *names,
                                     const char *const *withs, unsigned int count, int numbered)
{
    if (cli_json_is(json, from, CFG_JSON_STRING))
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(json, from, &length);
        unsigned long long size = 0ull;
        const char *const filled = arguments_string_filled(text, length, names, withs, count, &size);
        unsigned long long value = 0ull;
        if (numbered && arguments_digits(filled, size, &value))
        {
            char number[32];
            CliLine written = {number, sizeof(number), 0ull};
            cli_line_decimal(&written, value, 1u);
            cli_line_closed(&written);
            return cli_json_number_made(to, number);
        }
        return cli_json_string_made(to, filled, size);
    }
    if (cli_json_is(json, from, CFG_JSON_ARRAY))
    {
        const unsigned int array = cli_json_made(to, CFG_JSON_ARRAY);
        for (unsigned int item = json->node[from].first; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            cli_json_append(to, array, arguments_filled(to, json, item, names, withs, count, numbered));
        }
        return array;
    }
    if (cli_json_is(json, from, CFG_JSON_OBJECT))
    {
        const unsigned int object = cli_json_made(to, CFG_JSON_OBJECT);
        for (unsigned int member = json->node[from].first; member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const unsigned int value = arguments_filled(to, json, member, names, withs, count, numbered);
            cli_json_set(to, object, json->text + json->node[member].key, value);
        }
        return object;
    }
    return (from == CLI_JSON_NONE) ? cli_json_made(to, CFG_JSON_NULL) : cli_json_copy_from(to, json, from);
}

unsigned int cli_debug_filled(CliJson *to, const CliJson *json, unsigned int from, const char *const *names,
                              const char *const *withs, unsigned int count, int numbered)
{
    cli_lock_take(&s_filled_lock);
    const unsigned int filled = arguments_filled(to, json, from, names, withs, count, numbered);
    cli_lock_give(&s_filled_lock);
    return filled;
}

// a text trimmed of spaces at both ends: its start, its length into `length`
static const char *arguments_trimmed(const char *text, unsigned long long *length)
{
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    unsigned long long start = 0ull;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    *length = end - start;
    return text + start;
}

// whether a pattern names a path, as one with a slash, a star or a dot that is not a module's dotted name does
static int arguments_names_path(const char *one)
{
    int marked = 0;
    int dotted = 1;
    for (const char *letter = one; *letter != '\0'; letter += 1)
    {
        const char at = *letter;
        marked = marked || (at == '/') || (at == '\\') || (at == '*') || (at == '.');
        dotted = dotted && (((at >= 'a') && (at <= 'z')) || ((at >= 'A') && (at <= 'Z')) ||
                            ((at >= '0') && (at <= '9')) || (at == '_') || (at == '.') || ((unsigned char)at >= 0x80u));
    }
    return marked && !dotted;
}

void cli_debug_step_arguments(const char *language, const CliJson *json, unsigned int stepping, CliJson *to,
                              unsigned int arguments)
{
    if (!cli_json_is(to, arguments, CFG_JSON_OBJECT))
    {
        return;
    }
    const unsigned int skip = cli_json_member(json, stepping, "skip");
    const unsigned int mine = cli_json_member(json, stepping, "mine_only");
    if (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}))
    {
        const int mine_only = (mine == CLI_JSON_NONE) || cli_debug_true(json, mine);
        cli_json_set(to, arguments, "justMyCode", cli_json_made(to, mine_only ? CFG_JSON_TRUE : CFG_JSON_FALSE));
        const unsigned int rules = cli_json_made(to, CFG_JSON_ARRAY);
        unsigned int count = 0u;
        for (unsigned int at = 0u; cli_json_item(json, skip, at) != CLI_JSON_NONE; at += 1u)
        {
            const char *const one = cli_debug_text(json, cli_json_item(json, skip, at), "");
            unsigned long long length = 0ull;
            const char *const trimmed = arguments_trimmed(one, &length);
            if (length == 0ull)
            {
                continue;
            }
            const unsigned int rule = cli_json_made(to, CFG_JSON_OBJECT);
            cli_json_set(to, rule, arguments_names_path(one) ? "path" : "module",
                         cli_json_string_made(to, trimmed, length));
            cli_json_set(to, rule, "include", cli_json_made(to, CFG_JSON_FALSE));
            cli_json_append(to, rules, rule);
            count += 1u;
        }
        if (count > 0u)
        {
            cli_json_set(to, arguments, "rules", rules);
        }
        return;
    }
    char patterns[CLI_DEBUG_FILLED];
    CliLine joined = {patterns, sizeof(patterns), 0ull};
    cli_line_text(&joined, "settings set target.process.thread.step-avoid-regexp ");
    unsigned int count = 0u;
    for (unsigned int at = 0u; cli_json_item(json, skip, at) != CLI_JSON_NONE; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const trimmed = arguments_trimmed(cli_debug_text(json, cli_json_item(json, skip, at), ""), &length);
        if (length > 0ull)
        {
            cli_line_text(&joined, (count > 0u) ? "|" : "");
            cli_line_counted(&joined, trimmed, length);
            count += 1u;
        }
    }
    if (count == 0u)
    {
        return;
    }
    const unsigned int had = cli_json_member(to, arguments, "initCommands");
    const unsigned int commands =
        cli_json_is(to, had, CFG_JSON_ARRAY) ? cli_json_copy(to, had) : cli_json_made(to, CFG_JSON_ARRAY);
    cli_json_append(to, commands, cli_json_string_made(to, joined.out, joined.at));
    cli_json_set(to, arguments, "initCommands", commands);
}

void cli_debug_breakpoints_written(const CliJson *json, unsigned int breakpoints, CliLine *out)
{
    static const char *const s_keys[3] = {"condition", "hitCondition", "logMessage"};
    static const char *const s_fields[3] = {"condition", "hits", "log"};
    cli_line_character(out, '[');
    for (unsigned int at = 0u; cli_json_item(json, breakpoints, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, breakpoints, at);
        unsigned long long line = 0ull;
        (void)cli_debug_unsigned(json, cli_json_member(json, one, "line"), &line);
        cli_line_text(out, (at > 0u) ? ",{\"line\":" : "{\"line\":");
        cli_line_decimal(out, (line & 0xffffffffull) + 1ull, 1u);
        for (unsigned int field = 0u; field < 3u; field += 1u)
        {
            unsigned long long length = 0ull;
            const char *const trimmed =
                arguments_trimmed(cli_debug_text(json, cli_json_member(json, one, s_fields[field]), ""), &length);
            if (length > 0ull)
            {
                cli_line_text(out, ",\"");
                cli_line_text(out, s_keys[field]);
                cli_line_text(out, "\":");
                cli_json_quoted_lossy(out, (const unsigned char *)trimmed, length);
            }
        }
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
}
