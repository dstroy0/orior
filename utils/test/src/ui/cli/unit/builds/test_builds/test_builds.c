// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_builds.c: build files: the type goes by file name, values complete after what was typed, positions count UTF-16 columns,
// and near names are offered
#include "ui/cli/src/builds/builds.h"
#include "ui/cli/src/command_line/pool.h"
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
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = &s_items, .token = typed, .token_size = CLI_UNITY_LENGTH(typed), .value = s_values[at], .value_size = CLI_UNITY_LENGTH(s_values[at]), .type = "property", .detail = "", .detail_size = 0ull);
    }
    return &s_items;
}

// the item `at` inserts `required`
static void assert_inserts(const char *required, const CliCompletions *items, unsigned int at)
{
    TEST_ASSERT_LESS_THAN_UINT(items->count, at);
    CLI_TEST_ASSERT_TEXT(required, items->text + items->item[at].insert_at, items->item[at].insert_size);
}

// the names near `name` among the first `count` known, joined by `|`, into `out`, 256 bytes
static const char *adjacent_names(const char *name, unsigned int count, char *out)
{
    CliBuildAdjacent adjacent;
    EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent, .name = name, .size = CLI_UNITY_LENGTH(name));
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent, .one = s_known[at], .size = CLI_UNITY_LENGTH(s_known[at]));
    }
    VerbaLinea line = {out, 256u, 0ull};
    for (unsigned int at = 0u; at < adjacent.count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put_n(&line, adjacent.adjacent[at], adjacent.adjacent_size[at]);
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
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_GRADLE, EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = "a/build.gradle.kts"));
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_GRADLE, EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = "settings.gradle"));
}

void test_a_version_catalog_is_a_catalog(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_CATALOG, EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = "gradle/libs.versions.toml"));
}

void test_a_pom_is_maven(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_MAVEN, EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = "x/pom.xml"));
}

void test_a_sconscript_is_scons(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_BUILD_SCONS, EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = "src/SConscript"));
}

void test_a_cargo_manifest_is_none_of_them(void)
{
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = "Cargo.toml"));
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
    EMBED_CALL(cli_builds_text_size.build_lines_get, CliBuildsTextSizeCfg, .text = s_text, .size = sizeof(s_text) - 1u, .starts = s_starts, .max = 8u, .lines_out = &lines);
    const char *const z = cellul.chr(&(CatenaFinitaCfg){.src = s_text, .cap = CLI_POOL_BYTES, .byte = 'z'});
    TEST_ASSERT_NOT_NULL(z);
    // `z` lies in the text, past its start
    const unsigned long long at = (unsigned long long)(z - s_text);
    const CliTextPosition place = EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = &lines, .at = at);
    TEST_ASSERT_EQUAL_UINT(1u, place.line);
    TEST_ASSERT_EQUAL_UINT(4u, place.col);
    TEST_ASSERT_EQUAL_UINT64(at, EMBED_CALL(cli_builds_path.build_byte_at, CliBuildsPathCfg, .text = s_text, .size = sizeof(s_text) - 1u, .line = 1u, .col = 4u));
}

void test_a_adjacent_name_is_offered(void)
{
    char adjacent[256];
    TEST_ASSERT_EQUAL_STRING("implementation", adjacent_names("implmentation", 3u, adjacent));
}

void test_a_far_name_is_not_offered(void)
{
    char adjacent[256];
    TEST_ASSERT_EQUAL_STRING("", adjacent_names("zzz", 1u, adjacent));
}

void test_names_are_measured_a_folder_placed_in_its_tree_and_a_snapshot_version_known(void)
{
    TEST_ASSERT_EQUAL_UINT64(3ull, EMBED_CALL(cli_builds_a.build_distance, CliBuildsACfg, .a = "kitten", .a_size = 6ull, .b = "sitting", .b_size = 7ull));
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_builds_a.build_distance, CliBuildsACfg, .a = "same", .a_size = 4ull, .b = "same", .b_size = 4ull));
    char inside[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = "D:/trees/root", .folder = "D:/trees/root\\sub\\deep", .out_capacity = inside);
    TEST_ASSERT_EQUAL_STRING("sub/deep", inside);
    EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = "D:/trees/root", .folder = "D:/trees/root", .out_capacity = inside);
    TEST_ASSERT_EQUAL_STRING("", inside);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg, .version = " 1.0-SNAPSHOT ", .size = 14ull));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg, .version = "1.0", .size = 3ull));
}

void test_a_words_characters_and_the_run_of_them_before_a_place(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_each_character.build_word_character, CliEachLetterCfg, .character = 'a'));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_each_character.build_word_character, CliEachLetterCfg, .character = '_'));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_each_character.build_word_character, CliEachLetterCfg, .character = '$'));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_each_character.build_word_character, CliEachLetterCfg, .character = '-'));
    // the run of word characters that ends at the bracket is `bar_9`, from byte 6; one that ends at byte 1 is `x`
    const char text[] = "x foo.bar_9(";
    TEST_ASSERT_EQUAL_UINT64(6ull, EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = text, .at = 11ull, .of = cli_build_word_character));
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = text, .at = 1ull, .of = cli_build_word_character));
}

void test_lines_are_indexed_in_a_cell_sized_to_them_and_a_lines_start_found(void)
{
    const char text[] = "ab\ncd\n\nef";
    CliBuildLines lines;
    unsigned int *const starts = EMBED_CALL(cli_builds_text_size.build_lines_alloc, CliBuildsTextSizeCfg, .text = text, .size = sizeof(text) - 1u, .lines_out = &lines);
    TEST_ASSERT_NOT_NULL(starts);
    TEST_ASSERT_EQUAL_UINT(4u, lines.count);
    TEST_ASSERT_EQUAL_UINT64(3ull, EMBED_CALL(cli_builds_line.build_line_start, CliBuildsLineCfg, .lines = &lines, .line = 1u));
    TEST_ASSERT_EQUAL_UINT64(6ull, EMBED_CALL(cli_builds_line.build_line_start, CliBuildsLineCfg, .lines = &lines, .line = 2u));
    TEST_ASSERT_EQUAL_UINT64(7ull, EMBED_CALL(cli_builds_line.build_line_start, CliBuildsLineCfg, .lines = &lines, .line = 3u));
    g_cli_guard->persistent_buf_release(starts, mmgr_owner_of_buf(cli_pool_series_cellularum(), starts));
}

void test_findings_and_their_fixes_are_written_as_the_protocols_diagnostics(void)
{
    static const char s_text[] = "let a = 1\nlet b = 2\n";
    static unsigned int s_starts[4];
    CliBuildLines lines;
    EMBED_CALL(cli_builds_text_size.build_lines_get, CliBuildsTextSizeCfg, .text = s_text, .size = sizeof(s_text) - 1u, .starts = s_starts, .max = 4u, .lines_out = &lines);
    CliBuildFindings found = {NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, 0};
    EMBED_CALL(cli_builds_lines.build_findings_begin, CliBuildsLinesCfg, .found = &found);
    TEST_ASSERT_NOT_NULL(found.finding);
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = &found);
    mmgr_line_put(&msg, "snapshot here");
    const unsigned int finding = EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = &found, .message = &msg, .lines = &lines, .from = 4ull, .to = 5ull, .severity = CLI_BUILD_NOTE, .code = "snapshot");
    TEST_ASSERT_EQUAL_UINT(0u, finding);
    VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = &found);
    mmgr_line_put(&title, "Fix it");
    EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = &found, .finding = finding, .title = &title, .lines = &lines, .text = "x", .size = 1ull, .from = 4ull, .to = 5ull);
    VerbaLinea run = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = &found);
    mmgr_line_put(&run, "Run it");
    EMBED_CALL(cli_builds_found.build_run_fix, CliBuildsFoundCfg, .found = &found, .finding = finding, .title = &run, .folder = "sub", .folder_size = 3ull, .line_text = "mvn -U", .line_size = 6ull);
    VerbaLinea twice = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = &found);
    mmgr_line_put(&twice, "Both");
    EMBED_CALL(cli_builds_text.build_edit_fix_twice, CliBuildsTextCfg, .found = &found, .finding = finding, .title = &twice, .lines = &lines, .text = "yy", .size = 2ull, .from = 4ull, .to = 5ull, .also_from = 14ull, .also_to = 15ull);
    TEST_ASSERT_EQUAL_UINT(3u, found.fixes);
    static char s_written[2048];
    VerbaLinea out = mmgr_line_over(s_written, sizeof(s_written));
    EMBED_CALL(cli_builds_table.build_findings_write, CliBuildsCfg, .found_read = &found, .out = &out);
    (void)mmgr_line_finish(&out);
    CLI_TEST_ASSERT_CONTAINS("[{\"range\":{\"start\":{\"line\":0,\"character\":4},\"end\":{\"line\":0,\"character\":5}},\"severity\":3,\"message\":{\"type\":\"markdown\",\"value\":\"snapshot here\"},\"source\":\"orior\",\"code\":\"snapshot\",\"data\":{\"orior\":[", s_written);
    CLI_TEST_ASSERT_CONTAINS("{\"title\":\"Fix it\",\"edits\":[{\"from\":{\"line\":0,\"col\":4},\"to\":{\"line\":0,\"col\":5},\"text\":\"x\"}]}", s_written);
    CLI_TEST_ASSERT_CONTAINS("{\"title\":\"Run it\",\"run\":{\"folder\":\"sub\",\"line\":\"mvn -U\"}}", s_written);
    CLI_TEST_ASSERT_CONTAINS("{\"title\":\"Both\",\"edits\":[{\"from\":{\"line\":0,\"col\":4},\"to\":{\"line\":0,\"col\":5},\"text\":\"yy\"},{\"from\":{\"line\":1,\"col\":4},\"to\":{\"line\":1,\"col\":5},\"text\":\"yy\"}]}", s_written);
    g_cli_guard->persistent_buf_release(found.finding, mmgr_owner_of_buf(cli_pool_series_cellularum(), found.finding));
}

void test_plain_items_show_without_their_brackets_and_insert_whole(void)
{
    static const char *const s_words[2] = {"get()", "set"};
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    EMBED_CALL(cli_builds_items_type.build_items, CliBuildsItemsTypeCfg, .items = &s_items, .type = "method", .detail = "d", .words = s_words, .count = 2u);
    TEST_ASSERT_EQUAL_UINT(2u, s_items.count);
    assert_inserts("get()", &s_items, 0u);
    assert_inserts("set", &s_items, 1u);
    CLI_TEST_ASSERT_TEXT("get", s_items.text + s_items.item[0].label_at, s_items.item[0].label_size);
}

void test_the_snapshots_line_is_mavens_at_a_pom_and_none_where_no_build_is(void)
{
    char root[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char line[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder("builds_snapshots", root);
    cli_unity_file(root, "pom.xml", "<project/>");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_builds_path_root.build_snapshots_line, CliBuildsPathRootCfg, .path = NULL, .root = root, .folder_out = folder, .line_capacity = line, .err_msg = &err_msg));
    CLI_TEST_ASSERT_CONTAINS(" -U dependency:resolve", line);
    TEST_ASSERT_EQUAL_STRING("", folder);
    cli_unity_folder("builds_no_snapshots", root);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_builds_path_root.build_snapshots_line, CliBuildsPathRootCfg, .path = NULL, .root = root, .folder_out = folder, .line_capacity = line, .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, err_msg.at);
}
