#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-033
#
# The demon's arms over the zeros Turing's machine certifies: the identity:null permutation, run on the curves the
# zeros draw, in exact integers. Each cell runs once on the device with every point listed and its theta / pi less
# and more its bound, the listing word 3. A zero lies between two certified points of opposite sign. Theta / pi
# rises, and its u = theta / pi + 1 lies between the first point's theta / pi less its bound and the second's more its
# bound, plus 1: an exact interval of integers at 2^-62. A close pair no point falls between is found where |Z| dips
# and keeps its sign, listed again 64 times finer by pairs with the same brackets. The zeros placed must number N's
# difference between two F's, N held by Turing's method or given where a run of the machine closes over them.
#
# Each pair of zeros within 8 has its difference as the interval the two zeros' intervals give, and each eighth of u
# its pairs whose interval lies in it and those whose interval meets it and another. The steering is by those
# verdicts: a zero in a pair that meets two eighths is listed again 16 times finer between its two certified points,
# by pairs with the brackets, and takes the interval about its one change of sign there, for up to ROUNDS rounds. A
# zero whose two points stand in different cells keeps its interval.
#
#   Usage:  python examples/0_experimental/exact_zeta_demon.py <binary> <first> <last> [extra] [N at first's F N at last's F]
#
# Each zero stands at the midpoint of its interval, an integer at 2^-64, and every curve is read from those integers
# with no rounding: S between zeros, n - u at the midpoint of the n-th zero and the next; S's correlation over k zeros;
# S's pull back each zero; the neighbors' correlation of the gaps; the number variance of windows of L on u, from
# windows starting every half unit; and the pair counts, every pair of zeros within 8 of each other binned by eighths.
#
# The arms: the identity:null permutation, DRAWS draws an arm. Arm a shuffles the gaps' order with every gap kept;
# arm b shuffles them within each run of BLOCK gaps, which keeps S at every BLOCK-th zero. A curve's point stands
# above background where no draw reaches it, the real value past every draw's on one side; its reach is the fewer of
# the draws at or above it and at or below it, and the verdict is whether the reach is 0. A pair count's bin is marked
# where no draw reaches any count the intervals allow. No number is chosen. The identity permutation gives back every
# real value, and every draw keeps each run's gaps: both are checked.
#
# It sits in 0_experimental and is an entry in the analytic number theory workbook, on its rail. It proves nothing;
# it reads which of the zeros' curves their gaps alone draw, and which only their order does.

import bisect
import math
import operator
import os
import random
import sys
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import exact_zeta_turing as tm  # noqa: E402

ONE = 1 << tm.SCALE_BITS
# u at 2^-64: the midpoint of an interval at 2^-62 is a half integer there, and S between two zeros a quarter
WIDE = 1 << (tm.SCALE_BITS + 2)
DRAWS = 31
LAGS = 48
FINER = 6
DIPS = 200
LENGTHS = (1, 2, 4, 8, 16, 32, 64, 128, 256)
REACH = 8
BINS = 8
BLOCK = 64
NARROW = 4
ROUNDS = 3


def show(num, den, places=4):
    """num / den in decimal, truncated toward zero at `places`, for the page alone."""
    sign = "-" if (num < 0) != (den < 0) and num != 0 else ""
    whole, rest = divmod(abs(num) * 10 ** places // abs(den), 10 ** places)
    return "%s%d.%0*d" % (sign, whole, places, rest)


def bracketed_points(binary, constants, first, last, extra):
    """Every listed point of the cells, in order: (cell, lane, certified sign, Z, theta / pi less and more its bound)."""
    cells, points = {}, []
    for nu in range(first, last + 1):
        p = tm.lattice(nu, 4) + extra
        cell, _ = tm.run_cell(binary, constants, nu, p, "transform", listing=3)
        cells[nu] = cell
        with open(cell.path) as handle:
            lane = 0
            for line in handle:
                if line.startswith("point"):
                    v = [int(x, 16) for x in line.split()[1:]]
                    points.append((nu, lane, v[0], v[2], v[5], v[6]))
                    lane += 1
        os.remove(cell.path)
    return cells, points


def zero_records(binary, constants, points, start, end, wanted, extra):
    """Each zero between points start and end: its interval of u at 2^-62, and the cell, lattice and two lanes of the
    certified points about it, the lattice None where the two stand in different cells. At every change of certified
    sign, then in the deepest dips that keep their sign, listed again finer, until they number `wanted`."""
    held = [i for i in range(start, end + 1) if points[i][2] != 0]
    zeros = []
    for a, b in zip(held, held[1:]):
        if points[a][2] != points[b][2]:
            same = points[a][0] == points[b][0]
            zeros.append((points[a][4] + ONE, points[b][5] + ONE, points[a][0],
                          tm.lattice(points[a][0], 4) + extra if same else None, points[a][1], points[b][1]))
    dips = sorted((abs(points[i][3]), i) for i in range(start + 1, end)
                  if (points[i - 1][3] > 0) == (points[i][3] > 0) == (points[i + 1][3] > 0)
                  and abs(points[i][3]) < abs(points[i - 1][3]) and abs(points[i][3]) < abs(points[i + 1][3]))
    added, listed, failed = [], 0, 0
    for _, i in dips[:DIPS]:
        if len(zeros) + len(added) >= wanted:
            break
        nu, lane = points[i][0], points[i][1]
        p = tm.lattice(nu, 4) + extra
        if lane == 0 or lane + 1 >= 1 << p:
            continue
        js = [((lane - 1) << FINER) + k for k in range(2 << FINER)]
        rows, bad = tm.run_points(binary, constants, nu, p + FINER, js, bracketed=True)
        failed += bad
        listed += 1
        signed = [(j, row) for j, row in zip(js, rows) if row[0] != 0]
        added += [(a[2] + ONE, b[3] + ONE, nu, p + FINER, ja, jb) for (ja, a), (jb, b) in zip(signed, signed[1:])
                  if a[0] != b[0]]
    print("  zeros at a change of certified sign %d, dips listed again %d, zeros found in them %d, host checks failed %d"
          % (len(zeros), listed, len(added), failed))
    return sorted(zeros + added), failed


def narrowed(binary, constants, records, which):
    """The zeros in `which` listed again 2^NARROW times finer between their two certified points, by pairs with the
    brackets, each taking the interval between the certified points about its one change of sign there; a zero whose
    two points stand in different cells, or whose finer points change sign other than once, keeps its interval."""
    groups = {}
    for k in which:
        lo, hi, nu, p, ja, jb = records[k]
        if p is not None:
            groups.setdefault((nu, p), []).append(k)
    out, failed, kept = list(records), 0, 0
    for (nu, p), members in groups.items():
        js, spans = [], []
        for k in members:
            ja, jb = records[k][4], records[k][5]
            spans.append((len(js), (jb - ja << NARROW) + 1))
            js += range(ja << NARROW, (jb << NARROW) + 1)
        rows, bad = tm.run_points(binary, constants, nu, p + NARROW, js, bracketed=True)
        failed += bad
        for k, (at, count) in zip(members, spans):
            signed = [(js[at + i], rows[at + i]) for i in range(count) if rows[at + i][0] != 0]
            changes = [((ja, a), (jb, b)) for (ja, a), (jb, b) in zip(signed, signed[1:]) if a[0] != b[0]]
            one = len(changes) == 1
            kept += 1 - one
            if one:
                (ja, a), (jb, b) = changes[0]
                out[k] = (a[2] + ONE, b[3] + ONE, nu, p + NARROW, ja, jb)
    return out, failed, kept


def hold(cells, first, last, given):
    """The first and last cells whose F holds N to one value, and N there."""
    if given:
        return first, last, given[0], given[1]
    held = {nu: (math.ceil(tm.below(cells[nu], cells[nu - 1])), math.floor(tm.above(cells[nu], cells[nu + 1])))
            for nu in range(first + 1, last)}
    tight = [nu for nu in sorted(held) if held[nu][0] == held[nu][1]]
    if len(tight) < 2:
        raise SystemExit("  N is not held to one value at two F's; give N at the first and last cells' F")
    return tight[0], tight[-1], held[tight[0]][0], held[tight[-1]][0]


def curves(positions, n_start):
    """Every curve over the zeros at `positions`, integers at 2^-64, the first zero N's n_start + 1-th."""
    n = len(positions) - 1
    s = [((n_start + k + 1) * WIDE << 1) - positions[k] - positions[k + 1] for k in range(n)]
    total = sum(s)
    x = [n * v - total for v in s]
    covariance = [sum(map(operator.mul, x[:n - lag], x[lag:])) for lag in range(LAGS + 1)]
    pull = (sum(map(operator.mul, x[:-1], x[1:])) - covariance[0] + x[-1] * x[-1], covariance[0] - x[-1] * x[-1])
    gaps = [b - a for a, b in zip(positions, positions[1:])]
    gap_total = sum(gaps)
    y = [n * g - gap_total for g in gaps]
    neighbors = sum(map(operator.mul, y[:-1], y[1:]))
    starts = range(positions[0], positions[-1] - LENGTHS[-1] * WIDE, WIDE >> 1)
    variances = []
    for length in LENGTHS:
        reach = length * WIDE
        counts = [bisect.bisect_left(positions, at + reach) - bisect.bisect_left(positions, at) for at in starts]
        variances.append(len(counts) * sum(c * c for c in counts) - sum(counts) ** 2)
    bins = [0] * (REACH * BINS)
    for i, at in enumerate(positions):
        for j in range(i + 1, bisect.bisect_left(positions, at + REACH * WIDE)):
            bins[(positions[j] - at) * BINS // WIDE] += 1
    return {"covariance": covariance, "pull": pull, "neighbors": neighbors, "variances": variances, "bins": bins,
            "windows": len(starts), "n": n}


def pair_ranges(intervals):
    """Each pair of zeros within REACH by its difference's interval, from the two zeros' intervals: by bin, the pairs
    whose whole interval lies in it, and the pairs whose interval meets it and another bin besides; and the zeros of
    those pairs."""
    last = REACH * BINS - 1
    decided, straddling, open_zeros = [0] * (last + 1), [0] * (last + 1), set()
    lows = [a for a, _ in intervals]
    for i, (a, b) in enumerate(intervals):
        for j in range(i + 1, bisect.bisect_left(lows, b + REACH * ONE)):
            low = max(intervals[j][0] - b, 0) * BINS // ONE
            high = (intervals[j][1] - a) * BINS // ONE
            whole = int(low == high)
            for at in range(low, min(high, last) + 1):
                decided[at] += whole
                straddling[at] += 1 - whole
            if not whole and low <= last:
                open_zeros.update((i, j))
    return decided, straddling, open_zeros


def gaps_of(positions):
    return [b - a for a, b in zip(positions, positions[1:])]


def permuted(positions, rng, block):
    """The zeros again with their gaps' order shuffled within each run of `block` gaps, every gap kept; the identity
    where rng is None."""
    gaps = [b - a for a, b in zip(positions, positions[1:])]
    for at in range(0, len(gaps), block) if rng is not None else ():
        part = gaps[at:at + block]
        rng.shuffle(part)
        gaps[at:at + block] = part
    out = [positions[0]]
    for g in gaps:
        out.append(out[-1] + g)
    return out


def reach_of(real, draws, above):
    """The fewer of the draws at or past the real value on each side, and '+' where the real stands above most of
    them; `above(a, b)` is 1 where a stands above b."""
    up = sum(1 - above(real, d) for d in draws)
    down = sum(1 - above(d, real) for d in draws)
    return min(up, down), "+" if up < down else "-"


def mean_of(fractions):
    total = sum(fractions, Fraction(0))
    return total.numerator, total.denominator * len(fractions)


def report(real, arms, positions, ranges):
    """Each curve's real values beside each arm's mean of its draws, with the reach and the side."""
    n = real["n"]
    names = "; ".join("%s, %s" % (chr(ord("a") + i), name) for i, (name, _) in enumerate(arms))
    print("  the arms: %s. Each column: the draws' mean, the reach, the side" % names)
    print("  S's correlation over k zeros: k, the real, then each arm:")
    outside = [[] for _ in arms]
    for lag in range(1, LAGS + 1):
        ratio = Fraction(n, n - lag)
        rho = ratio * Fraction(real["covariance"][lag], real["covariance"][0])
        columns = []
        for i, (_, draws) in enumerate(arms):
            mean = mean_of([ratio * Fraction(d["covariance"][lag], d["covariance"][0]) for d in draws])
            reach, side = reach_of(real, draws, lambda a, b: int(a["covariance"][lag] * b["covariance"][0] >
                                                                 b["covariance"][lag] * a["covariance"][0]))
            outside[i] += ["%s%d" % (side, lag)] * (reach == 0)
            columns.append("%8s %2d %s" % (show(*mean), reach, side))
        print("    k %2d: %8s   %s" % (lag, show(rho.numerator, rho.denominator), "   ".join(columns)))
    for i in range(len(arms)):
        print("  lags no draw of %s reaches, with their side: %s" % (chr(ord("a") + i), " ".join(outside[i])))

    columns = []
    for _, draws in arms:
        reach, side = reach_of(real, draws, lambda a, b: int(a["pull"][0] * b["pull"][1] > b["pull"][0] * a["pull"][1]))
        columns.append("%s %d %s" % (show(*mean_of([Fraction(*d["pull"]) for d in draws])), reach, side))
    print("  S's pull back each zero %s;   %s" % (show(*real["pull"]), "   ".join(columns)))
    gaps = [n * (b - a) - (positions[-1] - positions[0]) for a, b in zip(positions, positions[1:])]
    square = sum(g * g for g in gaps) * (n - 1)
    columns = []
    for _, draws in arms:
        reach, side = reach_of(real, draws, lambda a, b: int(a["neighbors"] > b["neighbors"]))
        columns.append("%s %d %s" % (show(sum(d["neighbors"] for d in draws) * n, square * len(draws)), reach, side))
    print("  the gaps' neighbors' correlation %s;   %s" % (show(real["neighbors"] * n, square), "   ".join(columns)))

    windows = real["windows"]
    print("  number variance over windows of L on u, %d windows a length: L, the real, then each arm:" % windows)
    for at, length in enumerate(LENGTHS):
        columns = []
        for _, draws in arms:
            reach, side = reach_of(real, draws, lambda a, b: int(a["variances"][at] > b["variances"][at]))
            columns.append("%9s %2d %s" % (show(sum(d["variances"][at] for d in draws), windows * windows * len(draws)),
                                           reach, side))
        print("    L %3d: %9s   %s" % (length, show(real["variances"][at], windows * windows), "   ".join(columns)))

    decided, straddling, _ = ranges
    held = all(decided[b] <= real["bins"][b] <= decided[b] + straddling[b] for b in range(REACH * BINS))
    print("  pair counts a zero over differences of u by eighths: the bin, the real, the pairs whose difference's "
          "interval lies in the bin and those meeting it and another, then each arm. Every real count between the "
          "two: %s" % held)
    marks = [[] for _ in arms]
    for b in range(REACH * BINS):
        columns = []
        for i, (_, draws) in enumerate(arms):
            reach, side = reach_of(real, draws, lambda x, y: int(x["bins"][b] > y["bins"][b]))
            whole = (all(d["bins"][b] < decided[b] for d in draws) or
                     all(d["bins"][b] > decided[b] + straddling[b] for d in draws))
            marks[i].append(side + ("#" if whole else "*" if reach == 0 else " "))
            columns.append("%s %2d %s" % (show(sum(d["bins"][b] for d in draws), n * len(draws)), reach, side))
        if b < 2 * BINS or b % BINS == 0:
            print("    [%s, %s): %s  %s  %s   %s" % (show(b, BINS, 3), show(b + 1, BINS, 3), show(real["bins"][b], n),
                                                  show(decided[b], n), show(straddling[b], n), "   ".join(columns)))
    for i in range(len(arms)):
        print("  every eighth against %s, its side, '*' where no draw reaches the real and '#' where none reaches any "
              "count the intervals allow:" % chr(ord("a") + i))
        for row in range(REACH):
            print("    %d to %d: %s" % (row, row + 1, " ".join(marks[i][row * BINS:(row + 1) * BINS])))


def main():
    binary, first, last = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    extra = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    given = (int(sys.argv[5]), int(sys.argv[6])) if len(sys.argv) > 6 else None
    sys.stdout.reconfigure(line_buffering=True)
    constants = tm.Constants()
    cells, points = bracketed_points(binary, constants, first, last, extra)
    start_nu, end_nu, n_start, n_end = hold(cells, first, last, given)
    start = next(i for i, q in enumerate(points) if q[0] == start_nu and q[1] == cells[start_nu].first)
    end = next(i for i, q in enumerate(points) if q[0] == end_nu and q[1] == cells[end_nu].first)
    records, failed = zero_records(binary, constants, points, start, end, n_end - n_start, extra)
    failed += sum(cell.failed for cell in cells.values())
    if len(records) != n_end - n_start:
        raise SystemExit("  the zeros placed, %d, do not number N's difference, %d" % (len(records), n_end - n_start))
    for turn in range(ROUNDS + 1):
        intervals = [(r[0], r[1]) for r in records]
        decided, straddling, open_zeros = pair_ranges(intervals)
        print("  round %d: pairs within %d whose difference lies in one eighth %d; zeros in a pair meeting two %d" %
              (turn, REACH, sum(decided), len(open_zeros)))
        if turn == ROUNDS or not open_zeros:
            break
        records, bad, kept = narrowed(binary, constants, records, sorted(open_zeros))
        failed += bad
        print("    listed again %d times finer: %d zeros, %d kept their interval, host checks failed %d" %
              (1 << NARROW, len(open_zeros), kept, bad))
    widest = max(b - a for a, b in intervals)
    ordered = all(a0 < a1 and b0 < b1 for (a0, b0), (a1, b1) in zip(intervals, intervals[1:]))
    print("  from cell %d's F, N = %d, to cell %d's, N = %d: %d zeros, each an interval of u, the widest %s, both ends "
          "rising from each to the next: %s" % (start_nu, n_start, end_nu, n_end, len(intervals), show(widest, ONE, 6),
                                                 ordered))
    positions = [2 * (a + b) for a, b in intervals]

    real = curves(positions, n_start)
    identity = curves(permuted(positions, None, 1), n_start) == real
    rng = random.Random(20261004)
    arms, kept = [], True
    for name, block in (("the gaps shuffled", len(positions)), ("the gaps shuffled within runs of %d" % BLOCK, BLOCK)):
        draws = []
        for _ in range(DRAWS):
            drawn = permuted(positions, rng, block)
            kept = kept and all(sorted(gaps_of(drawn[at:at + block + 1])) == sorted(gaps_of(positions[at:at + block + 1]))
                                for at in range(0, len(positions) - 1, block))
            draws.append(curves(drawn, n_start))
        arms.append((name, draws))
    print("  the identity permutation gives back every real value: %s; every draw keeps each run's gaps: %s; %d draws "
          "an arm" % (identity, kept, DRAWS))
    report(real, arms, positions, (decided, straddling, open_zeros))
    print("  host checks failed: %d" % failed)
    return int(failed != 0 or not identity or not kept)


if __name__ == "__main__":
    sys.exit(main())
