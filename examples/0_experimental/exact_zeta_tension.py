#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-031
#
# The field's tension, read over the zeros Turing's machine certifies. Each cell runs once on the device by the
# multiple evaluation, every point listed, on the lattice `extra` steps finer than four points a zero. N is held at
# the cells' F's by Turing's method, or given where a run of the machine closes over them; every zero is placed at a
# change of sign of the device's Z between adjacent points, on the line through Z at the two, and a close pair no
# point falls between is found where |Z| dips without a change of sign, listed again 64 times finer by pairs on the
# device. The zeros placed must number N's difference between the two F's.
#
#   Usage:  python examples/0_experimental/exact_zeta_tension.py <binary> <first> <last> [extra] [N at first's F N at last's F]
#
# On theta / pi, u = theta / pi + 1, each zero at its u:
# - S between zeros, n - u at the midpoint of the n-th and the next, its mean, its variance against Selberg's leading
#   term (1 / (2 pi^2)) ln ln(t / 2 pi), and its pull back to 0, the slope of each step of S on S;
# - S's correlation over k zeros, k to 60, against the waves of S: S = -(1 / pi) times the sum over prime powers p^r
#   of sin(r t ln p) / (r p^(r / 2)), whose correlation at a lag tau in t, the phases taken as independent, is the sum
#   of cos(r tau ln p) / (r^2 p^r) over the sum of 1 / (r^2 p^r);
# - the number variance of the zeros over windows of L, against GUE's (1 / pi^2)(ln 2 pi L + gamma + 1 - pi^2 / 8),
#   Poisson's L and the same spacings shuffled, which keep every gap and drop how the gaps hold to one another.
#
# The statistics are the host's, in floating point, over the device's Z. It sits in 0_experimental and is an entry
# in the analytic number theory workbook, on its rail. It proves nothing; it reads how the zeros hold to one another.

import bisect
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import exact_zeta_turing as tm  # noqa: E402

UNIT = float(1 << tm.SCALE_BITS)
EULER = 0.5772156649015329
# the dips listed again: each 2^FINER times finer, at most DIPS of them, the deepest first
FINER = 6
DIPS = 200
LAGS = 60


def theta_pi(s):
    """theta / pi at t = 2 pi s, the point stage's series."""
    return s * math.log(s) - s - 0.125 + 1.0 / (96.0 * math.pi * math.pi * s)


def listed_points(binary, constants, first, last, extra):
    """Every listed point of the cells, in order: (cell, lane, s, certified sign, Z)."""
    cells, points = {}, []
    for nu in range(first, last + 1):
        p = tm.lattice(nu, 4) + extra
        cell, _ = tm.run_cell(binary, constants, nu, p, "transform", listing=1)
        cells[nu] = cell
        with open(cell.path) as handle:
            lane = 0
            for line in handle:
                if line.startswith("point"):
                    values = line.split()[1:]
                    points.append((nu, lane, int(values[1], 16) / float(1 << p), int(values[0], 16),
                                   int(values[2], 16) / UNIT))
                    lane += 1
        os.remove(cell.path)
    return cells, points


def place_zeros(binary, constants, cells, points, start, end, wanted, extra):
    """The zeros between points start and end: at every change of sign of Z, then in the deepest dips without one,
    listed again finer, until they number `wanted`."""
    zeros = []
    for i in range(start, end):
        (s0, z0), (s1, z1) = (points[i][2], points[i][4]), (points[i + 1][2], points[i + 1][4])
        if (z0 > 0) != (z1 > 0):
            zeros.append(s0 + (s1 - s0) * z0 / (z0 - z1))
    dips = sorted((abs(points[i][4]), i) for i in range(start + 1, end)
                  if (points[i - 1][4] > 0) == (points[i][4] > 0) == (points[i + 1][4] > 0)
                  and abs(points[i][4]) < abs(points[i - 1][4]) and abs(points[i][4]) < abs(points[i + 1][4]))
    added, listed = [], 0
    for _, i in dips[:DIPS]:
        if len(zeros) + len(added) >= wanted:
            break
        nu, lane = points[i][0], points[i][1]
        p = tm.lattice(nu, 4) + extra
        if lane == 0 or lane + 1 >= 1 << p:
            continue
        rows, failed = tm.run_points(binary, constants, nu, p + FINER, [((lane - 1) << FINER) + k for k in range(2 << FINER)])
        if failed:
            raise SystemExit("  a listed run's host check failed")
        listed += 1
        signs = [(row[0], row[1]) for row in rows if row[0] != 0]
        for (sign_a, s_a), (sign_b, s_b) in zip(signs, signs[1:]):
            if sign_a != sign_b:
                added.append(0.5 * (s_a + s_b) / float(1 << (p + FINER)))
    print("  zeros at a change of sign %d, dips listed again %d, zeros found in them %d" % (len(zeros), listed, len(added)))
    return sorted(zeros + added)


def autocorrelation(series, lag):
    m = sum(series) / len(series)
    v = sum((x - m) ** 2 for x in series) / len(series)
    return sum((series[i] - m) * (series[i + lag] - m) for i in range(len(series) - lag)) / ((len(series) - lag) * v)


def pull(series):
    """The slope of S's next step on S: 0 for a walk with no pull back, -1 for a pull all the way to 0 each step."""
    m = sum(series) / len(series)
    xs = [x - m for x in series[:-1]]
    ys = [series[i + 1] - series[i] for i in range(len(series) - 1)]
    return sum(x * y for x, y in zip(xs, ys)) / sum(x * x for x in xs)


def prime_powers(limit):
    """Every prime power p^r up to limit, as (p, r)."""
    sieve = bytearray([1]) * (limit + 1)
    found = []
    for p in range(2, limit + 1):
        if sieve[p]:
            sieve[p * p::p] = bytearray(len(sieve[p * p::p]))
            power, r = p, 1
            while power <= limit:
                found.append((p, r))
                power, r = power * p, r + 1
    return found


def number_variance(positions, length, samples):
    low, high = positions[0], positions[-1] - length
    counts = []
    for _ in range(samples):
        x = random.uniform(low, high)
        counts.append(bisect.bisect_right(positions, x + length) - bisect.bisect_right(positions, x))
    m = sum(counts) / samples
    return m, sum((c - m) ** 2 for c in counts) / samples


def certified_zeros(binary, first, last, extra, given=None):
    """The zeros from the first F where N is held to the last, every one placed: (zeros in s, N at the first F, the
    first F's cell, the last's). `given` is N at cells first and last's F, where a run of the machine closes over
    them."""
    constants = tm.Constants()
    cells, points = listed_points(binary, constants, first, last, extra)
    if given:
        start_nu, end_nu, (n_start, n_end) = first, last, given
        print("  cells %d to %d, %d points; N at the two F's as given" % (first, last, len(points)))
    else:
        held = {nu: (math.ceil(tm.below(cells[nu], cells[nu - 1])), math.floor(tm.above(cells[nu], cells[nu + 1])))
                for nu in range(first + 1, last)}
        tight = [nu for nu in sorted(held) if held[nu][0] == held[nu][1]]
        if len(tight) < 2:
            raise SystemExit("  N is not held to one value at two F's; give N at the first and last cells' F")
        start_nu, end_nu = tight[0], tight[-1]
        n_start, n_end = held[start_nu][0], held[end_nu][0]
        print("  cells %d to %d, %d points; N held to one value at %d of %d F's" %
              (first, last, len(points), len(tight), len(held)))
    start = next(i for i, q in enumerate(points) if q[0] == start_nu and q[1] == cells[start_nu].first)
    end = next(i for i, q in enumerate(points) if q[0] == end_nu and q[1] == cells[end_nu].first)
    zeros = place_zeros(binary, constants, cells, points, start, end, n_end - n_start, extra)
    if len(zeros) != n_end - n_start:
        raise SystemExit("  the zeros placed, %d, do not number N's difference, %d" % (len(zeros), n_end - n_start))
    return zeros, n_start, start_nu, end_nu


def main():
    binary, first, last = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    extra = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    sys.stdout.reconfigure(line_buffering=True)
    given = (int(sys.argv[5]), int(sys.argv[6])) if len(sys.argv) > 6 else None
    zeros, n_start, start_nu, end_nu = certified_zeros(binary, first, last, extra, given)
    n_end = n_start + len(zeros)
    u = [theta_pi(s) + 1.0 for s in zeros]
    s_low, s_high = zeros[0], zeros[-1]
    spacing = 2 * math.pi * (s_high - s_low) / (len(zeros) - 1)
    print("  from cell %d's F, N = %d, to cell %d's, N = %d: %d zeros, t from %.1f to %.1f, ln(t / 2 pi) %.3f, "
          "mean spacing in t %.4f" % (start_nu, n_start, end_nu, n_end, len(zeros), 2 * math.pi * s_low,
                                      2 * math.pi * s_high, math.log(0.5 * (s_low + s_high)), spacing))

    S = [n_start + k + 1 - 0.5 * (u[k] + u[k + 1]) for k in range(len(u) - 1)]
    mean = sum(S) / len(S)
    variance = sum((v - mean) ** 2 for v in S) / len(S)
    print("  S between zeros: mean %.4f, variance %.4f, least %.3f, most %.3f; Selberg's leading term %.4f" %
          (mean, variance, min(S), max(S), math.log(math.log(0.5 * (s_low + s_high))) / (2 * math.pi ** 2)))

    random.seed(20261004)
    gaps = [u[k + 1] - u[k] for k in range(len(u) - 1)]
    shuffled = gaps[:]
    random.shuffle(shuffled)
    u_shuffled = [u[0]]
    for gap in shuffled:
        u_shuffled.append(u_shuffled[-1] + gap)
    S_shuffled = [n_start + k + 1 - 0.5 * (u_shuffled[k] + u_shuffled[k + 1]) for k in range(len(u_shuffled) - 1)]
    gap_variance = sum((g - 1) ** 2 for g in gaps) / len(gaps)
    neighbors = sum((gaps[k] - 1) * (gaps[k + 1] - 1) for k in range(len(gaps) - 1)) / (len(gaps) - 1) / gap_variance
    print("  spacings on theta / pi: mean %.4f, variance %.4f, neighbors' correlation %+.4f" %
          (sum(gaps) / len(gaps), gap_variance, neighbors))
    print("  S's pull back each zero %+.4f; with the spacings shuffled %+.4f" % (pull(S), pull(S_shuffled)))

    correlations = [autocorrelation(S, lag) for lag in range(LAGS + 1)]
    deepest = min(range(1, LAGS + 1), key=lambda lag: correlations[lag])
    print("  S's correlation over k zeros, k 1 to %d; its least at k = %d, %+.3f, a lag of %.2f in t:" %
          (LAGS, deepest, correlations[deepest], deepest * spacing))
    for row in range(0, LAGS, 10):
        print("    %s" % " ".join("%+.3f" % c for c in correlations[row + 1:row + 11]))
    for limit in (100, 10000):
        waves = prime_powers(limit)
        weight = sum(1.0 / (r * r * p ** r) for p, r in waves)
        model = [sum(math.cos(r * lag * spacing * math.log(p)) / (r * r * p ** r) for p, r in waves) / weight
                 for lag in range(LAGS + 1)]
        scale = (sum(correlations[lag] * model[lag] for lag in range(1, LAGS + 1)) /
                 sum(model[lag] ** 2 for lag in range(1, LAGS + 1)))
        residual = math.sqrt(sum((correlations[lag] - scale * model[lag]) ** 2 for lag in range(1, LAGS + 1)) / LAGS)
        agree = sum(1 for lag in range(1, LAGS + 1) if (correlations[lag] > 0) == (model[lag] > 0))
        print("  the waves of the prime powers to %d, scaled by %.3f: %d of %d signs the same, rms %.3f" %
              (limit, scale, agree, LAGS, residual))
    for lag in (100, 200, 500, 1000):
        print("  S's correlation over %d zeros %+.4f; the shuffled spacings' %+.4f" %
              (lag, autocorrelation(S, lag), autocorrelation(S_shuffled, lag)))

    print("  number variance over L on theta / pi: the zeros, the shuffled spacings, GUE's, Poisson's")
    for length in (0.5, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 25, 40, 60, 100, 160, 250, 400):
        if length * 8 > len(u):
            continue
        _, v = number_variance(u, length, 80000)
        _, v_shuffled = number_variance(u_shuffled, length, 80000)
        gue = (math.log(2 * math.pi * length) + EULER + 1 - math.pi ** 2 / 8) / math.pi ** 2
        print("    L %6.1f: %.4f  %8.4f  %.4f  %.1f" % (length, v, v_shuffled, gue, length))
    return 0


if __name__ == "__main__":
    sys.exit(main())
