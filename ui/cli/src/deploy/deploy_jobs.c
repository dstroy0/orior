// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_jobs.c: one job for each deployment a tree names, which previews it, sends it, or rolls it back; and the act
// a job's `do` names
#include "../command_line/catalog.h"
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"

// the tree deploy.json is read into for the catalog, apart from a run's, given back once the jobs are listed, and its
// lock
static CliDeployTree s_tree;

int cli_deploy_act_get(const CliDeployCfg *args)
{
    static const char *const acts[3] = {"preview", "send", "roll back"};
    for (int act = 0; act < 3; act += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->word_text, .other = acts[act], .cap = CLI_CELLS_POOL}))
        {
            return act;
        }
    }
    return -1;
}

typedef struct
{
    CliCatalog *catalog;
    const char *key;
    const char *first;
    const char *second;
    const char *third;
} DeployJobsCtx;
// a choice param put on the last job, its first choice its default
EMBED_INLINE void jobs_choice(const DeployJobsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    const char *key = args->key;
    EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = catalog, .key = key, .kind = "choice",
               .required = 1);
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog, .choice = args->first);
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog, .choice = args->second);
    if (args->third != NULL)
    {
        EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog, .choice = args->third);
    }
}

void cli_deploy_jobs(const CliCatalogRootCfg *args)
{
    const char *root = args->root;
    unsigned int count = 0u;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ULL};
    const CliDeployment *deployments = NULL;
    if (!EMBED_CALL(cli_deploy_root_internal.read, CliDeployRootCtx, .root = root, .tree = &s_tree,
                    .deployments = &deployments, .count_pointer = &count, .err_msg = &none))
    {
        count = 0u;
    }
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const CliDeployment *const deployment = &deployments[index];
        static char s_id[CLI_PATH_MAX_LEN];
        VerbaLinea id = {s_id, sizeof(s_id), 0ULL};
        // the folder it sends from and each place, and the words around them
        const unsigned long long capacity = ((unsigned long long)deployment->tos + 2ULL) * CLI_PATH_MAX_LEN;
        char *const about_text = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_deploy_cells);
        VerbaLinea about = mmgr_line_over(about_text, capacity);
        mmgr_line_put(&id, "deploy/");
        mmgr_line_put(&id, deployment->name);
        mmgr_line_put(&about, "sends the files of ");
        mmgr_line_put(&about, deployment->from);
        mmgr_line_put(&about, " to ");
        for (unsigned int at = 0u; at < deployment->tos; at += 1u)
        {
            mmgr_line_put(&about, (at > 0u) ? ", " : "");
            mmgr_line_put(&about, deployment->to[at]);
        }
        mmgr_line_put(&about, ", a new release at each turned to once every place has it.");
        const int list_entries =
            (mmgr_line_finish(&id) != 0ULL) && mmgr_line_ok(&about) &&
            (EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = args->catalog, .id = s_id,
                        .group = "deploy", .title = deployment->name, .file = "deploy.json", .about = about_text,
                        .opens = "") != CLI_CATALOG_NONE);
        g_cli_cells->persistent_buf_release(about_text, &g_cli_deploy_cells);
        if (!list_entries)
        {
            continue;
        }
        EMBED_CALL(jobs_choice, DeployJobsCtx, .catalog = args->catalog, .key = "do", .first = "preview",
                   .second = "send", .third = "roll back");
        EMBED_CALL(jobs_choice, DeployJobsCtx, .catalog = args->catalog, .key = "changed there", .first = "retain them",
                   .second = "replace them", .third = NULL);
        EMBED_CALL(jobs_choice, DeployJobsCtx, .catalog = args->catalog, .key = "gone there", .first = "retain them",
                   .second = "delete them", .third = NULL);
        EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = args->catalog,
                   .program = CLI_PROGRAM_ORIOR, .text = NULL, .tool_program = NULL, .system = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_LIT,
                   .a = "deploy", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_LIT,
                   .a = deployment->name, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_VALUE,
                   .a = "do", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_VALUE,
                   .a = "changed there", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_VALUE,
                   .a = "gone there", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
    g_cli_cells->persistent_buf_release(s_tree.json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = s_tree.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_tree.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    while (s_tree.pool.page != NULL)
    {
        CliDeployPage *const before = s_tree.pool.page->before;
        g_cli_cells->persistent_buf_release(s_tree.pool.page, &g_cli_deploy_cells);
        s_tree.pool.page = before;
    }
    s_tree.pool.refused = 0;
}
