// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_tracked.c: whether git tracks a path or would, as the tree's branches stand: a file it lists, a file in a
// folder containing one it lists or at the tree's top, or a file in a folder the tree does not contain that git does
// not ignore, git queried once a listing for each such folder. Every path is, in a tree git cannot read
#include "../code/symbols.h"
#include "../machine/trust.h"
#include "../text/text.h"
#include "files_internal.h"

// the most folders outside the tree whose answers are cached for a listing
#define CLI_TRACKED_CACHE 64u

// What git reported of each folder outside the tree it was queried about, for the listing made at `at`, under their own
// lock
static char s_cached_paths[CLI_TRACKED_CACHE][CLI_PATH_MAX_LEN];
static int s_ignored[CLI_TRACKED_CACHE];
static unsigned int s_cached_count;
static unsigned long long s_cached_at;
static CliLock s_cache_lock = CLI_LOCK_INIT;

typedef struct
{
    const char *outside;
    unsigned long long at;
    int *ignored;
    int ignored_value;
} FilesTrackedCtx;
// the answer cached for `outside` of the listing made at `at`: 1 with whether git ignores it, or 0 where none is
// cached; the answers cached for a listing before let go
EMBED_INLINE int tracked_cache(const FilesTrackedCtx *args)
{
    int *ignored = args->ignored;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_cache_lock);
    if (s_cached_at != args->at)
    {
        s_cached_count = 0u;
        s_cached_at = args->at;
    }
    int found = 0;
    for (unsigned int each = 0u; !found && (each < s_cached_count); each += 1u)
    {
        found =
            cellul.eq(&(CatenaFinitaCfg){.src = s_cached_paths[each], .other = args->outside, .cap = CLI_CELLS_POOL});
        *ignored = found ? s_ignored[each] : *ignored;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_cache_lock);
    return found;
}

// git's answer for `outside` cached for the listing made at `at`, where it is that listing's still and room is left
EMBED_INLINE void tracked_put(const FilesTrackedCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_cache_lock);
    if ((s_cached_at == args->at) && (s_cached_count < CLI_TRACKED_CACHE))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_cached_paths[s_cached_count],
                          .src = args->outside,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->outside, .cap = CLI_CELLS_POOL}) + 1u});
        s_ignored[s_cached_count] = args->ignored_value;
        s_cached_count += 1u;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_cache_lock);
}

int cli_files_track(const CliFilesRootPathCfg *args)
{
    const char *root = args->root;
    const char *path = args->path;
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    if (!cli_tree_git())
    {
        return 1;
    }
    const char *const slash =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '/');
    const unsigned long long folder_length = (slash != NULL) ? (unsigned long long)(slash - path) : 0ull;
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ull};
    mmgr_line_put_n(&line, path, folder_length);
    mmgr_line_finish(&line);
    if (EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = path) ||
        (folder_length == 0ull) ||
        EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = folder))
    {
        return 1;
    }
    // the first folder down the path the tree does not contain: an empty folder git lists nothing of, a folder made
    // since it was listed, or one git ignores, as a build's output is
    char outside[CLI_PATH_MAX_LEN];
    unsigned long long end = 0ull;
    for (unsigned long long at = 0ull;;)
    {
        end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = path, .at = end,
                         .size = folder_length, .set = "/");
        VerbaLinea part = {outside, sizeof(outside), 0ull};
        mmgr_line_put_n(&part, path, end);
        mmgr_line_finish(&part);
        if (!EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = outside) ||
            (end >= folder_length))
        {
            break;
        }
        at = end + 1ull;
    }
    const unsigned long long read_at = cli_tree_listed_at();
    int ignored = 0;
    if (EMBED_CALL(tracked_cache, FilesTrackedCtx, .outside = outside, .at = read_at, .ignored = &ignored))
    {
        return !ignored;
    }
    char spec[CLI_PATH_MAX_LEN];
    VerbaLinea named = {spec, sizeof(spec), 0ull};
    mmgr_line_put(&named, outside);
    mmgr_line_ch(&named, '/');
    mmgr_line_finish(&named);
    const char *words[8] = {"git"};
    unsigned int count =
        1u + EMBED_CALL(cli_trust_tree_words.git_words, CliTrustTreeWordsCfg, .tree = root, .words = words + 1);
    words[count] = "check-ignore";
    words[count + 1u] = "-q";
    words[count + 2u] = "--";
    words[count + 3u] = spec;
    words[count + 4u] = NULL;
    CliRan *const ran = cli_ran_take();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    ignored = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = root, .settings = NULL,
                         .count = 0u, .ran = ran, .err_msg = &unstarted) &&
              (ran->code == 0ll);
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    EMBED_CALL(tracked_put, FilesTrackedCtx, .outside = outside, .at = read_at, .ignored_value = ignored);
    return !ignored;
}
