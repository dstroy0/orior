// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables.h: the programs a tree's builds make: the jobs that build them, where each is found once it is made, how
// it is signed and run, and what a reader is told of it (executables.c and the executables_*.c beside it). The build
// scripts are read from the tree's folders five deep, past the folders a build writes into or a package manager fills
// and those whose names start with a dot. A job's id is `executable/<script>:<name>`, the script by its path in the
// tree, and the program it made is read back from the id and the build's lines
#ifndef EXECUTABLES_H
#define EXECUTABLES_H

#include "../runs/runner.h"

// the program `name` of the toolchain `tool`, found where the toolchains window finds it, into `path`, which holds
// CLI_PATH_LONGEST: 1, or 0 with why said
int cli_executables_program(const char *tool, const char *name, char *path, CliLine *said);
// the C compiler and linker for the system and processor the job's params choose, each variable and its value handed
// to `each`, where they are another machine's, for a build of the kind named, "go" or "rust"
void cli_executables_cross_env(const char *kind, const CliValues *values,
                               void (*each)(void *context, const char *name, const char *value), void *context);
// whether this machine signs the programs its builds make
int cli_executables_signs(void);
// the program the job `id`'s build made, found from the lines its steps printed, into `path`, which holds
// CLI_PATH_LONGEST: 1, or 0 with why said
int cli_executables_made(const char *root, const char *id, const CliValues *values, const CliLines *lines, char *path,
                         CliLine *said);
// what a reader is told after a build: what the build's lines say of it; how the program compares with the one built
// at the tree's last commit; what a signing said; what a run of the program said; and what a build that failed said;
// each line handed to `say`
void cli_executables_built_advice(const char *root, const char *id, const CliLines *lines, QuerySink say);
void cli_executables_compare_build(const char *root, const char *id, const char *file, QuerySink say);
void cli_executables_signed_said(const char *file, QuerySink say);
void cli_executables_ran_advice(const char *root, const char *id, int has_code, long long code, const CliLines *lines,
                                QuerySink say);
void cli_executables_advice(const char *id, const CliLines *lines, QuerySink say);
// the commands that sign the program `file`, each into `commands` with its line into `shown`, which hold `room`: their
// count, or -1 with why said
int cli_executables_sign_steps(const char *file, CliCommand *commands, char (*shown)[CLI_PATH_LONGEST * 4u],
                               unsigned int room, CliLine *said);
// the command that runs the program `file` the build made, with the PATH a program opened on its own gets where
// `system`, into `command`, with its line into `shown`
void cli_executables_run_command(const char *root, const char *file, const CliValues *values, int system,
                                 CliCommand *command, CliLine *shown);

#endif
