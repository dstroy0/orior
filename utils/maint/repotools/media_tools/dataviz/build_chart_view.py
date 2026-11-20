#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: VIZ-x-003
#
"""Turns a table into a flat chart: lines, time series, scatter, steps or bars.

The other viewers here build a solid that you turn. This one is the ordinary two-axis chart. It is
for the readings that are honestly one dimensional. Examples are a quantity against time, one
column against another, and a run of measurements to look along and not into.

matplotlib draws these too, and draws them well. But it does not give one file that opens anywhere
with no interpreter and no install, and stays interactive once it is open. In this page, hover to
read a point, drag across to zoom, toggle a series, and switch the y axis to log. Thus this exists
beside matplotlib, and not in place of it.

Examples:

docs-check: quoting
    python examples/00_blob_viz_tools/build_chart_view.py readings.csv
    python examples/00_blob_viz_tools/build_chart_view.py readings.csv --x time --y temperature pressure
    python examples/00_blob_viz_tools/build_chart_view.py points.csv --type scatter
docs-check: end quoting

Options:

  --x NAME       the column along the bottom. The default is the first column named one of time,
                 t, x, date, step, round, index. Else it is the first column that parses as numbers.
  --y NAME...    the columns to draw. Default: every other numeric column.

  --split NAME   a column whose distinct values split the rows into separate series. Long-format
                 data usually arrives in that shape.
  --type KIND    line, scatter, step, bar or area. Default line.
  --title TEXT   heading for the page. Default: the file's name.
  --out FILE     where to write. Default: <name>_chart.html beside the table.

Writes a self-contained page: no server, no fetch at run time, nothing to install.
"""

import os, sys
_at = os.path.dirname(os.path.abspath(__file__))
while _at != os.path.dirname(_at) and not os.path.isdir(os.path.join(_at, "lib", "numerics")):
    _at = os.path.dirname(_at)
sys.path.insert(0, os.path.join(_at, "lib"))

import csv
import io
import os
import sys
import generate_template

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = os.path.join(HERE, "chart_view_template.html")

X_NAMES = ["time", "t", "x", "date", "step", "round", "index", "n"]


def numeric(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return None


def main():
    argv = sys.argv[1:]
    if not argv or argv[0].startswith("-"):
        sys.stderr.write(__doc__)
        return 2

    source = argv[0]
    if not os.path.exists(source):
        sys.stderr.write("no such file: %s\n" % source)
        return 1

    def one(name):
        return argv[argv.index(name) + 1] if name in argv else None

    def many(name):
        if name not in argv:
            return []
        out = []
        for each in argv[argv.index(name) + 1:]:
            if each.startswith("-"):
                break
            out.append(each)
        return out

    with io.open(source, encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        sys.stderr.write("%s has no rows\n" % source)
        return 1

    header = [name for name in rows[0].keys() if name is not None]

    x_at = one("--x")
    if x_at is None:
        for name in X_NAMES:
            match = [each for each in header if each.lower() == name]
            if match:
                x_at = match[0]
                break
    if x_at is None:
        for name in header:
            if all(numeric(row.get(name)) is not None for row in rows[:64]):
                x_at = name
                break
    if x_at is None or x_at not in header:
        sys.stderr.write("no x column: name one with --x, from %s\n" % ", ".join(header))
        return 1

    split_at = one("--split")
    if split_at is not None and split_at not in header:
        sys.stderr.write("no column named %s\n" % split_at)
        return 1

    y_names = many("--y")
    if not y_names:
        y_names = [name for name in header
                   if name != x_at and name != split_at
                   and any(numeric(row.get(name)) is not None for row in rows[:64])]
    if not y_names:
        sys.stderr.write("nothing numeric to draw. Name columns with --y\n")
        return 1

    # This drops a row whose x does not parse, and never draws it at zero. An unparsed label on the
    # bottom axis would put a point where it does not belong. That is worse than its omission.
    dropped = 0
    series = []
    if split_at:
        groups = {}
        for row in rows:
            x = numeric(row.get(x_at))
            if x is None:
                dropped += 1
                continue
            key = row.get(split_at, "")
            for name in y_names:
                y = numeric(row.get(name))
                if y is None:
                    continue
                label = key if len(y_names) == 1 else (key + " " + name)
                groups.setdefault(label, {"xs": [], "ys": []})
                groups[label]["xs"].append(x)
                groups[label]["ys"].append(y)
        for label in sorted(groups):
            series.append({"name": label, "xs": groups[label]["xs"], "ys": groups[label]["ys"]})
    else:
        holding = dict((name, {"xs": [], "ys": []}) for name in y_names)
        for row in rows:
            x = numeric(row.get(x_at))
            if x is None:
                dropped += 1
                continue
            for name in y_names:
                y = numeric(row.get(name))
                if y is None:
                    continue
                holding[name]["xs"].append(x)
                holding[name]["ys"].append(y)
        for name in y_names:
            series.append({"name": name, "xs": holding[name]["xs"], "ys": holding[name]["ys"]})

    series = [each for each in series if each["xs"]]

    # This puts the points in order of x. A line in file order sweeps back across the chart wherever
    # the rows are not already sorted. That reads as a diagonal through the data, which the data does
    # not hold. Scatter would not care, but one file draws the same series both ways.
    for each in series:
        together = sorted(zip(each["xs"], each["ys"]))
        each["xs"] = [pair[0] for pair in together]
        each["ys"] = [pair[1] for pair in together]
    if not series:
        sys.stderr.write("every row was dropped: check --x names a numeric column\n")
        return 1

    type = one("--type") or "line"
    if type not in ("line", "scatter", "step", "bar", "area"):
        sys.stderr.write("--type must be line, scatter, step, bar or area\n")
        return 1

    title = one("--title") or os.path.basename(source)
    payload = {
        "title": title,
        "xLabel": x_at,
        "type": type,
        "blurb": ("%s, %d series against %s. Hover to read the nearest point, drag across to zoom "
                  "to a range of %s, double click to go back. Series can be turned off one at a "
                  "time, and the y axis can be read on a log scale."
                  % (os.path.basename(source), len(series), x_at, x_at)),
        "series": series,
    }

    try:
        page = generate_template.assemble(TEMPLATE, payload)
    except generate_template.Refused as why:
        sys.stderr.write("%s: %s\n" % (os.path.basename(TEMPLATE), why))
        return 1

    target = one("--out")
    if target is None:
        target = os.path.join(os.path.dirname(os.path.abspath(source)),
                              os.path.splitext(os.path.basename(source))[0] + "_chart.html")
    with io.open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(page)

    print("wrote %s (%.1f KB)" % (target, os.path.getsize(target) / 1024.0))
    print("  x        %s" % x_at)
    print("  series   %s" % ", ".join(each["name"] for each in series[:8])
          + ("" if len(series) <= 8 else " and %d more" % (len(series) - 8)))
    print("  points   %d" % sum(len(each["xs"]) for each in series))
    if dropped:
        print("  dropped  %d row(s) whose %s did not parse" % (dropped, x_at))
    return 0


if __name__ == "__main__":
    sys.exit(main())
