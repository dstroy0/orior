// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose_lines.h: what the reading of a file's first comment shares: a span of its text, trimmed and matched, a
// license's comment known, and a comment's lines with their marks taken off
#ifndef PURPOSE_LINES_H
#define PURPOSE_LINES_H
typedef struct CliPurposeLinesCountCfg CliPurposeLinesCountCfg;
typedef struct CliPurposeLinesCfg CliPurposeLinesCfg;
typedef struct CliPurposeLinesSpanCfg CliPurposeLinesSpanCfg;

#include "embed_dispatch_layout.h"
#include "purpose.h"

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

// the span with the whitespace at its start left off, and at either end; whether a span starts with `text`; whether
// `length` bytes of a text state a copyright or a license; and the lines of `lines`, `count` of them, each with
// `mark` and the marks after it taken off, joined by newlines into `out` (purpose_lines.c)
CliSpan cli_purpose_trimmed_start(const CliPurposeLinesSpanCfg *args);
CliSpan cli_purpose_trim(const CliPurposeLinesSpanCfg *args);
int cli_purpose_starts(const CliPurposeLinesSpanCfg *args);
int cli_purpose_license(const CliPurposeLinesCfg *args);
void cli_purpose_uncommented(const CliPurposeLinesCountCfg *args);

struct CliPurposeLinesCountCfg
{
    const CliSpan *lines;
    unsigned int count;
    const char *mark;
    VerbaLinea *out;
};

struct CliPurposeLinesCfg
{
    const char *text;
    unsigned long long length;
};

struct CliPurposeLinesSpanCfg
{
    CliSpan span;
    const char *text;
};

typedef struct
{
    CliSpan (*purpose_trimmed_start)(const CliPurposeLinesSpanCfg *args);
    CliSpan (*purpose_trim)(const CliPurposeLinesSpanCfg *args);
    int (*purpose_starts)(const CliPurposeLinesSpanCfg *args);
} CliPurposeLinesSpanNs;
EMBED_TABLE_LAYOUT(CliPurposeLinesSpanNs, purpose_trimmed_start, purpose_trim, purpose_starts);
EMBED_TABLE_STORAGE CliPurposeLinesSpanNs cli_purpose_lines_span EMBED_UNUSED = {
    .purpose_trimmed_start = cli_purpose_trimmed_start,
    .purpose_trim = cli_purpose_trim,
    .purpose_starts = cli_purpose_starts,
};

typedef struct
{
    int (*purpose_license)(const CliPurposeLinesCfg *args);
} CliPurposeLinesNs;
EMBED_TABLE_LAYOUT(CliPurposeLinesNs, purpose_license);
EMBED_TABLE_STORAGE CliPurposeLinesNs cli_purpose_lines EMBED_UNUSED = {
    .purpose_license = cli_purpose_license,
};

typedef struct
{
    void (*purpose_uncommented)(const CliPurposeLinesCountCfg *args);
} CliPurposeLinesCountNs;
EMBED_TABLE_LAYOUT(CliPurposeLinesCountNs, purpose_uncommented);
EMBED_TABLE_STORAGE CliPurposeLinesCountNs cli_purpose_lines_count EMBED_UNUSED = {
    .purpose_uncommented = cli_purpose_uncommented,
};

#endif
