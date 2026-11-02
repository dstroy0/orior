// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_check.h: the checks of orior's C side: each printed as it is made, `ok` or `FAIL` and what it checks, and their
// count and failures printed last as the harness reads them, `N checks, M failed` (cli_check.c)
#ifndef CLI_CHECK_H
#define CLI_CHECK_H

#include "../../../../../ui/cli/src/builds/builds.h"
#include "../../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../../ui/cli/src/text/json_tree.h"

// the checks begun: this process put on a lane as the one command it runs, the lane's cells rewound by cli_on_check
void cli_check_begun(void);
// a check made: whether it passed, and what it checks
void cli_check(int passed, const char *what);
// a check that `size` bytes of `text` are `required`, printed with what it checks and, where it failed, what came
void cli_check_text(const char *text, unsigned long long size, const char *required, const char *what);
// the folder `name` of the run's folder, removed whole and made anew, its full path into `full`, which is sized for
// CLI_PATH_MAX_LEN
void cli_check_folder(const char *name, char *full);
// `size` bytes of JSON read into the tree the checks share, in place of what it contained: the tree, its root into
// `root`, CLI_JSON_NONE where it is no JSON
const CliJson *cli_check_parsed(const char *text, unsigned long long size, unsigned int *root);
// a string member of a node of the shared tree, the empty text where it has none
const char *cli_check_member_text(unsigned int node, const char *name);
// the first finding of the shared tree's array `found`, a build file's diagnostics, whose code is `code` and whose
// message contains `part`, each where it is not NULL: its node, CLI_JSON_NONE where none is
unsigned int cli_check_finding(unsigned int found, const char *code, const char *part);
// whether one of the completion items shows `label`
int cli_check_completes(const CliCompletions *items, const char *label);
// what completes the end of `text`, an ASCII text of the build file at `path` of the tree at `root`
const CliCompletions *cli_check_build_completed(const char *root, const char *path, const char *text);
// the server the tests of `name` reach, where one runs: its host into `host`, which is sized for CLI_NAME_MAX_LEN, and
// its port into `at`, from ORIOR_<name>_ADDRESS, `host:port`, else 127.0.0.1 and `port`
void cli_check_server(const char *name, unsigned int port, char *host, unsigned int *at);
// the checks' count and their failures printed: the code to exit with, 1 where one failed
int cli_on_check(void);

#endif
