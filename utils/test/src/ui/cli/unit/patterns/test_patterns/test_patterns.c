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

// git run in `root` with its words, its lines sorted and joined by `|` into `out`, 1024 bytes
static void git_list(const char *root, const char *const *words, char *out)
{
    static char s_lines[64][CLI_PATH_MAX_LEN];
    CliRan *const ran = cli_ran_take();
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned int lines = 0u;
    if (EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = root, .settings = NULL, .count = 0u, .ran = ran, .err_msg = &err_msg))
    {
        for (unsigned long long at = 0ull; (at < ran->out_size) && (lines < 64u);)
        {
            const char *const end = memor.chr(&(MemoriaCfg){.src = ran->out + at, .val = '\n', .bytes = ran->out_size - at});
            // a line ends inside the output, at or past its start
            const unsigned long long length = (end != NULL) ? (unsigned long long)(end - (ran->out + at)) : ran->out_size - at;
            VerbaLinea line = {s_lines[lines], CLI_PATH_MAX_LEN, 0ull};
            mmgr_line_put_n(&line, ran->out + at, length);
            (void)mmgr_line_finish(&line);
            lines += (length > 0ull) ? 1u : 0u;
            at += length + 1ull;
        }
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    for (unsigned int at = 1u; at < lines; at += 1u)
    {
        char entry[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = entry, .src = s_lines[at], .bytes = CLI_UNITY_LENGTH(s_lines[at]) + 1ull});
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_lines[place - 1u], .other_pointer = entry) > 0))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_lines[place], .src = s_lines[place - 1u], .bytes = CLI_UNITY_LENGTH(s_lines[place - 1u]) + 1ull});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_lines[place], .src = entry, .bytes = CLI_UNITY_LENGTH(entry) + 1ull});
    }
    VerbaLinea joined = {out, 1024u, 0ull};
    for (unsigned int at = 0u; at < lines; at += 1u)
    {
        mmgr_line_put(&joined, (at > 0u) ? "|" : "");
        mmgr_line_put(&joined, s_lines[at]);
    }
    (void)mmgr_line_finish(&joined);
}

// in a repository of nine files, git picks the files the patterns of `count` lines leave
static void assert_git_agrees(const char *const *set, unsigned int count)
{
    static const char *const s_init[4] = {"git", "init", "-q", NULL};
    static const char *const s_all[9] = {"src/a.rs", "src/gen/b.rs", "docs/gen/x.md", "docs/guide.md", "target/debug/o.exe", "run.log", "deep/run.log", "README.md", "lib/vendor/a.min.js"};
    static const char *const s_sorted[9] = {"README.md", "deep/run.log", "docs/gen/x.md", "docs/guide.md", "lib/vendor/a.min.js", "run.log", "src/a.rs", "src/gen/b.rs", "target/debug/o.exe"};
    static const char *const s_head[5] = {"git", "ls-files", "--cached", "--others", "--"};
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char listed[1024];
    cli_unity_folder("patterns_git", root);
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = s_all[at], .joined = path);
        char *const slash = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path, .length_value = CLI_UNITY_LENGTH(path), .byte = '/');
        if (slash != NULL)
        {
            slash[0] = '\0';
            TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
            slash[0] = '/';
        }
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "x", .size = 1ull));
    }
    git_list(root, s_init, listed);
    const CliPatterns *const patterns = patterns_read(set, count);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = s_head[at]);
    }
    EMBED_CALL(cli_patterns.pathspecs, CliPatternsCfg, .patterns = patterns, .words = &s_words);
    s_words.word[s_words.count] = NULL;
    git_list(root, s_words.word, listed);
    char left[1024];
    VerbaLinea line = {left, sizeof(left), 0ull};
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        if (!EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = s_sorted[at], .dir = 0))
        {
            mmgr_line_put(&line, (line.at > 0ull) ? "|" : "");
            mmgr_line_put(&line, s_sorted[at]);
        }
    }
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING(left, listed);
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

void test_git_picks_the_files_hiding_patterns_leave(void)
{
    static const char *const s_set[3] = {"target", "*.log", "docs/gen/"};
    assert_git_agrees(s_set, 3u);
}

void test_git_picks_the_files_an_including_folder_leaves(void)
{
    static const char *const s_set[2] = {"!src/**", "src/gen"};
    assert_git_agrees(s_set, 2u);
}

void test_git_picks_the_files_an_including_glob_leaves(void)
{
    static const char *const s_set[1] = {"!*.md"};
    assert_git_agrees(s_set, 1u);
}

void test_git_picks_the_files_a_hiding_and_an_including_pattern_leave(void)
{
    static const char *const s_set[2] = {"**/*.min.js", "!lib/**"};
    assert_git_agrees(s_set, 2u);
}

void test_no_patterns_name_the_whole_tree(void)
{
    s_words.count = 0u;
    s_words.used = 0u;
    EMBED_CALL(cli_patterns.pathspecs, CliPatternsCfg, .patterns = patterns_read(NULL, 0u), .words = &s_words);
    TEST_ASSERT_EQUAL_UINT(1u, s_words.count);
    TEST_ASSERT_EQUAL_STRING(".", s_words.word[0]);
}

void test_the_pathspecs_name_what_the_patterns_name(void)
{
    static const char *const s_specs[3] = {"target", "!src/**", "gen/"};
    static const char *const s_wanted[5] = {":(glob" PATTERNS_CASE ")src/**", ":(glob" PATTERNS_CASE ")src/**/**", ":(exclude,glob" PATTERNS_CASE ")**/target", ":(exclude,glob" PATTERNS_CASE ")**/target/**", ":(exclude,glob" PATTERNS_CASE ")**/gen/**"};
    s_words.count = 0u;
    s_words.used = 0u;
    EMBED_CALL(cli_patterns.pathspecs, CliPatternsCfg, .patterns = patterns_read(s_specs, 3u), .words = &s_words);
    TEST_ASSERT_EQUAL_UINT(5u, s_words.count);
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        TEST_ASSERT_EQUAL_STRING(s_wanted[at], s_words.word[at]);
    }
}
