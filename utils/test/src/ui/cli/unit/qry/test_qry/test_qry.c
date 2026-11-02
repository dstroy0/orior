// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_qry.c: a finished run is read whole and then from where a read stopped, a run's bytes open as its names each joined, a tree's
// runs are listed, and a lesson line is read and any other skipped
#include "src/cu/includes/file_defs/qry/qry.h"
#include "src/cu/includes/file_defs/qry/qry_read.h"
#include "ui/cli/src/runs/qry.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static unsigned char s_run[4096];
static char s_out[1u << 16u];
static char s_error[512];
static char s_path[CLI_PATH_MAX_LEN];
static unsigned long long s_first_size;
static const CliJson *s_json;
static unsigned int s_read;
static unsigned int s_one;
static unsigned int s_panic;

// a record of `name` containing `blob`, written at `time` with `priority`, placed after the `at` bytes of s_run: the bytes after it
static unsigned long long record_append(unsigned long long at, const char *name, const char *blob, unsigned long long time, unsigned int priority)
{
    const unsigned long long name_size = CLI_UNITY_LENGTH(name);
    const unsigned long long blob_size = CLI_UNITY_LENGTH(blob);
    const unsigned long long filled = 24ull + name_size + blob_size;
    const unsigned long long whole = ((filled + 7ull) & ~7ull) + 8ull;
    unsigned char *const bytes = s_run + at;
    memor.set(&(MemoriaCfg){.dst = bytes, .val = 0, .bytes = whole});
    const unsigned long long fields[4] = {time, blob_size, name_size, priority};
    const unsigned int widths[4] = {8u, 8u, 4u, 4u};
    unsigned int place = 0u;
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        for (unsigned int each = 0u; each < widths[field]; each += 1u)
        {
            // a byte of the field, its low eight bits
            bytes[place + each] = (unsigned char)((fields[field] >> (8u * each)) & 0xffu);
        }
        place += widths[field];
    }
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u, .src = name, .bytes = name_size});
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u + name_size, .src = blob, .bytes = blob_size});
    const unsigned long long crc = qry_record_crc(bytes, whole - 8ull);
    for (unsigned int each = 0u; each < 8u; each += 1u)
    {
        // a byte of the CRC, its low eight bits
        bytes[whole - 8ull + each] = (unsigned char)((crc >> (8u * each)) & 0xffu);
    }
    return at + whole;
}

// a run of three records, two names: its size
static unsigned long long two_names_written(void)
{
    unsigned long long size = record_append(0ull, "b.txt", "1", 1ull, QRY_ORDINARY);
    size = record_append(size, "a.txt", "2", 2ull, QRY_ORDINARY);
    return record_append(size, "b.txt", "3", 3ull, QRY_ORDINARY);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_runs_names_come_in_the_order_of_their_first_records(void)
{
    const unsigned long long size = two_names_written();
    char names[4][CLI_NAME_MAX_LEN];
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_qry_data.member_names, CliQryDataCfg, .data = s_run, .size = size, .names = names, .capacity = 4u));
    TEST_ASSERT_EQUAL_STRING("b.txt", names[0]);
    TEST_ASSERT_EQUAL_STRING("a.txt", names[1]);
}

void test_a_names_records_are_joined(void)
{
    const unsigned long long size = two_names_written();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_qry_out.member_join, CliQryOutCfg, .data = s_run, .size = size, .name = "b.txt", .out = &out);
    CLI_TEST_ASSERT_TEXT("13", s_out, out.at);
}

void test_another_names_records_are_joined(void)
{
    const unsigned long long size = two_names_written();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_qry_out.member_join, CliQryOutCfg, .data = s_run, .size = size, .name = "a.txt", .out = &out);
    CLI_TEST_ASSERT_TEXT("2", s_out, out.at);
}

