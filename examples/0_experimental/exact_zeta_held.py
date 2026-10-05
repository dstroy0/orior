#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-035
#
# Turing's method in held form, exact_zeta_held.cu, driven over one cell of the lattice in u, t = 2 pi u^4: the
# remainder's curves C_0 to C_K, the logarithm ln u, theta / pi, the main sum, Z's sign at every point and the sign
# changes, and N by Turing's method at a quarter and three quarters of the way across the cell, each stage checked
# against the host word for word and each output held against the exact value it stands for. Gabcke's bound on R_K
# holds from t = 200, and the cell's nu is 6 or more; Trudgian's bound on the integral of S holds past t = 168 pi, and
# the count closes from nu = 10.
#
#   Usage:  python examples/0_experimental/exact_zeta_held.py <binary> <nu> <b> <E> <W>
#
# It sits in 0_experimental and is an entry in the analytic number theory workbook, on its rail; it claims nothing
# about the Riemann hypothesis.
#
# THE CELL
#
# Lane l stands at u = U / 2^b, U = U_0 + l, U_0 the least U with U^2 >= nu 4^b, over every U with U^2 < (nu + 1) 4^b.
# x = U^2 / 4^b, z = 1 - 2 (x - nu) = Zt / 4^b, Zt = 4^b - 2 (U^2 - nu 4^b), and x^(-1/2) = 2^b / U: each exact.
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
# THE MAIN SUM AND THE VERDICT
#
# Each pole n to nu holds ln n and n^(-1/2) below and above at 2^-W, and cos's series in (pi m 2^-W)^2 its first J
# coefficients pi^(2j) / (2j)! below at 2^-W, with B bounding the rest and the coefficients' reads together. Z's
# bracket is the device's doubled sums and R, with Gabcke's bound d_K t^(-(2K + 3) / 4) at t = 2 pi nu^2 and the
# curves' coefficient reads on either side.
#
# Checks: the device's integers are the exact values of Horner's rule over the coefficients given, at every lane;
# R against exact_zeta_riemann_siegel's remainder_at through C_K at x, the gap written out; the bracket on A against
# zz's exact 2 artanh(l / D); the house's theta / pi between the device's below and above; the house's main sum and Z
# through C_K between the device's below and above; the device's sign at every point the one its bracket gives; and
# the device's count of sign changes the host's over the same signs; the clock's theta / pi below and above about the
# house's. Where the sign changes between the two points number N's difference there, every zero between them is
# on the line and simple.

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


def cos_constants(w):
    """cos(pi m 2^-W) by its first J terms, k_j = pi^(2j) / (2j)!: J the fewest whose rest, below the first term left
    out, (pi / 2)^(2J) / (2J)! at m 2^-W <= 1/2, falls below 2^-W; each k_j below at 2^-W, K_j; and B, above at 2^-W
    the rest and every read k_j - K_j 2^-W times (1/4)^j, y 2^-2W at most 1/4."""
    pi_low, pi_high = pi_bracket(w + 16)
    unit, terms = 2 ** w, 1
    while (pi_high / 2) ** (2 * terms) / math.factorial(2 * terms) >= Fraction(1, unit):
        terms += 1
    low = [math.floor(pi_low ** (2 * j) / math.factorial(2 * j) * unit) for j in range(terms)]
    reads = sum((pi_high ** (2 * j) / math.factorial(2 * j) - Fraction(k, unit)) / 4 ** j for j, k in enumerate(low))
    bound = math.ceil(((pi_high / 2) ** (2 * terms) / math.factorial(2 * terms) + reads) * unit)
    return terms, bound, low


def pole_constants(nu, w):
    """Each pole n to nu: ln n below and above and n^(-1/2) below and above, at 2^-W."""
    unit, out = 2 ** w, []
    for n in range(1, nu + 1):
        low, high = ln_bracket(n, 1, w + 4)
        root = math.isqrt(unit * unit // n)
        out += [math.floor(low * unit), math.ceil(high * unit), root, root + 1]
    return out


def root_ceiling(value, k):
    """The least integer r with r^k >= value, for value >= 0."""
    r = max(0, int(round(float(value) ** (1.0 / k))) - 2)
    while Fraction(r) ** k < value:
        r += 1
    return r


def verdict_bound(nu, w, big_e, gammas):
    """B, the verdict's bound above at 2^-W: G = d_K t^(-(2K + 3) / 4) at most d_K (2 pi nu^2)^(-(2K + 3) / 4) over
    the cell, the least integer whose fourth power holds d_K^4 2^(4W) / (2 pi nu^2)^(2K + 3) with pi below; and E_r,
    the curves' coefficient reads: each coefficient held, and each of the eight past the last that read 0, within
    2 2^-E of Gabcke's, |z| <= 1 and x^(-n - 1/2) <= nu^-n / isqrt(nu)."""
    pi_low, _ = pi_bracket(w + 16)
    gabcke = root_ceiling(GABCKE[TOP] ** 4 * Fraction(2 ** (4 * w)) / (2 * pi_low * nu * nu) ** (2 * TOP + 3), 4)
    reads = sum(Fraction(2 * (len(row) + 8), 2 ** big_e * nu ** n * math.isqrt(nu)) for n, row in enumerate(gammas))
    return gabcke + math.ceil(reads * 2 ** w)


def turing_count(stages, first, lanes, b, w):
    """N at points a = P / 4 and e = 3P / 4 below and above, by Turing's method over the windows below a and past e,
    and the changes between them. N(t) = theta / pi + 1 + S(t) off the ordinates, and for t_2 > t_1 > 168 pi,
    |integral of S from t_1 to t_2| <= 2.067 + 0.059 ln t_2 (Trudgian, Improvements to Turing's method, Theorem 2.2).
    theta rises, and on a step it lies between its ends; c(t), the changes in (T, t], is at least those up to the
    step's start, and k(t), those in (t, T], at least those from its end. Over a window of h = 2 pi D, D its x^2:
    N(T) >= 1 + (sum of d_i Theta_i below + sum of d_i k_(i + 1)) / D - B / (2 pi D) below T, and
    N(T) <= 1 + (sum of d_i Theta_(i + 1) above - sum of d_i c_i) / D + B / (2 pi D) past it. Every sum is the
    device's."""
    sums = {name: as_fraction(values[0], exponent) for name, (values, exponent) in stages["turing"]["sums"].items()}
    a, e, last = lanes // 4, 3 * lanes // 4, lanes - 1
    pi_low, pi_high = pi_bracket(64)

    def x2(lane):
        return Fraction((first + lane) ** 4, 16 ** b)

    def trudgian(lane):
        t = 2 * pi_high * x2(lane)
        return (Fraction(2067, 1000) + Fraction(59, 1000) * ln_bracket(t.numerator, t.denominator, 32)[1]) / (2 * pi_low)

    below_d, past_d = x2(a) - x2(0), x2(last) - x2(e)
    low = 1 + (sums["below_theta"] + sums["below_zeros"] - x2(0) * sums["below_count"]) / below_d \
        - trudgian(a) / below_d
    high = 1 + (sums["past_theta"] - (x2(last) * sums["past_count"] - sums["past_zeros"])) / past_d \
        + trudgian(last) / past_d
    return a, e, low, high, int(sums["between_count"])


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
    cos_terms, bound, k_low = cos_constants(w)
    with open(given, "wb") as handle:
        for word in (lanes, min(CHECKED, lanes), nu, b, TOP, big_e, len(cs), w, cos_terms):
            put_word(handle, word)
        put_mantissa(handle, first)
        for row in gammas:
            put_word(handle, len(row))
        for row in gammas:
            for g in row:
                put_mantissa(handle, g)
        for c in cs:
            put_mantissa(handle, c)
        for v in theta_constants(first, b, w, len(cs)):
            put_mantissa(handle, v)
        for v in [bound] + k_low + pole_constants(nu, w) + [verdict_bound(nu, w, big_e, gammas)]:
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
        elif head and head[0] == "sum":
            name, field, exponent, limbs, runs = head[1], head[2], int(head[3]), int(head[4]), int(head[6])
            values = []
            for row in lines[at + 1: at + 1 + runs]:
                word = sum(int(x, 16) << (32 * i) for i, x in enumerate(row.split()))
                values.append(word - (1 << (32 * limbs)) if word >> (32 * limbs - 1) else word)
            stage = stages.setdefault(name, {"host": 1})
            stage.setdefault("sums", {})[field] = (values, exponent)
            stage["host"] = stage["host"] and int(head[8])
            at += 1 + runs
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
    if nu < 6:
        raise SystemExit("  Gabcke's bound on R_K holds from t = 200, and cell %d reaches below it" % nu)
    gammas = curve_coefficients(big_e)
    big_lambda, cs = log_constants(terms)
    stages, first, lanes = run(binary, nu, b, big_e, gammas, cs, w)
    print("  cell %d: U from %d, %d lanes at 2^-%d; C_0 to C_%d, the coefficients at 2^-%d, %s a curve; ln u by %d terms" %
          (nu, first, lanes, b, TOP, big_e, ", ".join(str(len(row)) for row in gammas), terms))
    host = all(stages[name]["host"] == 1 for name in ("curves", "log", "theta_lower", "theta_upper",
                                                "term_lower", "term_upper", "verdict", "change", "clock", "turing"))
    exact, worst, held, widest = True, Fraction(0), True, Fraction(0)
    inside, theta_width = True, Fraction(0)
    main_inside, main_width = True, Fraction(0)
    z_inside, z_width = True, Fraction(0)
    clocked = True
    ends = {}
    sums = {**stages["term_lower"]["sums"], **stages["term_upper"]["sums"]}
    # Z below and above from the device's own sums and remainder, and B
    slack = Fraction(verdict_bound(nu, w, big_e, gammas), 2 ** w)
    rests = [remainder(read_lane(stages["curves"], lane)) for lane in range(lanes)]
    z_low = [2 * as_fraction(sums["lower"][0][lane], sums["lower"][1]) + rests[lane] - slack for lane in range(lanes)]
    z_high = [2 * as_fraction(sums["upper"][0][lane], sums["upper"][1]) + rests[lane] + slack for lane in range(lanes)]
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
            clock = read_lane(stages["clock"], lane)
            clocked = clocked and as_fraction(*clock["low"]) - spare <= house <= as_fraction(*clock["high"]) + spare
            if lane in (0, lanes - 1):
                ends[lane] = (below, above)
            low_sum = 2 * as_fraction(sums["lower"][0][lane], sums["lower"][1])
            high_sum = 2 * as_fraction(sums["upper"][0][lane], sums["upper"][1])
            house_main, house_r = (Fraction(v, 10 ** work) for v in rs.rs_cut_at((nu, p, work), TOP))
            main_inside = main_inside and low_sum - spare <= house_main <= high_sum + spare
            main_width = max(main_width, high_sum - low_sum)
            z_inside = z_inside and z_low[lane] - spare <= house_main + house_r <= z_high[lane] + spare
            z_width = max(z_width, z_high[lane] - z_low[lane])
    signs = [read_lane(stages["verdict"], lane)["sign"][0] for lane in range(lanes)]
    agree = signs == [1 if low > 0 else -1 if high < 0 else 0 for low, high in zip(z_low, z_high)]
    changes = sum(1 for a, c in zip(signs, signs[1:]) if a * c < 0)
    device_changes = stages["change"]["sums"]["change"][0][0]
    print("  the host's records %s the device's word for word" % ("equal" if host else "differ from"))
    print("  every lane's curves are Horner's rule over the coefficients given, exactly: %s" % exact)
    print("  R against remainder_at through C_%d at %d lanes: %.3e apart at most" % (TOP, len(sample), float(worst)))
    print("  every bracket on A holds zz's 2 artanh(l / D): %s, the rest at most %.3e" % (held, float(widest)))
    print("  theta / pi held below and above at %d lanes, the house's theta / pi between at each: %s, the widest %.3e"
          % (len(sample), inside, float(theta_width)))
    print("  2 sum over n to nu of n^(-1/2) cos(pi phi_n) held below and above, the house's main sum between at each: "
          "%s, the widest %.3e" % (main_inside, float(main_width)))
    print("  Z held below and above with Gabcke's bound on R_%d and the coefficient reads, the house's Z through C_%d "
          "between at each: %s, the widest %.3e" % (TOP, TOP, z_inside, float(z_width)))
    print("  the device's sign at every point is the bracket's: %s; decided at %d of %d points; %d sign changes between "
          "neighbors, the device's count %d" % (agree, sum(1 for s in signs if s), lanes, changes, device_changes))
    print("  theta / pi advances by %.6f from the first point to the last" % float(ends[lanes - 1][0] - ends[0][1]))
    a, e, low, high, between = turing_count(stages, first, lanes, b, w)
    held_n = (math.ceil(low), math.floor(high))
    print("  the clock's theta / pi at 2^-%d below and above holds the house's at every sampled lane: %s" % (w, clocked))
    print("  Turing's method: N at point %d at least %.4f and whole, %d; N at point %d at most %.4f and whole, %d" %
          (a, float(low), held_n[0], e, float(high), held_n[1]))
    signed = signs[a] != 0 and signs[e] != 0
    closed = signed and nu >= 10 and between == held_n[1] - held_n[0]
    print("  %d sign changes between them, %d zeros there: %s" %
          (between, held_n[1] - held_n[0],
           "every zero in (t_%d, t_%d] is on the line and simple" % (a, e) if closed else
           "the count does not close" if nu >= 10 else "Trudgian's bound holds past t = 168 pi, and the cell starts below it"))
    whole = main_inside and z_inside and agree and changes == device_changes and clocked and (closed or nu < 10)
    return 0 if host and exact and held and inside and whole else 1


if __name__ == "__main__":
    sys.exit(main())
