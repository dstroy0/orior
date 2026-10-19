// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_lex.c: JavaScript's tokens: strings, templates with the code of their `${}` read as tokens before them,
// regular expressions where a value is due, names, numbers and punctuation, the longest that matches; and the
// reading of them: the bracket each bracket matches, what each brace opens, and the brace each token stands directly
// inside
#include "inspect_js.h"

unsigned int *g_cli_js_pair;
unsigned char *g_cli_js_brace;
unsigned int *g_cli_js_within;

// the open brackets of the reading, a list of the reading's cell
static unsigned int *s_stack;

void cli_inspect_js_lex_laid(CliInspectCarve *carve)
{
    const unsigned long long tokens = CLI_INSPECT_TOKENS;
    g_cli_js_pair = cli_inspect_carved(carve, tokens * sizeof(unsigned int));
    g_cli_js_brace = cli_inspect_carved(carve, tokens);
    g_cli_js_within = cli_inspect_carved(carve, tokens * sizeof(unsigned int));
    s_stack = cli_inspect_carved(carve, tokens * sizeof(unsigned int));
}

const char *const g_cli_js_keywords[35] = {"break",   "case",     "catch", "class",  "const",  "continue",   "debugger",
                                           "default", "delete",   "do",    "else",   "export", "extends",    "finally",
                                           "for",     "function", "if",    "import", "in",     "instanceof", "let",
                                           "new",     "return",   "super", "switch", "this",   "throw",      "try",
                                           "typeof",  "var",      "void",  "while",  "with",   "yield",      "await"};

const char *const g_cli_js_control[7] = {"if", "for", "while", "switch", "catch", "with", "return"};

// the punctuation, the longest that matches first
static const char *const s_puncts[52] = {">>>=", "...", "===", "!==", "**=", "<<=", ">>=", ">>>", "&&=", "||=", "?\?=",
                                         "=>",   "==",  "!=",  "<=",  ">=",  "&&",  "||",  "??",  "?.",  "++",  "--",
                                         "+=",   "-=",  "*=",  "/=",  "%=",  "&=",  "|=",  "^=",  "**",  "<<",  ">>",
                                         "+",    "-",   "*",   "/",   "%",   "&",   "|",   "^",   "!",   "~",   "<",
                                         ">",    "=",   "?",   ":",   ";",   ",",   ".",   "@"};

// the words after which a slash starts a regular expression
static const char *const s_before_value[14] = {"return", "typeof", "instanceof", "in", "of",   "new",   "delete",
                                               "void",   "throw",  "case",       "do", "else", "yield", "await"};

static int lex_name_start(unsigned int letter)
{
    return (letter == '_') || (letter == '$') || cli_inspect_alphabetic(letter);
}

static int lex_name(unsigned int letter)
{
    return (letter == '_') || (letter == '$') || cli_inspect_alphanumeric(letter);
}

int cli_js_is(const char *src, unsigned int at, const char *word)
{
    if (at == CLI_JS_NO)
    {
        return 0;
    }
    const CliInspectTok *const tok = &g_cli_inspect_tok[at];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
    return (tok->end - tok->start == length) &&
           (memor.cmp(&(MemoriaCfg){.src = src + tok->start, .other = word, .bytes = length}) == 0);
}

int cli_js_one_of(const char *src, unsigned int at, const char *const *words, unsigned int count)
{
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        if (cli_js_is(src, at, words[index]))
        {
            return 1;
        }
    }
    return 0;
}

// whether a value is due after the tokens so far, and a slash starts a regular expression
static int lex_value_due(const char *src)
{
    if (g_cli_inspect_toks == 0u)
    {
        return 1;
    }
    const unsigned int last = g_cli_inspect_toks - 1u;
    const unsigned char kind = g_cli_inspect_tok[last].kind;
    if (kind == CLI_JS_PUNCT)
    {
        return !cli_js_is(src, last, ")") && !cli_js_is(src, last, "]") && !cli_js_is(src, last, "}");
    }
    return (kind == CLI_JS_NAME) && cli_js_one_of(src, last, s_before_value, 14u);
}

// a comment of `/* */` from where the scan stands
static void lex_block_comment(CliInspectScan *scan)
{
    const CliInspectPlace from = cli_inspect_place(scan);
    (void)cli_inspect_bump(scan);
    (void)cli_inspect_bump(scan);
    for (unsigned int letter = cli_inspect_peek(scan); letter != CLI_INSPECT_END; letter = cli_inspect_peek(scan))
    {
        if ((letter == '*') && (cli_inspect_peek_at(scan, 1u) == '/'))
        {
            (void)cli_inspect_bump(scan);
            (void)cli_inspect_bump(scan);
            break;
        }
        (void)cli_inspect_bump(scan);
    }
    cli_inspect_comment_put(from, cli_inspect_place(scan));
}

// a regular expression from its slash, its flags with it
static void lex_regex(CliInspectScan *scan)
{
    const unsigned long long start = scan->pos;
    const CliInspectPlace from = cli_inspect_place(scan);
    (void)cli_inspect_bump(scan);
    int in_class = 0;
    for (unsigned int letter = cli_inspect_peek(scan); letter != CLI_INSPECT_END; letter = cli_inspect_peek(scan))
    {
        (void)cli_inspect_bump(scan);
        if (letter == '\\')
        {
            (void)cli_inspect_bump(scan);
        }
        else if (letter == '[')
        {
            in_class = 1;
        }
        else if (letter == ']')
        {
            in_class = 0;
        }
        else if (((letter == '/') && !in_class) || (letter == '\n'))
        {
            break;
        }
    }
    while (lex_name(cli_inspect_peek(scan)))
    {
        (void)cli_inspect_bump(scan);
    }
    cli_inspect_tok_put(scan, CLI_JS_REGEX, start, from, 0);
}

// punctuation, the longest that matches, else the one letter
static void lex_punct(CliInspectScan *scan)
{
    const unsigned long long start = scan->pos;
    const CliInspectPlace from = cli_inspect_place(scan);
    size_t length = 1u;
    for (unsigned int at = 0u; at < 52u; at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = s_puncts[at], .cap = CLI_CELLS_POOL});
        if ((scan->size - start >= size) &&
            (memor.cmp(&(MemoriaCfg){.src = scan->src + start, .other = s_puncts[at], .bytes = size}) == 0))
        {
            length = size;
            break;
        }
    }
    for (size_t at = 0u; at < length; at += 1u)
    {
        (void)cli_inspect_bump(scan);
    }
    cli_inspect_tok_put(scan, CLI_JS_PUNCT, start, from, 0);
}

// tokens read until the text ends or, `inside` a template's `${`, until its `}`
static void lex(CliInspectScan *scan, int inside)
{
    int depth = 0;
    for (unsigned int letter = cli_inspect_peek(scan); letter != CLI_INSPECT_END; letter = cli_inspect_peek(scan))
    {
        const unsigned long long start = scan->pos;
        const CliInspectPlace from = cli_inspect_place(scan);
        const unsigned int next = cli_inspect_peek_at(scan, 1u);
        if ((letter == ' ') || (letter == '\t') || (letter == '\r') || (letter == '\n') || (letter == 0xfeffu) ||
            (letter == 0xa0u))
        {
            (void)cli_inspect_bump(scan);
        }
        else if ((letter == '/') && (next == '/'))
        {
            cli_inspect_skip_line(scan);
        }
        else if ((letter == '/') && (next == '*'))
        {
            lex_block_comment(scan);
        }
        else if ((letter == '\'') || (letter == '"'))
        {
            (void)cli_inspect_bump(scan);
            for (unsigned int one = cli_inspect_peek(scan); one != CLI_INSPECT_END; one = cli_inspect_peek(scan))
            {
                (void)cli_inspect_bump(scan);
                if (one == '\\')
                {
                    (void)cli_inspect_bump(scan);
                }
                else if ((one == letter) || (one == '\n'))
                {
                    break;
                }
            }
            cli_inspect_tok_put(scan, CLI_JS_STR, start, from, 0);
        }
        else if (letter == '`')
        {
            (void)cli_inspect_bump(scan);
            for (unsigned int one = cli_inspect_peek(scan); one != CLI_INSPECT_END; one = cli_inspect_peek(scan))
            {
                if (one == '\\')
                {
                    (void)cli_inspect_bump(scan);
                    (void)cli_inspect_bump(scan);
                }
                else if (one == '`')
                {
                    (void)cli_inspect_bump(scan);
                    break;
                }
                else if ((one == '$') && (cli_inspect_peek_at(scan, 1u) == '{'))
                {
                    (void)cli_inspect_bump(scan);
                    (void)cli_inspect_bump(scan);
                    lex(scan, 1);
                }
                else
                {
                    (void)cli_inspect_bump(scan);
                }
            }
            cli_inspect_tok_put(scan, CLI_JS_STR, start, from, 0);
        }
        else if ((letter == '/') && lex_value_due(scan->src))
        {
            lex_regex(scan);
        }
        else if (lex_name_start(letter) || (letter == '#'))
        {
            (void)cli_inspect_bump(scan);
            while (lex_name(cli_inspect_peek(scan)))
            {
                (void)cli_inspect_bump(scan);
            }
            cli_inspect_tok_put(scan, CLI_JS_NAME, start, from, 0);
        }
        else if (((letter >= '0') && (letter <= '9')) || ((letter == '.') && (next >= '0') && (next <= '9')))
        {
            for (unsigned int one = cli_inspect_peek(scan);
                 (one != CLI_INSPECT_END) && (cli_inspect_alphanumeric(one) || (one == '_') || (one == '.'));
                 one = cli_inspect_peek(scan))
            {
                (void)cli_inspect_bump(scan);
            }
            cli_inspect_tok_put(scan, CLI_JS_NUMBER, start, from, 0);
        }
        else if ((letter == '(') || (letter == '[') || (letter == '{') || (letter == ')') || (letter == ']') ||
                 (letter == '}'))
        {
            if (inside && (letter == '}') && (depth == 0))
            {
                (void)cli_inspect_bump(scan);
                return;
            }
            depth += ((letter == '(') || (letter == '[') || (letter == '{')) ? 1 : -1;
            (void)cli_inspect_bump(scan);
            cli_inspect_tok_put(scan, CLI_JS_PUNCT, start, from, 0);
        }
        else
        {
            lex_punct(scan);
        }
    }
}

// what the brace at `at` opens
static unsigned char read_brace(const char *src, unsigned int at)
{
    if (at == 0u)
    {
        return CLI_JS_BLOCK;
    }
    const unsigned int before = at - 1u;
    if (cli_js_is(src, before, "=>"))
    {
        return CLI_JS_FUNCTION;
    }
    if (cli_js_is(src, before, ")") && (g_cli_js_pair[before] != CLI_JS_NO))
    {
        const unsigned int open = g_cli_js_pair[before];
        const unsigned int name = (open > 0u) ? (open - 1u) : CLI_JS_NO;
        if (cli_js_is(src, name, "function"))
        {
            return CLI_JS_FUNCTION;
        }
        const int named = (name != CLI_JS_NO) && (g_cli_inspect_tok[name].kind == CLI_JS_NAME) &&
                          !cli_js_one_of(src, name, g_cli_js_control, 7u);
        return named ? CLI_JS_FUNCTION : CLI_JS_BLOCK;
    }
    // `class Name {` and `class Name extends Base {`
    for (unsigned int back = at; back > ((at > 8u) ? (at - 8u) : 0u); back -= 1u)
    {
        if (cli_js_is(src, back - 1u, "class") && (g_cli_inspect_tok[back - 1u].kind == CLI_JS_NAME))
        {
            return CLI_JS_CLASS;
        }
        if (cli_js_is(src, back - 1u, ";") || cli_js_is(src, back - 1u, "{") || cli_js_is(src, back - 1u, "}"))
        {
            break;
        }
    }
    static const char *const s_closers[4] = {")", "]", "}", ";"};
    static const char *const s_values[7] = {"return", "yield", "await", "case", "default", "in", "of"};
    const int punct = (g_cli_inspect_tok[before].kind == CLI_JS_PUNCT) && !cli_js_one_of(src, before, s_closers, 4u);
    return (punct || cli_js_one_of(src, before, s_values, 7u)) ? CLI_JS_OBJECT : CLI_JS_BLOCK;
}

void cli_js_read(const char *src, unsigned long long size)
{
    CliInspectScan scan;
    cli_inspect_scan_begun(&scan, src, size);
    g_cli_inspect_toks = 0u;
    g_cli_inspect_comments = 0u;
    g_cli_inspect_over = 0;
    lex(&scan, 0);
    unsigned int stack = 0u;
    unsigned int brace = CLI_JS_NO;
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        g_cli_js_pair[at] = CLI_JS_NO;
        g_cli_js_brace[at] = CLI_JS_NONE;
        brace = CLI_JS_NO;
        for (unsigned int open = stack; open > 0u; open -= 1u)
        {
            if (cli_js_is(src, s_stack[open - 1u], "{"))
            {
                brace = s_stack[open - 1u];
                break;
            }
        }
        g_cli_js_within[at] = brace;
        if (g_cli_inspect_tok[at].kind != CLI_JS_PUNCT)
        {
            continue;
        }
        if (cli_js_is(src, at, "(") || cli_js_is(src, at, "[") || cli_js_is(src, at, "{"))
        {
            s_stack[stack] = at;
            stack += 1u;
        }
        else if ((cli_js_is(src, at, ")") || cli_js_is(src, at, "]") || cli_js_is(src, at, "}")) && (stack > 0u))
        {
            stack -= 1u;
            g_cli_js_pair[s_stack[stack]] = at;
            g_cli_js_pair[at] = s_stack[stack];
        }
    }
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        if (cli_js_is(src, at, "{") && (g_cli_inspect_tok[at].kind == CLI_JS_PUNCT))
        {
            g_cli_js_brace[at] = read_brace(src, at);
        }
    }
}

int cli_js_is_read(const char *src, unsigned int at)
{
    const CliInspectTok *const tok = &g_cli_inspect_tok[at];
    if ((tok->kind != CLI_JS_NAME) || cli_js_one_of(src, at, g_cli_js_keywords, 35u) || (src[tok->start] == '#'))
    {
        return 0;
    }
    const unsigned int before = (at > 0u) ? (at - 1u) : CLI_JS_NO;
    if (cli_js_is(src, before, ".") || cli_js_is(src, before, "?."))
    {
        return 0;
    }
    const unsigned int after = (at + 1u < g_cli_inspect_toks) ? (at + 1u) : CLI_JS_NO;
    const unsigned int open = g_cli_js_within[at];
    const unsigned char holder = (open != CLI_JS_NO) ? g_cli_js_brace[open] : CLI_JS_NONE;
    if ((holder == CLI_JS_OBJECT) && (cli_js_is(src, before, "{") || cli_js_is(src, before, ",")) &&
        (cli_js_is(src, after, ":") || cli_js_is(src, after, "(")))
    {
        return 0;
    }
    static const char *const s_members[8] = {"{", "}", ";", "static", "async", "get", "set", "*"};
    return !((holder == CLI_JS_CLASS) && cli_js_is(src, after, "(") && cli_js_one_of(src, before, s_members, 8u));
}

int cli_js_pair_depth(const char *src, unsigned int from, unsigned int to)
{
    int depth = 0;
    for (unsigned int at = from; at < to; at += 1u)
    {
        depth += (cli_js_is(src, at, "(") || cli_js_is(src, at, "[") || cli_js_is(src, at, "{")) ? 1 : 0;
        depth -= (cli_js_is(src, at, ")") || cli_js_is(src, at, "]") || cli_js_is(src, at, "}")) ? 1 : 0;
    }
    return depth;
}
