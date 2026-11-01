// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells_test.c: the cellblock's check: the pool made anew for a request stands zero with no cell taken, a cell taken
// from it is given back, a cell counted to an owner is listed in the audit and taken off as it is given back, a take
// past the pool is refused, and a file of whole pages stored in a cell is measured and searched with a cap far past it,
// the word a walk loads over its last bytes landing in the pool and not past a page
#include "../../../../../ui/cli/src/command_line/cells.h"
#include "../../../../../ui/cli/src/editor/files.h"
#include "../../../../../ui/cli/src/editor/files_internal.h"
#include "../../../../../ui/cli/src/plugins/plugins.h"
#include "cli_check.h"

#define PAGE 4096ull

// the owner every cell the check takes is counted to
static CellOwner s_checked = MMGR_CELL_OWNER("cells_check");

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

// the paths a walk finds counted
static void cells_walk_counted(void *context, const char *path, unsigned long long length)
{
    (void)path;
    (void)length;
    *(unsigned int *)context += 1u;
}

// Work given back within its call, its batch or a watch's round, a walk, a list sorted, the languages' table, leaves
// the persistent tier's end where it stood, since a short cell there leaves a hole under whatever is laid above it; the
// temporary tier carries it. The pool is made anew for the reading, which puts the peaks back
static void cells_work_checked(void)
{
    cli_check(cli_cells_made(CLI_CELLS_POOL), "the pool is made anew for the work's reading");
    unsigned long long pool = 0ull;
    unsigned long long persistent = 0ull;
    unsigned long long temporary = 0ull;
    cli_cells_report(&pool, &persistent, &temporary);
    cli_check((persistent == 0ull) && (temporary == 0ull), "the pool made anew has reached nothing");

    unsigned int found = 0u;
    cli_files_walked(".", cells_walk_counted, &found);
    cli_cells_report(&pool, &persistent, &temporary);
    cli_check(found != 0u, "a walk finds the folder's files");
    cli_check(persistent == 0ull, "a walk's paths stay out of the persistent tier");
    cli_check(temporary != 0ull, "and are carried by the temporary tier");

    CliFiles files;
    cli_check(cli_files_made(&files, 2ull, 8ull), "a list is made");
    (void)cli_files_put(&files, "b", 1ull);
    (void)cli_files_put(&files, "a", 1ull);
    cli_check(cli_files_sorted(&files) && (files.start[0] == 2u), "it is sorted");
    cli_cells_report(&pool, &persistent, &temporary);
    cli_check(persistent == 0ull,
              "a list and its spare, which last a watch's round at the most, stay out of the persistent tier");
    cli_files_emptied(&files);

    CliLanguages languages;
    cli_cells_report(&pool, &persistent, &temporary);
    const unsigned long long before_languages = persistent;
    cli_plugins_languages(&languages);
    cli_check(languages.pair != NULL, "the languages' table is taken");
    cli_cells_report(&pool, &persistent, &temporary);
    cli_check(persistent == before_languages, "out of the persistent tier");
    cli_plugins_languages_given(&languages);
    cli_check(mmgr_owner_in_use(&g_cli_files_views_cells) == 0u, "the walk's and the list's cells are all given back");
}

// A hole under a movable cell, then the pool left quiet: the mover brings the tier's end back within the time, the
// cell's bytes kept and its identity standing. Last of the checks: the pool is not made anew once the option is
// assigned
static void cells_mover_checked(void)
{
    enum
    {
        CELL_BYTES = 4096,
        HOLE_BYTES = 1 << 20,
        PATIENCE_MS = 20000
    };
    static unsigned char s_written[CELL_BYTES];
    static unsigned char s_read[CELL_BYTES];
    cli_cells_movable_ready();
    void *const hole = g_cli_cells->persistent_buf_alloc(HOLE_BYTES, &s_checked);
    const CarcerMovableSlot *const cell = g_cli_cells->movable_buf_alloc(CELL_BYTES, &s_checked);
    cli_check((hole != NULL) && (cell != NULL), "a movable cell is taken above a plain one");
    for (unsigned int at = 0u; at < CELL_BYTES; at += 1u)
    {
        s_written[at] = (unsigned char)(at * 7u);
    }
    cli_check(g_cli_cells->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = s_written, .bytes = CELL_BYTES}),
              "its bytes are written through the guard");
    cli_check(
        !g_cli_cells->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = s_written, .bytes = CELL_BYTES + 1u}),
        "and a write past its end is refused");
    const size_t gap = g_cli_cells->buf_available();
    g_cli_cells->persistent_buf_release(hole, &s_checked);
    static CliLock s_lock = CLI_LOCK_INIT;
    static CliWake s_wake = CLI_WAKE_INIT;
    const unsigned long long began = cli_clock_milliseconds();
    while ((g_cli_cells->buf_available() <= gap) && ((cli_clock_milliseconds() - began) < PATIENCE_MS))
    {
        cli_lock_take(&s_lock);
        (void)cli_wake_wait_for(&s_wake, &s_lock, 50ull);
        cli_lock_give(&s_lock);
    }
    cli_check(g_cli_cells->buf_available() > gap,
              "the mover moves the cell at a quiet time and the tier's end comes back");
    cli_check(g_cli_cells->movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = s_read, .bytes = CELL_BYTES}),
              "the identity still reads");
    cli_check(memor.cmp(&(MemoriaCfg){.src = s_read, .other = s_written, .bytes = CELL_BYTES}) == 0,
              "and every byte moved with the cell");
    cli_check(g_cli_cells->movable_buf_release(cell, &s_checked), "the cell goes back");
    cli_check(embed_atomic_usize_load(&s_checked.in_use) == 0ull, "and comes off its owner");
    cli_check(!cli_cells_made(CLI_CELLS_POOL), "the pool is not made anew with the mover reading it");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    (void)mmgr_owner_register(&s_checked);
    cli_check_begun();
    cli_check(cli_cells_made(64ull << 20u), "the pool is made anew for a request of 64 MiB while no cell is taken");
    cli_check(cli_cells_zero(), "the pool made stands zero with no cell taken");
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    cli_check(cell != NULL, "a cell is taken from the pool made");
    cli_check(!cli_cells_made(32ull << 20u), "the pool is not made anew while a cell is taken");
    g_cli_cells->persistent_buf_release(cell, &s_checked);
    cli_check(cli_cells_made(32ull << 20u), "the pool is made anew once the cell is given back");
    // a cell counted to an owner, listed in the cellblock's audit while it is taken, and taken off as it is given back;
    // the pool made anew at its full size first, which the audit's parsed tree takes from
    cells_work_checked();
    cli_check(cli_cells_made(CLI_CELLS_POOL), "the pool is made anew at its full size while no cell is taken");
    void *const counted = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    cli_check((counted != NULL) && (embed_atomic_usize_load(&s_checked.in_use) >= 1000ull),
              "a take is counted to the owner it names");
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
    cli_check(listed_count == embed_atomic_usize_load(&s_checked.in_use),
              "the cellblock's audit lists the owner with the bytes it has taken");
    g_cli_cells->persistent_buf_release(counted, &s_checked);
    cli_check(embed_atomic_usize_load(&s_checked.in_use) == 0ull,
              "and the bytes come off the owner as the cell is given back");
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
        const unsigned char *const file_bytes = cli_file_taken(path, &size, &g_checks_cells);
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
        cli_file_given(file_bytes, &g_checks_cells);
    }
    cells_mover_checked();
    return cli_checked();
}
