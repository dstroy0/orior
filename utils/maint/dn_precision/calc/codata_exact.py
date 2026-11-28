#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The CODATA constants that are exact and truncated, carried to a thousand places.
#
# docs-check: quoting
#   Usage:  python utils/maint/dn_precision/calc/codata_exact.py
#           python utils/maint/dn_precision/calc/codata_exact.py --check
#           python utils/maint/dn_precision/calc/codata_exact.py --split    which uncertainties are removable
# docs-check: end quoting
#
# THE SPLIT THAT MAKES THIS WORTH DOING
#
# The 2022 CODATA listing holds 358 constants. Counted from the file itself:
#   - 81 carry the mark (exact).
#   - 19 of those terminate. The published digits ARE the value, and there is nothing to compute.
#   - 62 of those are exact and TRUNCATED, published as "1.054 571 817... e-34".
#   - 277 carry a measured uncertainty.
#
# The 62 are the target. Their decimal expansions do not terminate. NIST stops at about ten digits, and marks the rest
# with an ellipsis. Every one of them is a closed-form expression in the seven constants that define the SI. The absent
# digits are therefore not unknown. They simply do not appear in print.
#
# THE 277 ARE OUT OF REACH, AND THIS FILE SAYS SO. Alpha, G, the electron mass: a laboratory measured their uncertainty,
# and arithmetic cannot reduce it. A thousand places of alpha would be a thousand places of a number known to eleven.
# The useful statement is not "we removed the uncertainty". It is WHICH uncertainties are mathematical and removable,
# and which are physical and permanent. Nobody publishes that division.
#
# WHY THE VERIFICATION HERE IS STRONGER THAN THE REST OF THE TABLE
#
# The table dn_constants.csv rests on two routes that agree to a zero gap, which is evidence: two routes could share an
# error. A pure rational admits something better. If p/q reads as v at N places, then
#
#     v * q  <=  p * 10^N  <  (v + 1) * q
#
# is an EXACT integer statement, with no arithmetic error in it at all. Either it holds, or the digits are wrong. That
# is verification, and not agreement, and it is available for every constant here without pi.
#
# @note The seven constants that define the SI are exact BY DEFINITION since the 2019 redefinition, and not by
# measurement. They are c, h, e, k, N_A, the caesium frequency and the luminous efficacy, all integers times powers of
# ten. Everything built from them by field operations is therefore a rational, and this file can be exact at all for
# that reason.

import argparse
import io
import os
import re
import sys
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
FAMILY = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(FAMILY)))
for _where in (HERE, os.path.join(FAMILY, "support"), os.path.join(ROOT, "examples", "proofing")):
    if _where not in sys.path:
        sys.path.insert(0, _where)

import dn_constants

LISTING = os.path.join(FAMILY, "dn_const", "codata_2022_allascii.txt")
OUT = os.path.join(FAMILY, "dn_const", "codata_exact.csv")

# The seven constants that define the SI, exact since the 2019 redefinition. They are integers times powers of ten.
# Nothing here is a float, and no rounding enters at the source.
CAESIUM = Fraction(9192631770)                       # Hz, the caesium hyperfine transition
LIGHT = Fraction(299792458)                          # m/s
PLANCK = Fraction(662607015, 10 ** 42)               # J s
CHARGE = Fraction(1602176634, 10 ** 28)              # C
BOLTZMANN = Fraction(1380649, 10 ** 29)              # J/K
AVOGADRO = Fraction(602214076 * 10 ** 15)            # 1/mol
LUMINOUS = Fraction(683)                             # lm/W

# Every derived constant as an exact expression, with its NIST name. `needs_pi` marks the ones that leave the rationals.
# Those get their check against our own pi, and not by a multiplication back.
DERIVED = [
    ("Boltzmann constant in eV/K", BOLTZMANN / CHARGE, False),
    ("Boltzmann constant in Hz/K", BOLTZMANN / PLANCK, False),
    ("Boltzmann constant in inverse meter per kelvin", BOLTZMANN / (PLANCK * LIGHT), False),
    ("conductance quantum", 2 * CHARGE ** 2 / PLANCK, False),
    ("inverse of conductance quantum", PLANCK / (2 * CHARGE ** 2), False),
    ("Josephson constant", 2 * CHARGE / PLANCK, False),
    ("von Klitzing constant", PLANCK / CHARGE ** 2, False),
    ("mag. flux quantum", PLANCK / (2 * CHARGE), False),
    ("Faraday constant", AVOGADRO * CHARGE, False),
    ("molar gas constant", AVOGADRO * BOLTZMANN, False),
    ("molar Planck constant", AVOGADRO * PLANCK, False),
    ("electron volt-hertz relationship", CHARGE / PLANCK, False),
    ("electron volt-inverse meter relationship", CHARGE / (PLANCK * LIGHT), False),
    ("electron volt-kelvin relationship", CHARGE / BOLTZMANN, False),
    ("electron volt-kilogram relationship", CHARGE / LIGHT ** 2, False),
    ("hertz-electron volt relationship", PLANCK / CHARGE, False),
    ("hertz-inverse meter relationship", Fraction(1) / LIGHT, False),
    ("hertz-kelvin relationship", PLANCK / BOLTZMANN, False),
    ("hertz-kilogram relationship", PLANCK / LIGHT ** 2, False),
    ("inverse meter-electron volt relationship", PLANCK * LIGHT / CHARGE, False),
    ("inverse meter-joule relationship", PLANCK * LIGHT, False),
    ("inverse meter-kelvin relationship", PLANCK * LIGHT / BOLTZMANN, False),
    ("inverse meter-kilogram relationship", PLANCK / LIGHT, False),
    ("joule-electron volt relationship", Fraction(1) / CHARGE, False),
    ("joule-hertz relationship", Fraction(1) / PLANCK, False),
    ("joule-inverse meter relationship", Fraction(1) / (PLANCK * LIGHT), False),
    ("joule-kelvin relationship", Fraction(1) / BOLTZMANN, False),
    ("joule-kilogram relationship", Fraction(1) / LIGHT ** 2, False),
    ("kelvin-electron volt relationship", BOLTZMANN / CHARGE, False),
    ("kelvin-hertz relationship", BOLTZMANN / PLANCK, False),
    ("kelvin-inverse meter relationship", BOLTZMANN / (PLANCK * LIGHT), False),
    ("kelvin-kilogram relationship", BOLTZMANN / LIGHT ** 2, False),
    ("kilogram-electron volt relationship", LIGHT ** 2 / CHARGE, False),
    ("kilogram-hertz relationship", LIGHT ** 2 / PLANCK, False),
    ("kilogram-inverse meter relationship", LIGHT / PLANCK, False),
    ("kilogram-joule relationship", LIGHT ** 2, False),
    ("kilogram-kelvin relationship", LIGHT ** 2 / BOLTZMANN, False),
    ("second radiation constant", PLANCK * LIGHT / BOLTZMANN, False),
    # The pi ones. h-bar is h over two pi, and everything built on it carries that factor.
    ("reduced Planck constant", PLANCK, True),
    ("atomic unit of action", PLANCK, True),
    ("natural unit of action", PLANCK, True),
    ("reduced Planck constant in eV s", PLANCK / CHARGE, True),
    ("natural unit of action in eV s", PLANCK / CHARGE, True),
    ("elementary charge over h-bar", CHARGE / PLANCK, True),
]

# THE ONE ENTRY THAT MULTIPLIES BY 2 pi INSTEAD OF A DIVISION. It gets its own name, and not a flag in the table.
#
# e / h-bar = e / (h / 2 pi) = 2 pi e / h. Every other pi entry has h-bar AS its value, and divides. This one has h-bar
# in its DENOMINATOR, and multiplies. A division here, with a spurious factor of two, comes out low by (2 pi)^2 = 19.74.
#
# THE EXACT CHECK CANNOT SEE THAT. A multiplication back verifies a correct division of a rational, and says nothing
# about whether it is the right rational. A correctly computed wrong expression is the case that it misses. The NIST
# comparison catches it. An external check therefore stays, even beside an exact internal one.
MULTIPLIES_BY_TWO_PI = frozenset(["elementary charge over h-bar"])

# ------------------------------------------------------------------------------------------------
# The rest of the truncated set. Everything below is a closed form too. Each needs one more exact definition, and the
# same arithmetic.
# ------------------------------------------------------------------------------------------------

# The conventional 1990 values, exact by convention and not by measurement. The listing prints both as finite decimals.
# The seven -90 units are rationals in these and in the SI set.
JOSEPHSON_90 = Fraction(4835979, 10) * 10 ** 9        # Hz/V, 483 597.9 e9 exactly
KLITZING_90 = Fraction(25812807, 1000)                # ohm, 25 812.807 exactly

# The stated conditions for Loschmidt and the molar volumes. Exact decimals, and not measurements.
ICE_POINT = Fraction(27315, 100)                      # K, 273.15
PRESSURE_100 = Fraction(100000)                       # Pa, 100 kPa
PRESSURE_ATM = Fraction(101325)                       # Pa, one standard atmosphere

# Unit conversions used by the MeV fm entry, both exact.
MEV = 10 ** 6 * CHARGE                                # J per MeV
FEMTOMETRE = Fraction(1, 10 ** 15)                    # m per fm

# The real constants K_J and R_K, for the -90 ratios.
JOSEPHSON = 2 * CHARGE / PLANCK
KLITZING = PLANCK / CHARGE ** 2

# The seven -90 units. Each matches the printed value of the listing:
#   - ohm-90 and henry-90 both 1.000 000 017 79, and farad-90 its reciprocal at 0.999 999 982 20.
#   - volt-90 1.000 000 106 66, ampere-90 and coulomb-90 1.000 000 088 87, and watt-90 1.000 000 195 53.
#
# THE EXACT CHECK CANNOT SEE AN INVERTED UNIT, AND THE NIST COMPARISON CAN. A multiplication back verifies a division,
# and not a definition: a reciprocal is the error that it misses.
#
# The definition of the conventional units fixes the directions. An ohm-90 is the size of the ohm implied by R_K-90
# taken as true. It is therefore the RATIO OF THE REAL CONSTANT TO THE CONVENTIONAL ONE:
#
#     ohm-90  = R_K / R_K-90  = 25812.807459... / 25812.807  = 1.000 000 017 79...
#     volt-90 = K_J-90 / K_J  = 483597.9 / 483597.8484...    = 1.000 000 106 66...
#
# The Josephson one runs the other way, because K_J is a frequency PER volt: a larger K_J means a smaller volt. A check
# of each against the printed value of the listing settles the direction. The printed values are in this comment for
# that reason.
OHM_90 = KLITZING / KLITZING_90
VOLT_90 = JOSEPHSON_90 / JOSEPHSON
AMPERE_90 = VOLT_90 / OHM_90

MORE_DERIVED = [
    ("conventional value of ohm-90", OHM_90, 0),
    ("conventional value of henry-90", OHM_90, 0),
    ("conventional value of farad-90", Fraction(1) / OHM_90, 0),
    ("conventional value of volt-90", VOLT_90, 0),
    ("conventional value of ampere-90", AMPERE_90, 0),
    ("conventional value of coulomb-90", AMPERE_90, 0),
    ("conventional value of watt-90", VOLT_90 * AMPERE_90, 0),

    ("Planck constant in eV/Hz", PLANCK / CHARGE, 0),

    ("Loschmidt constant (273.15 K, 100 kPa)", PRESSURE_100 / (BOLTZMANN * ICE_POINT), 0),
    ("Loschmidt constant (273.15 K, 101.325 kPa)", PRESSURE_ATM / (BOLTZMANN * ICE_POINT), 0),
    ("molar volume of ideal gas (273.15 K, 100 kPa)",
     AVOGADRO * BOLTZMANN * ICE_POINT / PRESSURE_100, 0),
    ("molar volume of ideal gas (273.15 K, 101.325 kPa)",
     AVOGADRO * BOLTZMANN * ICE_POINT / PRESSURE_ATM, 0),

    # c1L = 2 h c^2 is rational; c1 = 2 pi h c^2 carries one power of pi.
    ("first radiation constant for spectral radiance", 2 * PLANCK * LIGHT ** 2, 0),
    ("first radiation constant", 2 * PLANCK * LIGHT ** 2, 1),

    # h-bar c expressed in MeV fm: (h / 2 pi) c, converted. One INVERSE power of pi.
    ("reduced Planck constant times c in MeV fm",
     PLANCK * LIGHT / (2 * MEV * FEMTOMETRE), -1),

    # Stefan-Boltzmann: 2 pi^5 k^4 / (15 h^3 c^2). Five powers of pi.
    ("Stefan-Boltzmann constant",
     2 * BOLTZMANN ** 4 / (15 * PLANCK ** 3 * LIGHT ** 2), 5),
]

# The two Wien constants, the ONLY entries in the whole exact set without a closed form in the constants that define the
# SI. Each needs the root of a transcendental:
#
# docs-check: quoting
#     wavelength law   b  = h c / (k x)    with x the root of  x = 5 (1 - e^-x)
#     frequency law    b' = x' k / h       with x' the root of x = 3 (1 - e^-x)
# docs-check: end quoting
#
# Those roots have no closed form. Newton finds them, and a SUBSTITUTION VERIFIES them: the residual of the equation
# that defines the root bounds the error directly. That is a stronger check than agreement with a printed value.
WIEN = [
    ("Wien wavelength displacement law constant", 5, "over"),
    ("Wien frequency displacement law constant", 3, "times"),
]


def published():
    """The NIST value for each name, as its printed digit string with spaces removed."""
    out = {}
    if not os.path.exists(LISTING):
        return out
    for line in io.open(LISTING, encoding="utf-8", errors="replace").read().splitlines():
        if "(exact)" not in line:
            continue
        name = line[:58].strip()
        value = line[58:82].strip()
        out[name] = value.replace(" ", "")
    return out


def digits_of_rational(value, digits):
    """floor(value * 10^digits) for an exact Fraction. No rounding, no float.

    Returns (scaled integer, exponent), where the exponent normalizes the value into [1, 10). The leading digits can
    then match against a printed mantissa.
    """
    if value <= 0:
        raise ValueError("only positive constants here")
    exponent = 0
    probe = value
    while probe >= 10:
        probe /= 10
        exponent += 1
    while probe < 1:
        probe *= 10
        exponent -= 1
    scaled = (probe.numerator * 10 ** digits) // probe.denominator
    return scaled, exponent


def verified_exactly(value, scaled, digits):
    """The exact integer check that the reported digits are right.

    The statement v <= p * 10^d / q < v + 1, rearranged to avoid any division at all:

            v * q <= p * 10^d < (v + 1) * q

    No arithmetic error can enter this. It either holds, or the digits are wrong.
    """
    exponent = 0
    probe = value
    while probe >= 10:
        probe /= 10
        exponent += 1
    while probe < 1:
        probe *= 10
        exponent -= 1
    left = scaled * probe.denominator
    middle = probe.numerator * 10 ** digits
    right = (scaled + 1) * probe.denominator
    return (left <= middle) and (middle < right)


def exp_negative(x_scaled, scale):
    """exp(-x) for a positive scaled x, by the series with alternate signs. Only the Wien root uses it.

    The terms are (-x)^k / k!. For x near five, they peak immediately, and then fall off a cliff. The cancellation is
    therefore mild, and the guard that the caller already carries covers it.
    """
    total = scale
    term = scale
    k = 0
    while True:
        k += 1
        term = (term * x_scaled) // (scale * k)
        if term == 0:
            break
        total += term if (k % 2 == 0) else -term
    return total


def wien_root(multiplier, digits, guard=40):
    """The root of x = m (1 - e^-x), by Newton, with the residual returned as its own bound.

    THE SPECTRAL CONSTANTS ARE THE ONLY ENTRIES IN THE EXACT SET WITHOUT A CLOSED FORM. Wien's displacement laws locate
    the peak of a blackbody's spectrum. That location is the root of a transcendental with no expression in the
    constants that define the SI. The value m = 5 gives 4.965114231... for the wavelength law, and m = 3 gives
    2.821439372... for the frequency law.

    VERIFIED BY SUBSTITUTION, AND NOT BY COMPARISON. The residual |x - m(1 - e^-x)| at the computed root bounds the
    error directly. This route therefore grades itself against the equation that defines it, and not against a printed
    value. That is possible here, and not for a constant defined by a series.

    Returns (scaled root at `digits` places, residual in units of the working scale).
    """
    places = digits + guard
    scale = 10 ** places
    m = multiplier

    # Start just under m, which is where the root sits for every m above one.
    x = m * scale - scale // 2
    for _ in range(200):
        e = exp_negative(x, scale)
        f = x - m * scale + m * e
        slope = scale - m * e
        if slope == 0:
            break
        step = (f * scale) // slope
        if step == 0:
            break
        x -= step

    e = exp_negative(x, scale)
    residual = abs(x - (m * scale - m * e))
    return x // (10 ** guard), residual


def with_pi(value, digits, multiply=False, pi_power=None):
    """The constants that involve pi, from the tree's own pi.

    The relation is h-bar = h / (2 pi). Pi comes from dn_constants at digits + guard. There it carries two routes that
    agree at a zero gap, and a derived error bound. The bound here therefore comes by inheritance, and not by
    assumption.
    """
    guard = 40
    places = digits + guard
    pi_scaled = dn_constants.pi_machin(places)

    exponent = 0
    probe = value
    while probe >= 10:
        probe /= 10
        exponent += 1
    while probe < 1:
        probe *= 10
        exponent -= 1

    # THE GENERAL FORM IS A RATIONAL TIMES AN INTEGER POWER OF pi, positive or negative. The 2 pi cases are that form,
    # with the two folded into the rational by the caller. The spectral constants need the general form:
    # Stefan-Boltzmann carries pi^5, and the MeV fm entry carries pi^-1. A flag for a divide or a multiply by 2 pi can
    # express neither.
    if pi_power is not None:
        power = abs(pi_power)
        pi_to = 10 ** (places * (power - 1)) if power else 1
        if power:
            pi_to = pi_scaled ** power // (10 ** (places * (power - 1)))
        else:
            pi_to = 10 ** places
        if pi_power > 0:
            top = probe.numerator * pi_to * 10 ** places
            bottom = probe.denominator * 10 ** places
        else:
            top = probe.numerator * 10 ** (places * 2)
            bottom = probe.denominator * pi_to
    elif multiply:
        top = probe.numerator * 2 * pi_scaled * 10 ** places
        bottom = probe.denominator * 10 ** places
    else:
        top = probe.numerator * 10 ** (places * 2)
        bottom = probe.denominator * 2 * pi_scaled
    wide = top // bottom
    # Normalize again, because a division by 2 pi can move the leading digit.
    scaled = wide // (10 ** guard)
    while scaled >= 10 ** (digits + 1):
        scaled //= 10
        exponent += 1
    while scaled < 10 ** digits:
        scaled *= 10
        exponent -= 1
    return scaled, exponent


def shown(scaled, digits):
    text = str(scaled).rjust(digits + 1, "0")
    return text[0] + "." + text[1:]


def build(digits=1000):
    nist = published()
    rows = []

    # The spectral constants first, because they are the only ones that a substitution into the equation that defines
    # them verifies. The others get a multiplication back, or a comparison.
    for name, multiplier, direction in WIEN:
        root, residual = wien_root(multiplier, digits)
        if direction == "over":
            # b = h c / (k x)
            coefficient = PLANCK * LIGHT / BOLTZMANN
            top = coefficient.numerator * 10 ** (digits * 2)
            bottom = coefficient.denominator * root
        else:
            # b' = x k / h
            coefficient = BOLTZMANN / PLANCK
            top = coefficient.numerator * root * 10 ** digits
            bottom = coefficient.denominator * 10 ** digits
        scaled = top // bottom
        exponent = 0
        while scaled >= 10 ** (digits + 1):
            scaled //= 10
            exponent += 1
        while scaled < 10 ** digits:
            scaled *= 10
            exponent -= 1
        text = shown(scaled, digits)
        printed = nist.get(name)
        agrees = None
        if printed:
            head = re.sub(r"[^0-9]", "", printed.split("...")[0]).lstrip("0")
            agrees = text.replace(".", "").lstrip("0")[:len(head)] == head

        # THE CRITERION IS THE WORKING SCALE, AND NOT ZERO. No root of a transcendental ever gives a Newton residual of
        # exactly zero. A residual of a few units out of 10^(digits + guard) is the arithmetic floor, and it sits below
        # the discarded guard digits. A demand for exactly zero would report both spectral constants as failures, and
        # they are correct.
        guard_units = 10 ** 40
        rows.append({
            "name": name, "value": text, "exponent": exponent, "digits": digits,
            "how": "root of x = %d(1 - e^-x) by Newton, substitution residual %d units below the "
                   "guard's 1e40" % (multiplier, residual),
            "exact_verified": (residual < guard_units), "agrees_with_nist": agrees,
            "published": printed or "",
        })

    for name, value, pi_power in [(a, b, None if c is False else (1 if c is True else c))
                                  for a, b, c in DERIVED] + \
                                 [(a, b, c) for a, b, c in MORE_DERIVED]:
        needs_pi = pi_power is not None and pi_power != 0
        if needs_pi:
            times = name in MULTIPLIES_BY_TWO_PI
            if name in [one[0] for one in MORE_DERIVED]:
                scaled, exponent = with_pi(value, digits, pi_power=pi_power)
                how = "exact rational times pi^%d, pi from dn_const at a zero gap" % pi_power
            else:
                scaled, exponent = with_pi(value, digits, multiply=times)
                how = ("exact rational times 2 pi" if times else "exact rational over 2 pi") \
                    + ", pi from dn_const at a zero gap"
            exact_ok = None
        else:
            scaled, exponent = digits_of_rational(value, digits)
            how = "exact rational in the SI defining constants"
            exact_ok = verified_exactly(value, scaled, digits)

        text = shown(scaled, digits)
        agrees = None
        printed = nist.get(name)
        if printed:
            head = printed.split("...")[0].replace("e", "").split("e")[0]
            head = re.sub(r"[^0-9.]", "", head)
            # LEADING ZEROS COME OFF BOTH SIDES. That is a fix of the comparison, and not of the value. This file
            # normalizes every value into [1, 10), and NIST prints farad-90 as 0.999 999 982 20 without a normalization.
            # Identical digits would otherwise fail the compare on the leading zero alone, and read as a wrong constant
            # in the report.
            compact = head.replace(".", "").lstrip("0")
            mine = text.replace(".", "").lstrip("0")[:len(compact)]
            agrees = (mine == compact)

        rows.append({
            "name": name, "value": text, "exponent": exponent, "digits": digits,
            "how": how, "exact_verified": exact_ok, "agrees_with_nist": agrees,
            "published": printed or "",
        })
    return rows


def _split():
    lines = io.open(LISTING, encoding="utf-8", errors="replace").read().splitlines()
    exact = [one for one in lines if "(exact)" in one]
    truncated = [one for one in exact if "..." in one]
    print("")
    print("  The 2022 CODATA listing, classified by whether its uncertainty is removable.")
    print("")
    print("  %-56s %6s" % ("class", "count"))
    print("  %-56s %6d" % ("constants in the listing", len([one for one in lines if len(one) > 60])))
    print("  %-56s %6d" % ("marked exact", len(exact)))
    print("  %-56s %6d" % ("  of those, terminating: the digits ARE the value", len(exact) - len(truncated)))
    print("  %-56s %6d" % ("  of those, truncated: NIST stops, we need not", len(truncated)))
    print("  %-56s %6d" % ("carrying a measured uncertainty", len([one for one in lines if len(one) > 60]) - len(exact)))
    print("")
    print("  THE REMOVABLE UNCERTAINTY IS THE TRUNCATED COLUMN ALONE. Those 62 have no")
    print("  uncertainty at all: they are closed forms in the seven SI defining constants, which")
    print("  have been exact by definition since 2019. Their missing digits are unprinted, not")
    print("  unknown.")
    print("")
    print("  THE MEASURED COLUMN IS PERMANENT. alpha, G, the electron mass. Their uncertainty is")
    print("  what a laboratory measured and no arithmetic reduces it. This tool does not touch")
    print("  them and any claim that precision could would be false.")
    return 0


def _report(digits=1000):
    rows = build(digits)
    exactly = [one for one in rows if one["exact_verified"] is True]
    failed_exact = [one for one in rows if one["exact_verified"] is False]
    agree = [one for one in rows if one["agrees_with_nist"] is True]
    disagree = [one for one in rows if one["agrees_with_nist"] is False]

    print("")
    print("  %d exact CODATA constants carried to %d places." % (len(rows), digits))
    print("")
    print("  %-48s %10s %10s %s" % ("constant", "exact", "vs NIST", "first 40 places"))
    for one in rows[:14]:
        print("  %-48s %10s %10s %s"
              % (one["name"][:47],
                 "yes" if one["exact_verified"] else ("pi" if one["exact_verified"] is None else "NO"),
                 "agrees" if one["agrees_with_nist"] else ("-" if one["agrees_with_nist"] is None else "DIFFERS"),
                 one["value"][:40]))
    print("  ... %d more" % (len(rows) - 14))
    print("")
    print("  exactly verified by multiplying back : %d" % len(exactly))
    print("  verified against our own pi          : %d" % len([one for one in rows if one["exact_verified"] is None]))
    print("  agreeing with the NIST printed digits: %d of %d checkable"
          % (len(agree), len(agree) + len(disagree)))
    if failed_exact:
        print("  EXACT CHECK FAILED on %d: %s" % (len(failed_exact), [one["name"] for one in failed_exact]))
    if disagree:
        print("  DISAGREES WITH NIST on %d: %s" % (len(disagree), [one["name"] for one in disagree]))

    with io.open(OUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("name\texponent\tdigits\thow\texact_verified\tagrees_with_nist\tvalue\n")
        for one in rows:
            handle.write("%s\t%d\t%d\t%s\t%s\t%s\t%s\n"
                         % (one["name"], one["exponent"], one["digits"], one["how"],
                            one["exact_verified"], one["agrees_with_nist"], one["value"]))
    print("")
    print("  wrote %s" % OUT)
    return len(failed_exact) + len(disagree)


def _check():
    failed = 0
    print("")

    rows = build(200)

    # THE EXACT CHECK IS THE CHECK THAT MATTERS FOR THE RATIONALS. Every one must pass. It is an integer statement, with
    # no tolerance to argue about.
    rationals = [one for one in rows if one["exact_verified"] is not None]
    bad = [one for one in rationals if not one["exact_verified"]]
    print("  %d rational constants, all exactly verified by multiplying back: %s"
          % (len(rationals), not bad))
    if bad:
        for one in bad:
            print("    FAIL %s" % one["name"])
        failed += len(bad)

    # AGREEMENT WITH NIST IS THE EXTERNAL CHECK, and it catches a wrong FORMULA. The exact check above would pass a
    # correctly divided wrong expression.
    checkable = [one for one in rows if one["agrees_with_nist"] is not None]
    off = [one for one in checkable if not one["agrees_with_nist"]]
    print("  %d checkable against the NIST printed digits, %d disagree" % (len(checkable), len(off)))
    for one in off:
        print("    DIFFERS %s: ours %s, NIST %s"
              % (one["name"], one["value"][:18], one["published"]))
    failed += len(off)

    # THE NEGATIVE CONTROL. The NIST comparison must catch a deliberately wrong expression, or that comparison is
    # decoration.
    wrong = BOLTZMANN / (CHARGE * 2)
    scaled, _exponent = digits_of_rational(wrong, 200)
    text = shown(scaled, 200)
    nist = published().get("Boltzmann constant in eV/K", "")
    head = re.sub(r"[^0-9]", "", nist.split("...")[0])
    caught = text.replace(".", "")[:len(head)] != head
    print("  a deliberately wrong expression is caught by the NIST check: %s" % caught)
    if not caught:
        print("    FAIL the NIST comparison does not detect a wrong formula")
        failed += 1

    # AND WIDTH MUST NOT CHANGE THE LEADING DIGITS OF THE ANSWER, because the parameters come from a derivation.
    narrow = build(100)
    wide = build(400)
    drift = 0
    for a, b in zip(narrow, wide):
        if a["value"][:60] != b["value"][:60]:
            drift += 1
    print("  the leading 60 places are the same at 100 and 400 places: %s" % (drift == 0))
    if drift:
        print("    FAIL %d constants moved with the requested width" % drift)
        failed += 1

    print("")
    print("  %d check(s) failed" % failed)
    return failed


def main():
    parser = argparse.ArgumentParser(description="the exact CODATA constants, carried out")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--split", action="store_true")
    parser.add_argument("--digits", type=int, default=1000)
    args = parser.parse_args()

    if args.check:
        return 1 if _check() else 0
    if args.split:
        return _split()
    return 1 if _report(args.digits) else 0


if __name__ == "__main__":
    sys.exit(main())
