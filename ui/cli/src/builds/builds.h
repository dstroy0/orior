// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds.h: build files: Gradle's build and settings scripts, in Groovy or Kotlin, and its version catalogs; Maven's
// `pom.xml`; and SCons's SConstruct and SConscript. Each is checked and completed as it is written, by its own text
// and what the tree around it holds: a catalog's aliases where a script names them, a POM's properties and its
// parent's, the SConscripts a script reads and the names they export.
//
// A dependency whose version is a snapshot is noted, and its note offers the command that fetches the snapshot's
// newest build, run in the terminal; the snapshots line gives the one for a whole build. Maven's reader also reads a
// POM's compiler settings and Maven's own, from `settings.xml`, and writes them again as they are set.
//
// What the readers share, and the schemas' reader with them: a text's lines, to turn a byte into a place; the findings
// of a file, each with the fixes it offers, an edit of the file or a line to run in the terminal; the completion
// items of what was typed; and the names nearest one that is not known (builds.c, builds_help.c, builds_near.c,
// version_catalog*.c, gradle*.c, maven*.c, scons*.c)
#ifndef BUILDS_H
#define BUILDS_H

#include "../text/edits.h"

// the kinds of build file read here, 0 for none
#define CLI_BUILD_GRADLE 1u
#define CLI_BUILD_CATALOG 2u
#define CLI_BUILD_MAVEN 3u
#define CLI_BUILD_SCONS 4u

// how bad a finding is, as the protocol numbers it
#define CLI_BUILD_ERROR 1u
#define CLI_BUILD_WARNING 2u
#define CLI_BUILD_NOTE 3u

// the most findings of one file, the most fixes of them, the bytes their texts take, and no finding
#define CLI_BUILD_FINDINGS 4096u
#define CLI_BUILD_FIXES 16384u
#define CLI_BUILD_TEXT (1u << 21u)
#define CLI_BUILD_NONE 0xffffffffu

// the kind of build file at `path`, by its name, 0 where it is none
unsigned int cli_build_kind_of(const char *path);

// the diagnostics of the build file at `path` of the tree at `root`, `size` bytes of its text, written as the
// protocol's array of them into `out`
void cli_build_check(const char *root, const char *path, const char *text, unsigned long long size, CliLine *out);
// what completes the word before `line`, `col` of the build file at `path`, `size` bytes of its text, into `items`,
// each label once
void cli_build_complete(const char *root, const char *path, const char *text, unsigned long long size,
                        unsigned int line, unsigned int col, CliCompletions *items);
// the names SCons gives the SConstruct or SConscript at `path` without its importing them: its globals and the names
// its `Import` calls take, each handed to `each`; none for any other file
void cli_build_given_names(const char *path, const char *text, unsigned long long size,
                           void (*each)(void *context, const char *name, unsigned long long size), void *context);
// the folder, in the tree, and the command line that update every snapshot dependency of the build `path`, NULL for
// none, belongs to: Maven's where it is a POM or a POM stands beside it, Gradle's where it is a Gradle file or one
// stands beside it, and either one at the tree's top where `path` is neither; each into what holds CLI_PATH_LONGEST:
// 1, or 0 with why said
int cli_build_snapshots_line(const char *root, const char *path, char *folder, char *line, CliLine *said);

// Where each line of a text starts, to turn a byte of it into a place: the starts of its first `count` lines, those
// past them found by reading on
typedef struct
{
    const char *text;
    unsigned long long size;
    const unsigned int *start;
    unsigned int count;
} CliBuildLines;

// the lines of `size` bytes of `text`, their starts kept in `starts`, which holds `most`; the line and the column, in
// UTF-16 units, of byte `at`; and the byte line `line` starts at, the text's end past its last (builds_help.c)
void cli_build_lines_of(const char *text, unsigned long long size, unsigned int *starts, unsigned int most,
                        CliBuildLines *lines);
CliTextPlace cli_build_place(const CliBuildLines *lines, unsigned long long at);
unsigned long long cli_build_line_start(const CliBuildLines *lines, unsigned int line);

// A finding in a file: its span, how bad it is, its code, and its message in Markdown, a text of its findings'
typedef struct
{
    CliTextPlace from;
    CliTextPlace to;
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
    CliTextPlace from;
    CliTextPlace to;
    int also;
    CliTextPlace also_from;
    CliTextPlace also_to;
    unsigned int text_at;
    unsigned int text_size;
    unsigned int folder_at;
    unsigned int folder_size;
} CliBuildFix;

// A file's findings and their fixes, and the texts they take, with 1 in `full` where one did not fit
typedef struct
{
    CliBuildFinding finding[CLI_BUILD_FINDINGS];
    unsigned int count;
    CliBuildFix fix[CLI_BUILD_FIXES];
    unsigned int fixes;
    char text[CLI_BUILD_TEXT];
    unsigned int used;
    int full;
} CliBuildFindings;

// findings made empty; a line over the room left in their texts, a message or a title written into it there kept by
// the finding or the fix made of it next; a finding of `message` over the bytes `from` to `to`: its place, or
// CLI_BUILD_NONE where it does not fit; a fix of the finding at `finding` that writes `size` bytes of `text` over the
// bytes `from` to `to`; and one that runs `line` in the terminal in `folder` of the tree (builds_help.c)
void cli_build_findings_begun(CliBuildFindings *found);
CliLine cli_build_said(CliBuildFindings *found);
unsigned int cli_build_finding(CliBuildFindings *found, const CliLine *message, const CliBuildLines *lines,
                               unsigned long long from, unsigned long long to, unsigned int severity, const char *code);
void cli_build_edit_fix(CliBuildFindings *found, unsigned int finding, const CliLine *title, const CliBuildLines *lines,
                        unsigned long long from, unsigned long long to, const char *text, unsigned long long size);
void cli_build_run_fix(CliBuildFindings *found, unsigned int finding, const CliLine *title, const char *folder,
                       unsigned long long folder_size, const char *line, unsigned long long line_size);
// a fix of the finding at `finding` that writes `size` bytes of `text` over the bytes `from` to `to` and over
// `also_from` to `also_to` (builds_help.c)
void cli_build_edit_fix_twice(CliBuildFindings *found, unsigned int finding, const CliLine *title,
                              const CliBuildLines *lines, unsigned long long from, unsigned long long to,
                              unsigned long long also_from, unsigned long long also_to, const char *text,
                              unsigned long long size);
// the findings written as the protocol's array of diagnostics, each `{"range", "severity", "message": {"kind",
// "value"}, "source": "orior", "code", "data": {"orior": fixes}}`
void cli_build_findings_written(const CliBuildFindings *found, CliLine *out);

// the byte of `size` bytes of `text` at `line`, `col`, the column in UTF-16 units; whether a letter is of a word, as
// the editor reads one; and the start of the run of letters `of` takes that ends at byte `at`, on its line
unsigned long long cli_build_byte_at(const char *text, unsigned long long size, unsigned int line, unsigned int col);
int cli_build_word_letter(unsigned int letter);
unsigned long long cli_build_run_before(const char *text, unsigned long long at, int (*of)(unsigned int letter));

// the item of a value put where it stands after what was typed before the word the cursor is in: of
// `maven.compiler.so` typed, `source` for `maven.compiler.source`; a value that does not start with what was typed is
// left out; and plain items for `words`, `count` of them, each of `kind` with `detail`, `()` left off what each shows
void cli_build_relative(CliCompletions *items, const char *token, unsigned long long token_size, const char *value,
                        unsigned long long value_size, const char *kind, const char *detail,
                        unsigned long long detail_size);
void cli_build_items(CliCompletions *items, const char *const *words, unsigned int count, const char *kind,
                     const char *detail);

// The names nearest one that is not known, near enough to be what was meant, the nearest first, at most three, each a
// text of the caller's
typedef struct
{
    const char *name;
    unsigned long long name_size;
    const char *near[3];
    unsigned long long near_size[3];
    unsigned long long far[3];
    unsigned int count;
} CliBuildNear;

// the fewest edits of one letter that turn `a` into `b`; the nearest names begun for `name`; and a known name offered
// to them
unsigned long long cli_build_distance(const char *a, unsigned long long a_size, const char *b,
                                      unsigned long long b_size);
void cli_build_near_begun(CliBuildNear *near, const char *name, unsigned long long size);
void cli_build_near_offered(CliBuildNear *near, const char *one, unsigned long long size);

// `folder` as a path in the tree at `root`, with forward slashes, empty for the top, into `out`, which holds
// CLI_PATH_LONGEST; and whether a version is a snapshot's
void cli_build_inside(const char *root, const char *folder, char *out);
int cli_build_snapshot(const char *version, unsigned long long size);

#endif
