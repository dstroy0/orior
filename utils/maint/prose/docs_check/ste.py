#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The ASD-STE100 rules over a source file's comments. The rules are the ones ste_check.py applies to
# the technical documents. This module reads them through that file. The tree holds one rule set.
# This module writes the comments of a file to build/gate/ste_comments.txt, one line a line with the
# code lines empty, and the checker reads that file. A finding then names the line a reader opens.
#

import importlib.util
import os

from .repository import REPOSITORY
from .scan import MARKER

PROSE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STE_CHECK = os.path.join(PROSE, "ste_check.py")
STE_NAMES = os.path.join(PROSE, "ste_names.txt")
COMMENTS = os.path.join(REPOSITORY, "build", "gate", "ste_comments.txt")

_CHECKER = []


def checker():
    """The ste_check module, loaded once from the prose folder."""
    if not _CHECKER:
        spec = importlib.util.spec_from_file_location("ste_check", STE_CHECK)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _CHECKER.append(module)
    return _CHECKER[0]


def comment_text(said):
    """The prose view with each comment marker taken off. A code line stays empty. A comment block
    ends where the code starts, and the line numbers stay the file's."""
    return [MARKER.sub("", line).strip() for line in said]


def ste_findings(said):
    """(line number, what) for each ASD-STE100 rule a comment breaks."""
    ste = checker()
    os.makedirs(os.path.dirname(COMMENTS), exist_ok=True)
    with open(COMMENTS, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(comment_text(said)) + "\n")
    names = ste.word_list(STE_NAMES) if os.path.isfile(STE_NAMES) else set()
    return [(number, "ste %s: %s" % (rule, what)) for number, rule, what in ste.findings(COMMENTS, set(), names)]
