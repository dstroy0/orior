// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_help.c: what the build files' readers and the schemas' share: a text's lines and the place of a byte; a
// file's findings and their fixes, kept and written as the protocol's diagnostics; the completion items of what was
// typed; a folder as a path in the tree; and a snapshot's version
#include "../text/json_tree.h"
#include "../text/text.h"
#include "builds.h"

void cli_build_lines_of(const char *text, unsigned long long size, unsigned int *starts, unsigned int most,
                        CliBuildLines *lines)
{
    lines->text = text;
    lines->size = size;
    lines->start = starts;
    lines->count = 0u;
    if (most == 0u)
    {
        return;
    }
    starts[0] = 0u;
    lines->count = 1u;
    for (unsigned long long at = 0ull; (at < size) && (lines->count < most);)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = (size_t)(size - at)});
        if (newline == NULL)
        {
            break;
        }
        at = (unsigned long long)(newline - text) + 1u;
        starts[lines->count] = (unsigned int)at;
        lines->count += 1u;
    }
}

CliTextPlace cli_build_place(const CliBuildLines *lines, unsigned long long at)
{
    unsigned int low = 0u;
    unsigned int high = lines->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (lines->start[middle] <= at)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    unsigned int line = (low > 0u) ? low - 1u : 0u;
    unsigned long long start = (lines->count > 0u) ? lines->start[line] : 0ull;
    const unsigned long long end = (at < lines->size) ? at : lines->size;
    for (unsigned long long walk = start; walk < end; walk += 1u)
    {
        if (lines->text[walk] == '\n')
        {
            line += 1u;
            start = walk + 1u;
        }
    }
    const unsigned long long units = (start <= end) ? cli_text_units(lines->text + start, end - start) : 0ull;
    return (CliTextPlace){line, (unsigned int)units};
}

unsigned long long cli_build_line_start(const CliBuildLines *lines, unsigned int line)
{
    if (line < lines->count)
    {
        return lines->start[line];
    }
    unsigned long long at = (lines->count > 0u) ? lines->start[lines->count - 1u] : 0ull;
    for (unsigned int walk = (lines->count > 0u) ? lines->count - 1u : 0u; walk < line; walk += 1u)
    {
        const char *const newline =
            (at < lines->size)
                ? memor.chr(&(MemoriaCfg){.src = lines->text + at, .val = '\n', .bytes = (size_t)(lines->size - at)})
                : NULL;
        if (newline == NULL)
        {
            return lines->size;
        }
        at = (unsigned long long)(newline - lines->text) + 1u;
    }
    return at;
}

void cli_build_findings_begun(CliBuildFindings *found)
{
    found->count = 0u;
    found->fixes = 0u;
    found->used = 0u;
    found->full = 0;
}

CliLine cli_build_said(CliBuildFindings *found)
{
    return (CliLine){found->text + found->used, CLI_BUILD_TEXT - found->used, 0ull};
}

// `line` kept in the findings' texts, in place where it was written there: where it starts, or CLI_BUILD_TEXT where it
// does not fit
static unsigned int help_kept(CliBuildFindings *found, const char *bytes, unsigned long long size)
{
    if (size + 1u > CLI_BUILD_TEXT - found->used)
    {
        found->full = 1;
        return CLI_BUILD_TEXT;
    }
    const unsigned int at = found->used;
    if (bytes != found->text + at)
    {
        memor.move_down(&(MemoriaCfg){.dst = found->text + at, .src = bytes, .bytes = (size_t)size});
    }
    found->text[at + size] = '\0';
    found->used += (unsigned int)size + 1u;
    return at;
}

// a line written in the findings' texts kept: where it starts, or CLI_BUILD_TEXT where it did not fit
static unsigned int help_line_kept(CliBuildFindings *found, const CliLine *line)
{
    if (!cli_line_fits(line))
    {
        found->full = 1;
        return CLI_BUILD_TEXT;
    }
    return help_kept(found, line->out, line->at);
}

unsigned int cli_build_finding(CliBuildFindings *found, const CliLine *message, const CliBuildLines *lines,
                               unsigned long long from, unsigned long long to, unsigned int severity, const char *code)
{
    if (found->count == CLI_BUILD_FINDINGS)
    {
        found->full = 1;
        return CLI_BUILD_NONE;
    }
    const unsigned int at = help_line_kept(found, message);
    if (at == CLI_BUILD_TEXT)
    {
        return CLI_BUILD_NONE;
    }
    found->finding[found->count] = (CliBuildFinding){
        cli_build_place(lines, from), cli_build_place(lines, to), severity, code, at, (unsigned int)message->at};
    found->count += 1u;
    return found->count - 1u;
}

// a fix of the finding at `finding` begun with its title: its place, or CLI_BUILD_FIXES where it does not fit
static unsigned int help_fix(CliBuildFindings *found, unsigned int finding, const CliLine *title)
{
    if ((finding == CLI_BUILD_NONE) || (found->fixes == CLI_BUILD_FIXES))
    {
        found->full = 1;
        return CLI_BUILD_FIXES;
    }
    const unsigned int at = help_line_kept(found, title);
    if (at == CLI_BUILD_TEXT)
    {
        return CLI_BUILD_FIXES;
    }
    CliBuildFix *const fix = &found->fix[found->fixes];
    memor.set(&(MemoriaCfg){.dst = fix, .val = 0, .bytes = sizeof(*fix)});
    fix->finding = finding;
    fix->title_at = at;
    fix->title_size = (unsigned int)title->at;
    return found->fixes;
}

void cli_build_edit_fix(CliBuildFindings *found, unsigned int finding, const CliLine *title, const CliBuildLines *lines,
                        unsigned long long from, unsigned long long to, const char *text, unsigned long long size)
{
    const unsigned int at = help_fix(found, finding, title);
    if (at == CLI_BUILD_FIXES)
    {
        return;
    }
    CliBuildFix *const fix = &found->fix[at];
    fix->from = cli_build_place(lines, from);
    fix->to = cli_build_place(lines, to);
    fix->text_at = help_kept(found, text, size);
    fix->text_size = (unsigned int)size;
    if (fix->text_at != CLI_BUILD_TEXT)
    {
        found->fixes += 1u;
    }
}

void cli_build_edit_fix_twice(CliBuildFindings *found, unsigned int finding, const CliLine *title,
                              const CliBuildLines *lines, unsigned long long from, unsigned long long to,
                              unsigned long long also_from, unsigned long long also_to, const char *text,
                              unsigned long long size)
{
    const unsigned int fixes = found->fixes;
    cli_build_edit_fix(found, finding, title, lines, from, to, text, size);
    if (found->fixes > fixes)
    {
        CliBuildFix *const fix = &found->fix[fixes];
        fix->also = 1;
        fix->also_from = cli_build_place(lines, also_from);
        fix->also_to = cli_build_place(lines, also_to);
    }
}

void cli_build_run_fix(CliBuildFindings *found, unsigned int finding, const CliLine *title, const char *folder,
                       unsigned long long folder_size, const char *line, unsigned long long line_size)
{
    const unsigned int at = help_fix(found, finding, title);
    if (at == CLI_BUILD_FIXES)
    {
        return;
    }
    CliBuildFix *const fix = &found->fix[at];
    fix->run = 1;
    fix->folder_at = help_kept(found, folder, folder_size);
    fix->folder_size = (unsigned int)folder_size;
    fix->text_at = help_kept(found, line, line_size);
    fix->text_size = (unsigned int)line_size;
    if ((fix->folder_at != CLI_BUILD_TEXT) && (fix->text_at != CLI_BUILD_TEXT))
    {
        found->fixes += 1u;
    }
}

// a place written as the protocol's `{"line", "character"}`
static void help_place_written(CliTextPlace place, CliLine *out)
{
    cli_line_text(out, "{\"line\":");
    cli_line_decimal(out, place.line, 1u);
    cli_line_text(out, ",\"character\":");
    cli_line_decimal(out, place.col, 1u);
    cli_line_character(out, '}');
}

// the fix at `at` written
static void help_fix_written(const CliBuildFindings *found, const CliBuildFix *fix, CliLine *out)
{
    cli_line_text(out, "{\"title\":");
    cli_json_quoted(out, found->text + fix->title_at, fix->title_size);
    if (fix->run)
    {
        cli_line_text(out, ",\"run\":{\"folder\":");
        cli_json_quoted(out, found->text + fix->folder_at, fix->folder_size);
        cli_line_text(out, ",\"line\":");
        cli_json_quoted(out, found->text + fix->text_at, fix->text_size);
        cli_line_text(out, "}}");
        return;
    }
    const CliTextEdit edit = {fix->from, fix->to, found->text + fix->text_at, fix->text_size};
    cli_line_text(out, ",\"edits\":[");
    cli_text_edit_written(&edit, out);
    if (fix->also)
    {
        const CliTextEdit also = {fix->also_from, fix->also_to, found->text + fix->text_at, fix->text_size};
        cli_line_character(out, ',');
        cli_text_edit_written(&also, out);
    }
    cli_line_text(out, "]}");
}

void cli_build_findings_written(const CliBuildFindings *found, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliBuildFinding *const one = &found->finding[at];
        cli_line_text(out, (at > 0u) ? ",{\"range\":{\"start\":" : "{\"range\":{\"start\":");
        help_place_written(one->from, out);
        cli_line_text(out, ",\"end\":");
        help_place_written(one->to, out);
        cli_line_text(out, "},\"severity\":");
        cli_line_decimal(out, one->severity, 1u);
        cli_line_text(out, ",\"message\":{\"kind\":\"markdown\",\"value\":");
        cli_json_quoted(out, found->text + one->message_at, one->message_size);
        cli_line_text(out, "},\"source\":\"orior\",\"code\":");
        cli_json_quoted(out, one->code, cellul.len(&(CatenaFinitaCfg){.src = one->code, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"data\":{\"orior\":[");
        int first = 1;
        for (unsigned int fix = 0u; fix < found->fixes; fix += 1u)
        {
            if (found->fix[fix].finding == at)
            {
                cli_line_text(out, first ? "" : ",");
                help_fix_written(found, &found->fix[fix], out);
                first = 0;
            }
        }
        cli_line_text(out, "]}}");
    }
    cli_line_character(out, ']');
}

unsigned long long cli_build_byte_at(const char *text, unsigned long long size, unsigned int line, unsigned int col)
{
    unsigned long long start = 0ull;
    for (unsigned int walk = 0u; walk < line; walk += 1u)
    {
        const char *const newline =
            (start < size) ? memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = (size_t)(size - start)})
                           : NULL;
        if (newline == NULL)
        {
            return size;
        }
        start = (unsigned long long)(newline - text) + 1u;
    }
    const char *const newline =
        (start < size) ? memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = (size_t)(size - start)})
                       : NULL;
    const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
    return start + cli_text_byte_of(text + start, end - start, col);
}

int cli_build_word_letter(unsigned int letter)
{
    return cli_text_alphanumeric(letter) || (letter == '_') || (letter == '$');
}

unsigned long long cli_build_run_before(const char *text, unsigned long long at, int (*of)(unsigned int letter))
{
    unsigned long long line_start = at;
    while ((line_start > 0u) && (text[line_start - 1u] != '\n'))
    {
        line_start -= 1u;
    }
    unsigned long long start = at;
    while (start > line_start)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter_before(text + line_start, start - line_start, &length);
        if (!of(letter))
        {
            break;
        }
        start -= length;
    }
    return start;
}

void cli_build_relative(CliCompletions *items, const char *token, unsigned long long token_size, const char *value,
                        unsigned long long value_size, const char *kind, const char *detail,
                        unsigned long long detail_size)
{
    const unsigned long long base = cli_build_run_before(token, token_size, cli_build_word_letter);
    if ((value_size <= base) || (memor.cmp(&(MemoriaCfg){.src = value, .other = token, .bytes = (size_t)base}) != 0))
    {
        return;
    }
    (void)cli_completion_put(items, value + base, value_size - base, kind, detail, detail_size, value + base,
                             value_size - base, 0);
}

void cli_build_items(CliCompletions *items, const char *const *words, unsigned int count, const char *kind,
                     const char *detail)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long label = cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL});
        while ((label >= 2u) &&
               (memor.cmp(&(MemoriaCfg){.src = words[at] + label - 2u, .other = "()", .bytes = 2u}) == 0))
        {
            label -= 2u;
        }
        (void)cli_completion_put(items, words[at], label, kind, detail,
                                 cellul.len(&(CatenaFinitaCfg){.src = detail, .cap = CLI_CELLS_POOL}), words[at],
                                 cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL}), 0);
    }
}

void cli_build_inside(const char *root, const char *folder, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    const char after = folder[length];
    if (!cli_text_same(folder, root, length) ||
        ((after != '\0') && (after != '/') && (after != '\\') && (length > 0u) && (root[length - 1u] != '/') &&
         (root[length - 1u] != '\\')))
    {
        out[0] = '\0';
        return;
    }
    const char *rest = folder + length;
    while ((*rest == '/') || (*rest == '\\'))
    {
        rest += 1;
    }
    for (; *rest != '\0'; rest += 1)
    {
        cli_line_character(&line, (*rest == '\\') ? '/' : *rest);
    }
    while ((line.at > 0u) && (out[line.at - 1u] == '/'))
    {
        line.at -= 1u;
    }
    (void)cli_line_closed(&line);
}

int cli_build_snapshot(const char *version, unsigned long long size)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(version, size, &from, &to);
    return cli_text_ends(version + from, to - from, "-SNAPSHOT");
}
