// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_internal.h: Gradle's version catalogs, `gradle/libs.versions.toml` and the rest, as their reader
// reads them: their sections, each entry a key and its value, a string and the span inside its quotes, a table of
// fields, a list of strings or a bare word; what kept them from reading; and the aliases they give a script, each with
// what it stands for (version_catalog_read.c, version_catalog_aliases.c, version_catalog_check.c,
// version_catalog_entries.c, version_catalog_complete.c)
#ifndef VERSION_CATALOG_INTERNAL_H
#define VERSION_CATALOG_INTERNAL_H

#include "../text/text.h"
#include "builds.h"

// what a value is: a string, a table of fields, a list of strings, or a bare word
#define CLI_VERSIONS_TEXT 1u
#define CLI_VERSIONS_TABLE 2u
#define CLI_VERSIONS_LIST 3u
#define CLI_VERSIONS_BARE 4u

// the most sections of a catalog, fields, strings of lists and things that kept it from reading, the bytes their
// texts take, and none; and the most bytes a message of the reader's own takes past the key it names
#define CLI_VERSIONS_SECTIONS 4096u
#define CLI_VERSIONS_FIELDS 65536u
#define CLI_VERSIONS_ITEMS 65536u
#define CLI_VERSIONS_ERRORS 4096u
#define CLI_VERSIONS_POOL (1u << 22u)
#define CLI_VERSIONS_NONE 0xffffffffu
#define CLI_VERSIONS_SAID 128u

// A key and its value, the key's span kept: the key and the value's text in the pool; the span inside a string's
// quotes; a table's first field or a list's first string, and how many; and the next field of its table or section
typedef struct
{
    unsigned int key_at;
    unsigned int key_size;
    unsigned long long from;
    unsigned long long to;
    unsigned int shape;
    unsigned int text_at;
    unsigned int text_size;
    unsigned long long value_from;
    unsigned long long value_to;
    unsigned int first;
    unsigned int count;
    unsigned int next;
} CliVersionsField;

// A string of a list: its text in the pool and the span inside its quotes
typedef struct
{
    unsigned int text_at;
    unsigned int text_size;
    unsigned long long from;
    unsigned long long to;
} CliVersionsItem;

// A section: its name in the pool and its span, and its entries, the first, the last and how many
typedef struct
{
    unsigned int name_at;
    unsigned int name_size;
    unsigned long long from;
    unsigned long long to;
    unsigned int first;
    unsigned int last;
    unsigned int count;
} CliVersionsSection;

// What kept a catalog from reading: its span and what it says, in the pool
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned int message_at;
    unsigned int message_size;
} CliVersionsError;

// A catalog as read, each part a cell of the cellblock and how many it holds and has room for, with 1 in `full` where
// a part of it did not fit; and the cell a key or a string is read into and a message is made in, `longest` bytes the
// longest line of the text
typedef struct
{
    CliVersionsSection *section;
    unsigned int sections;
    unsigned int sections_room;
    CliVersionsField *field;
    unsigned int fields;
    unsigned int fields_room;
    CliVersionsItem *item;
    unsigned int items;
    unsigned int items_room;
    CliVersionsError *error;
    unsigned int errors;
    unsigned int errors_room;
    char *pool;
    unsigned int used;
    unsigned int pool_room;
    int full;
    char *line;
    unsigned long long longest;
} CliVersionsParsed;

// the catalog read, under the catalog reader's lock (version_catalog_read.c); and the cells the catalogs' reader,
// check and aliases hold (version_catalog_cells.c)
extern CliVersionsParsed g_cli_versions;
extern CliLock g_cli_versions_lock;
extern CliCellsTally g_cli_versions_cells;

// `size` bytes of a catalog's text counted, each part of the catalog read taken as a cell of as many as the text can
// make, up to its most: 1, or 0 where the cellblock could not meet one and none is held; the catalog read's cells and
// the accessors' given back, called under the catalog reader's lock before it is given (version_catalog_cells.c)
int cli_versions_taken(const char *text, unsigned long long size);
void cli_versions_released(void);

// `size` bytes of a catalog's text read into the catalog read: 1, or 0 where the cellblock could not meet it and
// nothing is read; a text of its pool; the field among the fields from `first`, linked by their next, keyed `key`:
// its place, CLI_VERSIONS_NONE where none is; and what a value says, in a few words, for a completion's detail, into
// `out` (version_catalog_aliases.c)
int cli_versions_parse(const char *text, unsigned long long size);
const char *cli_versions_text(unsigned int at);
unsigned int cli_versions_get(unsigned int first, const char *key);
void cli_versions_describe(const CliVersionsField *field, CliLine *out);

// the accessor a script names an alias by, `-` and `_` a `.`, into `out`
void cli_versions_accessor(const char *alias, unsigned long long size, CliLine *out);

// the most aliases of a catalog, and the bytes their texts take
#define CLI_VERSIONS_ALIASES 16384u
#define CLI_VERSIONS_ALIASES_TEXT (1u << 21u)

// the sections of a catalog's aliases
#define CLI_VERSIONS_LIBRARIES 0u
#define CLI_VERSIONS_VERSIONS 1u
#define CLI_VERSIONS_PLUGINS 2u
#define CLI_VERSIONS_BUNDLES 3u

// An alias of a catalog: its section, the alias, and what it stands for, each a text of its aliases'
typedef struct
{
    unsigned int section;
    unsigned int alias_at;
    unsigned int alias_size;
    unsigned int about_at;
    unsigned int about_size;
} CliVersionsAlias;

// A catalog's aliases and the texts they take, each a cell of the cellblock, and the bytes the texts have room for
typedef struct
{
    CliVersionsAlias *alias;
    unsigned int count;
    char *text;
    unsigned int used;
    unsigned int room;
} CliVersionsAliases;

// the aliases of `size` bytes of a catalog's text into `aliases`, by section: 1, or 0 where the cellblock could not
// meet the catalog or its aliases and none is held; and their cells given back (version_catalog_aliases.c)
int cli_versions_aliases(const char *text, unsigned long long size, CliVersionsAliases *aliases);
void cli_versions_aliases_given(CliVersionsAliases *aliases);

// each name the catalog `name`, at `shown` in the tree, leads in the Gradle script read checked against its aliases,
// each it does not name a finding into `found` over `lines` (gradle_references.c)
void cli_gradle_references(const CliBuildLines *lines, CliBuildFindings *found, const char *name, const char *shown,
                           const CliVersionsAliases *aliases);

// the most accessors of one section kept, and the bytes they take
#define CLI_VERSIONS_NAMES 16384u
#define CLI_VERSIONS_NAMES_TEXT (1u << 20u)

// The accessors of a section: each where it starts in the text and its size, one cell of the cellblock with room for
// `most` of each, the sizes after the starts; and the text, a cell with room for `room` bytes
typedef struct
{
    unsigned int *at;
    unsigned int *size;
    unsigned int count;
    unsigned int most;
    char *text;
    unsigned int used;
    unsigned int room;
} CliVersionsNames;

// What a check works with: the text's lines, the findings, the build's folder in the tree and the program that runs
// it; and a cell a dependency's coordinates are written into, `room` bytes, and with them its version, twice that
typedef struct
{
    const CliBuildLines *lines;
    CliBuildFindings *found;
    char folder[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
    char *coordinates;
    char *full;
    unsigned long long room;
} CliVersionsCheck;

// the accessors of the catalog's versions and libraries, under the catalog reader's lock (version_catalog_check.c)
extern CliVersionsNames g_cli_versions_known;
extern CliVersionsNames g_cli_versions_libraries;

// the accessors of the section `name`'s entries into `names`: 1, or 0 where the cellblock could not meet them; the
// accessor of `size` bytes of `name` into `out`, and
// whether `names` holds it; a finding that `size` bytes of `text` over `from` to `to` is a snapshot, with the fix that
// refreshes the build's dependencies; an error over an entry: its key between backticks, then `rest`; the fixes that
// change the span `from` to `to` to each name of `known`, `count` of them, or of `names`, near `name`, between `open`
// and `close` in their titles; the keys of a table no table of its kind knows, its keys `known`, each a finding; a
// version checked; and the version a library's or a plugin's table names, by `version` or `version.ref`, of the thing
// at `coordinates` (version_catalog_check.c)
int cli_versions_names_of(const char *name, CliVersionsNames *names);
int cli_versions_holds(const CliVersionsNames *names, const char *name, unsigned long long size, CliLine *out);
void cli_versions_snapshot(CliVersionsCheck *check, unsigned long long from, unsigned long long to, const char *text,
                           unsigned long long size);
void cli_versions_entry_error(CliVersionsCheck *check, const CliVersionsField *field, const char *code,
                              const char *rest);
void cli_versions_near(CliVersionsCheck *check, unsigned int finding, const char *name, unsigned long long size,
                       const char *const *known, unsigned int count, const CliVersionsNames *names,
                       unsigned long long from, unsigned long long to, const char *open, const char *close);
void cli_versions_keys(CliVersionsCheck *check, const CliVersionsField *table, const char *const *known,
                       unsigned int count, const char *of);
void cli_versions_check_version(CliVersionsCheck *check, const CliVersionsField *entry);
void cli_versions_versioned(CliVersionsCheck *check, const CliVersionsField *table, const char *coordinates,
                            unsigned long long coordinates_size);

// the findings of the catalog at `path` of the tree at `root`, `size` bytes of its text (version_catalog_entries.c)
void cli_versions_check(const char *root, const char *path, const char *text, unsigned long long size,
                        const CliBuildLines *lines, CliBuildFindings *found);
// what completes the word before byte `at` of a catalog: a section's name after `[`, a table's keys, the versions'
// aliases in a `version.ref`, and the libraries' in a bundle (version_catalog_complete.c)
void cli_versions_complete(const char *text, unsigned long long size, unsigned long long at, CliCompletions *items);

#endif
