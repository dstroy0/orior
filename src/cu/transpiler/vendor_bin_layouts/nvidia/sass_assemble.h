// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// One line of SASS turned into the sixteen bytes the part runs
#ifndef SASS_ASSEMBLE_H
#define SASS_ASSEMBLE_H

// The assembler takes the encoding of the form of the instruction (sass_machine.h) as its base. It writes the own
// operands of the instruction into it. The assembler does not assume where each operand sits, because the base carries
// its own operands. Each operand gets the first field of its type whose value in the base is the value that the base
// printed there. A form whose operands do not all go in that way assembles nothing, and reports that. So does an
// operand that the assembler cannot make a number of, which must then be the one that the base carries.
//
// The fields are the ones that the probes of the cell found. They turned each of the 128 bits of an operation over,
// and decoded it. The fields are:
//
// - a guard predicate at 12 to 15
// - registers at 16 to 23, 24 to 31, 32 to 39 and 64 to 71
// - an immediate at 32 to 63, and a second at 72 to 79
// - predicates at 81 to 83, 84 to 86 and 87 to 89
// - the offset of a constant, and of an address, at 40 to 63
// - the own bits of the scheduler at 105 to 127

#include "sass_machine.h"

#include <stddef.h>

// what the assembler does with the scheduler's bits, which no listing prints and no probe read
enum SassControl
{
    // the own bits of the base, for an instruction that the call writes back as it read it
    SASS_CONTROL_BASE = 0,
    // every instruction stalls until the soonest read of the result of its operation. Where there is no measured count,
    // it stalls the longest that the field allows. It waits on every barrier, and one whose result comes back late sets
    // a barrier. That order is always in order
    SASS_CONTROL_SAFE = 1
};

// `text` assembled into `low` and `high` against `machine`. `address` is where the instruction lies in its section,
// which a branch counts its target from. `target` is the address of that target, and the call reads it only where the
// instruction takes a label. The return is 1, or 0 with the reason printed
int sass_assemble(const SassMachine *machine, const char *text, unsigned long long address, unsigned long long target,
                  unsigned int control, unsigned long long *low, unsigned long long *high);

// the count of instructions `text` contains, one a line, and each assembled into `code` at sixteen bytes apiece, labels
// resolved from the lines that name them. 0 where a line did not assemble, with the reason printed; `code` contains
// `room` bytes
unsigned int sass_assemble_lines(const SassMachine *machine, const char *text, unsigned int control,
                                 unsigned char *code, unsigned long long room);

// The read back: the assembler run the other way through the same forms and fields. The form that matches an encoding
// is the one whose bits are its own. That excludes the fields of its operands, its guard and the bits of the scheduler.
// Of several, it is the one with the fewest bits left to its operands. Its text is the guard, the operation and each
// operand, read from the field that the assembler writes it to. A label reads as the address that it lands on, and
// `address` is where the encoding lies.
//
// The return is 1, or 0 where no form matches it. Nothing here changes an encoding. The call reads an emitted
// instruction back, and never writes it again or optimizes it. Code written to run in constant time runs as written.
int sass_encoding_read(const SassMachine *machine, unsigned long long low, unsigned long long high,
                       unsigned long long address, char *text, size_t room);

// Where each form of `machine` puts its operands, and the bits that it leaves open. The call finds them once, and
// stores them in the machine for every later read of an encoding. Without them, each read finds them again for every
// form at every encoding. They belong to the forms and to no encoding, and a read answers the same either way. The
// return is 1, or 0 where the call could not store them
int sass_encoding_places_built(SassMachine *machine);

// A loop is an address that grows until it comes back where it began. The walk tests whether one encoding is the
// instruction that takes a loop back. Its guard must be the flag that alone steers the loop, and it branches only where
// the flag is true. One of its label or immediate operands, added to the address after it and limited to the own width
// of the field, must land on `target`. Its first operand must write no register that the loop uses, and not the flag.
//
// The walk is a bumper that a caller turns on or off, and can stop at any step. It never changes the encoding. With
// the walk off, what the compiler emitted is what runs.
typedef struct
{
    unsigned long long address;
    unsigned long long target;
    unsigned int flag;
    const unsigned int *live;
    unsigned int lives;
} SassLoopWalk;

// 1 where the encoding comes back to the target on the flag alone and writes nothing the loop uses. The step it
// stopped at through `step`: 0 no form matches it, 1 its guard, 2 where it lands, 3 what it writes, and 4 where it
// came through every step
int sass_loop_walk(const SassMachine *machine, const SassLoopWalk *walk, unsigned long long low, unsigned long long high,
                   unsigned int *step);

#endif
