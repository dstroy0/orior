// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_jobs.c: one job for each deployment a tree names, which previews it, sends it, or rolls it back; and the act
// a job's `do` names
#include "../command_line/catalog.h"
#include "../command_line/cells.h"
#include "../text/pieces.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"

// the tree deploy.json is read into for the catalog, apart from a run's, given back once the jobs are listed, and its
// lock
static CliDeployTree s_tree;
static CliLock s_lock = CLI_LOCK_INIT;

int cli_deploy_act_of(const char *word)
{
    static const char *const acts[3] = {"preview", "send", "roll back"};
    for (int act = 0; act < 3; act += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = acts[act], .cap = CLI_CELLS_POOL}))
        {
            return act;
        }
    }
    return -1;
}

// a choice param put on the last job, its first choice its default
static void jobs_choice(CliCatalog *catalog, const char *key, const char *first, const char *second, const char *third)
{
    cli_catalog_param(catalog, key, "choice", 1);
    cli_catalog_choice(catalog, first);
    cli_catalog_choice(catalog, second);
    if (third != NULL)
    {
        cli_catalog_choice(catalog, third);
    }
}

void cli_deploy_jobs(const char *root, CliCatalog *catalog)
{
    cli_lock_take(&s_lock);
    unsigned int count = 0u;
    char why[CLI_SAID_LONGEST];
    CliLine none = {why, sizeof(why), 0ull};
    const CliDeployment *deployments = NULL;
    if (!cli_deploy_read(root, &s_tree, &deployments, &count, &none))
    {
        count = 0u;
    }
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const CliDeployment *const deployment = &deployments[index];
        static char s_id[CLI_PATH_LONGEST];
        CliLine id = {s_id, sizeof(s_id), 0ull};
        char piece[CLI_DEPLOY_PIECE];
        CliPieces pieces;
        CliLine about;
        cli_pieces_begun(&pieces, &about, piece, sizeof(piece));
        cli_line_text(&id, "deploy/");
        cli_line_text(&id, deployment->name);
        cli_line_text(&about, "sends the files of ");
        cli_line_text(&about, deployment->from);
        cli_line_text(&about, " to ");
        for (unsigned int at = 0u; at < deployment->tos; at += 1u)
        {
            cli_line_text(&about, (at > 0u) ? ", " : "");
            cli_line_text(&about, deployment->to[at]);
        }
        cli_line_text(&about, ", a new release at each turned to once every place holds it.");
        unsigned long long size = 0ull;
        char *const joined = cli_pieces_joined(&pieces, &about, "a deployment's job", &size);
        const int listed =
            (cli_line_closed(&id) != 0ull) && (joined != NULL) &&
            (cli_catalog_job(catalog, s_id, "deploy", deployment->name, "deploy.json", joined, "") != CLI_CATALOG_NONE);
        cli_cell_given(joined);
        if (!listed)
        {
            continue;
        }
        jobs_choice(catalog, "do", "preview", "send", "roll back");
        jobs_choice(catalog, "changed there", "keep them", "replace them", NULL);
        jobs_choice(catalog, "gone there", "keep them", "delete them", NULL);
        cli_catalog_step(catalog, CLI_PROGRAM_ORIOR, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_LIT, "deploy", NULL, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_LIT, deployment->name, NULL, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_VALUE, "do", NULL, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_VALUE, "changed there", NULL, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_VALUE, "gone there", NULL, NULL, NULL, 0);
    }
    cli_deploy_tree_given(&s_tree);
    cli_lock_give(&s_lock);
}
