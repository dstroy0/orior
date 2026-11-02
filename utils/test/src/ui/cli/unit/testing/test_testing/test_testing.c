// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_testing.c: tests are found as pytest and unittest find them, a tree's configuration sets pytest's rules, and a tree is searched
// for its tests
#include "ui/cli/src/tests/testing.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_text[] = "import unittest\n\n\ndef test_one():\n    def test_inner():\n        pass\n\n\ndef helper():\n    pass\n\n\n"
                             "class TestThing:\n    def test_two(self):\n        pass\n\n    def other(self):\n        pass\n\n\n"
                             "class TestBuilt:\n    def __init__(self):\n        pass\n\n    def test_skipped(self):\n        pass\n\n\n"
                             "class Checks(unittest.TestCase):\n    def test_three(self):\n        pass\n\n    def setUp(self):\n"
                             "        pass\n";
static const char s_toml[] = "[project]\nname = \"x\"\n\n[tool.pytest.ini_options]\ntestpaths = [\"checks\"]\n"
                             "python_files = [\"check_*.py\"]\npython_functions = \"check\"\n";
static const char s_ini[] = "[pytest]\ntestpaths =\n    tests\n    more\npython_classes = Should*\n";

static CliTestRules s_rules;
static char s_ids[4096];
static VerbaLinea s_found = {s_ids, sizeof(s_ids), 0ull};
static unsigned int s_count;
static unsigned int s_second_line;

// a found test appended to the others, `|` between them
static void found_recorded(const CliTestEachCfg *args)
{
    mmgr_line_put(&s_found, (s_found.at > 0ull) ? "|" : "");
    mmgr_line_put(&s_found, args->test->id);
    s_second_line = (s_count == 1u) ? args->test->line : s_second_line;
    s_count += 1u;
}

// the found tests cleared
static void found_cleared(void)
{
    s_found.at = 0ull;
    s_count = 0u;
}

// the values of a rule, `|` between them, into `out`, 256 bytes
static const char *rule_values(const CliTestRule *rule, char *out)
{
    VerbaLinea line = {out, 256u, 0ull};
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, EMBED_CALL(cli_testing_rules_rule.test_rule_value, CliTestingRulesRuleCfg, .rules_read = &s_rules, .rule = rule, .at = at));
    }
    (void)mmgr_line_finish(&line);
    return out;
}

// the file `name` of the folder written with `text`, its folder made
static void tree_file(const char *dir, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = name, .joined = path);
    char *const slash = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path, .length_value = CLI_UNITY_LENGTH(path), .byte = '/');
    if (slash != NULL)
    {
        slash[0] = '\0';
        TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
        slash[0] = '/';
    }
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = CLI_UNITY_LENGTH(text)));
}

// a tree of a test, a test beside the code and a test in an environment, in the folder `dir`
static void tree_written(char *dir)
{
    cli_unity_folder("testing", dir);
    tree_file(dir, "tests/test_a.py", "def test_a():\n    pass\n");
    tree_file(dir, "pkg/b_test.py", "def test_b():\n    pass\n");
    tree_file(dir, "pkg/main.py", "def test_not():\n    pass\n");
    tree_file(dir, ".venv/lib/test_x.py", "def test_x():\n    pass\n");
}

void setUp(void)
{
    cli_unity_begun();
    found_cleared();
    EMBED_CALL(cli_testing_rules.test_rules_own, CliTestingRulesCfg, .rules = &s_rules);
}

void tearDown(void)
{
}

void test_tests_are_found_as_pytest_and_unittest_find_them(void)
{
    EMBED_CALL(cli_testing_rules_text.tests_in, CliTestingRulesTextCfg, .file = "tests/test_a.py", .text = s_text, .size = sizeof(s_text) - 1u, .rules_read = &s_rules, .each = found_recorded, .context = NULL);
    CLI_TEST_ASSERT_TEXT("tests/test_a.py::test_one|tests/test_a.py::TestThing::test_two|tests/test_a.py::Checks::test_three", s_ids, s_found.at);
}

void test_a_found_test_reports_the_line_of_its_name(void)
{
    EMBED_CALL(cli_testing_rules_text.tests_in, CliTestingRulesTextCfg, .file = "tests/test_a.py", .text = s_text, .size = sizeof(s_text) - 1u, .rules_read = &s_rules, .each = found_recorded, .context = NULL);
    TEST_ASSERT_EQUAL_UINT(13u, s_second_line);
}

void test_a_pyproject_sets_the_test_paths(void)
{
    char values[256];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_testing_rules.test_rules_in, CliTestingRulesCfg, .text = s_toml, .size = sizeof(s_toml) - 1u, .section = "[tool.pytest.ini_options]", .rules = &s_rules));
    TEST_ASSERT_EQUAL_STRING("checks", rule_values(&s_rules.paths, values));
}

void test_a_pyproject_sets_the_test_files(void)
{
    char values[256];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_testing_rules.test_rules_in, CliTestingRulesCfg, .text = s_toml, .size = sizeof(s_toml) - 1u, .section = "[tool.pytest.ini_options]", .rules = &s_rules));
    TEST_ASSERT_EQUAL_STRING("check_*.py", rule_values(&s_rules.files, values));
}

void test_a_pyproject_sets_the_test_functions(void)
{
    char values[256];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_testing_rules.test_rules_in, CliTestingRulesCfg, .text = s_toml, .size = sizeof(s_toml) - 1u, .section = "[tool.pytest.ini_options]", .rules = &s_rules));
    TEST_ASSERT_EQUAL_STRING("check", rule_values(&s_rules.functions, values));
}

void test_a_pytest_ini_value_of_many_lines_is_read(void)
{
    char values[256];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_testing_rules.test_rules_in, CliTestingRulesCfg, .text = s_ini, .size = sizeof(s_ini) - 1u, .section = "[pytest]", .rules = &s_rules));
    TEST_ASSERT_EQUAL_STRING("tests|more", rule_values(&s_rules.paths, values));
}

void test_a_pytest_ini_sets_the_test_classes(void)
{
    char values[256];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_testing_rules.test_rules_in, CliTestingRulesCfg, .text = s_ini, .size = sizeof(s_ini) - 1u, .section = "[pytest]", .rules = &s_rules));
    TEST_ASSERT_EQUAL_STRING("Should*", rule_values(&s_rules.classes, values));
}

void test_a_file_without_the_section_sets_no_rules(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_testing_rules.test_rules_in, CliTestingRulesCfg, .text = "[other]\nx = 1\n", .size = 14ull, .section = "[pytest]", .rules = &s_rules));
}

void test_a_prefix_glob_matches(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_testing_size.test_glob, CliTestingSizeCfg, .pattern = "test_*.py", .name = "test_a.py", .size = 9ull));
}

void test_a_suffix_glob_matches(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_testing_size.test_glob, CliTestingSizeCfg, .pattern = "*_test.py", .name = "a_test.py", .size = 9ull));
}

void test_a_glob_does_not_match_another_name(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_testing_size.test_glob, CliTestingSizeCfg, .pattern = "test_*.py", .name = "a_test.py", .size = 9ull));
}

void test_a_trees_tests_are_found_and_its_environment_is_skipped(void)
{
    char dir[CLI_PATH_MAX_LEN];
    tree_written(dir);
    EMBED_CALL(cli_testing_root.tests_find, CliTestingRootCfg, .root = dir, .each = found_recorded, .context = NULL);
    CLI_TEST_ASSERT_TEXT("pkg/b_test.py::test_b|tests/test_a.py::test_a", s_ids, s_found.at);
}

void test_only_the_folders_a_trees_rules_name_are_searched(void)
{
    char dir[CLI_PATH_MAX_LEN];
    tree_written(dir);
    tree_file(dir, "pytest.ini", "[pytest]\ntestpaths = tests\n");
    EMBED_CALL(cli_testing_root.tests_find, CliTestingRootCfg, .root = dir, .each = found_recorded, .context = NULL);
    CLI_TEST_ASSERT_TEXT("tests/test_a.py::test_a", s_ids, s_found.at);
}
