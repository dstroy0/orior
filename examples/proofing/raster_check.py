"""Grades the rasterizer against the Python reading, at the same directions and with the same filter.

docs-check: quoting
    python examples/proofing/raster_check.py build/bench/boundary_raster.bin
    python examples/proofing/raster_check.py --check
docs-check: end quoting

WHY THIS EXISTS

Three independent codings of one recurrence live in this tree. `sphere_field.py` carries `legendre_column` and
`harmonics_at` in Python, and `room_view_template.html` carries `legendreColumn` and `harmonicsAt` in JavaScript.
`examples/00_blob_viz_tools/boundary_raster.cu` carries a third in CUDA. Every measured number in this work comes from
the Python, and every picture comes from one of the other two. This file compares them.

The headers of the CUDA benches name a second implementation of a statistic as failure mode fifteen. The usual reason to
forbid it is drift. Here the drift would be invisible. A basis that is wrong in the fine structure draws a plausible
picture, and no checker in this tree reads a picture.

WHAT THE CHECK COMPARES, AND WHAT IT KEEPS FIXED

The rasterizer writes the coefficients and the base gain that it used. For every pixel, it also writes the colatitude,
the longitude and the filter width sigma that it applied. This reads that file, and evaluates the same coefficients
through `sphere_field.harmonics_at` at the same directions, with the same per-pixel filter.

All three stay fixed by design. New coefficients here would compare two readings, and not two bases. A check without
sigma would compare a filtered field against an unfiltered one. It would then report a disagreement that is really a
difference in the request.

WHAT THE ANSWER SHOULD BE

The kernel runs in float32, and its rounding is about 1.2e-7 per operation, over a sum of a hundred and twenty one
terms. The expected worst relative disagreement is therefore near 1e-6, and the reference is the double-precision
Python:
  - near 1e-6: the two bases agree, and the difference is float32 rounding.
  - above 1e-4: a disagreement in the basis, and every picture drawn from it is in question.

A pass here makes the output of the rasterizer evidence, and not a plausible image.
"""

import argparse
import io
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
VIEW = os.path.join(ROOT, "examples", "00_blob_viz_tools")
PROBE = os.path.join(ROOT, "build", "bench", "_raster_probe.bin")
if VIEW not in sys.path:
    sys.path.insert(0, VIEW)

import sphere_field

# Pixels sampled from the frame. A million comparisons in Python is minutes and buys nothing over a
# scattered sample, since the question is whether the bases agree and not where.
SAMPLED = 4000

# Above this, the disagreement is not float32 rounding on a sum of this length.
BASIS_FLOOR = 1.0e-4


def load(path):
    """The rasterizer's dump, as a dictionary. Raises on a short or malformed file."""
    with io.open(path, "rb") as handle:
        raw = handle.read()

    top, width, across, down = struct.unpack_from("<4i", raw, 0)
    at = 16
    coefficients = list(struct.unpack_from("<%df" % width, raw, at))
    at += 4 * width
    gain = list(struct.unpack_from("<%df" % (top + 1), raw, at))
    at += 4 * (top + 1)

    pixels = across * down
    wanted = at + 4 * pixels * 3 + 4 * pixels
    if len(raw) != wanted:
        raise ValueError("dump is %d bytes and the header describes %d" % (len(raw), wanted))

    directions = struct.unpack_from("<%df" % (pixels * 3), raw, at)
    at += 4 * pixels * 3
    values = struct.unpack_from("<%df" % pixels, raw, at)

    return {"top": top, "width": width, "across": across, "down": down,
            "coefficients": coefficients, "gain": gain,
            "directions": directions, "values": values}


def reference(dump, colatitude, longitude, sigma):
    """The field at one direction, in double precision, with the pixel's own filter applied.

    The function builds the gain per pixel, as the kernel does. The base gain carries depth and conduction, and the
    prefilter is the only term that depends on where the pixel is.
    """
    top = dump["top"]
    rows = sphere_field.harmonics_at(top, colatitude, longitude)
    flat = dump["coefficients"]

    tau = 0.5 * sigma * sigma
    total = 0.0
    at = 0
    for degree in range(top + 1):
        soften = math.exp(-degree * (degree + 1.0) * tau)
        weight = dump["gain"][degree] * soften
        row = rows[degree]
        for order in range(2 * degree + 1):
            total += weight * flat[at] * row[order]
            at += 1
    return total


def grade(path, sampled=SAMPLED):
    lines = ["  %s" % os.path.basename(path)]
    dump = load(path)
    pixels = dump["across"] * dump["down"]

    lines.append("  degree %d, %d coefficients, %d by %d"
                 % (dump["top"], dump["width"], dump["across"], dump["down"]))

    # Hit pixels only. A miss carries no direction, and a comparison of one would grade the background.
    hits = [at for at in range(pixels) if dump["directions"][at * 3] >= 0.0]
    lines.append("  %d pixels on the boundary, %d of them sampled" % (len(hits), min(sampled, len(hits))))
    if not hits:
        lines.append("  NOTHING TO GRADE: no pixel landed on the boundary")
        sys.stdout.write("\n".join(lines) + "\n\n1 check(s) failed\n")
        return 1

    step = max(1, len(hits) // sampled)
    taken = hits[::step][:sampled]

    worst_absolute = 0.0
    worst_relative = 0.0
    scale = 0.0
    worst_at = None
    for at in taken:
        colatitude = dump["directions"][at * 3]
        longitude = dump["directions"][at * 3 + 1]
        sigma = dump["directions"][at * 3 + 2]
        theirs = dump["values"][at]
        mine = reference(dump, colatitude, longitude, sigma)

        gap = abs(mine - theirs)
        if abs(mine) > scale:
            scale = abs(mine)
        if gap > worst_absolute:
            worst_absolute = gap
            worst_at = (at % dump["across"], at // dump["across"], colatitude, longitude, sigma,
                        theirs, mine)

    # Relative to the scale of the field over the sampled set, and never per pixel. A pixel with a true value near zero
    # would otherwise divide a rounding error by a rounding error, and report noise as a finding.
    worst_relative = worst_absolute / scale if scale > 0 else float("inf")

    lines.append("  field scale over the sample      %.6f" % scale)
    lines.append("  worst absolute disagreement      %.3e" % worst_absolute)
    lines.append("  worst against the field's scale  %.3e" % worst_relative)

    failed = 0
    if worst_relative > BASIS_FLOOR:
        lines.append("  BASIS DISAGREEMENT: above %.0e, which is not float32 rounding on %d terms"
                     % (BASIS_FLOOR, dump["width"]))
        if worst_at:
            lines.append("    worst at pixel %d,%d: colatitude %.6f longitude %.6f sigma %.3e"
                         % worst_at[:5])
            lines.append("    kernel %.8f against reference %.8f" % worst_at[5:])
        failed += 1
    else:
        lines.append("  the two bases agree to float32, so the picture is the reading")

    # The filter must do something. Otherwise the agreement above is of two unfiltered fields, and says nothing about
    # the anti-alias filter.
    widths = [dump["directions"][at * 3 + 2] for at in taken]
    smallest = min(widths)
    largest = max(widths)
    lines.append("  filter width sigma  %.3e to %.3e radians over the sample" % (smallest, largest))
    if largest <= 0.0:
        lines.append("  THE FILTER IS OFF: every sigma is zero, so nothing was prefiltered")
        failed += 1
    elif largest / max(smallest, 1e-30) < 1.5:
        lines.append("  the width barely varies, so the per-pixel part is not being exercised")

    sys.stdout.write("\n".join(lines) + "\n\n")
    sys.stdout.write("%d check(s) failed\n" % failed)
    return failed


def _check():
    """Properties this tool must have before any verdict it prints is worth reading."""
    lines = []
    failed = 0

    # The reference must reproduce a known value. Degree zero alone is constant on the sphere, and its field is the
    # coefficient times the degree-zero harmonic at any direction.
    top = 4
    flat = [0.0] * ((top + 1) * (top + 1))
    flat[0] = 1.0
    dump = {"top": top, "coefficients": flat, "gain": [1.0] * (top + 1)}
    unit = 1.0 / (2.0 * math.sqrt(math.pi))
    for colatitude, longitude in ((0.4, 1.1), (2.0, -0.7), (math.pi / 2.0, 3.0)):
        got = reference(dump, colatitude, longitude, 0.0)
        if abs(got - unit) > 1e-12:
            lines.append("    FAIL degree zero is not constant: %.12f against %.12f" % (got, unit))
            failed += 1
    lines.append("  degree zero is constant on the sphere, at %.12f" % unit)

    # The filter must attenuate, and it must attenuate the high degrees harder. Otherwise a sigma that does nothing
    # would pass the grade above.
    flat = [0.0] * ((top + 1) * (top + 1))
    flat[top * top + top] = 1.0
    dump = {"top": top, "coefficients": flat, "gain": [1.0] * (top + 1)}
    sharp = reference(dump, 0.9, 0.3, 0.0)
    soft = reference(dump, 0.9, 0.3, 0.2)
    ratio = abs(soft / sharp) if sharp else float("inf")
    lines.append("  a sigma of 0.2 leaves %.4f of a degree %d term" % (ratio, top))
    predicted = math.exp(-top * (top + 1) * 0.5 * 0.2 * 0.2)
    if abs(ratio - predicted) > 1e-9:
        lines.append("    FAIL attenuation is %.6f and the heat kernel says %.6f" % (ratio, predicted))
        failed += 1

    # A malformed dump must raise, and not get a grade. A truncated file read as a short frame would compare a handful
    # of pixels, and report a clean pass.
    broken = struct.pack("<4i", 2, 9, 4, 4) + b"\x00" * 8
    try:
        load_from_bytes = broken
        os.makedirs(os.path.dirname(PROBE), exist_ok=True)
        with io.open(PROBE, "wb") as handle:
            handle.write(load_from_bytes)
        try:
            load(PROBE)
            lines.append("    FAIL a truncated dump was accepted")
            failed += 1
        except (ValueError, struct.error):
            lines.append("  a truncated dump raises instead of being graded")
    finally:
        try:
            os.unlink(PROBE)
        except OSError:
            pass

    lines.append("")
    sys.stdout.write("\n".join(lines) + "\n")
    sys.stdout.write("%d check(s) failed\n" % failed)
    return failed


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="the rasterizer against the Python reading")
    parser.add_argument("dump", nargs="?", help="the .bin the rasterizer wrote")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check:
        sys.exit(1 if _check() else 0)
    if not args.dump:
        sys.stdout.write(__doc__)
        sys.exit(2)
    sys.exit(1 if grade(args.dump) else 0)
