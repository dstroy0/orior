// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef EXACT_RECORD_H
#define EXACT_RECORD_H

#include "../../../engine/engine.h"

// The exact integer's arithmetic on the device, as steps of a record program. A program is built here on the host and
// every value it names is a register the device holds for a lane: each call writes the steps of one operation and
// returns the step that holds its result. Nothing here computes a lane's value on the host; the host lays out
// constants, fields and tables.
//
// Every value is exact. A quotient rounds toward zero, and down and up give the integers below and above it whatever
// the signs, the outward pair a held value is read through. A register's width is the bound keymath finds at imprint;
// wrap and narrow give a value its own width where the caller holds the bound.

#ifdef __cplusplus
extern "C"
{
#endif

    // a program under construction: its steps, its fields and its tables, each table's values owned here
    typedef struct
    {
        EngineRecordStep *steps;
        unsigned int count;
        unsigned int step_room;
        unsigned int *field_bits;
        unsigned int *field_offset;
        unsigned int fields;
        unsigned int field_room;
        // the shared record's next free bit, where exact_record_field lays a field
        unsigned int shared_bits;
        EngineRecordTable *tables;
        unsigned int **table_values;
        unsigned int table_count;
        unsigned int table_room;
        // 1 where a step, field or table could not be held; every later call then writes nothing and returns 0
        int failed;
    } ExactRecordProgram;

    void exact_record_open(ExactRecordProgram *program);

    void exact_record_close(ExactRecordProgram *program);

    // ---- steps ----

    unsigned int exact_record_step(ExactRecordProgram *program, EngineRecordOperation operation, unsigned int left,
                                   unsigned int right, unsigned int member);

    // a constant below 2^64
    unsigned int exact_record_constant(ExactRecordProgram *program, unsigned long long value);

    // 2^bits, for any bits: one constant below 2^63, a product of constants past it
    unsigned int exact_record_power_two(ExactRecordProgram *program, unsigned int bits);

    // 2^bits - 1
    unsigned int exact_record_mask(ExactRecordProgram *program, unsigned int bits);

    // ---- fields ----

    // a field `bits` wide laid at the shared record's next bit
    unsigned int exact_record_field(ExactRecordProgram *program, unsigned int bits);

    // a field `bits` wide at bit `offset` of a member's records
    unsigned int exact_record_member_field(ExactRecordProgram *program, unsigned int bits, unsigned int offset);

    // a field of member `member` read as two's complement
    unsigned int exact_record_read(ExactRecordProgram *program, unsigned int field, unsigned int member);

    // a field of member `member` read as a magnitude, never negative
    unsigned int exact_record_read_unsigned(ExactRecordProgram *program, unsigned int field, unsigned int member);

    // the lane's own number, known below 2^bits, its register that width
    unsigned int exact_record_lane_below(ExactRecordProgram *program, unsigned int bits);

    // ---- arithmetic ----

    unsigned int exact_record_sum(ExactRecordProgram *program, unsigned int left, unsigned int right);

    unsigned int exact_record_difference(ExactRecordProgram *program, unsigned int left, unsigned int right);

    unsigned int exact_record_product(ExactRecordProgram *program, unsigned int left, unsigned int right);

    // left / right toward zero, and left - quotient right with left's sign
    unsigned int exact_record_quotient(ExactRecordProgram *program, unsigned int left, unsigned int right);

    unsigned int exact_record_remainder(ExactRecordProgram *program, unsigned int left, unsigned int right);

    // left / right where right divides left; a remainder errors on the lane
    unsigned int exact_record_exact_quotient(ExactRecordProgram *program, unsigned int left, unsigned int right);

    unsigned int exact_record_negate(ExactRecordProgram *program, unsigned int value);

    unsigned int exact_record_absolute(ExactRecordProgram *program, unsigned int value);

    // the quotient less 1 and more 1: below and above left / right, whatever their signs
    unsigned int exact_record_down(ExactRecordProgram *program, unsigned int left, unsigned int right);

    unsigned int exact_record_up(ExactRecordProgram *program, unsigned int left, unsigned int right);

    // ---- two's complement ----

    // a value known to lie in [-2^(bits - 1), 2^(bits - 1)), its register given that width: its residue mod 2^bits,
    // read back signed, is the value
    unsigned int exact_record_wrap(ExactRecordProgram *program, unsigned int value, unsigned int bits);

    // a value known to lie in [0, 2^bits), its register given that width: the and with 2^bits - 1 keeps it
    unsigned int exact_record_narrow(ExactRecordProgram *program, unsigned int value, unsigned int bits);

    // bit `bit` of a value's two's complement, 0 or 1
    unsigned int exact_record_bit(ExactRecordProgram *program, unsigned int value, unsigned int bit);

    // ---- comparison ----

    // [left > right], 1 or 0
    unsigned int exact_record_above(ExactRecordProgram *program, unsigned int left, unsigned int right);

    // [left == right], 1 or 0
    unsigned int exact_record_equal(ExactRecordProgram *program, unsigned int left, unsigned int right);

    // if_one where flag is 1, if_zero where it is 0: if_zero + flag (if_one - if_zero)
    unsigned int exact_record_select(ExactRecordProgram *program, unsigned int flag, unsigned int if_one,
                                     unsigned int if_zero);

    // ---- tables and powers ----

    // a table of 2^index_bits rows, each out_bits wide, rows of (out_bits + 31) / 32 limbs; the values are copied.
    // Returns the table's number, the right of its ENGINE_RECORD_TABLE step
    unsigned int exact_record_table(ExactRecordProgram *program, unsigned int index_bits, unsigned int out_bits,
                                    const unsigned int *values);

    // the row the low index_bits of `index` select
    unsigned int exact_record_lookup(ExactRecordProgram *program, unsigned int index, unsigned int table);

    // base^exponent, both registers, the exponent known in [0, 2^exponent_bits): square and multiply over the
    // exponent's bits from the top, each bit read by a quotient by its power of two
    unsigned int exact_record_power(ExactRecordProgram *program, unsigned int base, unsigned int exponent,
                                    unsigned int exponent_bits);

    // base^exponent for a constant base, the exponent a register known in [0, 2^exponent_bits): from the exponent's
    // top nibble down, the power is raised to the 16th by four squares and multiplied by base^(the nibble) from a
    // table of base^v, v below 16. With base 2 it is the lane's own power of two
    unsigned int exact_record_power_of(ExactRecordProgram *program, unsigned long long base, unsigned int exponent,
                                       unsigned int exponent_bits);

    // ---- widths ----

    // the bits of v, 0 for 0
    unsigned int exact_record_bits_of(unsigned long long value);

#ifdef __cplusplus
}
#endif

#endif
