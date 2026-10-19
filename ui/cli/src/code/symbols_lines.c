// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_lines.c: a line read for what it declares in the shell's, PowerShell's, TOML's, Markdown's and TeX's
// shapes
#include "symbols_internal.h"

// a shell word at the start of a text: letters, digits, `_` and `-`
static CliPiece cli_symbols_shell_word(CliPiece text)
{
    unsigned long long end = 0ull;
    while ((end < text.length) && (cli_piece_alnum(text.at[end]) || (text.at[end] == '_') || (text.at[end] == '-')))
    {
        end += 1ull;
    }
    const CliPiece word = {text.at, end};
    return word;
}

void cli_symbols_shell(CliPiece line, const CliOut *out)
{
    const CliPiece trimmed = cli_piece_trim_start(line);
    CliPiece rest;
    if (cli_piece_after(trimmed, "function", &rest))
    {
        const CliPiece name = cli_symbols_shell_word(rest);
        if (name.length != 0ull)
        {
            cli_piece_put(out, name, "function");
        }
        return;
    }
    const CliPiece name = cli_symbols_shell_word(trimmed);
    if ((name.length != 0ull) && (cli_piece_alpha(name.at[0]) || (name.at[0] == '_')) &&
        cli_piece_starts(cli_piece_trim_start(cli_piece_from(trimmed, name.length)), "()"))
    {
        cli_piece_put(out, name, "function");
    }
}

void cli_symbols_powershell(CliPiece line, const CliOut *out)
{
    const CliPiece trimmed = cli_piece_trim_start(line);
    if (trimmed.length <= 9u)
    {
        return;
    }
    static const char s_word[10] = "function ";
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        const char letter =
            ((trimmed.at[at] >= 'A') && (trimmed.at[at] <= 'Z')) ? (char)(trimmed.at[at] + 32) : trimmed.at[at];
        if (letter != s_word[at])
        {
            return;
        }
    }
    const CliPiece name = cli_symbols_shell_word(cli_piece_trim_start(cli_piece_from(trimmed, 9u)));
    if (name.length != 0ull)
    {
        cli_piece_put(out, name, "function");
    }
}

void cli_symbols_toml(CliPiece line, const CliOut *out)
{
    if (!cli_piece_starts(line, "["))
    {
        return;
    }
    CliPiece rest = line;
    while (cli_piece_starts(rest, "["))
    {
        rest = cli_piece_from(rest, 1u);
    }
    const unsigned long long close = cli_piece_find(rest, ']');
    const CliPiece whole = {rest.at, close};
    const CliPiece name = cli_piece_trim_end(cli_piece_trim_start(whole));
    if ((close < rest.length) && (name.length != 0ull))
    {
        cli_piece_put(out, name, "module");
    }
}

void cli_symbols_markdown(CliPiece line, const CliOut *out)
{
    unsigned long long marks = 0ull;
    while ((marks < line.length) && (line.at[marks] == '#'))
    {
        marks += 1ull;
    }
    if ((marks < 1u) || (marks > 6u) || !cli_piece_starts(cli_piece_from(line, marks), " "))
    {
        return;
    }
    CliPiece title = cli_piece_trim_end(cli_piece_trim_start(cli_piece_from(line, marks)));
    while ((title.length != 0ull) && (title.at[title.length - 1u] == '#'))
    {
        title.length -= 1ull;
    }
    title = cli_piece_trim_end(title);
    if (title.length != 0ull)
    {
        cli_piece_put(out, title, "heading");
    }
}

void cli_symbols_tex(CliPiece line, const CliOut *out)
{
    static const char *const s_levels[6] = {"part", "chapter", "section", "subsection", "subsubsection", "paragraph"};
    CliPiece text = line;
    for (;;)
    {
        const unsigned long long slash = cli_piece_find(text, '\\');
        if (slash >= text.length)
        {
            return;
        }
        text = cli_piece_from(text, slash + 1u);
        CliPiece word;
        CliPiece rest;
        if (!cli_piece_name_at(text, 0, &word, &rest) || !cli_piece_in(word, s_levels, 6u))
        {
            continue;
        }
        rest = cli_piece_trim_start(cli_piece_starts(rest, "*") ? cli_piece_from(rest, 1u) : rest);
        if (cli_piece_starts(rest, "["))
        {
            const unsigned long long close = cli_piece_find(rest, ']');
            rest = (close < rest.length) ? cli_piece_trim_start(cli_piece_from(rest, close + 1u))
                                         : cli_piece_from(rest, rest.length);
        }
        if (cli_piece_starts(rest, "{"))
        {
            const CliPiece inner = cli_piece_from(rest, 1u);
            const unsigned long long close = cli_piece_find(inner, '}');
            const CliPiece title = {inner.at, close};
            if (close < inner.length)
            {
                cli_piece_put(out, cli_piece_trim_end(cli_piece_trim_start(title)), "heading");
            }
        }
        return;
    }
}
