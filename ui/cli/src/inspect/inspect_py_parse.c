// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_parse.c: the parse of a Python file for the editor: each column's class, a name's by what it is; the
// regions that fold; and the spans Expand Selection steps through, its strings, its brackets, its lines and its
// blocks, a decorated function or class with its decorators
#include "inspect_py.h"

// the most open brackets a line holds, and the most functions open around a line
#define CLI_PY_STACK 4096u

// the brackets, the stops and the separators, which a parse colors as delimiters, every other operator as an operator
static const char *const s_delimiters[9] = {"(", ")", "[", "]", "{", "}", ",", ";", "."};

// the names the file's classes have; the functions open around the line being painted, each its header's line, the
// line past its block, and its parameters, `count` of them from `first` among the parameters' tokens
static CliInspectSet s_classes;
static unsigned int s_scope_at[CLI_PY_STACK];
static unsigned int s_scope_end[CLI_PY_STACK];
static unsigned int s_scope_first[CLI_PY_STACK];
static unsigned int s_scope_count[CLI_PY_STACK];
static unsigned int s_scopes;
static unsigned int *s_params;
static unsigned int s_param_count;

void cli_inspect_py_parse_laid(CliInspectCarve *carve)
{
    cli_inspect_set_laid(&s_classes, carve);
    s_params = cli_inspect_carved(carve, (unsigned long long)(CLI_INSPECT_TOKENS / 4u) * sizeof(unsigned int));
}
static unsigned int s_stack[CLI_PY_STACK];

// whether a token is one of `count` words
static int parse_one_of(const char *src, const CliInspectTok *tok, const char *const *words, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_py_is(src, tok, words[at]))
        {
            return 1;
        }
    }
    return 0;
}

// the names the header of the function at `at` declares as its parameters, put on s_params from their scope's first
static void parse_parameters(const char *src, unsigned int at)
{
    const CliPyLine *const line = &g_cli_py_line[at];
    static const char *const s_before[4] = {"(", ",", "*", "**"};
    static const char *const s_after[4] = {":", "=", ",", ")"};
    int depth = 0;
    for (unsigned int offset = 0u; offset < line->count; offset += 1u)
    {
        const CliInspectTok *const tok = cli_py_tok(line, offset);
        depth += (cli_py_is(src, tok, "(") || cli_py_is(src, tok, "[") || cli_py_is(src, tok, "{")) ? 1 : 0;
        depth -= (cli_py_is(src, tok, ")") || cli_py_is(src, tok, "]") || cli_py_is(src, tok, "}")) ? 1 : 0;
        const int before = (offset > 0u) && parse_one_of(src, cli_py_tok(line, offset - 1u), s_before, 4u);
        const int after = (offset + 1u < line->count) && parse_one_of(src, cli_py_tok(line, offset + 1u), s_after, 4u);
        if ((tok->kind == CLI_PY_NAME) && (depth == 1) && before && after && (s_param_count < CLI_INSPECT_TOKENS / 4u))
        {
            s_params[s_param_count] = line->first + offset;
            s_param_count += 1u;
        }
    }
}

// whether a name is a parameter of a function open around the line being painted
static int parse_parameter(const char *src, const CliInspectTok *tok)
{
    for (unsigned int scope = 0u; scope < s_scopes; scope += 1u)
    {
        for (unsigned int at = 0u; at < s_scope_count[scope]; at += 1u)
        {
            const CliInspectTok *const param = &g_cli_inspect_tok[s_params[s_scope_first[scope] + at]];
            if ((param->end - param->start == tok->end - tok->start) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = src + param->start, .other = src + tok->start, .bytes = tok->end - tok->start}) == 0))
            {
                return 1;
            }
        }
    }
    return 0;
}

// the functions open around line `index`: those whose blocks end at it let go, and its own taken on
static void parse_scopes(const char *src, unsigned int index)
{
    while ((s_scopes > 0u) && (s_scope_end[s_scopes - 1u] <= index))
    {
        s_param_count = s_scope_first[s_scopes - 1u];
        s_scopes -= 1u;
    }
    if (cli_py_head_is(src, &g_cli_py_line[index], "def") && (s_scopes < CLI_PY_STACK))
    {
        s_scope_at[s_scopes] = index;
        s_scope_end[s_scopes] = cli_py_block_end(index);
        s_scope_first[s_scopes] = s_param_count;
        parse_parameters(src, index);
        s_scope_count[s_scopes] = s_param_count - s_scope_first[s_scopes];
        s_scopes += 1u;
    }
}

// the class of a name at `offset` of a line, by what it is
static unsigned char parse_name_class(const char *src, const CliPyLine *line, unsigned int offset, int depth,
                                      int decorated, int importing, unsigned int imported_at)
{
    const CliInspectTok *const tok = cli_py_tok(line, offset);
    const CliInspectTok *const before = (offset > 0u) ? cli_py_tok(line, offset - 1u) : NULL;
    const CliInspectTok *const after = (offset + 1u < line->count) ? cli_py_tok(line, offset + 1u) : NULL;
    static const char *const s_predefined[5] = {"True", "False", "None", "self", "cls"};
    const int called = (after != NULL) && cli_py_is(src, after, "(");
    const int kept_class = cli_inspect_set_has(&s_classes, src + tok->start, tok->end - tok->start);
    if (parse_one_of(src, tok, s_predefined, 5u))
    {
        return CLI_INSPECT_PREDEFINED;
    }
    if (cli_py_keyword(src, tok))
    {
        return CLI_INSPECT_KEYWORD;
    }
    if (decorated && (depth == 0))
    {
        return CLI_INSPECT_ATTRIBUTE;
    }
    if (importing && (cli_py_is(src, cli_py_tok(line, 0u), "import") || (offset < imported_at)))
    {
        return CLI_INSPECT_MODULE;
    }
    if ((before != NULL) && cli_py_is(src, before, "def"))
    {
        return CLI_INSPECT_FUNCTION;
    }
    if ((before != NULL) && cli_py_is(src, before, "class"))
    {
        return CLI_INSPECT_TYPE;
    }
    if ((before != NULL) && cli_py_is(src, before, "."))
    {
        return called ? CLI_INSPECT_FUNCTION : CLI_INSPECT_PROPERTY;
    }
    if (called)
    {
        return kept_class ? CLI_INSPECT_TYPE : CLI_INSPECT_FUNCTION;
    }
    const int keyword_argument = (after != NULL) && cli_py_is(src, after, "=") && (depth > 0) && (before != NULL) &&
                                 (cli_py_is(src, before, "(") || cli_py_is(src, before, ","));
    if (keyword_argument || parse_parameter(src, tok))
    {
        return CLI_INSPECT_PARAMETER;
    }
    return kept_class ? CLI_INSPECT_TYPE : CLI_INSPECT_NONE;
}

// a string's span and the span inside its quotes put on the parse, with its fold
static void parse_string(const char *src, const CliInspectTok *tok, CliParsed *parsed)
{
    const char *const whole = src + tok->start;
    const unsigned int length = tok->end - tok->start;
    unsigned int open = 0u;
    while ((open < length) && (whole[open] != '"') && (whole[open] != '\''))
    {
        open += 1u;
    }
    open = (open == length) ? 0u : open;
    const int triple = (length - open >= 3u) &&
                       ((memor.cmp(&(MemoriaCfg){.src = whole + open, .other = "\"\"\"", .bytes = 3u}) == 0) ||
                        (memor.cmp(&(MemoriaCfg){.src = whole + open, .other = "'''", .bytes = 3u}) == 0));
    const unsigned int quotes = triple ? 3u : 1u;
    cli_inspect_range_put(parsed, tok->from, tok->to);
    if (length >= open + (quotes * 2u))
    {
        const unsigned int to_col = (tok->to.col > quotes) ? (tok->to.col - quotes) : 0u;
        cli_inspect_range_put(parsed, (CliInspectPlace){tok->from.line, tok->from.col + open + quotes},
                              (CliInspectPlace){tok->to.line, to_col});
    }
    cli_inspect_fold_put(parsed, tok->from.line, tok->to.line);
}

// one logical line painted, its brackets' and strings' spans put on the parse
static void parse_line(const char *src, unsigned int index, CliParsed *parsed)
{
    const CliPyLine *const line = &g_cli_py_line[index];
    const CliInspectTok *const head = cli_py_tok(line, 0u);
    const int importing = cli_py_is(src, head, "import") || cli_py_is(src, head, "from");
    unsigned int imported_at = line->count;
    for (unsigned int at = 0u; (imported_at == line->count) && (at < line->count); at += 1u)
    {
        imported_at = cli_py_is(src, cli_py_tok(line, at), "import") ? at : imported_at;
    }
    const int decorated = cli_py_is(src, head, "@");
    int depth = 0;
    unsigned int stack = 0u;
    for (unsigned int offset = 0u; offset < line->count; offset += 1u)
    {
        const CliInspectTok *const tok = cli_py_tok(line, offset);
        unsigned char class_of = CLI_INSPECT_NONE;
        if (tok->kind == CLI_PY_STR)
        {
            class_of = CLI_INSPECT_STRING;
        }
        else if (tok->kind == CLI_PY_NUMBER)
        {
            class_of = CLI_INSPECT_NUMBER;
        }
        else if (tok->kind == CLI_PY_NAME)
        {
            if (tok->field)
            {
                continue;
            }
            class_of = parse_name_class(src, line, offset, depth, decorated, importing, imported_at);
        }
        else
        {
            if (cli_py_is(src, tok, "(") || cli_py_is(src, tok, "[") || cli_py_is(src, tok, "{"))
            {
                depth += 1;
                if (stack < CLI_PY_STACK)
                {
                    s_stack[stack] = offset;
                    stack += 1u;
                }
            }
            else if (cli_py_is(src, tok, ")") || cli_py_is(src, tok, "]") || cli_py_is(src, tok, "}"))
            {
                depth -= 1;
                if (stack > 0u)
                {
                    stack -= 1u;
                    const CliInspectTok *const opened = cli_py_tok(line, s_stack[stack]);
                    cli_inspect_range_put(parsed, opened->to, tok->from);
                    cli_inspect_range_put(parsed, opened->from, tok->to);
                    cli_inspect_fold_put(parsed, opened->from.line, tok->to.line);
                }
            }
            class_of = (decorated && (depth == 0) && ((offset == 0u) || cli_py_is(src, tok, ".")))
                           ? CLI_INSPECT_ATTRIBUTE
                       : parse_one_of(src, tok, s_delimiters, 9u) ? CLI_INSPECT_DELIMITER
                                                                  : CLI_INSPECT_OPERATOR;
        }
        cli_inspect_paint(tok->from, tok->to, class_of);
        if (tok->kind == CLI_PY_STR)
        {
            parse_string(src, tok, parsed);
        }
    }
    const CliInspectPlace last = cli_py_tok(line, line->count - 1u)->to;
    cli_inspect_range_put(parsed, head->from, last);
    cli_inspect_fold_put(parsed, head->from.line, last.line);
    if (!cli_py_opens(src, line))
    {
        return;
    }
    const unsigned int end = cli_py_block_end(index);
    if (end <= index + 1u)
    {
        return;
    }
    const CliPyLine *const closing = &g_cli_py_line[end - 1u];
    const CliInspectPlace close = cli_py_tok(closing, closing->count - 1u)->to;
    cli_inspect_range_put(parsed, head->from, close);
    cli_inspect_range_put(parsed, cli_py_tok(&g_cli_py_line[index + 1u], 0u)->from, close);
    cli_inspect_fold_put(parsed, head->from.line, close.line);
    // a decorated function or class, with its decorators
    unsigned int first = index;
    while ((first > 0u) && (g_cli_py_line[first - 1u].indent == line->indent) &&
           cli_py_is(src, cli_py_tok(&g_cli_py_line[first - 1u], 0u), "@"))
    {
        first -= 1u;
    }
    if (first < index)
    {
        cli_inspect_range_put(parsed, cli_py_tok(&g_cli_py_line[first], 0u)->from, close);
    }
}

void cli_inspect_py_parse(const char *src, unsigned long long size, CliParsed *parsed)
{
    parsed->runs = 0u;
    parsed->lines = 0u;
    parsed->folds = 0u;
    parsed->ranges = 0u;
    parsed->full = 0;
    cli_py_read(src, size);
    cli_inspect_paint_begun(src, size);
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        cli_inspect_paint(g_cli_inspect_comment[at].from, g_cli_inspect_comment[at].to, CLI_INSPECT_COMMENT);
    }
    cli_inspect_set_cleared(&s_classes);
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if (!cli_py_head_is(src, line, "class"))
        {
            continue;
        }
        for (unsigned int offset = 0u; offset + 1u < line->count; offset += 1u)
        {
            if (cli_py_is(src, cli_py_tok(line, offset), "class"))
            {
                const CliInspectTok *const name = cli_py_tok(line, offset + 1u);
                cli_inspect_set_put(&s_classes, src + name->start, name->end - name->start);
                break;
            }
        }
    }
    s_scopes = 0u;
    s_param_count = 0u;
    for (unsigned int index = 0u; index < g_cli_py_lines; index += 1u)
    {
        parse_scopes(src, index);
        parse_line(src, index, parsed);
    }
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        cli_inspect_range_put(parsed, g_cli_inspect_comment[at].from, g_cli_inspect_comment[at].to);
    }
    cli_inspect_paint_runs(parsed);
    cli_inspect_folds_done(parsed);
    parsed->full = parsed->full || g_cli_inspect_over;
}
