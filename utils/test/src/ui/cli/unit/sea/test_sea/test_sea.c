// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_sea.c: Node single executables: a script's modules from outside Node are named, the fuse flips once, a PE signature at the end
// is removed, and an ELF note goes in a loaded segment past the others
#include "ui/cli/src/programs/sea.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define SIGNATURE_ENTRY (0x98u + 112u + 32u)

static unsigned char s_program[0x2000];
static CliSeaNote s_note;
static char s_why[512];
static VerbaLinea s_err_msg;

// `value` written little-endian, `width` bytes of it, at `at` of the program
static void number_put(unsigned long long at, unsigned long long value, unsigned int width)
{
    for (unsigned int each = 0u; each < width; each += 1u)
    {
        // a byte of the number, its low eight bits
        s_program[at + each] = (unsigned char)((value >> (8u * each)) & 0xffu);
    }
}

// the little-endian number `width` bytes long at `at` of `bytes`
static unsigned long long number_at(const unsigned char *bytes, unsigned long long at, unsigned int width)
{
    unsigned long long value = 0ull;
    for (unsigned int each = width; each > 0u; each -= 1u)
    {
        value = (value << 8u) | bytes[at + each - 1u];
    }
    return value;
}

// a PE program with a signature of 0x100 bytes at 0x300
static void signed_program_written(void)
{
    memor.set(&(MemoriaCfg){.dst = s_program, .val = 0, .bytes = 0x400u});
    memor.cpy(&(MemoriaCfg){.dst = s_program, .src = "MZ", .bytes = 2u});
    number_put(0x3Cu, 0x80u, 4u);
    memor.cpy(&(MemoriaCfg){.dst = s_program + 0x80u, .src = "PE\0\0", .bytes = 4u});
    number_put(0x98u, 0x20bu, 2u);
    number_put(SIGNATURE_ENTRY, 0x300u, 4u);
    number_put(SIGNATURE_ENTRY + 4u, 0x100u, 4u);
}

// the note made for an ELF program of two program headers
static void note_made(void)
{
    static const unsigned long long s_heads[2][6] = {{0x40u, 6u, 0x40u, 0x400040u, 112u, 112u}, {0x78u, 1u, 0u, 0x400000u, 0x2000u, 0x5000u}};
    memor.set(&(MemoriaCfg){.dst = s_program, .val = 0, .bytes = sizeof(s_program)});
    memor.cpy(&(MemoriaCfg){.dst = s_program,
                            .src = "\x7f"
                                   "ELF\x02\x01",
                            .bytes = 6u});
    number_put(0x20u, 0x40u, 8u);
    number_put(0x36u, 56u, 2u);
    number_put(0x38u, 2u, 2u);
    for (unsigned int head = 0u; head < 2u; head += 1u)
    {
        number_put(s_heads[head][0], s_heads[head][1], 4u);
        number_put(s_heads[head][0] + 8u, s_heads[head][2], 8u);
        number_put(s_heads[head][0] + 16u, s_heads[head][3], 8u);
        number_put(s_heads[head][0] + 32u, s_heads[head][4], 8u);
        number_put(s_heads[head][0] + 40u, s_heads[head][5], 8u);
    }
    TEST_ASSERT_TRUE(EMBED_CALL(cli_sea_err_msg.note, CliSeaErrMsgCfg, .program = s_program, .size = sizeof(s_program), .data_size = 5ull, .note = &s_note, .err_msg = &s_err_msg));
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ull};
}

void tearDown(void)
{
}

void test_a_scripts_modules_from_outside_node_are_named(void)
{
    static const char s_text[] = "const fs = require('fs');\nconst p = require(\"node:path\");\nconst x = require('./helper');"
                                 "\nimport y from 'left-pad';\nimport { readFile } from \"fs/promises\";\n"
                                 "const z = await import(`chalk`);\n";
    char found[8][CLI_NAME_MAX_LEN];
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_sea_table.outside, CliSeaCfg, .text = s_text, .found = found, .room = 8u));
    TEST_ASSERT_EQUAL_STRING("./helper", found[0]);
    TEST_ASSERT_EQUAL_STRING("chalk", found[1]);
    TEST_ASSERT_EQUAL_STRING("left-pad", found[2]);
}

void test_nodes_own_modules_are_not_named(void)
{
    char found[8][CLI_NAME_MAX_LEN];
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_sea_table.outside, CliSeaCfg, .text = "console.log(require('os').cpus().length);", .found = found, .room = 8u));
}

void test_a_script_that_requires_an_outside_module_is_refused_and_its_memory_released(void)
{
    static const char s_text[] = "const pad = require('left-pad');\n";
    char dir[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char out[CLI_PATH_MAX_LEN];
    char made[CLI_PATH_MAX_LEN];
    cli_unity_folder("sea", dir);
    cli_unity_file(dir, "main.js", s_text);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "main.js", .joined = path);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "main", .joined = out);
    const QuerySink sink = {NULL, NULL};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_sea_node.build, CliSeaNodeCfg, .node = "node", .main = path, .output = out, .module = 0, .sink = sink, .made = made, .err_msg = &s_err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_line_finish(&s_err_msg));
    CLI_TEST_ASSERT_CONTAINS("requires left-pad", s_why);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_sea_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_sea_cells.hw));
    TEST_ASSERT_LESS_THAN_UINT64(sizeof(s_text) * 4u, embed_atomic_usize_load(&g_cli_sea_cells.hw));
}

void test_the_fuse_is_found(void)
{
    static char s_fused[] = "head NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0 tail";
    unsigned long long at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_sea_err_msg_program_size.fuse_at, CliSeaErrMsgProgramSizeCfg, .program = (const unsigned char *)s_fused, .size = sizeof(s_fused) - 1u, .at = &at, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_CHAR('0', s_fused[at]);
}

void test_a_flipped_fuse_is_refused(void)
{
    static char s_fused[] = "head NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:1 tail";
    unsigned long long at = 0ull;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_sea_err_msg_program_size.fuse_at, CliSeaErrMsgProgramSizeCfg, .program = (const unsigned char *)s_fused, .size = sizeof(s_fused) - 1u, .at = &at, .err_msg = &s_err_msg));
}

void test_a_signature_at_the_end_is_found(void)
{
    unsigned long long found = 0ull;
    unsigned long long length = 0ull;
    signed_program_written();
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_sea_err_msg_program.signature, CliSeaErrMsgProgramCfg, .program = s_program, .size = 0x400u, .entry = &found, .length = &length, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_UINT64(SIGNATURE_ENTRY, found);
    TEST_ASSERT_EQUAL_UINT64(0x300ull, length);
}

void test_a_program_with_its_signature_removed_has_none(void)
{
    unsigned long long found = 0ull;
    unsigned long long length = 0ull;
    signed_program_written();
    memor.set(&(MemoriaCfg){.dst = s_program + SIGNATURE_ENTRY, .val = 0, .bytes = 8u});
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_sea_err_msg_program.signature, CliSeaErrMsgProgramCfg, .program = s_program, .size = 0x300u, .entry = &found, .length = &length, .err_msg = &s_err_msg));
}

void test_a_notes_segment_lies_past_the_others(void)
{
    note_made();
    TEST_ASSERT_EQUAL_UINT64(0x5000ull, s_note.region);
    TEST_ASSERT_EQUAL_UINT64(4ull * 56ull, s_note.table_size);
}

void test_the_program_headers_move_with_the_note(void)
{
    note_made();
    TEST_ASSERT_EQUAL_UINT64(6ull, number_at(s_note.table, 0u, 4u));
    TEST_ASSERT_EQUAL_UINT64(0x405000ull, number_at(s_note.table, 16u, 8u));
}

void test_a_loaded_segment_contains_the_note(void)
{
    note_made();
    TEST_ASSERT_EQUAL_UINT64(1ull, number_at(s_note.table, 2u * 56u, 4u));
    TEST_ASSERT_EQUAL_UINT64(0x5000ull, number_at(s_note.table, 2u * 56u + 8u, 8u));
}

void test_a_note_header_names_the_note(void)
{
    note_made();
    TEST_ASSERT_EQUAL_UINT64(4ull, number_at(s_note.table, 3u * 56u, 4u));
    TEST_ASSERT_EQUAL_UINT64(s_note.note_at, number_at(s_note.table, 3u * 56u + 8u, 8u));
}

void test_the_note_is_named_node_sea_blob_with_the_blob_after_it(void)
{
    note_made();
    TEST_ASSERT_EQUAL_UINT64(14ull, number_at(s_note.head, 0u, 4u));
    TEST_ASSERT_EQUAL_UINT64(5ull, number_at(s_note.head, 4u, 4u));
    TEST_ASSERT_EQUAL_UINT64(28ull, s_note.head_size);
    TEST_ASSERT_EQUAL_MEMORY("NODE_SEA_BLOB\0", s_note.head + 12u, 14u);
}
