// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs.h: the tree's environments: the places in the tree a Python environment is, found at any depth, each the
// tree's own environment once the reader chooses it.
//
// A virtual environment is a folder that contains `pyvenv.cfg`. A Pipfile's folder is a Pipenv environment, which
// Pipenv is queried where it stores; and a folder with `shell.nix`, `flake.nix` or `default.nix` a Nix environment,
// whose development shell Nix is queried for its PATH. Each is named by its folder's path in the tree: the name
// recorded, and one that moves with the tree. The packages a `setup.cfg`, a Pipfile or a `requirements.txt` beside it
// requires are read, and an environment chosen reports which of them it has not installed.
//
// The environment chosen is the toolchains' own while it is: its folders come first on the PATH of everything
// impero starts, and its Python is the tree's Python (envs.c, envs_find.c, envs_resolve.c)
#ifndef ENVS_H
#define ENVS_H
#include "embed_dispatch_layout.h"
#include "toolchains.h"

typedef struct CliEnvsRootCfg CliEnvsRootCfg;
typedef struct CliEnvsCfg CliEnvsCfg;
typedef struct CliEnvsOutFolderCfg CliEnvsOutFolderCfg;

// the cells the environments are found and their requirements read through, counted, each given back before the call

// the environment at `place` in the tree at `root`, of `type`, made ready to use into `environment`: for Pipenv, the
// virtual environment Pipenv stores for the folder; for Nix, the folders its development shell puts on the PATH. 1, or
// 0 with why given
int cli_envs_resolve(const CliEnvsRootCfg *args);

struct CliEnvsRootCfg
{
    const char *root;
    const char *type;
    const char *place;
    CliTreeEnvironment *environment;
    VerbaLinea *err_msg;
};

struct CliEnvsCfg
{
    const char *root;
    VerbaLinea *out;
};

struct CliEnvsOutFolderCfg
{
    VerbaLinea *out;
    const char *folder;
};

typedef struct
{
    int (*resolve)(const CliEnvsRootCfg *args);
} CliEnvsRootNs;
EMBED_TABLE_LAYOUT(CliEnvsRootNs, resolve);
EMBED_TABLE_STORAGE CliEnvsRootNs cli_envs_root EMBED_UNUSED = {
    .resolve = cli_envs_resolve,
};

#endif
