// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measured.c: a line that stores nothing of what is written into it and counts all of it, so a text is measured by
// writing it once before the cell that contains it is taken at its size
#include "measured.h"

// the pieces a measuring line fills handed on and let go
static int measured_append(void *context, const char *bytes, embed_word size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return 1;
}

void cli_measure_begin(const CliMeasuredPieceCfg *args)
{
    VerbaLinea *line = args->line;
    *line = (VerbaLinea){args->piece, args->piece_size, 0ULL, measured_append, NULL, 0ULL};
}

unsigned long long cli_measure(const CliMeasuredCfg *args)
{
    return args->line_read->flushed + args->line_read->at;
}
