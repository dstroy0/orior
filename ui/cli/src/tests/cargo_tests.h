// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_tests.h: a tree's Rust tests, found as Cargo and the test harness find them, and run with cargo test.
//
// Each Cargo.toml of the tree that makes a package gives its targets: the library, each binary and each test, by its
// [lib], [[bin]] and [[test]] tables and by the files Cargo finds on its own. Each target's modules are followed from
// its first file, `mod name;` by `mod name;`, and in each file every function marked `#[test]`, or with an attribute
// whose path ends in `test` as `#[tokio::test]`, is a test. A test is named as orior names a Python one, its file
// first, `src/files.rs::listing::name`, and runs by the name the harness gives it, `files::listing::name`.
//
// A run gives each target its own cargo test, every target's at once, with the words rust-analyzer gives an editor
// for a test: the package by its name, the target, `--no-fail-fast`, and the names of the tests asked of it given to
// the harness with `--exact`, or none where every test of it is asked, with `--show-output`, and `--include-ignored`
// where each test is asked by its name. The harness's own report is read, and RUSTC_BOOTSTRAP is not set for its report
// in JSON: Cargo builds every crate again for it, and again for the next build without it. Each result is passed on as
// the harness prints it, and each failure's message and the line it failed at as its process ends, from the panic it
// reports; a target whose build fails fails each of its tests with the compiler's first error (cargo_scan.c,
// cargo_targets.c, cargo_found.c, cargo_run.c, cargo_binary.c)
#ifndef CARGO_TESTS_H
#define CARGO_TESTS_H

#include "testing.h"

// the most tests found, and the bytes their texts take
#define CLI_CARGO_FOUND 16384u
#define CLI_CARGO_TEXT (1u << 22u)

// what a target is: the library, a binary, or a test
#define CLI_CARGO_LIB 0u
#define CLI_CARGO_BIN 1u
#define CLI_CARGO_TEST 2u

// A target of a package as Cargo builds it: its kind, and its name, NULL for the library
typedef struct
{
    unsigned int kind;
    const char *name;
} CliCargoTarget;

// A test as Cargo runs it: the test, its package's manifest in the tree, its package, its target, and its name as the
// harness gives it
typedef struct
{
    CliTest test;
    const char *manifest;
    const char *package;
    CliCargoTarget target;
    const char *full;
} CliCargoFound;

// The Rust tests of a tree and the texts they take, with 1 in `full` where one did not fit
typedef struct
{
    CliCargoFound found[CLI_CARGO_FOUND];
    unsigned int count;
    char text[CLI_CARGO_TEXT];
    unsigned long long used;
    int full;
} CliCargoTests;

// whether `id`, a test's name or a file's path, is of Rust (cargo_found.c)
int cli_cargo_is_rust(const char *id);
// the Rust tests of the tree at `root` into `tests`, package by package in the order of their manifests' paths
void cli_cargo_found(const char *root, CliCargoTests *tests);
// how many cargo tests a run of `given`, `count` tests by their names or files by their paths, starts (cargo_run.c)
unsigned int cli_cargo_targets_given(const char *root, const char *const *given, unsigned int count);
// the program that runs the test `id` and the name the harness gives it, built as cargo test builds it, with the
// package's folder for it to run in, for a debugger to start it: each into what holds CLI_PATH_LONGEST, 1, or 0 with
// why said (cargo_binary.c)
int cli_cargo_binary(const char *root, const char *id, char *program, char *full, char *package, CliLine *said);

#endif
