// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// probe_query_internal.h: what the modules of the probe's queries share: its includes, the safe image, what is
// preferred and queried, the loop's body, label, bounds and counts, and the functions one module reads from another
// (interface_sass_probe.h)
#ifndef PROBE_QUERY_INTERNAL_H
#define PROBE_QUERY_INTERNAL_H

#include "../../cubin_safe.h"
#include "../../sass_assemble.h"
#include "../interface_sass_probe.h"

#include <ctype.h>
#include <stdlib.h>

// the max bytes a cubin read for cubin_safe takes
#define SASS_SAFE_CUBIN 1048576u

// The count of turns of a coding that lifts its cost out of the harness. A run costs about 497 us of launch and copy,
// whatever the kernel holds, and one instruction costs under a nanosecond. The program queries a coding once and turns
// it this many times. The difference between two codings is then the difference between two costs, multiplied, and
// the harness is the same under both.
//
// The program raises the turns until the loop is the run. It times each coding SASS_PREFER_TAKES times and reads it at
// its least, because the other work of the host can only make a run longer. A difference under the spread of the own
// takes of a coding is no reading
#define SASS_PREFER_TURNS 400000u

#define SASS_PREFER_RUNS 20u

#define SASS_PREFER_TAKES 3u

// one tick of the part's clock in nanoseconds, which it named itself: 1770000 kHz (interface_ptx_probe clocks)
#define SASS_PREFER_TICK (1000.0 / 1770000.0 * 1000.0)

// The loop that tests each candidate, written in place of the IADD3 of form_0. R7 counts the turns up by R2, and R0
// counts the first word of the case down by R3, both with word_add. The form test_word_nonzero sets the flag P0 from R0
// (sass.krs). R8 and R9 hold an address where no code lies, thus a candidate that jumps through them faults, and does
// not start the kernel again. The candidate is the line after the body. One that comes back to the label on the flag
// answers the first word of the case, and one that falls through answers 1
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

// the label operand of the candidate, and its number operand. That is the distance back to the label from the
// instruction after the candidate. It covers the three lines of the loop and the own line of the candidate, at sixteen
// bytes each
#define SASS_LOOP_LABEL "`(.L_loop0)"

#define SASS_LOOP_BACK "-0x40"

// where the candidate and the label lie in the section of form_0. They are five lines of the body past the sixteen
// lines of the kernel that come before its IADD3. A branch counts from the instruction after it, and only the distance
// between the two reaches the encoding
#define SASS_LOOP_AT 0x180ULL

#define SASS_LOOP_TARGET 0x150ULL

// the flag, and the registers that the loop keeps, which a candidate that comes back leaves as they were
// (sass_loop_walk). R0 is the count down, R2 and R3 its steps, R4 and R5 the address of the answer, and R7 the count
// up
#define SASS_LOOP_FLAG 0u

static const unsigned int s_loop_live[] = {0u, 2u, 3u, 4u, 5u, 7u};

// the first words that the program tests the loop over. It tests 2 first, because a candidate that comes back answers
// 2 and one that falls through answers 1, and that prunes the most. It tests the forms that answer it on the rest. At
// 1, the loop ends after one turn in both cases
static const unsigned int s_loop_counts[] = {2u, 1u, 3u, 5u, 0x40u};

// the first word that the program times a form over, where the form comes back on every count. It gives enough turns
// that the cost of a turn is well above the launch that each run pays
#define SASS_LOOP_TIMED 0x100000u

// the max forms that the program keeps as ones that come back
#define SASS_LOOPS_MAX 64u

int sass_cubin_answer_first(SassProbe *probe, const char *path, unsigned int first, char *answered, size_t room);
void sass_environment(const char *name, const char *value);
double sass_cubin_nanoseconds(void);
int sass_query_text(const char *was, const char *instruction, char *text, size_t room);
void sass_query_named(const char *instruction, char *named, size_t room);

#endif
