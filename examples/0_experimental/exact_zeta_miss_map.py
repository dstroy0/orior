#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: EXP-x-030
#
# Where Turing's machine misses a zero, seen from the sampler. Each cell runs twice on the device, on a coarse lattice
# and on a fine one, by the multiple evaluation, every point listed: its sign, S, Z and w = exp(i theta) F, F the main
# sum. A coarse step that holds more of the fine run's zeros than the coarse signs show hides a pair, and each such
# pair is a miss.
#
#   Usage:  python examples/0_experimental/exact_zeta_miss_map.py <binary> [first] [last] [coarse] [fine] [folder]
#           python examples/0_experimental/exact_zeta_miss_map.py <binary> e <base> <cells> <heights> <rate> [folder]
#           python examples/0_experimental/exact_zeta_miss_map.py <binary> carrier <base> <cells> <rate> [folder]
#           python examples/0_experimental/exact_zeta_miss_map.py <binary> ripple <base> <cells> <rate>
#           python examples/0_experimental/exact_zeta_miss_map.py <binary> pulse <base> <cells> <rate>
#           python examples/0_experimental/exact_zeta_miss_map.py <binary> source <base> <cells> <rate>
#           python examples/0_experimental/exact_zeta_miss_map.py <binary> primes <base> <cells> <rate>
#           python examples/0_experimental/exact_zeta_miss_map.py <binary> twist <base> <cells> <rate>
#
# With twist, each cell is listed with the device's F'/F: checked against the fine lattice's own differences of w, and
# each step of the uniform coarse lattice flagged where Newton's step from either end places a source whose pulse
# passes the clock inside it, the flagged steps' share and the misses they hold read.
#
# With ripple, the misses of the uniform lattice at the rate are locked against the beats ln(n / m) of |F|^2, and F's
# drag and swell at their dips read against every point's. With pulse, each place F passes near a zero of its own is
# read against the single pole, its twist a Lorentzian and (swell, twist) a circle, and the misses inside one counted.
# With source, each pulse's source, the zero of F it passes, is placed, locked against the beats, and the misses and
# sources set against the density F'/F's prime lines place. With primes, the sources are locked on ln n for every n to
# N, and the certified zeros of the window read Lambda(x) at every integer past N by Landau, the calls graded by a
# sieve and proved by Proth's witness where his theorem reaches, and psi(x) summed from them.
#
# With carrier, each cell runs once on its fine lattice, and the coarse lattice is placed several ways: the uniform
# baseline at the rate, the lattice even in theta / pi at the rate, each point the first fine one whose theta / pi
# less its bound reaches its mark, from the listing word 3, the antinode trap where Im(w) crosses zero, the turning
# points of Z where Z' = 0, a uniform
# lattice of each trap's own point count, and the {1,1,2} and golden combs at the rate. Each lattice's points a zero,
# misses a thousand zeros against the pigeonhole floor, pickle width and share under us are read, with the turning
# points on the wrong side of zero. The coarse lattice chooses which certified points to compare; it is not part of
# the proof.
#
# With e, the same fields at heights an e-fold apart in t: cells from `base`, √e times it, e times it and on, each
# height's coarse lattice every k-th point of its fine one, k chosen to hold `rate` points a zero at every height, and
# each field read in its height's own units, the radius over its median and the turn over its median turn a step.
# Where the fields at every height fall on one another, the shift of a height by e carries them unchanged.
#
# It sits in 0_experimental and is an entry in the analytic number theory workbook, on its rail. It proves nothing;
# it reads where the misses fall.
#
# THE BALL
#
# Z = 2 Re w + R. w turns with theta, and its size |w| = |F| does not depend on theta at all:
# |F|^2 = sum over m, n of (m n)^(-1/2) cos(t ln(m / n)), the beats of the rotations n^(-it). The sampler stands at its
# last certified coarse point before the miss, the center of a ball of radius |w| there, heading along w's step from
# the point before. A miss is the fine point between its two zeros where |Z| is largest, the bottom of the dip, placed
# relative to the center: its direction from the heading, 0 to 360 degrees, and its distance in radii of the ball and
# in plain units. The ball spins, and a straight heading falls off its curve to one side: dead reckoning runs a
# steady arc through the last two coarse points, turning at the rate the heading turned over the last step, and the
# miss is placed again from that track, the spin taken out.
# The curve of the fine points through each miss is drawn over and under the plane through the center, its height
# the change in Z from the center's, in radii, up away from zero. The misses and the radii go to tab-separated files
# beside the map.
#
# The radius and the turn over a step at every coarse point are the baselines the misses' are read against.

import math
import os
import random
import sys
import tempfile
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "proofing"))
import exact_zeta_turing as tm  # noqa: E402
import twiddle_proof  # noqa: E402

UNIT = float(1 << tm.SCALE_BITS)


def points_of(cell):
    """The listed points of a cell, in lane order: sign, S, Z, Re w and Im w, the last three as reals."""
    out = []
    with open(cell.path) as handle:
        for line in handle:
            if line.startswith("point"):
                sign, big_s, z, re, im = (int(v, 16) for v in line.split()[1:6])
                out.append((sign, big_s, z / UNIT, complex(re / UNIT, im / UNIT)))
    return out


def zeros_of(points):
    """Each sign change between consecutive certified points: the two lanes it lies between."""
    held = [j for j, point in enumerate(points) if point[0] != 0]
    return [(a, b) for a, b in zip(held, held[1:]) if points[a][0] != points[b][0]], held


def misses_of(coarse, fine, lift):
    """Each pair the coarse lattice hides: the coarse step it hides in, and the two fine zeros."""
    fine_zeros, _ = zeros_of(fine)
    _, held = zeros_of(coarse)
    at = 0
    out = []
    for a, b in zip(held, held[1:]):
        low, high = a * lift, b * lift
        inside = []
        while at < len(fine_zeros) and fine_zeros[at][1] <= high:
            if fine_zeros[at][0] >= low:
                inside.append(fine_zeros[at])
            at += 1
        seen = int(coarse[a][0] != coarse[b][0])
        extra = len(inside) - seen
        if extra < 2:
            continue
        gaps = sorted(range(len(inside) - 1), key=lambda k: inside[k + 1][0] - inside[k][0])
        taken = set()
        for k in gaps:
            if len(taken) // 2 >= extra // 2:
                break
            if k in taken or k + 1 in taken:
                continue
            taken |= {k, k + 1}
            out.append((a, b, inside[k], inside[k + 1]))
    return out


def bearing(z):
    return math.degrees(math.atan2(z.imag, z.real)) % 360.0


def place(nu, coarse, fine, a, b, first, second, lift):
    """A miss seen from the center at coarse point a: the dip's direction from the heading and its distance in radii
    and in units; the same from the steady arc dead reckoning predicts, which takes out the curve of the ball's spin;
    the radius, the spin, the speed, and the curve of fine points from a to b in the center's frame, its height the
    change in Z toward zero."""
    center = coarse[a][3]
    behind = coarse[a - 1][3] if a > 0 else center
    before = coarse[a - 2][3] if a > 1 else behind
    heading = center - behind
    turn = heading / abs(heading) if abs(heading) > 0 else 1
    last = behind - before
    rate = math.atan2((heading / last).imag, (heading / last).real) if abs(last) > 0 and abs(heading) > 0 else 0.0
    radius = abs(center)
    toward = -1 if coarse[a][0] > 0 else 1
    between = range(first[1], second[0] + 1)
    dip = max(between, key=lambda j: abs(fine[j][2]))
    u = (dip - a * lift) / lift
    # a steady arc through the last two fixes: the heading is the chord a - 1 to a, the tangent at a is turned half
    # the rate past it, and the arc runs at the chord's length times (rate / 2) / sin(rate / 2) a step
    if abs(rate) > 1e-12:
        arc = (rate / 2) / math.sin(rate / 2)
        track = (heading * complex(math.cos(rate / 2), math.sin(rate / 2)) * arc *
                 (complex(math.cos(rate * u), math.sin(rate * u)) - 1) / complex(0, rate))
    else:
        track = heading * u
    d = (fine[dip][3] - center) / turn
    residual = (fine[dip][3] - center - track) / turn
    # the turn across the step: what the coarse chords show, and the tip's own, summed along the fine curve
    ahead = coarse[b][3] - center
    seen = math.atan2((ahead / heading).imag, (ahead / heading).real) if abs(heading) > 0 and abs(ahead) > 0 else 0.0
    turned, prior = 0.0, None
    for j in range(a * lift, b * lift):
        tangent = fine[j + 1][3] - fine[j][3]
        if prior is not None and abs(prior) > 0 and abs(tangent) > 0:
            turned += math.atan2((tangent / prior).imag, (tangent / prior).real)
        prior = tangent
    curve = []
    for j in range(a * lift, b * lift + 1):
        q = (fine[j][3] - center) / turn / radius
        curve.append((q.real, q.imag, -toward * (fine[j][2] - coarse[a][2]) / (2 * radius)))
    return {"nu": nu, "dip_s": fine[dip][1], "u": u, "angle": bearing(d), "radii": abs(d) / radius, "units": abs(d),
            "residual_angle": bearing(residual), "residual_radii": abs(residual) / radius,
            "residual_steps": abs(residual) / abs(heading) if abs(heading) > 0 else 0.0, "radius": radius,
            "spin": rate, "seen_turn": seen, "true_turn": turned, "speed": abs(heading),
            "dip_radius": abs(fine[dip][3]), "curve": curve}


MISS_KEYS = ("nu", "dip_s", "u", "angle", "radii", "units", "residual_angle", "residual_radii", "residual_steps",
             "radius", "spin", "seen_turn", "true_turn", "speed", "dip_radius")


def write_misses(misses, table):
    """One row a miss, the fields of MISS_KEYS, to the path `table`."""
    with open(table, "w") as handle:
        handle.write("\t".join(MISS_KEYS) + "\n")
        for m in misses:
            handle.write("\t".join(str(m[k]) for k in MISS_KEYS) + "\n")
    return table


def summary(misses, radii):
    radii = sorted(radii)
    n = len(radii)
    tenth = radii[n // 10]
    median = radii[n // 2]
    taken = sorted(m["radius"] for m in misses)
    print("  %d misses over %d coarse points" % (len(misses), n))
    if not misses:
        return
    print("  the ball's radius: median %.4f over every coarse point, %.4f at the misses' centers, %.4f at their dips" %
          (median, taken[len(taken) // 2], sorted(m["dip_radius"] for m in misses)[len(misses) // 2]))
    print("  misses whose center lies in the smallest tenth of radii: %.1f%%, against 10%% were the radius no field" %
          (100.0 * sum(1 for r in taken if r <= tenth) / len(taken)))
    for key, name in (("angle", "from the heading"), ("residual_angle", "from the dead-reckoned track")):
        sectors = [0] * 8
        for m in misses:
            sectors[int(m[key] // 45) % 8] += 1
        print("  direction %s, by 45 degrees from 0: %s" % (name, sectors))
    print("  distance from the dead-reckoned track in radii: median %.3f" %
          sorted(m["residual_radii"] for m in misses)[len(misses) // 2])
    print("  distance in radii: median %.3f; in units: median %.4f" %
          (sorted(m["radii"] for m in misses)[len(misses) // 2], sorted(m["units"] for m in misses)[len(misses) // 2]))


PANEL = 420
MARGIN = 40


def shade(value, top):
    """A color from dark blue at 0 to yellow at `top`."""
    f = max(0.0, min(1.0, value / top)) if top > 0 else 0.0
    return "rgb(%d,%d,%d)" % (int(40 + 213 * f), int(20 + 211 * f), int(110 - 75 * f))


def ninety_fifth(values):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, (95 * len(ordered)) // 100)] if ordered else 1.0


def polar(out, x0, y0, misses, angle, distance, title, top_radius):
    """Heading at the top, bearings clockwise, distance out to the misses' 95th percentile, past it on the rim."""
    cx, cy, r = x0 + PANEL / 2, y0 + PANEL / 2 + 10, PANEL / 2 - MARGIN
    reach = ninety_fifth([m[distance] for m in misses]) or 1.0
    out.append('<text x="%d" y="%d" font-size="13">%s</text>' % (x0 + 10, y0 + 18, title))
    for k in (1, 2, 3, 4):
        out.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="#ccc"/>' % (cx, cy, r * k / 4))
        out.append('<text x="%.1f" y="%.1f" font-size="10" fill="#666">%.3g</text>' %
                   (cx + 3, cy - r * k / 4 - 2, reach * k / 4))
    for degrees in range(0, 360, 45):
        a = math.radians(degrees)
        out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#ddd"/>' %
                   (cx, cy, cx + r * math.sin(a), cy - r * math.cos(a)))
        out.append('<text x="%.1f" y="%.1f" font-size="10" text-anchor="middle">%d</text>' %
                   (cx + (r + 14) * math.sin(a), cy - (r + 14) * math.cos(a) + 4, degrees))
    for m in misses:
        a = math.radians(m[angle])
        d = min(m[distance] / reach, 1.0) * r
        out.append('<circle cx="%.1f" cy="%.1f" r="2" fill="%s"/>' %
                   (cx + d * math.sin(a), cy - d * math.cos(a), shade(m["radius"], top_radius)))


def axes(out, x0, y0, title, xlabel, ylabel):
    left, bottom, right, top = x0 + MARGIN + 10, y0 + PANEL - MARGIN, x0 + PANEL - 10, y0 + MARGIN
    out.append('<text x="%d" y="%d" font-size="13">%s</text>' % (x0 + 10, y0 + 18, title))
    out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#000"/>' % (left, bottom, right, bottom))
    out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#000"/>' % (left, bottom, left, top))
    out.append('<text x="%d" y="%d" font-size="11" text-anchor="middle">%s</text>' %
               ((left + right) / 2, bottom + 28, xlabel))
    out.append('<text x="%d" y="%d" font-size="11" transform="rotate(-90 %d %d)" text-anchor="middle">%s</text>' %
               (x0 + 14, (top + bottom) / 2, x0 + 14, (top + bottom) / 2, ylabel))
    return left, bottom, right, top


def draw(misses, radii, folder):
    """The map as an SVG: three polar panels, the turn against the distance from the track, the radii, and the
    curves."""
    width, height = 3 * PANEL, 2 * PANEL
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="sans-serif">' % (width, height),
           '<rect width="100%" height="100%" fill="white"/>']
    top_radius = ninety_fifth([m["radius"] for m in misses])
    panels = (("angle", "radii", "from the center, heading at 0, in radii"),
              ("angle", "units", "the same, in units"),
              ("residual_angle", "residual_radii", "from the steady arc, the spin taken out, in radii"))
    for at, (angle, distance, title) in enumerate(panels):
        polar(out, at * PANEL, 0, misses, angle, distance, title, top_radius)
    # the turn over the last step against the distance from the steady arc
    left, bottom, right, top = axes(out, 0, PANEL, "the turn before against the distance from the arc",
                                    "turn over the last step, radians", "distance from the arc, radii")
    spin_low, spin_high = min(m["spin"] for m in misses), max(m["spin"] for m in misses)
    reach = ninety_fifth([m["residual_radii"] for m in misses]) or 1.0
    for m in misses:
        x = left + (right - left) * (m["spin"] - spin_low) / ((spin_high - spin_low) or 1.0)
        y = bottom - (bottom - top) * min(m["residual_radii"] / reach, 1.0)
        out.append('<circle cx="%.1f" cy="%.1f" r="2" fill="%s"/>' % (x, y, shade(m["radius"], top_radius)))
    # the radius: every coarse point's, the misses' centers' and their dips', each as a share of its own count
    left, bottom, right, top = axes(out, PANEL, PANEL, "the ball's radius", "|w|, out to the 99th percentile",
                                    "share in each of 40 bins")
    limit = sorted(radii)[(99 * len(radii)) // 100]
    series = ((radii, "#4a7fb5"), ([m["radius"] for m in misses], "#e08a2c"),
              ([m["dip_radius"] for m in misses], "#3a9a3a"))
    counted = []
    for values, color in series:
        bins = [0] * 40
        for v in values:
            if v < limit:
                bins[int(40 * v / limit)] += 1
        counted.append(([b / len(values) for b in bins], color))
    tallest = max(max(bins) for bins, _ in counted) or 1.0
    for bins, color in counted:
        path = []
        for k, share in enumerate(bins):
            x1 = left + (right - left) * k / 40
            x2 = left + (right - left) * (k + 1) / 40
            y = bottom - (bottom - top) * share / tallest
            path.append("%.1f,%.1f %.1f,%.1f" % (x1, y, x2, y))
        out.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="1.6"/>' % (" ".join(path), color))
    for k, (name, color) in enumerate((("every coarse point", "#4a7fb5"), ("misses' centers", "#e08a2c"),
                                       ("misses' dips", "#3a9a3a"))):
        out.append('<text x="%d" y="%d" font-size="11" fill="%s">%s</text>' % (right - 130, top + 14 + 14 * k, color, name))
    # each miss's fine curve, the plane through the center seen at a slant, height the change in Z away from zero
    x0, y0 = 2 * PANEL, PANEL
    out.append('<text x="%d" y="%d" font-size="13">each miss\'s curve over and under the center\'s plane</text>' %
               (x0 + 10, y0 + 18))
    shown = misses[:200]
    span = max([max(abs(a), abs(b), abs(c)) for m in shown for a, b, c in m["curve"]] or [1.0])
    span = min(span, ninety_fifth([max(abs(c) for _, _, c in m["curve"]) for m in shown]) * 3 or span)
    cx, cy, scale = x0 + PANEL / 2, y0 + PANEL / 2 + 20, (PANEL / 2 - MARGIN) / (span or 1.0)
    slant = (math.cos(math.radians(30)) * 0.5, math.sin(math.radians(30)) * 0.5)
    corners = [(-span, -span), (span, -span), (span, span), (-span, span)]
    plane = " ".join("%.1f,%.1f" % (cx + scale * (a + slant[0] * b), cy - scale * (slant[1] * b)) for a, b in corners)
    out.append('<polygon points="%s" fill="#eef" stroke="#99a"/>' % plane)
    for k, m in enumerate(shown):
        points = " ".join("%.1f,%.1f" % (cx + scale * (a + slant[0] * b), cy - scale * (c + slant[1] * b))
                          for a, b, c in m["curve"])
        out.append('<polyline points="%s" fill="none" stroke="hsl(%d,60%%,45%%)" stroke-width="0.7"/>' %
                   (points, (k * 47) % 360))
    out.append('<text x="%d" y="%d" font-size="11">along the heading to the right, across it into the page, up away '
               'from zero; radii</text>' % (x0 + 10, y0 + PANEL - 12))
    out.append("</svg>")
    path = os.path.join(folder, "miss_map.svg")
    with open(path, "w") as handle:
        handle.write("\n".join(out) + "\n")
    return path


def main():
    binary = sys.argv[1]
    first = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    last = int(sys.argv[3]) if len(sys.argv) > 3 else 304
    coarse_p = int(sys.argv[4]) if len(sys.argv) > 4 else 15
    fine_p = int(sys.argv[5]) if len(sys.argv) > 5 else 18
    folder = sys.argv[6] if len(sys.argv) > 6 else tempfile.mkdtemp(prefix="miss_map_")
    sys.stdout.reconfigure(line_buffering=True)
    constants = tm.Constants()
    lift = 1 << (fine_p - coarse_p)
    misses, radii, spins, failed = [], [], [], 0
    for nu in range(first, last + 1):
        coarse_cell, _ = tm.run_cell(binary, constants, nu, coarse_p, "transform", listing=1)
        fine_cell, _ = tm.run_cell(binary, constants, nu, fine_p, "transform", listing=1)
        failed += coarse_cell.failed + fine_cell.failed
        coarse, fine = points_of(coarse_cell), points_of(fine_cell)
        radii.extend(abs(point[3]) for point in coarse)
        for j in range(2, len(coarse)):
            heading, last = coarse[j][3] - coarse[j - 1][3], coarse[j - 1][3] - coarse[j - 2][3]
            if abs(heading) > 0 and abs(last) > 0:
                spins.append(math.atan2((heading / last).imag, (heading / last).real))
        found = [place(nu, coarse, fine, a, b, x, y, lift) for a, b, x, y in misses_of(coarse, fine, lift)]
        misses.extend(found)
        print("  cell %d: 2^%d against 2^%d points, %d misses" % (nu, coarse_p, fine_p, len(found)))
    summary(misses, radii)
    if misses and spins:
        ordered = sorted(spins)
        top = ordered[(9 * len(ordered)) // 10]
        print("  the turn over a step: median %.3f radians at every coarse point, %.3f at the misses' centers; misses "
              "in the largest tenth of turns: %.1f%%" %
              (ordered[len(ordered) // 2], sorted(m["spin"] for m in misses)[len(misses) // 2],
               100.0 * sum(1 for m in misses if m["spin"] >= top) / len(misses)))
        sixth = math.pi / 3
        print("  turns past a sixth of a turn: %.1f%% of every coarse step's; at the misses, %.1f%% of the steps' as "
              "the coarse chords see them and %.1f%% of the tip's own" %
              (100.0 * sum(1 for s in spins if abs(s) > sixth) / len(spins),
               100.0 * sum(1 for m in misses if abs(m["seen_turn"]) > sixth) / len(misses),
               100.0 * sum(1 for m in misses if abs(m["true_turn"]) > sixth) / len(misses)))
        print("  the tip's own turn across a miss's step: deciles %s degrees" %
              ["%.0f" % math.degrees(v) for v in sorted(abs(m["true_turn"]) for m in misses)[len(misses) // 10::
                                                                                             max(1, len(misses) // 10)]])
        with open(os.path.join(folder, "spins.tsv"), "w") as handle:
            handle.write("\n".join("%.6f" % s for s in spins) + "\n")
    table = write_misses(misses, os.path.join(folder, "misses.tsv"))
    with open(os.path.join(folder, "radii.tsv"), "w") as handle:
        handle.write("\n".join("%.6f" % r for r in radii) + "\n")
    print("  the misses: %s" % table)
    print("  the map: %s; %d host checks failed" % (draw(misses, radii, folder), failed))
    return 0 if failed == 0 else 1


def median(values):
    ordered = sorted(values)
    return ordered[len(ordered) // 2] if ordered else 0.0


def fold_cells(binary, constants, nu, count, rate):
    """Cells nu to nu + count - 1, each on a fine lattice, its coarse lattice every k-th fine point with k chosen for
    `rate` points a zero: the misses, every coarse point's radius and turn, the rate held, and the zeros."""
    misses, radii, turns, rates, zeros, failed = [], [], [], [], 0, 0
    for at in range(nu, nu + count):
        z = tm.rises(at)
        fine_p = math.ceil(math.log2(8 * rate * z))
        k = max(2, round((1 << fine_p) / (rate * z)))
        cell, _ = tm.run_cell(binary, constants, at, fine_p, "transform", listing=1)
        failed += cell.failed
        fine = points_of(cell)
        os.remove(cell.path)
        coarse = fine[::k]
        zeros += len(zeros_of(fine)[0])
        rates.append((1 << fine_p) / (k * z))
        radii.extend(abs(point[3]) for point in coarse)
        for j in range(2, len(coarse)):
            heading, last = coarse[j][3] - coarse[j - 1][3], coarse[j - 1][3] - coarse[j - 2][3]
            if abs(heading) > 0 and abs(last) > 0:
                turns.append(abs(math.atan2((heading / last).imag, (heading / last).real)))
        misses.extend(place(at, coarse, fine, a, b, x, y, k) for a, b, x, y in misses_of(coarse, fine, k))
    # the inertia the carrier frame predicts, the mean over t of |F|^2 with its cross terms gone: the sum of 1/n over
    # n <= nu, averaged over the cells
    harmonic = sum(sum(1.0 / n for n in range(1, at + 1)) for at in range(nu, nu + count)) / count
    return {"misses": misses, "radii": radii, "turns": turns, "rate": sum(rates) / len(rates), "zeros": zeros,
            "failed": failed, "nu": nu, "t": 2 * math.pi * nu * nu, "harmonic": harmonic}


def fold_row(fold):
    """The fields of one height in its own units."""
    misses, radii, turns = fold["misses"], fold["radii"], fold["turns"]
    r_mid, t_mid = median(radii), median(turns)
    r_tenth = sorted(radii)[len(radii) // 10]
    t_tenth = sorted(turns)[(9 * len(turns)) // 10]
    pull = sum(complex(math.cos(math.radians(m["residual_angle"])), math.sin(math.radians(m["residual_angle"])))
               for m in misses) / max(1, len(misses))
    # the scatter about the steady arc, in the walker's own frame: it holds its shape from height to height while the
    # walk reads the field, whatever the heading does
    scatter = sorted(m["residual_steps"] for m in misses)
    # under us: within one step of the arc. A miss past it is read as a curl of the field at a scale the walk has not
    # reached, and its own radius and turn say where it stands. The far misses' distance over the core's ninetieth
    # percentile is the separation: it rises as the core tightens under the walk and the far misses stand apart as
    # single dots
    near = [m for m in misses if m["residual_steps"] <= 1.0]
    far = [m for m in misses if m["residual_steps"] > 1.0]
    # the pickle: each miss's distance from the arc split into along-track, parallel to the heading and behind when
    # below zero, and cross-track, across it. The width is the cross-track RMS. Locking a carrier narrows the width
    # first, and the along-track contracts after, the scatter closing under the walk
    along = [m["residual_steps"] * math.cos(math.radians(m["residual_angle"])) for m in misses]
    cross = [m["residual_steps"] * math.sin(math.radians(m["residual_angle"])) for m in misses]
    cross_rms = math.sqrt(sum(c * c for c in cross) / max(1, len(cross)))
    along_rms = math.sqrt(sum(a * a for a in along) / max(1, len(along)))
    return [
        ("t at the first cell", "%.4g" % fold["t"]),
        ("points a zero, coarse", "%.3f" % fold["rate"]),
        ("misses a thousand zeros", "%.3f" % (1000.0 * 2 * len(misses) / max(1, fold["zeros"]))),
        ("median radius, every point", "%.4f" % r_mid),
        ("inertia, the mean of |w|^2", "%.4f" % (sum(r * r for r in radii) / max(1, len(radii)))),
        ("its law, the sum of 1/n to nu", "%.4f" % fold["harmonic"]),
        ("misses' radius over the median", "%.3f" % (median(m["radius"] for m in misses) / r_mid)),
        ("misses in the smallest tenth of radii", "%.1f%%" % (100.0 * sum(1 for m in misses if m["radius"] <= r_tenth) /
                                                             max(1, len(misses)))),
        ("median turn a step, every point", "%.3f" % t_mid),
        ("misses' turn over the median", "%.3f" % (median(abs(m["spin"]) for m in misses) / t_mid)),
        ("misses in the largest tenth of turns", "%.1f%%" % (100.0 * sum(1 for m in misses if abs(m["spin"]) >= t_tenth) /
                                                            max(1, len(misses)))),
        ("bearing from the heading, median", "%.1f" % median(m["angle"] for m in misses)),
        ("distance from the arc, steps, median", "%.3f" % median(m["residual_steps"] for m in misses)),
        ("distance from the arc, steps, tenth", "%.3f" % scatter[len(scatter) // 10] if scatter else "-"),
        ("distance from the arc, steps, ninetieth", "%.3f" % scatter[(9 * len(scatter)) // 10] if scatter else "-"),
        ("pull from the arc: length, bearing", "%.3f at %.0f" % (abs(pull), math.degrees(math.atan2(pull.imag, pull.real)) % 360)),
        ("bearing's spread from the arc, 1 - pull", "%.3f" % (1.0 - abs(pull))),
        ("the pickle's width, cross-track RMS steps", "%.3f" % cross_rms),
        ("the pickle's aspect, cross over along", "%.3f" % (cross_rms / along_rms) if along_rms else "-"),
        ("under us, within a step of the arc", "%.3f%%" % (100.0 * len(near) / max(1, len(misses)))),
        ("past a step: radius, turn over the median", "%.3f, %.3f" % (median(m["radius"] for m in far) / r_mid,
                                                                     median(abs(m["spin"]) for m in far) / t_mid)
         if far else "-"),
        ("past a step: dip radius over the median", "%.3f" % (median(m["dip_radius"] for m in far) / r_mid) if far else "-"),
        ("past a step over the core's ninetieth", "%.2f" % (median(m["residual_steps"] for m in far) /
                                                           (sorted(m["residual_steps"] for m in near)[(9 * len(near)) // 10]
                                                            or 1.0)) if far and near else "-"),
        ("misses", "%d" % len(misses)),
    ]


def draw_folds(folds, folder):
    """The heights side by side: the pull from the arc in steps, the radius over its median, and the turn over its
    median, every point dashed and the misses solid, one color a height."""
    colors = ("#4a7fb5", "#e08a2c", "#3a9a3a", "#a03ab0", "#b03a3a")
    width, height = 3 * PANEL, PANEL
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="sans-serif">' % (width, height),
           '<rect width="100%" height="100%" fill="white"/>']
    cx, cy, r = PANEL / 2, PANEL / 2 + 10, PANEL / 2 - MARGIN
    reach = ninety_fifth([m["residual_steps"] for f in folds for m in f["misses"]]) or 1.0
    out.append('<text x="10" y="18" font-size="13">from the steady arc, heading at 0, in steps; out to the 95th '
               'percentile</text>')
    for k in (1, 2, 3, 4):
        out.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="#ccc"/>' % (cx, cy, r * k / 4))
        out.append('<text x="%.1f" y="%.1f" font-size="10" fill="#666">%.3g</text>' % (cx + 3, cy - r * k / 4 - 2, reach * k / 4))
    for degrees in range(0, 360, 45):
        a = math.radians(degrees)
        out.append('<text x="%.1f" y="%.1f" font-size="10" text-anchor="middle">%d</text>' %
                   (cx + (r + 14) * math.sin(a), cy - (r + 14) * math.cos(a) + 4, degrees))
    for f, color in zip(folds, colors):
        for m in f["misses"]:
            a = math.radians(m["residual_angle"])
            d = min(m["residual_steps"] / reach, 1.0) * r
            out.append('<circle cx="%.1f" cy="%.1f" r="1.6" fill="%s" fill-opacity="0.6"/>' %
                       (cx + d * math.sin(a), cy - d * math.cos(a), color))
    for at, (key, title) in enumerate((("radius", "the radius over its median"), ("turn", "the turn a step over its median"))):
        left, bottom, right, top = axes(out, (at + 1) * PANEL, 0, title, "in the height's own units, to 4", "share")
        curves = []
        for f, color in zip(folds, colors):
            every = f["radii"] if key == "radius" else f["turns"]
            mid = median(every) or 1.0
            ours = [m["radius"] for m in f["misses"]] if key == "radius" else [abs(m["spin"]) for m in f["misses"]]
            for values, dash in ((every, "4,3"), (ours, "")):
                bins = [0] * 40
                for v in values:
                    x = v / mid
                    if x < 4:
                        bins[int(10 * x)] += 1
                curves.append(([b / max(1, len(values)) for b in bins], color, dash))
        tallest = max(max(b) for b, _, _ in curves) or 1.0
        for bins, color, dash in curves:
            path = " ".join("%.1f,%.1f" % (left + (right - left) * (k + 0.5) / 40, bottom - (bottom - top) * share / tallest)
                            for k, share in enumerate(bins))
            out.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="1.4"%s/>' %
                       (path, color, ' stroke-dasharray="%s"' % dash if dash else ""))
    for k, (f, color) in enumerate(zip(folds, colors)):
        out.append('<text x="%d" y="%d" font-size="11" fill="%s">t from %.3g</text>' % (width - 120, 40 + 14 * k, color, f["t"]))
    out.append("</svg>")
    path = os.path.join(folder, "folds.svg")
    with open(path, "w") as handle:
        handle.write("\n".join(out) + "\n")
    return path


def main_folds(binary, base, count, folds, rate, folder):
    """Cells from `base`, then √e times it, e times it and on, `folds` heights an e-fold apart in t, `count` cells
    each, every height's coarse lattice at `rate` points a zero."""
    constants = tm.Constants()
    found = []
    for j in range(folds):
        nu = round(base * math.exp(j / 2))
        fold = fold_cells(binary, constants, nu, count, rate)
        found.append(fold)
        write_misses(fold["misses"], os.path.join(folder, "misses_%d.tsv" % nu))
        print("  cells %d to %d, t from %.4g: %d misses, %d host checks failed" %
              (nu, nu + count - 1, fold["t"], len(fold["misses"]), fold["failed"]))
    rows = [fold_row(f) for f in found]
    print("  %-40s %s" % ("", "".join("%18s" % ("e^%d" % j) for j in range(folds))))
    for at in range(len(rows[0])):
        print("  %-40s %s" % (rows[0][at][0], "".join("%18s" % row[at][1] for row in rows)))
    failed = sum(f["failed"] for f in found)
    print("  the heights: %s; %d host checks failed" % (draw_folds(found, folder), failed))
    return 0 if failed == 0 else 1


def comb_indices(count_fine, step, pattern, shuffle=False, rng=None):
    """Coarse fine-indices whose gaps cycle through `pattern`, the carrier's partial quotients, scaled to mean `step`.
    A uniform lattice is the pattern (1,); {1,1,2} is a period-3 kick. With shuffle the gaps are permuted, the
    carrier's multiset with its order gone, the drawn null. Placement is not part of the proof: every fine point is
    certified whichever points the coarse lattice keeps."""
    scale = step * len(pattern) / sum(pattern)
    letters = [pattern[i % len(pattern)] for i in range((count_fine // max(1, step) + len(pattern)) * 2 + 8)]
    if shuffle and rng is not None:
        rng.shuffle(letters)
    idx, acc = [0], 0.0
    for length in letters:
        acc += length * scale
        j = int(round(acc))
        if j >= count_fine:
            break
        if j > idx[-1]:
            idx.append(j)
    return idx


def metallic_indices(count_fine, step, n, shuffle=False, rng=None):
    """Coarse fine-indices of the metallic quasicrystal {n,n,n,...}: the Sturmian word of slope 1 / beta, beta the
    metallic mean (n + sqrt(n^2 + 4)) / 2, two gaps in ratio beta scaled to mean `step`. n = 1 is golden, n = 2 is
    silver. With shuffle the word is permuted, the null."""
    beta = (n + math.sqrt(n * n + 4)) / 2
    omega = 1.0 / beta
    short = step / ((1.0 - omega) + beta * omega)
    long = beta * short
    count = int(count_fine / short) + 8
    word = [int(math.floor((i + 1) * omega) - math.floor(i * omega)) for i in range(count)]
    if shuffle and rng is not None:
        rng.shuffle(word)
    idx, acc = [0], 0.0
    for bit in word:
        acc += long if bit else short
        j = int(round(acc))
        if j >= count_fine:
            break
        if j > idx[-1]:
            idx.append(j)
    return idx


def antinode_indices(fine):
    """Coarse fine-indices at the antinodes of Z, where Im(w) changes sign: w crosses the real axis and |Z| sits near a
    lobe peak. A zero is where Re(w) crosses zero, on the imaginary axis, and the two axes alternate as w winds: each
    zero is trapped between two antinodes. A pair hides only where w wiggles, crossing the imaginary axis twice with no
    real-axis crossing between. This is the filter built to trap, not to space."""
    idx = [0]
    for j in range(1, len(fine)):
        if (fine[j - 1][3].imag <= 0.0) != (fine[j][3].imag <= 0.0):
            near = j if abs(fine[j][3].imag) <= abs(fine[j - 1][3].imag) else j - 1
            if near > idx[-1]:
                idx.append(near)
    if idx[-1] != len(fine) - 1:
        idx.append(len(fine) - 1)
    return idx


def extremum_indices(fine):
    """Coarse fine-indices at the turning points of Z, where its step changes sign: the zeros of Z' as the fine lattice
    sees them. Z = 2 |F| cos(phi) with phi = theta + arg F, and Z' = 2 |F| ((ln |F|)' cos(phi) - phi' sin(phi)): the
    antinode moves off Im(w) = 0 by F's own turning, phi' against theta', and by the swell of its size, (ln |F|)'. By
    Rolle, Z is monotone between consecutive turning points and holds at most one zero there."""
    idx = [0]
    for j in range(1, len(fine) - 1):
        if (fine[j][2] - fine[j - 1][2] > 0.0) != (fine[j + 1][2] - fine[j][2] > 0.0):
            idx.append(j)
    idx.append(len(fine) - 1)
    return idx


def wiggles_of(idx, fine):
    """Turning points on the wrong side of zero: a positive minimum or a negative maximum of Z. Each is a turn with no
    zero across it, a pair of critical points past the one each gap between zeros must hold."""
    count = 0
    for j in idx[1:-1]:
        low = fine[j][2] < fine[j - 1][2]
        if (low and fine[j][2] > 0.0) or (not low and fine[j][2] < 0.0):
            count += 1
    return count


def uniform_indices(count_fine, points):
    """A uniform coarse lattice of about `points` indices across the fine lattice, for a density-matched comparison."""
    step = max(1, count_fine // max(1, points))
    return list(range(0, count_fine, step))


def misses_on(idx, fine):
    """Each pair the coarse lattice of fine-indices `idx` hides: the two held coarse points it lies between and the two
    fine zeros. The carrier-stepped form of misses_of, with an explicit index list in place of a uniform lift."""
    fine_zeros, _ = zeros_of(fine)
    coarse = [fine[i] for i in idx]
    _, held = zeros_of(coarse)
    at, out = 0, []
    for a, b in zip(held, held[1:]):
        low, high = idx[a], idx[b]
        inside = []
        while at < len(fine_zeros) and fine_zeros[at][1] <= high:
            if fine_zeros[at][0] >= low:
                inside.append(fine_zeros[at])
            at += 1
        seen = int(coarse[a][0] != coarse[b][0])
        extra = len(inside) - seen
        if extra < 2:
            continue
        gaps = sorted(range(len(inside) - 1), key=lambda k: inside[k + 1][0] - inside[k][0])
        taken = set()
        for k in gaps:
            if len(taken) // 2 >= extra // 2:
                break
            if k in taken or k + 1 in taken:
                continue
            taken |= {k, k + 1}
            out.append((a, b, inside[k], inside[k + 1]))
    return out


def place_on(nu, idx, fine, a, b, first, second):
    """A miss placed from the center at coarse point a, the carrier-stepped form of place: the dip from the steady arc
    the last two coarse points turn along, split into along-track and cross-track, in steps of the last coarse chord."""
    coarse = [fine[i] for i in idx]
    center = coarse[a][3]
    behind = coarse[a - 1][3] if a > 0 else center
    before = coarse[a - 2][3] if a > 1 else behind
    heading = center - behind
    turn = heading / abs(heading) if abs(heading) > 0 else 1
    last = behind - before
    rate = math.atan2((heading / last).imag, (heading / last).real) if abs(last) > 0 and abs(heading) > 0 else 0.0
    radius = abs(center)
    step_a = (idx[a] - idx[a - 1]) if a > 0 else (idx[a + 1] - idx[a])
    between = range(first[1], second[0] + 1)
    dip = max(between, key=lambda j: abs(fine[j][2]))
    u = (dip - idx[a]) / step_a if step_a else 0.0
    if abs(rate) > 1e-12:
        arc = (rate / 2) / math.sin(rate / 2)
        track = (heading * complex(math.cos(rate / 2), math.sin(rate / 2)) * arc *
                 (complex(math.cos(rate * u), math.sin(rate * u)) - 1) / complex(0, rate))
    else:
        track = heading * u
    d = (fine[dip][3] - center) / turn
    residual = (fine[dip][3] - center - track) / turn
    steps = abs(residual) / abs(heading) if abs(heading) > 0 else 0.0
    ang = math.radians(bearing(residual))
    return {"nu": nu, "residual_angle": bearing(residual), "residual_steps": steps, "radius": radius,
            "along": steps * math.cos(ang), "cross": steps * math.sin(ang), "d_steps": abs(d) / abs(heading)
            if abs(heading) > 0 else 0.0}


def pickle_of(misses):
    """The pickle of a run: the cross-track RMS (the width), the along-track RMS, the aspect, and the share within a
    step of the arc."""
    if not misses:
        return (0.0, 0.0, 0.0, 0.0, 0)
    n = len(misses)
    cross = math.sqrt(sum(m["cross"] ** 2 for m in misses) / n)
    along = math.sqrt(sum(m["along"] ** 2 for m in misses) / n)
    under = 100.0 * sum(1 for m in misses if m["d_steps"] <= 1.0) / n
    return (cross, along, cross / along if along else 0.0, under, n)


CARRIER_NAMES = ["uniform @ rate", "theta / pi even @ rate", "antinode Im w=0", "uniform @ antinode n", "turning Z'=0",
                 "uniform @ turning n", "comb 1,1,2 @ rate", "golden @ rate"]


def lows_of(cell):
    """Each listed point's theta / pi less its bound, an integer at 2^-62, from the listing word 3."""
    with open(cell.path) as handle:
        return [int(line.split()[6], 16) for line in handle if line.startswith("point")]


def theta_indices(lows, rate):
    """The lattice even in theta / pi at `rate` points a unit: the first fine point whose theta / pi less its bound
    reaches the cell's first point's plus k / rate, for each k in turn, by integer comparison alone; `rate` read as
    the exact decimal it is written as."""
    exact = Fraction(str(rate))
    num, den = exact.numerator, exact.denominator
    unit, origin, out, k = 1 << tm.SCALE_BITS, lows[0], [], 0
    for j, low in enumerate(lows):
        reached = int((low - origin) * num >= k * unit * den)
        out += [j] * reached
        k += reached * ((low - origin) * num // (unit * den) + 1 - k)
    return out


def carrier_lattices(fine, step, lows, rate):
    """The coarse lattices to score, by name: the uniform baseline at the rate, the antinode trap where Im(w) crosses
    zero, the turning points of Z, each with a uniform lattice of its own point count for a density-matched
    comparison, and the {1,1,2} and golden combs at the rate for reference."""
    anti = antinode_indices(fine)
    turn = extremum_indices(fine)
    return {
        "uniform @ rate": comb_indices(len(fine), step, (1,)),
        "theta / pi even @ rate": theta_indices(lows, rate),
        "antinode Im w=0": anti,
        "uniform @ antinode n": uniform_indices(len(fine), len(anti)),
        "turning Z'=0": turn,
        "uniform @ turning n": uniform_indices(len(fine), len(turn)),
        "comb 1,1,2 @ rate": comb_indices(len(fine), step, (1, 1, 2)),
        "golden @ rate": metallic_indices(len(fine), step, 1),
    }


def main_carrier(binary, base, count, rate, folder):
    """Cells `base` to base + count - 1 on one fine lattice each. For each coarse lattice, place the misses it hides and
    report the misses a thousand zeros, the pickle's width and the share under us. The antinode lattice traps a zero
    between each pair of peaks; the density-matched uniform is the fair baseline for it."""
    constants = tm.Constants()
    tally = {name: [] for name in CARRIER_NAMES}
    points = {name: 0 for name in CARRIER_NAMES}
    floor = {name: 0 for name in CARRIER_NAMES}
    zeros_total, failed, wiggles = 0, 0, 0
    for at in range(base, base + count):
        z = tm.rises(at)
        fine_p = math.ceil(math.log2(8 * rate * z))
        step = max(2, round((1 << fine_p) / (rate * z)))
        cell, _ = tm.run_cell(binary, constants, at, fine_p, "transform", listing=3)
        failed += cell.failed
        fine, lows = points_of(cell), lows_of(cell)
        os.remove(cell.path)
        zeros = len(zeros_of(fine)[0])
        zeros_total += zeros
        for name, idx in carrier_lattices(fine, step, lows, rate).items():
            tally[name].extend(place_on(at, idx, fine, a, b, x, y) for a, b, x, y in misses_on(idx, fine))
            points[name] += len(idx)
            floor[name] += max(0, zeros - (len(idx) - 1))
            if name == "turning Z'=0":
                wiggles += wiggles_of(idx, fine)
        print("  cell %d done, %d fine points, step %d" % (at, len(fine), step), flush=True)
    print("  %-22s %8s %9s %12s %12s %9s %9s" % ("scheme", "points", "a zero", "miss/1k zero", "floor/1k", "width",
                                                 "under%"))
    for name in CARRIER_NAMES:
        cross, along, aspect, under, n = pickle_of(tally[name])
        permille = 1000.0 * 2 * n / max(1, zeros_total)
        print("  %-22s %8d %9.4f %12.3f %12.3f %9.4f %8.2f%%" % (
            name, points[name], points[name] / max(1, zeros_total), permille,
            1000.0 * floor[name] / max(1, zeros_total), cross, under))
    print("  zeros %d; turning points on the wrong side of zero %d (%.3f a thousand zeros); %d host checks failed" % (
        zeros_total, wiggles, 1000.0 * wiggles / max(1, zeros_total), failed))
    return 0 if failed == 0 else 1


RIPPLE_PAIRS = [(1, 2), (1, 3), (2, 3), (1, 4), (3, 4), (1, 5), (2, 5), (1, 6)]
RIPPLE_NULLS = [0.5, 0.9, 1.3, 1.9, 2.3]


def ripple_at(fine, x2, j):
    """F's drag and swell at fine point j, each over theta' = ln x: phi' / theta', 1 where F does not turn and below 0
    where the clock runs back, and (ln |F|)' / theta', the plane lifting. phi is arg w, read across j - 1 to j + 1."""
    low, high = max(0, j - 1), min(len(fine) - 1, j + 1)
    w0, w1 = fine[low][3], fine[high][3]
    dt = 2 * math.pi * (x2[high] - x2[low])
    clock = 0.5 * math.log(x2[j])
    if abs(w0) == 0.0 or abs(w1) == 0.0 or dt == 0.0:
        return 1.0, 0.0
    turn = w1 / w0
    return (math.atan2(turn.imag, turn.real) / dt / clock,
            (math.log(abs(w1)) - math.log(abs(w0))) / dt / clock)


def rayleigh(times, omega):
    """The mean resultant length of the phases omega t over `times`, its direction in degrees, and Rayleigh's z = n R^2,
    the chance of so tight a lock among uniform phases near exp(-z)."""
    if not times:
        return 0.0, 0.0, 0.0
    c = sum(math.cos(omega * t) for t in times) / len(times)
    s = sum(math.sin(omega * t) for t in times) / len(times)
    r = math.hypot(c, s)
    return r, math.degrees(math.atan2(s, c)) % 360.0, len(times) * r * r


def main_ripple(binary, base, count, rate):
    """Cells `base` to base + count - 1 on one fine lattice each, the uniform coarse lattice at the rate. The misses'
    times are locked against the beats of |F|^2 = sum over m, n of (m n)^(-1/2) cos(t ln(m / n)), at the frequencies
    ln(n / m) and at null frequencies between them; F's drag and swell at the misses' dips are read against every fine
    point's."""
    constants = tm.Constants()
    dips, every, failed = [], [], 0
    for at in range(base, base + count):
        z = tm.rises(at)
        fine_p = math.ceil(math.log2(8 * rate * z))
        step = max(2, round((1 << fine_p) / (rate * z)))
        cell, _ = tm.run_cell(binary, constants, at, fine_p, "transform", listing=1)
        failed += cell.failed
        fine = points_of(cell)
        os.remove(cell.path)
        x2 = [at * at + j * (2 * at + 1) / len(fine) for j in range(len(fine))]
        for j in range(1, len(fine) - 1, 7):
            every.append(ripple_at(fine, x2, j))
        idx = comb_indices(len(fine), step, (1,))
        for a, b, first, second in misses_on(idx, fine):
            dip = max(range(first[1], second[0] + 1), key=lambda j: abs(fine[j][2]))
            drag, swell = ripple_at(fine, x2, dip)
            dips.append((2 * math.pi * x2[dip], drag, swell, abs(fine[dip][3])))
        print("  cell %d done, %d misses so far" % (at, len(dips)), flush=True)
    times = [d[0] for d in dips]
    print("  misses %d; the lock of their times on each beat of |F|^2" % len(dips))
    print("  %-14s %10s %9s %10s %9s" % ("beat", "omega", "R", "direction", "z"))
    for m, n in RIPPLE_PAIRS:
        omega = math.log(n / m)
        r, ang, zz = rayleigh(times, omega)
        print("  %-14s %10.5f %9.4f %10.1f %9.2f" % ("ln(%d/%d)" % (n, m), omega, r, ang, zz))
    for omega in RIPPLE_NULLS:
        r, ang, zz = rayleigh(times, omega)
        print("  %-14s %10.5f %9.4f %10.1f %9.2f" % ("null", omega, r, ang, zz))

    def quantiles(rows, key):
        values = sorted(key(row) for row in rows)
        return [values[int(q * (len(values) - 1))] for q in (0.1, 0.5, 0.9)]
    print("  %-34s %28s %28s" % ("reading", "every point 10/50/90", "the misses' dips 10/50/90"))
    for label, key_all, key_dip in [
            ("drag phi' / theta'", lambda r: r[0], lambda d: d[1]),
            ("|swell| (ln |F|)' / theta'", lambda r: abs(r[1]), lambda d: abs(d[2]))]:
        print("  %-34s %28s %28s" % (label, " ".join("%9.3f" % v for v in quantiles(every, key_all)),
                                     " ".join("%9.3f" % v for v in quantiles(dips, key_dip))))
    back_all = 100.0 * sum(1 for r in every if r[0] < 0.0) / max(1, len(every))
    back_dip = 100.0 * sum(1 for d in dips if d[1] < 0.0) / max(1, len(dips))
    print("  clock running back, phi' < 0: every point %.2f%%, the misses' dips %.2f%%" % (back_all, back_dip))
    print("  %d host checks failed" % failed)
    return 0 if failed == 0 else 1


def pulses_of(fine, x2, reach=4.0, least=4):
    """The twist pulses F casts off where it passes near a zero of its own, t* = gamma + i delta off the real t axis.
    There d/dt ln F = 1 / (t - t*): the swell (ln |F|)' = u / (u^2 + delta^2) and the twist (arg F)' = delta /
    (u^2 + delta^2), u = t - gamma, one pole. The twist is a Lorentzian of height 1 / delta and turn pi, a parabola at
    its top, and (swell, twist) runs a circle of diameter 1 / delta through the origin; the neighbors and F's curve
    make it an egg. At each local minimum of |F| with delta = |F| / |F'| at least `least` fine steps and under the
    clock's own 1 / theta', the pulses past the clock, back or forward, read over `reach` deltas each side: the peak twist
    times delta, the loop's diameter (x^2 + y^2) / y over 1 / delta at its 10th and 90th percentiles, the swell's lead
    against its trail, the turn over the window against 2 atan(reach), and the window's fine indices."""
    out = []
    dt = 2 * math.pi * (x2[1] - x2[0])
    for j in range(2, len(fine) - 2):
        if not (abs(fine[j][3]) < abs(fine[j - 1][3]) and abs(fine[j][3]) <= abs(fine[j + 1][3])):
            continue
        clock = 0.5 * math.log(x2[j])
        w = fine[j][3]
        f_prime = abs((fine[j + 1][3] - fine[j - 1][3]) / (2 * dt) - complex(0, clock) * w)
        if f_prime == 0.0:
            continue
        delta = abs(w) / f_prime
        span = int(math.ceil(reach * delta / dt))
        if delta < least * dt or delta * clock >= 1.0 or j - span < 1 or j + span > len(fine) - 2:
            continue
        xs, ys = [], []
        for k in range(j - span, j + span + 1):
            turn = fine[k + 1][3] / fine[k - 1][3]
            twist = math.atan2(turn.imag, turn.real) / (2 * dt) - 0.5 * math.log(x2[k])
            swell = (math.log(abs(fine[k + 1][3])) - math.log(abs(fine[k - 1][3]))) / (2 * dt)
            xs.append(swell)
            ys.append(twist)
        sign = 1.0 if ys[span] > 0 else -1.0
        peak = max(sign * y for y in ys)
        loop = sorted((x * x + y * y) / (sign * y) * delta for x, y in zip(xs, ys) if sign * y > 0.25 * peak)
        lead, trail = -min(xs[:span + 1]), max(xs[span:])
        out.append({"j": j, "delta": delta, "clock": clock, "peak": peak * delta, "sign": sign,
                    "loop_low": loop[len(loop) // 10] if loop else 0.0,
                    "loop_high": loop[(9 * len(loop)) // 10] if loop else 0.0,
                    "egg": lead / trail if trail > 0 else 0.0,
                    "turn": sign * sum(ys) * dt / (2 * math.atan(reach)),
                    "low": j - span, "high": j + span})
    return out


def main_pulse(binary, base, count, rate):
    """Cells `base` to base + count - 1 on one fine lattice each: every twist pulse past the clock, read against the
    single pole, and the misses of the uniform coarse lattice at the rate, each counted inside a pulse or not and read
    across its two zeros. theta is monotone, and two zeros inside one coarse step want phi = theta + arg F to run back
    across a level, phi' < 0, or to sweep pi inside the step, phi' at least the rate times theta'; R moves the level
    by R / (2 |F|), the way left."""
    constants = tm.Constants()
    pulses, misses, inside, covered, total, failed = [], 0, 0, 0, 0, 0
    ways = {"back": 0, "spin": 0, "neither": 0, "neither_most": 0.0}
    for at in range(base, base + count):
        z = tm.rises(at)
        fine_p = math.ceil(math.log2(8 * rate * z))
        step = max(2, round((1 << fine_p) / (rate * z)))
        cell, _ = tm.run_cell(binary, constants, at, fine_p, "transform", listing=1)
        failed += cell.failed
        fine = points_of(cell)
        os.remove(cell.path)
        x2 = [at * at + j * (2 * at + 1) / len(fine) for j in range(len(fine))]
        found = pulses_of(fine, x2)
        pulses.extend(found)
        marks = bytearray(len(fine))
        for p in found:
            marks[p["low"]:p["high"] + 1] = b"\x01" * (p["high"] - p["low"] + 1)
        covered += sum(marks)
        total += len(fine)
        for a, b, first, second in misses_on(comb_indices(len(fine), step, (1,)), fine):
            dip = max(range(first[1], second[0] + 1), key=lambda j: abs(fine[j][2]))
            misses += 1
            inside += marks[dip]
            drags = [ripple_at(fine, x2, j)[0] for j in range(first[0], second[1] + 1)]
            if min(drags) < 0.0:
                ways["back"] += 1
            elif max(drags) >= rate:
                ways["spin"] += 1
            else:
                ways["neither"] += 1
                ways["neither_most"] = max(ways["neither_most"], max(drags))
        print("  cell %d done, %d pulses so far" % (at, len(pulses)), flush=True)

    def q(key):
        values = sorted(p[key] for p in pulses)
        return " ".join("%8.3f" % values[int(f * (len(values) - 1))] for f in (0.1, 0.5, 0.9)) if values else "-"
    back = sum(1 for p in pulses if p["sign"] < 0)
    print("  pulses past the clock, delta theta' under 1: %d, %d turning against theta and %d with it" % (
        len(pulses), back, len(pulses) - back))
    print("  %-40s %26s" % ("against the single pole", "10/50/90"))
    for p in pulses:
        p["clock_delta"] = p["delta"] * p["clock"]
    for label, key in [("delta theta', under 1 past the clock", "clock_delta"), ("peak twist times delta, pole 1", "peak"),
                       ("loop diameter low over 1/delta, pole 1", "loop_low"),
                       ("loop diameter high over 1/delta, pole 1", "loop_high"),
                       ("swell lead over trail, pole 1", "egg"), ("turn over 2 atan(4), pole 1", "turn")]:
        print("  %-40s %26s" % (label, q(key)))
    print("  misses %d, inside a pulse %d (%.1f%%); the pulses cover %.2f%% of the fine points" % (
        misses, inside, 100.0 * inside / max(1, misses), 100.0 * covered / max(1, total)))
    print("  each miss across its two zeros: the clock runs back %d, spins past %.2f theta' %d, neither %d (the most "
          "drag among them %.3f theta')" % (ways["back"], rate, ways["spin"], ways["neither"], ways["neither_most"]))
    print("  %d host checks failed" % failed)
    return 0 if failed == 0 else 1


SOURCE_BANDS = [0.25, 0.5, 1.0, 2.0, 4.0]
SOURCE_HARMONICS = [2, 3, 4, 6, 10, 20, 40]


def harmonic_size(t, k):
    """|F_k(t)|, F cut to its first k harmonics, the sum over n up to k of n^(-1/2) exp(-i t ln n)."""
    re = sum(math.cos(t * math.log(n)) / math.sqrt(n) for n in range(1, k + 1))
    im = sum(math.sin(t * math.log(n)) / math.sqrt(n) for n in range(1, k + 1))
    return math.hypot(re, im)


def mangoldt(n):
    """Lambda(n): ln p where n is a power of the prime p, else 0."""
    for p in range(2, n + 1):
        if n % p == 0:
            while n % p == 0:
                n //= p
            return math.log(p) if n == 1 else 0.0
    return 0.0


def source_density(t, k):
    """The sources' density read from F'/F cut at k: -(sum over n up to k of Lambda(n) n^(-1/2) cos(t ln n)), the
    lines log F shares with log zeta for every n up to N."""
    return -sum(mangoldt(n) * math.cos(t * math.log(n)) / math.sqrt(n) for n in range(2, k + 1))


def main_source(binary, base, count, rate):
    """Cells `base` to base + count - 1 on one fine lattice each. Each local minimum of |F| is a source, a zero of F at
    t* = gamma + i delta: gamma where |F| is least, |delta| = |F| / |F'|, its side the sign of the twist there. The
    sources a zero of Z, by delta theta'; their gammas locked on the beats ln(n / m); and F cut to its first k
    harmonics, its dents read against the misses' dips and the sources past the clock: the share of each in the
    lowest fifth of |F_k|, a fifth where the harmonics place nothing."""
    constants = tm.Constants()
    sources, dips, sample, zeros_total, failed = [], [], [], 0, 0
    for at in range(base, base + count):
        z = tm.rises(at)
        fine_p = math.ceil(math.log2(8 * rate * z))
        step = max(2, round((1 << fine_p) / (rate * z)))
        cell, _ = tm.run_cell(binary, constants, at, fine_p, "transform", listing=1)
        failed += cell.failed
        fine = points_of(cell)
        os.remove(cell.path)
        zeros_total += len(zeros_of(fine)[0])
        x2 = [at * at + j * (2 * at + 1) / len(fine) for j in range(len(fine))]
        dt = 2 * math.pi * (x2[1] - x2[0])
        for j in range(1, len(fine) - 1):
            w = fine[j][3]
            if not (abs(w) < abs(fine[j - 1][3]) and abs(w) <= abs(fine[j + 1][3])):
                continue
            clock = 0.5 * math.log(x2[j])
            f_rel = ((fine[j + 1][3] - fine[j - 1][3]) / (2 * dt) - complex(0, clock) * w) / w if abs(w) else 0j
            if f_rel == 0j:
                continue
            delta = math.copysign(1.0 / abs(f_rel), f_rel.imag)
            sources.append((2 * math.pi * x2[j], delta, abs(delta) * clock))
        for j in range(0, len(fine), 64):
            sample.append(2 * math.pi * x2[j])
        for a, b, first, second in misses_on(comb_indices(len(fine), step, (1,)), fine):
            dip = max(range(first[1], second[0] + 1), key=lambda j: abs(fine[j][2]))
            dips.append(2 * math.pi * x2[dip])
        print("  cell %d done, %d sources so far" % (at, len(sources)), flush=True)
    print("  zeros of Z %d, sources %d, %.4f a zero; Langer's count for a sum to ln N, one source each two zeros" % (
        zeros_total, len(sources), len(sources) / max(1, zeros_total)))
    print("  %-26s %10s %10s %10s" % ("delta theta' under", "sources", "a zero", "back share"))
    for band in SOURCE_BANDS:
        inside = [s for s in sources if s[2] < band]
        back = sum(1 for s in inside if s[1] < 0)
        print("  %-26s %10d %10.4f %9.1f%%" % (band, len(inside), len(inside) / max(1, zeros_total),
                                              100.0 * back / max(1, len(inside))))
    near = [s[0] for s in sources if s[2] < 1.0]
    print("  the lock of the sources past the clock, %d, on each beat of |F|^2" % len(near))
    print("  %-14s %10s %9s %10s %9s" % ("beat", "omega", "R", "direction", "z"))
    for m, n in RIPPLE_PAIRS:
        r, ang, zz = rayleigh(near, math.log(n / m))
        print("  %-14s %10.5f %9.4f %10.1f %9.2f" % ("ln(%d/%d)" % (n, m), math.log(n / m), r, ang, zz))
    for omega in RIPPLE_NULLS:
        r, ang, zz = rayleigh(near, omega)
        print("  %-14s %10.5f %9.4f %10.1f %9.2f" % ("null", omega, r, ang, zz))
    print("  %-10s %16s %22s %24s" % ("harmonics", "lowest fifth at", "misses in it", "sources past the clock"))
    for k in SOURCE_HARMONICS:
        cut = sorted(harmonic_size(t, k) for t in sample)[len(sample) // 5]
        hit_dips = sum(1 for t in dips if harmonic_size(t, k) <= cut)
        hit_near = sum(1 for t in near if harmonic_size(t, k) <= cut)
        print("  %-10d %16.4f %14d (%5.1f%%) %16d (%5.1f%%)" % (
            k, cut, hit_dips, 100.0 * hit_dips / max(1, len(dips)), hit_near, 100.0 * hit_near / max(1, len(near))))
    print("  %-10s %16s %22s %24s" % ("lines to", "highest fifth at", "misses in it", "sources past the clock"))
    for k in SOURCE_HARMONICS:
        cut = sorted(source_density(t, k) for t in sample)[(4 * len(sample)) // 5]
        hit_dips = sum(1 for t in dips if source_density(t, k) >= cut)
        hit_near = sum(1 for t in near if source_density(t, k) >= cut)
        print("  %-10d %16.4f %14d (%5.1f%%) %16d (%5.1f%%)" % (
            k, cut, hit_dips, 100.0 * hit_dips / max(1, len(dips)), hit_near, 100.0 * hit_near / max(1, len(near))))
    print("  %d host checks failed" % failed)
    return 0 if failed == 0 else 1


def sieve_mangoldt(top):
    """Lambda(n) for every n to `top`, by a sieve: ln p at each power of each prime p, else 0."""
    out = [0.0] * (top + 1)
    composite = bytearray(top + 1)
    for p in range(2, top + 1):
        if composite[p]:
            continue
        composite[p * p::p] = b"\x01" * len(composite[p * p::p])
        power = p
        while power <= top:
            out[power] = math.log(p)
            power *= p
    return out


def proth_verdict(n):
    """Proth's certificate for n where his theorem reaches it: ("prime" or "composite", the witness), else None."""
    _, _, reachable = twiddle_proof.proth_form(n)
    if not reachable:
        return None
    return twiddle_proof.proth_prime(n)


def zeros_located(fine, x2):
    """Each zero of Z on the fine lattice, its t by the chord between the two certified points it lies between."""
    out = []
    for a, b in zeros_of(fine)[0]:
        za, zb = fine[a][2], fine[b][2]
        ta, tb = 2 * math.pi * x2[a], 2 * math.pi * x2[b]
        out.append(ta + (tb - ta) * za / (za - zb) if za != zb else (ta + tb) / 2)
    return out


def main_primes(binary, base, count, rate):
    """Cells `base` to base + count - 1 on one fine lattice each.

    The wave origins: the sources of F, the zeros of the main sum F off the line, locked on ln n for every n to N.
    log F has zeta's Dirichlet coefficients to N, and the lock is Lambda(n) n^(-1/2), zero at every n not a prime
    power. This reads back what F was built from, and checks the sources are placed right.

    The primes past N: the zeros of Z in the window [T1, T2], by Landau, sum to -((T2 - T1) / 2 pi) Lambda(x) / sqrt(x)
    for every x > 1. With a Hann taper w over the window, D(x) = -(4 pi / (T2 - T1)) sqrt(x) sum of w cos(gamma ln x)
    reads Lambda(x), each integer apart while x is under (T2 - T1) / 4 pi. Each x read past ln 2 / 2 is called a
    prime power; the calls are graded by the sieve, every one Proth's theorem reaches is proved by its witness, and
    the sum of D to x is read against psi(x)."""
    constants = tm.Constants()
    sources, gammas, failed, top_n = [], [], 0, 0
    for at in range(base, base + count):
        z = tm.rises(at)
        fine_p = math.ceil(math.log2(8 * rate * z))
        cell, _ = tm.run_cell(binary, constants, at, fine_p, "transform", listing=1)
        failed += cell.failed
        fine = points_of(cell)
        os.remove(cell.path)
        x2 = [at * at + j * (2 * at + 1) / len(fine) for j in range(len(fine))]
        dt = 2 * math.pi * (x2[1] - x2[0])
        for j in range(1, len(fine) - 1):
            w = fine[j][3]
            if not (abs(w) < abs(fine[j - 1][3]) and abs(w) <= abs(fine[j + 1][3])) or abs(w) == 0.0:
                continue
            clock = 0.5 * math.log(x2[j])
            f_rel = ((fine[j + 1][3] - fine[j - 1][3]) / (2 * dt) - complex(0, clock) * w) / w
            if f_rel != 0j and clock / abs(f_rel) < 1.0:
                sources.append(2 * math.pi * x2[j])
        gammas.extend(zeros_located(fine, x2))
        top_n = max(top_n, at)
        print("  cell %d done, %d sources and %d zeros so far" % (at, len(sources), len(gammas)), flush=True)

    lam_n = sieve_mangoldt(top_n)
    print("  the wave origins: %d sources, locked on ln n for n from 2 to N = %d" % (len(sources), top_n))
    locks = []
    for n in range(2, top_n + 1):
        r, ang, zz = rayleigh(sources, math.log(n))
        locks.append((n, r, ang, zz, lam_n[n] > 0))
    powers = [k for k in locks if k[4]]
    others = [k for k in locks if not k[4]]
    cut = max(k[3] for k in others)
    print("  prime powers %d: z least %.1f, median %.1f; others %d: z most %.1f, median %.2f" % (
        len(powers), min(k[3] for k in powers), sorted(k[3] for k in powers)[len(powers) // 2], len(others), cut,
        sorted(k[3] for k in others)[len(others) // 2]))
    print("  prime powers locked past every other n: %d of %d; at 180 +- 30 degrees: %d" % (
        sum(1 for k in powers if k[3] > cut), len(powers), sum(1 for k in powers if abs(k[2] - 180.0) <= 30.0)))
    ratio = sorted(k[1] / (lam_n[k[0]] / math.sqrt(k[0])) for k in powers)
    print("  R over Lambda(n) n^(-1/2) across the prime powers, 10/50/90: %.4f %.4f %.4f" % (
        ratio[len(ratio) // 10], ratio[len(ratio) // 2], ratio[(9 * len(ratio)) // 10]))

    t_low, t_high = min(gammas), max(gammas)
    span = t_high - t_low
    reach = int(span / (4 * math.pi))
    lam = sieve_mangoldt(reach)
    weights = [math.sin(math.pi * (g - t_low) / span) ** 2 for g in gammas]
    scale = 4 * math.pi / span
    print("  the primes past N: %d zeros in [%.3f, %.3f], each integer apart to %d" % (
        len(gammas), t_low, t_high, reach))
    reading = [0.0, 0.0]
    for x in range(2, reach + 1):
        lx = math.log(x)
        total = 0.0
        for g, w in zip(gammas, weights):
            total += w * math.cos(g * lx)
        reading.append(-scale * math.sqrt(x) * total)
        if x % 500 == 0:
            print("    read to %d" % x, flush=True)
    bands = [(2, top_n), (top_n + 1, min(reach, 1000)), (1001, min(reach, 2000)), (2001, reach)]
    print("  %-14s %8s %8s %8s %8s %8s %12s" % ("x", "truth", "called", "right", "false", "missed",
                                                  "D / Lambda"))
    for low, high in bands:
        if low > high:
            continue
        truth = [x for x in range(low, high + 1) if lam[x] > 0]
        called = [x for x in range(low, high + 1) if reading[x] > math.log(2) / 2]
        right = [x for x in called if lam[x] > 0]
        ratio = sorted(reading[x] / lam[x] for x in truth)
        print("  %-14s %8d %8d %8d %8d %8d %12.3f" % ("%d to %d" % (low, high), len(truth), len(called), len(right),
                                                       len(called) - len(right), len(truth) - len(right),
                                                       ratio[len(ratio) // 2] if ratio else 0.0))
    wrong = [x for x in range(2, reach + 1) if (reading[x] > math.log(2) / 2) != (lam[x] > 0)]
    print("  every wrong call: %s" % ", ".join("%d (D %.3f, Lambda %.3f)" % (x, reading[x], lam[x]) for x in wrong))
    ratio = sorted(reading[x] / lam[x] for x in range(2, reach + 1) if lam[x] > 0)
    rest = sorted(abs(reading[x]) for x in range(2, reach + 1) if lam[x] == 0)
    print("  D / Lambda at the prime powers, 1/10/50/90/99: %s; |D| elsewhere, 50/90/99/most: %s" % (
        " ".join("%.3f" % ratio[int(f * (len(ratio) - 1))] for f in (0.01, 0.1, 0.5, 0.9, 0.99)),
        " ".join("%.3f" % rest[int(f * (len(rest) - 1))] for f in (0.5, 0.9, 0.99, 1.0))))
    proved = {"prime": 0, "composite": 0, "other": 0}
    disagree = 0
    for x in range(top_n + 1, reach + 1):
        if reading[x] <= math.log(2) / 2:
            continue
        verdict = proth_verdict(x)
        if verdict is None:
            continue
        kind = verdict[0] if verdict[0] in proved else "other"
        proved[kind] += 1
        is_prime = lam[x] > 0 and all(x % p for p in range(2, int(math.isqrt(x)) + 1))
        if (kind == "prime") != is_prime:
            disagree += 1
    print("  calls past N that Proth reaches: %d proved prime, %d proved composite, %d otherwise; "
          "%d against the sieve" % (proved["prime"], proved["composite"], proved["other"], disagree))
    print("  %-8s %12s %12s %12s" % ("x", "psi(x)", "sum of D", "x"))
    for x in [100, 300, 500, 1000, 1500, 2000, 2500, 3000]:
        if x > reach:
            continue
        print("  %-8d %12.2f %12.2f %12d" % (x, sum(lam[2:x + 1]), sum(reading[2:x + 1]), x))
    print("  %d host checks failed" % failed)
    return 0 if failed == 0 else 1


def twisted_points_of(path):
    """A cell listed with the twist: the shift, and each point's sign, Z, w and F'/F = 2^shift exp(i theta) F' / w."""
    shift, out = 0, []
    with open(path) as handle:
        for line in handle:
            if line.startswith("twist"):
                shift = int(line.split()[1])
            elif line.startswith("point"):
                sign, _, z, re, im, dre, dim = (int(v, 16) for v in line.split()[1:8])
                w = complex(re / UNIT, im / UNIT)
                turned = complex(dre / UNIT, dim / UNIT) * (1 << shift)
                out.append((sign, z / UNIT, w, turned / w if abs(w) else 0j))
    return shift, out


TWIST_REACH = [0.5, 1.0, 2.0]
TWIST_RULES = ["reach 0.5", "reach 1", "reach 2", "Hermite", "the pole's model", "the pole's own", "pole or Hermite",
               "every rule"]


def model_turns(w, ratio, z, clock, t_end, t_a, t_b, samples=32):
    """Whether the relational model from one end crosses zero inside the step while the ends agree in sign: F linear
    through its source, F(t) = F_e (1 + (F'/F)_e (t - t_e)), the carrier turning at theta', and R held, giving
    Z(t) =2 Re(w_e (1 + (F'/F)_e (t - t_e)) exp(i theta' (t - t_e))) + R_e. Spin, twist, swell and the level R / (2 |F|)
    each enter against the others; nothing is a threshold."""
    rest = z - 2 * w.real
    sign = z > 0.0
    for k in range(1, samples):
        u = t_a + (t_b - t_a) * k / samples - t_end
        value = 2 * (w * (1 + ratio * u) * complex(math.cos(clock * u), math.sin(clock * u))).real + rest
        if (value > 0.0) != sign:
            return True
    return False


def hermite_turns(z0, z1, d0, d1, h, samples=32):
    """Whether the cubic through Z and Z' at a step's two ends, h apart, changes sign inside it while its ends hold
    one sign: a pair the step can hide, by the spin or by the slide alike."""
    if (z0 > 0.0) != (z1 > 0.0):
        return False
    m0, m1 = d0 * h, d1 * h
    for k in range(1, samples):
        s = k / samples
        value = ((2 * s ** 3 - 3 * s ** 2 + 1) * z0 + (s ** 3 - 2 * s ** 2 + s) * m0 + (-2 * s ** 3 + 3 * s ** 2) * z1 +
                 (s ** 3 - s ** 2) * m1)
        if (value > 0.0) != (z0 > 0.0):
            return True
    return False


def pole_passes(source, clock, rate):
    """The stretch of t where a single pole at `source` = gamma + i delta passes the clock, or None. Its twist is
    delta / (u^2 + delta^2), u = t - gamma: below -theta' where delta < 0 and u^2 < |delta| / theta' - delta^2, the
    clock running back; past (rate - 1) theta' where delta > 0 and u^2 < delta / ((rate - 1) theta') - delta^2, the
    clock spun a step's pi."""
    delta = source.imag
    if delta < 0.0:
        room = -delta / clock - delta * delta
    elif rate > 1.0:
        room = delta / ((rate - 1.0) * clock) - delta * delta
    else:
        room = -1.0
    if room <= 0.0:
        return None
    half = math.sqrt(room)
    return source.real - half, source.real + half


def crossings_of(path):
    """The times a polyline crosses itself: each pair of its segments, not neighbors, that intersect."""
    def side(a, b, c):
        return (b.real - a.real) * (c.imag - a.imag) - (b.imag - a.imag) * (c.real - a.real)
    count = 0
    for i in range(len(path) - 1):
        for j in range(i + 2, len(path) - 1):
            p, q, r, s = path[i], path[i + 1], path[j], path[j + 1]
            if (side(p, q, r) > 0) != (side(p, q, s) > 0) and (side(r, s, p) > 0) != (side(r, s, q) > 0):
                count += 1
    return count


def theta_at(t):
    """theta(t) = (t / 2) ln(t / 2 pi) - t / 2 - pi / 8 + 1 / (48 t), in floating point, for the co-rotating frame."""
    return 0.5 * t * math.log(t / (2 * math.pi)) - 0.5 * t - math.pi / 8 + 1.0 / (48 * t)


def figure_of(fine, x2, low, high, listed=None):
    """Over fine points low to high: the self-crossings of w's track, the walker's plane, and of F's, the frame that
    turns with the carrier; the signs the angular momentum L = Im(conj(w) w') = |F|^2 phi' runs through, each run of
    one sign counted once; and with the device's F'/F, the self-crossings of the phase portrait (Z, Z' / theta'),
    Z' = 2 Re(w (i theta' + F'/F))."""
    portrait = []
    if listed is not None:
        for j in range(low, high + 1):
            clock = 0.5 * math.log(x2[j])
            slope = 2 * (fine[j][3] * complex(listed[j][3].real, listed[j][3].imag + clock)).real
            portrait.append(complex(fine[j][2], slope / clock))
    track = [fine[j][3] for j in range(low, high + 1)]
    frame = [w * complex(math.cos(-theta_at(2 * math.pi * x2[j])), math.sin(-theta_at(2 * math.pi * x2[j])))
             for w, j in zip(track, range(low, high + 1))]
    signs = []
    for k in range(len(track) - 1):
        moment = (track[k].conjugate() * (track[k + 1] - track[k])).imag
        if moment != 0.0 and (not signs or signs[-1] != (moment > 0)):
            signs.append(moment > 0)
    return (crossings_of(track), crossings_of(frame), "".join("+" if s else "-" for s in signs),
            crossings_of(portrait))


def main_twist(binary, base, count, rate):
    """Cells `base` to base + count - 1 on one fine lattice each, listed with the device's F'.

    The check: the device's twist and swell, F'/F at each point, against the fine lattice's own differences of w.

    The flag: at each point of the uniform coarse lattice at the rate, Newton's step t* = t - F/F' places the nearest
    source, exact for a single pole, from that point alone. A coarse step is flagged where a source placed from either
    end falls inside it with |Im t*| under `reach` / theta', a pulse that can pass the clock. Read: the misses in
    flagged steps, the share of steps flagged, and the misses a uniform lattice that refines the same share by lot
    would catch."""
    constants = tm.Constants()
    diffs, misses, caught, flagged, steps, failed = [], 0, [0] * len(TWIST_RULES), [0] * len(TWIST_RULES), 0, 0
    courses, miss_courses, coupling, miss_coupling = [], [], [], []
    figures, lots, halves, slips = [], [], [], []
    lot = random.Random(20261003)
    for at in range(base, base + count):
        z = tm.rises(at)
        fine_p = math.ceil(math.log2(8 * rate * z))
        step = max(2, round((1 << fine_p) / (rate * z)))
        cell, _ = tm.run_cell(binary, constants, at, fine_p, "transform", listing=2)
        failed += cell.failed
        shift, listed = twisted_points_of(cell.path)
        os.remove(cell.path)
        fine = [(s, 0, zz, w) for s, zz, w, _ in listed]
        x2 = [at * at + j * (2 * at + 1) / len(fine) for j in range(len(fine))]
        dt = 2 * math.pi * (x2[1] - x2[0])
        for j in range(1, len(fine) - 1, 97):
            drag, swell = ripple_at(fine, x2, j)
            courses.append(abs(math.degrees(math.atan2(swell, drag))))
            coupling.append(1 + listed[j][3] / complex(0, 0.5 * math.log(x2[j])))
            w0, w1 = fine[j - 1][3], fine[j + 1][3]
            if abs(w0) == 0.0 or abs(w1) == 0.0:
                continue
            ratio = w1 / w0
            clock = 0.5 * math.log(x2[j])
            seen = complex(math.log(abs(w1) / abs(w0)), math.atan2(ratio.imag, ratio.real)) / (2 * dt) - complex(0, clock)
            diffs.append(abs(listed[j][3] - seen) / clock)
        # the half twist: at each minimum of |F|, Newton's step places the source, and the device's twist summed over
        # u in [-3 |delta|, 3 |delta|] is read against the single pass's 2 atan(3) sign(delta)
        for j in range(1, len(fine) - 1):
            if not (abs(fine[j][3]) < abs(fine[j - 1][3]) and abs(fine[j][3]) <= abs(fine[j + 1][3])):
                continue
            ratio = listed[j][3]
            if ratio == 0j:
                continue
            source = 2 * math.pi * x2[j] - 1.0 / ratio
            delta = source.imag
            clock = 0.5 * math.log(x2[j])
            reach = int(math.ceil(3 * abs(delta) / dt))
            centre = j + int(round((source.real - 2 * math.pi * x2[j]) / dt))
            if abs(delta) * clock >= 1.0 or reach < 4 or centre - reach < 0 or centre + reach >= len(fine):
                continue
            turn = sum(listed[k][3].imag for k in range(centre - reach, centre + reach + 1)) * dt
            halves.append((abs(delta) * clock, turn / (2 * math.atan(3.0) * math.copysign(1.0, delta))))
        idx = comb_indices(len(fine), step, (1,))
        found = misses_on(idx, fine)
        for a, b, first, second in found:
            dip = max(range(first[1], second[0] + 1), key=lambda j: abs(fine[j][2]))
            drag, swell = ripple_at(fine, x2, dip)
            miss_courses.append(abs(math.degrees(math.atan2(swell, drag))))
            # the slip: how far past the level the ball goes, |Z| / (2 |F|) at the dip, where cos(phi) stands, and
            # the drag there; the zeros' distance apart in fine steps
            slips.append((abs(fine[dip][2]) / (2 * abs(fine[dip][3])), fine[dip][3].real / abs(fine[dip][3]), drag,
                          second[0] - first[1], 2 * math.pi * x2[dip],
                          abs(fine[dip][2] - 2 * fine[dip][3].real) / (2 * abs(fine[dip][3]))))
            # the coupling zeta = 1 + (F'/F) / (i theta') from the device, least |zeta - 1| is the best grip, across
            # the miss's two zeros its farthest from 1
            across = range(first[0], second[1] + 1)
            miss_coupling.append(max((1 + listed[j][3] / complex(0, 0.5 * math.log(x2[j])) for j in across),
                                     key=lambda c: abs(c - 1)))
        spans = [(a, b) for a, b, first, second in found]
        misses += len(spans)
        # the figure: each miss's track over its steps and one step each side, against windows as long placed by lot
        for a, b, first, second in found:
            low, high = idx[max(a - 1, 0)], idx[min(b + 1, len(idx) - 1)]
            figures.append(figure_of(fine, x2, low, high, listed))
        span = 3 * step
        for _ in range(60):
            low = lot.randrange(0, len(fine) - span - 1)
            lots.append(figure_of(fine, x2, low, low + span, listed))
        marked = [set() for _ in TWIST_RULES]
        for a in range(len(idx) - 1):
            steps += 1
            t_a, t_b = 2 * math.pi * x2[idx[a]], 2 * math.pi * x2[idx[a + 1]]
            clock = 0.5 * math.log(x2[idx[a]])
            places = []
            for j in (idx[a], idx[a + 1]):
                ratio = listed[j][3]
                if ratio != 0j:
                    places.append(2 * math.pi * x2[j] - 1.0 / ratio)
            hits = [any(t_a <= s.real <= t_b and abs(s.imag) * clock < reach for s in places) for reach in TWIST_REACH]
            # Z' = 2 Re(w (i theta' + F'/F)), the remainder's own slope left out
            slopes = [2 * (fine[j][3] * complex(listed[j][3].real, listed[j][3].imag + 0.5 * math.log(x2[j]))).real
                      for j in (idx[a], idx[a + 1])]
            hermite = hermite_turns(fine[idx[a]][2], fine[idx[a + 1]][2], slopes[0], slopes[1], t_b - t_a)
            hits.append(hermite)
            same = (fine[idx[a]][2] > 0.0) == (fine[idx[a + 1]][2] > 0.0)
            hits.append(same and any(model_turns(fine[j][3], listed[j][3], fine[j][2], 0.5 * math.log(x2[j]),
                                                 2 * math.pi * x2[j], t_a, t_b) for j in (idx[a], idx[a + 1])))
            spans_of = [pole_passes(s, clock, rate) for s in places]
            pole = any(span is not None and span[0] <= t_b and span[1] >= t_a for span in spans_of)
            hits.append(pole)
            hits.append(pole or hermite)
            hits.append(pole or hermite or hits[4])
            for r, hit in enumerate(hits):
                if hit:
                    flagged[r] += 1
                    marked[r].add(a)
        for r in range(len(TWIST_RULES)):
            caught[r] += sum(1 for a, b in spans if any(k in marked[r] for k in range(a, b)))
        # each miss the Hermite flag leaves, read across its two zeros: R = Z - 2 Re w, the level R / (2 |F|) the
        # phase's cosine must reach, the drag and the swell over theta', the step's length in zeros, and the coupling
        hermite_at = TWIST_RULES.index("Hermite")
        for a, b, first, second in found:
            if any(k in marked[hermite_at] for k in range(a, b)):
                continue
            couplings = [1 + listed[j][3] / complex(0, 0.5 * math.log(x2[j])) for j in range(first[0], second[1] + 1)]
            print("    Hermite leaves: steps %d, zeros apart %.3f of a step, zeta from %s to %s" % (
                b - a, (second[0] - first[1]) / max(1, idx[b] - idx[a]),
                min(couplings, key=lambda c: c.real), max(couplings, key=lambda c: abs(c - 1))), flush=True)
            across = range(first[0], second[1] + 1)
            levels = [(fine[j][2] - 2 * fine[j][3].real) / (2 * abs(fine[j][3])) for j in across]
            drags = [ripple_at(fine, x2, j) for j in across]
            print("    left: t %.4f to %.4f, |F| %.4f to %.4f, R / (2 |F|) %.4f to %.4f, cos phi %.4f to %.4f, "
                  "drag %.3f to %.3f, swell %.3f to %.3f" % (
                      2 * math.pi * x2[first[0]], 2 * math.pi * x2[second[1]],
                      min(abs(fine[j][3]) for j in across), max(abs(fine[j][3]) for j in across),
                      min(levels), max(levels),
                      min(fine[j][3].real / abs(fine[j][3]) for j in across),
                      max(fine[j][3].real / abs(fine[j][3]) for j in across),
                      min(d[0] for d in drags), max(d[0] for d in drags),
                      min(d[1] for d in drags), max(d[1] for d in drags)), flush=True)
        print("  cell %d done, shift %d, %d misses so far" % (at, shift, misses), flush=True)
    diffs.sort()
    print("  the device's F'/F against the fine lattice's differences, |difference| / theta', 50/90/99/most: %s" % (
        " ".join("%.5f" % diffs[int(f * (len(diffs) - 1))] for f in (0.5, 0.9, 0.99, 1.0))))
    print("  %-16s %14s %16s" % ("flag", "steps flagged", "misses caught"))
    for r, rule in enumerate(TWIST_RULES):
        print("  %-16s %13.2f%% %9d (%5.1f%%)" % (rule, 100.0 * flagged[r] / max(1, steps), caught[r],
                                                  100.0 * caught[r] / max(1, misses)))
    for label, values in (("every point", courses), ("the misses' dips", miss_courses)):
        values.sort()
        print("  the course off the tangent, |atan(swell / drag)| in degrees, %s, 10/50/90: %s; past 60: %.1f%%" % (
            label, " ".join("%.1f" % values[int(f * (len(values) - 1))] for f in (0.1, 0.5, 0.9)),
            100.0 * sum(1 for v in values if v > 60.0) / max(1, len(values))))
    print("  the flagged steps by count: %s" % ", ".join("%s %d" % (rule, flagged[r]) for r, rule in enumerate(TWIST_RULES)))
    for label, values in (("every point", coupling), ("the misses, farthest from 1", miss_coupling)):
        far = sorted(abs(c - 1) for c in values)
        print("  the coupling zeta, %s: |zeta - 1| 10/50/90 %s; Re zeta < 0 %.1f%%, |zeta| past %.2f %.1f%%, "
              "|arg zeta| past 60 degrees %.1f%%" % (
                  label, " ".join("%.3f" % far[int(f * (len(far) - 1))] for f in (0.1, 0.5, 0.9)),
                  100.0 * sum(1 for c in values if c.real < 0) / max(1, len(values)), rate,
                  100.0 * sum(1 for c in values if abs(c) > rate) / max(1, len(values)),
                  100.0 * sum(1 for c in values if abs(math.degrees(math.atan2(c.imag, c.real))) > 60.0) /
                  max(1, len(values))))
    for label, rows in (("the misses", figures), ("windows by lot", lots)):
        n = max(1, len(rows))
        patterns = {}
        for row in rows:
            patterns[row[2]] = patterns.get(row[2], 0) + 1
        common = sorted(patterns.items(), key=lambda kv: -kv[1])[:4]
        print("  the figure, %s (%d): the track crosses itself, in w %.1f%%, in F %.1f%%, in (Z, Z' / theta') %.1f%%; "
              "L's signs, the most common: %s" % (label, len(rows), 100.0 * sum(1 for r in rows if r[0] > 0) / n,
                                                  100.0 * sum(1 for r in rows if r[1] > 0) / n,
                                                  100.0 * sum(1 for r in rows if r[3] > 0) / n,
                                                  ", ".join("%s %.1f%%" % (k, 100.0 * v / n) for k, v in common)))
    slips.sort()
    print("  the slip at each miss's dip, |Z| / (2 |F|), 10/50/90: %s" % " ".join(
        "%.4f" % slips[int(f * (len(slips) - 1))][0] for f in (0.1, 0.5, 0.9)))
    for depth, cosine, drag, apart, t, level in slips[:8]:
        print("    shallowest: t %.4f, slip %.5f, level %.4f, cos(phi) %.4f, drag %.3f, the zeros %d fine steps apart" % (
            t, depth, level, cosine, drag, apart))
    shallow, deep = slips[:len(slips) // 4], slips[-(len(slips) // 4):]
    for label, rows in (("shallowest quarter", shallow), ("deepest quarter", deep)):
        print("  %s: |cos(phi)| median %.4f, |drag| median %.3f, the zeros apart median %d fine steps, the level "
              "|R| / (2 |F|) median %.4f, the slip over the level median %.4f" % (
                  label, sorted(abs(r[1]) for r in rows)[len(rows) // 2], sorted(abs(r[2]) for r in rows)[len(rows) // 2],
                  sorted(r[3] for r in rows)[len(rows) // 2], sorted(r[5] for r in rows)[len(rows) // 2],
                  sorted(r[0] / r[5] if r[5] else 0.0 for r in rows)[len(rows) // 2]))
    for low, high in ((0.0, 0.25), (0.25, 0.5), (0.5, 1.0)):
        rows = sorted(r for d, r in halves if low <= d < high)
        if rows:
            print("  the half twist, |delta| theta' in [%.2f, %.2f): %d passes, the turn over 2 atan(3) sign(delta), "
                  "10/50/90: %s; the sign of delta's %.1f%%" % (
                      low, high, len(rows), " ".join("%.3f" % rows[int(f * (len(rows) - 1))] for f in (0.1, 0.5, 0.9)),
                      100.0 * sum(1 for r in rows if r > 0) / len(rows)))
    print("  misses %d over %d coarse steps; %d host checks failed" % (misses, steps, failed))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[2] == "twist":
        sys.stdout.reconfigure(line_buffering=True)
        sys.exit(main_twist(sys.argv[1], int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])))
    if len(sys.argv) > 2 and sys.argv[2] == "primes":
        sys.stdout.reconfigure(line_buffering=True)
        sys.exit(main_primes(sys.argv[1], int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])))
    if len(sys.argv) > 2 and sys.argv[2] == "source":
        sys.stdout.reconfigure(line_buffering=True)
        sys.exit(main_source(sys.argv[1], int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])))
    if len(sys.argv) > 2 and sys.argv[2] == "pulse":
        sys.stdout.reconfigure(line_buffering=True)
        sys.exit(main_pulse(sys.argv[1], int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])))
    if len(sys.argv) > 2 and sys.argv[2] == "ripple":
        sys.stdout.reconfigure(line_buffering=True)
        sys.exit(main_ripple(sys.argv[1], int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])))
    if len(sys.argv) > 2 and sys.argv[2] == "e":
        sys.stdout.reconfigure(line_buffering=True)
        sys.exit(main_folds(sys.argv[1], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), float(sys.argv[6]),
                            sys.argv[7] if len(sys.argv) > 7 else tempfile.mkdtemp(prefix="miss_folds_")))
    if len(sys.argv) > 2 and sys.argv[2] == "carrier":
        sys.stdout.reconfigure(line_buffering=True)
        sys.exit(main_carrier(sys.argv[1], int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5]),
                              sys.argv[6] if len(sys.argv) > 6 else tempfile.mkdtemp(prefix="miss_carrier_")))
    sys.exit(main())
