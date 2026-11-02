// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ran.c: a program run and what it gave read, its errors on a thread of their own while its output is read, so
// neither waits on a pipe the other fills; each read a piece at a time into cells of the pieces' own, and once the
// pipe ends, the pieces joined in one cell the size of what came and given back
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

#include <stddef.h>

// the bytes one piece of what a run gives is read into
#define CLI_RAN_PIECE (64u << 10u)

// A piece of what a run gave: the piece after it, its bytes read, and its bytes
typedef struct CliRanPieceStruct
{
    struct CliRanPieceStruct *next;
    unsigned long long size;
    char bytes[CLI_RAN_PIECE];
} CliRanPiece;

// A room a run takes: the count of its reader's, writer's and watcher's ends, the process, the threads that read its
// errors, write its input and watch its time, the input, the milliseconds it may run, whether its output has ended, and
// what it gave
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
    CliRan ran;
} CliRanRoom;

// the empty text a run that gave nothing points at, and the run handed out where the cells cannot meet a run, which
// every run refuses
static char s_nothing[1];
static CliRan s_refused = {s_nothing, 0ull, s_nothing, 0ull, 1, -1ll, 0};

CliRan *cli_ran_take(void)
{
    CliRanRoom *const room = g_cli_cells->persistent_buf_alloc(sizeof(*room), &g_cli_runs_cells);
    if (room == NULL)
    {
        return &s_refused;
    }
    const CliWake wake = CLI_WAKE_INIT;
    room->wake = wake;
    room->ran = (CliRan){s_nothing, 0ull, s_nothing, 0ull, 1, -1ll, 0};
    return &room->ran;
}

typedef struct
{
    CliRan *ran;
    unsigned long long pipe;
    char **text;
    unsigned long long *size;
    int *whole;
} RanCtx;
// the room that contains `ran`, one cli_ran_take took from the cells
EMBED_INLINE CliRanRoom *cli_room_get(const RanCtx *args)
{
    return (CliRanRoom *)(void *)((char *)args->ran - offsetof(CliRanRoom, ran));
}

// what a run gave given back, its texts the empty one again
EMBED_INLINE void cli_ran_texts_free(const RanCtx *args)
{
    if (args->ran->out != s_nothing)
    {
        g_cli_cells->persistent_buf_release(args->ran->out, &g_cli_runs_cells);
    }
    if (args->ran->err != s_nothing)
    {
        g_cli_cells->persistent_buf_release(args->ran->err, &g_cli_runs_cells);
    }
    args->ran->out = s_nothing;
    args->ran->err = s_nothing;
    args->ran->out_size = 0ull;
    args->ran->err_size = 0ull;
}

void cli_ran_free(const CliWordsCtx *args)
{
    if (args->ran != &s_refused)
    {
        EMBED_CALL(cli_ran_texts_free, RanCtx, .ran = args->ran);
        g_cli_cells->persistent_buf_release(EMBED_CALL(cli_room_get, RanCtx, .ran = args->ran), &g_cli_runs_cells);
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
    CliRanPiece *first = NULL;
    CliRanPiece *last = NULL;
    unsigned long long total = 0ull;
    for (;;)
    {
        if ((last == NULL) || (last->size == CLI_RAN_PIECE))
        {
            CliRanPiece *const piece = g_cli_cells->persistent_buf_alloc(sizeof(CliRanPiece), &g_cli_runs_cells);
            if (piece == NULL)
            {
                // the pipe drained into a piece of the stack's, and the program is not blocked writing
                char spare[4096];
                *whole = 0;
                while (EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                                  .pipe_handle = args->pipe, .piece = spare, .room = sizeof(spare)) != 0ull)
                {
                }
                break;
            }
            piece->next = NULL;
            piece->size = 0ull;
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
        const unsigned long long got =
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx, .pipe_handle = args->pipe,
                       .piece = last->bytes + last->size, .room = CLI_RAN_PIECE - last->size);
        if (got == 0ull)
        {
            break;
        }
        last->size += got;
        total += got;
    }
    char *const joined = (total != 0ull) ? g_cli_cells->persistent_buf_alloc(total + 1ull, &g_cli_runs_cells) : NULL;
    *whole = *whole && ((total == 0ull) || (joined != NULL));
    unsigned long long at = 0ull;
    for (CliRanPiece *piece = first; piece != NULL;)
    {
        CliRanPiece *const next = piece->next;
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
    *size = (joined != NULL) ? at : 0ull;
}

// what the program reports went wrong read until it ends, its end reported
static void cli_errors_read(void *context)
{
    CliRanRoom *const room = (CliRanRoom *)context;
    CliRan *const ran = &room->ran;
    int whole = 1;
    EMBED_CALL(cli_pipe_read, RanCtx, .pipe = room->spawned.err, .text = &ran->err, .size = &ran->err_size,
               .whole = &whole);
    ran->whole = ran->whole && whole;
    room->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &room->wake);
}

// the run's input written whole and its pipe closed, which the program reads as its input's end, the end reported
static void cli_input_write(void *context)
{
    CliRanRoom *const room = (CliRanRoom *)context;
    EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = room->spawned.in,
               .bytes = room->input, .size = room->length);
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &room->spawned);
    room->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &room->wake);
}

// the run's time watched until its output ends, the run ended with every process under it where it runs past its
// limit first, the end reported
static void cli_on_watch(void *context)
{
    CliRanRoom *const room = (CliRanRoom *)context;
    const unsigned long long began = cli_clock_milliseconds();
    for (;;)
    {
        const int finished = room->finished;
        if (finished)
        {
            break;
        }
        if ((cli_clock_milliseconds() - began) >= room->limit)
        {
            room->ran.stopped = 1;
            EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = room->spawned.id);
            break;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
    room->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &room->wake);
}

int cli_on_run(const CliWordsCtx *args)
{
    return EMBED_CALL(cli_words_internal.ran_within, CliWordsCtx, .words = args->words, .folder = args->folder,
                      .settings = args->settings, .count = args->count, .input = NULL, .length = 0ull,
                      .milliseconds_u64 = 0ull, .ran = args->ran, .err_msg = args->err_msg);
}

int cli_ran_feed(const CliWordsCtx *args)
{
    return EMBED_CALL(cli_words_internal.ran_within, CliWordsCtx, .words = args->words, .folder = args->folder,
                      .settings = args->settings, .count = args->count, .input = args->input, .length = args->length,
                      .milliseconds_u64 = 0ull, .ran = args->ran, .err_msg = args->err_msg);
}

int cli_ran_within(const CliWordsCtx *args)
{
    const char *input = args->input;
    unsigned long long length = args->length;
    if (args->ran == &s_refused)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet a program run");
        return 0;
    }
    CliRanRoom *const room = EMBED_CALL(cli_room_get, RanCtx, .ran = args->ran);
    EMBED_CALL(cli_ran_texts_free, RanCtx, .ran = args->ran);
    args->ran->whole = 1;
    args->ran->code = -1ll;
    args->ran->stopped = 0;
    const int started =
        (input != NULL)
            ? EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &room->spawned,
                         .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                         .detached = 1, .err_msg = args->err_msg)
            : EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &room->spawned,
                         .words = args->words, .folder = args->folder, .settings = args->settings, .count = args->count,
                         .detached = 1, .err_msg = args->err_msg);
    if (!started)
    {
        return 0;
    }
    room->ended = 0u;
    room->input = input;
    room->length = length;
    room->reader = (CliThread){cli_errors_read, room};
    room->writer = (CliThread){cli_input_write, room};
    room->limit = args->milliseconds_u64;
    room->finished = 0;
    room->watcher = (CliThread){cli_on_watch, room};
    // the reader and the writer each report their end, on a thread of their own or, where none starts, here; a watcher
    // that cannot start leaves the run to take its time
    const unsigned int watching = ((args->milliseconds_u64 != 0ull) &&
                                   EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &room->watcher))
                                      ? 1u
                                      : 0u;
    const unsigned int ending = ((input != NULL) ? 2u : 1u) + watching;
    const int reading = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &room->reader);
    if ((input != NULL) && !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &room->writer))
    {
        cli_input_write(room);
    }
    int whole = 1;
    char *out = s_nothing;
    unsigned long long out_size = 0ull;
    EMBED_CALL(cli_pipe_read, RanCtx, .pipe = room->spawned.out, .text = &out, .size = &out_size, .whole = &whole);
    if (!reading)
    {
        cli_errors_read(room);
    }
    room->finished = 1;
    while (room->ended < ending)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &room->wake);
    }
    args->ran->out = out;
    args->ran->out_size = out_size;
    args->ran->whole = args->ran->whole && whole;
    args->ran->code = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &room->spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &room->spawned);
    return 1;
}
