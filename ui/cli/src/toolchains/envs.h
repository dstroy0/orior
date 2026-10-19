// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs.h: the tree's environments: the places in the tree a Python environment stands, found at any depth, each the
// tree's own environment once the reader chooses it.
//
// A virtual environment is a folder that holds `pyvenv.cfg`. A Pipfile's folder is a Pipenv environment, which Pipenv
// is asked where it keeps; and a folder with `shell.nix`, `flake.nix` or `default.nix` a Nix environment, whose
// development shell Nix is asked for its PATH. Each is named by its folder's path in the tree: the name kept, and one
// that moves with the tree. The packages a `setup.cfg`, a Pipfile or a `requirements.txt` beside it requires are read,
// and an environment chosen says which of them it has not installed.
//
// The environment chosen is the toolchains' own while it stands: its folders come first on the PATH of everything
// orior starts, and its Python is the tree's Python (envs.c, envs_resolve.c)
#ifndef ENVS_H
#define ENVS_H

#include "toolchains.h"

// every environment the tree at `root` holds, nearest the top first, written as `[{"kind", "place", "requires"}]`;
// the packages a folder's `setup.cfg`, Pipfile or `requirements.txt` requires, by name, in order, written as a JSON
// array of names
void cli_envs_found(const char *root, CliLine *out);
void cli_envs_requires_in(const char *folder, CliLine *out);

// the environment at `place` in the tree at `root`, of `kind`, made ready to use into `environment`: for Pipenv, the
// virtual environment Pipenv keeps for the folder; for Nix, the folders its development shell puts on the PATH. 1, or
// 0 with why said; and the packages `count` of `requires` names that the environment's Python has not installed,
// written as a JSON array of names
int cli_envs_resolve(const char *root, const char *kind, const char *place, CliTreeEnvironment *environment,
                     CliLine *said);
void cli_envs_missing(const CliTreeEnvironment *environment, const char *const *requires, unsigned int count,
                      CliLine *out);

#endif
