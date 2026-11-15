// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_tracked.c: whether git tracks a path or would, as the tree's branches are: a file it lists, a file in a
// folder containing one it lists or at the tree's top, or a file in a folder the tree does not contain that git does
// not ignore, git queried once a listing for each such folder. Every path is, in a tree git cannot read
#include "../code/symbols.h"
#include "../machine/trust.h"
#include "../text/text.h"
#include "../command_line/pool.h"
#include "files_internal.h"

// What git reported of one folder outside the tree it was queried about: where the folder's path starts in the
// cache's text, and whether git ignores it
typedef struct
{
    unsigned long long path_at;
    int ignored;
} TrackedCached;

// What git reported of each folder outside the tree it was queried about, for the listing made at `at`: the answers
// in a cell of their capacity, how many, and their paths in a text cell of its capacity, both grown to the most a
// listing has asked
typedef struct
{
    TrackedCached *cached;
    unsigned long long count;
    unsigned long long capacity;
    char *text;
    unsigned long long used;
    unsigned long long text_capacity;
    unsigned long long at;
} TrackedCache;
static TrackedCache s_cache;

// a cell of `unit`-byte entries holding at least `needed` of them: `cell` itself where its `capacity` entries
// hold them, else a cell of twice the capacity until they fit, the entries copied, `cell` given back and
// `capacity` set to the new count; NULL where the cells refuse the growth, `cell` and `capacity` kept
static void *tracked_cell_fit(void *cell, unsigned long long *capacity, unsigned long long needed,
                              unsigned long long unit)
{
    if (needed <= *capacity)
    {
        return cell;
    }
    unsigned long long grown = (*capacity != 0ULL) ? *capacity : 16ULL;
    while (grown < needed)
    {
        grown *= 2ULL;
    }
    void *const moved = g_cli_guard->persistent_buf_alloc(grown * unit, g_cli_editor_owner);
    if (moved == NULL)
    {
        return NULL;
    }
    if (*capacity != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = moved, .src = cell, .bytes = *capacity * unit});
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_editor_owner);
    *capacity = grown;
    return moved;
}

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
    if (s_cache.at != args->at)
    {
        s_cache.count = 0ULL;
        s_cache.used = 0ULL;
        s_cache.at = args->at;
    }
    int found = 0;
    unsigned long long each = 0ULL;
    while (!found && (each < s_cache.count))
    {
        found = cellul.eq(&(CatenaFinitaCfg){
            .src = s_cache.text + s_cache.cached[each].path_at, .other = args->outside, .cap = CLI_POOL_BYTES});
        *ignored = found ? s_cache.cached[each].ignored : *ignored;
        each += 1ULL;
    }
    return found;
}

// git's answer for `outside` cached for the listing made at `at`, where it is that listing's still, the cells grown
// where it does not fit
EMBED_INLINE void tracked_append(const FilesTrackedCtx *args)
{
    if (s_cache.at != args->at)
    {
        return;
    }
    const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = args->outside, .cap = CLI_POOL_BYTES}) + 1ULL;
    TrackedCached *const cached =
        tracked_cell_fit(s_cache.cached, &s_cache.capacity, s_cache.count + 1ULL, sizeof(TrackedCached));
    char *const text =
        (cached != NULL) ? tracked_cell_fit(s_cache.text, &s_cache.text_capacity, s_cache.used + bytes, 1ULL) : NULL;
    s_cache.cached = (cached != NULL) ? cached : s_cache.cached;
    s_cache.text = (text != NULL) ? text : s_cache.text;
    if ((cached == NULL) || (text == NULL))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = text + s_cache.used, .src = args->outside, .bytes = bytes});
    cached[s_cache.count] = (TrackedCached){s_cache.used, args->ignored_value};
    s_cache.count += 1ULL;
    s_cache.used += bytes;
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
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}), .byte = '/');
    const unsigned long long folder_length = (slash != NULL) ? (unsigned long long)(slash - path) : 0ULL;
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ULL};
    mmgr_line_put_n(&line, path, folder_length);
    mmgr_line_finish(&line);
    if (EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = path) ||
        (folder_length == 0ULL) ||
        EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = folder))
    {
        return 1;
    }
    // the first folder down the path the tree does not contain: an empty folder git lists nothing of, a folder made
    // since it was listed, or one git ignores, as a build's output is
    char outside[CLI_PATH_MAX_LEN];
    unsigned long long end = 0ULL;
    for (unsigned long long at = 0ULL;;)
    {
        end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = path, .at = end,
                         .size = folder_length, .set = "/");
        VerbaLinea part = {outside, sizeof(outside), 0ULL};
        mmgr_line_put_n(&part, path, end);
        mmgr_line_finish(&part);
        if (!EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = outside) ||
            (end >= folder_length))
        {
            break;
        }
        at = end + 1ULL;
    }
    const unsigned long long read_at = cli_tree_listing_at();
    int ignored = 0;
    if (EMBED_CALL(tracked_cache, FilesTrackedCtx, .outside = outside, .at = read_at, .ignored = &ignored))
    {
        return !ignored;
    }
    char spec[CLI_PATH_MAX_LEN];
    VerbaLinea named = {spec, sizeof(spec), 0ULL};
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
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    ignored = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = root, .settings = NULL,
                         .count = 0u, .run_result = run_result, .err_msg = &unstarted) &&
              (run_result->code == 0LL);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    EMBED_CALL(tracked_append, FilesTrackedCtx, .outside = outside, .at = read_at, .ignored_value = ignored);
    return !ignored;
}
