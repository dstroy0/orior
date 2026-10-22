// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose_lines.h: what the reading of a file's first comment shares: a span of its text, trimmed and matched, a
// license's comment known, and a comment's lines with their marks taken off
#ifndef PURPOSE_LINES_H
#define PURPOSE_LINES_H

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
CliSpan cli_purpose_trimmed_start(CliSpan span);
CliSpan cli_purpose_trimmed(CliSpan span);
int cli_purpose_starts(CliSpan span, const char *text);
int cli_purpose_licensed(const char *text, unsigned long long length);
void cli_purpose_uncommented(const CliSpan *lines, unsigned int count, const char *mark, CliLine *out);

#endif
