#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-035
#
# The point stage of Turing's method in held form, exact_zeta_held.cu, driven over one cell of the lattice in u,
# t = 2 pi u^4: the remainder's curves C_0 to C_K, the logarithm ln u and theta / pi, one lane a point, each stage
# checked against the host word for word and each output held against the exact value it stands for.
#
#   Usage:  python examples/0_experimental/exact_zeta_held.py <binary> <nu> <b> <E> <W>
#
# It sits in 0_experimental and is an entry in the analytic number theory workbook, on its rail; it claims nothing
# about the Riemann hypothesis.
#
# THE CELL
#
# Lane l stands at u = U / 2^b, U = U_0 + l, U_0 the least U with U^2 >= nu 4^b, over every U with U^2 < (nu + 1) 4^b.
# x = U^2 / 4^b, z = 1 - 2 (x - nu) = Zt / 4^b, Zt = (2 nu + 1) 4^b - 2 U^2, and x^(-1/2) = 2^b / U: each exact.
#
# THE CURVES
#
# C_n(z) is the sum over j of g_(n,j) z^j, each g one of Gabcke's Taylor coefficients of C_n read as a mantissa at the
# exponent -E, the coefficients taken until they read 0 there. K is the curve whose bound is least: Gabcke's thesis,
# Satz 3.2.2, for t >= 200, |R_K(t)| < d_K t^(-(2K + 3) / 4). d_K' / d_K t^(-(K' - K) / 2) falls as t rises, and the
# least at t = 200 is the least at every t past it. The device gives s sum over n of H_n U^(2 (K - n)), every curve at
# one exponent, and U^(2K + 1): R's curves sum to their quotient, an exact rational.
#
# THETA
#
# theta / pi below and above at every lane, from ln u and the held constants at 2^-W: pi by Machin's arctangents,
# whose alternating partial sums stand on either side of each, and ln(U_0 / 2^b) by artanh, its rest bounded. A's
# series takes the fewest terms whose rest falls below 2^-(W+4) at the cell's last lane.
#
# Checks: the device's integers are the exact values of Horner's rule over the coefficients given, at every lane;
# R against exact_zeta_riemann_siegel's remainder_at through C_K at x, the gap written out; the bracket on A against
# zz's exact 2 artanh(l / D); the house's theta / pi between the device's below and above.

import array
import math
import os
import subprocess
import sys
import tempfile
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import exact_zeta_riemann_siegel as rs  # noqa: E402
import exact_zeta_zeros as zz  # noqa: E402

CHECKED = 64
# Gabcke, Satz 3.2.2, for t >= 200: |R_K(t)| < d_K t^(-(2K + 3) / 4), by (b) for K <= 9 and by (a) for K = 10
GABCKE = (Fraction(127, 1000), Fraction(53, 1000), Fraction(11, 1000), Fraction(31, 1000), Fraction(17, 1000),
          Fraction(61, 1000), Fraction(661, 1000), Fraction(92, 10), Fraction(130), Fraction(1837), Fraction(25966))
TOP = min(range(len(GABCKE)), key=lambda k: GABCKE[k] ** 4 / Fraction(200) ** (2 * k + 3))


def digits_of(bits):
    """The decimal places 2^-bits takes."""
    return bits * 30103 // 100000 + 1


def toward_zero(a, b):
    quotient = abs(a) // abs(b)
    return quotient if (a >= 0) == (b > 0) else -quotient


def cell(nu, b):
    """U_0 and the count of lanes: every U with nu 4^b <= U^2 < (nu + 1) 4^b."""
    first = math.isqrt(nu * 4 ** b - 1) + 1
    last = math.isqrt((nu + 1) * 4 ** b - 1) + 1
    return first, last - first


def curve_coefficients(big_e):
    """Gabcke's Taylor coefficients of C_0 through C_K as mantissas at the exponent -E, each curve's taken until the
    last eight read 0 there, the zeros past its last nonzero one dropped."""
    out = []
    for n in range(TOP + 1):
        count = 16
        while True:
            digits = big_e * 30103 // 100000 + count + 3 * n + 20
            raw = rs.Curve(digits).gamma(n, count)
            binary = [toward_zero(g << big_e, 10 ** digits) for g in raw]
            if not any(binary[-8:]):
                break
            count *= 2
        last = max([j for j, g in enumerate(binary) if g != 0], default=0)
        out.append(binary[: last + 1])
    return out


def log_terms(first, lanes, w):
    """The fewest terms of A's series whose rest, 2 l^(2L+1) / ((2L+1) D^(2L-1) (D^2 - l^2)), falls below 2^-(W+4) at
    the cell's last lane, where l / D is largest."""
    lane, big_d, terms = lanes - 1, 2 * first + lanes - 1, 2
    while Fraction(2 * lane ** (2 * terms + 1), (2 * terms + 1) * big_d ** (2 * terms - 1) * (big_d ** 2 - lane ** 2)) \
            >= Fraction(1, 2 ** (w + 4)):
        terms += 1
    return terms


def log_constants(terms):
    """Lambda, the least common multiple of the odd numbers below 2L, and c_k = Lambda / (2k + 1) for k < L."""
    big_lambda = 1
    for odd in range(1, 2 * terms, 2):
        big_lambda = math.lcm(big_lambda, odd)
    return big_lambda, [big_lambda // (2 * k + 1) for k in range(terms)]


def arctan_bracket(k, bits):
    """arctan(1 / k), below and above: its alternating partial sums, taken until a term falls below 2^-bits, stand on
    either side of it."""
    total, j, sums = Fraction(0), 0, []
    while True:
        term = Fraction(1, (2 * j + 1) * k ** (2 * j + 1))
        total += term if j % 2 == 0 else -term
        sums.append(total)
        j += 1
        if term < Fraction(1, 2 ** bits) and len(sums) >= 2:
            return min(sums[-2:]), max(sums[-2:])


def pi_bracket(bits):
    """pi = 16 arctan(1/5) - 4 arctan(1/239), below and above."""
    five, many = arctan_bracket(5, bits + 6), arctan_bracket(239, bits + 6)
    return 16 * five[0] - 4 * many[1], 16 * five[1] - 4 * many[0]


def artanh_bracket(y, bits):
    """artanh(y), 0 <= y <= 1/3, below and above: the partial sum of y^(2k+1) / (2k+1), every term positive, and the
    rest past it below y^(2L+1) / ((2L+1) (1 - y^2))."""
    total, k = Fraction(0), 0
    while True:
        total += y ** (2 * k + 1) / (2 * k + 1)
        k += 1
        rest = y ** (2 * k + 1) / ((2 * k + 1) * (1 - y * y))
        if rest < Fraction(1, 2 ** bits):
            return total, total + rest


def ln_bracket(p, q, bits):
    """ln(p / q) for p >= q > 0, below and above: ln(p / (q 2^m)) + m ln 2, p / (q 2^m) in [1, 2), each by artanh of
    an argument at most 1/3."""
    m = (p // q).bit_length() - 1
    low, high = artanh_bracket(Fraction(p - q * 2 ** m, p + q * 2 ** m), bits + 4)
    two_low, two_high = artanh_bracket(Fraction(1, 3), bits + 4 + m.bit_length())
    return 2 * low + 2 * m * two_low, 2 * high + 2 * m * two_high


def theta_constants(first, b, w, terms):
    """theta's held constants as integers at 2^-W, each below and above: ln(U_0 / 2^b), 1 / (96 pi^2),
    7 / (46080 pi^4) and 31 / (2580480 pi^6); then the bound past Gabcke's last term, |R_theta| < 1 / (3322 t^7) for
    t >= 10 (his thesis, introduction, (5)), over pi at t = 2 pi u^4: 1 / (425216 pi^8 u^28), its constant above; then
    Lambda and 2L + 1."""
    pi_low, pi_high = pi_bracket(w + 16)
    unit = 2 ** w
    log_low, log_high = ln_bracket(first, 2 ** b, w + 4)
    out = [math.floor(log_low * unit), math.ceil(log_high * unit)]
    for num, den, power in ((1, 96, 2), (7, 46080, 4), (31, 2580480, 6)):
        out += [math.floor(Fraction(num, den) / pi_high ** power * unit),
                math.ceil(Fraction(num, den) / pi_low ** power * unit)]
    out.append(math.ceil(Fraction(1, 425216) / pi_low ** 8 * unit))
    big_lambda, _ = log_constants(terms)
    return out + [big_lambda, 2 * terms + 1]


def put_word(handle, value):
    array.array("q", (value,)).tofile(handle)


def put_mantissa(handle, value):
    magnitude = abs(value)
    limbs = [(magnitude >> (32 * i)) & 0xFFFFFFFF for i in range((magnitude.bit_length() + 31) // 32)]
    array.array("q", (len(limbs),)).tofile(handle)
    if limbs:
        array.array("I", limbs).tofile(handle)
    array.array("q", ((value > 0) - (value < 0),)).tofile(handle)


def run(binary, nu, b, big_e, gammas, cs, w):
    first, lanes = cell(nu, b)
    folder = tempfile.mkdtemp(prefix="held_")
    given, taken = os.path.join(folder, "in.bin"), os.path.join(folder, "out.txt")
    with open(given, "wb") as handle:
        for word in (lanes, min(CHECKED, lanes), nu, b, TOP, big_e, len(cs), w):
            put_word(handle, word)
        put_mantissa(handle, first)
        put_mantissa(handle, (2 * nu + 1) * 4 ** b)
        for row in gammas:
            put_word(handle, len(row))
        for row in gammas:
            for g in row:
                put_mantissa(handle, g)
        for c in cs:
            put_mantissa(handle, c)
        for v in theta_constants(first, b, w, len(cs)):
            put_mantissa(handle, v)
    ran = subprocess.run([binary, given, taken], capture_output=True, text=True)
    if ran.returncode:
        print(ran.stdout.strip()[-2000:])
        print(ran.stderr.strip()[-2000:])
        raise SystemExit("  the device run failed")
    stages = parse(taken, lanes)
    os.remove(taken)
    return stages, first, lanes


def parse(path, lanes):
    lines = open(path).read().splitlines()
    stages, at = {}, 0
    while at < len(lines):
        head = lines[at].split()
        if head and head[0] == "stage":
            name, out_limbs, at = head[1], int(head[3]), at + 1
            outputs = []
            while lines[at].split()[0] == "output":
                _, field, offset, bits, exponent = lines[at].split()
                outputs.append((field, int(offset), int(bits), int(exponent)))
                at += 1
            rows = []
            for _ in range(lanes):
                rows.append(sum(int(x, 16) << (32 * i) for i, x in enumerate(lines[at].split()[:out_limbs])))
                at += 1
            stages[name] = {"outputs": outputs, "rows": rows, "host": int(lines[at].split()[1])}
            at += 2
        else:
            at += 1
    return stages


def read_lane(stage, lane):
    """Each output of a lane as (mantissa, exponent)."""
    word = stage["rows"][lane]
    out = {}
    for name, offset, bits, exponent in stage["outputs"]:
        value = (word >> offset) & ((1 << bits) - 1)
        out[name] = (value - (1 << bits) if value >> (bits - 1) else value, exponent)
    return out


def as_fraction(mantissa, exponent):
    return Fraction(mantissa * 2 ** exponent) if exponent >= 0 else Fraction(mantissa, 2 ** -exponent)


def remainder(got):
    """R's curves summed, the exact rational the lane's outputs give."""
    return as_fraction(*got["remainder"]) / as_fraction(*got["denominator"])


def horner(nu, b, big_u, gammas, big_e):
    """The same sum from the coefficients given, in exact rationals: s sum over n of C~_n(z) x^(-n - 1/2)."""
    sign = 1 - 2 * ((nu - 1) % 2)
    z = Fraction((2 * nu + 1) * 4 ** b - 2 * big_u * big_u, 4 ** b)
    total = Fraction(0)
    for n, row in enumerate(gammas):
        acc = Fraction(0)
        for g in reversed(row):
            acc = acc * z + Fraction(g, 2 ** big_e)
        total += sign * acc * Fraction(2 ** (b * (2 * n + 1)), big_u ** (2 * n + 1))
    return total


def main():
    if len(sys.argv) != 6:
        raise SystemExit("  usage: exact_zeta_held.py <binary> <nu> <b> <E> <W>")
    binary, nu, b, big_e, w = sys.argv[1], *(int(v) for v in sys.argv[2:6])
    terms = log_terms(*cell(nu, b), w)
    if nu < 2:
        raise SystemExit("  Gabcke's bound on theta holds from t = 10, and cell %d reaches below it" % nu)
    gammas = curve_coefficients(big_e)
    big_lambda, cs = log_constants(terms)
    stages, first, lanes = run(binary, nu, b, big_e, gammas, cs, w)
    print("  cell %d: U from %d, %d lanes at 2^-%d; C_0 to C_%d, the coefficients at 2^-%d, %s a curve; ln u by %d terms" %
          (nu, first, lanes, b, TOP, big_e, ", ".join(str(len(row)) for row in gammas), terms))
    host = all(stages[name]["host"] == 1 for name in ("curves", "log", "theta_lower", "theta_upper"))
    exact, worst, held, widest = True, Fraction(0), True, Fraction(0)
    inside, theta_width = True, Fraction(0)
    work = digits_of(w) + 20
    pi_work = rs.zz.pi(work)
    digits = big_e * 30103 // 100000
    sample = sorted(set(range(0, lanes, max(1, lanes // 32))) | {lanes - 1})
    for lane in range(lanes):
        big_u = first + lane
        got = read_lane(stages["curves"], lane)
        device = remainder(got)
        exact = exact and device == horner(nu, b, big_u, gammas, big_e)
        if lane in sample:
            places = 2 * b
            p = (big_u * big_u * 5 ** (2 * b) - nu * 10 ** places, places)
            house = Fraction(rs.remainder_at(nu, p, TOP, digits), 10 ** (digits + rs.GUARD))
            worst = max(worst, abs(device - house))
        log = read_lane(stages["log"], lane)
        if lane:
            big_d = 2 * first + lane
            lower = Fraction(2 * log["numerator"][0], big_lambda * log["power"][0])
            tail = Fraction(2 * log["tail"][0], (2 * terms + 1) * log["power"][0] * log["gap"][0])
            scale, slack = 10 ** (digits + 60), Fraction(1, 10 ** (digits + 50))
            artanh = Fraction(2 * zz._artanh(lane, big_d, scale), scale)
            held = held and lower - slack <= artanh <= lower + tail + slack
            widest = max(widest, tail)
        if lane in sample:
            low, high = read_lane(stages["theta_lower"], lane), read_lane(stages["theta_upper"], lane)
            below = as_fraction(*low["lower"]) / as_fraction(*low["denominator"])
            above = as_fraction(*high["upper"]) / as_fraction(*high["denominator"])
            t = (2 * big_u ** 4 * pi_work // 2 ** (4 * b), work)
            house = Fraction(rs.theta_at(t, work)[0], pi_work)
            spare = Fraction(1, 10 ** (work - 12))
            inside = inside and below - spare <= house <= above + spare
            theta_width = max(theta_width, above - below)
    print("  the host's records %s the device's word for word" % ("equal" if host else "differ from"))
    print("  every lane's curves are Horner's rule over the coefficients given, exactly: %s" % exact)
    print("  R against remainder_at through C_%d at %d lanes: %.3e apart at most" % (TOP, len(sample), float(worst)))
    print("  every bracket on A holds zz's 2 artanh(l / D): %s, the rest at most %.3e" % (held, float(widest)))
    print("  theta / pi held below and above at %d lanes, the house's theta / pi between at each: %s, the widest %.3e"
          % (len(sample), inside, float(theta_width)))
    return 0 if host and exact and held and inside else 1


if __name__ == "__main__":
    sys.exit(main())
