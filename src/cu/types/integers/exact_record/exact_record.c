// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "exact_record.h"

#include <stdlib.h>
#include <string.h>

// the rows of a nibble's table, and the squares that raise a power to the 16th
#define EXACT_RECORD_NIBBLE_BITS 4u
#define EXACT_RECORD_NIBBLE_ROWS 16u

// the room an array grows to, at least `needed`, doubling
static unsigned int exact_record_room(unsigned int room, unsigned int needed)
{
    unsigned int grown = (room == 0u) ? 16u : room;
    while (grown < needed)
    {
        grown *= 2u;
    }
    return grown;
}

static int exact_record_grow(void **array, unsigned int *room, unsigned int needed, size_t size)
{
    if (needed <= *room)
    {
        return 1;
    }
    const unsigned int grown = exact_record_room(*room, needed);
    void *const moved = realloc(*array, (size_t)grown * size);
    if (moved == NULL)
    {
        return 0;
    }
    *array = moved;
    *room = grown;
    return 1;
}

void exact_record_open(ExactRecordProgram *program)
{
    memset(program, 0, sizeof(*program));
}

void exact_record_close(ExactRecordProgram *program)
{
    for (unsigned int at = 0u; at < program->table_count; at += 1u)
    {
        free(program->table_values[at]);
    }
    free(program->steps);
    free(program->field_bits);
    free(program->field_offset);
    free(program->tables);
    free(program->table_values);
    memset(program, 0, sizeof(*program));
}

unsigned int exact_record_step(ExactRecordProgram *program, EngineRecordOperation operation, unsigned int left,
                               unsigned int right, unsigned int member)
{
    if ((program->failed != 0) ||
        (exact_record_grow((void **)&program->steps, &program->step_room, program->count + 1u,
                           sizeof(EngineRecordStep)) == 0))
    {
        program->failed = 1;
        return 0u;
    }
    EngineRecordStep *const step = &program->steps[program->count];
    step->operation = operation;
    step->left = left;
    step->right = right;
    step->member = member;
    program->count += 1u;
    return program->count - 1u;
}

unsigned int exact_record_constant(ExactRecordProgram *program, unsigned long long value)
{
    return exact_record_step(program, ENGINE_RECORD_CONSTANT, (unsigned int)(value & 0xFFFFFFFFull),
                             (unsigned int)(value >> 32u), 0u);
}

unsigned int exact_record_power_two(ExactRecordProgram *program, unsigned int bits)
{
    unsigned int out = exact_record_constant(program, 1ull << (bits % 63u));
    for (unsigned int at = 0u; at < bits / 63u; at += 1u)
    {
        out = exact_record_product(program, out, exact_record_constant(program, 1ull << 63u));
    }
    return out;
}

unsigned int exact_record_mask(ExactRecordProgram *program, unsigned int bits)
{
    if (bits < 64u)
    {
        return exact_record_constant(program, (bits == 0u) ? 0ull : (~0ull >> (64u - bits)));
    }
    return exact_record_difference(program, exact_record_power_two(program, bits), exact_record_constant(program, 1ull));
}

static unsigned int exact_record_field_at(ExactRecordProgram *program, unsigned int bits, unsigned int offset)
{
    const unsigned int needed = program->fields + 1u;
    unsigned int bits_room = program->field_room;
    if ((program->failed != 0) ||
        (exact_record_grow((void **)&program->field_bits, &bits_room, needed, sizeof(unsigned int)) == 0) ||
        (exact_record_grow((void **)&program->field_offset, &program->field_room, needed, sizeof(unsigned int)) == 0))
    {
        program->failed = 1;
        return 0u;
    }
    program->field_bits[program->fields] = bits;
    program->field_offset[program->fields] = offset;
    program->fields += 1u;
    return program->fields - 1u;
}

unsigned int exact_record_field(ExactRecordProgram *program, unsigned int bits)
{
    const unsigned int field = exact_record_field_at(program, bits, program->shared_bits);
    program->shared_bits += (program->failed == 0) ? bits : 0u;
    return field;
}

unsigned int exact_record_member_field(ExactRecordProgram *program, unsigned int bits, unsigned int offset)
{
    return exact_record_field_at(program, bits, offset);
}

unsigned int exact_record_read(ExactRecordProgram *program, unsigned int field, unsigned int member)
{
    return exact_record_step(program, ENGINE_RECORD_FIELD_SIGNED, field, 0u, member);
}

unsigned int exact_record_read_unsigned(ExactRecordProgram *program, unsigned int field, unsigned int member)
{
    return exact_record_step(program, ENGINE_RECORD_FIELD, field, 0u, member);
}

unsigned int exact_record_lane_below(ExactRecordProgram *program, unsigned int bits)
{
    return exact_record_narrow(program, exact_record_step(program, ENGINE_RECORD_LANE, 0u, 0u, 0u), bits);
}

unsigned int exact_record_sum(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_step(program, ENGINE_RECORD_SUM, left, right, 0u);
}

unsigned int exact_record_difference(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_step(program, ENGINE_RECORD_DIFFERENCE, left, right, 0u);
}

unsigned int exact_record_product(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_step(program, ENGINE_RECORD_PRODUCT, left, right, 0u);
}

unsigned int exact_record_quotient(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_step(program, ENGINE_RECORD_QUOTIENT, left, right, 0u);
}

unsigned int exact_record_remainder(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_step(program, ENGINE_RECORD_REMAINDER, left, right, 0u);
}

unsigned int exact_record_exact_quotient(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_step(program, ENGINE_RECORD_EXACT_QUOTIENT, left, right, 0u);
}

unsigned int exact_record_negate(ExactRecordProgram *program, unsigned int value)
{
    return exact_record_difference(program, exact_record_constant(program, 0ull), value);
}

unsigned int exact_record_absolute(ExactRecordProgram *program, unsigned int value)
{
    return exact_record_step(program, ENGINE_RECORD_ABSOLUTE, value, 0u, 0u);
}

unsigned int exact_record_down(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_difference(program, exact_record_quotient(program, left, right), exact_record_constant(program, 1ull));
}

unsigned int exact_record_up(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    return exact_record_sum(program, exact_record_quotient(program, left, right), exact_record_constant(program, 1ull));
}

unsigned int exact_record_wrap(ExactRecordProgram *program, unsigned int value, unsigned int bits)
{
    return exact_record_step(program, ENGINE_RECORD_WRAP, value, bits, 0u);
}

unsigned int exact_record_narrow(ExactRecordProgram *program, unsigned int value, unsigned int bits)
{
    return exact_record_step(program, ENGINE_RECORD_AND, value, exact_record_mask(program, bits), 0u);
}

// the and with 2^bit is 2^bit or 0 on two's complement whatever the value's sign, and its quotient by 2^bit the bit
unsigned int exact_record_bit(ExactRecordProgram *program, unsigned int value, unsigned int bit)
{
    const unsigned int place = exact_record_power_two(program, bit);
    return exact_record_quotient(program, exact_record_step(program, ENGINE_RECORD_AND, value, place, 0u), place);
}

// (c + |c|) / 2 with c = compare(left, right)
unsigned int exact_record_above(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    const unsigned int c = exact_record_step(program, ENGINE_RECORD_COMPARE, left, right, 0u);
    return exact_record_quotient(program, exact_record_sum(program, c, exact_record_absolute(program, c)),
                                 exact_record_constant(program, 2ull));
}

// 1 - [left > right] - [right > left]
unsigned int exact_record_equal(ExactRecordProgram *program, unsigned int left, unsigned int right)
{
    const unsigned int once = exact_record_difference(program, exact_record_constant(program, 1ull),
                                                      exact_record_above(program, left, right));
    return exact_record_difference(program, once, exact_record_above(program, right, left));
}

unsigned int exact_record_select(ExactRecordProgram *program, unsigned int flag, unsigned int if_one,
                                 unsigned int if_zero)
{
    return exact_record_sum(program, if_zero,
                            exact_record_product(program, flag, exact_record_difference(program, if_one, if_zero)));
}

unsigned int exact_record_table(ExactRecordProgram *program, unsigned int index_bits, unsigned int out_bits,
                                const unsigned int *values)
{
    const unsigned int needed = program->table_count + 1u;
    unsigned int values_room = program->table_room;
    if ((program->failed != 0) ||
        (exact_record_grow((void **)&program->table_values, &values_room, needed, sizeof(unsigned int *)) == 0) ||
        (exact_record_grow((void **)&program->tables, &program->table_room, needed, sizeof(EngineRecordTable)) == 0))
    {
        program->failed = 1;
        return 0u;
    }
    const size_t words = ((size_t)1u << index_bits) * (size_t)((out_bits + 31u) / 32u);
    unsigned int *const copy = (unsigned int *)malloc(words * sizeof(unsigned int));
    if (copy == NULL)
    {
        program->failed = 1;
        return 0u;
    }
    memcpy(copy, values, words * sizeof(unsigned int));
    program->table_values[program->table_count] = copy;
    program->tables[program->table_count].index_bits = index_bits;
    program->tables[program->table_count].out_bits = out_bits;
    program->tables[program->table_count].values = copy;
    program->table_count += 1u;
    return program->table_count - 1u;
}

unsigned int exact_record_lookup(ExactRecordProgram *program, unsigned int index, unsigned int table)
{
    return exact_record_step(program, ENGINE_RECORD_TABLE, index, table, 0u);
}

unsigned int exact_record_power(ExactRecordProgram *program, unsigned int base, unsigned int exponent,
                                unsigned int exponent_bits)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    unsigned int power = one;
    for (unsigned int bit = exponent_bits; bit > 0u; bit -= 1u)
    {
        if (bit < exponent_bits)
        {
            power = exact_record_product(program, power, power);
        }
        const unsigned int set = exact_record_bit(program, exponent, bit - 1u);
        power = exact_record_product(program, power, exact_record_select(program, set, base, one));
    }
    return power;
}

// value *= factor over `limbs` limbs, a table row laid out on the host: the factor's two limbs, a row each as the long
// multiplication takes them. 1 where the product passes the limbs or its room cannot be held
static unsigned int exact_record_limbs_times(unsigned int *value, unsigned int limbs, unsigned long long factor)
{
    unsigned int *const product = (unsigned int *)calloc((size_t)limbs + 2u, sizeof(unsigned int));
    if (product == NULL)
    {
        return 1u;
    }
    const unsigned long long part[2] = {factor & 0xFFFFFFFFull, factor >> 32u};
    for (unsigned int row = 0u; row < 2u; row += 1u)
    {
        unsigned long long carry = 0ull;
        for (unsigned int at = 0u; at < limbs; at += 1u)
        {
            // (2^32 - 1)^2 + 2 (2^32 - 1) is 2^64 - 1, and the total fits the word
            const unsigned long long total = ((unsigned long long)value[at] * part[row]) + product[at + row] + carry;
            product[at + row] = (unsigned int)total;
            carry = total >> 32u;
        }
        product[limbs + row] = (unsigned int)carry;
    }
    const unsigned int over = (unsigned int)((product[limbs] != 0u) || (product[limbs + 1u] != 0u));
    memcpy(value, product, (size_t)limbs * sizeof(unsigned int));
    free(product);
    return over;
}

unsigned int exact_record_power_of(ExactRecordProgram *program, unsigned long long base, unsigned int exponent,
                                   unsigned int exponent_bits)
{
    // the table's rows base^v for v below 16, in limbs enough for base^15 at 15 times base's bits, then laid out as wide
    // as the widest row, base^15 for a base past 1
    const unsigned int bound_limbs = ((exact_record_bits_of(base) * (EXACT_RECORD_NIBBLE_ROWS - 1u)) / 32u) + 1u;
    unsigned int *const rows = (unsigned int *)calloc((size_t)EXACT_RECORD_NIBBLE_ROWS * bound_limbs, sizeof(unsigned int));
    if (rows == NULL)
    {
        program->failed = 1;
        return 0u;
    }
    rows[0] = 1u;
    for (unsigned int row = 1u; row < EXACT_RECORD_NIBBLE_ROWS; row += 1u)
    {
        unsigned int *const entry = &rows[(size_t)row * bound_limbs];
        memcpy(entry, &rows[(size_t)(row - 1u) * bound_limbs], (size_t)bound_limbs * sizeof(unsigned int));
        if (exact_record_limbs_times(entry, bound_limbs, base) != 0u)
        {
            program->failed = 1;
        }
    }
    unsigned int out_bits = 1u;
    for (unsigned int row = 0u; row < EXACT_RECORD_NIBBLE_ROWS; row += 1u)
    {
        for (unsigned int limb = bound_limbs; limb > 0u; limb -= 1u)
        {
            const unsigned int word = rows[((size_t)row * bound_limbs) + limb - 1u];
            if (word != 0u)
            {
                const unsigned int bits = ((limb - 1u) * 32u) + exact_record_bits_of(word);
                out_bits = (bits > out_bits) ? bits : out_bits;
                break;
            }
        }
    }
    const unsigned int row_limbs = (out_bits + 31u) / 32u;
    unsigned int *const values = (unsigned int *)calloc((size_t)EXACT_RECORD_NIBBLE_ROWS * row_limbs, sizeof(unsigned int));
    if (values == NULL)
    {
        free(rows);
        program->failed = 1;
        return 0u;
    }
    for (unsigned int row = 0u; row < EXACT_RECORD_NIBBLE_ROWS; row += 1u)
    {
        memcpy(&values[(size_t)row * row_limbs], &rows[(size_t)row * bound_limbs], (size_t)row_limbs * sizeof(unsigned int));
    }
    free(rows);
    const unsigned int table = exact_record_table(program, EXACT_RECORD_NIBBLE_BITS, out_bits, values);
    free(values);
    const unsigned int nibbles = (exponent_bits + EXACT_RECORD_NIBBLE_BITS - 1u) / EXACT_RECORD_NIBBLE_BITS;
    // 16^nibbles stands above every exponent: added, it leaves each nibble's bits as they are, and a quotient by any
    // lower power of 16 keeps a whole nibble for the table's index
    const unsigned int guarded =
        exact_record_sum(program, exponent, exact_record_power_two(program, EXACT_RECORD_NIBBLE_BITS * nibbles));
    unsigned int power = 0u;
    for (unsigned int nibble = nibbles; nibble > 0u; nibble -= 1u)
    {
        const unsigned int place = exact_record_power_two(program, EXACT_RECORD_NIBBLE_BITS * (nibble - 1u));
        const unsigned int row = exact_record_lookup(program, exact_record_quotient(program, guarded, place), table);
        if (nibble == nibbles)
        {
            power = row;
            continue;
        }
        for (unsigned int square = 0u; square < EXACT_RECORD_NIBBLE_BITS; square += 1u)
        {
            power = exact_record_product(program, power, power);
        }
        power = exact_record_product(program, power, row);
    }
    return (nibbles == 0u) ? exact_record_constant(program, 1ull) : power;
}

unsigned int exact_record_two_to(ExactRecordProgram *program, unsigned int exponent, unsigned int exponent_bits)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    unsigned int power = one;
    for (unsigned int bit = 0u; bit < exponent_bits; bit += 1u)
    {
        const unsigned int set = exact_record_bit(program, exponent, bit);
        const unsigned int factor = exact_record_select(program, set, exact_record_power_two(program, 1u << bit), one);
        power = (bit == 0u) ? factor : exact_record_product(program, power, factor);
    }
    return power;
}

unsigned int exact_record_bits_of(unsigned long long value)
{
    unsigned int bits = 0u;
    while ((bits < 64u) && ((value >> bits) != 0ull))
    {
        bits += 1u;
    }
    return bits;
}
