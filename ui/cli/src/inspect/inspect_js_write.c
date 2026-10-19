// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_write.c: a JSDoc comment drawn up for the JavaScript function at or around a line, its parameters, what
// it returns where it returns a value, and what it throws; and the class around a line with its methods sorted by
// name
#include "../text/json_tree.h"
#include "inspect_js.h"

// the most parameters and thrown names a JSDoc comment names, and the most members a class sorted holds
#define CLI_JS_NAMED 256u
#define CLI_JS_MEMBERS (CLI_INSPECT_TOKENS / 4u)

static CliInspectSet s_names;
static unsigned int s_params[CLI_JS_NAMED];
static const char *s_thrown[CLI_JS_NAMED];
static unsigned int s_thrown_size[CLI_JS_NAMED];
static unsigned long long *s_starts;
static unsigned long long *s_ends;
static unsigned int *s_firsts;
static unsigned int *s_lasts;
static const char **s_member_names;
static unsigned int *s_member_sizes;

void cli_inspect_js_write_laid(CliInspectCarve *carve)
{
    cli_inspect_set_laid(&s_names, carve);
    const unsigned long long lines = CLI_INSPECT_LINES;
    const unsigned long long members = CLI_JS_MEMBERS;
    s_starts = cli_inspect_carved(carve, lines * sizeof(unsigned long long));
    s_ends = cli_inspect_carved(carve, lines * sizeof(unsigned long long));
    s_firsts = cli_inspect_carved(carve, members * sizeof(unsigned int));
    s_lasts = cli_inspect_carved(carve, members * sizeof(unsigned int));
    s_member_names = cli_inspect_carved(carve, members * sizeof(const char *));
    s_member_sizes = cli_inspect_carved(carve, members * sizeof(unsigned int));
}

// the text of the token at `at`, `size` bytes of it
static const char *write_word(const char *src, unsigned int at, unsigned int *size)
{
    *size = g_cli_inspect_tok[at].end - g_cli_inspect_tok[at].start;
    return src + g_cli_inspect_tok[at].start;
}

// the white letters a physical line starts with, its start and their end, into `start` and `end`
static void write_indent(const char *src, unsigned long long size, unsigned int line, unsigned long long *start,
                         unsigned long long *end)
{
    unsigned long long at = 0ull;
    for (unsigned int index = 0u; (index < line) && (at < size); index += 1u)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(size - at)});
        at = (newline != NULL) ? ((unsigned long long)(newline - src) + 1u) : size;
    }
    *start = at;
    while (at < size)
    {
        unsigned int letter = 0u;
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)src, size, at, &letter);
        if ((letter == '\n') || !cli_inspect_white(letter))
        {
            break;
        }
        at += taken;
    }
    *end = at;
}

// the function at or around `line`: its body's opening brace, CLI_JS_NO where none is
static unsigned int write_function_at(const char *src, unsigned int line)
{
    unsigned int found = CLI_JS_NO;
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        if ((g_cli_js_brace[at] != CLI_JS_FUNCTION) || (cli_js_function_name(src, at) == CLI_JS_NO))
        {
            continue;
        }
        const unsigned int close = g_cli_js_pair[at];
        const int holds = (g_cli_inspect_tok[at].from.line <= line) && (close != CLI_JS_NO) &&
                          (g_cli_inspect_tok[close].to.line >= line);
        const int named_on = (g_cli_inspect_tok[cli_js_function_name(src, at)].from.line == line);
        found = (holds || named_on) ? at : found;
    }
    return found;
}

int cli_inspect_js_docstring(const char *src, unsigned long long size, unsigned int line, CliInspectEdit *edit,
                             CliLine *text, CliInspectPlace *summary, CliLine *said)
{
    cli_js_read(src, size);
    const unsigned int open = write_function_at(src, line);
    if (open == CLI_JS_NO)
    {
        cli_line_text(said, "Write Docstring writes one for the function the cursor is in.");
        return 0;
    }
    const unsigned int close = (g_cli_js_pair[open] != CLI_JS_NO) ? g_cli_js_pair[open] : open;
    cli_inspect_set_cleared(&s_names);
    const unsigned int first = cli_js_parameters(src, open, &s_names);
    const unsigned int named = cli_js_function_name(src, open);
    const unsigned int name_at = (named != CLI_JS_NO) ? named : first;
    const unsigned int start = (name_at < first) ? name_at : first;
    unsigned int lead = start;
    while ((lead > 0u) && !cli_js_is(src, lead - 1u, ";") && !cli_js_is(src, lead - 1u, "{") &&
           !cli_js_is(src, lead - 1u, "}") &&
           (g_cli_inspect_tok[lead - 1u].from.line == g_cli_inspect_tok[start].from.line))
    {
        lead -= 1u;
    }
    const unsigned int first_line = g_cli_inspect_tok[lead].from.line;
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        const unsigned int to_line = g_cli_inspect_comment[at].to.line;
        unsigned long long line_start = 0ull;
        unsigned long long line_end = 0ull;
        write_indent(src, size, to_line, &line_start, &line_end);
        unsigned long long end = line_start;
        while ((end < size) && (src[end] != '\n'))
        {
            end += 1u;
        }
        while ((end > line_start) && (cellul.chr(&(CatenaFinitaCfg){
                                          .src = " \t\r\v\f", .cap = CLI_CELLS_POOL, .byte = src[end - 1u]}) != NULL))
        {
            end -= 1u;
        }
        const int closed = (end - line_start >= 2u) &&
                           (memor.cmp(&(MemoriaCfg){.src = src + end - 2u, .other = "*/", .bytes = 2u}) == 0);
        if ((to_line + 1u == first_line) && closed)
        {
            cli_line_text(said, "The function has a comment over it.");
            return 0;
        }
    }
    unsigned int params = 0u;
    for (unsigned int at = first; (at < open) && (params < CLI_JS_NAMED); at += 1u)
    {
        unsigned int length = 0u;
        const char *const word = write_word(src, at, &length);
        int known = !cli_inspect_set_has(&s_names, word, length) || (g_cli_inspect_tok[at].kind != CLI_JS_NAME);
        for (unsigned int index = 0u; !known && (index < params); index += 1u)
        {
            unsigned int other = 0u;
            const char *const before = write_word(src, s_params[index], &other);
            known = (other == length) && (memor.cmp(&(MemoriaCfg){.src = before, .other = word, .bytes = length}) == 0);
        }
        if (!known)
        {
            s_params[params] = at;
            params += 1u;
        }
    }
    int gives = 0;
    unsigned int thrown = 0u;
    unsigned int at = open + 1u;
    while (at < close)
    {
        if (g_cli_js_brace[at] == CLI_JS_FUNCTION)
        {
            at = (g_cli_js_pair[at] != CLI_JS_NO) ? (g_cli_js_pair[at] + 1u) : (at + 1u);
            continue;
        }
        gives =
            gives || (cli_js_is(src, at, "return") && !cli_js_is(src, at + 1u, ";") && !cli_js_is(src, at + 1u, "}"));
        if (cli_js_is(src, at, "throw") && (thrown < CLI_JS_NAMED))
        {
            const int made = cli_js_is(src, at + 1u, "new") && (at + 2u < g_cli_inspect_toks);
            unsigned int length = 5u;
            const char *const name = made ? write_word(src, at + 2u, &length) : "Error";
            int known = 0;
            for (unsigned int index = 0u; !known && (index < thrown); index += 1u)
            {
                known = (s_thrown_size[index] == length) &&
                        (memor.cmp(&(MemoriaCfg){.src = s_thrown[index], .other = name, .bytes = length}) == 0);
            }
            if (!known)
            {
                s_thrown[thrown] = name;
                s_thrown_size[thrown] = length;
                thrown += 1u;
            }
        }
        at += 1u;
    }
    unsigned long long indent_start = 0ull;
    unsigned long long indent_end = 0ull;
    write_indent(src, size, first_line, &indent_start, &indent_end);
    const char *const indent = src + indent_start;
    const unsigned long long indent_size = indent_end - indent_start;
    cli_line_counted(text, indent, indent_size);
    cli_line_text(text, "/**\n");
    cli_line_counted(text, indent, indent_size);
    cli_line_text(text, " * \n");
    if ((params > 0u) || gives || (thrown > 0u))
    {
        cli_line_counted(text, indent, indent_size);
        cli_line_text(text, " *\n");
    }
    for (unsigned int index = 0u; index < params; index += 1u)
    {
        unsigned int length = 0u;
        const char *const word = write_word(src, s_params[index], &length);
        cli_line_counted(text, indent, indent_size);
        cli_line_text(text, " * @param ");
        cli_line_counted(text, word, length);
        cli_line_character(text, '\n');
    }
    if (gives)
    {
        cli_line_counted(text, indent, indent_size);
        cli_line_text(text, " * @returns\n");
    }
    for (unsigned int index = 0u; index < thrown; index += 1u)
    {
        cli_line_counted(text, indent, indent_size);
        cli_line_text(text, " * @throws {");
        cli_line_counted(text, s_thrown[index], s_thrown_size[index]);
        cli_line_text(text, "}\n");
    }
    cli_line_counted(text, indent, indent_size);
    cli_line_text(text, " */\n");
    unsigned int units = 0u;
    for (unsigned long long letter_at = indent_start; letter_at < indent_end;)
    {
        unsigned int letter = 0u;
        letter_at += cli_lossy_letter((const unsigned char *)src, indent_end, letter_at, &letter);
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    const CliInspectPlace place = {first_line, 0u};
    *edit = (CliInspectEdit){place, place, {0u, 0u}};
    *summary = (CliInspectPlace){first_line + 1u, units + 3u};
    return 1;
}

// the end of the class member that starts at `start` before `close`, the token it ends on, and its name where it is a
// method, into `name` and `name_size`
static unsigned int write_member_end(const char *src, unsigned int start, unsigned int close, const char **name,
                                     unsigned int *name_size)
{
    static const char *const s_runs_on[4] = {"(", "{", "=", "."};
    unsigned int end = start;
    *name = NULL;
    *name_size = 0u;
    while (end < close)
    {
        if (g_cli_js_brace[end] == CLI_JS_FUNCTION)
        {
            const unsigned int named = cli_js_function_name(src, end);
            if (named != CLI_JS_NO)
            {
                *name = write_word(src, named, name_size);
            }
            end = (g_cli_js_pair[end] != CLI_JS_NO) ? g_cli_js_pair[end] : end;
            break;
        }
        if (cli_js_is(src, end, ";"))
        {
            break;
        }
        const unsigned int pair = g_cli_js_pair[end];
        const int opens = cli_js_is(src, end, "(") || cli_js_is(src, end, "[");
        end = ((pair != CLI_JS_NO) && (pair > end) && opens) ? (pair + 1u) : (end + 1u);
        end = (end < close) ? end : close;
        const int broken = (end < close) && (g_cli_inspect_tok[end].from.line > g_cli_inspect_tok[end - 1u].to.line) &&
                           !cli_js_is(src, end - 1u, ",") && !cli_js_one_of(src, end, s_runs_on, 4u) &&
                           (g_cli_js_brace[end] != CLI_JS_FUNCTION);
        if (broken)
        {
            end -= 1u;
            break;
        }
    }
    return (end < close - 1u) ? end : (close - 1u);
}

int cli_inspect_js_sort_methods(const char *src, unsigned long long size, unsigned int line, CliInspectEdit *edit,
                                CliLine *text, CliLine *said)
{
    cli_js_read(src, size);
    unsigned int open = CLI_JS_NO;
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        const unsigned int close = g_cli_js_pair[at];
        const int around = (g_cli_js_brace[at] == CLI_JS_CLASS) && (g_cli_inspect_tok[at].from.line <= line) &&
                           (close != CLI_JS_NO) && (g_cli_inspect_tok[close].to.line >= line);
        open = around ? at : open;
    }
    if (open == CLI_JS_NO)
    {
        cli_line_text(said, "Sort Methods by Name sorts the class the cursor is in.");
        return 0;
    }
    const unsigned int close = g_cli_js_pair[open];
    const unsigned int physical = cli_inspect_physical(src, size, s_starts, s_ends, CLI_INSPECT_LINES);
    unsigned int members = 0u;
    unsigned int at = open + 1u;
    unsigned int floor = g_cli_inspect_tok[open].to.line + 1u;
    while ((at < close) && (members < CLI_JS_MEMBERS))
    {
        if (cli_js_is(src, at, ";"))
        {
            at += 1u;
            continue;
        }
        const unsigned int start = at;
        const unsigned int end =
            write_member_end(src, start, close, &s_member_names[members], &s_member_sizes[members]);
        unsigned int first = g_cli_inspect_tok[start].from.line;
        for (int moved = 1; moved && (first > floor);)
        {
            moved = 0;
            unsigned int least = first - 1u;
            int any = 0;
            for (unsigned int comment = 0u; comment < g_cli_inspect_comments; comment += 1u)
            {
                const CliInspectSpan span = g_cli_inspect_comment[comment];
                if (span.to.line != first - 1u)
                {
                    continue;
                }
                moved = moved || (span.from.line >= floor);
                least = (!any || (span.from.line < least)) ? span.from.line : least;
                any = 1;
            }
            first = moved ? least : first;
        }
        s_firsts[members] = first;
        s_lasts[members] = g_cli_inspect_tok[end].to.line;
        floor = s_lasts[members] + 1u;
        members += 1u;
        at = end + 1u;
    }
    if (members == 0u)
    {
        cli_line_text(said, "The class has no members.");
        return 0;
    }
    const unsigned int first = s_firsts[0];
    const unsigned int last = s_lasts[members - 1u];
    if ((last >= physical) || !cli_inspect_sorted_body(src, size, s_starts, s_ends, s_firsts, s_lasts, s_member_names,
                                                       s_member_sizes, members, text))
    {
        cli_line_text(said, "The class's methods stand in the order of their names.");
        return 0;
    }
    unsigned int end_col = 0u;
    for (unsigned long long letter_at = s_starts[last]; letter_at < s_ends[last];)
    {
        unsigned int letter = 0u;
        letter_at += cli_lossy_letter((const unsigned char *)src, s_ends[last], letter_at, &letter);
        end_col += (letter >= 0x10000u) ? 2u : 1u;
    }
    *edit = (CliInspectEdit){{first, 0u}, {last, end_col}, {0u, 0u}};
    return 1;
}
