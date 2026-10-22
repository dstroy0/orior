// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust.h: whether a tree's own code runs as orior's tools read the tree (trust.c). A tree holds code of its own where
// a tool runs one of its files as it reads it:
//
// - `build.rs` beside a Cargo.toml, and a crate whose Cargo.toml makes it a procedural macro, which rust-analyzer
//   builds and runs as it reads the tree;
// - a Prettier or ESLint configuration written in JavaScript, which Prettier and ESLint run as they format or check a
//   file;
// - a hook git runs as it reads the tree's state, `core.fsmonitor` naming a program in the tree's `.git/config`.
//
// The reader's answer is kept in trust.json in orior's home, by folder, and a folder's answer holds for every tree
// under it, the nearest folder's answer standing. A tree that holds code of its own and is not trusted has that code
// held back: rust-analyzer starts with its build scripts, its procedural macros and its checks off, the checkers the
// reader names do not run, Prettier does not format where the tree's configuration is JavaScript, and git is told to
// run no fsmonitor hook. A tree with no code of its own is never asked about.
#ifndef TRUST_H
#define TRUST_H

#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

// The files of a tree found to run as code, as the tree's paths: one after another, each with its zero after it, in a
// cell of their bytes given back with cli_trust_code_given, and their count
typedef struct
{
    char *paths;
    unsigned long long size;
    unsigned int count;
} CliTrustCode;

// the cells the files found to run as code and the files read to find them hold
extern CliCellsTally g_cli_trust_cells;

// the reader's answer for `tree`, the answer kept for the nearest folder that is the tree or holds it: 1 with whether
// it is trusted into `trusted`, or 0 where no answer is kept
int cli_trust_answer(const char *tree, int *trusted);
// the answer for `folder` and every tree under it kept, in place of those kept for it and the folders under it before:
// 1, or 0 with why said
int cli_trust_set(const char *folder, int trusted, CliLine *said);
// the files of `tree` its tools run as code as they read it into `code`, kept for ten seconds before the tree is
// looked through again: 1, or 0 with none into `code` where the cellblock could not meet them; and their cell given
// back, `code` left holding none
int cli_trust_own_code(const char *tree, CliTrustCode *code);
void cli_trust_code_given(CliTrustCode *code);
// whether `tree` holds code of its own the reader has not trusted, held back until they do; and whether Prettier is
// held back there, its configuration there being JavaScript
int cli_trust_held_back(const char *tree);
int cli_trust_prettier_held_back(const char *tree);
// the words git is started with in `tree` into `words`, which holds 2: that it run no fsmonitor hook, where the
// configuration of the repository holding the tree names one and the tree is not trusted: their count
unsigned int cli_trust_git_words(const char *tree, const char **words);
// what the window is told of `tree` written as JSON into `out`: the answer kept, the code it holds, the tree, and the
// folder above it, for an answer that holds for every tree there
void cli_trust_told(const char *tree, CliLine *out);

// what the checks hold: whether git's `core.fsmonitor`, as `config` sets it, names a program to run, and not git's own
// monitor or none; whether `inner` is `folder` or under it; the answer `tree` has by the file of answers at `path`; the
// answer for `folder` kept in the file at `path`; and the files of `tree` its tools run as code, looked for now, into
// `code` as cli_trust_own_code gives them
int cli_trust_names_a_hook(const char *config);
int cli_trust_under(const char *inner, const char *folder);
int cli_trust_answer_in(const char *path, const char *tree, int *trusted);
int cli_trust_set_in(const char *path, const char *folder, int trusted, CliLine *said);
int cli_trust_looked_for(const char *tree, CliTrustCode *code);

#endif
