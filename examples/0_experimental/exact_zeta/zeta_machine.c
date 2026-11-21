// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_machine.c
 * @brief The machine over the automata of exact_zeta_turing.cu, as zeta_machine.h states it.
 */
#ifndef _WIN32
// The process of the binary uses pipe, fork and setenv, which POSIX declares.
#define _POSIX_C_SOURCE 200809L
#endif

#include "zeta_machine.h"
#include "../../../src/platform/platform.h"
#include "zeta_riemann_siegel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/** @brief The terms of artanh on the device. */
#define ARTANH_TERMS 21

/** @brief The terms of the cosine on the device. */
#define COS_TERMS 18

/** @brief The Taylor coefficients of each curve on the device. */
#define GAMMA_TERMS 56

/** @brief The steps of the rule of Newton on the device. */
#define NEWTON_STEPS 8

/** @brief The folds of the logarithm on the device. */
#define FOLDS 33

/** @brief The bits of the lanes of one run. */
#define LANE_BITS 22

/** @brief The lanes that the host checks against the device. */
#define CHECKED 64

/** @brief The order of the expansions of the multiple evaluation. */
#define ORDER 28

/** @brief The bits of the frequencies of a leaf. */
#define BETA 4

/** @brief The terms of the exponential integral and of the sinc on the device. */
#define E1_TERMS 48
#define SINC_TERMS 18

/** @brief The bits of the rounding of the bound on the fourth derivative. */
#define FOURTH_BITS 100

const char *const TURING_SUM_NAMES[SUM_COUNT] = {"low_before",   "low_after",     "high_upto",    "high_after",
                                                 "zeros_upto",   "zeros_after",   "zeros_q_upto", "zeros_q_after",
                                                 "zeros_p_upto", "zeros_p_after", "loose"};

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *turing_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/* ---------------------------------------------------------------- rationals by value */

/**
 * @brief The rational a / b.
 *
 * @param[in] a The numerator.
 * @param[in] b The denominator.
 * @return      The rational.
 */
static ZetaRational qn(int64_t a, int64_t b)
{
    ZetaRational result;
    rational_small(a, b, &result);
    return result;
}

/**
 * @brief An integer as a rational.
 *
 * @param[in] value The integer [BORROWS].
 * @return          The rational.
 */
static ZetaRational qz(const OriorExactInteger *value)
{
    OriorExactInteger one;
    z_set(&one, 1);
    ZetaRational result;
    rational_make(value, &one, &result);
    return result;
}

/**
 * @brief The sum of two rationals.
 *
 * @param[in] a First addend.
 * @param[in] b Second addend.
 * @return      The sum.
 */
static ZetaRational qadd(ZetaRational a, ZetaRational b)
{
    ZetaRational result;
    rational_add(&a, &b, &result);
    return result;
}

/**
 * @brief The difference of two rationals.
 *
 * @param[in] a The minuend.
 * @param[in] b The subtrahend.
 * @return      The difference.
 */
static ZetaRational qsub(ZetaRational a, ZetaRational b)
{
    ZetaRational result;
    rational_sub(&a, &b, &result);
    return result;
}

/**
 * @brief The product of two rationals.
 *
 * @param[in] a First factor.
 * @param[in] b Second factor.
 * @return      The product.
 */
static ZetaRational qmul(ZetaRational a, ZetaRational b)
{
    ZetaRational result;
    rational_mul(&a, &b, &result);
    return result;
}

/**
 * @brief The quotient of two rationals.
 *
 * @param[in] a The dividend.
 * @param[in] b The divisor.
 * @return      The quotient.
 */
static ZetaRational qdiv(ZetaRational a, ZetaRational b)
{
    ZetaRational result;
    rational_div(&a, &b, &result);
    return result;
}

/**
 * @brief A rational times a machine integer.
 *
 * @param[in] a The rational.
 * @param[in] k The integer.
 * @return      The product.
 */
static ZetaRational qk(ZetaRational a, int64_t k)
{
    return qmul(a, qn(k, 1));
}

/**
 * @brief A rational to a power.
 *
 * @param[in] a The base.
 * @param[in] n The power, at least zero.
 * @return      a^n.
 */
static ZetaRational qpow(ZetaRational a, int64_t n)
{
    ZetaRational result = qn(1, 1);
    for (int64_t at = 0; at < n; at += 1)
    {
        result = qmul(result, a);
    }
    return result;
}

/**
 * @brief Whether a < b.
 *
 * @param[in] a The left.
 * @param[in] b The right.
 * @return      1 where it holds.
 */
static int qlt(ZetaRational a, ZetaRational b)
{
    return rational_cmp(&a, &b) < 0;
}

/**
 * @brief The ceiling of a rational.
 *
 * @param[in]  a      The rational.
 * @param[out] result The ceiling [BORROWS].
 */
static void qceil(ZetaRational a, OriorExactInteger *result)
{
    OriorExactInteger top;
    z_neg(&a.top, &top);
    z_floor_div(&top, &a.under, result);
    z_neg(result, result);
}

/** @brief The bounds on pi and on ln 2 that the bounds read. */
static ZetaRational pi_low(void)
{
    return qn(314159265, 100000000);
}

static ZetaRational pi_high(void)
{
    return qn(314159266, 100000000);
}

static ZetaRational ln_two_high(void)
{
    return qn(693148, 1000000);
}

/** @brief 2^62 as a rational. */
static ZetaRational unit_q(void)
{
    OriorExactInteger unit;
    z_two_to(TURING_SCALE_BITS, &unit);
    return qz(&unit);
}

/* ---------------------------------------------------------------- the errors of the device */

/**
 * @brief The least integer r with r^k >= value, for value >= 0.
 *
 * @param[in] value The value.
 * @param[in] k     The root.
 * @return          r.
 */
static int64_t root_ceiling(ZetaRational value, int k)
{
    const double start = pow(rational_to_double(&value), 1.0 / (double)k);
    // The roots in these bounds are below 2^53, where a double holds an integer.
    int64_t r = (int64_t)nearbyint(start) - 2;
    r = (r > 0) ? r : 0;
    while (qlt(qpow(qn(r, 1), k), value))
    {
        r += 1;
    }
    return r;
}

/**
 * @brief The error of the device on w^(-1/2), 1 <= w <= v, in units of 2^-62, after NEWTON_STEPS of the rule of
 *        Newton.
 *
 * @param[in] v The top of w.
 * @return      The error.
 * @note The rule starts from 2^(-g - 1), with 4^g < w <= 4^(g + 1), below the root and above half of it. Let
 *       R = 2^62 w^(-1/2). The floors of a step take y below y (3 - w y^2) / 2 by at most 1.5 units.
 * @note The floors of y^2 and of w y^2 put it above by at most y (w + 2) / 2^63, below (w + 2)^(1/2) units. The
 *       ratio r = y / R then stays above rho' = min(g(rho), g(1 + up / R)) - 1.5 / R, with g(r) = r (3 - r^2) / 2,
 *       from rho = 1/2.
 * @note The code takes it at 2^256, rounded down. The function g rises on [0, 1], and the start at 1/2 is the worst.
 */
static int64_t newton_error(int64_t v)
{
    OriorExactInteger scale;
    z_two_to(256, &scale);
    const int64_t s = root_ceiling(qn(v, 1), 2);
    const int64_t up = root_ceiling(qn(v + 2, 1), 2) + 1;
    OriorExactInteger value;
    OriorExactInteger power;
    OriorExactInteger one;
    z_set(&one, 1);
    OriorExactInteger down;
    z_mul_small(&scale, 3 * s, &down);
    z_two_to(TURING_SCALE_BITS + 1, &power);
    z_floor_div(&down, &power, &down);
    z_add(&down, &one, &down);
    OriorExactInteger up_ratio;
    z_mul_small(&scale, up * s, &up_ratio);
    z_two_to(TURING_SCALE_BITS, &power);
    z_floor_div(&up_ratio, &power, &up_ratio);
    z_add(&up_ratio, &one, &up_ratio);
    OriorExactInteger twice_scale;
    z_mul_small(&scale, 2, &twice_scale);
    OriorExactInteger three_scale;
    z_mul_small(&scale, 3, &three_scale);
    OriorExactInteger points[2];
    OriorExactInteger rho;
    OriorExactInteger two;
    z_set(&two, 2);
    z_floor_div(&scale, &two, &rho);
    for (int step = 0; step < NEWTON_STEPS; step += 1)
    {
        z_add(&scale, &up_ratio, &points[1]);
        points[0] = rho;
        OriorExactInteger results[2];
        for (int which = 0; which < 2; which += 1)
        {
            // g(r) = r (3 scale - ceil(r^2 / scale)) / (2 scale).
            OriorExactInteger square;
            z_mul(&points[which], &points[which], &square);
            z_neg(&square, &square);
            z_floor_div(&square, &scale, &square);
            z_neg(&square, &square);
            z_sub(&three_scale, &square, &value);
            z_mul(&points[which], &value, &value);
            z_floor_div(&value, &twice_scale, &results[which]);
        }
        rho = (z_cmp(&results[0], &results[1]) <= 0) ? results[0] : results[1];
        z_sub(&rho, &down, &rho);
    }
    z_sub(&scale, &rho, &value);
    z_two_to(TURING_SCALE_BITS, &power);
    z_mul(&value, &power, &value);
    z_neg(&value, &value);
    z_floor_div(&value, &scale, &value);
    z_neg(&value, &value);
    const int64_t error = z_small(&value);
    return ((error > up) ? error : up) + 1;
}

/**
 * @brief e_ln, the error on a folded logarithm, and e_cos, on cos(pi s) by the rule of Horner, in units of 2^-62.
 *
 * @param[out] e_ln  The error on a logarithm [BORROWS].
 * @param[out] e_cos The error on a cosine [BORROWS].
 * @note Every value that the device takes a product back from is below 12 times 2^62 in size. The partial sums of
 *       cos stay below cosh(pi) < 11.6, and those of artanh below 1.05. The device wraps each such value to 76 bits,
 *       and a value of that size passes the wrap unchanged.
 */
static void unit_errors(ZetaRational *e_ln, ZetaRational *e_cos)
{
    static int known = 0;
    static ZetaRational kept[2];
    if (!known)
    {
        const ZetaRational y = qn(1, 3);
        const ZetaRational y_square = qmul(y, y);
        OriorExactInteger power;
        z_two_to(60, &power);
        OriorExactInteger one;
        z_set(&one, 1);
        ZetaRational tiny;
        rational_make(&one, &power, &tiny);
        const ZetaRational tail = qdiv(qmul(unit_q(), qpow(y, 2 * ARTANH_TERMS)),
                                       qmul(qn((2 * ARTANH_TERMS) + 1, 1), qsub(qn(1, 1), y_square)));
        const ZetaRational e_y2 = qadd(qadd(qk(y, 2), qn(1, 1)), tiny);
        const ZetaRational held = qn(105, 100);
        ZetaRational e_acc = qn(2, 1);
        for (int at = 0; at < (ARTANH_TERMS - 1); at += 1)
        {
            e_acc = qadd(qadd(qmul(e_acc, qadd(y_square, tiny)), qmul(held, e_y2)), qn(3, 1));
        }
        const ZetaRational e_half = qadd(qadd(qmul(e_acc, y), held), qn(1, 1));
        kept[0] = qadd(qadd(qk(e_half, 2), qk(qadd(tail, qn(1, 1)), 2)), qn(FOLDS + 1, 1));
        ZetaRational cosine = qn(2, 1);
        for (int at = 0; at < (COS_TERMS - 1); at += 1)
        {
            cosine = qadd(qadd(cosine, qn(1160, 100)), qn(3, 1));
        }
        kept[1] = qadd(cosine, qn(1, 1));
        known = 1;
    }
    *e_ln = kept[0];
    *e_cos = kept[1];
}

/**
 * @brief The error of the device on Z and on theta / pi over cell nu, in units of 2^-62, from its steps.
 *
 * @param[in]  nu        The cell.
 * @param[in]  constants The constants [BORROWS].
 * @param[out] errors    The error on Z, on theta / pi, and that of R alone [BORROWS].
 * @note Every value that the device takes a product back from is below 12 times 2^62 in size. Besides those of
 *       unit_errors, these are the partial sums of each curve below its size, with the sizes below 1.2 in sum. The
 *       factor y (3 - w y^2) of the rule of Newton stays below 3.
 * @note ln k and ln s each carry e_ln. theta / pi = S ln s / 2^p - s - 1/8 + 2^p / (96 pi^2 S) carries s e_ln and
 *       three floors. Its terms in 1 / s^3 and 1 / s^5, with 1 / s within a unit and below 1, carry eight more.
 * @note The 2 s ln k of the pair carries 2 s e_ln and a floor. The value x = S s^(-1/2) / 2^p carries s times the
 *       error of the root and a floor, and z twice that. The root x^(-1/2), from the x that the device holds, carries
 *       half of it more.
 * @note Each C_n by the rule of Horner carries three units a coefficient and one more, and the error of z times its
 *       slope. 1 / x, the square of the root, carries twice the error of the root and a floor.
 * @note The sum over n of C_n x^(-n) comes by the rule of Horner in 1 / x from C_K. It carries the sum before it times
 *       1 / x at each step. That factor is below 1 / nu. The sum also carries its size times the error of 1 / x, a
 * floor, and the own error of the curve.
 * @note R carries that sum times x^(-1/2), its size times the error of the root, the error of the sum and a floor.
 */
static void arithmetic(int64_t nu, const TuringConstants *constants, ZetaRational errors[3])
{
    const int64_t s_top = (nu + 1) * (nu + 1);
    const ZetaRational unit = unit_q();
    ZetaRational e_ln;
    ZetaRational e_cos;
    unit_errors(&e_ln, &e_cos);
    const ZetaRational e_theta = qadd(qk(e_ln, s_top), qn(12, 1));
    const ZetaRational e_q = qadd(qadd(e_theta, qk(e_ln, 2 * s_top)), qn(1, 1));
    const ZetaRational e_term =
        qadd(qadd(qadd(e_cos, qmul(pi_high(), e_q)), qk(qn(101, 100), newton_error(nu))), qn(1, 1));
    const ZetaRational e_x = qn((s_top * newton_error(s_top)) + 1, 1);
    const ZetaRational e_root = qadd(qadd(qn(newton_error(nu + 1), 1), qdiv(e_x, qn(2, 1))), qn(1, 1));
    const ZetaRational e_over = qadd(qk(e_root, 2), qn(1, 1));
    ZetaRational e_sum = qn(0, 1);
    ZetaRational size = qn(0, 1);
    for (int64_t n = constants->curve_count - 1; n >= 0; n -= 1)
    {
        const ZetaRational e_curve = qadd(qn((3 * GAMMA_TERMS) + 1, 1), qmul(qk(e_x, 2), constants->slopes[n]));
        e_sum =
            qadd(qadd(qadd(qmul(e_sum, qadd(qn(1, nu), qdiv(e_over, unit))), qmul(size, e_over)), qn(1, 1)), e_curve);
        size = qadd(qdiv(size, qn(nu, 1)), constants->sizes[n]);
    }
    const ZetaRational e_held = qadd(qadd(e_sum, qmul(size, e_root)), qn(1, 1));
    errors[0] = qadd(qk(e_term, 2 * nu), e_held);
    errors[1] = e_theta;
    errors[2] = e_held;
}

/**
 * @brief A lower bound on sin x for 0 <= x <= pi / 2: x - x^3 / 6.
 *
 * @param[in] x The angle.
 * @return      The bound.
 */
static ZetaRational sine_low(ZetaRational x)
{
    return qsub(x, qdiv(qpow(x, 3), qn(6, 1)));
}

/**
 * @brief An upper bound on ln t, from the bits of the ceiling of t.
 *
 * @param[in] t The value.
 * @return      The bound.
 */
static ZetaRational ln_high(ZetaRational t)
{
    OriorExactInteger top;
    qceil(t, &top);
    return qk(ln_two_high(), z_bit_length(&top));
}

/**
 * @brief The error of the multiple evaluation on Z over cell nu at P = 2^p points, in units of 2^-62.
 *
 * @param[in] nu       The cell.
 * @param[in] p        The bits of the lattice.
 * @param[in] e_theta  The error on theta / pi.
 * @param[in] e_held   The error of R.
 * @param[in] weighted 1 for the poles times -i ln k / 2^shift, which give F' / 2^shift.
 * @return             The error.
 * @note Half of its main sum is Re(exp(i theta) F). Here F_j = sum over h of (u_h / P) exp(-2 pi i j h / P). The
 *       code bounds the poles as the device holds them, and the truncation of the expansions. It bounds their
 *       arithmetic, the near field, and the stages of the transform. exact_zeta_turing.c states each term.
 */
static ZetaRational transform_error(int64_t nu, int64_t p, ZetaRational e_theta, ZetaRational e_held, int weighted)
{
    ZetaRational e_ln;
    ZetaRational e_cos;
    unit_errors(&e_ln, &e_cos);
    const int64_t top = p - BETA;
    const int64_t width = (int64_t)1 << BETA;
    const int64_t points = (int64_t)1 << p;
    const int64_t root_nu = root_ceiling(qn(nu, 1), 2);
    ZetaRational e_a =
        qadd(qadd(qadd(qk(qn(101, 100), newton_error(nu)), e_cos), qmul(qk(pi_high(), 2 * nu * nu), e_ln)), qn(2, 1));
    ZetaRational e_q = qadd(qdiv(qadd(qadd(qk(e_cos, 4), qk(pi_high(), 2)), qn(6, 1)), qn(points, 1)), qn(1, 1));
    if (weighted)
    {
        e_a = qadd(qadd(e_a, e_ln), qn(4, 1));
        e_q = qadd(qadd(e_q, e_ln), qn(4, 1));
    }
    const ZetaRational held = qadd(qk(e_a, nu), qmul(qk(pi_high(), 2 * root_nu * 2 * ((2 * nu) + 1)), e_ln));
    const ZetaRational unit = unit_q();
    ZetaRational truncation = qn(0, 1);
    ZetaRational arithmetic_sum = qn(0, 1);
    ZetaRational geometric = qn(0, 1);
    for (int64_t m = 0; m < ORDER; m += 1)
    {
        geometric = qadd(geometric, qn(m, (int64_t)1 << m));
    }
    const ZetaRational leaf_norm = qadd(qk(qadd(e_q, qn(2, 1)), 2), qk(geometric, 4));
    // The poles lie in pos within [0, (2 nu + 1) ln nu], and at level l in at most this many boxes.
    const ZetaRational reach = qk(ln_high(qn(nu, 1)), (2 * nu) + 1);
    int64_t *const holding = (int64_t *)turing_memory(calloc((size_t)(top + 2), sizeof(int64_t)));
    for (int64_t level = 0; level <= top; level += 1)
    {
        OriorExactInteger value;
        qceil(qdiv(qk(reach, (int64_t)1 << level), qn(points, 1)), &value);
        const int64_t boxes = z_small(&value) + 1;
        holding[level] = (((int64_t)1 << level) < boxes) ? ((int64_t)1 << level) : boxes;
    }
    const ZetaRational third = qn(4, 3);
    for (int64_t level = 3; level <= top; level += 1)
    {
        const ZetaRational rho = qdiv(pi_high(), qn((int64_t)1 << level, 1));
        const ZetaRational d_low = qk(sine_low(qdiv(qk(pi_low(), 3), qn((int64_t)1 << level, 1))), 2);
        const ZetaRational c = qdiv(qk(rho, 2), d_low);
        truncation =
            qadd(truncation,
                 qmul(qdiv(qk(qpow(c, ORDER), 24 * root_nu), qmul(qk(qsub(qn(1, 1), c), (int64_t)1 << level), d_low)),
                      unit));
        ZetaRational multipoles = qmul(qk(leaf_norm, nu), qpow(third, top - level));
        for (int64_t finer = level + 1; finer <= top; finer += 1)
        {
            multipoles = qadd(multipoles, qk(qpow(third, finer - 1 - level), holding[finer] * (ORDER + 6)));
        }
        const ZetaRational across =
            qadd(qmul(qdiv(qn(127, 100), d_low), multipoles),
                 qk(qadd(qdiv(qn((4 * ORDER) + 2, 1), d_low), qn(2 * ORDER, 1)), holding[level]));
        arithmetic_sum =
            qadd(arithmetic_sum, qadd(qk(qmul(qn(points, (int64_t)1 << level), across), 6), qn(points * 8 * ORDER, 1)));
    }
    free(holding);
    const ZetaRational near_field = qk(qadd(e_cos, qn(40, 1)), 5 * ((width / 2) + 1));
    arithmetic_sum = qadd(arithmetic_sum, qk(qadd(qn(2 * ORDER, 1), near_field), points));
    const ZetaRational spread = qk(qadd(qk(ln_two_high(), p), qn(3, 1)), 2 * root_nu);
    const ZetaRational e_w = qadd(e_cos, pi_high());
    const ZetaRational stages = qk(qadd(qn(2, 1), qmul(spread, e_w)), 2 * points);
    const ZetaRational e_f = qadd(qadd(qadd(held, truncation), arithmetic_sum), stages);
    const ZetaRational phase = qk(qadd(e_cos, qmul(pi_high(), e_theta)), 2 * root_nu);
    return qadd(qk(qadd(qadd(e_f, phase), qn(2, 1)), 2), e_held);
}

/**
 * @brief r_k = (B_2k / (2k)!) / (B_(2k-2) / (2k - 2)!) for k from 2 to the terms, at 2^62, each toward zero.
 *
 * @param[in]  terms  M.
 * @param[out] ratios The ratios [BORROWS]. terms - 1 of them.
 */
static void em_ratios(int64_t terms, OriorExactInteger *ratios)
{
    for (int64_t k = 2; k <= terms; k += 1)
    {
        ZetaRational this_one;
        ZetaRational before;
        zeta_bernoulli(k, &this_one);
        zeta_bernoulli(k - 1, &before);
        OriorExactInteger num;
        OriorExactInteger den;
        z_mul(&this_one.top, &before.under, &num);
        z_mul(&this_one.under, &before.top, &den);
        const int sign = num.sign * den.sign;
        z_abs(&num, &num);
        z_abs(&den, &den);
        OriorExactInteger power;
        z_two_to(TURING_SCALE_BITS, &power);
        z_mul(&num, &power, &num);
        z_floor_div(&num, &den, &ratios[k - 2]);
        z_mul_small(&ratios[k - 2], sign, &ratios[k - 2]);
    }
}

int64_t turing_em_terms(int64_t level)
{
    return 20 + (10 * level);
}

int64_t turing_em_heads(int64_t nu, int64_t terms)
{
    const ZetaRational t_high = qk(pi_high(), 2 * (nu + 1) * (nu + 1));
    OriorExactInteger value;
    qceil(qdiv(qadd(t_high, qn(2 * terms, 1)), pi_low()), &value);
    const int64_t heads = z_small(&value);
    return (heads > 2) ? heads : 2;
}

/**
 * @brief The largest error of theta from the five terms of the device, through 31 / (80640 t^5), at t >= t_low.
 *
 * @param[in] t_low The least t.
 * @return          The error.
 * @note From t = 10 it is the 1 / (3322 t^7) of Gabcke, in the introduction of his thesis, (4) and (5). Below that,
 *       it is the bound of Brent on theta past 1 / (48 t), with the two terms after it added.
 */
static ZetaRational theta_error(ZetaRational t_low)
{
    if (!qlt(t_low, qn(10, 1)))
    {
        return qdiv(qn(1, 1), qk(qpow(t_low, 7), 3322));
    }
    const ZetaRational x = qmul(pi_low(), t_low);
    // exp(-x) below 1 over the first 40 Taylor terms of exp.
    ZetaRational series = qn(0, 1);
    ZetaRational term = qn(1, 1);
    for (int64_t k = 0; k < 40; k += 1)
    {
        if (k > 0)
        {
            term = qdiv(qmul(term, x), qn(k, 1));
        }
        series = qadd(series, term);
    }
    const ZetaRational cube = qpow(t_low, 3);
    const ZetaRational brent =
        qadd(qdiv(qadd(qn(7, 5760), qdiv(pi_high(), qn(960, 1))), cube), qdiv(qn(1, 1), qk(series, 2)));
    return qadd(qadd(brent, qdiv(qn(7, 5760), cube)), qdiv(qn(31, 80640), qpow(t_low, 5)));
}

void turing_em_bounds(int64_t nu, int64_t heads, int64_t terms, OriorExactInteger *bound,
                      OriorExactInteger *theta_bound)
{
    const ZetaRational unit = unit_q();
    const int64_t s_top = (nu + 1) * (nu + 1);
    const ZetaRational t_low = qk(pi_low(), 2 * nu * nu);
    const ZetaRational t_high = qk(pi_high(), 2 * s_top);
    const int64_t root_heads = root_ceiling(qn(heads, 1), 2);
    const ZetaRational sigma = qn(1, 2);
    // The remainder of Johansson, its square first, at the top of the cell.
    ZetaRational square = qn(16, 1);
    for (int64_t j = 0; j < (2 * terms); j += 1)
    {
        const ZetaRational shifted = qadd(qn(j, 1), sigma);
        square = qmul(square, qadd(qmul(shifted, shifted), qmul(t_high, t_high)));
    }
    const ZetaRational gap = qsub(qn(2 * terms, 1), sigma);
    square =
        qdiv(square, qmul(qmul(qpow(qk(pi_low(), 2), 4 * terms), qmul(gap, gap)), qpow(qn(heads, 1), (4 * terms) - 1)));
    const ZetaRational remainder = qn(root_ceiling(qmul(qmul(square, unit), unit), 2) + 1, 1);
    // theta at the foot of the cell.
    const ZetaRational e_theta = theta_error(t_low);
    // The size of C, and the error of the device on it.
    ZetaRational e_ln;
    ZetaRational e_cos;
    unit_errors(&e_ln, &e_cos);
    const ZetaRational e_point = qadd(qk(e_ln, s_top), qn(12, 1));
    const ZetaRational e_q = qadd(qadd(e_point, qk(e_ln, 2 * s_top)), qn(1, 1));
    const ZetaRational e_cs = qadd(e_cos, qmul(pi_high(), e_q));
    const ZetaRational e_t = qn((2 * s_top) + 2, 1);
    const ZetaRational e_inverse = qk(qadd(qdiv(e_t, qmul(t_low, t_low)), qn(1, 1)), 2);
    const ZetaRational size = qadd(qn(1, 2), t_high);
    ZetaRational held = qadd(qdiv(qn(heads, 1), t_low), qn(1, 2));
    ZetaRational a = qdiv(size, qn(12 * heads, 1));
    ZetaRational e_tau = qadd(qn(2, 1), qdiv(e_t, qn(12 * heads, 1)));
    held = qadd(held, a);
    ZetaRational e_c = qadd(qk(e_inverse, heads), e_tau);
    OriorExactInteger *const ratios =
        (OriorExactInteger *)turing_memory(calloc((size_t)terms + 1u, sizeof(OriorExactInteger)));
    em_ratios(terms, ratios);
    const ZetaRational heads_q = qn(heads, 1);
    for (int64_t k = 2; k <= terms; k += 1)
    {
        OriorExactInteger size_r;
        z_abs(&ratios[k - 2], &size_r);
        OriorExactInteger one;
        z_set(&one, 1);
        z_add(&size_r, &one, &size_r);
        const ZetaRational r = qdiv(qz(&size_r), unit);
        const ZetaRational w = qadd(qn((2 * k) - 3, 1), size);
        const ZetaRational e1 = qadd(qadd(qmul(e_tau, r), a), qn(2, 1));
        const ZetaRational a1 = qmul(a, r);
        const ZetaRational e2 = qadd(qdiv(e1, heads_q), qn(2, 1));
        const ZetaRational a2 = qdiv(a1, heads_q);
        const ZetaRational e3 = qadd(qadd(qmul(e2, w), qmul(a2, e_t)), qn(4, 1));
        const ZetaRational a3 = qmul(a2, w);
        const ZetaRational e4 = qadd(qdiv(e3, heads_q), qn(2, 1));
        const ZetaRational a4 = qdiv(a3, heads_q);
        const ZetaRational w_next = qadd(w, qn(1, 1));
        e_tau = qadd(qadd(qmul(e4, w_next), qmul(a4, e_t)), qn(4, 1));
        a = qmul(a4, w_next);
        held = qadd(held, a);
        e_c = qadd(e_c, e_tau);
    }
    free(ratios);
    const ZetaRational e_rotated = qadd(qadd(qmul(qk(e_cs, 2), held), e_c), qn(2, 1));
    const ZetaRational e_em = qadd(qadd(qmul(qn(101, 100), e_rotated), qk(held, newton_error(heads))), qn(1, 1));
    const ZetaRational e_term =
        qadd(qadd(qadd(e_cos, qmul(pi_high(), e_q)), qk(qn(101, 100), newton_error(heads))), qn(1, 1));
    const ZetaRational e_arith = qadd(qadd(qk(e_term, heads - 1), e_em), qn(2, 1));
    const ZetaRational phase =
        qmul(qdiv(qmul(e_theta, e_theta), qn(2, 1)), qadd(qmul(qadd(qn(2 * root_heads, 1), held), unit), remainder));
    qceil(qadd(qadd(e_arith, remainder), phase), bound);
    qceil(qadd(e_point, qdiv(qmul(e_theta, unit), pi_low())), theta_bound);
}

/** @brief Gabcke, Satz 3.2.2, for t >= 200: |R_K(t)| < d_K t^(-(2K + 3) / 4), by (b) for K <= 9 and (a) for 10. */
static const int64_t GABCKE[11][2] = {{127, 1000}, {53, 1000}, {11, 1000}, {31, 1000}, {17, 1000}, {61, 1000},
                                      {661, 1000}, {92, 10},   {130, 1},   {1837, 1},  {25966, 1}};

/**
 * @brief The curves C_0 to C_K, with K the one whose bound is least at t = 200.
 *
 * @return K + 1.
 * @note d_K' / d_K t^(-(K' - K) / 2) falls as t rises, and the least at t = 200 is the least at every t past it.
 */
static int64_t curve_count(void)
{
    int64_t best = 0;
    ZetaRational least = qn(0, 1);
    for (int64_t k = 0; k < 11; k += 1)
    {
        const ZetaRational value = qdiv(qpow(qn(GABCKE[k][0], GABCKE[k][1]), 4), qpow(qn(200, 1), (2 * k) + 3));
        if ((k == 0) || qlt(value, least))
        {
            least = value;
            best = k;
        }
    }
    return best + 1;
}

/**
 * @brief The bound of Gabcke on R_K and that of theta on Z, and that of theta on theta / pi, in units of 2^-62.
 *
 * @param[in]  nu     The cell.
 * @param[in]  curves The curve count.
 * @param[out] values The bound on Z and on theta / pi [BORROWS].
 */
static void analytic(int64_t nu, int64_t curves, ZetaRational values[2])
{
    const ZetaRational t_low = qk(pi_low(), 2 * nu * nu);
    const ZetaRational unit = unit_q();
    const int64_t k = curves - 1;
    const int64_t gabcke =
        root_ceiling(qdiv(qpow(qmul(qn(GABCKE[k][0], GABCKE[k][1]), unit), 4), qpow(t_low, (2 * k) + 3)), 4);
    const ZetaRational e_theta = theta_error(t_low);
    const ZetaRational phase = qmul(qk(e_theta, 4 * root_ceiling(qn(nu, 1), 2)), unit);
    values[0] = qadd(qn(gabcke, 1), phase);
    values[1] = qdiv(qmul(e_theta, unit), pi_low());
}

void turing_bounds(int64_t nu, const TuringConstants *constants, int64_t p, int method, OriorExactInteger *bound,
                   OriorExactInteger *theta_bound)
{
    ZetaRational errors[3];
    arithmetic(nu, constants, errors);
    ZetaRational e_arith = errors[0];
    if (method != TURING_PAIRS)
    {
        e_arith = transform_error(nu, p, errors[1], errors[2], 0);
    }
    ZetaRational analytic_values[2];
    analytic(nu, constants->curve_count, analytic_values);
    qceil(qadd(e_arith, analytic_values[0]), bound);
    qceil(qadd(errors[1], analytic_values[1]), theta_bound);
}

/* ---------------------------------------------------------------- the constants */

void turing_constants(TuringConstants *constants)
{
    memset(constants, 0, sizeof(*constants));
    OriorExactInteger work;
    z_two_to(TURING_SCALE_BITS + TURING_GUARD_BITS, &work);
    zeta_pi_machin(&work, &constants->pi);
    constants->curve_count = curve_count();
    const int64_t curves = constants->curve_count;
    // The places of the curves: 62 bits as digits, the growth of the series, three a curve, and a guard.
    const int32_t places =
        (int32_t)(((TURING_SCALE_BITS * 30103) / 100000) + ((61 * GAMMA_TERMS) / 100) + (3 * curves) + 40);
    ZetaCurve curve;
    rs_curve_begin(&curve, places);
    constants->gamma =
        (OriorExactInteger *)turing_memory(calloc((size_t)(curves * GAMMA_TERMS), sizeof(OriorExactInteger)));
    constants->sizes = (ZetaRational *)turing_memory(calloc((size_t)curves, sizeof(ZetaRational)));
    constants->slopes = (ZetaRational *)turing_memory(calloc((size_t)curves, sizeof(ZetaRational)));
    OriorExactInteger scale;
    z_ten_to(places, &scale);
    OriorExactInteger unit;
    z_two_to(TURING_SCALE_BITS, &unit);
    for (int64_t n = 0; n < curves; n += 1)
    {
        ZetaRow row;
        rs_curve_gamma(&curve, n, GAMMA_TERMS, &row);
        OriorExactInteger size_sum;
        OriorExactInteger slope_sum;
        orior_exact_zero(&size_sum);
        orior_exact_zero(&slope_sum);
        for (int64_t i = 0; i < GAMMA_TERMS; i += 1)
        {
            OriorExactInteger value;
            z_abs(&row.values[i], &value);
            z_mul(&value, &unit, &value);
            z_floor_div(&value, &scale, &value);
            OriorExactInteger *const slot = &constants->gamma[(n * GAMMA_TERMS) + i];
            z_mul_small(&value, row.values[i].sign, slot);
            OriorExactInteger plus;
            OriorExactInteger one;
            z_set(&one, 1);
            z_abs(slot, &plus);
            z_add(&plus, &one, &plus);
            z_add(&size_sum, &plus, &size_sum);
            z_mul_small(&plus, i, &plus);
            z_add(&slope_sum, &plus, &slope_sum);
        }
        rational_make(&size_sum, &unit, &constants->sizes[n]);
        rational_make(&slope_sum, &unit, &constants->slopes[n]);
        row_free(&row);
    }
    rs_curve_free(&curve);
    OriorExactInteger guard;
    z_two_to(TURING_GUARD_BITS, &guard);
    OriorExactInteger ln_two;
    zeta_ln_two_artanh(&work, &ln_two);
    z_floor_div(&ln_two, &guard, &constants->ln2);
    for (int64_t k = 0; k < ARTANH_TERMS; k += 1)
    {
        OriorExactInteger odd;
        z_set(&odd, (2 * k) + 1);
        z_floor_div(&unit, &odd, &constants->artanh[k]);
    }
    OriorExactInteger work_square;
    z_mul(&work, &work, &work_square);
    OriorExactInteger power = work;
    OriorExactInteger factorial;
    z_set(&factorial, 1);
    for (int64_t k = 0; k < COS_TERMS; k += 1)
    {
        if (k > 0)
        {
            z_mul_small(&factorial, ((2 * k) - 1) * (2 * k), &factorial);
        }
        OriorExactInteger value;
        z_floor_div(&power, &factorial, &value);
        z_floor_div(&value, &guard, &value);
        z_mul_small(&value, ((k % 2) == 0) ? 1 : -1, &constants->cosine[k]);
        z_mul(&power, &constants->pi, &power);
        z_mul(&power, &constants->pi, &power);
        z_floor_div(&power, &work_square, &power);
    }
    const int64_t work_bits = TURING_SCALE_BITS + TURING_GUARD_BITS;
    OriorExactInteger pi_square;
    z_mul(&constants->pi, &constants->pi, &pi_square);
    OriorExactInteger top;
    OriorExactInteger under;
    z_two_to(TURING_SCALE_BITS + (2 * work_bits), &top);
    z_mul_small(&pi_square, 96, &under);
    z_floor_div(&top, &under, &constants->c96);
    OriorExactInteger pi_fourth;
    z_mul(&pi_square, &pi_square, &pi_fourth);
    z_two_to(TURING_SCALE_BITS + (4 * work_bits), &top);
    z_mul_small(&top, 7, &top);
    z_mul_small(&pi_fourth, 46080, &under);
    z_floor_div(&top, &under, &constants->c3);
    OriorExactInteger pi_sixth;
    z_mul(&pi_fourth, &pi_square, &pi_sixth);
    z_two_to(TURING_SCALE_BITS + (6 * work_bits), &top);
    z_mul_small(&top, 31, &top);
    z_mul_small(&pi_sixth, 2580480, &under);
    z_floor_div(&top, &under, &constants->c5);
    z_floor_div(&constants->pi, &guard, &constants->pi_scaled);
    z_set(&factorial, 1);
    for (int64_t n = 0; n < E1_TERMS; n += 1)
    {
        z_mul_small(&factorial, n + 1, &factorial);
        z_floor_div(&unit, &factorial, &constants->fact[n]);
    }
    power = work;
    z_set(&factorial, 1);
    for (int64_t n = 0; n < SINC_TERMS; n += 1)
    {
        if (n > 0)
        {
            z_mul_small(&factorial, (2 * n) * ((2 * n) + 1), &factorial);
        }
        OriorExactInteger value;
        z_floor_div(&power, &factorial, &value);
        z_floor_div(&value, &guard, &value);
        z_mul_small(&value, ((n % 2) == 0) ? 1 : -1, &constants->sinc[n]);
        z_mul(&power, &constants->pi, &power);
        z_mul(&power, &constants->pi, &power);
        z_floor_div(&power, &work_square, &power);
    }
}

/* ---------------------------------------------------------------- the margin of a step */

/**
 * @brief An upper bound on ln q for a rational q >= 1: m ln 2 + 2 artanh(y), with q / 2^m in [1, 2).
 *
 * @param[in] q The value.
 * @return      The bound.
 * @note Here y = (q - 1) / (q + 1) is at most 1/3. The code cuts the series after 12 terms, and bounds its tail by a
 *       geometric sum.
 */
static ZetaRational ln_upper(ZetaRational q)
{
    int64_t m = 0;
    while (!qlt(q, qn(2, 1)))
    {
        q = qdiv(q, qn(2, 1));
        m += 1;
    }
    const ZetaRational y = qdiv(qsub(q, qn(1, 1)), qadd(q, qn(1, 1)));
    const int64_t terms = 12;
    ZetaRational total = qn(0, 1);
    for (int64_t k = 0; k < terms; k += 1)
    {
        total = qadd(total, qdiv(qpow(y, (2 * k) + 1), qn((2 * k) + 1, 1)));
    }
    total = qadd(total, qdiv(qpow(y, (2 * terms) + 1), qk(qsub(qn(1, 1), qmul(y, y)), (2 * terms) + 1)));
    return qadd(qk(ln_two_high(), m), qk(total, 2));
}

/**
 * @brief An upper bound on n^(-1/2): 2^20 over the floor of (4^20 n)^(1/2).
 *
 * @param[in] n The number.
 * @return      The bound.
 */
static ZetaRational root_upper_inverse(int64_t n)
{
    OriorExactInteger value;
    z_set(&value, n);
    OriorExactInteger power;
    z_two_to(40, &power);
    z_mul(&value, &power, &value);
    z_isqrt(&value, &value);
    OriorExactInteger top;
    z_two_to(20, &top);
    ZetaRational result;
    rational_make(&top, &value, &result);
    return result;
}

/**
 * @brief The largest |M^(4)| over cell nu, with M^(4) the fourth derivative of M, bounded above.
 *
 * @param[in] nu The cell.
 * @return       The bound.
 * @note The bound is 2 sum over n <= nu of n^(-1/2) (A^4 + 6 A^2 theta2 + 4 A theta3 + 3 theta2^2 + theta4), with
 *       A = ln((nu + 1) / n). The code rounds each term up to a whole number of 2^-FOURTH_BITS, and keeps the bound
 *       of each nu.
 */
static ZetaRational fourth_bound(int64_t nu)
{
    static int64_t kept_nu[256];
    static ZetaRational kept[256];
    static size_t kept_count = 0u;
    for (size_t at = 0u; at < kept_count; at += 1u)
    {
        if (kept_nu[at] == nu)
        {
            return kept[at];
        }
    }
    const ZetaRational t_low = qk(pi_low(), 2 * nu * nu);
    const ZetaRational theta2 = qadd(qdiv(qn(1, 1), qk(t_low, 2)), qdiv(qn(1, 1), qk(qpow(t_low, 3), 24)));
    const ZetaRational theta3 = qadd(qdiv(qn(1, 1), qk(qpow(t_low, 2), 2)), qdiv(qn(1, 1), qk(qpow(t_low, 4), 8)));
    const ZetaRational theta4 = qadd(qdiv(qn(1, 1), qpow(t_low, 3)), qdiv(qn(1, 1), qk(qpow(t_low, 5), 2)));
    OriorExactInteger grid;
    z_two_to(FOURTH_BITS, &grid);
    OriorExactInteger units;
    orior_exact_zero(&units);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        const ZetaRational a = ln_upper(qn(nu + 1, n));
        const ZetaRational a_square = qmul(a, a);
        ZetaRational inner = qmul(a_square, a_square);
        inner = qadd(inner, qmul(qk(a_square, 6), theta2));
        inner = qadd(inner, qmul(qk(a, 4), theta3));
        inner = qadd(inner, qk(qmul(theta2, theta2), 3));
        inner = qadd(inner, theta4);
        const ZetaRational term = qmul(root_upper_inverse(n), inner);
        OriorExactInteger value;
        qceil(qmul(term, qz(&grid)), &value);
        z_add(&units, &value, &units);
    }
    z_mul_small(&units, 2, &units);
    ZetaRational result;
    rational_make(&units, &grid, &result);
    if (kept_count < 256u)
    {
        kept_nu[kept_count] = nu;
        kept[kept_count] = result;
        kept_count += 1u;
    }
    return result;
}

void turing_margin(int64_t nu, int64_t p, const TuringConstants *constants, const OriorExactInteger *bound, int method,
                   OriorExactInteger values[4])
{
    const ZetaRational unit = unit_q();
    const ZetaRational t_low = qk(pi_low(), 2 * nu * nu);
    const ZetaRational h = qdiv(qk(pi_high(), 2 * ((2 * nu) + 1)), qn((int64_t)1 << p, 1));
    const ZetaRational fourth = fourth_bound(nu);
    OriorExactInteger nu_value;
    z_set(&nu_value, nu);
    OriorExactInteger root_value;
    z_isqrt(&nu_value, &root_value);
    const int64_t root_low = z_small(&root_value);
    // |dR/dx| <= sum over n of (n + 1/2) |C_n| x^(-n - 3/2) + 2 |C_n'| x^(-n - 1/2), and dx/dt = 1 / (4 pi x).
    ZetaRational remainder_slope = qn(0, 1);
    for (int64_t n = 0; n < constants->curve_count; n += 1)
    {
        const ZetaRational inner =
            qadd(qdiv(qmul(qn((2 * n) + 1, 2), constants->sizes[n]), qn(nu, 1)), qk(constants->slopes[n], 2));
        remainder_slope = qadd(remainder_slope, qdiv(inner, qk(qpow(qn(nu, 1), n), root_low)));
    }
    remainder_slope = qdiv(remainder_slope, qk(pi_low(), 4 * nu));
    const int64_t root = root_ceiling(qn(nu, 1), 2);
    const ZetaRational clock_top = ln_upper(qn(nu + 1, 1));
    ZetaRational e_ln;
    ZetaRational e_cos;
    unit_errors(&e_ln, &e_cos);
    ZetaRational errors[3];
    arithmetic(nu, constants, errors);
    ZetaRational e_w;
    ZetaRational e_turned;
    if (method == TURING_TRANSFORM)
    {
        e_w = qdiv(qsub(transform_error(nu, p, errors[1], errors[2], 0), errors[2]), qn(2, 1));
        int64_t length = 0;
        for (int64_t rest = nu; rest > 0; rest >>= 1)
        {
            length += 1;
        }
        int64_t shift = 0;
        for (int64_t rest = length; rest > 0; rest >>= 1)
        {
            shift += 1;
        }
        e_turned =
            qdiv(qk(qsub(transform_error(nu, p, errors[1], errors[2], 1), errors[2]), (int64_t)1 << shift), qn(2, 1));
    }
    else
    {
        const ZetaRational e_term = qdiv(qsub(errors[0], errors[2]), qn(2 * nu, 1));
        e_w = qk(e_term, nu);
        e_turned = qk(qadd(qadd(qmul(e_term, clock_top), e_ln), qn(2, 1)), nu);
    }
    const ZetaRational e_clock = qadd(qadd(qdiv(e_ln, qn(2, 1)), qn(1, 1)), qdiv(unit, qk(qmul(t_low, t_low), 48)));
    const ZetaRational e_d = qk(qadd(qadd(qadd(qk(e_clock, 2 * root), qmul(clock_top, e_w)), e_turned), qn(2, 1)), 2);
    const ZetaRational d_max = qadd(qk(clock_top, 8 * root), qn(1, 1));
    const ZetaRational bound_q = qz(bound);
    const ZetaRational h4 = qpow(h, 4);
    ZetaRational total = qadd(bound_q, qmul(qdiv(h, qn(4, 1)), e_d));
    total = qadd(total, qmul(qmul(qdiv(h4, qn(384, 1)), fourth), unit));
    total = qadd(total, qmul(qmul(h, remainder_slope), unit));
    total = qadd(qadd(total, qk(d_max, 2)), qn(4, 1));
    qceil(total, &values[0]);
    OriorExactInteger third;
    z_mul_small(&constants->pi, 2 * ((2 * nu) + 1), &third);
    OriorExactInteger under;
    z_two_to(p + TURING_GUARD_BITS, &under);
    z_mul_small(&under, 3, &under);
    z_floor_div(&third, &under, &values[1]);
    const ZetaRational h_low = qdiv(qk(pi_low(), 2 * ((2 * nu) + 1)), qn((int64_t)1 << p, 1));
    ZetaRational steep = qadd(e_d, qdiv(qk(bound_q, 3), h_low));
    steep = qadd(steep, qmul(qmul(qmul(qn(2, 81), qpow(h, 3)), fourth), unit));
    steep = qadd(steep, qmul(qmul(qn(5, 2), remainder_slope), unit));
    steep = qadd(steep, qdiv(qk(qadd(qk(d_max, 2), qn(4, 1)), 3), h_low));
    steep = qadd(steep, qn(2, 1));
    qceil(steep, &values[2]);
    qceil(e_d, &values[3]);
}

/* ---------------------------------------------------------------- the device */

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
 * @brief The paths of the next run of the device, under build/turing.
 *
 * @param[out] given The input [BORROWS]. 256 bytes.
 * @param[out] taken The output [BORROWS]. 256 bytes.
 */
static void run_paths(char given[256], char taken[256])
{
    static unsigned long long counter = 0u;
#ifdef _WIN32
    (void)_mkdir("build");
    (void)_mkdir("build/turing");
#else
    (void)mkdir("build", 0777);
    (void)mkdir("build/turing", 0777);
#endif
    counter += 1u;
    snprintf(given, 256u, "build/turing/run_%llu_in.bin", counter);
    snprintf(taken, 256u, "build/turing/run_%llu_out.txt", counter);
}

/** @brief The one process of the binary, started with serve: its pipes, and the bytes it wrote but the code has not
 *         read. */
typedef struct
{
    int running;      /**< 1 while the process runs. */
    char binary[512]; /**< The binary. */
#ifdef _WIN32
    HANDLE process; /**< The process. */
    HANDLE input;   /**< The write end of its standard input. */
    HANDLE output;  /**< The read end of its standard output. */
#else
    pid_t process; /**< The process. */
    int input;     /**< The write end of its standard input. */
    int output;    /**< The read end of its standard output. */
#endif
    char pending[8192];   /**< The bytes read past the last line. */
    size_t pending_count; /**< How many. */
} Served;

/** @brief The process of the binary. */
static Served served;

/**
 * @brief Ends the process of the binary: its input closes, and the code waits for it.
 */
static void served_stop(void)
{
    if (!served.running)
    {
        return;
    }
#ifdef _WIN32
    CloseHandle(served.input);
    (void)WaitForSingleObject(served.process, INFINITE);
    CloseHandle(served.output);
    CloseHandle(served.process);
#else
    (void)close(served.input);
    int status = 0;
    (void)waitpid(served.process, &status, 0);
    (void)close(served.output);
#endif
    served.running = 0;
    served.pending_count = 0u;
}

/**
 * @brief Starts the binary with serve where it is not running. Its standard error goes to that of this program.
 *
 * @param[in] binary The device program [BORROWS].
 * @return           1 where the process runs.
 * @note The longest programs of the multiple evaluation hold frames past the stack limit of the device, and stay as
 *       PTX. As new C source they would go to NVRTC, which compiles a program of their length far slower than the run.
 */
static int served_start(const char *binary)
{
    static int registered = 0;
    if (!registered)
    {
        (void)atexit(served_stop);
        registered = 1;
    }
    if (served.running && (strcmp(served.binary, binary) == 0))
    {
        return 1;
    }
    served_stop();
    snprintf(served.binary, sizeof(served.binary), "%s", binary);
#ifdef _WIN32
    (void)_putenv("CYCLE_RECORD_RETAIN_PTX=1");
    SECURITY_ATTRIBUTES inherit = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE child_input = NULL;
    HANDLE child_output = NULL;
    if (!CreatePipe(&child_input, &served.input, &inherit, 0) ||
        !CreatePipe(&served.output, &child_output, &inherit, 0))
    {
        return 0;
    }
    (void)SetHandleInformation(served.input, HANDLE_FLAG_INHERIT, 0);
    (void)SetHandleInformation(served.output, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA startup;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_input;
    startup.hStdOutput = child_output;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION information;
    // CreateProcess reads an absolute path with back slashes, not a relative one with forward slashes.
    char absolute[MAX_PATH];
    if (_fullpath(absolute, binary, sizeof(absolute)) == NULL)
    {
        snprintf(absolute, sizeof(absolute), "%s", binary);
    }
    for (char *at = absolute; *at != '\0'; at += 1)
    {
        *at = (*at == '/') ? '\\' : *at;
    }
    char command[600];
    snprintf(command, sizeof(command), "\"%s\" serve", absolute);
    const BOOL made = CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &information);
    CloseHandle(child_input);
    CloseHandle(child_output);
    if (!made)
    {
        CloseHandle(served.input);
        CloseHandle(served.output);
        return 0;
    }
    CloseHandle(information.hThread);
    served.process = information.hProcess;
#else
    int to_child[2];
    int from_child[2];
    if ((pipe(to_child) != 0) || (pipe(from_child) != 0))
    {
        return 0;
    }
    const pid_t child = fork();
    if (child < 0)
    {
        return 0;
    }
    if (child == 0)
    {
        (void)dup2(to_child[0], 0);
        (void)dup2(from_child[1], 1);
        (void)close(to_child[0]);
        (void)close(to_child[1]);
        (void)close(from_child[0]);
        (void)close(from_child[1]);
        (void)setenv("CYCLE_RECORD_RETAIN_PTX", "1", 1);
        (void)execl(binary, binary, "serve", (char *)NULL);
        _exit(127);
    }
    (void)close(to_child[0]);
    (void)close(from_child[1]);
    served.process = child;
    served.input = to_child[1];
    served.output = from_child[0];
#endif
    served.running = 1;
    served.pending_count = 0u;
    return 1;
}

/**
 * @brief Writes bytes to the process.
 *
 * @param[in] text The bytes [BORROWS].
 * @return         1 where every byte went.
 */
static int served_write(const char *text)
{
    const size_t length = strlen(text);
#ifdef _WIN32
    DWORD written = 0;
    // The request is two paths, far below 4 GiB.
    return WriteFile(served.input, text, (DWORD)length, &written, NULL) && (written == (DWORD)length);
#else
    // The request is two paths, and fits the count of one write.
    return write(served.input, text, length) == (ssize_t)length;
#endif
}

/**
 * @brief Reads one line from the process, its end of line dropped.
 *
 * @param[out] line     The line [BORROWS].
 * @param[in]  capacity Bytes of the line.
 * @return              1 where a line came, 0 where the process closed its output.
 */
static int served_line(char *line, size_t capacity)
{
    for (;;)
    {
        for (size_t at = 0u; at < served.pending_count; at += 1u)
        {
            if (served.pending[at] != '\n')
            {
                continue;
            }
            const size_t length = (at < (capacity - 1u)) ? at : (capacity - 1u);
            memcpy(line, served.pending, length);
            line[length] = '\0';
            if ((length > 0u) && (line[length - 1u] == '\r'))
            {
                line[length - 1u] = '\0';
            }
            memmove(served.pending, served.pending + at + 1u, served.pending_count - at - 1u);
            served.pending_count -= at + 1u;
            return 1;
        }
        if (served.pending_count >= sizeof(served.pending))
        {
            // A line longer than the buffer comes in pieces.
            served.pending_count = 0u;
        }
        const size_t room = sizeof(served.pending) - served.pending_count;
#ifdef _WIN32
        DWORD got = 0;
        // The room is below the size of the buffer.
        if (!ReadFile(served.output, served.pending + served.pending_count, (DWORD)room, &got, NULL) || (got == 0))
        {
            return 0;
        }
        served.pending_count += got;
#else
        const ssize_t got = read(served.output, served.pending + served.pending_count, room);
        if (got <= 0)
        {
            return 0;
        }
        // The count read is at most the room.
        served.pending_count += (size_t)got;
#endif
    }
}

/**
 * @brief The run of the device from the input to the output, sent to the one process of the binary.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] given  The input [BORROWS].
 * @param[in] taken  The output [BORROWS].
 * @param[in] what   The name of the run [BORROWS].
 * @note The process loads each program onto the device once, and runs every later stage with the same program on
 *       it. It answers each run with the line "done" and the code of the run, once it writes the output.
 * @note A run whose checks fail writes nothing that the machine reads. It runs once more in a new process, and the
 *       code prints both failures.
 */
static void device(const char *binary, const char *given, const char *taken, const char *what)
{
    static char printed[65536];
    for (int attempt = 0; attempt < 2; attempt += 1)
    {
        size_t printed_count = 0u;
        int code = -1;
        if (served_start(binary))
        {
            char request[600];
            snprintf(request, sizeof(request), "%s\n%s\n", given, taken);
            if (served_write(request))
            {
                char line[4096];
                while (served_line(line, sizeof(line)))
                {
                    if (strncmp(line, "done ", 5u) == 0)
                    {
                        code = atoi(line + 5);
                        break;
                    }
                    const size_t length = strlen(line);
                    if ((printed_count + length + 2u) < sizeof(printed))
                    {
                        memcpy(printed + printed_count, line, length);
                        printed[printed_count + length] = '\n';
                        printed_count += length + 1u;
                    }
                }
            }
        }
        if (code == 0)
        {
            return;
        }
        printed[printed_count] = '\0';
        const size_t start = (printed_count > 2000u) ? (printed_count - 2000u) : 0u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, printed + start);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, what);
            mmgr_line_put(output_line, ": the device run failed, attempt ");
            mmgr_line_decimal_signed(output_line, attempt + 1);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        served_stop();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, what);
        mmgr_line_put(output_line, ": the device run failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    exit(1);
}

void turing_parse_hex(const char *text, OriorExactInteger *result)
{
    const int negative = text[0] == '-';
    const char *digit = text + (negative ? 1 : 0);
    orior_exact_zero(result);
    for (; *digit != '\0'; digit += 1)
    {
        const char c = *digit;
        int value = -1;
        if ((c >= '0') && (c <= '9'))
        {
            value = c - '0';
        }
        else if ((c >= 'a') && (c <= 'f'))
        {
            value = 10 + (c - 'a');
        }
        else if ((c >= 'A') && (c <= 'F'))
        {
            value = 10 + (c - 'A');
        }
        if (value < 0)
        {
            break;
        }
        z_mul_small(result, 16, result);
        OriorExactInteger part;
        z_set(&part, value);
        z_add(result, &part, result);
    }
    if (negative)
    {
        z_neg(result, result);
    }
}

/**
 * @brief Splits a line into its words, in place.
 *
 * @param[in,out] line  The line [BORROWS].
 * @param[out]    words The words [BORROWS].
 * @param[in]     most  How many words the array holds.
 * @return              How many words.
 */
static size_t split_words(char *line, char **words, size_t most)
{
    size_t count = 0u;
    char *cursor = line;
    while (*cursor != '\0')
    {
        while ((*cursor == ' ') || (*cursor == '\t') || (*cursor == '\r') || (*cursor == '\n'))
        {
            cursor += 1;
        }
        if (*cursor == '\0')
        {
            break;
        }
        if (count < most)
        {
            words[count] = cursor;
            count += 1u;
        }
        while ((*cursor != '\0') && (*cursor != ' ') && (*cursor != '\t') && (*cursor != '\r') && (*cursor != '\n'))
        {
            cursor += 1;
        }
        if (*cursor != '\0')
        {
            *cursor = '\0';
            cursor += 1;
        }
    }
    return count;
}

/**
 * @brief Reads the output of the device for a cell, past its point lines.
 *
 * @param[in]  nu   The cell.
 * @param[in]  p    The bits of the lattice.
 * @param[in]  path The output [BORROWS].
 * @param[out] cell The cell [BORROWS].
 */
static void read_cell(int64_t nu, int64_t p, const char *path, TuringCell *cell)
{
    memset(cell, 0, sizeof(*cell));
    cell->nu = nu;
    cell->p = p;
    cell->lift = (int64_t)1 << ((2 * TURING_FINEST) - p);
    snprintf(cell->path, sizeof(cell->path), "%s", path);
    FILE *const handle = fopen(path, "r");
    if (handle == NULL)
    {
        zeta_stop("the device wrote no output");
    }
    static char line[1 << 20];
    while (fgets(line, sizeof(line), handle) != NULL)
    {
        char *words[64];
        const size_t count = split_words(line, words, 64u);
        if ((count == 0u) || (strcmp(words[0], "point") == 0))
        {
            continue;
        }
        if (strcmp(words[0], "cell") == 0)
        {
            cell->points = (count > 3u) ? atoll(words[3]) : 0;
            cell->first = (count > 4u) ? atoll(words[4]) : 0;
            cell->last = (count > 5u) ? atoll(words[5]) : 0;
            continue;
        }
        static const char *const records[3] = {"first", "last", "origin"};
        int matched = 0;
        for (int which = 0; which < 3; which += 1)
        {
            if (strcmp(words[0], records[which]) == 0)
            {
                cell->at_count[which] = 0u;
                for (size_t at = 1u; (at < count) && (at <= 8u); at += 1u)
                {
                    turing_parse_hex(words[at], &cell->at[which][at - 1u]);
                    cell->at_count[which] = at;
                }
                matched = 1;
            }
        }
        if (matched)
        {
            continue;
        }
        if (strcmp(words[0], "control") == 0)
        {
            for (size_t at = 1u; (at < count) && (at <= 2u); at += 1u)
            {
                turing_parse_hex(words[at], &cell->control[at - 1u]);
            }
            continue;
        }
        for (int which = 0; which < SUM_COUNT; which += 1)
        {
            if ((strcmp(words[0], TURING_SUM_NAMES[which]) == 0) && (count > 1u))
            {
                turing_parse_hex(words[1], &cell->sums[which]);
            }
        }
        if (strcmp(words[0], "host") == 0)
        {
            cell->failed = 0;
            for (size_t at = 1u; at < count; at += 1u)
            {
                cell->failed += strcmp(words[at], "1") != 0;
            }
        }
        if (strcmp(words[0], "steps") == 0)
        {
            cell->step_count = 0u;
            for (size_t at = 1u; (at < count) && (at <= 16u); at += 1u)
            {
                cell->steps[at - 1u] = atoll(words[at]);
                cell->step_count = at;
            }
        }
        if ((strcmp(words[0], "apart") == 0) && (count > 2u))
        {
            cell->has_apart = 1;
            turing_parse_hex(words[1], &cell->apart);
            cell->apart_at = atoll(words[2]);
        }
        static const char *const margins[3] = {"clean", "flagged", "single"};
        for (int which = 0; which < 3; which += 1)
        {
            if ((strcmp(words[0], margins[which]) == 0) && (count > 1u))
            {
                cell->has_margins[which] = 1;
                turing_parse_hex(words[1], &cell->margins[which]);
            }
        }
    }
    fclose(handle);
}

/**
 * @brief Writes the constants that every run reads, from ln 2 to the sinc terms, with the bounds of the run.
 *
 * @param[in] handle      The file [BORROWS].
 * @param[in] constants   The constants [BORROWS].
 * @param[in] bound       The bound on Z [BORROWS].
 * @param[in] theta_bound The bound on theta / pi [BORROWS].
 */
static void put_constants(FILE *handle, const TuringConstants *constants, const OriorExactInteger *bound,
                          const OriorExactInteger *theta_bound)
{
    put(handle, &constants->ln2);
    for (int64_t at = 0; at < (constants->curve_count * GAMMA_TERMS); at += 1)
    {
        put(handle, &constants->gamma[at]);
    }
    for (int at = 0; at < ARTANH_TERMS; at += 1)
    {
        put(handle, &constants->artanh[at]);
    }
    for (int at = 0; at < COS_TERMS; at += 1)
    {
        put(handle, &constants->cosine[at]);
    }
    put(handle, &constants->c96);
    put(handle, &constants->c3);
    put(handle, &constants->c5);
    put(handle, bound);
    put(handle, theta_bound);
    put(handle, &constants->pi_scaled);
    for (int at = 0; at < E1_TERMS; at += 1)
    {
        put(handle, &constants->fact[at]);
    }
    for (int at = 0; at < SINC_TERMS; at += 1)
    {
        put(handle, &constants->sinc[at]);
    }
}

void turing_run_cell(const char *binary, const TuringConstants *constants, int64_t nu, int64_t p, int method,
                     int64_t level, int listing, TuringCell *cell, OriorExactInteger *bound)
{
    const int64_t terms = (method == TURING_EM) ? turing_em_terms(level) : 0;
    const int64_t heads = (method == TURING_EM) ? turing_em_heads(nu, terms) : 0;
    const int64_t per_point = (method == TURING_EM) ? (heads - 1) : nu;
    int64_t piece = (int64_t)1 << p;
    while ((piece * per_point) >= ((int64_t)1 << LANE_BITS))
    {
        piece >>= 1;
    }
    OriorExactInteger theta_bound;
    if (method == TURING_EM)
    {
        turing_em_bounds(nu, heads, terms, bound, &theta_bound);
    }
    else
    {
        turing_bounds(nu, constants, p, method, bound, &theta_bound);
    }
    char given[256];
    char taken[256];
    run_paths(given, taken);
    FILE *const handle = fopen(given, "wb");
    if (handle == NULL)
    {
        zeta_stop("cannot write the input of the device");
    }
    const int64_t header[18] = {(int64_t)1 << p,
                                (piece < CHECKED) ? piece : CHECKED,
                                nu,
                                p,
                                ARTANH_TERMS,
                                COS_TERMS,
                                GAMMA_TERMS,
                                NEWTON_STEPS,
                                piece,
                                method,
                                ORDER,
                                BETA,
                                E1_TERMS,
                                SINC_TERMS,
                                heads,
                                terms,
                                listing,
                                constants->curve_count};
    fwrite(header, sizeof(int64_t), 18u, handle);
    put_constants(handle, constants, bound, &theta_bound);
    if (method == TURING_EM)
    {
        OriorExactInteger *const ratios =
            (OriorExactInteger *)turing_memory(calloc((size_t)terms + 1u, sizeof(OriorExactInteger)));
        em_ratios(terms, ratios);
        for (int64_t k = 0; k < (terms - 1); k += 1)
        {
            put(handle, &ratios[k]);
        }
        free(ratios);
    }
    if (listing == 2)
    {
        OriorExactInteger values[4];
        turing_margin(nu, p, constants, bound, TURING_TRANSFORM, values);
        for (int at = 0; at < 3; at += 1)
        {
            put(handle, &values[at]);
        }
    }
    fclose(handle);
    char what[128];
    snprintf(what, sizeof(what), "cell %lld at 2^%lld points", (long long)nu, (long long)p);
    device(binary, given, taken, what);
    (void)remove(given);
    read_cell(nu, p, taken, cell);
}

int64_t turing_run_points(const char *binary, const TuringConstants *constants, int64_t nu, int64_t p,
                          const int64_t *js, size_t count, int bracketed, TuringPoint *points)
{
    size_t piece = 1u;
    while (((piece * 2u) <= count) && (((int64_t)piece * 2 * nu) < ((int64_t)1 << LANE_BITS)))
    {
        piece *= 2u;
    }
    const size_t padded = count + ((piece - (count % piece)) % piece);
    OriorExactInteger bound;
    OriorExactInteger theta_bound;
    turing_bounds(nu, constants, p, TURING_PAIRS, &bound, &theta_bound);
    char given[256];
    char taken[256];
    run_paths(given, taken);
    FILE *const handle = fopen(given, "wb");
    if (handle == NULL)
    {
        zeta_stop("cannot write the input of the device");
    }
    // The pieces and the counts here fit a machine integer.
    const int64_t header[18] = {(int64_t)padded,
                                ((int64_t)piece < CHECKED) ? (int64_t)piece : CHECKED,
                                nu,
                                p,
                                ARTANH_TERMS,
                                COS_TERMS,
                                GAMMA_TERMS,
                                NEWTON_STEPS,
                                (int64_t)piece,
                                TURING_POINTS,
                                ORDER,
                                BETA,
                                E1_TERMS,
                                SINC_TERMS,
                                0,
                                0,
                                bracketed ? 3 : 2,
                                constants->curve_count};
    fwrite(header, sizeof(int64_t), 18u, handle);
    put_constants(handle, constants, &bound, &theta_bound);
    if (!bracketed)
    {
        OriorExactInteger values[4];
        turing_margin(nu, p, constants, &bound, TURING_PAIRS, values);
        for (int at = 0; at < 3; at += 1)
        {
            put(handle, &values[at]);
        }
    }
    for (size_t at = 0u; at < padded; at += 1u)
    {
        const int64_t j = (at < count) ? js[at] : js[count - 1u];
        fwrite(&j, sizeof(int64_t), 1u, handle);
    }
    fclose(handle);
    char what[160];
    snprintf(what, sizeof(what), "cell %lld, %llu listed points at 2^%lld", (long long)nu, (unsigned long long)count,
             (long long)p);
    device(binary, given, taken, what);
    (void)remove(given);
    FILE *const output = fopen(taken, "r");
    if (output == NULL)
    {
        zeta_stop("the device wrote no output");
    }
    static char line[1 << 16];
    size_t found = 0u;
    int64_t failed = 0;
    while (fgets(line, sizeof(line), output) != NULL)
    {
        char *words[64];
        const size_t words_count = split_words(line, words, 64u);
        if (words_count == 0u)
        {
            continue;
        }
        if ((strcmp(words[0], "point") == 0) && (words_count >= 3u))
        {
            if (found < count)
            {
                turing_parse_hex(words[1], &points[found].sign);
                turing_parse_hex(words[2], &points[found].s);
                if (bracketed)
                {
                    turing_parse_hex(words[words_count - 2u], &points[found].extra[0]);
                    turing_parse_hex(words[words_count - 1u], &points[found].extra[1]);
                }
                else
                {
                    turing_parse_hex(words[words_count - 1u], &points[found].extra[0]);
                    orior_exact_zero(&points[found].extra[1]);
                }
            }
            found += 1u;
        }
        else if (strcmp(words[0], "host") == 0)
        {
            failed = 0;
            for (size_t at = 1u; at < words_count; at += 1u)
            {
                failed += strcmp(words[at], "1") != 0;
            }
        }
    }
    fclose(output);
    (void)remove(taken);
    return failed;
}

/* ---------------------------------------------------------------- the count */

double turing_rises(int64_t nu)
{
    const double low = (double)nu;
    const double high = (double)(nu + 1);
    return ((2.0 * high * high * log(high)) - (high * high)) - ((2.0 * low * low * log(low)) - (low * low));
}

int64_t turing_lattice(int64_t nu, int64_t rate)
{
    // The bits of a lattice are small.
    const int64_t p = (int64_t)ceil(log2((double)rate * turing_rises(nu)));
    return (p > TURING_COARSEST) ? p : TURING_COARSEST;
}

void turing_x2(const TuringCell *cell, int which, OriorExactInteger *result)
{
    z_mul_small(&cell->at[which][1], cell->lift, result);
}

/**
 * @brief x^2 at the last point of the cell.
 *
 * @param[in]  cell   The cell [BORROWS].
 * @param[out] result x^2 [BORROWS].
 */
static void end_x2(const TuringCell *cell, OriorExactInteger *result)
{
    z_set(result, cell->nu * cell->nu);
    OriorExactInteger power;
    z_two_to(cell->p, &power);
    z_mul(result, &power, result);
    OriorExactInteger step;
    z_set(&step, (((int64_t)1 << cell->p) - 1) * ((2 * cell->nu) + 1));
    z_add(result, &step, result);
    z_mul_small(result, cell->lift, result);
}

/**
 * @brief The bound of Trudgian at t = 2 pi x^2, over 2 pi, at 4^-22.
 *
 * @param[in] x2 x^2 at 4^-22 [BORROWS].
 * @return       The bound.
 */
static ZetaRational trudgian(const OriorExactInteger *x2)
{
    OriorExactInteger fine;
    z_two_to(2 * TURING_FINEST, &fine);
    ZetaRational x2_q;
    rational_make(x2, &fine, &x2_q);
    const ZetaRational t = qmul(qk(pi_high(), 2), x2_q);
    const ZetaRational bound = qadd(qn(2067, 1000), qmul(qn(59, 1000), ln_high(t)));
    return qdiv(qmul(bound, qz(&fine)), qk(pi_low(), 2));
}

/**
 * @brief 1 where the last certified sign of before and the first of after differ: a zero between them.
 *
 * @param[in] before The cell before [BORROWS].
 * @param[in] after  The cell after [BORROWS].
 * @return           0 or 1.
 */
static int joined(const TuringCell *before, const TuringCell *after)
{
    return !orior_exact_equal(&before->at[AT_LAST][0], &after->at[AT_FIRST][0]);
}

void turing_below(const TuringCell *cell, const TuringCell *back, ZetaRational *result)
{
    const OriorExactInteger *const s = back->sums;
    OriorExactInteger start;
    turing_x2(back, AT_FIRST, &start);
    OriorExactInteger cell_first;
    turing_x2(cell, AT_FIRST, &cell_first);
    OriorExactInteger weight;
    z_sub(&cell_first, &start, &weight);
    OriorExactInteger theta;
    OriorExactInteger part;
    z_mul_small(&s[SUM_LOW_AFTER], back->lift, &theta);
    z_mul_small(&cell->sums[SUM_LOW_BEFORE], cell->lift, &part);
    z_add(&theta, &part, &theta);
    OriorExactInteger zeros;
    z_mul_small(&s[SUM_ZEROS_P_AFTER], back->lift, &zeros);
    z_mul(&s[SUM_ZEROS_AFTER], &start, &part);
    z_sub(&zeros, &part, &zeros);
    if (joined(back, cell))
    {
        OriorExactInteger back_last;
        turing_x2(back, AT_LAST, &back_last);
        z_sub(&back_last, &start, &part);
        z_add(&zeros, &part, &zeros);
    }
    OriorExactInteger power;
    z_two_to(TURING_SCALE_BITS, &power);
    z_mul(&zeros, &power, &zeros);
    z_add(&theta, &zeros, &theta);
    OriorExactInteger under;
    z_mul(&weight, &power, &under);
    ZetaRational main_part;
    rational_make(&theta, &under, &main_part);
    *result = qsub(qadd(qn(1, 1), main_part), qdiv(trudgian(&cell_first), qz(&weight)));
}

void turing_above(const TuringCell *cell, const TuringCell *ahead, ZetaRational *result)
{
    const OriorExactInteger *const s = cell->sums;
    OriorExactInteger start;
    turing_x2(cell, AT_FIRST, &start);
    OriorExactInteger end;
    if (ahead != NULL)
    {
        turing_x2(ahead, AT_FIRST, &end);
    }
    else
    {
        end_x2(cell, &end);
    }
    OriorExactInteger weight;
    z_sub(&end, &start, &weight);
    OriorExactInteger theta;
    OriorExactInteger part;
    z_mul_small(&s[SUM_HIGH_AFTER], cell->lift, &theta);
    if (ahead != NULL)
    {
        z_mul_small(&ahead->at[AT_ORIGIN][3], (2 * cell->nu + 1) * cell->lift, &part);
        z_add(&theta, &part, &theta);
        z_mul_small(&ahead->sums[SUM_HIGH_UPTO], ahead->lift, &part);
        z_add(&theta, &part, &theta);
    }
    OriorExactInteger zeros;
    z_mul(&s[SUM_ZEROS_AFTER], &end, &zeros);
    z_mul_small(&s[SUM_ZEROS_Q_AFTER], cell->lift, &part);
    z_sub(&zeros, &part, &zeros);
    OriorExactInteger power;
    z_two_to(TURING_SCALE_BITS, &power);
    z_mul(&zeros, &power, &zeros);
    z_sub(&theta, &zeros, &theta);
    OriorExactInteger under;
    z_mul(&weight, &power, &under);
    ZetaRational main_part;
    rational_make(&theta, &under, &main_part);
    *result = qadd(qadd(qn(1, 1), qdiv(trudgian(&end), qz(&weight))), main_part);
}

int64_t turing_between(const TuringCell *cell, const TuringCell *ahead)
{
    return z_small(&cell->sums[SUM_ZEROS_AFTER]) + joined(cell, ahead);
}

double turing_t_of(const OriorExactInteger *x2)
{
    // x^2 at 4^-22 stays below 2^63 for the cells of this machine.
    return ((2.0 * 3.141592653589793) * (double)z_small(x2)) / 17592186044416.0;
}
