// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-029
/**
 * @file exact_zeta_spherical.c
 * @brief The spherical function of Harish-Chandra on the hyperbolic plane, by two routes on the device.
 *
 * @note The grid is over the spectral parameter r and u = sinh^2(d / 2). The two routes must agree within a bound on
 *       each, and route one must meet the exact rational series at the corners of the grid.
 * @note Run build_exact_zeta.sh spherical <binary> [r_bits] [u_bits], beside this file. The script
 *       examples/0_experimental/exact_zeta_spherical.sh builds the device program, and prints its path last.
 * @note It sits in 0_experimental, and is an entry in the workbook of analytic number theory, on its rail.
 * @note The function: SL(2, R) acts on the upper half plane H by Mobius maps, SO(2) fixes i, and H = SL(2, R) / SO(2).
 *       A function on H, radial about i, an eigenfunction of the Laplacian with eigenvalue -(1/4 + r^2), and 1 at i, is
 *       the spherical function phi_r of Harish-Chandra.
 * @note At hyperbolic distance d from i, with u = sinh^2(d / 2), phi_r = 2F1(1/2 + i r, 1/2 - i r; 1; -u). The
 *       modular surface SL(2, Z)\H is a quotient of H. Its spectral theory reads every automorphic kernel through the
 *       phi_r, by the transform of Selberg.
 * @note Route one is the series in -u, real, with signs that alternate for u < 1. Route two is the transformation of
 *       Pfaff, phi_r = (1 + u)^(-1/2 - i r) 2F1(1/2 + i r, 1/2 + i r; 1; u / (1 + u)), a complex series. Its real part
 *       is route one, and its imaginary part is 0.
 * @note The bounds: every factor of the growth of a term rises with u and with r. For route one it is
 *       u ((n + 1/2)^2 + r^2) / (n + 1)^2. For route two it is z ((n + 1/2)^2 + r^2) / (n + 1)^2, with z = u / (1 + u).
 *       Thus the far corner of the grid bounds every lane.
 * @note Past the last term, the factor is below rho = u (1 + r^2 / (n + 1)^2), and the tail is below the last term
 *       over 1 - rho. At each step, the error of the device on a term grows by that factor. It also gains a few units
 *       of 2^-62 at most, as bounds() counts them.
 */
#include "zeta_exact.h"
#include "../../../src/platform/platform.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The bits of the scale of the device. */
#define SCALE_BITS 62

/** @brief The guard bits of the constants. */
#define GUARD_BITS 64

/** @brief The terms of the artanh series on the device. */
#define ARTANH_TERMS 21

/** @brief The terms of the cosine series on the device. */
#define COS_TERMS 18

/** @brief The Newton steps of the root on the device. */
#define NEWTON_STEPS 7

/** @brief The lanes that the host checks against the device. */
#define CHECKED 64

/** @brief The largest r of the grid. */
#define R_HIGH 8

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
static void *spherical_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief Writes an integer as the device reads it: a count of limbs, the limbs of the magnitude, and the sign.
 *
 * @param[in] handle The file [BORROWS].
 * @param[in] value  The integer [BORROWS].
 */
static void put(FILE *handle, const OriorExactInteger *value)
{
    const int64_t bits = z_bit_length(value);
    const int64_t count = (bits > 0) ? ((bits + 31) / 32) : 1;
    fwrite(&count, sizeof(int64_t), 1u, handle);
    // The limb count is at least one, and within the width.
    fwrite(value->limb, sizeof(uint32_t), (size_t)count, handle);
    const int64_t sign = value->sign;
    fwrite(&sign, sizeof(int64_t), 1u, handle);
}

/**
 * @brief The constants: 1 / pi, the artanh constants 1 / (2k + 1), and the cosine constants (-1)^k pi^(2k) / (2k)!.
 *
 * @param[out] inv_pi The value 1 / pi at 2^62 [BORROWS].
 * @param[out] artanh The artanh constants at 2^62 [BORROWS].
 * @param[out] cosine The cosine constants at 2^62 [BORROWS].
 */
static void constants(OriorExactInteger *inv_pi, OriorExactInteger artanh[ARTANH_TERMS],
                      OriorExactInteger cosine[COS_TERMS])
{
    OriorExactInteger work;
    z_two_to(SCALE_BITS + GUARD_BITS, &work);
    OriorExactInteger pi_value;
    zeta_pi_machin(&work, &pi_value);
    OriorExactInteger top;
    z_two_to((2 * SCALE_BITS) + GUARD_BITS, &top);
    z_floor_div(&top, &pi_value, inv_pi);
    OriorExactInteger unit;
    z_two_to(SCALE_BITS, &unit);
    for (int64_t k = 0; k < ARTANH_TERMS; k += 1)
    {
        OriorExactInteger odd;
        z_set(&odd, (2 * k) + 1);
        z_floor_div(&unit, &odd, &artanh[k]);
    }
    OriorExactInteger power = work;
    OriorExactInteger work_square;
    z_mul(&work, &work, &work_square);
    OriorExactInteger guard;
    z_two_to(GUARD_BITS, &guard);
    for (int64_t k = 0; k < COS_TERMS; k += 1)
    {
        OriorExactInteger factorial;
        z_set(&factorial, 1);
        for (int64_t j = 2; j <= (2 * k); j += 1)
        {
            z_mul_small(&factorial, j, &factorial);
        }
        OriorExactInteger value;
        z_floor_div(&power, &factorial, &value);
        z_floor_div(&value, &guard, &value);
        if ((k % 2) != 0)
        {
            value.sign = -value.sign;
        }
        cosine[k] = value;
        z_mul(&power, &pi_value, &power);
        z_mul(&power, &pi_value, &power);
        z_floor_div(&power, &work_square, &power);
    }
}

/**
 * @brief The factor of a term from n to n + 1: x ((n + 1/2)^2 + r^2) / (n + 1)^2.
 *
 * @param[in]  x      The argument [BORROWS].
 * @param[in]  r      The spectral parameter [BORROWS].
 * @param[in]  n      The index.
 * @param[out] result The factor [BORROWS].
 */
static void growth(const ZetaRational *x, const ZetaRational *r, int64_t n, ZetaRational *result)
{
    ZetaRational half;
    rational_small((2 * n) + 1, 2, &half);
    ZetaRational sum;
    rational_mul(&half, &half, &sum);
    ZetaRational r_square;
    rational_mul(r, r, &r_square);
    rational_add(&sum, &r_square, &sum);
    rational_mul(x, &sum, &sum);
    ZetaRational under;
    rational_small((n + 1) * (n + 1), 1, &under);
    rational_div(&sum, &under, result);
}

/**
 * @brief The least count of terms whose tail, at x and r, is below the need, with that tail and the largest partial
 *        sum.
 *
 * @param[in]  x       The argument [BORROWS].
 * @param[in]  r       The spectral parameter [BORROWS].
 * @param[in]  need    The bound on the tail [BORROWS].
 * @param[out] tail    The tail [BORROWS].
 * @param[out] largest The largest partial sum or term [BORROWS].
 * @return             The count of terms.
 */
static int64_t series_terms(const ZetaRational *x, const ZetaRational *r, const ZetaRational *need, ZetaRational *tail,
                            ZetaRational *largest)
{
    ZetaRational term;
    ZetaRational total;
    ZetaRational one;
    rational_small(1, 1, &term);
    rational_small(1, 1, &total);
    rational_small(1, 1, &one);
    *largest = one;
    ZetaRational r_square;
    rational_mul(r, r, &r_square);
    for (int64_t n = 0;; n += 1)
    {
        ZetaRational ratio;
        ZetaRational under;
        rational_small((n + 1) * (n + 1), 1, &under);
        rational_div(&r_square, &under, &ratio);
        rational_add(&one, &ratio, &ratio);
        ZetaRational rho;
        rational_mul(x, &ratio, &rho);
        if (rational_cmp(&rho, &one) < 0)
        {
            ZetaRational gap;
            rational_sub(&one, &rho, &gap);
            ZetaRational bound;
            rational_div(&term, &gap, &bound);
            if (rational_cmp(&bound, need) < 0)
            {
                *tail = bound;
                return n + 1;
            }
        }
        ZetaRational factor;
        growth(x, r, n, &factor);
        rational_mul(&term, &factor, &term);
        rational_add(&total, &term, &total);
        if (rational_cmp(&total, largest) > 0)
        {
            *largest = total;
        }
        if (rational_cmp(&term, largest) > 0)
        {
            *largest = term;
        }
    }
}

/** @brief The term counts of both routes, and the bound on the error of each route over the grid. */
typedef struct
{
    int64_t first;             /**< The terms of route one. */
    int64_t second;            /**< The terms of route two. */
    ZetaRational first_error;  /**< The bound of route one, in units of 2^-62. */
    ZetaRational second_error; /**< The bound of route two, in units of 2^-62. */
    ZetaRational largest;      /**< The largest partial sum of route one. */
} RouteBounds;

/**
 * @brief The term counts of both routes, and the bound on the error of each route over the grid, in units of 2^-62.
 *
 * @param[in] u_high The largest u of the grid [BORROWS].
 * @param[in] r_high The largest r of the grid [BORROWS].
 * @return           The bounds.
 * @note Term n + 1 of route one is three products taken back to the scale and one division, each within a unit,
 *       from term n. It grows the error of term n by u ((n + 1/2)^2 + r^2) / (n + 1)^2.
 * @note Term n + 1 of route two is a complex product, a product by z and a division, five units, from term n. It
 *       grows the error of term n by z ((n + 1/2) + r)^2 / (n + 1)^2.
 * @note ln(1 + u) by 21 terms of artanh in y <= 1/5 is within 8 units. r ln(1 + u) / pi is within 8 r + 3. The
 *       cosine and the sine are within 270 units plus pi times the error of the angle. The root of Newton from 4/5 is
 *       within 4.
 * @note The last two products add the errors of their factors times the size of the other factor, and a unit each.
 */
static RouteBounds bounds(const ZetaRational *u_high, const ZetaRational *r_high)
{
    RouteBounds result;
    OriorExactInteger top;
    OriorExactInteger under;
    z_set(&top, 1);
    z_two_to(64, &under);
    ZetaRational need;
    rational_make(&top, &under, &need);
    ZetaRational one;
    rational_small(1, 1, &one);
    ZetaRational z_high;
    ZetaRational denominator;
    rational_add(&one, u_high, &denominator);
    rational_div(u_high, &denominator, &z_high);
    ZetaRational first_tail;
    ZetaRational second_tail;
    ZetaRational ignored;
    result.first = series_terms(u_high, r_high, &need, &first_tail, &result.largest);
    result.second = series_terms(&z_high, r_high, &need, &second_tail, &ignored);
    ZetaRational unit;
    z_two_to(SCALE_BITS, &top);
    z_set(&under, 1);
    rational_make(&top, &under, &unit);

    ZetaRational error;
    ZetaRational total;
    rational_small(0, 1, &error);
    rational_small(0, 1, &total);
    ZetaRational four;
    rational_small(4, 1, &four);
    for (int64_t n = 0; n < result.first; n += 1)
    {
        rational_add(&total, &error, &total);
        ZetaRational factor;
        growth(u_high, r_high, n, &factor);
        rational_mul(&error, &factor, &error);
        rational_add(&error, &four, &error);
    }
    ZetaRational tail_units;
    rational_mul(&first_tail, &unit, &tail_units);
    rational_add(&total, &tail_units, &result.first_error);

    ZetaRational size;
    ZetaRational term;
    rational_small(0, 1, &error);
    rational_small(0, 1, &total);
    rational_small(0, 1, &size);
    rational_small(1, 1, &term);
    ZetaRational five;
    rational_small(5, 1, &five);
    for (int64_t n = 0; n < result.second; n += 1)
    {
        rational_add(&total, &error, &total);
        rational_add(&size, &term, &size);
        ZetaRational half;
        rational_small((2 * n) + 1, 2, &half);
        ZetaRational sum;
        rational_add(&half, r_high, &sum);
        rational_mul(&sum, &sum, &sum);
        rational_mul(&z_high, &sum, &sum);
        ZetaRational square;
        rational_small((n + 1) * (n + 1), 1, &square);
        ZetaRational grow;
        rational_div(&sum, &square, &grow);
        rational_mul(&error, &grow, &error);
        rational_add(&error, &five, &error);
        rational_mul(&term, &grow, &term);
    }
    ZetaRational turn_error;
    ZetaRational eight;
    rational_small(8, 1, &eight);
    rational_mul(&eight, r_high, &turn_error);
    ZetaRational three;
    rational_small(3, 1, &three);
    rational_add(&turn_error, &three, &turn_error);
    ZetaRational cis_error;
    rational_mul(&four, &turn_error, &cis_error);
    ZetaRational base;
    rational_small(270, 1, &base);
    rational_add(&cis_error, &base, &cis_error);
    ZetaRational turned_error;
    rational_mul(&cis_error, &size, &turned_error);
    rational_add(&turned_error, &total, &turned_error);
    ZetaRational two;
    rational_small(2, 1, &two);
    rational_mul(&turned_error, &two, &turned_error);
    rational_add(&turned_error, &two, &turned_error);
    ZetaRational second;
    rational_mul(&four, &size, &second);
    rational_mul(&second, &two, &second);
    rational_add(&turned_error, &second, &second);
    rational_add(&second, &one, &second);
    rational_mul(&two, &second_tail, &tail_units);
    rational_mul(&tail_units, &unit, &tail_units);
    rational_add(&second, &tail_units, &result.second_error);
    return result;
}

/**
 * @brief Route one exactly: the partial sum to a tail below the need, and that tail.
 *
 * @param[in]  u     The argument [BORROWS].
 * @param[in]  r     The spectral parameter [BORROWS].
 * @param[in]  need  The bound on the tail [BORROWS].
 * @param[out] total The partial sum [BORROWS].
 * @param[out] tail  The tail [BORROWS].
 */
static void exact_first(const ZetaRational *u, const ZetaRational *r, const ZetaRational *need, ZetaRational *total,
                        ZetaRational *tail)
{
    ZetaRational largest;
    const int64_t count = series_terms(u, r, need, tail, &largest);
    ZetaRational term;
    rational_small(1, 1, &term);
    rational_small(1, 1, total);
    for (int64_t n = 0; n < (count - 1); n += 1)
    {
        ZetaRational factor;
        growth(u, r, n, &factor);
        rational_mul(&term, &factor, &term);
        term.top.sign = -term.top.sign;
        rational_add(total, &term, total);
    }
}

/** @brief The results of the device run, with the bounds of the grid. */
typedef struct
{
    OriorExactInteger (
        *values)[3];    /**< Route one, route two, and the imaginary part of route two, at each lane [OWNS]. */
    size_t lanes;       /**< How many lanes. */
    int host;           /**< Whether the host records equal those of the device. */
    char steps[64];     /**< The steps of the device. */
    int64_t r_count;    /**< How many values of r. */
    int64_t u_count;    /**< How many values of u. */
    RouteBounds bounds; /**< The bounds of both routes. */
} SphericalRun;

/**
 * @brief Writes the input of the device, runs it, and reads its records.
 *
 * @param[in]  binary The device program [BORROWS].
 * @param[in]  r_bits The bits of the step of r.
 * @param[in]  u_bits The bits of the step of u.
 * @param[out] run    The results [BORROWS].
 */
static void run_device(const char *binary, int r_bits, int u_bits, SphericalRun *run)
{
    run->r_count = (int64_t)R_HIGH << r_bits;
    run->u_count = (int64_t)1 << (u_bits - 1);
    const int64_t lanes = run->r_count * run->u_count;
    run->lanes = (size_t)lanes;
    ZetaRational u_high;
    ZetaRational r_high;
    rational_small(run->u_count - 1, (int64_t)1 << u_bits, &u_high);
    rational_small(run->r_count - 1, (int64_t)1 << r_bits, &r_high);
    run->bounds = bounds(&u_high, &r_high);
    OriorExactInteger inv_pi;
    OriorExactInteger artanh[ARTANH_TERMS];
    OriorExactInteger cosine[COS_TERMS];
    constants(&inv_pi, artanh, cosine);

    const char *const given = "build/spherical_in.bin";
    const char *const taken = "build/spherical_out.txt";
    FILE *const input = fopen(given, "wb");
    if (input == NULL)
    {
        zeta_stop("cannot write the input of the device");
    }
    const int64_t header[8] = {lanes,
                               (lanes < CHECKED) ? lanes : CHECKED,
                               run->u_count,
                               ARTANH_TERMS,
                               COS_TERMS,
                               run->bounds.first,
                               run->bounds.second,
                               NEWTON_STEPS};
    fwrite(header, sizeof(int64_t), 8u, input);
    OriorExactInteger step;
    z_two_to(SCALE_BITS - u_bits, &step);
    put(input, &step);
    z_two_to(SCALE_BITS - r_bits, &step);
    put(input, &step);
    put(input, &inv_pi);
    for (int at = 0; at < ARTANH_TERMS; at += 1)
    {
        put(input, &artanh[at]);
    }
    for (int at = 0; at < COS_TERMS; at += 1)
    {
        put(input, &cosine[at]);
    }
    fclose(input);

    char command[1024];
#ifdef _WIN32
    (void)_putenv("CYCLE_RECORD_RETAIN_PTX=1");
    snprintf(command, sizeof(command), "\"\"%s\" \"%s\" \"%s\" >build\\spherical_device.txt 2>&1\"", binary, given,
             taken);
#else
    snprintf(command, sizeof(command),
             "CYCLE_RECORD_RETAIN_PTX=1 \"%s\" \"%s\" \"%s\" >build/spherical_device.txt 2>&1", binary, given, taken);
#endif
    if (system(command) != 0)
    {
        FILE *const log = fopen("build/spherical_device.txt", "r");
        if (log != NULL)
        {
            static char text[4096];
            const size_t length = fread(text, 1u, sizeof(text) - 1u, log);
            fclose(log);
            text[length] = '\0';
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, text);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        zeta_stop("the device run failed");
    }

    FILE *const output = fopen(taken, "r");
    if (output == NULL)
    {
        zeta_stop("the device wrote no output");
    }
    int64_t offsets[3];
    int64_t widths[3];
    size_t place_count = 0u;
    run->values = (OriorExactInteger(*)[3])spherical_memory(calloc(run->lanes + 1u, sizeof(OriorExactInteger[3])));
    size_t row = 0u;
    run->host = 0;
    run->steps[0] = '\0';
    static char line[65536];
    while (fgets(line, sizeof(line), output) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char first[32] = {0};
        if (sscanf(line, "%31s", first) != 1)
        {
            continue;
        }
        if (strcmp(first, "output") == 0)
        {
            long long offset = 0;
            long long width = 0;
            if ((sscanf(line, "%*s %*s %lld %lld", &offset, &width) == 2) && (place_count < 3u))
            {
                offsets[place_count] = offset;
                widths[place_count] = width;
                place_count += 1u;
            }
            continue;
        }
        if (strcmp(first, "host") == 0)
        {
            char flag[16] = {0};
            run->host = (sscanf(line, "%*s %15s", flag) == 1) && (strcmp(flag, "1") == 0);
            continue;
        }
        if (strcmp(first, "steps") == 0)
        {
            (void)sscanf(line, "%*s %63s", run->steps);
            continue;
        }
        if (strcmp(first, "out_limbs") == 0)
        {
            continue;
        }
        if (row >= run->lanes)
        {
            continue;
        }
        // The row is the record of the lane: 32-bit words, least significant first.
        OriorExactInteger word;
        orior_exact_zero(&word);
        size_t words = 0u;
        char *cursor = line;
        while ((*cursor != '\0') && (words < ORIOR_EXACT_LIMBS))
        {
            char *end = cursor;
            const unsigned long value = strtoul(cursor, &end, 16);
            if (end == cursor)
            {
                break;
            }
            word.limb[words] = (uint32_t)value;
            words += 1u;
            cursor = end;
        }
        word.sign = 0;
        for (size_t at = 0u; at < words; at += 1u)
        {
            if (word.limb[at] != 0u)
            {
                word.sign = 1;
                break;
            }
        }
        for (size_t place = 0u; place < place_count; place += 1u)
        {
            OriorExactInteger shifted;
            OriorExactInteger power;
            z_two_to(offsets[place], &power);
            z_floor_div(&word, &power, &shifted);
            OriorExactInteger modulus;
            z_two_to(widths[place], &modulus);
            OriorExactInteger value;
            z_mod(&shifted, &modulus, &value);
            OriorExactInteger half;
            z_two_to(widths[place] - 1, &half);
            if (z_cmp(&value, &half) >= 0)
            {
                z_sub(&value, &modulus, &value);
            }
            run->values[row][place] = value;
        }
        row += 1u;
    }
    fclose(output);
    run->lanes = row;
}

/**
 * @brief A count of units of 2^-62 as a rational.
 *
 * @param[in]  units  The count [BORROWS].
 * @param[out] result The rational [BORROWS].
 */
static void in_scale(const OriorExactInteger *units, ZetaRational *result)
{
    OriorExactInteger under;
    z_two_to(SCALE_BITS, &under);
    rational_make(units, &under, result);
}

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argument_count < 2)
    {
        zeta_stop("usage: exact_zeta_spherical <binary> [r_bits] [u_bits]");
    }
    const char *const binary = arguments[1];
    const int r_bits = (argument_count > 2) ? atoi(arguments[2]) : 3;
    const int u_bits = (argument_count > 3) ? atoi(arguments[3]) : 7;
    SphericalRun run;
    run_device(binary, r_bits, u_bits, &run);
    const RouteBounds *const route = &run.bounds;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  grid: r in [0, ");
        mmgr_line_decimal_signed(output_line, R_HIGH);
        mmgr_line_put(output_line, ") by 2^-");
        mmgr_line_decimal_signed(output_line, r_bits);
        mmgr_line_put(output_line, ", u in [0, 1/2) by 2^-");
        mmgr_line_decimal_signed(output_line, u_bits);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned long long)run.lanes, 1u);
        mmgr_line_put(output_line, " lanes; ");
        mmgr_line_put(output_line, run.steps);
        mmgr_line_put(output_line, " steps; route one ");
        mmgr_line_decimal_signed(output_line, (long long)route->first);
        mmgr_line_put(output_line, " terms, route two ");
        mmgr_line_decimal_signed(output_line, (long long)route->second);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    ZetaRational unit;
    OriorExactInteger one_unit;
    z_two_to(SCALE_BITS, &one_unit);
    OriorExactInteger one;
    z_set(&one, 1);
    rational_make(&one_unit, &one, &unit);
    ZetaRational first_scaled;
    ZetaRational second_scaled;
    rational_div(&route->first_error, &unit, &first_scaled);
    rational_div(&route->second_error, &unit, &second_scaled);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  bounds: route one ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&first_scaled)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", route two ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&second_scaled)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, "; route one's partial sums below ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&route->largest)), 3u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    OriorExactInteger apart;
    OriorExactInteger imaginary;
    orior_exact_zero(&apart);
    orior_exact_zero(&imaginary);
    for (size_t lane = 0u; lane < run.lanes; lane += 1u)
    {
        OriorExactInteger gap;
        z_sub(&run.values[lane][0], &run.values[lane][1], &gap);
        z_abs(&gap, &gap);
        apart = (z_cmp(&gap, &apart) > 0) ? gap : apart;
        z_abs(&run.values[lane][2], &gap);
        imaginary = (z_cmp(&gap, &imaginary) > 0) ? gap : imaginary;
    }
    ZetaRational together_rational;
    rational_add(&route->first_error, &route->second_error, &together_rational);
    // The ceiling of a rational with a positive denominator: minus the floor of its negative.
    OriorExactInteger negative;
    z_neg(&together_rational.top, &negative);
    OriorExactInteger together;
    z_floor_div(&negative, &together_rational.under, &together);
    z_neg(&together, &together);
    int origin = 1;
    for (int64_t ri = 0; ri < run.r_count; ri += 1)
    {
        origin = origin && orior_exact_equal(&run.values[ri * run.u_count][0], &one_unit);
    }
    ZetaRational apart_scaled;
    ZetaRational together_scaled;
    ZetaRational imaginary_scaled;
    in_scale(&apart, &apart_scaled);
    in_scale(&together, &together_scaled);
    in_scale(&imaginary, &imaginary_scaled);
    const int apart_held = z_cmp(&apart, &together) <= 0;
    ZetaRational imaginary_rational;
    rational_make(&imaginary, &one, &imaginary_rational);
    const int imaginary_held = rational_cmp(&imaginary_rational, &route->second_error) <= 0;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the routes differ by ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&apart_scaled)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " at max, within ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&together_scaled)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, truth(apart_held));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  route two's imaginary part is ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&imaginary_scaled)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " at max, within ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&second_scaled)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, truth(imaginary_held));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  phi_r = 1 at u = 0 for every r: ");
        mmgr_line_put(output_line, truth(origin));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    int control = 1;
    const int64_t corners[4][2] = {{0, run.u_count - 1},
                                   {run.r_count / 2, run.u_count / 2},
                                   {run.r_count - 1, run.u_count - 1},
                                   {run.r_count - 1, 1}};
    OriorExactInteger need_under;
    z_two_to(80, &need_under);
    ZetaRational need;
    rational_make(&one, &need_under, &need);
    for (unsigned at = 0u; at < 4u; at += 1u)
    {
        ZetaRational r;
        ZetaRational u;
        rational_small(corners[at][0], (int64_t)1 << r_bits, &r);
        rational_small(corners[at][1], (int64_t)1 << u_bits, &u);
        ZetaRational exact;
        ZetaRational tail;
        exact_first(&u, &r, &need, &exact, &tail);
        ZetaRational device;
        in_scale(&run.values[(corners[at][0] * run.u_count) + corners[at][1]][0], &device);
        ZetaRational gap;
        rational_sub(&device, &exact, &gap);
        if (gap.top.sign < 0)
        {
            gap.top.sign = 1;
        }
        ZetaRational bar;
        rational_add(&first_scaled, &tail, &bar);
        const int within = rational_cmp(&gap, &bar) <= 0;
        control = control && within;
        char r_text[128];
        char u_text[128];
        rational_text(&r, r_text, sizeof(r_text));
        rational_text(&u, u_text, sizeof(u_text));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    r = ");
            mmgr_line_put(output_line, r_text);
            mmgr_line_put(output_line, ", u = ");
            mmgr_line_put(output_line, u_text);
            mmgr_line_put(output_line, ": device ");
            mmgr_line_fixed(output_line, (double)(rational_to_double(&device)), 15u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", exact ");
            mmgr_line_fixed(output_line, (double)(rational_to_double(&exact)), 15u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", apart ");
            mmgr_line_exponent(output_line, (double)(rational_to_double(&gap)), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", within the bound: ");
            mmgr_line_put(output_line, truth(within));
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    const int held = run.host && apart_held && imaginary_held && origin && control;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line,
                      held ? "both routes agree at every lane within their bounds" : "the routes do not hold");
        mmgr_line_put(output_line, "; the host's records ");
        mmgr_line_put(output_line, run.host ? "equal" : "differ from");
        mmgr_line_put(output_line, " the device's word for word");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_line_flushed();
    free(run.values);
    return held ? 0 : 1;
}
