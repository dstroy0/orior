// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// probe_machine_internal.h: what the modules of the probe's machine share: its includes, the encodings and texts held, the bounds of the carriers, and the functions one module reads from another (interface_sass_probe.h)
#ifndef PROBE_MACHINE_INTERNAL_H
#define PROBE_MACHINE_INTERNAL_H

#include "../interface_sass_probe.h"

#include "../../sass_assemble.h"
#include "../../sass_machine.h"

#include <stdio.h>
#include <string.h>

// the carriers the sweep runs from, at most. Each is a form the part ran whose operands are shaped unlike the ones
// before it, since an operation that reads an address decodes as nothing from a carrier whose operand bits held two
// registers. More carriers reach more operations and cost a batch of decodes apiece
#define SASS_CARRIERS 8u

extern unsigned long long s_low[SASS_ENCODINGS];
extern unsigned long long s_high[SASS_ENCODINGS];
extern char s_texts[SASS_ENCODINGS][SASS_TEXT];

unsigned int sass_operand_changed(const SassInstructionParts *base, const char *text);
unsigned int sass_operand_twin(const SassInstructionParts *base, const char *text, unsigned int operand);
void sass_form_turned(const SassForm *form);
int sass_form_unprinted(SassForm *form, const SassInstructionParts *base, const char *architecture, const char *folder, unsigned int number);

#endif
