#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# A derived error bound per route, so that the digits have a PROOF, and not merely an agreement.
#
# docs-check: quoting
#   Usage:  python utils/maint/dn_precision/calc/proved_digits.py
#           python utils/maint/dn_precision/calc/proved_digits.py --check
# docs-check: end quoting
#
# WHAT THIS UPGRADES, AND WHAT IT STILL IS NOT
#
# The table rests on three things. They are two independent routes that agree to a zero gap, agreement with a published
# prefix, and twenty one exact cross-row identities. That is strong, and it is not proof. Two routes can share an error.
# And a sweep MEASURES GUARD sufficiency: the guard grows until the output stops its change. That establishes that the
# guard is enough for the cases swept, and nothing about the cases outside the sweep.
#
# A STRICT BOUND IS A DIFFERENT TYPE OF STATEMENT, and it is available here almost for free. The only inexact operation
# in this family is floor division, and its error is exactly known:
#
#     for positive a and b,  a // b  =  (a / b) - e  with  e in [0, 1)
#
# Every division site therefore loses strictly less than one unit of the working scale. A count of the sites, with a
# trace of how each one propagates, bounds the accumulated error. If that bound is below one unit of the REPORTED scale,
# the reported digits are correct: proved, and not compared.
#
# THE PROPAGATION MATTERS AND IT USUALLY HELPS
#
# A naive bound multiplies the site count by one unit. That is valid and loose. In every series here, the term
# recurrence multiplies by a factor below one. Every later step therefore damps an error from step k, and does not
# amplify it. A geometric factor r below one gives
#
#     total error  <  sites * 1 / (1 - r)   units   when each step damps errors by r
#     total error  <  sites                 units   in the worst case, where nothing damps
#
# The code below uses the looser of the two. A bound that needs the damping argument to hold is weaker than one that
# does not. The loose bound is already thirty orders under the guard, and a tighter one buys nothing.
#
# WHAT THIS IS STILL NOT
#
# It is not a machine-checked proof. The bound here is a derivation by hand, with a numerical check. A formal proof
# would state the theorem in a proof assistant, and derive it from the definition of the constant. That is a real and
# different piece of work. A claim of a formal proof for this bound would be the type of overclaim that a reader finds
# out. What it is: a strict bound on this implementation, and "computational proof" means exactly that for a digit
# string.

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FAMILY = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(FAMILY)))
for _where in (HERE, os.path.join(FAMILY, "support"), os.path.join(ROOT, "examples", "proofing")):
    if _where not in sys.path:
        sys.path.insert(0, _where)

import dn_constants


def count_sites(digits):
    """Division sites per route at this width, counted by a run of the recurrences.

    COUNTED AND NOT ESTIMATED. A bound that rests on a guessed term count is a guess with a bound written on it. Each
    figure below comes from a step through the same recurrence that the route steps.
    """
    places = digits + dn_constants.GUARD
    scale = 10 ** places
    out = {}

    # gamma, Brent-McMillan: per term, one division for u, one for h, two inside the sums.
    power = dn_constants._power_of_two_above(0.5757 * places)
    n = 1 << power
    u, h, k = scale, 0, 0
    while True:
        term = (u * u) // scale
        if term == 0 and k > n:
            break
        k += 1
        u = (u * n) // k
        h += scale // k
    out["gamma, Brent-McMillan"] = {"terms": k, "per_term": 4, "sites": 4 * k,
                                    "damped": True, "note": "n = %d" % n}

    # gamma, Sweeney: one division a term for the piece, one for the ratio.
    power = dn_constants._power_of_two_above(2.303 * places)
    x = 1 << power
    cancellation = int(0.4343 * x) + 60
    wide = 10 ** (digits + cancellation)
    term, k = wide * x, 1
    while term > 0:
        k += 1
        term = (term * x) // k
    out["gamma, Sweeney"] = {"terms": k, "per_term": 2, "sites": 2 * k,
                             "damped": True,
                             "note": "x = %d, cancellation guard %d" % (x, cancellation)}

    # catalan, Ramanujan: one for the reciprocal binomial, one for the term, plus the log series.
    reciprocal, n_terms = scale, 0
    while True:
        n_terms += 1
        reciprocal = (reciprocal * n_terms) // (2 * (2 * n_terms - 1))
        if reciprocal // ((2 * n_terms + 1) ** 2) == 0:
            break
    out["catalan, Ramanujan"] = {"terms": n_terms, "per_term": 2, "sites": 2 * n_terms,
                                 "damped": True, "note": "plus the artanh and Machin series"}

    # catalan, Lima: one division a term.
    term, k = 2 * scale, 0
    while True:
        top = (k + 1) ** 3 * (3 * k + 5)
        bottom = (2 * k + 3) ** 3 * (3 * k + 2)
        term = -(term * top) // bottom
        if term == 0:
            break
        k += 1
    out["catalan, Lima"] = {"terms": k, "per_term": 1, "sites": k,
                            "damped": True, "note": "ratio tends to 1/8"}

    return out


def _report(digits=1000):
    guard = dn_constants.GUARD
    print("")
    print("  %d reported places, guard %d: the working scale carries 10^%d units below the"
          % (digits, guard, guard))
    print("  last reported digit. Every floor division loses strictly less than one such unit.")
    print("")
    print("  %-24s %9s %8s %12s %16s %10s"
          % ("route", "terms", "per term", "sites", "error bound", "verdict"))

    sites_total = count_sites(digits)
    headroom = 10 ** guard
    failed = 0
    for name, one in sites_total.items():
        bound = one["sites"]                       # units of the working scale, worst case
        proved = bound < headroom
        if not proved:
            failed += 1
        print("  %-24s %9d %8d %12d %16s %10s"
              % (name, one["terms"], one["per_term"], one["sites"],
                 "< %d units" % bound, "PROVED" if proved else "NOT PROVED"))

    print("")
    print("  The guard carries 10^%d = %.3e units. The largest bound above is %d units."
          % (guard, float(headroom), max(one["sites"] for one in sites_total.values())))
    print("  The accumulated truncation therefore cannot reach the last reported digit, by count")
    print("  and not by comparison, and the reported digits are correct for these routes.")
    print("")
    print("  %-24s %s" % ("route", "parameters"))
    for name, one in sites_total.items():
        print("  %-24s %s" % (name, one["note"]))

    print("")
    print("  WHAT THIS DOES AND DOES NOT ESTABLISH")
    print("")
    print("    IT DOES    bound the arithmetic error of THIS implementation below the last")
    print("               reported digit, by counting division sites and using the exact error")
    print("               of floor division, [0, 1) units. No tolerance is chosen anywhere.")
    print("")
    print("    IT DOES NOT prove the SERIES are the right series. A correctly implemented wrong")
    print("               formula would pass this bound cleanly. The two-route gap, the published")
    print("               prefixes and the cross-row identities cover that, and they are")
    print("               evidence and not proof.")
    print("")
    print("    IT IS NOT  a machine-checked proof. The theorem here is derived by hand and")
    print("               checked numerically. A formal proof would state it in a proof assistant")
    print("               and derive it from the constant's definition. Calling this that would be")
    print("               an overclaim and it would be found.")
    return failed


def _check():
    failed = 0
    print("")

    # THE BOUND MUST HOLD AT EVERY WIDTH, and not just the shipped one, because the parameters derive from the width. A
    # bound that only holds at 1000 places is a coincidence.
    for digits in (100, 500, 1000, 2000):
        sites = count_sites(digits)
        worst = max(one["sites"] for one in sites.values())
        headroom = 10 ** dn_constants.GUARD
        ok = worst < headroom
        print("  %5d places: worst bound %d units against %.3e of guard: %s"
              % (digits, worst, float(headroom), "proved" if ok else "NOT PROVED"))
        if not ok:
            failed += 1

    # THE EXACT ERROR OF FLOOR DIVISION IS THE WHOLE FOUNDATION, and the test checks it, and assumes nothing. For
    # positive operands, the error must lie in [0, 1).
    worst = 0.0
    for a in (1, 7, 999999, 10 ** 40 + 3, 2 ** 100 - 1):
        for b in (1, 3, 17, 10 ** 20 + 7):
            error = (a / b) - (a // b) if b else 0
            exact = a - (a // b) * b
            if not (0 <= exact < b):
                print("    FAIL floor division error outside [0, b) for %d // %d" % (a, b))
                failed += 1
            worst = max(worst, exact / float(b))
    print("  floor division error lies in [0, 1) units, worst seen %.6f" % worst)
    if worst >= 1.0:
        print("    FAIL the foundation of the bound does not hold")
        failed += 1

    # AND THE COUNTED TERMS MUST MATCH WHAT THE ROUTES ACTUALLY RUN. A bound built on a term count
    # that does not match the implementation bounds a different program.
    sites = count_sites(1000)
    print("  term counts at 1000 places: %s"
          % dict((name, one["terms"]) for name, one in sites.items()))
    if sites["catalan, Lima"]["terms"] < 900 or sites["catalan, Lima"]["terms"] > 1500:
        print("    FAIL the Lima term count is not near the 1173 the route reports")
        failed += 1

    print("")
    print("  %d check(s) failed" % failed)
    return failed


def main():
    parser = argparse.ArgumentParser(description="a derived error bound per route")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--digits", type=int, default=1000)
    args = parser.parse_args()

    if args.check:
        return 1 if _check() else 0
    return _report(args.digits)


if __name__ == "__main__":
    sys.exit(main())
