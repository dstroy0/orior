// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// patterns.h: the patterns that scope the explorer, the search and the watching of files each from files of the tree,
// a list for each, one pattern to a line (patterns.c).
//
// A pattern names files and folders as a `.gitignore` line does: `*` is for any characters of a name, `**` for any
// folders, `?` for one character, and a trailing `/` names folders only. A pattern with no `/` but a trailing one names a
// file or folder by its name wherever it is; one with a `/` names a path from the tree's top. A folder named takes
// everything in it. A line starting with `#` and an empty line name nothing.
//
// A pattern starting with `!` includes the files it names and hides every other: `!src/` with two stars after it alone
// includes the files under src and no others. Several such patterns include the files any of them names, and a pattern
// with no `!` hides files even among those. On Windows a pattern matches whatever the case
#ifndef PATTERNS_H
#define PATTERNS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliPatternsPathCfg CliPatternsPathCfg;
typedef struct CliPatternsLinesCfg CliPatternsLinesCfg;
typedef struct CliPatternsLinesCountCfg CliPatternsLinesCountCfg;
typedef struct CliPatternsCfg CliPatternsCfg;
typedef struct CliPatternsPartCfg CliPatternsPartCfg;
typedef struct CliPatternsTextCfg CliPatternsTextCfg;

// the max patterns of each type a set contains, and the max characters all of them take
#define CLI_PATTERNS_MAX 256u

#define CLI_PATTERN_CHARACTERS 16384u

// One pattern: as a path from the tree's top, `**/` before it where it names a name anywhere, its characters `length` of
// the set's from `first`; and whether it names folders only
typedef struct
{
    unsigned int first;
    unsigned int length;
    int folders;
} CliPattern;

// The patterns of one part: those that hide what they name, and those that include only what they name; the characters
// they take, each as a pattern compares it
typedef struct
{
    CliPattern hide[CLI_PATTERNS_MAX];
    unsigned int hides;
    CliPattern include[CLI_PATTERNS_MAX];
    unsigned int includes;
    unsigned int characters[CLI_PATTERN_CHARACTERS];
    unsigned int used;
} CliPatterns;

// the parts of the window that patterns scope from files
enum
{
    CLI_PART_EXPLORER,
    CLI_PART_SEARCH,
    CLI_PART_WATCHING,
    CLI_PARTS
};

// the patterns of `count` lines into `patterns`
void cli_patterns_read(const CliPatternsLinesCfg *args);
// the patterns of a set of files, one to a line: each matches files in the set, and one starting with `!` takes the
// files it matches out; and the same read from `size` bytes of `text`, its lines split at line breaks where they are
void cli_patterns_read_set(const CliPatternsLinesCfg *args);
void cli_patterns_read_set_text(const CliPatternsTextCfg *args);
// whether there are none; whether any pattern includes only what it names
int cli_patterns_empty(const CliPatternsCfg *args);
int cli_patterns_include_only(const CliPatternsCfg *args);
// whether the patterns hide a file, or a folder where `dir` is 1: a folder is hidden here only where a pattern names
// it; whether it contains a file included is the caller's to determine
int cli_patterns_hides(const CliPatternsPathCfg *args);
// whether a pattern that includes names the folder, or a folder it is in
int cli_patterns_include_folder(const CliPatternsPathCfg *args);
// the patterns as git's pathspecs put after `words`: what the patterns that include name, or the whole tree where none
// do, less what the others name; what the patterns that include name alone; and what the patterns that hide leave out
void cli_patterns_pathspecs(const CliPatternsCfg *args);
void cli_patterns_include_specs(const CliPatternsCfg *args);
void cli_patterns_hidden_specs(const CliPatternsCfg *args);
// the patterns of a part set from `count` lines: 1 where they changed; and the patterns of a part copied into
// `patterns`, none where they were never set
int cli_patterns_set(const CliPatternsLinesCountCfg *args);
void cli_patterns_get(const CliPatternsPartCfg *args);

struct CliPatternsPathCfg
{
    const CliPatterns *patterns;
    const char *path;
    int dir;
};

struct CliPatternsLinesCfg
{
    const char *const *lines;
    unsigned int count;
    CliPatterns *patterns_out;
};

struct CliPatternsLinesCountCfg
{
    const char *const *lines;
    unsigned int count;
    unsigned int part;
};

struct CliPatternsCfg
{
    const CliPatterns *patterns;
    CliWords *words;
};

struct CliPatternsPartCfg
{
    CliPatterns *patterns_out;
    unsigned int part;
};

struct CliPatternsTextCfg
{
    const char *text;
    unsigned long long size;
    CliPatterns *patterns_out;
};

typedef struct
{
    void (*read)(const CliPatternsLinesCfg *args);
    void (*read_set)(const CliPatternsLinesCfg *args);
} CliPatternsLinesNs;
EMBED_TABLE_LAYOUT(CliPatternsLinesNs, read, read_set);
EMBED_TABLE_STORAGE CliPatternsLinesNs cli_patterns_lines EMBED_UNUSED = {
    .read = cli_patterns_read,
    .read_set = cli_patterns_read_set,
};

typedef struct
{
    void (*read_set_text)(const CliPatternsTextCfg *args);
} CliPatternsTextNs;
EMBED_TABLE_LAYOUT(CliPatternsTextNs, read_set_text);
EMBED_TABLE_STORAGE CliPatternsTextNs cli_patterns_text EMBED_UNUSED = {
    .read_set_text = cli_patterns_read_set_text,
};

typedef struct
{
    int (*empty)(const CliPatternsCfg *args);
    int (*include_only)(const CliPatternsCfg *args);
    void (*pathspecs)(const CliPatternsCfg *args);
    void (*include_specs)(const CliPatternsCfg *args);
    void (*hidden_specs)(const CliPatternsCfg *args);
} CliPatternsNs;
EMBED_TABLE_LAYOUT(CliPatternsNs, empty, include_only, pathspecs, include_specs, hidden_specs);
EMBED_TABLE_STORAGE CliPatternsNs cli_patterns EMBED_UNUSED = {
    .empty = cli_patterns_empty,
    .include_only = cli_patterns_include_only,
    .pathspecs = cli_patterns_pathspecs,
    .include_specs = cli_patterns_include_specs,
    .hidden_specs = cli_patterns_hidden_specs,
};

typedef struct
{
    int (*hides)(const CliPatternsPathCfg *args);
    int (*include_folder)(const CliPatternsPathCfg *args);
} CliPatternsPathNs;
EMBED_TABLE_LAYOUT(CliPatternsPathNs, hides, include_folder);
EMBED_TABLE_STORAGE CliPatternsPathNs cli_patterns_path EMBED_UNUSED = {
    .hides = cli_patterns_hides,
    .include_folder = cli_patterns_include_folder,
};

typedef struct
{
    int (*set)(const CliPatternsLinesCountCfg *args);
} CliPatternsLinesCountNs;
EMBED_TABLE_LAYOUT(CliPatternsLinesCountNs, set);
EMBED_TABLE_STORAGE CliPatternsLinesCountNs cli_patterns_lines_count EMBED_UNUSED = {
    .set = cli_patterns_set,
};

typedef struct
{
    void (*get)(const CliPatternsPartCfg *args);
} CliPatternsPartNs;
EMBED_TABLE_LAYOUT(CliPatternsPartNs, get);
EMBED_TABLE_STORAGE CliPatternsPartNs cli_patterns_part EMBED_UNUSED = {
    .get = cli_patterns_get,
};

#endif
