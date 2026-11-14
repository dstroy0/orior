// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_internal.h: Gradle's version catalogs, `gradle/libs.versions.toml` and the rest, as their reader
// reads them: their sections, each entry a key and its value, a string and the span inside its quotes, a table of
// fields, a list of strings or a bare word; what prevented them from reading; and the aliases they give a script, each
// with what it stands for (version_catalog_read.c, version_catalog_aliases.c, version_catalog_check.c,
// version_catalog_entries.c, version_catalog_complete.c)
#ifndef VERSION_CATALOG_INTERNAL_H
#define VERSION_CATALOG_INTERNAL_H
#include "../text/text.h"
#include "builds.h"
#include "embed_dispatch_layout.h"

typedef struct CliVersionCatalogSizeCtx CliVersionCatalogSizeCtx;
typedef struct CliVersionCatalogTextCtx CliVersionCatalogTextCtx;
typedef struct CliVersionCatalogLinesCtx CliVersionCatalogLinesCtx;
typedef struct CliVersionCatalogCheckCtx CliVersionCatalogCheckCtx;
typedef struct CliVersionCatalogTextSizeCtx CliVersionCatalogTextSizeCtx;
typedef struct CliVersionCatalogTextSizeAtCtx CliVersionCatalogTextSizeAtCtx;
typedef struct CliVersionCatalogSizeOutCtx CliVersionCatalogSizeOutCtx;
typedef struct CliVersionCatalogFieldCtx CliVersionCatalogFieldCtx;
typedef struct CliVersionCatalogCheckTableCtx CliVersionCatalogCheckTableCtx;
typedef struct CliVersionCatalogSizeOutAliasCtx CliVersionCatalogSizeOutAliasCtx;
typedef struct CliVersionCatalogTextSizeAliasesCtx CliVersionCatalogTextSizeAliasesCtx;
typedef struct CliVersionCatalogCheckEntryCtx CliVersionCatalogCheckEntryCtx;
typedef struct CliVersionCatalogFieldOutCtx CliVersionCatalogFieldOutCtx;
typedef struct CliVersionCatalogFirstCtx CliVersionCatalogFirstCtx;
typedef struct CliVersionCatalogNameCtx CliVersionCatalogNameCtx;
typedef struct CliVersionCatalogCtx CliVersionCatalogCtx;

// what a value is: a string, a table of fields, a list of strings, or a bare word
#define CLI_VERSIONS_TEXT 1u
#define CLI_VERSIONS_TABLE 2u
#define CLI_VERSIONS_LIST 3u
#define CLI_VERSIONS_BARE 4u

// the most sections of a catalog, fields, strings of lists and things that prevented it from reading, the bytes their
// texts take, and none; and the most bytes a message of the reader's own takes past the key it names
#define CLI_VERSIONS_SECTIONS 4096u
#define CLI_VERSIONS_FIELDS 65536u
#define CLI_VERSIONS_ITEMS 65536u
#define CLI_VERSIONS_ERRORS 4096u
#define CLI_VERSIONS_POOL (1u << 22u)
#define CLI_VERSIONS_NONE 0xffffffffu
#define CLI_VERSIONS_MSG_MAX_LEN 128u

// A key and its value, the key's span recorded: the key and the value's text in the pool; the span inside a string's
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

// What prevented a catalog from reading: its span and what it reports, in the pool
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned int message_at;
    unsigned int message_size;
} CliVersionsError;

// A catalog as read, each part a cell of the cellblock and how many it contains and has capacity for, with 1 in `full`
// where a part of it did not fit; and the cell a key or a string is read into and a message is made in, `longest` bytes
// the longest line of the text
typedef struct
{
    CliVersionsSection *section;
    unsigned int sections;
    unsigned int sections_capacity;
    CliVersionsField *field;
    unsigned int fields;
    unsigned int fields_capacity;
    CliVersionsItem *item;
    unsigned int items;
    unsigned int items_capacity;
    CliVersionsError *error;
    unsigned int errors;
    unsigned int errors_capacity;
    char *pool;
    unsigned int used;
    unsigned int pool_capacity;
    int full;
    char *line;
    unsigned long long longest;
} CliVersionsParsed;

// the catalog read, under the catalog reader's lock (version_catalog_read.c); and the cells the catalogs' reader,
// check and aliases occupy (version_catalog_cells.c)
extern CliVersionsParsed g_cli_versions;

// `size` bytes of a catalog's text counted, each part of the catalog read taken as a cell of as many as the text can
// make, up to its most: 1, or 0 where the cellblock could not meet one and none is allocated; the catalog read's cells
// and the accessors' given back, called under the catalog reader's lock before it is given (version_catalog_cells.c)
int cli_versions_alloc(const CliVersionCatalogTextCtx *args);
void cli_versions_release(void);

// `size` bytes of a catalog's text read into the catalog read: 1, or 0 where the cellblock could not meet it and
// nothing is read; a text of its pool; the field among the fields from `first`, linked by their next, keyed `key`:
// its place, CLI_VERSIONS_NONE where none is; and what a value describes, in a few words, for a completion's detail,
// into `out` (version_catalog_aliases.c)
int cli_versions_parse(const CliVersionCatalogTextCtx *args);
// the parse's cells, the known names and the libraries' names given back, each table left empty
void cli_versions_free(void);
const char *cli_versions_text(const CliVersionCatalogCtx *args);
unsigned int cli_versions_get(const CliVersionCatalogFirstCtx *args);
void cli_versions_describe(const CliVersionCatalogFieldOutCtx *args);

// the accessor a script names an alias by, `-` and `_` a `.`, into `out`
void cli_versions_accessor(const CliVersionCatalogSizeOutAliasCtx *args);

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

// A catalog's aliases and the texts they take, each a cell of the cellblock, and the bytes the texts have capacity for
typedef struct
{
    CliVersionsAlias *alias;
    unsigned int count;
    char *text;
    unsigned int used;
    unsigned int capacity;
} CliVersionsAliases;

// the aliases of `size` bytes of a catalog's text into `aliases`, by section: 1, or 0 where the cellblock could not
// meet the catalog or its aliases and none is allocated; and their cells given back (version_catalog_aliases.c)
int cli_versions_aliases(const CliVersionCatalogTextSizeAliasesCtx *args);

// each name the catalog `name`, at `displayed` in the tree, leads in the Gradle script read checked against its
// aliases, each it does not name a finding into `found` over `lines` (gradle_references.c)
void cli_gradle_references(const CliVersionCatalogLinesCtx *args);

// the most accessors of one section stored, and the bytes they take
#define CLI_VERSIONS_NAMES 16384u
#define CLI_VERSIONS_NAMES_TEXT (1u << 20u)

// The accessors of a section: each where it starts in the text and its size, one cell of the cellblock with capacity
// for `most` of each, the sizes after the starts; and the text, a cell with capacity for `capacity` bytes
typedef struct
{
    unsigned int *at;
    unsigned int *size;
    unsigned int count;
    unsigned int most;
    char *text;
    unsigned int used;
    unsigned int capacity;
} CliVersionsNames;

// What a check works with: the text's lines, the findings, the build's folder in the tree and the program that runs
// it; and a cell a dependency's coordinates are written into, `capacity` bytes, and with them its version, twice that
typedef struct
{
    const CliBuildLines *lines;
    CliBuildFindings *found;
    char folder[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    char *coordinates;
    char *full;
    unsigned long long capacity;
} CliVersionsCheck;

// the accessors of the catalog's versions and libraries, under the catalog reader's lock (version_catalog_check.c)
extern CliVersionsNames g_cli_versions_known;
extern CliVersionsNames g_cli_versions_libraries;

// the accessors of the section `name`'s entries into `names`: 1, or 0 where the cellblock could not meet them; the
// accessor of `size` bytes of `name` into `out`, and
// whether `names` contains it; a finding that `size` bytes of `text` over `from` to `to` is a snapshot, with the fix
// that refreshes the build's dependencies; an error over an entry: its key between backticks, then `rest`; the fixes
// that change the span `from` to `to` to each name of `known`, `count` of them, or of `names`, near `name`, between
// `open` and `close` in their titles; the keys of a table no table of its type knows, its keys `known`, each a finding;
// a version checked; and the version a library's or a plugin's table names, by `version` or `version.ref`, of the thing
// at `coordinates` (version_catalog_check.c)
int cli_versions_names_get(const CliVersionCatalogNameCtx *args);
int cli_versions_contains(const CliVersionCatalogSizeOutCtx *args);
void cli_versions_snapshot(const CliVersionCatalogTextSizeCtx *args);
void cli_versions_entry_error(const CliVersionCatalogFieldCtx *args);
void cli_versions_adjacent(const CliVersionCatalogSizeCtx *args);
void cli_versions_keys(const CliVersionCatalogCheckCtx *args);
void cli_versions_check_version(const CliVersionCatalogCheckEntryCtx *args);
void cli_versions_version(const CliVersionCatalogCheckTableCtx *args);

// the findings of the catalog at `path` of the tree at `root`, `size` bytes of its text (version_catalog_entries.c)
void cli_versions_check(const CliVersionCatalogTextCtx *args);
// what completes the word before byte `at` of a catalog: a section's name after `[`, a table's keys, the versions'
// aliases in a `version.ref`, and the libraries' in a bundle (version_catalog_complete.c)
void cli_versions_complete(const CliVersionCatalogTextSizeAtCtx *args);

struct CliVersionCatalogSizeCtx
{
    unsigned long long size;
    const char *name;
    const CliVersionsNames *names;
    CliVersionsCheck *check;
    unsigned long long from;
    unsigned long long to;
    unsigned int finding;
    const char *const *known;
    unsigned int count;
    const char *open;
    const char *close;
};

struct CliVersionCatalogTextCtx
{
    const char *text;
    unsigned long long size;
    const CliBuildLines *lines;
    CliBuildFindings *found;
    const char *root;
    const char *path;
};

struct CliVersionCatalogLinesCtx
{
    const CliBuildLines *lines;
    CliBuildFindings *found;
    const char *name;
    const char *displayed;
    const CliVersionsAliases *aliases_read;
};

struct CliVersionCatalogCheckCtx
{
    CliVersionsCheck *check;
    const char *const *known;
    unsigned int count;
    const CliVersionsField *table;
    const char *of;
};

struct CliVersionCatalogTextSizeCtx
{
    const char *text;
    unsigned long long size;
    CliVersionsCheck *check;
    unsigned long long from;
    unsigned long long to;
};

struct CliVersionCatalogTextSizeAtCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long at_u64;
    CliCompletions *items;
};

struct CliVersionCatalogSizeOutCtx
{
    unsigned long long size;
    VerbaLinea *out;
    const char *name;
    const CliVersionsNames *names;
};

struct CliVersionCatalogFieldCtx
{
    const CliVersionsField *field;
    CliVersionsCheck *check;
    const char *code;
    const char *rest;
};

struct CliVersionCatalogCheckTableCtx
{
    CliVersionsCheck *check;
    const CliVersionsField *table;
    const char *coordinates;
    unsigned long long coordinates_size;
};

struct CliVersionCatalogSizeOutAliasCtx
{
    unsigned long long size;
    VerbaLinea *out;
    const char *alias;
};

struct CliVersionCatalogTextSizeAliasesCtx
{
    const char *text;
    unsigned long long size;
    CliVersionsAliases *aliases;
};

struct CliVersionCatalogCheckEntryCtx
{
    CliVersionsCheck *check;
    const CliVersionsField *entry;
};

struct CliVersionCatalogFieldOutCtx
{
    const CliVersionsField *field;
    VerbaLinea *out;
};

struct CliVersionCatalogFirstCtx
{
    unsigned int first;
    const char *key;
};

struct CliVersionCatalogNameCtx
{
    const char *name;
    CliVersionsNames *names_out;
};

struct CliVersionCatalogCtx
{
    unsigned int at;
};

typedef struct
{
    int (*versions_alloc)(const CliVersionCatalogTextCtx *args);
    int (*versions_parse)(const CliVersionCatalogTextCtx *args);
    void (*versions_check)(const CliVersionCatalogTextCtx *args);
} CliVersionCatalogTextNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogTextNs, versions_alloc, versions_parse, versions_check);
EMBED_TABLE_STORAGE CliVersionCatalogTextNs cli_version_catalog_text_internal EMBED_UNUSED = {
    .versions_alloc = cli_versions_alloc,
    .versions_parse = cli_versions_parse,
    .versions_check = cli_versions_check,
};

typedef struct
{
    const char *(*versions_text)(const CliVersionCatalogCtx *args);
} CliVersionCatalogNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogNs, versions_text);
EMBED_TABLE_STORAGE CliVersionCatalogNs cli_version_catalog_internal EMBED_UNUSED = {
    .versions_text = cli_versions_text,
};

typedef struct
{
    unsigned int (*versions_get)(const CliVersionCatalogFirstCtx *args);
} CliVersionCatalogFirstNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogFirstNs, versions_get);
EMBED_TABLE_STORAGE CliVersionCatalogFirstNs cli_version_catalog_first_internal EMBED_UNUSED = {
    .versions_get = cli_versions_get,
};

typedef struct
{
    void (*versions_describe)(const CliVersionCatalogFieldOutCtx *args);
} CliVersionCatalogFieldOutNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogFieldOutNs, versions_describe);
EMBED_TABLE_STORAGE CliVersionCatalogFieldOutNs cli_version_catalog_field_out_internal EMBED_UNUSED = {
    .versions_describe = cli_versions_describe,
};

typedef struct
{
    void (*versions_accessor)(const CliVersionCatalogSizeOutAliasCtx *args);
} CliVersionCatalogSizeOutAliasNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogSizeOutAliasNs, versions_accessor);
EMBED_TABLE_STORAGE CliVersionCatalogSizeOutAliasNs cli_version_catalog_size_out_alias_internal EMBED_UNUSED = {
    .versions_accessor = cli_versions_accessor,
};

typedef struct
{
    int (*versions_aliases)(const CliVersionCatalogTextSizeAliasesCtx *args);
} CliVersionCatalogTextSizeAliasesNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogTextSizeAliasesNs, versions_aliases);
EMBED_TABLE_STORAGE CliVersionCatalogTextSizeAliasesNs cli_version_catalog_text_size_aliases_internal EMBED_UNUSED = {
    .versions_aliases = cli_versions_aliases,
};

typedef struct
{
    void (*gradle_references)(const CliVersionCatalogLinesCtx *args);
} CliVersionCatalogLinesNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogLinesNs, gradle_references);
EMBED_TABLE_STORAGE CliVersionCatalogLinesNs cli_version_catalog_lines_internal EMBED_UNUSED = {
    .gradle_references = cli_gradle_references,
};

typedef struct
{
    int (*versions_names_get)(const CliVersionCatalogNameCtx *args);
} CliVersionCatalogNameNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogNameNs, versions_names_get);
EMBED_TABLE_STORAGE CliVersionCatalogNameNs cli_version_catalog_name_internal EMBED_UNUSED = {
    .versions_names_get = cli_versions_names_get,
};

typedef struct
{
    int (*versions_contains)(const CliVersionCatalogSizeOutCtx *args);
} CliVersionCatalogSizeOutNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogSizeOutNs, versions_contains);
EMBED_TABLE_STORAGE CliVersionCatalogSizeOutNs cli_version_catalog_size_out_internal EMBED_UNUSED = {
    .versions_contains = cli_versions_contains,
};

typedef struct
{
    void (*versions_snapshot)(const CliVersionCatalogTextSizeCtx *args);
} CliVersionCatalogTextSizeNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogTextSizeNs, versions_snapshot);
EMBED_TABLE_STORAGE CliVersionCatalogTextSizeNs cli_version_catalog_text_size_internal EMBED_UNUSED = {
    .versions_snapshot = cli_versions_snapshot,
};

typedef struct
{
    void (*versions_entry_error)(const CliVersionCatalogFieldCtx *args);
} CliVersionCatalogFieldNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogFieldNs, versions_entry_error);
EMBED_TABLE_STORAGE CliVersionCatalogFieldNs cli_version_catalog_field_internal EMBED_UNUSED = {
    .versions_entry_error = cli_versions_entry_error,
};

typedef struct
{
    void (*versions_adjacent)(const CliVersionCatalogSizeCtx *args);
} CliVersionCatalogSizeNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogSizeNs, versions_adjacent);
EMBED_TABLE_STORAGE CliVersionCatalogSizeNs cli_version_catalog_size_internal EMBED_UNUSED = {
    .versions_adjacent = cli_versions_adjacent,
};

typedef struct
{
    void (*versions_keys)(const CliVersionCatalogCheckCtx *args);
} CliVersionCatalogCheckNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogCheckNs, versions_keys);
EMBED_TABLE_STORAGE CliVersionCatalogCheckNs cli_version_catalog_check_internal EMBED_UNUSED = {
    .versions_keys = cli_versions_keys,
};

typedef struct
{
    void (*versions_check_version)(const CliVersionCatalogCheckEntryCtx *args);
} CliVersionCatalogCheckEntryNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogCheckEntryNs, versions_check_version);
EMBED_TABLE_STORAGE CliVersionCatalogCheckEntryNs cli_version_catalog_check_entry_internal EMBED_UNUSED = {
    .versions_check_version = cli_versions_check_version,
};

typedef struct
{
    void (*versions_version)(const CliVersionCatalogCheckTableCtx *args);
} CliVersionCatalogCheckTableNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogCheckTableNs, versions_version);
EMBED_TABLE_STORAGE CliVersionCatalogCheckTableNs cli_version_catalog_check_table_internal EMBED_UNUSED = {
    .versions_version = cli_versions_version,
};

typedef struct
{
    void (*versions_complete)(const CliVersionCatalogTextSizeAtCtx *args);
} CliVersionCatalogTextSizeAtNs;
EMBED_TABLE_LAYOUT(CliVersionCatalogTextSizeAtNs, versions_complete);
EMBED_TABLE_STORAGE CliVersionCatalogTextSizeAtNs cli_version_catalog_text_size_at_internal EMBED_UNUSED = {
    .versions_complete = cli_versions_complete,
};

#endif
