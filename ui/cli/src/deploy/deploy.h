// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy.h: deployments: a build's files sent to the places a tree names, to several at once. The tree names them in
// deploy.json at its top folder, each deployment by its name, the folder whose files go, the patterns of the files
// that stay, the places, and how many releases each place retains:
//
//   {"deployments": [{"name": "site", "from": "dist", "exclude": ["*.map"], "to": ["me@web1:/srv/site"], "retain": 5}]}
//
// A place is an address of a machine as orior_cli reaches one, user@host:folder over ssh with the keys the system's
// agent has, docker:container:folder or wsl:distribution:folder, or a folder on this machine. Its files go as a tar
// stream, which preserves each file's bytes, line ends and all, and its mode, as git's index gives it where the tree is
// in git.
//
// On a machine, each deployment goes to a new folder under releases/, and `current` is turned to it only once every
// place has the new release: a place that fails leaves each place on the release it had. Roll back turns each
// place's `current` to the release before it. A deployment `inPosition` sends its files into the folder itself, as a
// folder on Windows takes them.
//
// Before a file goes, the copy the place has is compared with the copy last sent there: a copy changed there since
// is shown with its differences, to retain or to replace. The files a place has that the tree no longer does are
// listed, to retain or to delete (deploy_read.c, deploy_gather.c, deploy_walk.c, deploy_tar.c, deploy_there.c,
// deploy_existing.c, deploy_run.c, deploy_send.c, deploy_jobs.c)
#ifndef DEPLOY_H
#define DEPLOY_H
typedef struct CliDeployRootCfg CliDeployRootCfg;
typedef struct CliDeployCfg CliDeployCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// what a deployment does, as the job's `do` specifies
#define CLI_DEPLOY_PREVIEW 0
#define CLI_DEPLOY_SEND 1
#define CLI_DEPLOY_ROLL_BACK 2

// a line a deployment prints as it goes, `size` bytes of it
typedef void (*CliDeployPrintFn)(const CliEachLineSizeCfg *args);

// the act a job's `do` names: CLI_DEPLOY_PREVIEW, CLI_DEPLOY_SEND or CLI_DEPLOY_ROLL_BACK, or -1 where it names none
// (deploy_jobs.c)
int cli_deploy_act_get(const CliDeployCfg *args);

// the deployment `name` of the tree at `root` previewed, sent or rolled back as `act` specifies: files changed at a
// place since they were last sent retained there unless `replace`, and files there the tree no longer has retained
// unless `delete`, each step printed to `print` as it goes: 1, or 0 with why given (deploy_run.c)
int cli_deploy_run(const CliDeployRootCfg *args);

struct CliDeployRootCfg
{
    const char *root;
    const char *name;
    int act;
    int replace;
    int remove;
    CliDeployPrintFn print;
    void *context;
    VerbaLinea *err_msg;
};

struct CliDeployCfg
{
    const char *word;
};

typedef struct
{
    int (*act_get)(const CliDeployCfg *args);
} CliDeployNs;
EMBED_TABLE_LAYOUT(CliDeployNs, act_get);
EMBED_TABLE_STORAGE CliDeployNs cli_deploy_table EMBED_UNUSED = {
    .act_get = cli_deploy_act_get,
};

typedef struct
{
    int (*run)(const CliDeployRootCfg *args);
} CliDeployRootNs;
EMBED_TABLE_LAYOUT(CliDeployRootNs, run);
EMBED_TABLE_STORAGE CliDeployRootNs cli_deploy_root EMBED_UNUSED = {
    .run = cli_deploy_run,
};

#endif
