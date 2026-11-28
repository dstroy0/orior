// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CODEGEN_DEVICE_H
#define CODEGEN_DEVICE_H

// The code generator on the device (engine_table.md item 11(a)). The device writes the lane of a program from its step
// table, and the host writes no text. The device does these steps:
//
// - It arranges what each step reads from outside itself.
// - It decides the forms of each step, one thread to a step (codegen_core.h). It counts them first, then writes them
//   where a scan of the counts puts them.
// - It decides the own forms of the lane in one thread.
// - It arranges the forms as the records of the assembly printer (asm_printer.h).
// - It runs the text program on the record machine, one lane to a byte, and gathers the bytes that are not 0 into the
//   text.
//
// The host arranges once what belongs to the ruleset, the target and the header alone (asm_printer_ruleset_build). It
// reads back the counts that size the memory of the device and the text. The device also splits a lane that a language
// clocks into states (codegen_device_instrs), in one thread, because each state goes on from the state before. The
// text of the device is the text of the host (CodeGenerator::decided, written by asm_printer_host) byte for byte. The
// forms come from the core on both, and the assembly printer writes what the text of the ruleset would.
//
// The device also arranges the step table itself, from the steps of the program. It does the keymath encoding
// (keymath_core.h) and the key_schedule layout (key_schedule_core.h), each in one thread, because each step reads the
// steps before it. This is the same code that keymath_record_encode and key_schedule_record_layout run on the host. The
// device also arranges the assembly printer, from what asm_printer_ruleset_build kept of it. It checks the result
// against the layout of the host word for word before the record machine loads it

#include "asm_printer.h"
#include "code_generator.h"

// a program as keymath and key_schedule take it (KeymathRecordRequest, KeyScheduleRecordRequest). It has its steps,
// the widths and offsets of the fields, its members and the limbs that each reads, and its outputs and tables. It also
// tells whether the code uses a register again
struct LayoutRequest
{
    const EngineRecordStep *steps;
    unsigned int count;
    const unsigned int *field_bits;
    const unsigned int *field_offset;
    unsigned int fields;
    unsigned int members;
    const unsigned int *in_limbs;
    const unsigned int *outputs;
    unsigned int output_count;
    const EngineRecordTable *tables;
    unsigned int table_count;
    int reuse;
};

// the lane of `layout`, written by the device in the ruleset that `text_rules` came from. `places` is the places that
// it uses in shared memory: those of the file, where the language puts it there, else 0. Where the caller gives
// `costs`, the device splits the lane into states by it. Those are the CodeGenerator::program_schedule_costs of the
// language, where its program() splits the lane. The return is 1 where the device wrote the lane, with its text on
// `text`. It is 0, with the reason on `error`, where:
//
// - a step is one that the lane does not support
// - a form breaks the lane
// - the text is more than the assembly printer supports
// - a call on the device failed
// - the build has no device
int codegen_device(const EngineRecordLayout *layout, const AsmPrinterRuleset *text_rules, unsigned int places,
                   const ScheduleCosts *costs, PlatformText *text, VerbaLinea *error);

// the program of `request`, arranged by the device and read back into `layout`, as key_schedule_record_layout arranges
// it and key_schedule_record_release releases it. The return is 1 where the device arranged it. It is 0, with the
// reason on `error`, where keymath or key_schedule fails on it, a device call failed, or the build has no device
int layout_device(const LayoutRequest *request, EngineRecordLayout *layout, VerbaLinea *error);

// the lane of the program of `request`, written by the device from the step table that the device arranged. The host
// reads none of it back. The device arranges it as layout_device does, and writes it as codegen_device does
int codegen_device_steps(const LayoutRequest *request, const AsmPrinterRuleset *text_rules, unsigned int places,
                         const ScheduleCosts *costs, PlatformText *text, VerbaLinea *error);

// The forms of the lane of `layout` as the device decides them, in the order of the text. Each construct takes its
// scratch from `scratch`, `scratch_count` words (ruleset_scratch). The call reads them back into `items`, an array of
// MachineInstr that the caller began. Where `costs` is NULL, they are the forms that CodeGenerator::decided gives.
// Else the call splits the lane into states by `costs`, tells `report` how, and gives the forms as the scheduled
// decided gives them.
//
// The return is 1 where the device decided them. It is 0, with the reason on `error`, where a step is one that the lane
// does not support or a form breaks the lane. It is also 0 where a device call failed or the build has no device
int codegen_device_instrs(const EngineRecordLayout *layout, const unsigned int *scratch,
                          unsigned long long scratch_count, unsigned int places, const ScheduleCosts *costs,
                          ScheduleReport *report, PlatformArray *items, VerbaLinea *error);

// 1 where two layouts are the same word for word: their sizes, step tables and tables' values
int layout_same(const EngineRecordLayout *left, const EngineRecordLayout *right);

// the program of `request`, arranged by the device, the path that every program takes. The call checks it word for
// word against `layout`, the layout of the host. The return is 1 where the two agree. Then the call releases the
// layout of the host, and leaves the layout of the device in `layout`. It writes the line on stderr where `report`
// requests it. Else the return is 0, with the reason on stderr, and the layout of the host stays in `layout`
int layout_device_agrees(const LayoutRequest *request, EngineRecordLayout *layout, int report);

// asm_printer_ruleset_build on the device. The host checks the schema, and the device takes the scratch
// (ruleset_scratch_device). Then one device thread does the build (asm_printer_core.h), and the device arranges the
// program (layout_device). The call does not keep the key of the program, and its output is the output of the program.
// The return is 1 where the call builds it. Else it is 0, with the reason on `error`, where the host build fails, a
// device call failed, or the build has no device
int asm_printer_ruleset_device(const Ruleset *rules, const TargetInfo *target, const char *header,
                               AsmPrinterRuleset *text_rules, VerbaLinea *error);

// 1 where two builds of a ruleset are the same word for word. That covers their scratch and lists, the steps, fields,
// tables and output of the program, and its layout
int asm_printer_ruleset_same(const AsmPrinterRuleset *left, const AsmPrinterRuleset *right);

// the ruleset at `path`, read into `rules` against rules->schema as the reader of the host reads it. The host opens the
// file, and one device thread reads its lines (ruleset_core.h). The return is 1 where the device read it. Then `rules`
// is as the reader of the host leaves it, read or in error. The return is 0, with the reason on `error`, where a call
// on the device failed or the build has no device
int ruleset_read_device(Ruleset *rules, const char *path, VerbaLinea *error);

// ruleset_scratch arranged in one device thread (ruleset_core_scratch.h), into `scratch`, four words a form. 1 where
// it was; 0, and why on `error`, where the pool cannot meet the cell, the device errored on a call or the build has
// no device. The caller gives the cell of `scratch` back
int ruleset_scratch_device(const Ruleset *rules, const unsigned int *banks, AsmPrinterWords *scratch,
                           VerbaLinea *error);

// 1 where two rulesets are the same as read: their schema, path, reason, names, banks, registers, forms and
// constructs. They must also agree on which of these the file gave, and on the construct that the reader is on, with
// its parameters
int ruleset_same(const Ruleset *left, const Ruleset *right);

#endif
