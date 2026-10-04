"""The ops of 20-Lonsdale_ICSNL50_final-10: Deryle Lonsdale on a Snohomish telling of The Seal
Hunters, set down in French by Alphonse Pinart in the 1870s: the manuscript transcribed, put into
English, and set beside the other tellings of the story across the Coast Salish.

The paper is prose throughout, with no examples: Pinart's French and its English translation are
paragraphs, and the dialects of Lushootseed are a list of two items.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "Lushootseed"
AUTHORS = ["Deryle Lonsdale"]
paper = gen.Paper("20-Lonsdale_ICSNL50_final-10", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Pinart", "Alphonse Louis Pinart, French ethnographer and explorer, 1852–1911; the manuscript"),
         ("Chirouse", "Father Eugène Casimir Chirouse, Oblate missionary among the Snohomish"),
         ("Thom Hess", "standardized the orthography in the 1960s"),
         ("Ballard", "A. C. Ballard, tales of the South Puget Sound Salish (1927)"),
         ("Adamson", "T. Adamson, folk-tales of the Coast Salish (1934)"),
         ("Jacobs", "M. Jacobs, Clackamas Chinook texts (1958)"),
         ("Elmendorf", "W. W. Elmendorf, Skokomish and other Coast Salish tales (1961)"),
         ("Martha Lamont", "her telling in Lushootseed, in Bierwert (1996)"),
         ("Bierwert", "C. Bierwert, Lushootseed Texts (1996)"),
         ("Associated Press", "on Pinart's crystal skull (2008)")]
LANGUAGES = [(LANGUAGE, "Central Coast Salish, formerly Puget Salish"),
             ("Snohomish", "a northern dialect of Lushootseed; the telling"),
             ("Puget Salish", "the former name of Lushootseed"),
             ("French", "Pinart's manuscript"), ("English", "the translation"),
             ("Skagit", "a northern dialect of Lushootseed"), ("Nooksack", "grouped with Skagit"),
             ("Sauk-Suiattle", "a northern dialect of Lushootseed"),
             ("Skykomish", "a northern dialect of Lushootseed"),
             ("Snoqualmie", "a southern dialect of Lushootseed"),
             ("Suquamish", "a southern dialect of Lushootseed"),
             ("Duwamish", "a southern dialect of Lushootseed"),
             ("Muckleshoot", "a southern dialect of Lushootseed"),
             ("Chinook Jargon", "a source of loanwords"),
             ("Upper Chehalis", "The Seal Hunter, in Adamson (1934)"),
             ("Cowlitz", "The Wooden Fish, in Adamson (1934)"),
             ("Clackamas Chinook", "Jacobs's telling (1958)"),
             ("Skokomish", "two tellings in Elmendorf (1961)"),
             ("Chinook", "a teller of other versions"), ("Squamish", "a teller of other versions"),
             ("Musqueam", "a teller of other versions"), ("Katzie", "a teller of other versions")]

def dialects(start, where):
    """The dialects of Lushootseed, a note to each bulleted item, to the paragraph under them."""
    end = paper.find(r"^Though Pinart likely came in contact", start)
    parts = []
    for line in range(start, end):
        text = paper.text(line)
        if not text or paper.lines[line][2] or line in RUNNING:
            continue
        if text.startswith("•"):
            parts.append([text])
        else:
            parts[-1].append(text)
    for count, part in enumerate(parts, 1):
        body = " ".join(part)
        paper.add("%s bullet %d" % (where, count), A, "note", body, "page %d, a bulleted item" % paper.page(start))
        paper.mentions(where, body, LANGUAGES, "language")
    return end


def figure(start, where):
    """Figure 1's caption, a note; the figure itself is a picture of the manuscript."""
    paper.add("Figure 1", A, "note", paper.text(start),
              "page %d, the figure's caption; the figure is a picture" % paper.page(start))
    return start + 1


RUNNING = paper.running_numbers_set()
blocks = {paper.find(r"^• Skagit"): dialects, paper.find(r"^Figure 1 Pinart manuscript\.$"): figure}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# The Associated Press's entry opens on no surname and initials, and gen runs it onto Adamson's above it.
for index, row in enumerate(paper.rows):
    if row[2] == "reference" and " Associated Press. (2008" in row[3]:
        first, second = row[3].split(" Associated Press. (2008")
        paper.rows[index:index + 1] = [[row[0], row[1], row[2], first, row[4]],
                                       [row[0], row[1], row[2], "Associated Press. (2008" + second, row[4]]]
        break
# The contact line at the foot of page 1 carries no mark and is set at the body size, and gen runs it
# into the paragraph that turns the page.
CONTACT = "Contact info: lonz@byu.edu"
for index, row in enumerate(paper.rows):
    if row[2] == "note" and " %s " % CONTACT in row[3]:
        row[3] = row[3].replace(" %s " % CONTACT, " ")
        break
at = next(index for index, row in enumerate(paper.rows) if row[2] == "language")
paper.rows.insert(at, ["front", A, "note", CONTACT, "page 1, the note at the foot of the page with no mark"])
paper.write()
