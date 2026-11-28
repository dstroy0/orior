// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keymath_core_affine.h: word bits, reads, magnitudes and the forms (keymath_core.h includes the parts in order)
#ifndef KEYMATH_CORE_AFFINE_H
#define KEYMATH_CORE_AFFINE_H

// the record encoding of keymath, as one source that the host and the device both compile (engine_table.md item
// 11(a), the compiler on the device). It has the term and width of each step, read in step order from its operation
// and from those of its operands. It has the linear form of each register, which narrows the width where it is
// tighter.
//
// The keymath_record_encode of the host runs it, and arranges the key. The device runs it in one thread, because each
// step reads the steps before it. The terms of a linear form go in an arena that the caller gives, each form a run of
// it. The call reports an arena too small for the forms. The caller then gives a larger one and runs the encoding
// again, which decides the same

#include "../../engine_config.h"

#include <stddef.h>

#if defined(__CUDACC__)
#define KEYMATH_CORE __host__ __device__ static inline
#else
#define KEYMATH_CORE static inline
#endif

// a coefficient or constant of a linear form stays within this. The call can then check a sum of two, and the product
// by a constant, in one word before it forms either. Two at the limit sum to `2^63`, past a signed word
#define KEYMATH_COEFFICIENT_MAX (1LL << 62)

// the limbs a form's bound takes: an atom's register is at max 32 ENGINE_RECORD_LIMBS_MAX bits and a coefficient at
// max 2^62. Each term is below 2^(32 ENGINE_RECORD_LIMBS_MAX + 63), and the sum of fewer than 2^32 of them below
// 2^(32 ENGINE_RECORD_LIMBS_MAX + 95)
#define KEYMATH_BOUND_LIMBS (ENGINE_RECORD_LIMBS_MAX + 3u)

// the terms that the first run of an encode gives the linear form of each step, on the host and the device. A run
// that fills its arena runs again with one twice the size, which decides the same
#define KEYMATH_ARENA_PER_STEP 8u

// how an encode ends: every step succeeded, or a step, its table or an output gave an error, the one at `at`. Else the
// arena was too small for the forms
enum KeymathCoreEnd
{
    KEYMATH_CORE_OK = 0,
    KEYMATH_CORE_STEP = 1,
    KEYMATH_CORE_TABLE = 2,
    KEYMATH_CORE_OUTPUT = 3,
    KEYMATH_CORE_FULL = 4
};

// one term of a linear form: an atom, an earlier register the form does not open, and its coefficient
struct KeymathCoreTerm
{
    unsigned int atom;
    long long coefficient;
};

// a register as a linear form: integer coefficients over atoms, plus a constant. Each atom is an earlier register that
// the form does not open. That is a field, or a register that no sum, difference or constant product made. Every
// register has one, and that of an atom is itself with coefficient 1.
//
// Its terms are `count` of the arena from `first`, in atom order. The call reads its width from the form, as
// `|x| <= |c| + sum |c_i| (2^(b_i) - 1)` over the atom widths `b_i`. That is A16, the bulk-boundary map of Mathai and
// Thiang read as a restriction on the dual side
struct KeymathCoreAffine
{
    long long constant;
    unsigned long long first;
    unsigned long long count;
};

// the terms that the forms go in: `capacity` of them, `used` taken, and 1 in `full` after a form found no room
struct KeymathCoreArena
{
    KeymathCoreTerm *terms;
    unsigned long long capacity;
    unsigned long long used;
    int full;
};

// an encode:
//
// - its steps, the sizes of the fields, members and tables that it reads, and its outputs
// - the term of each step, 1 where its register is never negative, and its form, in step order
// - the arena
// - KEYMATH_BOUND_LIMBS words that the call sums the bound of a form in. They belong to the caller, because they are
//   past what the stack of a device thread fits.
// - where it ended, and at what
struct KeymathCoreEncode
{
    const EngineRecordStep *steps;
    unsigned int count;
    const unsigned int *field_bits;
    unsigned int fields;
    unsigned int members;
    const unsigned int *outputs;
    unsigned int output_count;
    const EngineRecordTable *tables;
    unsigned int table_count;
    EngineRecordTerm *terms;
    unsigned char *never_negative;
    KeymathCoreAffine *forms;
    KeymathCoreArena arena;
    unsigned int *bound;
    unsigned int end;
    unsigned int at;
};

KEYMATH_CORE unsigned int keymath_core_word_bits(unsigned long long value)
{
    unsigned int bits = 0u;
    while (value != 0ULL)
    {
        bits += 1u;
        value >>= 1u;
    }
    return bits;
}

KEYMATH_CORE int keymath_core_record_reads(unsigned int operation)
{
    return (operation == ENGINE_RECORD_PRODUCT) || (operation == ENGINE_RECORD_SUM) ||
           (operation == ENGINE_RECORD_DIFFERENCE) || (operation == ENGINE_RECORD_LADDER) ||
           (operation == ENGINE_RECORD_ABSOLUTE) || (operation == ENGINE_RECORD_COMPARE) ||
           (operation == ENGINE_RECORD_QUOTIENT) || (operation == ENGINE_RECORD_REMAINDER) ||
           (operation == ENGINE_RECORD_GCD) || (operation == ENGINE_RECORD_EXACT_QUOTIENT) ||
           (operation == ENGINE_RECORD_XOR) || (operation == ENGINE_RECORD_AND);
}

// whether the register of a step is never negative, read from its operation and from those of its operands. These are
// never negative:
//
// - a field read unsigned, a constant, an absolute value, a gcd, an entry of a table, and the number of the lane
// - a sum, product, quotient, exact quotient or xor of two such
// - a remainder of one such (it carries the sign of the numerator), and an and with one such
// - a wrap that passes one such through with no change
KEYMATH_CORE unsigned char keymath_core_never_negative(const EngineRecordStep *doing,
                                                       const unsigned char *never_negative, int wrap_passes)
{
    const unsigned int operation = doing->operation;
    if ((operation == ENGINE_RECORD_FIELD) || (operation == ENGINE_RECORD_CONSTANT) ||
        (operation == ENGINE_RECORD_ABSOLUTE) || (operation == ENGINE_RECORD_GCD) ||
        (operation == ENGINE_RECORD_TABLE) || (operation == ENGINE_RECORD_LANE))
    {
        return 1u;
    }
    if ((operation == ENGINE_RECORD_SUM) || (operation == ENGINE_RECORD_PRODUCT) ||
        (operation == ENGINE_RECORD_QUOTIENT) || (operation == ENGINE_RECORD_EXACT_QUOTIENT) ||
        (operation == ENGINE_RECORD_XOR))
    {
        return ((never_negative[doing->left] != 0u) && (never_negative[doing->right] != 0u)) ? 1u : 0u;
    }
    if (operation == ENGINE_RECORD_REMAINDER)
    {
        return never_negative[doing->left];
    }
    if (operation == ENGINE_RECORD_AND)
    {
        return ((never_negative[doing->left] != 0u) || (never_negative[doing->right] != 0u)) ? 1u : 0u;
    }
    if (operation == ENGINE_RECORD_WRAP)
    {
        return ((wrap_passes != 0) && (never_negative[doing->left] != 0u)) ? 1u : 0u;
    }
    return 0u;
}

KEYMATH_CORE long long keymath_core_magnitude(long long value)
{
    return (value < 0LL) ? -value : value;
}

// `left + right`, each within KEYMATH_COEFFICIENT_MAX, into `*sum`. It is 0, with `*sum` 0, where the sum would pass
// it. The call tests before it forms the sum, and each side of the test stays within the limit: no word overflows
KEYMATH_CORE int keymath_core_sum_fits(long long left, long long right, long long *sum)
{
    const int fits =
        (right >= 0LL) ? (left <= (KEYMATH_COEFFICIENT_MAX - right)) : (left >= (-KEYMATH_COEFFICIENT_MAX - right));
    *sum = fits ? (left + right) : 0LL;
    return fits;
}

// a term taken onto the arena's end; 0, and the arena marked full, where it has no room
KEYMATH_CORE int keymath_core_push(KeymathCoreArena *arena, unsigned int atom, long long coefficient)
{
    if (arena->used >= arena->capacity)
    {
        arena->full = 1;
        return 0;
    }
    arena->terms[arena->used].atom = atom;
    arena->terms[arena->used].coefficient = coefficient;
    arena->used += 1ULL;
    return 1;
}

// `left + sign right`, with the terms merged by atom, arranged at the end of the arena into `*sum`. It is 0 where a
// coefficient or the constant would pass KEYMATH_COEFFICIENT_MAX, or the arena is full
KEYMATH_CORE int keymath_core_affine_add(KeymathCoreArena *arena, const KeymathCoreAffine *left,
                                         const KeymathCoreAffine *right, long long sign, KeymathCoreAffine *sum)
{
    sum->first = arena->used;
    sum->count = 0ULL;
    int ok = keymath_core_sum_fits(left->constant, sign * right->constant, &sum->constant);
    unsigned long long at_left = 0ULL;
    unsigned long long at_right = 0ULL;
    while (ok && ((at_left < left->count) || (at_right < right->count)))
    {
        const KeymathCoreTerm *const left_term = &arena->terms[left->first + at_left];
        const KeymathCoreTerm *const right_term = &arena->terms[right->first + at_right];
        const int take_left =
            (at_right == right->count) || ((at_left < left->count) && (left_term->atom <= right_term->atom));
        const int take_right =
            (at_left == left->count) || ((at_right < right->count) && (right_term->atom <= left_term->atom));
        const unsigned int atom = take_left ? left_term->atom : right_term->atom;
        long long coefficient = 0LL;
        ok = keymath_core_sum_fits(take_left ? left_term->coefficient : 0LL,
                                   take_right ? (sign * right_term->coefficient) : 0LL, &coefficient);
        at_left += take_left ? 1ULL : 0ULL;
        at_right += take_right ? 1ULL : 0ULL;
        if (ok && (coefficient != 0LL))
        {
            ok = keymath_core_push(arena, atom, coefficient);
            sum->count += ok ? 1ULL : 0ULL;
        }
    }
    return ok;
}

// the form times a constant, arranged at the end of the arena into `*scaled`. It is 0 where a coefficient or the
// constant would pass KEYMATH_COEFFICIENT_MAX, or the arena is full
KEYMATH_CORE int keymath_core_affine_scale(KeymathCoreArena *arena, const KeymathCoreAffine *form, long long factor,
                                           KeymathCoreAffine *scaled)
{
    const long long max =
        (factor == 0LL) ? KEYMATH_COEFFICIENT_MAX : (KEYMATH_COEFFICIENT_MAX / keymath_core_magnitude(factor));
    int ok = keymath_core_magnitude(form->constant) <= max;
    scaled->constant = ok ? (form->constant * factor) : 0LL;
    scaled->first = arena->used;
    scaled->count = 0ULL;
    for (unsigned long long at = 0ULL; ok && (at < form->count) && (factor != 0LL); at += 1ULL)
    {
        const KeymathCoreTerm term = arena->terms[form->first + at];
        ok = keymath_core_magnitude(term.coefficient) <= max;
        // the call does not multiply a coefficient past the limit, because its product can pass a signed word
        if (ok)
        {
            ok = keymath_core_push(arena, term.atom, term.coefficient * factor);
            scaled->count += ok ? 1ULL : 0ULL;
        }
    }
    return ok;
}

// a magnitude of at max 2^62 shifted up by `shift` bits, added into the bound's limbs; the bound contains the sum
KEYMATH_CORE void keymath_core_add_shifted(unsigned int *bound, unsigned long long magnitude, unsigned long long shift)
{
    const unsigned int part = (unsigned int)(shift % 32ULL);
    // the shift is below 32 KEYMATH_BOUND_LIMBS, and its whole limbs fit an unsigned int
    const unsigned int limb_shift = (unsigned int)(shift / 32ULL);
    // the magnitude is at max 2^62, and shifted by fewer than 32 bits it spans at max three limbs
    const unsigned long long low = magnitude << part;
    const unsigned long long high = (part == 0u) ? 0ULL : (magnitude >> (64u - part));
    const unsigned int added[3] = {(unsigned int)(low & 0xFFFFFFFFULL), (unsigned int)(low >> 32u), (unsigned int)high};
    unsigned long long carry = 0ULL;
    // past the three limbs added, the sum changes only while a carry runs
    for (unsigned int limb = limb_shift;
         (limb < KEYMATH_BOUND_LIMBS) && (((limb - limb_shift) < 3u) || (carry != 0ULL)); limb += 1u)
    {
        const unsigned long long total =
            carry + bound[limb] + (((limb - limb_shift) < 3u) ? added[limb - limb_shift] : 0u);
        bound[limb] = (unsigned int)(total & 0xFFFFFFFFULL);
        carry = total >> 32u;
    }
}

#endif
