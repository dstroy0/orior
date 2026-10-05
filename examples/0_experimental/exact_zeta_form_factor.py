#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-032
#
# The form factor of the zeros Turing's machine certifies, on u = theta / pi + 1, the clock whose ticks are one zero
# apart on average. The zeros are placed as exact_zeta_tension.py places them, on the device, and number N's
# difference between two F's. S = N - u is the difference between the zeros' clock and theta's, and the form factor
# is S's spectrum: with D(u) = n(u) - (u - a) over [a, b] about the zeros,
#
#   sum over zeros of e(tau u_n) = the integral of e(tau u) over [a, b] + D(b) e(tau b) - 2 pi i tau (the integral of D e(tau u)),
#
# e(x) = exp(2 pi i x), and K(tau) = |sum|^2 / M is (2 pi tau)^2 times S's power over the window, the ends aside. Both
# sides are taken at single tau, D's integral exact over each line between zeros.
#
#   Usage:  python examples/0_experimental/exact_zeta_form_factor.py <binary> <first> <last> [extra] [N at first's F N at last's F]
#
# K is read smoothed by a Gaussian of width sigma in tau, from every pair of zeros within D of each other:
# K_sigma(tau) = 1 + (2 / M) the sum over pairs of exp(-2 pi^2 sigma^2 d^2) cos(2 pi tau d) - G_sigma(tau), the last
# term the mean density's. Against it:
# - GUE's min(|tau|, 1), smoothed the same;
# - the prime powers, the diagonal of the explicit formula: a spike of weight Lambda(x)^2 / (x L^2) at
#   tau = ln x / L, L = ln(t / 2 pi), for every prime power x up to t / 2 pi; their weights below tau sum to tau^2 / 2,
#   which is GUE's ramp, and none falls below ln 2 / L;
# - the same spacings shuffled, every gap kept and their order dropped;
# - each half of the zeros alone, its L a little apart from the other's.
#
# The statistics are the host's, in floating point, over the device's Z. It sits in 0_experimental and is an entry
# in the analytic number theory workbook, on its rail. It proves nothing; it reads how the zeros hold to one another.

import bisect
import cmath
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import exact_zeta_tension as zt  # noqa: E402

# the pairs' differences binned at 2^-WIDTH, out to REACH spacings for the narrow width and REACH_WIDE for the wide
WIDTH = 10
NARROW, WIDE = 0.01, 0.05
REACH, REACH_WIDE = 100.0, 20.0


def two_routes(u, tau):
    """sum over the zeros of e(tau u_n), and the same from D, the difference of the two clocks."""
    w = 2 * math.pi * tau
    zeros = sum(cmath.exp(1j * w * x) for x in u)
    a, b, m = u[0] - 0.5, u[-1] + 0.5, len(u)

    def line(c, x):
        z = cmath.exp(1j * w * x)
        return (c - x) * z / (1j * w) - z / (w * w)

    ends = [a] + u + [b]
    integral = sum(line(k + a, ends[k + 1]) - line(k + a, ends[k]) for k in range(m + 1))
    plain = (cmath.exp(1j * w * b) - cmath.exp(1j * w * a)) / (1j * w)
    clocks = plain + (m - (b - a)) * cmath.exp(1j * w * b) - 1j * w * integral
    return zeros, clocks


def pairs(u, reach, low, high):
    """The pairs' differences up to reach, binned, over the zeros low to high whose reach stays inside u."""
    bins = [0] * (int(reach * (1 << WIDTH)) + 1)
    counted = 0
    for n in range(low, high):
        if u[n] + reach > u[-1]:
            break
        counted += 1
        for j in range(n + 1, bisect.bisect_right(u, u[n] + reach)):
            bins[int((u[j] - u[n]) * (1 << WIDTH))] += 1
    return bins, counted


def smoothed(binned, sigma, taus):
    bins, counted = binned
    a = 2 * math.pi ** 2 * sigma ** 2
    held = [((b + 0.5) / (1 << WIDTH), count * math.exp(-a * ((b + 0.5) / (1 << WIDTH)) ** 2))
            for b, count in enumerate(bins) if count]
    out = []
    for tau in taus:
        w = 2 * math.pi * tau
        total = sum(weight * math.cos(w * d) for d, weight in held)
        out.append(1 + 2 * total / counted - gauss(sigma, tau))
    return out


def gauss(sigma, x):
    return math.exp(-0.5 * (x / sigma) ** 2) / (sigma * math.sqrt(2 * math.pi))


def gue(sigma, tau):
    """min(|tau|, 1) smoothed by the Gaussian, by Simpson's rule over 8 sigma about tau."""
    steps = 400
    h = 16 * sigma / steps
    total = 0.0
    for k in range(steps + 1):
        x = tau - 8 * sigma + k * h
        f = min(abs(x), 1.0) * gauss(sigma, x - tau)
        total += f * (1 if k in (0, steps) else 4 if k % 2 else 2)
    return total * h / 3


def primes_model(sigma, taus, length):
    """The prime powers' spikes up to e^L, smoothed."""
    spikes = [(math.log(p) ** 2 / (p ** r * length ** 2), r * math.log(p) / length)
              for p, r in zt.prime_powers(int(math.exp(length)))]
    return [sum(weight * (gauss(sigma, tau - at) + gauss(sigma, tau + at)) for weight, at in spikes
                if abs(tau - at) < 8 * sigma or abs(tau + at) < 8 * sigma) for tau in taus]


def analyse(zeros):
    u = [zt.theta_pi(s) + 1.0 for s in zeros]
    m = len(u)
    length = math.log(0.5 * (zeros[0] + zeros[-1]))
    print("  %d zeros, ln(t / 2 pi) %.3f; ln 2 / L = %.4f, the prime powers to %d" %
          (m, length, math.log(2) / length, int(math.exp(length))))

    worst = 0.0
    for tau in (0.03, 0.0606, 0.1, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5):
        zeros_sum, clocks = two_routes(u, tau)
        worst = max(worst, abs(zeros_sum - clocks) / abs(zeros_sum))
    print("  the zeros' sum against D's, at 10 tau from 0.03 to 2.5: they differ by %.1e of the sum at most" % worst)

    random.seed(20261004)
    gaps = [u[k + 1] - u[k] for k in range(m - 1)]
    random.shuffle(gaps)
    shuffled = [u[0]]
    for gap in gaps:
        shuffled.append(shuffled[-1] + gap)

    narrow = [k / 200 for k in range(0, 81)] + [k / 100 for k in range(41, 96)]
    whole = pairs(u, REACH, 0, m)
    first, second = pairs(u, REACH, 0, m // 2), pairs(u, REACH, m // 2, m)
    k_all = smoothed(whole, NARROW, narrow)
    k_first, k_second = smoothed(first, NARROW, narrow), smoothed(second, NARROW, narrow)
    k_shuffled = smoothed(pairs(shuffled, REACH, 0, m), NARROW, narrow)
    k_gue = [gue(NARROW, tau) for tau in narrow]
    k_primes = primes_model(NARROW, narrow, length)
    print("  K smoothed by sigma %.2f: tau, the zeros, half against half, the primes' spikes, GUE's, the shuffled spacings'"
          % NARROW)
    for i, tau in enumerate(narrow):
        if tau <= 0.2 or abs(tau * 20 - round(tau * 20)) < 1e-9:
            print("    %.3f  %.4f  %+.4f  %.4f  %.4f  %.4f" %
                  (tau, k_all[i], k_first[i] - k_second[i], k_primes[i], k_gue[i], k_shuffled[i]))
    below = math.log(2) / length - 3 * NARROW
    for low, high in ((0.0, below), (below, 0.3), (0.3, 0.9)):
        band = [i for i, tau in enumerate(narrow) if low <= tau < high]
        rms = lambda model: math.sqrt(sum((k_all[i] - model[i]) ** 2 for i in band) / len(band))  # noqa: E731
        halves = math.sqrt(sum((k_first[i] - k_second[i]) ** 2 for i in band) / len(band))
        print("    tau in [%.3f, %.3f): the zeros' mean %.4f, GUE's %.4f; rms from the primes %.4f, from GUE %.4f; the "
              "halves' difference, rms %.4f" % (low, high, sum(k_all[i] for i in band) / len(band),
                                                 sum(k_gue[i] for i in band) / len(band), rms(k_primes), rms(k_gue),
                                                 halves))

    wide = [k / 20 for k in range(1, 61)]
    k_wide = smoothed(pairs(u, REACH_WIDE, 0, m), WIDE, wide)
    k_wide_shuffled = smoothed(pairs(shuffled, REACH_WIDE, 0, m), WIDE, wide)
    k_wide_gue = [gue(WIDE, tau) for tau in wide]
    print("  K smoothed by sigma %.2f: tau, the zeros, GUE's, the shuffled spacings'" % WIDE)
    for i, tau in enumerate(wide):
        if abs(tau * 10 - round(tau * 10)) < 1e-9:
            print("    %.2f  %.4f  %.4f  %.4f" % (tau, k_wide[i], k_wide_gue[i], k_wide_shuffled[i]))
    for low, high in ((0.2, 0.8), (0.8, 1.2), (1.2, 3.01)):
        band = [i for i, tau in enumerate(wide) if low <= tau < high]
        print("    tau in [%.1f, %.1f]: the zeros less GUE's, mean %+.4f, most %.4f; the shuffled spacings', mean %+.4f" %
              (low, min(high, 3.0), sum(k_wide[i] - k_wide_gue[i] for i in band) / len(band),
               max(abs(k_wide[i] - k_wide_gue[i]) for i in band),
               sum(k_wide_shuffled[i] - k_wide_gue[i] for i in band) / len(band)))
    return 0


def main():
    binary, first, last = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    extra = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    sys.stdout.reconfigure(line_buffering=True)
    given = (int(sys.argv[5]), int(sys.argv[6])) if len(sys.argv) > 6 else None
    zeros, _, _, _ = zt.certified_zeros(binary, first, last, extra, given)
    return analyse(zeros)


if __name__ == "__main__":
    sys.exit(main())
