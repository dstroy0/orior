// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_cell_test.c: a file read straight into a cell its caller takes: the caller asks the size, takes a cell of that
// size and the zero bytes a word walk reads past the text, and reads into it. The file stands whole with zero after it
// in a cell that stored other bytes before, a file of no bytes reads as none, a file that is not there has no size, and
// every byte comes off the caller's owner as the caller gives the cell back; and a file taken whole or in a piece in one
// cell of the owner the caller names
#include "../../../../../ui/cli/src/command_line/cells.h"
#include "cli_check.h"

// the zero bytes the caller asks for past the text
#define FILE_CELL_SLACK CLI_FILE_SLACK

static const char s_text[] = "the bytes of a file";

// `path` made from the check's folder and `name`
static void file_path_made(const char *folder, const char *name, char *path)
{
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_put(&line, name);
    (void)mmgr_line_finish(&line);
}

// whether `count` bytes from `bytes`, at most the slack, are zero
static int file_zero(const unsigned char *bytes, unsigned long long count)
{
    static const unsigned char s_zeros[CLI_FILE_SLACK];
    return memor.cmp(&(MemoriaCfg){.src = bytes, .other = s_zeros, .bytes = (size_t)count}) == 0;
}

// a file taken whole, in a piece and as UTF-8, each one cell of the owner named and given back to it
static void file_taken_checks(const char *path, const char *empty, const char *missing, const char *folder)
{
    const size_t before = embed_atomic_usize_load(&g_checks_cells.in_use);
    unsigned long long size = 1ull;
    unsigned char *const whole = cli_file_taken(path, &size, &g_checks_cells);
    cli_check((whole != NULL) && (size == (sizeof(s_text) - 1u)) &&
                  (memor.cmp(&(MemoriaCfg){.src = whole, .other = s_text, .bytes = (size_t)size}) == 0),
              "a file taken is whole in its cell");
    cli_check((whole != NULL) && file_zero(whole + size, CLI_FILE_SLACK), "with the slack after it zero");
    cli_check(embed_atomic_usize_load(&g_checks_cells.in_use) > before, "and the cell counts to the owner named");
    cli_file_given(whole, &g_checks_cells);
    cli_check(embed_atomic_usize_load(&g_checks_cells.in_use) == before, "and comes off it as it is given back");

    unsigned char *const piece = cli_file_piece_taken(path, 4ull, 5ull, &size, &g_checks_cells);
    cli_check((piece != NULL) && (size == 5ull) && (memor.cmp(&(MemoriaCfg){.src = piece, .other = "bytes", .bytes = 5u}) == 0),
              "a piece taken holds the bytes asked of it");
    cli_file_given(piece, &g_checks_cells);
    // a larger cell lands over the word that stood before the one just given back, which a take does not zero: the
    // slack is zero by the read
    unsigned char *const past = cli_file_piece_taken(path, sizeof(s_text) - 3u, 64ull, &size, &g_checks_cells);
    cli_check((past != NULL) && (size == 2ull) && file_zero(past + size, CLI_FILE_SLACK),
              "fewer where the file ends first, the slack zero past them");
    cli_file_given(past, &g_checks_cells);

    unsigned char *const nothing = cli_file_taken(empty, &size, &g_checks_cells);
    cli_check((nothing != NULL) && (size == 0ull) && file_zero(nothing, CLI_FILE_SLACK),
              "a file of no bytes is taken as bytes of none");
    cli_file_given(nothing, &g_checks_cells);
    cli_check(cli_file_taken(missing, &size, &g_checks_cells) == NULL, "a file that is not there is taken as none");
    cli_check(size == 0ull, "of no size");
    cli_file_given(NULL, &g_checks_cells);

    char broken[CLI_PATH_MAX_LEN];
    file_path_made(folder, "/broken.txt", broken);
    (void)cli_file_appended(broken, "a\xff", 2ull, 1);
    cli_check(cli_file_taken_utf8(broken, &size, &g_checks_cells) == NULL, "bytes that are no UTF-8 are refused");
    unsigned char *const letters = cli_file_taken_utf8(path, &size, &g_checks_cells);
    cli_check(letters != NULL, "and bytes that are read as they are");
    cli_file_given(letters, &g_checks_cells);
    cli_check(embed_atomic_usize_load(&g_checks_cells.in_use) == before, "and every cell comes off the owner");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    char folder[CLI_PATH_MAX_LEN];
    cli_check_folder("file_cell", folder);
    char path[CLI_PATH_MAX_LEN];
    file_path_made(folder, "/text.txt", path);
    cli_check(cli_file_appended(path, s_text, sizeof(s_text) - 1u, 1), "a file is written");
    const size_t before = embed_atomic_usize_load(&g_checks_cells.in_use);

    // a cell given back after holding other bytes: the next take lands on bytes that were written
    const unsigned long long dirty = sizeof(s_text) - 1ull + FILE_CELL_SLACK;
    unsigned char *const used = g_cli_cells->temporary_buf_alloc(dirty, &g_checks_cells);
    memor.set(&(MemoriaCfg){.dst = used, .val = 0xa5, .bytes = (size_t)dirty});
    g_cli_cells->persistent_buf_release(used, &g_checks_cells);

    unsigned long long size = 0ull;
    cli_check(cli_file_size(path, &size) && (size == (sizeof(s_text) - 1u)), "the caller asks the file's size");
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(size + FILE_CELL_SLACK, &g_checks_cells);
    unsigned long long got = 0ull;
    cli_check((cell != NULL) && cli_file_read_at(path, 0ull, cell, size, &got) && (got == size),
              "and reads the file into the cell it took");
    cli_check((cell != NULL) && (memor.cmp(&(MemoriaCfg){.src = cell, .other = s_text, .bytes = (size_t)size}) == 0),
              "the cell holds the file whole");
    g_cli_cells->persistent_buf_release(cell, &g_checks_cells);
    cli_check(embed_atomic_usize_load(&g_checks_cells.in_use) == before,
              "and every byte comes off the caller's owner as it gives the cell back");

    char empty[CLI_PATH_MAX_LEN];
    file_path_made(folder, "/empty.txt", empty);
    cli_check(cli_file_appended(empty, "", 0ull, 1), "a file of no bytes is written");
    cli_check(cli_file_size(empty, &size) && (size == 0ull), "it has a size of none");
    unsigned char *const none = g_cli_cells->temporary_buf_alloc(size + FILE_CELL_SLACK, &g_checks_cells);
    cli_check((none != NULL) && cli_file_read_at(empty, 0ull, none, size, &got) && (got == 0ull),
              "and reads as bytes of none");
    g_cli_cells->persistent_buf_release(none, &g_checks_cells);

    char missing[CLI_PATH_MAX_LEN];
    file_path_made(folder, "/missing.txt", missing);
    cli_check(!cli_file_size(missing, &size), "a file that is not there has no size");
    file_taken_checks(path, empty, missing, folder);
    return cli_checked();
}
