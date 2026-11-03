// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_there.c: the hashes of the files a place has, and of those last sent there as the record stored in orior's
// own folder has them
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/digest.h"
#include "../text/text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

#include <stdlib.h>

// the files a place has and their hashes, by path, sorted, and those last sent there and theirs, each list in the
// place's pool at its count; the pool, which contains the texts the walk of a folder here finds and each pair as it is
// found, the newest first; the listing a machine gave of them, in a cell of its size; and the record of the hashes last
// sent there, read into a tree of its size. Each is given back as the next place is read and as the run ends
CliDeployPair *g_cli_deploy_there;
unsigned int g_cli_deploy_there_count;
CliDeployPair *g_cli_deploy_last;
unsigned int g_cli_deploy_last_count;
static CliDeployPool s_pool;
static CliDeployPairNode *s_pairs;
static char *s_listing;
static CliJson s_record;
static const char *s_walk_top;
static int s_walk_full;

void cli_deploy_there_free(void)
{
    g_cli_deploy_there = NULL;
    g_cli_deploy_there_count = 0u;
    g_cli_deploy_last = NULL;
    g_cli_deploy_last_count = 0u;
    s_pairs = NULL;
    EMBED_CALL(cli_deploy_pool_internal.pool_free, CliDeployPoolCtx, .pool = &s_pool);
    g_cli_cells->persistent_buf_release(s_listing, &g_cli_deploy_cells);
    s_listing = NULL;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_record);
}

typedef struct
{
    const char *path;
    const char *hash;
} DeployThereCtx;
// a file's path and hash stored before the pairs stored already
EMBED_INLINE void there_pair_append(const DeployThereCtx *args)
{
    CliDeployPairNode *const node =
        EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool, .size = sizeof(*node));
    if (node != NULL)
    {
        node->pair = (CliDeployPair){args->path, args->hash};
        node->before = s_pairs;
        s_pairs = node;
        g_cli_deploy_there_count += 1u;
    }
}

// two pairs in the order of their paths
static int there_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = ((const CliDeployPair *)one)->path,
                      .right = ((const CliDeployPair *)other)->path);
}

// the text beside `path` among `count` sorted pairs, NULL where it is not among them
const char *cli_deploy_find(const CliDeployCountCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = args->pairs[middle].path,
                                     .right = args->path);
        if (order == 0)
        {
            return args->pairs[middle].text;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return NULL;
}

// the hash the run sends `path` with, NULL where it does not send it
const char *cli_deploy_ours(const CliDeployCountCtx *args)
{
    const CliDeployRun *const run = &g_cli_deploy;
    unsigned int low = 0u;
    unsigned int high = run->sents;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = run->sent[middle].path,
                                     .right = args->path);
        if (order == 0)
        {
            return run->sent[middle].hash;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return NULL;
}

// the file of the record of the hashes last sent by the run's deployment to the place at `index` into `path`, which
// is sized for CLI_PATH_MAX_LEN: 1, or 0 where orior's own folder is not known
int cli_deploy_record_path(const CliDeployIndexPathCtx *args)
{
    const CliDeployRun *const run = &g_cli_deploy;
    char home[CLI_PATH_MAX_LEN];
    char key[(3u * CLI_PATH_MAX_LEN) + 8u];
    VerbaLinea key_line = {key, sizeof(key), 0ULL};
    mmgr_line_put(&key_line, run->root);
#if defined(_WIN32)
    for (unsigned long long at = 0ULL; at < key_line.at; at += 1u)
    {
        key[at] = (key[at] == '/') ? '\\' : key[at];
    }
#endif
    mmgr_line_ch(&key_line, '\n');
    mmgr_line_put(&key_line, run->deployment.name);
    mmgr_line_ch(&key_line, '\n');
    EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = &run->place[args->index],
               .out = &key_line);
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home) || !mmgr_line_ok(&key_line))
    {
        return 0;
    }
    char digest[65];
    VerbaLinea digest_line = {digest, sizeof(digest), 0ULL};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = key, .size = key_line.at,
               .text = &digest_line);
    VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/deployed/");
    mmgr_line_put_n(&line, digest, 16u);
    mmgr_line_put(&line, ".json");
    return mmgr_line_finish(&line) != 0ULL;
}

// the record of the hashes last sent to the place at `index` read into g_cli_deploy_last, placed in the place's pool at
// its count, none where it is not there or reads as no object of texts
void cli_deploy_last_read(const CliDeployErrMsgIndexCtx *args)
{
    g_cli_deploy_last = NULL;
    g_cli_deploy_last_count = 0u;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_record);
    char path[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ULL};
    const unsigned int top =
        EMBED_CALL(cli_deploy_index_path_internal.record_path, CliDeployIndexPathCtx, .index = args->index,
                   .path_out = path)
            ? EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &s_record, .path = path,
                         .nodes_more = 0u, .text_more = 0ULL, .err_msg = &none, .owner = &g_cli_deploy_cells)
            : CLI_JSON_NONE;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_record, .node = top,
                    .kind = CFG_JSON_OBJECT))
    {
        return;
    }
    unsigned int members = 0u;
    for (unsigned int member = s_record.node[top].first; member != CLI_JSON_NONE; member = s_record.node[member].next)
    {
        if (EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_record,
                       .node = member, .length_pointer = NULL) == NULL)
        {
            return;
        }
        members += 1u;
    }
    CliDeployPair *const last = (members > 0u)
                                    ? EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool,
                                                 .size = (unsigned long long)members * sizeof(last[0]))
                                    : NULL;
    if (last == NULL)
    {
        return;
    }
    unsigned int at = 0u;
    for (unsigned int member = s_record.node[top].first; member != CLI_JSON_NONE; member = s_record.node[member].next)
    {
        last[at] = (CliDeployPair){s_record.text + s_record.node[member].key,
                                   EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = &s_record, .node = member, .length_pointer = NULL)};
        at += 1u;
    }
    qsort(last, members, sizeof(last[0]), there_order);
    g_cli_deploy_last = last;
    g_cli_deploy_last_count = members;
}

// one entry of a folder of this machine hashed: a folder read in turn, a file's bytes hashed
static void there_walk(const CliEachNameIsFolderCfg *args)
{
    const void *context = args->context;
    const char *const folder = (const char *)context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&path_line, folder);
    mmgr_line_put(&path_line, (folder[0] != '\0') ? "/" : "");
    mmgr_line_put(&path_line, args->name);
    // the path ended before the whole path is written from it
    const unsigned long long ended = mmgr_line_finish(&path_line);
    char full[(2u * CLI_PATH_MAX_LEN) + 2u];
    VerbaLinea full_line = {full, sizeof(full), 0ULL};
    mmgr_line_put(&full_line, s_walk_top);
    mmgr_line_ch(&full_line, '/');
    mmgr_line_put(&full_line, (ended != 0ULL) ? path : "");
    if ((ended == 0ULL) || (mmgr_line_finish(&full_line) == 0ULL))
    {
        s_walk_full = 1;
        return;
    }
    if (args->is_folder)
    {
        (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = full, .each = there_walk,
                         .context_bytess = (void *)EMBED_CALL(cli_deploy_pool_internal.text_store, CliDeployPoolCtx,
                                                              .pool = &s_pool, .text = path, .size = path_line.at));
        return;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                  .size_pointer = &size, .owner = &g_cli_deploy_cells);
    if (bytes == NULL)
    {
        return;
    }
    char digest[65];
    VerbaLinea digest_line = {digest, sizeof(digest), 0ULL};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = bytes, .size = size, .text = &digest_line);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_deploy_cells);
    EMBED_CALL(there_pair_append, DeployThereCtx,
               .path = EMBED_CALL(cli_deploy_pool_internal.text_store, CliDeployPoolCtx, .pool = &s_pool, .text = path,
                                  .size = path_line.at),
               .hash = EMBED_CALL(cli_deploy_pool_internal.text_store, CliDeployPoolCtx, .pool = &s_pool,
                                  .text = digest, .size = 64u));
}

// the hashes of the files under `folder` of the place at `index`, by their paths under it, into g_cli_deploy_there,
// placed in the place's pool at their count and sorted: 1, or 0 with why given
int cli_deploy_hashes(const CliDeployErrMsgIndexCtx *args)
{
    const CliDeployPosition *const place = &g_cli_deploy.place[args->index];
    cli_deploy_there_free();
    s_walk_full = 0;
    if (place->here)
    {
        s_walk_top = args->folder;
        (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
                         .each = there_walk, .context_bytess = (void *)"");
    }
    else
    {
        char line[(2u * CLI_PATH_MAX_LEN) + 128u];
        VerbaLinea command = {line, sizeof(line), 0ULL};
        mmgr_line_put(&command, "cd ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = args->folder,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}), .out = &command);
        mmgr_line_put(&command, " 2>/dev/null || exit 0; find . -type f -exec sha256sum {} + 2>/dev/null; true");
        mmgr_line_finish(&command);
        s_listing = g_cli_cells->temporary_buf_alloc(CLI_DEPLOY_HASHES, &g_cli_deploy_cells);
        VerbaLinea listing = mmgr_line_over(s_listing, CLI_DEPLOY_HASHES);
        const int run_result =
            EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx, .place = place, .line_text = line,
                       .input = NULL, .size = 0ULL, .out = &listing, .err_msg = args->err_msg);
        if (!run_result)
        {
            return 0;
        }
        s_walk_full = !mmgr_line_ok(&listing);
        const unsigned long long size = s_walk_full ? 0ULL : listing.at;
        static const char s_split[] = "  ";
        // a line is ended by a 0 from its first write, and a line with none holds nothing to walk
        for (char *at = (size > 0ULL) ? s_listing : NULL; (at != NULL) && (*at != '\0');)
        {
            const char *const newline = memor.chr(
                &(MemoriaCfg){.src = at, .bytes = (size_t)(size - (unsigned long long)(at - s_listing)), .val = '\n'});
            char *const end = (newline != NULL) ? (at + (newline - at)) : (s_listing + size);
            const int more = (newline != NULL);
            *end = '\0';
            char *const line = at;
            at = more ? (end + 1) : end;
            const char *const split_read = cellul.find(&(CatenaFinitaCfg){
                .src = line, .cap = (size_t)(end - line) + 1u, .other = s_split, .other_cap = sizeof(s_split)});
            if (split_read == NULL)
            {
                continue;
            }
            char *const split = line + (split_read - line);
            *split = '\0';
            const char *path = split + 2;
            while ((path[0] == '.') && (path[1] == '/'))
            {
                path += 2;
            }
            const char *hash = line;
            hash += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = hash, .at = 0ULL,
                               .size = cellul.len(&(CatenaFinitaCfg){.src = hash, .cap = CLI_CELLS_POOL}), .set = "\\");
            EMBED_CALL(there_pair_append, DeployThereCtx, .path = path, .hash = hash);
        }
    }
    CliDeployPair *const there =
        (g_cli_deploy_there_count > 0u)
            ? EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &s_pool,
                         .size = (unsigned long long)g_cli_deploy_there_count * sizeof(there[0]))
            : NULL;
    if (s_walk_full || s_pool.refused)
    {
        cli_deploy_there_free();
        mmgr_line_put(args->err_msg, "it has more files than orior_cli supports");
        return 0;
    }
    for (unsigned int at = g_cli_deploy_there_count; (there != NULL) && (s_pairs != NULL); s_pairs = s_pairs->before)
    {
        at -= 1u;
        there[at] = s_pairs->pair;
    }
    // no pairs is no array to order, and qsort takes no null
    if (there != NULL)
    {
        qsort(there, g_cli_deploy_there_count, sizeof(there[0]), there_order);
    }
    g_cli_deploy_there = there;
    return 1;
}
