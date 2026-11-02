// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch_read.c: the tree read every two seconds where the system does not report its changes: listed again, each file
// that came or went reported, each file's stamp and git's index and HEAD set against the last reading
#include "../code/symbols.h"
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

// git's index and HEAD read, each that moved handed to `emit` where it is given
static void cli_git_stamped(const char *root, CliWatchChangeFn emit, void *context)
{
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char full[CLI_PATH_MAX_LEN];
        cli_full(root, s_git_paths[at], full);
        unsigned long long size = 0ull;
        unsigned long long written = 0ull;
        const unsigned long long stamp = cli_file_stamp(full, &size, &written) ? written : 0ull;
        if ((stamp != s_git_stamps[at]) && (emit != NULL))
        {
            emit(context, s_git_paths[at], CLI_CHANGE_MODIFIED);
        }
        s_git_stamps[at] = stamp;
    }
}

int cli_watch_first_reading(const char *root)
{
    cli_tree_listed(root);
    cli_tree_stamped(root, NULL, NULL);
    cli_git_stamped(root, NULL, NULL);
    return 1;
}

void cli_watch_read_again(const char *root, CliWatchChangeFn emit, void *context)
{
    cli_tree_relisted(root, emit, context);
    cli_tree_stamped(root, emit, context);
    cli_git_stamped(root, emit, context);
}

void cli_watch_polled(void *context)
{
    const char *const root = (const char *)context;
    for (;;)
    {
        int stopping = 0;
        for (unsigned int waited = 0u; !stopping && (waited < CLI_WATCH_POLL_EVERY); waited += CLI_WATCH_SLICE)
        {
            query_paused(CLI_WATCH_SLICE);
            stopping = cli_watch_stopping();
        }
        if (stopping)
        {
            break;
        }
        cli_watch_read_again(root, cli_watch_changed, NULL);
    }
    cli_watch_poll_ended();
}
