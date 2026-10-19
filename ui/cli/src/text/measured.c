// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measured.c: a line that keeps nothing of what is written into it and counts all of it, so a text is measured by
// writing it once before the cell that holds it is taken at its size
#include "measured.h"

// the pieces a measuring line fills handed on and let go
static int measured_put(void *context, const char *bytes, unsigned long long size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return 1;
}

void cli_measure_begun(CliLine *line, char *piece, unsigned long long piece_size)
{
    *line = (CliLine){piece, piece_size, 0ull, measured_put, NULL, 0ull};
}

unsigned long long cli_measured(const CliLine *line)
{
    return line->flushed + line->at;
}
