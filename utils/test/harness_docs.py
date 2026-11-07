# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py docs: every technical document checked against ASD-STE100.

    harness.py docs ste                 no document carries more findings than utils/maint/prose/ste_ratchet.tsv
                                        allows it, and the checker is the same file in orior, MMgr and embedded_types
    harness.py docs ste --show [DOC]    the findings of the documents named, or of the tree
    harness.py docs ste --lower         each document's count written into the ratchet, lowered and never raised

utils/maint/prose/ste_check.py is the checker and its docstring names the rules. The pre-commit hook runs
`docs ste`, and the check is part of every commit. Vocabulary is the docs check's banned words and voice to hold,
and this check reads no word list."""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
STE_CHECK = os.path.join(ROOT, "utils", "maint", "prose", "ste_check.py")
STE_RATCHET = os.path.join(ROOT, "utils", "maint", "prose", "ste_ratchet.tsv")
MMGR = os.path.join(ROOT, "src", "cu", "includes", "mmgr")
# The other copies, each compared byte for byte with orior's. MMgr's is a submodule and must be there; embedded_types sits
# under MMgr's deps/ only once MMgr's build has fetched it.
STE_COPIES = (
    (os.path.join(MMGR, "test", "ste_check.py"), True),
    (os.path.join(MMGR, "deps", "embedded_types", "test", "ste_check.py"), False),
)


def ste_copies_same():
    """0 where every copy present holds orior's checker byte for byte, 1 where one differs or MMgr's is missing."""
    with open(STE_CHECK, "rb") as handle:
        ours = handle.read()
    code = 0
    for path, required in STE_COPIES:
        name = os.path.relpath(path, ROOT).replace("\\", "/")
        if not os.path.isfile(path):
            if required:
                print("ste: %s is missing; the submodule is not checked out" % name)
                code = 1
            else:
                print("ste: %s is not fetched, not compared" % name)
            continue
        with open(path, "rb") as handle:
            if handle.read() != ours:
                print("ste: %s differs from utils/maint/prose/ste_check.py; the three copies are one file" % name)
                code = 1
    return code


def cmd_ste(a):
    """The ratchet checked and the copies compared, or the findings shown, or the ratchet lowered."""
    if a.show is not None:
        return subprocess.run([sys.executable, STE_CHECK] + (a.show or ["."]), cwd=ROOT).returncode
    if a.lower:
        return subprocess.run([sys.executable, STE_CHECK, "--ratchet-write", STE_RATCHET, "."], cwd=ROOT).returncode
    held = subprocess.run([sys.executable, STE_CHECK, "--ratchet", STE_RATCHET, "."], cwd=ROOT).returncode
    same = ste_copies_same()
    return 1 if held or same else 0


def register(sub):
    """The docs group's commands added to harness.py's subparsers."""
    group = sub.add_parser("docs", help="every technical document checked against ASD-STE100")
    docs = group.add_subparsers(dest="command", required=True)
    p = docs.add_parser("ste", help="the ASD-STE100 ratchet checked over every technical document, and the checker's three copies compared")
    p.add_argument("--show", nargs="*", default=None, metavar="DOC", help="the findings of the documents named, or of the tree")
    p.add_argument("--lower", action="store_true", help="each document's count written into the ratchet, lowered and never raised")
    p.set_defaults(fn=cmd_ste)
