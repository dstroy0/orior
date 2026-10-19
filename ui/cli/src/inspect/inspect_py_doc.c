// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_doc.c: code after a return, a raise, a break or a continue in the same block, which no path reaches; and
// the docstrings out of PEP 257's form: three double quotes, a one-line docstring on one line with no space inside its
// quotes, a summary line that ends in a period, a blank line after the summary line where more follows, and closing
// quotes on a line of their own after more than one line
#include "../text/json_tree.h"
#include "inspect_py.h"

// the page of PEP 257, which a docstring's finding links to
static const char s_pep_257[] = "https://peps.python.org/pep-0257/";

// A part of a text: where it starts and where it ends
typedef struct
{
    unsigned long long start;
    unsigned long long end;
} DocPart;

// a part trimmed of white letters at its two ends, as Rust's str::trim trims it
static DocPart doc_trimmed(const char *src, DocPart part)
{
    while (part.start < part.end)
    {
        unsigned int letter = 0u;
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)src, part.end, part.start, &letter);
        if (!cli_inspect_white(letter))
        {
            break;
        }
        part.start += taken;
    }
    unsigned long long end = part.start;
    for (unsigned long long at = part.start; at < part.end;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)src, part.end, at, &letter);
        end = cli_inspect_white(letter) ? end : at;
    }
    part.end = end;
    return part;
}

// the line of a part at `index`, as Rust's str::lines splits it: 1 with it into `line`, or 0 where it has none
static int doc_line(const char *src, DocPart part, unsigned int index, DocPart *line)
{
    unsigned long long at = part.start;
    for (unsigned int count = 0u; at < part.end; count += 1u)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(part.end - at)});
        unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - src) : part.end;
        const unsigned long long next = (newline != NULL) ? (end + 1u) : part.end;
        end -= ((newline != NULL) && (end > at) && (src[end - 1u] == '\r')) ? 1u : 0u;
        if (count == index)
        {
            *line = (DocPart){at, end};
            return 1;
        }
        at = next;
    }
    return 0;
}

void cli_py_unreachable(const char *src, unsigned long long size, CliFacts *facts)
{
    unsigned int at = 0u;
    while (at < g_cli_py_lines)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        const CliInspectTok *const word = cli_py_tok(line, 0u);
        const int leaves = cli_py_is(src, word, "return") || cli_py_is(src, word, "raise") ||
                           cli_py_is(src, word, "break") || cli_py_is(src, word, "continue");
        if (!leaves || cli_py_holds(src, &g_cli_inspect_tok[line->first], line->count, ";"))
        {
            at += 1u;
            continue;
        }
        unsigned int end = at + 1u;
        while ((end < g_cli_py_lines) && (g_cli_py_line[end].indent >= line->indent))
        {
            end += 1u;
        }
        if (end > at + 1u)
        {
            const CliPyLine *const first = &g_cli_py_line[at + 1u];
            const CliPyLine *const last_line = &g_cli_py_line[end - 1u];
            const CliInspectPlace last = cli_py_tok(last_line, last_line->count - 1u)->to;
            CliInspectPlace from = {0u, 0u};
            CliInspectPlace to = {0u, 0u};
            cli_py_whole_lines(src, size, cli_py_tok(first, 0u)->from.line, last.line, &from, &to);
            char message[CLI_INSPECT_UNREACHED_ROOM];
            CliLine said = {message, sizeof(message), 0ull};
            cli_line_text(&said, CLI_INSPECT_UNREACHED_BEFORE);
            cli_line_counted(&said, src + word->start, word->end - word->start);
            cli_line_text(&said, CLI_INSPECT_UNREACHED_AFTER);
            cli_inspect_finding_put(facts, cli_py_tok(first, 0u)->from, last, CLI_INSPECT_WARNING, &said, "unreachable",
                                    NULL);
            cli_inspect_fix_titled(facts, "Take out the code no path reaches");
            cli_inspect_edit_put(facts, from, to, "", 0ull);
        }
        at = end;
    }
}

// a docstring's note put on the facts: its message after `PEP 257: `, its code, and the link to PEP 257
static void doc_note(CliFacts *facts, const CliInspectTok *tok, const char *message, const char *code)
{
    char text[256];
    CliLine said = {text, sizeof(text), 0ull};
    cli_line_text(&said, "PEP 257: ");
    cli_line_text(&said, message);
    cli_inspect_finding_put(facts, tok->from, tok->to, CLI_INSPECT_NOTE, &said, code, s_pep_257);
}

// a fix put on the last note that writes the docstring whole again as `text` says
static void doc_whole_fix(CliFacts *facts, const CliInspectTok *tok, const char *title, const CliLine *text)
{
    cli_inspect_fix_titled(facts, title);
    cli_inspect_edit_put(facts, tok->from, tok->to, text->out, cli_line_fits(text) ? text->at : 0ull);
}

// the UTF-16 units of `size` bytes of `text`
static unsigned int doc_units(const char *text, unsigned long long size)
{
    unsigned int units = 0u;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)text, size, at, &letter);
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    return units;
}

// the physical line `index` of `size` bytes of `src`, empty where it has none
static DocPart doc_physical(const char *src, unsigned long long size, unsigned int index)
{
    unsigned long long at = 0ull;
    for (unsigned int line = 0u; line < index; line += 1u)
    {
        const char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(size - at)}) : NULL;
        if (newline == NULL)
        {
            return (DocPart){size, size};
        }
        at = (unsigned long long)(newline - src) + 1u;
    }
    const char *const newline =
        (at < size) ? memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(size - at)}) : NULL;
    return (DocPart){at, (newline != NULL) ? (unsigned long long)(newline - src) : size};
}

// the summary line of `trimmed`, the first of `inner`, without a period at its end, given its fix where its line is
// found
static void doc_period(const char *src, unsigned long long size, const CliInspectTok *tok, DocPart inner,
                       DocPart summary, CliFacts *facts)
{
    unsigned int first_line = 0u;
    DocPart line = {0ull, 0ull};
    for (unsigned int index = 0u; doc_line(src, inner, index, &line); index += 1u)
    {
        const DocPart trimmed = doc_trimmed(src, line);
        if (trimmed.start < trimmed.end)
        {
            first_line = index;
            break;
        }
    }
    doc_note(facts, tok, "a docstring's summary line ends in a period.", "docstring-period");
    const DocPart text = doc_physical(src, size, tok->from.line + first_line);
    const unsigned long long length = summary.end - summary.start;
    for (unsigned long long at = text.start; at + length <= text.end; at += 1u)
    {
        if (memor.cmp(&(MemoriaCfg){.src = src + at, .other = src + summary.start, .bytes = (size_t)length}) == 0)
        {
            const CliInspectPlace place = {tok->from.line + first_line,
                                           doc_units(src + text.start, at + length - text.start)};
            cli_inspect_fix_titled(facts, "End the summary line with a period");
            cli_inspect_edit_put(facts, place, place, ".", 1ull);
            return;
        }
    }
}

// what of PEP 257's form one docstring leaves out, put on the facts
static void doc_checked(const char *src, unsigned long long size, const CliInspectTok *tok, CliFacts *facts)
{
    const char *const whole = src + tok->start;
    const unsigned long long length = tok->end - tok->start;
    unsigned long long prefix = 0ull;
    while ((prefix < length) && (whole[prefix] != '"') && (whole[prefix] != '\''))
    {
        prefix += 1u;
    }
    prefix = (prefix == length) ? 0ull : prefix;
    for (unsigned long long at = 0ull; at < prefix; at += 1u)
    {
        if (cellul.chr(&(CatenaFinitaCfg){.src = "fFbBtT", .cap = CLI_CELLS_POOL, .byte = whole[at]}) != NULL)
        {
            return;
        }
    }
    const char *const body = whole + prefix;
    const unsigned long long body_size = length - prefix;
    const int triple =
        (body_size >= 3u) && ((memor.cmp(&(MemoriaCfg){.src = body, .other = "\"\"\"", .bytes = 3u}) == 0) ||
                              (memor.cmp(&(MemoriaCfg){.src = body, .other = "'''", .bytes = 3u}) == 0));
    const unsigned long long quote = triple ? 3u : 1u;
    if (body_size < quote * 2u)
    {
        return;
    }
    const DocPart inner = {tok->start + prefix + quote, tok->start + length - quote};
    static char s_fixed[1u << 20u];
    CliLine fixed = {s_fixed, sizeof(s_fixed), 0ull};
    if (!triple || (memor.cmp(&(MemoriaCfg){.src = body, .other = "\"\"\"", .bytes = 3u}) != 0))
    {
        doc_note(facts, tok, "a docstring is written in three double quotes.", "docstring-quotes");
        int inside = (inner.end > inner.start) && (src[inner.end - 1u] == '"');
        for (unsigned long long at = inner.start; !inside && (at + 3u <= inner.end); at += 1u)
        {
            inside = (memor.cmp(&(MemoriaCfg){.src = src + at, .other = "\"\"\"", .bytes = 3u}) == 0);
        }
        if (!inside)
        {
            cli_line_counted(&fixed, whole, prefix);
            cli_line_text(&fixed, "\"\"\"");
            cli_line_counted(&fixed, src + inner.start, inner.end - inner.start);
            cli_line_text(&fixed, "\"\"\"");
            doc_whole_fix(facts, tok, "Use three double quotes", &fixed);
        }
    }
    const DocPart trimmed = doc_trimmed(src, inner);
    if (trimmed.start == trimmed.end)
    {
        return;
    }
    DocPart first = {0ull, 0ull};
    DocPart second = {0ull, 0ull};
    (void)doc_line(src, trimmed, 0u, &first);
    const int more = doc_line(src, trimmed, 1u, &second);
    if (!more)
    {
        const int broken =
            (memor.chr(&(MemoriaCfg){
                 .src = src + inner.start, .val = '\n', .bytes = (size_t)(inner.end - inner.start)}) != NULL);
        const int spaced = (inner.start != trimmed.start) || (inner.end != trimmed.end);
        fixed.at = 0ull;
        cli_line_counted(&fixed, whole, prefix);
        cli_line_counted(&fixed, broken ? "\"\"\"" : body, broken ? 3u : quote);
        cli_line_counted(&fixed, src + trimmed.start, trimmed.end - trimmed.start);
        cli_line_counted(&fixed, broken ? "\"\"\"" : body, broken ? 3u : quote);
        if (broken)
        {
            doc_note(facts, tok, "a one-line docstring fits on one line with its quotes.", "docstring-one-line");
            doc_whole_fix(facts, tok, "Put the docstring on one line", &fixed);
        }
        else if (spaced)
        {
            doc_note(facts, tok, "a one-line docstring has no space inside its quotes.", "docstring-space");
            doc_whole_fix(facts, tok, "Take out the space inside the quotes", &fixed);
        }
    }
    DocPart summary = doc_trimmed(src, first);
    summary.start = first.start;
    if ((summary.end == summary.start) || (src[summary.end - 1u] != '.'))
    {
        doc_period(src, size, tok, inner, summary, facts);
    }
    if (!more)
    {
        return;
    }
    const DocPart blank = doc_trimmed(src, second);
    if (blank.start < blank.end)
    {
        doc_note(facts, tok, "a blank line comes between a docstring's summary line and what follows it.",
                 "docstring-blank");
    }
    unsigned long long last = inner.end;
    while ((last > inner.start) && (src[last - 1u] != '\n'))
    {
        last -= 1u;
    }
    const DocPart closing = doc_trimmed(src, (DocPart){last, inner.end});
    if (closing.start < closing.end)
    {
        const DocPart line = doc_physical(src, size, tok->from.line);
        unsigned long long indent = line.start;
        while (indent < line.end)
        {
            unsigned int letter = 0u;
            const unsigned long long taken = cli_lossy_letter((const unsigned char *)src, line.end, indent, &letter);
            if (!cli_inspect_white(letter))
            {
                break;
            }
            indent += taken;
        }
        doc_note(facts, tok, "the closing quotes of a docstring of more than one line stand on a line of their own.",
                 "docstring-close");
        const CliInspectPlace close = {tok->to.line, tok->to.col - (unsigned int)quote};
        fixed.at = 0ull;
        cli_line_character(&fixed, '\n');
        cli_line_counted(&fixed, src + line.start, indent - line.start);
        cli_inspect_fix_titled(facts, "Put the closing quotes on a line of their own");
        cli_inspect_edit_put(facts, close, close, fixed.out, fixed.at);
    }
}

// whether a line is a docstring alone: one string, or one string with the names its fields read
static int doc_lone(const CliPyLine *line)
{
    const CliInspectTok *const toks = &g_cli_inspect_tok[line->first];
    if (toks[0].kind != CLI_PY_STR)
    {
        return 0;
    }
    unsigned int strings = 0u;
    for (unsigned int at = 0u; at < line->count; at += 1u)
    {
        if ((toks[at].kind != CLI_PY_STR) && !toks[at].field)
        {
            return 0;
        }
        strings += (toks[at].kind == CLI_PY_STR) ? 1u : 0u;
    }
    return strings == 1u;
}

void cli_py_docstrings(const char *src, unsigned long long size, CliFacts *facts)
{
    if ((g_cli_py_lines > 0u) && doc_lone(&g_cli_py_line[0]))
    {
        doc_checked(src, size, cli_py_tok(&g_cli_py_line[0], 0u), facts);
    }
    for (unsigned int at = 0u; at + 1u < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        const CliPyLine *const next = &g_cli_py_line[at + 1u];
        const int declares = cli_py_head_is(src, line, "def") || cli_py_head_is(src, line, "class");
        if (declares && cli_py_opens(src, line) && (next->indent > line->indent) && doc_lone(next))
        {
            doc_checked(src, size, cli_py_tok(next, 0u), facts);
        }
    }
}
