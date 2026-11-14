"""List example tier rows whose type disagrees with their script.

usage: python tier_script.py <stem> <phonemic characters>

A row of an example holding any of the phonemic characters is the phonemic tier, segmentation; a row
with none is the practical orthography, transcription. Prints the rows that say otherwise.
"""
import os
import sys

from workdir import WORK

stem, characters = sys.argv[1], sys.argv[2]
with open(os.path.join(WORK, stem + ".draft.tsv"), encoding="utf-8") as handle:
    for line in list(handle)[1:]:
        where, who, type, form, gloss = line.rstrip("\n").split("\t")
        if not where.startswith("(") or type not in ("transcription", "segmentation"):
            continue
        phonemic = any(one in characters for one in form)
        if (type == "segmentation") != phonemic:
            print("%s | %s | %s" % (where, type, form[:80]))
