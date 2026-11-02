// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_learn.c: the learn's passes and the measuring stick: files are handed with forward slashes, the carrier's words, the steps'
// words are read, the part's files are beside the bridge, paths given to the vendor are native on Windows and on Linux, and the stick is
// compiled again only where its text changed
#include "ui/query/src/learn/learn_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char *const s_turns[3] = {"turns", "--base", "a.cu"};
static const char *const s_dry[2] = {"dry", "2"};
static const char *const s_clock[3] = {"clock", "0", "4000"};
static const char *const s_engine[2] = {"engine", "more"};

static QueryFiles s_files;
static QueryWords s_words;
static QueryLearnStep s_step;
static char s_joined[1024];
static QueryStickPaths s_paths;
static char s_error[512];
static char s_root[QUERY_PATH_MAX_LEN];
static char s_cu[QUERY_PATH_MAX_LEN];
static char s_fresh[QUERY_PATH_MAX_LEN];
static char s_cubin[QUERY_PATH_MAX_LEN];

// the words joined by spaces into s_joined
static const char *words_joined(void)
{
    VerbaLinea line = {s_joined, sizeof(s_joined), 0ull};
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? " " : "");
        mmgr_line_put(&line, s_words.word[at]);
    }
    (void)mmgr_line_finish(&line);
    return s_joined;
}

// the step's `count` words read into s_step: 1, or 0 where they are refused
static int step_read(const char *const *words, unsigned int count)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    memor.set(&(MemoriaCfg){.dst = &s_step, .val = 0, .bytes = sizeof(s_step)});
    return EMBED_CALL(query_learn_words_internal.step_read, QueryLearnWordsCtx, .words = words, .count = count, .step = &s_step, .err_msg = &err_msg);
}

// the tree's root in s_root with forward slashes, the test ignored where no tree is found
static void tree_required(void)
{
    char found[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = found))
    {
        TEST_IGNORE_MESSAGE("no tree is found");
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root, .src = found, .bytes = CLI_UNITY_LENGTH(found) + 1ull});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
}

// the stick's paths in s_paths
static void stick_paths_read(void)
{
    tree_required();
    QueryContext context;
    memor.set(&(MemoriaCfg){.dst = &context, .val = 0, .bytes = sizeof(context)});
    context.root = s_root;
    EMBED_CALL(query_loops_context.stick_paths, QueryLoopsContextCfg, .context_read = &context, .paths = &s_paths);
}

// the stick's text and its new text the same, and a compiled container present
static void stick_files_written(void)
{
    char out[QUERY_PATH_MAX_LEN];
    tree_required();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/stick_text", .joined = out);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = out);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = out));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = out, .relative = "measuring_stick.cu", .joined = s_cu);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = out, .relative = "measuring_stick.cu.new", .joined = s_fresh);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = out, .relative = "measuring_stick_nvcc.cubin", .joined = s_cubin);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_cu, .bytes = "a", .size = 1ull));
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_fresh, .bytes = "a", .size = 1ull));
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_cubin, .bytes = "", .size = 0ull));
}

// the stick words' answer for `count` words
static int stick_words(const char *const *words, unsigned int count)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    return EMBED_CALL(query_loops_err_msg_words.stick_words, QueryLoopsErrMsgWordsCfg, .words = words, .count = count, .err_msg = &err_msg);
}

void setUp(void)
{
    cli_unity_begun();
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
}

void tearDown(void)
{
}

void test_the_files_are_transferred_with_forward_slashes(void)
{
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = "D:/top/tree", .part = "sm_86", .files = &s_files);
    const unsigned long long length = CLI_UNITY_LENGTH(s_files.costs);
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = s_files.costs, .cap = CLI_CELLS_POOL, .byte = '\\'}));
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = s_files.layout, .cap = CLI_CELLS_POOL, .byte = '\\'}));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(16ull, length);
    TEST_ASSERT_EQUAL_STRING("/table/sm_86.ksc", s_files.costs + length - 16u);
}

void test_the_parts_files_are_adjacent_the_bridge(void)
{
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = "T", .part = "sm_86", .files = &s_files);
    TEST_ASSERT_EQUAL_STRING("T/" QUERY_TABLE "/sm_86.khw", s_files.machine);
    TEST_ASSERT_EQUAL_STRING("T/" QUERY_TABLE "/sm_86.kqr", s_files.record);
    TEST_ASSERT_EQUAL_STRING("T/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_nvidia.tsv", s_files.fields);
}

void test_the_transports_words_with_the_lessons(void)
{
    EMBED_CALL(query_loops_words_words_transport.transport_words, QueryLoopsWordsWordsTransportCfg, .words_words_pointer = &s_words, .transport = "C", .cross = "X", .nvdisasm = "N", .machine = "M", .layout = "L", .lessons = 1);
    TEST_ASSERT_EQUAL_STRING("C --gated gated.txt --lessons overseer.txt M L --pop C --dry --full --lessons overseer.txt M L --cross X N SM86 airlock.md gated.txt", words_joined());
}

void test_the_transports_words_without_the_lessons(void)
{
    EMBED_CALL(query_loops_words_words_transport.transport_words, QueryLoopsWordsWordsTransportCfg, .words_words_pointer = &s_words, .transport = "C", .cross = "X", .nvdisasm = "N", .machine = "M", .layout = "L", .lessons = 0);
    TEST_ASSERT_EQUAL_STRING("C --gated gated.txt M L --pop C --dry --full M L --cross X N SM86 airlock.md gated.txt", words_joined());
}

void test_a_pass_is_read(void)
{
    TEST_ASSERT_TRUE(step_read(s_turns, 1u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_PASS, s_step.kind);
    TEST_ASSERT_EQUAL_STRING("turns", s_step.word);
    TEST_ASSERT_NULL(s_step.base);
}

void test_a_pass_with_its_base_is_read(void)
{
    TEST_ASSERT_TRUE(step_read(s_turns, 3u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_PASS, s_step.kind);
    TEST_ASSERT_EQUAL_STRING("a.cu", s_step.base);
}

void test_the_costs_pass_is_read(void)
{
    static const char *const s_costs[1] = {"costs"};
    TEST_ASSERT_TRUE(step_read(s_costs, 1u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_PASS, s_step.kind);
    TEST_ASSERT_EQUAL_STRING("costs", s_step.word);
}

void test_a_seeded_discover_is_read(void)
{
    static const char *const s_seed[2] = {"seed", "listing.sass"};
    TEST_ASSERT_TRUE(step_read(s_seed, 2u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_SEED, s_step.kind);
    TEST_ASSERT_EQUAL_STRING("listing.sass", s_step.word);
}

void test_rounds_through_the_airlock_are_read(void)
{
    static const char *const s_rounds[2] = {"rounds", "3"};
    TEST_ASSERT_TRUE(step_read(s_rounds, 2u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_ROUNDS, s_step.kind);
    TEST_ASSERT_EQUAL_UINT(3u, s_step.rounds);
}

void test_dry_with_no_count_is_one_round(void)
{
    TEST_ASSERT_TRUE(step_read(s_dry, 1u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_DRY, s_step.kind);
    TEST_ASSERT_EQUAL_UINT(1u, s_step.rounds);
}

void test_dry_reads_its_count_of_rounds(void)
{
    TEST_ASSERT_TRUE(step_read(s_dry, 2u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_DRY, s_step.kind);
    TEST_ASSERT_EQUAL_UINT(2u, s_step.rounds);
}

void test_the_clock_with_no_span_uses_its_default(void)
{
    TEST_ASSERT_TRUE(step_read(s_clock, 1u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_CLOCK, s_step.kind);
    TEST_ASSERT_EQUAL_STRING(QUERY_CLOCK_FROM, s_step.word);
    TEST_ASSERT_EQUAL_STRING(QUERY_CLOCK_WORDS, s_step.base);
}

void test_the_clock_reads_a_input_span(void)
{
    TEST_ASSERT_TRUE(step_read(s_clock, 3u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_CLOCK, s_step.kind);
    TEST_ASSERT_EQUAL_STRING("0", s_step.word);
    TEST_ASSERT_EQUAL_STRING("4000", s_step.base);
}

void test_one_positions_questions_are_read(void)
{
    static const char *const s_enumerate[2] = {"enumerate", "loop_back_if"};
    TEST_ASSERT_TRUE(step_read(s_enumerate, 2u));
    TEST_ASSERT_EQUAL_INT(QUERY_LEARN_ENUMERATE, s_step.kind);
    TEST_ASSERT_EQUAL_STRING("loop_back_if", s_step.word);
}

void test_an_enumerate_with_no_position_is_refused(void)
{
    static const char *const s_enumerate[1] = {"enumerate"};
    TEST_ASSERT_FALSE(step_read(s_enumerate, 1u));
}

void test_a_seed_with_no_listing_is_refused(void)
{
    static const char *const s_seed[1] = {"seed"};
    TEST_ASSERT_FALSE(step_read(s_seed, 1u));
}

void test_rounds_that_are_not_a_number_are_refused(void)
{
    static const char *const s_many[2] = {"rounds", "many"};
    TEST_ASSERT_FALSE(step_read(s_many, 2u));
}

void test_an_unknown_step_is_refused(void)
{
    static const char *const s_fly[1] = {"fly"};
    TEST_ASSERT_FALSE(step_read(s_fly, 1u));
}

void test_no_words_are_refused(void)
{
    TEST_ASSERT_FALSE(step_read(NULL, 0u));
}

void test_the_sticks_text_is_found_by_its_name(void)
{
    stick_paths_read();
    const unsigned long long length = CLI_UNITY_LENGTH(s_paths.cu);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(18ull, length);
    TEST_ASSERT_EQUAL_STRING("measuring_stick.cu", s_paths.cu + length - 18u);
}

void test_the_sticks_path_is_not_a_cygwin_path_and_uses_forward_slashes(void)
{
    stick_paths_read();
    CLI_TEST_ASSERT_NOT_CONTAINS("\\\\?\\", s_paths.cu);
    TEST_ASSERT_FALSE(cellul.starts(&(CatenaFinitaCfg){.src = s_paths.cu, .other = "/c/", .cap = CLI_CELLS_POOL}));
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = s_paths.cu, .cap = CLI_CELLS_POOL, .byte = '\\'}));
}

void test_windows_the_sticks_path_has_its_drive(void)
{
#if defined(_WIN32)
    stick_paths_read();
    TEST_ASSERT_EQUAL_CHAR(':', s_paths.cu[1]);
#else
    TEST_IGNORE_MESSAGE("a Windows drive");
#endif
}

void test_posix_the_sticks_path_starts_at_the_root(void)
{
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("a POSIX path");
#else
    stick_paths_read();
    TEST_ASSERT_EQUAL_CHAR('/', s_paths.cu[0]);
#endif
}

void test_the_same_text_is_not_compiled_again(void)
{
    stick_files_written();
    TEST_ASSERT_FALSE(EMBED_CALL(query_loops_cu.stick_change, QueryLoopsCuCfg, .cu = s_cu, .fresh = s_fresh, .cubin = s_cubin));
}

void test_a_changed_text_is_compiled_again(void)
{
    stick_files_written();
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_fresh, .bytes = "b", .size = 1ull));
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_cu.stick_change, QueryLoopsCuCfg, .cu = s_cu, .fresh = s_fresh, .cubin = s_cubin));
}

void test_a_missing_container_is_compiled(void)
{
    stick_files_written();
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = s_cubin);
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_cu.stick_change, QueryLoopsCuCfg, .cu = s_cu, .fresh = s_fresh, .cubin = s_cubin));
}

void test_the_stick_with_no_words_reads_the_vendors_read_state(void)
{
    TEST_ASSERT_EQUAL_INT(QUERY_STICK_VENDOR, stick_words(NULL, 0u));
}

void test_the_stick_reads_vendor(void)
{
    static const char *const s_vendor[1] = {"vendor"};
    TEST_ASSERT_EQUAL_INT(QUERY_STICK_VENDOR, stick_words(s_vendor, 1u));
}

void test_the_stick_reads_engine(void)
{
    TEST_ASSERT_EQUAL_INT(QUERY_STICK_ENGINE, stick_words(s_engine, 1u));
}

void test_the_stick_reads_read_with_its_words(void)
{
    static const char *const s_read[3] = {"read", "a.sass", "b.tsv"};
    TEST_ASSERT_EQUAL_INT(QUERY_STICK_READ, stick_words(s_read, 3u));
}

void test_the_stick_refuses_engine_with_more_words(void)
{
    TEST_ASSERT_LESS_THAN_INT(0, stick_words(s_engine, 2u));
}

void test_the_stick_refuses_an_unknown_word(void)
{
    static const char *const s_ruler[1] = {"ruler"};
    TEST_ASSERT_LESS_THAN_INT(0, stick_words(s_ruler, 1u));
}
