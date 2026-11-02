// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_parse.c: the parse of a JavaScript file for the editor: each column's class, a name's by what it is; the
// regions that fold; and the spans Expand Selection steps through, its comments, strings, brackets, and each function
// declared and each class from its first word to its closing brace
#include "inspect_js.h"

// the most functions a parse records the parameters of, and the most parameters it records
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

void cli_inspect_js_parse_layout(const CliInspectCarveCtx *args)
{
    EMBED_CALL(cli_inspect_carve_set_internal.set_layout, CliInspectCarveSetCtx, .set = &s_classes,
               .carve = args->carve);
    EMBED_CALL(cli_inspect_carve_set_internal.set_layout, CliInspectCarveSetCtx, .set = &s_names, .carve = args->carve);
    EMBED_CALL(cli_inspect_carve_set_internal.set_layout, CliInspectCarveSetCtx, .set = &s_scope_names,
               .carve = args->carve);
    const unsigned long long scopes = CLI_JS_SCOPES;
    s_scope_from = EMBED_CALL(cli_inspect_carve_internal.carve, CliInspectCarveCtx, .carve = args->carve,
                              .bytes = scopes * sizeof(unsigned int));
    s_scope_to = EMBED_CALL(cli_inspect_carve_internal.carve, CliInspectCarveCtx, .carve = args->carve,
                            .bytes = scopes * sizeof(unsigned int));
    s_scope_first = EMBED_CALL(cli_inspect_carve_internal.carve, CliInspectCarveCtx, .carve = args->carve,
                               .bytes = scopes * sizeof(unsigned int));
    s_scope_count = EMBED_CALL(cli_inspect_carve_internal.carve, CliInspectCarveCtx, .carve = args->carve,
                               .bytes = scopes * sizeof(unsigned int));
    s_params = EMBED_CALL(cli_inspect_carve_internal.carve, CliInspectCarveCtx, .carve = args->carve,
                          .bytes = (unsigned long long)CLI_JS_PARAMS * sizeof(unsigned int));
}

unsigned int cli_js_parameters(const CliInspectJsOpenCfg *args)
{
    if (args->open == 0u)
    {
        return args->open;
    }
    const unsigned int before = args->open - 1u;
    unsigned int first = 0u;
    unsigned int last = 0u;
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before, .word = ")"))
    {
        first = (g_cli_js_pair[before] != CLI_JS_NO) ? g_cli_js_pair[before] : before;
        last = before;
    }
    else if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before,
                        .word = "=>"))
    {
        if (before == 0u)
        {
            first = before;
            last = before;
        }
        else if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before - 1u,
                            .word = ")"))
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
        return args->open;
    }
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before, .word = "=>") &&
        (last == first + 1u) && (g_cli_inspect_tok[first].kind == CLI_JS_NAME))
    {
        EMBED_CALL(cli_inspect_set_internal.set_put, CliInspectSetCtx, .set = args->names,
                   .at = args->src + g_cli_inspect_tok[first].start,
                   .size_u32 = g_cli_inspect_tok[first].end - g_cli_inspect_tok[first].start);
        return first;
    }
    static const char *const s_before[3] = {"(", ",", "..."};
    static const char *const s_after[3] = {",", ")", "="};
    for (unsigned int at = first + 1u; at < last; at += 1u)
    {
        if ((g_cli_inspect_tok[at].kind == CLI_JS_NAME) &&
            (EMBED_CALL(cli_inspect_js_from.js_pair_depth, CliInspectJsFromCfg, .src = args->src, .from = first + 1u,
                        .to = at) == 0) &&
            EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = args->src, .at = at - 1u,
                       .words = s_before, .count = 3u) &&
            EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = args->src, .at = at + 1u,
                       .words = s_after, .count = 3u))
        {
            EMBED_CALL(cli_inspect_set_internal.set_put, CliInspectSetCtx, .set = args->names,
                       .at = args->src + g_cli_inspect_tok[at].start,
                       .size_u32 = g_cli_inspect_tok[at].end - g_cli_inspect_tok[at].start);
        }
    }
    return first;
}

typedef struct
{
    const char *src;
    unsigned int at;
} InspectJsParseCtx;
// the functions' parameters recorded: each function's span, and its names among the parameters' tokens
EMBED_INLINE void parse_scopes(const InspectJsParseCtx *args)
{
    const char *src = args->src;
    s_scopes = 0u;
    s_param_count = 0u;
    EMBED_CALL(cli_inspect_carve_set_internal.set_clear, CliInspectCarveSetCtx, .set = &s_names);
    for (unsigned int open = 0u; (open < g_cli_inspect_toks) && (s_scopes < CLI_JS_SCOPES); open += 1u)
    {
        if (g_cli_js_brace[open] != CLI_JS_FUNCTION)
        {
            continue;
        }
        EMBED_CALL(cli_inspect_carve_set_internal.set_clear, CliInspectCarveSetCtx, .set = &s_scope_names);
        const unsigned int first = EMBED_CALL(cli_inspect_js_open.js_parameters, CliInspectJsOpenCfg, .src = src,
                                              .open = open, .names = &s_scope_names);
        s_scope_from[s_scopes] = first;
        s_scope_to[s_scopes] = (g_cli_js_pair[open] != CLI_JS_NO) ? g_cli_js_pair[open] : open;
        s_scope_first[s_scopes] = s_param_count;
        for (unsigned int at = first; at < open; at += 1u)
        {
            const CliInspectTok *const tok = &g_cli_inspect_tok[at];
            if ((tok->kind == CLI_JS_NAME) &&
                EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_scope_names,
                           .at = src + tok->start, .size_u32 = tok->end - tok->start) &&
                (s_param_count < CLI_JS_PARAMS))
            {
                s_params[s_param_count] = at;
                s_param_count += 1u;
                EMBED_CALL(cli_inspect_set_internal.set_put, CliInspectSetCtx, .set = &s_names, .at = src + tok->start,
                           .size_u32 = tok->end - tok->start);
            }
        }
        s_scope_count[s_scopes] = s_param_count - s_scope_first[s_scopes];
        s_scopes += 1u;
    }
}

// whether the name at `at` is a parameter of a function whose span contains it
EMBED_INLINE int parse_parameter(const InspectJsParseCtx *args)
{
    const char *src = args->src;
    unsigned int at = args->at;
    const CliInspectTok *const tok = &g_cli_inspect_tok[at];
    if (!EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_names, .at = src + tok->start,
                    .size_u32 = tok->end - tok->start))
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
EMBED_INLINE unsigned char parse_name_class(const InspectJsParseCtx *args)
{
    const char *src = args->src;
    unsigned int at = args->at;
    const CliInspectTok *const tok = &g_cli_inspect_tok[at];
    const unsigned int before = (at > 0u) ? (at - 1u) : CLI_JS_NO;
    const unsigned int after = (at + 1u < g_cli_inspect_toks) ? (at + 1u) : CLI_JS_NO;
    const unsigned int open = g_cli_js_within[at];
    const unsigned char holder = (open != CLI_JS_NO) ? g_cli_js_brace[open] : CLI_JS_NONE;
    static const char *const s_predefined[6] = {"true", "false", "null", "undefined", "this", "super"};
    static const char *const s_contextual[5] = {"async", "of", "static", "get", "set"};
    static const char *const s_not_after[7] = {"(", "=", ".", ",", ")", ";", ":"};
    static const char *const s_typed[3] = {"class", "new", "extends"};
    const int classed = EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_classes,
                                   .at = src + tok->start, .size_u32 = tok->end - tok->start);
    if (EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = at, .words = s_predefined,
                   .count = 6u))
    {
        return CLI_INSPECT_PREDEFINED;
    }
    const int contextual = EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = at,
                                      .words = s_contextual, .count = 5u) &&
                           (after != CLI_JS_NO) &&
                           !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = after,
                                       .words = s_not_after, .count = 7u);
    if (EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = at, .words = g_cli_js_keywords,
                   .count = 35u) ||
        contextual)
    {
        return CLI_INSPECT_KEYWORD;
    }
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word = "function"))
    {
        return CLI_INSPECT_FUNCTION;
    }
    if (EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = before, .words = s_typed,
                   .count = 3u))
    {
        return CLI_INSPECT_TYPE;
    }
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word = ".") ||
        EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word = "?."))
    {
        return EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = after, .word = "(")
                   ? CLI_INSPECT_FUNCTION
                   : CLI_INSPECT_PROPERTY;
    }
    if ((holder == CLI_JS_OBJECT) &&
        (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word = "{") ||
         EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word = ",")) &&
        EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = after, .word = ":"))
    {
        return CLI_INSPECT_PROPERTY;
    }
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = after, .word = "("))
    {
        return classed ? CLI_INSPECT_TYPE : CLI_INSPECT_FUNCTION;
    }
    if (EMBED_CALL(parse_parameter, InspectJsParseCtx, .src = src, .at = at))
    {
        return CLI_INSPECT_PARAMETER;
    }
    return classed ? CLI_INSPECT_TYPE : CLI_INSPECT_NONE;
}

void cli_inspect_js_parse(const CliInspectSizeSrcParsedCtx *args)
{
    args->parsed->runs = 0u;
    args->parsed->lines = 0u;
    args->parsed->folds = 0u;
    args->parsed->ranges = 0u;
    args->parsed->full = 0;
    EMBED_CALL(cli_inspect_js.js_read, CliInspectJsCfg, .src = args->src, .size = args->size);
    cli_inspect_size_src_parsed_internal.paint_begin(args);
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        const CliInspectSpan comment = g_cli_inspect_comment[at];
        EMBED_CALL(cli_inspect_from_to_internal.paint, CliInspectFromToCtx, .from = comment.from, .to = comment.to,
                   .class_of = CLI_INSPECT_COMMENT);
        EMBED_CALL(cli_inspect_parsed_internal.fold_put, CliInspectParsedCtx, .parsed = args->parsed,
                   .first = comment.from.line, .last = comment.to.line);
        EMBED_CALL(cli_inspect_from_to_parsed_internal.range_put, CliInspectFromToParsedCtx, .parsed = args->parsed,
                   .from = comment.from, .to = comment.to);
    }
    // strings first, as a template's own code is read inside it and drawn over it
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        const CliInspectTok *const tok = &g_cli_inspect_tok[at];
        if ((tok->kind != CLI_JS_STR) && (tok->kind != CLI_JS_REGEX))
        {
            continue;
        }
        EMBED_CALL(cli_inspect_from_to_internal.paint, CliInspectFromToCtx, .from = tok->from, .to = tok->to,
                   .class_of = CLI_INSPECT_STRING);
        EMBED_CALL(cli_inspect_parsed_internal.fold_put, CliInspectParsedCtx, .parsed = args->parsed,
                   .first = tok->from.line, .last = tok->to.line);
        EMBED_CALL(cli_inspect_from_to_parsed_internal.range_put, CliInspectFromToParsedCtx, .parsed = args->parsed,
                   .from = tok->from, .to = tok->to);
        if ((tok->end - tok->start >= 2u) && (tok->kind == CLI_JS_STR))
        {
            EMBED_CALL(cli_inspect_from_to_parsed_internal.range_put, CliInspectFromToParsedCtx, .parsed = args->parsed,
                       .from = (CliInspectPlace){tok->from.line, tok->from.col + 1u},
                       .to = (CliInspectPlace){tok->to.line, (tok->to.col > 0u) ? (tok->to.col - 1u) : 0u});
        }
    }
    EMBED_CALL(cli_inspect_carve_set_internal.set_clear, CliInspectCarveSetCtx, .set = &s_classes);
    for (unsigned int at = 1u; at < g_cli_inspect_toks; at += 1u)
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at - 1u,
                       .word = "class") &&
            (g_cli_inspect_tok[at].kind == CLI_JS_NAME))
        {
            EMBED_CALL(cli_inspect_set_internal.set_put, CliInspectSetCtx, .set = &s_classes,
                       .at = args->src + g_cli_inspect_tok[at].start,
                       .size_u32 = g_cli_inspect_tok[at].end - g_cli_inspect_tok[at].start);
        }
    }
    EMBED_CALL(parse_scopes, InspectJsParseCtx, .src = args->src);
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
            const int opens = EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src,
                                         .at = at, .word = "(") ||
                              EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src,
                                         .at = at, .word = "[") ||
                              EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src,
                                         .at = at, .word = "{");
            if ((close != CLI_JS_NO) && opens && (close > at))
            {
                const CliInspectTok *const closed = &g_cli_inspect_tok[close];
                EMBED_CALL(cli_inspect_from_to_parsed_internal.range_put, CliInspectFromToParsedCtx,
                           .parsed = args->parsed, .from = tok->to, .to = closed->from);
                EMBED_CALL(cli_inspect_from_to_parsed_internal.range_put, CliInspectFromToParsedCtx,
                           .parsed = args->parsed, .from = tok->from, .to = closed->to);
                EMBED_CALL(cli_inspect_parsed_internal.fold_put, CliInspectParsedCtx, .parsed = args->parsed,
                           .first = tok->from.line, .last = closed->to.line);
            }
            class_of = EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = args->src, .at = at,
                                  .words = s_delimiters, .count = 10u)
                           ? CLI_INSPECT_DELIMITER
                           : CLI_INSPECT_OPERATOR;
        }
        else
        {
            class_of = EMBED_CALL(parse_name_class, InspectJsParseCtx, .src = args->src, .at = at);
        }
        EMBED_CALL(cli_inspect_from_to_internal.paint, CliInspectFromToCtx, .from = tok->from, .to = tok->to,
                   .class_of = class_of);
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
        while ((first > 0u) &&
               !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = args->src, .at = first - 1u,
                           .words = s_stops, .count = 6u) &&
               (g_cli_js_within[first - 1u] == g_cli_js_within[open]))
        {
            first -= 1u;
        }
        EMBED_CALL(cli_inspect_from_to_parsed_internal.range_put, CliInspectFromToParsedCtx, .parsed = args->parsed,
                   .from = g_cli_inspect_tok[first].from, .to = g_cli_inspect_tok[close].to);
    }
    cli_inspect_size_src_parsed_internal.paint_runs(args);
    cli_inspect_size_src_parsed_internal.folds_finish(args);
    args->parsed->full = args->parsed->full || g_cli_inspect_over;
}
