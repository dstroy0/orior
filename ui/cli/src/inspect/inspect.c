// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect.c: the inspections' entry points, each taking the inspections' lock and handing the text to the reader of
// its language; the facts' own writers; a finding written as the protocol's diagnostic; and a class's body written
// again with its methods sorted by name
#include "../text/json_tree.h"
#include "inspect_internal.h"

#include "../text/text.h"

const char *const g_cli_inspect_classes[CLI_INSPECT_CLASSES] = {
    "",           "t-keyword", "t-string",    "t-number",   "t-comment", "t-operator",   "t-delimiter",
    "t-function", "t-type",    "t-parameter", "t-property", "t-module",  "t-predefined", "t-attribute"};

const char *const g_cli_inspect_docstring_forms[4] = {"google", "numpy", "rest", "plain"};

const char *cli_inspect_language_of(const char *path)
{
    const char *name = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    if ((dot == NULL) || (dot == name))
    {
        return NULL;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".py", .cap = CLI_CELLS_POOL}))
    {
        return "python";
    }
    return ((cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".js", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".mjs", .cap = CLI_CELLS_POOL})))
               ? "javascript"
               : NULL;
}

const char *cli_inspect_text(const char *pool, CliInspectText text)
{
    return pool + text.at;
}

void cli_facts_begun(CliFacts *facts, CliInspectImport *imports, unsigned int imports_most, CliInspectClass *classes,
                     unsigned int classes_most, CliInspectFunction *functions, unsigned int functions_most,
                     CliInspectFinding *findings, unsigned int findings_most, CliInspectFix *fixes,
                     unsigned int fixes_most, CliInspectEdit *edits, unsigned int edits_most, CliInspectName *names,
                     unsigned int names_most, char *text, unsigned int text_most)
{
    *facts = (CliFacts){imports,  0u, imports_most,  classes, 0u, classes_most, functions, 0u, functions_most,
                        findings, 0u, findings_most, fixes,   0u, fixes_most,   edits,     0u, edits_most,
                        names,    0u, names_most,    text,    0u, text_most,    0};
    if (text_most > 0u)
    {
        text[0] = '\0';
        facts->used = 1u;
    }
}

CliInspectText cli_inspect_kept(CliFacts *facts, const char *text, unsigned long long size)
{
    CliInspectText kept = {0u, 0u};
    if (facts->used + size + 1u > facts->text_most)
    {
        facts->full = 1;
        return kept;
    }
    // a pool holds at most 4 GiB, which its places count
    kept.at = facts->used;
    kept.size = (unsigned int)size;
    memor.cpy(&(MemoriaCfg){.dst = facts->text + facts->used, .src = text, .bytes = (size_t)size});
    facts->text[facts->used + size] = '\0';
    facts->used += (unsigned int)size + 1u;
    return kept;
}

CliInspectText cli_inspect_line_kept(CliFacts *facts, const CliLine *line)
{
    if (!cli_line_fits(line))
    {
        facts->full = 1;
        return (CliInspectText){0u, 0u};
    }
    return cli_inspect_kept(facts, line->out, line->at);
}

void cli_inspect_finding_put(CliFacts *facts, CliInspectPlace from, CliInspectPlace to, int severity,
                             const CliLine *message, const char *code, const char *href)
{
    if (facts->findings == facts->findings_most)
    {
        facts->full = 1;
        return;
    }
    const CliInspectText kept = cli_inspect_line_kept(facts, message);
    facts->finding[facts->findings] = (CliInspectFinding){from, to, severity, kept, code, href, facts->fixes, 0u};
    facts->findings += 1u;
}

void cli_inspect_fix_put(CliFacts *facts, const CliLine *title)
{
    if ((facts->findings == 0u) || (facts->fixes == facts->fixes_most))
    {
        facts->full = 1;
        return;
    }
    facts->fix[facts->fixes] = (CliInspectFix){cli_inspect_line_kept(facts, title), facts->edits, 0u};
    facts->fixes += 1u;
    facts->finding[facts->findings - 1u].fixes += 1u;
}

void cli_inspect_fix_titled(CliFacts *facts, const char *title)
{
    char text[CLI_NAME_LONGEST];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(&line, title);
    cli_inspect_fix_put(facts, &line);
}

void cli_inspect_edit_put(CliFacts *facts, CliInspectPlace from, CliInspectPlace to, const char *text,
                          unsigned long long size)
{
    if ((facts->fixes == 0u) || (facts->edits == facts->edits_most))
    {
        facts->full = 1;
        return;
    }
    facts->edit[facts->edits] = (CliInspectEdit){from, to, cli_inspect_kept(facts, text, size)};
    facts->edits += 1u;
    facts->fix[facts->fixes - 1u].edits += 1u;
}

unsigned int cli_inspect_name_put(CliFacts *facts, const char *name, unsigned long long size, const char *alias,
                                  unsigned long long alias_size, CliInspectPlace from, CliInspectPlace to)
{
    if (facts->names == facts->names_most)
    {
        facts->full = 1;
        return CLI_INSPECT_FULL;
    }
    const CliInspectText kept = cli_inspect_kept(facts, name, size);
    const CliInspectText alias_kept =
        (alias != NULL) ? cli_inspect_kept(facts, alias, alias_size) : (CliInspectText){0u, 0u};
    facts->name[facts->names] = (CliInspectName){kept, alias_kept, alias != NULL, from, to};
    facts->names += 1u;
    return facts->names - 1u;
}

int cli_inspect_facts(const char *language, const char *text, unsigned long long size, CliFacts *facts)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python && (!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    facts->imports = 0u;
    facts->classes = 0u;
    facts->functions = 0u;
    facts->findings = 0u;
    facts->fixes = 0u;
    facts->edits = 0u;
    facts->names = 0u;
    facts->used = (facts->text_most > 0u) ? 1u : 0u;
    facts->full = 0;
    cli_lock_take(&g_cli_inspect_lock);
    if (!cli_inspect_reading_held(language, text, size))
    {
        facts->full = 1;
    }
    else if (python)
    {
        cli_inspect_py_facts(text, size, facts);
    }
    else
    {
        cli_inspect_js_facts(text, size, facts);
    }
    cli_inspect_reading_given();
    cli_lock_give(&g_cli_inspect_lock);
    return 1;
}

void cli_parsed_begun(CliParsed *parsed, CliInspectRun *runs, unsigned int runs_most, unsigned int *line_first,
                      unsigned int *line_runs, unsigned int lines_most, unsigned int *fold_first,
                      unsigned int *fold_last, unsigned int folds_most, CliInspectSpan *ranges,
                      unsigned int ranges_most)
{
    *parsed = (CliParsed){runs,      0u, runs_most,  line_first, line_runs, 0u,          lines_most, fold_first,
                          fold_last, 0u, folds_most, ranges,     0u,        ranges_most, 0};
}

int cli_inspect_parse(const char *language, const char *text, unsigned long long size, CliParsed *parsed)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python && (!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    cli_lock_take(&g_cli_inspect_lock);
    const int held = cli_inspect_reading_held(language, text, size);
    if (held && python)
    {
        cli_inspect_py_parse(text, size, parsed);
    }
    else if (held)
    {
        cli_inspect_js_parse(text, size, parsed);
    }
    cli_inspect_reading_given();
    cli_lock_give(&g_cli_inspect_lock);
    return held;
}

int cli_inspect_docstring(const char *language, const char *source, unsigned long long size, unsigned int line,
                          const char *form, CliInspectEdit *edit, CliLine *text, CliInspectPlace *summary,
                          CliLine *said)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python && (!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, "Write Docstring reads Python and JavaScript.");
        return 0;
    }
    cli_lock_take(&g_cli_inspect_lock);
    const int held = cli_inspect_reading_held(language, source, size);
    const int written = held && (python ? cli_inspect_py_docstring(source, size, line, form, edit, text, summary, said)
                                        : cli_inspect_js_docstring(source, size, line, edit, text, summary, said));
    cli_inspect_reading_given();
    cli_lock_give(&g_cli_inspect_lock);
    if (!held)
    {
        cli_line_text(said, "a reading of the text could not be taken");
    }
    return written;
}

int cli_inspect_sort_methods(const char *language, const char *source, unsigned long long size, unsigned int line,
                             CliInspectEdit *edit, CliLine *text, CliLine *said)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python && (!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, "Sort Methods by Name reads Python and JavaScript.");
        return 0;
    }
    cli_lock_take(&g_cli_inspect_lock);
    const int held = cli_inspect_reading_held(language, source, size);
    const int sorted = held && (python ? cli_inspect_py_sort_methods(source, size, line, edit, text, said)
                                       : cli_inspect_js_sort_methods(source, size, line, edit, text, said));
    cli_inspect_reading_given();
    cli_lock_give(&g_cli_inspect_lock);
    if (!held)
    {
        cli_line_text(said, "a reading of the text could not be taken");
    }
    return sorted;
}

// a place written as the protocol's `{"line", "character"}`, or as an edit's `{"line", "col"}`
static void inspect_place_written(CliInspectPlace place, const char *col, CliLine *out)
{
    cli_line_text(out, "{\"line\":");
    cli_line_decimal(out, place.line, 1u);
    cli_line_text(out, col);
    cli_line_decimal(out, place.col, 1u);
    cli_line_character(out, '}');
}

void cli_inspect_finding_written(const CliFacts *facts, unsigned int finding, CliLine *out)
{
    const CliInspectFinding *const found = &facts->finding[finding];
    cli_line_text(out, "{\"range\":{\"start\":");
    inspect_place_written(found->from, ",\"character\":", out);
    cli_line_text(out, ",\"end\":");
    inspect_place_written(found->to, ",\"character\":", out);
    cli_line_text(out, (found->severity == CLI_INSPECT_WARNING) ? "},\"severity\":2" : "},\"severity\":3");
    cli_line_text(out, ",\"message\":{\"kind\":\"markdown\",\"value\":");
    cli_json_quoted(out, facts->text + found->message.at, found->message.size);
    cli_line_text(out, "},\"source\":\"orior\",\"code\":");
    cli_json_quoted(out, found->code, cellul.len(&(CatenaFinitaCfg){.src = found->code, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"data\":{\"orior\":[");
    for (unsigned int fix = 0u; fix < found->fixes; fix += 1u)
    {
        const CliInspectFix *const one = &facts->fix[found->fix_first + fix];
        cli_line_text(out, (fix > 0u) ? ",{\"title\":" : "{\"title\":");
        cli_json_quoted(out, facts->text + one->title.at, one->title.size);
        cli_line_text(out, ",\"edits\":[");
        for (unsigned int edit = 0u; edit < one->edits; edit += 1u)
        {
            const CliInspectEdit *const change = &facts->edit[one->edit_first + edit];
            cli_line_text(out, (edit > 0u) ? ",{\"from\":" : "{\"from\":");
            inspect_place_written(change->from, ",\"col\":", out);
            cli_line_text(out, ",\"to\":");
            inspect_place_written(change->to, ",\"col\":", out);
            cli_line_text(out, ",\"text\":");
            cli_json_quoted(out, facts->text + change->text.at, change->text.size);
            cli_line_character(out, '}');
        }
        cli_line_text(out, "]}");
    }
    cli_line_text(out, "]}");
    if (found->href != NULL)
    {
        cli_line_text(out, ",\"codeDescription\":{\"href\":");
        cli_json_quoted(out, found->href, cellul.len(&(CatenaFinitaCfg){.src = found->href, .cap = CLI_CELLS_POOL}));
        cli_line_character(out, '}');
    }
    cli_line_character(out, '}');
}

unsigned int cli_inspect_physical(const char *src, unsigned long long size, unsigned long long *starts,
                                  unsigned long long *ends, unsigned int most)
{
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; (at <= size) && (count < most);)
    {
        const char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(size - at)}) : NULL;
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - src) : size;
        starts[count] = at;
        ends[count] = end;
        count += 1u;
        at = end + 1u;
    }
    return count;
}

// whether method `one` sorts before method `other`: by their names in lower case, then by their bytes
static int inspect_sorted_before(const char *one, unsigned int one_size, const char *other, unsigned int other_size)
{
    const unsigned int least = (one_size < other_size) ? one_size : other_size;
    for (unsigned int at = 0u; at < least; at += 1u)
    {
        const char left = ((one[at] >= 'A') && (one[at] <= 'Z')) ? (char)(one[at] - 'A' + 'a') : one[at];
        const char right = ((other[at] >= 'A') && (other[at] <= 'Z')) ? (char)(other[at] - 'A' + 'a') : other[at];
        if (left != right)
        {
            return (unsigned char)left < (unsigned char)right;
        }
    }
    if (one_size != other_size)
    {
        return one_size < other_size;
    }
    const int order = memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = least});
    return order < 0;
}

// the methods of a body sorted, a list of the reading's cell, one for each token at the most
static unsigned int *s_methods;

void cli_inspect_sort_laid(CliInspectCarve *carve)
{
    s_methods = cli_inspect_carved(carve, (unsigned long long)CLI_INSPECT_TOKENS * sizeof(unsigned int));
}

int cli_inspect_sorted_body(const char *src, unsigned long long size, const unsigned long long *line_starts,
                            const unsigned long long *line_ends, const unsigned int *firsts, const unsigned int *lasts,
                            const char *const *names, const unsigned int *name_sizes, unsigned int members,
                            CliLine *out)
{
    (void)size;
    unsigned int methods = 0u;
    for (unsigned int at = 0u; (at < members) && (methods < CLI_INSPECT_TOKENS); at += 1u)
    {
        if (names[at] != NULL)
        {
            s_methods[methods] = at;
            methods += 1u;
        }
    }
    if (methods < 2u)
    {
        return 0;
    }
    int moved = 0;
    for (unsigned int at = 1u; at < methods; at += 1u)
    {
        const unsigned int held = s_methods[at];
        unsigned int place = at;
        while ((place > 0u) && inspect_sorted_before(names[held], name_sizes[held], names[s_methods[place - 1u]],
                                                     name_sizes[s_methods[place - 1u]]))
        {
            s_methods[place] = s_methods[place - 1u];
            place -= 1u;
            moved = 1;
        }
        s_methods[place] = held;
    }
    if (!moved)
    {
        return 0;
    }
    unsigned int next = 0u;
    unsigned int at_line = firsts[0];
    int first_line = 1;
    for (unsigned int member = 0u; member < members; member += 1u)
    {
        const unsigned int chosen = (names[member] != NULL) ? s_methods[next] : member;
        next += (names[member] != NULL) ? 1u : 0u;
        for (unsigned int line = at_line; line < firsts[member]; line += 1u)
        {
            cli_line_text(out, first_line ? "" : "\n");
            cli_line_counted(out, src + line_starts[line], line_ends[line] - line_starts[line]);
            first_line = 0;
        }
        for (unsigned int line = firsts[chosen]; line <= lasts[chosen]; line += 1u)
        {
            cli_line_text(out, first_line ? "" : "\n");
            cli_line_counted(out, src + line_starts[line], line_ends[line] - line_starts[line]);
            first_line = 0;
        }
        at_line = lasts[member] + 1u;
    }
    return 1;
}
