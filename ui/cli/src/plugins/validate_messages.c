// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_messages.c: the prover's messages as `lean --json` writes them, the barrier a message says stopped the
// check, the axioms a theorem rests on, and the copy of a file that is checked
#include "validate_internal.h"

// the most bytes of the lines of a check's output that are no message, kept as one error
#define CLI_VALIDATE_LOOSE (1u << 20u)

// the next line of `size` bytes from `at`, split at "\n", a "\r" before it taken off: 1 with its start and size, or 0
// where none is left
static int messages_line(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *start,
                         unsigned long long *length)
{
    if (*at >= size)
    {
        return 0;
    }
    *start = *at;
    const char *const found = memor.chr(&(MemoriaCfg){.src = text + *at, .val = '\n', .bytes = size - *at});
    unsigned long long end = (found != NULL) ? (unsigned long long)(found - text) : size;
    *at = (found != NULL) ? end + 1ull : size;
    if ((found != NULL) && (end > *start) && (text[end - 1ull] == '\r'))
    {
        end -= 1ull;
    }
    *length = end - *start;
    return 1;
}

// `length` bytes from `start` trimmed of ASCII whitespace, as their start and length
static void messages_trimmed(const char *text, unsigned long long *start, unsigned long long *length)
{
    while ((*length > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[*start]}) != NULL))
    {
        *start += 1ull;
        *length -= 1ull;
    }
    while ((*length > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[*start + *length - 1ull]}) != NULL))
    {
        *length -= 1ull;
    }
}

// the whole number `part` of the member `key` of `object`, as serde reads a u64: 1, or 0 where there is none
static int messages_place(const CliJson *json, unsigned int object, const char *key, const char *part,
                          unsigned long long *value)
{
    const unsigned int member = cli_json_member(json, cli_json_member(json, object, key), part);
    if (!cli_json_is(json, member, CFG_JSON_NUMBER))
    {
        return 0;
    }
    const char *const text = json->text + json->node[member].text;
    const unsigned long long length = json->node[member].length;
    unsigned long long sum = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        if ((text[at] < '0') || (text[at] > '9') || (sum > (~0ull - digit) / 10ull))
        {
            return 0;
        }
        sum = (sum * 10ull) + digit;
    }
    *value = sum;
    return length > 0ull;
}

// a text put in the messages' texts: its place, or CLI_VALIDATE_NONE where it does not fit
static unsigned long long messages_put(CliProverMessages *messages, const char *text, unsigned long long length)
{
    if (messages->used + length > CLI_VALIDATE_MESSAGE_TEXT)
    {
        return CLI_VALIDATE_NONE;
    }
    memor.cpy(&(MemoriaCfg){.dst = messages->text + messages->used, .src = text, .bytes = length});
    messages->used += length;
    return messages->used - length;
}

// a message put with its text and kind, where it and they fit
static void messages_kept(CliProverMessages *messages, CliProverMessage *message, const char *text,
                          unsigned long long text_size, const char *kind, unsigned long long kind_size)
{
    message->text = messages_put(messages, text, text_size);
    message->text_size = text_size;
    message->kind = messages_put(messages, kind, kind_size);
    message->kind_size = kind_size;
    if ((messages->count < CLI_VALIDATE_MESSAGES) && (message->text != CLI_VALIDATE_NONE) &&
        (message->kind != CLI_VALIDATE_NONE))
    {
        messages->message[messages->count] = *message;
        messages->count += 1u;
    }
}

void cli_validate_messages_of(const char *output, unsigned long long size, CliProverMessages *messages)
{
    static char s_loose[CLI_VALIDATE_LOOSE];
    CliLine loose = {s_loose, sizeof(s_loose), 0ull};
    messages->count = 0u;
    messages->used = 0ull;
    CliJson json;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    while (messages_line(output, size, &at, &start, &length))
    {
        cli_json_begun(&json, g_cli_validate->line_nodes, CLI_VALIDATE_LINE_NODES, g_cli_validate->line_text,
                       CLI_VALIDATE_LINE_TEXT);
        char why[CLI_SAID_LONGEST];
        CliLine unread = {why, sizeof(why), 0ull};
        const unsigned int value =
            cli_json_read(&json, output + start, length, g_cli_validate->line_tokens, CLI_VALIDATE_LINE_NODES, &unread);
        if (value == CLI_JSON_NONE)
        {
            messages_trimmed(output, &start, &length);
            if (length > 0ull)
            {
                cli_line_text(&loose, (loose.at > 0ull) ? "\n" : "");
                cli_line_counted(&loose, output + start, length);
            }
            continue;
        }
        unsigned long long line = 1ull;
        unsigned long long col = 0ull;
        unsigned long long end_line = 0ull;
        unsigned long long end_col = 0ull;
        (void)messages_place(&json, value, "pos", "line", &line);
        (void)messages_place(&json, value, "pos", "column", &col);
        // lines from 1 are read as lines from 0, a 32-bit number's wrapping as Rust's has it
        CliProverMessage message;
        message.line = (unsigned int)((line > 1ull) ? line : 1ull) - 1u;
        message.col = (unsigned int)col;
        message.end_line = messages_place(&json, value, "endPos", "line", &end_line)
                               ? (unsigned int)((end_line > 1ull) ? end_line : 1ull) - 1u
                               : message.line;
        message.end_col =
            messages_place(&json, value, "endPos", "column", &end_col) ? (unsigned int)end_col : message.col + 1u;
        const char *const severity = cli_json_string(&json, cli_json_member(&json, value, "severity"), NULL);
        message.severity =
            (severity == NULL)                                                                            ? 3u
            : (cellul.eq(&(CatenaFinitaCfg){.src = severity, .other = "error", .cap = CLI_CELLS_POOL}))   ? 1u
            : (cellul.eq(&(CatenaFinitaCfg){.src = severity, .other = "warning", .cap = CLI_CELLS_POOL})) ? 2u
                                                                                                          : 3u;
        unsigned long long text_size = 0ull;
        unsigned long long kind_size = 0ull;
        const char *const text = cli_json_string(&json, cli_json_member(&json, value, "data"), &text_size);
        const char *const kind = cli_json_string(&json, cli_json_member(&json, value, "kind"), &kind_size);
        messages_kept(messages, &message, (text != NULL) ? text : "", (text != NULL) ? text_size : 0ull,
                      (kind != NULL) ? kind : "", (kind != NULL) ? kind_size : 0ull);
    }
    if (loose.at > 0ull)
    {
        CliProverMessage message = {1u, 0u, 0u, 0u, 1u, 0ull, 0ull, 0ull, 0ull};
        messages_kept(messages, &message, s_loose, loose.at, "", 0ull);
    }
}

// whether the JSON array `array` holds a string `text` holds
static int messages_any_in(const CliJson *json, unsigned int array, const char *text, unsigned long long length)
{
    for (unsigned int index = 0u; cli_json_item(json, array, index) != CLI_JSON_NONE; index += 1u)
    {
        const char *const part = cli_json_string(json, cli_json_item(json, array, index), NULL);
        if ((part != NULL) && cli_validate_contains(text, length, part))
        {
            return 1;
        }
    }
    return 0;
}

int cli_validate_barrier_of(const CliProverMessages *messages, const CliProverMessage *message)
{
    const CliJson *const json = &g_cli_validate->json;
    const CliTool *const tool = &g_cli_validate->tool;
    const char *const text = messages->text + message->text;
    for (unsigned int at = 0u; at < tool->barrier_count; at += 1u)
    {
        const CliBarrier *const barrier = &tool->barriers[at];
        const int stopped =
            cli_validate_holds(json, barrier->kinds, messages->text + message->kind, message->kind_size) ||
            messages_any_in(json, barrier->says, text, message->text_size);
        if (stopped && !messages_any_in(json, barrier->unless, text, message->text_size))
        {
            return (int)at;
        }
    }
    return -1;
}

int cli_validate_axioms_of(const char *text, unsigned long long size, unsigned long long *name,
                           unsigned long long *name_size, unsigned long long *list, unsigned long long *list_size)
{
    if ((size == 0ull) || (text[0] != '\''))
    {
        return 0;
    }
    const char *const closed = memor.chr(&(MemoriaCfg){.src = text + 1, .val = '\'', .bytes = size - 1ull});
    *name = 1ull;
    *name_size = ((closed != NULL) ? (unsigned long long)(closed - text) : size) - 1ull;
    *list = 0ull;
    *list_size = 0ull;
    if (cli_validate_contains(text, size, "does not depend on any axioms"))
    {
        return 1;
    }
    static const char s_mark[] = "depends on axioms:";
    unsigned long long at = 0ull;
    while ((at + sizeof(s_mark) - 1ull <= size) &&
           (memor.cmp(&(MemoriaCfg){.src = text + at, .other = s_mark, .bytes = sizeof(s_mark) - 1ull}) != 0))
    {
        at += 1ull;
    }
    if (at + sizeof(s_mark) - 1ull > size)
    {
        return 0;
    }
    unsigned long long start = at + sizeof(s_mark) - 1ull;
    unsigned long long length = size - start;
    messages_trimmed(text, &start, &length);
    while ((length > 0ull) && (text[start] == '['))
    {
        start += 1ull;
        length -= 1ull;
    }
    while ((length > 0ull) && (text[start + length - 1ull] == ']'))
    {
        length -= 1ull;
    }
    *list = start;
    *list_size = length;
    return 1;
}

// one line of the copy: each option it sets that is grown set to its grown value where that is larger, each change
// said, written into `out` with "\n" after it
static void copy_line(const char *line, unsigned long long length, unsigned long long number, const char *const *names,
                      const unsigned long long *values, unsigned int count, CliLine *out, CliLine *changed)
{
    char held[8192];
    CliLine kept = {held, sizeof(held), 0ull};
    cli_line_counted(&kept, line, length);
    if (!cli_line_fits(&kept))
    {
        cli_line_counted(out, line, length);
        cli_line_character(out, '\n');
        return;
    }
    for (unsigned int option = 0u; option < count; option += 1u)
    {
        char set[512];
        CliLine set_line = {set, sizeof(set), 0ull};
        cli_line_text(&set_line, "set_option ");
        cli_line_text(&set_line, names[option]);
        cli_line_character(&set_line, ' ');
        const unsigned long long set_size = set_line.at;
        unsigned long long at = 0ull;
        while ((at + set_size <= kept.at) &&
               (memor.cmp(&(MemoriaCfg){.src = held + at, .other = set, .bytes = set_size}) != 0))
        {
            at += 1ull;
        }
        if (at + set_size > kept.at)
        {
            continue;
        }
        const unsigned long long digits_at = at + set_size;
        unsigned long long digits = 0ull;
        unsigned long long own = 0ull;
        int fits = 1;
        while ((digits_at + digits < kept.at) && (held[digits_at + digits] >= '0') && (held[digits_at + digits] <= '9'))
        {
            const unsigned long long digit = (unsigned long long)(held[digits_at + digits] - '0');
            fits = fits && (own <= (~0ull - digit) / 10ull);
            own = fits ? (own * 10ull) + digit : own;
            digits += 1ull;
        }
        if ((digits == 0ull) || !fits || (own == 0ull) || (own >= values[option]))
        {
            continue;
        }
        char value[24];
        CliLine value_line = {value, sizeof(value), 0ull};
        cli_line_decimal(&value_line, values[option], 1u);
        if (kept.at - digits + value_line.at > sizeof(held))
        {
            continue;
        }
        ((value_line.at > digits) ? memor.move_up
                                  : memor.move_down)(&(MemoriaCfg){.dst = held + digits_at + value_line.at,
                                                                   .src = held + digits_at + digits,
                                                                   .bytes = kept.at - digits_at - digits});
        memor.cpy(&(MemoriaCfg){.dst = held + digits_at, .src = value, .bytes = value_line.at});
        kept.at = kept.at - digits + value_line.at;
        cli_line_text(changed, (changed->at > 0ull) ? "; line " : "line ");
        cli_line_decimal(changed, number, 1u);
        cli_line_text(changed, ": set_option ");
        cli_line_text(changed, names[option]);
        cli_line_character(changed, ' ');
        cli_line_decimal(changed, own, 1u);
        cli_line_text(changed, ", checked at ");
        cli_line_counted(changed, value, value_line.at);
    }
    cli_line_counted(out, held, kept.at);
    cli_line_character(out, '\n');
}

unsigned long long cli_validate_copy_of(const char *text, unsigned long long size, const char *const *names,
                                        const unsigned long long *values, unsigned int count, CliLine *changed)
{
    CliLine out = {g_cli_validate->copy, CLI_VALIDATE_COPY, 0ull};
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    unsigned long long number = 0ull;
    while (messages_line(text, size, &at, &start, &length))
    {
        number += 1ull;
        copy_line(text + start, length, number, names, values, count, &out, changed);
    }
    if (number == 0ull)
    {
        cli_line_character(&out, '\n');
    }
    const CliTool *const tool = &g_cli_validate->tool;
    for (unsigned int theorem = 0u; tool->audited && (theorem < g_cli_validate->theorem_count); theorem += 1u)
    {
        const char *const name = g_cli_validate->names + g_cli_validate->theorems[theorem];
        for (const char *letter = tool->print; *letter != '\0';)
        {
            if (cellul.starts(&(CatenaFinitaCfg){.src = letter, .other = "{name}", .cap = CLI_CELLS_POOL}))
            {
                cli_line_text(&out, name);
                letter += 6;
                continue;
            }
            cli_line_character(&out, *letter);
            letter += 1;
        }
        cli_line_character(&out, '\n');
    }
    return cli_line_fits(&out) ? out.at : CLI_VALIDATE_COPY + 1ull;
}
