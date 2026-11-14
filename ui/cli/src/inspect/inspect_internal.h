// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_internal.h: what the inspections' sources share: a reading of a text by its characters, lines and UTF-16
// columns; the tokens a reader makes and the comments it passes; a painting of each column's class; a set of names;
// and the facts' own writers. One reading runs at a time, under the inspections' lock
#ifndef INSPECT_INTERNAL_H
#define INSPECT_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "inspect.h"

typedef struct CliInspectSizeCtx CliInspectSizeCtx;
typedef struct CliInspectSizeSrcCtx CliInspectSizeSrcCtx;
typedef struct CliInspectFromCtx CliInspectFromCtx;
typedef struct CliInspectSizeFromCtx CliInspectSizeFromCtx;
typedef struct CliInspectTextCtx CliInspectTextCtx;
typedef struct CliInspectSizeSrcStartsCtx CliInspectSizeSrcStartsCtx;
typedef struct CliInspectScanCtx CliInspectScanCtx;
typedef struct CliInspectParsedCtx CliInspectParsedCtx;
typedef struct CliInspectSizeSrcParsedCtx CliInspectSizeSrcParsedCtx;
typedef struct CliInspectSizeSrcFactsCtx CliInspectSizeSrcFactsCtx;
typedef struct CliInspectFromToCtx CliInspectFromToCtx;
typedef struct CliInspectFromToParsedCtx CliInspectFromToParsedCtx;
typedef struct CliInspectLanguageCtx CliInspectLanguageCtx;
typedef struct CliInspectSizeScanCtx CliInspectSizeScanCtx;
typedef struct CliInspectAtCtx CliInspectAtCtx;
typedef struct CliInspectSetCtx CliInspectSetCtx;
typedef struct CliInspectPartitionCtx CliInspectPartitionCtx;
typedef struct CliInspectFactsCtx CliInspectFactsCtx;
typedef struct CliInspectFactsTitleCtx CliInspectFactsTitleCtx;
typedef struct CliInspectFactsLineVerbaLineaCtx CliInspectFactsLineVerbaLineaCtx;
typedef struct CliInspectScanAheadCtx CliInspectScanAheadCtx;
typedef struct CliInspectPartitionSetCtx CliInspectPartitionSetCtx;
typedef struct CliInspectCtx CliInspectCtx;

// The max a reading of one text contains, counted from the text as its cell is taken: its tokens, a token for each
// name, number and other character that is no space and one for each line; its lines; its comments, one for each character
// a comment opens with; the columns a painting contains; and the names a set contains, one for each name of the text
typedef struct
{
    unsigned int tokens;
    unsigned int lines;
    unsigned int comments;
    unsigned long long columns;
    unsigned int names;
} CliInspectMax;

extern CliInspectMax g_cli_inspect_max;

// the max tokens, lines and comments a reading contains, and the max columns a painting contains
#define CLI_INSPECT_TOKENS (g_cli_inspect_max.tokens)
#define CLI_INSPECT_LINES (g_cli_inspect_max.lines)
#define CLI_INSPECT_COMMENTS (g_cli_inspect_max.comments)
#define CLI_INSPECT_COLUMNS (g_cli_inspect_max.columns)

// A carving of a reading's cell into the lists of the sources that read it: where the next list starts, NULL where
// the lists are only measured, and the bytes they take
typedef struct
{
    unsigned char *at;
    unsigned long long bytes;
} CliInspectPartition;

// the next list of a carving, `bytes` long, aligned to eight bytes: its start, NULL where the carving only measures
// (inspect_read_state.c)
void *cli_inspect_partition(const CliInspectPartitionCtx *args);
// the reading of `size` bytes of `text` in `language` taken, under the inspections' lock: its counts into
// g_cli_inspect_max and every list a source of that language reads it into a part of one cell sized to them, 1, or
// 0 where the cell cannot be taken; and the cell given back (inspect_read_state.c)
int cli_inspect_read_state_reserve(const CliInspectLanguageCtx *args);
void cli_inspect_read_state_free(void);
// each source's lists carved for a reading, the scan's, the painting's and the sorting's for every language, the rest
// for theirs (inspect_scan.c, inspect_paint.c, inspect.c, inspect_js_lex.c, inspect_js_parse.c, inspect_js_write.c,
// inspect_js_facts.c, inspect_py_lex.c, inspect_py_parse.c, inspect_py_sort.c, inspect_py_unread.c)
void cli_inspect_scan_layout(const CliInspectPartitionCtx *args);
void cli_inspect_paint_layout(const CliInspectPartitionCtx *args);
void cli_inspect_sort_layout(const CliInspectPartitionCtx *args);
void cli_inspect_js_lex_layout(const CliInspectPartitionCtx *args);
void cli_inspect_js_parse_layout(const CliInspectPartitionCtx *args);
void cli_inspect_js_write_layout(const CliInspectPartitionCtx *args);
void cli_inspect_js_facts_layout(const CliInspectPartitionCtx *args);
void cli_inspect_py_lex_layout(const CliInspectPartitionCtx *args);
void cli_inspect_py_parse_layout(const CliInspectPartitionCtx *args);
void cli_inspect_py_sort_layout(const CliInspectPartitionCtx *args);
void cli_inspect_py_unread_layout(const CliInspectPartitionCtx *args);

// code no path reaches reported as the text before the word that leaves the block, the word, and the text after; the
// words that leave a block in JavaScript are return, throw, break and continue and in Python return, raise, break and
// continue, so the longest is continue, and the report's capacity is the three and its 0
#define CLI_INSPECT_UNREACHED_BEFORE "No path reaches this code: the `"
#define CLI_INSPECT_UNREACHED_AFTER "` before it leaves the block."
#define CLI_INSPECT_LEAVING_MAX_LEN "continue"
#define CLI_INSPECT_UNREACHED_CAPACITY                                                                                 \
    ((sizeof(CLI_INSPECT_UNREACHED_BEFORE) - 1u) + (sizeof(CLI_INSPECT_LEAVING_MAX_LEN) - 1u) +                        \
     sizeof(CLI_INSPECT_UNREACHED_AFTER))
_Static_assert((sizeof(CLI_INSPECT_LEAVING_MAX_LEN) >= sizeof("return")) &&
                   (sizeof(CLI_INSPECT_LEAVING_MAX_LEN) >= sizeof("throw")) &&
                   (sizeof(CLI_INSPECT_LEAVING_MAX_LEN) >= sizeof("break")) &&
                   (sizeof(CLI_INSPECT_LEAVING_MAX_LEN) >= sizeof("raise")),
               "CLI_INSPECT_LEAVING_MAX_LEN must be the longest word that leaves a block");

// A reading of a text by its characters: the text and its size, where it is, its line and its column in UTF-16 units
typedef struct
{
    const char *src;
    unsigned long long size;
    unsigned long long pos;
    unsigned int line;
    unsigned int col;
} CliInspectScan;

// A token: its type, as each language names its types; 1 in `field` where it is a name read inside a Python
// f-string's field; its bytes from `start` to `end`; and its span
typedef struct
{
    unsigned char type;
    unsigned char field;
    unsigned int start;
    unsigned int end;
    CliInspectPosition from;
    CliInspectPosition to;
} CliInspectTok;

// A set of names, each `size` bytes at `at`, `buckets` of them, a power of two, and the buckets it took, to clear it
// by, at max half its buckets; its lists carved from a reading's cell
typedef struct
{
    const char **at;
    unsigned int *size;
    unsigned int *reserved;
    unsigned int count;
    unsigned int buckets;
} CliInspectSet;

// the inspections' lock, the tokens of the reading and their count, its comments' spans and their count, and 1 in
// `g_cli_inspect_over` where a token or a comment did not fit (inspect_scan.c)
extern CliInspectTok *g_cli_inspect_tok;
extern unsigned int g_cli_inspect_toks;
extern CliInspectSpan *g_cli_inspect_comment;
extern unsigned int g_cli_inspect_comments;
extern int g_cli_inspect_over;

// what a reading's character is past the text's end
#define CLI_INSPECT_END 0xffffffffu

// a reading begun at the start of `size` bytes of `src`; the character where it is, and `ahead` characters on,
// CLI_INSPECT_END past the text's end; the character where it is taken, its line and column moved past it, the same
// past the end, where nothing is taken; where it is; a comment's span
// recorded from where it is to the end of its line, before the line's end; a token put on the reading's tokens, its
// span from `from` to where the reading is; and a comment's span recorded (inspect_scan.c)
void cli_inspect_scan_begin(const CliInspectSizeScanCtx *args);
unsigned int cli_inspect_peek(const CliInspectScanAheadCtx *args);
unsigned int cli_inspect_peek_at(const CliInspectScanAheadCtx *args);
unsigned int cli_inspect_bump(const CliInspectSizeScanCtx *args);
CliInspectPosition cli_inspect_position(const CliInspectScanCtx *args);
void cli_inspect_skip_line(const CliInspectSizeScanCtx *args);
void cli_inspect_tok_append(const CliInspectScanCtx *args);
void cli_inspect_comment_append(const CliInspectFromToCtx *args);

// whether a character is alphabetic, alphanumeric or white as Rust's char defines, ASCII and the characters of the Unicode
// properties the regular expressions read (inspect_scan.c)
int cli_inspect_alphabetic(const CliInspectCtx *args);
int cli_inspect_alphanumeric(const CliInspectCtx *args);
int cli_inspect_white(const CliInspectCtx *args);

// a set's lists carved for a reading, its buckets twice the reading's names; a set made empty; a name put in it; and
// whether it contains a name (inspect_scan.c)
void cli_inspect_set_layout(const CliInspectPartitionSetCtx *args);
void cli_inspect_set_clear(const CliInspectPartitionSetCtx *args);
void cli_inspect_set_append(const CliInspectSetCtx *args);
int cli_inspect_set_has(const CliInspectAtCtx *args);

// a painting begun over `size` bytes of `src`, each column of each line of no class; a span painted `class_of`, a
// later span over an earlier; and each line's runs of one class written into `parsed` (inspect_paint.c)
void cli_inspect_paint_begin(const CliInspectSizeSrcParsedCtx *args);
void cli_inspect_paint(const CliInspectFromToCtx *args);
void cli_inspect_paint_runs(const CliInspectSizeSrcParsedCtx *args);
// a fold and a span put on a parse; and the parse's folds made each the widest that starts on its line
void cli_inspect_fold_append(const CliInspectParsedCtx *args);
void cli_inspect_range_append(const CliInspectFromToParsedCtx *args);
void cli_inspect_folds_finish(const CliInspectSizeSrcParsedCtx *args);

// `size` bytes stored in the facts' pool: where they are, empty where the pool is full and the facts marked full; the
// same of a text written into `line` and taken from it; a finding put on the facts, its fixes those put after it; a fix
// put on the last finding, `title` its title; and an edit put on the last fix (inspect.c)
CliInspectText cli_inspect_text_store(const CliInspectTextCtx *args);
CliInspectText cli_inspect_line_store(const CliInspectFactsLineVerbaLineaCtx *args);
void cli_inspect_finding_append(const CliInspectFromCtx *args);
void cli_inspect_fix_append(const CliInspectFactsCtx *args);
void cli_inspect_fix_title(const CliInspectFactsTitleCtx *args);
void cli_inspect_edit_append(const CliInspectTextCtx *args);
// a name put on the facts' list of names, its alias where `alias` is not NULL: its place, CLI_INSPECT_FULL where the
// list is full
unsigned int cli_inspect_name_append(const CliInspectSizeFromCtx *args);

// the body written again from its members' lines with the methods among them sorted by name, case aside and then by
// their bytes: each member a run of lines, `firsts` to `lasts`, and its name where `names` gives one; the lines between
// members stay as they were, and a method's position takes the next method in the order of their names. Into `out`: 1,
// or 0 where fewer than two methods are among them or they are sorted already (inspect.c) the physical lines of
// `size` bytes of `src`, split at each "\n" as Rust's str::split splits them, each where it starts and where it ends,
// into `starts` and `ends`, sized for `max`: their count, at max `max`
unsigned int cli_inspect_physical(const CliInspectSizeSrcStartsCtx *args);
int cli_inspect_sorted_body(const CliInspectSizeCtx *args);

// the facts, the parse, the docstring and the sorted methods of Python (inspect_py_*.c) and of JavaScript
// (inspect_js_*.c)
void cli_inspect_py_facts(const CliInspectSizeSrcFactsCtx *args);
void cli_inspect_py_parse(const CliInspectSizeSrcParsedCtx *args);
int cli_inspect_py_docstring(const CliInspectSizeSrcCtx *args);
int cli_inspect_py_sort_methods(const CliInspectSizeSrcCtx *args);
void cli_inspect_js_facts(const CliInspectSizeSrcFactsCtx *args);
void cli_inspect_js_parse(const CliInspectSizeSrcParsedCtx *args);
int cli_inspect_js_docstring(const CliInspectSizeSrcCtx *args);
int cli_inspect_js_sort_methods(const CliInspectSizeSrcCtx *args);

// a place among the facts' names that is none
#define CLI_INSPECT_FULL 0xffffffffu

struct CliInspectSizeCtx
{
    unsigned long long size;
    const char *src;
    const unsigned long long *line_starts;
    const unsigned long long *line_ends;
    const unsigned int *firsts;
    const unsigned int *lasts;
    const char *const *names;
    const unsigned int *name_sizes;
    unsigned int members;
    VerbaLinea *out;
};

struct CliInspectSizeSrcCtx
{
    unsigned long long size;
    const char *src;
    unsigned int line;
    const char *form;
    CliInspectEdit *edit;
    VerbaLinea *text;
    CliInspectPosition *summary;
    VerbaLinea *err_msg;
};

struct CliInspectFromCtx
{
    CliInspectPosition from;
    CliInspectPosition to;
    CliFacts *facts;
    int severity;
    const VerbaLinea *message;
    const char *code;
    const char *href;
};

struct CliInspectSizeFromCtx
{
    unsigned long long size;
    CliInspectPosition from;
    CliInspectPosition to;
    CliFacts *facts;
    const char *name;
    const char *alias;
    unsigned long long alias_size;
};

struct CliInspectTextCtx
{
    const char *text_text;
    unsigned long long size;
    CliInspectPosition from;
    CliInspectPosition to;
    CliFacts *facts;
};

struct CliInspectSizeSrcStartsCtx
{
    unsigned long long size;
    const char *src;
    unsigned long long *starts;
    unsigned long long *ends;
    unsigned int max;
};

struct CliInspectScanCtx
{
    const CliInspectScan *scan;
    unsigned char type;
    unsigned long long start;
    CliInspectPosition from;
    int field;
};

struct CliInspectParsedCtx
{
    CliParsed *parsed;
    unsigned int first;
    unsigned int last;
};

struct CliInspectSizeSrcParsedCtx
{
    unsigned long long size;
    const char *src;
    CliParsed *parsed;
};

struct CliInspectSizeSrcFactsCtx
{
    unsigned long long size;
    const char *src;
    CliFacts *facts;
};

struct CliInspectFromToCtx
{
    CliInspectPosition from;
    CliInspectPosition to;
    unsigned char class_of;
};

struct CliInspectFromToParsedCtx
{
    CliInspectPosition from;
    CliInspectPosition to;
    CliParsed *parsed;
};

struct CliInspectLanguageCtx
{
    const char *language;
    const char *text_text;
    unsigned long long size;
};

struct CliInspectSizeScanCtx
{
    unsigned long long size;
    CliInspectScan *scan_out;
    const char *src;
};

struct CliInspectAtCtx
{
    const char *at;
    unsigned int size_u32;
    const CliInspectSet *set_read;
};

struct CliInspectSetCtx
{
    CliInspectSet *set;
    const char *at;
    unsigned int size_u32;
};

struct CliInspectPartitionCtx
{
    CliInspectPartition *partition;
    unsigned long long bytes;
};

struct CliInspectFactsCtx
{
    CliFacts *facts;
    const VerbaLinea *title;
};

struct CliInspectFactsTitleCtx
{
    CliFacts *facts;
    const char *title_text;
};

struct CliInspectFactsLineVerbaLineaCtx
{
    CliFacts *facts;
    const VerbaLinea *line_verba_linea_pointer;
};

struct CliInspectScanAheadCtx
{
    const CliInspectScan *scan;
    unsigned int ahead;
};

struct CliInspectPartitionSetCtx
{
    CliInspectPartition *partition;
    CliInspectSet *set;
};

struct CliInspectCtx
{
    unsigned int character;
};

typedef struct
{
    void *(*partition)(const CliInspectPartitionCtx *args);
    void (*scan_layout)(const CliInspectPartitionCtx *args);
    void (*paint_layout)(const CliInspectPartitionCtx *args);
    void (*sort_layout)(const CliInspectPartitionCtx *args);
    void (*js_lex_layout)(const CliInspectPartitionCtx *args);
    void (*js_parse_layout)(const CliInspectPartitionCtx *args);
    void (*js_write_layout)(const CliInspectPartitionCtx *args);
    void (*js_facts_layout)(const CliInspectPartitionCtx *args);
    void (*py_lex_layout)(const CliInspectPartitionCtx *args);
    void (*py_parse_layout)(const CliInspectPartitionCtx *args);
    void (*py_sort_layout)(const CliInspectPartitionCtx *args);
    void (*py_unread_layout)(const CliInspectPartitionCtx *args);
} CliInspectPartitionNs;
EMBED_TABLE_LAYOUT(CliInspectPartitionNs, partition, scan_layout, paint_layout, sort_layout, js_lex_layout,
                   js_parse_layout, js_write_layout, js_facts_layout, py_lex_layout, py_parse_layout, py_sort_layout,
                   py_unread_layout);
EMBED_TABLE_STORAGE CliInspectPartitionNs cli_inspect_partition_internal EMBED_UNUSED = {
    .partition = cli_inspect_partition,
    .scan_layout = cli_inspect_scan_layout,
    .paint_layout = cli_inspect_paint_layout,
    .sort_layout = cli_inspect_sort_layout,
    .js_lex_layout = cli_inspect_js_lex_layout,
    .js_parse_layout = cli_inspect_js_parse_layout,
    .js_write_layout = cli_inspect_js_write_layout,
    .js_facts_layout = cli_inspect_js_facts_layout,
    .py_lex_layout = cli_inspect_py_lex_layout,
    .py_parse_layout = cli_inspect_py_parse_layout,
    .py_sort_layout = cli_inspect_py_sort_layout,
    .py_unread_layout = cli_inspect_py_unread_layout,
};

typedef struct
{
    int (*read_state_reserve)(const CliInspectLanguageCtx *args);
} CliInspectLanguageNsTable;
EMBED_TABLE_LAYOUT(CliInspectLanguageNsTable, read_state_reserve);
EMBED_TABLE_STORAGE CliInspectLanguageNsTable cli_inspect_language_internal EMBED_UNUSED = {
    .read_state_reserve = cli_inspect_read_state_reserve,
};

typedef struct
{
    void (*scan_begin)(const CliInspectSizeScanCtx *args);
    unsigned int (*bump)(const CliInspectSizeScanCtx *args);
    void (*skip_line)(const CliInspectSizeScanCtx *args);
} CliInspectSizeScanNs;
EMBED_TABLE_LAYOUT(CliInspectSizeScanNs, scan_begin, bump, skip_line);
EMBED_TABLE_STORAGE CliInspectSizeScanNs cli_inspect_size_scan_internal EMBED_UNUSED = {
    .scan_begin = cli_inspect_scan_begin,
    .bump = cli_inspect_bump,
    .skip_line = cli_inspect_skip_line,
};

typedef struct
{
    unsigned int (*peek)(const CliInspectScanAheadCtx *args);
    unsigned int (*peek_at)(const CliInspectScanAheadCtx *args);
} CliInspectScanAheadNs;
EMBED_TABLE_LAYOUT(CliInspectScanAheadNs, peek, peek_at);
EMBED_TABLE_STORAGE CliInspectScanAheadNs cli_inspect_scan_ahead_internal EMBED_UNUSED = {
    .peek = cli_inspect_peek,
    .peek_at = cli_inspect_peek_at,
};

typedef struct
{
    CliInspectPosition (*place)(const CliInspectScanCtx *args);
    void (*tok_append)(const CliInspectScanCtx *args);
} CliInspectScanNs;
EMBED_TABLE_LAYOUT(CliInspectScanNs, place, tok_append);
EMBED_TABLE_STORAGE CliInspectScanNs cli_inspect_scan_internal EMBED_UNUSED = {
    .place = cli_inspect_position,
    .tok_append = cli_inspect_tok_append,
};

typedef struct
{
    void (*comment_append)(const CliInspectFromToCtx *args);
    void (*paint)(const CliInspectFromToCtx *args);
} CliInspectFromToNs;
EMBED_TABLE_LAYOUT(CliInspectFromToNs, comment_append, paint);
EMBED_TABLE_STORAGE CliInspectFromToNs cli_inspect_from_to_internal EMBED_UNUSED = {
    .comment_append = cli_inspect_comment_append,
    .paint = cli_inspect_paint,
};

typedef struct
{
    int (*alphabetic)(const CliInspectCtx *args);
    int (*alphanumeric)(const CliInspectCtx *args);
    int (*white)(const CliInspectCtx *args);
} CliInspectNsTable;
EMBED_TABLE_LAYOUT(CliInspectNsTable, alphabetic, alphanumeric, white);
EMBED_TABLE_STORAGE CliInspectNsTable cli_inspect_internal EMBED_UNUSED = {
    .alphabetic = cli_inspect_alphabetic,
    .alphanumeric = cli_inspect_alphanumeric,
    .white = cli_inspect_white,
};

typedef struct
{
    void (*set_layout)(const CliInspectPartitionSetCtx *args);
    void (*set_clear)(const CliInspectPartitionSetCtx *args);
} CliInspectPartitionSetNs;
EMBED_TABLE_LAYOUT(CliInspectPartitionSetNs, set_layout, set_clear);
EMBED_TABLE_STORAGE CliInspectPartitionSetNs cli_inspect_partition_set_internal EMBED_UNUSED = {
    .set_layout = cli_inspect_set_layout,
    .set_clear = cli_inspect_set_clear,
};

typedef struct
{
    void (*set_append)(const CliInspectSetCtx *args);
} CliInspectSetNs;
EMBED_TABLE_LAYOUT(CliInspectSetNs, set_append);
EMBED_TABLE_STORAGE CliInspectSetNs cli_inspect_set_internal EMBED_UNUSED = {
    .set_append = cli_inspect_set_append,
};

typedef struct
{
    int (*set_has)(const CliInspectAtCtx *args);
} CliInspectAtNs;
EMBED_TABLE_LAYOUT(CliInspectAtNs, set_has);
EMBED_TABLE_STORAGE CliInspectAtNs cli_inspect_at_internal EMBED_UNUSED = {
    .set_has = cli_inspect_set_has,
};

typedef struct
{
    void (*paint_begin)(const CliInspectSizeSrcParsedCtx *args);
    void (*paint_runs)(const CliInspectSizeSrcParsedCtx *args);
    void (*folds_finish)(const CliInspectSizeSrcParsedCtx *args);
    void (*py_parse)(const CliInspectSizeSrcParsedCtx *args);
    void (*js_parse)(const CliInspectSizeSrcParsedCtx *args);
} CliInspectSizeSrcParsedNs;
EMBED_TABLE_LAYOUT(CliInspectSizeSrcParsedNs, paint_begin, paint_runs, folds_finish, py_parse, js_parse);
EMBED_TABLE_STORAGE CliInspectSizeSrcParsedNs cli_inspect_size_src_parsed_internal EMBED_UNUSED = {
    .paint_begin = cli_inspect_paint_begin,
    .paint_runs = cli_inspect_paint_runs,
    .folds_finish = cli_inspect_folds_finish,
    .py_parse = cli_inspect_py_parse,
    .js_parse = cli_inspect_js_parse,
};

typedef struct
{
    void (*fold_append)(const CliInspectParsedCtx *args);
} CliInspectParsedNsTable;
EMBED_TABLE_LAYOUT(CliInspectParsedNsTable, fold_append);
EMBED_TABLE_STORAGE CliInspectParsedNsTable cli_inspect_parsed_internal EMBED_UNUSED = {
    .fold_append = cli_inspect_fold_append,
};

typedef struct
{
    void (*range_append)(const CliInspectFromToParsedCtx *args);
} CliInspectFromToParsedNs;
EMBED_TABLE_LAYOUT(CliInspectFromToParsedNs, range_append);
EMBED_TABLE_STORAGE CliInspectFromToParsedNs cli_inspect_from_to_parsed_internal EMBED_UNUSED = {
    .range_append = cli_inspect_range_append,
};

typedef struct
{
    CliInspectText (*text_store)(const CliInspectTextCtx *args);
    void (*edit_append)(const CliInspectTextCtx *args);
} CliInspectTextNs;
EMBED_TABLE_LAYOUT(CliInspectTextNs, text_store, edit_append);
EMBED_TABLE_STORAGE CliInspectTextNs cli_inspect_text_internal EMBED_UNUSED = {
    .text_store = cli_inspect_text_store,
    .edit_append = cli_inspect_edit_append,
};

typedef struct
{
    CliInspectText (*line_store)(const CliInspectFactsLineVerbaLineaCtx *args);
} CliInspectFactsLineVerbaLineaNs;
EMBED_TABLE_LAYOUT(CliInspectFactsLineVerbaLineaNs, line_store);
EMBED_TABLE_STORAGE CliInspectFactsLineVerbaLineaNs cli_inspect_facts_line_verba_linea_internal EMBED_UNUSED = {
    .line_store = cli_inspect_line_store,
};

typedef struct
{
    void (*finding_append)(const CliInspectFromCtx *args);
} CliInspectFromNs;
EMBED_TABLE_LAYOUT(CliInspectFromNs, finding_append);
EMBED_TABLE_STORAGE CliInspectFromNs cli_inspect_from_internal EMBED_UNUSED = {
    .finding_append = cli_inspect_finding_append,
};

typedef struct
{
    void (*fix_append)(const CliInspectFactsCtx *args);
} CliInspectFactsNsTable;
EMBED_TABLE_LAYOUT(CliInspectFactsNsTable, fix_append);
EMBED_TABLE_STORAGE CliInspectFactsNsTable cli_inspect_facts_internal EMBED_UNUSED = {
    .fix_append = cli_inspect_fix_append,
};

typedef struct
{
    void (*fix_title)(const CliInspectFactsTitleCtx *args);
} CliInspectFactsTitleNs;
EMBED_TABLE_LAYOUT(CliInspectFactsTitleNs, fix_title);
EMBED_TABLE_STORAGE CliInspectFactsTitleNs cli_inspect_facts_title_internal EMBED_UNUSED = {
    .fix_title = cli_inspect_fix_title,
};

typedef struct
{
    unsigned int (*name_append)(const CliInspectSizeFromCtx *args);
} CliInspectSizeFromNs;
EMBED_TABLE_LAYOUT(CliInspectSizeFromNs, name_append);
EMBED_TABLE_STORAGE CliInspectSizeFromNs cli_inspect_size_from_internal EMBED_UNUSED = {
    .name_append = cli_inspect_name_append,
};

typedef struct
{
    unsigned int (*physical)(const CliInspectSizeSrcStartsCtx *args);
} CliInspectSizeSrcStartsNs;
EMBED_TABLE_LAYOUT(CliInspectSizeSrcStartsNs, physical);
EMBED_TABLE_STORAGE CliInspectSizeSrcStartsNs cli_inspect_size_src_starts_internal EMBED_UNUSED = {
    .physical = cli_inspect_physical,
};

typedef struct
{
    int (*sorted_body)(const CliInspectSizeCtx *args);
} CliInspectSizeNs;
EMBED_TABLE_LAYOUT(CliInspectSizeNs, sorted_body);
EMBED_TABLE_STORAGE CliInspectSizeNs cli_inspect_size_internal EMBED_UNUSED = {
    .sorted_body = cli_inspect_sorted_body,
};

typedef struct
{
    void (*py_facts)(const CliInspectSizeSrcFactsCtx *args);
    void (*js_facts)(const CliInspectSizeSrcFactsCtx *args);
} CliInspectSizeSrcFactsNs;
EMBED_TABLE_LAYOUT(CliInspectSizeSrcFactsNs, py_facts, js_facts);
EMBED_TABLE_STORAGE CliInspectSizeSrcFactsNs cli_inspect_size_src_facts_internal EMBED_UNUSED = {
    .py_facts = cli_inspect_py_facts,
    .js_facts = cli_inspect_js_facts,
};

typedef struct
{
    int (*py_docstring)(const CliInspectSizeSrcCtx *args);
    int (*py_sort_methods)(const CliInspectSizeSrcCtx *args);
    int (*js_docstring)(const CliInspectSizeSrcCtx *args);
    int (*js_sort_methods)(const CliInspectSizeSrcCtx *args);
} CliInspectSizeSrcNs;
EMBED_TABLE_LAYOUT(CliInspectSizeSrcNs, py_docstring, py_sort_methods, js_docstring, js_sort_methods);
EMBED_TABLE_STORAGE CliInspectSizeSrcNs cli_inspect_size_src_internal EMBED_UNUSED = {
    .py_docstring = cli_inspect_py_docstring,
    .py_sort_methods = cli_inspect_py_sort_methods,
    .js_docstring = cli_inspect_js_docstring,
    .js_sort_methods = cli_inspect_js_sort_methods,
};

#endif
