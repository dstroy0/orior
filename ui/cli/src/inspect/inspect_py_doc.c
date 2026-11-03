// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_doc.c: code after a return, a raise, a break or a continue in the same block, which no path reaches; and
// the docstrings out of PEP 257's form: three double quotes, a one-line docstring on one line with no space inside its
// quotes, a summary line that ends in a period, a blank line after the summary line where more follows, and closing
// quotes on a line of their own after more than one line
#include "../text/json_tree.h"
#include "../text/text.h"
#include "inspect_py.h"

// A part of a text: where it starts and where it ends
typedef struct
{
    unsigned long long start;
    unsigned long long end;
} DocPart;
// the page of PEP 257, which a docstring's finding links to
static const char s_pep_257[] = "https://peps.python.org/pep-0257/";

typedef struct
{
    const char *src;
    DocPart part;
    unsigned int index;
    DocPart *line;
    CliFacts *facts;
    const CliInspectTok *tok;
    const char *message;
    const char *code;
    const char *title;
    const VerbaLinea *text;
    const char *text_text;
    unsigned long long size;
    DocPart inner;
    DocPart summary;
    const CliPyLine *line_py_line_pointer;
} InspectPyDocCtx;
// a part trimmed of white letters at its two ends, as Rust's str::trim trims it
EMBED_INLINE DocPart doc_trim(const InspectPyDocCtx *args)
{
    DocPart part = args->part;
    while (part.start < part.end)
    {
        unsigned int letter = 0u;
        const unsigned long long reserved = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                       .text_bytes = (const unsigned char *)args->src,
                                                       .length = part.end, .at = part.start, .letter = &letter);
        if (!EMBED_CALL(cli_inspect_internal.white, CliInspectCtx, .letter = letter))
        {
            break;
        }
        part.start += reserved;
    }
    unsigned long long end = part.start;
    for (unsigned long long at = part.start; at < part.end;)
    {
        unsigned int letter = 0u;
        at +=
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)args->src, .length = part.end, .at = at, .letter = &letter);
        end = EMBED_CALL(cli_inspect_internal.white, CliInspectCtx, .letter = letter) ? end : at;
    }
    part.end = end;
    return part;
}

// the line of a part at `index`, as Rust's str::lines splits it: 1 with it into `line`, or 0 where it has none
EMBED_INLINE int doc_line(const InspectPyDocCtx *args)
{
    DocPart *line = args->line;
    unsigned long long at = args->part.start;
    for (unsigned int count = 0u; at < args->part.end; count += 1u)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->src, .at = at, .size = args->part.end);
        unsigned long long end = bounds.stop;
        const unsigned long long next = bounds.next;
        end -= bounds.stop - bounds.cut;
        if (count == args->index)
        {
            *line = (DocPart){at, end};
            return 1;
        }
        at = next;
    }
    return 0;
}

void cli_py_unreachable(const CliInspectPySrcSizeCfg *args)
{
    unsigned long long size = args->size;
    CliFacts *facts = args->facts;
    unsigned int at = 0u;
    while (at < g_cli_py_lines)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        const CliInspectTok *const word = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u);
        const int leaves = EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                                      .tok = word, .word = "return") ||
                           EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                                      .tok = word, .word = "raise") ||
                           EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                                      .tok = word, .word = "break") ||
                           EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                                      .tok = word, .word = "continue");
        if (!leaves || EMBED_CALL(cli_inspect_py_src_word.py_contains, CliInspectPySrcWordCfg, .src = args->src,
                                  .toks = &g_cli_inspect_tok[line->first], .count = line->count, .word = ";"))
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
            const CliInspectPosition last =
                EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = last_line, .at = last_line->count - 1u)->to;
            CliInspectPosition from = {0u, 0u};
            CliInspectPosition to = {0u, 0u};
            EMBED_CALL(cli_inspect_py_src.py_full_line_span, CliInspectPySrcCfg, .src = args->src, .size = size,
                       .from = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = first, .at = 0u)->from.line,
                       .to = last.line, .start = &from, .end = &to);
            char message[CLI_INSPECT_UNREACHED_CAPACITY];
            VerbaLinea msg = {message, sizeof(message), 0ULL};
            mmgr_line_put(&msg, CLI_INSPECT_UNREACHED_BEFORE);
            mmgr_line_put_n(&msg, args->src + word->start, word->end - word->start);
            mmgr_line_put(&msg, CLI_INSPECT_UNREACHED_AFTER);
            EMBED_CALL(cli_inspect_from_internal.finding_append, CliInspectFromCtx, .facts = facts,
                       .from = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = first, .at = 0u)->from,
                       .to = last, .severity = CLI_INSPECT_WARNING, .message = &msg, .code = "unreachable",
                       .href = NULL);
            EMBED_CALL(cli_inspect_facts_title_internal.fix_title, CliInspectFactsTitleCtx, .facts = facts,
                       .title_text = "Take out the code no path reaches");
            EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = from, .to = to,
                       .text_text = "", .size = 0ULL);
        }
        at = end;
    }
}

// a docstring's note put on the facts: its message after `PEP 257: `, its code, and the link to PEP 257
EMBED_INLINE void doc_note(const InspectPyDocCtx *args)
{
    CliFacts *facts = args->facts;
    const char *message = args->message;
    const char *code = args->code;
    char text[256];
    VerbaLinea msg = {text, sizeof(text), 0ULL};
    mmgr_line_put(&msg, "PEP 257: ");
    mmgr_line_put(&msg, message);
    EMBED_CALL(cli_inspect_from_internal.finding_append, CliInspectFromCtx, .facts = facts, .from = args->tok->from,
               .to = args->tok->to, .severity = CLI_INSPECT_NOTE, .message = &msg, .code = code, .href = s_pep_257);
}

// a fix put on the last note that writes the docstring whole again as `text` specifies
EMBED_INLINE void doc_rewrite_fix(const InspectPyDocCtx *args)
{
    CliFacts *facts = args->facts;
    const char *title = args->title;
    const VerbaLinea *text = args->text;
    EMBED_CALL(cli_inspect_facts_title_internal.fix_title, CliInspectFactsTitleCtx, .facts = facts,
               .title_text = title);
    EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = args->tok->from,
               .to = args->tok->to, .text_text = text->out, .size = mmgr_line_ok(text) ? text->at : 0ULL);
}

// the UTF-16 units of `size` bytes of `text`
EMBED_INLINE unsigned int doc_units(const InspectPyDocCtx *args)
{
    const char *text = args->text_text;
    unsigned int units = 0u;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned int letter = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                         .text_bytes = (const unsigned char *)text, .length = args->size, .at = at, .letter = &letter);
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    return units;
}

// the physical line `index` of `size` bytes of `src`, empty where it has none
EMBED_INLINE DocPart doc_physical(const InspectPyDocCtx *args)
{
    const char *src = args->src;
    unsigned long long at = 0ULL;
    for (unsigned int line = 0u; line < args->index; line += 1u)
    {
        const char *const newline =
            (at < args->size)
                ? memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(args->size - at)})
                : NULL;
        if (newline == NULL)
        {
            return (DocPart){args->size, args->size};
        }
        at = (unsigned long long)(newline - src) + 1u;
    }
    const char *const newline =
        (at < args->size) ? memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(args->size - at)})
                          : NULL;
    return (DocPart){at, (newline != NULL) ? (unsigned long long)(newline - src) : args->size};
}

// the summary line of `trimmed`, the first of `inner`, without a period at its end, given its fix where its line is
// found
EMBED_INLINE void doc_period(const InspectPyDocCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliFacts *facts = args->facts;
    unsigned int first_line = 0u;
    DocPart line = {0ULL, 0ULL};
    for (unsigned int index = 0u;
         EMBED_CALL(doc_line, InspectPyDocCtx, .src = src, .part = args->inner, .index = index, .line = &line);
         index += 1u)
    {
        const DocPart trimmed = EMBED_CALL(doc_trim, InspectPyDocCtx, .src = src, .part = line);
        if (trimmed.start < trimmed.end)
        {
            first_line = index;
            break;
        }
    }
    EMBED_CALL(doc_note, InspectPyDocCtx, .facts = facts, .tok = args->tok,
               .message = "a docstring's summary line ends in a period.", .code = "docstring-period");
    const DocPart text =
        EMBED_CALL(doc_physical, InspectPyDocCtx, .src = src, .size = size, .index = args->tok->from.line + first_line);
    const unsigned long long length = args->summary.end - args->summary.start;
    for (unsigned long long at = text.start; at + length <= text.end; at += 1u)
    {
        if (memor.cmp(&(MemoriaCfg){.src = src + at, .other = src + args->summary.start, .bytes = (size_t)length}) == 0)
        {
            const CliInspectPosition place = {args->tok->from.line + first_line,
                                              EMBED_CALL(doc_units, InspectPyDocCtx, .text_text = src + text.start,
                                                         .size = at + length - text.start)};
            EMBED_CALL(cli_inspect_facts_title_internal.fix_title, CliInspectFactsTitleCtx, .facts = facts,
                       .title_text = "End the summary line with a period");
            EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = place,
                       .to = place, .text_text = ".", .size = 1ULL);
            return;
        }
    }
}

// what of PEP 257's form one docstring leaves out, put on the facts
static void doc_check(const InspectPyDocCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliFacts *facts = args->facts;
    const char *const whole = src + args->tok->start;
    const unsigned long long length = args->tok->end - args->tok->start;
    unsigned long long prefix = 0ULL;
    prefix = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = whole, .at = prefix,
                        .size = length, .set = "\"'");
    prefix = (prefix == length) ? 0ULL : prefix;
    for (unsigned long long at = 0ULL; at < prefix; at += 1u)
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
    const DocPart inner = {args->tok->start + prefix + quote, args->tok->start + length - quote};
    static char s_fixed[1u << 20u];
    VerbaLinea fixed = {s_fixed, sizeof(s_fixed), 0ULL};
    if (!triple || (memor.cmp(&(MemoriaCfg){.src = body, .other = "\"\"\"", .bytes = 3u}) != 0))
    {
        EMBED_CALL(doc_note, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                   .message = "a docstring is written in three double quotes.", .code = "docstring-quotes");
        int inside = (inner.end > inner.start) && (src[inner.end - 1u] == '"');
        for (unsigned long long at = inner.start; !inside && (at + 3u <= inner.end); at += 1u)
        {
            inside = (memor.cmp(&(MemoriaCfg){.src = src + at, .other = "\"\"\"", .bytes = 3u}) == 0);
        }
        if (!inside)
        {
            mmgr_line_put_n(&fixed, whole, prefix);
            mmgr_line_put(&fixed, "\"\"\"");
            mmgr_line_put_n(&fixed, src + inner.start, inner.end - inner.start);
            mmgr_line_put(&fixed, "\"\"\"");
            EMBED_CALL(doc_rewrite_fix, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                       .title = "Use three double quotes", .text = &fixed);
        }
    }
    const DocPart trimmed = EMBED_CALL(doc_trim, InspectPyDocCtx, .src = src, .part = inner);
    if (trimmed.start == trimmed.end)
    {
        return;
    }
    DocPart first = {0ULL, 0ULL};
    DocPart second = {0ULL, 0ULL};
    (void)EMBED_CALL(doc_line, InspectPyDocCtx, .src = src, .part = trimmed, .index = 0u, .line = &first);
    const int more = EMBED_CALL(doc_line, InspectPyDocCtx, .src = src, .part = trimmed, .index = 1u, .line = &second);
    if (!more)
    {
        const int broken =
            (memor.chr(&(MemoriaCfg){
                 .src = src + inner.start, .val = '\n', .bytes = (size_t)(inner.end - inner.start)}) != NULL);
        const int spaced = (inner.start != trimmed.start) || (inner.end != trimmed.end);
        fixed.at = 0ULL;
        mmgr_line_put_n(&fixed, whole, prefix);
        mmgr_line_put_n(&fixed, broken ? "\"\"\"" : body, broken ? 3u : quote);
        mmgr_line_put_n(&fixed, src + trimmed.start, trimmed.end - trimmed.start);
        mmgr_line_put_n(&fixed, broken ? "\"\"\"" : body, broken ? 3u : quote);
        if (broken)
        {
            EMBED_CALL(doc_note, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                       .message = "a one-line docstring fits on one line with its quotes.",
                       .code = "docstring-one-line");
            EMBED_CALL(doc_rewrite_fix, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                       .title = "Put the docstring on one line", .text = &fixed);
        }
        else if (spaced)
        {
            EMBED_CALL(doc_note, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                       .message = "a one-line docstring has no space inside its quotes.", .code = "docstring-space");
            EMBED_CALL(doc_rewrite_fix, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                       .title = "Take out the space inside the quotes", .text = &fixed);
        }
    }
    DocPart summary = EMBED_CALL(doc_trim, InspectPyDocCtx, .src = src, .part = first);
    summary.start = first.start;
    if ((summary.end == summary.start) || (src[summary.end - 1u] != '.'))
    {
        EMBED_CALL(doc_period, InspectPyDocCtx, .src = src, .size = size, .tok = args->tok, .inner = inner,
                   .summary = summary, .facts = facts);
    }
    if (!more)
    {
        return;
    }
    const DocPart blank = EMBED_CALL(doc_trim, InspectPyDocCtx, .src = src, .part = second);
    if (blank.start < blank.end)
    {
        EMBED_CALL(doc_note, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                   .message = "a blank line comes between a docstring's summary line and what follows it.",
                   .code = "docstring-blank");
    }
    unsigned long long last = inner.end;
    last = (inner.start + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                     .text = src + inner.start, .at = last - inner.start, .set = "\n"));
    const DocPart closing = EMBED_CALL(doc_trim, InspectPyDocCtx, .src = src, .part = (DocPart){last, inner.end});
    if (closing.start < closing.end)
    {
        const DocPart line =
            EMBED_CALL(doc_physical, InspectPyDocCtx, .src = src, .size = size, .index = args->tok->from.line);
        unsigned long long indent = line.start;
        while (indent < line.end)
        {
            unsigned int letter = 0u;
            const unsigned long long reserved = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                           .text_bytes = (const unsigned char *)src, .length = line.end,
                                                           .at = indent, .letter = &letter);
            if (!EMBED_CALL(cli_inspect_internal.white, CliInspectCtx, .letter = letter))
            {
                break;
            }
            indent += reserved;
        }
        EMBED_CALL(doc_note, InspectPyDocCtx, .facts = facts, .tok = args->tok,
                   .message = "the closing quotes of a docstring of more than one line stand on a line of their own.",
                   .code = "docstring-close");
        const CliInspectPosition close = {args->tok->to.line, args->tok->to.col - (unsigned int)quote};
        fixed.at = 0ULL;
        mmgr_line_ch(&fixed, '\n');
        mmgr_line_put_n(&fixed, src + line.start, indent - line.start);
        EMBED_CALL(cli_inspect_facts_title_internal.fix_title, CliInspectFactsTitleCtx, .facts = facts,
                   .title_text = "Put the closing quotes on a line of their own");
        EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = close, .to = close,
                   .text_text = fixed.out, .size = fixed.at);
    }
}

// whether a line is a docstring alone: one string, or one string with the names its fields read
EMBED_INLINE int doc_lone(const InspectPyDocCtx *args)
{
    const CliInspectTok *const toks = &g_cli_inspect_tok[args->line_py_line_pointer->first];
    if (toks[0].kind != CLI_PY_STR)
    {
        return 0;
    }
    unsigned int strings = 0u;
    for (unsigned int at = 0u; at < args->line_py_line_pointer->count; at += 1u)
    {
        if ((toks[at].kind != CLI_PY_STR) && !toks[at].field)
        {
            return 0;
        }
        strings += (toks[at].kind == CLI_PY_STR) ? 1u : 0u;
    }
    return strings == 1u;
}

void cli_py_docstrings(const CliInspectPySrcSizeCfg *args)
{
    if ((g_cli_py_lines > 0u) && EMBED_CALL(doc_lone, InspectPyDocCtx, .line_py_line_pointer = &g_cli_py_line[0]))
    {
        EMBED_CALL(doc_check, InspectPyDocCtx, .src = args->src, .size = args->size,
                   .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = &g_cli_py_line[0], .at = 0u),
                   .facts = args->facts);
    }
    for (unsigned int at = 0u; at + 1u < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        const CliPyLine *const next = &g_cli_py_line[at + 1u];
        const int declares = EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src,
                                        .line = line, .word = "def") ||
                             EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src,
                                        .line = line, .word = "class");
        if (declares &&
            EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = line) &&
            (next->indent > line->indent) && EMBED_CALL(doc_lone, InspectPyDocCtx, .line_py_line_pointer = next))
        {
            EMBED_CALL(doc_check, InspectPyDocCtx, .src = args->src, .size = args->size,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = next, .at = 0u),
                       .facts = args->facts);
        }
    }
}
