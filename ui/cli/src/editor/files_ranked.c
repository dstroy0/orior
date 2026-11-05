// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_ranked.c: the quick open: the tree's files, less what the search's patterns hide, and those that answer a
// query, the best first. A file named exactly as typed scores over any match, one whose name starts with what was typed
// half that, and one opened lately over the match itself
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "files_internal.h"

// what a file named as typed scores over the match itself, and what a file opened lately scores over it
#define CLI_RANKED_EXACT 100000.0
#define CLI_RANKED_LATELY 6.0

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

// A file that answers: its place among the files ranked and its score
typedef struct
{
    unsigned int index;
    double score;
} CliFound;

// The query as the files are matched against it: its letters in lower case and where its last part after a slash
// starts, which a file's name is matched against; those less whitespace, as a match looks for them; the same less
// ASCII's whitespace, read where the query in lower case is all ASCII; and the letters of the file being matched, as
// they are and in lower case, and the places that matched
typedef struct
{
    unsigned int lowered[CLI_FUZZY_LETTERS];
    unsigned int lowered_count;
    unsigned int query_tail;
    unsigned int required[CLI_FUZZY_LETTERS];
    unsigned int requested_count;
    unsigned int requested_bytes[CLI_FUZZY_LETTERS];
    unsigned int requested_bytes_count;
    int ascii;
    unsigned int displayed[CLI_FUZZY_LETTERS];
    unsigned int lower[CLI_FUZZY_LETTERS];
    unsigned int count;
    unsigned int from;
    unsigned int hits[CLI_FUZZY_LETTERS];
    unsigned int latest[CLI_FUZZY_LETTERS];
    unsigned int hit_count;
    const char *path[CLI_FILES_MAX];
    CliFound found[CLI_FILES_MAX];
    CliFound reserve[CLI_FILES_MAX];
} CliRanking;

typedef struct
{
    VerbaLinea *out;
    const char *query;
    double *score;
    const char *path;
    int fuzzy;
    unsigned int at;
    unsigned int count;
    unsigned int paths;
    const char *const *recent;
    unsigned int recents;
    unsigned int most;
    int read;
} FilesRankedCtx;
