// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_facts.c: what one JavaScript file says alone: a `const`, `let` or `var` a function declares and never
// reads, and code after a return, a throw, a break or a continue in the same block; and, for the tree's checks, the
// functions it declares with the calls in each, and the files it imports by a relative path
#include "inspect_js.h"

// the most names one function declares that are kept
#define CLI_JS_DECLARED 16384u

static unsigned int s_declared[CLI_JS_DECLARED];
static CliInspectSet s_reads;

void cli_inspect_js_facts_laid(CliInspectCarve *carve)
{
    cli_inspect_set_laid(&s_reads, carve);
}

// the token past the bracket at `at` where it opens one that closes after it, else the token after it
static unsigned int facts_past(const char *src, unsigned int at)
{
    const unsigned int close = g_cli_js_pair[at];
    const int opens = cli_js_is(src, at, "(") || cli_js_is(src, at, "[") || cli_js_is(src, at, "{");
    return ((close != CLI_JS_NO) && (close > at) && opens) ? (close + 1u) : (at + 1u);
}

// the span of the lines from the line of token `from` to the line of token `to` whole, where nothing else stands on
// them: 1, or 0 where something does
static int facts_whole_lines(const char *src, unsigned long long size, unsigned int from, unsigned int to,
                             CliInspectPlace *start, CliInspectPlace *end)
{
    const CliInspectTok *const first = &g_cli_inspect_tok[from];
    const CliInspectTok *const last = &g_cli_inspect_tok[to];
    unsigned long long line_start = first->start;
    while ((line_start > 0u) && (src[line_start - 1u] != '\n'))
    {
        line_start -= 1u;
    }
    unsigned long long line_end = last->end;
    while ((line_end < size) && (src[line_end] != '\n'))
    {
        line_end += 1u;
    }
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
    *start = (CliInspectPlace){first->from.line, 0u};
    *end = (line_end < size) ? (CliInspectPlace){last->to.line + 1u, 0u} : last->to;
    return 1;
}

// the fix that takes out an unread declaration of one name, given a value and ended by a semicolon on its own lines:
// the declaration with its lines, or, where its value calls something, all but its value; put on the last finding
static void facts_declaration_fix(const char *src, unsigned long long size, unsigned int name_at, CliFacts *facts)
{
    const unsigned int keyword = name_at - 1u;
    if (!cli_js_is(src, name_at + 1u, "="))
    {
        return;
    }
    unsigned int end = name_at + 2u;
    while ((end < g_cli_inspect_toks) && !cli_js_is(src, end, ";"))
    {
        if (cli_js_is(src, end, ",") && (g_cli_js_within[end] == g_cli_js_within[name_at]) &&
            (cli_js_pair_depth(src, name_at + 2u, end) == 0))
        {
            return;
        }
        end = facts_past(src, end);
    }
    if (end >= g_cli_inspect_toks)
    {
        return;
    }
    int calls = 0;
    for (unsigned int at = name_at + 2u; !calls && (at < end); at += 1u)
    {
        calls = cli_js_is(src, at, "(");
    }
    const CliInspectTok *const name = &g_cli_inspect_tok[name_at];
    const CliInspectTok *const kept = &g_cli_inspect_tok[keyword];
    char title[CLI_NAME_LONGEST * 2u];
    CliLine titled = {title, sizeof(title), 0ull};
    if (calls)
    {
        cli_line_text(&titled, "Take out `");
        cli_line_counted(&titled, src + kept->start, kept->end - kept->start);
        cli_line_character(&titled, ' ');
        cli_line_counted(&titled, src + name->start, name->end - name->start);
        cli_line_text(&titled, " =` and keep its value");
        cli_inspect_fix_put(facts, &titled);
        cli_inspect_edit_put(facts, kept->from, g_cli_inspect_tok[name_at + 2u].from, "", 0ull);
        return;
    }
    CliInspectPlace from = {0u, 0u};
    CliInspectPlace to = {0u, 0u};
    if (!facts_whole_lines(src, size, keyword, end, &from, &to))
    {
        return;
    }
    cli_line_text(&titled, "Take out the declaration of ");
    cli_line_counted(&titled, src + name->start, name->end - name->start);
    cli_inspect_fix_put(facts, &titled);
    cli_inspect_edit_put(facts, from, to, "", 0ull);
}

// the `const`, `let` and `var` the function whose body opens at `open` declares and never reads
static void facts_unread(const char *src, unsigned long long size, unsigned int open, CliFacts *facts)
{
    const unsigned int close = g_cli_js_pair[open];
    if (close == CLI_JS_NO)
    {
        return;
    }
    unsigned int declared = 0u;
    cli_inspect_set_cleared(&s_reads);
    unsigned int nested_end = 0u;
    for (unsigned int at = open + 1u; at < close; at += 1u)
    {
        if ((at > nested_end) && (g_cli_js_brace[at] == CLI_JS_FUNCTION))
        {
            nested_end = (g_cli_js_pair[at] != CLI_JS_NO) ? g_cli_js_pair[at] : at;
        }
        const int direct = (at > nested_end);
        const int declares = cli_js_is(src, at, "const") || cli_js_is(src, at, "let") || cli_js_is(src, at, "var");
        const int named = (at + 1u < g_cli_inspect_toks) && (g_cli_inspect_tok[at + 1u].kind == CLI_JS_NAME);
        const int given = cli_js_is(src, at + 2u, "=") || cli_js_is(src, at + 2u, ";");
        const int looped = (at >= 2u) && cli_js_is(src, at - 1u, "(") && cli_js_is(src, at - 2u, "for");
        if (direct && declares && named && given && !looped)
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
        if (cli_js_is_read(src, at) && !own)
        {
            const CliInspectTok *const tok = &g_cli_inspect_tok[at];
            cli_inspect_set_put(&s_reads, src + tok->start, tok->end - tok->start);
        }
    }
    if (cli_inspect_set_has(&s_reads, "eval", 4u))
    {
        return;
    }
    for (unsigned int index = 0u; index < declared; index += 1u)
    {
        const CliInspectTok *const name = &g_cli_inspect_tok[s_declared[index]];
        if ((src[name->start] == '_') || cli_inspect_set_has(&s_reads, src + name->start, name->end - name->start))
        {
            continue;
        }
        char message[CLI_NAME_LONGEST * 2u];
        CliLine said = {message, sizeof(message), 0ull};
        cli_line_character(&said, '`');
        cli_line_counted(&said, src + name->start, name->end - name->start);
        cli_line_text(&said, "` is declared and never read.");
        cli_inspect_finding_put(facts, name->from, name->to, CLI_INSPECT_WARNING, &said, "unread", NULL);
        facts_declaration_fix(src, size, s_declared[index], facts);
    }
}

// code after a return, a throw, a break or a continue that stands directly in the block opening at `open`, up to the
// block's end or its switch's next label; a function declared there is left out, as it is made before the block runs
static void facts_unreachable(const char *src, unsigned long long size, unsigned int open, CliFacts *facts)
{
    const unsigned int close = g_cli_js_pair[open];
    if (close == CLI_JS_NO)
    {
        return;
    }
    static const char *const s_leaves[4] = {"return", "throw", "break", "continue"};
    static const char *const s_starts[4] = {"{", ";", "}", ":"};
    static const char *const s_labels[2] = {"case", "default"};
    static const char *const s_stops[4] = {"case", "default", "function", "}"};
    unsigned int at = open + 1u;
    while (at < close)
    {
        const int leaves = (g_cli_js_within[at] == open) && cli_js_one_of(src, at, s_leaves, 4u) &&
                           (g_cli_inspect_tok[at].kind == CLI_JS_NAME) && cli_js_one_of(src, at - 1u, s_starts, 4u);
        if (!leaves)
        {
            const unsigned int end = g_cli_js_pair[at];
            at = ((end != CLI_JS_NO) && (end > at)) ? (end + 1u) : (at + 1u);
            continue;
        }
        unsigned int end = at + 1u;
        while ((end < close) && !cli_js_is(src, end, ";"))
        {
            end = facts_past(src, end);
        }
        const unsigned int first = end + 1u;
        if ((end >= close) || (first >= close) || cli_js_one_of(src, first, s_stops, 4u))
        {
            at = first;
            continue;
        }
        unsigned int last = first;
        unsigned int next = first;
        while ((next < close) && !((g_cli_js_within[next] == open) && cli_js_one_of(src, next, s_labels, 2u)))
        {
            const unsigned int pair = g_cli_js_pair[next];
            last = ((pair != CLI_JS_NO) && (pair > next)) ? pair : next;
            next = last + 1u;
        }
        const CliInspectTok *const word = &g_cli_inspect_tok[at];
        char message[CLI_INSPECT_UNREACHED_ROOM];
        CliLine said = {message, sizeof(message), 0ull};
        cli_line_text(&said, CLI_INSPECT_UNREACHED_BEFORE);
        cli_line_counted(&said, src + word->start, word->end - word->start);
        cli_line_text(&said, CLI_INSPECT_UNREACHED_AFTER);
        cli_inspect_finding_put(facts, g_cli_inspect_tok[first].from, g_cli_inspect_tok[last].to, CLI_INSPECT_WARNING,
                                &said, "unreachable", NULL);
        CliInspectPlace from = {0u, 0u};
        CliInspectPlace to = {0u, 0u};
        if (facts_whole_lines(src, size, first, last, &from, &to))
        {
            cli_inspect_fix_titled(facts, "Take out the code no path reaches");
            cli_inspect_edit_put(facts, from, to, "", 0ull);
        }
        at = next;
    }
}

// the name a function is given by an assignment before `at`, `name = ...` or `name: ...`: its token, or CLI_JS_NO
static unsigned int facts_named_by_assignment(const char *src, unsigned int at)
{
    const int assigned = (at >= 2u) && (cli_js_is(src, at - 1u, "=") || cli_js_is(src, at - 1u, ":")) &&
                         (g_cli_inspect_tok[at - 2u].kind == CLI_JS_NAME);
    return assigned ? (at - 2u) : CLI_JS_NO;
}

unsigned int cli_js_function_name(const char *src, unsigned int open)
{
    if (open == 0u)
    {
        return CLI_JS_NO;
    }
    const unsigned int before = open - 1u;
    if (cli_js_is(src, before, "=>"))
    {
        if (before == 0u)
        {
            return CLI_JS_NO;
        }
        const unsigned int params = before - 1u;
        unsigned int start = params;
        if (cli_js_is(src, params, ")"))
        {
            start = g_cli_js_pair[params];
            if (start == CLI_JS_NO)
            {
                return CLI_JS_NO;
            }
        }
        start = ((start >= 1u) && cli_js_is(src, start - 1u, "async")) ? (start - 1u) : start;
        return facts_named_by_assignment(src, start);
    }
    if (!cli_js_is(src, before, ")") || (g_cli_js_pair[before] == CLI_JS_NO) || (g_cli_js_pair[before] == 0u))
    {
        return CLI_JS_NO;
    }
    const unsigned int name = g_cli_js_pair[before] - 1u;
    if (cli_js_is(src, name, "function"))
    {
        const unsigned int start = ((name >= 1u) && cli_js_is(src, name - 1u, "async")) ? (name - 1u) : name;
        return facts_named_by_assignment(src, start);
    }
    return (g_cli_inspect_tok[name].kind == CLI_JS_NAME) ? name : CLI_JS_NO;
}

// the calls in the function whose body runs from `open` to `close`, outside the functions it holds: a name before a
// bracket, after no dot or after `this.`, put on the facts' names
static void facts_calls(const char *src, unsigned int open, unsigned int close, CliFacts *facts)
{
    unsigned int at = open + 1u;
    while (at < close)
    {
        if (g_cli_js_brace[at] == CLI_JS_FUNCTION)
        {
            at = (g_cli_js_pair[at] != CLI_JS_NO) ? (g_cli_js_pair[at] + 1u) : (at + 1u);
            continue;
        }
        const unsigned int before = (at >= 1u) ? (at - 1u) : CLI_JS_NO;
        const unsigned int owner = (at >= 2u) ? (at - 2u) : CLI_JS_NO;
        const unsigned int end = g_cli_js_pair[at + 1u];
        const int declares = (end != CLI_JS_NO) && (end + 1u < g_cli_inspect_toks) && cli_js_is(src, end + 1u, "{");
        const CliInspectTok *const tok = &g_cli_inspect_tok[at];
        const int called = (tok->kind == CLI_JS_NAME) && cli_js_is(src, at + 1u, "(") &&
                           !cli_js_one_of(src, at, g_cli_js_keywords, 35u) &&
                           !cli_js_one_of(src, at, g_cli_js_control, 7u);
        const int reaches = !cli_js_is(src, before, ".") || cli_js_is(src, owner, "this");
        if (called && !declares && reaches && !cli_js_is(src, before, "function"))
        {
            (void)cli_inspect_name_put(facts, src + tok->start, tok->end - tok->start, NULL, 0ull, tok->from, tok->to);
        }
        at += 1u;
    }
}

// the functions a file declares, with a name, and the calls in each
static void facts_functions(const char *src, CliFacts *facts)
{
    for (unsigned int open = 0u; open < g_cli_inspect_toks; open += 1u)
    {
        if (g_cli_js_brace[open] != CLI_JS_FUNCTION)
        {
            continue;
        }
        const unsigned int name_at = cli_js_function_name(src, open);
        const unsigned int close = g_cli_js_pair[open];
        if ((name_at == CLI_JS_NO) || (close == CLI_JS_NO))
        {
            continue;
        }
        const unsigned int call_first = facts->names;
        facts_calls(src, open, close, facts);
        if (facts->functions == facts->functions_most)
        {
            facts->full = 1;
            return;
        }
        const CliInspectTok *const name = &g_cli_inspect_tok[name_at];
        facts->function[facts->functions] =
            (CliInspectFunction){cli_inspect_kept(facts, src + name->start, name->end - name->start),
                                 name->from,
                                 name->to,
                                 g_cli_inspect_tok[(name_at < open) ? name_at : open].from.line,
                                 g_cli_inspect_tok[close].to.line,
                                 call_first,
                                 facts->names - call_first};
        facts->functions += 1u;
    }
}

// the files the file imports by a relative path, at its top level: `import ... from`, a bare `import` and
// `export ... from`
static void facts_imports(const char *src, CliFacts *facts)
{
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        const int starts = cli_js_is(src, at, "import") || cli_js_is(src, at, "export");
        if ((g_cli_js_within[at] != CLI_JS_NO) || !starts || cli_js_is(src, at + 1u, "(") ||
            cli_js_is(src, at + 1u, "."))
        {
            continue;
        }
        unsigned int end = at + 1u;
        while ((end < g_cli_inspect_toks) && !cli_js_is(src, end, ";") &&
               !((g_cli_inspect_tok[end].kind == CLI_JS_NAME) &&
                 (cli_js_is(src, end, "import") || cli_js_is(src, end, "export")) &&
                 (g_cli_inspect_tok[end].from.line > g_cli_inspect_tok[at].from.line) &&
                 (g_cli_js_within[end] == CLI_JS_NO)))
        {
            end += 1u;
        }
        unsigned int spec = CLI_JS_NO;
        for (unsigned int one = at + 1u; (spec == CLI_JS_NO) && (one < end); one += 1u)
        {
            const int stated =
                (g_cli_inspect_tok[one].kind == CLI_JS_STR) && (cli_js_is(src, one - 1u, "from") || (one == at + 1u));
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
        if (facts->imports == facts->imports_most)
        {
            facts->full = 1;
            return;
        }
        const unsigned int last = ((end - 1u) > at) ? (end - 1u) : at;
        facts->import[facts->imports] = (CliInspectImport){cli_inspect_kept(facts, path, length),
                                                           facts->names,
                                                           0u,
                                                           {0u, 0u},
                                                           0,
                                                           g_cli_inspect_tok[at].from,
                                                           g_cli_inspect_tok[last].to};
        facts->imports += 1u;
    }
}

void cli_inspect_js_facts(const char *src, unsigned long long size, CliFacts *facts)
{
    cli_js_read(src, size);
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        if (g_cli_js_brace[at] == CLI_JS_FUNCTION)
        {
            facts_unread(src, size, at, facts);
            facts_unreachable(src, size, at, facts);
        }
        else if (g_cli_js_brace[at] == CLI_JS_BLOCK)
        {
            facts_unreachable(src, size, at, facts);
        }
    }
    facts_imports(src, facts);
    facts_functions(src, facts);
    facts->full = facts->full || g_cli_inspect_over;
}
