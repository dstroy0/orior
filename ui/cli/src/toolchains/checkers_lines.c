// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers_lines.c: findings written a line each, `path:line:col: message`, with a severity word and a code where the
// checker writes them, as flake8, mypy and pylint write them
#include "checkers_internal.h"

// whether a byte is ASCII's whitespace, and a span with that whitespace at either end left off
static int cli_blank(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

static void cli_trimmed(const char **at, unsigned long long *length)
{
    while ((*length > 0ull) && cli_blank((*at)[0]))
    {
        *at += 1;
        *length -= 1ull;
    }
    while ((*length > 0ull) && cli_blank((*at)[*length - 1ull]))
    {
        *length -= 1ull;
    }
}

// a line of findings, `path:line:col: rest`, the column where it is written, read into a finding: 1 with the path's
// length, or 0 where the line is none
static int cli_line_parts(const char *line, unsigned long long length, unsigned long long *path_length,
                          CliFinding *finding)
{
    for (unsigned long long at = 1ull; at < length; at += 1ull)
    {
        if (line[at] != ':')
        {
            continue;
        }
        unsigned long long digits = 0ull;
        while (((at + 1ull + digits) < length) && (line[at + 1ull + digits] >= '0') &&
               (line[at + 1ull + digits] <= '9'))
        {
            digits += 1ull;
        }
        if ((digits == 0ull) || ((at + 1ull + digits) >= length) || (line[at + 1ull + digits] != ':'))
        {
            continue;
        }
        unsigned long long number = 0ull;
        for (unsigned long long place = 0ull; place < digits; place += 1ull)
        {
            number = (number * 10ull) + (unsigned long long)(line[at + 1ull + place] - '0');
        }
        const unsigned long long after = at + 2ull + digits;
        unsigned long long col_digits = 0ull;
        while (((after + col_digits) < length) && (line[after + col_digits] >= '0') &&
               (line[after + col_digits] <= '9'))
        {
            col_digits += 1ull;
        }
        unsigned long long col = 1ull;
        unsigned long long rest = after;
        if ((col_digits > 0ull) && ((after + col_digits) < length) && (line[after + col_digits] == ':'))
        {
            col = 0ull;
            for (unsigned long long place = 0ull; place < col_digits; place += 1ull)
            {
                col = (col * 10ull) + (unsigned long long)(line[after + place] - '0');
            }
            rest = after + col_digits + 1ull;
        }
        *path_length = at;
        finding->line = number;
        finding->col = col;
        finding->message = line + rest;
        finding->message_length = length - rest;
        cli_trimmed(&finding->message, &finding->message_length);
        return 1;
    }
    return 0;
}

// a severity word at the start of a message, as mypy writes one, taken off it: an error, a warning, or a note; a
// warning where none is written
static unsigned int cli_severity_of(CliFinding *finding)
{
    static const char *const s_words[4] = {"error:", "warning:", "note:", "info:"};
    static const unsigned int s_severity[4] = {1u, 2u, 3u, 3u};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_words[at], .cap = CLI_CELLS_POOL});
        if ((finding->message_length >= size) &&
            (memor.cmp(&(MemoriaCfg){.src = finding->message, .other = s_words[at], .bytes = size}) == 0))
        {
            finding->message += size;
            finding->message_length -= size;
            cli_trimmed(&finding->message, &finding->message_length);
            return s_severity[at];
        }
    }
    return 2u;
}

// a finding's code taken off its message: as flake8 writes one before it, `E501 ...`, as mypy writes one in brackets
// after it, `...  [arg-type]`, or as pylint writes one in brackets before it, `[C0114(missing-module-docstring), ]`
static void cli_code_of(CliFinding *finding)
{
    const char *const rest = finding->message;
    const unsigned long long length = finding->message_length;
    finding->code = "";
    finding->code_length = 0ull;
    const char *const close = ((length > 0ull) && (rest[0] == '['))
                                  ? memor.chr(&(MemoriaCfg){.src = rest + 1, .val = ']', .bytes = length - 1ull})
                                  : NULL;
    if (close != NULL)
    {
        const char *code = rest + 1;
        unsigned long long size = 0ull;
        while (((code + size) < close) && (code[size] != '(') && (code[size] != ','))
        {
            size += 1ull;
        }
        cli_trimmed(&code, &size);
        finding->code = code;
        finding->code_length = size;
        finding->message = close + 1;
        finding->message_length = (unsigned long long)((rest + length) - (close + 1));
        cli_trimmed(&finding->message, &finding->message_length);
        return;
    }
    for (unsigned long long open = length; (length > 0ull) && (rest[length - 1ull] == ']') && (open >= 3ull);
         open -= 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = rest + open - 3ull, .other = "  [", .bytes = 3u}) == 0)
        {
            finding->code = rest + open;
            finding->code_length = length - open - 1ull;
            finding->message_length = open - 3ull;
            cli_trimmed(&finding->message, &finding->message_length);
            return;
        }
    }
    unsigned long long first = 0ull;
    while ((first < length) && !cli_blank(rest[first]))
    {
        first += 1ull;
    }
    int code = (first >= 2ull) && (rest[0] >= 'A') && (rest[0] <= 'Z');
    for (unsigned long long at = 1ull; code && (at < first); at += 1ull)
    {
        code = (rest[at] >= '0') && (rest[at] <= '9');
    }
    if (code)
    {
        finding->code = rest;
        finding->code_length = first;
        finding->message = rest + first;
        finding->message_length = length - first;
        cli_trimmed(&finding->message, &finding->message_length);
    }
}

void cli_checkers_lines_read(const char *text, unsigned long long size)
{
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        const char *const line = text + at;
        at = end + 1ull;
        CliFinding finding = {0ull, 1ull, 0, 0ull, 0ull, 2u, NULL, 0ull, "", 0ull, "[]"};
        unsigned long long path_length = 0ull;
        if (!cli_line_parts(line, length, &path_length, &finding))
        {
            continue;
        }
        finding.severity = cli_severity_of(&finding);
        cli_code_of(&finding);
        cli_checkers_finding_put(cli_checkers_file_of(line, path_length), &finding);
    }
}
