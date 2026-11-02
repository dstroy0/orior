// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_prints.c: a line prints by its file's language, a diff's added lines report their line in the new file, and the prints a
// change adds are found in changed files and in files new to git
#include "ui/cli/src/code/prints.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[1u << 14u];
static char s_error[256];

// whether `line` of the file `path` prints
static int line_prints(const char *path, const char *line)
{
    return EMBED_CALL(cli_prints_path.line, CliPrintsPathCfg, .path = path, .text = line, .length = CLI_UNITY_LENGTH(line));
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_python_print_prints(void)
{
    TEST_ASSERT_TRUE(line_prints("a.py", "    print(total)"));
}

void test_a_longer_name_does_not_print(void)
{
    TEST_ASSERT_FALSE(line_prints("a.py", "    printer(total)"));
}

void test_a_comment_does_not_print(void)
{
    TEST_ASSERT_FALSE(line_prints("a.py", "    # print(total)"));
}

void test_javascript_console_log_prints(void)
{
    TEST_ASSERT_TRUE(line_prints("a.js", "  console.log('x', y);"));
}

void test_rust_dbg_prints(void)
{
    TEST_ASSERT_TRUE(line_prints("a.rs", "    dbg!(&value);"));
}

void test_c_printf_prints(void)
{
    TEST_ASSERT_TRUE(line_prints("a.c", "  printf(\"%d\\n\", n);"));
}

void test_c_fprintf_prints(void)
{
    TEST_ASSERT_TRUE(line_prints("a.c", "  fprintf(stderr, \"x\");"));
}

void test_go_println_prints(void)
{
    TEST_ASSERT_TRUE(line_prints("a.go", "\tfmt.Println(x)"));
}

void test_ruby_appends_prints(void)
{
    TEST_ASSERT_TRUE(line_prints("a.rb", "puts value"));
}

void test_a_longer_ruby_word_does_not_print(void)
{
    TEST_ASSERT_FALSE(line_prints("a.rb", "putsch value"));
}

void test_a_file_with_no_known_language_does_not_print(void)
{
    TEST_ASSERT_FALSE(line_prints("a.md", "print(x)"));
}

