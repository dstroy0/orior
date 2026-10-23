// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document.h: the texts open in the page's editors, held on this side as the page holds them, for the page's slow
// work on a large text to run here and hand back only its answer. Each text is a list of lines of UTF-16 units, as the
// page's own strings hold them, split as the page splits a file it reads: at each "\n", a "\r" before it taken off.
// The page writes each change it makes to a text here as it makes it, in the edits its document writes, and a text
// held here goes once the page lets it go.
//
// Find runs here only where its answer is the page's own: text sought as it is, with case, or without case where it
// is ASCII, and whole words where each match stands between ASCII. A pattern is the page's own: the page's engine
// matches the tests' long file several times as fast as this side's, and any other search is the page's own too. Find
// reads the text's lines joined, each ended by "\n", which no line holds and nothing sought holds, in one pass a block
// of units at a time (document.c, document_held.c, diff.c)
#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "../command_line/cli_internal.h"
#include "../text/sift.h"

// A place in a text: its line and its col, both from 0, the col in UTF-16 units
typedef struct
{
    unsigned long long line;
    unsigned long long col;
} CliPlace;

// One edit of a change, as the page's document writes it: the span it removes and what it writes, `length` bytes
typedef struct
{
    CliPlace from;
    CliPlace to;
    const char *text;
    unsigned long long length;
} CliEdit;

// How many lines a text holds and how many units in all, for the page to compare with its own
typedef struct
{
    unsigned long long lines;
    unsigned long long units;
} CliShape;

// What find seeks and how, as the page's find bar sets it: the query, a pattern or not, with case or not, whole words
// or not, and whether the page read the pattern with the `u` flag
typedef struct
{
    const char *query;
    int regex;
    int match_case;
    int word;
    int unicode;
} CliDocSought;

// A text's lines of UTF-16 units: each where it starts in the units and how many it holds, in storage the holder gives;
// and the census of the units' bytes as the sift reads them, each unit its low byte then its high, counted as the units
// are put and kept with the text, so a search plans from it with no count over the text
typedef struct
{
    unsigned short *unit;
    unsigned long long units_most;
    unsigned long long used;
    unsigned long long *start;
    unsigned long long *length;
    unsigned int lines_most;
    unsigned int lines;
    int full;
    CliCensus census;
} CliLines;

// the two bytes of a unit counted into a text's census as the unit is put
static inline void cli_lines_census_put(CliLines *lines, unsigned short unit)
{
    lines->census.occurrences[unit & 0xffu] += 1ull;
    lines->census.occurrences[unit >> 8u] += 1ull;
    lines->census.total += 2ull;
}

// a text's lines, `size` bytes of UTF-8, split as the page splits a file it reads, into `lines`; its shape; and the
// matches find gives for lines, each `[line, from col, to col]`, lines searched until `most` are found, each line
// searched whole, as the page searches, written as a JSON array: 1, or 0 with why said where the search is the page's
// own (document.c)
void cli_document_lines_of(const char *text, unsigned long long size, CliLines *lines);
CliShape cli_document_shape_of(const CliLines *lines);
// `count` UTF-16 units written as a JSON string, each unpaired surrogate as U+FFFD, as from_utf16_lossy reads them
void cli_document_units_written(CliLine *out, const unsigned short *units, unsigned long long count);
int cli_document_find_in(const CliLines *lines, const CliDocSought *sought, unsigned long long most, CliLine *out,
                         CliLine *said);
// the same matches each handed to `each` with its line, from col and to col, once the search is known to be this side's
typedef void (*CliFound3)(void *context, unsigned long long line, unsigned long long from, unsigned long long to);
int cli_document_find_each(const CliLines *lines, const CliDocSought *sought, unsigned long long most, CliFound3 each,
                           void *context, CliLine *said);

// `size` bytes of `text` held under `key` in place of what was held there, its shape into `shape`: 1, or 0 where no
// more texts are held; a text the page gives a part at a time held: each part kept, and the last, `done`, holds them
// all: 1 with its shape at the last, else 0; the text under `key` let go; whether a text is held under `key`, its
// shape into `shape`; the changes `changes` writes, `count` of them, the edits of each, `edits[at]` of them, none
// overlapping, each placed against the text as it stood before any, written from the last to the first as the page
// writes them: 1 with the shape, or 0 with why said where no text is held or an edit does not fit it, the text let go;
// the marks of the text under `key` against its file as the last commit holds it, `head` given as that file where
// `head_given` is set, NULL where the commit holds none, kept beside the text in place of what was kept, written as
// `{"head", "changes"}`: 1, or 0 where no text is held or no head was ever given; whether the text keeps the last
// commit's file beside it, or that it holds none; and the matches find gives for the text under `key` (document_held.c)
int cli_document_hold(const char *key, const char *text, unsigned long long size, CliShape *shape);
int cli_document_hold_part(const char *key, const char *part, unsigned long long size, int first, int done,
                           CliShape *shape);
void cli_document_release(const char *key);
int cli_document_shape(const char *key, CliShape *shape);
// the census kept with the text held under `key`, into `census`: 1, or 0 where none is held under the key
int cli_document_census(const char *key, CliCensus *census);
int cli_document_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits, unsigned int count,
                      CliShape *shape, CliLine *said);
int cli_document_marks(const char *key, int head_given, const char *head, unsigned long long head_size, CliLine *out);
int cli_document_has_head(const char *key);
int cli_document_find(const char *key, const CliDocSought *sought, unsigned long long most, CliLine *out,
                      CliLine *said);

// the lines of `now` that differ from `then`, as the page's own diff marks them, step for step: added, changed, and
// where lines were taken out, with each change's hunk and the lines it spans in `then`, written as `{"added",
// "changed", "removed", "hunks": [{"now", "then", "thenLines"}]}`: 1, or 0 where the two are too far apart to mark
// (diff.c)
int cli_diff_line_changes(const CliLines *then, const CliLines *now, CliLine *out);
// how many lines of `then` from `x` and of `now` from `y` are the same, one for one, each line read as its hash in
// `hash_then` and `hash_now` and each line of a run checked against the other's, under the diff's lock, 0 where the
// cells cannot meet the diff's room (diff.c)
long long cli_diff_run_of(const CliLines *then, const CliLines *now, const unsigned long long *hash_then,
                          const unsigned long long *hash_now, long long x, long long y);

#endif
