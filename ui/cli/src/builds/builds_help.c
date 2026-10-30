// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_help.c: what the build files' readers and the schemas' share: a text's lines and the place of a byte; a
// file's findings and their fixes, stored and written as the protocol's diagnostics; the completion items of what was
// typed; a folder as a path in the tree; and a snapshot's version
#include "../text/json_tree.h"
#include "../text/text.h"
#include "builds.h"

CellOwner g_cli_build_cells = MMGR_CELL_OWNER("cli_builds");

unsigned int *cli_build_lines_taken(const char *text, unsigned long long size, CliBuildLines *lines)
{
    unsigned long long count = 1ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        count += (text[at] == '\n') ? 1ull : 0ull;
    }
    unsigned int *const starts =
        (count <= 0xffffffffull) ? cli_lane_alloc(count * sizeof(unsigned int), &g_cli_build_cells)
                                 : NULL;
    // a count no more than 0xffffffff where the cell is taken, which an unsigned int stores
    cli_build_lines_of(text, size, starts, (starts != NULL) ? (unsigned int)count : 0u, lines);
    return starts;
}

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
    cli_build_findings_given(found);
}

void cli_build_findings_given(CliBuildFindings *found)
{
    cli_cell_release(found->finding, &g_cli_build_cells);
    cli_cell_release(found->fix, &g_cli_build_cells);
    cli_cell_release(found->text, &g_cli_build_cells);
    *found = (CliBuildFindings){NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, 0};
}

// `list`, `count` items of `each` bytes in a room of `*room`, made to fit `wanted` in all, `most` at the most: the
// list, moved into a cell of doubled room where it is full; or NULL with the findings full where `wanted` is past
// `most` or the cell is refused
static void *help_room(CliBuildFindings *found, void *list, unsigned int count, unsigned int *room,
                       unsigned long long each, unsigned long long wanted, unsigned int most, const char *what)
{
    if (wanted <= *room)
    {
        return list;
    }
    unsigned long long grown = (*room == 0u) ? CLI_BUILD_FIRST : *room;
    while (grown < wanted)
    {
        grown *= 2ull;
    }
    grown = (grown < most) ? grown : most;
    void *const larger =
        (wanted <= most) ? cli_cell_regrown(list, (size_t)(count * each), grown * each, &g_cli_build_cells) : NULL;
    if (larger == NULL)
    {
        found->full = 1;
        return NULL;
    }
    // a room no more than `most`, which an unsigned int stores
    *room = (unsigned int)grown;
    return larger;
}

// room made in the texts for `size` bytes and their terminator past those stored, and CLI_BUILD_MSG_MIN_LEN more where
// the texts fit them: 1, or 0 with the findings full
static int help_text_room(CliBuildFindings *found, unsigned long long size)
{
    const unsigned long long needed = found->used + size + 1ull;
    const unsigned long long wanted =
        ((needed + CLI_BUILD_MSG_MIN_LEN) < CLI_BUILD_TEXT) ? (needed + CLI_BUILD_MSG_MIN_LEN) : CLI_BUILD_TEXT;
    char *const texts = help_room(found, found->text, found->used, &found->text_room, 1ull,
                                  (needed <= CLI_BUILD_TEXT) ? wanted : needed, CLI_BUILD_TEXT, "a file's findings");
    found->text = (texts != NULL) ? texts : found->text;
    return texts != NULL;
}

VerbaLinea cli_build_msg_line(CliBuildFindings *found)
{
    (void)help_text_room(found, 0ull);
    return (found->text != NULL) ? (VerbaLinea){found->text + found->used, found->text_room - found->used, 0ull}
                                 : (VerbaLinea){NULL, 0ull, 0ull};
}

// `line` stored in the findings' texts, in place where it was written there: where it starts, or CLI_BUILD_TEXT where
// it does not fit
static unsigned int help_text_stored(CliBuildFindings *found, const char *bytes, unsigned long long size)
{
    // a line written in place already has its room, and nothing it points at is moved; any other is copied in
    const int in_place = (found->text != NULL) && (bytes == found->text + found->used);
    const int room = in_place ? ((found->used + size + 1u) <= found->text_room) : help_text_room(found, size);
    if (!room)
    {
        found->full = 1;
        return CLI_BUILD_TEXT;
    }
    const unsigned int at = found->used;
    if (!in_place)
    {
        memor.move_down(&(MemoriaCfg){.dst = found->text + at, .src = bytes, .bytes = (size_t)size});
    }
    found->text[at + size] = '\0';
    found->used += (unsigned int)size + 1u;
    return at;
}

// a line written in the findings' texts stored: where it starts, or CLI_BUILD_TEXT where it did not fit
static unsigned int help_line_stored(CliBuildFindings *found, const VerbaLinea *line)
{
    if (!mmgr_line_ok(line))
    {
        found->full = 1;
        return CLI_BUILD_TEXT;
    }
    return help_text_stored(found, line->out, line->at);
}

unsigned int cli_build_finding(CliBuildFindings *found, const VerbaLinea *message, const CliBuildLines *lines,
                               unsigned long long from, unsigned long long to, unsigned int severity, const char *code)
{
    CliBuildFinding *const findings =
        help_room(found, found->finding, found->count, &found->finding_room, sizeof(CliBuildFinding),
                  found->count + 1ull, CLI_BUILD_FINDINGS, "a file's findings");
    if (findings == NULL)
    {
        return CLI_BUILD_NONE;
    }
    found->finding = findings;
    const unsigned int at = help_line_stored(found, message);
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
static unsigned int help_fix(CliBuildFindings *found, unsigned int finding, const VerbaLinea *title)
{
    CliBuildFix *const fixes = (finding != CLI_BUILD_NONE)
                                   ? help_room(found, found->fix, found->fixes, &found->fix_room, sizeof(CliBuildFix),
                                               found->fixes + 1ull, CLI_BUILD_FIXES, "a file's fixes")
                                   : NULL;
    if (fixes == NULL)
    {
        found->full = 1;
        return CLI_BUILD_FIXES;
    }
    found->fix = fixes;
    const unsigned int at = help_line_stored(found, title);
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

void cli_build_edit_fix(CliBuildFindings *found, unsigned int finding, const VerbaLinea *title,
                        const CliBuildLines *lines, unsigned long long from, unsigned long long to, const char *text,
                        unsigned long long size)
{
    const unsigned int at = help_fix(found, finding, title);
    if (at == CLI_BUILD_FIXES)
    {
        return;
    }
    CliBuildFix *const fix = &found->fix[at];
    fix->from = cli_build_place(lines, from);
    fix->to = cli_build_place(lines, to);
    fix->text_at = help_text_stored(found, text, size);
    fix->text_size = (unsigned int)size;
    if (fix->text_at != CLI_BUILD_TEXT)
    {
        found->fixes += 1u;
    }
}

void cli_build_edit_fix_twice(CliBuildFindings *found, unsigned int finding, const VerbaLinea *title,
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

void cli_build_run_fix(CliBuildFindings *found, unsigned int finding, const VerbaLinea *title, const char *folder,
                       unsigned long long folder_size, const char *line, unsigned long long line_size)
{
    const unsigned int at = help_fix(found, finding, title);
    if (at == CLI_BUILD_FIXES)
    {
        return;
    }
    CliBuildFix *const fix = &found->fix[at];
    fix->run = 1;
    fix->folder_at = help_text_stored(found, folder, folder_size);
    fix->folder_size = (unsigned int)folder_size;
    fix->text_at = help_text_stored(found, line, line_size);
    fix->text_size = (unsigned int)line_size;
    if ((fix->folder_at != CLI_BUILD_TEXT) && (fix->text_at != CLI_BUILD_TEXT))
    {
        found->fixes += 1u;
    }
}

// a place written as the protocol's `{"line", "character"}`
static void help_place_written(CliTextPlace place, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"line\":");
    mmgr_line_decimal(out, place.line, 1u);
    mmgr_line_put(out, ",\"character\":");
    mmgr_line_decimal(out, place.col, 1u);
    mmgr_line_ch(out, '}');
}

// the fix at `at` written
static void help_fix_written(const CliBuildFindings *found, const CliBuildFix *fix, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"title\":");
    cli_json_quoted(out, found->text + fix->title_at, fix->title_size);
    if (fix->run)
    {
        mmgr_line_put(out, ",\"run\":{\"folder\":");
        cli_json_quoted(out, found->text + fix->folder_at, fix->folder_size);
        mmgr_line_put(out, ",\"line\":");
        cli_json_quoted(out, found->text + fix->text_at, fix->text_size);
        mmgr_line_put(out, "}}");
        return;
    }
    const CliTextEdit edit = {fix->from, fix->to, found->text + fix->text_at, fix->text_size};
    mmgr_line_put(out, ",\"edits\":[");
    cli_text_edit_written(&edit, out);
    if (fix->also)
    {
        const CliTextEdit also = {fix->also_from, fix->also_to, found->text + fix->text_at, fix->text_size};
        mmgr_line_ch(out, ',');
        cli_text_edit_written(&also, out);
    }
    mmgr_line_put(out, "]}");
}

void cli_build_findings_written(const CliBuildFindings *found, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliBuildFinding *const one = &found->finding[at];
        mmgr_line_put(out, (at > 0u) ? ",{\"range\":{\"start\":" : "{\"range\":{\"start\":");
        help_place_written(one->from, out);
        mmgr_line_put(out, ",\"end\":");
        help_place_written(one->to, out);
        mmgr_line_put(out, "},\"severity\":");
        mmgr_line_decimal(out, one->severity, 1u);
        mmgr_line_put(out, ",\"message\":{\"kind\":\"markdown\",\"value\":");
        cli_json_quoted(out, found->text + one->message_at, one->message_size);
        mmgr_line_put(out, "},\"source\":\"orior\",\"code\":");
        cli_json_quoted(out, one->code, cellul.len(&(CatenaFinitaCfg){.src = one->code, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"data\":{\"orior\":[");
        int first = 1;
        for (unsigned int fix = 0u; fix < found->fixes; fix += 1u)
        {
            if (found->fix[fix].finding == at)
            {
                mmgr_line_put(out, first ? "" : ",");
                help_fix_written(found, &found->fix[fix], out);
                first = 0;
            }
        }
        mmgr_line_put(out, "]}}");
    }
    mmgr_line_ch(out, ']');
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
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
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
        mmgr_line_ch(&line, (*rest == '\\') ? '/' : *rest);
    }
    while ((line.at > 0u) && (out[line.at - 1u] == '/'))
    {
        line.at -= 1u;
    }
    (void)mmgr_line_finish(&line);
}

int cli_build_snapshot(const char *version, unsigned long long size)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(version, size, &from, &to);
    return cli_text_ends(version + from, to - from, "-SNAPSHOT");
}
