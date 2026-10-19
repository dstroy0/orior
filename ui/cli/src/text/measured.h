// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measured.h: a line that measures what is written into it, keeping none of it (measured.c)
#ifndef MEASURED_H
#define MEASURED_H

#include "../command_line/cli_internal.h"

// `line` made over `piece`, which holds `piece_size`, a line whose pieces are let go as it fills; and the bytes
// written into it so far
void cli_measure_begun(CliLine *line, char *piece, unsigned long long piece_size);
unsigned long long cli_measured(const CliLine *line);

#endif
