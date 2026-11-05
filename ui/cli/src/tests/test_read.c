// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_read.c: what a process of a run writes: its output read line by line as it comes, each line stored in its output
// file, and its errors read whole into its errors file on a thread of their own
#include "../command_line/cells.h"
#include "test_runs_internal.h"

typedef struct
{
    CliTestWorker *worker;
    int fresh;
    void (*line)(const CliWorkerTextSizeCfg *args);
    int *fresh_pointer;
} TestReadCtx;

// the output read and not yet written added to the process's output file, which is made anew with the first
EMBED_INLINE void read_batch_write(const TestReadCtx *args)
{
    int fresh = args->fresh;
    (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->worker->seen,
                     .bytes = args->worker->batch, .size = args->worker->batch_size, .fresh = fresh);
    args->worker->batch_size = 0ULL;
}

// a line ended: handed on, and stored, with "\n" after it, in the output; a "\r" before its end taken off
EMBED_INLINE void read_line_on_end(const TestReadCtx *args)
{
    int *fresh = args->fresh_pointer;
    unsigned long long size = args->worker->line_size;
    size -= ((size > 0ULL) && (args->worker->line[size - 1ULL] == '\r')) ? 1ULL : 0ULL;
    EMBED_CALL(args->line, CliWorkerTextSizeCfg, .worker = args->worker, .text = args->worker->line, .size = size);
    if (args->worker->batch_size + size + 1ULL > sizeof(args->worker->batch))
    {
        EMBED_CALL(read_batch_write, TestReadCtx, .worker = args->worker, .fresh = *fresh);
        *fresh = 0;
    }
    if (size + 1ULL <= sizeof(args->worker->batch))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->worker->batch + args->worker->batch_size, .src = args->worker->line, .bytes = size});
        args->worker->batch[args->worker->batch_size + size] = '\n';
        args->worker->batch_size += size + 1ULL;
    }
    args->worker->line_size = 0ULL;
}

void cli_test_lines_read(const CliTestRunsWorkerLineCtx *args)
{
    CliTestWorker *worker = args->worker;
    void (*line)(const CliWorkerTextSizeCfg *) = args->line_callback;
    char piece[4096];
    int fresh = 1;
    int stopped = 0;
    worker->line_size = 0ULL;
    worker->batch_size = 0ULL;
    while (!stopped)
    {
        const unsigned long long count =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = worker->spawned.out, .piece = piece, .capacity = sizeof(piece));
        if (count == 0ULL)
        {
            break;
        }
        unsigned long long at = 0ULL;
        while ((at < count) && !stopped)
        {
            // a line longer than the capacity is handed on in pieces of the capacity's length
            const int ends = (piece[at] == '\n') || (worker->line_size == sizeof(worker->line));
            stopped = ends ? cli_test_stop() : stopped;
            if (ends && !stopped)
            {
                EMBED_CALL(read_line_on_end, TestReadCtx, .worker = worker, .line = line, .fresh_pointer = &fresh);
            }
            if ((piece[at] != '\n') && !stopped)
            {
                worker->line[worker->line_size] = piece[at];
                worker->line_size += 1ULL;
            }
            at += 1ULL;
        }
    }
    if ((worker->line_size > 0ULL) && !stopped && !cli_test_stop())
    {
        EMBED_CALL(read_line_on_end, TestReadCtx, .worker = worker, .line = line, .fresh_pointer = &fresh);
    }
    EMBED_CALL(read_batch_write, TestReadCtx, .worker = worker, .fresh = fresh);
}

// a process's errors read whole into its errors file, then reported read
static void read_errors(void *context)
{
    CliTestWorker *const worker = (CliTestWorker *)context;
    char piece[4096];
    int fresh = 1;
    for (;;)
    {
        const unsigned long long count =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = worker->spawned.err, .piece = piece, .capacity = sizeof(piece));
        if (count == 0ULL)
        {
            break;
        }
        (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = worker->errors, .bytes = piece,
                         .size = count, .fresh = fresh);
        fresh = 0;
    }
    if (fresh)
    {
        (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = worker->errors, .bytes = "",
                         .size = 0ULL, .fresh = 1);
    }
    CliTestRun *const run = worker->run;
    worker->errors_read = 1;
    run->going -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &run->wake);
}

void cli_test_errors_start(const CliTestRunsWorkerLineCtx *args)
{
    CliTestRun *const run = args->worker->run;
    args->worker->errors_read = 0;
    run->going += 1u;
    args->worker->error_thread = (CliThread){read_errors, args->worker};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &args->worker->error_thread))
    {
        read_errors(args->worker);
    }
}

void cli_test_errors_wait(const CliTestRunsWorkerLineCtx *args)
{
    CliTestRun *const run = args->worker->run;
    while (!args->worker->errors_read)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &run->wake);
    }
}

const char *cli_test_file_read(const CliTestRunsPathCtx *args)
{
    unsigned long long *size = args->size_pointer;
    *size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = size, .owner = &g_cli_tests_cells);
    if ((bytes != NULL) &&
        !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = *size))
    {
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_tests_cells));
        *size = 0ULL;
        return NULL;
    }
    return (const char *)bytes;
}

void cli_test_file_let_go(const CliTestRunsTextWorkerCtx *args)
{
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)args->text), (&g_cli_tests_cells));
}
