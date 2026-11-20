#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Copies the general viewer tools into utils/maint/repotools, in the layout of the toolkit.

Two sets. Membership is by what a thing is, and never by who calls it:

  lib/numerics/          computation over numeric sequences. Imported, never run.
  media_tools/dataviz/   the viewers, their generator and their toolbox. Imports numerics.

Run python utils/maint/repotools/publish_dataviz.py to report what the copy would change.
Run it with --write to copy.

This does not copy a tool that opens a file of this tree. The list below names each one with its
reason.

WHY THIS TRANSFORMS AND DOES NOT ONLY COPY

A builder in examples/00_blob_viz_tools says `import dsp`. That works because Python puts the
directory of a script on sys.path when the script runs. In the toolkit, dsp lives in lib/numerics,
and not beside the builder. Thus the published form of a runnable file opens with a preamble that
walks up to lib/numerics, and imports by set. The transform applies at publish time and never by
hand: one source, one transform, no drift.

Only Python files change. A template, a toolbox script, a style sheet and the manifest copy byte
for byte. The reason: generate_template --verify holds a built page to the toolbox byte for byte.
"""

import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
VIEW = os.path.join(ROOT, "examples", "00_blob_viz_tools")
TOOLKIT = os.path.join(ROOT, "utils", "maint", "repotools")

HEADER = (
    "# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>\n"
    "# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial"
    " OR LicenseRef-Educational\n"
)

# The preamble finds the directory that holds lib/numerics by a walk up from the file. A count of
# parents breaks when a tool moves, and the walk does not.
PREAMBLE = (
    "import os, sys\n"
    "_at = os.path.dirname(os.path.abspath(__file__))\n"
    "while _at != os.path.dirname(_at) and not os.path.isdir("
    "os.path.join(_at, \"lib\", \"numerics\")):\n"
    "    _at = os.path.dirname(_at)\n"
    "sys.path.insert(0, os.path.join(_at, \"lib\"))\n"
)

# The source name of each numerics module, and its name in the toolkit.
NUMERICS = {
    "dsp.py": "dsp.py",
    "exact.py": "precision.py",
}

# The viewer files. generate_template and out_path sit beside the builders that import them, and
# the directory of a script is on sys.path. Thus their imports need no rewrite.
DATAVIZ = [
    "build_blob_view.py",
    "build_chart_view.py",
    "build_field_view.py",
    "build_plot_view.py",
    "build_sound_view.py",
    "build_sweep_view.py",
    "generate_template.py",
    "out_path.py",
    "settings.py",
    "chart_view_template.html",
    "voxel_view_template.html",
]

# The toolbox directory, copied whole. generate_template reads it from beside itself.
TOOLBOX = "toolbox"

# The rewrite of each import in the published form. The left side is the statement in this tree.
#
# The match is on the whole statement after its indentation, and not on the line. Thus an
# indentation cannot make a rule miss. LEFTOVERS below makes a miss an error, and never a quiet
# pass-through.
IMPORTS = [
    ("import dsp", "from numerics import dsp"),
    ("import exact as extended", "from numerics import precision as extended"),
    ("import exact", "from numerics import precision as exact"),
]

# No published file may still name a module that exists only in this tree. The check runs after
# the rewrite, because the failure that it catches is a rewrite that does nothing.
LEFTOVERS = ("import dsp", "import exact")

STAYS = {
    "build_voxel_view.py": "opens build/bench/shadows.csv",
    "build_shadow_view.py": "opens build/bench/shadows.csv",
    "build_sources_view.py": "opens build/bench/sources.csv",
    "make_shadow_figure.py": "opens build/bench/shadows.csv",
    "build_step_view.py": "traces SHA-256, the subject of this tree",
    "step_view_template.html": "belongs to build_step_view.py",
    "shadow_view_template.html": "belongs to build_shadow_view.py",
    "sources_view_template.html": "belongs to build_sources_view.py",
}


def published(text, name, runnable):
    """The file as the toolkit holds it. A file that is not Python returns unchanged."""
    if not name.endswith(".py"):
        return text

    # Whole statements. The rule touches a line only where its whole text is the import. The same words
    # inside a docstring or a comment stay as they are.
    out = []
    for line in text.split("\n"):
        bare = line.strip()
        for was, now in IMPORTS:
            if bare == was:
                line = line[:len(line) - len(line.lstrip())] + now
                break
        out.append(line)
    text = "\n".join(out)

    if "SPDX-License-Identifier" not in text[:400]:
        text = HEADER + text

    # Only a file that runs must find the toolkit. A module that something imports is already on
    # the path when it loads, and a preamble in it would be dead code.
    if runnable and "_at = os.path.dirname" not in text:
        marker = '"""\n'
        end = text.find(marker, text.find('"""') + 3)
        if end > 0:
            cut = end + len(marker)
            text = text[:cut] + "\n" + PREAMBLE + text[cut:]

    # The check uses startswith, and not equality. Thus `import exact as extended` matches the
    # entry `import exact`. A guard that catches only one form of the statement is no guard.
    for line in text.split("\n"):
        if any(line.strip().startswith(one) for one in LEFTOVERS):
            raise SystemExit(
                "publish refused: %s still says %r after the rewrite.\n"
                "  Add the statement to IMPORTS. The published copy would fail at run time."
                % (name, line.strip()))
    return text


def put(source, target, runnable, write):
    """Copies one file, transformed, and returns 1 where it wrote."""
    if not os.path.exists(source):
        print("  MISSING   %s" % os.path.relpath(source, ROOT).replace(os.sep, "/"))
        return 0
    with io.open(source, encoding="utf-8", newline="") as handle:
        text = handle.read()
    out = published(text, os.path.basename(target), runnable)
    same = False
    if os.path.exists(target):
        with io.open(target, encoding="utf-8", newline="") as handle:
            same = handle.read() == out
    where = os.path.relpath(target, TOOLKIT).replace(os.sep, "/")
    print("  %-10s %s" % ("unchanged" if same else ("copy" if write else "would copy"), where))
    if write and not same:
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with io.open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(out)
        return 1
    return 0


def toolbox_files():
    """Every file under the toolbox, as a path relative to the viewer directory."""
    found = []
    for at, folders, files in os.walk(os.path.join(VIEW, TOOLBOX)):
        folders[:] = sorted(one for one in folders if one != "__pycache__")
        for name in sorted(files):
            found.append(os.path.relpath(os.path.join(at, name), VIEW))
    return found


def main():
    write = "--write" in sys.argv
    if not os.path.isdir(VIEW):
        sys.stderr.write("no viewer directory at %s\n" % VIEW)
        return 1

    moved = 0
    numerics = os.path.join(TOOLKIT, "lib", "numerics")
    for name, as_name in sorted(NUMERICS.items()):
        moved += put(os.path.join(VIEW, name), os.path.join(numerics, as_name), False, write)

    dataviz = os.path.join(TOOLKIT, "media_tools", "dataviz")
    published_names = set()
    for name in DATAVIZ + toolbox_files():
        runnable = os.path.basename(name).startswith(("build_", "generate_"))
        moved += put(os.path.join(VIEW, name), os.path.join(dataviz, name), runnable, write)
        published_names.add(os.path.normpath(name))

    # A file in the toolkit that this script does not publish has no source here. The report names
    # it, and the script does not remove it.
    for at, folders, files in os.walk(dataviz):
        folders[:] = [one for one in folders if one != "__pycache__"]
        for name in files:
            relative = os.path.normpath(os.path.relpath(os.path.join(at, name), dataviz))
            if relative not in published_names:
                print("  extra      media_tools/dataviz/%s, with no source in this tree"
                      % relative.replace(os.sep, "/"))

    print("")
    print("staying in this tree:")
    for name in sorted(STAYS):
        print("  %-28s %s" % (name, STAYS[name]))

    print("")
    print(("%d written" % moved) if write else "nothing written. Run with --write.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
