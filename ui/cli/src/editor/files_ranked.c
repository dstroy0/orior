// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_ranked.c: the quick open's files: the tree listed behind the window, and the files that answer a query, the
// best first. A file whose name equals what was typed scores over any match, one whose name starts with it half that,
// and one opened lately over the match itself. The ranking is one pass over the sorted files into a bounded list kept
// in score order, a file's place in it found by a binary search; the answer is one cell sized to it
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

// A file in the bounded list: its place among the files, or among the recent paths while the tree is not listed, and
// its score
typedef struct
{
    unsigned int at;
    double score;
} RankedTop;

// the query's letters, a path's letters as they are and in lower case, the latest place each letter of a match may
// take, the places a match took, and the bounded list, for one find at a time on the view's thread
static unsigned int s_required[CLI_FUZZY_LETTERS];
static unsigned int s_shown[CLI_FUZZY_LETTERS];
static unsigned int s_lower[CLI_FUZZY_LETTERS];
static unsigned int s_latest[CLI_FUZZY_LETTERS];
static unsigned int s_hit[CLI_FUZZY_LETTERS];
static RankedTop s_top[CLI_FILES_RANKED_MOST];

typedef struct
{
    const CliFilesRootRowsCfg *find;
    const char *path;
    unsigned int required;
    double *score_out;
    unsigned int *hits_out;
    unsigned int count;
    unsigned int most;
    unsigned int at;
    double score;
} FilesRankedCtx;

// whether `path` answers the find's `required` letters: 1 with its score into `score_out` and the places of its
// letters that matched into s_hit, their count into `hits_out`, else 0
EMBED_INLINE int ranked_matched(const FilesRankedCtx *args)
{
    const CliFilesRootRowsCfg *const find = args->find;
    const unsigned int letters = EMBED_CALL(cli_files_text.letters, CliFilesTextCfg, .text = args->path,
                                            .displayed = s_shown, .lower = s_lower, .capacity = CLI_FUZZY_LETTERS);
    unsigned int name = 0u;
    for (unsigned int at = 0u; at < letters; at += 1u)
    {
        name = (s_lower[at] == '/') ? (at + 1u) : name;
    }
    double score = 0.0;
    unsigned int hits = 0u;
    int matched = 0;
    if (find->fuzzy)
    {
        matched = EMBED_CALL(cli_files_bytes_requested_internal.fuzzy_letters, CliFilesBytesRequestedCtx,
                             .required = s_required, .requested_count = args->required, .displayed = s_shown,
                             .lower = s_lower, .count_u32 = letters, .from_u32 = name, .bytes_i32 = 0,
                             .latest = s_latest, .score = &score, .hits = s_hit, .hit_count = &hits);
    }
    else
    {
        // the query's letters in a row anywhere in the path, case aside, the first place they stand
        for (unsigned int from = 0u; !matched && ((from + args->required) <= letters); from += 1u)
        {
            unsigned int same = 0u;
            while ((same < args->required) && (s_lower[from + same] == s_required[same]))
            {
                same += 1u;
            }
            matched = same == args->required;
            for (unsigned int at = 0u; matched && (at < args->required); at += 1u)
            {
                s_hit[at] = from + at;
            }
            hits = matched ? args->required : 0u;
            score = matched ? ((double)args->required - ((double)letters * 0.02)) : 0.0;
        }
    }
    if (!matched)
    {
        return 0;
    }
    // the name equal to the query, or starting with it, case aside
    const unsigned int name_letters = letters - name;
    unsigned int prefix = 0u;
    while ((prefix < args->required) && (prefix < name_letters) && (s_lower[name + prefix] == s_required[prefix]))
    {
        prefix += 1u;
    }
    const int starts = (args->required != 0u) && (prefix == args->required);
    const int exact = starts && (name_letters == args->required);
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

// the file `at` with `score` put in the bounded list of `count` files, at most `most`, after every file scoring as much
// or more, found by a binary search, the last let go where the list is full: the list's count
EMBED_INLINE unsigned int ranked_put(const FilesRankedCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (s_top[middle].score >= args->score)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if (low >= args->most)
    {
        return args->count;
    }
    const unsigned int count = (args->count < args->most) ? (args->count + 1u) : args->most;
    for (unsigned int at = count - 1u; at > low; at -= 1u)
    {
        s_top[at] = s_top[at - 1u];
    }
    s_top[low] = (RankedTop){args->at, args->score};
    return count;
}

int cli_files_ranked_find(const CliFilesRootRowsCfg *args)
{
    CliFilesRanked *const rows = args->rows;
    g_cli_guard->persistent_buf_release(rows->cell, args->owner);
    *rows = (CliFilesRanked){NULL, 0u, NULL, NULL, 0};
    const unsigned int most = (args->most < CLI_FILES_RANKED_MOST) ? args->most : CLI_FILES_RANKED_MOST;
    const unsigned int required =
        EMBED_CALL(cli_files_query_letters.want, CliFilesQueryLettersCfg, .query = args->query, .letters = s_required,
                   .capacity = CLI_FUZZY_LETTERS);
    const int unread =
        (args->root == NULL) || EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = args->root);
    const CliFiles *const files =
        unread ? NULL : EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = args->root);
    const unsigned int candidates = (files != NULL) ? files->count : (unread ? args->recents : 0u);
    unsigned int count = 0u;
    for (unsigned int at = 0u; (most != 0u) && (at < candidates); at += 1u)
    {
        const char *const path =
            (files != NULL) ? EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at) : args->recent[at];
        double score = 0.0;
        unsigned int hits = 0u;
        if (EMBED_CALL(ranked_matched, FilesRankedCtx, .find = args, .path = path, .required = required,
                       .score_out = &score, .hits_out = &hits))
        {
            count = EMBED_CALL(ranked_put, FilesRankedCtx, .at = at, .score = score, .count = count, .most = most);
        }
    }
    // the answer's bytes: the rows, every row's places, and the paths with their ends
    unsigned long long places = 0ULL;
    unsigned long long text = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = (files != NULL)
                                     ? EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = s_top[at].at)
                                     : args->recent[s_top[at].at];
        double score = 0.0;
        unsigned int hits = 0u;
        (void)EMBED_CALL(ranked_matched, FilesRankedCtx, .find = args, .path = path, .required = required,
                         .score_out = &score, .hits_out = &hits);
        places += hits;
        text += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    const unsigned long long row_bytes = (unsigned long long)count * sizeof(CliFileRanked);
    const unsigned long long place_bytes = places * sizeof(unsigned int);
    unsigned char *const cell =
        (count != 0u) ? g_cli_guard->persistent_buf_alloc(row_bytes + place_bytes + text, args->owner) : NULL;
    if ((count != 0u) && (cell == NULL))
    {
        if (files != NULL)
        {
            EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
        }
        return 0;
    }
    // the rows lie first in the cell, which its alignment saves aligned, the places after them and the paths last
    CliFileRanked *const row = (CliFileRanked *)(void *)cell;
    unsigned int *const hit = (unsigned int *)(void *)(cell + row_bytes);
    char *const paths = (char *)(cell + row_bytes + place_bytes);
    unsigned int put = 0u;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = (files != NULL)
                                     ? EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = s_top[at].at)
                                     : args->recent[s_top[at].at];
        double score = 0.0;
        unsigned int hits = 0u;
        (void)EMBED_CALL(ranked_matched, FilesRankedCtx, .find = args, .path = path, .required = required,
                         .score_out = &score, .hits_out = &hits);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = paths + used, .src = path, .bytes = length + 1ULL});
        if (hits != 0u)
        {
            memor.cpy(&(MemoriaCfg){.dst = hit + put, .src = s_hit, .bytes = hits * sizeof(unsigned int)});
        }
        row[at] = (CliFileRanked){paths + used, s_top[at].score, put, hits};
        put += hits;
        used += length + 1ULL;
    }
    if (files != NULL)
    {
        EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    }
    *rows = (CliFilesRanked){cell, count, row, hit, unread && (args->root != NULL)};
    return 1;
}
