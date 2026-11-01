// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_table.c: the files the tree stores, in a table at its most, each found by its path through a table of buckets
// twice its room, made again as the table is put together
#include "../command_line/cells.h"
#include "symbols_tree.h"

// the files the table's room is taken with, its paths' table twice as many buckets

// an empty bucket of the table of paths, and one whose file was let go
#define CLI_BUCKET_EMPTY 0u

#define CLI_BUCKET_GONE 0xffffffffu

// every file a table can name, and the one taken while the cell it replaces still stands
_Static_assert(CLI_SYMBOL_FILES < CLI_CELLS_MOVABLE, "the movable cells cannot name every symbol file");

CliSymbolFile *g_cli_symbol_files;
unsigned int g_cli_symbol_file_count;
static unsigned int s_cell_most;
static unsigned int s_file_room;
static unsigned int *s_buckets;
static unsigned int s_bucket_room;

static unsigned long long cli_hash(const char *path)
{
    unsigned long long value = 0xcbf29ce484222325ull;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        value = (value ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    return value;
}

char *cli_symbol_scratch_taken(void)
{
    return g_cli_cells->temporary_buf_alloc(s_cell_most, &g_cli_code_cells);
}

void cli_symbol_file_read(const CliSymbolFile *branch, char *scratch)
{
    (void)g_cli_cells->movable_buf_read(&(CarcerMovableCfg){.cell = branch->cell, .dst = scratch, .bytes = branch->bytes});
}

void cli_symbol_scratch_given(char *scratch)
{
    g_cli_cells->persistent_buf_release(scratch, &g_cli_code_cells);
}

const char *cli_symbol_file_path(const CliSymbolFile *branch, const char *scratch)
{
    return scratch + branch->path;
}

const CliSymbol *cli_symbol_file_symbols(const char *scratch)
{
    return (const CliSymbol *)(const void *)scratch;
}

void cli_symbol_path_read(const CliSymbolFile *branch, char *path)
{
    const size_t measured = g_cli_cells->movable_buf_measure(&(CarcerMovableCfg){.cell = branch->cell, .offset = branch->path});
    const size_t bytes = (measured < CLI_PATH_MAX_LEN) ? measured : (CLI_PATH_MAX_LEN - 1u);

    (void)g_cli_cells->movable_buf_read(
        &(CarcerMovableCfg){.cell = branch->cell, .offset = branch->path, .dst = path, .bytes = bytes});
    path[bytes] = '\0';
}

void cli_symbol_cell_counted(unsigned int bytes)
{
    s_cell_most = (bytes > s_cell_most) ? bytes : s_cell_most;
}

unsigned int cli_symbols_file_of(const char *path)
{
    if (s_bucket_room == 0u)
    {
        return CLI_SYMBOL_FILES;
    }
    unsigned long long bucket = cli_hash(path) % s_bucket_room;
    // the path with its terminator, so that a stored path it only starts differs from it there
    const size_t bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
    for (unsigned int tried = 0u; (tried < s_bucket_room) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        const unsigned int place = s_buckets[bucket];
        if ((place != CLI_BUCKET_GONE) &&
            (g_cli_cells->movable_buf_compare(&(CarcerMovableCfg){.cell = g_cli_symbol_files[place - 1u].cell,
                                                                  .offset = g_cli_symbol_files[place - 1u].path,
                                                                  .src = path,
                                                                  .bytes = bytes}) == 0))
        {
            return place - 1u;
        }
        bucket = (bucket + 1u) % s_bucket_room;
    }
    return CLI_SYMBOL_FILES;
}

void cli_symbols_bucket_put(const char *path, unsigned int file)
{
    unsigned long long bucket = cli_hash(path) % s_bucket_room;
    while ((s_buckets[bucket] != CLI_BUCKET_EMPTY) && (s_buckets[bucket] != CLI_BUCKET_GONE))
    {
        bucket = (bucket + 1u) % s_bucket_room;
    }
    s_buckets[bucket] = file + 1u;
}

// the paths' table emptied, then each file stored put in it again
static void cli_buckets_made(void)
{
    memor.set(&(MemoriaCfg){.dst = s_buckets, .val = 0, .bytes = (size_t)s_bucket_room * sizeof(unsigned int)});
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        if (g_cli_symbol_files[at].used)
        {
            char path[CLI_PATH_MAX_LEN];
            cli_symbol_path_read(&g_cli_symbol_files[at], path);
            cli_symbols_bucket_put(path, at);
        }
    }
}

// the table of files and the paths' table taken at their most, each once: 1, or 0 where they stand taken or a cell is
// refused
static int cli_files_made(void)
{
    // the files' table and its buckets, one cell each, taken once at the most the tree holds
    CliSymbolFile *const files =
        (s_file_room == 0u) ? g_cli_cells->persistent_buf_alloc(
                                  (unsigned long long)CLI_SYMBOL_FILES * sizeof(CliSymbolFile), &g_cli_code_cells)
                            : NULL;
    if (files == NULL)
    {
        return 0;
    }
    unsigned int *const buckets =
        g_cli_cells->persistent_buf_alloc(2ull * CLI_SYMBOL_FILES * sizeof(unsigned int), &g_cli_code_cells);
    if (buckets == NULL)
    {
        g_cli_cells->persistent_buf_release(files, &g_cli_code_cells);
        return 0;
    }
    cli_cells_movable_ready();
    g_cli_symbol_files = files;
    s_file_room = CLI_SYMBOL_FILES;
    s_buckets = buckets;
    s_bucket_room = 2u * CLI_SYMBOL_FILES;
    cli_buckets_made();
    return 1;
}

void cli_symbols_let_go(unsigned int file)
{
    char path[CLI_PATH_MAX_LEN];
    cli_symbol_path_read(&g_cli_symbol_files[file], path);
    unsigned long long bucket = cli_hash(path) % s_bucket_room;
    for (unsigned int tried = 0u; (tried < s_bucket_room) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        if (s_buckets[bucket] == (file + 1u))
        {
            s_buckets[bucket] = CLI_BUCKET_GONE;
            break;
        }
        bucket = (bucket + 1u) % s_bucket_room;
    }
    (void)g_cli_cells->movable_buf_release(g_cli_symbol_files[file].cell, &g_cli_code_cells);
    g_cli_symbol_files[file].cell = NULL;
    g_cli_symbol_files[file].used = 0;
}

void cli_symbols_tree_cleared(void)
{
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        (void)g_cli_cells->movable_buf_release(g_cli_symbol_files[at].cell, &g_cli_code_cells);
        g_cli_symbol_files[at].cell = NULL;
        g_cli_symbol_files[at].used = 0;
    }
    g_cli_symbol_file_count = 0u;
    g_cli_cells->persistent_buf_release(g_cli_symbol_files, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(s_buckets, &g_cli_code_cells);
    g_cli_symbol_files = NULL;
    s_buckets = NULL;
    s_file_room = 0u;
    s_bucket_room = 0u;
}

// The table of files put together again: each stored file moved to the front and the table of paths made again
static void cli_compacted(void)
{
    unsigned int files = 0u;
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        if (g_cli_symbol_files[at].used)
        {
            g_cli_symbol_files[files] = g_cli_symbol_files[at];
            files += 1u;
        }
    }
    g_cli_symbol_file_count = files;
    cli_buckets_made();
}

int cli_symbols_room_made(void)
{
    if (g_cli_symbol_file_count < s_file_room)
    {
        return 1;
    }
    if (!cli_files_made() && (s_file_room != 0u))
    {
        cli_compacted();
    }
    return g_cli_symbol_file_count < s_file_room;
}
