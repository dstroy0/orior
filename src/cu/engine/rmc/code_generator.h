// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

// The register lane, the code generator that names no language. It unrolls each step at its widths into straight-line
// text over registers that the lane uses itself. Every line is a form of the ruleset of the language. A language is the
// file of its ruleset. The file names these things:
//
// - the toolchain that builds its text, and where its header comes from
// - how many memory writes its text can make in one state (write_ports)
// - whether it uses the file and signs of the lane in the shared memory of the thread block (shared)
// - the part whose files the program reads after it (part)

#include "codegen_core.h"
#include "target.h"

#include <stddef.h>

// a budget or a port count that contains anything
#define SCHEDULE_UNBOUNDED 0xFFFFFFFFu

// the cost of one form, by the name its ruleset gives it
struct ScheduleCost
{
    const char *form;
    unsigned int cost;
};

// How the code splits a lane into states, a clock each. The model holds the cost of each form by the name its ruleset
// gives it, as the target's construction set measured it, `cost_count` of them. It holds the cost of a form it does
// not name, and how much cost a state may chain. It holds how many memory writes a state may make, which the
// language's own write ports bound as well. The budget and the writes are the refinement loop's to choose
struct ScheduleModel
{
    const ScheduleCost *cost;
    unsigned int cost_count;
    unsigned int other;
    unsigned int budget;
    unsigned int writes;
};

// a lane as the core split it: its states and the max cost that one state chains. Then the forms that alone cost more
// than a state is true
struct ScheduleReport
{
    unsigned int states;
    unsigned int max;
    unsigned int over;
};

// a schedule model as the core splits by it (Schedule). It has the cost of each form by its number, and the cost that
// a state can chain. Then the writes that the model lets a state make, and the write ports of the language
struct ScheduleCosts
{
    unsigned int cost[OPCODE_COUNT];
    unsigned int budget;
    unsigned int writes;
    unsigned int ports;
};

class CodeGenerator : public Target
{
  public:
    // the code generator of the ruleset `file`, read at its first call to ruleset()
    explicit CodeGenerator(const char *file);

    ~CodeGenerator() override;

    // a code generator that code_generator() makes lives in a cell of the pool, which `cell` is
    static void *operator new(size_t bytes, void *cell);

    // the cell given back where the construction in it failed
    static void operator delete(void *cell, void *place);

    // the cell of a code generator given back
    static void operator delete(void *cell);

    int program(const EngineRecordLayout *layout, const TargetInfo *target, const char *header, PlatformText *text,
                unsigned int *places, unsigned int *live) override;

    // the lane split into states by `model`, as program() writes it in other cases. `report` gets how the call split it
    int scheduled(const EngineRecordLayout *layout, const TargetInfo *target, const char *header, PlatformText *text,
                  unsigned int *places, unsigned int *live, const ScheduleModel &model, ScheduleReport *report);

    // The forms program() writes the lane in, as the core decides them (codegen_core.h). They go into `items`, which
    // the caller began with an element of MachineInstr, in the text's order. The header's place holds an item of the
    // form ASM_PRINTER_ALL_ONES. Also the places the lane uses in shared memory. 0 where program() writes nothing for
    // want of a form. The device decides the same (codegen_device.h)
    int decided(const EngineRecordLayout *layout, unsigned int *places, PlatformArray *items);

    // the forms that scheduled() writes the lane in, as decided() gives those of program(). `report` gets how the call
    // split it
    int decided(const EngineRecordLayout *layout, const ScheduleModel &model, ScheduleReport *report,
                unsigned int *places, PlatformArray *items);

    // `model` as the core splits by it. The call finds the cost of each form by the name that the ruleset gives it,
    // with the write ports of the language. It does so on the host and the device alike
    ScheduleCosts schedule_costs(const ScheduleModel &model) const;

    // the schedule model that program() splits the lane by, where the language always splits it into states (a clocked
    // one). NULL where program() writes the lane whole
    virtual const ScheduleModel *program_schedule_model(void) const;

    // how many registers the one register file of the language holds for the lane. There, every bank is in that file,
    // and the banks go end to end from the counts that the lane declares. They do not each have numbers from 0. A
    // language whose registers are virtual gives each bank a namespace of its own, and answers 0. Nothing writes a lane
    // whose banks run past what the file holds. The call program() refuses it, and does not write a register that the
    // language pinned to a different thing
    virtual unsigned int register_file_size(void) const;

    // Whether the language writes the program resident itself. Most do: the resident is a form of the ruleset as any
    // other is, and program() puts it after the lane. A language whose resident reaches it already built answers 0.
    // Then program() leaves the form out, and does not query the ruleset for it
    virtual int program_unit_written(void) const;

    // 1 where program() splits the lane, `costs` then its model as the core splits by it, for the device to split the
    // same
    int program_schedule_costs(ScheduleCosts *costs) const;

    // each register that the lane written last uses, one to a line. A line has the register as the ruleset writes it,
    // and what holds it. That is its bank, the name of its fixed register, or the form whose scratch it is. Then come
    // the items that claim and release it, by their place in the lane from 1 and their form (codegen_claims)
    const char *register_claims(void);

  private:
    int decide(const EngineRecordLayout *layout, const ScheduleModel *model, ScheduleReport *report,
               unsigned int *places, unsigned int *live, PlatformArray *items);

    PlatformText claim_lines;

    int lane(const EngineRecordLayout *layout, const TargetInfo *target, const char *header, PlatformText *text,
             unsigned int *places, unsigned int *live, const ScheduleModel *model, ScheduleReport *report);
};

// the code generator that a process keeps for the ruleset `file`, with its ruleset read at its first call to
// ruleset(). A ruleset that gives shared 1 arranges the file and its signs in shared memory, as the record machine
// sizes them. The places of a thread go one word each across the threads of the thread block, and the signs one byte
// each after them. The lane then uses the places of the file there, and opens with a search for its signs
CodeGenerator &code_generator(const char *file);

#endif
