// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_patterns.c: a pattern hides what it names and what is in it, a pattern that includes hides every other file, a set names its files
// and removes some, git picks the files the patterns leave, and the pathspecs name what the patterns name, ignoring case on Windows
#include "ui/cli/src/text/patterns.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the pathspec magic of the platform: Windows' file names ignore case
#if defined(_WIN32)
#define PATTERNS_CASE ",icase"
#else
#define PATTERNS_CASE ""
#endif

static const char *const s_hiding_lines[7] = {"target", "*.log", "docs/gen/", "# a note", "", "src/**/*.min.js", "?.tmp"};
static const char *const s_including_lines[3] = {"!src/**", "!*.md", "src/**/*.o"};

static CliPatterns s_patterns;
static CliWords s_words;

// the patterns of `count` lines read into s_patterns
static const CliPatterns *patterns_read(const char *const *lines, unsigned int count)
{
    EMBED_CALL(cli_patterns_lines.read, CliPatternsLinesCfg, .lines = lines, .count = count, .patterns_out = &s_patterns);
    return &s_patterns;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_name_hides_itself_and_what_is_in_it_at_any_depth(void)
{
    const CliPatterns *const patterns = patterns_read(s_hiding_lines, 7u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "target", .dir = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "src/ui/target/debug/orior.exe", .dir = 0));
}

void test_a_glob_hides_the_names_it_matches(void)
{
    const CliPatterns *const patterns = patterns_read(s_hiding_lines, 7u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "run.log", .dir = 0));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "deep/in/it/run.log", .dir = 0));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "run.log.txt", .dir = 0));
}

void test_a_folders_path_hides_that_folder_only(void)
{
    const CliPatterns *const patterns = patterns_read(s_hiding_lines, 7u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "docs/gen", .dir = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "docs/gen/page.html", .dir = 0));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "docs/gen", .dir = 0));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "other/docs/gen/page.html", .dir = 0));
}

void test_a_double_star_matches_any_depth(void)
{
    const CliPatterns *const patterns = patterns_read(s_hiding_lines, 7u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "src/a.min.js", .dir = 0));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "src/ui/vendor/a.min.js", .dir = 0));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "lib/a.min.js", .dir = 0));
}

void test_a_question_mark_matches_one_letter(void)
{
    const CliPatterns *const patterns = patterns_read(s_hiding_lines, 7u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "x.tmp", .dir = 0));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "xy.tmp", .dir = 0));
}

void test_a_comment_hides_nothing(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns_read(s_hiding_lines, 7u), .path = "note", .dir = 0));
}

void test_a_bang_pattern_includes_only_what_it_names(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns.include_only, CliPatternsCfg, .patterns = patterns_read(s_including_lines, 3u)));
}

void test_what_a_bang_pattern_names_stays(void)
{
    const CliPatterns *const patterns = patterns_read(s_including_lines, 3u);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "src/ui/main.rs", .dir = 0));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "README.md", .dir = 0));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "docs/guide.md", .dir = 0));
}

void test_every_other_file_and_what_a_pattern_hides_among_the_included_are_hidden(void)
{
    const CliPatterns *const patterns = patterns_read(s_including_lines, 3u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "Cargo.toml", .dir = 0));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = "src/ui/main.o", .dir = 0));
}

void test_a_folder_is_hidden_only_where_a_pattern_names_it(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns_read(s_including_lines, 3u), .path = "docs", .dir = 1));
}

void test_an_included_folder_is_known(void)
{
    const CliPatterns *const patterns = patterns_read(s_including_lines, 3u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.include_folder, CliPatternsPathCfg, .patterns = patterns, .path = "src/ui"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.include_folder, CliPatternsPathCfg, .patterns = patterns, .path = "docs"));
}

void test_an_included_folder_includes_what_is_in_it(void)
{
    static const char *const s_src[1] = {"!src"};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns_read(s_src, 1u), .path = "src/a.rs", .dir = 0));
}

void test_a_set_names_its_files_and_removes_some(void)
{
    static const char *const s_set[4] = {"src/**", "!src/gen/**", "", "# a note"};
    EMBED_CALL(cli_patterns_lines.read_set, CliPatternsLinesCfg, .lines = s_set, .count = 4u, .patterns_out = &s_patterns);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = &s_patterns, .path = "src/a.rs", .dir = 0));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = &s_patterns, .path = "src/gen/b.rs", .dir = 0));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = &s_patterns, .path = "docs/x.md", .dir = 0));
}

void test_a_set_of_none_is_empty(void)
{
    EMBED_CALL(cli_patterns_lines.read_set, CliPatternsLinesCfg, .lines = NULL, .count = 0u, .patterns_out = &s_patterns);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_patterns.empty, CliPatternsCfg, .patterns = &s_patterns));
}

void test_no_patterns_name_the_complete_tree(void)
{
    s_words.count = 0u;
    s_words.used = 0u;
    EMBED_CALL(cli_patterns.pathspecs, CliPatternsCfg, .patterns = patterns_read(NULL, 0u), .words = &s_words);
    TEST_ASSERT_EQUAL_UINT(1u, s_words.count);
    TEST_ASSERT_EQUAL_STRING(".", s_words.word_text[0]);
}

void test_the_pathspecs_name_what_the_patterns_name(void)
{
    static const char *const s_specs[3] = {"target", "!src/**", "gen/"};
    static const char *const s_requested[5] = {":(glob" PATTERNS_CASE ")src/**", ":(glob" PATTERNS_CASE ")src/**/**", ":(exclude,glob" PATTERNS_CASE ")**/target", ":(exclude,glob" PATTERNS_CASE ")**/target/**", ":(exclude,glob" PATTERNS_CASE ")**/gen/**"};
    s_words.count = 0u;
    s_words.used = 0u;
    EMBED_CALL(cli_patterns.pathspecs, CliPatternsCfg, .patterns = patterns_read(s_specs, 3u), .words = &s_words);
    TEST_ASSERT_EQUAL_UINT(5u, s_words.count);
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        TEST_ASSERT_EQUAL_STRING(s_requested[at], s_words.word_text[at]);
    }
}
