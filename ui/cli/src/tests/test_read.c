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
    args->worker->batch_size = 0ull;
}

// a line ended: handed on, and stored, with "\n" after it, in the output; a "\r" before its end taken off
EMBED_INLINE void read_line_on_end(const TestReadCtx *args)
{
    int *fresh = args->fresh_pointer;
    unsigned long long size = args->worker->line_size;
    size -= ((size > 0ull) && (args->worker->line[size - 1ull] == '\r')) ? 1ull : 0ull;
    EMBED_CALL(args->line, CliWorkerTextSizeCfg, .worker = args->worker, .text = args->worker->line, .size = size);
    if (args->worker->batch_size + size + 1ull > sizeof(args->worker->batch))
    {
        EMBED_CALL(read_batch_write, TestReadCtx, .worker = args->worker, .fresh = *fresh);
        *fresh = 0;
    }
    if (size + 1ull <= sizeof(args->worker->batch))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->worker->batch + args->worker->batch_size, .src = args->worker->line, .bytes = size});
        args->worker->batch[args->worker->batch_size + size] = '\n';
        args->worker->batch_size += size + 1ull;
    }
    args->worker->line_size = 0ull;
}

void cli_test_lines_read(const CliTestRunsWorkerLineCtx *args)
{
    CliTestWorker *worker = args->worker;
    void (*line)(const CliWorkerTextSizeCfg *args) = args->line_callback;
    char piece[4096];
    int fresh = 1;
    int stopped = 0;
    worker->line_size = 0ull;
    worker->batch_size = 0ull;
    while (!stopped)
    {
        const unsigned long long count =
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                       .pipe_handle = worker->spawned.out, .piece = piece, .room = sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; (at < count) && !stopped; at += 1ull)
        {
            // a line longer than the room is handed on in pieces of the room's length
            if ((piece[at] == '\n') || (worker->line_size == sizeof(worker->line)))
            {
                stopped = cli_test_stop();
                if (!stopped)
                {
                    EMBED_CALL(read_line_on_end, TestReadCtx, .worker = worker, .line = line, .fresh_pointer = &fresh);
                }
            }
            if ((piece[at] != '\n') && !stopped)
            {
                worker->line[worker->line_size] = piece[at];
                worker->line_size += 1ull;
            }
        }
    }
    if ((worker->line_size > 0ull) && !stopped && !cli_test_stop())
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
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                       .pipe_handle = worker->spawned.err, .piece = piece, .room = sizeof(piece));
        if (count == 0ull)
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
                         .size = 0ull, .fresh = 1);
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
    *size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->path,
                                                  .size_pointer = size, .owner = &g_cli_tests_cells);
    if ((bytes != NULL) &&
        !EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = *size))
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_tests_cells);
        *size = 0ull;
        return NULL;
    }
    return (const char *)bytes;
}

void cli_test_file_let_go(const CliTestRunsTextWorkerCtx *args)
{
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = (const unsigned char *)args->text,
               .owner = &g_cli_tests_cells);
}
