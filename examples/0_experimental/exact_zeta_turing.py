#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-028
#
# Turing's method, run as a machine over the automata of exact_zeta_turing.cu. Each automaton is a fixed program of the
# record machine, checked against the host word for word. This machine sits one level up and runs them a cell at a time.
# It joins what each cell returns to the results of its neighbors. It halts with a proven count, or with a cell that it
# could not close.
#
#   Usage:  `python examples/0_experimental/exact_zeta_turing.py <binary> [first] [last] [rate] [pairs|transform|both]`
#           `python examples/0_experimental/exact_zeta_turing.py <binary> [first] [last] [rate] em`
#           `python examples/0_experimental/exact_zeta_turing.py <binary> [first] [last] [rate] refine [check]`
#
# For each run, it writes the input of the device to a file in a temporary directory, and reads the output back. Every
# run goes to one process of the binary, started with `serve`. That process loads each program onto the device once, and
# runs every later stage with the same program on it.
#
# It sits in 0_experimental and is an entry in the analytic number theory workbook, on its rail. It verifies the zeros
# up to a height that the caller gives, and says nothing past it.
#
# THE CELLS
#
# Cell nu holds the points `t = 2 pi s`, `s = x^2 = nu^2 + j (2 nu + 1) / P`, j below `P = 2^p`. The points are even in
# t, and in theta to within `1 / nu` across the cell. P is the least power of 2 that gives the cell `rate` points or
# more for each unit that `theta / pi` rises across it, the share of each zero.
#
# The device works out every value of the cell itself. It starts from nothing but nu, p, and constants that do not
# depend on nu:
#   - ln k and `k^(-1/2)` once for each `k <= nu`.
#   - `theta / pi` and the remainder term once for each point.
#   - the main sum of each point over `k <= nu`, and the sign of Z that it certifies.
#   - the zeros that it finds between certified signs.
#   - the sums of Turing's method over the two ranges of the cell about its first certified point F.
#
# A cell returns F, its last certified point, its point 0, and those sums.
#
# THE MAIN SUM
#
# By pairs, the device takes the nu terms of each point one lane apiece, nu P lanes a cell.
#
# By the multiple evaluation of Odlyzko and Schonhage, it takes `F_j = sum over k of a_k exp(-2 pi i j pos_k / P)` at
# every point at once, with `a_k = k^(-1/2) exp(-2 pi i nu^2 ln k)` and `pos_k = (2 nu + 1) ln k`. F is the discrete
# Fourier transform of `u_h / P = sum over k of q_k / (z_h - w_k)`. That is a sum of nu poles on the unit circle at the
# P roots of unity z_h. A tree of multipole and local expansions of order ORDER, over leaves of 2^BETA frequencies,
# evaluates it. The tree takes the near poles of each leaf whole through the Dirichlet kernel. Half of the main sum is
# `Re(exp(i theta) F)`, and its lanes a cell grow as P instead of nu P.
#
# With both, the device runs the two and writes the max that their Z differ by over the cell.
#
# THE BOUND ON Z
#
# `Z = 2 sum + (-1)^(nu - 1) x^(-1/2) sum over n <= K of C_n(z) x^(-n) + R_K`, and for `t >= 200`,
# `|R_K| < d_K t^(-(2K + 3) / 4)`. These are Gabcke's bounds, Satz 3.2.2 of Gabcke's thesis (Neue Herleitung und
# explizite Restabschätzung der Riemann-Siegel-Formel, Göttingen 1979), `d_0 = 0.127` up to `d_10 = 25966`, in GABCKE.
# The bound of K is the least of them, and every C_n to it runs on the device.
#
# The value theta is `(t/2) log(t / (2 pi e)) - pi/8 + 1/(48 t) + 7/(5760 t^3) + 31/(80640 t^5) + E_theta`, with
# `|E_theta| < 1 / (3322 t^7)` for `t >= 10` (the same thesis, introduction, (4) and (5)). Below `t = 10`, Brent's bound
# past `1/(48 t)` applies, with the two terms after it: `(7/5760 + pi/960) t^(-3) + exp(-pi t) / 2`. The source is On
# asymptotic approximations to the log-Gamma and Riemann-Siegel theta functions, Theorems 5 and 6.
#
# An error in theta moves Z by at max `2 sum of m^(-1/2) <= 4 nu^(1/2)` times it. The device's own error is a unit of
# 2^-62 a division and a unit a constant. `arithmetic` bounds it step by step. `transform_error` bounds that of the
# multiple evaluation, with its truncation. The code certifies a sign of Z where |Z| exceeds the sum of the three.
#
# By Euler-Maclaurin, `Z = sum over n < N of n^(-1/2) cos(theta - t ln n) + Re(exp(i theta) N^(-s) C)`. The cell chooses
# N and the Bernoulli terms M. Johansson's bound on the remainder holds at every `t > 0` (Johansson, arXiv:1309.2877,
# Theorem 1): `em_bounds`.
#
# THE CELLS BELOW 168 PI
#
# With em, cells `first` to `last` run by Euler-Maclaurin, from cell 1 up. Riemann-Siegel over cells `last` to `last` +
# 2 certifies N at the F of cell `last` + 1, T_a. Where the certified sign changes in (cell `first`'s F, T_a] number
# N(T_a), every zero up to T_a is on the line and simple.
#
# THE COUNT
#
# Two certified points of opposite sign, with no certified point between, hold a zero of Z between them: a zero of zeta
# on the line. `N(t) = theta(t) / pi + 1 + S(t)` off the ordinates. For `t_2 > t_1 > 168 pi`,
# `|integral of S from t_1 to t_2| <= 2.067 + 0.059 log t_2` (Trudgian, Improvements to Turing's method, Theorem 2.2).
# On a lattice step `[t_i, t_(i+1)]`, theta lies between its ends, and the zeros certified past T are at max
# `N(t) - N(T)`.
#
# Over a window after T, `N(T) <= 1 + B / (2 pi D) + sum of d_i (Theta_(i+1) - c_i) / D`. Over one before it,
# `N(T) >= 1 + sum of d_i (k_(i+1) + Theta_i) / D - B / (2 pi D)`. Here d_i are the steps of x^2, and D their sum. `c_i`
# is the count of zeros certified in `(T, t_i]`, and `k_(i+1)` the count in `(t_(i+1), T]`. N is a whole number.
#
# The sum of `d_i c_i` sums, over those zeros, x^2 at the end of the window less x^2 at the later point of the zero. The
# sum of `d_i k_(i+1)` is the sum of x^2 at the earlier point of the zero, less x^2 at the start of the window. The
# count of zeros of a cell, and its sums of x^2 at their ends, carry all of both. Every step of x^2 in cell nu is
# `(2 nu + 1) / P`, the one from its last point to the point 0 of the next cell too. That one weighs `theta / pi` at
# point 0.
#
# THE MACHINE
#
# The machine certifies N at the F of every cell. It uses the window back to the F of the cell before, and the one on to
# the F of the cell after. Two sets of cells run again on a lattice four times finer, and the machine takes the counts
# again:
#   - a cell whose certified zeros fall short of the difference of N at its F and at the F of the next cell.
#   - the cells about an F where N has no certified single value.
#
# Where every cell closes, every zero from the F of the second cell to the last F is a certified sign change. Each is on
# the line, and simple.
#
# With refine, each cell runs by the multiple evaluation with F', and `Z' = 2 Re(exp(i theta) F (i theta' + F'/F))` at
# every point. The device calls a step clean where the cubic through Z and Z' at its ends clears the margin. The margin
# is the bound in `margin` on how far the cubic can lie from Z over the step. A clean step holds no zero.
#
# A run of one sign between two certified points, where the run holds a step that is not clean, gets a new
# certification. It runs at 2^REFINE_BITS times the points, by pairs at the listed points with their own Z' (method 4 of
# the device). It runs again inside that run where a fine step is not clean, to REFINE_DEPTH times.
#
# With check, a step whose ends change sign closes only where the slope of the cubic clears the steepness and Z crosses
# it once. A run that holds one that does not gets a new certification the same way. Every zero that it adds is a
# certified sign change, bracketed between two certified points. A cell that is still short runs again on the whole
# lattice four times finer.
#
# Positive control: the device's Z at two points of the first cell against the house's main sum and remainder,
# exact_zeta_riemann_siegel's rs_cut_at. By Euler-Maclaurin, against its em_at, and at cell `last` against
# Riemann-Siegel's Z within the two bounds.

import array
import atexit
import math
import os
import subprocess
import sys
import tempfile
import time
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import exact_zeta_riemann_siegel as rs  # noqa: E402
import exact_zeta_zeros as zz  # noqa: E402

naturals = zz.naturals

SCALE_BITS = 62
GUARD_BITS = 64
WORK = 1 << (SCALE_BITS + GUARD_BITS)
ARTANH_TERMS = 21
COS_TERMS = 18
GAMMA_TERMS = 56
NEWTON_STEPS = 8
FOLDS = 33
LANE_BITS = 22
CHECKED = 64
FINEST = 22
COARSEST = 4
ROUNDS = 4
ORDER = 28
BETA = 4
E1_TERMS = 48
SINC_TERMS = 18
METHODS = {"pairs": 0, "transform": 1, "both": 2, "em": 3, "points": 4}
PI_LOW = Fraction(314159265, 100000000)
PI_HIGH = Fraction(314159266, 100000000)
LN_TWO_HIGH = Fraction(693148, 1000000)
SUMS = ("low_before", "low_after", "high_upto", "high_after", "zeros_upto", "zeros_after", "zeros_q_upto",
        "zeros_q_after", "zeros_p_upto", "zeros_p_after", "loose")
# Gabcke, Satz 3.2.2, for t >= 200: |R_K(t)| < d_K t^(-(2K + 3) / 4), by (b) for K <= 9 and by (a) for K = 10
GABCKE = (Fraction(127, 1000), Fraction(53, 1000), Fraction(11, 1000), Fraction(31, 1000), Fraction(17, 1000),
          Fraction(61, 1000), Fraction(661, 1000), Fraction(92, 10), Fraction(130), Fraction(1837), Fraction(25966))
# the curves C_0 to C_K, with K the one whose bound is least at `t = 200`. `d_K' / d_K t^(-(K' - K) / 2)` falls as t
# rises, and the least there is the least at every t past it
CURVES = 1 + min(range(len(GABCKE)), key=lambda k: GABCKE[k] ** 4 / Fraction(200) ** (2 * k + 3))


def floor_at_scale(value):
    """A value stored at WORK, floored to 2^62."""
    return value >> GUARD_BITS


class Constants:
    """Every constant that the automata read, at 2^62, each within a unit, and none of them dependent on the cell."""

    def __init__(self):
        self.pi = naturals._pi_machin(WORK)
        decimal = SCALE_BITS * 30103 // 100000 + 61 * GAMMA_TERMS // 100 + 3 * CURVES + 40
        curve = rs.Curve(decimal)
        self.curves = [[rs.compare(g, 0) * ((abs(g) << SCALE_BITS) // 10 ** decimal) for g in curve.gamma(n, GAMMA_TERMS)]
                       for n in range(CURVES)]
        self.gamma = [g for row in self.curves for g in row]
        # each curve's size and slope on [-1, 1] from its coefficients at 2^62, with their error
        self.sizes = [Fraction(sum(abs(g) + 1 for g in row), 1 << SCALE_BITS) for row in self.curves]
        self.slopes = [Fraction(sum(n * (abs(g) + 1) for n, g in enumerate(row)), 1 << SCALE_BITS) for row in self.curves]
        self.ln2 = floor_at_scale(zz._ln_by_two(2, WORK))
        self.artanh = [(1 << SCALE_BITS) // (2 * k + 1) for k in range(ARTANH_TERMS)]
        self.cosine = []
        power = WORK
        for k in range(COS_TERMS):
            self.cosine.append((1 - 2 * (k % 2)) * floor_at_scale(power // math.factorial(2 * k)))
            power = power * self.pi * self.pi // (WORK * WORK)
        self.c96 = (1 << (SCALE_BITS + 2 * (SCALE_BITS + GUARD_BITS))) // (96 * self.pi * self.pi)
        self.c3 = (7 << (SCALE_BITS + 4 * (SCALE_BITS + GUARD_BITS))) // (46080 * self.pi ** 4)
        self.c5 = (31 << (SCALE_BITS + 6 * (SCALE_BITS + GUARD_BITS))) // (2580480 * self.pi ** 6)
        self.pi_scaled = floor_at_scale(self.pi)
        self.fact = [(1 << SCALE_BITS) // math.factorial(n + 1) for n in range(E1_TERMS)]
        self.sinc = []
        power = WORK
        for n in range(SINC_TERMS):
            self.sinc.append((1 - 2 * (n % 2)) * floor_at_scale(power // math.factorial(2 * n + 1)))
            power = power * self.pi * self.pi // (WORK * WORK)


def put(handle, v):
    mag = abs(v)
    limbs = [(mag >> (32 * i)) & 0xFFFFFFFF for i in range(max(1, (mag.bit_length() + 31) // 32))]
    array.array("q", (len(limbs),)).tofile(handle)
    array.array("I", limbs).tofile(handle)
    array.array("q", (rs.compare(v, 0),)).tofile(handle)


def root_ceiling(value, k):
    """The least integer r with r^k >= value, for value >= 0."""
    r = max(0, int(round(float(value) ** (1.0 / k))) - 2)
    while Fraction(r) ** k < value:
        r += 1
    return r


def newton_error(v):
    """The device's error on `w^(-1/2)`, `1 <= w <= v`, in units of 2^-62, after NEWTON_STEPS of Newton's rule.

    The rule starts from `2^(-g - 1)`, with `4^g < w <= 4^(g + 1)`, which is below the root and above half of it. Let
    `R = 2^62 w^(-1/2)`. The floors of a step take y below `y (3 - w y^2) / 2` by at max 1.5 units. The floors of y^2
    and of `w y^2` put it above by at max `y (w + 2) / 2^63`, below `(w + 2)^(1/2)` units. The ratio `r = y / R` then
    stays above the sequence `rho' = min(g(rho), g(1 + up / R)) - 1.5 / R`, with `g(r) = r (3 - r^2) / 2`, from
    `rho = 1/2`, taken here at 2^256 and rounded down. The function g rises on [0, 1], and the start at 1/2 is the
    worst."""
    scale = 1 << 256
    s = root_ceiling(Fraction(v), 2)
    up = root_ceiling(Fraction(v + 2), 2) + 1
    down = 3 * s * scale // (2 << SCALE_BITS) + 1
    up_ratio = up * s * scale // (1 << SCALE_BITS) + 1

    def g(r):
        square = -(-r * r // scale)
        return r * (3 * scale - square) // (2 * scale)

    rho = scale // 2
    for _ in range(NEWTON_STEPS):
        rho = min(g(rho), g(scale + up_ratio)) - down
    return max(-(-(scale - rho) * (1 << SCALE_BITS) // scale), up) + 1


def unit_errors():
    """e_ln, the error on a folded logarithm, and e_cos, on cos(pi s) by Horner's rule, in units of 2^-62.

    Every value that the device takes a product back from is below 12 times 2^62 in size. The partial sums of cos stay
    below `cosh(pi) < 11.6`, and those of artanh below 1.05. The device wraps each such value to 76 bits, and a value of
    that size passes the wrap unchanged."""
    y_top = Fraction(1, 3)
    tail = Fraction(1 << SCALE_BITS) * y_top ** (2 * ARTANH_TERMS) / ((2 * ARTANH_TERMS + 1) * (1 - y_top ** 2))
    e_y2 = 2 * y_top + 1 + Fraction(1, 1 << 60)
    held = Fraction(105, 100)
    e_acc = Fraction(2)
    for _ in range(ARTANH_TERMS - 1):
        e_acc = e_acc * (y_top ** 2 + Fraction(1, 1 << 60)) + held * e_y2 + 3
    e_half = e_acc * y_top + held + 1
    e_ln = 2 * e_half + 2 * (tail + 1) + FOLDS + 1
    cosh_pi = Fraction(1160, 100)
    e_cos = Fraction(2)
    for _ in range(COS_TERMS - 1):
        e_cos = e_cos + cosh_pi + 3
    return e_ln, e_cos + 1


def arithmetic(nu, constants):
    """The device's error on Z and on theta / pi over cell nu, in units of 2^-62, from its steps.

    Every value that the device takes a product back from is below 12 times 2^62 in size. Besides those of unit_errors,
    these are the partial sums of each curve below its size, with the sizes below 1.2 in sum, and Newton's
    `y (3 - w y^2)` below 3.

    The errors of the steps:
      - ln k and ln s each carry e_ln.
      - `theta / pi = S ln s / 2^p - s - 1/8 + 2^p / (96 pi^2 S)` carries `s e_ln` and three floors. Its terms in
        `1 / s^3` and `1 / s^5`, with `1 / s` within a unit and below 1, carry eight more.
      - the `2 s ln k` of the pair carries `2 s e_ln` and a floor.
      - `x = S s^(-1/2) / 2^p` carries s times the error of the root and a floor, and z twice that. `x^(-1/2)`, from the
        x that the device holds, carries half of it more.
      - each C_n by Horner's rule carries three units a coefficient and one more, and the error of z times its slope.
      - `1 / x`, the square of the root, carries twice the error of the root and a floor.
      - the sum over n of `C_n x^(-n)`, by Horner's rule in `1 / x` from C_K, carries the sum before it times `1 / x` at
        each step, which is below `1 / nu`. It also carries the size of that sum times the error of `1 / x`, a floor,
        and the curve's own error.
      - R carries that sum times `x^(-1/2)`, its size times the error of the root, the error of the sum and a floor."""
    s_top = (nu + 1) ** 2
    unit = 1 << SCALE_BITS
    e_ln, e_cos = unit_errors()
    e_theta = s_top * e_ln + 12
    e_q = e_theta + 2 * s_top * e_ln + 1
    e_term = e_cos + PI_HIGH * e_q + Fraction(101, 100) * newton_error(nu) + 1
    e_x = s_top * newton_error(s_top) + 1
    e_root = newton_error(nu + 1) + e_x / 2 + 1
    e_over = 2 * e_root + 1
    e_sum, size = Fraction(0), Fraction(0)
    for n in reversed(range(CURVES)):
        e_curve = 3 * GAMMA_TERMS + 1 + 2 * e_x * constants.slopes[n]
        e_sum = e_sum * (Fraction(1, nu) + e_over / unit) + size * e_over + 1 + e_curve
        size = size / nu + constants.sizes[n]
    e_held = e_sum + size * e_root + 1
    return 2 * nu * e_term + e_held, e_theta, e_held


def sine_low(x):
    """A lower bound on sin x for 0 <= x <= pi / 2, x a Fraction: x - x^3 / 6."""
    return x - x ** 3 / 6


def transform_error(nu, p, e_theta, e_held, weighted=False):
    """The multiple evaluation's error on Z over cell nu at `P = 2^p` points, in units of 2^-62.

    Half of its main sum is `Re(exp(i theta) F)`, with `F_j = sum over h of (u_h / P) exp(-2 pi i j h / P)`. `u_h / P`
    is the sum over k of `q_k / (z_h - w_k)`. The charge of each pole, `q_k = -a_k (1 - exp(-2 pi i pos_k)) w_k / P`, is
    at max `2 k^(-1/2) / P` in size. The poles as the device holds them, with pos_k and a_k each within its error, are
    poles of the same type. F over them is F over the true poles within
    `sum over k of e_a + 2 pi k^(-1/2) (2 nu + 1) e_ln`. What follows is the error on F over the poles kept.

    Truncation: take a pole of a source box S at level l, carried to a target box T of its interaction list, with
    `|T - S| >= 3` boxes. It is `1 / (z - w) = (1 / (sigma_S D)) sum of binom(m + n, n) x^n y^m`, with
    `|x|, |y| <= rho / |D|`, `rho = pi / 2^l`, and `|D| = 2 sin(|Delta| pi / 2^l) >= 2 sin(3 pi / 2^l)`. The terms left
    out, those with `m >= N` or `n >= N`, sum to at max `c^N / ((1 - c) |D|)`, with `c = 2 rho / |D|`, at each of the
    `P / 2^l` points of T. Each source box is in at max six lists. The charges sum to at max `4 nu^(1/2) / P`. Over all
    the points, the sum over l is `24 nu^(1/2) c_l^N / ((1 - c_l) 2^l |D_l|)`.

    Arithmetic: the error of each expansion is in the norm `sum over m of w^m |e_m|`, with `w = 1/2`. The leaf terms of
    a pole, `(q / sigma) eta^m`, carry `e_q + 2 (m + 1)`. The shift to a parent multiplies the norm by at max 4/3, and
    adds `N + 6` a child for its roundings. There the weight of `M_c,j` in `M_p,m` is `binom(m, j) |s|^(m - j) |r|^j`,
    with `|s|, |r| <= 1/2`.

    The shift across is `L_n = (1 / D) sum of binom(m + n, n) c^n d^m M_m`, with `|c|, |d| = rho / |D| <= 0.22`. It
    takes the norm to a plain sum over n of at max `1.27 / |D|` times it, and adds `(4 N + 2) / |D| + 2 N`. The shift
    down keeps the plain sum, and adds 8 N. Its weights `binom(n, j) |s|^(n - j) |r|^j` in `L_p,n` sum over j to
    `(|s| + |r|)^n <= 1`. Summed over the points as the truncation is, the errors of a level reach the `P / 2^l` points
    of each of at max six targets.

    A point adds the Horner's rule of its leaf, 2 N, and its near field: five leaves of at max `W / 2 + 1` poles, each
    `e_cos + 40`. Each of the p stages of the transform adds `2 + A e_w` at each of the `2^(p - s)` values that feed an
    output. A is the sum over h of `|u_h / P|`, at max `2 nu^(1/2) (ln P + 3)` by the Lebesgue constant of the Dirichlet
    kernel. `e_w` is the error of the twiddle. The values cos theta and sin theta carry `e_cos + pi e_theta` against
    `|F| <= 2 nu^(1/2)`, and `Z = 2 Re(exp(i theta) F)` + the remainder term.

    `weighted`, the poles times `-i ln k / 2^shift`, gives `F' / 2^shift`. There `g = ln k / 2^shift` is at max 1 and
    within `e_ln + 1`. Each a and charge, at max 1, gains `e_ln + 2` from the product. Every charge is at max that of F,
    and that leaves the truncation and the spread of the transform as they are."""
    e_ln, e_cos = unit_errors()
    order, top, width = ORDER, p - BETA, 1 << BETA
    points = 1 << p
    root_nu = root_ceiling(Fraction(nu), 2)
    e_a = Fraction(101, 100) * newton_error(nu) + e_cos + PI_HIGH * 2 * nu * nu * e_ln + 2
    e_q = (e_cos * 4 + 2 * PI_HIGH + 6) / points + 1
    if weighted:
        e_a += e_ln + 4
        e_q += e_ln + 4
    held = nu * e_a + 2 * root_nu * 2 * PI_HIGH * (2 * nu + 1) * e_ln
    unit = Fraction(1 << SCALE_BITS)
    truncation = Fraction(0)
    arithmetic_sum = Fraction(0)
    leaf_norm = 2 * (e_q + 2) + 4 * sum(Fraction(m, 2 ** m) for m in range(order))
    # the poles lie in pos within [0, (2 nu + 1) ln nu], and at level l in at max this many boxes
    reach = (2 * nu + 1) * ln_high(nu)

    def holding(level):
        return min(2 ** level, math.ceil(reach * 2 ** level / points) + 1)

    for level in range(3, top + 1):
        rho = PI_HIGH / 2 ** level
        d_low = 2 * sine_low(3 * PI_LOW / 2 ** level)
        c = 2 * rho / d_low
        truncation += 24 * root_nu * c ** order / ((1 - c) * 2 ** level * d_low) * unit
        # every pole's leaf norm carried up top - level shifts, and each shift's roundings carried the rest of the way
        multipoles = nu * leaf_norm * Fraction(4, 3) ** (top - level) + sum(
            holding(finer) * (order + 6) * Fraction(4, 3) ** (finer - 1 - level) for finer in range(level + 1, top + 1))
        across = Fraction(127, 100) / d_low * multipoles + holding(level) * ((4 * order + 2) / d_low + 2 * order)
        arithmetic_sum += Fraction(points, 2 ** level) * 6 * across + points * 8 * order
    near = 5 * (width // 2 + 1) * (e_cos + 40)
    arithmetic_sum += points * (2 * order + near)
    spread = 2 * root_nu * (p * LN_TWO_HIGH + 3)
    e_w = e_cos + PI_HIGH
    stages = 2 * points * (2 + spread * e_w)
    e_f = held + truncation + arithmetic_sum + stages
    return 2 * (e_f + 2 * root_nu * (e_cos + PI_HIGH * e_theta) + 2) + e_held


def em_ratios(terms):
    """r_k = (B_2k / (2k)!) / (B_(2k-2) / (2k - 2)!) for k from 2 to `terms`, at 2^62, each toward zero."""
    out = []
    for k in range(2, terms + 1):
        num_k, den_k = zz.bernoulli_over_factorial(k)
        num_j, den_j = zz.bernoulli_over_factorial(k - 1)
        num, den = num_k * den_j, den_k * num_j
        out.append(rs.compare(num * den, 0) * ((abs(num) << SCALE_BITS) // abs(den)))
    return out


def em_terms(level):
    """M, the Bernoulli terms Euler-Maclaurin takes at widening `level`."""
    return 20 + 10 * level


def em_heads(nu, terms):
    """N for cell nu at M = `terms`: the least with |s + j| / (2 pi N) <= 1/2 for every j < 2M over the cell."""
    t_high = 2 * PI_HIGH * (nu + 1) ** 2
    return max(2, math.ceil((t_high + 2 * terms) / PI_LOW))


def em_bounds(nu, heads, terms):
    """The bound on Z and on theta / pi over cell nu by Euler-Maclaurin at `N = heads` and `M = terms`, in whole units
    of 2^-62.

    `zeta(s) = sum over n < N of n^(-s) + N^(-s) C`. Here
    `C = N / (s - 1) + 1/2 + sum over k <= M of B_2k / (2k)! (s)_(2k-1) N^(1 - 2k)`.
    `|R| <= 4 |(s)_2M| / ((2 pi)^(2M) (sigma + 2M - 1) N^(sigma + 2M - 1))` for `N >= 2` and `sigma + 2M > 1`:
    Johansson, arXiv:1309.2877, Theorem 1, at `a = 1` with no derivative. It holds at every t, and `|(s)_2M|` rises with
    t. The top of the cell bounds it.

    The error delta of theta, from theta_error, is one angle at a point, common to every term. The sum is
    `Re(exp(i (theta + delta)) (zeta - R))`, and `exp(i theta) zeta` is Z and real. The sum is therefore `Z cos(delta)`
    less at max |R|. Delta moves it by `|Z| (1 - cos(delta)) <= |Z| delta^2 / 2`, with |Z| at max the sum of `n^(-1/2)`,
    |C| and |R|. The bound of the remainder charges R.

    The code carries the device's own error step by step. Each term of the pair stage goes as in `arithmetic`. The
    Euler-Maclaurin stage takes `t = 2 pi S / 2^p`, then `1 / (s - 1)`, then each tau_k from tau_(k-1) by r_k, `1 / N`,
    `s + 2k - 3`, `1 / N` and `s + 2k - 2`. Each error grows by the factor of the step, and a unit a division."""
    unit = Fraction(1 << SCALE_BITS)
    s_top = (nu + 1) ** 2
    t_low = 2 * PI_LOW * nu * nu
    t_high = 2 * PI_HIGH * s_top
    root_heads = root_ceiling(Fraction(heads), 2)
    sigma = Fraction(1, 2)
    # Johansson's remainder, its square first, at the cell's top
    square = Fraction(16)
    for j in range(2 * terms):
        square *= (j + sigma) ** 2 + t_high ** 2
    square /= (2 * PI_LOW) ** (4 * terms) * (2 * terms - sigma) ** 2 * Fraction(heads) ** (4 * terms - 1)
    remainder = Fraction(root_ceiling(square * unit * unit, 2) + 1)
    # theta at the cell's foot
    e_theta = theta_error(t_low)
    # the size of C, and the device's error on it
    e_ln, e_cos = unit_errors()
    e_point = s_top * e_ln + 12
    e_q = e_point + 2 * s_top * e_ln + 1
    e_cs = e_cos + PI_HIGH * e_q
    e_t = 2 * s_top + 2
    e_inverse = 2 * (e_t / t_low ** 2 + 1)
    size = Fraction(1, 2) + t_high
    held = Fraction(heads) / t_low + Fraction(1, 2)
    a = size / (12 * heads)
    e_tau = 2 + e_t / (12 * heads)
    held += a
    e_c = heads * e_inverse + e_tau
    ratios = em_ratios(terms)
    for k in range(2, terms + 1):
        r = Fraction(abs(ratios[k - 2]) + 1, 1 << SCALE_BITS)
        w = 2 * k - 3 + size
        e1 = e_tau * r + a + 2
        a1 = a * r
        e2 = e1 / heads + 2
        a2 = a1 / heads
        e3 = e2 * w + a2 * e_t + 4
        a3 = a2 * w
        e4 = e3 / heads + 2
        a4 = a3 / heads
        e_tau = e4 * (w + 1) + a4 * e_t + 4
        a = a4 * (w + 1)
        held += a
        e_c += e_tau
    e_rotated = 2 * e_cs * held + e_c + 2
    e_em = Fraction(101, 100) * e_rotated + held * newton_error(heads) + 1
    e_term = e_cos + PI_HIGH * e_q + Fraction(101, 100) * newton_error(heads) + 1
    e_arith = (heads - 1) * e_term + e_em + 2
    phase = e_theta ** 2 / 2 * ((2 * root_heads + held) * unit + remainder)
    return math.ceil(e_arith + remainder + phase), math.ceil(e_point + e_theta * unit / PI_LOW)


def theta_error(t_low):
    """The max error of theta from the five terms of the device, through `31 / (80640 t^5)`, at `t >= t_low`.

    From `t = 10` it is the `1 / (3322 t^7)` of Gabcke (the thesis, introduction, (4) and (5)). Below that, it is
    Brent's bound on theta past `1 / (48 t)`, with the two terms after it added."""
    if t_low >= 10:
        return 1 / (3322 * t_low ** 7)
    x = PI_LOW * t_low
    # exp(-x) below 1 over exp's first 40 Taylor terms
    brent = (Fraction(7, 5760) + PI_HIGH / 960) / t_low ** 3 + 1 / (2 * sum(x ** k / math.factorial(k) for k in range(40)))
    return brent + Fraction(7, 5760) / t_low ** 3 + Fraction(31, 80640) / t_low ** 5


def analytic(nu):
    """Gabcke's bound on R_K and theta's on Z, and theta's on theta / pi, over cell nu, in units of 2^-62."""
    t_low = 2 * PI_LOW * nu * nu
    unit = Fraction(1 << SCALE_BITS)
    k = CURVES - 1
    gabcke = root_ceiling((GABCKE[k] * unit) ** 4 / t_low ** (2 * k + 3), 4)
    e_theta = theta_error(t_low)
    phase = 4 * root_ceiling(Fraction(nu), 2) * e_theta * unit
    return gabcke + phase, e_theta * unit / PI_LOW


def bounds(nu, constants, p, method):
    """The bound on Z and on theta / pi over cell nu at 2^p points, in whole units of 2^-62.

    It is the bound of the arithmetic of the pairs, or that of the multiple evaluation where that gives the verdict."""
    e_arith, e_q, e_held = arithmetic(nu, constants)
    if method != "pairs":
        e_arith = transform_error(nu, p, e_q, e_held)
    e_z, e_theta = analytic(nu)
    return math.ceil(e_arith + e_z), math.ceil(e_q + e_theta)


class Cell:
    """What the device returns for cell nu at P = 2^p points, its values of S = x^2 2^p lifted to 4^-FINEST."""

    def __init__(self, nu, p, lines):
        self.nu, self.p = nu, p
        self.lift = 1 << (2 * FINEST - p)
        rows = {line.split()[0]: line.split()[1:] for line in lines}
        _, _, points, first, last = (int(v) for v in rows["cell"])
        self.points, self.first, self.last = points, first, last
        self.at = {key: [int(v, 16) for v in rows[key]] for key in ("first", "last", "origin")}
        self.control = [int(v, 16) for v in rows["control"]]
        self.sums = {key: int(rows[key][0], 16) for key in SUMS}
        self.failed = sum(1 for v in rows["host"] if v != "1")
        self.steps = [int(v) for v in rows["steps"]]
        self.apart = (int(rows["apart"][0], 16), int(rows["apart"][1])) if "apart" in rows else None
        self.margins = {key: int(rows[key][0], 16) for key in ("clean", "flagged", "single") if key in rows}
        self.programs = tuple(int(v) for v in rows["programs"])

    def sign(self, which):
        return self.at[which][0]

    def x2(self, which):
        return self.at[which][1] * self.lift

    def end_x2(self):
        """x^2 at the cell's last point."""
        return ((self.nu ** 2 << self.p) + ((1 << self.p) - 1) * (2 * self.nu + 1)) * self.lift

    def crossing(self):
        """The step of x^2 from the cell's last point to the next cell's point 0, at 4^-FINEST."""
        return (2 * self.nu + 1) * self.lift


def rises(nu):
    """How far theta / pi rises across cell nu, near enough to choose its lattice: 2 x^2 ln x - x^2 at its ends."""
    def theta(x):
        return 2 * x * x * math.log(x) - x * x
    return theta(nu + 1) - theta(nu)


def lattice(nu, rate):
    """The p of cell nu: the least with 2^p at least `rate` times theta / pi's rise across it."""
    return max(COARSEST, math.ceil(math.log2(rate * rises(nu))))


class Control:
    """Each cell's lattice from one local cost, its every term a field measured on the run.

    A cell at 2^p points holds `r = 2^p / Z` points a zero, with Z the rise of `theta / pi` across it. Two zeros closer
    than a step hide between the same two points. With the spacing of the GUE, small gaps g at density `(pi^2 / 3) g^2`,
    the missed zeros number `mu = c Z / r^3`, with `c = pi^2 / 18`.

    A cell that misses one runs again. The cost to close it from p is `C(p) = T(p) + (1 - exp(-mu(p))) C(q)`, with q the
    next lattice up. T(p) is the time that a run at 2^p points takes. The points cost T, and the misses cost the run
    that they force. The choice of p is the least C, moved at max one a cell.

    The code takes c as the ratio of the missed zeros to the sum of `Z / r^3` over the measured cells. It starts from
    the value of the GUE, with the weight of PRIOR cells' worth of misses. The misses of a cell are the shortfall that N
    holds it to. `T(p) = f + a 2^p`, with f and a fit by least squares to the timed runs. The value f comes from FIXED,
    and a from the first run, until there are times for two lattices. None of it touches the proof: the count certifies
    whatever lattice a cell runs on."""

    PRIOR = 4.0
    FIXED = 2.0

    def __init__(self):
        self.missed = Fraction(0)
        self.exposure = Fraction(0)
        self.timed = []
        self.last_p = None

    def c(self):
        prior = Fraction(PI_LOW ** 2 / 18)
        return (prior * self.PRIOR + self.missed) / (self.PRIOR + self.exposure)

    def measure(self, nu, p, missed):
        z = rises(nu)
        self.missed += missed
        self.exposure += Fraction(z ** 4) / Fraction(8 ** p)

    def timing(self, p, seconds):
        self.timed.append((1 << p, seconds))

    def cost(self, p):
        sizes = {n for n, _ in self.timed}
        if len(sizes) >= 2:
            n_bar = sum(n for n, _ in self.timed) / len(self.timed)
            s_bar = sum(s for _, s in self.timed) / len(self.timed)
            a = (sum((n - n_bar) * (s - s_bar) for n, s in self.timed) /
                 sum((n - n_bar) ** 2 for n, _ in self.timed))
            a = max(a, 0.0)
            f = max(s_bar - a * n_bar, 0.0)
        elif self.timed:
            n, s = self.timed[-1]
            f = min(self.FIXED, s)
            a = (s - f) / n
        else:
            f, a = self.FIXED, 1e-6
        return f + a * (1 << p)

    def close_cost(self, nu, p):
        """C(p): the time to close cell nu from 2^p points, with the reruns that it forces on average included."""
        z = rises(nu)
        c = float(self.c())
        total = self.cost(FINEST)
        for q in range(FINEST - 1, p - 1, -1):
            mu = c * z ** 4 / 8.0 ** q
            total = self.cost(q) + (1.0 - math.exp(-mu)) * total
        return total

    def choose(self, nu, above=None):
        """The p of least C for cell nu, past `above` where given, within one of the last cell's."""
        low = COARSEST if above is None else above + 1
        options = [p for p in range(max(low, math.ceil(math.log2(rises(nu)))), FINEST + 1)]
        p = min(options, key=lambda q: self.close_cost(nu, q))
        if above is None and self.last_p is not None:
            p = min(max(p, self.last_p - 1), self.last_p + 1)
            self.last_p = p
        elif above is None:
            self.last_p = p
        return p


SERVED = {}


def serve(binary):
    """The binary's one process, started with `serve` where it is not running."""
    served = SERVED.get(binary)
    if served is None or served.poll() is not None:
        # the longest programs of the multiple evaluation hold frames past the stack limit of the device, and stay as
        # PTX. As new C source they would go to NVRTC, which compiles a program of their length far slower than the run
        served = subprocess.Popen([binary, "serve"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
                                  env=dict(os.environ, CYCLE_RECORD_RETAIN_PTX="1"))
        SERVED[binary] = served
    return served


def stop(binary):
    """The binary's process ended, its programs released."""
    served = SERVED.pop(binary, None)
    if served is not None and served.poll() is None:
        served.stdin.close()
        served.wait()


atexit.register(lambda: [stop(binary) for binary in list(SERVED)])


def device(binary, given, taken, what):
    """The device's run from the input at `given` to the output at `taken`. A run whose checks fail writes nothing that
    the machine reads. It runs once more in a new process, and the code prints both failures."""
    for attempt in range(2):
        served = serve(binary)
        printed, code = [], None
        try:
            served.stdin.write(given + "\n" + taken + "\n")
            served.stdin.flush()
            for line in served.stdout:
                if line.startswith("done "):
                    code = int(line.split()[1])
                    break
                printed.append(line)
        except OSError:
            code = None
        if code == 0:
            return
        print("".join(printed).strip()[-2000:])
        print("  %s: the device run failed, attempt %d" % (what, attempt + 1))
        stop(binary)
    raise SystemExit("  %s: the device run failed" % what)


def run_cell(binary, constants, nu, p, method, level=0, listing=0):
    """Cell nu at 2^p points from the device, with its main sum by `method`, or by Euler-Maclaurin at search depth
    `level`.

    With `listing`, the device writes every point's sign, S, Z and w beside the sums, at the path that the cell keeps.
    With `listing` 2 and the multiple evaluation, it also writes each point's `exp(i theta) F' / 2^shift` beside them,
    and the shift."""
    terms = em_terms(level) if method == "em" else 0
    heads = em_heads(nu, terms) if method == "em" else 0
    per_point = heads - 1 if method == "em" else nu
    piece = 1 << p
    while piece * per_point >= 1 << LANE_BITS:
        piece >>= 1
    if method == "em":
        bound, theta_bound = em_bounds(nu, heads, terms)
    else:
        bound, theta_bound = bounds(nu, constants, p, method)
    folder = tempfile.mkdtemp(prefix="turing_")
    given, taken = os.path.join(folder, "in.bin"), os.path.join(folder, "out.txt")
    with open(given, "wb") as handle:
        array.array("q", (1 << p, min(CHECKED, piece), nu, p, ARTANH_TERMS, COS_TERMS, GAMMA_TERMS, NEWTON_STEPS,
                          piece, METHODS[method], ORDER, BETA, E1_TERMS, SINC_TERMS, heads, terms, listing,
                          CURVES)).tofile(handle)
        for v in ([constants.ln2] + constants.gamma + constants.artanh + constants.cosine +
                  [constants.c96, constants.c3, constants.c5, bound, theta_bound, constants.pi_scaled] +
                  constants.fact +
                  constants.sinc + (em_ratios(terms) if method == "em" else [])):
            put(handle, v)
        if listing == 2:
            for v in margin(nu, p, constants, bound)[:3]:
                put(handle, v)
    device(binary, given, taken, "cell %d at 2^%d points" % (nu, p))
    with open(taken) as handle:
        cell = Cell(nu, p, [line for line in handle if not line.startswith("point")])
    cell.path = taken
    return cell, bound


def run_points(binary, constants, nu, p, js, bracketed=False):
    """The certified signs of Z at the points j of cell nu's lattice of 2^p, by pairs at those points alone.

    For each point, in the order listed, it gives the sign, S, and the flag of the step from the point listed before it.
    With `bracketed`, it gives the sign, S, and `theta / pi` less and more its bound for each point. The code pads the
    list with its last point to a whole number of pieces, and a repeated point changes no sign."""
    piece = 1
    while piece * 2 <= len(js) and piece * 2 * nu < 1 << LANE_BITS:
        piece *= 2
    listed = list(js) + [js[-1]] * ((-len(js)) % piece)
    bound, theta_bound = bounds(nu, constants, p, "pairs")
    folder = tempfile.mkdtemp(prefix="turing_")
    given, taken = os.path.join(folder, "in.bin"), os.path.join(folder, "out.txt")
    with open(given, "wb") as handle:
        array.array("q", (len(listed), min(CHECKED, piece), nu, p, ARTANH_TERMS, COS_TERMS, GAMMA_TERMS, NEWTON_STEPS,
                          piece, METHODS["points"], ORDER, BETA, E1_TERMS, SINC_TERMS, 0, 0, 3 if bracketed else 2,
                          CURVES)).tofile(handle)
        least, third, steep, _ = margin(nu, p, constants, bound, "pairs")
        for v in ([constants.ln2] + constants.gamma + constants.artanh + constants.cosine +
                  [constants.c96, constants.c3, constants.c5, bound, theta_bound, constants.pi_scaled] +
                  constants.fact + constants.sinc +
                  ([] if bracketed else [least, third, steep])):
            put(handle, v)
        array.array("q", listed).tofile(handle)
    device(binary, given, taken, "cell %d, %d listed points at 2^%d" % (nu, len(js), p))
    signs, failed = [], 0
    with open(taken) as handle:
        for line in handle:
            if line.startswith("point"):
                values = [int(v, 16) for v in line.split()[1:]]
                signs.append((values[0], values[1], values[-2], values[-1]) if bracketed else (values[0], values[1], values[-1]))
            elif line.startswith("host"):
                failed = sum(1 for v in line.split()[1:] if v != "1")
    os.remove(taken)
    return signs[:len(js)], failed


REFINE_BITS = 3


def ln_upper(q):
    """An upper bound on ln q for a rational `q >= 1`: `m ln 2 + 2 artanh(y)`, with `q / 2^m` in [1, 2).

    Here `y = (q - 1) / (q + 1)` is at max 1/3. The code cuts the series after 12 terms, and bounds its tail by a
    geometric sum."""
    q, m = Fraction(q), 0
    while q >= 2:
        q, m = q / 2, m + 1
    y = (q - 1) / (q + 1)
    terms = 12
    total = sum(y ** (2 * k + 1) / (2 * k + 1) for k in range(terms))
    total += y ** (2 * terms + 1) / ((2 * terms + 1) * (1 - y * y))
    return m * LN_TWO_HIGH + 2 * total


def root_upper_inverse(n):
    """An upper bound on n^(-1/2): 2^20 over the floor of (4^20 n)^(1/2)."""
    return Fraction(1 << 20, math.isqrt(n << 40))


FOURTH_BITS = 100
FOURTH = {}


def fourth_bound(nu):
    """max |M^(4)| over cell nu, with M^(4) the fourth derivative of M, bounded above by
    `2 sum over n <= nu of n^(-1/2) (A^4 + 6 A^2 theta2 + 4 A theta3 + 3 theta2^2 + theta4)`, with
    `A = ln((nu + 1) / n)`.

    The code rounds each term up to a whole number of 2^-FOURTH_BITS. It depends on nu alone, and the code keeps it for
    each nu.
    """
    if nu not in FOURTH:
        t_low = 2 * PI_LOW * nu * nu
        theta2 = 1 / (2 * t_low) + 1 / (24 * t_low ** 3)
        theta3 = 1 / (2 * t_low ** 2) + 1 / (8 * t_low ** 4)
        theta4 = 1 / t_low ** 3 + 1 / (2 * t_low ** 5)
        grid = 1 << FOURTH_BITS
        units = 0
        for n in range(1, nu + 1):
            a = ln_upper(Fraction(nu + 1, n))
            term = root_upper_inverse(n) * (a ** 4 + 6 * a * a * theta2 + 4 * a * theta3 + 3 * theta2 ** 2 + theta4)
            units += -((-term.numerator * grid) // term.denominator)
        FOURTH[nu] = Fraction(2 * units, grid)
    return FOURTH[nu]


def margin(nu, p, constants, bound, method="transform"):
    """The margin that a step's hull must clear over cell nu at 2^p points, and `h / 3` at 2^62.

    The code holds Z against `Zh = M + R`, with `M = 2 sum over n <= nu of n^(-1/2) cos(theta~ - t ln n)`. Here theta~
    is the series that the point stage takes, and R is the remainder term that the device evaluates. `|Z - Zh|` at every
    t of the cell is within the bound on Z, as at its points. On a step `[t0, t1]`, with `h = 2 pi (2 nu + 1) / 2^p`,
    let C be the cubic through the Z of the device and `Z' = M'` at its ends. Then `Zh >= C - e`, with e the sum of:
      - the bound on Z, for the values of Z at the ends, and Zh against Z.
      - `(h / 4) e_D`, the error e_D of Z' at the ends, weighted by `s (1 - s) <= 1/4`, the slope terms of the cubic.
      - `h^4 / 384 max |M^(4)|`, the remainder of the cubic through the values and slopes of M. Here
        `|M^(4)| <= 2 sum of n^(-1/2) (A^4 + 6 A^2 theta2 + 4 A theta3 + 3 theta2^2 + theta4)`, with
        `A = ln((nu + 1) / n)` at max `|theta~' - ln n|`. The sizes theta2, theta3 and theta4 are those of the next
        derivatives of theta~ at the foot of the cell.
      - `h max |R'|`, with the values of R in C through weights that sum to 1.
      - the error of the two inner points of the hull from `h / 3` kept at 2^62, at max |Z'| a unit each, and their
        roundings.

    `Z' = 2 (Re(exp(i theta) F') - theta' Im w)`, with `theta' = ln s / 2` against `theta~' = ln s / 2 - 1 / (48 t^2)`.
    The error e_D is the sum of four parts:
      - twice the error of theta' times `|w| <= 2 nu^(1/2)`.
      - theta' times the error of w.
      - 2^shift times the error of `exp(i theta) F' / 2^shift`.
      - the roundings.

    The errors on w and on `F' / 2^shift` are those of the multiple evaluation over the poles and the weighted poles. By
    pairs, `Z' = 2 (sum of k^(-1/2) ln k sin(phi) - theta' sum of k^(-1/2) sin(phi))`. The same holds, with the error of
    the first sum `nu e_term`, and each sine term within the error of a cosine term. The error of the second sum is
    `nu (e_term ln(nu + 1) + e_ln + 2)`, with each term at max 1 and ln k within e_ln.

    The steepness C' must hold over a step whose ends change sign, the way Z crosses, for Zh to cross once. Let
    `e = M - C_M`, with C_M the cubic through the values and slopes of M. Then e' is 0 at both ends, and once between by
    Rolle's theorem on e. The function e' thus has three zeros in the step, and its third derivative is M^(4). Hence
    `|e'| <= max |M^(4)| / 6` times the max that `|(t - t0)(t - c)(t - t1)|` reaches, `4 h^3 / 27`. That gives
    `(2 / 81) h^3 max |M^(4)|`.

    The C' of the device comes from `C_M' + R'` within e_D, with the weights of the slopes at max 1 in sum. It also
    takes `3 e_Z / h` from the values, with e_Z the bound on Z and their weight `6 s (1 - s) / h`. R enters by its
    values at the ends and its slope, within `5/2 max |R'|`. The steepness is their sum with the roundings of the hull.
    Where C' clears it, Zh' holds one sign over the step, and Zh is monotone across it.

    Returns the margin, `h / 3`, the steepness and e_D, each in units of 2^-62."""
    unit = 1 << SCALE_BITS
    t_low = 2 * PI_LOW * nu * nu
    h = 2 * PI_HIGH * (2 * nu + 1) / Fraction(1 << p)
    fourth = fourth_bound(nu)
    root_low = math.isqrt(nu)
    # |dR/dx| <= sum over n of (n + 1/2) |C_n| x^(-n - 3/2) + 2 |C_n'| x^(-n - 1/2), and dx/dt = 1 / (4 pi x)
    remainder_slope = sum(((n + Fraction(1, 2)) * constants.sizes[n] / nu + 2 * constants.slopes[n]) / (nu ** n * root_low)
                          for n in range(CURVES)) / (4 * PI_LOW * nu)
    root = root_ceiling(Fraction(nu), 2)
    clock_top = ln_upper(nu + 1)
    e_ln, _ = unit_errors()
    e_pairs, e_q, e_held = arithmetic(nu, constants)
    if method == "transform":
        e_w = (transform_error(nu, p, e_q, e_held) - e_held) / 2
        shift = nu.bit_length().bit_length()
        e_turned = (1 << shift) * (transform_error(nu, p, e_q, e_held, weighted=True) - e_held) / 2
    else:
        e_term = (e_pairs - e_held) / (2 * nu)
        e_w = nu * e_term
        e_turned = nu * (e_term * clock_top + e_ln + 2)
    e_clock = e_ln / 2 + 1 + unit / (48 * t_low ** 2)
    e_d = 2 * (e_clock * 2 * root + clock_top * e_w + e_turned + 2)
    d_max = 8 * root * clock_top + 1
    total = bound + h / 4 * e_d + h ** 4 / 384 * fourth * unit + h * remainder_slope * unit + 2 * d_max + 4
    third = (2 * constants.pi * (2 * nu + 1)) // (3 << (p + GUARD_BITS))
    h_low = 2 * PI_LOW * (2 * nu + 1) / Fraction(1 << p)
    steep = (e_d + 3 * bound / h_low + Fraction(2, 81) * h ** 3 * fourth * unit + Fraction(5, 2) * remainder_slope * unit +
             3 * (2 * d_max + 4) / h_low + 2)
    return math.ceil(total), third, math.ceil(steep), math.ceil(e_d)


def flagged_spans(cell, check=False):
    """The spans of a cell listed with Z' that the margin stage could not close.

    Each is a run between two consecutive certified points, past F, that holds a step that the device flags. Such a step
    is neither clean nor a single crossing. A run whose ends change sign counts only with `check`, which proves each
    crossing single. Each span carries the signs and S of its ends, and the zeros that the count of the device already
    holds in it. That is one zero where the signs change and the points are at max two apart, the reach of the count
    stage, else none. The flags choose where to look again, and the count certifies whatever they choose."""
    rows = []
    with open(cell.path) as handle:
        for line in handle:
            if line.startswith("point"):
                values = [int(v, 16) for v in line.split()[1:]]
                rows.append((values[0], values[1], values[-1]))
    held = [j for j in range(cell.first, len(rows)) if rows[j][0] != 0]
    out = []
    for a, b in zip(held, held[1:]):
        if (check or rows[a][0] == rows[b][0]) and any(rows[q][2] for q in range(a + 1, b + 1)):
            counted = 1 if rows[a][0] != rows[b][0] and b - a <= 2 else 0
            out.append((a, b, rows[a][0], rows[b][0], rows[a][1], rows[b][1], [counted]))
    return out


REFINE_DEPTH = 3


def refine(binary, constants, cell, spans, check=False):
    """The zeros that the flagged spans hide, certified at a lattice 2^REFINE_BITS times finer by the listed points.

    The ends of each span are among those points, and the zeros go into the sums of the cell past F. The margin stage
    lists a fine step that it flags again, finer by as much, to REFINE_DEPTH times. A fine step closes where it is
    clean. It also closes where its ends change sign, and with `check` where Z also crosses it once. A span closes where
    all its steps do.

    Each zero lies between two certified fine points. The sums take the S of the earlier point rounded down to the
    lattice of the cell, and that of the later one rounded up. A later point than the zero loosens the upper bound of N,
    and an earlier one the lower bound, and both are still bounds. A zero that the count of the device already holds in
    a span counts for the first zero found in it. The coarse ends of the span certify it. A span still open after the
    last lattice gives the zero that the signs of its ends certify, where they change.

    Returns the points listed, the zeros added, the spans still open and the host checks that failed."""
    listed, added, failed = 0, 0, 0

    def take(span_counted, low, high, lift):
        nonlocal added
        if span_counted[0]:
            span_counted[0] -= 1
            return
        cell.sums["zeros_after"] += 1
        cell.sums["zeros_p_after"] += low >> lift
        cell.sums["zeros_q_after"] += -(-high >> lift)
        added += 1

    p = cell.p
    for _ in range(REFINE_DEPTH):
        if not spans:
            break
        p += REFINE_BITS
        lift = p - cell.p
        js, ranges = [], []
        for span in spans:
            low = len(js)
            js.extend(range(span[0] << REFINE_BITS, (span[1] << REFINE_BITS) + 1))
            ranges.append((low, len(js)))
        rows, missed = run_points(binary, constants, cell.nu, p, js)
        listed, failed = listed + len(js), failed + missed
        opened = []
        for (a, b, sign_a, sign_b, s_a, s_b, counted), (low, high) in zip(spans, ranges):
            # the coarser run already certified the ends of the span
            run = ([(sign_a, s_a << REFINE_BITS, 0)] + rows[low + 1:high - 1] +
                   [(sign_b, s_b << REFINE_BITS, rows[high - 1][2])])
            held = [q for q in range(len(run)) if run[q][0] != 0]
            for u, v in zip(held, held[1:]):
                crossed = run[u][0] != run[v][0]
                if (check or not crossed) and any(run[q][2] for q in range(u + 1, v + 1)):
                    opened.append((js[low + u], js[low + v], run[u][0], run[v][0], run[u][1], run[v][1], counted))
                elif crossed:
                    take(counted, run[u][1], run[v][1], lift)
        spans = opened
    for a, b, sign_a, sign_b, s_a, s_b, counted in spans:
        if sign_a != sign_b:
            take(counted, s_a, s_b, p - cell.p)
    return listed, added, len(spans), failed


def ln_high(t):
    """An upper bound on ln t, from the bits of t's ceiling."""
    return math.ceil(t).bit_length() * LN_TWO_HIGH


def trudgian(x2):
    """Trudgian's bound at t = 2 pi x^2, x^2 at 4^-FINEST, over 2 pi, at 4^-FINEST."""
    t = 2 * PI_HIGH * Fraction(x2, 4 ** FINEST)
    return (Fraction(2067, 1000) + Fraction(59, 1000) * ln_high(t)) * 4 ** FINEST / (2 * PI_LOW)


def joined(before, after):
    """1 where the last certified sign of `before` and the first of `after` differ: a zero between them."""
    return int(before.sign("last") != after.sign("first"))


def below(cell, back):
    """N at cell's F is at least this, over the window from back's F."""
    s, start = back.sums, back.x2("first")
    weight = cell.x2("first") - start
    theta = s["low_after"] * back.lift + cell.sums["low_before"] * cell.lift
    zeros = s["zeros_p_after"] * back.lift - s["zeros_after"] * start + joined(back, cell) * (back.x2("last") - start)
    return 1 + Fraction(theta + (zeros << SCALE_BITS), weight << SCALE_BITS) - trudgian(cell.x2("first")) / weight


def above(cell, ahead):
    """N at cell's F is at max this, over the window on to ahead's F, or to the cell's last point where none is."""
    s, start = cell.sums, cell.x2("first")
    end = ahead.x2("first") if ahead else cell.end_x2()
    weight = end - start
    theta = s["high_after"] * cell.lift
    if ahead:
        theta += cell.crossing() * ahead.at["origin"][3] + ahead.sums["high_upto"] * ahead.lift
    zeros = s["zeros_after"] * end - s["zeros_q_after"] * cell.lift
    return 1 + trudgian(end) / weight + Fraction(theta - (zeros << SCALE_BITS), weight << SCALE_BITS)


def between(cell, ahead):
    """The zeros certified after cell's F, up to ahead's F."""
    return cell.sums["zeros_after"] + joined(cell, ahead)


def t_of(x2):
    return 2 * math.pi * x2 / 4 ** FINEST


def control(cell):
    """The device's Z against the house's at two points of the cell, at 2^-62 and 40 places."""
    print("  positive control, cell %d:" % cell.nu)
    places = 40
    for j, z in zip((0, cell.points // 3), cell.control):
        device = Fraction(z, 1 << SCALE_BITS)
        s = (cell.nu ** 2 << cell.p) + j * (2 * cell.nu + 1)
        x = naturals._integer_sqrt(s * 10 ** (2 * places) >> cell.p)
        main, remainder = rs.rs_cut_at((cell.nu, rs.zz.pair(x - cell.nu * 10 ** places, places), places), CURVES - 1)
        house = Fraction(main + remainder, 10 ** places)
        print("    s = %d^2 + %d (2 %d + 1) / 2^%d  device Z %.15f  house Z %.15f  apart %.3e" %
              (cell.nu, j, cell.nu, cell.p, float(device), float(house), abs(float(device - house))))


def control_em(cell, other=None, bounds_sum=None):
    """The Z of Euler-Maclaurin at two points of a cell against the house's exact_zeta_zeros routes.

    Where `other` is the same cell by Riemann-Siegel, the check also holds it against that Z within the two bounds."""
    print("  positive control, cell %d by Euler-Maclaurin:" % cell.nu)
    places = 40
    for at, (j, z) in enumerate(zip((0, cell.points // 3), cell.control)):
        device = Fraction(z, 1 << SCALE_BITS)
        big_s = (cell.nu ** 2 << cell.p) + j * (2 * cell.nu + 1)
        x = naturals._integer_sqrt(big_s * 10 ** (2 * places) >> cell.p)
        house = Fraction(rs.em_at((cell.nu, rs.zz.pair(x - cell.nu * 10 ** places, places), 30)), 10 ** 30)
        line = "    s = %d^2 + %d (2 %d + 1) / 2^%d  device Z %.15f  house Z %.15f  apart %.3e" % (
            cell.nu, j, cell.nu, cell.p, float(device), float(house), abs(float(device - house)))
        if other is not None:
            gap = abs(device - Fraction(other.control[at], 1 << SCALE_BITS))
            line += "  from Riemann-Siegel's %.3e, within %.3e: %s" % (
                float(gap), float(bounds_sum / 2 ** SCALE_BITS), gap <= Fraction(bounds_sum, 1 << SCALE_BITS))
        print(line)


def main_em(binary, first, last, rate):
    """Every zero up to T_a, the F of cell `last` + 1, by Euler-Maclaurin over cells `first` to `last`.

    Turing's method certifies N(T_a) over cells `last`, `last` + 1 and `last` + 2 by Riemann-Siegel, and the zeros go
    against it.

    In this dead reckoning walk, the Z of each point is a fix, and its bound is the error about it. A point that clears
    its bound is a certified sign. Where the count falls short, the search widens in some cells. These are each cell
    with a mark of two uncertified points, or every cell where none has one. The wider search uses four times the points
    and ten more Bernoulli terms, with N to match. It goes on until the count closes or the rounds run out.

    If the certified sign changes in (t at cell `first`'s F, T_a] reach N(T_a), each is a zero on the line and simple.
    None lies below, because the count is at max N(T_a) less N at the start."""
    sys.stdout.reconfigure(line_buffering=True)
    constants = Constants()
    above_cells = {}
    for nu in (last, last + 1, last + 2):
        above_cells[nu], _ = run_cell(binary, constants, nu, lattice(nu, rate), "pairs")
    target = above_cells[last + 1]
    low = math.ceil(below(target, above_cells[last]))
    high = math.floor(above(target, above_cells[last + 2]))
    print("  T_a = %.6f, cell %d's F by Riemann-Siegel: %d <= N(T_a) <= %d" % (t_of(target.x2("first")), last + 1, low, high))
    cells, levels = {}, {}
    for nu in range(first, last + 1):
        levels[nu] = 0
        cells[nu], bound = run_cell(binary, constants, nu, lattice(nu, rate), "em", 0)
        print("  cell %d at 2^%d points by Euler-Maclaurin, N = %d, M = %d: bound on Z %.3e, %d zeros past F, %d loose" %
              (nu, cells[nu].p, em_heads(nu, em_terms(0)), em_terms(0), bound / 2 ** SCALE_BITS,
               cells[nu].sums["zeros_after"], cells[nu].sums["loose"]))
    control_em(cells[first])
    print("  steps of the pole, point, pair, verdict and count programs, then Euler-Maclaurin's: %s" %
          cells[first].steps)
    rs_bound, _ = bounds(last, constants, cells[last].p, "pairs")
    em_bound, _ = em_bounds(last, em_heads(last, em_terms(0)), em_terms(0))
    control_em(cells[last], above_cells[last], rs_bound + em_bound)

    def counted():
        joins = [between(cells[nu], cells[nu + 1]) for nu in range(first, last)]
        return sum(joins) + between(cells[last], target)

    count = counted()
    for round_ in range(ROUNDS + 1):
        print("  round %d: %d sign changes certified in (%.6f, %.6f], N(T_a) %d" %
              (round_, count, t_of(cells[first].x2("first")), t_of(target.x2("first")), low))
        if count >= low or round_ == ROUNDS:
            break
        need_fix = [nu for nu in cells if cells[nu].sums["loose"] > 0] or list(cells)
        for nu in need_fix:
            if cells[nu].p + 2 <= FINEST:
                levels[nu] += 1
                cells[nu], _ = run_cell(binary, constants, nu, cells[nu].p + 2, "em", levels[nu])
        count = counted()
    failed = sum(cell.failed for cell in cells.values()) + sum(cell.failed for cell in above_cells.values())
    proven = (count == high) and (failed == 0)
    print("  %s; %d host checks failed" %
          ("every zero in (0, %.6f] is on the line and simple: %d" % (t_of(target.x2("first")), count) if proven
           else "the count does not close", failed))
    return 0 if proven else 1


def main():
    binary = sys.argv[1]
    if len(sys.argv) > 5 and sys.argv[5] == "em":
        first = int(sys.argv[2]) if len(sys.argv) > 2 else 1
        last = int(sys.argv[3]) if len(sys.argv) > 3 else 10
        rate = int(sys.argv[4]) if len(sys.argv) > 4 else 4
        if first < 1 or 2 * last * last <= 168:
            raise SystemExit("  Euler-Maclaurin runs from cell 1 up, and its last cell must start past t = 168 pi")
        return main_em(binary, first, last, rate)
    first = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    last = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    rate = int(sys.argv[4]) if len(sys.argv) > 4 else 4
    method = sys.argv[5] if len(sys.argv) > 5 else "pairs"
    check = len(sys.argv) > 6 and sys.argv[6] == "check"
    if 2 * first * first <= 168 or last < first + 2 or (method not in METHODS and method != "refine"):
        raise SystemExit("  the first cell must start past t = 168 pi, at nu = 10 or above, three cells are the least, "
                         "and the method is one of %s or refine" % ", ".join(METHODS))
    sys.stdout.reconfigure(line_buffering=True)
    constants = Constants()
    control_ = Control() if rate == 0 else None
    cells = {}

    def fetch(nu, p):
        """Cell nu at 2^p points by the method asked.

        With refine, it runs by the multiple evaluation with the twist, and certifies the flagged spans again at the
        finer lattice by the listed points. With `check`, it certifies the crossings too."""
        if method != "refine":
            return run_cell(binary, constants, nu, p, method)
        cell, bound = run_cell(binary, constants, nu, p, "transform", listing=2)
        spans = flagged_spans(cell, check)
        os.remove(cell.path)
        listed, added, still, failed = refine(binary, constants, cell, spans, check)
        cell.failed += failed
        cell.refined = (len(spans), listed, added, still, cell.margins["single"], cell.sums["zeros_after"])
        return cell, bound

    def shortfall(nu):
        """The zeros cell nu misses, from N certified at its F and the next cell's: 0 where it closes."""
        low = math.ceil(below(cells[nu], cells[nu - 1]))
        high = math.floor(above(cells[nu + 1], cells[nu + 2]))
        return max(0, high - low - between(cells[nu], cells[nu + 1]))

    def timed(nu, p):
        began = time.time()
        cell, bound = fetch(nu, p)
        if control_:
            control_.timing(p, time.time() - began)
        return cell, bound

    for nu in range(first, last + 1):
        p = control_.choose(nu) if control_ else lattice(nu, rate)
        cells[nu], bound = timed(nu, p)
        if control_ and nu - 3 >= first:
            m = nu - 2
            control_.measure(m, cells[m].p, shortfall(m))
        if nu == first:
            control(cells[nu])
            print("  steps of the pole, point, pair, verdict and count programs, then the multiple evaluation's: %s" %
                  cells[nu].steps)
        print("  cell %d at 2^%d points: bound on Z %.3e, F %d, %d zeros past F, %d loose%s" %
              (nu, cells[nu].p, bound / 2 ** SCALE_BITS, cells[nu].first, cells[nu].sums["zeros_after"],
               cells[nu].sums["loose"], "; c %.3f, a run %.2f s at 2^%d" %
               (float(control_.c()), control_.timed[-1][1], cells[nu].p) if control_ else ""))
        if method == "refine":
            print("    %d spans flagged, %d points listed, %d zeros found in them, %d spans still open; "
                  "%d crossings proven single on the coarse lattice, of %d zeros past F" % cells[nu].refined)
        if cells[nu].apart:
            print("    the transform's Z and the pairs' differ by %.3e at max, at point %d" %
                  (cells[nu].apart[0] / 2 ** SCALE_BITS, cells[nu].apart[1]))
    for round_ in range(ROUNDS + 1):
        held = {}
        for nu in range(first + 1, last + 1):
            ahead = cells.get(nu + 1) if nu < last else None
            held[nu] = (math.ceil(below(cells[nu], cells[nu - 1])), math.floor(above(cells[nu], ahead)))
        loose = [nu for nu, (low, high) in held.items() if low != high]
        short = [nu for nu in range(first + 1, last) if held[nu + 1][1] - held[nu][0] > between(cells[nu], cells[nu + 1])]
        count = sum(between(cells[nu], cells[nu + 1]) for nu in range(first + 1, last))
        print("  round %d: %d zeros certified, %d cells short %s, %d points where N is not certified to one value" %
              (round_, count, len(short), short[:12], len(loose)))
        if not short or round_ == ROUNDS:
            break
        coarse = set(short) | {m for nu in loose for m in (nu - 1, nu, nu + 1) if first <= m <= last}
        for nu in sorted(coarse):
            if cells[nu].p + 2 <= FINEST and not control_:
                cells[nu], _ = fetch(nu, cells[nu].p + 2)
            elif control_ and cells[nu].p < FINEST:
                cells[nu], _ = timed(nu, control_.choose(nu, cells[nu].p))
    failed = sum(cell.failed for cell in cells.values())
    (low_a, high_a), (low_z, high_z) = held[first + 1], held[last]
    print("  T_a = %.6f: %d <= N(T_a) <= %d" % (t_of(cells[first + 1].x2("first")), low_a, high_a))
    print("  T_b = %.6f: %d <= N(T_b) <= %d" % (t_of(cells[last].x2("first")), low_z, high_z))
    print("  zeros certified in (T_a, T_b]: %d; N(T_b) - N(T_a) <= %d" % (count, high_z - low_a))
    proven = (high_z - low_a <= count) and (failed == 0)
    print("  %s; %d host checks failed" %
          ("every zero in (T_a, T_b] is on the line and simple: %d" % count if proven else "the count does not close",
           failed))
    return 0 if proven else 1


if __name__ == "__main__":
    sys.exit(main())
