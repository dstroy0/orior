// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_tests_internal.h: what the cargo test sources share: one file's text read as the search for tests reads it,
// and the capacity the search stores what files contain in
#ifndef CARGO_TESTS_INTERNAL_H
#define CARGO_TESTS_INTERNAL_H
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "cargo_tests.h"
#include "embed_dispatch_layout.h"

typedef struct CliCargoTestsACtx CliCargoTestsACtx;
typedef struct CliCargoTestsTextCtx CliCargoTestsTextCtx;
typedef struct CliCargoTestsTextSizeCtx CliCargoTestsTextSizeCtx;
typedef struct CliCargoTestsCtx CliCargoTestsCtx;

// the cells the search for Rust's tests occupies; and a cell of `most` of `width` bytes, or of twice `*capacity` where
// that is more, containing the `*capacity` of `cell`, `cell` given back and `*capacity` raised: the cell, or NULL where
// it is refused and `cell` is retained (cargo_scan.c)
extern CellOwner g_cli_cargo_cells;

// what a token of Rust's text is: a word, `::`, or a sign
#define CLI_RUST_WORD 0u
#define CLI_RUST_PATH 1u
#define CLI_RUST_MARK 2u

// A token: its kind, its letter where it is a sign, where its text starts in the file and its bytes where it is a word,
// and the line it stands on, counted from 0
typedef struct
{
    unsigned int kind;
    unsigned int letter;
    unsigned long long start;
    unsigned int size;
    unsigned int line;
} CliRustToken;

// What one file contains for the search: a test, or a module the file names with `mod name;`: where in the capacity's
// names the modules written in the file it stands in start, joined by `::`, and its name, and the line a test's name
// stands on
typedef struct
{
    int test;
    unsigned long long inline_names;
    unsigned long long name;
    unsigned int line;
} CliRustItem;

// What the search reads into: one file's tokens, in a cell of their count; what the files followed contain, stored in
// turn as each file is followed and let go once it and the files it names are; and the bytes their names take, each in
// a cell at their most
typedef struct
{
    CliRustToken *tokens;
    unsigned int token_count;
    unsigned long long token_capacity;
    CliRustItem *items;
    unsigned int item_count;
    unsigned long long item_capacity;
    char *names;
    unsigned long long name_used;
    unsigned long long name_capacity;
} CliCargoScan;

extern CliCargoScan g_cli_cargo_scan;

// A target and its first file, as a path from the package's folder
typedef struct
{
    unsigned int kind;
    char name[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
} CliCargoFirst;

// the next line of `size` bytes from `at`, trimmed, as its start and length: 1, or 0 where none is left; the name of
// the package a Cargo.toml describes into `name`, sized for CLI_NAME_MAX_LEN: 1, or 0 where it describes none; `a`
// and `b` joined by `between`, either of them left out where it is empty, into `out`, sized for `capacity`; and the
// targets of the package a Cargo.toml in `folder` describes, `package` its name, each with its first file: the
// library, the binaries and the tests, as Cargo finds them, their count into `count` (cargo_targets.c)
int cli_cargo_line(const CliCargoTestsTextCtx *args);
int cli_cargo_package_name(const CliCargoTestsCtx *args);
void cli_cargo_join(const CliCargoTestsACtx *args);
const CliCargoFirst *cli_cargo_targets(const CliCargoTestsTextSizeCtx *args);
// the targets cli_cargo_targets found given back, which their next finding does as well (cargo_targets.c)
void cli_cargo_firsts_free(void);

// the tokens of `size` bytes of Rust, each with the line it stands on: comments, strings and characters passed over,
// and a lifetime no token, counted and then put into a cell of their count, the cell of the file before given back;
// what they contain put after what the search contains: the count put; and every cell of the search given back, the
// search left empty (cargo_scan.c)
void cli_cargo_tokens(const CliCargoTestsCtx *args);
unsigned int cli_cargo_scan(const CliCargoTestsCtx *args);
void cli_cargo_scan_free(void);

struct CliCargoTestsACtx
{
    const char *a;
    const char *b;
    const char *between;
    char *out;
    unsigned long long capacity;
};

struct CliCargoTestsTextCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long *at;
    const char **line;
    unsigned long long *length;
};

struct CliCargoTestsTextSizeCtx
{
    const char *text;
    unsigned long long size;
    const char *folder;
    const char *package;
    unsigned int *count;
};

struct CliCargoTestsCtx
{
    const char *text;
    unsigned long long size;
    char *name;
};

typedef struct
{
    int (*cargo_line)(const CliCargoTestsTextCtx *args);
} CliCargoTestsTextNs;
EMBED_TABLE_LAYOUT(CliCargoTestsTextNs, cargo_line);
EMBED_TABLE_STORAGE CliCargoTestsTextNs cli_cargo_tests_text_internal EMBED_UNUSED = {
    .cargo_line = cli_cargo_line,
};

typedef struct
{
    int (*cargo_package_name)(const CliCargoTestsCtx *args);
    void (*cargo_tokens)(const CliCargoTestsCtx *args);
    unsigned int (*cargo_scan)(const CliCargoTestsCtx *args);
} CliCargoTestsNsTable;
EMBED_TABLE_LAYOUT(CliCargoTestsNsTable, cargo_package_name, cargo_tokens, cargo_scan);
EMBED_TABLE_STORAGE CliCargoTestsNsTable cli_cargo_tests_internal EMBED_UNUSED = {
    .cargo_package_name = cli_cargo_package_name,
    .cargo_tokens = cli_cargo_tokens,
    .cargo_scan = cli_cargo_scan,
};

typedef struct
{
    void (*cargo_join)(const CliCargoTestsACtx *args);
} CliCargoTestsANs;
EMBED_TABLE_LAYOUT(CliCargoTestsANs, cargo_join);
EMBED_TABLE_STORAGE CliCargoTestsANs cli_cargo_tests_a_internal EMBED_UNUSED = {
    .cargo_join = cli_cargo_join,
};

typedef struct
{
    const CliCargoFirst *(*cargo_targets)(const CliCargoTestsTextSizeCtx *args);
} CliCargoTestsTextSizeNs;
EMBED_TABLE_LAYOUT(CliCargoTestsTextSizeNs, cargo_targets);
EMBED_TABLE_STORAGE CliCargoTestsTextSizeNs cli_cargo_tests_text_size_internal EMBED_UNUSED = {
    .cargo_targets = cli_cargo_targets,
};

#endif
