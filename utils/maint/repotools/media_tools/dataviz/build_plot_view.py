#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: VIZ-x-006
#
"""Plots one or more expressions over a grid and passes them to the shape viewer.

The other generators read a measurement. This one evaluates a written expression. Thus the same
instrument also serves for a surface that you already understand. A function of known shape shows
what an embedding does to a shape. A function that nobody drew on a torus before is worth a look
on its own.

docs-check: quoting
    python examples/00_blob_viz_tools/build_plot_view.py "sin(x)*cos(y)"
    python examples/00_blob_viz_tools/build_plot_view.py "sin(x)*cos(y)" "sin(2*x)*cos(2*y)" --n 128
    python examples/00_blob_viz_tools/build_plot_view.py "exp(-(x**2+y**2))" --x -2 2 --y -2 2 --title Gaussian
docs-check: end quoting

Each expression becomes one step. Thus the page can step through a sequence of them: a function
and its derivative, or the same function at a higher frequency each step.

  --x LOW HIGH   range of x, taken as the depth axis. Default -pi to pi.
  --y LOW HIGH   range of y, taken as the series axis. Default -pi to pi.

  --n COUNT      samples on each axis. Default 96. 9216 cells per expression.
  --title TEXT   heading for the page. Default: the first expression.
  --out FILE     where to write. Default: plot_view.html in build/view, through out_path.

An expression may hold x, y, pi, e and tau, and min and max. It may also hold these functions:

docs-check: quoting
    sin cos tan asin acos atan atan2 sinh cosh tanh exp log log2 log10 sqrt abs floor ceil hypot
    copysign fmod pow degrees radians erf gamma
docs-check: end quoting

Nothing else is in scope, and nothing in the expression can reach the interpreter. The script
compiles it with no builtins, and evaluates it against that table alone.

Writes a self-contained page: no server, no fetch at run time, nothing to install.
"""

import os, sys
_at = os.path.dirname(os.path.abspath(__file__))
while _at != os.path.dirname(_at) and not os.path.isdir(os.path.join(_at, "lib", "numerics")):
    _at = os.path.dirname(_at)
sys.path.insert(0, os.path.join(_at, "lib"))

import io
import math
import os
import sys

import settings
import out_path
import generate_template

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = os.path.join(HERE, "voxel_view_template.html")

# Everything an expression can see. Each entry is a pure function of numbers.
ALLOWED = {
    "pi": math.pi,
    "e": math.e,
    "tau": math.tau,
    "sin": math.sin,
    "cos": math.cos,
    "tan": math.tan,
    "asin": math.asin,
    "acos": math.acos,
    "atan": math.atan,
    "atan2": math.atan2,
    "sinh": math.sinh,
    "cosh": math.cosh,
    "tanh": math.tanh,
    "exp": math.exp,
    "log": math.log,
    "log2": math.log2,
    "log10": math.log10,
    "sqrt": math.sqrt,
    "abs": abs,
    "floor": math.floor,
    "ceil": math.ceil,
    "hypot": math.hypot,
    "copysign": math.copysign,
    "fmod": math.fmod,
    "pow": math.pow,
    "degrees": math.degrees,
    "radians": math.radians,
    "erf": math.erf,
    "gamma": math.gamma,
    "min": min,
    "max": max,
}


def sample(source, xs, ys):
    """Evaluates one expression over the grid, one row per y.

    A point that fails to evaluate becomes zero, and does not stop the run. Tan has poles and log
    has a domain. A plot that draws nothing because of one bad column is less useful than one that
    draws the rest. The function returns the count of such points, for the report.
    """
    code = compile(source, "<expression>", "eval")
    rows = []
    bad = 0
    for y in ys:
        row = []
        for x in xs:
            try:
                value = eval(code, {"__builtins__": {}}, dict(ALLOWED, x=x, y=y))
                value = float(value)
                if value != value or value in (float("inf"), float("-inf")):
                    raise ValueError("not finite")
            except Exception:
                value = 0.0
                bad += 1
            row.append(round(value, 6))
        rows.append(row)
    return rows, bad


def main():
    argv = sys.argv[1:]
    if not argv or argv[0].startswith("-"):
        sys.stderr.write(__doc__)
        return 2

    def pair(name, low, high):
        if name not in argv:
            return low, high
        at = argv.index(name)
        return float(argv[at + 1]), float(argv[at + 2])

    def number(name, fallback):
        return int(argv[argv.index(name) + 1]) if name in argv else fallback

    def text(name):
        return argv[argv.index(name) + 1] if name in argv else None

    # Everything before the first option is an expression.
    sources = []
    for one in argv:
        if one.startswith("-"):
            break
        sources.append(one)
    if not sources:
        sys.stderr.write("give at least one expression\n")
        return 1

    x_low, x_high = pair("--x", -math.pi, math.pi)
    y_low, y_high = pair("--y", -math.pi, math.pi)
    count = number("--n", 96)
    if count < 2:
        sys.stderr.write("--n must be at least 2\n")
        return 1

    xs = [x_low + ((x_high - x_low) * i / (count - 1.0)) for i in range(count)]
    ys = [y_low + ((y_high - y_low) * i / (count - 1.0)) for i in range(count)]

    fields = []
    for source in sources:
        rows, bad = sample(source, xs, ys)
        fields.append({"key": source, "label": source, "axis": "y", "rows": rows})
        note = (
            ""
            if not bad
            else "  (%d point%s undefined, drawn as zero)"
            % (bad, "" if bad == 1 else "s")
        )
        print("  %s%s" % (source, note))

    title = text("--title") or sources[0]
    payload = {
        "depth": count,
        "depthLabel": "x from %g to %g" % (x_low, x_high),
        "valueLabel": "value",
        "eyebrow": "Plotted expression - rendered as a solid",
        "title": title,
        "blurb": (
            "%s over x in [%g, %g] and y in [%g, %g], sampled %d by %d. Depth runs left to "
            "right as x, the other horizontal axis is y, and height and color are the value. "
            "Each expression is a step. A list of them can be stepped through in place."
            % (", ".join(sources), x_low, x_high, y_low, y_high, count, count)
        ),
        "noteTitle": "The shape is yours, the embedding is a claim",
        "note": (
            "A surface you already know is the way to see what an embedding does. Draw it as a "
            "plane first, then as a tube or a toroid, and what changes is the map and not "
                 "the function. Joining the ends of an axis says the last x is next to the first, "
            "which is true for a periodic function and false for max others."
        ),
        "settings": settings.collect(sys.argv[1:]),
        "schema": settings.schema(sorted(settings.KNOWN)),
        "fields": fields,
    }

    try:
        page = generate_template.assemble(TEMPLATE, payload)
    except generate_template.Refused as why:
        sys.stderr.write("%s: %s\n" % (os.path.basename(TEMPLATE), why))
        return 1

    target = out_path.resolve("plot_view.html", text("--out"))
    with io.open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(page)

    print("wrote %s (%.1f KB)" % (target, os.path.getsize(target) / 1024.0))
    print(
        "  %d expression%s, %d by %d samples"
        % (len(sources), "" if len(sources) == 1 else "s", count, count)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
