// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_safe_internal.h: what the modules of the gate share: its includes, the stall and wait shifts, the bounds of the sections and the register fields, the held and admitted encodings, and the functions one module reads from another (cubin_safe.h)
#ifndef CUBIN_SAFE_INTERNAL_H
#define CUBIN_SAFE_INTERNAL_H

#include "../cubin_safe.h"

#include "../cubin_write.h"
#include "../sass_assemble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the four bits of the stall and the six of the wait, counted from the high word's first bit
#define CUBIN_STALL_SHIFT (SASS_STALL_FIRST - 64u)

#define CUBIN_WAIT_SHIFT (SASS_WAIT_FIRST - 64u)

// the most code sections one image is read for: a kernel and the functions it calls
#define CUBIN_SAFE_SECTIONS 64u

// where a register sits in an instruction, read whole however much of it a run covered: the first bit of each of the
// four fixed eight-bit register fields (sass_assemble.h), the width of one, and RZ, the zero register every kernel
// holds whatever count it allots
#define CUBIN_REGISTER_FIELDS 4u

#define CUBIN_REGISTER_WIDTH 8u

#define CUBIN_REGISTER_ZERO 255u

// the vendor's verdict, the held list the cross writes: the longest line read, the room a first grow gives, and the
// bits kept of the high word, the scheduler's own left out since whatever reaches the part sets them. The list holds
// as many encodings as the file names: it is sized to its input, with no ceiling of its own, since a ceiling short of
// the file would hold the rest off nothing and let it reach the part
#define CUBIN_SAFE_HELD_LINE 1024u

#define CUBIN_SAFE_HELD_FIRST 1024u

#define CUBIN_SAFE_SCHED_KEPT_HIGH 0x000001ffffffffffull

// the vendor's instruction set rule 13 reads, read by cubin_safe_tables_read (constant.c)
const struct TablesSetTag *cubin_safe_tables(void);

// 1 where the class the vendor's instruction set reads the encoding `low` and `high` as is named for an operation
// that transfers control or waits, EXIT apart, whatever name the machine file's form gives it (rules 2 and 3, class.c)
int cubin_class_control_or_wait(const struct TablesSetTag *tables, unsigned long long low, unsigned long long high);

// 1 where the class the vendor's instruction set reads the encoding `low` and `high` as names it illegal by one of
// its conditions on the registers, out of range or misaligned for its width: the vendor's own pipe, read before the
// part, apart from what the part answers (rule 14, class.c)
int cubin_class_illegal(const struct TablesSetTag *tables, unsigned long long low, unsigned long long high);



#endif
