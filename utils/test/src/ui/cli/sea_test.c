// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea_test.c: the single executable's check: a script's modules from outside Node are named, the fuse turns once, a
// signature at the end is cut away, and a note goes in a loaded segment past the others
#include "../../../../../ui/cli/src/programs/sea.h"
#include "cli_check.h"

static unsigned char s_program[0x2000];

// `value` written little-endian, `width` bytes of it, at `at` of `bytes`
static void sea_put(unsigned char *bytes, unsigned long long at, unsigned long long value, unsigned int width)
{
    for (unsigned int each = 0u; each < width; each += 1u)
    {
        // a byte of the number, its low eight bits kept
        bytes[at + each] = (unsigned char)((value >> (8u * each)) & 0xffu);
    }
}

// the little-endian number `width` bytes long at `at` of `bytes`
static unsigned long long sea_number(const unsigned char *bytes, unsigned long long at, unsigned int width)
{
    unsigned long long value = 0ull;
    for (unsigned int each = width; each > 0u; each -= 1u)
    {
        value = (value << 8u) | bytes[at + each - 1u];
    }
    return value;
}

// a script's modules from outside Node are named
static void sea_outside(void)
{
    static const char s_text[] =
        "const fs = require('fs');\nconst p = require(\"node:path\");\nconst x = require('./helper');"
        "\nimport y from 'left-pad';\nimport { readFile } from \"fs/promises\";\n"
        "const z = await import(`chalk`);\n";
    char found[8][CLI_NAME_LONGEST];
    const unsigned int count = cli_sea_outside(s_text, found, 8u);
    cli_check((count == 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = found[0], .other = "./helper", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = found[1], .other = "chalk", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = found[2], .other = "left-pad", .cap = CLI_CELLS_POOL})),
              "a script's modules from outside Node are named");
    cli_check(cli_sea_outside("console.log(require('os').cpus().length);", found, 8u) == 0u, "and Node's own are not");
}

// a script that asks for a module from outside Node is refused before node runs, its text read into a cell of its
// size and given back
static void sea_refused(void)
{
    static const char s_text[] = "const pad = require('left-pad');\n";
    char dir[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char out[CLI_PATH_LONGEST];
    char made[CLI_PATH_LONGEST];
    cli_check_folder("sea", dir);
    cli_full(dir, "main.js", path);
    cli_full(dir, "main", out);
    (void)query_file_written(path, s_text, sizeof(s_text) - 1u);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    const QuerySink say = {NULL, NULL};
    cli_check(
        !cli_sea_build("node", path, out, 0, say, made, &said) && (cli_line_closed(&said) != 0ull) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = sizeof(why), .other = "asks for left-pad", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (g_cli_sea_cells.holds == 0ull) && (g_cli_sea_cells.hw != 0ull) &&
            (g_cli_sea_cells.hw < (sizeof(s_text) * 4u)),
        "a script that asks for a module outside Node is refused, read into a cell of its size");
}

// the fuse turns once
static void sea_fuse(void)
{
    static char s_fused[] = "head NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0 tail";
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned long long at = 0ull;
    const unsigned long long size = sizeof(s_fused) - 1u;
    cli_check(cli_sea_fuse_at((const unsigned char *)s_fused, size, &at, &said) && (s_fused[at] == '0'),
              "the fuse is found");
    s_fused[at] = '1';
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_fused, .cap = CLI_CELLS_POOL, .other = "b2:1 tail", .other_cap = CLI_CELLS_POOL}) != NULL,
              "and turned");
    cli_check(!cli_sea_fuse_at((const unsigned char *)s_fused, size, &at, &said), "a fuse turned turns no more");
}

// a signature at the end is cut away
static void sea_signature(void)
{
    memor.set(&(MemoriaCfg){.dst = s_program, .val = 0, .bytes = 0x400u});
    memor.cpy(&(MemoriaCfg){.dst = s_program, .src = "MZ", .bytes = 2u});
    sea_put(s_program, 0x3Cu, 0x80u, 4u);
    memor.cpy(&(MemoriaCfg){.dst = s_program + 0x80u, .src = "PE\0\0", .bytes = 4u});
    sea_put(s_program, 0x98u, 0x20bu, 2u);
    const unsigned long long entry = 0x98u + 112u + 32u;
    sea_put(s_program, entry, 0x300u, 4u);
    sea_put(s_program, entry + 4u, 0x100u, 4u);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned long long found = 0ull;
    unsigned long long length = 0ull;
    cli_check((cli_sea_signature(s_program, 0x400u, &found, &length, &said) == 1) && (found == entry) &&
                  (length == 0x300u),
              "a signature at the end is cut away");
    memor.set(&(MemoriaCfg){.dst = s_program + entry, .val = 0, .bytes = 8u});
    cli_check(cli_sea_signature(s_program, 0x300u, &found, &length, &said) == 0, "and a program cut has none");
}

// a note goes in a loaded segment past the others
static void sea_note(void)
{
    static CliSeaNote s_note;
    memor.set(&(MemoriaCfg){.dst = s_program, .val = 0, .bytes = sizeof(s_program)});
    memor.cpy(&(MemoriaCfg){.dst = s_program,
                            .src = "\x7f"
                                   "ELF\x02\x01",
                            .bytes = 6u});
    sea_put(s_program, 0x20u, 0x40u, 8u);
    sea_put(s_program, 0x36u, 56u, 2u);
    sea_put(s_program, 0x38u, 2u, 2u);
    const unsigned long long heads[2][6] = {{0x40u, 6u, 0x40u, 0x400040u, 112u, 112u},
                                            {0x78u, 1u, 0u, 0x400000u, 0x2000u, 0x5000u}};
    for (unsigned int head = 0u; head < 2u; head += 1u)
    {
        sea_put(s_program, heads[head][0], heads[head][1], 4u);
        sea_put(s_program, heads[head][0] + 8u, heads[head][2], 8u);
        sea_put(s_program, heads[head][0] + 16u, heads[head][3], 8u);
        sea_put(s_program, heads[head][0] + 32u, heads[head][4], 8u);
        sea_put(s_program, heads[head][0] + 40u, heads[head][5], 8u);
    }
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_sea_note(s_program, sizeof(s_program), 5ull, &s_note, &said), "a note is made");
    cli_check((s_note.region == 0x5000u) && (s_note.table_size == 4u * 56u), "its segment stands past the others");
    cli_check((sea_number(s_note.table, 0u, 4u) == 6u) && (sea_number(s_note.table, 16u, 8u) == 0x405000u),
              "the program headers move with it");
    cli_check((sea_number(s_note.table, 2u * 56u, 4u) == 1u) &&
                  (sea_number(s_note.table, 2u * 56u + 8u, 8u) == 0x5000u),
              "a loaded segment holds it");
    cli_check((sea_number(s_note.table, 3u * 56u, 4u) == 4u) &&
                  (sea_number(s_note.table, 3u * 56u + 8u, 8u) == s_note.note_at),
              "and a note's header names it");
    cli_check((sea_number(s_note.head, 0u, 4u) == 14u) && (sea_number(s_note.head, 4u, 4u) == 5u) &&
                  (s_note.head_size == 28u) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_note.head + 12u, .other = "NODE_SEA_BLOB\0", .bytes = 14u}) == 0),
              "the note is named NODE_SEA_BLOB, the blob after it");
}

int main(void)
{
    sea_outside();
    sea_refused();
    sea_fuse();
    sea_signature();
    sea_note();
    return cli_checked();
}
