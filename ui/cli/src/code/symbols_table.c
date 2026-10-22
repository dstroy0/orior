// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_table.c: the files the symbols' index holds, in a table whose room doubles as files are held, each found by
// its path through a table of buckets twice its room, made again as it grows and as it is put together
#include "../command_line/cells.h"
#include "symbols_index.h"

// the files the table's room is begun with, doubling as files are held, its paths' table twice as many buckets
#define CLI_SYMBOL_FILES_FIRST 256u

// an empty bucket of the table of paths, and one whose file was let go
#define CLI_BUCKET_EMPTY 0u

#define CLI_BUCKET_GONE 0xffffffffu

CliHeldFile *g_cli_symbol_files;
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

// a file held's path, and its declarations
const char *cli_held_path(const CliHeldFile *held)
{
    return held->cell + held->path;
}

const CliSymbol *cli_held_symbols(const CliHeldFile *held)
{
    return (const CliSymbol *)(const void *)held->cell;
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
        const unsigned int held = s_buckets[bucket];
        if ((held != CLI_BUCKET_GONE) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_held_path(&g_cli_symbol_files[held - 1u]), .other = path, .cap = CLI_CELLS_POOL})))
        {
            return held - 1u;
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

// the paths' table emptied, then each file held put in it again
static void cli_buckets_made(void)
{
    memor.set(&(MemoriaCfg){.dst = s_buckets, .val = 0, .bytes = (size_t)s_bucket_room * sizeof(unsigned int)});
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        if (g_cli_symbol_files[at].used)
        {
            cli_symbols_bucket_put(cli_held_path(&g_cli_symbol_files[at]), at);
        }
    }
}

// the table of files' room doubled, and the paths' table with it, made again: 1, or 0 where it holds the most or a
// cell is refused
static int cli_files_grown(void)
{
    const unsigned int doubled = (s_file_room == 0u) ? CLI_SYMBOL_FILES_FIRST : (s_file_room * 2u);
    const unsigned int room = (doubled < CLI_SYMBOL_FILES) ? doubled : CLI_SYMBOL_FILES;
    CliHeldFile *const files =
        (s_file_room < CLI_SYMBOL_FILES)
            ? cli_cell_taken((unsigned long long)room * sizeof(CliHeldFile), "the files of the symbols' index")
            : NULL;
    unsigned int *const buckets =
        (files != NULL) ? cli_cell_taken(2ull * room * sizeof(unsigned int), "the paths' table of the symbols' index")
                        : NULL;
    if (buckets == NULL)
    {
        cli_cell_given(files);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = files, .src = g_cli_symbol_files, .bytes = (size_t)g_cli_symbol_file_count * sizeof(CliHeldFile)});
    cli_cell_given(g_cli_symbol_files);
    cli_cell_given(s_buckets);
    g_cli_symbol_files = files;
    s_file_room = room;
    s_buckets = buckets;
    s_bucket_room = 2u * room;
    cli_buckets_made();
    return 1;
}

void cli_symbols_let_go(unsigned int file)
{
    unsigned long long bucket = cli_hash(cli_held_path(&g_cli_symbol_files[file])) % s_bucket_room;
    for (unsigned int tried = 0u; (tried < s_bucket_room) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        if (s_buckets[bucket] == (file + 1u))
        {
            s_buckets[bucket] = CLI_BUCKET_GONE;
            break;
        }
        bucket = (bucket + 1u) % s_bucket_room;
    }
    cli_cell_given(g_cli_symbol_files[file].cell);
    g_cli_symbol_files[file].cell = NULL;
    g_cli_symbol_files[file].used = 0;
}

void cli_symbols_held_forgotten(void)
{
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        cli_cell_given(g_cli_symbol_files[at].cell);
        g_cli_symbol_files[at].cell = NULL;
        g_cli_symbol_files[at].used = 0;
    }
    g_cli_symbol_file_count = 0u;
    cli_cell_given(g_cli_symbol_files);
    cli_cell_given(s_buckets);
    g_cli_symbol_files = NULL;
    s_buckets = NULL;
    s_file_room = 0u;
    s_bucket_room = 0u;
}

// The table of files put together again: each held file moved to the front and the table of paths made again
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
