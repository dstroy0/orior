// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_mapped.c: memory-mapped file reads: a mapped file reads as its bytes across page sizes, an empty file maps to no bytes,
// and a missing file does not map
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_folder[CLI_PATH_MAX_LEN];
static unsigned char s_bytes[(1u << 20u) + 4099u];

// a file of `length` patterned bytes written and mapped: the mapping holds exactly those bytes
static void assert_maps_as_written(unsigned long long length)
{
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "bytes.txt", .joined = full);
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        // the remainder of a division by 251 is a byte
        s_bytes[at] = (unsigned char)(at % 251ull);
    }
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = full, .bytes = s_bytes, .size = length));
    unsigned long long size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(full, &size);
    TEST_ASSERT_NOT_NULL(mapped);
    TEST_ASSERT_EQUAL_UINT64(length, size);
    TEST_ASSERT_EQUAL_MEMORY(s_bytes, mapped, length);
    qry_member_file_unmapped(mapped, size);
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("mapped", s_folder);
}

void tearDown(void)
{
}

void test_a_file_of_many_pages_maps_as_written(void)
{
    assert_maps_as_written(70000ull);
}

void test_a_file_end_a_mebibyte_maps_as_written(void)
{
    assert_maps_as_written((1ull << 20u) + 4099ull);
}

void test_an_empty_file_maps_to_no_bytes(void)
{
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "empty.txt", .joined = full);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = full, .bytes = "", .size = 0ull));
    unsigned long long size = 1ull;
    const unsigned char *const mapped = qry_member_file_mapped(full, &size);
    TEST_ASSERT_NOT_NULL(mapped);
    TEST_ASSERT_EQUAL_UINT64(0ull, size);
    qry_member_file_unmapped(mapped, size);
}

void test_a_missing_file_does_not_map(void)
{
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "absent.txt", .joined = full);
    unsigned long long size = 0ull;
    TEST_ASSERT_NULL(qry_member_file_mapped(full, &size));
}
