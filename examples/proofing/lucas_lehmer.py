"""Lucas-Lehmer, graded against every Mersenne prime that the world already knows.

docs-check: quoting
    python examples/proofing/lucas_lehmer.py --check      the test against the known exponents
    python examples/proofing/lucas_lehmer.py --ntt        the exact transform against Python's own
    python examples/proofing/lucas_lehmer.py --timing     where the cost actually is
docs-check: end quoting

WHAT THIS IS FOR

A Mersenne number `M_p = 2^p - 1` is prime exactly when the Lucas-Lehmer sequence

    s_0 = 4,    s_(i+1) = (s_i^2 - 2) mod M_p

reaches zero at step `p - 2`. The test is that alone, and its entire cost is the square: `p` iterations, each the square
of a `p`-bit number.

WHY THE CONTROL COMES FIRST AND IS MOST OF THIS FILE

The verdict is a single bit, and a single bit is the shape of answer that can be wrong and look right. The test
therefore grades itself against the exponents that the world has already settled: every Mersenne prime up to 11213, and
the composites between them. It does so BEFORE it meets an exponent that nobody knows. A run that cannot reproduce
`M127` has an opinion about `M332000000` that is worth nothing.

The drawn null is not a tolerance. The known list is exact, the verdicts are exact, and a single disagreement anywhere
is a refusal.

WHY EXACT, AND WHERE THAT IS THE ADVANTAGE

GIMPS squares with floating-point FFTs, and must check a round-off bound EVERY iteration. One bad rounding silently
corrupts a run of months. A number theoretic transform carries no round-off to check. It is integer arithmetic modulo a
proven prime from `twiddle_proof`, and a wrong answer is impossible, and not merely improbable. `four_step.py` in this
directory holds that transform, proven against a direct O(N^2) transform and against Python's own multiply.

This belongs here for that reason, and is not another Prime95: the instrument has no error budget.

No computing libraries. Python's own integers, and this tree's own transform.
"""

import argparse
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import four_step

# Every Mersenne prime exponent below 12000: the settled record that grades the test. These are not a sample, and not a
# guess. They are the complete list in that range.
KNOWN_MERSENNE_EXPONENTS = (
    2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281,
    3217, 4253, 4423, 9689, 9941, 11213,
)


def mersenne_reduce(value, exponent, modulus):
    """`value mod (2^exponent - 1)`, by a fold, and not by a division.

    A number splits as `high * 2^exponent + low`, and `2^exponent = 1` modulo the Mersenne number. The fold is therefore
    `high + low`, repeated until it fits, then one conditional subtraction. No division happens anywhere. Mersenne
    numbers are worth a test at this size for that reason.
    """
    while value.bit_length() > exponent:
        value = (value >> exponent) + (value & modulus)
    if value == modulus:
        return 0
    return value


def lucas_lehmer(exponent, square=None):
    """Is `2^exponent - 1` prime? Deterministic, with no probabilistic step anywhere.

    `square` is the square to use. The transform can then swap for Python's own, and the two grade each other. Defaults
    to Python's, the reference.
    """
    if exponent == 2:
        return True          # M2 = 3, and the sequence below does not cover it
    if exponent < 2:
        return False

    modulus = (1 << exponent) - 1
    residue = 4
    for _ in range(exponent - 2):
        product = (residue * residue) if square is None else square(residue)
        residue = mersenne_reduce(product - 2, exponent, modulus)
    return residue == 0


def limb_bits_for(length, prime):
    """The largest limb that the modulus can carry at this transform length, derived and not chosen.

    A convolution output is a sum of at most `length` products of two limbs, and the bound is therefore

            length * (2^bits - 1)^2  <  prime

    Past it the sum wraps the modulus, and the wrap is SILENT: the transform returns a perfectly well-formed wrong
    answer. Sixteen-bit limbs against a 2^31 modulus reach 2^43 at length 2048. The control catches that, because seven
    of eight known Mersenne primes then come back composite.
    """
    bits = 1
    while True:
        span = (1 << (bits + 1)) - 1
        if (length * span * span) >= prime:
            return bits
        bits += 1


def ntt_square(value):
    """Square through the exact transform, so that the instrument under test is the transform itself."""
    if value == 0:
        return 0

    # The limb size follows from the length, and the length follows from the limb size. A walk down from the largest
    # limb that could possibly serve settles both.
    bits = 16
    while True:
        mask = (1 << bits) - 1
        count = (value.bit_length() + bits - 1) // bits
        length = 1
        while length < (count * 2):
            length *= 2
        allowed = limb_bits_for(length, four_step.PRIME)
        if bits <= allowed:
            break
        bits = allowed
        if bits < 1:
            raise ValueError("this modulus cannot carry a number of this size in one prime")

    limbs = []
    held = value
    while held > 0:
        limbs.append(held & mask)
        held >>= bits
    if not limbs:
        return 0

    length = 1
    while length < (len(limbs) * 2):
        length *= 2
    padded = limbs + ([0] * (length - len(limbs)))

    folded = four_step.convolve(padded, list(padded), four_step.PRIME, four_step.GENERATOR)

    # Carry the convolution back into an integer. The transform is exact, and this is therefore the only place where a
    # bound matters. Each output is a sum of at most `length` products of limbs, which must stay below the modulus.
    # Otherwise the convolution wraps, and the answer is silently wrong.
    total = 0
    for at in range(len(folded) - 1, -1, -1):
        total = (total << bits) + folded[at]
    return total


def is_prime_small(value):
    """Trial division, for the choice of exponents. Exponents are small, and the Mersennes are not."""
    if value < 2:
        return False
    if value % 2 == 0:
        return value == 2
    factor = 3
    while (factor * factor) <= value:
        if value % factor == 0:
            return False
        factor += 2
    return True


def run_control(limit):
    """Grade the test against the settled record. Any disagreement is a refusal."""
    print("")
    print("  THE CONTROL. Every Mersenne exponent below %d, against the settled record." % limit)
    print("")
    print("  %8s %10s %10s   %s" % ("exponent", "this test", "known", "verdict"))
    print("  " + ("-" * 52))

    wrong = 0
    checked = 0
    for exponent in range(2, limit):
        if not is_prime_small(exponent):
            continue          # a composite exponent cannot give a Mersenne prime
        mine = lucas_lehmer(exponent)
        known = exponent in KNOWN_MERSENNE_EXPONENTS
        checked += 1
        if mine != known:
            wrong += 1
            print("  %8d %10s %10s   ** DISAGREES **"
                  % (exponent, "prime" if mine else "composite", "prime" if known else "composite"))
        elif known:
            print("  %8d %10s %10s   agrees" % (exponent, "prime", "prime"))

    print("")
    print("  %d exponents tested, %d disagreements" % (checked, wrong))
    if wrong == 0:
        print("  The test reproduces the settled record exactly.")
    else:
        print("  REFUSED. A test that cannot reproduce the known record says nothing about a new one.")
    print("")
    return wrong == 0


def run_ntt_check(limit):
    """Grade the exact transform against Python's own multiply, iteration for iteration."""
    print("")
    print("  THE TRANSFORM, against Python's own multiply. Exact means exact or it is a bug.")
    print("")

    wrong = 0
    for exponent in (13, 17, 19, 31, 61, 89, 107, 127):
        if exponent >= limit:
            continue
        plain = lucas_lehmer(exponent)
        through = lucas_lehmer(exponent, square=ntt_square)
        mark = "agrees" if (plain == through) else "** DISAGREES **"
        wrong += 0 if (plain == through) else 1
        print("  M%-6d  plain %-10s transform %-10s  %s"
              % (exponent, "prime" if plain else "composite",
                 "prime" if through else "composite", mark))

    print("")
    print("  %d disagreements" % wrong)
    print("")
    return wrong == 0


def run_timing():
    """The location of the cost, measured and not assumed."""
    print("")
    print("  WHERE THE COST IS. Iterations are `p`, each squaring a `p`-bit number.")
    print("")
    print("  %8s %12s %14s   %s" % ("exponent", "seconds", "per iteration", "digits of M_p"))
    print("  " + ("-" * 60))

    for exponent in (521, 1279, 2203, 3217, 4253):
        started = time.time()
        lucas_lehmer(exponent)
        took = time.time() - started
        digits = int(exponent * 0.30103) + 1
        print("  %8d %12.3f %14.3f ms   %d"
              % (exponent, took, (took / max(exponent - 2, 1)) * 1000.0, digits))
    print("")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="grade against the settled record")
    parser.add_argument("--ntt", action="store_true", help="grade the transform against Python")
    parser.add_argument("--timing", action="store_true", help="measure where the cost is")
    parser.add_argument("--limit", type=int, default=2300, help="highest exponent to control on")
    args = parser.parse_args()

    if not (args.check or args.ntt or args.timing):
        args.check = True

    passed = True
    if args.check:
        passed = run_control(args.limit) and passed
    if args.ntt:
        passed = run_ntt_check(args.limit) and passed
    if args.timing:
        run_timing()

    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
