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
#include "embed_dispatch_layout.h"

typedef struct CliTestingRulesTextCfg CliTestingRulesTextCfg;
typedef struct CliTestingRulesCfg CliTestingRulesCfg;
typedef struct CliTestingRulesRuleSizeCfg CliTestingRulesRuleSizeCfg;
typedef struct CliTestingSizeCfg CliTestingSizeCfg;
typedef struct CliTestingRulesRuleCfg CliTestingRulesRuleCfg;
typedef struct CliTestingRootCfg CliTestingRootCfg;
typedef struct CliTestingCfg CliTestingCfg;

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
void cli_test_write(const CliTestingCfg *args);

// the most values one of pytest's rules has, and the bytes all of them take
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
// from the first of its configuration files that contains them, as pytest reads them (testing.c)
void cli_test_rules_own(const CliTestingRulesCfg *args);
const char *cli_test_rule_value(const CliTestingRulesRuleCfg *args);
int cli_test_rules_in(const CliTestingRulesCfg *args);
void cli_test_rules_get(const CliTestingRulesCfg *args);

// whether `size` bytes of `name` match the glob `pattern`, its `*` any run of letters and its `?` any one; and whether
// they match one of the values of `rule`, a value with neither by starting with it
int cli_test_glob(const CliTestingSizeCfg *args);
int cli_test_by_name(const CliTestingRulesRuleSizeCfg *args);

typedef struct
{
    void *context;
    const CliTest *test;
} CliTestEachCfg;

// A test found, handed on with its texts retained while the call lasts
typedef void (*CliTestEach)(const CliTestEachCfg *args);

// the tests of one Python file, `size` bytes of its text, at `file` in the tree, each handed to `each` in the order of
// their lines (testing_found.c)
void cli_tests_in(const CliTestingRulesTextCfg *args);
// the Python tests of the tree at `root`, file by file in the order of their paths; and its Python tests, then its
// Rust tests as cargo_tests.h finds them, each handed to `each` under the finder's lock
void cli_tests_python_find(const CliTestingRootCfg *args);
void cli_tests_find(const CliTestingRootCfg *args);

struct CliTestingRulesTextCfg
{
    const CliTestRules *rules_read;
    const char *text;
    unsigned long long size;
    const char *file;
    CliTestEach each;
    void *context;
};

struct CliTestingRulesCfg
{
    CliTestRules *rules;
    const char *text;
    unsigned long long size;
    const char *section;
    const char *root;
};

struct CliTestingRulesRuleSizeCfg
{
    const CliTestRules *rules_read;
    const CliTestRule *rule;
    unsigned long long size;
    const char *name;
};

struct CliTestingSizeCfg
{
    unsigned long long size;
    const char *pattern;
    const char *name;
};

struct CliTestingRulesRuleCfg
{
    const CliTestRules *rules_read;
    const CliTestRule *rule;
    unsigned int at;
};

struct CliTestingRootCfg
{
    const char *root;
    CliTestEach each;
    void *context;
};

struct CliTestingCfg
{
    const CliTest *test;
    VerbaLinea *out;
};

typedef struct
{
    void (*test_write)(const CliTestingCfg *args);
} CliTestingNs;
EMBED_TABLE_LAYOUT(CliTestingNs, test_write);
EMBED_TABLE_STORAGE CliTestingNs cli_testing EMBED_UNUSED = {
    .test_write = cli_test_write,
};

typedef struct
{
    void (*test_rules_own)(const CliTestingRulesCfg *args);
    int (*test_rules_in)(const CliTestingRulesCfg *args);
    void (*test_rules_get)(const CliTestingRulesCfg *args);
} CliTestingRulesNs;
EMBED_TABLE_LAYOUT(CliTestingRulesNs, test_rules_own, test_rules_in, test_rules_get);
EMBED_TABLE_STORAGE CliTestingRulesNs cli_testing_rules EMBED_UNUSED = {
    .test_rules_own = cli_test_rules_own,
    .test_rules_in = cli_test_rules_in,
    .test_rules_get = cli_test_rules_get,
};

typedef struct
{
    const char *(*test_rule_value)(const CliTestingRulesRuleCfg *args);
} CliTestingRulesRuleNs;
EMBED_TABLE_LAYOUT(CliTestingRulesRuleNs, test_rule_value);
EMBED_TABLE_STORAGE CliTestingRulesRuleNs cli_testing_rules_rule EMBED_UNUSED = {
    .test_rule_value = cli_test_rule_value,
};

typedef struct
{
    int (*test_glob)(const CliTestingSizeCfg *args);
} CliTestingSizeNs;
EMBED_TABLE_LAYOUT(CliTestingSizeNs, test_glob);
EMBED_TABLE_STORAGE CliTestingSizeNs cli_testing_size EMBED_UNUSED = {
    .test_glob = cli_test_glob,
};

typedef struct
{
    int (*test_by_name)(const CliTestingRulesRuleSizeCfg *args);
} CliTestingRulesRuleSizeNs;
EMBED_TABLE_LAYOUT(CliTestingRulesRuleSizeNs, test_by_name);
EMBED_TABLE_STORAGE CliTestingRulesRuleSizeNs cli_testing_rules_rule_size EMBED_UNUSED = {
    .test_by_name = cli_test_by_name,
};

typedef struct
{
    void (*tests_in)(const CliTestingRulesTextCfg *args);
} CliTestingRulesTextNs;
EMBED_TABLE_LAYOUT(CliTestingRulesTextNs, tests_in);
EMBED_TABLE_STORAGE CliTestingRulesTextNs cli_testing_rules_text EMBED_UNUSED = {
    .tests_in = cli_tests_in,
};

typedef struct
{
    void (*tests_python_find)(const CliTestingRootCfg *args);
    void (*tests_find)(const CliTestingRootCfg *args);
} CliTestingRootNs;
EMBED_TABLE_LAYOUT(CliTestingRootNs, tests_python_find, tests_find);
EMBED_TABLE_STORAGE CliTestingRootNs cli_testing_root EMBED_UNUSED = {
    .tests_python_find = cli_tests_python_find,
    .tests_find = cli_tests_find,
};

#endif
