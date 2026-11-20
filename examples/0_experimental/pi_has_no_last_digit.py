#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-012
#
# What is the last digit of pi at the floor? There is no last digit, because there is no floor. This
# shows it. It names the digit at each floor that it asks for, and then names one below it.
#
#   Usage:  python examples/0_experimental/pi_has_no_last_digit.py
#
# This reads no corpus, and sits in 0_experimental. It is an arithmetic result that runs, ahead of any
# stage reading.
#
# Pi is transcendental, and its decimal expansion never ends and never repeats. A floor is a scale,
# 10^-N. Pi cut at that floor has a definite last digit, its Nth decimal digit, exact and computable.
# But N has no ceiling on this engine, and a higher floor always turns up another digit. The last
# digit at the floor exists for every floor, and is never the last. The precision document beside
# this file makes the same claim, and this shows it on the most familiar irrational.
#
# The digits come from utils/maint/dn_precision/calc/dn_constants.py. Its two routes for pi are the
# Chudnovsky series of digit_engine and the arctangent formula of Machin. This script accepts a value
# only where the two agree at that precision. That agreement is the positive control, and it uses no
# stored expansion: the script derives a natural constant, and never looks it up. The command
# dn_constants.py --check grades both routes, and a caller runs it before any digit counts.
#
# The null: a computation at a deeper floor keeps every digit above the old floor, and moves only the
# last one. Thus "no floor" looks like this from inside. No bound: the floor is a declared input, and
# the arithmetic is exact integers throughout.

import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "utils", "maint", "dn_precision", "calc"))
import dn_constants  # noqa: E402

# The floors at which to name the digit of pi. The last is deep enough to make the point, and quick.
FLOORS = [1, 10, 100, 1000, 5000, 10000]
DEEPEST = FLOORS[-1]


def pi(places):
    """floor(pi * 10^places), from two routes that must agree.

    Chudnovsky and Machin share no term. A value that both give is the two-route positive control.
    A disagreement stops the run, and the script prints no digit.
    """
    fast = dn_constants.digit_engine.chudnovsky(places)
    slow = dn_constants.pi_machin(places)
    if fast != slow:
        raise SystemExit("Chudnovsky and Machin disagree at %d places: gap %d" % (places, abs(fast - slow)))
    return fast


def digit_at_floor(floor):
    """The last decimal digit of pi at scale 10^-floor, its floor-th decimal digit.

    pi(floor) is floor(pi * 10^floor), an exact integer. Its last digit is decimal place floor of pi.
    """
    return pi(floor) % 10


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    out.write("  the last digit of pi at the floor: there is none, because there is no floor\n\n")

    # Pi to the deepest floor. pi() returns only where Chudnovsky and Machin agree. A value at all is
    # the two-route positive control, with no published expansion in use.
    pi_deep = pi(DEEPEST)
    out.write("  pi computed to %d places, Chudnovsky and Machin agreeing, no stored expansion consulted\n\n"
              % DEEPEST)

    out.write("  %-14s %s\n" % ("floor (place)", "pi's last digit at that floor"))
    for floor in FLOORS:
        out.write("  %-14d %d\n" % (floor, digit_at_floor(floor)))

    # The null: go deeper, and show that every digit above the old floor stays the same. Only the
    # last one moves. Thus "no floor" looks like this from inside.
    deeper = DEEPEST + 5
    pi_deeper = pi(deeper)
    above_old_floor = pi_deeper // (10 ** (deeper - DEEPEST))   # pi at DEEPEST, read from the deeper value
    prefix_stable = above_old_floor == pi_deep                 # every digit above DEEPEST is unchanged
    old_digit = pi_deep % 10
    new_digit = pi_deeper % 10

    out.write("\n  raise the floor to %d places: every digit above place %d is unchanged (%s), the digit\n"
              % (deeper, DEEPEST, prefix_stable))
    out.write("  at place %d is still %d, and a new last digit appears at place %d, the value %d. the floor\n"
              % (DEEPEST, old_digit, deeper, new_digit))
    out.write("  dropped and pi passed up another digit. there is no bottom, and there is no last digit.\n")
    out.flush()

    return 0 if prefix_stable else 1


if __name__ == "__main__":
    raise SystemExit(main())
