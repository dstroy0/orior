#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-034
#
# The walker w = exp(i theta) F read against known carriers laid on theta / pi, and the demon's arms against each. Each
# cell runs once on the device by the multiple evaluation on a lattice of 16 points a zero or more, every point listed
# with w and theta / pi less and more its bound, the listing word 3.
#
#   Usage:  python examples/0_experimental/exact_zeta_carrier.py <binary> <first> <last> [rate]
#
# A carrier is a run of marks on theta / pi from the cell's first point, `rate` marks a unit on average: the uniform
# comb, the 1, 1, 2 comb, and the golden and silver words, the Sturmian words of slope 1 / beta with beta = (n +
# sqrt(n^2 + 4)) / 2, n 1 and 2, two gaps in ratio beta. The word's letter k is floor((k + 1) / beta) - floor(k /
# beta), and floor(k / beta) = (isqrt(k^2 (n^2 + 4)) - k n) // 2, exactly. A mark is read at the first listed point
# whose theta / pi less its bound reaches it: for the uniform comb and 1, 1, 2 by integer comparison, and for the
# metallic words by A >= 0 and A^2 >= (n^2 + 4) B^2 with A and B the integers the mark's place gives, sqrt(n^2 + 4)
# never written down.
#
# At the points a carrier reads, the moments of w, exact sums of the device's integers at 2^-62, each over the
# count: the inertia |w|^2; w itself; w against its conjugate, w^2; and from each read point to the next, the
# coherence Re(conj(w_k) w_(k+1)) and the angular momentum Im(conj(w_k) w_(k+1)). The beat is the difference of two
# carriers' moments.
#
# The demon's arms, DRAWS draws each: a, the carrier's steps shuffled, every step kept; b, the carrier shifted along
# theta / pi by d / (DRAWS + 1) of its mean step, d from 1 to DRAWS. A moment stands above background where no draw
# reaches it, its reach the fewer of the draws at or above it and at or below it. The uniform comb's steps are all
# alike, and its arm a gives it back; that is checked. No number is chosen.
#
# It sits in 0_experimental and is an entry in the analytic number theory workbook, on its rail. It proves nothing;
# it reads whether w holds to a carrier laid on theta's own clock.

import bisect
import math
import os
import random
import sys
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import exact_zeta_turing as tm  # noqa: E402

ONE = 1 << tm.SCALE_BITS
DRAWS = 31
CARRIERS = ("uniform", "comb 1,1,2", "golden", "silver")
MOMENTS = ("inertia |w|^2", "Re w", "Im w", "Re w^2", "Im w^2", "coherence", "angular momentum")


def show(value, places=6):
    """An exact rational in decimal, truncated toward zero, for the page alone."""
    sign = "-" if value < 0 else ""
    whole, rest = divmod(abs(value.numerator) * 10 ** places // value.denominator, 10 ** places)
    return "%s%d.%0*d" % (sign, whole, places, rest)


def listed(binary, constants, nu):
    """Cell nu at 16 points a zero or more: each point's w, two integers at 2^-62, and theta / pi less its bound."""
    p = math.ceil(math.log2(16 * tm.rises(nu)))
    cell, _ = tm.run_cell(binary, constants, nu, p, "transform", listing=3)
    ws, lows = [], []
    with open(cell.path) as handle:
        for line in handle:
            if line.startswith("point"):
                v = [int(x, 16) for x in line.split()[1:]]
                ws.append((v[3], v[4]))
                lows.append(v[5])
    os.remove(cell.path)
    return ws, lows, cell.failed


def metallic_word(n, count):
    """The first `count` letters of the Sturmian word of slope 1 / beta, 1 for the long gap."""
    d = n * n + 4

    def floor_k(k):
        return (math.isqrt(k * k * d) - k * n) // 2
    return [floor_k(k + 1) - floor_k(k) for k in range(count)]


class Carrier:
    """A carrier's marks as a reached test: `reached(x, letters)` is 1 where x, theta / pi past the origin at 2^-62
    times the rate's denominator over its numerator, stands at or past the mark after `letters`."""

    def __init__(self, name, rate, span):
        self.name, self.n = name, {"golden": 1, "silver": 2}.get(name, 0)
        exact = Fraction(str(rate))
        self.num, self.den = exact.numerator, exact.denominator
        count = int(span * rate) * 2 + 8
        if self.n:
            self.letters = metallic_word(self.n, count)
        elif name == "comb 1,1,2":
            self.letters = [(0, 0, 1)[k % 3] for k in range(count)]
        else:
            self.letters = [0] * count

    def reached(self, x, longs, shorts, shift):
        """1 where x, theta / pi past the origin as an integer at 2^-62, stands at or past the mark after `longs` long
        and `shorts` short steps, the whole carrier moved on by `shift`, a Fraction of its mean step; in integers."""
        num, den, sn, sd = self.num, self.den, shift.numerator, shift.denominator
        if self.n == 0:
            # the combs: a long step is two short ones, and the mean step is 1 / rate, the short `size` / `total`
            size, total = len(self.letters), len(self.letters) + sum(self.letters)
            return int(x * num * total * sd >= ONE * den * ((2 * longs + shorts) * size * sd + sn * total))
        # the metallic words: steps short and beta short, the mean step 1 / rate with short = 1 / (rate (2 - omega));
        # q (4 + n - sqrt d) / 2 >= longs (n + sqrt d) / 2 + shorts is A >= sqrt(d) B, everything times ONE den sd
        n, d = self.n, self.n * self.n + 4
        scale = ONE * den * sd
        q = x * num * sd - sn * ONE * den
        a = q * (4 + n) - scale * (2 * shorts + longs * n)
        b = q + scale * longs
        return int(a >= 0 and a * a >= d * b * b)

    def guess(self, longs, shorts, shift):
        """Where the mark stands, in units of theta / pi, near enough to start the search; the verdicts place it."""
        if self.n == 0:
            size, total = len(self.letters), len(self.letters) + sum(self.letters)
            return ((2 * longs + shorts) * size / total + float(shift)) * self.den / self.num
        beta = (self.n + math.sqrt(self.n * self.n + 4)) / 2
        return ((longs * beta + shorts) / (2 - 1 / beta) + float(shift)) * self.den / self.num

    def points(self, lows, letters, shift):
        """The listed points the carrier reads: for each mark in turn, the first point whose theta / pi less its bound
        reaches it, found from a guess by the verdicts alone."""
        origin, out, longs, shorts = lows[0], [], 0, 0
        for k, letter in enumerate([0] + letters):
            longs, shorts = longs + letter * int(k > 0), shorts + (1 - letter) * int(k > 0)
            j = bisect.bisect_left(lows, origin + int(self.guess(longs, shorts, shift) * ONE))
            while j > 0 and self.reached(lows[j - 1] - origin, longs, shorts, shift):
                j -= 1
            while j < len(lows) and not self.reached(lows[j] - origin, longs, shorts, shift):
                j += 1
            if j == len(lows):
                break
            out.append(j)
        return out


def moments(ws, idx):
    """The exact sums of the moments over the points `idx`, and their count."""
    sums = [0] * len(MOMENTS)
    for at, j in enumerate(idx):
        re, im = ws[j]
        sums[0] += re * re + im * im
        sums[1] += re
        sums[2] += im
        sums[3] += re * re - im * im
        sums[4] += 2 * re * im
        if at + 1 < len(idx):
            re2, im2 = ws[idx[at + 1]]
            sums[5] += re * re2 + im * im2
            sums[6] += re * im2 - im * re2
    return sums, len(idx)


def scaled(sums, count):
    """Each moment over its count in units of w: the squares over 2^124, the first powers over 2^62."""
    powers = (2, 1, 1, 2, 2, 2, 2)
    return [Fraction(s, count * ONE ** k) for s, k in zip(sums, powers)]


def reach_of(real, draws):
    up = sum(int(d >= real) for d in draws)
    down = sum(int(d <= real) for d in draws)
    return min(up, down), "+" if up < down else "-"


def main():
    binary, first, last = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    rate = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
    sys.stdout.reconfigure(line_buffering=True)
    constants = tm.Constants()
    real = {name: [0] * len(MOMENTS) for name in CARRIERS}
    counts = {name: 0 for name in CARRIERS}
    arms = {(arm, name): [([0] * len(MOMENTS), [0]) for _ in range(DRAWS)] for arm in "ab" for name in CARRIERS}
    rng = random.Random(20261004)
    failed, given_back = 0, True
    for nu in range(first, last + 1):
        ws, lows, bad = listed(binary, constants, nu)
        failed += bad
        span = (lows[-1] - lows[0]) // ONE + 1
        for name in CARRIERS:
            carrier = Carrier(name, rate, span)
            idx = carrier.points(lows, carrier.letters, Fraction(0))
            sums, count = moments(ws, idx)
            real[name] = [a + b for a, b in zip(real[name], sums)]
            counts[name] += count
            for d in range(DRAWS):
                letters = list(carrier.letters)
                rng.shuffle(letters)
                drawn = carrier.points(lows, letters, Fraction(0))
                given_back = given_back and (name != "uniform" or drawn == idx)
                for arm, chosen in (("a", drawn), ("b", carrier.points(lows, carrier.letters, Fraction(d + 1, DRAWS + 1)))):
                    s, c = moments(ws, chosen)
                    held, total = arms[(arm, name)][d]
                    arms[(arm, name)][d] = ([x + y for x, y in zip(held, s)], [total[0] + c])
        print("  cell %d: %d points listed, %s read" % (nu, len(lows), ", ".join(
            "%s %d" % (name, counts[name]) for name in CARRIERS)))
    print("  rate %s marks a unit of theta / pi; the uniform comb's arm a gives it back: %s; host checks failed %d" %
          (rate, given_back, failed))

    values = {name: scaled(real[name], counts[name]) for name in CARRIERS}
    drawn = {key: [scaled(s, c[0]) for s, c in draws] for key, draws in arms.items()}
    print("  each moment over its count, for each carrier: the real; arm a's mean, reach, side; arm b's mean, reach, side")
    for at, moment in enumerate(MOMENTS):
        print("  %s" % moment)
        for name in CARRIERS:
            columns = []
            for arm in "ab":
                draws = [d[at] for d in drawn[(arm, name)]]
                reach, side = reach_of(values[name][at], draws)
                columns.append("%12s %2d %s" % (show(sum(draws, Fraction(0)) / DRAWS), reach, side))
            print("    %-11s %12s   %s" % (name, show(values[name][at]), "   ".join(columns)))
    print("  the beats, golden less each other carrier, against the same draws' beats:")
    for at, moment in enumerate(MOMENTS):
        columns = []
        for other in ("uniform", "comb 1,1,2", "silver"):
            beat = values["golden"][at] - values[other][at]
            reaches = []
            for arm in "ab":
                draws = [g[at] - o[at] for g, o in zip(drawn[(arm, "golden")], drawn[(arm, other)])]
                reach, side = reach_of(beat, draws)
                reaches.append("%s%d%s" % (arm, reach, side))
            columns.append("%s %12s %s" % (other, show(beat), " ".join(reaches)))
        print("    %-17s %s" % (moment, "   ".join(columns)))
    return int(failed != 0 or not given_back)


if __name__ == "__main__":
    sys.exit(main())
