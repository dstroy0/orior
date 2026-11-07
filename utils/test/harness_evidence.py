# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py evidence: the distributions the theory models, tested where the tests carry no assumption of their own.

    harness.py evidence r               each CSV the R hypotheses read written from the corpora under build/corpora,
                                        then every script of evidence/sims/r/hypotheses run on its CSV

The writers and the scripts, in the order run:

    examples/any_corpus/4_measure/symbol_ratios.py   build/ratios.csv            hypotheses/ratio_normality.R
    examples/any_corpus/4_measure/ladder_sweep.py    build/ladder.csv            hypotheses/ladder_analysis.R
    examples/language/4_measure/language_constant.py build/language_constant.csv hypotheses/language_variance.R

The byte corpora are every .sym under build/corpora, and the language texts every lang_*.txt there. Rscript is
the program the PATH names, or the newest under R's install folder on Windows. A writer or a script that fails, or
a corpus folder with none of the files a writer reads, fails the command."""

import glob
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
CORPORA = os.path.join(ROOT, "build", "corpora")
HYPOTHESES = os.path.join(ROOT, "evidence", "sims", "r", "hypotheses")
# (writer, whether it takes the byte corpora as arguments, the CSV it writes, the script that reads it)
R_STEPS = (
    ("examples/any_corpus/4_measure/symbol_ratios.py", True, "build/ratios.csv", "ratio_normality.R"),
    ("examples/any_corpus/4_measure/ladder_sweep.py", True, "build/ladder.csv", "ladder_analysis.R"),
    ("examples/language/4_measure/language_constant.py", False, "build/language_constant.csv", "language_variance.R"),
)


def rscript():
    """The Rscript to run: the PATH's, or the newest of R's install folders on Windows, or None."""
    found = shutil.which("Rscript")
    if found:
        return found
    for base in (os.environ.get("ProgramFiles", r"C:\Program Files"), os.environ.get("ProgramW6432", "")):
        if not base:
            continue
        installed = sorted(glob.glob(os.path.join(base, "R", "R-*", "bin", "Rscript.exe")))
        if installed:
            return installed[-1]
    return None


def cmd_r(a):
    """Every CSV written, then every hypothesis script run on it."""
    program = rscript()
    if program is None:
        print("evidence: no Rscript on the PATH or under R's install folder")
        return 1
    symbols = sorted(glob.glob(os.path.join(CORPORA, "*.sym")))
    texts = glob.glob(os.path.join(CORPORA, "lang_*.txt"))
    if not symbols or not texts:
        print("evidence: build/corpora holds %d .sym corpora and %d lang_*.txt texts; both are needed"
              % (len(symbols), len(texts)))
        return 1
    failed = []
    for writer, takes_corpora, csv, script in R_STEPS:
        print("\n== %s" % script, flush=True)
        command = [sys.executable, os.path.join(ROOT, writer)] + (symbols if takes_corpora else [])
        if subprocess.run(command, cwd=ROOT).returncode != 0:
            failed.append(writer)
            continue
        if subprocess.run([program, os.path.join(HYPOTHESES, script), os.path.join(ROOT, csv)], cwd=ROOT).returncode != 0:
            failed.append(script)
    for name in failed:
        print("evidence: %s failed" % name)
    return 1 if failed else 0


def register(sub):
    """The evidence group's commands added to harness.py's subparsers."""
    group = sub.add_parser("evidence", help="the distributions the theory models, tested in R")
    evidence = group.add_subparsers(dest="command", required=True)
    p = evidence.add_parser("r", help="each CSV the R hypotheses read written from build/corpora, then every script of evidence/sims/r/hypotheses run on it")
    p.set_defaults(fn=cmd_r)
