// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// edits.h: a span of a text and what is written over it, and the completion items, as the language servers, the build
// files' and the schemas' readers and the fills give them: a place's line from 0 and its column in UTF-16 units from 0,
// as the protocol counts them. Edits are written over a text each placed against the text as it stood before any, the
// first in the text first; a line ends at its "\n", and a "\r" before that stays the line's end (edits.c,
// completions.c)
#ifndef EDITS_H
#define EDITS_H

#include "../command_line/cli_internal.h"

// the most edits written over a text at once
#define CLI_TEXT_EDITS 65536u

// A place in a text: its line and its column in UTF-16 units, each from 0
typedef struct
{
    unsigned int line;
    unsigned int col;
} CliTextPlace;

// One span of a text and what is written over it, `size` bytes
typedef struct
{
    CliTextPlace from;
    CliTextPlace to;
    const char *text;
    unsigned long long size;
} CliTextEdit;

// a place written as `{"line", "col"}`, and an edit as `{"from", "to", "text"}`
void cli_text_place_written(CliTextPlace place, CliLine *out);
void cli_text_edit_written(const CliTextEdit *edit, CliLine *out);

// the byte where the UTF-16 column `col` of `size` bytes of a line falls, the line's end where it is past it
unsigned long long cli_text_byte_of(const char *line, unsigned long long size, unsigned int col);

// `size` bytes of `text` with `edits`, `count` of them, written over it into `out`: 1, or 0 where there are more than
// CLI_TEXT_EDITS of them
int cli_text_applied(const char *text, unsigned long long size, const CliTextEdit *edits, unsigned int count,
                     CliLine *out);

// the most completion items one ask gives, and the bytes their texts take
#define CLI_COMPLETIONS_MOST 4096u
#define CLI_COMPLETIONS_TEXT (1u << 20u)

// A completion item: what it shows, its kind, what it says of itself and what it inserts, each a text of its items',
// and whether what it inserts is a snippet, its stops written `$1` and `${1:name}`
typedef struct
{
    unsigned int label_at;
    unsigned int label_size;
    const char *kind;
    unsigned int detail_at;
    unsigned int detail_size;
    unsigned int insert_at;
    unsigned int insert_size;
    int snippet;
} CliCompletion;

// Completion items and the texts they take, with 1 in `full` where one did not fit
typedef struct
{
    CliCompletion item[CLI_COMPLETIONS_MOST];
    unsigned int count;
    char text[CLI_COMPLETIONS_TEXT];
    unsigned int used;
    int full;
} CliCompletions;

// items made empty; an item put after the others: 1, or 0 where it does not fit; the items made unique by what they
// show, the first of each kept; and an item written as the page reads one, `{"label", "kind", "detail", "insert"}`
// and `"snippet": true` where it is one (completions.c)
void cli_completions_begun(CliCompletions *items);
int cli_completion_put(CliCompletions *items, const char *label, unsigned long long label_size, const char *kind,
                       const char *detail, unsigned long long detail_size, const char *insert,
                       unsigned long long insert_size, int snippet);
void cli_completions_unique(CliCompletions *items);
void cli_completion_written(const CliCompletions *items, unsigned int at, CliLine *out);

#endif
