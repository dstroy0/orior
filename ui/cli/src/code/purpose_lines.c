// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose_lines.c: the spans of a text a comment is read from, trimmed and matched, a license's comment known, and a
// comment's lines with their marks taken off
#include "purpose_lines.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"

CliSpan cli_purpose_trimmed_start(CliSpan span)
{
    while (span.length > 0ull)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)span.at, span.length, 0ull, &letter);
        if (!cli_fuzzy_white(letter))
        {
            break;
        }
        span = (CliSpan){span.at + step, span.length - step};
    }
    return span;
}

CliSpan cli_purpose_trimmed(CliSpan span)
{
    span = cli_purpose_trimmed_start(span);
    unsigned long long end = 0ull;
    for (unsigned long long at = 0ull; at < span.length;)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)span.at, span.length, at, &letter);
        at += step;
        end = cli_fuzzy_white(letter) ? end : at;
    }
    return (CliSpan){span.at, end};
}

int cli_purpose_starts(CliSpan span, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return (span.length >= length) && (memor.cmp(&(MemoriaCfg){.src = span.at, .other = text, .bytes = length}) == 0);
}

int cli_purpose_licensed(const char *text, unsigned long long length)
{
    static const char *const s_words[2] = {"copyright", "license"};
    for (unsigned int word = 0u; word < 2u; word += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_words[word], .cap = CLI_CELLS_POOL});
        for (unsigned long long at = 0ull; (at + size) <= length; at += 1ull)
        {
            unsigned long long same = 0ull;
            while ((same < size) &&
                   ((((text[at + same] >= 'A') && (text[at + same] <= 'Z')) ? (text[at + same] + 32)
                                                                            : text[at + same]) == s_words[word][same]))
            {
                same += 1ull;
            }
            if (same == size)
            {
                return 1;
            }
        }
    }
    return 0;
}

void cli_purpose_uncommented(const CliSpan *lines, unsigned int count, const char *mark, CliLine *out)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        CliSpan rest = cli_purpose_trimmed_start(lines[at]);
        if (cli_purpose_starts(rest, mark))
        {
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
            rest = (CliSpan){rest.at + size, rest.length - size};
        }
        while ((rest.length > 0ull) &&
               ((rest.at[0] == '!') || (rest.at[0] == '/') || (rest.at[0] == '*') || (rest.at[0] == '#')))
        {
            rest = (CliSpan){rest.at + 1, rest.length - 1ull};
        }
        rest = cli_purpose_trimmed(rest);
        if (at != 0u)
        {
            cli_line_character(out, '\n');
        }
        cli_line_counted(out, rest.at, rest.length);
    }
}
