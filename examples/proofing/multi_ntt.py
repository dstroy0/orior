"""Multi-prime NTT with CRT: the transform, lifted past what one modulus can carry.

docs-check: quoting
    python examples/proofing/multi_ntt.py --check      against Python's own multiply
    python examples/proofing/multi_ntt.py --reach      how large a number each modulus set carries
docs-check: end quoting

WHY MORE THAN ONE MODULUS

A convolution output is a sum of at most `length` products of two limbs. A single modulus `P` demands

    length * (2^bits - 1)^2  <  P

and at `P` near 2^31, that forces the limb down as the transform grows. Past a few tens of thousands of digits, the
limbs shrink to nothing, and the transform simply cannot carry the number. No adjustment fixes that: it is arithmetic.

THE LIFT. Run the SAME convolution modulo several coprime moduli, and put the results back together with the Chinese
Remainder Theorem. The bound becomes

    length * (2^bits - 1)^2  <  P1 * P2 * ... * Pk

Three 31-bit moduli therefore buy a 94-bit ceiling, and the limb can stay wide while the transform grows. Nothing is an
approximation at any point. Each residue is exact, and CRT reconstruction of a value below the product is exact.

WHY THIS, AND NOT FLOATS

GIMPS squares with floating-point FFTs, and must check a round-off bound every single iteration. One bad rounding
silently corrupts a run of months. Here that failure mode does not exist: the arithmetic is integers modulo proven
primes. ONLY a bound above its limit can go wrong. The check of that runs once, in advance, from the sizes, and not as a
sample per iteration.

The moduli do not come from a table. They come from `twiddle_proof`, each with the Proth witness that proves it prime,
and the generator that proves the root has full order:

    2013265921 = 15 * 2^27 + 1   witness 11
    2281701377 = 17 * 2^27 + 1   witness 3
    3892314113 = 29 * 2^27 + 1   witness 3

No computing libraries. Python's own integers, and this tree's own transform.
"""

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import four_step
import twiddle_proof

# Proven moduli, each `k * 2^27 + 1`, so that every one supports transforms to length 2^27. `twiddle_proof` proves that
# the generator beside each generates the whole group.
MODULI = (
    (2013265921, 31),
    (2281701377, 3),
    (3892314113, 3),
)


def carrying_product(moduli):
    """What the set can hold: a convolution output must stay below this."""
    total = 1
    for prime, _ in moduli:
        total *= prime
    return total


def limb_bits_for(length, ceiling):
    """The largest limb that this ceiling carries at this length. Derived, never chosen.

    The rule is `length * (2^bits - 1)^2 < ceiling`. Past it the product wraps, and the wrap is SILENT: the transform
    returns a well-formed wrong answer. A single 2^31 modulus at length 2048 with 16-bit limbs reaches 2^43, and does
    exactly that. The Lucas-Lehmer control catches it, because seven of eight known Mersenne primes then come out
    composite.
    """
    bits = 1
    while True:
        span = (1 << (bits + 1)) - 1
        if (length * span * span) >= ceiling:
            return bits
        bits += 1


def crt_combine(residues, moduli):
    """Put the residues back together. Exact for any value below the product of the moduli.

    Garner's form, which builds the answer one modulus at a time and never forms a number larger
    than it has to.
    """
    total = residues[0]
    running = moduli[0][0]
    for at in range(1, len(moduli)):
        prime = moduli[at][0]
        # The next digit is how far the running answer is from this residue, in units of `running`.
        gap = (residues[at] - total) % prime
        step = (gap * pow(running % prime, prime - 2, prime)) % prime
        total += running * step
        running *= prime
    return total


def convolve_multi(left, right, moduli):
    """One convolution, taken modulo every modulus, then reassembled."""
    residues = []
    for prime, generator in moduli:
        residues.append(four_step.convolve(list(left), list(right), prime, generator))

    out = []
    for at in range(len(residues[0])):
        column = [residues[which][at] for which in range(len(moduli))]
        out.append(crt_combine(column, moduli))
    return out


def multiply(left_value, right_value, moduli=MODULI):
    """Multiply two integers through the multi-prime transform. Exact or it refuses."""
    if (left_value == 0) or (right_value == 0):
        return 0

    ceiling = carrying_product(moduli)
    widest = max(left_value.bit_length(), right_value.bit_length())

    # Settle the limb and the length together. Each depends on the other, and the walk therefore goes down from the
    # widest limb that could serve, until the bound holds.
    bits = 32
    while True:
        count = (widest + bits - 1) // bits
        length = 1
        while length < (count * 2):
            length *= 2
        allowed = limb_bits_for(length, ceiling)
        if bits <= allowed:
            break
        bits = allowed
        if bits < 1:
            raise ValueError("these moduli cannot carry a number this large; add another")

    mask = (1 << bits) - 1

    def split(value):
        limbs = []
        held = value
        while held > 0:
            limbs.append(held & mask)
            held >>= bits
        return limbs

    left = split(left_value)
    right = split(right_value)
    length = 1
    while length < ((max(len(left), len(right))) * 2):
        length *= 2
    left += [0] * (length - len(left))
    right += [0] * (length - len(right))

    folded = convolve_multi(left, right, moduli)

    total = 0
    for at in range(len(folded) - 1, -1, -1):
        total = (total << bits) + folded[at]
    return total


def run_check():
    """Grade against Python's own multiply, the reference, which is not in doubt."""
    print("")
    print("  THE MULTI-PRIME TRANSFORM, against Python's own multiply.")
    print("")
    print("  %10s %10s %8s   %s" % ("bits in", "bits out", "limb", "verdict"))
    print("  " + ("-" * 48))

    wrong = 0
    seed = 0x9E3779B97F4A7C15
    for width in (64, 256, 1024, 4096, 16384, 65536):
        # Two numbers of this width, built from a fixed mix, so that the run repeats exactly.
        left = 0
        right = 0
        state = seed ^ width
        for _ in range((width // 64) + 1):
            state = (state * 6364136223846793005 + 1442695040888963407) & ((1 << 64) - 1)
            left = (left << 64) | state
            state = (state * 6364136223846793005 + 1442695040888963407) & ((1 << 64) - 1)
            right = (right << 64) | state
        left &= (1 << width) - 1
        right &= (1 << width) - 1
        if (left == 0) or (right == 0):
            continue

        mine = multiply(left, right)
        truth = left * right
        agrees = (mine == truth)
        wrong += 0 if agrees else 1

        count = (max(left.bit_length(), right.bit_length()) + 31) // 32
        length = 1
        while length < (count * 2):
            length *= 2
        print("  %10d %10d %8d   %s"
              % (width, truth.bit_length(),
                 limb_bits_for(length, carrying_product(MODULI)),
                 "agrees" if agrees else "** DISAGREES **"))

    print("")
    print("  %d disagreements" % wrong)
    print("")
    return wrong == 0


def run_reach():
    """How large a number each modulus set carries, stated and not assumed."""
    print("")
    print("  THE REACH. What each set of moduli can carry before the bound is exceeded.")
    print("")
    print("  %8s %14s %8s %16s   %s" % ("moduli", "ceiling bits", "limb", "digits", "at length"))
    print("  " + ("-" * 68))

    for count in range(1, len(MODULI) + 1):
        chosen = MODULI[:count]
        ceiling = carrying_product(chosen)
        for length in (1 << 12, 1 << 16, 1 << 20):
            bits = limb_bits_for(length, ceiling)
            # Half the transform holds the number; the other half is the zero padding.
            carried = (length // 2) * bits
            digits = int(carried * 0.30103)
            print("  %8d %14d %8d %16d   2^%d"
                  % (count, ceiling.bit_length(), bits, digits, length.bit_length() - 1))
    print("")
    print("  A single modulus runs out of limb as the transform grows. Adding moduli lifts the")
    print("  ceiling, and leaves the length alone. That is the purpose of the CRT.")
    print("")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="grade against Python's multiply")
    parser.add_argument("--reach", action="store_true", help="how large a number each set carries")
    args = parser.parse_args()

    if not (args.check or args.reach):
        args.check = True

    passed = True
    if args.check:
        passed = run_check() and passed
    if args.reach:
        run_reach()
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
