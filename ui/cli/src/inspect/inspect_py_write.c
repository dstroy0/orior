// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_write.c: a docstring drawn up for the Python function at or around a line, in Google's, NumPy's or
// reStructuredText's form or a summary line alone, from what the function says of itself: its parameters with their
// annotations, what it gives back, whether it yields, and what it raises
#include "../text/json_tree.h"
#include "inspect_py.h"

// the most parameters and raised names a function's docstring names
#define CLI_PY_PARAMS 256u

// What a function says of itself for its docstring: its parameters, each its name with its stars and its
// annotation where it has one, what it gives back where its header names it, whether it gives anything, whether it
// yields, and what it raises, each a part of the source
typedef struct
{
    char param[CLI_PY_PARAMS][CLI_NAME_LONGEST];
    unsigned long long annotation_start[CLI_PY_PARAMS];
    unsigned long long annotation_end[CLI_PY_PARAMS];
    int annotated[CLI_PY_PARAMS];
    unsigned int params;
    unsigned long long returns_start;
    unsigned long long returns_end;
    int has_returns;
    int gives;
    int yields;
    char raised[CLI_PY_PARAMS][CLI_NAME_LONGEST];
    unsigned int raises;
} PySignature;

static PySignature s_sign;

// the function whose header or block holds `line`, the innermost: its line's index, or g_cli_py_lines for none
static unsigned int write_function_around(const char *src, unsigned int line, const char *head)
{
    unsigned int found = g_cli_py_lines;
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const one = &g_cli_py_line[at];
        if (!cli_py_head_is(src, one, head) || !cli_py_opens(src, one) || (cli_py_tok(one, 0u)->from.line > line))
        {
            continue;
        }
        const CliPyLine *const last = &g_cli_py_line[cli_py_block_end(at) - 1u];
        if (line <= cli_py_tok(last, last->count - 1u)->to.line)
        {
            found = at;
        }
    }
    return found;
}

// one parameter of a header, its tokens from `first` to `end` among the line's, kept in the signature
static void write_parameter(const char *src, const CliPyLine *header, unsigned int first, unsigned int end, int index)
{
    char stars[8];
    CliLine starred = {stars, sizeof(stars), 0ull};
    unsigned int at = first;
    while ((at < end) && (cli_py_is(src, cli_py_tok(header, at), "*") || cli_py_is(src, cli_py_tok(header, at), "**")))
    {
        const CliInspectTok *const star = cli_py_tok(header, at);
        cli_line_counted(&starred, src + star->start, star->end - star->start);
        at += 1u;
    }
    unsigned int name_at = end;
    for (unsigned int part = first; (name_at == end) && (part < end); part += 1u)
    {
        name_at = (cli_py_tok(header, part)->kind == CLI_PY_NAME) ? part : name_at;
    }
    if ((name_at == end) || (s_sign.params == CLI_PY_PARAMS))
    {
        return;
    }
    const CliInspectTok *const name = cli_py_tok(header, name_at);
    if ((index == 0) && (starred.at == 0u) && (cli_py_is(src, name, "self") || cli_py_is(src, name, "cls")))
    {
        return;
    }
    CliLine param = {s_sign.param[s_sign.params], CLI_NAME_LONGEST, 0ull};
    cli_line_counted(&param, stars, starred.at);
    cli_line_counted(&param, src + name->start, name->end - name->start);
    cli_line_closed(&param);
    unsigned int colon = end;
    unsigned int equal = end;
    for (unsigned int part = first; part < end; part += 1u)
    {
        colon = ((colon == end) && cli_py_is(src, cli_py_tok(header, part), ":")) ? part : colon;
        equal = ((equal == end) && cli_py_is(src, cli_py_tok(header, part), "=")) ? part : equal;
    }
    const unsigned int stop = (equal < end) ? equal : end;
    s_sign.annotated[s_sign.params] = (colon < end) && (colon + 1u < stop);
    if (s_sign.annotated[s_sign.params])
    {
        s_sign.annotation_start[s_sign.params] = cli_py_tok(header, colon + 1u)->start;
        s_sign.annotation_end[s_sign.params] = cli_py_tok(header, stop - 1u)->end;
    }
    s_sign.params += 1u;
}

// the parameters and what a function's header says it gives back
static void write_header(const char *src, const CliPyLine *header)
{
    unsigned int open = 0u;
    while ((open < header->count) && !cli_py_is(src, cli_py_tok(header, open), "("))
    {
        open += 1u;
    }
    open = (open == header->count) ? 0u : open;
    int depth = 0;
    unsigned int close = open;
    unsigned int part_first = open + 1u;
    int index = 0;
    for (unsigned int offset = open; offset < header->count; offset += 1u)
    {
        const CliInspectTok *const tok = cli_py_tok(header, offset);
        if (cli_py_is(src, tok, "(") || cli_py_is(src, tok, "[") || cli_py_is(src, tok, "{"))
        {
            depth += 1;
            part_first = (depth == 1) ? (offset + 1u) : part_first;
            continue;
        }
        if (cli_py_is(src, tok, ")") || cli_py_is(src, tok, "]") || cli_py_is(src, tok, "}"))
        {
            depth -= 1;
            if (depth == 0)
            {
                close = offset;
                if (offset > part_first)
                {
                    write_parameter(src, header, part_first, offset, index);
                }
                break;
            }
            continue;
        }
        if ((depth == 1) && cli_py_is(src, tok, ","))
        {
            if (offset > part_first)
            {
                write_parameter(src, header, part_first, offset, index);
                index += 1;
            }
            part_first = offset + 1u;
        }
    }
    for (unsigned int at = close; at < header->count; at += 1u)
    {
        if (cli_py_is(src, cli_py_tok(header, at), "->"))
        {
            const unsigned int end = header->count - 1u;
            if (at + 1u < end)
            {
                s_sign.has_returns = 1;
                s_sign.returns_start = cli_py_tok(header, at + 1u)->start;
                s_sign.returns_end = cli_py_tok(header, end - 1u)->end;
            }
            break;
        }
    }
}

// what the function whose header is the line at `at` says of itself, into s_sign
static void write_signature(const char *src, unsigned int at)
{
    memor.set(&(MemoriaCfg){.dst = &s_sign, .val = 0, .bytes = sizeof(s_sign)});
    write_header(src, &g_cli_py_line[at]);
    const unsigned int end = cli_py_block_end(at);
    unsigned int nested_end = 0u;
    for (unsigned int index = at + 1u; index < end; index += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[index];
        if (index < nested_end)
        {
            continue;
        }
        if ((cli_py_head_is(src, line, "def") || cli_py_head_is(src, line, "class")) && cli_py_opens(src, line))
        {
            nested_end = cli_py_block_end(index);
            continue;
        }
        const CliInspectTok *const first = cli_py_tok(line, 0u);
        if (cli_py_is(src, first, "return") && (line->count > 1u) && !cli_py_is(src, cli_py_tok(line, 1u), "None"))
        {
            s_sign.gives = 1;
        }
        s_sign.yields = s_sign.yields || cli_py_holds(src, &g_cli_inspect_tok[line->first], line->count, "yield");
        if (cli_py_is(src, first, "raise") && (line->count > 1u) && (s_sign.raises < CLI_PY_PARAMS))
        {
            CliLine raised = {s_sign.raised[s_sign.raises], CLI_NAME_LONGEST, 0ull};
            for (unsigned int part = 1u; part < line->count; part += 1u)
            {
                const CliInspectTok *const word = cli_py_tok(line, part);
                if (cli_py_is(src, word, "(") || cli_py_is(src, word, "from"))
                {
                    break;
                }
                cli_line_counted(&raised, src + word->start, word->end - word->start);
            }
            int known = (cli_line_closed(&raised) == 0ull) || (raised.at == 0u);
            for (unsigned int one = 0u; !known && (one < s_sign.raises); one += 1u)
            {
                known = (cellul.eq(&(CatenaFinitaCfg){
                    .src = s_sign.raised[one], .other = s_sign.raised[s_sign.raises], .cap = CLI_CELLS_POOL}));
            }
            s_sign.raises += known ? 0u : 1u;
        }
    }
    const int none = s_sign.has_returns && (s_sign.returns_end - s_sign.returns_start == 4u) &&
                     (memor.cmp(&(MemoriaCfg){.src = src + s_sign.returns_start, .other = "None", .bytes = 4u}) == 0);
    s_sign.gives = s_sign.gives || (s_sign.has_returns && !none);
}

// the lines of the docstring being drawn up, each its start and length in their pool, and how many there are
static char s_pool[1u << 18u];
static CliLine s_pool_line;
static unsigned long long s_line_start[CLI_PY_PARAMS * 4u];
static unsigned long long s_line_size[CLI_PY_PARAMS * 4u];
static unsigned int s_lines;

// the docstring's next line begun in the pool
static CliLine *write_line(void)
{
    if (s_lines < CLI_PY_PARAMS * 4u)
    {
        s_line_start[s_lines] = s_pool_line.at;
        s_line_size[s_lines] = 0ull;
        s_lines += 1u;
    }
    return &s_pool_line;
}

// the docstring's last line ended where the pool stands
static void write_line_done(void)
{
    s_line_size[s_lines - 1u] = s_pool_line.at - s_line_start[s_lines - 1u];
}

// a line of `text` put on the docstring
static void write_text_line(const char *text)
{
    cli_line_text(write_line(), text);
    write_line_done();
}

// a section begun: a blank line and its title, underlined in NumPy's form and with a colon after it in Google's
static void write_section(const char *form, const char *title)
{
    write_text_line("");
    if (cellul.eq(&(CatenaFinitaCfg){.src = form, .other = "numpy", .cap = CLI_CELLS_POOL}))
    {
        write_text_line(title);
        CliLine *const line = write_line();
        for (size_t at = 0u; at < cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_CELLS_POOL}); at += 1u)
        {
            cli_line_character(line, '-');
        }
        write_line_done();
        return;
    }
    CliLine *const line = write_line();
    cli_line_text(line, title);
    cli_line_character(line, ':');
    write_line_done();
}

// a part of the source put on the docstring's line
static void write_part(const char *src, unsigned long long start, unsigned long long end)
{
    cli_line_counted(&s_pool_line, src + start, end - start);
}

// the fields of reStructuredText's form put on the docstring
static void write_rest(const char *src)
{
    if ((s_sign.params > 0u) || s_sign.gives || s_sign.yields || (s_sign.raises > 0u))
    {
        write_text_line("");
    }
    for (unsigned int at = 0u; at < s_sign.params; at += 1u)
    {
        cli_line_text(write_line(), ":param ");
        cli_line_text(&s_pool_line, s_sign.param[at]);
        cli_line_character(&s_pool_line, ':');
        write_line_done();
        if (s_sign.annotated[at])
        {
            cli_line_text(write_line(), ":type ");
            cli_line_text(&s_pool_line, s_sign.param[at]);
            cli_line_text(&s_pool_line, ": ");
            write_part(src, s_sign.annotation_start[at], s_sign.annotation_end[at]);
            write_line_done();
        }
    }
    if (s_sign.gives || s_sign.yields)
    {
        write_text_line(s_sign.yields ? ":yield:" : ":return:");
        if (s_sign.has_returns)
        {
            cli_line_text(write_line(), ":rtype: ");
            write_part(src, s_sign.returns_start, s_sign.returns_end);
            write_line_done();
        }
    }
    for (unsigned int at = 0u; at < s_sign.raises; at += 1u)
    {
        cli_line_text(write_line(), ":raises ");
        cli_line_text(&s_pool_line, s_sign.raised[at]);
        cli_line_character(&s_pool_line, ':');
        write_line_done();
    }
}

// the sections of Google's or NumPy's form put on the docstring
static void write_sections(const char *src, const char *form)
{
    const int numpy = (cellul.eq(&(CatenaFinitaCfg){.src = form, .other = "numpy", .cap = CLI_CELLS_POOL}));
    if (s_sign.params > 0u)
    {
        write_section(form, numpy ? "Parameters" : "Args");
        for (unsigned int at = 0u; at < s_sign.params; at += 1u)
        {
            CliLine *const line = write_line();
            cli_line_text(line, numpy ? "" : "    ");
            cli_line_text(line, s_sign.param[at]);
            if (s_sign.annotated[at])
            {
                cli_line_text(line, numpy ? " : " : " (");
                write_part(src, s_sign.annotation_start[at], s_sign.annotation_end[at]);
                cli_line_text(line, numpy ? "" : ")");
            }
            cli_line_text(line, numpy ? "" : ":");
            write_line_done();
        }
    }
    if (s_sign.gives || s_sign.yields)
    {
        write_section(form, s_sign.yields ? "Yields" : "Returns");
        CliLine *const line = write_line();
        if (s_sign.has_returns)
        {
            cli_line_text(line, numpy ? "" : "    ");
            write_part(src, s_sign.returns_start, s_sign.returns_end);
            cli_line_text(line, numpy ? "" : ":");
        }
        write_line_done();
        if (s_line_size[s_lines - 1u] == 0u)
        {
            s_lines -= 1u;
        }
    }
    if (s_sign.raises > 0u)
    {
        write_section(form, "Raises");
        for (unsigned int at = 0u; at < s_sign.raises; at += 1u)
        {
            CliLine *const line = write_line();
            cli_line_text(line, numpy ? "" : "    ");
            cli_line_text(line, s_sign.raised[at]);
            cli_line_text(line, numpy ? "" : ":");
            write_line_done();
        }
    }
}

unsigned int cli_py_around(const char *src, unsigned int line, const char *head)
{
    return write_function_around(src, line, head);
}

int cli_inspect_py_docstring(const char *src, unsigned long long size, unsigned int line, const char *form,
                             CliInspectEdit *edit, CliLine *text, CliInspectPlace *summary, CliLine *said)
{
    cli_py_read(src, size);
    const unsigned int at = write_function_around(src, line, "def");
    if (at == g_cli_py_lines)
    {
        cli_line_text(said, "Write Docstring writes one for the function the cursor is in.");
        return 0;
    }
    if (cli_py_block_end(at) <= at + 1u)
    {
        cli_line_text(said, "The function's body stands on its header's line.");
        return 0;
    }
    const CliPyLine *const first = &g_cli_py_line[at + 1u];
    int documented = (cli_py_tok(first, 0u)->kind == CLI_PY_STR);
    for (unsigned int part = 1u; documented && (part < first->count); part += 1u)
    {
        documented = cli_py_tok(first, part)->field;
    }
    if (documented)
    {
        cli_line_text(said, "The function has a docstring.");
        return 0;
    }
    write_signature(src, at);
    const unsigned int header_end = cli_py_tok(&g_cli_py_line[at], g_cli_py_line[at].count - 1u)->to.line;
    unsigned long long body = cli_py_tok(first, 0u)->start;
    while ((body > 0u) && (src[body - 1u] != '\n'))
    {
        body -= 1u;
    }
    unsigned long long indent = body;
    unsigned int units = 0u;
    while (indent < size)
    {
        unsigned int letter = 0u;
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)src, size, indent, &letter);
        if ((letter == '\n') || !cli_inspect_white(letter))
        {
            break;
        }
        indent += taken;
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    s_pool_line = (CliLine){s_pool, sizeof(s_pool), 0ull};
    s_lines = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = form, .other = "rest", .cap = CLI_CELLS_POOL}))
    {
        write_rest(src);
    }
    else if (!cellul.eq(&(CatenaFinitaCfg){.src = form, .other = "plain", .cap = CLI_CELLS_POOL}))
    {
        write_sections(src, form);
    }
    cli_line_counted(text, src + body, indent - body);
    cli_line_text(text, "\"\"\"");
    if (s_lines == 0u)
    {
        cli_line_text(text, "\"\"\"\n");
    }
    else
    {
        cli_line_character(text, '\n');
        for (unsigned int index = 0u; index < s_lines; index += 1u)
        {
            if (s_line_size[index] > 0u)
            {
                cli_line_counted(text, src + body, indent - body);
                cli_line_counted(text, s_pool + s_line_start[index], s_line_size[index]);
            }
            cli_line_character(text, '\n');
        }
        cli_line_counted(text, src + body, indent - body);
        cli_line_text(text, "\"\"\"\n");
    }
    const CliInspectPlace place = {header_end + 1u, 0u};
    *edit = (CliInspectEdit){place, place, {0u, 0u}};
    *summary = (CliInspectPlace){header_end + 1u, units + 3u};
    return 1;
}
