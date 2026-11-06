// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_problems.c: the problems read where the servers' reports stand: a report published shows at the reading
// thread's next step, worst first with each message's first line; a file's next report replaces it at the end; a report
// with none takes the file out; a press closes a file's rows; and threads publishing while a reader steps and reads
// every row leave the table with each file's last report
#include "ui/cli/src/command_line/pool.h"
#include "ui/cli/src/command_line/cli_internal.h"
#include "ui/cli/src/servers/servers.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include <string.h>

static _Atomic unsigned int s_woken;

static void woken_counted(void)
{
    atomic_fetch_add(&s_woken, 1u);
}

void setUp(void)
{
    cli_unity_begun();
    atomic_store(&s_woken, 0u);
    cli_servers_problems_begin(woken_counted);
}

void tearDown(void)
{
    cli_servers_problems_end();
}

// a report of `count` diagnostics for `path`, each with `message`, its severity 2 then 1 by turns and its line its place,
// the path and the message laid in the report's cell after its diagnostics, as the servers lay a report: NULL where the
// cell could not be taken, which the counts then fail on, since a writer's thread cannot fail a test itself
static CliServerReport *report_laid(const char *path, unsigned int count, const char *message)
{
    const size_t path_bytes = strlen(path) + 1u;
    const size_t message_bytes = strlen(message) + 1u;
    CliServerReport *const report = g_cli_guard->persistent_buf_alloc(
        sizeof(*report) + (count * sizeof(CliServerDiagnostic)) + path_bytes + message_bytes, g_cli_servers_owner);
    if (report == NULL)
    {
        return NULL;
    }
    memset(&report->json, 0, sizeof(report->json));
    char *const text = (char *)(void *)(report->item + count);
    memcpy(text, path, path_bytes);
    memcpy(text + path_bytes, message, message_bytes);
    report->path = text;
    report->count = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        report->item[at] = (CliServerDiagnostic){at, 0ULL, at, 4ULL, ((at % 2u) == 0u) ? 2u : 1u, text + path_bytes,
                                                 message_bytes - 1u};
    }
    return report;
}

// the report of `count` diagnostics for `path` handed to the problems' sink as the servers hand one
static void reported(const char *path, unsigned int count, const char *message)
{
    const CliServersSink sink = cli_servers_problems_sink("");
    CliServerReport *const report = report_laid(path, count, message);
    if (report == NULL)
    {
        return;
    }
    int taken = 0;
    sink.diagnostics(&(CliServerDiagnosticsCfg){.path = report->path, .item = report->item, .count = report->count,
                                                .report = report, .taken = &taken});
    if (!taken)
    {
        cli_servers_report_release(report);
    }
}

void test_a_report_shows_at_the_next_step_worst_first_with_each_message_cut_to_its_first_line(void)
{
    reported("src/one.c", 3u, "first line\nsecond line");
    TEST_ASSERT_TRUE(atomic_load(&s_woken) != 0u);
    TEST_ASSERT_EQUAL_UINT(0u, cli_servers_problems_counts().files);
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_step());
    const CliProblemsCounts counts = cli_servers_problems_counts();
    TEST_ASSERT_EQUAL_UINT(1u, counts.files);
    TEST_ASSERT_EQUAL_UINT(3u, counts.items);
    TEST_ASSERT_EQUAL_UINT(1u, counts.errors);
    TEST_ASSERT_EQUAL_UINT(4u, counts.rows);
    CliProblemRow row;
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(0u, &row));
    TEST_ASSERT_EQUAL_INT(1, row.is_file);
    TEST_ASSERT_EQUAL_STRING("src/one.c", row.path);
    TEST_ASSERT_EQUAL_STRING("one.c", row.name);
    TEST_ASSERT_EQUAL_UINT64(3ULL, row.folder_length);
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(1u, &row));
    TEST_ASSERT_EQUAL_INT(0, row.is_file);
    TEST_ASSERT_EQUAL_UINT(1u, row.severity);
    TEST_ASSERT_EQUAL_UINT64(1ULL, row.line);
    TEST_ASSERT_EQUAL_UINT64(10ULL, row.message_length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(row.message, "first line", 10u));
    TEST_ASSERT_EQUAL_INT(0, cli_servers_problems_row(4u, &row));
    TEST_ASSERT_EQUAL_INT(0, cli_servers_problems_step());
}

void test_a_files_next_report_replaces_it_at_the_end_and_one_with_none_takes_it_out(void)
{
    reported("src/one.c", 1u, "one");
    reported("src/two.c", 2u, "two");
    (void)cli_servers_problems_step();
    CliProblemRow row;
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(0u, &row));
    TEST_ASSERT_EQUAL_STRING("src/one.c", row.path);
    reported("src/one.c", 3u, "again");
    // the rows read before the next step stand as they were
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(0u, &row));
    TEST_ASSERT_EQUAL_STRING("src/one.c", row.path);
    TEST_ASSERT_EQUAL_UINT(1u, row.count);
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_step());
    TEST_ASSERT_EQUAL_UINT(2u, cli_servers_problems_counts().files);
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(0u, &row));
    TEST_ASSERT_EQUAL_STRING("src/two.c", row.path);
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(3u, &row));
    TEST_ASSERT_EQUAL_STRING("src/one.c", row.path);
    TEST_ASSERT_EQUAL_UINT(3u, row.count);
    reported("src/two.c", 0u, "");
    (void)cli_servers_problems_step();
    TEST_ASSERT_EQUAL_UINT(1u, cli_servers_problems_counts().files);
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(0u, &row));
    TEST_ASSERT_EQUAL_STRING("src/one.c", row.path);
}

void test_a_press_on_a_files_row_closes_its_rows_and_one_on_a_diagnostic_names_its_place(void)
{
    reported("src/one.c", 2u, "one");
    (void)cli_servers_problems_step();
    CliProblemRow row;
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_press(2u, &row));
    TEST_ASSERT_EQUAL_STRING("src/one.c", row.path);
    TEST_ASSERT_EQUAL_UINT64(0ULL, row.line);
    TEST_ASSERT_EQUAL_INT(0, cli_servers_problems_press(0u, &row));
    TEST_ASSERT_EQUAL_UINT(1u, cli_servers_problems_counts().rows);
    TEST_ASSERT_EQUAL_INT(1, cli_servers_problems_row(0u, &row));
    TEST_ASSERT_EQUAL_INT(1, row.closed);
}

// the writers that publish beside the reader, and the reports each publishes
#define TEST_WRITERS 4u
#define TEST_REPORTS 1500u

static _Atomic unsigned int s_writers_done;

// a writer's reports for a path of its own, each with one more diagnostic than the last, up to TEST_REPORTS, the count
// starting again from 1 past 8
static void writer_run(void *context)
{
    const char *const path = (const char *)context;
    for (unsigned int report = 1u; report <= TEST_REPORTS; report += 1u)
    {
        reported(path, ((report - 1u) % 8u) + 1u, "found\nmore");
    }
    atomic_fetch_add(&s_writers_done, 1u);
}

void test_threads_publishing_while_a_reader_steps_and_reads_every_row_leave_each_files_last_report(void)
{
    static const char *const s_paths[TEST_WRITERS] = {"src/a.c", "src/b.c", "src/c.c", "src/d.c"};
    static CliThread s_threads[TEST_WRITERS];
    atomic_store(&s_writers_done, 0u);
    for (unsigned int writer = 0u; writer < TEST_WRITERS; writer += 1u)
    {
        s_threads[writer] = (CliThread){writer_run, (void *)s_paths[writer], CLI_PRIORITY_WORK};
        TEST_ASSERT_EQUAL_INT(1, cli_thread_start(&(CliThreadCtx){.thread = &s_threads[writer]}));
    }
    unsigned long long rows_read = 0ULL;
    const unsigned long long until = cli_clock_milliseconds() + 60000ULL;
    while ((atomic_load(&s_writers_done) < TEST_WRITERS) && (cli_clock_milliseconds() < until))
    {
        (void)cli_servers_problems_step();
        CliProblemRow row;
        for (unsigned int at = 0u; cli_servers_problems_row(at, &row); at += 1u)
        {
            TEST_ASSERT_EQUAL_INT(0, memcmp(row.path, "src/", 4u));
            TEST_ASSERT_TRUE(row.is_file || ((row.message_length == 5ULL) && (memcmp(row.message, "found", 5u) == 0)));
            rows_read += 1ULL;
        }
    }
    TEST_ASSERT_EQUAL_UINT(TEST_WRITERS, atomic_load(&s_writers_done));
    TEST_ASSERT_TRUE(rows_read != 0ULL);
    (void)cli_servers_problems_step();
    const CliProblemsCounts counts = cli_servers_problems_counts();
    TEST_ASSERT_EQUAL_UINT(TEST_WRITERS, counts.files);
    TEST_ASSERT_EQUAL_UINT(TEST_WRITERS * (((TEST_REPORTS - 1u) % 8u) + 1u), counts.items);
}
