// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_builds.c: build files: the kind goes by file name, values complete after what was typed, positions count UTF-16 columns,
// and near names are offered
#include "ui/cli/src/builds/builds.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliCompletions s_items;
static const char *const s_known[3] = {"implementation", "api", "compileOnly"};

// what completes `typed` among two property names
static const CliCompletions *completed(const char *typed)
{
    static const char *const s_values[2] = {"maven.compiler.source", "project.version"};
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = &s_items, .token = typed, .token_size = CLI_UNITY_LENGTH(typed), .value = s_values[at], .value_size = CLI_UNITY_LENGTH(s_values[at]), .kind = "property", .detail = "", .detail_size = 0ull);
    }
    return &s_items;
}

// the item `at` inserts `wanted`
static void assert_inserts(const char *wanted, const CliCompletions *items, unsigned int at)
{
    TEST_ASSERT_LESS_THAN_UINT(items->count, at);
    CLI_TEST_ASSERT_TEXT(wanted, items->text + items->item[at].insert_at, items->item[at].insert_size);
}

// the names near `name` among the first `count` known, joined by `|`, into `out`, 256 bytes
static const char *near_names(const char *name, unsigned int count, char *out)
{
    CliBuildNear near;
    EMBED_CALL(cli_builds_size.build_near_begin, CliBuildsSizeCfg, .near = &near, .name = name, .size = CLI_UNITY_LENGTH(name));
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_builds_size_near.build_near_offer, CliBuildsSizeNearCfg, .near = &near, .one = s_known[at], .size = CLI_UNITY_LENGTH(s_known[at]));
    }
    VerbaLinea line = {out, 256u, 0ull};
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put_n(&line, near.near[at], near.near_size[at]);
    }
    (void)mmgr_line_finish(&line);
    return out;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_gradle_scripts_and_settings_are_gradle(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_GRADLE, EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = "a/build.gradle.kts"));
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_GRADLE, EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = "settings.gradle"));
}

void test_a_version_catalog_is_a_catalog(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_CATALOG, EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = "gradle/libs.versions.toml"));
}

void test_a_pom_is_maven(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_MAVEN, EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = "x/pom.xml"));
}

void test_a_sconscript_is_scons(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_SCONS, EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = "src/SConscript"));
}

void test_a_cargo_manifest_is_none_of_them(void)
{
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = "Cargo.toml"));
}

void test_a_value_completes_after_what_was_typed(void)
{
    const CliCompletions *const items = completed("maven.compiler.so");
    TEST_ASSERT_EQUAL_UINT(1u, items->count);
    assert_inserts("source", items, 0u);
}

void test_every_value_completes_when_nothing_is_typed(void)
{
    TEST_ASSERT_EQUAL_UINT(2u, completed("")->count);
}

void test_a_value_completes_after_a_dot(void)
{
    assert_inserts("compiler.source", completed("maven."), 0u);
}

void test_a_position_counts_its_column_in_utf16(void)
{
    static const char s_text[] = "ab\nx\xC3\xA9\xF0\x9F\x98\x80z\n";
    static unsigned int s_starts[8];
    CliBuildLines lines;
    EMBED_CALL(cli_builds_text_size.build_lines_get, CliBuildsTextSizeCfg, .text = s_text, .size = sizeof(s_text) - 1u, .starts = s_starts, .most = 8u, .lines_out = &lines);
    const char *const z = cellul.chr(&(CatenaFinitaCfg){.src = s_text, .cap = CLI_CELLS_POOL, .byte = 'z'});
    TEST_ASSERT_NOT_NULL(z);
    // `z` lies in the text, past its start
    const unsigned long long at = (unsigned long long)(z - s_text);
    const CliTextPlace place = EMBED_CALL(cli_builds_lines_at.build_place, CliBuildsLinesAtCfg, .lines = &lines, .at = at);
    TEST_ASSERT_EQUAL_UINT(1u, place.line);
    TEST_ASSERT_EQUAL_UINT(4u, place.col);
    TEST_ASSERT_EQUAL_UINT64(at, EMBED_CALL(cli_builds_path.build_byte_at, CliBuildsPathCfg, .text = s_text, .size = sizeof(s_text) - 1u, .line = 1u, .col = 4u));
}

void test_a_near_name_is_offered(void)
{
    char near[256];
    TEST_ASSERT_EQUAL_STRING("implementation", near_names("implmentation", 3u, near));
}

void test_a_far_name_is_not_offered(void)
{
    char near[256];
    TEST_ASSERT_EQUAL_STRING("", near_names("zzz", 1u, near));
}
