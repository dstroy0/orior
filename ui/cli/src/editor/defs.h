// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// defs.h: the definitions the editor reads its languages from, as the tree holds them (defs.c).
//
// Each file type's oracle table under src/cu/includes/file_defs, the head every k-file opens with, and the language
// tables under src/lng. The editor builds its highlighting, its hover and its completion from these and holds no word
// of its own. A form added to an oracle table is a form the editor knows the next time it opens
#ifndef DEFS_H
#define DEFS_H

#include "../command_line/cli_internal.h"

// the cells the definitions are read through, counted, each given back before cli_defs_read returns
extern CliCellsTally g_cli_defs_cells;

// the tree's definitions written into `out` as the window reads them: `{"types": [{"ext", "table": {"name", "path",
// "columns", "rows"}, "header"}...], "head": {"bytes", "magic", "version"} or null, "languages": [table...]}`
void cli_defs_read(const char *root, CliLine *out);

#endif
