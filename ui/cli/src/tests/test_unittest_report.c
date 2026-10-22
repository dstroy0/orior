// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_unittest_report.c: unittest's report of its failures read, each with its last line and the line of its file its
// traceback last names
#include "test_runs_internal.h"

// one failure of unittest's report told: its name as pytest gives it, the last line of its traceback, and the line of
// its file the traceback last names, the rule under its head and the rule after it bounding the traceback
static void report_failure_told(CliTestWorker *worker, const char *block, unsigned long long size)
{
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    (void)cli_report_line(block, size, &at, &start, &end);
    const unsigned long long open = cli_report_find(block, end, " (");
    if (open >= end)
    {
        return;
    }
    unsigned long long dotted_end = end;
    while ((dotted_end > open + 2ull) && (block[dotted_end - 1ull] == ')'))
    {
        dotted_end -= 1ull;
    }
    char id[CLI_PATH_LONGEST * 2u];
    if (!cli_test_unittest_id(block + open + 2ull, dotted_end - open - 2ull, id, sizeof(id)))
    {
        return;
    }
    char wanted[CLI_PATH_LONGEST];
    const char *const path =
        cellul.find(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){
        .dst = wanted, .src = id, .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    wanted[(path != NULL) ? (size_t)(path - id) : cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL})] =
        '\0';
    unsigned long long body = size;
    unsigned long long body_end = size;
    while (cli_report_line(block, size, &at, &start, &end))
    {
        if ((end - start >= 3ull) && (memor.cmp(&(MemoriaCfg){.src = block + start, .other = "---", .bytes = 3u}) == 0))
        {
            body = at;
            break;
        }
    }
    at = body;
    while (cli_report_line(block, size, &at, &start, &end))
    {
        if ((end - start >= 3ull) && (memor.cmp(&(MemoriaCfg){.src = block + start, .other = "---", .bytes = 3u}) == 0))
        {
            body_end = start;
            break;
        }
    }
    body = (body < body_end) ? body : body_end;
    long long line = -1ll;
    at = body;
    while (cli_report_line(block, body_end, &at, &start, &end))
    {
        while ((start < end) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = block[start]}) != NULL))
        {
            start += 1ull;
        }
        if ((end - start < 6ull) ||
            (memor.cmp(&(MemoriaCfg){.src = block + start, .other = "File \"", .bytes = 6u}) != 0))
        {
            continue;
        }
        const unsigned long long quote = start + 6ull + cli_report_find(block + start + 6ull, end - start - 6ull, "\"");
        if (quote >= end)
        {
            continue;
        }
        unsigned long long number = quote + 1ull;
        while ((number + 7ull <= end) &&
               (memor.cmp(&(MemoriaCfg){.src = block + number, .other = ", line ", .bytes = 7u}) == 0))
        {
            number += 7ull;
        }
        const unsigned long long number_end = number + cli_report_find(block + number, end - number, ",");
        long long value = 0ll;
        if (cli_report_ends_with(block + start + 6ull, quote - start - 6ull, wanted) &&
            cli_report_count(block + number, number_end - number, &value))
        {
            line = (value > 0ll) ? value - 1ll : 0ll;
        }
    }
    unsigned long long last = 0ull;
    unsigned long long last_size = 0ull;
    const int found = cli_report_last_line(block + body, body_end - body, 1, &last, &last_size);
    cli_test_result_told(worker, id, "failed", found ? block + body + last : "", found ? last_size : 0ull, wanted, line,
                         0.0);
}

void cli_test_unittest_failures_told(CliTestWorker *worker, const char *text, unsigned long long size)
{
    static const char s_rule[] = "\n======================================================================\n";
    const unsigned long long rule = sizeof(s_rule) - 1ull;
    unsigned long long at = cli_report_find(text, size, s_rule);
    while (at < size)
    {
        const unsigned long long from = at + rule;
        const unsigned long long next = from + cli_report_find(text + from, size - from, s_rule);
        report_failure_told(worker, text + from, ((next < size) ? next : size) - from);
        at = next;
    }
}
