// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-032
/**
 * @file exact_zeta_form_factor.c
 * @brief The form factor of the zeros that the machine of Turing certifies, on u = theta / pi + 1.
 *
 * @note That clock ticks one zero apart on average. The module zeta_tension.c places the zeros on the device. They
 *       number the difference of N between two F. S = N - u is the difference between the clock of the zeros and that
 *       of theta, and the form factor is the spectrum of S.
 * @note Let D(u) = n(u) - (u - a) over [a, b] about the zeros, and e(x) = exp(2 pi i x). Then the sum over zeros of
 *       e(tau u_n) has three parts. The first is the integral of e(tau u) over [a, b], and the second is D(b) e(tau b).
 *       The third is minus 2 pi i tau times the integral of D e(tau u).
 * @note K(tau) = |sum|^2 / M is (2 pi tau)^2 times the power of S over the window, the ends aside. The code takes
 *       both sides at single tau, with the integral of D exact over each line between zeros.
 * @note Run build_exact_zeta.sh form_factor <binary> <first> <last> [extra] [N at the F of first] [N at the F of
 *       last], beside this file.
 * @note The code reads K smoothed by a Gaussian of width sigma in tau, from every pair of zeros within D of each
 *       other. Then K_sigma(tau) = 1 + (2 / M) the sum over pairs of exp(-2 pi^2 sigma^2 d^2) cos(2 pi tau d), less
 *       G_sigma(tau). The last term is that of the mean density.
 * @note Against it: the min(|tau|, 1) of the GUE, smoothed the same. Also the prime powers, the diagonal of the
 *       explicit formula. Each is a spike of weight Lambda(x)^2 / (x L^2) at tau = ln x / L, with L = ln(t / 2 pi).
 * @note That is for every prime power x up to t / 2 pi. Their weights below tau sum to tau^2 / 2, the ramp of the
 *       GUE, and none falls below ln 2 / L. Also the same spacings shuffled, every gap kept and their order dropped.
 *       Also each half of the zeros alone, its L a little apart from the other.
 * @note The statistics are those of the host, in floating point, over the Z of the device. It sits in
 *       0_experimental, and is an entry in the workbook of analytic number theory, on its rail. It proves nothing.
 *       It reads how the zeros hold to one another.
 */
#include "zeta_shuffle.h"
#include "zeta_tension.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The differences of the pairs bin at 2^-WIDTH. */
#define WIDTH 10

/** @brief The narrow and the wide widths of the Gaussian. */
#define NARROW 0.01
#define WIDE 0.05

/** @brief The reach of the pairs for the narrow and the wide width, in spacings. */
#define REACH 100.0
#define REACH_WIDE 20.0

/** @brief The seed of the shuffle. */
#define FORM_SEED 20261004u

/** @brief Pi as a double, as math.pi holds it. */
#define PI_DOUBLE 3.141592653589793

/** @brief A complex number, as CPython holds one. */
typedef struct
{
    double re; /**< The real part. */
    double im; /**< The imaginary part. */
} Complex;

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *form_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief exp(i y), as cmath.exp of CPython gives it for a purely imaginary argument.
 *
 * @param[in] y The angle.
 * @return      cos y + i sin y.
 */
static Complex unit(double y)
{
    const Complex result = {cos(y), sin(y)};
    return result;
}

/**
 * @brief A complex over i w, as CPython divides by a purely imaginary divisor: (im / w, -re / w).
 *
 * @param[in] value The dividend.
 * @param[in] w     The imaginary part of the divisor.
 * @return          The quotient.
 */
static Complex over_iw(Complex value, double w)
{
    const Complex result = {value.im / w, -value.re / w};
    return result;
}

/**
 * @brief The sum over the zeros of e(tau u_n), and the same from D, the difference of the two clocks.
 *
 * @param[in]  u      The zeros on u [BORROWS].
 * @param[in]  m      How many.
 * @param[in]  tau    The frequency.
 * @param[out] zeros  The sum over the zeros [BORROWS].
 * @param[out] clocks The sum from D [BORROWS].
 */
static void two_routes(const double *u, size_t m, double tau, Complex *zeros, Complex *clocks)
{
    const double w = 2.0 * PI_DOUBLE * tau;
    zeros->re = 0.0;
    zeros->im = 0.0;
    for (size_t at = 0u; at < m; at += 1u)
    {
        const Complex z = unit(w * u[at]);
        zeros->re += z.re;
        zeros->im += z.im;
    }
    const double a = u[0] - 0.5;
    const double b = u[m - 1u] + 0.5;
    Complex integral = {0.0, 0.0};
    for (size_t k = 0u; k <= m; k += 1u)
    {
        const double c = (double)k + a;
        const double ends[2] = {(k == 0u) ? a : u[k - 1u], (k == m) ? b : u[k]};
        Complex lines[2];
        for (int side = 0; side < 2; side += 1)
        {
            // line(c, x) = (c - x) z / (i w) - z / (w w), with z = e^(i w x).
            const double x = ends[side];
            const Complex z = unit(w * x);
            const Complex scaled = {(c - x) * z.re, (c - x) * z.im};
            const Complex first = over_iw(scaled, w);
            lines[side].re = first.re - (z.re / (w * w));
            lines[side].im = first.im - (z.im / (w * w));
        }
        integral.re += lines[1].re - lines[0].re;
        integral.im += lines[1].im - lines[0].im;
    }
    const Complex top = unit(w * b);
    const Complex bottom = unit(w * a);
    const Complex difference = {top.re - bottom.re, top.im - bottom.im};
    const Complex plain = over_iw(difference, w);
    const double weight = (double)m - (b - a);
    // i w times the integral: (0 + i w)(re + i im) = (-w im, w re).
    const Complex turned = {(0.0 * integral.re) - (w * integral.im), (0.0 * integral.im) + (w * integral.re)};
    clocks->re = (plain.re + (weight * top.re)) - turned.re;
    clocks->im = (plain.im + (weight * top.im)) - turned.im;
}

/** @brief The binned differences of the pairs, and how many zeros they start from. */
typedef struct
{
    int64_t *bins;   /**< The counts of each bin [OWNS]. */
    size_t count;    /**< How many bins. */
    int64_t counted; /**< How many zeros. */
} Binned;

/**
 * @brief The place of the first value past x in an ascending list, as bisect_right finds it.
 *
 * @param[in] values The list [BORROWS].
 * @param[in] count  How many.
 * @param[in] x      The value.
 * @return           The place.
 */
static size_t bisect_right(const double *values, size_t count, double x)
{
    size_t low = 0u;
    size_t high = count;
    while (low < high)
    {
        const size_t middle = (low + high) / 2u;
        if (x < values[middle])
        {
            high = middle;
        }
        else
        {
            low = middle + 1u;
        }
    }
    return low;
}

/**
 * @brief The differences of the pairs up to the reach, binned, over the zeros low to high whose reach stays inside u.
 *
 * @param[in] u     The zeros on u [BORROWS].
 * @param[in] m     How many.
 * @param[in] reach The reach.
 * @param[in] low   The first zero.
 * @param[in] high  The end.
 * @return          The bins.
 */
static Binned pairs(const double *u, size_t m, double reach, size_t low, size_t high)
{
    Binned binned;
    // The reach times 2^10 is small, and a machine integer holds it.
    binned.count = (size_t)(reach * (double)(1 << WIDTH)) + 1u;
    binned.bins = (int64_t *)form_memory(calloc(binned.count, sizeof(int64_t)));
    binned.counted = 0;
    for (size_t n = low; n < high; n += 1u)
    {
        if ((u[n] + reach) > u[m - 1u])
        {
            break;
        }
        binned.counted += 1;
        const size_t end = bisect_right(u, m, u[n] + reach);
        for (size_t j = n + 1u; j < end; j += 1u)
        {
            // The difference is below the reach, and its bin is in the array.
            binned.bins[(size_t)((u[j] - u[n]) * (double)(1 << WIDTH))] += 1;
        }
    }
    return binned;
}

/**
 * @brief The density of a Gaussian of width sigma at x.
 *
 * @param[in] sigma The width.
 * @param[in] x     The point.
 * @return          The density.
 */
static double gauss(double sigma, double x)
{
    const double ratio = x / sigma;
    return exp(-0.5 * (ratio * ratio)) / (sigma * sqrt(2.0 * PI_DOUBLE));
}

/**
 * @brief K smoothed by a Gaussian of width sigma, at each tau, from the binned pairs.
 *
 * @param[in]  binned The bins [BORROWS].
 * @param[in]  sigma  The width.
 * @param[in]  taus   The frequencies [BORROWS].
 * @param[in]  count  How many.
 * @param[out] out    The values [BORROWS].
 */
static void smoothed(const Binned *binned, double sigma, const double *taus, size_t count, double *out)
{
    const double a = (2.0 * (PI_DOUBLE * PI_DOUBLE)) * (sigma * sigma);
    double *const distances = (double *)form_memory(malloc(binned->count * sizeof(double)));
    double *const weights = (double *)form_memory(malloc(binned->count * sizeof(double)));
    size_t held = 0u;
    for (size_t b = 0u; b < binned->count; b += 1u)
    {
        if (binned->bins[b] == 0)
        {
            continue;
        }
        const double d = ((double)b + 0.5) / (double)(1 << WIDTH);
        distances[held] = d;
        weights[held] = (double)binned->bins[b] * exp(-a * (d * d));
        held += 1u;
    }
    for (size_t at = 0u; at < count; at += 1u)
    {
        const double w = 2.0 * PI_DOUBLE * taus[at];
        double total = 0.0;
        for (size_t k = 0u; k < held; k += 1u)
        {
            total += weights[k] * cos(w * distances[k]);
        }
        out[at] = (1.0 + ((2.0 * total) / (double)binned->counted)) - gauss(sigma, taus[at]);
    }
    free(distances);
    free(weights);
}

/**
 * @brief min(|tau|, 1) smoothed by the Gaussian, by the rule of Simpson over 8 sigma about tau.
 *
 * @param[in] sigma The width.
 * @param[in] tau   The frequency.
 * @return          The value.
 */
static double gue(double sigma, double tau)
{
    const int steps = 400;
    const double h = (16.0 * sigma) / (double)steps;
    double total = 0.0;
    for (int k = 0; k <= steps; k += 1)
    {
        const double x = (tau - (8.0 * sigma)) + ((double)k * h);
        const double size = (fabs(x) < 1.0) ? fabs(x) : 1.0;
        const double f = size * gauss(sigma, x - tau);
        const double weight = ((k == 0) || (k == steps)) ? 1.0 : (((k % 2) != 0) ? 4.0 : 2.0);
        total += f * weight;
    }
    return (total * h) / 3.0;
}

/**
 * @brief The spikes of the prime powers up to e^L, smoothed.
 *
 * @param[in]  sigma  The width.
 * @param[in]  taus   The frequencies [BORROWS].
 * @param[in]  count  How many.
 * @param[in]  length L.
 * @param[out] out    The values [BORROWS].
 */
static void primes_model(double sigma, const double *taus, size_t count, double length, double *out)
{
    int64_t (*powers)[2] = NULL;
    // e^L is the height over 2 pi, far below 2^63.
    const size_t power_count = tension_prime_powers((int64_t)exp(length), &powers);
    double *const weights = (double *)form_memory(malloc((power_count + 1u) * sizeof(double)));
    double *const places = (double *)form_memory(malloc((power_count + 1u) * sizeof(double)));
    for (size_t at = 0u; at < power_count; at += 1u)
    {
        const double log_p = log((double)powers[at][0]);
        int64_t value = 1;
        for (int64_t r = 0; r < powers[at][1]; r += 1)
        {
            value *= powers[at][0];
        }
        weights[at] = (log_p * log_p) / ((double)value * (length * length));
        places[at] = ((double)powers[at][1] * log_p) / length;
    }
    for (size_t at = 0u; at < count; at += 1u)
    {
        double total = 0.0;
        for (size_t k = 0u; k < power_count; k += 1u)
        {
            if ((fabs(taus[at] - places[k]) < (8.0 * sigma)) || (fabs(taus[at] + places[k]) < (8.0 * sigma)))
            {
                total += weights[k] * (gauss(sigma, taus[at] - places[k]) + gauss(sigma, taus[at] + places[k]));
            }
        }
        out[at] = total;
    }
    free(weights);
    free(places);
    free(powers);
}

/**
 * @brief Whether a value lies within 10^-9 of a whole number, as round of Python reads it.
 *
 * @param[in] value The value.
 * @return          1 where it does.
 */
static int near_whole(double value)
{
    return fabs(value - nearbyint(value)) < 1e-9;
}

/**
 * @brief The readings of the form factor over the zeros.
 *
 * @param[in] zeros The zeros in s [BORROWS].
 * @param[in] m     How many.
 * @return          The exit code.
 */
static int analyse(const double *zeros, size_t m)
{
    double *const u = (double *)form_memory(malloc(m * sizeof(double)));
    for (size_t at = 0u; at < m; at += 1u)
    {
        u[at] = tension_theta_pi(zeros[at]) + 1.0;
    }
    const double length = log(0.5 * (zeros[0] + zeros[m - 1u]));
    // e^L is the height over 2 pi, far below 2^63.
    printf("  %llu zeros, ln(t / 2 pi) %.3f; ln 2 / L = %.4f, the prime powers to %lld\n", (unsigned long long)m,
           length, log(2.0) / length, (long long)exp(length));
    static const double CHECKS[10] = {0.03, 0.0606, 0.1, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5};
    double worst = 0.0;
    for (int at = 0; at < 10; at += 1)
    {
        Complex zeros_sum;
        Complex clocks;
        two_routes(u, m, CHECKS[at], &zeros_sum, &clocks);
        const double gap =
            hypot(zeros_sum.re - clocks.re, zeros_sum.im - clocks.im) / hypot(zeros_sum.re, zeros_sum.im);
        worst = (gap > worst) ? gap : worst;
    }
    printf("  the zeros' sum against D's, at 10 tau from 0.03 to 2.5: they differ by %.1e of the sum at max\n", worst);

    ZetaTwister twister;
    twister_seed(&twister, FORM_SEED);
    double *const gaps = (double *)form_memory(malloc(m * sizeof(double)));
    for (size_t k = 0u; (k + 1u) < m; k += 1u)
    {
        gaps[k] = u[k + 1u] - u[k];
    }
    for (size_t at = m - 2u; (m > 2u) && (at > 0u); at -= 1u)
    {
        // The count of gaps is small, and the index fits a uint32_t.
        const size_t other = twister_below(&twister, (uint32_t)(at + 1u));
        const double held = gaps[at];
        gaps[at] = gaps[other];
        gaps[other] = held;
    }
    double *const shuffled = (double *)form_memory(malloc(m * sizeof(double)));
    shuffled[0] = u[0];
    for (size_t k = 0u; (k + 1u) < m; k += 1u)
    {
        shuffled[k + 1u] = shuffled[k] + gaps[k];
    }

    double narrow[136];
    size_t narrow_count = 0u;
    for (int k = 0; k <= 80; k += 1)
    {
        narrow[narrow_count] = (double)k / 200.0;
        narrow_count += 1u;
    }
    for (int k = 41; k <= 95; k += 1)
    {
        narrow[narrow_count] = (double)k / 100.0;
        narrow_count += 1u;
    }
    Binned whole = pairs(u, m, REACH, 0u, m);
    Binned first = pairs(u, m, REACH, 0u, m / 2u);
    Binned second = pairs(u, m, REACH, m / 2u, m);
    Binned shuffled_pairs = pairs(shuffled, m, REACH, 0u, m);
    double k_all[136];
    double k_first[136];
    double k_second[136];
    double k_shuffled[136];
    double k_gue[136];
    double k_primes[136];
    smoothed(&whole, NARROW, narrow, narrow_count, k_all);
    smoothed(&first, NARROW, narrow, narrow_count, k_first);
    smoothed(&second, NARROW, narrow, narrow_count, k_second);
    smoothed(&shuffled_pairs, NARROW, narrow, narrow_count, k_shuffled);
    for (size_t at = 0u; at < narrow_count; at += 1u)
    {
        k_gue[at] = gue(NARROW, narrow[at]);
    }
    primes_model(NARROW, narrow, narrow_count, length, k_primes);
    printf("  K smoothed by sigma %.2f: tau, the zeros, half against half, the primes' spikes, GUE's, the shuffled "
           "spacings'\n",
           NARROW);
    for (size_t at = 0u; at < narrow_count; at += 1u)
    {
        const double tau = narrow[at];
        if ((tau <= 0.2) || near_whole(tau * 20.0))
        {
            printf("    %.3f  %.4f  %+.4f  %.4f  %.4f  %.4f\n", tau, k_all[at], k_first[at] - k_second[at],
                   k_primes[at], k_gue[at], k_shuffled[at]);
        }
    }
    const double below = (log(2.0) / length) - (3.0 * NARROW);
    const double bands[3][2] = {{0.0, below}, {below, 0.3}, {0.3, 0.9}};
    for (int band = 0; band < 3; band += 1)
    {
        double sums[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        size_t band_count = 0u;
        for (size_t at = 0u; at < narrow_count; at += 1u)
        {
            if ((narrow[at] < bands[band][0]) || (narrow[at] >= bands[band][1]))
            {
                continue;
            }
            band_count += 1u;
            sums[0] += k_all[at];
            sums[1] += k_gue[at];
            sums[2] += (k_all[at] - k_primes[at]) * (k_all[at] - k_primes[at]);
            sums[3] += (k_all[at] - k_gue[at]) * (k_all[at] - k_gue[at]);
            sums[4] += (k_first[at] - k_second[at]) * (k_first[at] - k_second[at]);
        }
        const double n = (double)band_count;
        printf(
            "    tau in [%.3f, %.3f): the zeros' mean %.4f, GUE's %.4f; rms from the primes %.4f, from GUE %.4f; the "
            "halves' difference, rms %.4f\n",
            bands[band][0], bands[band][1], sums[0] / n, sums[1] / n, sqrt(sums[2] / n), sqrt(sums[3] / n),
            sqrt(sums[4] / n));
    }

    double wide[60];
    for (int k = 1; k <= 60; k += 1)
    {
        wide[k - 1] = (double)k / 20.0;
    }
    Binned wide_pairs = pairs(u, m, REACH_WIDE, 0u, m);
    Binned wide_shuffled_pairs = pairs(shuffled, m, REACH_WIDE, 0u, m);
    double k_wide[60];
    double k_wide_shuffled[60];
    double k_wide_gue[60];
    smoothed(&wide_pairs, WIDE, wide, 60u, k_wide);
    smoothed(&wide_shuffled_pairs, WIDE, wide, 60u, k_wide_shuffled);
    for (int at = 0; at < 60; at += 1)
    {
        k_wide_gue[at] = gue(WIDE, wide[at]);
    }
    printf("  K smoothed by sigma %.2f: tau, the zeros, GUE's, the shuffled spacings'\n", WIDE);
    for (int at = 0; at < 60; at += 1)
    {
        if (near_whole(wide[at] * 10.0))
        {
            printf("    %.2f  %.4f  %.4f  %.4f\n", wide[at], k_wide[at], k_wide_gue[at], k_wide_shuffled[at]);
        }
    }
    const double wide_bands[3][2] = {{0.2, 0.8}, {0.8, 1.2}, {1.2, 3.01}};
    for (int band = 0; band < 3; band += 1)
    {
        double total = 0.0;
        double most = 0.0;
        double shuffled_total = 0.0;
        size_t band_count = 0u;
        for (int at = 0; at < 60; at += 1)
        {
            if ((wide[at] < wide_bands[band][0]) || (wide[at] >= wide_bands[band][1]))
            {
                continue;
            }
            const double gap = k_wide[at] - k_wide_gue[at];
            total += gap;
            most = ((band_count == 0u) || (fabs(gap) > most)) ? fabs(gap) : most;
            shuffled_total += k_wide_shuffled[at] - k_wide_gue[at];
            band_count += 1u;
        }
        const double top = (wide_bands[band][1] < 3.0) ? wide_bands[band][1] : 3.0;
        printf(
            "    tau in [%.1f, %.1f]: the zeros less GUE's, mean %+.4f, max %.4f; the shuffled spacings', mean %+.4f\n",
            wide_bands[band][0], top, total / (double)band_count, most, shuffled_total / (double)band_count);
    }
    free(whole.bins);
    free(first.bins);
    free(second.bins);
    free(shuffled_pairs.bins);
    free(wide_pairs.bins);
    free(wide_shuffled_pairs.bins);
    free(u);
    free(gaps);
    free(shuffled);
    return 0;
}

int main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argument_count < 4)
    {
        fprintf(stderr,
                "usage: exact_zeta_form_factor <binary> <first> <last> [extra] [N at first's F N at last's F]\n");
        return 2;
    }
    const int64_t extra = (argument_count > 4) ? atoll(arguments[4]) : 2;
    int64_t given[2] = {0, 0};
    const int has_given = argument_count > 6;
    if (has_given)
    {
        given[0] = atoll(arguments[5]);
        given[1] = atoll(arguments[6]);
    }
    TensionZeros zeros;
    tension_certified_zeros(arguments[1], atoll(arguments[2]), atoll(arguments[3]), extra, has_given ? given : NULL,
                            &zeros);
    const int code = analyse(zeros.zeros, zeros.count);
    free(zeros.zeros);
    return code;
}
