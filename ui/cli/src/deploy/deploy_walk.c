// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_walk.c: the folder a deployment sends walked: each file it sends with its mode and the hash of its bytes,
// and each file and folder its patterns leave out with the pattern that leaves it out, each list sorted by path
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/digest.h"
#include "../text/text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"

#include <stdlib.h>

// A folder still to read, stored above the ones before it
typedef struct CliDeployFolderStruct
{
    struct CliDeployFolderStruct *below;
    const char *path;
} CliDeployFolder;

// A file sent, stored before the ones found already
typedef struct CliDeploySentNodeStruct
{
    struct CliDeploySentNodeStruct *before;
    CliDeploySent sent;
} CliDeploySentNode;

// the folder sent and its path in the tree, its `.` parts taken out; the walk's pool; the folders under it still to
// read, by their paths under it, and the files sent and left out found, each stored in that pool; and the patterns that
// end in `/`, which leave out folders
static char s_top[CLI_PATH_MAX_LEN];
static char s_from[CLI_PATH_MAX_LEN];
static CliDeployPool s_pool;
static CliDeployFolder *s_folders;
static CliDeploySentNode *s_sents;
static CliDeployPairNode *s_outs;
static const char **s_folder_patterns;
static unsigned int s_folder_pattern_count;
static int s_overflowed;

typedef struct
{
    CliDeployPool *pool;
    const char *folder;
    const char *name;
    const char *after;
    const char *from;
} DeployWalkCtx;
// a path under the folder sent, `name` in the folder `folder`, stored in `pool`
EMBED_INLINE const char *walk_path(const DeployWalkCtx *args)
{
    CliDeployPool *pool = args->pool;
    char path[CLI_PATH_MAX_LEN + 2u];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_put(&line, (args->folder[0] != '\0') ? "/" : "");
    mmgr_line_put(&line, args->name);
    mmgr_line_put(&line, args->after);
    s_overflowed = s_overflowed || !mmgr_line_ok(&line);
    return EMBED_CALL(cli_deploy_pool_internal.text_store, CliDeployPoolCtx, .pool = pool, .text = path,
                      .size = line.at);
}

// one entry of a folder read: a folder to read in turn or left out, a file sent or left out
static void walk_entry(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliDeployRun *const run = &g_cli_deploy;
    const char *const folder = (const char *)args->context;
    const char *const path =
        EMBED_CALL(walk_path, DeployWalkCtx, .pool = &s_pool, .folder = folder, .name = name, .after = "");
    char probe[CLI_PATH_MAX_LEN + 4u];
    VerbaLinea probe_line = {probe, sizeof(probe), 0ULL};
    mmgr_line_put(&probe_line, path);
    mmgr_line_put(&probe_line, "/x");
    mmgr_line_finish(&probe_line);
    const char *const rule =
        args->is_folder ? EMBED_CALL(cli_deploy_patterns_internal.left_out_by, CliDeployPatternsCtx,
                                     .patterns = s_folder_patterns, .count = s_folder_pattern_count, .path = probe)
                        : EMBED_CALL(cli_deploy_patterns_internal.left_out_by, CliDeployPatternsCtx,
                                     .patterns = run->pattern, .count = run->patterns, .path = path);
    if (rule != NULL)
    {
        CliDeployPairNode *const out =
            EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool, .size = sizeof(*out));
        if (out != NULL)
        {
            out->pair = (CliDeployPair){EMBED_CALL(walk_path, DeployWalkCtx, .pool = &run->pool, .folder = folder,
                                                   .name = name, .after = args->is_folder ? "/" : ""),
                                        rule};
            out->before = s_outs;
            s_outs = out;
            run->outs += 1u;
        }
        return;
    }
    if (args->is_folder)
    {
        CliDeployFolder *const below =
            EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool, .size = sizeof(*below));
        if (below != NULL)
        {
            below->path = path;
            below->below = s_folders;
            s_folders = below;
        }
        return;
    }
    CliDeploySentNode *const sent =
        EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool, .size = sizeof(*sent));
    if (sent != NULL)
    {
        char in_tree[(2u * CLI_PATH_MAX_LEN) + 2u];
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea tree_line = {in_tree, sizeof(in_tree), 0ULL};
        VerbaLinea full_line = {full, sizeof(full), 0ULL};
        mmgr_line_put(&tree_line, s_from);
        mmgr_line_put(&tree_line, (s_from[0] != '\0') ? "/" : "");
        mmgr_line_put(&tree_line, path);
        mmgr_line_finish(&tree_line);
        mmgr_line_put(&full_line, s_top);
        mmgr_line_ch(&full_line, '/');
        mmgr_line_put(&full_line, path);
        mmgr_line_finish(&full_line);
        sent->sent.path =
            EMBED_CALL(cli_deploy_pool_internal.text_store, CliDeployPoolCtx, .pool = &run->pool, .text = path,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        sent->sent.mode =
            EMBED_CALL(cli_deploy_in_tree_internal.mode, CliDeployInTreeCtx, .in_tree = in_tree, .full = full);
        sent->sent.hash[0] = '\0';
        sent->before = s_sents;
        s_sents = sent;
        run->sents += 1u;
    }
}

void cli_deploy_full(const CliDeployOutIndexCtx *args)
{
    char top[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = g_cli_deploy.root, .relative = g_cli_deploy.deployment.from,
               .joined = top);
    mmgr_line_put(args->out, top);
    mmgr_line_ch(args->out, '/');
    mmgr_line_put(args->out, g_cli_deploy.sent[args->index].path);
}

// the path of the folder sent in the tree, its `.` and empty parts taken out, into s_from
EMBED_INLINE void walk_from(const DeployWalkCtx *args)
{
    VerbaLinea line = {s_from, sizeof(s_from), 0ULL};
    for (const char *part = args->from; *part != '\0';)
    {
        size_t length = 0u;
        length = (size_t)EMBED_CALL(
            cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = part, .at = length,
            .size = length + cellul.len(&(CatenaFinitaCfg){.src = part + length, .cap = CLI_CELLS_POOL}), .set = "/\\");
        if ((length > 0u) && !((length == 1u) && (part[0] == '.')))
        {
            mmgr_line_put(&line, (line.at > 0u) ? "/" : "");
            mmgr_line_put_n(&line, part, length);
        }
        part += length;
        part += (*part != '\0') ? 1 : 0;
    }
    mmgr_line_finish(&line);
}

// two files sent in the order of their paths
static int walk_sent_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = ((const CliDeploySent *)one)->path,
                      .right = ((const CliDeploySent *)other)->path);
}

// two files left out in the order of their paths, then of their patterns
static int walk_out_order(const void *one, const void *other)
{
    const CliDeployPair *const left = (const CliDeployPair *)one;
    const CliDeployPair *const right = (const CliDeployPair *)other;
    const int paths =
        EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = left->path, .right = right->path);
    return (paths != 0)
               ? paths
               : EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = left->text, .right = right->text);
}

int cli_deploy_gather(const CliDeployTextCtx *args)
{
    CliDeployRun *const run = &g_cli_deploy;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = run->root, .relative = run->deployment.from,
               .joined = s_top);
    if (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = s_top))
    {
        mmgr_line_put(args->err_msg, run->deployment.name);
        mmgr_line_put(args->err_msg, " names ");
        mmgr_line_put(args->err_msg, run->deployment.from);
        mmgr_line_put(args->err_msg, ", which is no folder of the tree");
        return 0;
    }
    EMBED_CALL(walk_from, DeployWalkCtx, .from = run->deployment.from);
    while (s_pool.page != NULL)
    {
        CliDeployPage *const before = s_pool.page->before;
        g_cli_cells->persistent_buf_release(s_pool.page, &g_cli_deploy_cells);
        s_pool.page = before;
    }
    s_pool.refused = 0;
    s_folder_patterns = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool,
                                   .size = (unsigned long long)run->patterns * sizeof(s_folder_patterns[0]));
    s_folder_pattern_count = 0u;
    for (unsigned int at = 0u; (s_folder_patterns != NULL) && (at < run->patterns); at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = run->pattern[at], .cap = CLI_CELLS_POOL});
        if ((length > 0u) && (run->pattern[at][length - 1u] == '/'))
        {
            s_folder_patterns[s_folder_pattern_count] = run->pattern[at];
            s_folder_pattern_count += 1u;
        }
    }
    EMBED_CALL(cli_deploy_pool_internal.modes_read, CliDeployPoolCtx, .pool = &s_pool);
    run->sent = NULL;
    run->sents = 0u;
    run->out = NULL;
    run->outs = 0u;
    s_sents = NULL;
    s_outs = NULL;
    s_overflowed = 0;
    s_folders =
        EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool, .size = sizeof(*s_folders));
    if (s_folders != NULL)
    {
        s_folders->path = "";
        s_folders->below = NULL;
    }
    while (s_folders != NULL)
    {
        const char *const folder = s_folders->path;
        s_folders = s_folders->below;
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea line = {full, sizeof(full), 0ULL};
        mmgr_line_put(&line, s_top);
        mmgr_line_put(&line, (folder[0] != '\0') ? "/" : "");
        mmgr_line_put(&line, folder);
        if (mmgr_line_finish(&line) != 0ULL)
        {
            (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = full,
                             .each = walk_entry, .context_bytess = (void *)folder);
        }
    }
    run->sent = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                           .size = (unsigned long long)run->sents * sizeof(run->sent[0]));
    run->out = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                          .size = (unsigned long long)run->outs * sizeof(run->out[0]));
    for (unsigned int at = run->sents; (s_sents != NULL) && (run->sent != NULL); s_sents = s_sents->before)
    {
        at -= 1u;
        run->sent[at] = s_sents->sent;
    }
    for (unsigned int at = run->outs; (s_outs != NULL) && (run->out != NULL); s_outs = s_outs->before)
    {
        at -= 1u;
        run->out[at] = s_outs->pair;
    }
    const int overflowed = s_overflowed || s_pool.refused || run->pool.refused;
    while (s_pool.page != NULL)
    {
        CliDeployPage *const before = s_pool.page->before;
        g_cli_cells->persistent_buf_release(s_pool.page, &g_cli_deploy_cells);
        s_pool.page = before;
    }
    s_pool.refused = 0;
    s_folder_patterns = NULL;
    s_sents = NULL;
    s_outs = NULL;
    if (overflowed)
    {
        run->sents = 0u;
        run->outs = 0u;
        mmgr_line_put(args->err_msg, run->deployment.name);
        mmgr_line_put(args->err_msg, " sends more files than orior_cli supports");
        return 0;
    }
    qsort(run->sent, run->sents, sizeof(run->sent[0]), walk_sent_order);
    qsort(run->out, run->outs, sizeof(run->out[0]), walk_out_order);
    for (unsigned int at = 0u; at < run->sents; at += 1u)
    {
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea line = {full, sizeof(full), 0ULL};
        EMBED_CALL(cli_deploy_out_index_internal.full, CliDeployOutIndexCtx, .index = at, .out = &line);
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = (mmgr_line_finish(&line) != 0ULL)
                                               ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                            .size_pointer = &size, .owner = &g_cli_deploy_cells)
                                               : NULL;
        if (bytes != NULL)
        {
            VerbaLinea hash = {run->sent[at].hash, sizeof(run->sent[at].hash), 0ULL};
            EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = bytes, .size = size, .text = &hash);
            mmgr_line_finish(&hash);
            g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_deploy_cells));
        }
    }
    return 1;
}
