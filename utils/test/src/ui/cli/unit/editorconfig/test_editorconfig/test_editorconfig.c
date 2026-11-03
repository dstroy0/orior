// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_editorconfig.c: .editorconfig: the nearest matching section sets a file's indentation
#include "ui/cli/src/editor/editorconfig.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[CLI_PATH_MAX_LEN];

// `text` written to the file `name` under the root
static void written(const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = CLI_UNITY_LENGTH(text)));
}

// the indentation of the file `name` under the root is `tabs`, -1 for unset, and `size`, 0 for unset
static void assert_indent(const char *name, int tabs, unsigned int size)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    CliIndent indent;
    EMBED_CALL(cli_editorconfig.indent, CliEditorconfigCfg, .path = path, .indent = &indent);
    TEST_ASSERT_EQUAL_INT_MESSAGE(tabs, indent.tabs, name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(size != 0u, indent.sized, name);
    if (size != 0u)
    {
        TEST_ASSERT_EQUAL_UINT_MESSAGE(size, indent.size, name);
    }
}

void setUp(void)
{
    cli_unity_begun();
    char deep[CLI_PATH_MAX_LEN];
    cli_unity_folder("editorconfig", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src/deep", .joined = deep);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = deep));
    written(".editorconfig", "root = true\n\n[*]\nindent_style = space\nindent_size = 4\n\n[*.{go,mk}]\n"
                             "indent_style = tab\ntab_width = 8\n\n[Makefile]\nindent_style = tab\n\n"
                             "[lib/**.js]\nindent_size = 2\n\n[file[0-9].txt]\nindent_size = 3\n");
    written("src/.editorconfig", "[*.py]\nindent_size = 2\n");
}

void tearDown(void)
{
}

void test_the_star_section_applies(void)
{
    assert_indent("a.rs", 0, 4u);
}

void test_a_brace_list_matches_each_extension(void)
{
    assert_indent("main.go", 1, 4u);
    assert_indent("rules.mk", 1, 4u);
}

void test_a_nearer_file_overrides_its_parent(void)
{
    assert_indent("src/deep/x.py", 0, 2u);
}

void test_a_double_star_crosses_folders(void)
{
    assert_indent("lib/a/b.js", 0, 2u);
}

void test_a_character_class_matches_its_digits_only(void)
{
    assert_indent("file7.txt", 0, 3u);
    assert_indent("filex.txt", 0, 4u);
}

void test_tab_width_sizes_tabs_with_no_indent_size(void)
{
    written(".editorconfig", "root = true\n[*.go]\nindent_style = tab\ntab_width = 8\n");
    assert_indent("main.go", 1, 8u);
}

void test_a_range_nested_braces_a_negated_class_a_question_mark_and_an_escape_match(void)
{
    written(".editorconfig", "root = true\n[part{2..4}.c]\nindent_size = 2\n[{x,y{1,z}}.h]\nindent_size = 3\n[[!a-m]?.md]\nindent_size = 5\n"
                             "[\\{one\\}.txt]\nindent_size = 6\n[{solo}.ini]\nindent_size = 7\n");
    assert_indent("part3.c", -1, 2u);
    assert_indent("part5.c", -1, 0u);
    assert_indent("x.h", -1, 3u);
    assert_indent("y1.h", -1, 3u);
    assert_indent("yz.h", -1, 3u);
    assert_indent("zq.md", -1, 5u);
    assert_indent("aq.md", -1, 0u);
    assert_indent("{one}.txt", -1, 6u);
    assert_indent("{solo}.ini", -1, 7u);
}

void test_a_file_no_section_matches_has_no_indentation_set(void)
{
    written(".editorconfig", "root = true\n[*.go]\nindent_style = tab\ntab_width = 8\n");
    assert_indent("a.rs", -1, 0u);
}
