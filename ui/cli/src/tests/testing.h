// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// testing.h: a tree's tests as the window lists them. A tree's Python tests are found as pytest and unittest find
// them, with no setup: each file named `test_*.py`, `*_test.py` or `test*.py`, and in it each function whose name
// starts with `test`, each such method of a class whose name starts with `Test` and that sets no `__init__`, and each
// such method of a class that inherits a `TestCase`. Where the tree gives pytest its own rules, in `pytest.ini`,
// `pyproject.toml`'s `[tool.pytest.ini_options]`, `tox.ini` or `setup.cfg`, its `testpaths`, `python_files`,
// `python_classes` and `python_functions` are read in place of these. The folders of environments, of what a build
// writes and of caches are not searched, nor a file larger than 2 MiB or not UTF-8.
//
// A test is named as pytest names it, `path::Class::name`, and a Rust test as orior_cli names one, its file first,
// `src/files.rs::listing::name` (testing.c, testing_found.c)
#ifndef TESTING_H
#define TESTING_H

#include "../command_line/cli_internal.h"

// A test: its name as pytest gives it, its file in the tree, its class where it has one, NULL where it has none, its
// own name, and the line its name stands on, counted from 0
typedef struct
{
    const char *id;
    const char *file;
    const char *class_name;
    const char *name;
    unsigned int line;
} CliTest;

// a test written as the window reads one, `{"id", "file", "class", "name", "line"}`
void cli_test_written(const CliTest *test, CliLine *out);

// the most values one of pytest's rules holds, and the bytes all of them take
#define CLI_TEST_VALUES 64u
#define CLI_TEST_RULES_TEXT 8192u

// One of pytest's rules: its values, each where it starts in the rules' text, ended by a 0
typedef struct
{
    unsigned int value[CLI_TEST_VALUES];
    unsigned int count;
} CliTestRule;

// The rules a tree gives pytest, or pytest's own where it gives none: the folders searched, none for every one, and
// what a file's, a class's and a function's names start with or match
typedef struct
{
    CliTestRule paths;
    CliTestRule files;
    CliTestRule classes;
    CliTestRule functions;
    char text[CLI_TEST_RULES_TEXT];
    unsigned int used;
} CliTestRules;

// pytest's own rules into `rules`; a value of one of them; the rules of `size` bytes of a configuration file's text,
// read from its `section`, as `[pytest]`: 1, or 0 where it has no such section; and the rules the tree at `root` gives,
// from the first of its configuration files that holds them, as pytest reads them (testing.c)
void cli_test_rules_own(CliTestRules *rules);
const char *cli_test_rule_value(const CliTestRules *rules, const CliTestRule *rule, unsigned int at);
int cli_test_rules_in(const char *text, unsigned long long size, const char *section, CliTestRules *rules);
void cli_test_rules_of(const char *root, CliTestRules *rules);

// whether `size` bytes of `name` match the glob `pattern`, its `*` any run of letters and its `?` any one; and whether
// they match one of the values of `rule`, a value with neither by starting with it
int cli_test_glob(const char *pattern, const char *name, unsigned long long size);
int cli_test_named(const CliTestRules *rules, const CliTestRule *rule, const char *name, unsigned long long size);

// A test found, handed on with its texts kept while the call lasts
typedef void (*CliTestEach)(void *context, const CliTest *test);

// the tests of one Python file, `size` bytes of its text, at `file` in the tree, each handed to `each` in the order of
// their lines (testing_found.c)
void cli_tests_in(const char *file, const char *text, unsigned long long size, const CliTestRules *rules,
                  CliTestEach each, void *context);
// the Python tests of the tree at `root`, file by file in the order of their paths; and its Python tests, then its
// Rust tests as cargo_tests.h finds them, each handed to `each` under the finder's lock
void cli_tests_python_found(const char *root, CliTestEach each, void *context);
void cli_tests_found(const char *root, CliTestEach each, void *context);

#endif
