// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_classes.c: the classes a Python file declares at its top level: each one's name, the bases it names,
// what its body and its methods set, and the attributes its methods read of `self`
#include "../text/text.h"
#include "inspect_py.h"

// the most attributes one class sets and reads that are recorded, and the bytes of the base being read
#define CLI_PY_ATTRIBUTES 16384u
#define CLI_PY_BASE (4u * CLI_NAME_MAX_LEN)

// the operators that set a name to a value made from its own
static const char *const s_augmented[13] = {
    "+=", "-=", "*=", "/=", "//=", "%=", "**=", "|=", "&=", "^=", "@=", "<<=", ">>="};

// the attributes a class sets, each `size` bytes at `at`, and the tokens of those it reads
static const char *s_has[CLI_PY_ATTRIBUTES];
static unsigned int s_has_size[CLI_PY_ATTRIBUTES];
static unsigned int s_has_count;
static const CliInspectTok *s_reads[CLI_PY_ATTRIBUTES];
static unsigned int s_read_count;

typedef struct
{
    const char *src;
    const CliInspectTok *tok;
    const char *at;
    unsigned int size;
    const CliInspectTok *toks;
    unsigned int count;
    unsigned int offset;
    const CliPyLine *line;
    unsigned int name_at;
    CliFacts *facts;
    const CliPyLine *inner;
    int *dynamic;
} InspectPyClassesCtx;
// whether a token is one of the operators that set a name from its own
EMBED_INLINE int classes_augmented(const InspectPyClassesCtx *args)
{
    const char *src = args->src;
    const CliInspectTok *tok = args->tok;
    for (unsigned int at = 0u; at < 13u; at += 1u)
    {
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                       .word = s_augmented[at]))
        {
            return 1;
        }
    }
    return 0;
}

// an attribute the class sets recorded, once
EMBED_INLINE void classes_has(const InspectPyClassesCtx *args)
{
    for (unsigned int index = 0u; index < s_has_count; index += 1u)
    {
        if ((s_has_size[index] == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_has[index], .other = args->at, .bytes = args->size}) == 0))
        {
            return;
        }
    }
    if (s_has_count < CLI_PY_ATTRIBUTES)
    {
        s_has[s_has_count] = args->at;
        s_has_size[s_has_count] = args->size;
        s_has_count += 1u;
    }
}

// whether `self.name` at `offset` of a line's tokens stands before the line's assignment, as one of its targets:
// before its last `=` outside brackets, as each of a chain's targets does, or before an operator that sets it from
// itself, or before the colon of its annotation
EMBED_INLINE int classes_setting_target(const InspectPyClassesCtx *args)
{
    const char *src = args->src;
    int depth = 0;
    int has_last = 0;
    unsigned int last = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const CliInspectTok *const tok = &args->toks[at];
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "(") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "[") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "{"))
        {
            depth += 1;
        }
        else if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ")") ||
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "]") ||
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "}"))
        {
            depth -= 1;
        }
        else if ((depth == 0) &&
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "="))
        {
            has_last = 1;
            last = at;
        }
        else if ((depth == 0) && EMBED_CALL(classes_augmented, InspectPyClassesCtx, .src = src, .tok = tok) &&
                 !has_last)
        {
            return args->offset < at;
        }
        else if ((depth == 0) &&
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ":") &&
                 (at == args->offset + 3u))
        {
            return 1;
        }
    }
    return has_last && (args->offset < last);
}

// the bases a class's header names after its name at `name_at`, put on the facts' names
EMBED_INLINE void classes_bases(const InspectPyClassesCtx *args)
{
    const char *src = args->src;
    const CliPyLine *line = args->line;
    CliFacts *facts = args->facts;
    if ((args->name_at + 1u >= line->count) ||
        !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                    .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = args->name_at + 1u),
                    .word = "("))
    {
        return;
    }
    int depth = 0;
    char current[CLI_PY_BASE];
    VerbaLinea base = {current, sizeof(current), 0ull};
    int keyword = 0;
    for (unsigned int offset = args->name_at + 2u; offset < line->count; offset += 1u)
    {
        const CliInspectTok *const tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset);
        const int closes =
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ")") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "]") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "}");
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "(") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "[") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "{"))
        {
            depth += 1;
        }
        else if (closes && (depth > 0))
        {
            depth -= 1;
        }
        else if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ")") ||
                 ((depth == 0) &&
                  EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ",")))
        {
            const unsigned long long cut =
                (memor.chr(&(MemoriaCfg){.src = current, .val = '[', .bytes = (size_t)base.at}) != NULL)
                    ? (unsigned long long)((const char *)memor.chr(
                                               &(MemoriaCfg){.src = current, .val = '[', .bytes = (size_t)base.at}) -
                                           current)
                    : base.at;
            if (!keyword && (cut > 0u) && mmgr_line_ok(&base))
            {
                (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = facts,
                                 .name = current, .size = cut, .alias = NULL, .alias_size = 0ull, .from = tok->from,
                                 .to = tok->from);
            }
            if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ")"))
            {
                return;
            }
            base.at = 0ull;
            keyword = 0;
            continue;
        }
        else if ((depth == 0) &&
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "="))
        {
            keyword = 1;
        }
        if ((depth == 0) &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "(") &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ")") &&
            (tok->kind != CLI_PY_STR))
        {
            mmgr_line_put_n(&base, src + tok->start, tok->end - tok->start);
        }
        else if ((depth > 0) &&
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "["))
        {
            mmgr_line_ch(&base, '[');
        }
    }
}

// what a line of a class's body at its own indent sets: a method's or a class's name, or each target of an
// assignment
EMBED_INLINE void classes_body_line(const InspectPyClassesCtx *args)
{
    const char *src = args->src;
    int *dynamic = args->dynamic;
    if (EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src, .line = args->inner,
                   .word = "def") ||
        EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src, .line = args->inner,
                   .word = "class"))
    {
        unsigned int offset = 1u;
        for (unsigned int at = 0u; at < args->inner->count; at += 1u)
        {
            if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                           .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->inner, .at = at),
                           .word = "def") ||
                EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                           .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->inner, .at = at),
                           .word = "class"))
            {
                offset = at + 1u;
                break;
            }
        }
        if (offset < args->inner->count)
        {
            const CliInspectTok *const name =
                EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->inner, .at = offset);
            EMBED_CALL(classes_has, InspectPyClassesCtx, .at = src + name->start, .size = name->end - name->start);
            *dynamic = *dynamic ||
                       EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = name,
                                  .word = "__getattr__") ||
                       EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = name,
                                  .word = "__getattribute__");
        }
        return;
    }
    if (EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->inner, .at = 0u)->kind != CLI_PY_NAME)
    {
        return;
    }
    for (unsigned int offset = 1u; offset < args->inner->count; offset += 1u)
    {
        const CliInspectTok *const tok =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->inner, .at = offset);
        if (!EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "=") &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ":"))
        {
            continue;
        }
        for (unsigned int target = 0u; target < offset; target += 1u)
        {
            const CliInspectTok *const one =
                EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->inner, .at = target);
            if (!EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = one, .word = ",") &&
                !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = one, .word = "(") &&
                !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = one, .word = ")"))
            {
                EMBED_CALL(classes_has, InspectPyClassesCtx, .at = src + one->start, .size = one->end - one->start);
            }
        }
        return;
    }
}

// what a line of a method sets and reads of `self`
EMBED_INLINE void classes_method_line(const InspectPyClassesCtx *args)
{
    const char *src = args->src;
    int *dynamic = args->dynamic;
    const CliInspectTok *const toks = &g_cli_inspect_tok[args->inner->first];
    const unsigned int count = args->inner->count;
    *dynamic = *dynamic ||
               EMBED_CALL(cli_inspect_py_src_word.py_contains, CliInspectPySrcWordCfg, .src = src, .toks = toks,
                          .count = count, .word = "setattr") ||
               EMBED_CALL(cli_inspect_py_src_word.py_contains, CliInspectPySrcWordCfg, .src = src, .toks = toks,
                          .count = count, .word = "__dict__");
    for (unsigned int offset = 0u; offset + 2u < count; offset += 1u)
    {
        const int attribute = EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                         .tok = &toks[offset], .word = "self") &&
                              EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                         .tok = &toks[offset + 1u], .word = ".") &&
                              (toks[offset + 2u].kind == CLI_PY_NAME) &&
                              ((offset == 0u) || !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                             .src = src, .tok = &toks[offset - 1u], .word = "."));
        if (attribute)
        {
            const CliInspectTok *const name = &toks[offset + 2u];
            const CliInspectTok *const next = (offset + 3u < count) ? &toks[offset + 3u] : NULL;
            const int setter =
                (next != NULL) && (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                              .tok = next, .word = "=") ||
                                   EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                              .tok = next, .word = ",") ||
                                   EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                              .tok = next, .word = ":") ||
                                   EMBED_CALL(classes_augmented, InspectPyClassesCtx, .src = src, .tok = next));
            const int sets = (EMBED_CALL(cli_inspect_py_src_toks.py_depth_at, CliInspectPySrcToksCfg, .src = src,
                                         .toks = toks, .offset = offset) == 0) &&
                             setter &&
                             EMBED_CALL(classes_setting_target, InspectPyClassesCtx, .src = src, .toks = toks,
                                        .count = count, .offset = offset);
            if (sets)
            {
                EMBED_CALL(classes_has, InspectPyClassesCtx, .at = src + name->start, .size = name->end - name->start);
            }
            else if (s_read_count < CLI_PY_ATTRIBUTES)
            {
                s_reads[s_read_count] = name;
                s_read_count += 1u;
            }
        }
        // `getattr(self, "name", ...)` and `hasattr(self, "name")` test whether it is set, and take it as set
        const int reflects = (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                         .tok = &toks[offset], .word = "getattr") ||
                              EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                         .tok = &toks[offset], .word = "hasattr")) &&
                             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                        .tok = &toks[offset + 1u], .word = "(") &&
                             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                        .tok = &toks[offset + 2u], .word = "self") &&
                             (offset + 3u < count) &&
                             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                        .tok = &toks[offset + 3u], .word = ",");
        if (reflects && (offset + 4u < count) && (toks[offset + 4u].kind == CLI_PY_STR))
        {
            const CliInspectTok *const named = &toks[offset + 4u];
            unsigned int start = named->start;
            unsigned int end = named->end;
            start = (unsigned int)EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = src,
                                             .at = start, .size = end, .set = "'\"");
            end = (unsigned int)(start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                                    .text = src + start, .at = end - start, .set = "'\""));
            EMBED_CALL(classes_has, InspectPyClassesCtx, .at = src + start, .size = end - start);
        }
    }
}

void cli_py_classes(const CliInspectPySrcSizeCfg *args)
{
    CliFacts *facts = args->facts;
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if ((line->indent != 0u) ||
            !EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = line,
                        .word = "class") ||
            !EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = line))
        {
            continue;
        }
        unsigned int name_at = line->count;
        for (unsigned int offset = 0u; (name_at == line->count) && (offset < line->count); offset += 1u)
        {
            name_at = EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                                 .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset),
                                 .word = "class")
                          ? (offset + 1u)
                          : name_at;
        }
        if (name_at >= line->count)
        {
            continue;
        }
        const unsigned int base_first = facts->names;
        EMBED_CALL(classes_bases, InspectPyClassesCtx, .src = args->src, .line = line, .name_at = name_at,
                   .facts = facts);
        const unsigned int bases = facts->names - base_first;
        const unsigned int end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
        const unsigned int body_indent = (at + 1u < g_cli_py_lines) ? g_cli_py_line[at + 1u].indent : 0u;
        s_has_count = 0u;
        s_read_count = 0u;
        int dynamic = 0;
        for (unsigned int index = at + 1u; index < end; index += 1u)
        {
            const CliPyLine *const inner = &g_cli_py_line[index];
            if (inner->indent == body_indent)
            {
                EMBED_CALL(classes_body_line, InspectPyClassesCtx, .src = args->src, .inner = inner,
                           .dynamic = &dynamic);
            }
            else
            {
                EMBED_CALL(classes_method_line, InspectPyClassesCtx, .src = args->src, .inner = inner,
                           .dynamic = &dynamic);
            }
        }
        const unsigned int has_first = facts->names;
        for (unsigned int index = 0u; index < s_has_count; index += 1u)
        {
            const CliInspectPosition none = {0u, 0u};
            (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = facts,
                             .name = s_has[index], .size = s_has_size[index], .alias = NULL, .alias_size = 0ull,
                             .from = none, .to = none);
        }
        const unsigned int read_first = facts->names;
        for (unsigned int index = 0u; index < s_read_count; index += 1u)
        {
            const CliInspectTok *const read = s_reads[index];
            (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = facts,
                             .name = args->src + read->start, .size = read->end - read->start, .alias = NULL,
                             .alias_size = 0ull, .from = read->from, .to = read->to);
        }
        if (facts->classes == facts->classes_max)
        {
            facts->full = 1;
            return;
        }
        const CliInspectTok *const name =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = name_at);
        const CliInspectClass class_found = {EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx,
                                                        .facts = facts, .text_text = args->src + name->start,
                                                        .size = name->end - name->start),
                                             name->from,
                                             name->to,
                                             base_first,
                                             bases,
                                             has_first,
                                             read_first - has_first,
                                             read_first,
                                             facts->names - read_first,
                                             dynamic};
        if (!facts->counting)
        {
            facts->class_of[facts->classes] = class_found;
        }
        facts->classes += 1u;
    }
}
