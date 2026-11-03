// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_unread.c: the names a Python function sets and never reads: each name set by an assignment of it alone,
// by `with ... as` or by `except ... as`, directly in the function, and read nowhere in it or in what it contains. A
// name that starts with an underscore, is declared global or nonlocal, or sits in a function that reads its locals
// whole, is left alone
#include "../text/json_tree.h"
#include "inspect_py.h"

// A place a name is set that may go unread: the name's token, and the fix that takes it out and its span
typedef struct
{
    CliInspectTok tok;
    unsigned int fix;
    CliInspectPosition from;
    CliInspectPosition to;
} PySet;
// the most offsets of a statement's tokens that set a name, which are no reads
#define CLI_PY_SKIPS 4096u

// how a set name's fix takes it out: it has none; `as name` taken out; `name =` taken out and its value retained; the
// line that sets it taken out
#define CLI_PY_FIX_NONE 0u
#define CLI_PY_FIX_AS 1u
#define CLI_PY_FIX_VALUE 2u
#define CLI_PY_FIX_LINE 3u

static PySet *s_sets;
static unsigned int s_set_count;
static CliInspectSet s_read;
static CliInspectSet s_declared;
static unsigned int *s_equals;

void cli_inspect_py_unread_layout(const CliInspectPartitionCtx *args)
{
    s_sets = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                        .bytes = (unsigned long long)CLI_INSPECT_LINES * sizeof(PySet));
    EMBED_CALL(cli_inspect_partition_set_internal.set_layout, CliInspectPartitionSetCtx, .set = &s_read,
               .partition = args->partition);
    EMBED_CALL(cli_inspect_partition_set_internal.set_layout, CliInspectPartitionSetCtx, .set = &s_declared,
               .partition = args->partition);
    s_equals =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)(CLI_INSPECT_TOKENS / 8u + 1u) * sizeof(unsigned int));
}
static unsigned int s_skip[CLI_PY_SKIPS];
static unsigned int s_skips;

int cli_py_depth_at(const CliInspectPySrcToksCfg *args)
{
    int depth = 0;
    for (unsigned int at = 0u; at < args->offset; at += 1u)
    {
        depth += (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                             .tok = &args->toks[at], .word_text = "(") ||
                  EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                             .tok = &args->toks[at], .word_text = "[") ||
                  EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                             .tok = &args->toks[at], .word_text = "{"))
                     ? 1
                     : 0;
        depth -= (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                             .tok = &args->toks[at], .word_text = ")") ||
                  EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                             .tok = &args->toks[at], .word_text = "]") ||
                  EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                             .tok = &args->toks[at], .word_text = "}"))
                     ? 1
                     : 0;
    }
    return depth;
}

void cli_py_full_line_span(const CliInspectPySrcCfg *args)
{
    CliInspectPosition *start = args->start;
    CliInspectPosition *end = args->end;
    unsigned int count = 1u;
    unsigned long long last_start = 0ULL;
    unsigned long long last_end = args->size;
    for (unsigned long long at = 0ULL; at < args->size; at += 1u)
    {
        if (args->src[at] == '\n')
        {
            if (count - 1u == args->to)
            {
                last_end = at;
            }
            count += 1u;
            last_start = (count - 1u == args->to) ? (at + 1u) : last_start;
        }
    }
    *start = (CliInspectPosition){args->from, 0u};
    if (args->to + 1u < count)
    {
        *end = (CliInspectPosition){args->to + 1u, 0u};
        return;
    }
    unsigned int units = 0u;
    for (unsigned long long at = last_start; at < last_end;)
    {
        unsigned int letter = 0u;
        at +=
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)args->src, .length = last_end, .at = at, .letter = &letter);
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    *end = (CliInspectPosition){args->to, units};
}

typedef struct
{
    const char *src;
    const CliInspectTok *toks;
    unsigned int count;
    unsigned int offset;
    const CliInspectTok *tok;
    unsigned int fix;
    CliInspectPosition from;
    CliInspectPosition to;
    unsigned long long size;
    int lone;
    const CliPyLine *line;
    const PySet *set;
    CliFacts *facts;
} InspectPyUnreadCtx;
// whether the token at `offset` of `count` is a name read: a name that is no keyword, no attribute after a dot, and no
// keyword argument's name
EMBED_INLINE int unread_is_read(const InspectPyUnreadCtx *args)
{
    const char *src = args->src;
    const CliInspectTok *toks = args->toks;
    unsigned int offset = args->offset;
    const CliInspectTok *const tok = &toks[offset];
    if (tok->kind != CLI_PY_NAME)
    {
        return 0;
    }
    if (tok->field)
    {
        return 1;
    }
    if (EMBED_CALL(cli_inspect_py_src_tok.py_keyword, CliInspectPySrcTokCfg, .src = src, .tok = tok))
    {
        return 0;
    }
    const CliInspectTok *const before = (offset > 0u) ? &toks[offset - 1u] : NULL;
    if ((before != NULL) &&
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before, .word_text = "."))
    {
        return 0;
    }
    const int equals =
        (offset + 1u < args->count) && EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                  .tok = &toks[offset + 1u], .word_text = "=");
    const int opened =
        (before != NULL) &&
        (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before, .word_text = "(") ||
         EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = before, .word_text = ","));
    return !(equals && opened &&
             (EMBED_CALL(cli_inspect_py_src_toks.py_depth_at, CliInspectPySrcToksCfg, .src = src, .toks = toks,
                         .offset = offset) > 0));
}

// the names `count` tokens read put in the set of names read
EMBED_INLINE void unread_reads_get(const InspectPyUnreadCtx *args)
{
    for (unsigned int offset = 0u; offset < args->count; offset += 1u)
    {
        if (EMBED_CALL(unread_is_read, InspectPyUnreadCtx, .src = args->src, .toks = args->toks, .count = args->count,
                       .offset = offset))
        {
            EMBED_CALL(cli_inspect_set_internal.set_append, CliInspectSetCtx, .set = &s_read,
                       .at = args->src + args->toks[offset].start,
                       .size_u32 = args->toks[offset].end - args->toks[offset].start);
        }
    }
}

// a name that may go unread noted
EMBED_INLINE void unread_set(const InspectPyUnreadCtx *args)
{
    if (s_set_count < CLI_INSPECT_LINES)
    {
        s_sets[s_set_count] = (PySet){*args->tok, args->fix, args->from, args->to};
        s_set_count += 1u;
    }
}

// an offset that sets a name, and so is no read, recorded
EMBED_INLINE void unread_skip(const InspectPyUnreadCtx *args)
{
    if (s_skips < CLI_PY_SKIPS)
    {
        s_skip[s_skips] = args->offset;
        s_skips += 1u;
    }
}

// the names `with ... as name` and `except ... as name` set noted
EMBED_INLINE void unread_as(const InspectPyUnreadCtx *args)
{
    const char *src = args->src;
    int depth = 0;
    for (unsigned int offset = 0u; offset < args->count; offset += 1u)
    {
        const CliInspectTok *const tok = &args->toks[offset];
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
        const int named = (offset + 1u < args->count) && (args->toks[offset + 1u].kind == CLI_PY_NAME);
        const int ended =
            (offset + 2u < args->count) && (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                       .tok = &args->toks[offset + 2u], .word_text = ",") ||
                                            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                       .tok = &args->toks[offset + 2u], .word_text = ":"));
        if ((depth == 0) &&
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                       .word_text = "as") &&
            named && ended && (offset > 0u))
        {
            EMBED_CALL(unread_skip, InspectPyUnreadCtx, .offset = offset + 1u);
            EMBED_CALL(unread_set, InspectPyUnreadCtx, .tok = &args->toks[offset + 1u], .fix = CLI_PY_FIX_AS,
                       .from = args->toks[offset - 1u].to, .to = args->toks[offset + 1u].to);
        }
    }
}

// the names a statement of `count` tokens sets that may go unread noted, and the offsets of its tokens that set them,
// which are no reads, recorded in s_skip
EMBED_INLINE void unread_setting(const InspectPyUnreadCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    const CliInspectTok *toks = args->toks;
    unsigned int count = args->count;
    s_skips = 0u;
    const int async_with = EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[0],
                                      .word_text = "async") &&
                           (count > 1u) &&
                           EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[1],
                                      .word_text = "with");
    if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[0],
                   .word_text = "with") ||
        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[0],
                   .word_text = "except") ||
        async_with)
    {
        EMBED_CALL(unread_as, InspectPyUnreadCtx, .src = src, .toks = toks, .count = count);
        return;
    }
    unsigned int equals = 0u;
    int depth = 0;
    for (unsigned int offset = 0u; offset < count; offset += 1u)
    {
        const CliInspectTok *const tok = &toks[offset];
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
        if ((depth == 0) &&
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "=") &&
            (tok->kind == CLI_PY_OP) && (equals < CLI_INSPECT_TOKENS / 8u))
        {
            s_equals[equals] = offset;
            equals += 1u;
        }
    }
    if (equals == 0u)
    {
        return;
    }
    const unsigned int last = s_equals[equals - 1u];
    unsigned int start = 0u;
    for (unsigned int index = 0u; index < equals; index += 1u)
    {
        const unsigned int equal = s_equals[index];
        const unsigned int targets = equal - start;
        const int annotated = (targets > 2u) && (start == 0u) && (count > 1u) &&
                              EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                         .tok = &toks[1], .word_text = ":");
        if (((targets == 1u) || annotated) && (toks[start].kind == CLI_PY_NAME) &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_keyword, CliInspectPySrcTokCfg, .src = src, .tok = &toks[start]))
        {
            EMBED_CALL(unread_skip, InspectPyUnreadCtx, .offset = start);
            unsigned int fix = CLI_PY_FIX_NONE;
            CliInspectPosition from = toks[0].from;
            CliInspectPosition to = toks[0].from;
            if ((equals == 1u) && args->lone && !annotated)
            {
                const int calls = EMBED_CALL(cli_inspect_py_src_word.py_contains, CliInspectPySrcWordCfg, .src = src,
                                             .toks = &toks[last + 1u], .count = count - last - 1u, .word_text = "(");
                fix = calls ? CLI_PY_FIX_VALUE : CLI_PY_FIX_LINE;
                if (calls)
                {
                    to = toks[last + 1u].from;
                }
                else
                {
                    const unsigned int end_line = g_cli_inspect_tok[args->line->first + args->line->count - 1u].to.line;
                    EMBED_CALL(cli_inspect_py_src.py_full_line_span, CliInspectPySrcCfg, .src = src, .size = size,
                               .from = toks[0].from.line, .to = end_line, .start = &from, .end = &to);
                }
            }
            if (last + 1u < count)
            {
                EMBED_CALL(unread_set, InspectPyUnreadCtx, .tok = &toks[start], .fix = fix, .from = from, .to = to);
            }
        }
        else
        {
            int names_only = 1;
            int comma = 0;
            for (unsigned int at = start; at < equal; at += 1u)
            {
                names_only = names_only && ((toks[at].kind == CLI_PY_NAME) ||
                                            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                       .tok = &toks[at], .word_text = ","));
                comma = comma || EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                            .tok = &toks[at], .word_text = ",");
            }
            // a tuple's names are set, and not reported
            for (unsigned int at = start; names_only && comma && (at < equal); at += 1u)
            {
                if (toks[at].kind == CLI_PY_NAME)
                {
                    EMBED_CALL(unread_skip, InspectPyUnreadCtx, .offset = at);
                }
            }
        }
        start = equal + 1u;
    }
}

// whether `offset` is among the offsets that set a name
EMBED_INLINE int unread_on_skip(const InspectPyUnreadCtx *args)
{
    for (unsigned int at = 0u; at < s_skips; at += 1u)
    {
        if (s_skip[at] == args->offset)
        {
            return 1;
        }
    }
    return 0;
}

// each statement of a line read: the names it declares global or nonlocal, those it sets, and those it reads
EMBED_INLINE void unread_line(const InspectPyUnreadCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    const CliInspectTok *const toks = &g_cli_inspect_tok[args->line->first];
    unsigned int start = 0u;
    int depth = 0;
    for (unsigned int at = 0u; at <= args->line->count; at += 1u)
    {
        const int ends = (at == args->line->count) || ((toks[at].kind == CLI_PY_OP) && (depth == 0) &&
                                                       EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                                  .src = src, .tok = &toks[at], .word_text = ";"));
        if ((at < args->line->count) && (toks[at].kind == CLI_PY_OP))
        {
            depth += (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[at],
                                 .word_text = "(") ||
                      EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[at],
                                 .word_text = "[") ||
                      EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[at],
                                 .word_text = "{"))
                         ? 1
                         : 0;
            depth -= (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[at],
                                 .word_text = ")") ||
                      EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[at],
                                 .word_text = "]") ||
                      EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &toks[at],
                                 .word_text = "}"))
                         ? 1
                         : 0;
        }
        if (!ends)
        {
            continue;
        }
        const CliInspectTok *const statement = &toks[start];
        const unsigned int count = at - start;
        start = at + 1u;
        if (count == 0u)
        {
            continue;
        }
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &statement[0],
                       .word_text = "global") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = &statement[0],
                       .word_text = "nonlocal"))
        {
            for (unsigned int name = 1u; name < count; name += 1u)
            {
                if (statement[name].kind == CLI_PY_NAME)
                {
                    EMBED_CALL(cli_inspect_set_internal.set_append, CliInspectSetCtx, .set = &s_declared,
                               .at = src + statement[name].start,
                               .size_u32 = statement[name].end - statement[name].start);
                }
            }
            continue;
        }
        EMBED_CALL(unread_setting, InspectPyUnreadCtx, .src = src, .size = size, .toks = statement, .count = count,
                   .lone = args->line->count == count, .line = args->line);
        for (unsigned int offset = 0u; offset < count; offset += 1u)
        {
            if (!EMBED_CALL(unread_on_skip, InspectPyUnreadCtx, .offset = offset) &&
                EMBED_CALL(unread_is_read, InspectPyUnreadCtx, .src = src, .toks = statement, .count = count,
                           .offset = offset))
            {
                EMBED_CALL(cli_inspect_set_internal.set_append, CliInspectSetCtx, .set = &s_read,
                           .at = src + statement[offset].start,
                           .size_u32 = statement[offset].end - statement[offset].start);
            }
        }
    }
}

// a set name that goes unread reported, with its fix
EMBED_INLINE void unread_report(const InspectPyUnreadCtx *args)
{
    CliFacts *facts = args->facts;
    const char *const name = args->src + args->set->tok.start;
    const unsigned int length = args->set->tok.end - args->set->tok.start;
    char message[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea msg = {message, sizeof(message), 0ULL};
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(&msg, name, length);
    mmgr_line_put(&msg, "` is set and never read.");
    EMBED_CALL(cli_inspect_from_internal.finding_append, CliInspectFromCtx, .facts = facts, .from = args->set->tok.from,
               .to = args->set->tok.to, .severity = CLI_INSPECT_WARNING, .message = &msg, .code = "unread",
               .href = NULL);
    if (args->set->fix == CLI_PY_FIX_NONE)
    {
        return;
    }
    char title[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea titled = {title, sizeof(title), 0ULL};
    mmgr_line_put(&titled, (args->set->fix == CLI_PY_FIX_AS)      ? "Take out `as "
                           : (args->set->fix == CLI_PY_FIX_VALUE) ? "Take out `"
                                                                  : "Take out the line that sets ");
    mmgr_line_put_n(&titled, name, length);
    mmgr_line_put(&titled, (args->set->fix == CLI_PY_FIX_AS)      ? "`"
                           : (args->set->fix == CLI_PY_FIX_VALUE) ? " =` and retain its value"
                                                                  : "");
    EMBED_CALL(cli_inspect_facts_internal.fix_append, CliInspectFactsCtx, .facts = facts, .title = &titled);
    EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = args->set->from,
               .to = args->set->to, .text_text = "", .size = 0ULL);
}

void cli_py_unread(const CliInspectPySrcSizeCfg *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    unsigned int at = args->at;
    const unsigned int end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
    s_set_count = 0u;
    EMBED_CALL(cli_inspect_partition_set_internal.set_clear, CliInspectPartitionSetCtx, .set = &s_read);
    EMBED_CALL(cli_inspect_partition_set_internal.set_clear, CliInspectPartitionSetCtx, .set = &s_declared);
    unsigned int nested_end = 0u;
    for (unsigned int index = at + 1u; index < end; index += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[index];
        const int nested = (index < nested_end);
        if (!nested &&
            (EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src, .line = line,
                        .word_text = "def") ||
             EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src, .line = line,
                        .word_text = "class")) &&
            EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = src, .line = line))
        {
            nested_end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = index);
            EMBED_CALL(unread_reads_get, InspectPyUnreadCtx, .src = src, .toks = &g_cli_inspect_tok[line->first],
                       .count = line->count);
            continue;
        }
        if (nested)
        {
            EMBED_CALL(unread_reads_get, InspectPyUnreadCtx, .src = src, .toks = &g_cli_inspect_tok[line->first],
                       .count = line->count);
            continue;
        }
        EMBED_CALL(unread_line, InspectPyUnreadCtx, .src = src, .size = size, .line = line);
    }
    static const char *const s_coordinates[4] = {"locals", "vars", "eval", "exec"};
    for (unsigned int word_text = 0u; word_text < 4u; word_text += 1u)
    {
        if (EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_read,
                       .at = s_coordinates[word_text],
                       .size_u32 = (unsigned int)cellul.len(
                           &(CatenaFinitaCfg){.src = s_coordinates[word_text], .cap = CLI_CELLS_POOL})))
        {
            return;
        }
    }
    for (unsigned int index = 0u; index < s_set_count; index += 1u)
    {
        const PySet *const set = &s_sets[index];
        const char *const name = src + set->tok.start;
        const unsigned int length = set->tok.end - set->tok.start;
        if ((name[0] == '_') ||
            EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_read, .at = name,
                       .size_u32 = length) ||
            EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_declared, .at = name,
                       .size_u32 = length))
        {
            continue;
        }
        EMBED_CALL(unread_report, InspectPyUnreadCtx, .src = src, .set = set, .facts = args->facts);
    }
}
