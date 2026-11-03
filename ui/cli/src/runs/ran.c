// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ran.c: a program run and what it gave read, its errors on a thread of their own while its output is read, so
// neither waits on a pipe the other fills; each read a piece at a time into cells of the pieces' own, and once the
// pipe ends, the pieces joined in one cell the size of what came and given back
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

#include <stddef.h>

// the bytes one piece of what a run gives is read into
#define CLI_RUN_RESULT_PIECE (64u << 10u)

// A piece of what a run gave: the piece after it, its bytes read, and its bytes, after it in its cell
typedef struct CliRunResultPieceStruct
{
    struct CliRunResultPieceStruct *next;
    unsigned long long size;
    char *bytes;
} CliRunResultPiece;

// A work state a run takes: the count of its reader's, writer's and watcher's ends, the process, the threads that read
// its errors, write its input and watch its time, the input, the milliseconds it may run, whether its output has ended,
// and what it gave
typedef struct
{
    CliWake wake;
    unsigned int ended;
    CliSpawned spawned;
    CliThread reader;
    CliThread writer;
    CliThread watcher;
    const char *input;
    unsigned long long length;
    unsigned long long limit;
    int finished;
    CliRunResult run_result;
} CliRunResultWorkState;

// the empty text a run that gave nothing points at, and the run handed out where the cells cannot meet a run, which
// every run refuses
static char s_nothing[1];
static CliRunResult s_refused = {s_nothing, 0ULL, s_nothing, 0ULL, 1, -1LL, 0};

CliRunResult *cli_run_result_alloc(void)
{
    CliRunResultWorkState *const work_state = g_cli_cells->persistent_buf_alloc(sizeof(*work_state), &g_cli_runs_cells);
    if (work_state == NULL)
    {
        return &s_refused;
    }
    const CliWake wake = CLI_WAKE_INIT;
    work_state->wake = wake;
    work_state->run_result = (CliRunResult){s_nothing, 0ULL, s_nothing, 0ULL, 1, -1LL, 0};
    return &work_state->run_result;
}

typedef struct
{
    CliRunResult *run_result;
    unsigned long long pipe;
    char **text;
    unsigned long long *size;
    int *whole;
} RanCtx;
// the work state that contains `run_result`, one cli_run_result_alloc took from the cells
EMBED_INLINE CliRunResultWorkState *cli_capacity_get(const RanCtx *args)
{
    return (CliRunResultWorkState *)(void *)((char *)args->run_result - offsetof(CliRunResultWorkState, run_result));
}

// what a run gave given back, its texts the empty one again
EMBED_INLINE void cli_run_result_texts_free(const RanCtx *args)
{
    if (args->run_result->out != s_nothing)
    {
        g_cli_cells->persistent_buf_release(args->run_result->out, &g_cli_runs_cells);
    }
    if (args->run_result->err != s_nothing)
    {
        g_cli_cells->persistent_buf_release(args->run_result->err, &g_cli_runs_cells);
    }
    args->run_result->out = s_nothing;
    args->run_result->err = s_nothing;
    args->run_result->out_size = 0ULL;
    args->run_result->err_size = 0ULL;
}

void cli_run_result_free(const CliWordsCtx *args)
{
    if (args->run_result != &s_refused)
    {
        EMBED_CALL(cli_run_result_texts_free, RanCtx, .run_result = args->run_result);
        g_cli_cells->persistent_buf_release(EMBED_CALL(cli_capacity_get, RanCtx, .run_result = args->run_result),
                                            &g_cli_runs_cells);
    }
}

// what is left in `pipe` read into a piece of the stack's and dropped, so that the program does not stand blocked on a
// full pipe
EMBED_INLINE void cli_pipe_drain(const RanCtx *args)
{
    char reserve[4096];
    while (EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx, .pipe_handle = args->pipe,
                      .piece = reserve, .capacity = sizeof(reserve)) != 0ULL)
    {
        // each piece read is dropped
    }
}

// what `pipe` gives read until it ends, a piece at a time, then joined in one cell of its size with a 0 after: the
// text into `text`, the empty one where nothing came, its size into `size`, and 0 into `whole` where a cell could not
// be taken and what came is not all stored
static void cli_pipe_read(const RanCtx *args)
{
    char **text = args->text;
    unsigned long long *size = args->size;
    int *whole = args->whole;
    CliRunResultPiece *first = NULL;
    CliRunResultPiece *last = NULL;
    unsigned long long total = 0ULL;
    int going = 1;
    while (going)
    {
        if ((last == NULL) || (last->size == CLI_RUN_RESULT_PIECE))
        {
            CliRunResultPiece *const piece =
                g_cli_cells->persistent_buf_alloc(sizeof(CliRunResultPiece) + CLI_RUN_RESULT_PIECE, &g_cli_runs_cells);
            if (piece == NULL)
            {
                *whole = 0;
                EMBED_CALL(cli_pipe_drain, RanCtx, .pipe = args->pipe);
                going = 0;
                continue;
            }
            piece->bytes = (char *)(void *)(piece + 1);
            piece->next = NULL;
            piece->size = 0ULL;
            if (last != NULL)
            {
                last->next = piece;
            }
            else
            {
                first = piece;
            }
            last = piece;
        }
        const unsigned long long got = EMBED_CALL(
            cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx, .pipe_handle = args->pipe,
            .piece = last->bytes + last->size, .capacity = CLI_RUN_RESULT_PIECE - last->size);
        if (got == 0ULL)
        {
            going = 0;
            continue;
        }
        last->size += got;
        total += got;
    }
    char *const joined = (total != 0ULL) ? g_cli_cells->persistent_buf_alloc(total + 1ULL, &g_cli_runs_cells) : NULL;
    *whole = *whole && ((total == 0ULL) || (joined != NULL));
    unsigned long long at = 0ULL;
    for (CliRunResultPiece *piece = first; piece != NULL;)
    {
        CliRunResultPiece *const next = piece->next;
        if (joined != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = joined + at, .src = piece->bytes, .bytes = (size_t)piece->size});
            at += piece->size;
        }
        g_cli_cells->persistent_buf_release(piece, &g_cli_runs_cells);
        piece = next;
    }
    if (joined != NULL)
    {
        joined[at] = '\0';
    }
    *text = (joined != NULL) ? joined : s_nothing;
    *size = (joined != NULL) ? at : 0ULL;
}

// what the program reports went wrong read until it ends, its end reported
static void cli_errors_read(void *context)
{
    CliRunResultWorkState *const work_state = (CliRunResultWorkState *)context;
    CliRunResult *const run_result = &work_state->run_result;
    int whole = 1;
    EMBED_CALL(cli_pipe_read, RanCtx, .pipe = work_state->spawned.err, .text = &run_result->err,
               .size = &run_result->err_size, .whole = &whole);
    run_result->whole = run_result->whole && whole;
    work_state->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &work_state->wake);
}

// the run's input written whole and its pipe closed, which the program reads as its input's end, the end reported
static void cli_input_write(void *context)
{
    CliRunResultWorkState *const work_state = (CliRunResultWorkState *)context;
    EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = work_state->spawned.in,
               .bytes = work_state->input, .size = work_state->length);
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &work_state->spawned);
    work_state->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &work_state->wake);
}

// the run's time watched until its output ends, the run ended with every process under it where it runs past its
// limit first, the end reported
static void cli_on_watch(void *context)
{
    CliRunResultWorkState *const work_state = (CliRunResultWorkState *)context;
    const unsigned long long began = cli_clock_milliseconds();
    int going = 1;
    while (going)
    {
        const int finished = work_state->finished;
        if (finished)
        {
            going = 0;
            continue;
        }
        if ((cli_clock_milliseconds() - began) >= work_state->limit)
        {
            work_state->run_result.stopped = 1;
            EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = work_state->spawned.id);
            going = 0;
            continue;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
    work_state->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &work_state->wake);
}

int cli_on_run(const CliWordsCtx *args)
{
    return EMBED_CALL(cli_words_internal.run_result_within, CliWordsCtx, .words = args->words, .folder = args->folder,
                      .settings = args->settings, .count = args->count, .input = NULL, .length = 0ULL,
                      .milliseconds_u64 = 0ULL, .run_result = args->run_result, .err_msg = args->err_msg);
}

int cli_run_result_feed(const CliWordsCtx *args)
{
    return EMBED_CALL(cli_words_internal.run_result_within, CliWordsCtx, .words = args->words, .folder = args->folder,
                      .settings = args->settings, .count = args->count, .input = args->input, .length = args->length,
                      .milliseconds_u64 = 0ULL, .run_result = args->run_result, .err_msg = args->err_msg);
}

int cli_run_result_within(const CliWordsCtx *args)
{
    const char *input = args->input;
    unsigned long long length = args->length;
    if (args->run_result == &s_refused)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet a program run");
        return 0;
    }
    CliRunResultWorkState *const work_state = EMBED_CALL(cli_capacity_get, RanCtx, .run_result = args->run_result);
    EMBED_CALL(cli_run_result_texts_free, RanCtx, .run_result = args->run_result);
    args->run_result->whole = 1;
    args->run_result->code = -1LL;
    args->run_result->stopped = 0;
    const int started =
        (input != NULL)
            ? EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &work_state->spawned,
                         .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                         .detached = 1, .err_msg = args->err_msg)
            : EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &work_state->spawned,
                         .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                         .detached = 1, .err_msg = args->err_msg);
    if (!started)
    {
        return 0;
    }
    work_state->ended = 0u;
    work_state->input = input;
    work_state->length = length;
    work_state->reader = (CliThread){cli_errors_read, work_state};
    work_state->writer = (CliThread){cli_input_write, work_state};
    work_state->limit = args->milliseconds_u64;
    work_state->finished = 0;
    work_state->watcher = (CliThread){cli_on_watch, work_state};
    // the reader and the writer each report their end, on a thread of their own or, where none starts, here; a watcher
    // that cannot start leaves the run to take its time
    const unsigned int watching =
        ((args->milliseconds_u64 != 0ULL) &&
         EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &work_state->watcher))
            ? 1u
            : 0u;
    const unsigned int ending = ((input != NULL) ? 2u : 1u) + watching;
    const int read_state = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &work_state->reader);
    if ((input != NULL) && !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &work_state->writer))
    {
        cli_input_write(work_state);
    }
    int whole = 1;
    char *out = s_nothing;
    unsigned long long out_size = 0ULL;
    EMBED_CALL(cli_pipe_read, RanCtx, .pipe = work_state->spawned.out, .text = &out, .size = &out_size,
               .whole = &whole);
    if (!read_state)
    {
        cli_errors_read(work_state);
    }
    work_state->finished = 1;
    while (work_state->ended < ending)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &work_state->wake);
    }
    args->run_result->out = out;
    args->run_result->out_size = out_size;
    args->run_result->whole = args->run_result->whole && whole;
    args->run_result->code =
        EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &work_state->spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &work_state->spawned);
    return 1;
}
