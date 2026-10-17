// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_funnel_check.c: the vendor's tables read off a capture of its disassembler
//
//   records    a capture's records are read in order, each its addresses and its bytes, and the reading ends at a line
//   that is no record archive    an archive is its own byte count, a zero and a first block of TABLES_BLOCK; a block of
//   another size
//              is no archive
//   unpacked   an archive's LZ4 blocks unpack with one window: a match reaches into the block before, and a match
//              longer than its offset repeats what it copies
//   tables     the instruction set is the first archive unpacked and the latencies the copies out of the buffer
//              that begins with OPERATION SETS, a block of that buffer passed over
//
//     tables_funnel_check
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/tables/funnel.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

static void word_put(unsigned char *at, unsigned long long value)
{
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        at[byte] = (unsigned char)((value >> (8u * byte)) & 0xffu);
    }
}

// the archive of three blocks: the first TABLES_BLOCK literal bytes, the head and dots; the second one sequence,
// `CLASS "a"\n` and a match twice its length over it; the third a match reaching back to the archive's first byte
static const char s_head[] = "ARCHITECTURE \"Test\"\n";
static const char s_class[] = "CLASS \"a\"\n";

static size_t archive_made(unsigned char *bytes)
{
    size_t at = 16u;
    // block 1: the token's literal nibble at 15, the rest of the count in bytes of 255 and a last one
    const size_t literals = TABLES_BLOCK;
    bytes[at] = 0xf0u;
    at += 1u;
    size_t left = literals - 15u;
    while (left >= 255u)
    {
        bytes[at] = 255u;
        at += 1u;
        left -= 255u;
    }
    bytes[at] = (unsigned char)left;
    at += 1u;
    memcpy(bytes + at, s_head, sizeof(s_head) - 1u);
    memset(bytes + at + sizeof(s_head) - 1u, '.', literals - (sizeof(s_head) - 1u));
    at += literals;
    word_put(bytes + 8u, TABLES_BLOCK);
    word_put(bytes + 12u, (unsigned long long)(at - 16u));
    // block 2: 10 literals and a match of 20 at offset 10, the count 16 past the nibble's 4 as one more byte
    const size_t block2 = at;
    at += 8u;
    bytes[at] = 0xafu;
    at += 1u;
    memcpy(bytes + at, s_class, 10u);
    at += 10u;
    bytes[at] = 10u;
    bytes[at + 1u] = 0u;
    bytes[at + 2u] = 1u;
    at += 3u;
    word_put(bytes + block2, 30ull);
    word_put(bytes + block2 + 4u, (unsigned long long)(at - block2 - 8u));
    // block 3: no literal and a match of 12 at the offset of the whole text so far
    const size_t block3 = at;
    at += 8u;
    bytes[at] = 0x08u;
    bytes[at + 1u] = (unsigned char)((TABLES_BLOCK + 30u) & 0xffu);
    bytes[at + 2u] = (unsigned char)(((TABLES_BLOCK + 30u) >> 8u) & 0xffu);
    at += 3u;
    word_put(bytes + block3, 12ull);
    word_put(bytes + block3 + 4u, 3ull);
    word_put(bytes, (unsigned long long)at);
    word_put(bytes + 4u, 0ull);
    return at;
}

static size_t record_put(unsigned char *capture, size_t at, unsigned long long to, unsigned long long from,
                         const unsigned char *bytes, size_t size)
{
    const int length = sprintf((char *)capture + at, "\n<0x%llx 0x%llx %zu>\n", to, from, size);
    at += (size_t)length;
    memcpy(capture + at, bytes, size);
    return at + size;
}

int main(void)
{
    static unsigned char s_archive[TABLES_BLOCK + 256u];
    static unsigned char s_capture[2u * TABLES_BLOCK + 2048u];
    static unsigned char s_expected[TABLES_BLOCK + 64u];
    const size_t archive = archive_made(s_archive);
    check_that(tables_archive(s_archive, archive), "archive: its own count, a zero and a first block of TABLES_BLOCK");
    unsigned char other[32];
    memcpy(other, s_archive, 16u);
    word_put(other, 32ull);
    word_put(other + 8u, 100ull);
    check_that(!tables_archive(other, 32u), "archive: a first block of another size is no archive");

    TablesText text = {NULL, 0u, 0u};
    check_that(tables_unpacked(s_archive, archive, &text), "unpacked: the three blocks unpack");
    memcpy(s_expected, s_head, sizeof(s_head) - 1u);
    memset(s_expected + sizeof(s_head) - 1u, '.', TABLES_BLOCK - (sizeof(s_head) - 1u));
    size_t expected = TABLES_BLOCK;
    for (unsigned int times = 0u; times < 3u; times += 1u)
    {
        memcpy(s_expected + expected, s_class, 10u);
        expected += 10u;
    }
    memcpy(s_expected + expected, "ARCHITECTURE", 12u);
    expected += 12u;
    check_that((text.length == expected) && (memcmp(text.bytes, s_expected, expected) == 0),
               "unpacked: a match longer than its offset repeats, and one reaches into the block before");
    tables_text_freed(&text);

    // the capture: a copy out of the latencies' buffer, a copy of other bytes, the archive as a block, a block of
    // the latencies' buffer, then a last copy out of it
    const unsigned char first[] = "\nOPERATION SETS\n    int_pipe = {";
    const unsigned char middle[] = "no table here";
    const unsigned char last[] = "A,B};\n";
    size_t at = 0u;
    at = record_put(s_capture, at, 0x2000ull, 0x1000ull, first, sizeof(first) - 1u);
    at = record_put(s_capture, at, 0x3000ull, 0x4000ull, middle, sizeof(middle) - 1u);
    at = record_put(s_capture, at, 0x5000ull, 0x5000ull, s_archive, archive);
    at = record_put(s_capture, at, 0x1000ull, 0x1000ull, first, sizeof(first) - 1u);
    at = record_put(s_capture, at, 0x2100ull, 0x1000ull, last, sizeof(last) - 1u);
    size_t read_at = 0u;
    TablesRecord record;
    unsigned int records = 0u;
    while (tables_record_read(s_capture, at, &read_at, &record))
    {
        records += 1u;
        if (records == 1u)
        {
            check_that((record.to == 0x2000ull) && (record.from == 0x1000ull) && (record.size == sizeof(first) - 1u) &&
                           (memcmp(record.bytes, first, record.size) == 0),
                       "records: the first record's addresses and bytes");
        }
    }
    check_that((records == 5u) && (read_at == at), "records: five records read to the capture's end");

    TablesText instructions = {NULL, 0u, 0u};
    TablesText latencies = {NULL, 0u, 0u};
    check_that(tables_funnelled(s_capture, at, &instructions, &latencies) == 2u, "tables: both tables found");
    check_that((instructions.length == expected) && (memcmp(instructions.bytes, s_expected, expected) == 0),
               "tables: the instruction set is the archive unpacked");
    const char *const want = "\nOPERATION SETS\n    int_pipe = {A,B};\n";
    check_that((latencies.length == strlen(want)) && (memcmp(latencies.bytes, want, latencies.length) == 0),
               "tables: the latencies are the copies out of their buffer, which begins with a line end, its block "
               "passed over");
    tables_text_freed(&instructions);
    tables_text_freed(&latencies);

    printf("tables_funnel_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
