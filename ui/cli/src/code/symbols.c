// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols.c: how well a query names a name, and the index of a tree's declarations: read on a thread of its own, kept
// with each file's time, each file's declarations a cell of their own, and searched in symbols_find.c
#include "../command_line/cells.h"
#include "symbols_index.h"

// a file larger than this is not read for its declarations; how long the index stands after it was last brought up to
// date before a search starts it again, in milliseconds; and the most bytes of the paths a reading goes through
#define CLI_SYMBOLS_LARGEST (1u << 20u)

#define CLI_FRESH_FOR 4000ull

#define CLI_SYMBOL_BUCKETS (2u * CLI_SYMBOL_FILES)

#define CLI_SYMBOL_TEXT (1u << 24u)

// an empty bucket of the table of paths, and one whose file was let go
#define CLI_BUCKET_EMPTY 0u

#define CLI_BUCKET_GONE 0xffffffffu

// The storage a reading goes through, a cell taken as it starts and given back as it ends: the paths of the files it
// reads one after another, one file's bytes, and its declarations as read, before they are held
typedef struct
{
    char listed[CLI_SYMBOL_TEXT];
    unsigned char bytes[CLI_SYMBOLS_LARGEST];
    char read_text[CLI_SYMBOLS_LARGEST];
    CliSymbol read_symbols[CLI_SYMBOLS_LARGEST / 4u];
} CliSymbolsReading;

CliHeldFile g_cli_symbol_files[CLI_SYMBOL_FILES];
unsigned int g_cli_symbol_file_count;
static unsigned int s_buckets[CLI_SYMBOL_BUCKETS];
static CliSymbolsReading *s_listing;

// The index's state: its tree, whether a reading goes and when the last ended, how far it has gone, whether it is asked
// to stop, which reading of the tree it is, and whether its thread runs
static char s_root[CLI_PATH_LONGEST];
static int s_rooted;
int g_cli_symbols_reading;
static int s_read;
static unsigned long long s_read_at;
static unsigned long long s_done;
static unsigned long long s_total;
static int s_stop;
static unsigned long long s_generation;
static int s_running;
CliLock g_cli_symbols_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

// how many files a reading goes through, and the thread reading them
static unsigned int s_listed_count;
static CliThread s_thread;

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

// the place of a path's file, or CLI_SYMBOL_FILES where none is held
static unsigned int cli_file_of(const char *path)
{
    unsigned long long bucket = cli_hash(path) % CLI_SYMBOL_BUCKETS;
    for (unsigned int tried = 0u; (tried < CLI_SYMBOL_BUCKETS) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        const unsigned int held = s_buckets[bucket];
        if ((held != CLI_BUCKET_GONE) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_held_path(&g_cli_symbol_files[held - 1u]), .other = path, .cap = CLI_CELLS_POOL})))
        {
            return held - 1u;
        }
        bucket = (bucket + 1u) % CLI_SYMBOL_BUCKETS;
    }
    return CLI_SYMBOL_FILES;
}

static void cli_bucket_put(const char *path, unsigned int file)
{
    unsigned long long bucket = cli_hash(path) % CLI_SYMBOL_BUCKETS;
    while ((s_buckets[bucket] != CLI_BUCKET_EMPTY) && (s_buckets[bucket] != CLI_BUCKET_GONE))
    {
        bucket = (bucket + 1u) % CLI_SYMBOL_BUCKETS;
    }
    s_buckets[bucket] = file + 1u;
}

// a file held let go: its place taken out of the table and its cell given back
static void cli_held_let_go(unsigned int file)
{
    unsigned long long bucket = cli_hash(cli_held_path(&g_cli_symbol_files[file])) % CLI_SYMBOL_BUCKETS;
    for (unsigned int tried = 0u; (tried < CLI_SYMBOL_BUCKETS) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        if (s_buckets[bucket] == (file + 1u))
        {
            s_buckets[bucket] = CLI_BUCKET_GONE;
            break;
        }
        bucket = (bucket + 1u) % CLI_SYMBOL_BUCKETS;
    }
    cli_cell_given(g_cli_symbol_files[file].cell);
    g_cli_symbol_files[file].cell = NULL;
    g_cli_symbol_files[file].used = 0;
}

static void cli_forget_held(void)
{
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        cli_cell_given(g_cli_symbol_files[at].cell);
        g_cli_symbol_files[at].cell = NULL;
        g_cli_symbol_files[at].used = 0;
    }
    memor.set(&(MemoriaCfg){.dst = s_buckets, .val = 0, .bytes = sizeof(s_buckets)});
    g_cli_symbol_file_count = 0u;
}

// The table of files put together again: each held file moved to the front and the table of paths made again
static void cli_compacted(void)
{
    unsigned int files = 0u;
    memor.set(&(MemoriaCfg){.dst = s_buckets, .val = 0, .bytes = sizeof(s_buckets)});
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        if (g_cli_symbol_files[at].used)
        {
            g_cli_symbol_files[files] = g_cli_symbol_files[at];
            cli_bucket_put(cli_held_path(&g_cli_symbol_files[files]), files);
            files += 1u;
        }
    }
    g_cli_symbol_file_count = files;
}

// how many declarations of a file were read and the bytes of their names, before they are held
static unsigned int s_read_count;
static unsigned int s_read_used;

static void cli_read_each(void *context, const char *name, unsigned long long length, const char *kind,
                          unsigned int line)
{
    CliSymbolsReading *const reading = (CliSymbolsReading *)context;
    if ((s_read_count >= (CLI_SYMBOLS_LARGEST / 4u)) || ((s_read_used + length) > sizeof(reading->read_text)))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = reading->read_text + s_read_used, .src = name, .bytes = length});
    // a name is below a file's largest size, which an unsigned int counts
    const CliSymbol symbol = {s_read_used, (unsigned int)length, kind, line};
    reading->read_symbols[s_read_count] = symbol;
    s_read_count += 1u;
    s_read_used += (unsigned int)length;
}

// the declarations read put as the file at `path`, written at `written`, in a cell of its own: 1, or 0 where the
// table holds no more files or the cells cannot meet it. Called under the lock
static int cli_held_put(const CliSymbolsReading *reading, const char *path, unsigned long long written)
{
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
    unsigned int file = cli_file_of(path);
    if ((file == CLI_SYMBOL_FILES) && (g_cli_symbol_file_count >= CLI_SYMBOL_FILES))
    {
        cli_compacted();
    }
    if ((file == CLI_SYMBOL_FILES) && (g_cli_symbol_file_count >= CLI_SYMBOL_FILES))
    {
        return 0;
    }
    const unsigned long long symbols_size = s_read_count * sizeof(CliSymbol);
    char *const cell = cli_cell_taken(symbols_size + path_length + s_read_used, "a file's declarations");
    if (cell == NULL)
    {
        return 0;
    }
    if (file == CLI_SYMBOL_FILES)
    {
        file = g_cli_symbol_file_count;
        g_cli_symbol_file_count += 1u;
        memor.cpy(&(MemoriaCfg){.dst = cell + symbols_size, .src = path, .bytes = path_length});
        g_cli_symbol_files[file].cell = cell;
        // a file's declarations, path and names are below what an unsigned int counts
        g_cli_symbol_files[file].path = (unsigned int)symbols_size;
        cli_bucket_put(path, file);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = cell + symbols_size, .src = path, .bytes = path_length});
        cli_cell_given(g_cli_symbol_files[file].cell);
        g_cli_symbol_files[file].cell = cell;
        g_cli_symbol_files[file].path = (unsigned int)symbols_size;
    }
    CliHeldFile *const held = &g_cli_symbol_files[file];
    held->used = 1;
    held->written = written;
    held->count = s_read_count;
    held->seen = 1;
    CliSymbol *const symbols = (CliSymbol *)(void *)cell;
    unsigned int at_name = (unsigned int)(symbols_size + path_length);
    for (unsigned int at = 0u; at < s_read_count; at += 1u)
    {
        CliSymbol symbol = reading->read_symbols[at];
        memor.cpy(
            &(MemoriaCfg){.dst = cell + at_name, .src = reading->read_text + symbol.name, .bytes = symbol.length});
        symbol.name = at_name;
        at_name += symbol.length;
        symbols[at] = symbol;
    }
    return 1;
}

// The reading: each file listed, the stamp of its file read, and read again where its time moved; past the last, the
// files it did not see let go where it was not stopped
static void cli_reading(void *context)
{
    CliSymbolsReading *const reading = (CliSymbolsReading *)context;
    cli_lock_take(&g_cli_symbols_lock);
    const unsigned long long generation = s_generation;
    char root[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = root,
                            .src = s_root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_root, .cap = CLI_CELLS_POOL}) + 1u});
    s_done = 0ull;
    s_total = s_listed_count;
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        g_cli_symbol_files[at].seen = 0;
    }
    int stopped = 0;
    unsigned long long offset = 0ull;
    for (unsigned int each = 0u; each < s_listed_count; each += 1u)
    {
        char path[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){
            .dst = path,
            .src = reading->listed + offset,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = reading->listed + offset, .cap = CLI_CELLS_POOL}) + 1u});
        offset += cellul.len(&(CatenaFinitaCfg){.src = reading->listed + offset, .cap = CLI_CELLS_POOL}) + 1ull;
        if (s_stop || (s_generation != generation))
        {
            stopped = 1;
            break;
        }
        s_done += 1ull;
        cli_lock_give(&g_cli_symbols_lock);
        char full[CLI_PATH_LONGEST];
        CliLine line = {full, sizeof(full), 0ull};
        cli_line_text(&line, root);
        cli_line_character(&line, '/');
        cli_line_text(&line, path);
        unsigned long long size = 0ull;
        unsigned long long written = 0ull;
        const int stamped =
            (cli_line_closed(&line) != 0ull) && cli_file_stamp(full, &size, &written) && (size <= CLI_SYMBOLS_LARGEST);
        cli_lock_take(&g_cli_symbols_lock);
        const unsigned int file = stamped ? cli_file_of(path) : CLI_SYMBOL_FILES;
        if (file != CLI_SYMBOL_FILES)
        {
            g_cli_symbol_files[file].seen = 1;
            if (g_cli_symbol_files[file].written == written)
            {
                continue;
            }
        }
        if (!stamped)
        {
            continue;
        }
        cli_lock_give(&g_cli_symbols_lock);
        const unsigned long long read = qry_member_file_read(full, reading->bytes, sizeof(reading->bytes));
        s_read_count = 0u;
        s_read_used = 0u;
        if (read != QRY_MEMBER_UNREAD)
        {
            cli_symbols_declared(path, (const char *)reading->bytes, read, cli_read_each, reading);
        }
        cli_lock_take(&g_cli_symbols_lock);
        if (s_generation != generation)
        {
            stopped = 1;
            break;
        }
        if (read != QRY_MEMBER_UNREAD)
        {
            cli_held_put(reading, path, written);
        }
    }
    if (s_generation == generation)
    {
        // A reading stopped keeps every file it has read, and those it had not come to
        for (unsigned int at = 0u; !stopped && (at < g_cli_symbol_file_count); at += 1u)
        {
            if (g_cli_symbol_files[at].used && !g_cli_symbol_files[at].seen)
            {
                cli_held_let_go(at);
            }
        }
        g_cli_symbols_reading = 0;
        s_read = 1;
        s_read_at = cli_clock_milliseconds();
    }
    cli_cell_given(reading);
    s_listing = NULL;
    s_running = 0;
    cli_wake_all(&s_wake);
    cli_lock_give(&g_cli_symbols_lock);
}

void cli_symbols_forget(void)
{
    cli_lock_take(&g_cli_symbols_lock);
    s_generation += 1ull;
    s_rooted = 0;
    g_cli_symbols_reading = 0;
    s_read = 0;
    s_stop = 0;
    cli_forget_held();
    cli_lock_give(&g_cli_symbols_lock);
}

void cli_symbols_refresh(const char *root, const char *const *paths, unsigned int count)
{
    cli_lock_take(&g_cli_symbols_lock);
    if (!s_rooted || (!cellul.eq(&(CatenaFinitaCfg){.src = s_root, .other = root, .cap = CLI_CELLS_POOL})))
    {
        s_generation += 1ull;
        memor.cpy(&(MemoriaCfg){.dst = s_root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        s_rooted = 1;
        g_cli_symbols_reading = 0;
        s_read = 0;
        s_stop = 0;
        cli_forget_held();
    }
    if (g_cli_symbols_reading || (s_read && ((cli_clock_milliseconds() - s_read_at) < CLI_FRESH_FOR)))
    {
        cli_lock_give(&g_cli_symbols_lock);
        return;
    }
    g_cli_symbols_reading = 1;
    s_stop = 0;
    // the list the last reading went through is let go only once it has ended
    while (s_running)
    {
        cli_wake_wait(&s_wake, &g_cli_symbols_lock);
    }
    s_listing = cli_cell_taken(sizeof(*s_listing), "a reading of the tree's declarations");
    if (s_listing == NULL)
    {
        g_cli_symbols_reading = 0;
        cli_lock_give(&g_cli_symbols_lock);
        return;
    }
    unsigned long long offset = 0ull;
    s_listed_count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1ull;
        if (cli_symbols_reads(paths[at]) && ((offset + length) <= sizeof(s_listing->listed)) &&
            (length < CLI_PATH_LONGEST))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_listing->listed + offset, .src = paths[at], .bytes = length});
            offset += length;
            s_listed_count += 1u;
        }
    }
    s_thread.run = cli_reading;
    s_thread.context = s_listing;
    s_running = cli_thread_started(&s_thread);
    if (!s_running)
    {
        cli_cell_given(s_listing);
        s_listing = NULL;
    }
    g_cli_symbols_reading = s_running;
    cli_lock_give(&g_cli_symbols_lock);
}

int cli_symbols_progress(unsigned long long *done, unsigned long long *total)
{
    cli_lock_take(&g_cli_symbols_lock);
    const int going = g_cli_symbols_reading && !s_read;
    *done = s_done;
    *total = s_total;
    cli_lock_give(&g_cli_symbols_lock);
    return going;
}

void cli_symbols_stop(void)
{
    cli_lock_take(&g_cli_symbols_lock);
    s_stop = 1;
    cli_lock_give(&g_cli_symbols_lock);
}

int cli_symbols_unread(void)
{
    cli_lock_take(&g_cli_symbols_lock);
    const int unread = g_cli_symbols_reading && !s_read;
    cli_lock_give(&g_cli_symbols_lock);
    return unread;
}
