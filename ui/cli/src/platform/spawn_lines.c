// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// spawn_lines.c: a started process's pipe read until it ends and cut into lines, each passed on as it is whole, on
// every platform through the platform's own read
#include "../command_line/cli_internal.h"

// the bytes a read of the pipe takes at a time
#define SPAWN_LINES_PIECE 4096ULL

void cli_spawn_lines_read(const CliSpawnLinesCtx *args)
{
    char piece[SPAWN_LINES_PIECE];
    unsigned long long used = 0ULL;
    for (;;)
    {
        const unsigned long long count =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = args->pipe_handle, .piece = piece, .capacity = sizeof(piece));
        if (count == 0ULL)
        {
            break;
        }
        for (unsigned long long at = 0ULL; at < count; at += 1ULL)
        {
            if ((piece[at] == '\n') || (used == args->capacity))
            {
                args->each(args->context, args->line, used);
                used = 0ULL;
            }
            if (piece[at] != '\n')
            {
                args->line[used] = piece[at];
                used += 1ULL;
            }
        }
    }
    if (used > 0ULL)
    {
        args->each(args->context, args->line, used);
    }
}
