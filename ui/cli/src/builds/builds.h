// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds.h: build files: Gradle's build and settings scripts, in Groovy or Kotlin, and its version catalogs; Maven's
// `pom.xml`; and SCons's SConstruct and SConscript. Each is checked and completed as it is written, by its own text
// and what the tree around it contains: a catalog's aliases where a script names them, a POM's properties and its
// parent's, the SConscripts a script reads and the names they export.
//
// A dependency whose version is a snapshot is noted, and its note offers the command that fetches the snapshot's
// newest build, run in the terminal; the snapshots line gives the one for a whole build. Maven's reader also reads a
// POM's compiler settings and Maven's own, from `settings.xml`, and writes them again as they are set.
//
// What the readers share, and the schemas' reader with them: a text's lines, to turn a byte into a place; the findings
// of a file, each with the fixes it offers, an edit of the file or a line to run in the terminal; the completion
// items of what was typed; and the names nearest one that is not known (builds.c, builds_help.c, builds_adjacent.c,
// version_catalog*.c, gradle*.c, maven*.c, scons*.c)
#ifndef BUILDS_H
#define BUILDS_H
#include "../command_line/callback_cfg.h"
#include "../text/edits.h"
#include "embed_dispatch_layout.h"

typedef struct CliBuildsTextCfg CliBuildsTextCfg;
typedef struct CliBuildsPathCfg CliBuildsPathCfg;
typedef struct CliBuildsItemsCfg CliBuildsItemsCfg;
typedef struct CliBuildsLinesCfg CliBuildsLinesCfg;
typedef struct CliBuildsFoundCfg CliBuildsFoundCfg;
typedef struct CliBuildsPathTextCfg CliBuildsPathTextCfg;
typedef struct CliBuildsItemsTypeCfg CliBuildsItemsTypeCfg;
typedef struct CliBuildsTextSizeCfg CliBuildsTextSizeCfg;
typedef struct CliBuildsPathRootCfg CliBuildsPathRootCfg;
typedef struct CliBuildsACfg CliBuildsACfg;
typedef struct CliBuildsRootCfg CliBuildsRootCfg;
typedef struct CliBuildsSizeCfg CliBuildsSizeCfg;
typedef struct CliBuildsSizeAdjacentCfg CliBuildsSizeAdjacentCfg;
typedef struct CliBuildsTextAtCfg CliBuildsTextAtCfg;
typedef struct CliBuildsCfg CliBuildsCfg;
typedef struct CliBuildsLineCfg CliBuildsLineCfg;
typedef struct CliBuildsLinesAtCfg CliBuildsLinesAtCfg;
typedef struct CliBuildsSizeVersionCfg CliBuildsSizeVersionCfg;

// the types of build file read here, 0 for none
#define CLI_BUILD_GRADLE 1u
#define CLI_BUILD_CATALOG 2u
#define CLI_BUILD_MAVEN 3u
#define CLI_BUILD_SCONS 4u

// how bad a finding is, as the protocol numbers it
#define CLI_BUILD_ERROR 1u
#define CLI_BUILD_WARNING 2u
#define CLI_BUILD_NOTE 3u

// the max findings of one file, the max fixes of them, the max bytes their texts take, and no finding; the findings
// and fixes their capacities are taken with; and the bytes the texts have free past the last stored, the capacity a
// message or a title is written in before it is stored
#define CLI_BUILD_FINDINGS 4096u
#define CLI_BUILD_FIXES 16384u
#define CLI_BUILD_TEXT (1u << 21u)
#define CLI_BUILD_NONE 0xffffffffu
#define CLI_BUILD_FIRST 16u
#define CLI_BUILD_MSG_MIN_LEN 16384u

// the type of build file at `path`, by its name, 0 where it is none
unsigned int cli_build_type_get(const CliBuildsPathCfg *args);

// the diagnostics of the build file at `path` of the tree at `root`, `size` bytes of its text, written as the
// protocol's array of them into `out`
void cli_build_check(const CliBuildsPathCfg *args);
// what completes the word before `line`, `col` of the build file at `path`, `size` bytes of its text, into `items`,
// each label once
void cli_build_complete(const CliBuildsPathCfg *args);
// the names SCons gives the SConstruct or SConscript at `path` without its importing them: its globals and the names
// its `Import` calls take, each passed to `each`; none for any other file
void cli_build_input_names(const CliBuildsPathTextCfg *args);
// the folder, in the tree, and the command line that update every snapshot dependency of the build `path`, NULL for
// none, belongs to: Maven's where it is a POM or a POM is beside it, Gradle's where it is a Gradle file or one
// is beside it, and either one at the tree's top where `path` is neither; each into what is sized for
// CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_build_snapshots_line(const CliBuildsPathRootCfg *args);

// Where each line of a text starts, to turn a byte of it into a place: the starts of its first `count` lines, those
// past them found by reading on
typedef struct
{
    const char *text;
    unsigned long long size;
    const unsigned int *start;
    unsigned int count;
} CliBuildLines;

// the lines of `size` bytes of `text`, their starts stored in `starts`, sized for `max`; the line and the column, in
// UTF-16 units, of byte `at`; and the byte line `line` starts at, the text's end past its last (builds_help.c)
void cli_build_lines_get(const CliBuildsTextSizeCfg *args);
// the lines of `size` bytes of `text` indexed whole, their starts in a cell counted to g_cli_build_owner sized to
// them: the cell, given back with g_cli_guard->persistent_buf_release once the lines are done with, or NULL with no
// line indexed where it is refused
unsigned int *cli_build_lines_alloc(const CliBuildsTextSizeCfg *args);
CliTextPosition cli_build_position(const CliBuildsLinesAtCfg *args);
unsigned long long cli_build_line_start(const CliBuildsLineCfg *args);

// A finding in a file: its span, how bad it is, its code, and its message in Markdown, a text of its findings'
typedef struct
{
    CliTextPosition from;
    CliTextPosition to;
    unsigned int severity;
    const char *code;
    unsigned int message_at;
    unsigned int message_size;
} CliBuildFinding;

// A fix a finding offers: the finding, its title, and either the edit that writes `text` over `from` to `to`, and over
// `also_from` to `also_to` too where `also`, or, where `run`, the line `text` run in the terminal in `folder` of the
// tree
typedef struct
{
    unsigned int finding;
    int run;
    unsigned int title_at;
    unsigned int title_size;
    CliTextPosition from;
    CliTextPosition to;
    int also;
    CliTextPosition also_from;
    CliTextPosition also_to;
    unsigned int text_at;
    unsigned int text_size;
    unsigned int folder_at;
    unsigned int folder_size;
} CliBuildFix;

// A file's findings and their fixes, and the texts they take, each in a cell at their max, with 1 in `full` where one
// did not fit
typedef struct
{
    CliBuildFinding *finding;
    unsigned int count;
    unsigned int finding_capacity;
    CliBuildFix *fix;
    unsigned int fixes;
    unsigned int fix_capacity;
    char *text;
    unsigned int used;
    unsigned int text_capacity;
    int full;
} CliBuildFindings;

// findings made empty, those allocated before given back; their cells given back; a line over the capacity left in
// their texts, CLI_BUILD_MSG_MIN_LEN bytes or more, a message or a title written into it there retained by the finding
// or the fix made of it next, one at a time; a finding of `message` over the bytes `from` to `to`: its place, or
// CLI_BUILD_NONE where it does not fit; a fix of the finding at `finding` that writes `size` bytes of `text` over the
// bytes `from` to `to`; and one that runs `line` in the terminal in `folder` of the tree (builds_help.c)
void cli_build_findings_begin(const CliBuildsLinesCfg *args);
VerbaLinea cli_build_msg_line(const CliBuildsLinesCfg *args);
unsigned int cli_build_finding(const CliBuildsLinesCfg *args);
void cli_build_edit_fix(const CliBuildsTextCfg *args);
void cli_build_run_fix(const CliBuildsFoundCfg *args);
// a fix of the finding at `finding` that writes `size` bytes of `text` over the bytes `from` to `to` and over
// `also_from` to `also_to` (builds_help.c)
void cli_build_edit_fix_twice(const CliBuildsTextCfg *args);
// the findings written as the protocol's array of diagnostics, each `{"range", "severity", "message": {"type",
// "value"}, "source": "orior", "code", "data": {"orior": fixes}}`
void cli_build_findings_write(const CliBuildsCfg *args);

// the byte of `size` bytes of `text` at `line`, `col`, the column in UTF-16 units; whether a character is of a word, as
// the editor reads one; and the start of the run of characters `of` takes that ends at byte `at`, on its line
unsigned long long cli_build_byte_at(const CliBuildsPathCfg *args);
int cli_build_word_character(const CliEachLetterCfg *args);
unsigned long long cli_build_run_before(const CliBuildsTextAtCfg *args);

// the item of a value put where it is after what was typed before the word the cursor is in: of
// `maven.compiler.so` typed, `source` for `maven.compiler.source`; a value that does not start with what was typed is
// left out; and plain items for `words`, `count` of them, each of `type` with `detail`, `()` left off what each shows
void cli_build_relative(const CliBuildsItemsCfg *args);
void cli_build_items(const CliBuildsItemsTypeCfg *args);

// The names nearest one that is not known, near enough to be what was meant, the nearest first, at max three, each a
// text of the caller's
typedef struct
{
    const char *name;
    unsigned long long name_size;
    const char *adjacent[3];
    unsigned long long adjacent_size[3];
    unsigned long long far[3];
    unsigned int count;
} CliBuildAdjacent;

// the fewest edits of one character that turn `a` into `b`; the nearest names begun for `name`; and a known name offered
// to them
unsigned long long cli_build_distance(const CliBuildsACfg *args);
void cli_build_adjacent_begin(const CliBuildsSizeCfg *args);
void cli_build_adjacent_offer(const CliBuildsSizeAdjacentCfg *args);

// `folder` as a path in the tree at `root`, with forward slashes, empty for the top, into `out`, sized for
// CLI_PATH_MAX_LEN; and whether a version is a snapshot's
void cli_build_inside(const CliBuildsRootCfg *args);
int cli_build_snapshot(const CliBuildsSizeVersionCfg *args);

struct CliBuildsTextCfg
{
    const char *text;
    unsigned long long size;
    const CliBuildLines *lines;
    CliBuildFindings *found;
    unsigned long long from;
    unsigned long long to;
    unsigned int finding;
    const VerbaLinea *title;
    unsigned long long also_from;
    unsigned long long also_to;
};

struct CliBuildsPathCfg
{
    const char *path;
    const char *root;
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
    unsigned int line;
    unsigned int col;
    CliCompletions *items;
};

struct CliBuildsItemsCfg
{
    CliCompletions *items;
    const char *token;
    unsigned long long token_size;
    const char *value;
    unsigned long long value_size;
    const char *type;
    const char *detail;
    unsigned long long detail_size;
};

struct CliBuildsLinesCfg
{
    const CliBuildLines *lines;
    CliBuildFindings *found;
    const VerbaLinea *message;
    unsigned long long from;
    unsigned long long to;
    unsigned int severity;
    const char *code;
};

struct CliBuildsFoundCfg
{
    CliBuildFindings *found;
    unsigned int finding;
    const VerbaLinea *title;
    const char *folder;
    unsigned long long folder_size;
    const char *line_text;
    unsigned long long line_size;
};

struct CliBuildsPathTextCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    void (*each)(const CliEachNameSizeCfg *args);
    void *context;
};

struct CliBuildsItemsTypeCfg
{
    CliCompletions *items;
    const char *type;
    const char *detail;
    const char *const *words;
    unsigned int count;
};

struct CliBuildsTextSizeCfg
{
    const char *text;
    unsigned long long size;
    unsigned int *starts;
    unsigned int max;
    CliBuildLines *lines_out;
};

struct CliBuildsPathRootCfg
{
    const char *path;
    const char *root;
    char *folder_out;
    char *line_capacity;
    VerbaLinea *err_msg;
};

struct CliBuildsACfg
{
    const char *a;
    unsigned long long a_size;
    const char *b;
    unsigned long long b_size;
};

struct CliBuildsRootCfg
{
    const char *root;
    const char *folder;
    char *out_capacity;
};

struct CliBuildsSizeCfg
{
    unsigned long long size;
    CliBuildAdjacent *adjacent;
    const char *name;
};

struct CliBuildsSizeAdjacentCfg
{
    unsigned long long size;
    CliBuildAdjacent *adjacent;
    const char *one;
};

struct CliBuildsTextAtCfg
{
    const char *text;
    unsigned long long at;
    int (*of)(const CliEachLetterCfg *args);
};

struct CliBuildsCfg
{
    VerbaLinea *out;
    const CliBuildFindings *found_read;
};

struct CliBuildsLineCfg
{
    unsigned int line;
    const CliBuildLines *lines;
};

struct CliBuildsLinesAtCfg
{
    const CliBuildLines *lines;
    unsigned long long at;
};

struct CliBuildsSizeVersionCfg
{
    unsigned long long size;
    const char *version;
};

typedef struct
{
    unsigned int (*build_type_get)(const CliBuildsPathCfg *args);
    void (*build_check)(const CliBuildsPathCfg *args);
    void (*build_complete)(const CliBuildsPathCfg *args);
    unsigned long long (*build_byte_at)(const CliBuildsPathCfg *args);
} CliBuildsPathNs;
EMBED_TABLE_LAYOUT(CliBuildsPathNs, build_type_get, build_check, build_complete, build_byte_at);
EMBED_TABLE_STORAGE CliBuildsPathNs cli_builds_path EMBED_UNUSED = {
    .build_type_get = cli_build_type_get,
    .build_check = cli_build_check,
    .build_complete = cli_build_complete,
    .build_byte_at = cli_build_byte_at,
};

typedef struct
{
    void (*build_input_names)(const CliBuildsPathTextCfg *args);
} CliBuildsPathTextNs;
EMBED_TABLE_LAYOUT(CliBuildsPathTextNs, build_input_names);
EMBED_TABLE_STORAGE CliBuildsPathTextNs cli_builds_path_text EMBED_UNUSED = {
    .build_input_names = cli_build_input_names,
};

typedef struct
{
    int (*build_snapshots_line)(const CliBuildsPathRootCfg *args);
} CliBuildsPathRootNs;
EMBED_TABLE_LAYOUT(CliBuildsPathRootNs, build_snapshots_line);
EMBED_TABLE_STORAGE CliBuildsPathRootNs cli_builds_path_root EMBED_UNUSED = {
    .build_snapshots_line = cli_build_snapshots_line,
};

typedef struct
{
    void (*build_lines_get)(const CliBuildsTextSizeCfg *args);
    unsigned int *(*build_lines_alloc)(const CliBuildsTextSizeCfg *args);
} CliBuildsTextSizeNs;
EMBED_TABLE_LAYOUT(CliBuildsTextSizeNs, build_lines_get, build_lines_alloc);
EMBED_TABLE_STORAGE CliBuildsTextSizeNs cli_builds_text_size EMBED_UNUSED = {
    .build_lines_get = cli_build_lines_get,
    .build_lines_alloc = cli_build_lines_alloc,
};

typedef struct
{
    CliTextPosition (*build_position)(const CliBuildsLinesAtCfg *args);
} CliBuildsLinesAtNs;
EMBED_TABLE_LAYOUT(CliBuildsLinesAtNs, build_position);
EMBED_TABLE_STORAGE CliBuildsLinesAtNs cli_builds_lines_at EMBED_UNUSED = {
    .build_position = cli_build_position,
};

typedef struct
{
    unsigned long long (*build_line_start)(const CliBuildsLineCfg *args);
} CliBuildsLineNs;
EMBED_TABLE_LAYOUT(CliBuildsLineNs, build_line_start);
EMBED_TABLE_STORAGE CliBuildsLineNs cli_builds_line EMBED_UNUSED = {
    .build_line_start = cli_build_line_start,
};

typedef struct
{
    void (*build_findings_begin)(const CliBuildsLinesCfg *args);
    VerbaLinea (*build_msg_line)(const CliBuildsLinesCfg *args);
    unsigned int (*build_finding)(const CliBuildsLinesCfg *args);
} CliBuildsLinesNs;
EMBED_TABLE_LAYOUT(CliBuildsLinesNs, build_findings_begin, build_msg_line, build_finding);
EMBED_TABLE_STORAGE CliBuildsLinesNs cli_builds_lines EMBED_UNUSED = {
    .build_findings_begin = cli_build_findings_begin,
    .build_msg_line = cli_build_msg_line,
    .build_finding = cli_build_finding,
};

typedef struct
{
    void (*build_edit_fix)(const CliBuildsTextCfg *args);
    void (*build_edit_fix_twice)(const CliBuildsTextCfg *args);
} CliBuildsTextNs;
EMBED_TABLE_LAYOUT(CliBuildsTextNs, build_edit_fix, build_edit_fix_twice);
EMBED_TABLE_STORAGE CliBuildsTextNs cli_builds_text EMBED_UNUSED = {
    .build_edit_fix = cli_build_edit_fix,
    .build_edit_fix_twice = cli_build_edit_fix_twice,
};

typedef struct
{
    void (*build_run_fix)(const CliBuildsFoundCfg *args);
} CliBuildsFoundNs;
EMBED_TABLE_LAYOUT(CliBuildsFoundNs, build_run_fix);
EMBED_TABLE_STORAGE CliBuildsFoundNs cli_builds_found EMBED_UNUSED = {
    .build_run_fix = cli_build_run_fix,
};

typedef struct
{
    void (*build_findings_write)(const CliBuildsCfg *args);
} CliBuildsNsTable;
EMBED_TABLE_LAYOUT(CliBuildsNsTable, build_findings_write);
EMBED_TABLE_STORAGE CliBuildsNsTable cli_builds_table EMBED_UNUSED = {
    .build_findings_write = cli_build_findings_write,
};

typedef struct
{
    int (*build_word_character)(const CliEachLetterCfg *args);
} CliEachLetterNs;
EMBED_TABLE_LAYOUT(CliEachLetterNs, build_word_character);
EMBED_TABLE_STORAGE CliEachLetterNs cli_each_character EMBED_UNUSED = {
    .build_word_character = cli_build_word_character,
};

typedef struct
{
    unsigned long long (*build_run_before)(const CliBuildsTextAtCfg *args);
} CliBuildsTextAtNs;
EMBED_TABLE_LAYOUT(CliBuildsTextAtNs, build_run_before);
EMBED_TABLE_STORAGE CliBuildsTextAtNs cli_builds_text_at EMBED_UNUSED = {
    .build_run_before = cli_build_run_before,
};

typedef struct
{
    void (*build_relative)(const CliBuildsItemsCfg *args);
} CliBuildsItemsNs;
EMBED_TABLE_LAYOUT(CliBuildsItemsNs, build_relative);
EMBED_TABLE_STORAGE CliBuildsItemsNs cli_builds_items EMBED_UNUSED = {
    .build_relative = cli_build_relative,
};

typedef struct
{
    void (*build_items)(const CliBuildsItemsTypeCfg *args);
} CliBuildsItemsTypeNs;
EMBED_TABLE_LAYOUT(CliBuildsItemsTypeNs, build_items);
EMBED_TABLE_STORAGE CliBuildsItemsTypeNs cli_builds_items_type EMBED_UNUSED = {
    .build_items = cli_build_items,
};

typedef struct
{
    unsigned long long (*build_distance)(const CliBuildsACfg *args);
} CliBuildsANs;
EMBED_TABLE_LAYOUT(CliBuildsANs, build_distance);
EMBED_TABLE_STORAGE CliBuildsANs cli_builds_a EMBED_UNUSED = {
    .build_distance = cli_build_distance,
};

typedef struct
{
    void (*build_adjacent_begin)(const CliBuildsSizeCfg *args);
} CliBuildsSizeNs;
EMBED_TABLE_LAYOUT(CliBuildsSizeNs, build_adjacent_begin);
EMBED_TABLE_STORAGE CliBuildsSizeNs cli_builds_size EMBED_UNUSED = {
    .build_adjacent_begin = cli_build_adjacent_begin,
};

typedef struct
{
    void (*build_adjacent_offer)(const CliBuildsSizeAdjacentCfg *args);
} CliBuildsSizeAdjacentNs;
EMBED_TABLE_LAYOUT(CliBuildsSizeAdjacentNs, build_adjacent_offer);
EMBED_TABLE_STORAGE CliBuildsSizeAdjacentNs cli_builds_size_adjacent EMBED_UNUSED = {
    .build_adjacent_offer = cli_build_adjacent_offer,
};

typedef struct
{
    void (*build_inside)(const CliBuildsRootCfg *args);
} CliBuildsRootNs;
EMBED_TABLE_LAYOUT(CliBuildsRootNs, build_inside);
EMBED_TABLE_STORAGE CliBuildsRootNs cli_builds_root EMBED_UNUSED = {
    .build_inside = cli_build_inside,
};

typedef struct
{
    int (*build_snapshot)(const CliBuildsSizeVersionCfg *args);
} CliBuildsSizeVersionNs;
EMBED_TABLE_LAYOUT(CliBuildsSizeVersionNs, build_snapshot);
EMBED_TABLE_STORAGE CliBuildsSizeVersionNs cli_builds_size_version EMBED_UNUSED = {
    .build_snapshot = cli_build_snapshot,
};

#endif
