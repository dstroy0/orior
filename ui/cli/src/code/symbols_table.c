// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_table.c: the files the tree stores, in a table whose room doubles as files are stored, each found by its
// path through a table of buckets twice its room, made again as it grows and as it is put together
#include "../command_line/cells.h"
#include "symbols_tree.h"

// the files the table's room is begun with, doubling as files are stored, its paths' table twice as many buckets
#define CLI_SYMBOL_FILES_FIRST 256u

// an empty bucket of the table of paths, and one whose file was let go
#define CLI_BUCKET_EMPTY 0u

#define CLI_BUCKET_GONE 0xffffffffu

CliSymbolFile *g_cli_symbol_files;
unsigned int g_cli_symbol_file_count;
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

// a file stored's path, and its declarations
const char *cli_symbol_file_path(const CliSymbolFile *branch)
{
    return branch->cell + branch->path;
}

const CliSymbol *cli_symbol_file_symbols(const CliSymbolFile *branch)
{
    return (const CliSymbol *)(const void *)branch->cell;
}

unsigned int cli_symbols_file_of(const char *path)
{
    if (s_bucket_room == 0u)
    {
        return CLI_SYMBOL_FILES;
    }
    unsigned long long bucket = cli_hash(path) % s_bucket_room;
    for (unsigned int tried = 0u; (tried < s_bucket_room) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        const unsigned int place = s_buckets[bucket];
        if ((place != CLI_BUCKET_GONE) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_symbol_file_path(&g_cli_symbol_files[place - 1u]), .other = path, .cap = CLI_CELLS_POOL})))
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
            cli_symbols_bucket_put(cli_symbol_file_path(&g_cli_symbol_files[at]), at);
        }
    }
}

// the table of files' room doubled, and the paths' table with it, made again: 1, or 0 where it contains the most or a
// cell is refused
static int cli_files_grown(void)
{
    const unsigned int doubled = (s_file_room == 0u) ? CLI_SYMBOL_FILES_FIRST : (s_file_room * 2u);
    const unsigned int room = (doubled < CLI_SYMBOL_FILES) ? doubled : CLI_SYMBOL_FILES;
    // the files' table grows where it stands or is moved with its entries; the buckets are made again whole over a
    // cell of the new room, and nothing of the old ones is carried
    CliSymbolFile *const files =
        (s_file_room < CLI_SYMBOL_FILES)
            ? cli_cell_regrown(g_cli_symbol_files, (size_t)g_cli_symbol_file_count * sizeof(CliSymbolFile),
                               (unsigned long long)room * sizeof(CliSymbolFile), &g_cli_code_cells)
            : NULL;
    if (files == NULL)
    {
        return 0;
    }
    g_cli_symbol_files = files;
    unsigned int *const buckets =
        cli_cell_regrown(s_buckets, 0u, 2ull * room * sizeof(unsigned int), &g_cli_code_cells);
    if (buckets == NULL)
    {
        return 0;
    }
    s_file_room = room;
    s_buckets = buckets;
    s_bucket_room = 2u * room;
    cli_buckets_made();
    return 1;
}

void cli_symbols_let_go(unsigned int file)
{
    unsigned long long bucket = cli_hash(cli_symbol_file_path(&g_cli_symbol_files[file])) % s_bucket_room;
    for (unsigned int tried = 0u; (tried < s_bucket_room) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        if (s_buckets[bucket] == (file + 1u))
        {
            s_buckets[bucket] = CLI_BUCKET_GONE;
            break;
        }
        bucket = (bucket + 1u) % s_bucket_room;
    }
    cli_cell_release(g_cli_symbol_files[file].cell, &g_cli_code_cells);
    g_cli_symbol_files[file].cell = NULL;
    g_cli_symbol_files[file].used = 0;
}

void cli_symbols_tree_cleared(void)
{
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        cli_cell_release(g_cli_symbol_files[at].cell, &g_cli_code_cells);
        g_cli_symbol_files[at].cell = NULL;
        g_cli_symbol_files[at].used = 0;
    }
    g_cli_symbol_file_count = 0u;
    cli_cell_release(g_cli_symbol_files, &g_cli_code_cells);
    cli_cell_release(s_buckets, &g_cli_code_cells);
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
    if (!cli_files_grown() && (s_file_room != 0u))
    {
        cli_compacted();
    }
    return g_cli_symbol_file_count < s_file_room;
}
