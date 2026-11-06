// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_report.c: error reports: a path is scrubbed in any case, the recent list holds the newest twenty, a fingerprint ignores
// numbers, a panic is categorized by orior's first frame, and a report page stays within its limit
#include "ui/cli/src/machine/report.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_logs[20001];
static char s_page[1u << 16u];

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_path_is_scrubbed_whatever_its_case(void)
{
    char scrubbed[256];
    VerbaLinea line = {scrubbed, sizeof(scrubbed), 0ull};
    EMBED_CALL(cli_report_text.scrub, CliReportTextCfg, .text = "at D:\\Git\\Tree\\x.rs", .root = "d:\\git\\tree", .out = &line);
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING("at <tree>\\x.rs", scrubbed);
}

void test_the_recent_list_holds_the_newest_twenty_errors_scrubbed(void)
{
    for (unsigned int at = 0u; at < 21u; at += 1u)
    {
        char error[64];
        VerbaLinea error_line = {error, sizeof(error), 0ull};
        mmgr_line_put(&error_line, "error ");
        mmgr_line_decimal(&error_line, at, 2u);
        mmgr_line_put(&error_line, " in D:\\Git\\Tree\\x.rs");
        (void)mmgr_line_finish(&error_line);
        EMBED_CALL(cli_report_text.save, CliReportTextCfg, .text = error, .root = "d:\\git\\tree");
    }
    char recent[4096];
    VerbaLinea line = {recent, sizeof(recent), 0ull};
    EMBED_CALL(cli_report_root_out.recent, CliReportRootOutCfg, .out = &line);
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_NULL(cellul.find(&(CatenaFinitaCfg){.src = recent, .cap = CLI_POOL_BYTES, .other = "error 00", .other_cap = CLI_POOL_BYTES}));
    CLI_TEST_ASSERT_CONTAINS("error 01 in <tree>\\x.rs\n\nerror 02", recent);
    CLI_TEST_ASSERT_CONTAINS("error 20", recent);
}

void test_a_fingerprint_ignores_numbers(void)
{
    char one[17];
    char other[17];
    EMBED_CALL(cli_report_category_text.fingerprint, CliReportCategoryTextCfg, .category = "ui", .text = "edit.js:12:3 broke", .out_capacity = one);
    EMBED_CALL(cli_report_category_text.fingerprint, CliReportCategoryTextCfg, .category = "ui", .text = "edit.js:40:9 broke", .out_capacity = other);
    TEST_ASSERT_EQUAL_STRING(one, other);
}

void test_a_fingerprint_differs_by_category(void)
{
    char one[17];
    char other[17];
    EMBED_CALL(cli_report_category_text.fingerprint, CliReportCategoryTextCfg, .category = "ui", .text = "edit.js broke", .out_capacity = one);
    EMBED_CALL(cli_report_category_text.fingerprint, CliReportCategoryTextCfg, .category = "cli", .text = "edit.js broke", .out_capacity = other);
    TEST_ASSERT_FALSE(cellul.eq(&(CatenaFinitaCfg){.src = one, .other = other, .cap = CLI_POOL_BYTES}));
}

void test_a_panic_in_the_library_is_categorized_library(void)
{
    TEST_ASSERT_EQUAL_STRING("library", EMBED_CALL(cli_report.panic_category, CliReportCfg, .trace = "std::panicking\n  orior_cli::files::read\n  orior_gui_lib::run"));
}

void test_a_panic_in_the_command_line_is_categorized_cli(void)
{
    TEST_ASSERT_EQUAL_STRING("cli", EMBED_CALL(cli_report.panic_category, CliReportCfg, .trace = "std::panicking\n  orior_cli::cli::run"));
}

void test_a_panic_in_the_window_is_categorized_ui(void)
{
    TEST_ASSERT_EQUAL_STRING("ui", EMBED_CALL(cli_report.panic_category, CliReportCfg, .trace = "std::panicking\n  orior_gui_lib::start"));
}

void test_a_panic_outside_orior_is_categorized_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("unknown", EMBED_CALL(cli_report.panic_category, CliReportCfg, .trace = "std::panicking\n  core::option"));
}

void test_a_page_stays_within_its_limit_and_names_its_template_and_category(void)
{
    memor.set(&(MemoriaCfg){.dst = s_logs, .val = 'x', .bytes = sizeof(s_logs) - 1u});
    const CliReport report = {"ui", "t", "w", "", s_logs};
    VerbaLinea page = {s_page, sizeof(s_page), 0ull};
    EMBED_CALL(cli_report_root_out.page, CliReportRootOutCfg, .report = &report, .root = NULL, .out = &page);
    (void)mmgr_line_finish(&page);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(CLI_REPORT_ADDRESS + 200u, page.at);
    CLI_TEST_ASSERT_CONTAINS("template=bug_report.yml", s_page);
    CLI_TEST_ASSERT_CONTAINS("category=ui", s_page);
}
