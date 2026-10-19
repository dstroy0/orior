// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_parse.c: the parse of a JavaScript file for the editor: each column's class, a name's by what it is; the
// regions that fold; and the spans Expand Selection steps through, its comments, strings, brackets, and each function
// declared and each class from its first word to its closing brace
#include "inspect_js.h"

// the most functions a parse keeps the parameters of, and the most parameters it keeps
#define CLI_JS_SCOPES (CLI_INSPECT_TOKENS / 8u)
#define CLI_JS_PARAMS (CLI_INSPECT_TOKENS / 4u)

// the names the file's classes have; every parameter's name; and each function, the first token of its parameters,
// its closing brace, and its parameters, `count` of them from `first` among the parameters' tokens
static CliInspectSet s_classes;
static CliInspectSet s_names;
static CliInspectSet s_scope_names;
static unsigned int *s_scope_from;
static unsigned int *s_scope_to;
static unsigned int *s_scope_first;
static unsigned int *s_scope_count;
static unsigned int s_scopes;
static unsigned int *s_params;
static unsigned int s_param_count;

void cli_inspect_js_parse_laid(CliInspectCarve *carve)
{
    cli_inspect_set_laid(&s_classes, carve);
    cli_inspect_set_laid(&s_names, carve);
    cli_inspect_set_laid(&s_scope_names, carve);
    const unsigned long long scopes = CLI_JS_SCOPES;
    s_scope_from = cli_inspect_carved(carve, scopes * sizeof(unsigned int));
    s_scope_to = cli_inspect_carved(carve, scopes * sizeof(unsigned int));
    s_scope_first = cli_inspect_carved(carve, scopes * sizeof(unsigned int));
    s_scope_count = cli_inspect_carved(carve, scopes * sizeof(unsigned int));
    s_params = cli_inspect_carved(carve, (unsigned long long)CLI_JS_PARAMS * sizeof(unsigned int));
}

unsigned int cli_js_parameters(const char *src, unsigned int open, CliInspectSet *names)
{
    if (open == 0u)
    {
        return open;
    }
    const unsigned int before = open - 1u;
    unsigned int first = 0u;
    unsigned int last = 0u;
    if (cli_js_is(src, before, ")"))
    {
        first = (g_cli_js_pair[before] != CLI_JS_NO) ? g_cli_js_pair[before] : before;
        last = before;
    }
    else if (cli_js_is(src, before, "=>"))
    {
        if (before == 0u)
        {
            first = before;
            last = before;
        }
        else if (cli_js_is(src, before - 1u, ")"))
        {
            first = (g_cli_js_pair[before - 1u] != CLI_JS_NO) ? g_cli_js_pair[before - 1u] : (before - 1u);
            last = before - 1u;
        }
        else
        {
            first = before - 1u;
            last = before;
        }
    }
    else
    {
        return open;
    }
    if (cli_js_is(src, before, "=>") && (last == first + 1u) && (g_cli_inspect_tok[first].kind == CLI_JS_NAME))
    {
        cli_inspect_set_put(names, src + g_cli_inspect_tok[first].start,
                            g_cli_inspect_tok[first].end - g_cli_inspect_tok[first].start);
        return first;
    }
    static const char *const s_before[3] = {"(", ",", "..."};
    static const char *const s_after[3] = {",", ")", "="};
    for (unsigned int at = first + 1u; at < last; at += 1u)
    {
        if ((g_cli_inspect_tok[at].kind == CLI_JS_NAME) && (cli_js_pair_depth(src, first + 1u, at) == 0) &&
            cli_js_one_of(src, at - 1u, s_before, 3u) && cli_js_one_of(src, at + 1u, s_after, 3u))
        {
            cli_inspect_set_put(names, src + g_cli_inspect_tok[at].start,
                                g_cli_inspect_tok[at].end - g_cli_inspect_tok[at].start);
        }
    }
    return first;
}

// the functions' parameters kept: each function's span, and its names among the parameters' tokens
static void parse_scopes(const char *src)
{
    s_scopes = 0u;
    s_param_count = 0u;
    cli_inspect_set_cleared(&s_names);
    for (unsigned int open = 0u; (open < g_cli_inspect_toks) && (s_scopes < CLI_JS_SCOPES); open += 1u)
    {
        if (g_cli_js_brace[open] != CLI_JS_FUNCTION)
        {
            continue;
        }
        cli_inspect_set_cleared(&s_scope_names);
        const unsigned int first = cli_js_parameters(src, open, &s_scope_names);
        s_scope_from[s_scopes] = first;
        s_scope_to[s_scopes] = (g_cli_js_pair[open] != CLI_JS_NO) ? g_cli_js_pair[open] : open;
        s_scope_first[s_scopes] = s_param_count;
        for (unsigned int at = first; at < open; at += 1u)
        {
            const CliInspectTok *const tok = &g_cli_inspect_tok[at];
            if ((tok->kind == CLI_JS_NAME) &&
                cli_inspect_set_has(&s_scope_names, src + tok->start, tok->end - tok->start) &&
                (s_param_count < CLI_JS_PARAMS))
            {
                s_params[s_param_count] = at;
                s_param_count += 1u;
                cli_inspect_set_put(&s_names, src + tok->start, tok->end - tok->start);
            }
        }
        s_scope_count[s_scopes] = s_param_count - s_scope_first[s_scopes];
        s_scopes += 1u;
    }
}

// whether the name at `at` is a parameter of a function whose span holds it
static int parse_parameter(const char *src, unsigned int at)
{
    const CliInspectTok *const tok = &g_cli_inspect_tok[at];
    if (!cli_inspect_set_has(&s_names, src + tok->start, tok->end - tok->start))
    {
        return 0;
    }
    for (unsigned int scope = 0u; scope < s_scopes; scope += 1u)
    {
        if ((s_scope_from[scope] > at) || (at > s_scope_to[scope]))
        {
            continue;
        }
        for (unsigned int index = 0u; index < s_scope_count[scope]; index += 1u)
        {
            const CliInspectTok *const param = &g_cli_inspect_tok[s_params[s_scope_first[scope] + index]];
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

// the class of the name at `at`, by what it is
static unsigned char parse_name_class(const char *src, unsigned int at)
{
    const CliInspectTok *const tok = &g_cli_inspect_tok[at];
    const unsigned int before = (at > 0u) ? (at - 1u) : CLI_JS_NO;
    const unsigned int after = (at + 1u < g_cli_inspect_toks) ? (at + 1u) : CLI_JS_NO;
    const unsigned int open = g_cli_js_within[at];
    const unsigned char holder = (open != CLI_JS_NO) ? g_cli_js_brace[open] : CLI_JS_NONE;
    static const char *const s_predefined[6] = {"true", "false", "null", "undefined", "this", "super"};
    static const char *const s_contextual[5] = {"async", "of", "static", "get", "set"};
    static const char *const s_not_after[7] = {"(", "=", ".", ",", ")", ";", ":"};
    static const char *const s_typed[3] = {"class", "new", "extends"};
    const int classed = cli_inspect_set_has(&s_classes, src + tok->start, tok->end - tok->start);
    if (cli_js_one_of(src, at, s_predefined, 6u))
    {
        return CLI_INSPECT_PREDEFINED;
    }
    const int contextual =
        cli_js_one_of(src, at, s_contextual, 5u) && (after != CLI_JS_NO) && !cli_js_one_of(src, after, s_not_after, 7u);
    if (cli_js_one_of(src, at, g_cli_js_keywords, 35u) || contextual)
    {
        return CLI_INSPECT_KEYWORD;
    }
    if (cli_js_is(src, before, "function"))
    {
        return CLI_INSPECT_FUNCTION;
    }
    if (cli_js_one_of(src, before, s_typed, 3u))
    {
        return CLI_INSPECT_TYPE;
    }
    if (cli_js_is(src, before, ".") || cli_js_is(src, before, "?."))
    {
        return cli_js_is(src, after, "(") ? CLI_INSPECT_FUNCTION : CLI_INSPECT_PROPERTY;
    }
    if ((holder == CLI_JS_OBJECT) && (cli_js_is(src, before, "{") || cli_js_is(src, before, ",")) &&
        cli_js_is(src, after, ":"))
    {
        return CLI_INSPECT_PROPERTY;
    }
    if (cli_js_is(src, after, "("))
    {
        return classed ? CLI_INSPECT_TYPE : CLI_INSPECT_FUNCTION;
    }
    if (parse_parameter(src, at))
    {
        return CLI_INSPECT_PARAMETER;
    }
    return classed ? CLI_INSPECT_TYPE : CLI_INSPECT_NONE;
}

void cli_inspect_js_parse(const char *src, unsigned long long size, CliParsed *parsed)
{
    parsed->runs = 0u;
    parsed->lines = 0u;
    parsed->folds = 0u;
    parsed->ranges = 0u;
    parsed->full = 0;
    cli_js_read(src, size);
    cli_inspect_paint_begun(src, size);
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        const CliInspectSpan comment = g_cli_inspect_comment[at];
        cli_inspect_paint(comment.from, comment.to, CLI_INSPECT_COMMENT);
        cli_inspect_fold_put(parsed, comment.from.line, comment.to.line);
        cli_inspect_range_put(parsed, comment.from, comment.to);
    }
    // strings first, as a template's own code is read inside it and drawn over it
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        const CliInspectTok *const tok = &g_cli_inspect_tok[at];
        if ((tok->kind != CLI_JS_STR) && (tok->kind != CLI_JS_REGEX))
        {
            continue;
        }
        cli_inspect_paint(tok->from, tok->to, CLI_INSPECT_STRING);
        cli_inspect_fold_put(parsed, tok->from.line, tok->to.line);
        cli_inspect_range_put(parsed, tok->from, tok->to);
        if ((tok->end - tok->start >= 2u) && (tok->kind == CLI_JS_STR))
        {
            cli_inspect_range_put(parsed, (CliInspectPlace){tok->from.line, tok->from.col + 1u},
                                  (CliInspectPlace){tok->to.line, (tok->to.col > 0u) ? (tok->to.col - 1u) : 0u});
        }
    }
    cli_inspect_set_cleared(&s_classes);
    for (unsigned int at = 1u; at < g_cli_inspect_toks; at += 1u)
    {
        if (cli_js_is(src, at - 1u, "class") && (g_cli_inspect_tok[at].kind == CLI_JS_NAME))
        {
            cli_inspect_set_put(&s_classes, src + g_cli_inspect_tok[at].start,
                                g_cli_inspect_tok[at].end - g_cli_inspect_tok[at].start);
        }
    }
    parse_scopes(src);
    static const char *const s_delimiters[10] = {"(", ")", "[", "]", "{", "}", ",", ";", ".", "?."};
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        const CliInspectTok *const tok = &g_cli_inspect_tok[at];
        unsigned char class_of = CLI_INSPECT_NONE;
        if ((tok->kind == CLI_JS_STR) || (tok->kind == CLI_JS_REGEX))
        {
            continue;
        }
        if (tok->kind == CLI_JS_NUMBER)
        {
            class_of = CLI_INSPECT_NUMBER;
        }
        else if (tok->kind == CLI_JS_PUNCT)
        {
            const unsigned int close = g_cli_js_pair[at];
            const int opens = cli_js_is(src, at, "(") || cli_js_is(src, at, "[") || cli_js_is(src, at, "{");
            if ((close != CLI_JS_NO) && opens && (close > at))
            {
                const CliInspectTok *const closed = &g_cli_inspect_tok[close];
                cli_inspect_range_put(parsed, tok->to, closed->from);
                cli_inspect_range_put(parsed, tok->from, closed->to);
                cli_inspect_fold_put(parsed, tok->from.line, closed->to.line);
            }
            class_of = cli_js_one_of(src, at, s_delimiters, 10u) ? CLI_INSPECT_DELIMITER : CLI_INSPECT_OPERATOR;
        }
        else
        {
            class_of = parse_name_class(src, at);
        }
        cli_inspect_paint(tok->from, tok->to, class_of);
    }
    // a function declared, or a class, from its first word to its closing brace
    static const char *const s_stops[6] = {";", "{", "}", ",", "(", "="};
    for (unsigned int open = 0u; open < g_cli_inspect_toks; open += 1u)
    {
        const int declared = (g_cli_js_brace[open] == CLI_JS_FUNCTION) || (g_cli_js_brace[open] == CLI_JS_CLASS);
        const unsigned int close = g_cli_js_pair[open];
        if (!declared || (close == CLI_JS_NO))
        {
            continue;
        }
        unsigned int first = open;
        while ((first > 0u) && !cli_js_one_of(src, first - 1u, s_stops, 6u) &&
               (g_cli_js_within[first - 1u] == g_cli_js_within[open]))
        {
            first -= 1u;
        }
        cli_inspect_range_put(parsed, g_cli_inspect_tok[first].from, g_cli_inspect_tok[close].to);
    }
    cli_inspect_paint_runs(parsed);
    cli_inspect_folds_done(parsed);
    parsed->full = parsed->full || g_cli_inspect_over;
}
