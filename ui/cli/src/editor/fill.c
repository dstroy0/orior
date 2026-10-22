// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill.c: Fill Paragraph: the text's lines walked from the cursor's, the paragraph's words gathered by the texts they
// stand in, and filled into lines no wider than the margin. A paragraph of a comment a line takes the lines that start
// with the cursor line's indent and mark and hold text after it, a line of a longer mark not among them; a docstring's
// the lines of its quotes that are not blank; plain text's the lines that are not blank. One fill runs at a time
#include "fill_internal.h"

const char **g_cli_fill_slice;
unsigned long long *g_cli_fill_slice_size;
unsigned int g_cli_fill_slices;

static CliLock s_fill_lock = CLI_LOCK_INIT;

// the marks a comment line starts with, the longest first
static const char *const s_marks[6] = {"///", "//!", "//", "##", "#", "--"};

int cli_fill_line_at(const char *text, unsigned long long size, unsigned int index, CliFillLine *line)
{
    line->start = 0ull;
    line->index = 0u;
    for (;;)
    {
        const char *const newline =
            (line->start < size) ? memor.chr(&(MemoriaCfg){
                                       .src = text + line->start, .val = '\n', .bytes = (size_t)(size - line->start)})
                                 : NULL;
        line->end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        if (line->index == index)
        {
            return 1;
        }
        if (newline == NULL)
        {
            return 0;
        }
        line->start = line->end + 1u;
        line->index += 1u;
    }
}

int cli_fill_before(const char *text, CliFillLine *line)
{
    if (line->index == 0u)
    {
        return 0;
    }
    line->end = line->start - 1u;
    line->start = line->end;
    while ((line->start > 0u) && (text[line->start - 1u] != '\n'))
    {
        line->start -= 1u;
    }
    line->index -= 1u;
    return 1;
}

int cli_fill_after(const char *text, unsigned long long size, CliFillLine *line)
{
    if (line->end >= size)
    {
        return 0;
    }
    line->start = line->end + 1u;
    const char *const newline =
        (line->start < size)
            ? memor.chr(&(MemoriaCfg){.src = text + line->start, .val = '\n', .bytes = (size_t)(size - line->start)})
            : NULL;
    line->end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
    line->index += 1u;
    return 1;
}

void cli_filled_begun(CliFilled *filled)
{
    if (filled->lines > 0u)
    {
        cli_line_character(filled->out, '\n');
    }
    filled->line_start = filled->out->at;
    filled->lines += 1u;
}

const char *cli_filled_last(const CliFilled *filled, unsigned long long *size)
{
    *size = (filled->lines > 0u) ? filled->out->at - filled->line_start : 0ull;
    return filled->out->out + filled->line_start;
}

void cli_filled_line(CliFilled *filled, const char *bytes, unsigned long long size)
{
    cli_filled_begun(filled);
    cli_line_counted(filled->out, bytes, size);
}

void cli_filled_dropped(CliFilled *filled)
{
    if (filled->lines == 0u)
    {
        return;
    }
    filled->lines -= 1u;
    filled->out->at = (filled->lines > 0u) ? filled->line_start - 1u : 0ull;
    unsigned long long start = filled->out->at;
    while ((filled->lines > 0u) && (start > 0u) && (filled->out->out[start - 1u] != '\n'))
    {
        start -= 1u;
    }
    filled->line_start = start;
}

// the last line written with its whitespace at its end taken off
static void fill_ended(CliFilled *filled)
{
    const unsigned long long size = filled->out->at - filled->line_start;
    filled->out->at = filled->line_start + cli_text_trimmed_end(filled->out->out + filled->line_start, size);
}

void cli_fill_words(CliFilled *filled, unsigned int slice, unsigned int count, const char *first,
                    unsigned long long first_size, const char *rest, unsigned long long rest_size, unsigned int width)
{
    cli_filled_line(filled, first, first_size);
    unsigned long long letters = cli_text_letters(first, first_size);
    int empty = 1;
    for (unsigned int at = slice; at < slice + count; at += 1u)
    {
        const char *const text = g_cli_fill_slice[at];
        const unsigned long long size = g_cli_fill_slice_size[at];
        unsigned long long walk = 0ull;
        unsigned long long word = 0ull;
        unsigned long long length = 0ull;
        while (cli_text_word(text, size, &walk, &word, &length))
        {
            const unsigned long long word_letters = cli_text_letters(text + word, length);
            const unsigned long long lead = empty ? letters : letters + 1u;
            if (!empty && (lead + word_letters > width))
            {
                fill_ended(filled);
                cli_filled_line(filled, rest, rest_size);
                letters = cli_text_letters(rest, rest_size);
                empty = 1;
            }
            if (!empty)
            {
                cli_line_character(filled->out, ' ');
                letters += 1u;
            }
            cli_line_counted(filled->out, text + word, length);
            letters += word_letters;
            empty = 0;
        }
    }
    fill_ended(filled);
}

void cli_fill_edit_of(const char *text, const CliFillLine *first, const CliFillLine *last, const CliFilled *filled,
                      CliTextEdit *edit)
{
    edit->from.line = first->index;
    edit->from.col = 0u;
    edit->to.line = last->index;
    edit->to.col = (unsigned int)cli_text_units(text + last->start, last->end - last->start);
    edit->text = filled->out->out;
    edit->size = filled->out->at;
}

// the mark of the comment a line of `size` bytes, whitespace taken off its start, starts with: its place among the
// marks, 6 where it starts with none
static unsigned int fill_mark_of(const char *trimmed, unsigned long long size)
{
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cli_text_starts(trimmed, size, s_marks[at]))
        {
            return at;
        }
    }
    return 6u;
}

// the text after `prefix` of a line that starts with it, trimmed, into `body`: 1, or 0 where the line does not start
// with it
static int fill_body_of(const char *text, const CliFillLine *line, const char *prefix, unsigned long long prefix_size,
                        const char **body, unsigned long long *body_size)
{
    const unsigned long long size = line->end - line->start;
    if ((size < prefix_size) ||
        (memor.cmp(&(MemoriaCfg){.src = text + line->start, .other = prefix, .bytes = (size_t)prefix_size}) != 0))
    {
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(text + line->start + prefix_size, size - prefix_size, &from, &to);
    *body = text + line->start + prefix_size + from;
    *body_size = to - from;
    return 1;
}

// whether a line is of the paragraph of the comment of mark `mark` and `prefix`: it starts with the prefix and holds
// text after it, and starts with no longer mark
static int fill_in_comment(const char *text, const CliFillLine *line, unsigned int mark, const char *prefix,
                           unsigned long long prefix_size)
{
    const char *body = NULL;
    unsigned long long body_size = 0ull;
    if (!fill_body_of(text, line, prefix, prefix_size, &body, &body_size) || (body_size == 0u))
    {
        return 0;
    }
    const unsigned long long size = line->end - line->start;
    const unsigned long long lead = cli_text_lead(text + line->start, size);
    for (unsigned int other = 0u; other < 6u; other += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_marks[other], .cap = CLI_CELLS_POOL}) >
             cellul.len(&(CatenaFinitaCfg){.src = s_marks[mark], .cap = CLI_CELLS_POOL})) &&
            cli_text_starts(text + line->start + lead, size - lead, s_marks[other]))
        {
            return 0;
        }
    }
    return 1;
}

// the paragraph of a comment a line around `here`, whose mark is the one at `mark`
static int fill_comment_lines(const char *text, unsigned long long size, const CliFillLine *here, unsigned int mark,
                              unsigned int width, CliFilled *filled, CliTextEdit *edit, CliLine *said)
{
    const char *const prefix = text + here->start;
    const unsigned long long prefix_size = cli_text_lead(prefix, here->end - here->start) +
                                           cellul.len(&(CatenaFinitaCfg){.src = s_marks[mark], .cap = CLI_CELLS_POOL});
    if (!fill_in_comment(text, here, mark, prefix, prefix_size))
    {
        cli_line_text(said, "Fill Paragraph fills the paragraph the cursor is in.");
        return 0;
    }
    CliFillLine first = *here;
    CliFillLine before = first;
    while (cli_fill_before(text, &before) && fill_in_comment(text, &before, mark, prefix, prefix_size))
    {
        first = before;
    }
    CliFillLine last = *here;
    CliFillLine after = last;
    while (cli_fill_after(text, size, &after) && fill_in_comment(text, &after, mark, prefix, prefix_size))
    {
        last = after;
    }
    unsigned int count = 0u;
    for (CliFillLine line = first; count < g_cli_fill_slices;)
    {
        if (!fill_body_of(text, &line, prefix, prefix_size, &g_cli_fill_slice[count], &g_cli_fill_slice_size[count]))
        {
            g_cli_fill_slice_size[count] = 0ull;
        }
        count += 1u;
        if ((line.index == last.index) || !cli_fill_after(text, size, &line))
        {
            break;
        }
    }
    static char s_lead[1u << 16u];
    CliLine lead = {s_lead, sizeof(s_lead), 0ull};
    cli_line_counted(&lead, prefix, prefix_size);
    cli_line_character(&lead, ' ');
    cli_fill_words(filled, 0u, count, s_lead, lead.at, s_lead, lead.at, width);
    cli_fill_edit_of(text, &first, &last, filled, edit);
    return 1;
}

// whether a line is blank
static int fill_blank(const char *text, const CliFillLine *line)
{
    return cli_text_lead(text + line->start, line->end - line->start) == line->end - line->start;
}

// the paragraph of plain text around `here`
static int fill_plain(const char *text, unsigned long long size, const CliFillLine *here, unsigned int width,
                      CliFilled *filled, CliTextEdit *edit, CliLine *said)
{
    if (fill_blank(text, here))
    {
        cli_line_text(said, "Fill Paragraph fills the paragraph the cursor is in.");
        return 0;
    }
    CliFillLine first = *here;
    CliFillLine before = first;
    while (cli_fill_before(text, &before) && !fill_blank(text, &before))
    {
        first = before;
    }
    CliFillLine last = *here;
    CliFillLine after = last;
    while (cli_fill_after(text, size, &after) && !fill_blank(text, &after))
    {
        last = after;
    }
    unsigned int count = 0u;
    for (CliFillLine line = first; count < g_cli_fill_slices;)
    {
        g_cli_fill_slice[count] = text + line.start;
        g_cli_fill_slice_size[count] = line.end - line.start;
        count += 1u;
        if ((line.index == last.index) || !cli_fill_after(text, size, &line))
        {
            break;
        }
    }
    const char *const lead = text + first.start;
    const unsigned long long lead_size = cli_text_lead(lead, first.end - first.start);
    cli_fill_words(filled, 0u, count, lead, lead_size, lead, lead_size, width);
    cli_fill_edit_of(text, &first, &last, filled, edit);
    return 1;
}

int cli_fill(const char *text, unsigned long long size, unsigned int line, unsigned int width, CliLine *written,
             CliTextEdit *edit, CliLine *said)
{
    cli_lock_take(&s_fill_lock);
    if (!cli_fill_comment_held(said))
    {
        cli_lock_give(&s_fill_lock);
        return 0;
    }
    // a slice is a line of the text, and one place is left after the lines, so its newlines and two hold them
    unsigned long long slices = 2ull;
    for (unsigned long long at = 0ull; (at < size) && (slices < CLI_FILL_SLICES); at += 1ull)
    {
        slices += (text[at] == '\n') ? 1ull : 0ull;
    }
    // the slices are under CLI_FILL_SLICES, which an unsigned int holds
    g_cli_fill_slices = (unsigned int)slices;
    g_cli_fill_slice =
        cli_cell_taken(slices * (sizeof(const char *) + sizeof(unsigned long long)), "the texts a fill fills");
    if (g_cli_fill_slice == NULL)
    {
        cli_fill_comment_released();
        cli_lock_give(&s_fill_lock);
        cli_line_text(said, "the texts a fill fills could not be taken");
        return 0;
    }
    g_cli_fill_slice_size = (unsigned long long *)(void *)(g_cli_fill_slice + slices);
    CliFilled filled = {written, 0ull, 0u};
    written->at = 0ull;
    CliFillLine here = {0ull, 0ull, 0u};
    int done = 0;
    if (!cli_fill_line_at(text, size, line, &here))
    {
        cli_line_text(said, "No line is there.");
    }
    else if (cli_fill_comment(text, size, &here, width, &filled, edit))
    {
        done = 1;
    }
    else
    {
        const unsigned long long lead = cli_text_lead(text + here.start, here.end - here.start);
        const unsigned int mark = fill_mark_of(text + here.start + lead, here.end - here.start - lead);
        if (mark < 6u)
        {
            done = fill_comment_lines(text, size, &here, mark, width, &filled, edit, said);
        }
        else if (cli_fill_docstring(text, size, &here, width, &filled, edit))
        {
            done = 1;
        }
        else
        {
            done = fill_plain(text, size, &here, width, &filled, edit, said);
        }
    }
    done = done && cli_line_fits(written);
    cli_cell_given(g_cli_fill_slice);
    g_cli_fill_slice = NULL;
    g_cli_fill_slice_size = NULL;
    g_cli_fill_slices = 0u;
    cli_fill_comment_released();
    cli_lock_give(&s_fill_lock);
    return done;
}
