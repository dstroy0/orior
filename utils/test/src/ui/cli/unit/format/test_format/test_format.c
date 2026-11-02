// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_format.c: formatting: each formatter names a language, a width is read from each formatter's own settings, and a file's
// language is the window's
#include "ui/cli/src/toolchains/format.h"
#include "ui/cli/src/toolchains/format_internal.h"
#include "ui/cli/src/toolchains/toolchains.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_dir[CLI_PATH_MAX_LEN];
static char s_pkg[CLI_PATH_MAX_LEN];

// the number `key` of `text`, within `table` where it is not NULL, -1 where it has none
static long long number_in(const char *text, const char *table, const char *key)
{
    unsigned int number = 0u;
    return EMBED_CALL(cli_format_text_internal.number_in, CliFormatTextCtx, .text = text, .table = table, .key = key, .number = &number) ? (long long)number : -1ll;
}

// the width the formatter of `language` limits the file `name` of the package to, -1 where it gives none
static long long width_of(const char *name, const char *language)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_pkg, .relative = name, .joined = path);
    unsigned int width = 0u;
    return EMBED_CALL(cli_format_path_language.width, CliFormatPathLanguageCfg, .path = path, .language = language, .width = &width) ? (long long)width : -1ll;
}

// the language `path` opens in, the empty string where none does
static const char *language_of(const char *path)
{
    static char s_id[CLI_NAME_MAX_LEN];
    return EMBED_CALL(cli_format_table.language_get, CliFormatCfg, .path = path, .id = s_id) ? s_id : "";
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("format_width", s_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "pkg", .joined = s_pkg);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_pkg));
    cli_unity_file(s_dir, "pyproject.toml", "[tool.black]\nline-length = 100\n");
    cli_unity_file(s_dir, "Cargo.toml", "[package]\nname = \"x\"\nedition = \"2024\"\n");
}

void tearDown(void)
{
}

void test_each_formatter_names_a_language(void)
{
    static CliJson s_json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg, .json_out = &s_json, .nodes_more = 0u, .text_more = 0ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = manifest, .kind = CFG_JSON_ARRAY));
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = manifest, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = manifest, .index = at);
        const unsigned int formats = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = tool, .name = "formats");
        TEST_ASSERT_TRUE((EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = tool, .name = "format") == CLI_JSON_NONE) || (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = formats, .kind = CFG_JSON_ARRAY) && (s_json.node[formats].count > 0u)));
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
}

void test_python_is_formatted_by_black(void)
{
    char program[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_format_language.program_get, CliFormatLanguageCfg, .language = "python", .program = program));
    TEST_ASSERT_EQUAL_STRING("black", program);
}

void test_c_is_formatted_by_clang_format(void)
{
    char program[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_format_language.program_get, CliFormatLanguageCfg, .language = "c", .program = program));
    TEST_ASSERT_EQUAL_STRING("clang-format", program);
}

void test_tex_has_no_formatter(void)
{
    char program[CLI_NAME_MAX_LEN];
    TEST_ASSERT_FALSE(EMBED_CALL(cli_format_language.program_get, CliFormatLanguageCfg, .language = "tex", .program = program));
}

void test_blacks_width_is_read_within_its_table(void)
{
    TEST_ASSERT_EQUAL_INT64(120ll, number_in("[project]\nname = \"x\"\n[tool.black]\nline-length = 120 # wide\n", "tool.black", "line-length"));
}

void test_a_width_in_another_table_is_not_read(void)
{
    TEST_ASSERT_EQUAL_INT64(-1ll, number_in("[tool.ruff]\nline-length = 99\n", "tool.black", "line-length"));
}

void test_rustfmts_width_is_read_before_any_table(void)
{
    TEST_ASSERT_EQUAL_INT64(110ll, number_in("max_width = 110\n[other]\nmax_width = 3\n", NULL, "max_width"));
}

void test_clang_formats_width_is_read_from_its_yaml(void)
{
    TEST_ASSERT_EQUAL_INT64(140ll, number_in("BasedOnStyle: LLVM\nColumnLimit: 140\n", NULL, "ColumnLimit"));
}

void test_prettiers_width_is_read_from_its_json(void)
{
    TEST_ASSERT_EQUAL_INT64(100ll, number_in("{ \"semi\": false, \"printWidth\": 100 }", NULL, "printWidth"));
}

void test_a_width_is_read_from_the_nearest_folder_above(void)
{
    TEST_ASSERT_EQUAL_INT64(100ll, width_of("a.py", "python"));
}

void test_a_crates_edition_is_read(void)
{
    char edition[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_pkg, .relative = "a.rs", .joined = path);
    EMBED_CALL(cli_format_internal.edition_get, CliFormatCtx, .path = path, .edition = edition);
    TEST_ASSERT_EQUAL_STRING("2024", edition);
}

void test_a_language_with_no_formatter_has_no_width(void)
{
    TEST_ASSERT_EQUAL_INT64(-1ll, width_of("a.tex", "tex"));
}

void test_markdown_prose_has_no_width_unless_it_wraps(void)
{
    cli_unity_file(s_dir, ".prettierrc", "{ \"printWidth\": 100 }");
    TEST_ASSERT_EQUAL_INT64(-1ll, width_of("a.md", "markdown"));
}

void test_prettiers_width_is_the_margin_of_a_script(void)
{
    char by[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned int width = 0u;
    cli_unity_file(s_dir, ".prettierrc", "{ \"printWidth\": 100 }");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_pkg, .relative = "a.js", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_format_path_language.margin, CliFormatPathLanguageCfg, .path = path, .language = "javascript", .width = &width, .by = by));
    TEST_ASSERT_EQUAL_UINT(100u, width);
}

void test_markdown_prose_takes_the_width_where_it_wraps(void)
{
    cli_unity_file(s_dir, ".prettierrc", "{ \"printWidth\": 100, \"proseWrap\": \"always\" }");
    TEST_ASSERT_EQUAL_INT64(100ll, width_of("a.md", "markdown"));
}

void test_a_python_file_is_python(void)
{
    TEST_ASSERT_EQUAL_STRING("python", language_of("a/b.py"));
}

void test_an_extensions_case_does_not_matter(void)
{
    TEST_ASSERT_EQUAL_STRING("c", language_of("k.CU"));
}

void test_of_two_languages_the_first_by_id_wins(void)
{
    TEST_ASSERT_EQUAL_STRING("matlab", language_of("smooth.m"));
}

void test_a_file_with_no_extension_has_no_language(void)
{
    TEST_ASSERT_EQUAL_STRING("", language_of("none"));
}
