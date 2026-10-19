// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// patterns.h: the patterns that keep the explorer, the search and the watching of files each from files of the tree,
// a list for each, one pattern to a line (patterns.c).
//
// A pattern names files and folders as a `.gitignore` line does: `*` stands for any letters of a name, `**` for any
// folders, `?` for one letter, and a trailing `/` names folders only. A pattern with no `/` but a trailing one names a
// file or folder by its name wherever it is; one with a `/` names a path from the tree's top. A folder named takes
// everything in it. A line starting with `#` and an empty line name nothing.
//
// A pattern starting with `!` keeps the files it names and hides every other: `!src/**` alone keeps the files under
// src and no others. Several such patterns keep the files any of them names, and a pattern with no `!` hides files
// even among those. On Windows a pattern matches whatever the case
#ifndef PATTERNS_H
#define PATTERNS_H

#include "../command_line/cli_internal.h"

// the most patterns of each kind a set holds, and the most letters all of them take
#define CLI_PATTERNS_MOST 256u

#define CLI_PATTERN_LETTERS 16384u

// One pattern: as a path from the tree's top, `**/` before it where it names a name anywhere, its letters `length` of
// the set's from `first`; and whether it names folders only
typedef struct
{
    unsigned int first;
    unsigned int length;
    int folders;
} CliPattern;

// The patterns of one part: those that hide what they name, and those that keep only what they name; the letters they
// take, each as a pattern compares it
typedef struct
{
    CliPattern hide[CLI_PATTERNS_MOST];
    unsigned int hides;
    CliPattern keep[CLI_PATTERNS_MOST];
    unsigned int keeps;
    unsigned int letters[CLI_PATTERN_LETTERS];
    unsigned int used;
} CliPatterns;

// the parts of the window that patterns keep from files
enum
{
    CLI_PART_EXPLORER,
    CLI_PART_SEARCH,
    CLI_PART_WATCHING,
    CLI_PARTS
};

// the patterns of `count` lines into `patterns`
void cli_patterns_read(const char *const *lines, unsigned int count, CliPatterns *patterns);
// the patterns of a set of files, one to a line: each names files in the set, and one starting with `!` takes the
// files it names out
void cli_patterns_read_set(const char *const *lines, unsigned int count, CliPatterns *patterns);
// whether there are none; whether any pattern keeps only what it names
int cli_patterns_empty(const CliPatterns *patterns);
int cli_patterns_keeps_only(const CliPatterns *patterns);
// whether the patterns hide a file, or a folder where `dir` is 1: a folder is hidden here only where a pattern names
// it; whether it holds a file kept is the caller's to say
int cli_patterns_hides(const CliPatterns *patterns, const char *path, int dir);
// whether a pattern that keeps names the folder, or a folder it is in
int cli_patterns_keeps_folder(const CliPatterns *patterns, const char *path);
// the patterns as git's pathspecs put after `words`: what the patterns that keep name, or the whole tree where none
// do, less what the others name; what the patterns that keep name alone; and what the patterns that hide leave out
void cli_patterns_pathspecs(const CliPatterns *patterns, CliWords *words);
void cli_patterns_kept_specs(const CliPatterns *patterns, CliWords *words);
void cli_patterns_hidden_specs(const CliPatterns *patterns, CliWords *words);
// the patterns of a part set from `count` lines: 1 where they changed; and the patterns of a part copied into
// `patterns`, none where they were never set
int cli_patterns_set(unsigned int part, const char *const *lines, unsigned int count);
void cli_patterns_of(unsigned int part, CliPatterns *patterns);

#endif
