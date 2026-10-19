// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// probe_ask_internal.h: what the modules of the probe's asks share: its includes, the safe image, what is preferred and asked, the loop's body, label, bounds and counts, and the functions one module reads from another (interface_sass_probe.h)
#ifndef PROBE_ASK_INTERNAL_H
#define PROBE_ASK_INTERNAL_H

#include "../interface_sass_probe.h"
#include "../../cubin_safe.h"
#include "../../sass_assemble.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a cubin read for cubin_safe takes
#define SASS_SAFE_CUBIN 1048576u

// The count a coding is turned over to lift its cost out of the harness. A run costs about 497 us of
// launch and copy whatever the kernel holds, and one instruction costs under a nanosecond. A coding is asked for
// once and turned this many times: the difference between two codings is then the difference between two costs,
// multiplied, and the harness is the same under both. The turns are raised until the loop is the
// run, and each coding is timed SASS_PREFER_TAKES times and read at its least, since a run can only be lengthened
// by what else the host is doing. A difference under the spread of a coding's own takes is no reading
#define SASS_PREFER_TURNS 400000u

#define SASS_PREFER_RUNS 20u

#define SASS_PREFER_TAKES 3u

// one tick of the part's clock in nanoseconds, which it named itself: 1770000 kHz (interface_ptx_probe clocks)
#define SASS_PREFER_TICK (1000.0 / 1770000.0 * 1000.0)

// The loop each candidate is asked in, written in place of form_0's IADD3. R7 counts the turns up by R2 and R0 counts
// the case's first word down by R3, both with word_add, and test_word_nonzero sets the flag P0 from R0 (sass.krs). R8
// and R9 hold an address no code lies at, so that a candidate jumping through them faults in place of starting the
// kernel over. The candidate is the line after the body: one that comes back to the label on the flag answers the
// case's first word, and one that falls through answers 1
#define SASS_LOOP_BODY                                                                                                 \
    "IMAD.MOV.U32 R7, RZ, RZ, RZ\n"                                                                                    \
    "IMAD.MOV.U32 R2, RZ, RZ, 0x1\n"                                                                                   \
    "IMAD.MOV.U32 R3, RZ, RZ, 4294967295\n"                                                                            \
    "IMAD.MOV.U32 R8, RZ, RZ, 0x7ffffff0\n"                                                                            \
    "IMAD.MOV.U32 R9, RZ, RZ, 0x7ffffff0\n"                                                                            \
    ".L_loop0:\n"                                                                                                      \
    "IADD3 R7, R7, R2, RZ\n"                                                                                           \
    "IADD3 R0, R0, R3, RZ\n"                                                                                           \
    "ISETP.NE.U32.AND P0, PT, R0, RZ, PT\n"

// the candidate's label operand, and its number operand: the distance back to the label from the instruction after the
// candidate, over the loop's three lines and the candidate's own at sixteen bytes apiece
#define SASS_LOOP_LABEL "`(.L_loop0)"

#define SASS_LOOP_BACK "-0x40"

// where the candidate and the label lie in form_0's section, five lines of the body past the sixteen of the kernel
// that come before its IADD3. A branch counts from the instruction after it, and only the distance between the two
// reaches the encoding
#define SASS_LOOP_AT 0x180ull

#define SASS_LOOP_TARGET 0x150ull

// the flag, and the registers the loop keeps, which a candidate that comes back leaves as they were (sass_loop_walk):
// R0 the count down, R2 and R3 its steps, R4 and R5 the answer's address and R7 the count up
#define SASS_LOOP_FLAG 0u

static const unsigned int s_loop_live[] = {0u, 2u, 3u, 4u, 5u, 7u};

// the first words the loop is asked over. 2 is asked first, since coming back answers 2 and falling through 1, and
// it prunes the most; the forms that answer it are asked the rest. At 1 the loop is left after one turn either way
static const unsigned int s_loop_counts[] = {2u, 1u, 3u, 5u, 0x40u};

// the first word a form that comes back on every count is timed over: enough turns that a turn's cost stands well
// above the launch each run pays
#define SASS_LOOP_TIMED 0x100000u

// the most forms kept as coming back
#define SASS_LOOP_KEPT 64u


int sass_cubin_answer_first(SassProbe *probe, const char *path, unsigned int first, char *answered, size_t room);
void sass_environment(const char *name, const char *value);
double sass_cubin_nanoseconds(void);
int sass_ask_text(const char *was, const char *instruction, char *text, size_t room);
void sass_ask_named(const char *instruction, char *named, size_t room);

#endif
