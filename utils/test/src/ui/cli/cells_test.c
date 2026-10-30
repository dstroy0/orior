// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells_test.c: the cellblock's check: the pool made anew for a request stands zero with no cell taken, a cell taken
// from it is given back, a cell counted to an owner is listed in the audit and taken off as it is given back, a take
// past the pool is refused, and a file of whole pages stored in a cell is measured and searched with a cap far past it,
// the word a walk loads over its last bytes landing in the pool and not past a page
#include "../../../../../ui/cli/src/command_line/cells.h"
#include "cli_check.h"

#define PAGE 4096ull

// a whole number member of a node, 0 where it has none
static unsigned long long cells_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = cli_json_member(json, node, name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; cli_json_is(json, number, CFG_JSON_NUMBER) && (at < json->node[number].length);
         at += 1ull)
    {
        value = value * 10ull + (unsigned long long)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check(cli_cells_made(64ull << 20u), "the pool is made anew for a request of 64 MiB while no cell is taken");
    cli_check(cli_cells_zero(), "the pool made stands zero with no cell taken");
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(1000ull, NULL);
    cli_check(cell != NULL, "a cell is taken from the pool made");
    cli_check(!cli_cells_made(32ull << 20u), "the pool is not made anew while a cell is taken");
    g_cli_cells->persistent_buf_release(cell, NULL);
    cli_check(cli_cells_made(32ull << 20u), "the pool is made anew once the cell is given back");
    // a cell counted to an owner, listed in the cellblock's audit while it is taken, and taken off as it is given back;
    // the pool made anew at its full size first, which the audit's parsed tree takes from
    cli_check(cli_cells_made(CLI_CELLS_POOL), "the pool is made anew at its full size while no cell is taken");
    static CellOwner s_checked = MMGR_CELL_OWNER("cells_check");
    void *const counted = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    cli_check((counted != NULL) && (embed_atomic_usize_load(&s_checked.in_use) >= 1000ull), "a take is counted to the owner it names");
    static char s_listed[1u << 14u];
    VerbaLinea listing = {s_listed, sizeof(s_listed), 0u, NULL, NULL, 0u};
    cli_cells_owners_written(&listing);
    unsigned int owners = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_listed, mmgr_line_ok(&listing) ? listing.at : 0u, &owners);
    unsigned long long listed_count = 0ull;
    for (unsigned int at = 0u; cli_json_item(json, owners, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, owners, at);
        const char *const name = cli_json_string(json, cli_json_member(json, one, "name"), NULL);
        if ((name != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "cells_check", .cap = 64u}))
        {
            listed_count = cells_whole(json, one, "in_use");
        }
    }
    cli_check(listed_count == embed_atomic_usize_load(&s_checked.in_use), "the cellblock's audit lists the owner with the bytes it has taken");
    g_cli_cells->persistent_buf_release(counted, &s_checked);
    cli_check(embed_atomic_usize_load(&s_checked.in_use) == 0ull, "and the bytes come off the owner as the cell is given back");
    cli_check(g_cli_cells->persistent_buf_alloc((size_t)CLI_CELLS_POOL, &s_checked) == NULL,
              "a take past the pool is refused with NULL, and nothing is counted");
    cli_check(embed_atomic_usize_load(&s_checked.in_use) == 0ull, "the refused take left the owner as it was");
    // a file of exactly PAGE bytes, then of 2 PAGE, each stored and walked with a cap past it
    char folder[CLI_PATH_MAX_LEN];
    cli_check_folder("cells", folder);
    static char s_bytes[2u * PAGE];
    for (unsigned long long at = 0ull; at < sizeof(s_bytes); at += 1ull)
    {
        s_bytes[at] = (char)('a' + (at % 26ull));
    }
    for (unsigned int pages = 1u; pages <= 2u; pages += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ull};
        mmgr_line_put(&line, folder);
        mmgr_line_put(&line, (pages == 1u) ? "/one.txt" : "/two.txt");
        cli_check(mmgr_line_finish(&line) != 0ull && cli_file_appended(path, s_bytes, pages * PAGE, 1),
                  "a file of whole pages is written");
        unsigned long long size = 0ull;
        const unsigned char *const file_bytes = cli_file_read(path, &size);
        cli_check((file_bytes != NULL) && (size == (pages * PAGE)), "the file is stored whole in a cell");
        const unsigned long long measured =
            (file_bytes != NULL)
                ? cellul.len(&(CatenaFinitaCfg){.src = (const char *)file_bytes, .cap = CLI_CELLS_POOL})
                : 0ull;
        cli_check(measured == (pages * PAGE), "the text ends where the file ends, the word past it zero");
        const char *const missed =
            (file_bytes != NULL)
                ? cellul.chr(&(CatenaFinitaCfg){.src = (const char *)file_bytes, .cap = CLI_CELLS_POOL, .byte = '!'})
                : NULL;
        cli_check(missed == NULL, "a byte the file does not contain is not found past its end");
        cli_file_let_go(file_bytes, size);
    }
    return cli_checked();
}
