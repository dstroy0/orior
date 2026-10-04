"""The ops of 2009_vanEijk: Jan P. van Eijk's Salish words for 'black bear' and 'grizzly bear', the
names of the two bears in each Salish language and dialect they are recorded for, their sources,
and the proto-forms Kinkade (1991a) and Kuipers (2002) reconstruct.

The table of §2, on pages 2 and 3, sets a language or a dialect under it (Sliammon under Comox) to
a line, the dialect opening on a dash (U+2014), its form for 'black bear' in the second column and
its form for 'grizzly bear' in the third, the alternatives of one cell numbered (1), (2) and (3) on
lines of their own, and a remark
in brackets or a gloss in quotes after a form wrapping onto the next line. Each word goes to the
column whose left edge it stands at, and each form is a cited form of its language, the dialect in
its gloss. §3 to §5 are prose: each paragraph of §3 is about the language it opens on, The Comox
data ..., and each cited form in a paragraph is given to the language or dialect named just before
it, Squamish míx̌aɬ, to its proto-language where it is starred, and otherwise to the language the
paragraph is about.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
STEM = "2009_vanEijk"
AUTHORS = ["Jan P. van Eijk"]
paper = gen.Paper(STEM, authors=AUTHORS[0], language="Salish")
# The paper is set ragged right: a line ends on a hyphen only where the hyphen is printed:
# Spokane- / Kalispel-Flathead, síʔ- / sinƛ̕, 13-04- / 2009.
paper.hyphen_ends_join = True
# The languages of the table, each with the dialects the table sets under it.
DIALECTS = {
    "Comox": ("Sliammon", "Island"), "Halkomelem": ("Upriver", "Musqueam", "Nanaimo", "Cowichan", "Chilliwack"),
    "Northern Straits": ("Songish", "Lummi", "Saanich", "Samish"),
    "Lushootseed": ("Skagit", "Snohomish", "Southern"), "Upper Chehalis": ("Oakville", "Satsop", "Tenino"),
    "Spokane": ("Kalispel", "Flathead"),
}
LANGUAGE_NAMES = ["Bella Coola", "Comox", "Pentlatch", "Sechelt", "Squamish", "Halkomelem", "Nooksack",
                  "Northern Straits", "Klallam", "Lushootseed", "Twana", "Quinault", "Lower Chehalis",
                  "Upper Chehalis", "Cowlitz", "Tillamook-Siletz", "Lillooet", "Shuswap", "Thompson", "Columbian",
                  "Okanagan", "Spokane", "Coeur d’Alene"]
OTHER = {"Nuxalk": "Bella Coola", "Heiltsuk": "Heiltsuk", "Kwak’wala": "Kwak’wala", "Wakashan": "Wakashan",
         "Kutenai": "Kutenai", "Athapaskan": "Athapaskan", "English": "English", "Russian": "Russian",
         "Island Comox": "Comox", "Spokane-Kalispel-Flathead": "Spokane", "Spokane-Kalispel": "Spokane",
         "Proto-Salish": "Proto-Salish", "Proto-Central Salish": "Proto-Central Salish",
         "Proto-Tsamosan Salish": "Proto-Tsamosan Salish", "Proto-Coast-Salish": "Proto-Coast Salish",
         "Proto-Indo-European": "Proto-Indo-European", "Upriver Halkomelem": "Halkomelem",
         "Oakville Upper Chehalis": "Upper Chehalis", "Tenino Upper Chehalis": "Upper Chehalis",
         "Southern Lushootseed": "Lushootseed", "Colville": "Okanagan", "Northern Wakashan": "Wakashan"}
NAMED = {name: name for name in LANGUAGE_NAMES}
NAMED.update(OTHER)
for language, dialects in DIALECTS.items():
    NAMED.update({dialect: language for dialect in dialects if dialect != "Southern"})
NAME_ALTERNATION = "|".join(re.escape(one) for one in sorted(NAMED, key=len, reverse=True))
# The name a form stands right after: Squamish míx̌aɬ, the Okanagan word for 'black bear,' s.kmx̌ist,
# the Pentlatch form for 'black bear' (sx̌ʷəsə́lqin), and the second of a pair, Heiltsuk ƛ̕a and nán.
# A remark in brackets can stand between the name and the form, Upper Chehalis (Oakville dialect) s.mə́š.
NAME_AT_END = re.compile(r"(?:^|[\s(])(%s)\)?(?: \([^)]*\))?(?:\s+(?:forms?|words?))?(?:\s+for\s+‘[^’]*’,?)?"
                         r"(?:\s+\S+\s+(?:and|or))?\s*\(?\s*$" % NAME_ALTERNATION)
# The name a form stands before in its clause: a second root, gla-, for 'grizzly bear' in Northern
# Wakashan, and míx̌aɬ may be a loan from Lillooet.
NAME_AFTER = re.compile(r"^,?\s*(?:for\s+‘[^’]*’\s*)?(?:in|may be a loan from)\s+(%s)\b" % NAME_ALTERNATION)
# The name of a form a source lists, the Kutenai form for 'grizzly bear,' which Boas 1918:364 lists as kɬáwɬa.
NAME_LISTS = re.compile(r"\b(%s) (?:forms?|words?) for ‘[^’]*’,? which [^.]*? as\s*$" % NAME_ALTERNATION)
TOKEN = re.compile(r"[^\s,;‘’“”]+(?:’(?=[^\s,;:)]))?[^\s,;‘“”]*")
# A form of two words, its pieces each a form too: c.kʷím s.pɛ́:θ 'brown bear', and Saanich
# nəq̓ix̌ s.peʔəθ 'black bear'.
PHRASES = ("c.kʷím-əlqəl s.pέ:θ", "c.kʷím s.pέ:θ", "nəq̓ix̌ s.peʔəθ")
NAMES = [("Kinkade", "M. Dale Kinkade (1981, 1991a, 1991b, 1995, 2004), the source of the Proto-Salish forms"),
         ("Kuipers", "Aert H. Kuipers (1967, 1969, 1974, 2002), Squamish, Shuswap and the Salish etymological dictionary"),
         ("Nater", "Hank Nater (1977, 1990), Bella Coola"), ("Timmers", "Jan A. Timmers (1977), Sechelt"),
         ("Galloway", "Brent Galloway (1990, 2008, 2009), Samish, Nooksack and Upriver Halkomelem"),
         ("Montler", "Timothy Montler (1991, 2000), Saanich and Klallam"), ("Hess", "Thom Hess (1976), Lushootseed"),
         ("Mattina", "Anthony Mattina (1987), Okanagan"), ("Vogt", "Hans Vogt (1940), Kalispel"),
         ("Reichard", "Gladys A. Reichard (1939), Coeur d’Alene"), ("Henry Davis", "who presented the paper at the 44th ICSNL"),
         ("Peter Jacobs", "who gave Squamish data and comments"), ("John Lyon", "who gave comments"),
         ("Steve Egesdal", "who gave Flathead data"), ("Nile Thompson", "who gave the Twana forms"),
         ("Jan Timmers", "who gave the Island Comox forms"), ("John Davis", "who gave the Sliammon form")]
LANGUAGES = [("Salish", "the family whose words for the two bears the paper compares")] + \
    [(one, "a Salish language of the table") for one in LANGUAGE_NAMES] + \
    [("Heiltsuk", "Wakashan, the source of the Bella Coola forms"), ("Kwak’wala", "Wakashan"),
     ("Wakashan", "the family the Bella Coola forms are borrowed from"), ("Kutenai", "a neighbor of Okanagan"),
     ("Athapaskan", "the source of the words for 'roe'")]


def table(start, where):
    """§2's table: each line's words under the column whose left edge they stand at."""
    end = paper.find(r"^3 Comments")
    head = paper.word_positions(start)
    edges = [head[0][0]] + [left for left, word in head if word in ("Black", "Grizzly")]
    paper.add("table", A, "note", paper.text(start), "page %d, the heads of the columns" % paper.page(start))
    state = {"language": None, "dialect": None, "cells": None}
    rows = []

    def put(column, text):
        """A cell's line, run on to the cell above where that leaves a bracket open, (spirit /
        power name), or where the line is a remark alone, Thompson's 'silvertip grizzly'."""
        held = state["cells"][column]
        if held and (held[-1].count("(") > held[-1].count(")") or re.search(r"‘[^’]*$", held[-1]) or
                     re.match(r"^(?:‘|\((?!\d\)))", text)):
            held[-1] += " " + text
        else:
            held.append(text)

    for number in range(start + 1, end):
        text = paper.text(number)
        if paper.lines[number][2] or not text or re.fullmatch(r"\d{2}", text):
            continue
        positions = paper.word_positions(number) or [(edges[0], one) for one in text.split()]
        cells = [[], [], []]
        for left, word in positions:
            cells[max(column for column in range(3) if edges[column] <= left + 6) if left + 6 >= edges[0]
                  else 0].append(word)
        first = " ".join(cells[0])
        dialect = first.startswith("—")
        named = first.lstrip("—").strip()
        if named and (named in NAMED or dialect or named in ("Tillamook-Siletz",)) and named[:1].isupper():
            if dialect:
                state["dialect"] = named
            else:
                state["language"], state["dialect"] = NAMED.get(named, named), None
            state["cells"] = {1: [], 2: []}
            rows.append([state["language"], state["dialect"], state["cells"], paper.page(number)])
            for column in (1, 2):
                if cells[column]:
                    put(column, " ".join(cells[column]))
        elif state["cells"] is not None:
            # A line at the margin carries on the last cell's remark, grizzly' under 'silvertip.
            for column in (1, 2):
                if cells[column]:
                    put(column, " ".join(cells[column]))
            if cells[0]:
                last = max((column for column in (1, 2) if state["cells"][column]), default=2)
                if state["cells"][last]:
                    state["cells"][last][-1] += " " + first
                else:
                    state["cells"][last].append(first)
    for language, dialect, cells, page in rows:
        label = "table, %s%s" % (language, ", " + dialect if dialect else "")
        for column, meaning in ((1, "‘black bear’"), (2, "‘grizzly bear’")):
            for cell in cells[column]:
                number = re.match(r"^\((\d)\)\s*", cell)
                body = cell[number.end():] if number else cell
                remark = re.search(r"\s*(\(.*\)|‘.*’?|\S*’)\s*$", body)
                forms_text = body[:remark.start()] if remark else body
                said = remark.group(1) if remark else ""
                for form in filter(None, (one.strip() for one in forms_text.split(","))):
                    gloss = "page %d, the table, %s%s%s%s" % (
                        page, meaning, ", the %s dialect" % dialect if dialect and language != "Spokane" else
                        ", %s, of a dialect continuum with Spokane" % dialect if dialect else "",
                        ", form (%s)" % number.group(1) if number else "", ", " + said if said else "")
                    paper.add(label, language, "cited form", form, gloss)
    return end


def language_of(note, form, default):
    """The language a cited form of the prose is given to: the name just before it, its proto-
    language where starred, or the language the paragraph is about."""
    at = note.find(form)
    before = note[:at] if at >= 0 else ""
    named = NAME_AT_END.search(before)
    if named:
        return NAMED[named.group(1)]
    if form.startswith("*"):
        stage = re.search(r"(Proto-[\w-]+(?: Salish)?) form\b[^*]*$", before)
        return NAMED.get(stage.group(1), stage.group(1)) if stage else "Proto-Salish"
    return default


BLOCKS = {paper.find(r"^Language\s+Black Bear"): table}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, notes_title=tuple(gen.TITLE_MARKS) + ("1",))


def gloss_after(after):
    """The gloss in quotes after a form, for 'grizzly bear' or 'secret/ /mysterious', without the
    comma or stop the prose sets inside the closing quote, and None where there is none."""
    said = re.match(r"\s*(?:for\s+)?(‘[^’]*’)", after)
    return re.sub(r"[,.]’$", "’", said.group(1)) if said else None


def prose_forms(note, about):
    """[(form, language, gloss)] for the forms of a paragraph: each word with a letter outside plain
    English, an affix's hyphen or a star, each form of the table, and the gloss in quotes after it."""
    found = []
    known = KNOWN
    pattern = re.compile("|".join(re.escape(one) for one in PHRASES) + "|" + TOKEN.pattern)
    tokens = list(pattern.finditer(note))
    for index, token in enumerate(tokens):
        form = re.sub(r"[.,]?’$|[.:]$", "", token.group(0))
        # A bracket of the prose at either end is no part of the form, (Kuipers or 1967:304), but
        # one inside it is, s.čkʷáy̓(ə)c and (ʔə)sqʷúqʷus.
        while form.startswith("(") and form.count("(") > form.count(")"):
            form = form[1:]
        while form.endswith(")") and form.count(")") > form.count("("):
            form = form[:-1]
        form = form.rstrip(".,:")
        if form.startswith("(") and form.endswith(")") and "(" not in form[1:-1]:
            form = form[1:-1]
        bare = re.sub(r"[̀-ͯʰ-˿*\-–.:()√]", "", form)
        before, after = note[:token.start()], note[token.end():]
        # A form in plain letters is a form of the table, kn-keknm and nan, or a word broken by a
        # hyphen or a prefix's period with its gloss after it, ken-m 'to do what/something'.
        plain = form in TABLE_FORMS or re.fullmatch(r"[a-z]+(?:[-.][a-z]+)+", form) and re.match(r"\s*‘", after)
        # A letter the prose names alone, q̓ʷ against k̕ʷ, is no form, and a name broken at its hyphen.
        if not form or form.rstrip("-") in NAMED or re.match(r"^\d", form) or len(bare) < 2 or \
                not (gen.orthographic(form) or form.startswith("*") or plain):
            continue
        named = NAME_AT_END.search(before) or NAME_AFTER.search(after) or NAME_LISTS.search(before)
        sentence = re.split(r"(?<=[.!?])\s+(?=[A-Z])", before)[-1]
        recent = re.findall(r"(?:^|[\s(])(%s)\b" % NAME_ALTERNATION, sentence)
        whole = sentence + re.split(r"(?<=[.!?])\s+(?=[A-Z])", after)[0]
        # The languages of the forms already given that the sentence also holds, √ɬal for ɬal-m.
        beside = {known[one] for one in known if one != form and re.search(r"(?<!\S)%s(?![^\s,.;)’])" % re.escape(one), whole)}
        if named:
            language = NAMED[named.group(1)]
        elif form.startswith("*"):
            # The stage a starred form is set up for is named after it, *s.čə́txʷən as the
            # Proto-Central Salish form, past a remark in brackets.
            stage = re.match(r"^\s*(?:\([^)]*\)\s*)?as the (Proto-[\w-]+(?: Salish)?) form", after)
            language = NAMED.get(stage.group(1), stage.group(1)) if stage else "Proto-Salish"
        elif form in SEEN and about == L:
            # A form met before keeps the language it was given; a later mention in another
            # language's paragraph, under s.mx̌-ikn̓, names no new one.
            continue
        elif about == L and recent:
            # A paragraph about no one language, in §4 and §5, gives a form to the language its
            # sentence named last: Squamish (ʔə)sqʷúqʷus and qʷúqʷusam 'porcupine.'
            language = NAMED[recent[-1]]
        elif about == L and len(beside) == 1:
            # Or, its sentence naming none, to the one language of the forms it sets the form
            # among: Squamish √ɬal with the shape ɬal-m, and ƛ̕ə in Squamish sƛ̕əɬálm.
            language = beside.pop()
        else:
            language = about
        SEEN.add(form)
        known.setdefault(form, language)
        gloss = gloss_after(after)
        if not gloss:
            # The forms of a pair share the gloss after the second, yəqʷ-íl-mət or s.yəqʷ-íl-mətxʷ
            # for 'male grizzly bear', or take theirs in turn after respectively.
            pair = re.match(r"\s+(or|and)\s+\S+(?:\s+s\.p\S+)?\s+for\s+(respectively\s+)?(‘[^’]*’)(?:\s+and\s+(‘[^’]*’))?", after)
            second = re.match(r"\s+for\s+respectively\s+‘[^’]*’\s+and\s+(‘[^’]*’)", after)
            if pair and (pair.group(1) == "or") != bool(pair.group(2)):
                gloss = re.sub(r"[,.]’$", "’", pair.group(3))
            elif second:
                gloss = re.sub(r"[,.]’$", "’", second.group(1))
        found.append((form, language, gloss or ""))
    return found


SEEN = set()
# The language each form of the prose was first given, across its paragraphs.
KNOWN = {}


# The language a paragraph of §3 is about, named in its first sentence before the word for what it
# gives: The Comox data, the Lower Chehalis items, the Spokane forms.
OPENING = re.compile(r"^[^.]*?\b(%s)(?: \([^)]*\))? (?:data|forms?|items)\b" % NAME_ALTERNATION)
TABLE_FORMS = {row[3] for row in paper.rows if row[0].startswith("table, ")}
rows, done = [], set()
for row in paper.rows:
    if row[0].startswith("§") and row[2] in ("cited form", "cited affix"):
        continue
    rows.append(row)
    if row[2] == "note" and row[0].startswith("§"):
        opening = OPENING.match(row[3])
        about = NAMED[opening.group(1)] if opening else L
        for form, language, gloss in prose_forms(row[3], about):
            if (form, language) in done:
                continue
            done.add((form, language))
            # A root is marked √ or called one, a second root, gla-; an affix opens or closes on its
            # hyphen or dash, -eqs, –aɬ- and *-ik(n).
            if form.startswith("√") or "root, %s" % form in row[3]:
                kind = "root"
            elif re.match(r"^\*?[-–]", form) or form.endswith(("-", "–")):
                kind = "cited affix"
            else:
                kind = "cited form"
            rows.append([row[0], language, kind, form, "%s, in the prose%s" % (row[4], ", " + gloss if gloss else "")])
paper.rows = rows
# The title carries footnote 1 on its closing quote, 'grizzly bear'1.
for row in paper.rows:
    if row[2] == "title" and row[3].endswith("’1"):
        row[3], row[4] = row[3][:-1], row[4] + ", carries footnote 1"
# The references, an entry to each line at the margin: one under the author above opens on a dash
# (U+2014 twice), a place wrapped onto its own line, Victoria, B.C.: under Akrigg and Nespelem,
# Washington: under Kinkade 1981, carries on the entry above, and Lincoln and Rath's entry ends
# with no stop before Mattina's. The author's name and address close the last page.
first = paper.find(r"^References$")
address = paper.find(r"^jvaneijk@", first)
paper.reference_lines_run_on = {paper.find(r"^Victoria, B\.C\.:\s+Sono", first), paper.find(r"^Nespelem, Washington:", first)}
mattina = paper.find(r"^Mattina, Anthony\.", first)
entries = []
for start, end in ((first + 1, mattina - 1), (mattina, address - 2)):
    entries += paper.references(start, end, opens=r"^——\.|^[A-Z][\w’'\-]+(?:,| &| and)"
                                                  r"|^[A-Z][\w’'\-]+(?: [A-Z][\w’'\-]+)*\. \d{4}\.")
paper.rows = [row for row in paper.rows if row[2] != "reference"]
for entry, at in entries:
    paper.add("references", A, "reference", entry, "page %d" % at)
paper.add("references", A, "note", paper.joined([address - 1, address]), "page %d, the author's name and address" % paper.page(address))
paper.meta = {
    "title": "Salish words for ‘black bear’ and ‘grizzly bear’",
    "byline": "Jan P. van Eijk, First Nations University of Canada",
    "volume": "44",
    "whose": "The forms of the table are each language's as the sources in §3 give them: Kinkade (1991a) for most "
             "languages, Nater for Bella Coola, Timmers for Sechelt, Kuipers for Squamish and Shuswap, Galloway for "
             "Upriver Halkomelem, Nooksack and Samish, Montler for Saanich and Klallam, Hess for Lushootseed, "
             "Thompson and Thompson for Thompson, Mattina for Okanagan, and the author's own research for Lillooet. "
             "The proto-forms are Kinkade's and Kuipers's.",
    "letters": "The forms are in the Americanist Phonetic Alphabet, standardized by the author: x̌ for the "
               "voiceless uvular fricative, ̕ for glottalization over or after its letter, k̕ʷ and ƛ̕, ʔ, ɬ, ə, "
               "ʷ, the acute of stress, and a period after a prefix, s.čə́txʷn. The pre-APA forms keep their "
               "sources' letters, ′ for stress, ō and ū, ˑ for length, and ι and υ in Tenino k·ι′t ʷυn.",
}
paper.write()
