// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sass_machine_internal.h: what the modules of the machine file's reader and writer share: its includes, the line ending, the names of the operand kinds and marks, and the functions one module reads from another (sass_machine.h)
#ifndef SASS_MACHINE_INTERNAL_H
#define SASS_MACHINE_INTERNAL_H

#include "../sass_machine.h"

#include "../../../../includes/file_defs/ksc/ksc.h"
#include "../../../../includes/file_defs/qry/writer/qry_member.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the end of a line of a machine file as the system writes a text file's: the file is handed to the run's writer as
// bytes, which it writes as they are
#if defined(_WIN32)

#define SASS_LINE_END "\r\n"

#else

#define SASS_LINE_END "\n"

#endif

// a kind as a machine file writes it, by its place in SassOperandKind, and a mark by its place in SassOperandMark
static const char *const s_kind_names[] = {"unknown",  "register", "predicate", "immediate", "constant",
                                           "address",  "label",    "uniform",   "system"};

static const char *const s_mark_names[] = {"", "-", "~", "!"};

#define SASS_KIND_COUNT (sizeof(s_kind_names) / sizeof(s_kind_names[0]))

#define SASS_MARK_COUNT (sizeof(s_mark_names) / sizeof(s_mark_names[0]))

// 1 where `letter` stands between one token of an instruction and the next. A listing lays its instructions out with
// spaces and a ruleset with tabs, and the two say the same thing: sass.krs writes "\tIADD3 \t{to}, {left}, ..." and
// nvdisasm prints "        IADD3 R8, R0, R1, RZ ;", and this reader takes both
#define SASS_SPACES " \t"


int sass_form_same(const SassForm *form, const SassInstructionParts *parts);

#endif
