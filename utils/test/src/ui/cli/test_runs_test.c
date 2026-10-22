// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_runs_test.c: the test runs' check: a run is spread by whole files, the largest first; verbose reports are read;
// a report gives each failure its message and line; a run tells each result, its failures and its coverage; and
// unittest's failures are read
#include "../../../../../ui/cli/src/tests/test_runs.h"
#include "../../../../../ui/cli/src/tests/test_runs_internal.h"
#include "cli_check.h"

static CliTestRun s_run = {.lock = CLI_LOCK_INIT, .wake = CLI_WAKE_INIT};
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static char s_told[1u << 20u];
static unsigned long long s_told_at;
static char s_seen[1u << 20u];

// what a run tells kept, a line each
static void runs_told(void *context, const char *heard, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_lock);
    if (s_told_at + size + 1ull < sizeof(s_told))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_told + s_told_at, .src = heard, .bytes = size});
        s_told[s_told_at + size] = '\n';
        s_told_at += size + 1ull;
    }
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

// the first line told of `kind`, and of the result `id` where it is not NULL, after the `from`th line: its node in the
// shared tree, CLI_JSON_NONE where none is; how many lines there are into `lines`
static unsigned int runs_line(const char *kind, const char *id, unsigned int from, const CliJson **json,
                              unsigned int *lines)
{
    cli_lock_take(&s_lock);
    const unsigned long long size = s_told_at;
    memor.cpy(&(MemoriaCfg){.dst = s_seen, .src = s_told, .bytes = size});
    cli_lock_give(&s_lock);
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
            const CliJson *const read = cli_check_parsed(s_seen + at, length, &one);
            const unsigned int outcome = cli_json_member(read, one, "outcome");
            if ((cellul.eq(&(CatenaFinitaCfg){
                    .src = cli_check_member_text(one, "kind"), .other = kind, .cap = CLI_CELLS_POOL})) &&
                ((id == NULL) ||
                 (cellul.eq(&(CatenaFinitaCfg){
                     .src = cli_check_member_text(outcome, "id"), .other = id, .cap = CLI_CELLS_POOL}))))
            {
                *json = read;
                found = one;
            }
        }
        at += length + 1ull;
    }
    *lines = index;
    return found;
}

// a whole number member of a node of the shared tree, -1 where it is none
static long long runs_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = cli_json_member(json, node, name);
    if (!cli_json_is(json, number, CFG_JSON_NUMBER))
    {
        return -1ll;
    }
    long long value = 0ll;
    for (unsigned long long at = 0ull; at < json->node[number].length; at += 1ull)
    {
        value = value * 10ll + (long long)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

// a run is spread by whole files, the largest first
static void runs_shares(void)
{
    static const char *const s_given[6] = {"a.py::t1", "a.py::t2", "a.py::t3", "b.py::t1", "c.py::t1", "c.py::t2"};
    static const char *const s_wanted[2][3] = {{"a.py::t1", "a.py::t2", "a.py::t3"},
                                               {"c.py::t1", "c.py::t2", "b.py::t1"}};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        s_run.given[at] = s_given[at];
    }
    cli_test_shares(&s_run, 6u, 2u);
    int spread = s_run.shares == 2u;
    for (unsigned int share = 0u; spread && (share < 2u); share += 1u)
    {
        spread = s_run.share_size[share] == 3u;
        for (unsigned int at = 0u; spread && (at < 3u); at += 1u)
        {
            spread = cellul.eq(&(CatenaFinitaCfg){.src = s_run.given[s_run.order[s_run.share_from[share] + at]],
                                                  .other = s_wanted[share][at],
                                                  .cap = CLI_CELLS_POOL});
        }
    }
    cli_check(spread, "a run is spread by whole files, the largest first");
    cli_test_shares(&s_run, 5u, 8u);
    cli_check(s_run.shares == 3u, "no more shares than files");
    cli_test_shares(&s_run, 5u, 1u);
    cli_check(s_run.shares == 1u, "and one where one is asked");
}

// whether pytest's verbose line names `id` with `outcome`
static int runs_pytest(const char *line, const char *id, const char *outcome)
{
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
    const char *read = NULL;
    if (!cli_test_pytest_line(line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}), &at, &size,
                              &read))
    {
        return id == NULL;
    }
    return (id != NULL) && (size == cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = line + at, .other = id, .bytes = size}) == 0) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = read, .other = outcome, .cap = CLI_CELLS_POOL}));
}

// whether unittest's verbose line names `id` with `outcome`
static int runs_unittest(const char *line, const char *id, const char *outcome)
{
    char read_id[CLI_PATH_LONGEST];
    const char *read = NULL;
    return cli_test_unittest_line(line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}), read_id,
                                  sizeof(read_id), &read) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = read_id, .other = id, .cap = CLI_CELLS_POOL})) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = read, .other = outcome, .cap = CLI_CELLS_POOL}));
}

// verbose reports are read
static void runs_verbose(void)
{
    cli_check(runs_pytest("tests/test_a.py::TestX::test_two PASSED                [ 50%]",
                          "tests/test_a.py::TestX::test_two", "passed"),
              "pytest's verbose line is read");
    cli_check(runs_pytest("tests/test_a.py::test_one[1-2] FAILED", "tests/test_a.py::test_one[1-2]", "failed"),
              "with a test's parameters");
    cli_check(runs_pytest("FAILED tests/test_a.py::test_one - assert 1 == 2", NULL, NULL), "and not its summary");
    cli_check(
        runs_unittest("test_two (tests.test_a.Checks.test_two) ... ok", "tests/test_a.py::Checks::test_two", "passed"),
        "unittest's verbose line is read");
    cli_check(runs_unittest("test_three (pkg.test_b.Checks.test_three) ... skipped 'later'",
                            "pkg/test_b.py::Checks::test_three", "skipped"),
              "and a test it skipped");
    static CliWords s_words;
    cli_test_dotted(&s_words, "pkg/test_b.py::Checks::test_three");
    cli_check((s_words.count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_words.word[0], .other = "pkg.test_b.Checks.test_three", .cap = CLI_CELLS_POOL})),
              "a test is named as unittest takes it");
}

// a report gives each failure its message and line, and unittest's failures are read
static void runs_reports(void)
{
    static const char s_xml[] =
        "<testsuites><testsuite><testcase classname=\"tests.test_a\" name=\"test_one\" file=\"tests/test_a.py\" "
        "line=\"3\" time=\"0.002\"><failure message=\"assert 1 == 2\">tests/test_a.py:5: in test_one\n    helper()\n"
        "tests/test_a.py:9: in helper\n    assert 1 == 2\nE   assert 1 == 2</failure></testcase><testcase "
        "classname=\"tests.test_a.TestX\" name=\"test_two\" file=\"tests/test_a.py\" line=\"12\" time=\"0.1\" />"
        "<testcase classname=\"tests.test_a.TestX\" name=\"test_skip\" file=\"tests/test_a.py\" line=\"14\" "
        "time=\"0\"><skipped type=\"pytest.skip\" message=\"not here\">skip</skipped></testcase></testsuite>"
        "</testsuites>";
    s_run.tell = (CliTell){runs_told, NULL};
    memor.cpy(&(MemoriaCfg){.dst = s_run.root,
                            .src = "D:/t",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "D:/t", .cap = CLI_CELLS_POOL}) + 1u});
    s_run.workers[0].run = &s_run;
    s_told_at = 0ull;
    cli_test_report_told(&s_run.workers[0], s_xml, sizeof(s_xml) - 1u);
    const CliJson *json = NULL;
    unsigned int lines = 0u;
    unsigned int one = runs_line("result", "tests/test_a.py::test_one", 0u, &json, &lines);
    unsigned int outcome = cli_json_member(json, one, "outcome");
    cli_check(
        (lines == 3u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(outcome, "message"), .other = "assert 1 == 2", .cap = CLI_CELLS_POOL})) &&
            (runs_whole(json, outcome, "line") == 8ll),
        "a failure has its message and the line it failed at");
    one = runs_line("result", "tests/test_a.py::TestX::test_two", 0u, &json, &lines);
    outcome = cli_json_member(json, one, "outcome");
    char seconds[32];
    CliLine line = {seconds, sizeof(seconds), 0ull};
    cli_json_write(json, cli_json_member(json, outcome, "seconds"), &line, 0u, 0);
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(outcome, "outcome"), .other = "passed", .cap = CLI_CELLS_POOL})) &&
                  (line.at == 3ull) && (memor.cmp(&(MemoriaCfg){.src = seconds, .other = "0.1", .bytes = 3u}) == 0),
              "a test that passed has its time");
    one = runs_line("result", "tests/test_a.py::TestX::test_skip", 0u, &json, &lines);
    outcome = cli_json_member(json, one, "outcome");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(outcome, "outcome"), .other = "skipped", .cap = CLI_CELLS_POOL})) &&
                  (runs_whole(json, outcome, "line") == 14ll),
              "and one skipped its line");
    cli_check((g_cli_test_report_cells.holds == 0ull) && (g_cli_test_report_cells.hw != 0ull) &&
                  (g_cli_test_report_cells.hw < (sizeof(s_xml) * 4u)),
              "a report's tags and texts are held in cells sized to the report, and given back");
    static const char s_failures[] =
        "test_two (tests.test_a.Checks.test_two) ... FAIL\n\n"
        "======================================================================\n"
        "FAIL: test_two (tests.test_a.Checks.test_two)\n"
        "----------------------------------------------------------------------\n"
        "Traceback (most recent call last):\n  File \"D:\\t\\tests\\test_a.py\", line 7, in test_two\n"
        "    self.assertEqual(1, 2)\nAssertionError: 1 != 2\n\n"
        "----------------------------------------------------------------------\nRan 1 test in 0.001s\n";
    s_told_at = 0ull;
    cli_test_unittest_failures_told(&s_run.workers[0], s_failures, sizeof(s_failures) - 1u);
    one = runs_line("result", "tests/test_a.py::Checks::test_two", 0u, &json, &lines);
    cli_check((lines == 1u) && (runs_whole(json, cli_json_member(json, one, "outcome"), "line") == 6ll),
              "unittest's failures are read");
}

// a run tells each result, its failures and its coverage
static void runs_python(void)
{
    char python[CLI_PATH_LONGEST];
    char dir[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("test_runs", dir);
    cli_full(dir, "tests", path);
    (void)query_folder_made(path);
    if (!cli_test_python(python) || !cli_test_has_pytest(python, dir))
    {
        cli_check(1, "the machine has no Python with pytest, and no test runs");
        return;
    }
    static const char *const s_files[3][2] = {
        {"shapes.py", "def area(width, height):\n    return width * height\n\n\ndef unused():\n    return 0\n"},
        {"tests/test_a.py",
         "import sys, os\nsys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath("
         "__file__))))\nfrom shapes import area\n\n\ndef test_area():\n    assert area(2, 3) == 6\n\n\n"
         "def test_wrong():\n    assert area(2, 3) == 7\n"},
        {"tests/test_b.py",
         "import pytest\n\n\nclass TestB:\n    def test_skipped(self):\n        pytest.skip(\"not here\")\n"}};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_full(dir, s_files[at][0], path);
        (void)query_file_written(path, s_files[at][1],
                                 cellul.len(&(CatenaFinitaCfg){.src = s_files[at][1], .cap = CLI_CELLS_POOL}));
    }
    static const char *const s_given[2] = {"tests/test_a.py", "tests/test_b.py"};
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned long long run = 0ull;
    s_told_at = 0ull;
    cli_check(cli_test_runs_start(dir, s_given, 2u, 1, 1, (CliTell){runs_told, NULL}, &run, &said), "the run starts");
    const CliJson *json = NULL;
    unsigned int lines = 0u;
    const unsigned long long until = cli_clock_milliseconds() + 120000ull;
    while ((runs_line("done", NULL, 0u, &json, &lines) == CLI_JSON_NONE) && (cli_clock_milliseconds() < until))
    {
        cli_lock_take(&s_lock);
        (void)cli_wake_wait_for(&s_wake, &s_lock, 100ull);
        cli_lock_give(&s_lock);
    }
    unsigned int found = runs_line("started", NULL, 0u, &json, &lines);
    cli_check(runs_whole(json, found, "workers") == 2ll, "two files, two processes");
    found = runs_line("done", NULL, 0u, &json, &lines);
    cli_check((runs_whole(json, found, "passed") == 1ll) && (runs_whole(json, found, "failed") == 1ll) &&
                  (runs_whole(json, found, "skipped") == 1ll),
              "the run's end has its counts");
    unsigned int wrong = CLI_JSON_NONE;
    for (unsigned int from = 0u; from < lines; from += 1u)
    {
        found = runs_line("result", "tests/test_a.py::test_wrong", from, &json, &lines);
        const unsigned int outcome = cli_json_member(json, found, "outcome");
        if ((found != CLI_JSON_NONE) && (cli_check_member_text(outcome, "message")[0] != '\0'))
        {
            wrong = outcome;
            break;
        }
    }
    cli_check((wrong != CLI_JSON_NONE) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(wrong, "outcome"), .other = "failed", .cap = CLI_CELLS_POOL})) &&
                  (runs_whole(json, wrong, "line") == 10ll) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = cli_check_member_text(wrong, "message"),
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "assert",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a failure has its message and line");
    found = runs_line("result", "tests/test_b.py::TestB::test_skipped", 0u, &json, &lines);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, found, "outcome"), "outcome"),
                                     .other = "skipped",
                                     .cap = CLI_CELLS_POOL}),
        "a test skipped is told so");
    found = runs_line("coverage", NULL, 0u, &json, &lines);
    unsigned int shapes = CLI_JSON_NONE;
    const unsigned int files = cli_json_member(json, found, "files");
    for (unsigned int at = 0u; cli_json_item(json, files, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int file = cli_json_item(json, files, at);
        shapes = (cellul.eq(&(CatenaFinitaCfg){
                     .src = cli_check_member_text(file, "file"), .other = "shapes.py", .cap = CLI_CELLS_POOL}))
                     ? file
                     : shapes;
    }
    int ran_one = 0;
    int missed_five = 0;
    const unsigned int ran = cli_json_member(json, shapes, "ran");
    const unsigned int missed = cli_json_member(json, shapes, "missed");
    for (unsigned int at = 0u; cli_json_item(json, ran, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int number = cli_json_item(json, ran, at);
        ran_one |= (json->node[number].length == 1u) && (json->text[json->node[number].text] == '1');
    }
    for (unsigned int at = 0u; cli_json_item(json, missed, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int number = cli_json_item(json, missed, at);
        missed_five |= (json->node[number].length == 1u) && (json->text[json->node[number].text] == '5');
    }
    cli_check((shapes != CLI_JSON_NONE) && ran_one && missed_five, "shapes.py is covered, its lines ran and missed");
}

int main(void)
{
    runs_shares();
    runs_verbose();
    runs_reports();
    runs_python();
    return cli_checked();
}
