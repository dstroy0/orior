// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the asm_printer_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef ASM_PRINTER_INTERNAL_H
#define ASM_PRINTER_INTERNAL_H

#include "../../../../includes/file_defs/krs/ruleset_flat.h"
#include "../../../../includes/file_defs/krs/ruleset_reader.h"
#include "../../engine/analysis/cycle/cycle.h"
#include "../../engine/analysis/key_schedule/key_schedule.h"
#include "../../engine/analysis/keymath/keymath.h"
#include "asm_printer.h"
#include "asm_printer_core.h"

#include <stddef.h>

// The ruleset, the target and the header as the core's build reads them, in one cell of host memory. That is every
// text's characters end to end, the texts and the forms' placeholders, and the core's view of them, which points into
// the cell
struct AsmPrinterFlat
{
    void *cell;
    AsmPrinterCoreRules rules;
    unsigned int text_count;
    unsigned int character_count;
    unsigned int placeholder_count;
};

// the ruleset, the target and the header flattened into `flat`: 1, or 0 where the pool cannot meet the cell
int asm_printer_flatten(const Ruleset *rules, const TargetInfo *target, const char *header, AsmPrinterFlat *flat);

void asm_printer_flat_ended(AsmPrinterFlat *flat);

// the memory a build arranges into, in one cell, sized from the flattened ruleset by asm_printer_memory_size, its
// steps by `step_capacity`
struct AsmPrinterMemory
{
    void *cell;
};

// the capacities a build of `flat` takes, every count but the steps' computed exactly, into `build`
void asm_printer_capacities(const AsmPrinterFlat *flat, unsigned int step_capacity, AsmPrinterCoreBuild *build);

// the lists of `build` laid out on `plan` from its capacities
void asm_printer_memory_layout(RulesetCellPlan *plan, AsmPrinterCoreBuild *build);

// `memory` sized to `build`'s capacities, and `build` pointed at it: 1, or 0 where the pool cannot meet the cell
int asm_printer_memory_size(AsmPrinterMemory *memory, AsmPrinterCoreBuild *build);

void asm_printer_memory_ended(AsmPrinterMemory *memory);

// the steps the first build is given; a build left full runs again with twice as many
#define ASM_PRINTER_STEP_CAPACITY 1024u

// What a finished build arranged, its memory in host memory, stored in `text_rules`: the lists and the program's steps,
// fields, tables and output. 1, or 0 where the pool cannot meet a list
int asm_printer_store(const AsmPrinterCoreBuild *build, AsmPrinterRuleset *text_rules);

// why a build ended as `end`, as the host's build reported it
const char *asm_printer_ended(unsigned int end);

#endif
