// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_cargo_tests.c: a tree's Rust tests: tests are found past strings, comments and lifetimes, a package's targets and modules name each
// test, and the harness's report places each result and failure
#include "ui/cli/src/tests/cargo_report.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/tests/cargo_tests_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliCargoTests s_tests;
static char s_out[4096];
static char s_scanned_tests[1024];
static char s_scanned_mods[1024];
static unsigned long long s_scanned_tests_size;
static unsigned long long s_scanned_mods_size;

// a sample's items scanned into s_scanned_tests and s_scanned_mods; the scan's memory is left for the test to read and release
static void sample_scanned(void)
{
    static const char s_text[] = "\n//! #[test] fn not_in_a_comment() {}\nmod outer;\n/* #[test]\n   fn not_in_a_block() {} */\nfn "
                                 "helper<'a>(text: &'a str) -> char { let _ = \"#[test] fn not_in_a_string() {}\"; let _ = r#\"fn raw() { "
                                 "#[test] "
                                 "}\"#; '{' }\n\n#[cfg(test)]\nmod tests {\n    use super::*;\n\n    #[test]\n    fn one() {}\n\n    "
                                 "#[tokio::test]\n    async fn two() {}\n\n    #[test]\n    #[should_panic(expected = \"no {\")]\n    pub fn "
                                 "three() { let _ = '}'; }\n\n    fn not_a_test() {}\n\n    mod deeper {\n        mod inner_file;\n        "
                                 "#[test]\n        fn four() {}\n    }\n}\n\n#[test]\nfn five() {}\n";
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.tokens, &g_cli_cargo_cells);
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.items, &g_cli_cargo_cells);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
    EMBED_CALL(cli_cargo_tests_internal.cargo_tokens, CliCargoTestsCtx, .text = s_text, .size = sizeof(s_text) - 1u);
    const unsigned int count = EMBED_CALL(cli_cargo_tests_internal.cargo_scan, CliCargoTestsCtx, .text = s_text);
    VerbaLinea test_line = {s_scanned_tests, sizeof(s_scanned_tests), 0ull};
    VerbaLinea mod_line = {s_scanned_mods, sizeof(s_scanned_mods), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliRustItem *const item = &g_cli_cargo_scan.items[at];
        const char *const inline_names = g_cli_cargo_scan.names + item->inline_names;
        VerbaLinea *const line = item->test ? &test_line : &mod_line;
        mmgr_line_put(line, (line->at > 0ull) ? "|" : "");
        mmgr_line_put(line, inline_names);
        mmgr_line_put(line, (inline_names[0] != '\0') ? "::" : "");
        mmgr_line_put(line, g_cli_cargo_scan.names + item->name);
        if (item->test)
        {
            mmgr_line_ch(line, ' ');
            mmgr_line_decimal(line, item->line, 1u);
        }
    }
    s_scanned_tests_size = test_line.at;
    s_scanned_mods_size = mod_line.at;
}

// the `count` files, each a path and its text, written in a fresh folder `folder_name`, and its tests searched into s_tests
static void files_found(const char *const (*files)[2], unsigned int count, const char *folder_name)
{
    char dir[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    cli_unity_folder(folder_name, dir);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = files[at][0], .joined = path);
        if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
        {
            TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder));
        }
        cli_unity_file(dir, files[at][0], files[at][1]);
    }
    EMBED_CALL(cli_cargo_tests_table.cargo_find, CliCargoTestsCfg, .root = dir, .tests = &s_tests);
}

// a package of a library, a binary, an integration test and a named test searched into s_tests
static void package_found(void)
{
    static const char *const s_files[9][2] = {{"crate/Cargo.toml", "[package]\nname = \"thing\"\nedition = \"2021\"\n\n[[test]]\nname = \"named\"\npath = \"checks/named.rs\"\n"},
                                              {"crate/src/lib.rs", "pub mod parts;\nmod flat;\n#[test]\nfn at_top() {}\n"},
                                              {"crate/src/parts/mod.rs", "mod inner;\n"},
                                              {"crate/src/parts/inner.rs", "#[cfg(test)]\nmod checks {\n    #[test]\n    fn deep() {}\n}\n"},
                                              {"crate/src/flat.rs", "mod below;\n"},
                                              {"crate/src/flat/below.rs", "#[test]\nfn under_flat() {}\n"},
                                              {"crate/src/main.rs", "#[test]\nfn in_main() {}\n"},
                                              {"crate/tests/whole.rs", "#[test]\nfn integration() {}\n"},
                                              {"crate/checks/named.rs", "#[test]\nfn listed() {}\n"}};
    files_found(s_files, 9u, "cargo_found");
}

// a target written as `kind` or `kind:name`
static void target_written(VerbaLinea *out, const CliCargoTarget *target)
{
    static const char *const s_kinds[3] = {"lib", "bin", "test"};
    mmgr_line_put(out, s_kinds[(target->kind < 3u) ? target->kind : 0u]);
    if (target->name != NULL)
    {
        mmgr_line_ch(out, ':');
        mmgr_line_put(out, target->name);
    }
}

// each test s_tests holds written `id target full class` on a line of its own into s_out, the tests then released: the bytes written
static unsigned long long found_written(void)
{
    VerbaLinea line = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; at < s_tests.count; at += 1u)
    {
        const CliCargoFound *const one = &s_tests.found[at];
        mmgr_line_put(&line, (at > 0u) ? "\n" : "");
        mmgr_line_put(&line, one->test.id);
        mmgr_line_ch(&line, ' ');
        target_written(&line, &one->target);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, one->full);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, (one->test.class_name != NULL) ? one->test.class_name : "-");
    }
    CliCargoPiece *piece = s_tests.pieces;
    while (piece != NULL)
    {
        CliCargoPiece *const next = piece->next;
        g_cli_cells->persistent_buf_release(piece, &g_cli_cargo_cells);
        piece = next;
    }
    g_cli_cells->persistent_buf_release(s_tests.found, &g_cli_cargo_cells);
    *(&s_tests) = (CliCargoTests){NULL, 0u, 0u, NULL, 0ULL, 0};
    return line.at;
}

// the result `line` reads as, `name outcome`, into s_out, or "-" where it is none
static const char *result_of(const char *line)
{
    unsigned long long name = 0ull;
    unsigned long long size = 0ull;
    const char *outcome = NULL;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (EMBED_CALL(cli_cargo_report_line.cargo_result_line, CliCargoReportLineCfg, .line_text = line, .size = CLI_UNITY_LENGTH(line), .name = &name, .name_size = &size, .outcome = &outcome))
    {
        mmgr_line_put_n(&out, line + name, size);
        mmgr_line_ch(&out, ' ');
        mmgr_line_put(&out, outcome);
    }
    else
    {
        mmgr_line_ch(&out, '-');
    }
    (void)mmgr_line_finish(&out);
    return s_out;
}

// a failure reported, `name: message @ path:line` appended on its own line
static void failure_recorded(const CliEachNameMessagePathLineCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "\n" : "");
    mmgr_line_put_n(out, args->name, args->name_size);
    mmgr_line_put(out, ": ");
    mmgr_line_put_n(out, args->message, args->message_size);
    mmgr_line_put(out, " @ ");
    mmgr_line_put(out, (args->path != NULL) ? args->path : "-");
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, (args->line > 0ll) ? (unsigned long long)args->line : 0ull, 1u);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_tests_are_found_end_strings_comments_and_lifetimes(void)
{
    sample_scanned();
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.tokens, &g_cli_cargo_cells);
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.items, &g_cli_cargo_cells);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
    CLI_TEST_ASSERT_TEXT("tests::one 12|tests::two 15|tests::three 19|tests::deeper::four 26|five 31", s_scanned_tests, s_scanned_tests_size);
}

void test_the_modules_a_file_names_are_found(void)
{
    sample_scanned();
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.tokens, &g_cli_cargo_cells);
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.items, &g_cli_cargo_cells);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
    CLI_TEST_ASSERT_TEXT("outer|tests::deeper::inner_file", s_scanned_mods, s_scanned_mods_size);
}

void test_the_tokens_cell_is_the_size_of_their_count(void)
{
    sample_scanned();
    const unsigned int capacity = g_cli_cargo_scan.token_capacity;
    const unsigned int count = g_cli_cargo_scan.token_count;
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.tokens, &g_cli_cargo_cells);
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.items, &g_cli_cargo_cells);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
    TEST_ASSERT_EQUAL_UINT(count, capacity);
    TEST_ASSERT_NOT_EQUAL(0u, count);
}

void test_every_cell_of_the_scan_is_released(void)
{
    sample_scanned();
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.tokens, &g_cli_cargo_cells);
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.items, &g_cli_cargo_cells);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_cargo_cells.in_use));
}

void test_a_packages_targets_and_modules_name_each_test(void)
{
    package_found();
    const unsigned long long size = found_written();
    CLI_TEST_ASSERT_TEXT("crate/src/lib.rs::at_top lib at_top -\ncrate/src/parts/inner.rs::checks::deep lib "
                         "parts::inner::checks::deep checks\ncrate/src/flat/below.rs::under_flat lib flat::below::under_flat "
                         "-\ncrate/checks/named.rs::listed test:named listed -\ncrate/src/main.rs::in_main bin:thing in_main "
                         "-\ncrate/tests/whole.rs::integration test:whole integration -",
                         s_out, size);
}

void test_a_manifests_lib_path_listed_binaries_and_autotests_choose_the_targets(void)
{
    static const char *const s_files[5][2] = {{"crate/Cargo.toml", "[package]\nname = \"tool\"\nautotests = false # none sought\n\n[lib]\npath = \"core/root.rs\"\n\n[[bin]]\nname = \"tool\"\n\n[[bin]]\nname = 'extra'\n"},
                                              {"crate/core/root.rs", "#[test]\nfn in_root() {}\n"},
                                              {"crate/src/main.rs", "#[test]\nfn in_main() {}\n"},
                                              {"crate/src/bin/extra.rs", "#[test]\nfn in_extra() {}\n"},
                                              {"crate/tests/skipped.rs", "#[test]\nfn skipped() {}\n"}};
    files_found(s_files, 5u, "cargo_listed");
    const unsigned long long size = found_written();
    CLI_TEST_ASSERT_TEXT("crate/core/root.rs::in_root lib in_root -\ncrate/src/main.rs::in_main bin:tool in_main -\n"
                         "crate/src/bin/extra.rs::in_extra bin:extra in_extra -",
                         s_out, size);
}

void test_a_found_test_reports_its_line(void)
{
    package_found();
    TEST_ASSERT_GREATER_THAN_UINT(1u, s_tests.count);
    const unsigned int line = s_tests.found[1].test.line;
    CliCargoPiece *piece = s_tests.pieces;
    while (piece != NULL)
    {
        CliCargoPiece *const next = piece->next;
        g_cli_cells->persistent_buf_release(piece, &g_cli_cargo_cells);
        piece = next;
    }
    g_cli_cells->persistent_buf_release(s_tests.found, &g_cli_cargo_cells);
    *(&s_tests) = (CliCargoTests){NULL, 0u, 0u, NULL, 0ULL, 0};
    TEST_ASSERT_EQUAL_UINT(3u, line);
}

void test_the_tests_are_stored_in_grown_capacity_with_their_texts_in_one_piece(void)
{
    package_found();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(s_tests.count, s_tests.capacity);
    TEST_ASSERT_NOT_NULL(s_tests.pieces);
    TEST_ASSERT_NULL(s_tests.pieces->next);
    CliCargoPiece *piece = s_tests.pieces;
    while (piece != NULL)
    {
        CliCargoPiece *const next = piece->next;
        g_cli_cells->persistent_buf_release(piece, &g_cli_cargo_cells);
        piece = next;
    }
    g_cli_cells->persistent_buf_release(s_tests.found, &g_cli_cargo_cells);
    *(&s_tests) = (CliCargoTests){NULL, 0u, 0u, NULL, 0ULL, 0};
}

void test_the_searchs_cells_are_released_as_it_ends(void)
{
    package_found();
    CliCargoPiece *piece = s_tests.pieces;
    while (piece != NULL)
    {
        CliCargoPiece *const next = piece->next;
        g_cli_cells->persistent_buf_release(piece, &g_cli_cargo_cells);
        piece = next;
    }
    g_cli_cells->persistent_buf_release(s_tests.found, &g_cli_cargo_cells);
    *(&s_tests) = (CliCargoTests){NULL, 0u, 0u, NULL, 0ULL, 0};
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_cargo_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_cargo_cells.hw));
}

void test_a_rust_test_is_rust(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = "crate/src/lib.rs::at_top"));
}

void test_a_rust_file_is_rust(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = "crate/src/lib.rs"));
}

void test_a_python_test_is_not_rust(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = "tests/test_a.py::test_one"));
}

void test_a_passing_result_line(void)
{
    TEST_ASSERT_EQUAL_STRING("files::listing::one passed", result_of("test files::listing::one ... ok"));
}

void test_a_failing_result_line(void)
{
    TEST_ASSERT_EQUAL_STRING("two failed", result_of("test two - should panic ... FAILED"));
}

void test_an_ignored_result_line(void)
{
    TEST_ASSERT_EQUAL_STRING("three skipped", result_of("test three ... ignored, slow"));
}

void test_the_summary_is_no_result(void)
{
    TEST_ASSERT_EQUAL_STRING("-", result_of("test result: ok. 3 passed; 0 failed"));
}

void test_each_failures_message_and_panic_location_are_read(void)
{
    static const char s_report[] = "running 2 tests\ntest a::one ... FAILED\ntest a::two ... FAILED\n\nfailures:\n\n---- a::one stdout "
                                   "----\n\nthread 'a::one' panicked at src\\a.rs:12:9:\nassertion `left == right` failed\n  left: 1\n right: "
                                   "2\nnote: run with `RUST_BACKTRACE=1`\n\n---- a::two stdout ----\nthread 'a::two' panicked at 'old words', "
                                   "src/a.rs:20:5\n\nfailures:\n    a::one\n    a::two\n";
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_cargo_report_size_out.cargo_failures_each, CliCargoReportSizeOutCfg, .out = s_report, .size = sizeof(s_report) - 1u, .each = failure_recorded, .context = &out);
    CLI_TEST_ASSERT_TEXT("a::one: assertion `left == right` failed\n  left: 1\n right: 2 @ src/a.rs:12\na::two: old words @ src/a.rs:20", s_out, out.at);
}

void test_a_builds_first_error_and_its_location_are_read(void)
{
    static const char s_build_output[] = "   Compiling thing v0.1.0\nerror[E0308]: mismatched types\n --> src/lib.rs:3:18\n  |\n";
    char error[256];
    char path[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    long long line = 0ll;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cargo_report_size.cargo_build_error, CliCargoReportSizeCfg, .output = s_build_output, .size = sizeof(s_build_output) - 1u, .error = error, .capacity = sizeof(error), .length = &length, .path = path, .line = &line));
    CLI_TEST_ASSERT_TEXT("error[E0308]: mismatched types", error, length);
    TEST_ASSERT_EQUAL_STRING("src/lib.rs", path);
    TEST_ASSERT_EQUAL_INT64(3ll, line);
}
