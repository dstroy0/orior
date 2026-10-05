"""List example tier rows whose kind disagrees with their script.

usage: python tier_script.py <stem> <phonemic letters>

A row of an example holding any of the phonemic letters is the phonemic tier, segmentation; a row
with none is the practical orthography, transcription. Prints the rows that say otherwise.
"""
import os
import sys

from workdir import WORK

stem, letters = sys.argv[1], sys.argv[2]
with open(os.path.join(WORK, stem + ".draft.tsv"), encoding="utf-8") as handle:
    for line in list(handle)[1:]:
        where, who, kind, form, gloss = line.rstrip("\n").split("\t")
        if not where.startswith("(") or kind not in ("transcription", "segmentation"):
            continue
        phonemic = any(one in letters for one in form)
        if (kind == "segmentation") != phonemic:
            print("%s | %s | %s" % (where, kind, form[:80]))
