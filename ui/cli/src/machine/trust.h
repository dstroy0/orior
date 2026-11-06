// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust.h: whether a tree's own code runs as orior's tools read the tree (trust.c). A tree contains code of its own
// where a tool runs one of its files as it reads it:
//
// - `build.rs` beside a Cargo.toml, and a crate whose Cargo.toml makes it a procedural macro, which rust-analyzer
//   builds and runs as it reads the tree;
// - a Prettier or ESLint configuration written in JavaScript, which Prettier and ESLint run as they format or check a
//   file;
// - a hook git runs as it reads the tree's state, `core.fsmonitor` naming a program in the tree's `.git/config`.
//
// The reader's answer is stored in trust.json in orior's home, by folder, and a folder's answer applies to every tree
// under it, the nearest folder's answer standing. A tree that contains code of its own and is not trusted has that code
// disabled: rust-analyzer starts with its build scripts, its procedural macros and its checks off, the checkers the
// reader names do not run, Prettier does not format where the tree's configuration is JavaScript, and git is instructed
// to run no fsmonitor hook. A tree with no code of its own is never prompted about.
#ifndef TRUST_H
#define TRUST_H
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"

typedef struct CliTrustFolderCfg CliTrustFolderCfg;
typedef struct CliTrustTreeCfg CliTrustTreeCfg;
typedef struct CliTrustTreeWordsCfg CliTrustTreeWordsCfg;
typedef struct CliTrustTreeCodeCfg CliTrustTreeCodeCfg;
typedef struct CliTrustTreeOutCfg CliTrustTreeOutCfg;
typedef struct CliTrustFolderInnerCfg CliTrustFolderInnerCfg;
typedef struct CliTrustCfg CliTrustCfg;

// The files of a tree found to run as code, as the tree's paths: one after another, each with its zero after it, in a
// cell of their bytes released through persistent_buf_release, and their count
typedef struct
{
    char *paths;
    unsigned long long size;
    unsigned int count;
} CliTrustCode;

// the reader's answer for `tree`, the answer stored for the nearest folder that is the tree or contains it: 1 with
// whether it is trusted into `trusted`, or 0 where no answer is stored
int cli_trust_answer(const CliTrustTreeCfg *args);
// the files of `tree` its tools run as code as they read it into `code`, cached for ten seconds before the tree is
// looked through again: 1, or 0 with none into `code` where the cellblock could not meet them; and their cell given
// back, `code` left containing none
int cli_trust_own_code(const CliTrustTreeCodeCfg *args);
// whether `tree` contains code of its own the reader has not trusted, disabled until they do; and whether Prettier is
// disabled there, its configuration there being JavaScript
int cli_trust_block(const CliTrustTreeWordsCfg *args);
int cli_trust_prettier_block(const CliTrustTreeWordsCfg *args);
// the words git is started with in `tree` into `words`, sized for 2: that it run no fsmonitor hook, where the
// configuration of the repository containing the tree names one and the tree is not trusted: their count
unsigned int cli_trust_git_words(const CliTrustTreeWordsCfg *args);
// what the checks check: whether git's `core.fsmonitor`, as `config` sets it, names a program to run, and not git's own
// monitor or none; whether `inner` is `folder` or under it; the answer `tree` has by the file of answers at `path`; the
// answer for `folder` stored in the file at `path`; and the files of `tree` its tools run as code, looked for now, into
// `code` as cli_trust_own_code gives them
int cli_trust_names_a_hook(const CliTrustCfg *args);
int cli_trust_under(const CliTrustFolderInnerCfg *args);
int cli_trust_answer_in(const CliTrustTreeCfg *args);
int cli_trust_looked_for(const CliTrustTreeCodeCfg *args);

struct CliTrustFolderCfg
{
    const char *folder;
    int trusted_value;
    VerbaLinea *err_msg;
    const char *path;
};

struct CliTrustTreeCfg
{
    const char *tree;
    int *trusted;
    const char *path;
};

struct CliTrustTreeWordsCfg
{
    const char *tree;
    const char **words;
};

struct CliTrustTreeCodeCfg
{
    const char *tree;
    CliTrustCode *code;
};

struct CliTrustTreeOutCfg
{
    const char *tree;
    VerbaLinea *out;
};

struct CliTrustFolderInnerCfg
{
    const char *folder;
    const char *inner;
};

struct CliTrustCfg
{
    const char *config;
};

typedef struct
{
    int (*answer)(const CliTrustTreeCfg *args);
    int (*answer_in)(const CliTrustTreeCfg *args);
} CliTrustTreeNs;
EMBED_TABLE_LAYOUT(CliTrustTreeNs, answer, answer_in);
EMBED_TABLE_STORAGE CliTrustTreeNs cli_trust_tree EMBED_UNUSED = {
    .answer = cli_trust_answer,
    .answer_in = cli_trust_answer_in,
};

typedef struct
{
    int (*own_code)(const CliTrustTreeCodeCfg *args);
    int (*looked_for)(const CliTrustTreeCodeCfg *args);
} CliTrustTreeCodeNs;
EMBED_TABLE_LAYOUT(CliTrustTreeCodeNs, own_code, looked_for);
EMBED_TABLE_STORAGE CliTrustTreeCodeNs cli_trust_tree_code EMBED_UNUSED = {
    .own_code = cli_trust_own_code,
    .looked_for = cli_trust_looked_for,
};

typedef struct
{
    int (*block)(const CliTrustTreeWordsCfg *args);
    int (*prettier_block)(const CliTrustTreeWordsCfg *args);
    unsigned int (*git_words)(const CliTrustTreeWordsCfg *args);
} CliTrustTreeWordsNs;
EMBED_TABLE_LAYOUT(CliTrustTreeWordsNs, block, prettier_block, git_words);
EMBED_TABLE_STORAGE CliTrustTreeWordsNs cli_trust_tree_words EMBED_UNUSED = {
    .block = cli_trust_block,
    .prettier_block = cli_trust_prettier_block,
    .git_words = cli_trust_git_words,
};

typedef struct
{
    int (*names_a_hook)(const CliTrustCfg *args);
} CliTrustNs;
EMBED_TABLE_LAYOUT(CliTrustNs, names_a_hook);
EMBED_TABLE_STORAGE CliTrustNs cli_trust_table EMBED_UNUSED = {
    .names_a_hook = cli_trust_names_a_hook,
};

typedef struct
{
    int (*under)(const CliTrustFolderInnerCfg *args);
} CliTrustFolderInnerNs;
EMBED_TABLE_LAYOUT(CliTrustFolderInnerNs, under);
EMBED_TABLE_STORAGE CliTrustFolderInnerNs cli_trust_folder_inner EMBED_UNUSED = {
    .under = cli_trust_under,
};

#endif
