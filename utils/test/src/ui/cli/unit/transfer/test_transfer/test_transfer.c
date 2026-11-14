// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_transfer.c: shared regions: a region passed over contains what was written, a region declared larger than it is is refused,
// a region shared on Windows reads and cannot be written, and a region on Linux is sealed against every change
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "ui/cli/src/comms/transfer.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define REGION_BYTES (CLI_TRANSFER_SMALL * 3u + 7u)

static unsigned char s_bytes[REGION_BYTES];
static char s_error[512];
static VerbaLinea s_err_msg;
static CliRegion s_region;
static CliRegion s_opened;

// `length` bytes of the pattern into s_bytes
static void pattern_written(unsigned long long length)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        // the pattern's byte is below 251
        s_bytes[at] = (unsigned char)((at * 31ull) % 251ull);
    }
}

// the region opened again as another process opens it: 1, or 0 with the reason written
static int region_opened(unsigned long long length)
{
#if defined(_WIN32)
    return EMBED_CALL(cli_region_alloc, CliTransferRegionCfg, .region = &s_opened, .process = cli_process_own(), .handle = s_region.section, .length = length, .err_msg = &s_err_msg);
#else
    // the region's descriptor is stored in its section, an int's worth
    return EMBED_CALL(cli_transfer_region.region_receive, CliTransferRegionCfg, .region = &s_opened, .handle = (unsigned long long)dup((int)s_region.section), .length = length, .err_msg = &s_err_msg);
#endif
}

void setUp(void)
{
    cli_unity_begun();
    memor.set(&(MemoriaCfg){.dst = &s_region, .val = 0, .bytes = sizeof(s_region)});
    memor.set(&(MemoriaCfg){.dst = &s_opened, .val = 0, .bytes = sizeof(s_opened)});
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
    pattern_written(REGION_BYTES);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_transfer_region_bytes.region_with, CliTransferRegionBytesCfg, .region = &s_region, .bytes = s_bytes, .size = REGION_BYTES, .err_msg = &s_err_msg));
}

void tearDown(void)
{
    EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &s_region);
    EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &s_opened);
}

void test_a_region_contains_what_was_written(void)
{
    TEST_ASSERT_EQUAL_UINT64(REGION_BYTES, s_region.length);
    TEST_ASSERT_EQUAL_MEMORY(s_bytes, s_region.view, REGION_BYTES);
}

void test_a_region_opens_as_another_process_opens_it(void)
{
    TEST_ASSERT_TRUE(region_opened(REGION_BYTES));
    TEST_ASSERT_EQUAL_MEMORY(s_bytes, s_opened.view, REGION_BYTES);
}

void test_a_region_outlives_the_handle_it_was_created_by(void)
{
    TEST_ASSERT_TRUE(region_opened(REGION_BYTES));
    EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &s_region);
    TEST_ASSERT_NOT_NULL(s_opened.view);
    TEST_ASSERT_EQUAL_MEMORY(s_bytes, s_opened.view, REGION_BYTES);
}

void test_a_region_declared_larger_than_it_is_is_refused(void)
{
    TEST_ASSERT_FALSE(region_opened(1ull << 28u));
    TEST_ASSERT_NOT_EQUAL(0ull, s_err_msg.at);
}

void test_windows_a_shared_region_cannot_be_mapped_for_writing(void)
{
#if defined(_WIN32)
    unsigned long long shared = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_region_free, CliTransferCfg, .region_read = &s_region, .process = cli_process_own(), .theirs = &shared, .err_msg = &s_err_msg));
    // the handle was made in this process, and a view the system refuses is NULL
    void *const written = MapViewOfFile((HANDLE)(uintptr_t)shared, FILE_MAP_WRITE, 0, 0, 100);
    if (written != NULL)
    {
        (void)UnmapViewOfFile(written);
    }
    TEST_ASSERT_NULL(written);
#else
    TEST_IGNORE_MESSAGE("a Windows section");
#endif
}

void test_windows_a_shared_region_reads_what_it_contains(void)
{
#if defined(_WIN32)
    unsigned long long shared = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_region_free, CliTransferCfg, .region_read = &s_region, .process = cli_process_own(), .theirs = &shared, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_transfer_region.region_receive, CliTransferRegionCfg, .region = &s_opened, .handle = shared, .length = REGION_BYTES, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_MEMORY(s_bytes, s_opened.view, REGION_BYTES);
#else
    TEST_IGNORE_MESSAGE("a Windows section");
#endif
}

void test_linux_a_sealed_region_cannot_shrink(void)
{
#if defined(__linux__)
    // the region's descriptor is stored in its section, an int's worth
    TEST_ASSERT_NOT_EQUAL(0, ftruncate((int)s_region.section, 10));
#else
    TEST_IGNORE_MESSAGE("a Linux memfd seal");
#endif
}

void test_linux_a_sealed_region_cannot_grow(void)
{
#if defined(__linux__)
    // the region's descriptor is stored in its section, an int's worth
    TEST_ASSERT_NOT_EQUAL(0, ftruncate((int)s_region.section, (off_t)REGION_BYTES + 4096));
#else
    TEST_IGNORE_MESSAGE("a Linux memfd seal");
#endif
}

void test_linux_a_sealed_region_cannot_be_written(void)
{
#if defined(__linux__)
    // the region's descriptor is stored in its section, an int's worth
    TEST_ASSERT_EQUAL_INT(-1, (int)pwrite((int)s_region.section, s_bytes, 1u, 0));
#else
    TEST_IGNORE_MESSAGE("a Linux memfd seal");
#endif
}
