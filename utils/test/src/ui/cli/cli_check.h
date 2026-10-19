// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_check.h: the checks of orior's C side: each said as it is made, `ok` or `FAIL` and what it holds, and their
// count and failures said last as the harness reads them, `N checks, M failed` (cli_check.c)
#ifndef CLI_CHECK_H
#define CLI_CHECK_H

#include "../../../../../ui/cli/src/builds/builds.h"
#include "../../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../../ui/cli/src/text/json_tree.h"

// a check made: whether it held, and what it holds
void cli_check(int held, const char *what);
// a check that `size` bytes of `text` are `wanted`, said with what it holds and, where it failed, what came
void cli_check_text(const char *text, unsigned long long size, const char *wanted, const char *what);
// the folder `name` of the run's folder, removed whole and made anew, its full path into `full`, which holds
// CLI_PATH_LONGEST
void cli_check_folder(const char *name, char *full);
// `size` bytes of JSON read into the tree the checks share, in place of what it held: the tree, its root into `root`,
// CLI_JSON_NONE where it is no JSON
const CliJson *cli_check_parsed(const char *text, unsigned long long size, unsigned int *root);
// a string member of a node of the shared tree, the empty text where it has none
const char *cli_check_member_text(unsigned int node, const char *name);
// the first finding of the shared tree's array `found`, a build file's diagnostics, whose code is `code` and whose
// message holds `holds`, each where it is not NULL: its node, CLI_JSON_NONE where none is
unsigned int cli_check_finding(unsigned int found, const char *code, const char *holds);
// whether one of the completion items shows `label`
int cli_check_completes(const CliCompletions *items, const char *label);
// what completes the end of `text`, an ASCII text of the build file at `path` of the tree at `root`
const CliCompletions *cli_check_build_completed(const char *root, const char *path, const char *text);
// the server the tests of `name` reach, where one runs: its host into `host`, which holds CLI_NAME_LONGEST, and its
// port into `at`, from ORIOR_<name>_ADDRESS, `host:port`, else 127.0.0.1 and `port`
void cli_check_server(const char *name, unsigned int port, char *host, unsigned int *at);
// the checks' count and their failures said: the code to exit with, 1 where one failed
int cli_checked(void);

#endif
