// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_text.c: the measures the review takes of a text: its whitespace at either end, the lines two texts hold
// alike, and whether a text calls a name
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "review_internal.h"

// A text's lines, each where it starts and its bytes, less a carriage return before its newline, in a cell with room
// for `most` of them
typedef struct
{
    unsigned int *start;
    unsigned int *length;
    unsigned int lines;
    unsigned int most;
} CliTextLines;

// the lines `size` bytes of `text` can hold, its newlines and one, up to CLI_REVIEW_LINES
static unsigned int text_lines_most(const char *text, unsigned long long size)
{
    unsigned int most = 1u;
    for (unsigned long long at = 0ull; (at < size) && (most < CLI_REVIEW_LINES); at += 1ull)
    {
        most += (text[at] == '\n') ? 1u : 0u;
    }
    return most;
}

// a text's lines read, `size` bytes of it, up to the lines it has room for
static void cli_lines_of(const char *text, unsigned long long size, CliTextLines *lines)
{
    lines->lines = 0u;
    for (unsigned long long at = 0ull; (at < size) && (lines->lines < lines->most);)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        // a text is below CLI_REVIEW_KEPT bytes, which an unsigned int holds
        lines->start[lines->lines] = (unsigned int)at;
        lines->length[lines->lines] = (unsigned int)length;
        lines->lines += 1u;
        at = end + 1ull;
    }
}

void cli_review_trimmed(const char **text, unsigned long long *length)
{
    unsigned long long start = *length;
    unsigned long long end = 0ull;
    for (unsigned long long at = 0ull; at < *length;)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)*text, *length, at, &letter);
        if (!cli_fuzzy_white(letter))
        {
            start = (start == *length) ? at : start;
            end = at + step;
        }
        at += step;
    }
    *text = (start < end) ? (*text + start) : *text;
    *length = (start < end) ? (end - start) : 0ull;
}

int cli_review_share(const char *one, const char *other, unsigned long long *same, unsigned long long *longest)
{
    const unsigned long long one_size = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
    const unsigned long long other_size = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_CELLS_POOL});
    // both texts' lines, and a mark for each of the first's that one of the second's holds, in one cell
    const unsigned int one_most = text_lines_most(one, one_size);
    const unsigned int other_most = text_lines_most(other, other_size);
    const unsigned long long words = (2ull * one_most) + (2ull * other_most);
    unsigned int *const cell = cli_cell_taken_by(&g_cli_review_cells, (words * sizeof(unsigned int)) + one_most,
                                                 "the lines of two texts compared");
    if (cell == NULL)
    {
        return 0;
    }
    CliTextLines first = {cell, cell + one_most, 0u, one_most};
    CliTextLines second = {cell + (2u * one_most), cell + (2u * one_most) + other_most, 0u, other_most};
    unsigned char *const used = (unsigned char *)(cell + words);
    cli_lines_of(one, one_size, &first);
    cli_lines_of(other, other_size, &second);
    *longest = (first.lines > second.lines) ? first.lines : second.lines;
    *same = 0ull;
    memor.set(&(MemoriaCfg){.dst = used, .val = 0, .bytes = first.lines});
    for (unsigned int right = 0u; right < second.lines; right += 1u)
    {
        const char *right_text = other + second.start[right];
        unsigned long long right_length = second.length[right];
        cli_review_trimmed(&right_text, &right_length);
        for (unsigned int left = 0u; left < first.lines; left += 1u)
        {
            const char *left_text = one + first.start[left];
            unsigned long long left_length = first.length[left];
            cli_review_trimmed(&left_text, &left_length);
            if (!used[left] && (left_length == right_length) &&
                (memor.cmp(&(MemoriaCfg){.src = left_text, .other = right_text, .bytes = left_length}) == 0))
            {
                used[left] = 1u;
                *same += 1ull;
                break;
            }
        }
    }
    cli_cell_given_by(&g_cli_review_cells, cell);
    // two texts of no lines hold all alike
    if (*longest == 0ull)
    {
        *same = 1ull;
        *longest = 1ull;
    }
    return 1;
}

int cli_review_calls(const char *text, const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (length == 0ull)
    {
        return 0;
    }
    for (const char *at = cellul.find(
             &(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = name, .other_cap = CLI_CELLS_POOL});
         at != NULL; at = cellul.find(&(CatenaFinitaCfg){
                         .src = at + 1, .cap = CLI_CELLS_POOL, .other = name, .other_cap = CLI_CELLS_POOL}))
    {
        // the letter before the name, read back to where it starts
        const char *before = at;
        while ((before > text) && ((((unsigned char)before[-1]) & 0xc0u) == 0x80u))
        {
            before -= 1;
        }
        unsigned int letter = 0u;
        int joined = 0;
        if (before > text)
        {
            before -= 1;
            cli_lossy_letter((const unsigned char *)before, (unsigned long long)(at - before), 0ull, &letter);
            joined = cli_hit_wordy(letter);
        }
        const char *after = at + length;
        unsigned long long rest = cellul.len(&(CatenaFinitaCfg){.src = after, .cap = CLI_CELLS_POOL});
        while (rest > 0ull)
        {
            const unsigned long long step = cli_lossy_letter((const unsigned char *)after, rest, 0ull, &letter);
            if (!cli_fuzzy_white(letter))
            {
                break;
            }
            after += step;
            rest -= step;
        }
        if (!joined && (after[0] == '('))
        {
            return 1;
        }
    }
    return 0;
}
