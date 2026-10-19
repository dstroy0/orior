// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy.h: deployments: a build's files sent to the places a tree names, to several at once. The tree names them in
// deploy.json at its top folder, each deployment by its name, the folder whose files go, the patterns of the files
// that stay, the places, and how many releases each place keeps:
//
//   {"deployments": [{"name": "site", "from": "dist", "exclude": ["*.map"], "to": ["me@web1:/srv/site"], "keep": 5}]}
//
// A place is an address of a machine as orior reaches one, user@host:folder over ssh with the keys the system's agent
// holds, docker:container:folder or wsl:distribution:folder, or a folder on this machine. Its files go as a tar stream,
// which keeps each file's bytes, line ends and all, and its mode, as git's index gives it where the tree is in git.
//
// On a machine, each deployment goes to a new folder under releases/, and `current` is turned to it only once every
// place holds the new release: a place that fails leaves each place on the release it had. Roll back turns each
// place's `current` to the release before it. A deployment `inPlace` sends its files into the folder itself, as a
// folder on Windows takes them.
//
// Before a file goes, the copy the place holds is compared with the copy last sent there: a copy changed there since
// is shown with its differences, to keep or to replace. The files a place holds that the tree no longer does are
// listed, to keep or to delete (deploy_read.c, deploy_gather.c, deploy_walk.c, deploy_tar.c, deploy_there.c,
// deploy_standing.c, deploy_run.c, deploy_send.c, deploy_jobs.c)
#ifndef DEPLOY_H
#define DEPLOY_H

#include "../command_line/cli_internal.h"

// what a deployment does, as the job's `do` says
#define CLI_DEPLOY_PREVIEW 0
#define CLI_DEPLOY_SEND 1
#define CLI_DEPLOY_ROLL_BACK 2

// a line a deployment says as it goes, `size` bytes of it
typedef void (*CliDeploySay)(void *context, const char *line, unsigned long long size);

// the act a job's `do` names: CLI_DEPLOY_PREVIEW, CLI_DEPLOY_SEND or CLI_DEPLOY_ROLL_BACK, or -1 where it names none
// (deploy_jobs.c)
int cli_deploy_act_of(const char *word);

// the deployment `name` of the tree at `root` previewed, sent or rolled back as `act` says: files changed at a place
// since they were last sent kept there unless `replace`, and files there the tree no longer holds kept unless `delete`,
// each step said to `say` as it goes: 1, or 0 with why said (deploy_run.c)
int cli_deploy_run(const char *root, const char *name, int act, int replace, int remove, CliDeploySay say,
                   void *context, CliLine *said);

#endif
