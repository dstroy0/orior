// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_tracked.c: whether git tracks a path or would, as the view of the tree stores it: a file it lists, a file in a
// folder containing one it lists or at the tree's top, or a file in a folder the view does not contain that git does
// not ignore, git queried once a view for each such folder. Every path is, in a tree git cannot read
#include "../machine/trust.h"
#include "../text/text.h"
#include "files_internal.h"

// the most folders outside the view whose answers are cached for it
#define CLI_TRACKED_CACHE 64u

// What git reported of each folder outside its view it was queried about, for the view read at `at`, under their own
// lock
static char s_cached_paths[CLI_TRACKED_CACHE][CLI_PATH_MAX_LEN];
static int s_ignored[CLI_TRACKED_CACHE];
static unsigned int s_cached_count;
static unsigned long long s_cached_at;
static CliLock s_cache_lock = CLI_LOCK_INIT;

// the answer cached for `outside` of the view read at `at`: 1 with whether git ignores it, or 0 where none is cached;
// the answers cached for a view read before let go
static int tracked_cached(const char *outside, unsigned long long at, int *ignored)
{
    cli_lock_take(&s_cache_lock);
    if (s_cached_at != at)
    {
        s_cached_count = 0u;
        s_cached_at = at;
    }
    int found = 0;
    for (unsigned int each = 0u; !found && (each < s_cached_count); each += 1u)
    {
        found = cellul.eq(&(CatenaFinitaCfg){.src = s_cached_paths[each], .other = outside, .cap = CLI_CELLS_POOL});
        *ignored = found ? s_ignored[each] : *ignored;
    }
    cli_lock_give(&s_cache_lock);
    return found;
}

// git's answer for `outside` cached for the view read at `at`, where it is that view's still and room is left
static void tracked_put(const char *outside, unsigned long long at, int ignored)
{
    cli_lock_take(&s_cache_lock);
    if ((s_cached_at == at) && (s_cached_count < CLI_TRACKED_CACHE))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_cached_paths[s_cached_count],
                                .src = outside,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = outside, .cap = CLI_CELLS_POOL}) + 1u});
        s_ignored[s_cached_count] = ignored;
        s_cached_count += 1u;
    }
    cli_lock_give(&s_cache_lock);
}

int cli_files_tracked(const char *root, const char *path)
{
    const CliSeen *const seen = cli_seen_acquire(root);
    if ((seen == NULL) || !seen->git)
    {
        cli_seen_let(seen);
        return 1;
    }
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    const unsigned long long folder_length = (slash != NULL) ? (unsigned long long)(slash - path) : 0ull;
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ull};
    mmgr_line_put_n(&line, path, folder_length);
    mmgr_line_finish(&line);
    if (cli_files_contains(&seen->files, path) || (folder_length == 0ull) || cli_files_contains(&seen->dirs, folder))
    {
        cli_seen_let(seen);
        return 1;
    }
    // the first folder down the path the view does not contain: an empty folder git lists nothing of, a folder made
    // since the view was read, or one git ignores, as a build's output is
    char outside[CLI_PATH_MAX_LEN];
    unsigned long long end = 0ull;
    for (unsigned long long at = 0ull;;)
    {
        end = at;
        while ((end < folder_length) && (path[end] != '/'))
        {
            end += 1ull;
        }
        VerbaLinea part = {outside, sizeof(outside), 0ull};
        mmgr_line_put_n(&part, path, end);
        mmgr_line_finish(&part);
        if (!cli_files_contains(&seen->dirs, outside) || (end >= folder_length))
        {
            break;
        }
        at = end + 1ull;
    }
    const unsigned long long read_at = seen->at;
    cli_seen_let(seen);
    int ignored = 0;
    if (tracked_cached(outside, read_at, &ignored))
    {
        return !ignored;
    }
    char spec[CLI_PATH_MAX_LEN];
    VerbaLinea named = {spec, sizeof(spec), 0ull};
    mmgr_line_put(&named, outside);
    mmgr_line_ch(&named, '/');
    mmgr_line_finish(&named);
    const char *words[8] = {"git"};
    unsigned int count = 1u + cli_trust_git_words(root, words + 1);
    words[count] = "check-ignore";
    words[count + 1u] = "-q";
    words[count + 2u] = "--";
    words[count + 3u] = spec;
    words[count + 4u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    ignored = cli_ran(words, root, NULL, 0u, ran, &unstarted) && (ran->code == 0ll);
    cli_ran_given(ran);
    tracked_put(outside, read_at, ignored);
    return !ignored;
}
