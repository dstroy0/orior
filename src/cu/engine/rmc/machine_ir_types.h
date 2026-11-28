// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// machine_ir_types.h: the machine IR's types (machine_ir.h includes the parts in order)
#ifndef MACHINE_IR_TYPES_H
#define MACHINE_IR_TYPES_H

// The forms and banks of the register lane, the structures that the core decides in, and the arguments that it names.
// Then the forms that it writes into the sink of a lane, and what it reads of the step table of the program
// (codegen_core.h)

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

// every register the lane uses throughout that the code generator passes to a form by name
#define PHYSREGS(fixed_)                                                                                               \
    fixed_(ZERO, "zero") fixed_(LANE_NUMBER, "lane_number") fixed_(RECORD, "record") fixed_(INDEX, "index")            \
        fixed_(BODY, "body") fixed_(BODIES, "bodies") fixed_(TABLES, "tables") fixed_(THREADS, "threads")              \
            fixed_(SIGN_BASE, "sign_base") fixed_(INDEXED, "indexed") fixed_(ONE, "one") fixed_(OK, "ok")              \
                fixed_(LAUNCH, "launch") fixed_(CARRY, "carry")

#define OPCODE_NAMED(name_, text_, parameters_, type_) OPCODE_##name_,
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
#define OPCODE_PARAMETERS(name_, text_, parameters_, type_)                                                                   \
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

// the max limb products a lane unrolls a product into; a wider product is a loop over the left's limbs
#define CODEGEN_PRODUCT_MAX 1024u

// the state that a lane in error goes to, which sends it on to the states of its error. Then the state where the
// opening of the lane starts. State 0 belongs to the language, where the lane waits to start
#define SCHEDULE_STATE_DISPATCH 1u
#define SCHEDULE_STATE_FIRST 2u

// the max arguments of a form that a step decides. The note of the program and its resident take more, all from the
// layout of the target or of the launch. The program writes them from those (codegen_unit)
#define MACHINE_INSTR_OPERANDS 4u

// the resident's parameters (program_unit)
#define PROGRAM_UNIT_PARAMETERS 25u

// a loop that no state started, in the schedule
#define CODEGEN_UNBEGUN 0xFFFFFFFFu

// what an argument is. It is a register of a bank by its number, or a register that the lane uses throughout. The bank
// immediate is the value of a word. Else it is a count or an offset in decimal, or a small signed number in decimal
enum MachineOperandType
{
    OPERAND_REGISTER = 1,
    OPERAND_PHYSREG = 2,
    OPERAND_NUMBER = 3,
    OPERAND_SIGNED = 4
};

// an argument: its type, the bank or the fixed register that it names, and its number. For a number, it is its value,
// the two's complement of a signed number
struct MachineOperand
{
    unsigned int type;
    unsigned int which;
    unsigned int number;
};

// a form decided: its place in the schema, how many arguments it takes, and its arguments. Where the ruleset gives it
// as a construct, it also has where the scratch of its construct starts. That is in the temporaries, the 64-bit
// temporaries and the predicates. The note of the program holds its step count alone, and its resident holds none
struct MachineInstr
{
    unsigned int form;
    unsigned int count;
    MachineOperand arguments[MACHINE_INSTR_OPERANDS];
    unsigned int scratch[3];
};

// a run of registers. Below `count`, element i is `first` with i added to its number, and from there to `width` it is
// the zero register. A register read past its limbs reads 0
struct MachineOperandRange
{
    MachineOperand first;
    unsigned int count;
    unsigned int width;
};

// what every step reads and none decides:
//
// - the step table and the sizes of the program
// - the first put of each record word, and 1 past its last (0 for a word that no put writes)
// - the first reader of each atom word (the step count for a word that none reads)
// - where the words of each member start among the words of the atoms
// - four words for each form: the scratch that its construct takes in the temporaries, the 64-bit temporaries and the
//   predicates. The fourth is 1 where it takes scratch of a bank that the lane gives none of (ruleset_scratch,
//   ruleset_reader.h)
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

// a lane that the core now decides:
//
// - the program, and the step that the core now decides
// - the temporaries, 64-bit temporaries and predicates taken, and the max taken
// - the number of the next loop
// - three flags, each 1 where its case holds: the step can leave the lane in error, it reads the tables, and a form
//   breaks the lane
// - one past the atom word that the step read last
// - the sink that the forms go to, the forms that it holds, and how many the core decided. A sink too small for them
//   does not stop the count
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

// a carry chain through the limbs of a register. It has the form that a register of one limb takes, then the first,
// the middle and the last of a longer one. Each sets or reads the carry as the ruleset writes it
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

// the body of the lane, which the core now splits into states, one clock for each:
//
// - the cost of each form by its place in the schema, how much a state can chain, and how many memory writes it can
//   make
// - the state that the core now writes, the cost that it chained and the writes that it made
// - 1 where it holds a form, 1 where its last form ends it, and 1 where its last form left the lane
// - 1 from the label of the first error on, where each state belongs to an error
// - the states, the max cost that one state chains, and the forms that alone cost more than a state holds
// - the label of each error, by the error that it names (-1 for the opening), and its first state, as many as
//   `dispatch_max`
// - the first state of each loop, by the number of the loop, as many as `loop_count`
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
    unsigned int handling;
    unsigned int max;
    unsigned int over;
    MachineOperand *dispatch_error;
    unsigned int *dispatch_state;
    unsigned int dispatch_count;
    unsigned int dispatch_max;
    unsigned int *loop_state;
    unsigned int loop_count;
};

#endif
