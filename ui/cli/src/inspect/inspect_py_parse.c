// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_parse.c: the parse of a Python file for the editor: each column's class, a name's by what it is; the
// regions that fold; and the spans Expand Selection steps through, its strings, its brackets, its lines and its
// blocks, a decorated function or class with its decorators
#include "../text/text.h"
#include "inspect_py.h"

// the max open brackets a line contains, and the max functions open around a line
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

void cli_inspect_py_parse_layout(const CliInspectPartitionCtx *args)
{
    EMBED_CALL(cli_inspect_partition_set_internal.set_layout, CliInspectPartitionSetCtx, .set = &s_classes,
               .partition = args->partition);
    s_params =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)(CLI_INSPECT_TOKENS / 4u) * sizeof(unsigned int));
}
static unsigned int s_stack[CLI_PY_STACK];

typedef struct
{
    const char *src;
    const CliInspectTok *tok;
    const char *const *words;
    unsigned int count;
    unsigned int at;
    unsigned int index;
    const CliPyLine *line;
    unsigned int offset;
    int depth;
    int decorated;
    int importing;
    unsigned int imported_at;
    CliParsed *parsed;
    int *depth_pointer;
    unsigned int *stack;
} InspectPyParseCtx;
// whether a token is one of `count` words
EMBED_INLINE int parse_one_get(const InspectPyParseCtx *args)
{
    const char *src = args->src;
    const CliInspectTok *tok = args->tok;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                       .word_text = args->words[at]))
        {
            return 1;
        }
    }
    return 0;
}

// the names the header of the function at `at` declares as its parameters, put on s_params from their scope's first
EMBED_INLINE void parse_parameters(const InspectPyParseCtx *args)
{
    const char *src = args->src;
    unsigned int at = args->at;
    const CliPyLine *const line = &g_cli_py_line[at];
    static const char *const s_before[4] = {"(", ",", "*", "**"};
    static const char *const s_after[4] = {":", "=", ",", ")"};
    int depth = 0;
    for (unsigned int offset = 0u; offset < line->count; offset += 1u)
    {
        const CliInspectTok *const tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset);
        depth +=
            (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                        .word_text = "(") ||
             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                        .word_text = "[") ||
             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "{"))
                ? 1
                : 0;
        depth -=
            (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                        .word_text = ")") ||
             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                        .word_text = "]") ||
             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "}"))
                ? 1
                : 0;
        const int before = (offset > 0u) && EMBED_CALL(parse_one_get, InspectPyParseCtx, .src = src,
                                                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg,
                                                                         .line = line, .at = offset - 1u),
                                                       .words = s_before, .count = 4u);
        const int after =
            (offset + 1u < line->count) &&
            EMBED_CALL(parse_one_get, InspectPyParseCtx, .src = src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset + 1u),
                       .words = s_after, .count = 4u);
        if ((tok->type == CLI_PY_NAME) && (depth == 1) && before && after && (s_param_count < CLI_INSPECT_TOKENS / 4u))
        {
            s_params[s_param_count] = line->first + offset;
            s_param_count += 1u;
        }
    }
}

// whether a name is a parameter of a function open around the line being painted
EMBED_INLINE int parse_parameter(const InspectPyParseCtx *args)
{
    const char *src = args->src;
    for (unsigned int scope = 0u; scope < s_scopes; scope += 1u)
    {
        for (unsigned int at = 0u; at < s_scope_count[scope]; at += 1u)
        {
            const CliInspectTok *const param = &g_cli_inspect_tok[s_params[s_scope_first[scope] + at]];
            if ((param->end - param->start == args->tok->end - args->tok->start) &&
                (memor.cmp(&(MemoriaCfg){.src = src + param->start,
                                         .other = src + args->tok->start,
                                         .bytes = args->tok->end - args->tok->start}) == 0))
            {
                return 1;
            }
        }
    }
    return 0;
}

// the functions open around line `index`: those whose blocks end at it let go, and its own taken on
EMBED_INLINE void parse_scopes(const InspectPyParseCtx *args)
{
    const char *src = args->src;
    while ((s_scopes > 0u) && (s_scope_end[s_scopes - 1u] <= args->index))
    {
        s_param_count = s_scope_first[s_scopes - 1u];
        s_scopes -= 1u;
    }
    if (EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src,
                   .line = &g_cli_py_line[args->index], .word_text = "def") &&
        (s_scopes < CLI_PY_STACK))
    {
        s_scope_at[s_scopes] = args->index;
        s_scope_end[s_scopes] = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = args->index);
        s_scope_first[s_scopes] = s_param_count;
        EMBED_CALL(parse_parameters, InspectPyParseCtx, .src = src, .at = args->index);
        s_scope_count[s_scopes] = s_param_count - s_scope_first[s_scopes];
        s_scopes += 1u;
    }
}

// the class of a name at `offset` of a line, by what it is
EMBED_INLINE unsigned char parse_name_class(const InspectPyParseCtx *args)
{
    const char *src = args->src;
    const CliPyLine *line = args->line;
    const CliInspectTok *const tok =
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = args->offset);
    const CliInspectTok *const before =
        (args->offset > 0u) ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = args->offset - 1u)
                            : NULL;
    const CliInspectTok *const after =
        (args->offset + 1u < line->count)
            ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = args->offset + 1u)
            : NULL;
    static const char *const s_predefined[5] = {"True", "False", "None", "self", "cls"};
    const int called = (after != NULL) && EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                     .tok = after, .word_text = "(");
    const int is_class = EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_classes,
                                    .at = src + tok->start, .size_u32 = tok->end - tok->start);
    if (EMBED_CALL(parse_one_get, InspectPyParseCtx, .src = src, .tok = tok, .words = s_predefined, .count = 5u))
    {
        return CLI_INSPECT_PREDEFINED;
    }
    if (EMBED_CALL(cli_inspect_py_src_tok.py_keyword, CliInspectPySrcTokCfg, .src = src, .tok = tok))
    {
        return CLI_INSPECT_KEYWORD;
    }
    if (args->decorated && (args->depth == 0))
    {
        return CLI_INSPECT_ATTRIBUTE;
    }
    if (args->importing &&
        (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                    .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u),
                    .word_text = "import") ||
         (args->offset < args->imported_at)))
    {
        return CLI_INSPECT_MODULE;
    }
    if ((before != NULL) &&
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before, .word_text = "def"))
    {
        return CLI_INSPECT_FUNCTION;
    }
    if ((before != NULL) && EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before,
                                       .word_text = "class"))
    {
        return CLI_INSPECT_TYPE;
    }
    if ((before != NULL) &&
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before, .word_text = "."))
    {
        return called ? CLI_INSPECT_FUNCTION : CLI_INSPECT_PROPERTY;
    }
    if (called)
    {
        return is_class ? CLI_INSPECT_TYPE : CLI_INSPECT_FUNCTION;
    }
    const int keyword_argument =
        (after != NULL) &&
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = after, .word_text = "=") &&
        (args->depth > 0) && (before != NULL) &&
        (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before, .word_text = "(") ||
         EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before, .word_text = ","));
    if (keyword_argument || EMBED_CALL(parse_parameter, InspectPyParseCtx, .src = src, .tok = tok))
    {
        return CLI_INSPECT_PARAMETER;
    }
    return is_class ? CLI_INSPECT_TYPE : CLI_INSPECT_NONE;
}

// a string's span and the span inside its quotes put on the parse, with its fold
EMBED_INLINE void parse_string(const InspectPyParseCtx *args)
{
    const char *src = args->src;
    CliParsed *parsed = args->parsed;
    const char *const whole = src + args->tok->start;
    const unsigned int length = args->tok->end - args->tok->start;
    unsigned int open = 0u;
    open = (unsigned int)EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = whole, .at = open,
                                    .size = length, .set = "\"'");
    open = (open == length) ? 0u : open;
    const int triple = (length - open >= 3u) &&
                       ((memor.cmp(&(MemoriaCfg){.src = whole + open, .other = "\"\"\"", .bytes = 3u}) == 0) ||
                        (memor.cmp(&(MemoriaCfg){.src = whole + open, .other = "'''", .bytes = 3u}) == 0));
    const unsigned int quotes = triple ? 3u : 1u;
    EMBED_CALL(cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = parsed,
               .from = args->tok->from, .to = args->tok->to);
    if (length >= open + (quotes * 2u))
    {
        const unsigned int to_col = (args->tok->to.col > quotes) ? (args->tok->to.col - quotes) : 0u;
        EMBED_CALL(cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = parsed,
                   .from = (CliInspectPosition){args->tok->from.line, args->tok->from.col + open + quotes},
                   .to = (CliInspectPosition){args->tok->to.line, to_col});
    }
    EMBED_CALL(cli_inspect_parsed_internal.fold_append, CliInspectParsedCtx, .parsed = parsed,
               .first = args->tok->from.line, .last = args->tok->to.line);
}

// a bracket at `tok`, the `offset`th token of `line`, counted into `*depth_pointer` and onto s_stack at `*stack`: an
// opening one pushed, and a closing one popping the one it closes and putting the pair's spans and fold on the parse
EMBED_INLINE void parse_bracket(const InspectPyParseCtx *args)
{
    const char *const src = args->src;
    const CliInspectTok *const tok = args->tok;
    if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "(") ||
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "[") ||
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "{"))
    {
        *args->depth_pointer += 1;
        if (*args->stack < CLI_PY_STACK)
        {
            s_stack[*args->stack] = args->offset;
            *args->stack += 1u;
        }
        return;
    }
    if (!EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = ")") &&
        !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "]") &&
        !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "}"))
    {
        return;
    }
    *args->depth_pointer -= 1;
    if (*args->stack == 0u)
    {
        return;
    }
    *args->stack -= 1u;
    const CliInspectTok *const opened =
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->line, .at = s_stack[*args->stack]);
    EMBED_CALL(cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = args->parsed,
               .from = opened->to, .to = tok->from);
    EMBED_CALL(cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = args->parsed,
               .from = opened->from, .to = tok->to);
    EMBED_CALL(cli_inspect_parsed_internal.fold_append, CliInspectParsedCtx, .parsed = args->parsed,
               .first = opened->from.line, .last = tok->to.line);
}

// the class of the symbol `tok`, the `offset`th token of its line, at `depth`: the head of a `decorated` line or a dot
// in it outside brackets an attribute, a delimiter, else an operator
EMBED_INLINE unsigned char parse_symbol_class(const InspectPyParseCtx *args)
{
    if (args->decorated && (args->depth == 0) &&
        ((args->offset == 0u) || EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                                            .tok = args->tok, .word_text = ".")))
    {
        return CLI_INSPECT_ATTRIBUTE;
    }
    if (EMBED_CALL(parse_one_get, InspectPyParseCtx, .src = args->src, .tok = args->tok, .words = s_delimiters,
                   .count = 9u))
    {
        return CLI_INSPECT_DELIMITER;
    }
    return CLI_INSPECT_OPERATOR;
}

// where the logical line `index` opens a block with lines in it, the block's spans and fold put on the parse, and the
// span of a decorated function or class with its decorators
EMBED_INLINE void parse_block_fold(const InspectPyParseCtx *args)
{
    const char *const src = args->src;
    CliParsed *const parsed = args->parsed;
    const CliPyLine *const line = &g_cli_py_line[args->index];
    const CliInspectTok *const head = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u);
    if (!EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = src, .line = line))
    {
        return;
    }
    const unsigned int end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = args->index);
    if (end <= args->index + 1u)
    {
        return;
    }
    const CliPyLine *const closing = &g_cli_py_line[end - 1u];
    const CliInspectPosition close =
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = closing, .at = closing->count - 1u)->to;
    EMBED_CALL(cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = parsed,
               .from = head->from, .to = close);
    EMBED_CALL(
        cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = parsed,
        .from = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = &g_cli_py_line[args->index + 1u], .at = 0u)
                    ->from,
        .to = close);
    EMBED_CALL(cli_inspect_parsed_internal.fold_append, CliInspectParsedCtx, .parsed = parsed, .first = head->from.line,
               .last = close.line);
    // a decorated function or class, with its decorators
    unsigned int first = args->index;
    while ((first > 0u) && (g_cli_py_line[first - 1u].indent == line->indent) &&
           EMBED_CALL(
               cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
               .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = &g_cli_py_line[first - 1u], .at = 0u),
               .word_text = "@"))
    {
        first -= 1u;
    }
    if (first < args->index)
    {
        EMBED_CALL(
            cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = parsed,
            .from = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = &g_cli_py_line[first], .at = 0u)->from,
            .to = close);
    }
}

// one logical line painted, its brackets' and strings' spans put on the parse
EMBED_INLINE void parse_line(const InspectPyParseCtx *args)
{
    const char *src = args->src;
    CliParsed *parsed = args->parsed;
    const CliPyLine *const line = &g_cli_py_line[args->index];
    const CliInspectTok *const head = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u);
    const int importing =
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = head,
                   .word_text = "import") ||
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = head, .word_text = "from");
    unsigned int imported_at = line->count;
    for (unsigned int at = 0u; (imported_at == line->count) && (at < line->count); at += 1u)
    {
        imported_at = EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                 .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = at),
                                 .word_text = "import")
                          ? at
                          : imported_at;
    }
    const int decorated =
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = head, .word_text = "@");
    int depth = 0;
    unsigned int stack = 0u;
    for (unsigned int offset = 0u; offset < line->count; offset += 1u)
    {
        const CliInspectTok *const tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset);
        if ((tok->type == CLI_PY_NAME) && tok->field)
        {
            continue;
        }
        unsigned char class_of = CLI_INSPECT_NONE;
        if (tok->type == CLI_PY_STR)
        {
            class_of = CLI_INSPECT_STRING;
        }
        else if (tok->type == CLI_PY_NUMBER)
        {
            class_of = CLI_INSPECT_NUMBER;
        }
        else if (tok->type == CLI_PY_NAME)
        {
            class_of =
                EMBED_CALL(parse_name_class, InspectPyParseCtx, .src = src, .line = line, .offset = offset,
                           .depth = depth, .decorated = decorated, .importing = importing, .imported_at = imported_at);
        }
        else
        {
            EMBED_CALL(parse_bracket, InspectPyParseCtx, .src = src, .tok = tok, .line = line, .offset = offset,
                       .parsed = parsed, .depth_pointer = &depth, .stack = &stack);
            class_of = EMBED_CALL(parse_symbol_class, InspectPyParseCtx, .src = src, .tok = tok, .offset = offset,
                                  .depth = depth, .decorated = decorated);
        }
        EMBED_CALL(cli_inspect_from_to_internal.paint, CliInspectFromToCtx, .from = tok->from, .to = tok->to,
                   .class_of = class_of);
        if (tok->type == CLI_PY_STR)
        {
            EMBED_CALL(parse_string, InspectPyParseCtx, .src = src, .tok = tok, .parsed = parsed);
        }
    }
    const CliInspectPosition last =
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = line->count - 1u)->to;
    EMBED_CALL(cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = parsed,
               .from = head->from, .to = last);
    EMBED_CALL(cli_inspect_parsed_internal.fold_append, CliInspectParsedCtx, .parsed = parsed, .first = head->from.line,
               .last = last.line);
    EMBED_CALL(parse_block_fold, InspectPyParseCtx, .src = src, .index = args->index, .parsed = parsed);
}

void cli_inspect_py_parse(const CliInspectSizeSrcParsedCtx *args)
{
    args->parsed->runs = 0u;
    args->parsed->lines = 0u;
    args->parsed->folds = 0u;
    args->parsed->ranges = 0u;
    args->parsed->full = 0;
    EMBED_CALL(cli_inspect_py_src_size.py_read, CliInspectPySrcSizeCfg, .src = args->src, .size = args->size);
    cli_inspect_size_src_parsed_internal.paint_begin(args);
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        EMBED_CALL(cli_inspect_from_to_internal.paint, CliInspectFromToCtx, .from = g_cli_inspect_comment[at].from,
                   .to = g_cli_inspect_comment[at].to, .class_of = CLI_INSPECT_COMMENT);
    }
    EMBED_CALL(cli_inspect_partition_set_internal.set_clear, CliInspectPartitionSetCtx, .set = &s_classes);
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if (!EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = line,
                        .word_text = "class"))
        {
            continue;
        }
        for (unsigned int offset = 0u; offset + 1u < line->count; offset += 1u)
        {
            if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                           .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset),
                           .word_text = "class"))
            {
                const CliInspectTok *const name =
                    EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset + 1u);
                EMBED_CALL(cli_inspect_set_internal.set_append, CliInspectSetCtx, .set = &s_classes,
                           .at = args->src + name->start, .size_u32 = name->end - name->start);
                break;
            }
        }
    }
    s_scopes = 0u;
    s_param_count = 0u;
    for (unsigned int index = 0u; index < g_cli_py_lines; index += 1u)
    {
        EMBED_CALL(parse_scopes, InspectPyParseCtx, .src = args->src, .index = index);
        EMBED_CALL(parse_line, InspectPyParseCtx, .src = args->src, .index = index, .parsed = args->parsed);
    }
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        EMBED_CALL(cli_inspect_from_to_parsed_internal.range_append, CliInspectFromToParsedCtx, .parsed = args->parsed,
                   .from = g_cli_inspect_comment[at].from, .to = g_cli_inspect_comment[at].to);
    }
    cli_inspect_size_src_parsed_internal.paint_runs(args);
    cli_inspect_size_src_parsed_internal.folds_finish(args);
    args->parsed->full = args->parsed->full || g_cli_inspect_over;
}
