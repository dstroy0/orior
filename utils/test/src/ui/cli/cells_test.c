// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells_test.c: the cellblock's check: the pool made anew for an ask stands zero with no cell taken, a cell taken
// from it is given back, and a file of whole pages held in a cell is measured and searched with a cap far past it,
// the word a walk loads over its last bytes landing in the pool and not past a page
#include "../../../../../ui/cli/src/command_line/cells.h"
#include "cli_check.h"

#define PAGE 4096ull

int main(void)
{
    cli_check(cli_cells_made(64ull << 20u), "the pool is made anew for an ask of 64 MiB while no cell is held");
    cli_check(cli_cells_zero(), "the pool made stands zero with no cell taken");
    unsigned char *const cell = cli_cell_taken(1000ull, "a check's cell");
    cli_check(cell != NULL, "a cell is taken from the pool made");
    cli_check(!cli_cells_made(32ull << 20u), "the pool is not made anew while a cell is held");
    cli_cell_given(cell);
    cli_check(cli_cells_made(32ull << 20u), "the pool is made anew once the cell is given back");
    // a file of exactly PAGE bytes, then of 2 PAGE, each kept and walked with a cap past it
    char folder[CLI_PATH_LONGEST];
    cli_check_folder("cells", folder);
    static char s_bytes[2u * PAGE];
    for (unsigned long long at = 0ull; at < sizeof(s_bytes); at += 1ull)
    {
        s_bytes[at] = (char)('a' + (at % 26ull));
    }
    for (unsigned int pages = 1u; pages <= 2u; pages += 1u)
    {
        char path[CLI_PATH_LONGEST];
        CliLine line = {path, sizeof(path), 0ull};
        cli_line_text(&line, folder);
        cli_line_text(&line, (pages == 1u) ? "/one.txt" : "/two.txt");
        cli_check(cli_line_closed(&line) != 0ull && cli_file_appended(path, s_bytes, pages * PAGE, 1),
                  "a file of whole pages is written");
        unsigned long long size = 0ull;
        const unsigned char *const held = cli_file_held(path, &size);
        cli_check((held != NULL) && (size == (pages * PAGE)), "the file is held whole in a cell");
        const unsigned long long measured =
            (held != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = (const char *)held, .cap = CLI_CELLS_POOL}) : 0ull;
        cli_check(measured == (pages * PAGE), "the text ends where the file ends, the word past it zero");
        const char *const missed =
            (held != NULL)
                ? cellul.chr(&(CatenaFinitaCfg){.src = (const char *)held, .cap = CLI_CELLS_POOL, .byte = '!'})
                : NULL;
        cli_check(missed == NULL, "a byte the file does not hold is not found past its end");
        cli_file_let_go(held, size);
    }
    return cli_checked();
}
