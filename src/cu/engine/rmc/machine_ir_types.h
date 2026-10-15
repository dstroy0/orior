// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// machine_ir_types.h: the machine IR's types (machine_ir.h includes the parts in order)
#ifndef MACHINE_IR_TYPES_H
#define MACHINE_IR_TYPES_H

// The register lane's forms and banks, the structures the core decides in, the arguments it names, the forms it
// writes into a lane's sink, and what it reads of the program's step table (codegen_core.h)

#include "target.h"

#include <stddef.h>

#if defined(__CUDACC__)
#define CODEGEN_CORE __host__ __device__ static inline
#else
#define CODEGEN_CORE static inline
#endif
#include "schema_forms.h"

// clang-format off
#define REGCLASSES(bank_)                                                                                              \
    bank_(FILE, "file") bank_(SIGN, "sign") bank_(OUT, "out") bank_(ATOM, "atom") bank_(TEMPORARY, "temporary")        \
        bank_(WIDE, "wide") bank_(PREDICATE, "predicate") bank_(MEMBER, "member") bank_(IMMEDIATE, "immediate")

// every register the lane holds throughout that the code generator passes to a form by name
#define PHYSREGS(fixed_)                                                                                               \
    fixed_(ZERO, "zero") fixed_(LANE_NUMBER, "lane_number") fixed_(RECORD, "record") fixed_(INDEX, "index")            \
        fixed_(BODY, "body") fixed_(BODIES, "bodies") fixed_(TABLES, "tables") fixed_(THREADS, "threads")              \
            fixed_(SIGN_BASE, "sign_base") fixed_(INDEXED, "indexed") fixed_(ONE, "one") fixed_(OK, "ok")              \
                fixed_(LAUNCH, "launch") fixed_(CARRY, "carry")

#define OPCODE_NAMED(name_, text_, parameters_, kind_) OPCODE_##name_,
#define REGCLASS_NAMED(name_, text_) REGCLASS_##name_,
#define PHYSREG_NAMED(name_, text_) PHYSREG_##name_,

enum Opcode
{
    OPCODES(OPCODE_NAMED) OPCODE_COUNT
};

enum RegisterClass
{
    REGCLASSES(REGCLASS_NAMED) REGCLASS_COUNT
};

enum PhysicalRegister
{
    PHYSREGS(PHYSREG_NAMED) PHYSREG_COUNT
};

// the parameters each form takes, by its place in the schema
#define OPCODE_PARAMETERS(name_, text_, parameters_, kind_)                                                                   \
    case OPCODE_##name_:                                                                                               \
        return parameters_;

CODEGEN_CORE unsigned int codegen_operand_count(unsigned int form)
{
    switch (form)
    {
        OPCODES(OPCODE_PARAMETERS)
    default:
        return 0u;
    }
}

// the most limb products a lane unrolls a product into; a wider product is a loop over the left's limbs
#define CODEGEN_PRODUCT_MAX 1024u

// the state an errored lane goes to, which sends it on to its error's states, and the state the lane's opening begins
// in; state 0 is the language's own, where the lane waits to begin
#define SCHEDULE_STATE_DISPATCH 1u
#define SCHEDULE_STATE_FIRST 2u

// the most arguments a form a step decides takes. The program's note and its resident take more, every one of them the
// target's or the launch's layout, and are written from those (codegen_unit)
#define MACHINE_INSTR_OPERANDS 4u

// the resident's parameters (program_unit)
#define PROGRAM_UNIT_PARAMETERS 25u

// a loop no state has begun, in the schedule
#define CODEGEN_UNBEGUN 0xFFFFFFFFu

// what an argument is: a register of a bank by its number (the bank immediate a word's value), a register the lane
// holds throughout, a count or an offset in decimal, or a small signed number in decimal
enum MachineOperandKind
{
    OPERAND_REGISTER = 1,
    OPERAND_PHYSREG = 2,
    OPERAND_NUMBER = 3,
    OPERAND_SIGNED = 4
};

// an argument: its kind, the bank or the fixed register it names, and its number, or its value for a number (a signed
// number's two's complement)
struct MachineOperand
{
    unsigned int kind;
    unsigned int which;
    unsigned int number;
};

// a form decided: its place in the schema, how many arguments it takes, its arguments, and where the scratch its
// construct takes begins in each of the temporaries, the 64-bit temporaries and the predicates, where the ruleset gives
// it as a construct. The program's note holds its step count alone and its resident none
struct MachineInstr
{
    unsigned int form;
    unsigned int count;
    MachineOperand arguments[MACHINE_INSTR_OPERANDS];
    unsigned int scratch[3];
};

// a run of registers: element i is `first` with i added to its number below `count`, and the zero register from there
// to `width`; a register read past its limbs reads 0
struct MachineOperandRange
{
    MachineOperand first;
    unsigned int count;
    unsigned int width;
};

// what every step reads and none decides: the step table and the program's sizes; each record word's first put, and 1
// past its last (0 for a word no put writes); each atom word's first reader (the step count for a word none reads),
// where each member's words begin among the atoms' words; and for each form four words, the scratch its construct takes
// in the temporaries, the 64-bit temporaries and the predicates, and 1 where it takes scratch of a bank the lane gives
// none of (ruleset_scratch, ruleset_reader.h)
struct IrProgram
{
    const DeviceRecordStep *steps;
    unsigned int step_count;
    unsigned int members;
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX];
    unsigned int file_limbs;
    unsigned int out_limbs;
    const unsigned int *put_first;
    const unsigned int *put_last;
    const unsigned int *atom_reader;
    unsigned int atom_first[ENGINE_RECORD_MEMBERS_MAX];
    const unsigned int *scratch;
};

// a lane being decided: the program; the step being decided; the temporaries, 64-bit temporaries and predicates taken
// and the most taken; the next loop's number; 1 where the step can leave the lane errored, where it reads the tables,
// and where a form breaks the lane; one past the atom word the step last read; and the sink the forms go to, the forms
// it holds, and how many were decided, which a sink too small for them does not stop
struct MachineFunction
{
    const IrProgram *program;
    unsigned int at;
    unsigned int temps;
    unsigned int temps_max;
    unsigned int wides;
    unsigned int wides_max;
    unsigned int predicates;
    unsigned int predicates_max;
    unsigned int loops;
    unsigned int errors;
    unsigned int tables;
    unsigned int broken;
    unsigned int atom_seen;
    MachineInstr *items;
    unsigned long long capacity;
    unsigned long long count;
};

// a carry chain through a register's limbs: the form a register of one limb takes, then the first, the middle and the
// last of a longer one, each setting or reading the carry as the ruleset writes it
struct CarryChain
{
    unsigned int alone;
    unsigned int first;
    unsigned int middle;
    unsigned int last;
};

// a step as its forms read it: the step, and its register's place and its operands'
struct IrStep
{
    const DeviceRecordStep *step;
    unsigned int place;
    unsigned int left_place;
    unsigned int right_place;
};

// the lane's body being split into states, a clock each: each form's cost by its place in the schema, how much a state
// may chain and how many memory writes it may make; the state being written, the cost it has chained and the writes it
// has made, 1 where it holds a form, 1 where its last form ends it and 1 where its last form left the lane; the states,
// the most cost one state chains, and the forms that alone cost more than a state holds; each error's label, by the
// error it names (-1 the opening's), and its first state, as many as `dispatch_max`; and each loop's first state, by
// the loop's number, as many as `loop_count`
struct Schedule
{
    const unsigned int *cost;
    unsigned int budget;
    unsigned int writes_max;
    unsigned int state;
    unsigned int chained;
    unsigned int writes;
    unsigned int filled;
    unsigned int ending;
    unsigned int left;
    unsigned int maximum;
    unsigned int over;
    MachineOperand *dispatch_error;
    unsigned int *dispatch_state;
    unsigned int dispatch_count;
    unsigned int dispatch_max;
    unsigned int *loop_state;
    unsigned int loop_count;
};

#endif
