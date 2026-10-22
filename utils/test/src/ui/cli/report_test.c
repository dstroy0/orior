// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_test.c: the reports' check: a path is struck whatever its case, an error keeps its name when only numbers
// change, a panic is placed by orior's first frame, and a page stays within its limit
#include "../../../../../ui/cli/src/machine/report.h"
#include "cli_check.h"

static char s_logs[20001];
static char s_page[1u << 16u];

int main(void)
{
    char struck[256];
    CliLine line = {struck, sizeof(struck), 0ull};
    cli_report_scrub("at D:\\Git\\Tree\\x.rs", "d:\\git\\tree", &line);
    (void)cli_line_closed(&line);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = struck, .other = "at <tree>\\x.rs", .cap = CLI_CELLS_POOL}),
              "a path is struck whatever its case");
    for (unsigned int at = 0u; at < 21u; at += 1u)
    {
        char error[64];
        CliLine error_line = {error, sizeof(error), 0ull};
        cli_line_text(&error_line, "error ");
        cli_line_decimal(&error_line, at, 2u);
        cli_line_text(&error_line, " in D:\\Git\\Tree\\x.rs");
        (void)cli_line_closed(&error_line);
        cli_report_keep(error, "d:\\git\\tree");
    }
    char recent[4096];
    CliLine recent_line = {recent, sizeof(recent), 0ull};
    cli_report_recent(&recent_line);
    (void)cli_line_closed(&recent_line);
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = recent, .cap = CLI_CELLS_POOL, .other = "error 00", .other_cap = CLI_CELLS_POOL}) == NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = recent,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "error 01 in <tree>\\x.rs\n\nerror 02",
                                            .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = recent, .cap = CLI_CELLS_POOL, .other = "error 20", .other_cap = CLI_CELLS_POOL}) != NULL),
        "the newest twenty errors are kept struck, the oldest let go");
    char one[17];
    char other[17];
    cli_report_fingerprint("ui", "edit.js:12:3 broke", one);
    cli_report_fingerprint("ui", "edit.js:40:9 broke", other);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = one, .other = other, .cap = CLI_CELLS_POOL}),
              "an error keeps its name when only numbers change");
    cli_report_fingerprint("ui", "edit.js broke", one);
    cli_report_fingerprint("cli", "edit.js broke", other);
    cli_check(!cellul.eq(&(CatenaFinitaCfg){.src = one, .other = other, .cap = CLI_CELLS_POOL}),
              "and another category's error is another");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_report_panic_category("std::panicking\n  orior_cli::files::read\n  orior_gui_lib::run"),
                  .other = "library",
                  .cap = CLI_CELLS_POOL}),
              "a panic in orior's library is the library's");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_report_panic_category("std::panicking\n  orior_cli::cli::run"),
                                           .other = "cli",
                                           .cap = CLI_CELLS_POOL}),
              "one in the command line's own code is the command line's");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_report_panic_category("std::panicking\n  orior_gui_lib::start"),
                                           .other = "ui",
                                           .cap = CLI_CELLS_POOL}),
              "one in the window's is the window's");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_report_panic_category("std::panicking\n  core::option"),
                                           .other = "unknown",
                                           .cap = CLI_CELLS_POOL}),
              "and one outside orior_cli is unknown");
    memor.set(&(MemoriaCfg){.dst = s_logs, .val = 'x', .bytes = sizeof(s_logs) - 1u});
    const CliReport report = {"ui", "t", "w", "", s_logs};
    CliLine page = {s_page, sizeof(s_page), 0ull};
    cli_report_page(&report, NULL, &page);
    (void)cli_line_closed(&page);
    cli_check(page.at <= CLI_REPORT_ADDRESS + 200u, "a page stays within its limit");
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = s_page, .cap = CLI_CELLS_POOL, .other = "template=bug_report.yml", .other_cap = CLI_CELLS_POOL}) !=
         NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_page, .cap = CLI_CELLS_POOL, .other = "category=ui", .other_cap = CLI_CELLS_POOL}) != NULL),
        "and names its template and category");
    return cli_checked();
}
