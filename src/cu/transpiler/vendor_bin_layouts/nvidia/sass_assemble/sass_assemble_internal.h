// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sass_assemble_internal.h: what the modules of the assembler share: its includes, the guard and branch fields, the fields of each operand kind, the places, labels, holders and placed forms, and the functions one module reads from another (sass_assemble.h)
#ifndef SASS_ASSEMBLE_INTERNAL_H
#define SASS_ASSEMBLE_INTERNAL_H

#include "../sass_assemble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the guard predicate: three bits its number and the fourth the negation
#define SASS_GUARD_FIRST 12u

#define SASS_GUARD_BITS 3u

#define SASS_GUARD_NOT 15u

// a branch's distance in four-byte steps from bit 34 to bit 81: bits 32 and 33 below it are the operation's own,
// as BRA, BRA.U and BRA.DIV show, the same distance with 0, 1 and 2 there
#define SASS_BRANCH_FIRST 34u

#define SASS_BRANCH_BITS 48u

#define SASS_BRANCH_STEP 4u

// one place a value sits: its first bit, how many bits it holds, what one of them counts, and 1 where the field holds
// the value with every bit inverted
typedef struct
{
    unsigned int first;
    unsigned int bits;
    unsigned int scale;
    unsigned int inverted;
} SassField;

#define SASS_DESCRIPTOR_FIELDS (sizeof(s_descriptor_fields) / sizeof(s_descriptor_fields[0]))

#define SASS_DESCRIPTOR_SHOWN 101u

// the field's value for URZ, every bit of it set
#define SASS_DESCRIPTOR_ZERO 63ull

#define SASS_REGISTER_FIELDS (sizeof(s_register_fields) / sizeof(s_register_fields[0]))

#define SASS_PREDICATE_FIELDS (sizeof(s_predicate_fields) / sizeof(s_predicate_fields[0]))

#define SASS_IMMEDIATE_FIELDS (sizeof(s_immediate_fields) / sizeof(s_immediate_fields[0]))

// where one operand of a form was placed: the field its value sits in, and for an address the field its offset sits
// in as well
typedef struct
{
    SassField value;
    SassField offset;
    int has_offset;
    int by_text;
    // for a memory operand that reads a descriptor, the field its uniform register sits in
    SassField descriptor;
    int has_descriptor;
} SassPlace;

// the most labels one text names, and the longest a label's name is
#define SASS_LABELS 256u

#define SASS_LABEL_TOKEN 64u

// the form whose own bits the encoding holds outside what it leaves open, and that form's places through `places`.
// Of several: one that reads every field before one with a relocated label, whose field holds nothing; a label
// before a number of the same operation; then the one leaving the fewest bits open. NULL where none holds it
// the most forms one encoding is held by that the reader weighs
#define SASS_HOLDERS 32u


void sass_bits_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int bits, unsigned long long value);
unsigned long long sass_register_value(const char *text);
unsigned long long sass_constant_value(const char *text, unsigned long long *bank);
int sass_operand_value(const SassInstructionParts *parts, unsigned int place, unsigned long long address, unsigned long long target, unsigned long long *value, unsigned long long *offset);
int sass_places_find(const SassForm *form, SassPlace *places, unsigned int *unplaced);
void sass_control_safe(const SassMachine *machine, const SassForm *form, unsigned long long *high);

#endif
