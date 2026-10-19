// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// testing_test.c: the tests' finder's check: tests are found as pytest and unittest find them, a tree gives pytest its
// own rules, and a tree is searched for its tests
#include "../../../../../ui/cli/src/tests/testing.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static CliTestRules s_rules;
static char s_ids[4096];
static CliLine s_found = {s_ids, sizeof(s_ids), 0ull};
static unsigned int s_count;
static unsigned int s_second_line;

// a test found put after the others, `|` between them
static void testing_kept(void *context, const CliTest *test)
{
    (void)context;
    cli_line_text(&s_found, (s_found.at > 0ull) ? "|" : "");
    cli_line_text(&s_found, test->id);
    s_second_line = (s_count == 1u) ? test->line : s_second_line;
    s_count += 1u;
}

// the tests found begun again
static void testing_begun(void)
{
    s_found.at = 0ull;
    s_count = 0u;
}

// the values of a rule, `|` between them, into `out`, which holds 256
static const char *testing_values(const CliTestRule *rule, char *out)
{
    CliLine line = {out, 256u, 0ull};
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_test_rule_value(&s_rules, rule, at));
    }
    (void)cli_line_closed(&line);
    return out;
}

// tests are found as pytest and unittest find them
static void testing_found_in_text(void)
{
    static const char s_text[] =
        "import unittest\n\n\ndef test_one():\n    def test_inner():\n        pass\n\n\ndef helper():\n    pass\n\n\n"
        "class TestThing:\n    def test_two(self):\n        pass\n\n    def other(self):\n        pass\n\n\n"
        "class TestBuilt:\n    def __init__(self):\n        pass\n\n    def test_skipped(self):\n        pass\n\n\n"
        "class Checks(unittest.TestCase):\n    def test_three(self):\n        pass\n\n    def setUp(self):\n"
        "        pass\n";
    cli_test_rules_own(&s_rules);
    testing_begun();
    cli_tests_in("tests/test_a.py", s_text, sizeof(s_text) - 1u, &s_rules, testing_kept, NULL);
    cli_check_text(s_ids, s_found.at,
                   "tests/test_a.py::test_one|tests/test_a.py::TestThing::test_two|tests/test_a.py::Checks::test_three",
                   "tests are found as pytest and unittest find them");
    cli_check(s_second_line == 13u, "each on the line its name stands on");
}

// a tree gives pytest its own rules
static void testing_rules(void)
{
    static const char s_toml[] = "[project]\nname = \"x\"\n\n[tool.pytest.ini_options]\ntestpaths = [\"checks\"]\n"
                                 "python_files = [\"check_*.py\"]\npython_functions = \"check\"\n";
    static const char s_ini[] = "[pytest]\ntestpaths =\n    tests\n    more\npython_classes = Should*\n";
    char values[256];
    cli_check(cli_test_rules_in(s_toml, sizeof(s_toml) - 1u, "[tool.pytest.ini_options]", &s_rules),
              "a pyproject.toml gives pytest's rules");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = testing_values(&s_rules.paths, values), .other = "checks", .cap = CLI_CELLS_POOL}),
              "its folders");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = testing_values(&s_rules.files, values), .other = "check_*.py", .cap = CLI_CELLS_POOL}),
              "its files");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = testing_values(&s_rules.functions, values), .other = "check", .cap = CLI_CELLS_POOL}),
              "and its functions");
    cli_check(cli_test_rules_in(s_ini, sizeof(s_ini) - 1u, "[pytest]", &s_rules), "a pytest.ini gives them");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = testing_values(&s_rules.paths, values), .other = "tests|more", .cap = CLI_CELLS_POOL}),
              "a value of many lines");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = testing_values(&s_rules.classes, values), .other = "Should*", .cap = CLI_CELLS_POOL}),
              "and its classes");
    cli_check(!cli_test_rules_in("[other]\nx = 1\n", 16ull, "[pytest]", &s_rules), "a file of no such section none");
    cli_check(cli_test_glob("test_*.py", "test_a.py", 9ull) && cli_test_glob("*_test.py", "a_test.py", 9ull) &&
                  !cli_test_glob("test_*.py", "a_test.py", 9ull),
              "a glob matches as pytest's do");
}

// the file `name` of the folder written with `text`, its folder made
static void testing_file(const char *dir, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(dir, name, path);
    char *const slash = cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    if (slash != NULL)
    {
        slash[0] = '\0';
        (void)query_folder_made(path);
        slash[0] = '/';
    }
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// a tree is searched for its tests
static void testing_tree(void)
{
    char dir[CLI_PATH_LONGEST];
    cli_check_folder("testing", dir);
    testing_file(dir, "tests/test_a.py", "def test_a():\n    pass\n");
    testing_file(dir, "pkg/b_test.py", "def test_b():\n    pass\n");
    testing_file(dir, "pkg/main.py", "def test_not():\n    pass\n");
    testing_file(dir, ".venv/lib/test_x.py", "def test_x():\n    pass\n");
    testing_begun();
    cli_tests_found(dir, testing_kept, NULL);
    cli_check_text(s_ids, s_found.at, "pkg/b_test.py::test_b|tests/test_a.py::test_a",
                   "a tree's tests are found, its environment's left out");
    testing_file(dir, "pytest.ini", "[pytest]\ntestpaths = tests\n");
    testing_begun();
    cli_tests_found(dir, testing_kept, NULL);
    cli_check_text(s_ids, s_found.at, "tests/test_a.py::test_a", "in the folders its rules name");
}

int main(void)
{
    testing_found_in_text();
    testing_rules();
    testing_tree();
    return cli_checked();
}
