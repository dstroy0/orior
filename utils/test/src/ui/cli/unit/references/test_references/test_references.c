// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_references.c: the references of a name are listed by file from the tree, a file's row closes and opens its
// references, a reference's row names its place, the definition in effect is found, a rename writes the name in every
// file's view, a new name that is no name is refused, and a place on no name gives nothing
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/code/references.h"
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/editor/view.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_shape_text[] = "#ifndef SHAPE_H\n#define SHAPE_H\nint area(int side);\n#define SIDES 4\n#endif\n";
static const char s_area_text[] = "#include \"shape.h\"\nint area(int n)\n{\n    return n * n * SIDES;\n}\n";
static const char s_use_text[] =
    "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n";

static char s_tree[CLI_PATH_MAX_LEN];
static char s_why[CLI_ERR_MSG_MAX_LEN];
static VerbaLinea s_err_msg;
static int s_begun;

// the tree made once: a git repository of three C files under src
static void tree_begun(void)
{
    if (s_begun)
    {
        return;
    }
    s_begun = 1;
    cli_unity_folder("build/harness/references_tree", s_tree);
    const char *const init[2] = {"init", "-q"};
    cli_unity_git(s_tree, init, 2u);
    char src[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "src", .joined = src);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src);
    cli_unity_file(s_tree, "src/shape.h", s_shape_text);
    cli_unity_file(s_tree, "src/area.c", s_area_text);
    cli_unity_file(s_tree, "src/use.c", s_use_text);
    const char *const add[2] = {"add", "-A"};
    cli_unity_git(s_tree, add, 2u);
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_tree);
    // the tree's declarations read on the walk's thread, waited for as the window waits for them
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = s_tree);
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (cli_tree_unwalked() && (cli_clock_milliseconds() < last_ms))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
    TEST_ASSERT_FALSE_MESSAGE(cli_tree_unwalked(), "the tree is walked");
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ULL};
    tree_begun();
    cli_references_end();
}

void tearDown(void)
{
    cli_references_end();
}

// every view closed
static void views_closed(void)
{
    while (cli_view_count() != 0u)
    {
        cli_view_file_close(&(CliViewAtCfg){.at = cli_view_count() - 1u});
    }
}

// the references of `area` at its call in use.c found: what the find answered
static int area_found(void)
{
    return cli_references_find(
        &(CliReferencesPlaceCfg){.path = "src/use.c", .line = 3u, .col = 12u, .err_msg = &s_err_msg});
}

void test_the_references_of_a_name_are_listed_by_file(void)
{
    (void)mmgr_line_finish(&s_err_msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, area_found(), s_why);
    // three files and four references: a row a file and a row a reference
    TEST_ASSERT_EQUAL_UINT(7u, cli_references_rows());
    CliReferenceRow row;
    TEST_ASSERT_TRUE(cli_references_row(&(CliReferencesAtRowCfg){.at = 0u, .row = &row}));
    TEST_ASSERT_EQUAL_UINT(CLI_REFERENCES_ROW_FILE, row.type);
    TEST_ASSERT_TRUE(row.open);
    TEST_ASSERT_EQUAL_UINT64(4ull, row.folder_length);
    CLI_TEST_ASSERT_TEXT("src/", row.path, row.folder_length);
    unsigned long long length = 0ull;
    const char *const report = cli_references_report(&(CliReferencesLengthCfg){.length_out = &length});
    CLI_TEST_ASSERT_TEXT("4 references of area in 3 files", report, length);
    TEST_ASSERT_FALSE(cli_references_row(&(CliReferencesAtRowCfg){.at = 7u, .row = &row}));
}

void test_a_reference_row_names_its_place_and_marks_the_name(void)
{
    TEST_ASSERT_EQUAL_INT(1, area_found());
    CliReferenceRow row;
    TEST_ASSERT_TRUE(cli_references_row(&(CliReferencesAtRowCfg){.at = 1u, .row = &row}));
    TEST_ASSERT_EQUAL_UINT(CLI_REFERENCES_ROW_PLACE, row.type);
    TEST_ASSERT_EQUAL_UINT64(4ull, row.match_length);
    CLI_TEST_ASSERT_TEXT("area", row.text + row.match_at, row.match_length);
    TEST_ASSERT_EQUAL_UINT64(row.to - row.col, row.match_length);
    // a press on a reference's row opens its file at its place
    CliReferenceRow pressed;
    TEST_ASSERT_TRUE(cli_references_press(&(CliReferencesAtRowCfg){.at = 1u, .row = &pressed}));
    TEST_ASSERT_EQUAL_UINT64(row.line, pressed.line);
}

void test_a_file_row_pressed_closes_and_opens_its_references(void)
{
    TEST_ASSERT_EQUAL_INT(1, area_found());
    CliReferenceRow first;
    TEST_ASSERT_TRUE(cli_references_row(&(CliReferencesAtRowCfg){.at = 0u, .row = &first}));
    const unsigned int count = first.count;
    CliReferenceRow row;
    TEST_ASSERT_FALSE(cli_references_press(&(CliReferencesAtRowCfg){.at = 0u, .row = &row}));
    TEST_ASSERT_EQUAL_UINT(7u - count, cli_references_rows());
    TEST_ASSERT_TRUE(cli_references_row(&(CliReferencesAtRowCfg){.at = 0u, .row = &row}));
    TEST_ASSERT_FALSE(row.open);
    TEST_ASSERT_TRUE(cli_references_row(&(CliReferencesAtRowCfg){.at = 1u, .row = &row}));
    TEST_ASSERT_EQUAL_UINT(CLI_REFERENCES_ROW_FILE, row.type);
    TEST_ASSERT_FALSE(cli_references_press(&(CliReferencesAtRowCfg){.at = 0u, .row = &row}));
    TEST_ASSERT_EQUAL_UINT(7u, cli_references_rows());
}

void test_the_definition_is_the_declaration_the_hover_shows_first(void)
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    TEST_ASSERT_EQUAL_INT(1, cli_references_definition(&(CliReferencesPlaceCfg){.root = s_tree,
                                                                                .path = "src/use.c",
                                                                                .line = 3u,
                                                                                .name = "area",
                                                                                .path_out = path,
                                                                                .line_out = &line,
                                                                                .col_out = &col}));
    TEST_ASSERT_EQUAL_STRING("src/area.c", path);
    TEST_ASSERT_EQUAL_UINT64(1ull, line);
    TEST_ASSERT_EQUAL_UINT64(4ull, col);
    TEST_ASSERT_EQUAL_INT(0, cli_references_definition(&(CliReferencesPlaceCfg){.root = s_tree,
                                                                                .path = "src/use.c",
                                                                                .line = 3u,
                                                                                .name = "nowhere",
                                                                                .path_out = path,
                                                                                .line_out = &line,
                                                                                .col_out = &col}));
}

void test_a_place_on_no_name_gives_no_references(void)
{
    TEST_ASSERT_EQUAL_INT(0, cli_references_find(&(CliReferencesPlaceCfg){
                                 .path = "src/use.c", .line = 2u, .col = 0u, .err_msg = &s_err_msg}));
    TEST_ASSERT_EQUAL_UINT(0u, cli_references_rows());
    unsigned long long length = 0ull;
    const char *const report = cli_references_report(&(CliReferencesLengthCfg){.length_out = &length});
    CLI_TEST_ASSERT_TEXT("No references", report, length);
}

void test_a_rename_writes_the_name_in_every_files_view(void)
{
    views_closed();
    const int opened = cli_view_file_open(s_tree, "src/use.c", 20u, 80u, NULL, &s_err_msg);
    (void)mmgr_line_finish(&s_err_msg);
    TEST_ASSERT_TRUE_MESSAGE(opened >= 0, s_why);
    unsigned int files = 0u;
    (void)mmgr_line_finish(&s_err_msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(4,
                                  cli_references_rename(&(CliReferencesPlaceCfg){.path = "src/use.c",
                                                                                 .line = 3u,
                                                                                 .col = 12u,
                                                                                 .name = "extent",
                                                                                 .root = s_tree,
                                                                                 .rows = 20u,
                                                                                 .columns = 80u,
                                                                                 .files_out = &files,
                                                                                 .err_msg = &s_err_msg}),
                                  s_why);
    TEST_ASSERT_EQUAL_UINT(3u, files);
    // the save writes the two files with no view and closes them again; use.c stays the view displayed, changed
    TEST_ASSERT_EQUAL_UINT(1u, cli_view_count());
    TEST_ASSERT_EQUAL_INT(0, cli_view_displayed());
    TEST_ASSERT_EQUAL_UINT(0u, cli_references_rows());
    TEST_ASSERT_TRUE(cli_view_file_open(s_tree, "src/area.c", 20u, 80u, NULL, &s_err_msg) >= 0);
    TEST_ASSERT_TRUE(cli_view_file_open(s_tree, "src/shape.h", 20u, 80u, NULL, &s_err_msg) >= 0);
    char path[CLI_PATH_MAX_LEN];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char key[CLI_NAME_MAX_LEN];
        TEST_ASSERT_TRUE(cli_view_key(&(CliViewAtCfg){.at = at, .key_out = key}) &&
                         cli_view_path_at(&(CliViewAtCfg){.at = at, .path_out = path}));
        const CliViewFrame *const frame = cli_unity_frame(key);
        TEST_ASSERT_NOT_NULL(frame);
        const char *const text = frame->bytes;
        int extent_found = 0;
        for (unsigned int row = 0u; row < frame->count; row += 1u)
        {
            const char *const line = text + frame->row[row].text;
            for (unsigned long long col = 0ull; col + 6ull <= frame->row[row].length; col += 1ull)
            {
                extent_found =
                    extent_found || (memor.cmp(&(MemoriaCfg){.src = line + col, .other = "extent", .bytes = 6u}) == 0);
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(extent_found, path);
    }
    views_closed();
}

void test_a_rename_to_no_name_is_refused(void)
{
    unsigned int files = 0u;
    TEST_ASSERT_EQUAL_INT(0, cli_references_rename(&(CliReferencesPlaceCfg){.path = "src/use.c",
                                                                            .line = 3u,
                                                                            .col = 12u,
                                                                            .name = "9x",
                                                                            .root = s_tree,
                                                                            .rows = 20u,
                                                                            .columns = 80u,
                                                                            .files_out = &files,
                                                                            .err_msg = &s_err_msg}));
    TEST_ASSERT_TRUE(mmgr_line_finish(&s_err_msg) > 0ull);
}
