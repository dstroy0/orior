// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_bench_files.c: the bench's passes over the tree's files as files: the corpus read by several threads at once,
// through the cells or through the system's allocator alone; the view of the tree's files picked as a search picks it;
// and the tree searched as the window's search does it
#include "../../../../../../ui/cli/src/code/symbols.h"
#include "../../../../../../ui/cli/src/command_line/callback_cfg.h"
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
    CliWake wake;
} BenchReaders;

static BenchReaders s_readers = {NULL, 0u, 0u, 0, CLI_WAKE_INIT};

// each file read in turn: through the cells, or through the system's allocator alone where the readers are plain
static void readers_run(void *context)
{
    (void)context;
    const BenchCorpus *const corpus = s_readers.corpus;
    for (;;)
    {
        const unsigned int index = s_readers.next;
        s_readers.next += (index < corpus->count) ? 1u : 0u;
        if (index >= corpus->count)
        {
            break;
        }
        unsigned long long size = 0ull;
        if (s_readers.plain)
        {
            // capacity for the whole file from the system's allocator, outside the cells
            unsigned char *capacity = EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = corpus->paths[index], .size_pointer = &size) ? malloc((size_t)size + 1u) : NULL;
            if (capacity != NULL)
            {
                (void)EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = corpus->paths[index], .from = 0ull, .bytes_bytes = capacity, .count_u64 = size, .got = &size);
            }
            free(capacity);
            continue;
        }
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = corpus->paths[index], .size_pointer = &size, .owner = &g_checks_cells);
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_checks_cells));
    }
    s_readers.running -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_readers.wake);
}

BenchMeasure bench_files_read_by(const BenchCorpus *corpus, unsigned int threads, int plain)
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
            if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread[at]))
            {
                readers_run(NULL);
            }
        }
        while (s_readers.running > 0u)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_readers.wake);
        }
        const unsigned long long consumed = cli_clock_microseconds() - began;
        measure.bytes = corpus->whole;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
    }
    return measure;
}

static int picked_include(const CliEachPathCfg *args)
{
    (void)args;
    return 1;
}

BenchMeasure bench_picked(const BenchCorpus *corpus)
{
    BenchMeasure measure = {0ull, ~0ull};
    char under[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = corpus->root, .relative = "ui/cli/src", .joined = under);
    for (unsigned int round = 0u; round < corpus->repeat; round += 1u)
    {
        // the view is read once, as the window caches it between searches; the first repeat pays for it
        if (round == 0u)
        {
            cli_tree_forgotten();
        }
        CliPicked picked = {NULL, 0u};
        const unsigned long long began = cli_clock_microseconds();
        (void)EMBED_CALL(cli_files_picked_internal.pick, CliFilesPickedCtx, .picked = &picked, .root = under, .filter = picked_include, .context = NULL);
        const unsigned long long consumed = cli_clock_microseconds() - began;
        EMBED_CALL(cli_files_picked_internal.picked_let, CliFilesPickedCtx, .picked = &picked);
        measure.bytes = corpus->whole;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
    }
    return measure;
}

BenchMeasure bench_search(const BenchCorpus *corpus, const char *needle)
{
    BenchMeasure measure = {0ull, ~0ull};
    char under[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = corpus->root, .relative = "ui/cli/src", .joined = under);
    const unsigned long long out_capacity = 1ull << 20u;
    char *const out_cell = g_cli_cells->temporary_buf_alloc(out_capacity, &g_checks_cells);
    VerbaLinea out = mmgr_line_over(out_cell, out_capacity);
    for (unsigned int round = 0u; round < corpus->repeat; round += 1u)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        const CliPatterns none = {0};
        out.at = 0ull;
        if (round == 0u)
        {
            cli_tree_forgotten();
        }
        const unsigned long long began = cli_clock_microseconds();
        (void)EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = under, .query = needle, .how = (CliSearching){0, 0, 0}, .set = &none, .out = &out, .err_msg = &err_msg);
        const unsigned long long consumed = cli_clock_microseconds() - began;
        measure.bytes = corpus->whole;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
    }
    g_cli_cells->persistent_buf_release(out.out, &g_checks_cells);
    return measure;
}
