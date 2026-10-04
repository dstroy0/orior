"""The ops of ICSNL55_program: the ICSNL 55 schedule, August 13 to 15, 2020, a list of talks and
not a paper. The text layer holds each page as one line; the page text is read by glyph rows
(page_text.py rows). Each printed line is a row under its day, a talk's title run on across its
wrapped lines, and each talk's languages a row after it.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

WHO = "ICSNL 55 organizers"
paper = gen.Paper("ICSNL55_program")
add, text, page = paper.add, paper.text, paper.page

LANGUAGES = [("ʔayʔaǰuθəm", "Comox-Sliammon"), ("St’át’imcets", "Lillooet"), ("Kwak̓wala", "Kwakw'ala"),
             ("Kwak̕ wala", "Kwakw'ala, as printed"), ("Nuuchahnulth", "Nuu-chah-nulth"),
             ("Nootka Jargon", "a pidgin of the Nuu-chah-nulth coast"), ("Proto-Salish", "the reconstructed ancestor"),
             ("Hul’q’umi’num’", "Island Halkomelem"), ("Tsimshianic", "the family of Gitksan and its sisters"),
             ("Halkomelem", "Central Salish"), ("Nxaʔamxčín", "Columbia-Moses"),
             ("N̓ syilxčn̓", "nsyilxcən, Okanagan, as printed"), ("Gumbaynggirr", "Australian, New South Wales")]

add("front", WHO, "title", text(2), "page 1, the schedule's heading line")
add("front", WHO, "notation", "the schedule of a conference, not a paper", "the talks of ICSNL 55, August 13 to 15, 2020")
add("front", WHO, "note", text(3), "page 1, the dates")
day, time, talk = "front", None, None
for number in range(4, paper.last + 1):
    line = text(number)
    if not line or paper.lines[number][2]:
        continue
    at = page(number)
    if re.match(r"^(Thursday|Friday|Saturday), ", line):
        day = line.split(",")[0]
        add(day, WHO, "heading", line, "page %d, the day" % at)
        talk = None
        continue
    if line.startswith("Session "):
        add(day, WHO, "heading", line, "page %d, the session, its chair and its hours" % at)
        talk = None
        continue
    opened = re.match(r"^(\d{1,2}:\d\d)\s*(?:[–-]+)?\s*(.*)$", line)
    if opened:
        time = opened.group(1)
        talk = [line, at, number]
        paper.rows.append(["%s %s" % (day, time), WHO, "note", line, "page %d, a talk: its time, the presenters and the title" % at])
        continue
    # A line run on from the talk above it.
    paper.rows[-1][3] += " " + line
for row in list(paper.rows):
    pass
# Each talk's languages, after the talk that names them.
out = []
for row in paper.rows:
    out.append(row)
    if row[2] == "note" and row[0] not in ("front",):
        for name, why in LANGUAGES:
            if name in row[3]:
                out.append([row[0], WHO, "language", name, "%s, a language of the talk" % why])
paper.rows = out
paper.write()
