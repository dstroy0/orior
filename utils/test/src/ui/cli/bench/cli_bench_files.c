// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_bench_files.c: the bench's passes over the tree's files as files: the corpus read by several threads at once,
// through the cells or through the system's allocator alone; the view of the tree's files picked as a search picks it;
// and the tree searched as the window's search does it
#include "../../../../../../ui/cli/src/editor/files.h"
#include "../../../../../../ui/cli/src/editor/files_internal.h"
#include "bench.h"

#include <stdlib.h>

// The corpus read by several threads at once: the corpus, the next file to read, how many threads still run, their
// wake, and whether the reads go through the system's allocator alone
typedef struct
{
    const BenchCorpus *corpus;
    unsigned int next;
    unsigned int running;
    int plain;
    CliLock lock;
    CliWake wake;
} BenchReaders;

static BenchReaders s_readers = {NULL, 0u, 0u, 0, CLI_LOCK_INIT, CLI_WAKE_INIT};

// room for a whole file from the system's allocator, outside the cells, kept in `context` to be freed
static unsigned char *readers_roomed(void *context, unsigned long long size)
{
    unsigned char **const room = context;
    *room = malloc((size_t)size + 1u);
    return *room;
}

// each file read in turn: through the cells, or through the system's allocator alone where the readers are plain
static void readers_run(void *context)
{
    (void)context;
    const BenchCorpus *const corpus = s_readers.corpus;
    for (;;)
    {
        cli_lock_take(&s_readers.lock);
        const unsigned int index = s_readers.next;
        s_readers.next += (index < corpus->count) ? 1u : 0u;
        cli_lock_give(&s_readers.lock);
        if (index >= corpus->count)
        {
            break;
        }
        unsigned long long size = 0ull;
        if (s_readers.plain)
        {
            unsigned char *room = NULL;
            (void)cli_file_read_whole(corpus->paths[index], readers_roomed, &room, &size);
            free(room);
            continue;
        }
        const unsigned char *const bytes = cli_file_held(corpus->paths[index], &size);
        cli_file_let_go(bytes, size);
    }
    cli_lock_take(&s_readers.lock);
    s_readers.running -= 1u;
    cli_wake_all(&s_readers.wake);
    cli_lock_give(&s_readers.lock);
}

BenchMeasure bench_held_by(const BenchCorpus *corpus, unsigned int threads, int plain)
{
    BenchMeasure measure = {0ull, ~0ull};
    static CliThread s_thread[16];
    threads = (threads > 16u) ? 16u : threads;
    s_readers.corpus = corpus;
    s_readers.plain = plain;
    for (unsigned int round = 0u; round < corpus->repeat; round += 1u)
    {
        s_readers.next = 0u;
        s_readers.running = threads;
        const unsigned long long began = cli_clock_microseconds();
        for (unsigned int at = 0u; at < threads; at += 1u)
        {
            s_thread[at] = (CliThread){readers_run, NULL, CLI_PRIORITY_WORK};
            if (!cli_thread_started(&s_thread[at]))
            {
                readers_run(NULL);
            }
        }
        cli_lock_take(&s_readers.lock);
        while (s_readers.running > 0u)
        {
            cli_wake_wait(&s_readers.wake, &s_readers.lock);
        }
        cli_lock_give(&s_readers.lock);
        const unsigned long long took = cli_clock_microseconds() - began;
        measure.bytes = corpus->whole;
        measure.least = (took < measure.least) ? took : measure.least;
    }
    return measure;
}

static int picked_keeps(void *context, const char *path)
{
    (void)context;
    (void)path;
    return 1;
}

BenchMeasure bench_picked(const BenchCorpus *corpus)
{
    BenchMeasure measure = {0ull, ~0ull};
    char under[CLI_PATH_LONGEST];
    cli_full(corpus->root, "ui/cli/src", under);
    for (unsigned int round = 0u; round < corpus->repeat; round += 1u)
    {
        // the view is read once, as the window holds it between searches; the first repeat pays for it
        if (round == 0u)
        {
            cli_files_forget_seen();
        }
        CliPicked picked = {NULL, NULL, 0u};
        const unsigned long long began = cli_clock_microseconds();
        (void)cli_files_picked(&picked, under, picked_keeps, NULL);
        const unsigned long long took = cli_clock_microseconds() - began;
        cli_picked_let(&picked);
        measure.bytes = corpus->whole;
        measure.least = (took < measure.least) ? took : measure.least;
    }
    return measure;
}

BenchMeasure bench_search(const BenchCorpus *corpus, const char *needle)
{
    BenchMeasure measure = {0ull, ~0ull};
    char under[CLI_PATH_LONGEST];
    cli_full(corpus->root, "ui/cli/src", under);
    CliLine out;
    (void)cli_line_growing(&out, 1ull << 20u, "the search's hits");
    for (unsigned int round = 0u; round < corpus->repeat; round += 1u)
    {
        char why[CLI_SAID_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        const CliPatterns none = {0};
        out.at = 0ull;
        if (round == 0u)
        {
            cli_files_forget_seen();
        }
        const unsigned long long began = cli_clock_microseconds();
        (void)cli_files_search(under, needle, (CliSearching){0, 0, 0}, &none, &out, &said);
        const unsigned long long took = cli_clock_microseconds() - began;
        measure.bytes = corpus->whole;
        measure.least = (took < measure.least) ? took : measure.least;
    }
    cli_cell_given(out.out);
    return measure;
}
