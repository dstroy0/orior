// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_compare.c: two texts compared by structure: the lines changed, moved and reshaped, and the tokens of each
// change changed or renamed, written as the window reads them
#include "../command_line/pool.h"
#include "structure_internal.h"

#include <stdlib.h>

// the one compare's state
CliStructureState *g_cli_structure;

typedef struct
{
    CliCompare *compare;
    unsigned int steps;
    const CliSide *side;
    unsigned int line;
    const unsigned char *types;
    unsigned int count;
    unsigned int run_out;
    unsigned int run_in;
} StructureCompareCtx;
