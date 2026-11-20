"""The wave content of a SHA-256 state, read exactly, round by round, against a matched null.

The harmonics of exact_harmonics calibrate only. They show that the invariance of per-degree power
is exact, and does not only sit at the floor of a double. They read no state. This tool reads a
state. It expands the lit set of each round on the sphere. Then it reports where the power of that
set sits across angular scales, the purpose of the expansion.

THE TOOL COMPUTES THE BASIS FIRST, BECAUSE IT DOES NOT DEPEND ON THE STATE

Legendre values and the complex exponential depend on the POINT, not on which bits are lit. Thus the
tool builds the whole basis once at full width. Every round is then a sum of the rows that its lit
bits select. That turns an expensive exact evaluation into an addition per lit bit, and it is the only
reason exact arithmetic is affordable here at all.

THE NULL MATCHES THE POPCOUNT, AND THAT IS NECESSARY

The spectrum of a state depends heavily on how MANY bits are lit, and the weight wanders from round
to round. Thus the null is not a random bit pattern. It is a random pattern with the SAME number of
lit bits as the round under test. Without that match, the reading reports the weight and calls it
structure. That is the fault of the sorted-list band and the periodic generator, by a different
route.

    python utils/maint/audit/wave_trace.py
    python utils/maint/audit/wave_trace.py --top 6 --places 64 --draws 200
"""

import argparse
import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "examples", "proofing"))
sys.path.insert(0, os.path.join(ROOT, "examples", "00_blob_viz_tools"))

import exact_harmonics
import state_deflection

sys.path.insert(0, os.path.join(ROOT, "utils", "maint", "audit"))
import exact_spectral


def build_basis(points, top, places):
    """Per point, the real and imaginary part of every mode. Computed once, exactly."""
    scale = 1 << places
    rows = []
    for x, y, z in points:
        sin_square = scale - ((z * z) >> places)
        sin_theta = exact_spectral.isqrt_scaled(sin_square << places) if sin_square > 0 else 0
        if sin_theta > 0:
            ex = (x << places) // sin_theta
            ey = (y << places) // sin_theta
        else:
            ex, ey = scale, 0

        row = {}
        for degree in range(top + 1):
            mr, mi = scale, 0
            for order in range(degree + 1):
                legendre = exact_harmonics.legendre(degree, order, z, places)
                legendre = (legendre * exact_spectral.normalise(degree, order, places)) >> places
                row[(degree, order)] = ((legendre * mr) >> places, -((legendre * mi) >> places))
                nr = ((mr * ex) - (mi * ey)) >> places
                ni = ((mr * ey) + (mi * ex)) >> places
                mr, mi = nr, ni
        rows.append(row)
    return rows


def spectrum_of(basis, live, top, places):
    """Per-degree power of one lit set, as the sum of the rows that it selects."""
    real = {}
    imag = {}
    for index in live:
        for key, (re, im) in basis[index].items():
            real[key] = real.get(key, 0) + re
            imag[key] = imag.get(key, 0) + im
    power = [0] * (top + 1)
    for (degree, order), re in real.items():
        weight = 1 if order == 0 else 2
        im = imag[(degree, order)]
        power[degree] += weight * (((re * re) >> places) + ((im * im) >> places))
    return power


def main():
    parser = argparse.ArgumentParser(description="Exact wave content of a SHA-256 state.")
    parser.add_argument("--top", type=int, default=5)
    parser.add_argument("--places", type=int, default=64)
    parser.add_argument("--points", type=int, default=128)
    parser.add_argument("--draws", type=int, default=120)
    given = parser.parse_args()

    places, top, count = given.places, given.top, given.points
    print("  %d points, degree %d, %d-bit fixed point, %d null draws per round"
          % (count, top, places, given.draws))
    print("  the null matches the popcount, thus it reports shape and not weight")
    print()

    points = exact_spectral.unit_points(count, places)
    print("  building the basis once...")
    basis = build_basis(points, top, places)

    block = [0x80000000] + [0] * 15
    states = state_deflection.states_of(block)
    rng = random.Random(0x7A7E)

    print()
    print("    round   lit   " + "".join("  P%-8d" % d for d in range(1, top + 1)))
    print("            " + " " * 6 + "  (each cell: standard errors from a popcount-matched null)")
    loud = []
    for step in (1, 4, 8, 12, 16, 24, 32, 48, 64):
        live = [index for index in state_deflection.lit_of(states[step]) if index < count]
        weight = len(live)
        if weight < 4 or weight > count - 4:
            continue
        real_power = spectrum_of(basis, live, top, places)

        # The null: same number of lit points, chosen at random, many times.
        pool = list(range(count))
        draws = [[] for _ in range(top + 1)]
        for _ in range(given.draws):
            rng.shuffle(pool)
            null_power = spectrum_of(basis, pool[:weight], top, places)
            for degree in range(top + 1):
                draws[degree].append(null_power[degree])

        cells = ""
        for degree in range(1, top + 1):
            column = draws[degree]
            middle = sum(column) // len(column)
            spread_sq = sum((v - middle) ** 2 for v in column) // len(column)
            spread = exact_spectral.isqrt_scaled(spread_sq) if spread_sq > 0 else 1
            deviation = real_power[degree] - middle
            z = (deviation * 100) // (spread if spread else 1)
            cells += "  %+8.2f" % (z / 100.0)
            if abs(z) >= 350:
                loud.append((step, degree, z / 100.0))
        print("    %5d   %3d   %s" % (step, weight, cells))

    print()
    print("=" * 76)
    print("  READING")
    print("=" * 76)
    print()
    bar = 3.5
    print("    bar: %.1f standard errors, being the loudest of %d cells under the null"
          % (bar, 9 * top))
    print()
    if loud:
        print("    %d cell(s) clear it:" % len(loud))
        for step, degree, z in loud:
            print("      round %d, degree %d at %+.2f sd" % (step, degree, z))
        print()
        print("    A degree carrying power a popcount-matched null cannot produce is angular")
        print("    structure in the lit set: the bits are not merely the right NUMBER, they sit")
        print("    in a pattern on the sphere.")
    else:
        print("    None clear it. At every sampled round the lit set's power sits across angular")
        print("    scales exactly where a random set of the same weight puts it. The waves are")
        print("    only the weight. A mixed state should look like that, and this")
        print("    measures it, at a width where the format has no say.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
