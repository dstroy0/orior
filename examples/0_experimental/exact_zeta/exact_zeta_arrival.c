// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-024
/**
 * @file exact_zeta_arrival.c
 * @brief What each wave lands in where it joins the main sum, read at the boundaries t = 2pi n^2 by the device.
 *
 * @note Run build_exact_zeta.sh arrival [n0 ...], beside this file. Run build_exact_zeta.sh arrival stretch <n0>
 *       <windows> for a stretch of windows.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number theory,
 *       on its rail: it claims nothing about the Riemann hypothesis.
 * @note The boundary: the main sum of Riemann-Siegel is a sum of waves m^(-1/2) e^(i(theta - t ln m)). In the frame
 *       where e^(i theta) turns, wave m spins at ln(x / m), with x = sqrt(t / 2pi). It is stationary at x = m, and
 *       joins the sum there, at t = 2pi n^2 for n = m.
 * @note There its phase is theta - t ln n = -pi n^2 - pi / 8 to the first terms of theta. That is -pi / 8 for even
 *       n, and pi - pi / 8 for odd n. The waves already there, m < n, are at 2pi n^2 ln m modulo a turn.
 * @note The reading at a boundary is the angle between the new wave and their sum. That sum is V_n, the sum
 *       over m < n of m^(-1/2) e^(-2pi i n^2 ln m). The own phase of the new wave turns it:
 *       P_n = V_n e^(2pi i n^2 ln n). theta turns both alike, and drops out.
 * @note The relation: along n, the phase n^2 ln m of wave m has the constant second difference 2 ln m. A lane steps
 *       from one boundary to the next by u <- u r and r <- r q. Here u = e^(-2pi i n^2 ln m),
 *       r = e^(-2pi i (2n + 1) ln m), and q = e^(-4pi i ln m).
 * @note exact_zeta_arrival.cu runs that on the device, one lane a wave, and sums every boundary across its lanes with
 *       cycle_record_sum. At a fixed n0, u, r, q and m^(-1/2) are completely multiplicative in m. Each comes at the
 *       primes, from ln p and the phase n0^2 ln p reduced modulo a turn as an exact integer. Each composite m is the
 *       product over its least prime factor. Only the primes ask for a cosine and a sine.
 * @note The mean: the reading of each boundary is the unit pair P_n / |P_n| at the scale S = 2^62. The tally is
 *       their exact sum, and the run is their count. The mean at every boundary is tally / run, an exact rational.
 *       The tally merges, rounds and drops nothing, and the mean of every boundary stays.
 * @note The spread is |tally|^2 / (run S^2), an exact rational. It is the run where every reading points one way,
 *       and near 1 where the readings spread like independent angles.
 * @note With stretch, windows run end to end from n0, each seeded at its own first boundary. Every reading goes into
 *       one tally over the whole stretch, beside the tally of its own window. The host sums the first and last
 *       boundaries of the stretch directly.
 * @note Positive control: a window from n0 = 0 against V_n summed directly on the host, with the phase of every wave
 *       from its own logarithm. The gap is in units of 2^-62, and every first and last boundary of a window runs the
 *       same way. The bar: the V_n of the device meets the direct sum within 2^-40 at every boundary checked.
 */
#include "zeta_exact.h"
#include "../../../src/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The bits of the scale S of a reading. */
#define SCALE_BITS 62

/** @brief The bits of the work scale of the seeds. */
#define WORK_BITS 96

/** @brief The boundaries of one sweep of the device. */
#define FLOORS 16

/** @brief The boundaries of one window. */
#define WINDOW 1024

/** @brief The most lanes that the host checks against the device. */
#define CHECKED 4096

/** @brief The bits of the bar: a gap under 2^(SCALE_BITS - BAR_BITS) passes. */
#define BAR_BITS 40

/** @brief The device program, and its two sources. */
#define DEVICE_BUILD "examples/0_experimental/exact_zeta_arrival.sh"
#define DEVICE_SOURCE "examples/0_experimental/exact_zeta_arrival.cu"

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int value)
{
    return value ? "True" : "False";
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *arrival_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief The seconds of the wall clock, for the times that the report prints.
 *
 * @return The seconds.
 */
static double seconds_now(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (double)now.tv_sec + ((double)now.tv_nsec / 1e9);
}

/**
 * @brief The decimal places for the phases: n0^2 ln m keeps 30 places past its whole turns.
 *
 * @param[in] n0 The first boundary.
 * @return       The places.
 */
static int32_t digits_for(int64_t n0)
{
    char text[32];
    snprintf(text, sizeof(text), "%lld", (long long)((n0 > 1) ? n0 : 1));
    // The text of a machine integer has at most 20 characters.
    return 30 + (2 * (int32_t)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR));
}

/**
 * @brief An integer times 2^bits.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[in]  bits   The shift.
 * @param[out] result The product [BORROWS].
 */
static void shift_left(const OriorExactInteger *value, int64_t bits, OriorExactInteger *result)
{
    OriorExactInteger power;
    z_two_to(bits, &power);
    z_mul(value, &power, result);
}

/**
 * @brief An integer shifted right by bits, rounded toward minus infinity, as >> of Python.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[in]  bits   The shift.
 * @param[out] result The quotient [BORROWS].
 */
static void shift_right(const OriorExactInteger *value, int64_t bits, OriorExactInteger *result)
{
    OriorExactInteger power;
    z_two_to(bits, &power);
    z_floor_div(value, &power, result);
}

/**
 * @brief e^(-2pi i turns / 10^digits) at the scale 2^bits, from the cosine and the sine at the digits.
 *
 * @param[in]  turns  The turns times 10^digits [BORROWS].
 * @param[in]  digits The places.
 * @param[in]  bits   The scale.
 * @param[out] unit   The real and imaginary parts [BORROWS].
 */
static void unit_at(const OriorExactInteger *turns, int32_t digits, int64_t bits, OriorExactInteger unit[2])
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger angle;
    zeta_pi(digits, &angle);
    z_mul_small(&angle, 2, &angle);
    z_mul(&angle, turns, &angle);
    z_floor_div(&angle, &scale, &angle);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&angle, digits, &cosine, &sine);
    shift_left(&cosine, bits, &unit[0]);
    z_floor_div(&unit[0], &scale, &unit[0]);
    shift_left(&sine, bits, &unit[1]);
    z_floor_div(&unit[1], &scale, &unit[1]);
    z_neg(&unit[1], &unit[1]);
}

/**
 * @brief The product of two complex values at the scale 2^bits.
 *
 * @param[in]  left   First factor [BORROWS].
 * @param[in]  right  Second factor [BORROWS].
 * @param[in]  bits   The scale.
 * @param[out] result The product [BORROWS].
 */
static void times(const OriorExactInteger left[2], const OriorExactInteger right[2], int64_t bits,
                  OriorExactInteger result[2])
{
    OriorExactInteger first;
    OriorExactInteger second;
    OriorExactInteger real;
    OriorExactInteger imaginary;
    z_mul(&left[0], &right[0], &first);
    z_mul(&left[1], &right[1], &second);
    z_sub(&first, &second, &real);
    z_mul(&left[0], &right[1], &first);
    z_mul(&left[1], &right[0], &second);
    z_add(&first, &second, &imaginary);
    shift_right(&real, bits, &result[0]);
    shift_right(&imaginary, bits, &result[1]);
}

/**
 * @brief The phase n^2 ln m modulo a turn, n^2 ln m mod 10^digits, as an exact integer at the digits.
 *
 * @param[in]  factor The factor before ln m, such as n^2.
 * @param[in]  m      The wave.
 * @param[in]  digits The places.
 * @param[out] turns  The value [BORROWS].
 */
static void phase_turns(const OriorExactInteger *factor, int64_t m, int32_t digits, OriorExactInteger *turns)
{
    OriorExactInteger log;
    zeta_ln(m, digits, &log);
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    z_mul(factor, &log, turns);
    z_mod(turns, &scale, turns);
}

/**
 * @brief The seeds u, r, q and m^(-1/2) for m from 1 to top at n0, each at 2^SCALE_BITS.
 *
 * @param[in] n0  The first boundary.
 * @param[in] top The last wave.
 * @return        Seven machine integers per wave [OWNS].
 * @note The primes come from ln p. Each composite is the product over its least prime factor, worked at
 *       2^WORK_BITS.
 */
static int64_t *seeds(int64_t n0, int64_t top)
{
    const int32_t digits = digits_for(n0);
    // The sieve of least prime factors.
    int64_t *const least = (int64_t *)arrival_memory(
        platform_cell_taken(((size_t)top + 1u) * sizeof(int64_t), platform_place_g_program_owner, 0));
    for (int64_t m = 0; m <= top; m += 1)
    {
        least[m] = m;
    }
    for (int64_t p = 2; (p * p) <= top; p += 1)
    {
        if (least[p] == p)
        {
            for (int64_t multiple = p * p; multiple <= top; multiple += p)
            {
                if (least[multiple] == multiple)
                {
                    least[multiple] = p;
                }
            }
        }
    }
    // Each wave keeps u, r, q (two parts each) and m^(-1/2) at the work scale.
    OriorExactInteger(*const kept)[7] = (OriorExactInteger(*)[7])arrival_memory(
        platform_cell_taken(((size_t)top + 1u) * sizeof(OriorExactInteger[7]), platform_place_g_program_owner, 0));
    OriorExactInteger one;
    z_two_to(WORK_BITS, &one);
    for (unsigned part = 0u; part < 7u; part += 1u)
    {
        orior_exact_zero(&kept[1][part]);
    }
    kept[1][0] = one;
    kept[1][2] = one;
    kept[1][4] = one;
    kept[1][6] = one;
    OriorExactInteger n0_square;
    OriorExactInteger n0_odd;
    OriorExactInteger two;
    z_set(&n0_square, n0 * n0);
    z_set(&n0_odd, (2 * n0) + 1);
    z_set(&two, 2);
    for (int64_t m = 2; m <= top; m += 1)
    {
        const int64_t p = least[m];
        if (p == m)
        {
            OriorExactInteger turns;
            phase_turns(&n0_square, p, digits, &turns);
            unit_at(&turns, digits, WORK_BITS, &kept[m][0]);
            phase_turns(&n0_odd, p, digits, &turns);
            unit_at(&turns, digits, WORK_BITS, &kept[m][2]);
            phase_turns(&two, p, digits, &turns);
            unit_at(&turns, digits, WORK_BITS, &kept[m][4]);
            OriorExactInteger square;
            z_mul(&one, &one, &square);
            OriorExactInteger prime;
            z_set(&prime, p);
            z_floor_div(&square, &prime, &square);
            z_isqrt(&square, &kept[m][6]);
        }
        else
        {
            const int64_t rest = m / p;
            times(&kept[p][0], &kept[rest][0], WORK_BITS, &kept[m][0]);
            times(&kept[p][2], &kept[rest][2], WORK_BITS, &kept[m][2]);
            times(&kept[p][4], &kept[rest][4], WORK_BITS, &kept[m][4]);
            z_mul(&kept[p][6], &kept[rest][6], &kept[m][6]);
            shift_right(&kept[m][6], WORK_BITS, &kept[m][6]);
        }
    }
    int64_t *const rows = (int64_t *)arrival_memory(
        platform_cell_taken((size_t)top * 7u * sizeof(int64_t), platform_place_g_program_owner, 0));
    for (int64_t m = 1; m <= top; m += 1)
    {
        for (unsigned part = 0u; part < 7u; part += 1u)
        {
            OriorExactInteger value;
            shift_right(&kept[m][part], WORK_BITS - SCALE_BITS, &value);
            rows[((size_t)(m - 1) * 7u) + part] = z_small(&value);
        }
    }
    platform_cell_released(kept, platform_place_g_program_owner);
    platform_cell_released(least, platform_place_g_program_owner);
    return rows;
}

/** @brief V_n at each boundary of a device run, by n. */
typedef struct
{
    int64_t first;                 /**< The first n. */
    size_t count;                  /**< How many boundaries the table spans. */
    unsigned char *present;        /**< Which n have a value [OWNS]. */
    OriorExactInteger (*value)[2]; /**< The value at each n [OWNS]. */
} BoundaryValues;

/**
 * @brief The device run: V_n from n0 over sweeps * FLOORS boundaries, every wave from 1 to top a lane.
 *
 * @param[in]  binary The device program [BORROWS].
 * @param[in]  n0     The first boundary.
 * @param[in]  top    The last wave.
 * @param[in]  sweeps How many sweeps.
 * @param[out] values The values [BORROWS].
 * @param[out] same   Whether the records of the host for the first sweep equal those of the device [BORROWS].
 * @param[out] steps  The steps line of the device [BORROWS]. 256 bytes.
 */
static void run_device(const char *binary, int64_t n0, int64_t top, int64_t sweeps, BoundaryValues *values, int *same,
                       char steps[256])
{
    char given[256];
    char taken[256];
    snprintf(given, sizeof(given), "build/arrival_in_%lld.bin", (long long)n0);
    snprintf(taken, sizeof(taken), "build/arrival_out_%lld.txt", (long long)n0);
    FILE *const input = fopen(given, "wb");
    if (input == NULL)
    {
        zeta_stop("cannot write the input of the device");
    }
    const int64_t header[6] = {top, n0, sweeps, FLOORS, (top < CHECKED) ? top : CHECKED, top};
    fwrite(header, sizeof(int64_t), 6u, input);
    int64_t *const rows = seeds(n0, top);
    fwrite(rows, sizeof(int64_t), (size_t)top * 7u, input);
    platform_cell_released(rows, platform_place_g_program_owner);
    fclose(input);
    char command[1024];
#ifdef _WIN32
    snprintf(command, sizeof(command), "\"\"%s\" \"%s\" \"%s\" >NUL 2>&1\"", binary, given, taken);
#else
    snprintf(command, sizeof(command), "\"%s\" \"%s\" \"%s\" >/dev/null 2>&1", binary, given, taken);
#endif
    (void)system(command);

    FILE *const output = fopen(taken, "r");
    if (output == NULL)
    {
        zeta_stop("the device wrote no output");
    }
    values->first = n0;
    values->count = (size_t)(sweeps * FLOORS) + 2u;
    values->present =
        (unsigned char *)arrival_memory(platform_cell_cleared(values->count, 1u, platform_place_g_program_owner, 0));
    values->value = (OriorExactInteger(*)[2])arrival_memory(
        platform_cell_cleared(values->count, sizeof(OriorExactInteger[2]), platform_place_g_program_owner, 0));
    *same = 0;
    steps[0] = '\0';
    static char line[65536];
    int limbs = 0;
    if (fgets(line, sizeof(line), output) == NULL || sscanf(line, "%*s %d", &limbs) != 1)
    {
        zeta_stop("the device output has no width");
    }
    while (fgets(line, sizeof(line), output) != NULL)
    {
        line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "\r\n")] = '\0';
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "host",
                       .ci = EMBED_FALSE))
        {
            *same = atoi(line + 5);
            continue;
        }
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "steps", .ci = EMBED_FALSE))
        {
            snprintf(steps, 256u, "%.255s", line);
            continue;
        }
        char *cursor = line;
        const int64_t n = strtoll(cursor, &cursor, 10);
        if ((n < n0) || ((size_t)(n - n0) >= values->count))
        {
            continue;
        }
        for (unsigned part = 0u; part < 2u; part += 1u)
        {
            // The words are the two's complement of the value, least significant word first.
            OriorExactInteger value;
            orior_exact_zero(&value);
            uint32_t words[64];
            for (int at = 0; at < limbs; at += 1)
            {
                words[at] = (uint32_t)strtoul(cursor, &cursor, 16);
            }
            const int negative = (words[limbs - 1] & 0x80000000u) != 0u;
            uint32_t carry = 1u;
            for (int at = 0; at < limbs; at += 1)
            {
                uint32_t word = words[at];
                if (negative)
                {
                    // Two's complement: invert, then add one.
                    const uint64_t sum = (uint64_t)(~word) + carry;
                    word = (uint32_t)(sum & 0xFFFFFFFFu);
                    carry = (uint32_t)(sum >> 32u);
                }
                value.limb[at] = word;
            }
            value.sign = 0;
            for (int at = 0; at < limbs; at += 1)
            {
                if (value.limb[at] != 0u)
                {
                    value.sign = negative ? -1 : 1;
                    break;
                }
            }
            values->value[n - n0][part] = value;
        }
        values->present[n - n0] = 1u;
    }
    fclose(output);
}

/**
 * @brief The value at a boundary. A boundary with no value stops the program.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] n      The boundary.
 * @return           The value [BORROWS].
 */
static const OriorExactInteger *value_at(const BoundaryValues *values, int64_t n)
{
    if ((n < values->first) || ((size_t)(n - values->first) >= values->count) || !values->present[n - values->first])
    {
        zeta_stop("the device gave no value at a boundary");
    }
    return values->value[n - values->first];
}

/**
 * @brief Frees the values of a run.
 *
 * @param[in,out] values The values [BORROWS].
 */
static void values_free(BoundaryValues *values)
{
    platform_cell_released(values->present, platform_place_g_program_owner);
    platform_cell_released(values->value, platform_place_g_program_owner);
    values->present = NULL;
    values->value = NULL;
}

/**
 * @brief V_n summed on the host, with the phase of every wave from its own logarithm, at 2^SCALE_BITS.
 *
 * @param[in]  n      The boundary.
 * @param[in]  digits The places.
 * @param[out] result The real and imaginary parts [BORROWS].
 */
static void direct(int64_t n, int32_t digits, OriorExactInteger result[2])
{
    orior_exact_zero(&result[0]);
    orior_exact_zero(&result[1]);
    OriorExactInteger n_square;
    z_set(&n_square, n * n);
    OriorExactInteger full;
    z_two_to(2 * SCALE_BITS, &full);
    for (int64_t m = 1; m < n; m += 1)
    {
        OriorExactInteger turns;
        phase_turns(&n_square, m, digits, &turns);
        OriorExactInteger unit[2];
        unit_at(&turns, digits, SCALE_BITS, unit);
        OriorExactInteger share;
        OriorExactInteger wave;
        z_set(&wave, m);
        z_floor_div(&full, &wave, &share);
        OriorExactInteger amplitude;
        z_isqrt(&share, &amplitude);
        for (unsigned part = 0u; part < 2u; part += 1u)
        {
            OriorExactInteger term;
            z_mul(&unit[part], &amplitude, &term);
            shift_right(&term, SCALE_BITS, &term);
            z_add(&result[part], &term, &result[part]);
        }
    }
}

/**
 * @brief The largest gap between the device and the direct sum at a boundary, in units of 2^-62.
 *
 * @param[in]  values The values of the device [BORROWS].
 * @param[in]  n      The boundary.
 * @param[in]  digits The places.
 * @param[out] gap    The gap [BORROWS].
 */
static void gap_at(const BoundaryValues *values, int64_t n, int32_t digits, OriorExactInteger *gap)
{
    OriorExactInteger host[2];
    direct(n, digits, host);
    const OriorExactInteger *const device = value_at(values, n);
    OriorExactInteger first;
    OriorExactInteger second;
    z_sub(&device[0], &host[0], &first);
    z_abs(&first, &first);
    z_sub(&device[1], &host[1], &second);
    z_abs(&second, &second);
    *gap = (z_cmp(&first, &second) >= 0) ? first : second;
}

/** @brief The exact sum of the unit readings, their count, and the mean at every boundary. */
typedef struct
{
    OriorExactInteger re;         /**< The real part of the tally. */
    OriorExactInteger im;         /**< The imaginary part of the tally. */
    int64_t run;                  /**< The count. */
    int64_t first;                /**< The first boundary that holds a mean. */
    size_t count;                 /**< How many boundaries the means span. */
    unsigned char *present;       /**< Which boundaries hold a mean [OWNS]. */
    OriorExactInteger (*mean)[2]; /**< The tally at each boundary [OWNS]. */
    int64_t *runs;                /**< The run at each boundary [OWNS]. */
} Tally;

/**
 * @brief An empty tally over the boundaries first .. first + count - 1.
 *
 * @param[out] tally The tally [BORROWS].
 * @param[in]  first The first boundary.
 * @param[in]  count How many boundaries.
 */
static void tally_begin(Tally *tally, int64_t first, size_t count)
{
    orior_exact_zero(&tally->re);
    orior_exact_zero(&tally->im);
    tally->run = 0;
    tally->first = first;
    tally->count = count;
    tally->present =
        (unsigned char *)arrival_memory(platform_cell_cleared(count + 1u, 1u, platform_place_g_program_owner, 0));
    tally->mean = (OriorExactInteger(*)[2])arrival_memory(
        platform_cell_cleared(count + 1u, sizeof(OriorExactInteger[2]), platform_place_g_program_owner, 0));
    tally->runs = (int64_t *)arrival_memory(
        platform_cell_cleared(count + 1u, sizeof(int64_t), platform_place_g_program_owner, 0));
}

/**
 * @brief Frees a tally.
 *
 * @param[in,out] tally The tally [BORROWS].
 */
static void tally_free(Tally *tally)
{
    platform_cell_released(tally->present, platform_place_g_program_owner);
    platform_cell_released(tally->mean, platform_place_g_program_owner);
    platform_cell_released(tally->runs, platform_place_g_program_owner);
}

/**
 * @brief Reads one boundary into the tally: P_n / |P_n| at the scale, added, and the mean kept.
 *
 * @param[in,out] tally  The tally [BORROWS].
 * @param[in]     n      The boundary.
 * @param[in]     v      V_n [BORROWS].
 * @param[in]     digits The places.
 */
static void tally_read(Tally *tally, int64_t n, const OriorExactInteger v[2], int32_t digits)
{
    // The phase of the new wave, e^(+2pi i n^2 ln n): the conjugate of unit_at.
    OriorExactInteger n_square;
    z_set(&n_square, n * n);
    OriorExactInteger turns;
    phase_turns(&n_square, n, digits, &turns);
    OriorExactInteger unit[2];
    unit_at(&turns, digits, SCALE_BITS, unit);
    OriorExactInteger first;
    OriorExactInteger second;
    OriorExactInteger p_re;
    OriorExactInteger p_im;
    z_mul(&v[0], &unit[0], &first);
    z_mul(&v[1], &unit[1], &second);
    z_add(&first, &second, &p_re);
    shift_right(&p_re, SCALE_BITS, &p_re);
    z_mul(&v[1], &unit[0], &first);
    z_mul(&v[0], &unit[1], &second);
    z_sub(&first, &second, &p_im);
    shift_right(&p_im, SCALE_BITS, &p_im);
    OriorExactInteger size;
    z_mul(&p_re, &p_re, &first);
    z_mul(&p_im, &p_im, &second);
    z_add(&first, &second, &size);
    z_isqrt(&size, &size);
    if (size.sign == 0)
    {
        z_set(&size, 1);
    }
    OriorExactInteger part;
    shift_left(&p_re, SCALE_BITS, &part);
    z_floor_div(&part, &size, &part);
    z_add(&tally->re, &part, &tally->re);
    shift_left(&p_im, SCALE_BITS, &part);
    z_floor_div(&part, &size, &part);
    z_add(&tally->im, &part, &tally->im);
    tally->run += 1;
    if ((n >= tally->first) && ((size_t)(n - tally->first) < tally->count))
    {
        tally->present[n - tally->first] = 1u;
        tally->mean[n - tally->first][0] = tally->re;
        tally->mean[n - tally->first][1] = tally->im;
        tally->runs[n - tally->first] = tally->run;
    }
}

/**
 * @brief Whether the tally holds a mean at a boundary.
 *
 * @param[in] tally The tally [BORROWS].
 * @param[in] n     The boundary.
 * @return          1 where it does.
 */
static int tally_has(const Tally *tally, int64_t n)
{
    return (n >= tally->first) && ((size_t)(n - tally->first) < tally->count) && tally->present[n - tally->first];
}

/**
 * @brief Writes |tally|^2 / (run S^2) at a boundary, at the places, into a buffer.
 *
 * @param[in]  tally    The tally [BORROWS].
 * @param[in]  n        The boundary.
 * @param[in]  places   The places.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void spread_at(const Tally *tally, int64_t n, int32_t places, char *text, size_t capacity)
{
    const OriorExactInteger *const mean = tally->mean[n - tally->first];
    OriorExactInteger first;
    OriorExactInteger second;
    z_mul(&mean[0], &mean[0], &first);
    z_mul(&mean[1], &mean[1], &second);
    z_add(&first, &second, &first);
    zeta_require(orior_exact_scale_by_ten(&first, (uint32_t)places), "scale");
    OriorExactInteger under;
    z_set(&under, tally->runs[n - tally->first]);
    shift_left(&under, 2 * SCALE_BITS, &under);
    z_floor_div(&first, &under, &first);
    DecimalPair pair;
    pair_make(&first, places, &pair);
    pair_text(&pair, places, text, capacity);
}

/**
 * @brief An exact integer as the nearest double, ties to even, as the conversion of Python rounds.
 *
 * @param[in] value The integer [BORROWS].
 * @return          The double.
 */
static double to_double(const OriorExactInteger *value)
{
    const int64_t bits = z_bit_length(value);
    if (bits <= 53)
    {
        return (double)z_small(value);
    }
    OriorExactInteger size;
    z_abs(value, &size);
    OriorExactInteger top;
    shift_right(&size, bits - 54, &top);
    // The top 54 bits fit a machine integer. The last bit and the rest decide the rounding to 53 bits.
    int64_t mantissa = z_small(&top);
    OriorExactInteger back;
    shift_left(&top, bits - 54, &back);
    const int rest = z_cmp(&back, &size) != 0;
    const int half = (int)(mantissa & 1);
    mantissa >>= 1;
    if (half && (rest || ((mantissa & 1) != 0)))
    {
        mantissa += 1;
    }
    // The shift is at least zero, and fits an int.
    const double magnitude = ldexp((double)mantissa, (int)(bits - 53));
    return (value->sign < 0) ? -magnitude : magnitude;
}

/**
 * @brief Finds the newest device program newer than its sources, else builds one.
 *
 * @param[out] binary The path of the program [BORROWS]. 512 bytes.
 */
static void find_binary(char binary[512])
{
    struct stat facts;
    double newest_source = 0.0;
    if (stat(DEVICE_BUILD, &facts) == 0)
    {
        newest_source = (double)facts.st_mtime;
    }
    if ((stat(DEVICE_SOURCE, &facts) == 0) && ((double)facts.st_mtime > newest_source))
    {
        newest_source = (double)facts.st_mtime;
    }
    binary[0] = '\0';
    double newest = 0.0;
    DIR *const folder = opendir("build");
    if (folder != NULL)
    {
        const char *const tail = "_exact_zeta_arrival";
        struct dirent *entry;
        while ((entry = readdir(folder)) != NULL)
        {
            const size_t length =
                EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = entry->d_name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
            if ((length < EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = tail, .cap = PLATFORM_TEXT_TO_TERMINATOR)) ||
                (!EMBED_CALL(
                    cellul.eq, CatenaFinitaCfg,
                    .src = entry->d_name + length -
                           EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = tail, .cap = PLATFORM_TEXT_TO_TERMINATOR),
                    .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = tail, .ci = EMBED_FALSE)))
            {
                continue;
            }
            static const char *const names[2] = {"exact_zeta_arrival.exe", "exact_zeta_arrival"};
            for (unsigned which = 0u; which < 2u; which += 1u)
            {
                char path[512];
                snprintf(path, sizeof(path), "build/%s/%s", entry->d_name, names[which]);
                if ((stat(path, &facts) == 0) && ((double)facts.st_mtime > newest))
                {
                    newest = (double)facts.st_mtime;
                    snprintf(binary, 512u, "%s", path);
                }
            }
        }
        closedir(folder);
    }
    if ((binary[0] != '\0') && (newest > newest_source))
    {
        return;
    }
    binary[0] = '\0';
    FILE *const made = popen("bash " DEVICE_BUILD, "r");
    if (made == NULL)
    {
        zeta_stop("cannot run the build of the device program");
    }
    char line[512];
    while (fgets(line, sizeof(line), made) != NULL)
    {
        line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "\r\n")] = '\0';
        if (line[0] != '\0')
        {
            snprintf(binary, 512u, "%s", line);
        }
    }
    if ((pclose(made) != 0) || (binary[0] == '\0'))
    {
        zeta_stop("the build of the device program failed");
    }
}

/**
 * @brief The windows from each n0, after the control from n0 = 0.
 *
 * @param[in] out     The stream [BORROWS].
 * @param[in] windows The first boundary of each window [BORROWS].
 * @param[in] count   How many windows.
 * @return            The exit code: 0 where every check lands.
 */
static int main_windows(FILE *out, const int64_t *windows, size_t count)
{
    fputs("  what each wave lands in where it joins the main sum, at t = 2pi n^2, on the device\n", out);
    fputs("  this reads angles at boundaries and claims nothing about the Riemann hypothesis\n\n", out);
    char binary[512];
    find_binary(binary);
    fprintf(out, "  binary %s\n", binary);

    // Positive control: from n0 = 0, against the direct sum.
    double begun = seconds_now();
    const int64_t sweeps = 8;
    int64_t top = sweeps * FLOORS;
    BoundaryValues values;
    int same = 0;
    char steps[256];
    run_device(binary, 0, top, sweeps, &values, &same, steps);
    int32_t digits = digits_for(top);
    OriorExactInteger worst;
    orior_exact_zero(&worst);
    for (int64_t n = 2; n < top; n += 17)
    {
        OriorExactInteger gap;
        gap_at(&values, n, digits, &gap);
        worst = (z_cmp(&gap, &worst) > 0) ? gap : worst;
    }
    values_free(&values);
    OriorExactInteger bar;
    z_two_to(SCALE_BITS - BAR_BITS, &bar);
    const int held = z_cmp(&worst, &bar) < 0;
    fprintf(out, "  control: n0 = 0, %lld boundaries, V_n against the direct sum at every 17th: largest gap ",
            (long long)top);
    z_print(&worst, out);
    fprintf(out, " units of 2^-62, within 2^-%d: %s; host equals device on the first sweep: %s; %s; %.1fs\n\n",
            BAR_BITS, truth(held), truth(same), steps, seconds_now() - begun);
    fflush(out);

    int every = held && same;
    for (size_t which = 0u; which < count; which += 1u)
    {
        const int64_t n0 = windows[which];
        begun = seconds_now();
        top = n0 + WINDOW;
        run_device(binary, n0, top, WINDOW / FLOORS, &values, &same, steps);
        Tally tally;
        tally_begin(&tally, n0, WINDOW + 2u);
        digits = digits_for(top);
        for (int64_t n = (n0 > 2) ? n0 : 2; n < (n0 + WINDOW); n += 1)
        {
            tally_read(&tally, n, value_at(&values, n), digits);
        }
        static const int64_t marks[4] = {16, 64, 256, 1024};
        char text[1024];
        size_t used = 0u;
        text[0] = '\0';
        for (unsigned mark = 0u; mark < 4u; mark += 1u)
        {
            const int64_t n = n0 + marks[mark] - 1;
            if (!tally_has(&tally, n))
            {
                continue;
            }
            char spread[64];
            spread_at(&tally, n, 4, spread, sizeof(spread));
            used += (size_t)snprintf(text + used, sizeof(text) - used, "%srun %lld: %s", (used > 0u) ? "  " : "",
                                     (long long)tally.runs[n - tally.first], spread);
        }
        const double spent = seconds_now() - begun;
        // The second route of the window: its first and last boundaries, summed directly on the host.
        const int64_t first_n = (n0 > 2) ? n0 : 2;
        OriorExactInteger gaps[2];
        gap_at(&values, first_n, digits, &gaps[0]);
        gap_at(&values, n0 + WINDOW - 1, digits, &gaps[1]);
        const OriorExactInteger *const larger = (z_cmp(&gaps[0], &gaps[1]) >= 0) ? &gaps[0] : &gaps[1];
        const int far = z_cmp(larger, &bar) < 0;
        fprintf(out,
                "  n0 = %lld: %lld waves, boundaries %lld to %lld; |tally|^2 / (run S^2) at %s; host equals device: "
                "%s; first and last boundary against the direct sum [",
                (long long)n0, (long long)top, (long long)first_n, (long long)(n0 + WINDOW - 1), text, truth(same));
        z_print(&gaps[0], out);
        fputs(", ", out);
        z_print(&gaps[1], out);
        fprintf(out, "] units of 2^-62, within 2^-%d: %s; %s; %.1fs on the device and the readings\n", BAR_BITS,
                truth(far), steps, spent);
        fflush(out);
        every = every && same && far;
        tally_free(&tally);
        values_free(&values);
    }
    return every ? 0 : 1;
}

/**
 * @brief A count of windows end to end from n0, each seeded at its own first boundary.
 *
 * @param[in] out   The stream [BORROWS].
 * @param[in] n0    The first boundary.
 * @param[in] count How many windows.
 * @return          The exit code: 0 where every check lands.
 * @note Every reading goes into one tally over the whole stretch, beside the tally of its own window.
 */
static int main_stretch(FILE *out, int64_t n0, int64_t count)
{
    char binary[512];
    find_binary(binary);
    fprintf(out, "  a stretch of %lld windows from n0 = %lld, binary %s\n", (long long)count, (long long)n0, binary);
    Tally whole;
    tally_begin(&whole, n0, (size_t)(count * WINDOW) + 2u);
    double *const spreads = (double *)arrival_memory(
        platform_cell_cleared((size_t)count + 1u, sizeof(double), platform_place_g_program_owner, 0));
    int every = 1;
    const double begun = seconds_now();
    OriorExactInteger bar;
    z_two_to(SCALE_BITS - BAR_BITS, &bar);
    for (int64_t j = 0; j < count; j += 1)
    {
        const int64_t start = n0 + (j * WINDOW);
        const int64_t top = start + WINDOW;
        BoundaryValues values;
        int same = 0;
        char steps[256];
        run_device(binary, start, top, WINDOW / FLOORS, &values, &same, steps);
        Tally window;
        tally_begin(&window, start, WINDOW + 2u);
        const int32_t digits = digits_for(top);
        for (int64_t n = (start > 2) ? start : 2; n < top; n += 1)
        {
            tally_read(&window, n, value_at(&values, n), digits);
            tally_read(&whole, n, value_at(&values, n), digits);
        }
        OriorExactInteger first;
        OriorExactInteger second;
        z_mul(&window.re, &window.re, &first);
        z_mul(&window.im, &window.im, &second);
        z_add(&first, &second, &first);
        OriorExactInteger under;
        z_set(&under, window.run);
        shift_left(&under, 2 * SCALE_BITS, &under);
        spreads[j] = to_double(&first) / to_double(&under);
        every = every && same;
        if ((j == 0) || (j == (count - 1)))
        {
            const int64_t n = (j == 0) ? ((start > 2) ? start : 2) : (top - 1);
            OriorExactInteger gap;
            gap_at(&values, n, digits, &gap);
            const int within = z_cmp(&gap, &bar) < 0;
            every = every && within;
            fprintf(out, "  boundary %lld against the direct sum: ", (long long)n);
            z_print(&gap, out);
            fprintf(out, " units of 2^-62, within 2^-%d: %s\n", BAR_BITS, truth(within));
            fflush(out);
        }
        tally_free(&window);
        values_free(&values);
    }
    double total = 0.0;
    for (int64_t j = 0; j < count; j += 1)
    {
        total += spreads[j];
    }
    const double mean = total / (double)count;
    double squares = 0.0;
    double least = spreads[0];
    double most = spreads[0];
    int64_t below = 0;
    for (int64_t j = 0; j < count; j += 1)
    {
        squares += (spreads[j] - mean) * (spreads[j] - mean);
        least = (spreads[j] < least) ? spreads[j] : least;
        most = (spreads[j] > most) ? spreads[j] : most;
        below += (spreads[j] < 1.0) ? 1 : 0;
    }
    const double deviation = sqrt(squares / (double)count);
    fprintf(out,
            "  each window's spread at its last boundary: mean %.4f, deviation %.4f, least %.4f, max %.4f, %lld of %lld"
            " below 1; independent angles give 1 and 1\n",
            mean, deviation, least, most, (long long)below, (long long)count);
    const int64_t first = (n0 > 2) ? n0 : 2;
    int64_t marks[66];
    size_t mark_count = 0u;
    for (int64_t k = 0; (((int64_t)1) << k) <= count; k += 1)
    {
        marks[mark_count] = first + (WINDOW << k) - 1;
        mark_count += 1u;
    }
    marks[mark_count] = n0 + (count * WINDOW) - 1;
    mark_count += 1u;
    // The marks in ascending order, each once.
    for (size_t at = 1u; at < mark_count; at += 1u)
    {
        const int64_t moving = marks[at];
        size_t place = at;
        while ((place > 0u) && (moving < marks[place - 1u]))
        {
            marks[place] = marks[place - 1u];
            place -= 1u;
        }
        marks[place] = moving;
    }
    for (size_t at = 0u; at < mark_count; at += 1u)
    {
        if (((at > 0u) && (marks[at] == marks[at - 1u])) || !tally_has(&whole, marks[at]))
        {
            continue;
        }
        char spread[64];
        spread_at(&whole, marks[at], 4, spread, sizeof(spread));
        fprintf(out, "  the whole stretch at boundary %lld, run %lld: %s\n", (long long)marks[at],
                (long long)whole.runs[marks[at] - whole.first], spread);
        fflush(out);
    }
    fprintf(out, "  host equals device and both ends within the bar: %s; %.1fs\n", truth(every), seconds_now() - begun);
    platform_cell_released(spreads, platform_place_g_program_owner);
    tally_free(&whole);
    return every ? 0 : 1;
}

// The pool of the program. A window of 1000000 places keeps seven exact integers a place, which is more than 1 GB
const unsigned long long g_program_pool_bytes = 1ULL << 31u;

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    if ((argument_count > 3) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[1],
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "stretch", .ci = EMBED_FALSE)))
    {
        return main_stretch(out, atoll(arguments[2]), atoll(arguments[3]));
    }
    static const int64_t defaults[4] = {1000, 10000, 100000, 1000000};
    int64_t given[64];
    size_t count = 0u;
    for (int at = 1; (at < argument_count) && (count < 64u); at += 1)
    {
        given[count] = atoll(arguments[at]);
        count += 1u;
    }
    return (count > 0u) ? main_windows(out, given, count) : main_windows(out, defaults, 4u);
}
