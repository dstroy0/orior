"""The ops of 10_Central-Salish-words-for-salmon: Ethan Pincott on contact and change in the Central
Salish words for salmon, the seventeen cognate sets shared by two languages or more, their
isoglosses, and what the overlaps say of diffusion in the branch.

The paper's data stand in the numbered cognate sets of §2.1, each a meaning in quotes, a
reconstruction with its source or none, and the attested forms under the language that prints
them, a gloss after a form where its meaning differs: (1) ‘any fish, salmon’: *sčaliɬtən (Kuipers,
2002:24, modified); Sechelt sčáliɬtən ‘fish, salmon (generic)’, ... Each set is a note as printed,
its reconstruction a cited form of Central Salish with the set's meaning, the source a citation, and
each attested form a cited form of its language with the dialect and the form's own gloss. The prose
under a set is read as the body's. Figures 1 to 3 are isogloss maps, their captions notes.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import residue  # noqa: E402

A = gen.A
LANGUAGE = "Central Salish"
AUTHORS = ["Ethan Pincott"]
paper = gen.Paper("10_Central-Salish-words-for-salmon", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Thom Hess", "Thom Hess, Central Coast Salish words for deer (1979), borrowed words (1986)"),
         ("Donna Gerdts", "Donna B. Gerdts, Dialect survey of Halkomelem Salish (1977)"),
         ("Aert Kuipers", "Aert H. Kuipers, Salish etymological dictionary (2002)"),
         ("Kuipers", "Aert H. Kuipers (1967, 1969, 1996, 2002)"), ("Hess", "Thom Hess (1979, 1986)"),
         ("Gerdts", "Donna B. Gerdts (1977), and the Hul̓q̓umín̓um̓ Words (1997)"),
         ("Donald", "Leland Donald (2003)"), ("Suttles", "Wayne P. Suttles (1990, 2004)"),
         ("François", "Alexandre François, Trees, waves and linkages (2015)"),
         ("Kinkade", "M. Dale Kinkade, with Thompson (1990), and 1995"),
         ("Peter Jacobs", "the author's language teacher, with whom he builds the lexical database"),
         ("Khelsilem", "the author's Sḵwx̱wú7mesh language teacher"),
         ("Watanabe", "Honoré Watanabe, Sliammon (2003)"), ("Beaumont", "Ronald C. Beaumont, Sechelt dictionary (2011)"),
         ("Galloway", "Brent D. Galloway (1988, 2009), with Richardson (2011)"),
         ("Richardson", "Allan Richardson, with Galloway (2011)"),
         ("Montler", "Timothy Montler (1991, 1997, 2012)"),
         ("Bates", "Dawn Bates, with Hess and Hilbert (1994)"), ("Hilbert", "Vi Hilbert, with Bates and Hess (1994)"),
         ("Nile Thompson", "personal communication on Twana"), ("Grubb", "David M. Grubb, Kwakw'ala (1977)"),
         ("Fortescue", "Michael D. Fortescue, Comparative Wakashan Dictionary (2007)"),
         ("Kennedy", "Dorothy I. D. Kennedy, with Bouchard (1990)"), ("Bouchard", "Randy T. Bouchard, with Kennedy (1990)"),
         ("Ware", "Reuben M. Ware (1983)"), ("Romanoff", "Steven Romanoff (1992)"),
         ("Hock", "Hans Henrich Hock, with Joseph (2009)"), ("Joseph", "Brian D. Joseph, with Hock (2009)"),
         ("Anttila", "Raimo Anttila (1989)"), ("Efrat", "Barbara S. Efrat, with the Thompsons (1974)"),
         ("Swadesh", "Morris Swadesh (1950)"), ("Ross", "Malcolm Ross (1988)")]
# Thompson names both a language, Thompson kəkn'íy, and the authors of Thompson & Kinkade, whose
# mention on page 2 comes first; it is left out of both lists and stands in the rows it prints in.
LANGUAGES = [(LANGUAGE, "the branch of Salish whose words for salmon the paper sets out"),
             ("Proto-Central Salish", "the reconstructed ancestor of the branch"), ("Proto-Salish", "the family's"),
             ("Comox-Sliammon", "Central Salish"), ("Sechelt", "Central Salish"), ("Squamish", "Central Salish"),
             ("Halkomelem", "Central Salish, its Island, Downriver and Upriver dialects"),
             ("Nooksack", "Central Salish"), ("Northern Straits", "Central Salish, Samish, Songish, Saanich and Lummi"),
             ("Samish", "Northern Straits"), ("Songish", "Northern Straits"), ("Saanich", "Northern Straits"),
             ("Lummi", "Northern Straits"), ("Klallam", "Central Salish"), ("Lushootseed", "Central Salish"),
             ("Twana", "Central Salish"), ("Kwak’wala", "Wakashan"), ("Nuu-chah-nulth", "Wakashan"),
             ("Wakashan", "a neighbouring family"), ("Proto-Wakashan", "the family's"),
             ("Interior Salish", "a branch of Salish"), ("Lillooet", "Interior Salish"), ("Shuswap", "Interior Salish"),
             ("Okanagan", "Interior Salish"), ("Columbian", "Interior Salish"), ("Upper Chehalis", "Tsamosan"),
             ("Tsamosan", "a branch of Salish"), ("Sḵwx̱wú7mesh", "Squamish, in its own name")]

# The languages a set prints its forms under, longest first. A dialect the set names before the
# language, Island Halkomelem, goes to the why with the language as printed.
SET_LANGUAGES = ["Downriver and Island Halkomelem", "Downriver and Upriver Halkomelem", "Island Halkomelem",
                 "Downriver Halkomelem", "Upriver Halkomelem", "Halkomelem", "Samish and Songish",
                 "Samish and Saanich", "Samish", "Saanich", "Songish", "Lummi", "Klallam", "Lushootseed",
                 "Comox-Sliammon", "Comox", "Sliammon", "Sechelt", "Squamish", "Nooksack", "Twana"]
SET_LANGUAGE = "|".join(re.escape(one) for one in SET_LANGUAGES)
# An attested form opens where a language's name follows a comma outside parentheses; a gloss's
# own comma, ‘fish, salmon (generic)’, has a word in lower case after it.
ITEM = re.compile(r"^(%s)(?:\s+\(([^()]*)\))?\s+(.*)$" % SET_LANGUAGE)
SET = re.compile(r"^\((\d{1,2})\)\s+‘([^’]+)’:\s+(.*)\.$")
# Each of the three captions wraps onto one line more.
CAPTION_LINES = 2


def printed_lines(start, count=None, until=None):
    """The line numbers of the text from line start, page breaks, page numbers and blank lines
    passed over: count of them, or up to the first whose text until matches."""
    running = paper.running_numbers_set()
    found, number = [], start
    while number <= paper.last:
        if not (paper.lines[number][2] or not paper.text(number) or number in running):
            found.append(number)
            if count and len(found) == count or until and until(paper.text(number)):
                return found, number + 1
        number += 1
    return found, number


def run_on(numbers):
    """The lines joined as one text, a line ending on a hyphen, Comox- / Sliammon, closed up."""
    text = ""
    for one in numbers:
        line = paper.text(one)
        text += line if not text or text.endswith("-") else " " + line
    return text


def split_items(text):
    """The attested forms of a set, split at each comma outside parentheses that a language's name
    follows."""
    items, depth, current = [], 0, ""
    at = 0
    while at < len(text):
        symbol = text[at]
        depth += {"(": 1, ")": -1}.get(symbol, 0)
        if symbol == "," and depth == 0 and re.match(r"\s+(?:%s)\s" % SET_LANGUAGE, text[at + 1:]):
            items.append(current.strip())
            current = ""
            at += 1
            continue
        current += symbol
        at += 1
    return items + ([current.strip()] if current.strip() else [])


def cognate_set(start, where):
    """A cognate set: the set as printed a note, its reconstruction a cited form of Central Salish
    with the set's meaning, each ~ variant a form of its own, the source a citation, and each
    attested form a cited form of its language."""
    numbers, after = printed_lines(start, until=lambda text: text.endswith("."))
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    page = paper.page(start)
    count = [0]

    def row(who, kind, form, why):
        count[0] += 1
        paper.add("(%s) line %d" % (label, count[0]), who, kind, form, why)
        if kind == "cited form":
            paper.cited_done.add(form)

    printed = paper.joined(numbers)
    row(A, "note", printed, "page %d, cognate set %s as printed" % (page, label))
    matched = SET.match(run_on(numbers))
    if not matched:
        print("# set (%s) not read: %s" % (label, run_on(numbers)), file=sys.stderr)
        return after
    meaning, rest = matched.group(2), matched.group(3)
    head = "page %d, set %s" % (page, label)
    reconstruction, attested = rest.split("; ", 1) if ";" in rest else ("", rest)
    if reconstruction:
        source = re.match(r"^(.*?)(?:\s+(\([^()]*\)))?$", reconstruction)
        for form in source.group(1).split(" ~ "):
            row(LANGUAGE, "cited form", form, "%s, ‘%s’, the reconstruction" % (head, meaning))
        if source.group(2):
            row(A, "citation", source.group(2), "%s, the source of the reconstruction" % head)
    for item in split_items(attested):
        found = ITEM.match(item)
        if not found:
            print("# set (%s) form not read: %s" % (label, item), file=sys.stderr)
            continue
        named, dialect, forms = found.groups()
        # A form the set gives for two languages at once, Samish and Songish sče:nəxʷ, is a row to
        # each language; two dialects of Halkomelem are one language.
        whos = ["Halkomelem"] if named.endswith("Halkomelem") else named.split(" and ")
        as_printed = named + (" (%s)" % dialect if dialect else "")
        gloss = extra = ""
        if "‘" in forms:
            forms, gloss = forms[:forms.index("‘")].strip(), forms[forms.index("‘"):]
        else:
            trailing = re.match(r"^(.*?)\s+\(([^()]*)\)$", forms)
            if trailing:
                forms, extra = trailing.groups()
        why = head + (", " + as_printed if [as_printed] != whos else "") + (", " + gloss if gloss else "") + \
            (", " + extra if extra else "")
        for who in whos:
            for form in forms.split(" ~ "):
                row(who, "cited form", form, why)
    paper.mentions(where, printed, NAMES, "name")
    paper.mentions(where, printed, LANGUAGES, "language")
    return after


def caption(start, where):
    """A figure's caption, an isogloss map's key: a note, and its cited forms after it. A run of the
    page that the caption holds only after a star, Kwak'wala sac'əm in *sac'əm of Figure 3, is the
    body's and not the caption's."""
    numbers, after = printed_lines(start, CAPTION_LINES)
    here = re.match(r"^(Figure \d+):", paper.text(start)).group(1)
    body = paper.joined(numbers)
    page = paper.page(start)
    paper.add(here, A, "note", body, "page %d, the figure's caption" % page)
    starred = set()
    for run in paper.italics().get(page, ()):
        found = list(re.finditer(r"(?<![\w’])%s(?![\w’])" % re.escape(run), body))
        if found and all(body[one.start() - 1:one.start()] == "*" and not run.startswith("*") for one in found):
            starred.add(run)
    paper.cited(here, body, [page], skip=starred)
    paper.mentions(here, body, LANGUAGES, "language")
    return after


def glottal_tails():
    """An italic run the reader closes before a word's final glottal mark, scqaz for scqaz’ and
    sχəw for sχəw’, takes the mark back where its page prints the run only with it."""
    for page, runs in paper.italics().items():
        text = " ".join(paper.text(one) for one in range(1, paper.last + 1)
                        if paper.page(one) == page and not paper.lines[one][2])
        for index, run in enumerate(runs):
            bare = r"(?<![\w’])%s(?![\w’])" % re.escape(run)
            tailed = r"(?<![\w’])%s’(?![\w’])" % re.escape(run)
            if not re.search(bare, text) and re.search(tailed, text):
                runs[index] = run + "’"


def form_language(run):
    """The paper's italic runs are forms where gen.orthographic finds a letter outside plain
    English, and also where a glottal mark stands inside a word, Nuu-chah-nulth sac’up."""
    return gen.L if gen.orthographic(run) or re.search(r"\w’\w", run) else None


def split_variants():
    """A cited form of the prose that holds two variants, *hənəw ~ hənəy, is a row to each, as
    the sets give theirs; so is each step of a chain of changes, *caw'in > *caw'n > ... > səʔn and
    *k > č, its star kept as printed. The note the chain stands in keeps it whole."""
    rows = []
    for row in paper.rows:
        if row[2] == "cited form" and re.search(r" [~>] ", row[3]) and not row[0].startswith("("):
            rows.extend(row[:3] + [one, row[4]] for one in re.split(r" [~>] ", row[3])
                        if one.strip(" .…"))
        else:
            rows.append(row)
    paper.rows = rows


def split_reference(opening):
    """A reference entry that opens on a line the entry pattern does not know, Squamish Nation
    Education Department, & University of Washington. (2011). after Ross (1988), is a row of its
    own."""
    for index, row in enumerate(paper.rows):
        if row[2] == "reference" and " " + opening in row[3]:
            at = row[3].index(" " + opening)
            paper.rows[index:index + 1] = [row[:3] + [row[3][:at], row[4]],
                                           row[:3] + [row[3][at + 1:], row[4]]]
            return
    print("# reference not split: %s" % opening, file=sys.stderr)


REFERENCES = paper.find(r"^References$")
blocks = {number: cognate_set for number in range(1, REFERENCES) if gen.EXAMPLE.match(paper.text(number))
          and SET.match(run_on(printed_lines(number, until=lambda text: text.endswith("."))[0]))}
blocks.update({number: caption for number in range(1, REFERENCES) if re.match(r"^Figure \d+:", paper.text(number))})
glottal_tails()
paper.form_language = form_language
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks)
split_variants()
split_reference("Squamish Nation Education Department")
paper.write()
