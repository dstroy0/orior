// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_read.c: each line of a file read for what it declares, by the shape a line takes in its file's language:
// Python, Rust, script and C here, the rest in symbols_lines.c
#include "../text/text.h"
#include "symbols_internal.h"

static void cli_python(CliPiece line, const CliOut *out)
{
    static const char *const s_async[1] = {"async"};
    const CliPiece trimmed = cli_piece_trim_start(line);
    const CliPiece rest = cli_piece_without(trimmed, s_async, 1u);
    CliPiece after;
    CliPiece name;
    CliPiece tail;
    if (cli_piece_after(rest, "def", &after))
    {
        if (cli_piece_name_at(after, 0, &name, &tail))
        {
            cli_piece_put(out, name, "function");
        }
    }
    else if (cli_piece_after(trimmed, "class", &after))
    {
        if (cli_piece_name_at(after, 0, &name, &tail))
        {
            cli_piece_put(out, name, "class");
        }
    }
    else if ((trimmed.length == line.length) && cli_piece_name_at(line, 0, &name, &tail))
    {
        // A constant: a name in capitals at the left edge, given a value
        int capitals = 1;
        int upper = 0;
        for (unsigned long long at = 0ull; at < name.length; at += 1ull)
        {
            const char letter = name.at[at];
            capitals = capitals && (((letter >= 'A') && (letter <= 'Z')) || ((letter >= '0') && (letter <= '9')) ||
                                    (letter == '_'));
            upper = upper || ((letter >= 'A') && (letter <= 'Z'));
        }
        const CliPiece value = cli_piece_trim_start(tail);
        int assigned = 0;
        if ((value.length != 0ull) && (value.at[0] == ':'))
        {
            // a type before the value: what follows its `=` must not open `==`; with no `=` it reads as given
            const CliPiece typed = cli_piece_from(value, 1u);
            const unsigned long long equals = cli_piece_find(typed, '=');
            assigned = (equals >= typed.length) || !cli_piece_starts(cli_piece_from(typed, equals + 1u), "=");
        }
        else
        {
            assigned = cli_piece_starts(value, "=") && !cli_piece_starts(value, "==");
        }
        if (capitals && upper && assigned)
        {
            cli_piece_put(out, name, "constant");
        }
    }
}

static void cli_rust(CliPiece line, const CliOut *out)
{
    static const char *const s_leads[3] = {"const", "async", "unsafe"};
    static const char *const s_kinds[6][2] = {{"struct", "class"}, {"enum", "class"}, {"union", "class"},
                                              {"trait", "class"},  {"type", "class"}, {"mod", "module"}};
    static const char *const s_mut[1] = {"mut"};
    CliPiece text = cli_piece_trim_start(line);
    CliPiece rest;
    CliPiece name;
    CliPiece tail;
    if (cli_piece_after(text, "pub", &rest))
    {
        text = rest;
        if (cli_piece_starts(text, "("))
        {
            const unsigned long long close = cli_piece_find(text, ')');
            text = (close < text.length) ? cli_piece_trim_start(cli_piece_from(text, close + 1u))
                                         : cli_piece_from(text, text.length);
        }
    }
    if (cli_piece_starts(text, "macro_rules!"))
    {
        if (cli_piece_name_at(cli_piece_trim_start(cli_piece_from(text, 12u)), 0, &name, &tail))
        {
            cli_piece_put(out, name, "function");
        }
        return;
    }
    CliPiece leading = text;
    for (;;)
    {
        const CliPiece before = leading;
        leading = cli_piece_without(leading, s_leads, 3u);
        if (cli_piece_after(leading, "extern", &rest))
        {
            if (cli_piece_starts(rest, "\""))
            {
                const CliPiece named = cli_piece_from(rest, 1u);
                const unsigned long long close = cli_piece_find(named, '"');
                leading = (close < named.length) ? cli_piece_trim_start(cli_piece_from(named, close + 1u))
                                                 : cli_piece_from(named, named.length);
            }
            else
            {
                leading = rest;
            }
        }
        if (leading.at == before.at)
        {
            break;
        }
    }
    if (cli_piece_after(leading, "fn", &rest))
    {
        if (cli_piece_name_at(rest, 0, &name, &tail))
        {
            cli_piece_put(out, name, "function");
        }
        return;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cli_piece_after(text, s_kinds[at][0], &rest))
        {
            if (cli_piece_name_at(rest, 0, &name, &tail))
            {
                cli_piece_put(out, name, s_kinds[at][1]);
            }
            return;
        }
    }
    static const char *const s_values[2] = {"const", "static"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (cli_piece_after(text, s_values[at], &rest))
        {
            if (cli_piece_name_at(cli_piece_without(rest, s_mut, 1u), 0, &name, &tail) &&
                cli_piece_starts(cli_piece_trim_start(tail), ":"))
            {
                cli_piece_put(out, name, "constant");
            }
            return;
        }
    }
}

static void cli_script(CliPiece line, const CliOut *out)
{
    static const char *const s_leads[3] = {"export", "default", "async"};
    static const char *const s_keeps[7] = {"if", "for", "while", "switch", "catch", "return", "function"};
    static const char *const s_methods[4] = {"static", "async", "get", "set"};
    static const char *const s_async[1] = {"async"};
    const CliPiece trimmed = cli_piece_trim_start(line);
    const int indented = trimmed.length != line.length;
    CliPiece rest;
    CliPiece name;
    CliPiece tail;
    if (cli_piece_after(cli_piece_without(trimmed, s_leads, 3u), "function", &rest))
    {
        while ((rest.length != 0ull) && (rest.at[0] == '*'))
        {
            rest = cli_piece_from(rest, 1u);
        }
        if (cli_piece_name_at(cli_piece_trim_start(rest), 1, &name, &tail))
        {
            cli_piece_put(out, name, "function");
        }
        return;
    }
    if (cli_piece_after(cli_piece_without(trimmed, s_leads, 2u), "class", &rest))
    {
        if (cli_piece_name_at(rest, 1, &name, &tail))
        {
            cli_piece_put(out, name, "class");
        }
        return;
    }
    if (!indented)
    {
        static const char *const s_declares[3] = {"const", "let", "var"};
        const CliPiece declared = cli_piece_without(trimmed, s_leads, 1u);
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (!cli_piece_after(declared, s_declares[at], &rest))
            {
                continue;
            }
            if (cli_piece_name_at(rest, 1, &name, &tail))
            {
                const CliPiece given = cli_piece_trim_start(tail);
                if (cli_piece_starts(given, "=") && !cli_piece_starts(cli_piece_from(given, 1u), "="))
                {
                    const CliPiece value =
                        cli_piece_without(cli_piece_trim_start(cli_piece_from(given, 1u)), s_async, 1u);
                    const unsigned long long close = cli_piece_find(value, ')');
                    CliPiece called;
                    CliPiece after_name;
                    const int arrow =
                        (cli_piece_starts(value, "(") && (close < value.length) &&
                         cli_piece_starts(cli_piece_trim_start(cli_piece_from(value, close + 1u)), "=>")) ||
                        (cli_piece_name_at(value, 1, &called, &after_name) &&
                         cli_piece_starts(cli_piece_trim_start(after_name), "=>"));
                    cli_piece_put(out, name, arrow ? "function" : "constant");
                }
            }
            return;
        }
        return;
    }
    // A method: an indented name and its parameters, its body opening at the line's end
    if (cli_piece_name_at(cli_piece_without(trimmed, s_methods, 4u), 1, &name, &tail) &&
        !cli_piece_in(name, s_keeps, 7u))
    {
        const CliPiece parameters = cli_piece_trim_start(tail);
        const CliPiece ending = cli_piece_trim_end(parameters);
        const unsigned long long close = cli_piece_find(parameters, ')');
        if (cli_piece_starts(parameters, "(") && (ending.length != 0ull) && (ending.at[ending.length - 1u] == '{') &&
            (close < parameters.length) &&
            cli_piece_equals(cli_piece_trim_end(cli_piece_trim_start(cli_piece_from(parameters, close + 1u))), "{"))
        {
            cli_piece_put(out, name, "method");
        }
    }
}

static void cli_c(CliPiece line, const CliOut *out)
{
    static const char *const s_keeps[12] = {"if", "for",     "while",  "switch", "return", "else",
                                            "do", "typedef", "struct", "enum",   "union",  "sizeof"};
    static const char *const s_typedef[1] = {"typedef"};
    static const char *const s_kinds[4] = {"struct", "enum", "union", "class"};
    CliPiece rest;
    CliPiece name;
    CliPiece tail;
    if (cli_piece_starts(line, "#"))
    {
        if (cli_piece_after(cli_piece_trim_start(cli_piece_from(line, 1u)), "define", &rest) &&
            cli_piece_name_at(rest, 0, &name, &tail))
        {
            cli_piece_put(out, name, "constant");
        }
        return;
    }
    if ((line.length == 0ull) || cli_piece_space(line.at[0]))
    {
        return;
    }
    const CliPiece lead = cli_piece_without(line, s_typedef, 1u);
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (cli_piece_after(lead, s_kinds[at], &rest))
        {
            if (cli_piece_name_at(rest, 0, &name, &tail))
            {
                const CliPiece left = cli_piece_trim_end(cli_piece_trim_start(tail));
                if ((left.length == 0ull) || cli_piece_equals(left, "{"))
                {
                    cli_piece_put(out, name, "class");
                }
            }
            return;
        }
    }
    // A function: words for its type, then its name and a parenthesis, at the left edge, not a statement, and either
    // opening its body or running on to the next line
    if (!cli_piece_name_at(line, 0, &name, &tail) || cli_piece_in(name, s_keeps, 12u))
    {
        return;
    }
    const unsigned long long open = cli_piece_find(line, '(');
    if (open >= line.length)
    {
        return;
    }
    const CliPiece whole_head = {line.at, open};
    if ((cli_piece_find(whole_head, '=') < open) || (cli_piece_find(whole_head, ';') < open) ||
        (cli_piece_find(whole_head, '"') < open))
    {
        return;
    }
    const CliPiece head = cli_piece_trim_end(whole_head);
    unsigned long long start = head.length;
    while ((start > 0ull) && (cli_piece_alnum(head.at[start - 1u]) || (head.at[start - 1u] == '_')))
    {
        start -= 1ull;
    }
    const CliPiece found = cli_piece_from(head, start);
    if ((found.length == 0ull) || (start == 0ull) || ((found.at[0] >= '0') && (found.at[0] <= '9')))
    {
        return;
    }
    const CliPiece rest_of = cli_piece_from(line, open);
    const CliPiece ending = cli_piece_trim_end(rest_of);
    const int body = (ending.length != 0ull) && (ending.at[ending.length - 1u] == '{');
    if (body || (cli_piece_find(rest_of, ';') >= rest_of.length))
    {
        cli_piece_put(out, found, "function");
    }
}

// The language of a file by its extension, in either case
static unsigned int cli_language_of(const char *path)
{
    static const char *const s_extensions[] = {"py",  "pyi",  "rs",   "js", "mjs",      "cjs", "jsx",  "ts",
                                               "tsx", "mts",  "cts",  "c",  "h",        "cc",  "cpp",  "cxx",
                                               "hpp", "hh",   "hxx",  "cu", "cuh",      "sh",  "bash", "zsh",
                                               "ps1", "psm1", "toml", "md", "markdown", "tex", "sty",  "cls"};
    static const unsigned int s_languages[] = {
        CLI_LANG_PYTHON,     CLI_LANG_PYTHON, CLI_LANG_RUST,     CLI_LANG_SCRIPT,   CLI_LANG_SCRIPT,
        CLI_LANG_SCRIPT,     CLI_LANG_SCRIPT, CLI_LANG_SCRIPT,   CLI_LANG_SCRIPT,   CLI_LANG_SCRIPT,
        CLI_LANG_SCRIPT,     CLI_LANG_C,      CLI_LANG_C,        CLI_LANG_C,        CLI_LANG_C,
        CLI_LANG_C,          CLI_LANG_C,      CLI_LANG_C,        CLI_LANG_C,        CLI_LANG_C,
        CLI_LANG_C,          CLI_LANG_SHELL,  CLI_LANG_SHELL,    CLI_LANG_SHELL,    CLI_LANG_POWERSHELL,
        CLI_LANG_POWERSHELL, CLI_LANG_TOML,   CLI_LANG_MARKDOWN, CLI_LANG_MARKDOWN, CLI_LANG_TEX,
        CLI_LANG_TEX,        CLI_LANG_TEX};
    const char *const dot =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '.');
    if ((dot == NULL) || (cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL}) >= 16u))
    {
        return CLI_LANG_NONE;
    }
    char extension[16];
    unsigned int length = 0u;
    for (const char *at = dot + 1; at[0] != '\0'; at += 1)
    {
        extension[length] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
        length += 1u;
    }
    extension[length] = '\0';
    for (unsigned int at = 0u; at < (sizeof(s_extensions) / sizeof(s_extensions[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = s_extensions[at], .cap = CLI_CELLS_POOL}))
        {
            return s_languages[at];
        }
    }
    return CLI_LANG_NONE;
}

int cli_symbols_reads(const char *path)
{
    return cli_language_of(path) != CLI_LANG_NONE;
}

void cli_symbols_declared(const char *path, const char *text, unsigned long long size, CliDeclared each, void *context)
{
    const unsigned int language = cli_language_of(path);
    int fenced = 0;
    unsigned long long at = 0ull;
    CliOut out = {each, context, 0u};
    while ((language != CLI_LANG_NONE) && (at < size))
    {
        unsigned long long end = at;
        while ((end < size) && (text[end] != '\n'))
        {
            end += 1ull;
        }
        // a line as Rust's lines() gives it, its `\r` before `\n` left off
        CliPiece line = {text + at, end - at};
        line.length -= ((line.length != 0ull) && (line.at[line.length - 1u] == '\r')) ? 1ull : 0ull;
        switch (language)
        {
        case CLI_LANG_PYTHON:
            cli_python(line, &out);
            break;
        case CLI_LANG_RUST:
            cli_rust(line, &out);
            break;
        case CLI_LANG_SCRIPT:
            cli_script(line, &out);
            break;
        case CLI_LANG_C:
            cli_c(line, &out);
            break;
        case CLI_LANG_SHELL:
            cli_symbols_shell(line, &out);
            break;
        case CLI_LANG_POWERSHELL:
            cli_symbols_powershell(line, &out);
            break;
        case CLI_LANG_TOML:
            cli_symbols_toml(line, &out);
            break;
        case CLI_LANG_MARKDOWN:
            if (cli_piece_starts(cli_piece_trim_start(line), "```"))
            {
                fenced = !fenced;
            }
            else if (!fenced)
            {
                cli_symbols_markdown(line, &out);
            }
            break;
        default:
            cli_symbols_tex(line, &out);
            break;
        }
        out.line += 1u;
        at = end + 1ull;
    }
}
