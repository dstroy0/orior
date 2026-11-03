// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers.h: checkers: the type checkers and linters the reader names, each a toolchain of the manifest with a
// `checker` that specifies how it runs and how its findings read. A checker is found as any toolchain is, and needs
// nothing set up past its name.
//
// A checker that reads a file's text on its input checks the text as the editor has it, and one that reads files
// checks the file as the disk stores it. One that reads a tree whole is run over the tree from its top folder. Each
// finding is a diagnostic of the protocol's form, its source the checker's name, and a fix the checker gives, as ruff
// does, a fix the editor offers.
//
// Findings read in one of four forms: `lines`, a finding a line as `path:line:col: message`, the column, a severity
// word after it, and a code before the message or in brackets after it where the checker writes one, as flake8, mypy
// and pylint write them; and the JSON of ruff, ESLint and ShellCheck (checkers.c, checkers_read.c)
#ifndef CHECKERS_H
#define CHECKERS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliCheckersOutCfg CliCheckersOutCfg;
typedef struct CliCheckersCfg CliCheckersCfg;
typedef struct CliCheckersNameCfg CliCheckersNameCfg;

// the checker a name names, by its toolchain's id or its name in any case, its id into `id`, sized for
// CLI_NAME_MAX_LEN: 1, or 0 where none does; whether the checker `id` checks a tree whole, whether it reads a file's
// text on its input, and whether it checks `language`; and the languages of every checker, written as a JSON object of
// each checker's id and its languages
int cli_checkers_by_name(const CliCheckersNameCfg *args);
int cli_checkers_reads_tree(const CliCheckersCfg *args);
int cli_checkers_reads_input(const CliCheckersCfg *args);
int cli_checkers_checks(const CliCheckersCfg *args);

// the file at `path` of the tree at `root` checked by the checker `id`, as `length` bytes of `text` where the checker
// reads its input and `text` is given, and as the disk stores it otherwise; and the tree at `root` checked whole, where
// the checker reads a tree. Each writes the findings by file, the file's own first where it checks a file, as
// `[[path, [diagnostic]]]`: 1, or 0 with why given. A checker runs no tree whose own code is disabled
int cli_checkers_check_file(const CliCheckersOutCfg *args);
int cli_checkers_check_tree(const CliCheckersOutCfg *args);

struct CliCheckersOutCfg
{
    const char *id;
    VerbaLinea *out;
    const char *root;
    const char *path;
    const char *text;
    unsigned long long length;
    VerbaLinea *err_msg;
};

struct CliCheckersCfg
{
    const char *id;
    const char *language;
};

struct CliCheckersNameCfg
{
    const char *name;
    char *id_out;
};

typedef struct
{
    int (*by_name)(const CliCheckersNameCfg *args);
} CliCheckersNameNs;
EMBED_TABLE_LAYOUT(CliCheckersNameNs, by_name);
EMBED_TABLE_STORAGE CliCheckersNameNs cli_checkers_name EMBED_UNUSED = {
    .by_name = cli_checkers_by_name,
};

typedef struct
{
    int (*reads_tree)(const CliCheckersCfg *args);
    int (*reads_input)(const CliCheckersCfg *args);
    int (*checks)(const CliCheckersCfg *args);
} CliCheckersNs;
EMBED_TABLE_LAYOUT(CliCheckersNs, reads_tree, reads_input, checks);
EMBED_TABLE_STORAGE CliCheckersNs cli_checkers EMBED_UNUSED = {
    .reads_tree = cli_checkers_reads_tree,
    .reads_input = cli_checkers_reads_input,
    .checks = cli_checkers_checks,
};

typedef struct
{
    int (*check_file)(const CliCheckersOutCfg *args);
    int (*check_tree)(const CliCheckersOutCfg *args);
} CliCheckersOutNs;
EMBED_TABLE_LAYOUT(CliCheckersOutNs, check_file, check_tree);
EMBED_TABLE_STORAGE CliCheckersOutNs cli_checkers_out EMBED_UNUSED = {
    .check_file = cli_checkers_check_file,
    .check_tree = cli_checkers_check_tree,
};

#endif
