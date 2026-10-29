// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document.h: the texts open in the page's editors, stored on this side as the page stores them, for the page's slow
// work on a large text to run here and hand back only its answer. Each text is a list of lines of UTF-16 units, as the
// page's own strings store them, split as the page splits a file it reads: at each "\n", a "\r" before it taken off.
// The page writes each change it makes to a text here as it makes it, in the edits its document writes, and a text
// stored here goes once the page lets it go.
//
// Find runs here, every search of the page's find bar. Text sought as it is, with case, or without case where it is
// ASCII, the sift reads off the text's lines joined, each ended by "\n", which no line contains and nothing sought
// contains, in one pass a block of units at a time; a pattern, text sought without case outside ASCII, and whole words
// beside a letter outside ASCII, the regular expressions the colorer reads match a line at a time (document.c,
// document_store.c, diff.c)
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

// How many lines a text contains and how many units in all, for the page to compare with its own
typedef struct
{
    unsigned long long lines;
    unsigned long long units;
} CliShape;

// What find seeks and how, as the page's find bar sets it: the query, a pattern or not, with case or not, and whole
// words or not
typedef struct
{
    const char *query;
    int regex;
    int match_case;
    int word;
} CliDocSought;

// a line's state where it is not colored, and the place of its runs where it keeps none
#define CLI_DOC_UNCOLORED 0xffffffffu
#define CLI_DOC_NO_RUNS 0xffffffffu

// A text's lines of UTF-16 units: each where it starts in the units and how many it contains, in storage the holder
// gives; the census of the units' bytes as the sift reads them, each unit its low byte then its high, counted as the
// units are put and stored with the text, the census a search plans from with no count over the text; and for a text
// that is colored, the state each line was colored from and the state it ends in, CLI_DOC_UNCOLORED for a line not
// colored, the regions its grammar's roles open and close on it as it was colored (document_internal.h), and the place
// among the text's kept runs of the runs the walk found on it, CLI_DOC_NO_RUNS where it keeps none, NULL for a text
// that is not
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
    unsigned int *begun;
    unsigned int *ended;
    unsigned int *regions;
    unsigned int *runs;
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
// searched whole, written as a JSON array: 1, or 0 with why given where a pattern does not read or the cells cannot
// meet the search (document.c)
void cli_document_lines_of(const char *text, unsigned long long size, CliLines *lines);
CliShape cli_document_shape_of(const CliLines *lines);
// the letter at `at` of `count` UTF-16 units, a surrogate pair read as one letter and an unpaired surrogate as
// U+FFFD, as from_utf16_lossy reads them, the units it takes into `took`; the same units written as a JSON string;
// and the same written as UTF-8
unsigned int cli_document_unit_letter(const unsigned short *units, unsigned long long count, unsigned long long at,
                                      unsigned long long *took);
void cli_document_units_written(VerbaLinea *out, const unsigned short *units, unsigned long long count);
void cli_document_units_utf8(VerbaLinea *out, const unsigned short *units, unsigned long long count);
int cli_document_find_in(const CliLines *lines, const CliDocSought *sought, unsigned long long most, VerbaLinea *out,
                         VerbaLinea *err_msg);
// the same matches each handed to `each` with its line, from col and to col, once the search is whole
typedef void (*CliFound3)(void *context, unsigned long long line, unsigned long long from, unsigned long long to);
int cli_document_find_each(const CliLines *lines, const CliDocSought *sought, unsigned long long most, CliFound3 each,
                           void *context, VerbaLinea *err_msg);

// `size` bytes of `text` stored under `key` in place of what was stored there, its shape into `shape`: 1, or 0 where no
// more texts are stored; the text under `key` let go; whether a text is stored under `key`, its shape into `shape`; the
// changes `changes` writes, `count` of them, the edits of each, `edits[at]` of them, none overlapping, each placed
// against the text as it stood before any, written from the last to the first as the page writes them: 1 with the
// shape, or 0 with why given where no text is stored or an edit does not fit it, the text let go; the marks of the text
// under `key` against its file as the last commit stores it, `head` given as that file where `head_given` is set, NULL
// where the commit stores none, stored beside the text in place of what was stored, written as `{"head", "changes"}`:
// 1, or 0 where no text is stored or no head was ever given; whether the text stores the last commit's file beside it,
// or that it stores none; and the matches find gives for the text under `key` (document_store.c)
int cli_document_store(const char *key, const char *text, unsigned long long size, CliShape *shape);
void cli_document_release(const char *key);
int cli_document_shape(const char *key, CliShape *shape);
// the census stored with the text stored under `key`, into `census`: 1, or 0 where none is stored under the key
int cli_document_census(const char *key, CliCensus *census);
int cli_document_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits, unsigned int count,
                      CliShape *shape, VerbaLinea *err_msg);
int cli_document_marks(const char *key, int head_given, const char *head, unsigned long long head_size,
                       VerbaLinea *out);
int cli_document_has_head(const char *key);
int cli_document_find(const char *key, const CliDocSought *sought, unsigned long long most, VerbaLinea *out,
                      VerbaLinea *err_msg);

// the most lines above a run of lines not colored that are colored with it from root, where the line above it ends in
// no state known
#define CLI_DOC_LEAD 300u

// where a branch's colors go as they are worked out: `size` bytes of `body`, `{"key", "seq", "names", "colors":
// [{"from", "runs"}]}`, `seq` the changes written to the text when its lines were colored, `names` the names as
// cli_document_names_written writes them, the first colors handed on after it is colored by a grammar alone carrying
// them, and each line from `from` its runs `[column, name, column, name, ...]`, each column in UTF-16 units and each
// name a place among those names, a run that opens or closes a pair whose depth it counts named with the depth its pair
// stands at, as the walk carries it from line to line in the state each line ends in; and its folds once every line is
// colored and its regions moved, `{"key", "seq", "folds": [[first, last, shown], ...]}`, each region's that the
// grammar's roles open and close, from the line it opens on to the last line its fold hides, and 1 in `shown` where the
// fold shows the line it closes on after what it hides, else 0
typedef void (*CliDocColored)(const char *body, unsigned long long size);
// the names a branch's tokens take written into `out` as a JSON array: the names the grammar stored under `grammar`
// gives, then for each that counts the depth of a pair, in their order, that name followed by `.` and each depth from 0
// to 15, a deeper pair taking the name of its depth less 16; `[""]` where no grammar is stored under `grammar`
// (document_branch.c)
void cli_document_names_written(const char *grammar, VerbaLinea *out);
// the colors worked out from here on handed to `colored`, NULL for nowhere, and walked out by a thread of their own
// where `threaded` is 1; the text stored under `key` colored by the grammar stored under `grammar`, empty for none,
// every line of it not colored yet and the walk woken, the names its grammar's tokens take handed on by the caller
// where `named` is 1, else with the colors handed on first: 1, or 0 where no text is stored under `key`; the lines from
// `first` to `last` of the text under `key` colored where they are not, the walk worked out from `first` after: each
// run of lines not colored entered in the state the line above it ends in, else from root as far above it as
// CLI_DOC_LEAD lines not colored reach, the lines after it colored again until one is entered in the state it was
// before, a paged text's no line outside those the page holds, their runs written into `out` as a JSON array of
// `{"from", "runs"}`, or handed to `colored` where `out` is NULL: 1, or 0 where no text is stored under `key`; and every
// text's walk taken to its end on the calling thread
// (document_branch.c)
void cli_document_colors_to(CliDocColored colored, int threaded);
int cli_document_colored_by(const char *key, const char *grammar, int named);
int cli_document_viewed(const char *key, unsigned long long first, unsigned long long last, VerbaLinea *out);
void cli_document_walked(void);
// how many walkers walk the texts' colors out on threads of their own now (document_branch.c)
unsigned int cli_document_walkers(void);
// the lines from `first` to `last` of the text under `key` as UTF-8, each but the last followed by "\n", in a cell taken
// for them, given back with g_cli_cells->persistent_buf_release, their bytes into `size`, and the state the first is
// entered in written into `state`, `room` bytes, root where it is not colored yet, empty where it does not fit: NULL
// where no text is stored under `key` or the cells cannot meet the lines; and the same lines written into `out` as a
// JSON string: 1, or 0 where no text is stored under `key` or the cells cannot meet the lines (document_branch.c)
char *cli_document_lines_taken(const char *key, unsigned long long first, unsigned long long last,
                               unsigned long long *size, char *state, unsigned long long room);
// the lines of the text under `key` read for it, from `*low` to before `*high`: a paged text's those the page holds,
// and every line of any other: 1, or 0 where no text is stored under `key` (document_branch.c)
int cli_document_bounds(const char *key, unsigned long long *low, unsigned long long *high);
int cli_document_text_written(const char *key, unsigned long long first, unsigned long long last, VerbaLinea *out,
                              char *state, unsigned long long room);
// the text read whole stored under `key` as UTF-8, its lines joined by "\n" and a NUL after the last, in a cell taken
// for it, given back with g_cli_cells->persistent_buf_release, its bytes into `size`: NULL where no text read whole is
// stored under `key` or the cells cannot meet it (document_branch.c)
char *cli_document_text_taken(const char *key, unsigned long long *size);
// the lines from `from` to before `to` of the branch under `key` the page's mirror, those about its view: those it did
// not mirror before handed on with the runs the walk kept for them, and colored again where they keep none, and lines
// outside them colored with no runs handed on: 1, or 0 where no text is stored under `key`. A text stored hands every
// line's runs on until the page names the lines it mirrors (document_branch.c)
int cli_document_held(const char *key, unsigned long long from, unsigned long long to);
// the lines from `first` to `last` of the text under `key`, each with the runs the walk kept for it, colored where it
// keeps none, entered in the state the line above it ends in, written into `out` as a JSON array of `{"from", "runs"}`
// whatever lines the page mirrors. Where
// `mirrored` is 1 the page mirrors them from here on, as it does the lines it reads: the lines it mirrors take them in,
// joined to those it mirrors where the two meet and in their place where they do not, the walk hands their runs on no
// second time, and each one's runs are written as far as the view draws; where 0 the lines it mirrors stand as they
// were and each line's runs are written whole, as a print of them asks: 1, or 0 where no text is stored under `key`
// (document_branch.c)
int cli_document_runs_written(const char *key, unsigned long long first, unsigned long long last, int mirrored,
                              VerbaLinea *out);
// the columns of each line of the text under `key` the view draws, `columns`, 0 for every one: a line's runs are handed
// on no further from here on, each line colored whole, and where the view draws farther than they were handed, the
// lines the page mirrors are handed on again as far with the runs the walk kept: 1, or 0 where no text is stored under
// `key` (document_branch.c)
int cli_document_columns(const char *key, unsigned long long columns);
// the lines that open the regions the line `line` of the text under `key` stands inside, read off its lines' regions
// as they were colored, outermost first, into `opener`, `most` at the most, the innermost kept where more stand open:
// how many, 0 where no text is stored under `key`; and the folds of the text under `key`, as the walk hands them on,
// each handed to `each` with its first line, the last line it hides and whether it shows the line it closes on after
// what it hides, in the order of their first lines, a paged text's those of the lines the page mirrors alone and none
// where the cells cannot meet the room they are found in: 1, or 0 where no text is stored under `key`
// (document_branch.c)
unsigned int cli_document_openers(const char *key, unsigned int line, unsigned int *opener, unsigned int most);
typedef void (*CliFoldEach)(void *context, unsigned long long first, unsigned long long last, int shown);
int cli_document_folds_each(const char *key, CliFoldEach each, void *context);

// the paged text stored in the pages under `key` a branch here to color under the same key, its lines read through the
// pages, none colored, the lines from `from` to before `to` the page's mirror: 1, or 0 where no paged text
// is stored under `key` or the cells cannot meet its lines' states; and the page's changes written to the paged text
// under `key` as cli_pages_edit writes them, its shape written, the states of the lines each edit leaves carried with
// them and the lines it writes not colored, the walk woken: 1, or 0 with why given (document_store.c)
int cli_document_paged(const char *key, unsigned long long from, unsigned long long to);
int cli_document_paged_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits,
                            unsigned int count, VerbaLinea *out, VerbaLinea *err_msg);

// the lines of `now` that differ from `then`, as the page's own diff marks them, step for step: added, changed, and
// where lines were taken out, with each change's hunk and the lines it spans in `then`, written as `{"added",
// "changed", "removed", "hunks": [{"now", "then", "thenLines"}]}`: 1, or 0 where the two are too far apart to mark
// (diff.c)
int cli_diff_line_changes(const CliLines *then, const CliLines *now, VerbaLinea *out);
// how many lines of `then` from `x` and of `now` from `y` are the same, one for one, each line read as its hash in
// `hash_then` and `hash_now` and each line of a run checked against the other's, under the diff's lock, 0 where the
// cells cannot meet the diff's room (diff.c)
long long cli_diff_run_of(const CliLines *then, const CliLines *now, const unsigned long long *hash_then,
                          const unsigned long long *hash_now, long long x, long long y);

#endif
