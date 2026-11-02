// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_file_cell.c: a file read into a cell its caller allocates, with the zero bytes a word scan reads past the text, and a file
// read whole or in a piece into one cell of the owner the caller names
#include "ui/cli/src/command_line/cells.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_text[] = "the bytes of a file";

static char s_path[CLI_PATH_MAX_LEN];
static char s_empty[CLI_PATH_MAX_LEN];
static char s_missing[CLI_PATH_MAX_LEN];
static char s_broken[CLI_PATH_MAX_LEN];
static unsigned long long s_size;

// `count` bytes from `bytes`, at most the slack, are zero
static void assert_zero(const unsigned char *bytes, unsigned long long count)
{
    static const unsigned char s_zeros[CLI_FILE_SLACK];
    TEST_ASSERT_EQUAL_MEMORY(s_zeros, bytes, (size_t)count);
}

void setUp(void)
{
    char folder[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    cli_unity_folder("file_cell", folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "text.txt", .joined = s_path);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "empty.txt", .joined = s_empty);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "missing.txt", .joined = s_missing);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "broken.txt", .joined = s_broken);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = s_path, .bytes = s_text, .size = sizeof(s_text) - 1u, .fresh = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = s_empty, .bytes = "", .size = 0ull, .fresh = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = s_broken, .bytes = "a\xff", .size = 2ull, .fresh = 1));
    s_size = 1ull;
}

void tearDown(void)
{
}

void test_a_files_size_is_answered(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = s_path, .size_pointer = &s_size));
    TEST_ASSERT_EQUAL_UINT64(sizeof(s_text) - 1u, s_size);
}

void test_a_file_reads_complete_into_a_cell_that_held_other_bytes(void)
{
    const unsigned long long dirty = sizeof(s_text) - 1ull + CLI_FILE_SLACK;
    unsigned char *const used = g_cli_cells->temporary_buf_alloc(dirty, &g_checks_cells);
    memor.set(&(MemoriaCfg){.dst = used, .val = 0xa5, .bytes = (size_t)dirty});
    g_cli_cells->persistent_buf_release(used, &g_checks_cells);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = s_path, .size_pointer = &s_size));
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(s_size + CLI_FILE_SLACK, &g_checks_cells);
    TEST_ASSERT_NOT_NULL(cell);
    unsigned long long read = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = s_path, .from = 0ull, .bytes_bytes = cell, .count_u64 = s_size, .got = &read));
    TEST_ASSERT_EQUAL_UINT64(s_size, read);
    TEST_ASSERT_EQUAL_MEMORY(s_text, cell, (size_t)s_size);
    g_cli_cells->persistent_buf_release(cell, &g_checks_cells);
}

void test_a_released_cell_comes_off_the_callers_owner(void)
{
    const size_t before = embed_atomic_usize_load(&g_checks_cells.in_use);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = s_path, .size_pointer = &s_size));
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(s_size + CLI_FILE_SLACK, &g_checks_cells);
    TEST_ASSERT_NOT_NULL(cell);
    g_cli_cells->persistent_buf_release(cell, &g_checks_cells);
    TEST_ASSERT_EQUAL_UINT64(before, embed_atomic_usize_load(&g_checks_cells.in_use));
}

void test_an_empty_file_has_size_zero(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = s_empty, .size_pointer = &s_size));
    TEST_ASSERT_EQUAL_UINT64(0ull, s_size);
}

void test_an_empty_file_reads_no_bytes(void)
{
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(CLI_FILE_SLACK, &g_checks_cells);
    TEST_ASSERT_NOT_NULL(cell);
    unsigned long long read = 1ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = s_empty, .from = 0ull, .bytes_bytes = cell, .count_u64 = 0ull, .got = &read));
    TEST_ASSERT_EQUAL_UINT64(0ull, read);
    g_cli_cells->persistent_buf_release(cell, &g_checks_cells);
}

void test_a_missing_file_has_no_size(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = s_missing, .size_pointer = &s_size));
}

void test_a_file_read_complete_is_in_its_cell(void)
{
    unsigned char *const whole = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = s_path, .size_pointer = &s_size, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(whole);
    CLI_TEST_ASSERT_TEXT(s_text, whole, s_size);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = whole, .owner = &g_checks_cells);
}

void test_a_file_read_complete_is_followed_by_zero_slack(void)
{
    unsigned char *const whole = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = s_path, .size_pointer = &s_size, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(whole);
    assert_zero(whole + s_size, CLI_FILE_SLACK);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = whole, .owner = &g_checks_cells);
}

void test_a_file_read_complete_counts_to_the_named_owner_until_released(void)
{
    const size_t before = embed_atomic_usize_load(&g_checks_cells.in_use);
    unsigned char *const whole = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = s_path, .size_pointer = &s_size, .owner = &g_checks_cells);
    TEST_ASSERT_GREATER_THAN_UINT64(before, embed_atomic_usize_load(&g_checks_cells.in_use));
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = whole, .owner = &g_checks_cells);
    TEST_ASSERT_EQUAL_UINT64(before, embed_atomic_usize_load(&g_checks_cells.in_use));
}

void test_a_piece_contains_the_bytes_requested_for(void)
{
    unsigned char *const piece = EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = s_path, .from = 4ull, .count_u64 = 5ull, .size_pointer = &s_size, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(piece);
    CLI_TEST_ASSERT_TEXT("bytes", piece, s_size);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = piece, .owner = &g_checks_cells);
}

void test_a_piece_end_the_end_is_shorter_with_zero_slack(void)
{
    // a larger cell lands over the word before the one just released, which an allocation does not zero: the read zeroes the slack
    unsigned char *const piece = EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = s_path, .from = 4ull, .count_u64 = 5ull, .size_pointer = &s_size, .owner = &g_checks_cells);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = piece, .owner = &g_checks_cells);
    unsigned char *const end = EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = s_path, .from = sizeof(s_text) - 3u, .count_u64 = 64ull, .size_pointer = &s_size, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_EQUAL_UINT64(2ull, s_size);
    assert_zero(end + s_size, CLI_FILE_SLACK);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = end, .owner = &g_checks_cells);
}

void test_an_empty_file_read_complete_has_no_bytes(void)
{
    unsigned char *const nothing = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = s_empty, .size_pointer = &s_size, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(nothing);
    TEST_ASSERT_EQUAL_UINT64(0ull, s_size);
    assert_zero(nothing, CLI_FILE_SLACK);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = nothing, .owner = &g_checks_cells);
}

void test_a_missing_file_read_complete_is_null_with_size_zero(void)
{
    TEST_ASSERT_NULL(EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = s_missing, .size_pointer = &s_size, .owner = &g_checks_cells));
    TEST_ASSERT_EQUAL_UINT64(0ull, s_size);
}

void test_releasing_null_changes_nothing(void)
{
    const size_t before = embed_atomic_usize_load(&g_checks_cells.in_use);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = NULL, .owner = &g_checks_cells);
    TEST_ASSERT_EQUAL_UINT64(before, embed_atomic_usize_load(&g_checks_cells.in_use));
}

void test_bytes_that_are_not_utf8_are_refused(void)
{
    TEST_ASSERT_NULL(EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = s_broken, .size_pointer = &s_size, .owner = &g_checks_cells));
}

void test_bytes_that_are_utf8_are_read(void)
{
    unsigned char *const letters = EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = s_path, .size_pointer = &s_size, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(letters);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = letters, .owner = &g_checks_cells);
}
