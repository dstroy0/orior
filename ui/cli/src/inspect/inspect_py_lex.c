// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_lex.c: Python's text read as logical lines of tokens: a line runs on inside brackets and past a backslash
// at its end, its indent the width of its first physical line's, a tab reaching the next multiple of eight; a string,
// with its prefix, is one token, and the names an f-string's or a t-string's fields read are tokens after it
#include "inspect_py.h"

// the most names one string's fields read that are kept
#define CLI_PY_FIELD_NAMES 4096u

CliPyLine *g_cli_py_line;
unsigned int g_cli_py_lines;

void cli_inspect_py_lex_laid(CliInspectCarve *carve)
{
    g_cli_py_line = cli_inspect_carved(carve, (unsigned long long)CLI_INSPECT_LINES * sizeof(CliPyLine));
}

const char *const g_cli_py_keywords[35] = {"False",  "None",   "True",    "and",      "as",       "assert", "async",
                                           "await",  "break",  "class",   "continue", "def",      "del",    "elif",
                                           "else",   "except", "finally", "for",      "from",     "global", "if",
                                           "import", "in",     "is",      "lambda",   "nonlocal", "not",    "or",
                                           "pass",   "raise",  "return",  "try",      "while",    "with",   "yield"};

// the operators, the longest that matches first
static const char *const s_ops[47] = {"**=", "//=", ">>=", "<<=", "...", "->", ":=", "==", "!=", "<=", ">=", "+=",
                                      "-=",  "*=",  "/=",  "%=",  "&=",  "|=", "^=", "@=", "**", "//", "<<", ">>",
                                      "+",   "-",   "*",   "/",   "%",   "@",  "&",  "|",  "^",  "~",  "<",  ">",
                                      "(",   ")",   "[",   "]",   "{",   "}",  ",",  ":",  ".",  ";",  "="};

// the prefixes a string may have, in lower case
static const char *const s_prefixes[13] = {"r", "u", "b", "f", "t", "br", "rb", "fr", "rf", "tr", "rt", "bt", "tb"};

// the names a string's fields read, kept until the string's own token is put
static CliInspectTok s_field_names[CLI_PY_FIELD_NAMES];

static int lex_name_start(unsigned int letter)
{
    return (letter == '_') || cli_inspect_alphabetic(letter);
}

static int lex_name(unsigned int letter)
{
    return (letter == '_') || cli_inspect_alphanumeric(letter);
}

// whether the bytes from `start` to `end` of `src` are a prefix a string may have, case aside
static int lex_prefix(const char *src, unsigned long long start, unsigned long long end)
{
    char lower[4];
    if (end - start > 2u)
    {
        return 0;
    }
    for (unsigned long long at = start; at < end; at += 1u)
    {
        lower[at - start] = ((src[at] >= 'A') && (src[at] <= 'Z')) ? (char)(src[at] - 'A' + 'a') : src[at];
    }
    lower[end - start] = '\0';
    for (unsigned int at = 0u; at < 13u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = s_prefixes[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// a string from its prefix at `start`, `prefix_size` bytes before the quote, to its closing quote, and the names its
// fields read where it is an f-string or a t-string
static void lex_string(CliInspectScan *scan, unsigned long long start, CliInspectPlace from,
                       unsigned long long prefix_size)
{
    const unsigned int peeked = cli_inspect_peek(scan);
    const unsigned int quote = (peeked == CLI_INSPECT_END) ? '"' : peeked;
    const int triple = (cli_inspect_peek_at(scan, 1u) == quote) && (cli_inspect_peek_at(scan, 2u) == quote);
    int fields = 0;
    for (unsigned long long at = start; at < start + prefix_size; at += 1u)
    {
        fields = fields || (scan->src[at] == 'f') || (scan->src[at] == 'F') || (scan->src[at] == 't') ||
                 (scan->src[at] == 'T');
    }
    for (unsigned int at = 0u; at < (triple ? 3u : 1u); at += 1u)
    {
        (void)cli_inspect_bump(scan);
    }
    unsigned int names = 0u;
    unsigned int braces = 0u;
    for (unsigned int letter = cli_inspect_peek(scan); letter != CLI_INSPECT_END; letter = cli_inspect_peek(scan))
    {
        if (letter == '\\')
        {
            (void)cli_inspect_bump(scan);
            (void)cli_inspect_bump(scan);
            continue;
        }
        if ((letter == quote) &&
            (!triple || ((cli_inspect_peek_at(scan, 1u) == quote) && (cli_inspect_peek_at(scan, 2u) == quote))))
        {
            for (unsigned int at = 0u; at < (triple ? 3u : 1u); at += 1u)
            {
                (void)cli_inspect_bump(scan);
            }
            break;
        }
        if ((letter == '\n') && !triple)
        {
            break;
        }
        if (fields && (letter == '{'))
        {
            if ((braces == 0u) && (cli_inspect_peek_at(scan, 1u) == '{'))
            {
                (void)cli_inspect_bump(scan);
                (void)cli_inspect_bump(scan);
                continue;
            }
            braces += 1u;
        }
        else if (fields && (letter == '}') && (braces > 0u))
        {
            braces -= 1u;
        }
        else if ((braces > 0u) && lex_name_start(letter))
        {
            const int after_dot = (scan->pos > 0u) && (scan->src[scan->pos - 1u] == '.');
            const int after_digit =
                (scan->pos > 0u) && (scan->src[scan->pos - 1u] >= '0') && (scan->src[scan->pos - 1u] <= '9');
            const unsigned long long name_start = scan->pos;
            const CliInspectPlace name_from = cli_inspect_place(scan);
            while (lex_name(cli_inspect_peek(scan)))
            {
                (void)cli_inspect_bump(scan);
            }
            if (!after_dot && !after_digit && (names < CLI_PY_FIELD_NAMES))
            {
                // a text read is at most 4 GiB, which a token's bytes count
                s_field_names[names] = (CliInspectTok){CLI_PY_NAME,
                                                       1u,
                                                       (unsigned int)name_start,
                                                       (unsigned int)scan->pos,
                                                       name_from,
                                                       cli_inspect_place(scan)};
                names += 1u;
            }
            continue;
        }
        (void)cli_inspect_bump(scan);
    }
    cli_inspect_tok_put(scan, CLI_PY_STR, start, from, 0);
    for (unsigned int at = 0u; at < names; at += 1u)
    {
        if (g_cli_inspect_toks == CLI_INSPECT_TOKENS)
        {
            g_cli_inspect_over = 1;
            break;
        }
        g_cli_inspect_tok[g_cli_inspect_toks] = s_field_names[at];
        g_cli_inspect_toks += 1u;
    }
}

// the logical line of the tokens from `first` put on the lines where it holds any
static void lex_line_put(unsigned int indent, unsigned int *first)
{
    if (g_cli_inspect_toks > *first)
    {
        if (g_cli_py_lines == CLI_INSPECT_LINES)
        {
            g_cli_inspect_over = 1;
        }
        else
        {
            g_cli_py_line[g_cli_py_lines] = (CliPyLine){indent, *first, g_cli_inspect_toks - *first};
            g_cli_py_lines += 1u;
        }
    }
    *first = g_cli_inspect_toks;
}

// a number from where the scan stands
static void lex_number(CliInspectScan *scan)
{
    const unsigned long long start = scan->pos;
    const CliInspectPlace from = cli_inspect_place(scan);
    for (unsigned int letter = cli_inspect_peek(scan);
         (letter != CLI_INSPECT_END) && (cli_inspect_alphanumeric(letter) || (letter == '_') || (letter == '.'));
         letter = cli_inspect_peek(scan))
    {
        const int hex = (scan->pos - start >= 2u) && (scan->src[start] == '0') &&
                        ((scan->src[start + 1u] == 'x') || (scan->src[start + 1u] == 'X'));
        const int exponent = ((letter == 'e') || (letter == 'E')) && !hex;
        (void)cli_inspect_bump(scan);
        if (exponent && ((cli_inspect_peek(scan) == '+') || (cli_inspect_peek(scan) == '-')))
        {
            (void)cli_inspect_bump(scan);
        }
    }
    cli_inspect_tok_put(scan, CLI_PY_NUMBER, start, from, 0);
}

// an operator, the longest that matches, else the one letter, from where the scan stands, the brackets' depth kept
static void lex_operator(CliInspectScan *scan, int *depth)
{
    const unsigned long long start = scan->pos;
    const CliInspectPlace from = cli_inspect_place(scan);
    const char *op = NULL;
    for (unsigned int at = 0u; (op == NULL) && (at < 47u); at += 1u)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = s_ops[at], .cap = CLI_CELLS_POOL});
        op = ((scan->size - start >= length) &&
              (memor.cmp(&(MemoriaCfg){.src = scan->src + start, .other = s_ops[at], .bytes = length}) == 0))
                 ? s_ops[at]
                 : NULL;
    }
    if (op == NULL)
    {
        (void)cli_inspect_bump(scan);
    }
    else
    {
        for (size_t at = 0u; at < cellul.len(&(CatenaFinitaCfg){.src = op, .cap = CLI_CELLS_POOL}); at += 1u)
        {
            (void)cli_inspect_bump(scan);
        }
        *depth += ((op[0] == '(') || (op[0] == '[') || (op[0] == '{')) ? 1 : 0;
        if (((op[0] == ')') || (op[0] == ']') || (op[0] == '}')) && (*depth > 0))
        {
            *depth -= 1;
        }
    }
    cli_inspect_tok_put(scan, CLI_PY_OP, start, from, 0);
}

// the indent of a line read at its start, its blank and comment lines passed over: 1, or 0 at the text's end
static int lex_line_start(CliInspectScan *scan, unsigned int *indent)
{
    for (;;)
    {
        unsigned int width = 0u;
        for (unsigned int letter = cli_inspect_peek(scan);; letter = cli_inspect_peek(scan))
        {
            if (letter == ' ')
            {
                width += 1u;
            }
            else if (letter == '\t')
            {
                width = ((width / 8u) + 1u) * 8u;
            }
            else if ((letter != 0x0cu) && (letter != '\r'))
            {
                break;
            }
            (void)cli_inspect_bump(scan);
        }
        const unsigned int letter = cli_inspect_peek(scan);
        if (letter == CLI_INSPECT_END)
        {
            return 0;
        }
        if (letter == '\n')
        {
            (void)cli_inspect_bump(scan);
            continue;
        }
        if (letter == '#')
        {
            cli_inspect_skip_line(scan);
            continue;
        }
        *indent = width;
        return 1;
    }
}

void cli_py_read(const char *src, unsigned long long size)
{
    CliInspectScan scan;
    cli_inspect_scan_begun(&scan, src, size);
    g_cli_inspect_toks = 0u;
    g_cli_inspect_comments = 0u;
    g_cli_inspect_over = 0;
    g_cli_py_lines = 0u;
    unsigned int first = 0u;
    unsigned int indent = 0u;
    int depth = 0;
    int at_start = 1;
    for (;;)
    {
        if (at_start)
        {
            if (!lex_line_start(&scan, &indent))
            {
                break;
            }
            at_start = 0;
        }
        const unsigned int letter = cli_inspect_peek(&scan);
        if (letter == CLI_INSPECT_END)
        {
            break;
        }
        const unsigned long long start = scan.pos;
        const CliInspectPlace from = cli_inspect_place(&scan);
        if (letter == '\n')
        {
            (void)cli_inspect_bump(&scan);
            if (depth == 0)
            {
                lex_line_put(indent, &first);
                at_start = 1;
            }
        }
        else if ((letter == ' ') || (letter == '\t') || (letter == '\r') || (letter == 0x0cu))
        {
            (void)cli_inspect_bump(&scan);
        }
        else if (letter == '#')
        {
            cli_inspect_skip_line(&scan);
        }
        else if (letter == '\\')
        {
            (void)cli_inspect_bump(&scan);
            if (cli_inspect_peek(&scan) == '\r')
            {
                (void)cli_inspect_bump(&scan);
            }
            if (cli_inspect_peek(&scan) == '\n')
            {
                (void)cli_inspect_bump(&scan);
            }
        }
        else if ((letter == '\'') || (letter == '"'))
        {
            lex_string(&scan, start, from, 0ull);
        }
        else if (lex_name_start(letter))
        {
            while (lex_name(cli_inspect_peek(&scan)))
            {
                (void)cli_inspect_bump(&scan);
            }
            const unsigned int next = cli_inspect_peek(&scan);
            if (((next == '\'') || (next == '"')) && lex_prefix(src, start, scan.pos))
            {
                lex_string(&scan, start, from, scan.pos - start);
            }
            else
            {
                cli_inspect_tok_put(&scan, CLI_PY_NAME, start, from, 0);
            }
        }
        else if (((letter >= '0') && (letter <= '9')) || ((letter == '.') && (cli_inspect_peek_at(&scan, 1u) >= '0') &&
                                                          (cli_inspect_peek_at(&scan, 1u) <= '9')))
        {
            lex_number(&scan);
        }
        else
        {
            lex_operator(&scan, &depth);
        }
    }
    lex_line_put(indent, &first);
}

const CliInspectTok *cli_py_tok(const CliPyLine *line, unsigned int at)
{
    return &g_cli_inspect_tok[line->first + at];
}

int cli_py_is(const char *src, const CliInspectTok *tok, const char *word)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
    return (tok->end - tok->start == length) &&
           (memor.cmp(&(MemoriaCfg){.src = src + tok->start, .other = word, .bytes = length}) == 0);
}

int cli_py_keyword(const char *src, const CliInspectTok *tok)
{
    for (unsigned int at = 0u; at < 35u; at += 1u)
    {
        if (cli_py_is(src, tok, g_cli_py_keywords[at]))
        {
            return 1;
        }
    }
    return 0;
}

int cli_py_opens(const char *src, const CliPyLine *line)
{
    const CliInspectTok *const last = cli_py_tok(line, line->count - 1u);
    return (last->kind == CLI_PY_OP) && cli_py_is(src, last, ":");
}

unsigned int cli_py_block_end(unsigned int at)
{
    const unsigned int indent = g_cli_py_line[at].indent;
    unsigned int end = at + 1u;
    while ((end < g_cli_py_lines) && (g_cli_py_line[end].indent > indent))
    {
        end += 1u;
    }
    return end;
}

int cli_py_head_is(const char *src, const CliPyLine *line, const char *word)
{
    const CliInspectTok *const first = cli_py_tok(line, 0u);
    const CliInspectTok *const head =
        (cli_py_is(src, first, "async") && (line->count > 1u)) ? cli_py_tok(line, 1u) : first;
    return cli_py_is(src, head, word);
}

int cli_py_holds(const char *src, const CliInspectTok *toks, unsigned int count, const char *word)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_py_is(src, &toks[at], word))
        {
            return 1;
        }
    }
    return 0;
}
