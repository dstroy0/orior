// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SIM_H
#define SIM_H

#include "../../cu/engine/engine_config.h"

#include "../../../includes/exact_types/integers/exact_integer.h"
#include "../../platform/platform.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>
#include <string.h>

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIM_LINE_CAPACITY 8192ULL

// the words that a counter owns in the run of counters of its key. One word goes to each 64 trials of
// sim_binomial_half. One goes to each 6 electrons of sim_poisson_four_cumulants, and one to each 64 / bits electrons of
// sim_thinned. A word past these
// comes from a key of the counter's own (sim_counter_draw). No count runs into the words of the next
// counter
#define SIM_COUNTER_STRIDE 65536ULL

#define SIM_COUNTER_PAST_PURPOSE 0x50415354ULL

struct TesseraClient;

typedef struct
{
    unsigned long long checks;
    unsigned long long failures;
    VerbaLinea line;
    struct TesseraClient *job;
    unsigned long long job_declared;
} SimResults;

// a sim that uses the device is one job on the device's tessera daemon, submitted before its first device
// allocation and released by sim_close. The signum is the host BLAKE3 of the sim's name and arguments
int sim_job_submit(SimResults *results, const char *name, int count, char *const *arguments,
                   unsigned long long declared);

void sim_job_release(SimResults *results);

static inline __host__ __device__ unsigned long long sim_mix(unsigned long long word)
{
    unsigned long long mixed = word + 0x9E3779B97F4A7C15ULL;
    mixed = (mixed ^ (mixed >> 30u)) * 0xBF58476D1CE4E5B9ULL;
    mixed = (mixed ^ (mixed >> 27u)) * 0x94D049BB133111EBULL;
    return mixed ^ (mixed >> 31u);
}

static inline __host__ __device__ unsigned long long sim_draw(unsigned long long key, unsigned long long counter)
{
    return sim_mix(key ^ sim_mix(counter));
}

static inline __host__ __device__ unsigned long long sim_draw_below(unsigned long long key, unsigned long long counter,
                                                                    unsigned long long bound)
{
    return sim_draw(key, counter) % bound;
}

// word `word` of the draws of a counter. The first SIM_COUNTER_STRIDE lie in the stride of the counter among the
// counters of the key. Each word past them comes from a key made from the counter. A count of any size stays within its
// own words
static inline __host__ __device__ unsigned long long sim_counter_draw(unsigned long long key,
                                                                      unsigned long long counter,
                                                                      unsigned long long word)
{
    if (word < SIM_COUNTER_STRIDE)
    {
        return sim_draw(key, (counter * SIM_COUNTER_STRIDE) + word);
    }
    return sim_draw(key ^ sim_mix(counter ^ SIM_COUNTER_PAST_PURPOSE), word);
}

static inline __host__ __device__ unsigned long long sim_bits_set(unsigned long long word)
{
    word = word - ((word >> 1u) & 0x5555555555555555ULL);
    word = (word & 0x3333333333333333ULL) + ((word >> 2u) & 0x3333333333333333ULL);
    word = (word + (word >> 4u)) & 0x0F0F0F0F0F0F0F0FULL;
    return (word * 0x0101010101010101ULL) >> 56u;
}

static inline __host__ __device__ unsigned long long sim_binomial_half(unsigned long long key,
                                                                       unsigned long long counter,
                                                                       unsigned long long trials)
{
    unsigned long long heads = 0ULL;
    unsigned long long word = 0ULL;
    for (unsigned long long done = 0ULL; done < trials; done += 64ULL)
    {
        const unsigned long long draw = sim_counter_draw(key, counter, word);
        const unsigned long long left = trials - done;
        const unsigned long long masked = (left >= 64ULL) ? draw : (draw & ((1ULL << left) - 1ULL));
        heads += sim_bits_set(masked);
        word += 1ULL;
    }
    return heads;
}

// one electron's draw of the shot below: 0, 1, 2 or 4 with chances 9, 8, 6 and 1 in 24
#define SIM_SHOT_CHANCES 24ULL

// six electrons' chances from one word, 24^6 of them
#define SIM_SHOT_DIGITS 6ULL

#define SIM_SHOT_DIGIT_WORD 191102976ULL

// A count of mean S whose first four cumulants are each S, as those of a Poisson count are. Each of the S expected
// electrons adds 0, 1, 2 or 4 with chances 9, 8, 6 and 1 in 24. The factorial moments of that law are 1 to the fourth,
// as those of Poisson(1) are. It parts from Poisson at the fifth. A word's draw below 24^6 gives six electrons'
// chances. A counter's SIM_COUNTER_STRIDE words contain 6 of them each.
static inline __host__ __device__ unsigned long long sim_poisson_four_cumulants(unsigned long long key,
                                                                                unsigned long long counter,
                                                                                unsigned long long electrons)
{
    unsigned long long collected = 0ULL;
    unsigned long long word = 0ULL;
    for (unsigned long long done = 0ULL; done < electrons; done += SIM_SHOT_DIGITS)
    {
        unsigned long long digits = sim_counter_draw(key, counter, word) % SIM_SHOT_DIGIT_WORD;
        const unsigned long long left = electrons - done;
        const unsigned long long taken = (left < SIM_SHOT_DIGITS) ? left : SIM_SHOT_DIGITS;
        for (unsigned long long digit = 0ULL; digit < taken; digit += 1ULL)
        {
            const unsigned long long chance = digits % SIM_SHOT_CHANCES;
            digits /= SIM_SHOT_CHANCES;
            collected += (chance < 9ULL) ? 0ULL : ((chance < 17ULL) ? 1ULL : ((chance < 23ULL) ? 2ULL : 4ULL));
        }
        word += 1ULL;
    }
    return collected;
}

static inline void sim_open(SimResults *results, char *capacity)
{
    memset(results, 0, sizeof(*results));
    results->line.out = capacity;
    results->line.capacity = SIM_LINE_CAPACITY;
}

static inline void sim_flush(SimResults *results)
{
    (void)(mmgr_line_ok(&results->line) &&
           (fwrite(results->line.out, 1u, mmgr_line_finish(&results->line), stdout) == results->line.at));
    results->line.at = 0ULL;
    fflush(stdout);
}

static inline void sim_check(SimResults *results, int passed, const char *what)
{
    results->checks += 1ULL;
    if (passed == 0)
    {
        results->failures += 1ULL;
        mmgr_line_put(&results->line, "  FAILED: ");
        mmgr_line_put(&results->line, what);
        mmgr_line_ch(&results->line, '\n');
    }
}

static inline int sim_close(SimResults *results, const char *name)
{
    sim_job_release(results);
    mmgr_line_put(&results->line, "  ");
    mmgr_line_put(&results->line, name);
    mmgr_line_put(&results->line, ": ");
    mmgr_line_decimal(&results->line, results->checks, 1u);
    mmgr_line_put(&results->line, " checks, ");
    mmgr_line_decimal(&results->line, results->failures, 1u);
    mmgr_line_put(&results->line, " failed\n");
    sim_flush(results);
    return (results->failures == 0ULL) ? 0 : 1;
}

static inline void sim_exact_unsigned(OriorExactInteger *value, unsigned long long number)
{
    orior_exact_zero(value);
    // the low and high halves of a 64-bit word each fit one 32-bit limb
    value->limb[0] = (uint32_t)(number & 0xFFFFFFFFULL);
    // the high half, shifted down, is below 2^32
    value->limb[1] = (uint32_t)(number >> 32u);
    value->sign = (number == 0ULL) ? 0 : 1;
}

static inline void sim_exact_signed(OriorExactInteger *value, long long number)
{
    // the magnitude of a negative 64-bit word is its two's complement negation, taken unsigned
    const unsigned long long magnitude =
        (number < 0LL) ? (0ULL - (unsigned long long)number) : (unsigned long long)number;
    sim_exact_unsigned(value, magnitude);
    if (number < 0LL)
    {
        value->sign = -1;
    }
}

static inline int sim_exact_product(const OriorExactInteger *left, const OriorExactInteger *right,
                                    OriorExactInteger *result)
{
    return orior_exact_multiply(left, right, result) == ORIOR_EXACT_OK;
}

static inline int sim_exact_scaled(const OriorExactInteger *value, unsigned long long factor, OriorExactInteger *result)
{
    OriorExactInteger scale;
    sim_exact_unsigned(&scale, factor);
    return sim_exact_product(value, &scale, result);
}

static inline int sim_exact_sum(const OriorExactInteger *left, const OriorExactInteger *right,
                                OriorExactInteger *result)
{
    return orior_exact_add(left, right, result) == ORIOR_EXACT_OK;
}

static inline int sim_exact_less(const OriorExactInteger *left, const OriorExactInteger *right,
                                 OriorExactInteger *result)
{
    return orior_exact_subtract(left, right, result) == ORIOR_EXACT_OK;
}

static inline int sim_ratio_compare(const OriorExactInteger *numerator, const OriorExactInteger *denominator,
                                    const OriorExactInteger *other_numerator,
                                    const OriorExactInteger *other_denominator, int *order)
{
    OriorExactInteger left;
    OriorExactInteger right;
    if ((sim_exact_product(numerator, other_denominator, &left) == 0) ||
        (sim_exact_product(other_numerator, denominator, &right) == 0))
    {
        return 0;
    }
    *order = orior_exact_compare(&left, &right);
    return 1;
}

// the floor of numerator / denominator, both non-negative, by bisection below `bound`, which the caller knows the
// quotient is under
static inline unsigned long long sim_ratio_floor(const OriorExactInteger *numerator,
                                                 const OriorExactInteger *denominator, unsigned long long bound)
{
    unsigned long long below = 0ULL;
    unsigned long long above = bound - 1ULL;
    OriorExactInteger trial;
    while (below < above)
    {
        const unsigned long long middle = below + ((above - below) / 2ULL) + ((above - below) & 1ULL);
        if ((sim_exact_scaled(denominator, middle, &trial) != 0) && (orior_exact_compare(&trial, numerator) <= 0))
        {
            below = middle;
        }
        else
        {
            above = middle - 1ULL;
        }
    }
    return below;
}

// one group of an exact integer's decimal digits: 10^9, the widest power of ten whose remainder shifted up a limb
// still fits a word
#define SIM_DECIMAL_GROUP 1000000000ULL

#define SIM_DECIMAL_GROUP_DIGITS 9u

// 10^9 exceeds 2^29. A magnitude of ORIOR_EXACT_BITS bits has at max ORIOR_EXACT_BITS / 29 + 1 groups
#define SIM_DECIMAL_GROUPS ((((unsigned long long)(ORIOR_EXACT_BITS)) / 29ULL) + 1ULL)

// the count of limbs up to the top limb that is not zero, with that limb in the count; 0 for zero
static inline unsigned long long sim_exact_limbs_used(const OriorExactInteger *value)
{
    unsigned long long used = ORIOR_EXACT_LIMBS;
    while ((used > 0ULL) && (value->limb[used - 1ULL] == 0u))
    {
        used -= 1ULL;
    }
    return used;
}

// the magnitude of `value` in decimal, every digit exact. Each group of nine is the remainder of a short division of
// the limbs by 10^9, from the top limb down
static inline void sim_exact_decimal(VerbaLinea *line, const OriorExactInteger *value)
{
    OriorExactInteger work = *value;
    unsigned int groups[SIM_DECIMAL_GROUPS];
    unsigned long long count = 0ULL;
    unsigned long long used = sim_exact_limbs_used(&work);
    do
    {
        unsigned long long remainder = 0ULL;
        for (unsigned long long index = used; index > 0ULL; index -= 1ULL)
        {
            const unsigned long long dividend = (remainder << 32u) | work.limb[index - 1ULL];
            // the remainder is below 10^9. The dividend is below 10^9 * 2^32 and its quotient fits a limb
            work.limb[index - 1ULL] = (uint32_t)(dividend / SIM_DECIMAL_GROUP);
            remainder = dividend % SIM_DECIMAL_GROUP;
        }
        // a remainder by 10^9 is below 2^30
        groups[count] = (unsigned int)remainder;
        count += 1ULL;
        used = sim_exact_limbs_used(&work);
    } while (used > 0ULL);
    mmgr_line_decimal(line, groups[count - 1ULL], 1u);
    for (unsigned long long group = count - 1ULL; group > 0ULL; group -= 1ULL)
    {
        mmgr_line_decimal(line, groups[group - 1ULL], SIM_DECIMAL_GROUP_DIGITS);
    }
}

// the ratio truncated, not rounded, to `places`, at any width that the exact integer supports. The line takes the
// whole part exact. The fraction is the floor of the remainder times 10^places over the denominator, below 10^places
static inline void sim_ratio_print(VerbaLinea *line, const OriorExactInteger *numerator,
                                   const OriorExactInteger *denominator, unsigned int places)
{
    if ((denominator->sign == 0) || (places > 18u))
    {
        mmgr_line_put(line, "undefined");
        return;
    }
    OriorExactInteger top = *numerator;
    OriorExactInteger bottom = *denominator;
    const int negative = (top.sign * bottom.sign) < 0;
    top.sign = (top.sign == 0) ? 0 : 1;
    bottom.sign = 1;
    OriorExactInteger integer_part;
    OriorExactInteger rest;
    if ((orior_exact_divide(&top, &bottom, &integer_part, &rest) != ORIOR_EXACT_OK) ||
        ((places > 0u) && (orior_exact_scale_by_ten(&rest, places) != ORIOR_EXACT_OK)))
    {
        mmgr_line_put(line, "too wide");
        return;
    }
    if (negative)
    {
        mmgr_line_ch(line, '-');
    }
    sim_exact_decimal(line, &integer_part);
    if (places > 0u)
    {
        unsigned long long unit = 1ULL;
        for (unsigned int place = 0u; place < places; place += 1u)
        {
            unit *= 10ULL;
        }
        mmgr_line_ch(line, '.');
        mmgr_line_decimal(line, sim_ratio_floor(&rest, &bottom, unit), places);
    }
}

static inline void sim_fraction_print(VerbaLinea *line, unsigned long long numerator, unsigned long long denominator,
                                      unsigned int places)
{
    OriorExactInteger top;
    OriorExactInteger bottom;
    sim_exact_unsigned(&top, numerator);
    sim_exact_unsigned(&bottom, denominator);
    sim_ratio_print(line, &top, &bottom, places);
}

static inline int sim_status_check(SimResults *results, cudaError_t status, const char *what)
{
    if (status != cudaSuccess)
    {
        // the check takes the last error of the runtime here, after the report. The next launch check does not read it
        // again
        (void)cudaGetLastError();
        sim_check(results, 0, what);
        mmgr_line_put(&results->line, "    cuda: ");
        mmgr_line_put(&results->line, cudaGetErrorString(status));
        mmgr_line_ch(&results->line, '\n');
        return 0;
    }
    return 1;
}

static inline unsigned long long sim_launch_blocks(unsigned long long count, unsigned long long threads)
{
    return (count + threads - 1ULL) / threads;
}

static inline EngineSignum sim_content(const unsigned short *lanes, unsigned long long count, unsigned long long key)
{
    EngineSignum content;
    for (unsigned int word = 0u; word < (ENGINE_SIGNUM_BYTES / 8u); word += 1u)
    {
        unsigned long long running = sim_mix(key + word);
        for (unsigned long long lane = word; lane < count; lane += (ENGINE_SIGNUM_BYTES / 8u))
        {
            running = sim_mix(running ^ lanes[lane]);
        }
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            // one byte of the running word, taken from the bottom after the shift
            content.bytes[(word * 8u) + byte] = (unsigned char)(running >> (8u * byte));
        }
    }
    return content;
}

#endif
