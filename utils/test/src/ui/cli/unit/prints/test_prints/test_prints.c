// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_prints.c: a line prints by its file's language, a diff's added lines report their line in the new file, and the prints a
// change adds are found in changed files and in files new to git
#include "ui/cli/src/code/prints.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[1u << 14u];
static char s_why[256];

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

void test_ruby_puts_prints(void)
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

void test_a_diffs_added_lines_report_their_line_in_the_new_file(void)
{
    static const char s_diff[] = "diff --git a/a.py b/a.py\n--- a/a.py\n+++ b/a.py\n@@ -3,0 +4,2 @@ def f():\n"
                                 "+    print(x)\n+    y = 1\n@@ -10 +12 @@\n-    old\n+    print(\"new\")\n";
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prints_out.put_in, CliPrintsOutCfg, .diff = s_diff, .size = sizeof(s_diff) - 1u, .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT("[{\"path\":\"a.py\",\"line\":3,\"text\":\"    print(x)\"},"
                         "{\"path\":\"a.py\",\"line\":11,\"text\":\"    print(\\\"new\\\")\"}]",
                         s_out, out.at);
}

void test_a_change_reports_only_the_prints_it_added(void)
{
    static const char *const s_init[2] = {"init", "-q"};
    static const char *const s_name[3] = {"config", "user.name", "Tester"};
    static const char *const s_email[3] = {"config", "user.email", "tester@example.com"};
    static const char *const s_add[2] = {"add", "."};
    static const char *const s_first[4] = {"commit", "-q", "-m", "first"};
    char root[CLI_PATH_MAX_LEN];
    cli_unity_folder("prints_added", root);
    cli_unity_git(root, s_init, 2u);
    cli_unity_git(root, s_name, 3u);
    cli_unity_git(root, s_email, 3u);
    cli_unity_file(root, "a.py", "def f(x):\n    print(\"stays\")\n    return x\n");
    cli_unity_git(root, s_add, 2u);
    cli_unity_git(root, s_first, 4u);
    cli_unity_file(root, "a.py", "def f(x):\n    print(\"stays\")\n    print(x)\n    return x\n");
    cli_unity_file(root, "b.js", "const a = 1;\nconsole.log(a);\n");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prints.add, CliPrintsCfg, .root = root, .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT("[{\"path\":\"a.py\",\"line\":2,\"text\":\"    print(x)\"},"
                         "{\"path\":\"b.js\",\"line\":1,\"text\":\"console.log(a);\"}]",
                         s_out, out.at);
}
