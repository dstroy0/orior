# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py docs: the check of every Markdown file of the tree against ASD-STE100.

    Run `harness.py docs ste` to hold each document to ste_ratchet.tsv and to compare the three checkers.
    Run `harness.py docs ste --show [DOC]` to see the findings of the documents named, or of every Markdown file.
    Run `harness.py docs ste --lower` to write each count into the ratchet. A count goes down and never up.

utils/maint/prose/ste_check.py is the checker and its docstring names the rules. The harness gives it each Markdown
file of the tree by name. The walk of the checker reads only docs/ and the READMEs, and the checker must stay the
same file in the three repositories. The tool docs_check in utils/maint/prose checks the comments in the code, and
it reads the whole tree. The pre-commit hook runs `docs ste`, and the check is part of every commit. Vocabulary is the docs
check's banned words and voice to hold, and this check reads no word list."""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
STE_CHECK = os.path.join(ROOT, "utils", "maint", "prose", "ste_check.py")
STE_RATCHET = os.path.join(ROOT, "utils", "maint", "prose", "ste_ratchet.tsv")
MMGR = os.path.join(ROOT, "includes", "mmgr")
# The other copies, each compared byte for byte with orior's. MMgr's is a submodule and must be there; embedded_types sits
# under MMgr's deps/ only once MMgr's build fetched it.
STE_COPIES = (
    (os.path.join(MMGR, "test", "ste_check.py"), True),
    (os.path.join(MMGR, "deps", "embedded_types", "test", "ste_check.py"), False),
)


def ste_copies_same():
    """0 where every copy present holds orior's checker byte for byte. 1 where one differs or MMgr's copy is not
    there."""
    with open(STE_CHECK, "rb") as handle:
        ours = handle.read()
    code = 0
    for path, required in STE_COPIES:
        name = os.path.relpath(path, ROOT).replace("\\", "/")
        if not os.path.isfile(path):
            if required:
                print("ste: %s is not there. Check out the submodule." % name)
                code = 1
            else:
                print("ste: %s is not fetched, not compared" % name)
            continue
        with open(path, "rb") as handle:
            if handle.read() != ours:
                print("ste: %s differs from utils/maint/prose/ste_check.py; the three copies are one file" % name)
                code = 1
    return code


def markdown_files():
    """Every Markdown file of the tree, by its path from the top. That is each file that git tracks, and each new
    file that git does not ignore. Git lists no file of a submodule, which is a repository with its own check.
    None where git cannot give the list. A check that cannot read its list does not pass."""
    listed = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--", "*.md"],
        cwd=ROOT,
        capture_output=True,
    )
    if listed.returncode != 0:
        print("ste: git did not list the Markdown files: %s" % listed.stderr.decode("utf-8", "replace").strip())
        return None
    names = sorted(set(one.decode("utf-8") for one in listed.stdout.split(b"\0") if one))
    return [name for name in names if os.path.isfile(os.path.join(ROOT, name))]


def cmd_ste(a):
    """The ratchet checked and the copies compared, or the findings displayed, or the ratchet lowered."""
    files = markdown_files()
    if files is None:
        return 1
    if a.show is not None:
        return subprocess.run([sys.executable, STE_CHECK] + (a.show or files), cwd=ROOT).returncode
    if a.lower:
        return subprocess.run([sys.executable, STE_CHECK, "--ratchet-write", STE_RATCHET] + files, cwd=ROOT).returncode
    held = subprocess.run([sys.executable, STE_CHECK, "--ratchet", STE_RATCHET] + files, cwd=ROOT).returncode
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
