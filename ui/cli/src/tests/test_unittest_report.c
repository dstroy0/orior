// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_unittest_report.c: unittest's report of its failures read, each with its last line and the line of its file its
// traceback last names
#include "../text/text.h"
#include "test_runs_internal.h"

typedef struct
{
    CliTestWorker *worker;
    const char *block;
    unsigned long long size;
} TestUnittestReportCtx;

// one failure of unittest's report reported: its name as pytest gives it, the last line of its traceback, and the line
// of its file the traceback last names, the rule under its head and the rule after it bounding the traceback
EMBED_INLINE void report_failure_emit(const TestUnittestReportCtx *args)
{
    CliTestWorker *worker = args->worker;
    unsigned long long size = args->size;
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long end = 0ULL;
    (void)EMBED_CALL(cli_test_runs_text_size_internal.report_line, CliTestRunsTextSizeCtx, .text = args->block,
                     .size = size, .at = &at, .start = &start, .end = &end);
    const unsigned long long open =
        EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                   .text = args->block, .length = end, .needle = " (");
    if (open >= end)
    {
        return;
    }
    unsigned long long dotted_end = end;
    if (dotted_end > (open + 2ULL))
    {
        dotted_end =
            (open + 2ULL) + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                       .text = args->block + open + 2ULL, .at = dotted_end - (open + 2ULL), .set = ")");
    }
    char id[CLI_PATH_MAX_LEN * 2u];
    if (!EMBED_CALL(cli_test_runs_length_internal.test_unittest_id, CliTestRunsLengthCtx,
                    .dotted = args->block + open + 2ULL, .length = dotted_end - open - 2ULL, .id_out = id,
                    .capacity = sizeof(id)))
    {
        return;
    }
    char required[CLI_PATH_MAX_LEN];
    const char *const path =
        cellul.find(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){
        .dst = required, .src = id, .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    required[(path != NULL) ? (size_t)(path - id) : cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL})] =
        '\0';
    unsigned long long body = size;
    unsigned long long body_end = size;
    while (EMBED_CALL(cli_test_runs_text_size_internal.report_line, CliTestRunsTextSizeCtx, .text = args->block,
                      .size = size, .at = &at, .start = &start, .end = &end))
    {
        if ((end - start >= 3ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = args->block + start, .other = "---", .bytes = 3u}) == 0))
        {
            body = at;
            break;
        }
    }
    at = body;
    while (EMBED_CALL(cli_test_runs_text_size_internal.report_line, CliTestRunsTextSizeCtx, .text = args->block,
                      .size = size, .at = &at, .start = &start, .end = &end))
    {
        if ((end - start >= 3ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = args->block + start, .other = "---", .bytes = 3u}) == 0))
        {
            body_end = start;
            break;
        }
    }
    body = (body < body_end) ? body : body_end;
    long long line = -1LL;
    at = body;
    while (EMBED_CALL(cli_test_runs_text_size_internal.report_line, CliTestRunsTextSizeCtx, .text = args->block,
                      .size = body_end, .at = &at, .start = &start, .end = &end))
    {
        while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t", .cap = CLI_CELLS_POOL, .byte = args->block[start]}) != NULL))
        {
            start += 1ULL;
        }
        if ((end - start < 6ULL) ||
            (memor.cmp(&(MemoriaCfg){.src = args->block + start, .other = "File \"", .bytes = 6u}) != 0))
        {
            continue;
        }
        const unsigned long long quote =
            start + 6ULL +
            EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                       .text = args->block + start + 6ULL, .length = end - start - 6ULL, .needle = "\"");
        if (quote >= end)
        {
            continue;
        }
        unsigned long long number = quote + 1ULL;
        while ((number + 7ULL <= end) &&
               (memor.cmp(&(MemoriaCfg){.src = args->block + number, .other = ", line ", .bytes = 7u}) == 0))
        {
            number += 7ULL;
        }
        const unsigned long long number_end =
            number + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                                .text = args->block + number, .length = end - number, .needle = ",");
        long long value = 0LL;
        if (EMBED_CALL(cli_test_runs_length_path_internal.report_ends_with, CliTestRunsLengthPathCtx,
                       .path = args->block + start + 6ULL, .length = quote - start - 6ULL, .required = required) &&
            EMBED_CALL(cli_test_runs_text_length_internal.report_count, CliTestRunsTextLengthCtx,
                       .text = args->block + number, .length = number_end - number, .value = &value))
        {
            line = (value > 0LL) ? value - 1LL : 0LL;
        }
    }
    unsigned long long last = 0ULL;
    unsigned long long last_size = 0ULL;
    const int found =
        EMBED_CALL(cli_test_runs_text_internal.report_last_line, CliTestRunsTextCtx, .text = args->block + body,
                   .size = body_end - body, .skip_rules = 1, .last = &last, .last_size = &last_size);
    EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker, .id = id,
               .outcome = "failed", .message = found ? args->block + body + last : "",
               .message_size = found ? last_size : 0ULL, .file = required, .line_i64 = line, .seconds = 0.0);
}

void cli_test_unittest_failures_emit(const CliTestRunsTextWorkerCtx *args)
{
    static const char s_rule[] = "\n======================================================================\n";
    const unsigned long long rule = sizeof(s_rule) - 1ULL;
    unsigned long long at =
        EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                   .text = args->text, .length = args->size, .needle = s_rule);
    while (at < args->size)
    {
        const unsigned long long from = at + rule;
        const unsigned long long next =
            from + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                              .text = args->text + from, .length = args->size - from, .needle = s_rule);
        EMBED_CALL(report_failure_emit, TestUnittestReportCtx, .worker = args->worker, .block = args->text + from,
                   .size = ((next < args->size) ? next : args->size) - from);
        at = next;
    }
}
