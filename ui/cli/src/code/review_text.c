// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_text.c: the measures the review takes of a text: its whitespace at either end, the lines two texts hold
// alike, and whether a text calls a name
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "review_internal.h"

// A text's lines, each where it starts and its bytes, less a carriage return before its newline
typedef struct
{
    unsigned int start[CLI_REVIEW_LINES];
    unsigned int length[CLI_REVIEW_LINES];
    unsigned int lines;
} CliTextLines;

// a text's lines read
static void cli_lines_of(const char *text, CliTextLines *lines)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    lines->lines = 0u;
    for (unsigned long long at = 0ull; (at < size) && (lines->lines < CLI_REVIEW_LINES);)
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

void cli_review_share(const char *one, const char *other, unsigned long long *same, unsigned long long *longest)
{
    static CliTextLines s_one;
    static CliTextLines s_other;
    static unsigned char s_used[CLI_REVIEW_LINES];
    cli_lines_of(one, &s_one);
    cli_lines_of(other, &s_other);
    *longest = (s_one.lines > s_other.lines) ? s_one.lines : s_other.lines;
    *same = 0ull;
    memor.set(&(MemoriaCfg){.dst = s_used, .val = 0, .bytes = s_one.lines});
    for (unsigned int right = 0u; right < s_other.lines; right += 1u)
    {
        const char *right_text = other + s_other.start[right];
        unsigned long long right_length = s_other.length[right];
        cli_review_trimmed(&right_text, &right_length);
        for (unsigned int left = 0u; left < s_one.lines; left += 1u)
        {
            const char *left_text = one + s_one.start[left];
            unsigned long long left_length = s_one.length[left];
            cli_review_trimmed(&left_text, &left_length);
            if (!s_used[left] && (left_length == right_length) &&
                (memor.cmp(&(MemoriaCfg){.src = left_text, .other = right_text, .bytes = left_length}) == 0))
            {
                s_used[left] = 1u;
                *same += 1ull;
                break;
            }
        }
    }
    // two texts of no lines hold all alike
    if (*longest == 0ull)
    {
        *same = 1ull;
        *longest = 1ull;
    }
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
