// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_tests_internal.h: what the cargo test sources share: one file's text read as the search for tests reads it,
// and the room the search keeps what files hold in
#ifndef CARGO_TESTS_INTERNAL_H
#define CARGO_TESTS_INTERNAL_H

#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "cargo_tests.h"

// the cells the search for Rust's tests holds; and a cell of `most` of `width` bytes, or of twice `*room` where that
// is more, holding the `*room` of `cell`, `cell` given back and `*room` raised: the cell, or NULL where it is refused
// and `cell` is kept (cargo_scan.c)
extern CliCellsTally g_cli_cargo_cells;
void *cli_cargo_grown(void *cell, unsigned long long *room, unsigned long long most, unsigned long long width,
                      const char *what);

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

// What one file holds for the search: a test, or a module the file names with `mod name;`: where in the room's names
// the modules written in the file it stands in start, joined by `::`, and its name, and the line a test's name stands
// on
typedef struct
{
    int test;
    unsigned long long inline_names;
    unsigned long long name;
    unsigned int line;
} CliRustHeld;

// What the search reads into: one file's tokens, in a cell of their count; what the files followed hold, kept in turn
// as each file is followed and let go once it and the files it names are; and the bytes their names take, each in a
// cell grown as it fills
typedef struct
{
    CliRustToken *tokens;
    unsigned int token_count;
    unsigned long long token_room;
    CliRustHeld *held;
    unsigned int held_count;
    unsigned long long held_room;
    char *names;
    unsigned long long name_used;
    unsigned long long name_room;
} CliCargoScan;

extern CliCargoScan g_cli_cargo_scan;

// A target and its first file, as a path from the package's folder
typedef struct
{
    unsigned int kind;
    char name[CLI_NAME_LONGEST];
    char path[CLI_PATH_LONGEST];
} CliCargoFirst;

// the next line of `size` bytes from `at`, trimmed, as its start and length: 1, or 0 where none is left; the name of
// the package a Cargo.toml describes into `name`, which holds CLI_NAME_LONGEST: 1, or 0 where it describes none; `a`
// and `b` joined by `between`, either of them left out where it is empty, into `out`, which holds `room`; and the
// targets of the package a Cargo.toml in `folder` describes, `package` its name, each with its first file: the
// library, the binaries and the tests, as Cargo finds them, their count into `count` (cargo_targets.c)
int cli_cargo_line(const char *text, unsigned long long size, unsigned long long *at, const char **line,
                   unsigned long long *length);
int cli_cargo_package_name(const char *text, unsigned long long size, char *name);
void cli_cargo_joined(const char *a, const char *b, const char *between, char *out, unsigned long long room);
const CliCargoFirst *cli_cargo_targets(const char *folder, const char *text, unsigned long long size,
                                       const char *package, unsigned int *count);
// the targets cli_cargo_targets found given back, which their next finding does as well (cargo_targets.c)
void cli_cargo_firsts_given(void);

// the tokens of `size` bytes of Rust, each with the line it stands on: comments, strings and characters passed over,
// and a lifetime no token, counted and then put into a cell of their count, the cell of the file before given back;
// what they hold put after what the search holds: the count put; and every cell of the search given back, the search
// left empty (cargo_scan.c)
void cli_cargo_tokens(const char *text, unsigned long long size);
unsigned int cli_cargo_scan(const char *text);
void cli_cargo_scan_given(void);

#endif
