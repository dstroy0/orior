// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_text.c: the measures the review takes of a text: its whitespace at either end, the lines two texts contain
// alike, and whether a text calls a name
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "review_internal.h"

// A text's lines, each where it starts and its bytes, less a carriage return before its newline, in a cell with
// capacity for `most` of them
typedef struct
{
    unsigned int *start;
    unsigned int *length;
    unsigned int lines;
    unsigned int most;
} CliTextLines;

typedef struct
{
    const char *text;
    unsigned long long size;
    CliTextLines *lines;
} ReviewTextCtx;
// the lines `size` bytes of `text` can contain, its newlines and one, up to CLI_REVIEW_LINES
EMBED_INLINE unsigned int text_lines_max(const ReviewTextCtx *args)
{
    unsigned int most = 1u;
    for (unsigned long long at = 0ULL; (at < args->size) && (most < CLI_REVIEW_LINES); at += 1ULL)
    {
        most += (args->text[at] == '\n') ? 1u : 0u;
    }
    return most;
}

// a text's lines read, `size` bytes of it, up to the lines it has capacity for
EMBED_INLINE void cli_lines_get(const ReviewTextCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    CliTextLines *lines = args->lines;
    lines->lines = 0u;
    for (unsigned long long at = 0ULL; (at < size) && (lines->lines < lines->most);)
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        // a text is below CLI_REVIEW_TEXTS_MAX bytes, which an unsigned int stores
        lines->start[lines->lines] = (unsigned int)at;
        lines->length[lines->lines] = (unsigned int)length;
        lines->lines += 1u;
        at = end + 1ULL;
    }
}

void cli_review_trim(const CliReviewTextLengthTypeCtx *args)
{
    const char **text = args->text_pointer;
    unsigned long long *length = args->length_pointer;
    unsigned long long start = *length;
    unsigned long long end = 0ULL;
    for (unsigned long long at = 0ULL; at < *length;)
    {
        unsigned int letter = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)*text, .length = *length, .at = at, .letter = &letter);
        if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
        {
            start = (start == *length) ? at : start;
            end = at + step;
        }
        at += step;
    }
    *text = (start < end) ? (*text + start) : *text;
    *length = (start < end) ? (end - start) : 0ULL;
}

int cli_review_share(const CliReviewOneCtx *args)
{
    const char *other = args->other;
    unsigned long long *same = args->same;
    unsigned long long *longest = args->longest;
    const unsigned long long one_size = cellul.len(&(CatenaFinitaCfg){.src = args->one, .cap = CLI_POOL_BYTES});
    const unsigned long long other_size = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_POOL_BYTES});
    // both texts' lines, and a mark for each of the first's that one of the second's contains, in one cell
    const unsigned int one_max = EMBED_CALL(text_lines_max, ReviewTextCtx, .text = args->one, .size = one_size);
    const unsigned int other_max = EMBED_CALL(text_lines_max, ReviewTextCtx, .text = other, .size = other_size);
    const unsigned long long words = (2ULL * one_max) + (2ULL * other_max);
    unsigned int *const cell =
        g_cli_guard->temporary_buf_alloc((words * sizeof(unsigned int)) + one_max, g_cli_review_owner);
    if (cell == NULL)
    {
        return 0;
    }
    CliTextLines first = {cell, cell + one_max, 0u, one_max};
    CliTextLines second = {cell + (2u * one_max), cell + (2u * one_max) + other_max, 0u, other_max};
    unsigned char *const used = (unsigned char *)(cell + words);
    EMBED_CALL(cli_lines_get, ReviewTextCtx, .text = args->one, .size = one_size, .lines = &first);
    EMBED_CALL(cli_lines_get, ReviewTextCtx, .text = other, .size = other_size, .lines = &second);
    *longest = (first.lines > second.lines) ? first.lines : second.lines;
    *same = 0ULL;
    memor.set(&(MemoriaCfg){.dst = used, .val = 0, .bytes = first.lines});
    for (unsigned int right = 0u; right < second.lines; right += 1u)
    {
        const char *right_text = other + second.start[right];
        unsigned long long right_length = second.length[right];
        EMBED_CALL(cli_review_text_length_type_internal.trim, CliReviewTextLengthTypeCtx, .text_pointer = &right_text,
                   .length_pointer = &right_length);
        for (unsigned int left = 0u; left < first.lines; left += 1u)
        {
            const char *left_text = args->one + first.start[left];
            unsigned long long left_length = first.length[left];
            EMBED_CALL(cli_review_text_length_type_internal.trim, CliReviewTextLengthTypeCtx,
                       .text_pointer = &left_text, .length_pointer = &left_length);
            if (!used[left] && (left_length == right_length) &&
                (memor.cmp(&(MemoriaCfg){.src = left_text, .other = right_text, .bytes = left_length}) == 0))
            {
                used[left] = 1u;
                *same += 1ULL;
                break;
            }
        }
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_review_owner);
    // two texts of no lines contain all alike
    if (*longest == 0ULL)
    {
        *same = 1ULL;
        *longest = 1ULL;
    }
    return 1;
}

int cli_review_calls(const CliReviewCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    if (length == 0ULL)
    {
        return 0;
    }
    for (const char *at = cellul.find(&(CatenaFinitaCfg){
             .src = args->text, .cap = CLI_POOL_BYTES, .other = args->name, .other_cap = CLI_POOL_BYTES});
         at != NULL; at = cellul.find(&(CatenaFinitaCfg){
                         .src = at + 1, .cap = CLI_POOL_BYTES, .other = args->name, .other_cap = CLI_POOL_BYTES}))
    {
        // the letter before the name, read back to where it starts
        const char *before = at;
        while ((before > args->text) && ((((unsigned char)before[-1]) & 0xc0u) == 0x80u))
        {
            before -= 1;
        }
        unsigned int letter = 0u;
        int joined = 0;
        if (before > args->text)
        {
            before -= 1;
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)before, .length = (unsigned long long)(at - before),
                       .at = 0ULL, .letter = &letter);
            joined = EMBED_CALL(cli_files_letter_internal.hit_wordy, CliFilesLetterCtx, .letter = letter);
        }
        const char *after = at + length;
        unsigned long long rest = cellul.len(&(CatenaFinitaCfg){.src = after, .cap = CLI_POOL_BYTES});
        while (rest > 0ULL)
        {
            const unsigned long long step =
                EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                           .text_bytes = (const unsigned char *)after, .length = rest, .at = 0ULL, .letter = &letter);
            if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
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
