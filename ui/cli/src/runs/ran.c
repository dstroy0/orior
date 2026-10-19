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
    CliLock lock;
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

CliRan *cli_ran_taken(void)
{
    CliRanRoom *const room = cli_cell_taken(sizeof(*room), "a program run");
    if (room == NULL)
    {
        return &s_refused;
    }
    const CliLock lock = CLI_LOCK_INIT;
    const CliWake wake = CLI_WAKE_INIT;
    room->lock = lock;
    room->wake = wake;
    room->ran = (CliRan){s_nothing, 0ull, s_nothing, 0ull, 1, -1ll, 0};
    return &room->ran;
}

// the room that holds `ran`, one cli_ran_taken took from the cells
static CliRanRoom *cli_room_of(CliRan *ran)
{
    return (CliRanRoom *)(void *)((char *)ran - offsetof(CliRanRoom, ran));
}

// what a run gave given back, its texts the empty one again
static void cli_ran_texts_given(CliRan *ran)
{
    if (ran->out != s_nothing)
    {
        cli_cell_given(ran->out);
    }
    if (ran->err != s_nothing)
    {
        cli_cell_given(ran->err);
    }
    ran->out = s_nothing;
    ran->err = s_nothing;
    ran->out_size = 0ull;
    ran->err_size = 0ull;
}

void cli_ran_given(CliRan *ran)
{
    if (ran != &s_refused)
    {
        cli_ran_texts_given(ran);
        cli_cell_given(cli_room_of(ran));
    }
}

// what `pipe` gives read until it ends, a piece at a time, then joined in one cell of its size with a 0 after: the
// text into `text`, the empty one where nothing came, its size into `size`, and 0 into `whole` where a cell could not
// be taken and what came is not all kept
static void cli_pipe_read(unsigned long long pipe, char **text, unsigned long long *size, int *whole)
{
    CliRanPiece *first = NULL;
    CliRanPiece *last = NULL;
    unsigned long long total = 0ull;
    for (;;)
    {
        if ((last == NULL) || (last->size == CLI_RAN_PIECE))
        {
            CliRanPiece *const piece = cli_cell_taken(sizeof(CliRanPiece), "a piece of what a program gave");
            if (piece == NULL)
            {
                // the pipe drained into a piece of the stack's, so the program is not held up writing
                char spare[4096];
                *whole = 0;
                while (cli_spawn_read(pipe, spare, sizeof(spare)) != 0ull)
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
        const unsigned long long got = cli_spawn_read(pipe, last->bytes + last->size, CLI_RAN_PIECE - last->size);
        if (got == 0ull)
        {
            break;
        }
        last->size += got;
        total += got;
    }
    char *const joined = (total != 0ull) ? cli_cell_taken(total + 1ull, "what a program gave") : NULL;
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
        cli_cell_given(piece);
        piece = next;
    }
    if (joined != NULL)
    {
        joined[at] = '\0';
    }
    *text = (joined != NULL) ? joined : s_nothing;
    *size = (joined != NULL) ? at : 0ull;
}

// what the program says went wrong read until it ends, its end told
static void cli_errors_read(void *context)
{
    CliRanRoom *const room = (CliRanRoom *)context;
    CliRan *const ran = &room->ran;
    int whole = 1;
    cli_pipe_read(room->spawned.err, &ran->err, &ran->err_size, &whole);
    cli_lock_take(&room->lock);
    ran->whole = ran->whole && whole;
    room->ended += 1u;
    cli_wake_all(&room->wake);
    cli_lock_give(&room->lock);
}

// the run's input written whole and its pipe closed, which the program reads as its input's end, the end told
static void cli_input_written(void *context)
{
    CliRanRoom *const room = (CliRanRoom *)context;
    cli_spawn_written(room->spawned.in, room->input, room->length);
    cli_spawn_input_closed(&room->spawned);
    cli_lock_take(&room->lock);
    room->ended += 1u;
    cli_wake_all(&room->wake);
    cli_lock_give(&room->lock);
}

// the run's time watched until its output ends, the run ended with every process under it where it runs past its
// limit first, the end told
static void cli_watched(void *context)
{
    CliRanRoom *const room = (CliRanRoom *)context;
    const unsigned long long began = cli_clock_milliseconds();
    for (;;)
    {
        cli_lock_take(&room->lock);
        const int finished = room->finished;
        cli_lock_give(&room->lock);
        if (finished)
        {
            break;
        }
        if ((cli_clock_milliseconds() - began) >= room->limit)
        {
            room->ran.stopped = 1;
            cli_spawn_tree_end(room->spawned.id);
            break;
        }
        query_paused(20u);
    }
    cli_lock_take(&room->lock);
    room->ended += 1u;
    cli_wake_all(&room->wake);
    cli_lock_give(&room->lock);
}

int cli_ran(const char *const *words, const char *folder, const CliSetting *settings, unsigned int count, CliRan *ran,
            CliLine *said)
{
    return cli_ran_within(words, folder, settings, count, NULL, 0ull, 0ull, ran, said);
}

int cli_ran_fed(const char *const *words, const char *folder, const CliSetting *settings, unsigned int count,
                const char *input, unsigned long long length, CliRan *ran, CliLine *said)
{
    return cli_ran_within(words, folder, settings, count, input, length, 0ull, ran, said);
}

int cli_ran_within(const char *const *words, const char *folder, const CliSetting *settings, unsigned int count,
                   const char *input, unsigned long long length, unsigned long long milliseconds, CliRan *ran,
                   CliLine *said)
{
    if (ran == &s_refused)
    {
        cli_line_text(said, "the cells cannot meet a program run");
        return 0;
    }
    CliRanRoom *const room = cli_room_of(ran);
    cli_ran_texts_given(ran);
    ran->whole = 1;
    ran->code = -1ll;
    ran->stopped = 0;
    const int started = (input != NULL) ? cli_spawn_fed(&room->spawned, words, folder, settings, count, 1, said)
                                        : cli_spawn(&room->spawned, words, folder, settings, count, 1, said);
    if (!started)
    {
        return 0;
    }
    room->ended = 0u;
    room->input = input;
    room->length = length;
    room->reader = (CliThread){cli_errors_read, room};
    room->writer = (CliThread){cli_input_written, room};
    room->limit = milliseconds;
    room->finished = 0;
    room->watcher = (CliThread){cli_watched, room};
    // the reader and the writer each tell their end, on a thread of their own or, where none starts, here; a watcher
    // that cannot start leaves the run to take its time
    const unsigned int watching = ((milliseconds != 0ull) && cli_thread_started(&room->watcher)) ? 1u : 0u;
    const unsigned int ending = ((input != NULL) ? 2u : 1u) + watching;
    const int reading = cli_thread_started(&room->reader);
    if ((input != NULL) && !cli_thread_started(&room->writer))
    {
        cli_input_written(room);
    }
    int whole = 1;
    char *out = s_nothing;
    unsigned long long out_size = 0ull;
    cli_pipe_read(room->spawned.out, &out, &out_size, &whole);
    if (!reading)
    {
        cli_errors_read(room);
    }
    cli_lock_take(&room->lock);
    room->finished = 1;
    while (room->ended < ending)
    {
        cli_wake_wait(&room->wake, &room->lock);
    }
    ran->out = out;
    ran->out_size = out_size;
    ran->whole = ran->whole && whole;
    cli_lock_give(&room->lock);
    ran->code = cli_spawn_wait(&room->spawned);
    cli_spawn_closed(&room->spawned);
    return 1;
}
