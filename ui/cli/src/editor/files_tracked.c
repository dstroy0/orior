// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_tracked.c: whether git tracks a path or would, as the view of the tree holds it: a file it lists, a file in a
// folder holding one it lists or at the tree's top, or a file in a folder the view does not hold that git does not
// ignore, git asked once a view for each such folder. Every path is, in a tree git cannot read
#include "../machine/trust.h"
#include "../text/text.h"
#include "files_internal.h"

// the most folders outside the view whose answers are kept for it
#define CLI_TRACKED_ASKS 64u

// What git said of each folder outside its view it was asked about, for the view read at `at`, under their own lock
static char s_asked[CLI_TRACKED_ASKS][CLI_PATH_LONGEST];
static int s_ignored[CLI_TRACKED_ASKS];
static unsigned int s_asks;
static unsigned long long s_asked_at;
static CliLock s_asks_lock = CLI_LOCK_INIT;

// the answer kept for `outside` of the view read at `at`: 1 with whether git ignores it, or 0 where none is kept; the
// answers kept for a view read before let go
static int tracked_kept(const char *outside, unsigned long long at, int *ignored)
{
    cli_lock_take(&s_asks_lock);
    if (s_asked_at != at)
    {
        s_asks = 0u;
        s_asked_at = at;
    }
    int kept = 0;
    for (unsigned int each = 0u; !kept && (each < s_asks); each += 1u)
    {
        kept = cellul.eq(&(CatenaFinitaCfg){.src = s_asked[each], .other = outside, .cap = CLI_CELLS_POOL});
        *ignored = kept ? s_ignored[each] : *ignored;
    }
    cli_lock_give(&s_asks_lock);
    return kept;
}

// git's answer for `outside` kept for the view read at `at`, where it is that view's still and room is left
static void tracked_put(const char *outside, unsigned long long at, int ignored)
{
    cli_lock_take(&s_asks_lock);
    if ((s_asked_at == at) && (s_asks < CLI_TRACKED_ASKS))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_asked[s_asks],
                                .src = outside,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = outside, .cap = CLI_CELLS_POOL}) + 1u});
        s_ignored[s_asks] = ignored;
        s_asks += 1u;
    }
    cli_lock_give(&s_asks_lock);
}

int cli_files_tracked(const char *root, const char *path)
{
    const CliSeen *const seen = cli_seen_held(root);
    if ((seen == NULL) || !seen->git)
    {
        cli_seen_let(seen);
        return 1;
    }
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    const unsigned long long folder_length = (slash != NULL) ? (unsigned long long)(slash - path) : 0ull;
    char folder[CLI_PATH_LONGEST];
    CliLine line = {folder, sizeof(folder), 0ull};
    cli_line_counted(&line, path, folder_length);
    cli_line_closed(&line);
    if (cli_files_holds(&seen->files, path) || (folder_length == 0ull) || cli_files_holds(&seen->dirs, folder))
    {
        cli_seen_let(seen);
        return 1;
    }
    // the first folder down the path the view does not hold: an empty folder git lists nothing of, a folder made since
    // the view was read, or one git ignores, as a build's output is
    char outside[CLI_PATH_LONGEST];
    unsigned long long end = 0ull;
    for (unsigned long long at = 0ull;;)
    {
        end = at;
        while ((end < folder_length) && (path[end] != '/'))
        {
            end += 1ull;
        }
        CliLine part = {outside, sizeof(outside), 0ull};
        cli_line_counted(&part, path, end);
        cli_line_closed(&part);
        if (!cli_files_holds(&seen->dirs, outside) || (end >= folder_length))
        {
            break;
        }
        at = end + 1ull;
    }
    const unsigned long long read_at = seen->at;
    cli_seen_let(seen);
    int ignored = 0;
    if (tracked_kept(outside, read_at, &ignored))
    {
        return !ignored;
    }
    char spec[CLI_PATH_LONGEST];
    CliLine named = {spec, sizeof(spec), 0ull};
    cli_line_text(&named, outside);
    cli_line_character(&named, '/');
    cli_line_closed(&named);
    const char *words[8] = {"git"};
    unsigned int count = 1u + cli_trust_git_words(root, words + 1);
    words[count] = "check-ignore";
    words[count + 1u] = "-q";
    words[count + 2u] = "--";
    words[count + 3u] = spec;
    words[count + 4u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    ignored = cli_ran(words, root, NULL, 0u, ran, &unstarted) && (ran->code == 0ll);
    cli_ran_given(ran);
    tracked_put(outside, read_at, ignored);
    return !ignored;
}
