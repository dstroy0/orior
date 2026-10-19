// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_internal.h: what the inspections' sources share: a reading of a text by its letters, lines and UTF-16
// columns; the tokens a reader makes and the comments it passes; a painting of each column's class; a set of names;
// and the facts' own writers. One reading runs at a time, under the inspections' lock
#ifndef INSPECT_INTERNAL_H
#define INSPECT_INTERNAL_H

#include "inspect.h"

// The most a reading of one text holds, counted from the text as its cell is taken: its tokens, a token for each
// name, number and other letter that is no space and one for each line; its lines; its comments, one for each letter
// a comment opens with; the columns a painting holds; and the names a set holds, one for each name of the text
typedef struct
{
    unsigned int tokens;
    unsigned int lines;
    unsigned int comments;
    unsigned long long columns;
    unsigned int names;
} CliInspectMost;

extern CliInspectMost g_cli_inspect_most;

// the most tokens, lines and comments a reading holds, and the most columns a painting holds
#define CLI_INSPECT_TOKENS (g_cli_inspect_most.tokens)
#define CLI_INSPECT_LINES (g_cli_inspect_most.lines)
#define CLI_INSPECT_COMMENTS (g_cli_inspect_most.comments)
#define CLI_INSPECT_COLUMNS (g_cli_inspect_most.columns)

// A carving of a reading's cell into the lists of the sources that read it: where the next list starts, NULL where
// the lists are only measured, and the bytes they take
typedef struct
{
    unsigned char *at;
    unsigned long long bytes;
} CliInspectCarve;

// the next list of a carving, `bytes` long, kept to eight bytes: its start, NULL where the carving only measures
// (inspect_reading.c)
void *cli_inspect_carved(CliInspectCarve *carve, unsigned long long bytes);
// the reading of `size` bytes of `text` in `language` taken, under the inspections' lock: its counts into
// g_cli_inspect_most and every list a source of that language reads it into a part of one cell sized to them, 1, or
// 0 where the cell cannot be taken; and the cell given back (inspect_reading.c)
int cli_inspect_reading_held(const char *language, const char *text, unsigned long long size);
void cli_inspect_reading_given(void);
// each source's lists carved for a reading, the scan's, the painting's and the sorting's for every language, the rest
// for theirs (inspect_scan.c, inspect_paint.c, inspect.c, inspect_js_lex.c, inspect_js_parse.c, inspect_js_write.c,
// inspect_js_facts.c, inspect_py_lex.c, inspect_py_parse.c, inspect_py_sort.c, inspect_py_unread.c)
void cli_inspect_scan_laid(CliInspectCarve *carve);
void cli_inspect_paint_laid(CliInspectCarve *carve);
void cli_inspect_sort_laid(CliInspectCarve *carve);
void cli_inspect_js_lex_laid(CliInspectCarve *carve);
void cli_inspect_js_parse_laid(CliInspectCarve *carve);
void cli_inspect_js_write_laid(CliInspectCarve *carve);
void cli_inspect_js_facts_laid(CliInspectCarve *carve);
void cli_inspect_py_lex_laid(CliInspectCarve *carve);
void cli_inspect_py_parse_laid(CliInspectCarve *carve);
void cli_inspect_py_sort_laid(CliInspectCarve *carve);
void cli_inspect_py_unread_laid(CliInspectCarve *carve);

// code no path reaches told as the text before the word that leaves the block, the word, and the text after; the
// words that leave a block in JavaScript are return, throw, break and continue and in Python return, raise, break and
// continue, so the longest is continue, and the telling's room is the three and its 0
#define CLI_INSPECT_UNREACHED_BEFORE "No path reaches this code: the `"
#define CLI_INSPECT_UNREACHED_AFTER "` before it leaves the block."
#define CLI_INSPECT_LEAVING_LONGEST "continue"
#define CLI_INSPECT_UNREACHED_ROOM                                                                                     \
    ((sizeof(CLI_INSPECT_UNREACHED_BEFORE) - 1u) + (sizeof(CLI_INSPECT_LEAVING_LONGEST) - 1u) +                        \
     sizeof(CLI_INSPECT_UNREACHED_AFTER))
_Static_assert((sizeof(CLI_INSPECT_LEAVING_LONGEST) >= sizeof("return")) &&
                   (sizeof(CLI_INSPECT_LEAVING_LONGEST) >= sizeof("throw")) &&
                   (sizeof(CLI_INSPECT_LEAVING_LONGEST) >= sizeof("break")) &&
                   (sizeof(CLI_INSPECT_LEAVING_LONGEST) >= sizeof("raise")),
               "CLI_INSPECT_LEAVING_LONGEST must be the longest word that leaves a block");

// A reading of a text by its letters: the text and its size, where it stands, its line and its column in UTF-16 units
typedef struct
{
    const char *src;
    unsigned long long size;
    unsigned long long pos;
    unsigned int line;
    unsigned int col;
} CliInspectScan;

// A token: its kind, as each language names its kinds; 1 in `field` where it is a name read inside a Python
// f-string's field; its bytes from `start` to `end`; and its span
typedef struct
{
    unsigned char kind;
    unsigned char field;
    unsigned int start;
    unsigned int end;
    CliInspectPlace from;
    CliInspectPlace to;
} CliInspectTok;

// A set of names, each `size` bytes at `at`, `buckets` of them, a power of two, and the buckets it took, to clear it
// by, at most half its buckets; its lists carved from a reading's cell
typedef struct
{
    const char **at;
    unsigned int *size;
    unsigned int *taken;
    unsigned int count;
    unsigned int buckets;
} CliInspectSet;

// the inspections' lock, the tokens of the reading and their count, its comments' spans and their count, and 1 in
// `g_cli_inspect_over` where a token or a comment did not fit (inspect_scan.c)
extern CliLock g_cli_inspect_lock;
extern CliInspectTok *g_cli_inspect_tok;
extern unsigned int g_cli_inspect_toks;
extern CliInspectSpan *g_cli_inspect_comment;
extern unsigned int g_cli_inspect_comments;
extern int g_cli_inspect_over;

// what a reading's letter is past the text's end
#define CLI_INSPECT_END 0xffffffffu

// a reading begun at the start of `size` bytes of `src`; the letter where it stands, and `ahead` letters on,
// CLI_INSPECT_END past the text's end; the letter where it stands taken, its line and column moved past it, the same
// past the end, where nothing is taken; where it stands; a comment's span
// kept from where it stands to the end of its line, before the line's end; a token put on the reading's tokens, its
// span from `from` to where the reading stands; and a comment's span kept (inspect_scan.c)
void cli_inspect_scan_begun(CliInspectScan *scan, const char *src, unsigned long long size);
unsigned int cli_inspect_peek(const CliInspectScan *scan);
unsigned int cli_inspect_peek_at(const CliInspectScan *scan, unsigned int ahead);
unsigned int cli_inspect_bump(CliInspectScan *scan);
CliInspectPlace cli_inspect_place(const CliInspectScan *scan);
void cli_inspect_skip_line(CliInspectScan *scan);
void cli_inspect_tok_put(const CliInspectScan *scan, unsigned char kind, unsigned long long start, CliInspectPlace from,
                         int field);
void cli_inspect_comment_put(CliInspectPlace from, CliInspectPlace to);

// whether a letter is alphabetic, alphanumeric or white as Rust's char says, ASCII and the letters of the Unicode
// properties the regular expressions read (inspect_scan.c)
int cli_inspect_alphabetic(unsigned int letter);
int cli_inspect_alphanumeric(unsigned int letter);
int cli_inspect_white(unsigned int letter);

// a set's lists carved for a reading, its buckets twice the reading's names; a set made empty; a name put in it; and
// whether it holds a name (inspect_scan.c)
void cli_inspect_set_laid(CliInspectSet *set, CliInspectCarve *carve);
void cli_inspect_set_cleared(CliInspectSet *set);
void cli_inspect_set_put(CliInspectSet *set, const char *at, unsigned int size);
int cli_inspect_set_has(const CliInspectSet *set, const char *at, unsigned int size);

// a painting begun over `size` bytes of `src`, each column of each line of no class; a span painted `class_of`, a
// later span over an earlier; and each line's runs of one class written into `parsed` (inspect_paint.c)
void cli_inspect_paint_begun(const char *src, unsigned long long size);
void cli_inspect_paint(CliInspectPlace from, CliInspectPlace to, unsigned char class_of);
void cli_inspect_paint_runs(CliParsed *parsed);
// a fold and a span put on a parse; and the parse's folds made each the widest that starts on its line
void cli_inspect_fold_put(CliParsed *parsed, unsigned int first, unsigned int last);
void cli_inspect_range_put(CliParsed *parsed, CliInspectPlace from, CliInspectPlace to);
void cli_inspect_folds_done(CliParsed *parsed);

// `size` bytes kept in the facts' pool: where they stand, empty where the pool is full and the facts marked full; the
// same of a text written into `line` and taken from it; a finding put on the facts, its fixes those put after it; a fix
// put on the last finding, `title` its title; and an edit put on the last fix (inspect.c)
CliInspectText cli_inspect_kept(CliFacts *facts, const char *text, unsigned long long size);
CliInspectText cli_inspect_line_kept(CliFacts *facts, const CliLine *line);
void cli_inspect_finding_put(CliFacts *facts, CliInspectPlace from, CliInspectPlace to, int severity,
                             const CliLine *message, const char *code, const char *href);
void cli_inspect_fix_put(CliFacts *facts, const CliLine *title);
void cli_inspect_fix_titled(CliFacts *facts, const char *title);
void cli_inspect_edit_put(CliFacts *facts, CliInspectPlace from, CliInspectPlace to, const char *text,
                          unsigned long long size);
// a name put on the facts' list of names, its alias where `alias` is not NULL: its place, CLI_INSPECT_FULL where the
// list is full
unsigned int cli_inspect_name_put(CliFacts *facts, const char *name, unsigned long long size, const char *alias,
                                  unsigned long long alias_size, CliInspectPlace from, CliInspectPlace to);

// the body written again from its members' lines with the methods among them sorted by name, case aside and then by
// their bytes: each member a run of lines, `firsts` to `lasts`, and its name where `names` gives one; the lines between
// members stay as they were, and a method's slot takes the next method in the order of their names. Into `out`: 1, or
// 0 where fewer than two methods are among them or they stand sorted already (inspect.c)
// the physical lines of `size` bytes of `src`, split at each "\n" as Rust's str::split splits them, each where it
// starts and where it ends, into `starts` and `ends`, which hold `most`: their count, at most `most`
unsigned int cli_inspect_physical(const char *src, unsigned long long size, unsigned long long *starts,
                                  unsigned long long *ends, unsigned int most);
int cli_inspect_sorted_body(const char *src, unsigned long long size, const unsigned long long *line_starts,
                            const unsigned long long *line_ends, const unsigned int *firsts, const unsigned int *lasts,
                            const char *const *names, const unsigned int *name_sizes, unsigned int members,
                            CliLine *out);

// the facts, the parse, the docstring and the sorted methods of Python (inspect_py_*.c) and of JavaScript
// (inspect_js_*.c)
void cli_inspect_py_facts(const char *src, unsigned long long size, CliFacts *facts);
void cli_inspect_py_parse(const char *src, unsigned long long size, CliParsed *parsed);
int cli_inspect_py_docstring(const char *src, unsigned long long size, unsigned int line, const char *form,
                             CliInspectEdit *edit, CliLine *text, CliInspectPlace *summary, CliLine *said);
int cli_inspect_py_sort_methods(const char *src, unsigned long long size, unsigned int line, CliInspectEdit *edit,
                                CliLine *text, CliLine *said);
void cli_inspect_js_facts(const char *src, unsigned long long size, CliFacts *facts);
void cli_inspect_js_parse(const char *src, unsigned long long size, CliParsed *parsed);
int cli_inspect_js_docstring(const char *src, unsigned long long size, unsigned int line, CliInspectEdit *edit,
                             CliLine *text, CliInspectPlace *summary, CliLine *said);
int cli_inspect_js_sort_methods(const char *src, unsigned long long size, unsigned int line, CliInspectEdit *edit,
                                CliLine *text, CliLine *said);

// a place among the facts' names that is none
#define CLI_INSPECT_FULL 0xffffffffu

#endif
