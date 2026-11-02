// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch_read.c: the tree read every two seconds where the system does not report its changes: listed again, each file
// that came or went reported, each file's stamp and git's index and HEAD set against the last reading
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "files_internal.h"
#include "watch_internal.h"

// how often the tree is read, and the slices the wait between readings is taken in, in milliseconds
#define CLI_WATCH_POLL_EVERY 2000u
#define CLI_WATCH_SLICE 50u

// git's index and HEAD, read in every reading, and when each was last written, 0 where it is not there
static const char *const s_git_paths[2] = {".git/index", ".git/HEAD"};
static unsigned long long s_git_stamps[2];

typedef struct
{
    const char *root;
    CliWatchChangeFn emit;
    void *context;
} WatchReadCtx;
// git's index and HEAD read, each that moved handed to `emit` where it is given
EMBED_INLINE void cli_git_stamp(const WatchReadCtx *args)
{
    const char *root = args->root;
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = s_git_paths[at], .joined = full);
        unsigned long long size = 0ull;
        unsigned long long written = 0ull;
        const unsigned long long stamp = EMBED_CALL(cli_path_size_internal.file_stamp, CliPathSizeCtx, .path = full,
                                                    .size_pointer = &size, .changed = &written)
                                             ? written
                                             : 0ull;
        if ((stamp != s_git_stamps[at]) && (args->emit != NULL))
        {
            EMBED_CALL(args->emit, CliEachPathKindCfg, .context = args->context, .path = s_git_paths[at],
                       .kind = CLI_CHANGE_MODIFIED);
        }
        s_git_stamps[at] = stamp;
    }
}

int cli_watch_first_reading(const CliWatchCtx *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    EMBED_CALL(cli_symbols_context.tree_stamp, CliSymbolsContextCfg, .root = root, .emit = NULL, .context = NULL);
    EMBED_CALL(cli_git_stamp, WatchReadCtx, .root = root, .emit = NULL, .context = NULL);
    return 1;
}

void cli_watch_read_again(const CliWatchCtx *args)
{
    const char *root = args->root;
    CliWatchChangeFn emit = args->emit;
    void *context = args->context;
    EMBED_CALL(cli_symbols_context.tree_relist, CliSymbolsContextCfg, .root = root, .emit = emit, .context = context);
    EMBED_CALL(cli_symbols_context.tree_stamp, CliSymbolsContextCfg, .root = root, .emit = emit, .context = context);
    EMBED_CALL(cli_git_stamp, WatchReadCtx, .root = root, .emit = emit, .context = context);
}

void cli_watch_poll(void *context)
{
    const char *const root = (const char *)context;
    for (;;)
    {
        int stopping = 0;
        for (unsigned int waited = 0u; !stopping && (waited < CLI_WATCH_POLL_EVERY); waited += CLI_WATCH_SLICE)
        {
            EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = CLI_WATCH_SLICE);
            stopping = cli_watch_stopping();
        }
        if (stopping)
        {
            break;
        }
        EMBED_CALL(cli_watch_internal.read_again, CliWatchCtx, .root = root, .emit = cli_watch_change, .context = NULL);
    }
    cli_watch_poll_end();
}
