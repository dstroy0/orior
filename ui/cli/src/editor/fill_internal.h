// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_internal.h: what the fill's sources share: a line of the text by its bounds and its place, the lines written
// into the edit's text, the texts whose words are filled, and words filled into lines no wider than the margin
#ifndef FILL_INTERNAL_H
#define FILL_INTERNAL_H

#include "../text/text.h"
#include "fill.h"

// the most texts whose words one fill takes
#define CLI_FILL_SLICES (1u << 20u)

// A line of the text, split at each "\n": where it starts, where it ends, and its place among the lines
typedef struct
{
    unsigned long long start;
    unsigned long long end;
    unsigned int index;
} CliFillLine;

// The lines written into the edit's text: where they go, where the last starts, and how many there are
typedef struct
{
    CliLine *out;
    unsigned long long line_start;
    unsigned int lines;
} CliFilled;

// the texts whose words are filled, each where it starts and its size, a cell taken under the fill's lock with room
// for `g_cli_fill_slices` of them, each a line of the text or a place after the lines (fill.c)
extern const char **g_cli_fill_slice;
extern unsigned long long *g_cli_fill_slice_size;
extern unsigned int g_cli_fill_slices;

// the line at `index` of `size` bytes of `text`: 1, or 0 where it has none; the line before `line`, and the line after
// it, each into it: 1, or 0 where it has none (fill.c)
int cli_fill_line_at(const char *text, unsigned long long size, unsigned int index, CliFillLine *line);
int cli_fill_before(const char *text, CliFillLine *line);
int cli_fill_after(const char *text, unsigned long long size, CliFillLine *line);

// a line begun after the last; the text of the last; a line of `size` bytes of `bytes` put whole; and the last taken
// off (fill.c)
void cli_filled_begun(CliFilled *filled);
const char *cli_filled_last(const CliFilled *filled, unsigned long long *size);
void cli_filled_line(CliFilled *filled, const char *bytes, unsigned long long size);
void cli_filled_dropped(CliFilled *filled);

// the words of the texts `slice` to `slice` and `count`, filled into lines no wider than `width` letters, the first
// starting with `first` and each after it with `rest`, each line's whitespace at its end taken off (fill.c)
void cli_fill_words(CliFilled *filled, unsigned int slice, unsigned int count, const char *first,
                    unsigned long long first_size, const char *rest, unsigned long long rest_size, unsigned int width);

// the edit that writes the lines `first` to `last` as the lines written (fill.c)
void cli_fill_edit_of(const char *text, const CliFillLine *first, const CliFillLine *last, const CliFilled *filled,
                      CliTextEdit *edit);

// the `/** */` comment around `at`, its paragraphs filled and its tags lined up: 1 with its edit, or 0 where `at` is
// in none (fill_comment.c)
int cli_fill_comment(const char *text, unsigned long long size, const CliFillLine *at, unsigned int width,
                     CliFilled *filled, CliTextEdit *edit);
// the comment fill's room, a cell taken under the fill's lock: 1, or 0 with why said; and the room given back before
// the lock is (fill_comment.c)
int cli_fill_comment_held(CliLine *said);
void cli_fill_comment_released(void);

// the paragraph of the Python docstring around `at`, up to a blank line or a line of the closing quotes alone, the
// opening quotes kept at the head of its first line and the closing quotes at the end of its last: 1 with its edit,
// or 0 where `at` stands in none (fill_docstring.c)
int cli_fill_docstring(const char *text, unsigned long long size, const CliFillLine *at, unsigned int width,
                       CliFilled *filled, CliTextEdit *edit);

#endif
