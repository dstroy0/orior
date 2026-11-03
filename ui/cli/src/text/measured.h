// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measured.h: a line that measures what is written into it, storing none of it (measured.c)
#ifndef MEASURED_H
#define MEASURED_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliMeasuredPieceCfg CliMeasuredPieceCfg;
typedef struct CliMeasuredCfg CliMeasuredCfg;

// `line` made over `piece`, sized for `piece_size`, a line whose pieces are let go as it fills; and the bytes
// written into it so far
void cli_measure_begin(const CliMeasuredPieceCfg *args);
unsigned long long cli_measure(const CliMeasuredCfg *args);

struct CliMeasuredPieceCfg
{
    VerbaLinea *line;
    char *piece;
    unsigned long long piece_size;
};

struct CliMeasuredCfg
{
    const VerbaLinea *line_read;
};

typedef struct
{
    void (*measure_begin)(const CliMeasuredPieceCfg *args);
} CliMeasuredPieceNs;
EMBED_TABLE_LAYOUT(CliMeasuredPieceNs, measure_begin);
EMBED_TABLE_STORAGE CliMeasuredPieceNs cli_measured_piece EMBED_UNUSED = {
    .measure_begin = cli_measure_begin,
};

typedef struct
{
    unsigned long long (*measure)(const CliMeasuredCfg *args);
} CliMeasuredNs;
EMBED_TABLE_LAYOUT(CliMeasuredNs, measure);
EMBED_TABLE_STORAGE CliMeasuredNs cli_measured_table EMBED_UNUSED = {
    .measure = cli_measure,
};

#endif
