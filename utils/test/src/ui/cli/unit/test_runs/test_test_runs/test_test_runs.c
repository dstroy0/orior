// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_test_runs.c: a run is spread by whole files, the largest first; verbose reports are read; a report records each failure's message and line; a
// run reports each result, its failures and its coverage; and unittest's failures are read
#include "ui/cli/src/tests/test_runs.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/tests/test_runs_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_xml[] = "<testsuites><testsuite><testcase classname=\"tests.test_a\" name=\"test_one\" file=\"tests/test_a.py\" "
                            "line=\"3\" time=\"0.002\"><failure message=\"assert 1 == 2\">tests/test_a.py:5: in test_one\n    helper()\n"
                            "tests/test_a.py:9: in helper\n    assert 1 == 2\nE   assert 1 == 2</failure></testcase><testcase "
                            "classname=\"tests.test_a.TestX\" name=\"test_two\" file=\"tests/test_a.py\" line=\"12\" time=\"0.1\" />"
                            "<testcase classname=\"tests.test_a.TestX\" name=\"test_skip\" file=\"tests/test_a.py\" line=\"14\" "
                            "time=\"0\"><skipped type=\"pytest.skip\" message=\"not here\">skip</skipped></testcase></testsuite>"
                            "</testsuites>";

static CliTestRun s_run = {.lock = CLI_LOCK_INIT, .wake = CLI_WAKE_INIT};
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static char s_events[1u << 20u];
static unsigned long long s_events_at;
static char s_seen[1u << 20u];
static const CliJson *s_json;
static int s_python_ran;
static int s_python_usable;
static int s_python_started;

// what a run reports recorded, a line each
static void event_recorded(const CliEachJsonSizeCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (s_events_at + args->size + 1ull < sizeof(s_events))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_events + s_events_at, .src = args->json, .bytes = args->size});
        s_events[s_events_at + args->size] = '\n';
        s_events_at += args->size + 1ull;
    }
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// the first line reported of `kind`, and of the result `id` where it is not NULL, after the `from`th line: its node in the shared tree, CLI_JSON_NONE
// where none is; how many lines there are into `lines`
static unsigned int line_found(const char *kind, const char *id, unsigned int from, unsigned int *lines)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned long long size = s_events_at;
    memor.cpy(&(MemoriaCfg){.dst = s_seen, .src = s_events, .bytes = size});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    unsigned int index = 0u;
    unsigned int found = CLI_JSON_NONE;
    for (unsigned long long at = 0ull; at < size; index += 1u)
    {
        const char *const end = memor.chr(&(MemoriaCfg){.src = s_seen + at, .val = '\n', .bytes = size - at});
        // a line ends inside what was seen, past its start
        const unsigned long long length = (unsigned long long)(end - (s_seen + at));
        if ((found == CLI_JSON_NONE) && (index >= from))
        {
            unsigned int one = CLI_JSON_NONE;
            const CliJson *const read = cli_unity_parsed(s_seen + at, length, &one);
            const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = read, .object = one, .name = "outcome");
            const int kind_matches = cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(one, "kind"), .other = kind, .cap = CLI_CELLS_POOL});
            const int id_matches = (id == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(outcome, "id"), .other = id, .cap = CLI_CELLS_POOL});
            if (kind_matches && id_matches)
            {
                s_json = read;
                found = one;
            }
        }
        at += length + 1ull;
    }
    *lines = index;
    return found;
}

// a whole number member of a node of the shared tree, -1 where it is none
static long long whole_member(unsigned int node, const char *name)
{
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = node, .name = name);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = number, .kind = CFG_JSON_NUMBER))
    {
        return -1ll;
    }
    long long value = 0ll;
    for (unsigned long long at = 0ull; at < s_json->node[number].length; at += 1ull)
    {
        // a digit's value is below 10
        value = value * 10ll + (long long)(s_json->text[s_json->node[number].text + at] - '0');
    }
    return value;
}

// whether a pytest verbose line reads as `id` with `outcome`, or, where `id` is NULL, as none
static int pytest_line_is(const char *line, const char *id, const char *outcome)
{
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
    const char *read = NULL;
    if (!EMBED_CALL(cli_test_runs_size_internal.test_pytest_line, CliTestRunsSizeCtx, .line = line, .size = CLI_UNITY_LENGTH(line), .id_u64s = &at, .id_size = &size, .outcome_pointer = &read))
    {
        return id == NULL;
    }
    return (id != NULL) && (size == CLI_UNITY_LENGTH(id)) && (memor.cmp(&(MemoriaCfg){.src = line + at, .other = id, .bytes = size}) == 0) && cellul.eq(&(CatenaFinitaCfg){.src = read, .other = outcome, .cap = CLI_CELLS_POOL});
}

// whether a unittest verbose line reads as `id` with `outcome`
static int unittest_line_is(const char *line, const char *id, const char *outcome)
{
    char read_id[CLI_PATH_MAX_LEN];
    const char *read = NULL;
    return EMBED_CALL(cli_test_runs_size_line_internal.test_unittest_line, CliTestRunsSizeLineCtx, .line = line, .size = CLI_UNITY_LENGTH(line), .id_out = read_id, .room = sizeof(read_id), .outcome_pointer = &read) && cellul.eq(&(CatenaFinitaCfg){.src = read_id, .other = id, .cap = CLI_CELLS_POOL}) && cellul.eq(&(CatenaFinitaCfg){.src = read, .other = outcome, .cap = CLI_CELLS_POOL});
}

// a worker of the run set up to report into the recorded events, empty
static void report_ready(void)
{
    s_run.sink = (CliJsonSink){event_recorded, NULL};
    memor.cpy(&(MemoriaCfg){.dst = s_run.root, .src = "D:/t", .bytes = 5ull});
    s_run.workers[0].run = &s_run;
    s_events_at = 0ull;
}

// a real run of three test files under pytest with coverage, once for the process
static void python_ran(void)
{
    static const char *const s_files[3][2] = {
        {"shapes.py", "def area(width, height):\n    return width * height\n\n\ndef unused():\n    return 0\n"},
        {"tests/test_a.py", "import sys, os\nsys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath("
                            "__file__))))\nfrom shapes import area\n\n\ndef test_area():\n    assert area(2, 3) == 6\n\n\n"
                            "def test_wrong():\n    assert area(2, 3) == 7\n"},
        {"tests/test_b.py", "import pytest\n\n\nclass TestB:\n    def test_skipped(self):\n        pytest.skip(\"not here\")\n"}};
    static const char *const s_paths[2] = {"tests/test_a.py", "tests/test_b.py"};
    char python[CLI_PATH_MAX_LEN];
    char dir[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char why[512];
    if (s_python_ran)
    {
        return;
    }
    s_python_ran = 1;
    cli_unity_folder("test_runs", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "tests", .joined = path);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
    s_python_usable = EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python) && EMBED_CALL(cli_test_runs_python_internal.test_has_pytest, CliTestRunsPythonCtx, .python = python, .root = dir);
    if (!s_python_usable)
    {
        return;
    }
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = s_files[at][0], .joined = path);
        (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_files[at][1], .size = CLI_UNITY_LENGTH(s_files[at][1]));
    }
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long run = 0ull;
    s_events_at = 0ull;
    s_python_started = EMBED_CALL(cli_test_runs_root.start, CliTestRunsRootCfg, .root = dir, .given = s_paths, .count = 2u, .parallel = 1, .cover = 1, .sink = (CliJsonSink){event_recorded, NULL}, .number = &run, .err_msg = &err_msg);
    unsigned int lines = 0u;
    const unsigned long long until = cli_clock_milliseconds() + 120000ull;
    while (s_python_started && (line_found("done", NULL, 0u, &lines) == CLI_JSON_NONE) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
        (void)EMBED_CALL(cli_lock_internal.wake_wait_for, CliLockCtx, .wake = &s_wake, .lock = &s_lock, .milliseconds_u64 = 100ull);
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    }
}

// the real run made, the case ignored on a machine with no Python with pytest
static void python_required(void)
{
    python_ran();
    if (!s_python_usable)
    {
        TEST_IGNORE_MESSAGE("no Python with pytest on this machine");
    }
    TEST_ASSERT_TRUE(s_python_started);
}

// whether the number `digit` is among the numbers of the array
static int numbers_contain(unsigned int array, char digit)
{
    int seen = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = array, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int number = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = array, .index = at);
        seen |= (s_json->node[number].length == 1u) && (s_json->text[s_json->node[number].text] == digit);
    }
    return seen;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_run_is_spread_by_whole_files_the_largest_first(void)
{
    static const char *const s_paths[6] = {"a.py::t1", "a.py::t2", "a.py::t3", "b.py::t1", "c.py::t1", "c.py::t2"};
    static const char *const s_wanted[2][3] = {{"a.py::t1", "a.py::t2", "a.py::t3"}, {"c.py::t1", "c.py::t2", "b.py::t1"}};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        s_run.given[at] = s_paths[at];
    }
    EMBED_CALL(cli_test_runs_run_count_internal.test_shares, CliTestRunsRunCountCtx, .run = &s_run, .count = 6u, .most = 2u);
    TEST_ASSERT_EQUAL_UINT(2u, s_run.shares);
    for (unsigned int share = 0u; share < 2u; share += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(3u, s_run.share_size[share]);
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            TEST_ASSERT_EQUAL_STRING(s_wanted[share][at], s_run.given[s_run.order[s_run.share_from[share] + at]]);
        }
    }
}

void test_a_run_has_no_more_shares_than_files(void)
{
    static const char *const s_paths[5] = {"a.py::t1", "a.py::t2", "b.py::t1", "c.py::t1", "c.py::t2"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        s_run.given[at] = s_paths[at];
    }
    EMBED_CALL(cli_test_runs_run_count_internal.test_shares, CliTestRunsRunCountCtx, .run = &s_run, .count = 5u, .most = 8u);
    TEST_ASSERT_EQUAL_UINT(3u, s_run.shares);
}

void test_a_run_has_one_share_where_one_is_requested(void)
{
    static const char *const s_paths[5] = {"a.py::t1", "a.py::t2", "b.py::t1", "c.py::t1", "c.py::t2"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        s_run.given[at] = s_paths[at];
    }
    EMBED_CALL(cli_test_runs_run_count_internal.test_shares, CliTestRunsRunCountCtx, .run = &s_run, .count = 5u, .most = 1u);
    TEST_ASSERT_EQUAL_UINT(1u, s_run.shares);
}

void test_pytests_verbose_line_is_read(void)
{
    TEST_ASSERT_TRUE(pytest_line_is("tests/test_a.py::TestX::test_two PASSED                [ 50%]", "tests/test_a.py::TestX::test_two", "passed"));
}

void test_pytests_verbose_line_is_read_with_a_tests_parameters(void)
{
    TEST_ASSERT_TRUE(pytest_line_is("tests/test_a.py::test_one[1-2] FAILED", "tests/test_a.py::test_one[1-2]", "failed"));
}

void test_pytests_summary_is_not_a_result(void)
{
    TEST_ASSERT_TRUE(pytest_line_is("FAILED tests/test_a.py::test_one - assert 1 == 2", NULL, NULL));
}

void test_unittests_verbose_line_is_read(void)
{
    TEST_ASSERT_TRUE(unittest_line_is("test_two (tests.test_a.Checks.test_two) ... ok", "tests/test_a.py::Checks::test_two", "passed"));
}

void test_unittests_verbose_line_reads_a_skipped_test(void)
{
    TEST_ASSERT_TRUE(unittest_line_is("test_three (pkg.test_b.Checks.test_three) ... skipped 'later'", "pkg/test_b.py::Checks::test_three", "skipped"));
}

void test_a_test_is_named_as_unittest_accepts_it(void)
{
    static CliWords s_words;
    EMBED_CALL(cli_test_runs_internal.test_dot, CliTestRunsCtx, .words = &s_words, .id = "pkg/test_b.py::Checks::test_three");
    TEST_ASSERT_EQUAL_UINT(1u, s_words.count);
    TEST_ASSERT_EQUAL_STRING("pkg.test_b.Checks.test_three", s_words.word[0]);
}

void test_a_failure_has_its_message_and_the_line_it_failed_at(void)
{
    unsigned int lines = 0u;
    report_ready();
    EMBED_CALL(cli_test_runs_worker_internal.test_report_emit, CliTestRunsWorkerCtx, .worker = &s_run.workers[0], .xml = s_xml, .size = sizeof(s_xml) - 1u);
    const unsigned int one = line_found("result", "tests/test_a.py::test_one", 0u, &lines);
    const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = one, .name = "outcome");
    TEST_ASSERT_EQUAL_UINT(3u, lines);
    TEST_ASSERT_EQUAL_STRING("assert 1 == 2", cli_unity_member_text(outcome, "message"));
    TEST_ASSERT_EQUAL_INT64(8ll, whole_member(outcome, "line"));
}

void test_a_test_that_passed_has_its_time(void)
{
    char seconds[32];
    unsigned int lines = 0u;
    report_ready();
    EMBED_CALL(cli_test_runs_worker_internal.test_report_emit, CliTestRunsWorkerCtx, .worker = &s_run.workers[0], .xml = s_xml, .size = sizeof(s_xml) - 1u);
    const unsigned int one = line_found("result", "tests/test_a.py::TestX::test_two", 0u, &lines);
    const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = one, .name = "outcome");
    VerbaLinea line = {seconds, sizeof(seconds), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = outcome, .name = "seconds"), .out = &line, .indent = 0u, .sorted = 0);
    TEST_ASSERT_EQUAL_STRING("passed", cli_unity_member_text(outcome, "outcome"));
    CLI_TEST_ASSERT_TEXT("0.1", seconds, line.at);
}

void test_a_test_skipped_has_its_line(void)
{
    unsigned int lines = 0u;
    report_ready();
    EMBED_CALL(cli_test_runs_worker_internal.test_report_emit, CliTestRunsWorkerCtx, .worker = &s_run.workers[0], .xml = s_xml, .size = sizeof(s_xml) - 1u);
    const unsigned int one = line_found("result", "tests/test_a.py::TestX::test_skip", 0u, &lines);
    const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = one, .name = "outcome");
    TEST_ASSERT_EQUAL_STRING("skipped", cli_unity_member_text(outcome, "outcome"));
    TEST_ASSERT_EQUAL_INT64(14ll, whole_member(outcome, "line"));
}

void test_a_reports_tags_and_texts_are_in_cells_sized_to_it_and_released(void)
{
    report_ready();
    EMBED_CALL(cli_test_runs_worker_internal.test_report_emit, CliTestRunsWorkerCtx, .worker = &s_run.workers[0], .xml = s_xml, .size = sizeof(s_xml) - 1u);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_test_report_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_test_report_cells.hw));
    TEST_ASSERT_LESS_THAN_UINT64(sizeof(s_xml) * 4u, embed_atomic_usize_load(&g_cli_test_report_cells.hw));
}

void test_unittests_failures_are_read(void)
{
    static const char s_failures[] = "test_two (tests.test_a.Checks.test_two) ... FAIL\n\n"
                                     "======================================================================\n"
                                     "FAIL: test_two (tests.test_a.Checks.test_two)\n"
                                     "----------------------------------------------------------------------\n"
                                     "Traceback (most recent call last):\n  File \"D:\\t\\tests\\test_a.py\", line 7, in test_two\n"
                                     "    self.assertEqual(1, 2)\nAssertionError: 1 != 2\n\n"
                                     "----------------------------------------------------------------------\nRan 1 test in 0.001s\n";
    unsigned int lines = 0u;
    report_ready();
    EMBED_CALL(cli_test_runs_text_worker_internal.test_unittest_failures_emit, CliTestRunsTextWorkerCtx, .worker = &s_run.workers[0], .text = s_failures, .size = sizeof(s_failures) - 1u);
    const unsigned int one = line_found("result", "tests/test_a.py::Checks::test_two", 0u, &lines);
    TEST_ASSERT_EQUAL_UINT(1u, lines);
    TEST_ASSERT_EQUAL_INT64(6ll, whole_member(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = one, .name = "outcome"), "line"));
}

void test_two_files_use_two_processes(void)
{
    unsigned int lines = 0u;
    python_required();
    TEST_ASSERT_EQUAL_INT64(2ll, whole_member(line_found("started", NULL, 0u, &lines), "workers"));
}

void test_a_runs_end_has_its_counts(void)
{
    unsigned int lines = 0u;
    python_required();
    const unsigned int found = line_found("done", NULL, 0u, &lines);
    TEST_ASSERT_EQUAL_INT64(1ll, whole_member(found, "passed"));
    TEST_ASSERT_EQUAL_INT64(1ll, whole_member(found, "failed"));
    TEST_ASSERT_EQUAL_INT64(1ll, whole_member(found, "skipped"));
}

void test_a_real_failure_has_its_message_and_line(void)
{
    unsigned int lines = 0u;
    unsigned int wrong = CLI_JSON_NONE;
    python_required();
    (void)line_found("done", NULL, 0u, &lines);
    for (unsigned int from = 0u; from < lines; from += 1u)
    {
        const unsigned int found = line_found("result", "tests/test_a.py::test_wrong", from, &lines);
        const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "outcome");
        if ((found != CLI_JSON_NONE) && (cli_unity_member_text(outcome, "message")[0] != '\0'))
        {
            wrong = outcome;
            break;
        }
    }
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, wrong);
    TEST_ASSERT_EQUAL_STRING("failed", cli_unity_member_text(wrong, "outcome"));
    TEST_ASSERT_EQUAL_INT64(10ll, whole_member(wrong, "line"));
    CLI_TEST_ASSERT_CONTAINS("assert", cli_unity_member_text(wrong, "message"));
}

void test_a_real_skipped_test_is_reported_skipped(void)
{
    unsigned int lines = 0u;
    python_required();
    const unsigned int found = line_found("result", "tests/test_b.py::TestB::test_skipped", 0u, &lines);
    TEST_ASSERT_EQUAL_STRING("skipped", cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "outcome"), "outcome"));
}

void test_coverage_reports_a_files_lines_ran_and_missed(void)
{
    unsigned int lines = 0u;
    unsigned int shapes = CLI_JSON_NONE;
    python_required();
    const unsigned int found = line_found("coverage", NULL, 0u, &lines);
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "files");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int file = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = at);
        shapes = cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(file, "file"), .other = "shapes.py", .cap = CLI_CELLS_POOL}) ? file : shapes;
    }
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, shapes);
    TEST_ASSERT_TRUE(numbers_contain(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = shapes, .name = "ran"), '1'));
    TEST_ASSERT_TRUE(numbers_contain(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = shapes, .name = "missed"), '5'));
}
