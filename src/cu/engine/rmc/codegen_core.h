// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CODEGEN_CORE_H
#define CODEGEN_CORE_H

// The decisions of the register lane, as one source that the host and the device both compile. That is
// engine_table.md item 11(a), the code generator on the device. For each step, it decides which forms of the ruleset
// of the language the step goes in, and with which arguments. It decides the declarations, opening, close and resident
// of the lane, and the schedule of its body into states. It writes nothing.
//
// An argument is a register of a bank by its number, a register that the lane uses throughout, or a number. The forms
// that it decides are the same on any language. The ruleset writes them: code_generator.cu on the host, and the
// assembly printer on the device (asm_printer.h).
//
// The code arranges what a step decides from outside itself before the first step decides. That is the first and last
// put of each record word, the first reader of each atom word, and the start loop of each step. Then the
// steps decide apart, and in any order.
//
// A step writes its forms into a sink, or counts them where the sink has room for none. A device decides a lane in two
// passes, a count and a write, with one thread for each step. The own forms of the lane come after every step, from
// what the steps left. The scratch that a construct takes goes on from where the last step left each bank.
//
// The host and the device write the text of one lane from this header, form for form and argument for argument.
// The check of engine_table.md item 11(a) puts the two against each other.

#include "instruction_selection.h"
#include "lowering.h"
#include "machine_ir.h"
#include "prologue_epilogue.h"
#include "scheduling.h"

#endif
