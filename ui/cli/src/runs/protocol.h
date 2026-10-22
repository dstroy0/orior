// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// protocol.h: the query's face on the command line: the programs the query needs found through the toolchains,
// every command of the query a job of the Protocol menu (catalog.h), and the words of `orior_cli protocol` run by the
// query.
//
// Nothing here holds a list of languages or parts. Each is the files of the bridge's folder that name it: a language
// its map to the bridge, .klm, and its ruleset, .krs; a part its machine file, .khw, its record, .kqr, and its costs,
// .ksc. The query reads them the same way (query_jobs.c)
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "../command_line/cli_internal.h"

// the programs as the toolchains find them: the gcc or the llvm toolchain's compilers, whichever is chosen, the cuda
// toolchain's nvcc and the programs beside it, the msvc toolchain's cl, the python toolchain's program; the PATH alone
// where no toolchain is chosen. A program not found is said as the toolchains say it: the tool's name, and that File,
// Toolchains opens its install page or takes its folder
QueryPrograms cli_protocol_programs(void);

// `orior_cli protocol <words>`, `count` of them, run on the tree at `root`, each line said through `sink`: the code to exit
// with, or -1 with why the words could not be run said through `sink`
int cli_protocol_run(const char *root, const char *const *words, unsigned int count, QuerySink sink);

#endif
