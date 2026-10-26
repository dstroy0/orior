// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_walk.c: the folder a deployment sends walked: each file it sends with its mode and the hash of its bytes,
// and each file and folder its patterns leave out with the pattern that leaves it out, each list sorted by path
#include "../command_line/cells.h"
#include "../text/digest.h"
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

// a path under the folder sent, `name` in the folder `folder`, stored in `pool`
static const char *walk_path(CliDeployPool *pool, const char *folder, const char *name, const char *after)
{
    char path[CLI_PATH_MAX_LEN + 2u];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_put(&line, (folder[0] != '\0') ? "/" : "");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, after);
    s_overflowed = s_overflowed || !mmgr_line_ok(&line);
    return cli_deploy_text_stored(pool, path, line.at);
}

// one entry of a folder read: a folder to read in turn or left out, a file sent or left out
static void walk_entry(void *context, const char *name, int is_folder)
{
    CliDeployRun *const run = &g_cli_deploy;
    const char *const folder = (const char *)context;
    const char *const path = walk_path(&s_pool, folder, name, "");
    char probe[CLI_PATH_MAX_LEN + 4u];
    VerbaLinea probe_line = {probe, sizeof(probe), 0ull};
    mmgr_line_put(&probe_line, path);
    mmgr_line_put(&probe_line, "/x");
    mmgr_line_finish(&probe_line);
    const char *const rule = is_folder ? cli_deploy_left_out_by(s_folder_patterns, s_folder_pattern_count, probe)
                                       : cli_deploy_left_out_by(run->pattern, run->patterns, path);
    if (rule != NULL)
    {
        CliDeployPairNode *const out = cli_deploy_placed(&s_pool, sizeof(*out));
        if (out != NULL)
        {
            out->pair = (CliDeployPair){walk_path(&run->pool, folder, name, is_folder ? "/" : ""), rule};
            out->before = s_outs;
            s_outs = out;
            run->outs += 1u;
        }
        return;
    }
    if (is_folder)
    {
        CliDeployFolder *const below = cli_deploy_placed(&s_pool, sizeof(*below));
        if (below != NULL)
        {
            below->path = path;
            below->below = s_folders;
            s_folders = below;
        }
        return;
    }
    CliDeploySentNode *const sent = cli_deploy_placed(&s_pool, sizeof(*sent));
    if (sent != NULL)
    {
        char in_tree[(2u * CLI_PATH_MAX_LEN) + 2u];
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea tree_line = {in_tree, sizeof(in_tree), 0ull};
        VerbaLinea full_line = {full, sizeof(full), 0ull};
        mmgr_line_put(&tree_line, s_from);
        mmgr_line_put(&tree_line, (s_from[0] != '\0') ? "/" : "");
        mmgr_line_put(&tree_line, path);
        mmgr_line_finish(&tree_line);
        mmgr_line_put(&full_line, s_top);
        mmgr_line_ch(&full_line, '/');
        mmgr_line_put(&full_line, path);
        mmgr_line_finish(&full_line);
        sent->sent.path = cli_deploy_text_stored(&run->pool, path,
                                                 cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        sent->sent.mode = cli_deploy_mode(in_tree, full);
        sent->sent.hash[0] = '\0';
        sent->before = s_sents;
        s_sents = sent;
        run->sents += 1u;
    }
}

void cli_deploy_full(unsigned int index, VerbaLinea *out)
{
    char top[CLI_PATH_MAX_LEN];
    cli_full(g_cli_deploy.root, g_cli_deploy.deployment.from, top);
    mmgr_line_put(out, top);
    mmgr_line_ch(out, '/');
    mmgr_line_put(out, g_cli_deploy.sent[index].path);
}

// the path of the folder sent in the tree, its `.` and empty parts taken out, into s_from
static void walk_from(const char *from)
{
    VerbaLinea line = {s_from, sizeof(s_from), 0ull};
    for (const char *part = from; *part != '\0';)
    {
        size_t length = 0u;
        while ((part[length] != '\0') && (part[length] != '/') && (part[length] != '\\'))
        {
            length += 1u;
        }
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
    return cli_deploy_order(((const CliDeploySent *)one)->path, ((const CliDeploySent *)other)->path);
}

// two files left out in the order of their paths, then of their patterns
static int walk_out_order(const void *one, const void *other)
{
    const CliDeployPair *const left = (const CliDeployPair *)one;
    const CliDeployPair *const right = (const CliDeployPair *)other;
    const int paths = cli_deploy_order(left->path, right->path);
    return (paths != 0) ? paths : cli_deploy_order(left->text, right->text);
}

int cli_deploy_gather(VerbaLinea *err_msg)
{
    CliDeployRun *const run = &g_cli_deploy;
    cli_full(run->root, run->deployment.from, s_top);
    if (!query_is_folder(s_top))
    {
        mmgr_line_put(err_msg, run->deployment.name);
        mmgr_line_put(err_msg, " names ");
        mmgr_line_put(err_msg, run->deployment.from);
        mmgr_line_put(err_msg, ", which is no folder of the tree");
        return 0;
    }
    walk_from(run->deployment.from);
    cli_deploy_pool_given(&s_pool);
    s_folder_patterns = cli_deploy_placed(&s_pool, (unsigned long long)run->patterns * sizeof(s_folder_patterns[0]));
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
    cli_deploy_modes_read(&s_pool);
    run->sent = NULL;
    run->sents = 0u;
    run->out = NULL;
    run->outs = 0u;
    s_sents = NULL;
    s_outs = NULL;
    s_overflowed = 0;
    s_folders = cli_deploy_placed(&s_pool, sizeof(*s_folders));
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
        VerbaLinea line = {full, sizeof(full), 0ull};
        mmgr_line_put(&line, s_top);
        mmgr_line_put(&line, (folder[0] != '\0') ? "/" : "");
        mmgr_line_put(&line, folder);
        if (mmgr_line_finish(&line) != 0ull)
        {
            (void)query_folder_each(full, walk_entry, (void *)folder);
        }
    }
    run->sent = cli_deploy_placed(&run->pool, (unsigned long long)run->sents * sizeof(run->sent[0]));
    run->out = cli_deploy_placed(&run->pool, (unsigned long long)run->outs * sizeof(run->out[0]));
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
    cli_deploy_pool_given(&s_pool);
    s_folder_patterns = NULL;
    s_sents = NULL;
    s_outs = NULL;
    if (overflowed)
    {
        run->sents = 0u;
        run->outs = 0u;
        mmgr_line_put(err_msg, run->deployment.name);
        mmgr_line_put(err_msg, " sends more files than orior_cli supports");
        return 0;
    }
    qsort(run->sent, run->sents, sizeof(run->sent[0]), walk_sent_order);
    qsort(run->out, run->outs, sizeof(run->out[0]), walk_out_order);
    for (unsigned int at = 0u; at < run->sents; at += 1u)
    {
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea line = {full, sizeof(full), 0ull};
        cli_deploy_full(at, &line);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = (mmgr_line_finish(&line) != 0ull) ? cli_file_read(full, &size) : NULL;
        if (bytes != NULL)
        {
            VerbaLinea hash = {run->sent[at].hash, sizeof(run->sent[at].hash), 0ull};
            cli_sha256_text(bytes, size, &hash);
            mmgr_line_finish(&hash);
            cli_file_let_go(bytes, size);
        }
    }
    return 1;
}
