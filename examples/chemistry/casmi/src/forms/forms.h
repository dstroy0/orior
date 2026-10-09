// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.h: the record-program pieces a stored double is read into its forms by. Each call writes record steps into a
// caller's program and returns the registers holding its results; nothing here computes a lane's value on the host.
#ifndef CASMI_FORMS_H
#define CASMI_FORMS_H

#include "../../../../../src/cu/types/integers/exact_record/exact_record.h"

// a double's 64 bits, as IEEE 754 binary64 lays them
#define FORMS_MANTISSA_BITS 52u
#define FORMS_EXPONENT_BITS 11u

// the unit's exponent: on 2^(E - 1077) a double is 4M with its preimage's ends whole
#define FORMS_LIFT 1077ull

// a base B from 1 to 2^53: its biased exponent 1023 to 1075, and the 0 to 52 low mantissa bits a whole B leaves 0
#define FORMS_BASE_BIASED_LEAST 1023ull
#define FORMS_BASE_BIASED_MOST 1075ull
#define FORMS_SHIFT_BITS 7u

// the powers of ten below 2^64, 10^0 to 10^19
#define FORMS_TENS 20u

// a double's two fields in a record, the fraction and the biased exponent, laid once for every member
typedef struct
{
    unsigned int fraction;
    unsigned int exponent;
} FormsFields;

// the fraction and the biased exponent of one member's double
typedef struct
{
    unsigned int fraction;
    unsigned int biased;
} FormsDouble;

// A double's preimage on the unit 2^(E - 1077): its ends `low` and `high`, `odd_low` and `odd_high` 1 where each end is
// open, and `divisor` 2^(1077 - E); a real r rounds to the double exactly where r 2^(1077 - E) lies between the ends.
// `centre` is the double itself on the unit, 4M
typedef struct
{
    unsigned int centre;
    unsigned int low;
    unsigned int high;
    unsigned int odd_low;
    unsigned int odd_high;
    unsigned int divisor;
} FormsPreimage;

unsigned long long forms_ten(unsigned int power);

void forms_fields(ExactRecordProgram *program, FormsFields *fields);

// the double member `member` of the record holds
void forms_double(ExactRecordProgram *program, const FormsFields *fields, unsigned int member, FormsDouble *read);

// the double's preimage; `most_placed` is the column's most exponent as placed, and `shift_bits` holds
// 1077 - its least
void forms_preimage(ExactRecordProgram *program, const FormsDouble *read, unsigned long long most_placed,
                    unsigned int shift_bits, FormsPreimage *preimage);

// the doubles D with fl(D / base) the double, and the reals rounding to one of them: their preimage, on the unit
// u / 4, and `exists` 1 where some D divides to the double
void forms_divided(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned long long base,
                   FormsPreimage *divided, unsigned int *exists);

// The integer on the unit 10^-places: k 10^(places - p) where some p from 0 to `places` holds an integer k with
// k 10^-p inside the preimage, the least p and its least k, and `held` 1 there; 0 and `held` 0 where none does
void forms_unit(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int places, unsigned int *held,
                unsigned int *unit);

// The least places p from 0 to `most` at which an integer k has k 10^-p inside the preimage, and the least such k:
// `places` counts the places tried before the first that holds, most + 1 where none does, and `least` is k there and 0
// where none holds
void forms_places(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int most, unsigned int *places,
                  unsigned int *least);

// floor(10^places x) of the double x the preimage is of
unsigned int forms_floor(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int places);

// The count c with fl(c / B) the double, B the base read from `base`, a whole number from 1 to 2^53: `held` 1 where B
// is such a number and such a c exists, and `count` c there and 0 elsewhere
void forms_count(ExactRecordProgram *program, const FormsPreimage *preimage, const FormsDouble *base,
                 unsigned int *held, unsigned int *count);

// the base a double holds as a whole number, and `whole` 1 where it is one from 1 to 2^53
void forms_whole(ExactRecordProgram *program, const FormsDouble *base, unsigned int *value, unsigned int *whole);

// 1 where value / scale lies inside the preimage, its ends closed or open as the preimage holds them: value times the
// divisor against the ends times the scale, exact
unsigned int forms_contains(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int scale,
                            unsigned int value);

#endif
