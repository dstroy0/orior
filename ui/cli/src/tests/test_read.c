// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_read.c: what a process of a run writes: its output read line by line as it comes, each line kept in its output
// file, and its errors read whole into its errors file on a thread of their own
#include "test_runs_internal.h"

// the output read and not yet written added to the process's output file, which is made anew with the first
static void read_batch_written(CliTestWorker *worker, int fresh)
{
    (void)cli_file_appended(worker->seen, worker->batch, worker->batch_size, fresh);
    worker->batch_size = 0ull;
}

// a line ended: handed on, and kept, with "\n" after it, in the output; a "\r" before its end taken off
static void read_line_ended(CliTestWorker *worker,
                            void (*line)(CliTestWorker *worker, const char *text, unsigned long long size), int *fresh)
{
    unsigned long long size = worker->line_size;
    size -= ((size > 0ull) && (worker->line[size - 1ull] == '\r')) ? 1ull : 0ull;
    line(worker, worker->line, size);
    if (worker->batch_size + size + 1ull > sizeof(worker->batch))
    {
        read_batch_written(worker, *fresh);
        *fresh = 0;
    }
    if (size + 1ull <= sizeof(worker->batch))
    {
        memor.cpy(&(MemoriaCfg){.dst = worker->batch + worker->batch_size, .src = worker->line, .bytes = size});
        worker->batch[worker->batch_size + size] = '\n';
        worker->batch_size += size + 1ull;
    }
    worker->line_size = 0ull;
}

void cli_test_lines_read(CliTestWorker *worker,
                         void (*line)(CliTestWorker *worker, const char *text, unsigned long long size))
{
    char piece[4096];
    int fresh = 1;
    int stopped = 0;
    worker->line_size = 0ull;
    worker->batch_size = 0ull;
    while (!stopped)
    {
        const unsigned long long count = cli_spawn_read(worker->spawned.out, piece, sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; (at < count) && !stopped; at += 1ull)
        {
            // a line longer than the room is handed on in pieces of the room's length
            if ((piece[at] == '\n') || (worker->line_size == sizeof(worker->line)))
            {
                stopped = cli_test_stopped();
                if (!stopped)
                {
                    read_line_ended(worker, line, &fresh);
                }
            }
            if ((piece[at] != '\n') && !stopped)
            {
                worker->line[worker->line_size] = piece[at];
                worker->line_size += 1ull;
            }
        }
    }
    if ((worker->line_size > 0ull) && !stopped && !cli_test_stopped())
    {
        read_line_ended(worker, line, &fresh);
    }
    read_batch_written(worker, fresh);
}

// a process's errors read whole into its errors file, then told read
static void read_errors(void *context)
{
    CliTestWorker *const worker = (CliTestWorker *)context;
    char piece[4096];
    int fresh = 1;
    for (;;)
    {
        const unsigned long long count = cli_spawn_read(worker->spawned.err, piece, sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        (void)cli_file_appended(worker->errors, piece, count, fresh);
        fresh = 0;
    }
    if (fresh)
    {
        (void)cli_file_appended(worker->errors, "", 0ull, 1);
    }
    CliTestRun *const run = worker->run;
    cli_lock_take(&run->lock);
    worker->errors_read = 1;
    run->going -= 1u;
    cli_wake_all(&run->wake);
    cli_lock_give(&run->lock);
}

void cli_test_errors_started(CliTestWorker *worker)
{
    CliTestRun *const run = worker->run;
    worker->errors_read = 0;
    cli_lock_take(&run->lock);
    run->going += 1u;
    cli_lock_give(&run->lock);
    worker->error_thread = (CliThread){read_errors, worker};
    if (!cli_thread_started(&worker->error_thread))
    {
        read_errors(worker);
    }
}

void cli_test_errors_waited(CliTestWorker *worker)
{
    CliTestRun *const run = worker->run;
    cli_lock_take(&run->lock);
    while (!worker->errors_read)
    {
        cli_wake_wait(&run->wake, &run->lock);
    }
    cli_lock_give(&run->lock);
}

const char *cli_test_file_read(const char *path, unsigned long long *size)
{
    *size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, size);
    if ((bytes != NULL) && !cli_utf8_whole((const char *)bytes, *size))
    {
        cli_file_let_go(bytes, *size);
        *size = 0ull;
        return NULL;
    }
    return (const char *)bytes;
}

void cli_test_file_let_go(const char *text, unsigned long long size)
{
    if (text != NULL)
    {
        cli_file_let_go((const unsigned char *)text, size);
    }
}
