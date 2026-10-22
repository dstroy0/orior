// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_tests_test.c: a tree's Rust tests: tests are found past strings, comments and lifetimes; a package's targets
// and modules give each test its name; and the harness's report gives each result and each failure its place
#include "../../../../../ui/cli/src/tests/cargo_report.h"
#include "../../../../../ui/cli/src/tests/cargo_tests_internal.h"
#include "cli_check.h"

static CliCargoTests s_tests;
static char s_out[4096];

// tests are found past strings, comments and lifetimes, and the modules a file names
static void cargo_scanned(void)
{
    static const char s_text[] =
        "\n//! #[test] fn not_in_a_comment() {}\nmod outer;\n/* #[test]\n   fn not_in_a_block() {} */\nfn "
        "helper<'a>(text: &'a str) -> char { let _ = \"#[test] fn not_in_a_string() {}\"; let _ = r#\"fn raw() { "
        "#[test] "
        "}\"#; '{' }\n\n#[cfg(test)]\nmod tests {\n    use super::*;\n\n    #[test]\n    fn one() {}\n\n    "
        "#[tokio::test]\n    async fn two() {}\n\n    #[test]\n    #[should_panic(expected = \"no {\")]\n    pub fn "
        "three() { let _ = '}'; }\n\n    fn not_a_test() {}\n\n    mod deeper {\n        mod inner_file;\n        "
        "#[test]\n        fn four() {}\n    }\n}\n\n#[test]\nfn five() {}\n";
    cli_cargo_scan_given();
    cli_cargo_tokens(s_text, sizeof(s_text) - 1u);
    const unsigned int count = cli_cargo_scan(s_text);
    char tests[1024];
    char mods[1024];
    CliLine test_line = {tests, sizeof(tests), 0ull};
    CliLine mod_line = {mods, sizeof(mods), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliRustHeld *const held = &g_cli_cargo_scan.held[at];
        const char *const inline_names = g_cli_cargo_scan.names + held->inline_names;
        CliLine *const line = held->test ? &test_line : &mod_line;
        cli_line_text(line, (line->at > 0ull) ? "|" : "");
        cli_line_text(line, inline_names);
        cli_line_text(line, (inline_names[0] != '\0') ? "::" : "");
        cli_line_text(line, g_cli_cargo_scan.names + held->name);
        if (held->test)
        {
            cli_line_character(line, ' ');
            cli_line_decimal(line, held->line, 1u);
        }
    }
    cli_check_text(tests, test_line.at, "tests::one 12|tests::two 15|tests::three 19|tests::deeper::four 26|five 31",
                   "tests are found past strings, comments and lifetimes");
    cli_check_text(mods, mod_line.at, "outer|tests::deeper::inner_file", "and the modules a file names");
    cli_check((g_cli_cargo_scan.token_room == g_cli_cargo_scan.token_count) && (g_cli_cargo_scan.token_count != 0u),
              "the tokens' cell is the size of their count");
    cli_cargo_scan_given();
    cli_check(g_cli_cargo_cells.holds == 0ull, "and every cell of the search is given back");
}

// a target written as `kind` or `kind:name`
static void cargo_target(CliLine *out, const CliCargoTarget *target)
{
    static const char *const s_kinds[3] = {"lib", "bin", "test"};
    cli_line_text(out, s_kinds[(target->kind < 3u) ? target->kind : 0u]);
    if (target->name != NULL)
    {
        cli_line_character(out, ':');
        cli_line_text(out, target->name);
    }
}

// a package's targets and modules give each test its name
static void cargo_found(void)
{
    static const char *const s_files[9][2] = {
        {"crate/Cargo.toml", "[package]\nname = \"thing\"\nedition = \"2021\"\n\n[[test]]\nname = \"named\"\npath = "
                             "\"checks/named.rs\"\n"},
        {"crate/src/lib.rs", "pub mod parts;\nmod flat;\n#[test]\nfn at_top() {}\n"},
        {"crate/src/parts/mod.rs", "mod inner;\n"},
        {"crate/src/parts/inner.rs", "#[cfg(test)]\nmod checks {\n    #[test]\n    fn deep() {}\n}\n"},
        {"crate/src/flat.rs", "mod below;\n"},
        {"crate/src/flat/below.rs", "#[test]\nfn under_flat() {}\n"},
        {"crate/src/main.rs", "#[test]\nfn in_main() {}\n"},
        {"crate/tests/whole.rs", "#[test]\nfn integration() {}\n"},
        {"crate/checks/named.rs", "#[test]\nfn listed() {}\n"}};
    char dir[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_check_folder("cargo_found", dir);
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        cli_full(dir, s_files[at][0], path);
        if (cli_path_parent(path, folder))
        {
            (void)query_folder_made(folder);
        }
        (void)query_file_written(path, s_files[at][1],
                                 cellul.len(&(CatenaFinitaCfg){.src = s_files[at][1], .cap = CLI_CELLS_POOL}));
    }
    cli_cargo_found(dir, &s_tests);
    CliLine line = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; at < s_tests.count; at += 1u)
    {
        const CliCargoFound *const one = &s_tests.found[at];
        cli_line_text(&line, (at > 0u) ? "\n" : "");
        cli_line_text(&line, one->test.id);
        cli_line_character(&line, ' ');
        cargo_target(&line, &one->target);
        cli_line_character(&line, ' ');
        cli_line_text(&line, one->full);
        cli_line_character(&line, ' ');
        cli_line_text(&line, (one->test.class_name != NULL) ? one->test.class_name : "-");
    }
    cli_check_text(s_out, line.at,
                   "crate/src/lib.rs::at_top lib at_top -\ncrate/src/parts/inner.rs::checks::deep lib "
                   "parts::inner::checks::deep checks\ncrate/src/flat/below.rs::under_flat lib flat::below::under_flat "
                   "-\ncrate/checks/named.rs::listed test:named listed -\ncrate/src/main.rs::in_main bin:thing in_main "
                   "-\ncrate/tests/whole.rs::integration test:whole integration -",
                   "a package's targets and modules give each test its name");
    cli_check((s_tests.count > 1u) && (s_tests.found[1].test.line == 3u), "and its line");
    cli_check((s_tests.room >= s_tests.count) && (s_tests.pieces != NULL) && (s_tests.pieces->next == NULL),
              "the tests are held in a room grown to them, their texts in one piece");
    cli_cargo_tests_given(&s_tests);
    cli_check((g_cli_cargo_cells.holds == 0ull) && (g_cli_cargo_cells.hw != 0ull),
              "the search's cells are each given back as it ends");
    cli_check(cli_cargo_is_rust("crate/src/lib.rs::at_top") && cli_cargo_is_rust("crate/src/lib.rs") &&
                  !cli_cargo_is_rust("tests/test_a.py::test_one"),
              "a Rust test and file are told from a Python one");
}

// the result `line` reads as, `name outcome`, into s_out, or "-" where it is none
static const char *cargo_result(const char *line)
{
    unsigned long long name = 0ull;
    unsigned long long size = 0ull;
    const char *outcome = NULL;
    CliLine out = {s_out, sizeof(s_out), 0ull};
    if (cli_cargo_result_line(line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}), &name, &size,
                              &outcome))
    {
        cli_line_counted(&out, line + name, size);
        cli_line_character(&out, ' ');
        cli_line_text(&out, outcome);
    }
    else
    {
        cli_line_character(&out, '-');
    }
    (void)cli_line_closed(&out);
    return s_out;
}

// a failure told, `name: message @ path:line` put after the others on a line of its own
static void cargo_failure(void *context, const char *name, unsigned long long name_size, const char *message,
                          unsigned long long message_size, const char *path, long long line)
{
    CliLine *const out = context;
    cli_line_text(out, (out->at > 0ull) ? "\n" : "");
    cli_line_counted(out, name, name_size);
    cli_line_text(out, ": ");
    cli_line_counted(out, message, message_size);
    cli_line_text(out, " @ ");
    cli_line_text(out, (path != NULL) ? path : "-");
    cli_line_character(out, ':');
    cli_line_decimal(out, (line > 0ll) ? (unsigned long long)line : 0ull, 1u);
}

// the harness's report gives each result and each failure its place
static void cargo_report(void)
{
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cargo_result("test files::listing::one ... ok"),
                                           .other = "files::listing::one passed",
                                           .cap = CLI_CELLS_POOL}),
              "a test that passed");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cargo_result("test two - should panic ... FAILED"),
                                           .other = "two failed",
                                           .cap = CLI_CELLS_POOL}),
              "one that failed");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cargo_result("test three ... ignored, slow"),
                                           .other = "three skipped",
                                           .cap = CLI_CELLS_POOL}),
              "one ignored");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cargo_result("test result: ok. 3 passed; 0 failed"), .other = "-", .cap = CLI_CELLS_POOL}),
              "and the summary is none");
    static const char s_report[] =
        "running 2 tests\ntest a::one ... FAILED\ntest a::two ... FAILED\n\nfailures:\n\n---- a::one stdout "
        "----\n\nthread 'a::one' panicked at src\\a.rs:12:9:\nassertion `left == right` failed\n  left: 1\n right: "
        "2\nnote: run with `RUST_BACKTRACE=1`\n\n---- a::two stdout ----\nthread 'a::two' panicked at 'old words', "
        "src/a.rs:20:5\n\nfailures:\n    a::one\n    a::two\n";
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_cargo_failures_each(s_report, sizeof(s_report) - 1u, cargo_failure, &out);
    cli_check_text(s_out, out.at,
                   "a::one: assertion `left == right` failed\n  left: 1\n right: 2 @ src/a.rs:12\na::two: old words @ "
                   "src/a.rs:20",
                   "each failure's message and the place it panicked");
    static const char s_said[] =
        "   Compiling thing v0.1.0\nerror[E0308]: mismatched types\n --> src/lib.rs:3:18\n  |\n";
    char error[256];
    char path[CLI_PATH_LONGEST];
    unsigned long long length = 0ull;
    long long line = 0ll;
    cli_check(
        cli_cargo_build_error(s_said, sizeof(s_said) - 1u, error, sizeof(error), &length, path, &line) &&
            (length == 30ull) &&
            (memor.cmp(&(MemoriaCfg){.src = error, .other = "error[E0308]: mismatched types", .bytes = 30u}) == 0) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = "src/lib.rs", .cap = CLI_CELLS_POOL})) && (line == 3ll),
        "a build's first error and the place it names");
}

int main(void)
{
    cargo_scanned();
    cargo_found();
    cargo_report();
    return cli_checked();
}
