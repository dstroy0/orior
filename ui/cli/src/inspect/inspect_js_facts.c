// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_facts.c: what one JavaScript file shows alone: a `const`, `let` or `var` a function declares and never
// reads, and code after a return, a throw, a break or a continue in the same block; and, for the tree's checks, the
// functions it declares with the calls in each, and the files it imports by a relative path
#include "../text/text.h"
#include "inspect_js.h"

// the most names one function declares that are recorded
#define CLI_JS_DECLARED 16384u

static unsigned int s_declared[CLI_JS_DECLARED];
static CliInspectSet s_reads;

void cli_inspect_js_facts_layout(const CliInspectPartitionCtx *args)
{
    EMBED_CALL(cli_inspect_partition_set_internal.set_layout, CliInspectPartitionSetCtx, .set = &s_reads,
               .partition = args->partition);
}

typedef struct
{
    const char *src;
    unsigned int at;
    unsigned long long size;
    unsigned int from;
    unsigned int to;
    CliInspectPosition *start;
    CliInspectPosition *end;
    unsigned int name_at;
    CliFacts *facts;
    unsigned int open;
    unsigned int close;
} InspectJsFactsCtx;
// the token past the bracket at `at` where it opens one that closes after it, else the token after it
EMBED_INLINE unsigned int facts_end(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    unsigned int at = args->at;
    const unsigned int close = g_cli_js_pair[at];
    const int opens =
        EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "(") ||
        EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "[") ||
        EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "{");
    return ((close != CLI_JS_NO) && (close > at) && opens) ? (close + 1u) : (at + 1u);
}

// the span of the lines from the line of token `from` to the line of token `to` whole, where nothing else stands on
// them: 1, or 0 where something does
EMBED_INLINE int facts_full_line_span(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliInspectPosition *start = args->start;
    CliInspectPosition *end = args->end;
    const CliInspectTok *const first = &g_cli_inspect_tok[args->from];
    const CliInspectTok *const last = &g_cli_inspect_tok[args->to];
    unsigned long long line_start = first->start;
    line_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = src, .at = line_start,
                            .set = "\n");
    unsigned long long line_end = last->end;
    line_end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = src, .at = line_end,
                          .size = size, .set = "\n");
    for (unsigned long long at = line_start; at < first->start; at += 1u)
    {
        if (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\v\f", .cap = CLI_CELLS_POOL, .byte = src[at]}) == NULL)
        {
            return 0;
        }
    }
    for (unsigned long long at = last->end; at < line_end; at += 1u)
    {
        if (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\v\f", .cap = CLI_CELLS_POOL, .byte = src[at]}) == NULL)
        {
            return 0;
        }
    }
    *start = (CliInspectPosition){first->from.line, 0u};
    *end = (line_end < size) ? (CliInspectPosition){last->to.line + 1u, 0u} : last->to;
    return 1;
}

// the fix that takes out an unread declaration of one name, given a value and ended by a semicolon on its own lines:
// the declaration with its lines, or, where its value calls something, all but its value; put on the last finding
EMBED_INLINE void facts_declaration_fix(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliFacts *facts = args->facts;
    const unsigned int keyword = args->name_at - 1u;
    if (!EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = args->name_at + 1u,
                    .word = "="))
    {
        return;
    }
    unsigned int end = args->name_at + 2u;
    while ((end < g_cli_inspect_toks) &&
           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word = ";"))
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word = ",") &&
            (g_cli_js_within[end] == g_cli_js_within[args->name_at]) &&
            (EMBED_CALL(cli_inspect_js_from.js_pair_depth, CliInspectJsFromCfg, .src = src, .from = args->name_at + 2u,
                        .to = end) == 0))
        {
            return;
        }
        end = EMBED_CALL(facts_end, InspectJsFactsCtx, .src = src, .at = end);
    }
    if (end >= g_cli_inspect_toks)
    {
        return;
    }
    int calls = 0;
    for (unsigned int at = args->name_at + 2u; !calls && (at < end); at += 1u)
    {
        calls = EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "(");
    }
    const CliInspectTok *const name = &g_cli_inspect_tok[args->name_at];
    const CliInspectTok *const keyword_tok = &g_cli_inspect_tok[keyword];
    char title[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea titled = {title, sizeof(title), 0ULL};
    if (calls)
    {
        mmgr_line_put(&titled, "Take out `");
        mmgr_line_put_n(&titled, src + keyword_tok->start, keyword_tok->end - keyword_tok->start);
        mmgr_line_ch(&titled, ' ');
        mmgr_line_put_n(&titled, src + name->start, name->end - name->start);
        mmgr_line_put(&titled, " =` and retain its value");
        EMBED_CALL(cli_inspect_facts_internal.fix_append, CliInspectFactsCtx, .facts = facts, .title = &titled);
        EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = keyword_tok->from,
                   .to = g_cli_inspect_tok[args->name_at + 2u].from, .text_text = "", .size = 0ULL);
        return;
    }
    CliInspectPosition from = {0u, 0u};
    CliInspectPosition to = {0u, 0u};
    if (!EMBED_CALL(facts_full_line_span, InspectJsFactsCtx, .src = src, .size = size, .from = keyword, .to = end,
                    .start = &from, .end = &to))
    {
        return;
    }
    mmgr_line_put(&titled, "Take out the declaration of ");
    mmgr_line_put_n(&titled, src + name->start, name->end - name->start);
    EMBED_CALL(cli_inspect_facts_internal.fix_append, CliInspectFactsCtx, .facts = facts, .title = &titled);
    EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = from, .to = to,
               .text_text = "", .size = 0ULL);
}

// the `const`, `let` and `var` the function whose body opens at `open` declares and never reads
EMBED_INLINE void facts_unread(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliFacts *facts = args->facts;
    const unsigned int close = g_cli_js_pair[args->open];
    if (close == CLI_JS_NO)
    {
        return;
    }
    unsigned int declared = 0u;
    EMBED_CALL(cli_inspect_partition_set_internal.set_clear, CliInspectPartitionSetCtx, .set = &s_reads);
    unsigned int nested_end = 0u;
    for (unsigned int at = args->open + 1u; at < close; at += 1u)
    {
        if ((at > nested_end) && (g_cli_js_brace[at] == CLI_JS_FUNCTION))
        {
            nested_end = (g_cli_js_pair[at] != CLI_JS_NO) ? g_cli_js_pair[at] : at;
        }
        const int direct = (at > nested_end);
        const int declares =
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "const") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "let") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "var");
        const int named = (at + 1u < g_cli_inspect_toks) && (g_cli_inspect_tok[at + 1u].kind == CLI_JS_NAME);
        const int supplied =
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 2u, .word = "=") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 2u, .word = ";");
        const int looped =
            (at >= 2u) &&
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at - 1u, .word = "(") &&
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at - 2u, .word = "for");
        if (direct && declares && named && supplied && !looped)
        {
            if (declared < CLI_JS_DECLARED)
            {
                s_declared[declared] = at + 1u;
                declared += 1u;
            }
            continue;
        }
        int own = 0;
        for (unsigned int index = 0u; !own && (index < declared); index += 1u)
        {
            own = (s_declared[index] == at);
        }
        if (EMBED_CALL(cli_inspect_js_at_word.js_is_read, CliInspectJsAtWordCfg, .src = src, .at = at) && !own)
        {
            const CliInspectTok *const tok = &g_cli_inspect_tok[at];
            EMBED_CALL(cli_inspect_set_internal.set_append, CliInspectSetCtx, .set = &s_reads, .at = src + tok->start,
                       .size_u32 = tok->end - tok->start);
        }
    }
    if (EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_reads, .at = "eval",
                   .size_u32 = 4u))
    {
        return;
    }
    for (unsigned int index = 0u; index < declared; index += 1u)
    {
        const CliInspectTok *const name = &g_cli_inspect_tok[s_declared[index]];
        if ((src[name->start] == '_') ||
            EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_reads, .at = src + name->start,
                       .size_u32 = name->end - name->start))
        {
            continue;
        }
        char message[CLI_NAME_MAX_LEN * 2u];
        VerbaLinea msg = {message, sizeof(message), 0ULL};
        mmgr_line_ch(&msg, '`');
        mmgr_line_put_n(&msg, src + name->start, name->end - name->start);
        mmgr_line_put(&msg, "` is declared and never read.");
        EMBED_CALL(cli_inspect_from_internal.finding_append, CliInspectFromCtx, .facts = facts, .from = name->from,
                   .to = name->to, .severity = CLI_INSPECT_WARNING, .message = &msg, .code = "unread", .href = NULL);
        EMBED_CALL(facts_declaration_fix, InspectJsFactsCtx, .src = src, .size = size, .name_at = s_declared[index],
                   .facts = facts);
    }
}

// code after a return, a throw, a break or a continue that stands directly in the block opening at `open`, up to the
// block's end or its switch's next label; a function declared there is left out, as it is made before the block runs
static void facts_unreachable(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliFacts *facts = args->facts;
    const unsigned int close = g_cli_js_pair[args->open];
    if (close == CLI_JS_NO)
    {
        return;
    }
    static const char *const s_leaves[4] = {"return", "throw", "break", "continue"};
    static const char *const s_starts[4] = {"{", ";", "}", ":"};
    static const char *const s_labels[2] = {"case", "default"};
    static const char *const s_stops[4] = {"case", "default", "function", "}"};
    unsigned int at = args->open + 1u;
    while (at < close)
    {
        const int leaves = (g_cli_js_within[at] == args->open) &&
                           EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = at,
                                      .words = s_leaves, .count = 4u) &&
                           (g_cli_inspect_tok[at].kind == CLI_JS_NAME) &&
                           EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = at - 1u,
                                      .words = s_starts, .count = 4u);
        if (!leaves)
        {
            const unsigned int end = g_cli_js_pair[at];
            at = ((end != CLI_JS_NO) && (end > at)) ? (end + 1u) : (at + 1u);
            continue;
        }
        unsigned int end = at + 1u;
        while ((end < close) &&
               !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word = ";"))
        {
            end = EMBED_CALL(facts_end, InspectJsFactsCtx, .src = src, .at = end);
        }
        const unsigned int first = end + 1u;
        if ((end >= close) || (first >= close) ||
            EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = first, .words = s_stops,
                       .count = 4u))
        {
            at = first;
            continue;
        }
        unsigned int last = first;
        unsigned int next = first;
        while ((next < close) && !((g_cli_js_within[next] == args->open) &&
                                   EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = next,
                                              .words = s_labels, .count = 2u)))
        {
            const unsigned int pair = g_cli_js_pair[next];
            last = ((pair != CLI_JS_NO) && (pair > next)) ? pair : next;
            next = last + 1u;
        }
        const CliInspectTok *const word = &g_cli_inspect_tok[at];
        char message[CLI_INSPECT_UNREACHED_CAPACITY];
        VerbaLinea msg = {message, sizeof(message), 0ULL};
        mmgr_line_put(&msg, CLI_INSPECT_UNREACHED_BEFORE);
        mmgr_line_put_n(&msg, src + word->start, word->end - word->start);
        mmgr_line_put(&msg, CLI_INSPECT_UNREACHED_AFTER);
        EMBED_CALL(cli_inspect_from_internal.finding_append, CliInspectFromCtx, .facts = facts,
                   .from = g_cli_inspect_tok[first].from, .to = g_cli_inspect_tok[last].to,
                   .severity = CLI_INSPECT_WARNING, .message = &msg, .code = "unreachable", .href = NULL);
        CliInspectPosition from = {0u, 0u};
        CliInspectPosition to = {0u, 0u};
        if (EMBED_CALL(facts_full_line_span, InspectJsFactsCtx, .src = src, .size = size, .from = first, .to = last,
                       .start = &from, .end = &to))
        {
            EMBED_CALL(cli_inspect_facts_title_internal.fix_title, CliInspectFactsTitleCtx, .facts = facts,
                       .title_text = "Take out the code no path reaches");
            EMBED_CALL(cli_inspect_text_internal.edit_append, CliInspectTextCtx, .facts = facts, .from = from, .to = to,
                       .text_text = "", .size = 0ULL);
        }
        at = next;
    }
}

// the name a function is given by an assignment before `at`, `name = ...` or `name: ...`: its token, or CLI_JS_NO
EMBED_INLINE unsigned int facts_name_by_assignment(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    unsigned int at = args->at;
    const int assigned =
        (at >= 2u) &&
        (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at - 1u, .word = "=") ||
         EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at - 1u, .word = ":")) &&
        (g_cli_inspect_tok[at - 2u].kind == CLI_JS_NAME);
    return assigned ? (at - 2u) : CLI_JS_NO;
}

unsigned int cli_js_function_name(const CliInspectJsOpenCfg *args)
{
    if (args->open == 0u)
    {
        return CLI_JS_NO;
    }
    const unsigned int before = args->open - 1u;
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before, .word = "=>"))
    {
        if (before == 0u)
        {
            return CLI_JS_NO;
        }
        const unsigned int params = before - 1u;
        unsigned int start = params;
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = params,
                       .word = ")"))
        {
            start = g_cli_js_pair[params];
            if (start == CLI_JS_NO)
            {
                return CLI_JS_NO;
            }
        }
        start = ((start >= 1u) && EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src,
                                             .at = start - 1u, .word = "async"))
                    ? (start - 1u)
                    : start;
        return EMBED_CALL(facts_name_by_assignment, InspectJsFactsCtx, .src = args->src, .at = start);
    }
    if (!EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before, .word = ")") ||
        (g_cli_js_pair[before] == CLI_JS_NO) || (g_cli_js_pair[before] == 0u))
    {
        return CLI_JS_NO;
    }
    const unsigned int name = g_cli_js_pair[before] - 1u;
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = name,
                   .word = "function"))
    {
        const unsigned int start = ((name >= 1u) && EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg,
                                                               .src = args->src, .at = name - 1u, .word = "async"))
                                       ? (name - 1u)
                                       : name;
        return EMBED_CALL(facts_name_by_assignment, InspectJsFactsCtx, .src = args->src, .at = start);
    }
    return (g_cli_inspect_tok[name].kind == CLI_JS_NAME) ? name : CLI_JS_NO;
}

// the calls in the function whose body runs from `open` to `close`, outside the functions it contains: a name before a
// bracket, after no dot or after `this.`, put on the facts' names
EMBED_INLINE void facts_calls(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    CliFacts *facts = args->facts;
    unsigned int at = args->open + 1u;
    while (at < args->close)
    {
        if (g_cli_js_brace[at] == CLI_JS_FUNCTION)
        {
            at = (g_cli_js_pair[at] != CLI_JS_NO) ? (g_cli_js_pair[at] + 1u) : (at + 1u);
            continue;
        }
        const unsigned int before = (at >= 1u) ? (at - 1u) : CLI_JS_NO;
        const unsigned int owner = (at >= 2u) ? (at - 2u) : CLI_JS_NO;
        const unsigned int end = g_cli_js_pair[at + 1u];
        const int declares =
            (end != CLI_JS_NO) && (end + 1u < g_cli_inspect_toks) &&
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end + 1u, .word = "{");
        const CliInspectTok *const tok = &g_cli_inspect_tok[at];
        const int called =
            (tok->kind == CLI_JS_NAME) &&
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u, .word = "(") &&
            !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = at,
                        .words = g_cli_js_keywords, .count = 35u) &&
            !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = at,
                        .words = g_cli_js_control, .count = 7u);
        const int reaches =
            !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word = ".") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = owner, .word = "this");
        if (called && !declares && reaches &&
            !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before,
                        .word = "function"))
        {
            (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = facts,
                             .name = src + tok->start, .size = tok->end - tok->start, .alias = NULL, .alias_size = 0ULL,
                             .from = tok->from, .to = tok->to);
        }
        at += 1u;
    }
}

// the functions a file declares, with a name, and the calls in each
EMBED_INLINE void facts_functions(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    CliFacts *facts = args->facts;
    for (unsigned int open = 0u; open < g_cli_inspect_toks; open += 1u)
    {
        if (g_cli_js_brace[open] != CLI_JS_FUNCTION)
        {
            continue;
        }
        const unsigned int name_at =
            EMBED_CALL(cli_inspect_js_open.js_function_name, CliInspectJsOpenCfg, .src = src, .open = open);
        const unsigned int close = g_cli_js_pair[open];
        if ((name_at == CLI_JS_NO) || (close == CLI_JS_NO))
        {
            continue;
        }
        const unsigned int call_first = facts->names;
        EMBED_CALL(facts_calls, InspectJsFactsCtx, .src = src, .open = open, .close = close, .facts = facts);
        if (facts->functions == facts->functions_max)
        {
            facts->full = 1;
            return;
        }
        const CliInspectTok *const name = &g_cli_inspect_tok[name_at];
        const CliInspectFunction function = {EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx,
                                                        .facts = facts, .text_text = src + name->start,
                                                        .size = name->end - name->start),
                                             name->from,
                                             name->to,
                                             g_cli_inspect_tok[(name_at < open) ? name_at : open].from.line,
                                             g_cli_inspect_tok[close].to.line,
                                             call_first,
                                             facts->names - call_first};
        if (!facts->counting)
        {
            facts->function[facts->functions] = function;
        }
        facts->functions += 1u;
    }
}

// the files the file imports by a relative path, at its top level: `import ... from`, a bare `import` and
// `export ... from`
EMBED_INLINE void facts_imports(const InspectJsFactsCtx *args)
{
    const char *src = args->src;
    CliFacts *facts = args->facts;
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        const int starts =
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "import") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "export");
        if ((g_cli_js_within[at] != CLI_JS_NO) || !starts ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u, .word = "(") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u, .word = "."))
        {
            continue;
        }
        unsigned int end = at + 1u;
        while ((end < g_cli_inspect_toks) &&
               !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word = ";") &&
               !((g_cli_inspect_tok[end].kind == CLI_JS_NAME) &&
                 (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end,
                             .word = "import") ||
                  EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end,
                             .word = "export")) &&
                 (g_cli_inspect_tok[end].from.line > g_cli_inspect_tok[at].from.line) &&
                 (g_cli_js_within[end] == CLI_JS_NO)))
        {
            end += 1u;
        }
        unsigned int spec = CLI_JS_NO;
        for (unsigned int one = at + 1u; (spec == CLI_JS_NO) && (one < end); one += 1u)
        {
            const int stated = (g_cli_inspect_tok[one].kind == CLI_JS_STR) &&
                               (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src,
                                           .at = one - 1u, .word = "from") ||
                                (one == at + 1u));
            spec = stated ? one : spec;
        }
        if (spec == CLI_JS_NO)
        {
            continue;
        }
        const CliInspectTok *const named = &g_cli_inspect_tok[spec];
        const char *const path = src + named->start + 1u;
        const unsigned int length = (named->end - named->start >= 2u) ? (named->end - named->start - 2u) : 0u;
        const int relative =
            ((length >= 2u) && (memor.cmp(&(MemoriaCfg){.src = path, .other = "./", .bytes = 2u}) == 0)) ||
            ((length >= 3u) && (memor.cmp(&(MemoriaCfg){.src = path, .other = "../", .bytes = 3u}) == 0));
        if (!relative)
        {
            continue;
        }
        if (facts->imports == facts->imports_max)
        {
            facts->full = 1;
            return;
        }
        const unsigned int last = ((end - 1u) > at) ? (end - 1u) : at;
        const CliInspectImport import = {EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx,
                                                    .facts = facts, .text_text = path, .size = length),
                                         facts->names,
                                         0u,
                                         {0u, 0u},
                                         0,
                                         g_cli_inspect_tok[at].from,
                                         g_cli_inspect_tok[last].to};
        if (!facts->counting)
        {
            facts->import[facts->imports] = import;
        }
        facts->imports += 1u;
    }
}

void cli_inspect_js_facts(const CliInspectSizeSrcFactsCtx *args)
{
    EMBED_CALL(cli_inspect_js.js_read, CliInspectJsCfg, .src = args->src, .size = args->size);
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        if (g_cli_js_brace[at] == CLI_JS_FUNCTION)
        {
            EMBED_CALL(facts_unread, InspectJsFactsCtx, .src = args->src, .size = args->size, .open = at,
                       .facts = args->facts);
            EMBED_CALL(facts_unreachable, InspectJsFactsCtx, .src = args->src, .size = args->size, .open = at,
                       .facts = args->facts);
        }
        else if (g_cli_js_brace[at] == CLI_JS_BLOCK)
        {
            EMBED_CALL(facts_unreachable, InspectJsFactsCtx, .src = args->src, .size = args->size, .open = at,
                       .facts = args->facts);
        }
    }
    EMBED_CALL(facts_imports, InspectJsFactsCtx, .src = args->src, .facts = args->facts);
    EMBED_CALL(facts_functions, InspectJsFactsCtx, .src = args->src, .facts = args->facts);
    args->facts->full = args->facts->full || g_cli_inspect_over;
}
