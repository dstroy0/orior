// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.h: a run's record, the cfg text and every line written whole
#ifndef RECORD_H
#define RECORD_H

// A record is plain text. It opens with the cfg as it was read, then each line as the run gives it. Nothing is
// truncated; two runs that give the same lines give the same bytes.

#include "run_cfg.h"
#include "sim_rational.h"
#include "verba_scribo/verba_linea.h"

#include <stdio.h>

// the record at the cfg's member `member`, a path from the directory the cfg is in, opened and the cfg written into
// it: the file, or NULL where the member is missing or the file does not open
FILE *record_open(const char *cfg_path, const RunCfg *cfg, const char *member);

// the room the text of a rational takes at max: two parts, each a sign, "0x" and each limb in eight hex digits, then
// the brackets, the slash and the NUL
#define RECORD_RATIONAL_ROOM ((2u * (3u + (8u * (unsigned int)ORIOR_EXACT_LIMBS))) + 4u)

// `value` as text after the bytes of `line`: p/q in decimal, p alone where q is 1. Where a part passes a word, both
// parts' limbs in hex from the highest, in brackets. Two values share a text only where they are equal
void record_rational(VerbaLinea *line, SimRational value);

// the bytes a line of the record grows to at max
#define RECORD_LINE_MAX (1ULL << 26u)

// a line of the record, a text of the temporary tier, begun empty, its writes through `line->line`
void record_line_begun(PlatformText *line);

// the line written to `file` as one line of the record, and its cell given back. A line that passed RECORD_LINE_MAX
// is no line of the record, and record_close then gives 0
void record_line_ended(FILE *file, PlatformText *line);

// one line of text as it is given
void record_text(FILE *file, const char *text);

// the record closed: 1 where every byte and every line was written
int record_close(FILE *file);

// 1 where a value in this module outgrew the build's width
int record_short(void);

#endif
