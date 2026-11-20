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
// beside a character outside ASCII, the regular expressions the colorer reads match a line at a time (document.c,
// document_store.c, diff.c)
#ifndef DOCUMENT_H
#define DOCUMENT_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliDocumentKeyCfg CliDocumentKeyCfg;
typedef struct CliDocumentErrMsgCfg CliDocumentErrMsgCfg;
typedef struct CliDocumentLinesCfg CliDocumentLinesCfg;
typedef struct CliDocumentOutCfg CliDocumentOutCfg;
typedef struct CliDocumentOutNeedleCfg CliDocumentOutNeedleCfg;
typedef struct CliDocumentLinesOutCfg CliDocumentLinesOutCfg;
typedef struct CliDocumentOutKeyHeadInputCfg CliDocumentOutKeyHeadInputCfg;
typedef struct CliDocumentKeyLineCfg CliDocumentKeyLineCfg;
typedef struct CliDocumentTextCfg CliDocumentTextCfg;
typedef struct CliDocumentLineUnitsCfg CliDocumentLineUnitsCfg;
typedef struct CliDocumentUnitsCfg CliDocumentUnitsCfg;
typedef struct CliDocumentOutThenCfg CliDocumentOutThenCfg;
typedef struct CliDocumentKeyLowCfg CliDocumentKeyLowCfg;
typedef struct CliDocumentKeyFromCfg CliDocumentKeyFromCfg;
typedef struct CliDocumentKeyGrammarCfg CliDocumentKeyGrammarCfg;
typedef struct CliDocumentContextCfg CliDocumentContextCfg;
typedef struct CliDocumentTextSizeCfg CliDocumentTextSizeCfg;
typedef struct CliDocumentUnitsCountCfg CliDocumentUnitsCountCfg;
typedef struct CliDocumentCfg CliDocumentCfg;
typedef struct CliDocumentColoredCfg CliDocumentColoredCfg;
typedef struct CliDocumentKeyColumnsCfg CliDocumentKeyColumnsCfg;
typedef struct CliDocumentOutGrammarCfg CliDocumentOutGrammarCfg;

// A place in a text: its line and its col, both from 0, the col in UTF-16 units
typedef struct
{
    unsigned long long line;
    unsigned long long col;
} CliPosition;

// One change between an older text and a newer, passed to a caller with its `context`: the newer text's lines from
// `now_from` to before `now_to`, located in place of the older's from `then_from` to before `then_to`, lines counted
// from 0; a change that takes lines out alone has `now_to` at `now_from`, one that puts lines in alone has `then_to`
// at `then_from`
typedef struct
{
    void *context;
    unsigned int now_from;
    unsigned int now_to;
    unsigned int then_from;
    unsigned int then_to;
} CliEachHunkCfg;

// One edit of a change, as the page's document writes it: the span it removes and what it writes, `length` bytes
typedef struct
{
    CliPosition from;
    CliPosition to;
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
    int word_text;
} CliDocNeedle;

// a line's state where it is not colored, and the place of its runs where it saves none
#define CLI_DOC_UNCOLORED 0xffffffffu
#define CLI_DOC_NO_RUNS 0xffffffffu

// A text's lines of UTF-16 units: each where it starts in the units and how many it contains, in storage the holder
// gives; and for a text
// that is colored, the state each line was colored from and the state it ends in, CLI_DOC_UNCOLORED for a line not
// colored, the regions its grammar's roles open and close on it as it was colored (document_internal.h), and the place
// among the text's saved runs of the runs the walk found on it, CLI_DOC_NO_RUNS where it saves none, NULL for a text
// that is not; and whether a line was read ending in a carriage return and a newline
typedef struct
{
    unsigned short *unit;
    unsigned long long units_max;
    unsigned long long used;
    unsigned long long *start;
    unsigned long long *length;
    unsigned int lines_max;
    unsigned int lines;
    int full;
    int crlf;
    unsigned int *begun;
    unsigned int *ended;
    unsigned int *regions;
    unsigned int *runs;
} CliLines;

// a text's lines, `size` bytes of UTF-8, split as the page splits a file it reads, into `lines`; its shape; and the
// matches find gives for lines, each `[line, from col, to col]`, lines searched until `max` are found, each line
// searched whole, written as a JSON array: 1, or 0 with why given where a pattern does not read or the cells cannot
// meet the search (document.c)
void cli_document_lines_get(const CliDocumentTextSizeCfg *args);
CliShape cli_document_shape_get(const CliDocumentLinesOutCfg *args);
// the character at `at` of `count` UTF-16 units, a surrogate pair read as one character and an unpaired surrogate as
// U+FFFD, as from_utf16_lossy reads them, the units it takes into `consumed`; the same units written as a JSON string;
// and the same written as UTF-8
unsigned int cli_document_unit_character(const CliDocumentUnitsCfg *args);
void cli_document_units_write(const CliDocumentUnitsCountCfg *args);
void cli_document_units_utf8(const CliDocumentUnitsCountCfg *args);
int cli_document_find_in(const CliDocumentLinesOutCfg *args);
// the same matches each passed to `each` with its line, from col and to col, once the search is whole
typedef void (*CliFound3)(const CliFound3Cfg *args);
int cli_document_find_each(const CliDocumentLinesCfg *args);

// `size` bytes of `text` stored under `key` in place of what was stored there, its shape into `shape`: 1, or 0 where no
// more texts are stored; the text under `key` let go; whether a text is stored under `key`, its shape into `shape`; the
// changes `changes` writes, `count` of them, the edits of each, `edits[at]` of them, none overlapping, each placed
// against the text as it was before any, written from the last to the first as the page writes them: 1 with the
// shape, or 0 with why written where no text is stored or an edit does not fit it, the text let go; whether the text
// knows the last commit's file, stored beside it or none; and the matches find gives for the text under `key`
// (document_store.c)
int cli_document_store(const CliDocumentTextCfg *args);
void cli_document_release(const CliDocumentCfg *args);
// whether the text stored under `key` was read with its lines ending in a carriage return and a newline
int cli_document_crlf(const CliDocumentCfg *args);
int cli_document_shape(const CliDocumentTextCfg *args);
int cli_document_edit(const CliDocumentErrMsgCfg *args);
// the units of the line `line` of the text read whole stored under `key` from its unit `from` on, copied into `dst`,
// `capacity` of them at the max, the line's count into `count`: 1, or 0 with `count` 0 where no such text or line is
// stored
unsigned long long cli_document_line_units(const CliDocumentLineUnitsCfg *args);
int cli_document_has_head(const CliDocumentCfg *args);
// the changes of the text read whole stored under `key` against its file as the last commit has it, each passed to
// `each` with `context` in order: where `head_given`, `head`, `head_size` bytes, is stored beside the text as that
// file first, NULL for a file the last commit does not contain; 1, or 0 where no such text is stored, the last commit's
// file is not known or is none, or the texts are too far apart (document_store.c, diff.c); and the units of the line
// `line` of that file copied into `dst`, `capacity` at max, the line's count into `count`: 1, or 0 with `count` 0
// where no such text, file or line is stored
int cli_document_changes(const CliDocumentOutKeyHeadInputCfg *args);
unsigned long long cli_document_head_line_units(const CliDocumentLineUnitsCfg *args);
// the changes between the lines of `then` and the lines of `now`, each passed to `each` with `context` in order, the
// lines the two share at their start and end set aside first and Myers' shortest edit run on what is left, each line
// read as its hash and checked against the other's where the hashes agree: 1, or 0 where the edit takes more than
// CLI_DIFF_MAX_EDITS rounds, the texts too far apart, or the cells cannot meet the edit's capacity (diff.c)
int cli_diff_line_changes(const CliDocumentOutThenCfg *args);
int cli_document_find(const CliDocumentOutNeedleCfg *args);

// the max lines above a run of lines not colored that are colored with it from root, where the line above it ends in
// no state known
#define CLI_DOC_PREFIX 300u

// what is called, on the walk's thread, as a branch's colors or folds are worked out: nothing is passed over, the
// lines' runs are in the branch for whoever reads them
typedef void (*CliDocColored)(void);
// A read of the names the tokens of the text under `key` take: the names table of its grammar, which the grammar
// store owns (highlight.h). The empty name is first, and a run's name is a place among them. `name` is a place,
// `text` a name looked up, `out` the line a name is put after, and `serial` the serial the table was built at
typedef struct
{
    const char *key;
    unsigned int name;
    const char *text;
    VerbaLinea *out;
    unsigned int *serial;
} CliDocumentNamesCfg;
// how many names the text's tokens take, the table's serial written into `serial` where that is not NULL: 0 where no
// text is stored under `key` (document_branch.c)
unsigned int cli_document_names_count(const CliDocumentNamesCfg *args);
// the name at `name` put after `out`: 1, or 0 where no text is stored under `key` or no name is at `name`
// (document_branch.c)
int cli_document_name_read(const CliDocumentNamesCfg *args);
// the place of the name `text`: the count of names where none is, and 0 where no text is stored under `key`
// (document_branch.c)
unsigned int cli_document_name_place(const CliDocumentNamesCfg *args);
// the colors worked out from here on passed to `colored`, NULL for nowhere, and walked out by a thread of their own
// where `threaded` is 1; the text stored under `key` colored by the grammar stored under `grammar`, empty for none,
// every line of it not colored yet and the walk woken, the names its grammar's tokens take passed on by the caller
// where `named` is 1, else with the colors passed on first: 1, or 0 where no text is stored under `key`; and every
// text's walk taken to its end on the calling thread (document_branch.c)
void cli_document_colors_to(const CliDocumentColoredCfg *args);
int cli_document_colored_by(const CliDocumentKeyGrammarCfg *args);
void cli_document_walk(void);
// the walk's telemetry turned on: a line to the error stream a step, the text's key, the lines colored and the origin;
// and turned off (document_branch.c)
void cli_document_telemetry_on(void);
void cli_document_telemetry_off(void);
// how many walkers walk the texts' colors out on threads of their own now (document_branch.c)
unsigned int cli_document_walkers(void);
// The lines from `first` to `last` of the text under `key` go after `out`, the caller's line, as UTF-8. A "\n"
// follows each line but the last. The state of the first line goes into `state`, `capacity` bytes. The state is root
// where the walk has not colored the line, and empty where it does not fit. Returns 1, or 0 where `key` holds no text
// or `out` cannot hold the lines (document_branch.c)
int cli_document_lines_read(const CliDocumentKeyCfg *args);
// the same lines read into a cell of the document module for this call alone and given to `each` with `context`, and
// the state written as above. The cell goes back before the call returns: 1, or 0 where no text is stored under `key`
// or the cells cannot meet the lines (document_branch.c)
int cli_document_lines_each(const CliDocumentKeyCfg *args);
// the lines of the text under `key` read for it, from `*low` to before `*high`: a paged text's those the page holds,
// and every line of any other: 1, or 0 where no text is stored under `key` (document_branch.c)
int cli_document_bounds(const CliDocumentKeyLowCfg *args);
// The text read whole under `key` goes to `each` with `context` as UTF-8, its lines joined by "\n" and a NUL after the
// last. The text sits in a cell of the document module for this call alone, and the cell goes back before the call
// returns. Returns 1, or 0 where `key` holds no text read whole or the cells cannot meet it (document_branch.c)
int cli_document_text_each(const CliDocumentKeyCfg *args);
// the text read whole stored under `key` put into `out` as its file had it, a line at a time, its characters as the
// file was read and each line but the last ended as the text's lines were read, by a newline or a carriage return and a
// newline; `out` passes its bytes on as it fills, as the file writer's line does: 1, or 0 with the reason written where
// no text read whole is stored under `key` or `out` refused its bytes (document_branch.c)
int cli_document_file_put(const CliDocumentOutCfg *args);
// the lines from `from` to before `to` of the branch under `key` the page's mirror, those about its view: those it did
// not mirror before passed on with the runs the walk saved for them, and colored again where they save none, and lines
// outside them colored with no runs passed on: 1, or 0 where no text is stored under `key`. A text stored passes every
// line's runs on until the page names the lines it mirrors (document_branch.c)
int cli_document_cache(const CliDocumentKeyFromCfg *args);
// the `count` lines at `number`, in order, of the text under `key` walked where they save no runs yet, and the lines a
// mirrored view shows recorded where `mirrored` is 1: 1, or 0 where no text is stored under the key (document_branch.c)
typedef struct
{
    const char *key;
    const unsigned long long *number;
    unsigned long long count;
    int mirrored;
} CliDocumentRunsCfg;
int cli_document_runs_walk(const CliDocumentRunsCfg *args);
// A read of the runs the document saves for the line `line` of the text under `key`. Where `mirrored` is 1, the line's
// runs end at the columns its mirrored view shows. `run` is a run of the line, `column` takes its first column, and
// `name` takes its name's place among the names the text's tokens take
typedef struct
{
    const char *key;
    unsigned long long line;
    int mirrored;
    unsigned int run;
    unsigned int *column;
    unsigned int *name;
} CliDocumentLineRunCfg;
// how many runs the line saves: 0 where it saves none or no text is stored under `key` (document_branch.c)
unsigned int cli_document_line_runs(const CliDocumentLineRunCfg *args);
// the run `run`'s column and name written where each is not NULL: 1, or 0 where the line saves no run `run`
// (document_branch.c)
int cli_document_line_run(const CliDocumentLineRunCfg *args);
// One bar of the minimap, a run the grammar names: its first cell, its width in cells and its name
typedef struct
{
    unsigned int start;
    unsigned int width;
    unsigned int name;
} CliDocumentMapBar;
// The minimap's lines in one cell: the first and how many, each line's bars from `line_at[line]` to `line_at[line + 1]`
typedef struct
{
    unsigned long long from;
    unsigned int count;
    unsigned int bars;
    unsigned int *line_at;
    CliDocumentMapBar *bar;
} CliDocumentMap;
// the lines from `first` to `last` of the text under `key` walked where they save no runs that are and positioned in a
// cell of `owner`, sized to them: each run with a name a bar, the empty name a gap, and a gap that opens a line `tab`
// cells a unit, every other run one cell a unit. A line with no runs is positioned as one bar of the empty name, its
// units wide, where the text holds its units: the cell, or NULL where no text is stored under the key or the cells
// cannot meet it
typedef struct
{
    const char *key;
    unsigned long long first;
    unsigned long long last;
    unsigned int tab;
    const CellOwner *owner;
} CliDocumentMapCfg;
CliDocumentMap *cli_document_map_alloc(const CliDocumentMapCfg *args);
// the runs the walk has saved on the lines from `first` to `last` of the text under `key` read for the names among
// them, each of two units or more once, the one `cursor` is in or at the end of left out. Each name goes to `each`
// with `context` as UTF-8 before the call returns. The list lives in the document module for this call alone. A paged
// text holds no units here and gives none
typedef struct
{
    const char *key;
    unsigned long long first;
    unsigned long long last;
    CliPosition cursor;
    void (*each)(const CliEachTextLengthCfg *args);
    void *context;
} CliDocumentWordsCfg;
void cli_document_words_each(const CliDocumentWordsCfg *args);
// the lines that open the regions the line `line` of the text under `key` is inside, read off its lines' regions
// as they were colored, outermost first, into `opener`, `max` at max, the innermost saved where more are open:
// how many, 0 where no text is stored under `key`; and the folds of the text under `key`, as the walk passes them on,
// each passed to `each` with its first line, the last line it hides and whether it shows the line it closes on after
// what it hides, in the order of their first lines, a paged text's those of the lines the page mirrors alone and none
// where the cells cannot meet the capacity they are found in: 1, or 0 where no text is stored under `key`
// (document_branch.c)
unsigned int cli_document_openers(const CliDocumentKeyLineCfg *args);
// the roles the run that holds the unit `col` of the line `line` of the text under `key` carries, as the grammar's
// name gives them (symbols.h): 0 where no text is stored under the key, the line saves no runs, the col is before
// its first run or the names' roles are not read yet (document_branch.c)
unsigned int cli_document_run_roles(const CliDocumentKeyLineCfg *args);
typedef void (*CliFoldEach)(const CliFoldEachCfg *args);
int cli_document_folds_each(const CliDocumentContextCfg *args);

// the paged text stored in the pages under `key` a branch here to color under the same key, its lines read through the
// pages, none colored, the lines from `from` to before `to` the page's mirror: 1, or 0 where no paged text
// is stored under `key` or the cells cannot meet its lines' states; and the page's changes written to the paged text
// under `key` as cli_pages_edit writes them, its shape written, the states of the lines each edit leaves carried with
// them and the lines it writes not colored, the walk woken: 1, or 0 with why given (document_store.c)
int cli_document_page(const CliDocumentKeyFromCfg *args);
int cli_document_paged_edit(const CliDocumentOutCfg *args);

struct CliDocumentKeyCfg
{
    const char *key;
    unsigned long long first;
    unsigned long long last;
    unsigned long long *size_pointer;
    char *state;
    unsigned long long capacity;
    const CellOwner *owner;
    VerbaLinea *out;
    void (*each)(const CliEachTextLengthCfg *args);
    void *context;
};

struct CliDocumentErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *key;
    CliShape *shape;
    const CliEdit *const *changes;
    const unsigned int *edits;
    unsigned int count_u32;
};

struct CliDocumentLinesCfg
{
    const CliLines *lines;
    const CliDocNeedle *needle;
    unsigned long long max;
    VerbaLinea *err_msg;
    CliFound3 each;
    void *context;
};

struct CliDocumentOutCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *key;
    const CliEdit *const *changes;
    const unsigned int *edits;
    unsigned int count_u32;
};

struct CliDocumentOutNeedleCfg
{
    VerbaLinea *out;
    const CliDocNeedle *needle;
    unsigned long long max;
    VerbaLinea *err_msg;
    const char *key;
};

struct CliDocumentLinesOutCfg
{
    const CliLines *lines;
    VerbaLinea *out;
    const CliDocNeedle *needle;
    unsigned long long max;
    VerbaLinea *err_msg;
};

struct CliDocumentOutKeyHeadInputCfg
{
    const char *key;
    int head_given;
    const char *head;
    unsigned long long head_size;
    void (*each)(const CliEachHunkCfg *args);
    void *context;
};

struct CliDocumentKeyLineCfg
{
    const char *key;
    unsigned int line;
    unsigned long long col;
    unsigned int *opener;
    unsigned int max_u32;
};

struct CliDocumentTextCfg
{
    const char *text;
    unsigned long long size;
    const char *key;
    CliShape *shape;
};

struct CliDocumentLineUnitsCfg
{
    const char *key;
    unsigned long long line;
    unsigned long long *count;
    unsigned short *dst;
    unsigned long long capacity;
    unsigned long long from;
};

struct CliDocumentUnitsCfg
{
    const unsigned short *units;
    unsigned long long count;
    unsigned long long at;
    unsigned long long *consumed;
};

struct CliDocumentOutThenCfg
{
    const CliLines *then;
    const CliLines *now;
    void (*each)(const CliEachHunkCfg *args);
    void *context;
};

struct CliDocumentKeyLowCfg
{
    const char *key;
    unsigned long long *low;
    unsigned long long *high;
};

struct CliDocumentKeyFromCfg
{
    const char *key;
    unsigned long long from;
    unsigned long long to;
};

struct CliDocumentKeyGrammarCfg
{
    const char *key;
    const char *grammar;
    int named;
};

struct CliDocumentContextCfg
{
    void *context;
    const char *key;
    CliFoldEach each_fold_each;
};

struct CliDocumentTextSizeCfg
{
    const char *text;
    unsigned long long size;
    CliLines *lines_out;
};

struct CliDocumentUnitsCountCfg
{
    const unsigned short *units;
    unsigned long long count;
    VerbaLinea *out;
};

struct CliDocumentCfg
{
    const char *key;
};

struct CliDocumentColoredCfg
{
    CliDocColored colored;
    int threaded;
};

struct CliDocumentKeyColumnsCfg
{
    const char *key;
    unsigned long long columns;
};

struct CliDocumentOutGrammarCfg
{
    VerbaLinea *out;
    const char *grammar;
};

typedef struct
{
    void (*lines_get)(const CliDocumentTextSizeCfg *args);
} CliDocumentTextSizeNs;
EMBED_TABLE_LAYOUT(CliDocumentTextSizeNs, lines_get);
EMBED_TABLE_STORAGE CliDocumentTextSizeNs cli_document_text_size EMBED_UNUSED = {
    .lines_get = cli_document_lines_get,
};

typedef struct
{
    CliShape (*shape_get)(const CliDocumentLinesOutCfg *args);
    int (*find_in)(const CliDocumentLinesOutCfg *args);
} CliDocumentLinesOutNs;
EMBED_TABLE_LAYOUT(CliDocumentLinesOutNs, shape_get, find_in);
EMBED_TABLE_STORAGE CliDocumentLinesOutNs cli_document_lines_out EMBED_UNUSED = {
    .shape_get = cli_document_shape_get,
    .find_in = cli_document_find_in,
};

typedef struct
{
    unsigned int (*unit_character)(const CliDocumentUnitsCfg *args);
} CliDocumentUnitsNs;
EMBED_TABLE_LAYOUT(CliDocumentUnitsNs, unit_character);
EMBED_TABLE_STORAGE CliDocumentUnitsNs cli_document_units EMBED_UNUSED = {
    .unit_character = cli_document_unit_character,
};

typedef struct
{
    void (*units_write)(const CliDocumentUnitsCountCfg *args);
    void (*units_utf8)(const CliDocumentUnitsCountCfg *args);
} CliDocumentUnitsCountNs;
EMBED_TABLE_LAYOUT(CliDocumentUnitsCountNs, units_write, units_utf8);
EMBED_TABLE_STORAGE CliDocumentUnitsCountNs cli_document_units_count EMBED_UNUSED = {
    .units_write = cli_document_units_write,
    .units_utf8 = cli_document_units_utf8,
};

typedef struct
{
    int (*find_each)(const CliDocumentLinesCfg *args);
} CliDocumentLinesNs;
EMBED_TABLE_LAYOUT(CliDocumentLinesNs, find_each);
EMBED_TABLE_STORAGE CliDocumentLinesNs cli_document_lines EMBED_UNUSED = {
    .find_each = cli_document_find_each,
};

typedef struct
{
    int (*store)(const CliDocumentTextCfg *args);
    int (*shape)(const CliDocumentTextCfg *args);
} CliDocumentTextNs;
EMBED_TABLE_LAYOUT(CliDocumentTextNs, store, shape);
EMBED_TABLE_STORAGE CliDocumentTextNs cli_document_text EMBED_UNUSED = {
    .store = cli_document_store,
    .shape = cli_document_shape,
};

typedef struct
{
    unsigned long long (*line_units)(const CliDocumentLineUnitsCfg *args);
    unsigned long long (*head_line_units)(const CliDocumentLineUnitsCfg *args);
} CliDocumentLineUnitsNs;
EMBED_TABLE_LAYOUT(CliDocumentLineUnitsNs, line_units, head_line_units);
EMBED_TABLE_STORAGE CliDocumentLineUnitsNs cli_document_line EMBED_UNUSED = {
    .line_units = cli_document_line_units,
    .head_line_units = cli_document_head_line_units,
};

typedef struct
{
    int (*changes)(const CliDocumentOutKeyHeadInputCfg *args);
} CliDocumentOutKeyHeadInputNs;
EMBED_TABLE_LAYOUT(CliDocumentOutKeyHeadInputNs, changes);
EMBED_TABLE_STORAGE CliDocumentOutKeyHeadInputNs cli_document_out_key_head_input EMBED_UNUSED = {
    .changes = cli_document_changes,
};

typedef struct
{
    int (*line_changes)(const CliDocumentOutThenCfg *args);
} CliDocumentOutThenNs;
EMBED_TABLE_LAYOUT(CliDocumentOutThenNs, line_changes);
EMBED_TABLE_STORAGE CliDocumentOutThenNs cli_document_out_then EMBED_UNUSED = {
    .line_changes = cli_diff_line_changes,
};

typedef struct
{
    void (*release)(const CliDocumentCfg *args);
    int (*crlf)(const CliDocumentCfg *args);
    int (*has_head)(const CliDocumentCfg *args);
} CliDocumentNs;
EMBED_TABLE_LAYOUT(CliDocumentNs, release, crlf, has_head);
EMBED_TABLE_STORAGE CliDocumentNs cli_document EMBED_UNUSED = {
    .release = cli_document_release,
    .crlf = cli_document_crlf,
    .has_head = cli_document_has_head,
};

typedef struct
{
    int (*edit)(const CliDocumentErrMsgCfg *args);
} CliDocumentErrMsgNs;
EMBED_TABLE_LAYOUT(CliDocumentErrMsgNs, edit);
EMBED_TABLE_STORAGE CliDocumentErrMsgNs cli_document_err_msg EMBED_UNUSED = {
    .edit = cli_document_edit,
};

typedef struct
{
    int (*find)(const CliDocumentOutNeedleCfg *args);
} CliDocumentOutNeedleNs;
EMBED_TABLE_LAYOUT(CliDocumentOutNeedleNs, find);
EMBED_TABLE_STORAGE CliDocumentOutNeedleNs cli_document_out_needle EMBED_UNUSED = {
    .find = cli_document_find,
};

typedef struct
{
    void (*colors_to)(const CliDocumentColoredCfg *args);
} CliDocumentColoredNs;
EMBED_TABLE_LAYOUT(CliDocumentColoredNs, colors_to);
EMBED_TABLE_STORAGE CliDocumentColoredNs cli_document_colored EMBED_UNUSED = {
    .colors_to = cli_document_colors_to,
};

typedef struct
{
    int (*colored_by)(const CliDocumentKeyGrammarCfg *args);
} CliDocumentKeyGrammarNs;
EMBED_TABLE_LAYOUT(CliDocumentKeyGrammarNs, colored_by);
EMBED_TABLE_STORAGE CliDocumentKeyGrammarNs cli_document_key_grammar EMBED_UNUSED = {
    .colored_by = cli_document_colored_by,
};

typedef struct
{
    int (*lines_read)(const CliDocumentKeyCfg *args);
    int (*lines_each)(const CliDocumentKeyCfg *args);
    int (*text_each)(const CliDocumentKeyCfg *args);
} CliDocumentKeyNs;
EMBED_TABLE_LAYOUT(CliDocumentKeyNs, lines_read, lines_each, text_each);
EMBED_TABLE_STORAGE CliDocumentKeyNs cli_document_key EMBED_UNUSED = {
    .lines_read = cli_document_lines_read,
    .lines_each = cli_document_lines_each,
    .text_each = cli_document_text_each,
};

typedef struct
{
    int (*bounds)(const CliDocumentKeyLowCfg *args);
} CliDocumentKeyLowNs;
EMBED_TABLE_LAYOUT(CliDocumentKeyLowNs, bounds);
EMBED_TABLE_STORAGE CliDocumentKeyLowNs cli_document_key_low EMBED_UNUSED = {
    .bounds = cli_document_bounds,
};

typedef struct
{
    int (*cache)(const CliDocumentKeyFromCfg *args);
    int (*page)(const CliDocumentKeyFromCfg *args);
} CliDocumentKeyFromNs;
EMBED_TABLE_LAYOUT(CliDocumentKeyFromNs, cache, page);
EMBED_TABLE_STORAGE CliDocumentKeyFromNs cli_document_key_from EMBED_UNUSED = {
    .cache = cli_document_cache,
    .page = cli_document_page,
};

typedef struct
{
    unsigned int (*openers)(const CliDocumentKeyLineCfg *args);
    unsigned int (*run_roles)(const CliDocumentKeyLineCfg *args);
} CliDocumentKeyLineNs;
EMBED_TABLE_LAYOUT(CliDocumentKeyLineNs, openers, run_roles);
EMBED_TABLE_STORAGE CliDocumentKeyLineNs cli_document_key_line EMBED_UNUSED = {
    .openers = cli_document_openers,
    .run_roles = cli_document_run_roles,
};

typedef struct
{
    int (*folds_each)(const CliDocumentContextCfg *args);
} CliDocumentContextNs;
EMBED_TABLE_LAYOUT(CliDocumentContextNs, folds_each);
EMBED_TABLE_STORAGE CliDocumentContextNs cli_document_context EMBED_UNUSED = {
    .folds_each = cli_document_folds_each,
};

typedef struct
{
    int (*paged_edit)(const CliDocumentOutCfg *args);
    int (*file_put)(const CliDocumentOutCfg *args);
} CliDocumentOutNs;
EMBED_TABLE_LAYOUT(CliDocumentOutNs, paged_edit, file_put);
EMBED_TABLE_STORAGE CliDocumentOutNs cli_document_out EMBED_UNUSED = {
    .paged_edit = cli_document_paged_edit,
    .file_put = cli_document_file_put,
};

typedef struct
{
    unsigned int (*count)(const CliDocumentNamesCfg *args);
    int (*read)(const CliDocumentNamesCfg *args);
    unsigned int (*place)(const CliDocumentNamesCfg *args);
} CliDocumentNamesNs;
EMBED_TABLE_LAYOUT(CliDocumentNamesNs, count, read, place);
EMBED_TABLE_STORAGE CliDocumentNamesNs cli_document_names EMBED_UNUSED = {
    .count = cli_document_names_count,
    .read = cli_document_name_read,
    .place = cli_document_name_place,
};

typedef struct
{
    int (*runs_walk)(const CliDocumentRunsCfg *args);
} CliDocumentRunsNs;
EMBED_TABLE_LAYOUT(CliDocumentRunsNs, runs_walk);
EMBED_TABLE_STORAGE CliDocumentRunsNs cli_document_runs EMBED_UNUSED = {
    .runs_walk = cli_document_runs_walk,
};

typedef struct
{
    unsigned int (*count)(const CliDocumentLineRunCfg *args);
    int (*read)(const CliDocumentLineRunCfg *args);
} CliDocumentLineRunNs;
EMBED_TABLE_LAYOUT(CliDocumentLineRunNs, count, read);
EMBED_TABLE_STORAGE CliDocumentLineRunNs cli_document_line_run_table EMBED_UNUSED = {
    .count = cli_document_line_runs,
    .read = cli_document_line_run,
};

typedef struct
{
    CliDocumentMap *(*map_alloc)(const CliDocumentMapCfg *args);
} CliDocumentMapNs;
EMBED_TABLE_LAYOUT(CliDocumentMapNs, map_alloc);
EMBED_TABLE_STORAGE CliDocumentMapNs cli_document_map EMBED_UNUSED = {
    .map_alloc = cli_document_map_alloc,
};

typedef struct
{
    void (*words_each)(const CliDocumentWordsCfg *args);
} CliDocumentWordsNs;
EMBED_TABLE_LAYOUT(CliDocumentWordsNs, words_each);
EMBED_TABLE_STORAGE CliDocumentWordsNs cli_document_words EMBED_UNUSED = {
    .words_each = cli_document_words_each,
};

#endif
