// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_ranked.c: the quick open's files: the tree listed behind the window, and the files that answer a query, the
// best first. A file whose name equals what was typed scores over any match, one whose name starts with it half that,
// and one opened lately over the match itself. The ranking is one pass over the sorted files into a bounded list
// retained in score order, a file's place in it found by a binary search; the answer is one cell sized to it
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "files_internal.h"

void cli_files_read_behind(const CliFilesRootPathsCfg *args)
{
    const char *root = args->root;
    if (!EMBED_CALL(cli_symbols_root.tree_is_list, CliSymbolsRootCfg, .root = root))
    {
        EMBED_CALL(cli_files_root_paths.warm, CliFilesRootPathsCfg, .root = root);
    }
}

int cli_files_unread(const CliFilesRootPathsCfg *args)
{
    const char *root = args->root;
    return !EMBED_CALL(cli_symbols_root.tree_is_list, CliSymbolsRootCfg, .root = root);
}

// A find's lists, one take sized to the find. First the bounded list of `max` places, each with its score. A place
// is a file's place among the files, or among the recent paths while the tree has no listing. Then the query's
// characters, the latest place each character of a match can take, and the places a match took. Each of those has
// one entry for each byte of the query. Last, a path's characters as they are and in lower case, one entry for each
// byte of the longest candidate
typedef struct
{
    CliTopPlace *top;
    unsigned int *required;
    unsigned int *latest;
    unsigned int *hit;
    unsigned int *displayed;
    unsigned int *lower;
    unsigned int query_bytes;
    unsigned int path_bytes;
} FilesRankedLists;

typedef struct
{
    const CliFilesRootRowsCfg *find;
    const FilesRankedLists *lists;
    const char *path;
    unsigned int required;
    double *score_out;
    unsigned int *hits_out;
    unsigned int count;
    unsigned int max;
    unsigned int at;
    double score;
} FilesRankedCtx;

// whether `path` answers the find's `required` characters: 1 with its score into `score_out` and the places of its
// characters that matched into the lists' `hit`, their count into `hits_out`, else 0
EMBED_INLINE int ranked_matched(const FilesRankedCtx *args)
{
    const CliFilesRootRowsCfg *const find = args->find;
    const FilesRankedLists *const lists = args->lists;
    const unsigned int characters =
        EMBED_CALL(cli_files_text.characters, CliFilesTextCfg, .text = args->path, .displayed = lists->displayed,
                   .lower = lists->lower, .capacity = lists->path_bytes);
    unsigned int name = 0u;
    for (unsigned int at = 0u; at < characters; at += 1u)
    {
        name = (lists->lower[at] == '/') ? (at + 1u) : name;
    }
    double score = 0.0;
    unsigned int hits = 0u;
    int matched = 0;
    if (find->fuzzy)
    {
        matched =
            EMBED_CALL(cli_files_bytes_requested_internal.fuzzy_characters, CliFilesBytesRequestedCtx,
                       .required = lists->required, .requested_count = args->required, .displayed = lists->displayed,
                       .lower = lists->lower, .count_u32 = characters, .from_u32 = name, .bytes_i32 = 0,
                       .latest = lists->latest, .score = &score, .hits = lists->hit, .hit_count = &hits);
    }
    else
    {
        // the query's characters in a row anywhere in the path, case aside, the first place they are
        for (unsigned int from = 0u; !matched && ((from + args->required) <= characters); from += 1u)
        {
            unsigned int same = 0u;
            while ((same < args->required) && (lists->lower[from + same] == lists->required[same]))
            {
                same += 1u;
            }
            matched = same == args->required;
            for (unsigned int at = 0u; matched && (at < args->required); at += 1u)
            {
                lists->hit[at] = from + at;
            }
            hits = matched ? args->required : 0u;
            score = matched ? ((double)args->required - ((double)characters * 0.02)) : 0.0;
        }
    }
    if (!matched)
    {
        return 0;
    }
    // the name equal to the query, or the name that starts with it, case aside
    const unsigned int name_characters = characters - name;
    unsigned int prefix = 0u;
    while ((prefix < args->required) && (prefix < name_characters) &&
           (lists->lower[name + prefix] == lists->required[prefix]))
    {
        prefix += 1u;
    }
    const int starts = (args->required != 0u) && (prefix == args->required);
    const int exact = starts && (name_characters == args->required);
    score += exact ? CLI_FILES_RANKED_EXACT : (starts ? (CLI_FILES_RANKED_EXACT / 2.0) : 0.0);
    int lately = 0;
    for (unsigned int at = 0u; !lately && (at < find->recents); at += 1u)
    {
        lately = cellul.eq(&(CatenaFinitaCfg){.src = find->recent[at], .other = args->path, .cap = CLI_PATH_MAX_LEN});
    }
    score += lately ? CLI_FILES_RANKED_LATELY : 0.0;
    *args->score_out = score;
    *args->hits_out = hits;
    return 1;
}

int cli_files_ranked_find(const CliFilesRootRowsCfg *args)
{
    const unsigned int max = (args->max < CLI_FILES_RANKED_MAX) ? args->max : CLI_FILES_RANKED_MAX;
    const int unread =
        (args->root == NULL) || EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = args->root);
    const CliFiles *const files =
        unread ? NULL : EMBED_CALL(cli_files_view_internal.view, CliFilesRootPathsCfg, .root = args->root);
    const unsigned int candidates = (files != NULL) ? files->count : (unread ? args->recents : 0u);
    // the longest candidate's bytes bound a path's characters, and the query's bytes bound its characters
    unsigned long long longest = 0ULL;
    for (unsigned int at = 0u; (max != 0u) && (at < candidates); at += 1u)
    {
        const char *const path =
            (files != NULL) ? EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at) : args->recent[at];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        longest = (length > longest) ? length : longest;
    }
    const unsigned long long query_bytes = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_POOL_BYTES});
    const unsigned long long top_bytes = (unsigned long long)max * sizeof(CliTopPlace);
    const unsigned long long list_bytes =
        top_bytes + (((3ULL * query_bytes) + (2ULL * longest)) * sizeof(unsigned int));
    unsigned char *const list_cell =
        (list_bytes != 0ULL) ? g_cli_guard->temporary_buf_alloc(list_bytes, g_cli_files_views_owner) : NULL;
    if ((list_bytes != 0ULL) && (list_cell == NULL))
    {
        if (files != NULL)
        {
            EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = files);
        }
        return 0;
    }
    // The bounded list lies first in the take, which keeps it aligned. The query's three lists come after it, and
    // the path's two last. A text's bytes are below CLI_POOL_BYTES, which an unsigned int stores
    unsigned int *const query_lists = (unsigned int *)(void *)(list_cell + top_bytes);
    const FilesRankedLists lists = {(CliTopPlace *)(void *)list_cell,   query_lists,
                                    query_lists + query_bytes,          query_lists + (2ULL * query_bytes),
                                    query_lists + (3ULL * query_bytes), query_lists + (3ULL * query_bytes) + longest,
                                    (unsigned int)query_bytes,          (unsigned int)longest};
    const unsigned int required =
        EMBED_CALL(cli_files_query_characters.want, CliFilesQueryLettersCfg, .query = args->query,
                   .characters = lists.required, .capacity = lists.query_bytes);
    unsigned int count = 0u;
    for (unsigned int at = 0u; (max != 0u) && (at < candidates); at += 1u)
    {
        const char *const path =
            (files != NULL) ? EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at) : args->recent[at];
        double score = 0.0;
        unsigned int hits = 0u;
        if (EMBED_CALL(ranked_matched, FilesRankedCtx, .find = args, .lists = &lists, .path = path,
                       .required = required, .score_out = &score, .hits_out = &hits))
        {
            count = EMBED_CALL(cli_files_top_internal.put, CliFilesTopCtx, .top = lists.top, .at = at, .score = score,
                               .count = count, .max = max);
        }
    }
    // the answer's bytes: the rows, every row's places, and the paths with their ends
    unsigned long long places = 0ULL;
    unsigned long long text = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = (files != NULL)
                                     ? EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = lists.top[at].at)
                                     : args->recent[lists.top[at].at];
        double score = 0.0;
        unsigned int hits = 0u;
        (void)EMBED_CALL(ranked_matched, FilesRankedCtx, .find = args, .lists = &lists, .path = path,
                         .required = required, .score_out = &score, .hits_out = &hits);
        places += hits;
        text += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    const unsigned long long row_bytes = (unsigned long long)count * sizeof(CliFileRanked);
    const unsigned long long place_bytes = places * sizeof(unsigned int);
    const CarcerMovableEntry *const cell =
        (count != 0u) ? g_cli_guard->movable_buf_alloc(row_bytes + place_bytes + text, g_cli_files_views_owner) : NULL;
    if ((count != 0u) && (cell == NULL))
    {
        g_cli_guard->persistent_buf_release(list_cell, g_cli_files_views_owner);
        if (files != NULL)
        {
            EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = files);
        }
        return 0;
    }
    // The rows lie first in the cell, the places after them and the paths last. A row keeps its path's place among
    // the paths, which stays true where the mover moves the cell
    const unsigned long long paths_at = row_bytes + place_bytes;
    unsigned int put = 0u;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = (files != NULL)
                                     ? EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = lists.top[at].at)
                                     : args->recent[lists.top[at].at];
        double score = 0.0;
        unsigned int hits = 0u;
        (void)EMBED_CALL(ranked_matched, FilesRankedCtx, .find = args, .lists = &lists, .path = path,
                         .required = required, .score_out = &score, .hits_out = &hits);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = paths_at + used, .src = path, .bytes = length + 1ULL});
        if (hits != 0u)
        {
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = cell,
                                    .offset = row_bytes + ((unsigned long long)put * sizeof(unsigned int)),
                                    .src = lists.hit,
                                    .bytes = hits * sizeof(unsigned int)});
        }
        const CliFileRanked one = {used, lists.top[at].score, put, hits};
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = cell, .offset = (unsigned long long)at * sizeof(one), .src = &one, .bytes = sizeof(one)});
        put += hits;
        used += length + 1ULL;
    }
    // the module lets its work go before the answer goes to the caller
    g_cli_guard->persistent_buf_release(list_cell, g_cli_files_views_owner);
    if (files != NULL)
    {
        EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = files);
    }
    const CliFilesRanked ranked = {cell, count, row_bytes, paths_at, unread && (args->root != NULL)};
    args->each(&(CliFilesRankedEachCfg){.context = args->context, .ranked = &ranked});
    if (cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(cell, g_cli_files_views_owner);
    }
    return 1;
}

int cli_files_ranked_at(const CliFilesRankedAtCfg *args)
{
    const CliFilesRanked *const ranked = args->ranked;
    char *const out = args->path_out;
    out[0] = '\0';
    if ((ranked->cell == NULL) || (args->index >= ranked->count))
    {
        return 0;
    }
    CliFileRanked *const row = args->row_out;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = ranked->cell,
                                                            .offset = (unsigned long long)args->index * sizeof(*row),
                                                            .dst = row,
                                                            .bytes = sizeof(*row)});
    const unsigned long long path_at = ranked->text_at + row->path_at;
    const unsigned long long length =
        g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = ranked->cell, .offset = path_at});
    const unsigned long long bytes = (length < CLI_PATH_MAX_LEN - 1ULL) ? length : CLI_PATH_MAX_LEN - 1ULL;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = ranked->cell, .offset = path_at, .dst = out, .bytes = bytes});
    out[bytes] = '\0';
    row->hit_count = (row->hit_count < args->hits_max) ? row->hit_count : args->hits_max;
    if (row->hit_count != 0u)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = ranked->cell,
                                .offset = ranked->hit_at + ((unsigned long long)row->hit_first * sizeof(unsigned int)),
                                .dst = args->hits_out,
                                .bytes = (unsigned long long)row->hit_count * sizeof(unsigned int)});
    }
    return 1;
}
