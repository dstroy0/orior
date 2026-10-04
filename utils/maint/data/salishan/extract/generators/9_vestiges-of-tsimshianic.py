"""The ops of 9_vestiges-of-tsimshianic: Hank Nater on Tsimshianic and other Penutian vestiges in
Bella Coola: 31 Bella Coola morphemes matched in Tsimshianic, with or without North Wakashan,
Quileute and Tlingit, Bella Coola vowel length traced to Tsimshianic pre-glottalization, and links
with Coast Oregon Penutian and Upper Chehalis.

The entries of §2 and the list of §4.3 are one shape: a head that sets a Bella Coola form and its
gloss against its matches, BC ʔaχʷ ‘not’ = Ni (F) ʔaχ- ‘not’, then comments set in a line. The head
is a note, and each form it gives is a cited form under its language, Ni Nisqaʔ, Gi Gitksan and so
on, with the gloss it gives; a form two languages share, Gi (H) & Ni (F) saxʷ, is cited under each.
A source in brackets, (F) or (Tarpent, p.c.), and a bracketed aside stay in the note alone. Each
comment is a note. The text layer opens each comment's first line with a space, which tells it from
the head's wrapped lines; an entry ends at a blank line with no comment after it. Figures 2, 3, 5,
6 and 7 are set as text and are a note to each printed line and a caption; Figures 1, 4, 8 and 9
are pictures and a caption.
"""
import os
import re
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import residue  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Bella Coola"
AUTHORS = ["Hank Nater"]
STEM = "9_vestiges-of-tsimshianic"
paper = gen.Paper(STEM, authors=", ".join(AUTHORS), language=LANGUAGE)
RAW = [""] + open(residue.source_path(STEM), encoding="utf-8").read().split("\n")

NAMES = [("Franz Boas", "The Mythology of the Bella Coola Indians (1898), Salishan Texts (1895)"),
         ("Stanley Newman", "the North Wakashan origin of Bella Coola vocabulary (1973)"),
         ("Tarpent", "Marie-Lucie Tarpent, proto-Tsimshianic (1997) and personal communication"),
         ("Dunn", "John A. Dunn, the Sm’algyax dictionary (1995)"),
         ("Hindle & Rigsby", "Lonnie Hindle and Bruce Rigsby, the Gitksan dictionary (1973)"),
         ("Lincoln & Rath", "Neville J. Lincoln and John Rath, North Wakashan roots (1980), Haisla (1986)"),
         ("Rath", "John Rath, the Heiltsuk-English dictionary (2010) and personal communication"),
         ("Peterson", "Tyler R. G. Peterson, Gitksan modality (2010)"),
         ("Matthewson", "Lisa Matthewson, Gitksan modals (2013)"),
         ("Davidson", "Matthew Davidson, Nootkan grammar (2002)"),
         ("Powell & Woodruff", "J. V. Powell and Fred Woodruff, Sr., the Quileute dictionary (1976)"),
         ("Edwards", "Keri Edwards, the Tlingit dictionary (2009)"),
         ("Kuipers", "Aert H. Kuipers, the Salish etymological dictionary (2002)"),
         ("Kinkade", "M. Dale Kinkade, Upper Chehalis (1991), proto-Salish *-awalxʷ (1989), Alsea pronouns (2005)"),
         ("Swadesh", "Morris Swadesh, the century units of divergence"),
         ("Baker", "James W. E. Baker, Bella Coola prehistory (1973)"),
         ("Carlson", "Roy Carlson, the Cathedral phase at Kwatna"),
         ("Hobler", "Philip M. Hobler, the Bella Coola survey (1970)"),
         ("Suttles & Elmendorf", "W. Suttles and W. W. Elmendorf, Salish prehistory (1963)"),
         ("Frachtenberg", "Leo J. Frachtenberg, Coos (1914) and Siuslawan (1917)"),
         ("Hymes", "Dell Hymes, Siuslaw phonology (1966)"),
         ("Walker", "Deward E. Walker, Jr., the Yakama trade system (1997)"),
         ("Rigsby & Ingram", "Bruce Rigsby and John Ingram, Gitksan obstruents (1987)"),
         ("Beavert & Hargus", "Virginia Beavert and Sharon Hargus, the Yakama Sahaptin dictionary (2009)")]
LANGUAGES = [(LANGUAGE, "Nuxalk, Salish"), ("Nuxalk", "Bella Coola"), ("Tsimshianic", "the family the paper compares"),
             ("Penutian", "the stock Tsimshianic is placed in"), ("North Wakashan", "Wakashan, the other source of Bella Coola copies"),
             ("Coast Salish", "Salish"), ("Salish", "the family"), ("Heiltsuk", "North Wakashan"),
             ("Kwakwala", "North Wakashan"), ("Kwak’wala", "North Wakashan"), ("Oowekyala", "North Wakashan"),
             ("Ooweekeno", "North Wakashan"), ("Haisla", "North Wakashan"), ("Athabascan", "Na-Dene"),
             ("Nisqaʔ", "Inland Tsimshianic"), ("Gitksan", "Inland Tsimshianic"), ("North Tsimshian", "Maritime Tsimshianic"),
             ("Sm’algyax", "North Tsimshian, Dunn's name"), ("Sgüüχs", "Maritime Tsimshianic, extinct"),
             ("Quileute", "Chimakuan"), ("Tlingit", "Na-Dene"), ("Nootkan", "South Wakashan"),
             ("Chimakuan", "the family"), ("South Wakashan", "Wakashan"), ("Chinookan", "the family"),
             ("Chinook", "Chinookan"), ("Siuslaw", "Coast Oregon Penutian"), ("Coos", "Coast Oregon Penutian"),
             ("Alsea", "Coast Oregon Penutian"), ("Tillamook", "Coast Salish"), ("Upper Chehalis", "Tsamosan Salish"),
             ("Interior Salish", "Salish"), ("Kalispel", "Interior Salish"), ("Colville", "Interior Salish"),
             ("Spokane", "Interior Salish"), ("Squamish", "Coast Salish"), ("Sechelt", "Coast Salish"),
             ("Cowlitz", "Tsamosan Salish"), ("Quinault", "Tsamosan Salish"), ("Tahltan", "Athabascan"),
             ("Sahaptin", "Sahaptian"), ("Takelma", "Oregon Penutian"), ("English", "the contact language")]

# The head's language abbreviations, from footnote 3, and the names it defines.
ABBREVIATIONS = {"BC": L, "Ni": "Nisqaʔ", "Gi": "Gitksan", "NT": "North Tsimshian", "He": "Heiltsuk",
                 "Ha": "Haisla", "Oo": "Oowekyala", "PT": "proto-Tsimshianic", "Ts": "Tsimshianic",
                 "NW": "North Wakashan", "TEA": "Tlingit-Eyak-Athabascan", "Ch": "Upper Chehalis",
                 "Quileute": "Quileute", "Nootkan": "Nootkan", "Tlingit": "Tlingit", "Chinook": "Chinook",
                 "proto-Salish": "proto-Salish", "proto-Coast Salish": "proto-Coast Salish",
                 "proto-Interior Salish": "proto-Interior Salish", "Coast Oregon Penutian": "Coast Oregon Penutian"}
OPENS = re.compile(r"(?:^|(?<=[=,&] ))(%s)(?= )" % "|".join(
    re.escape(one) for one in sorted(ABBREVIATIONS, key=len, reverse=True)))
GLOSS = re.compile(r"‘(.*?)’(?!\w)")
# A gloss label after a form in §4.3's list, -(s)t(u)- CAUS and -nχ 2SG.SBJ.
LABEL = re.compile(r"^(\S+) ([1-3]?[A-Z]{2,}(?:\.[A-Z]+)*)$")

# The paper sets its forms in italics, in plain letters too, NT taagan, BC mnmnta. A form in the
# prose is the language's the text names right ahead of it, with its source in brackets or a star
# between, cf. Kwakwala √ʔwm, PT *łaq-ʔ[a]s-kʷ. A form with no language named ahead of it is
# Bella Coola's, save those below, each read off its sentence.
NAMED = dict(ABBREVIATIONS, **{name: name for name, _ in LANGUAGES})
NAMED.update({LANGUAGE: L, "Nuxalk": L})
NAMED_AHEAD = re.compile(r"(?:^|[\s(])(%s)(?: \([^()]*\))?\s*[*(]*[-–]?$" % "|".join(
    re.escape(one) for one in sorted(NAMED, key=len, reverse=True)))
FORM_LANGUAGE = {
    # Tarpent's account of Tsimshianic's sounds on page 4, quoted: CT has only X; others also have x,
    # xw ~ Xw ... glides ü and ü', ... counterparts of w and w'.
    "X": "Tsimshianic", "x": "Tsimshianic", "xw": "Tsimshianic", "Xw": "Tsimshianic",
    "ü": "Tsimshianic", "w": "Tsimshianic",
    # Note 4 on page 5: the Tsimshianic stops rendered as bV, dV, gV, written pV, tV, kV, where c equals
    # Tarpent's ts; then NT's plain against aspirated pairs, taagan ‘planking’ vs. daaw ‘frozen’.
    "bV": "Tsimshianic", "dV": "Tsimshianic", "gV": "Tsimshianic", "pV": "Tsimshianic",
    "tV": "Tsimshianic", "kV": "Tsimshianic", "c": "Tsimshianic", "ts": "Tsimshianic",
    "taagan": "North Tsimshian", "daaw": "North Tsimshian", "puksk": "North Tsimshian",
    "bu’il": "North Tsimshian", "kyooxt": "North Tsimshian", "gyoos": "North Tsimshian",
    # Entry (1): Ts origin: Tarpent (p.c.) relates ʔaχ- to ʔaq ‘not to be’.
    "ʔaq": "Tsimshianic",
    # Entry (15): Gi (P:140) ˽ima˽s ... = ˽ima + ˽s (a noun determiner).
    "˽ima": "Gitksan", "˽s": "Gitksan",
    # Entry (17): Tarpent states that st… is "common in some Northern Penutian", the Ts st… of the head.
    "st…": "Tsimshianic",
    # Kinkade on page 14, quoted: If Alsea has borrowed from Salish, how did it get forms with p or m;
    # and page 15, the *p → h and *m → w shifts of Tillamook.
    "p": "Alsea", "m": "Alsea", "h": "Tillamook",
    # Kwak’wala ʒunuq’ʷa ‘Sasquatch’ (√ʒuqʷ ‘to pucker lips’), page 2.
    "ʒuqʷ": "Kwak’wala",
    # Entry (19): Oo hauhaukʷ is derived from √hwkʷ.
    "hwkʷ": "Oowekyala",
    # Entry (21): Tarpent (p.c.) posits PT *łaq-ʔ[a]s-kʷ = √łaq- ANTIP.
    "łaq": "proto-Tsimshianic",
    # Page 12: Oo xʷuxʷciʒa ‘mountain goat suet’, now analyzable as √xʷw(xʷ)s ‘ball, airbag, lungs’ +
    # -siʒ-a ‘foot, base’, may be based on substratal *xʷu(xʷ)ci ~ *χʷu(χʷ)ci.
    "xʷw(xʷ)s": "Oowekyala", "siʒ-a": "Oowekyala", "xʷu(xʷ)ci": "substrate", "χʷu(χʷ)ci": "substrate",
    # Page 13: As concerns Siuslaw -muxʷ/-muχʷ.
    "muχʷ": "Siuslaw",
}
# Italic runs that are no form: the map's label Heiltsuk and its letter u on page 4, note 4's plain vs.
# aspirated, the terms century unit and relative units of §4.1, the words Nater italicizes in
# Kinkade's passage on page 14 and in his own after it. Squamish -way in Figure 6 on page 13 is cited
# off the figure's line, ahead of the English the other way around.
NOT_FORMS = {"Heiltsuk", "u", "plain vs. aspirated", "century units", "century unit", "relative units",
             "Intermarriage", "slavery", "did", "recently", "prehistoric", "protohistoric"}


def form_language(run):
    if run in NOT_FORMS or len(run.split()) >= 4 and not gen.orthographic(run):
        return None
    return FORM_LANGUAGE.get(run, L)


def language_before(text):
    named = NAMED_AHEAD.search(text[-60:])
    return NAMED[named.group(1)] if named else None


paper.form_language = form_language
paper.language_before = language_before
# An italic run that sets two forms of an alternation, łuk’ ~ √łuuk on page 10, or a list, -maxʷ, -χ
# on page 13, is two forms. Tahltan xú∙ʒe on page 12 sets its raised dot upright between two italic
# runs, and is one form. A run the page also sets with an upright hyphen ahead, awχ of -awχ on page
# 13, is that one. The marks the upright type sets at a short form's edge are the form's: the
# ellipsis of BC …ił and qacq… on page 6 and of …χ on page 13, the clitic mark of BC ˽ma on page 7,
# each named below, and a glottal mark after the form, kic’ ~ √kiic on page 10.
AS_PRINTED = {(6, "ił"): "…ił", (6, "an"): "…an", (6, "qacq"): "qacq…", (7, "ma"): "˽ma",
              (7, "ima"): "˽ima", (7, "mas"): "˽mas", (7, "s"): "˽s", (7, "st"): "st…", (9, "st"): "-st",
              (13, "χ"): "…χ", (13, "t’əχʷ"): "*t’əχʷ", (14, "əłkʷ-ən"): "*m-əłkʷ-ən", (14, "mołkʷ"): "*mołkʷ"}
# The repairs residue makes to the page text, made to the runs too: (27)'s He q'ʷḿ̩̩xsiwa on page 9.
REPAIRS = [(raw, fixed) for raw, fixed in residue.CORRECTIONS.get(STEM, ()) if raw]
PAGE_TEXT = {}
for number in range(1, paper.last + 1):
    PAGE_TEXT[paper.page(number)] = PAGE_TEXT.get(paper.page(number), "") + " " + paper.text(number)
for page, runs in paper.italics().items():
    text = PAGE_TEXT.get(page, "")
    # A line's hyphens joined up, sm̩-nm̩nm̩- at a line's end and uuc on the next as one word.
    joined = re.sub(r"-\s+", "-", text)
    runs[:] = [one for run in runs for part in run.split(" ~ ") for one in part.split(", ")]
    for at, run in enumerate(runs):
        # The layer's soft hyphen at a line's end, nu-ʔakʷn-als- im on page 7, is the hyphen the
        # page prints.
        run = run.replace("￾", "-")
        for raw, fixed in REPAIRS:
            run = unicodedata.normalize("NFC", run.replace(raw, fixed))
        # The italic reader sets a space after the syllabic mark and after a hyphen that the page
        # does not print, nánáskʷm̩ ala for nánáskʷm̩ala on page 2.
        closed = re.sub(r"(?<=[̩-]) +", "", run)
        if run not in joined and closed in joined:
            run = closed
        runs[at] = run
    # Two runs the upright type joins with a hyphen or a raised dot are one form: Tahltan xú∙ʒe on
    # page 12, Quileute ʔó∙-t'iqʷ on page 13.
    for at in reversed(range(len(runs) - 1)):
        for mark in ("∙", "-"):
            both = runs[at] + mark + runs[at + 1]
            if not runs[at].endswith(mark) and re.search(r"(?<![\w’])%s(?![\w’])" % re.escape(both), joined):
                runs[at:at + 2] = [both]
                break
    for at, run in enumerate(runs):
        held = r"(?<![\w’])%s" % re.escape(run)
        if (page, run) in AS_PRINTED:
            runs[at] = AS_PRINTED[page, run]
        elif not re.search(held + r"(?![\w’])", text) and re.search(held + r"’(?![\w’])", text):
            runs[at] = run + "’"
    runs[:] = [run for run in runs if "-" + run not in runs]

found = paper.page_footnotes()
SKIP = {one for parts, _ in found.values() for one in parts} | set(paper.volume_header())
RUNNING = paper.running_numbers_set()
BODY_END = paper.find(r"^References$")
HEADINGS = paper.headings(paper.find(r"^Keywords:") + 1, BODY_END - 1, skip=SKIP)
SECTION = {heading: number for number, heading in HEADINGS.items()}


def bracket_end(text, start):
    """The index after the bracket that closes the one opening at start."""
    depth = 0
    for index in range(start, len(text)):
        depth += {"(": 1, ")": -1}.get(text[index], 0)
        if depth == 0:
            return index + 1
    return len(text)


def forms(segment, asides):
    """[(form, [glosses])] of one language's stretch of a head: its forms, each a comma apart, with
    the quoted glosses after them; a bracket that opens after a space is a source or an aside. An
    aside that opens on a star is the form's older shape, BC t’nχʷ (*t’əχʷ), and is the language's
    form too; one that names a language gives that language's forms, (and cf. Nootkan √k’ʷič
    ‘spiny’), and these go on asides as (language, form, glosses)."""
    found, pending, chunk, index = [], [], "", 0

    def close():
        text = chunk.strip(" =&")
        if not text:
            return
        # (20) prints BC c'ik'ʷic' sea urchin' with no opening quote.
        if " " in text and text.endswith("’"):
            form, gloss = text.split(" ", 1)
            found.append((form, [gloss[:-1]]))
            return
        labeled = LABEL.match(text)
        if labeled:
            found.append((labeled.group(1), [labeled.group(2)]))
            return
        found.append((text, []))
        pending.append(len(found) - 1)

    while index < len(segment):
        here = segment[index]
        if here == "‘":
            quoted = GLOSS.match(segment, index)
            close()
            chunk = ""
            gloss = quoted.group(1) if quoted else segment[index + 1:]
            if pending:
                for one in pending:
                    found[one][1].append(gloss)
                del pending[:]
            elif found:
                found[-1][1].append(gloss)
            index = quoted.end() if quoted else len(segment)
        elif here == "(" and (index == 0 or segment[index - 1] == " "):
            close()
            chunk = ""
            end = bracket_end(segment, index)
            inside = segment[index + 1:end - 1]
            if inside.startswith("*"):
                found.append((inside, []))
            else:
                inside = re.sub(r"^(?:and )?cf\. ", "", inside)
                if OPENS.match(inside):
                    asides.extend(head_forms(inside))
            index = end
        elif here == ",":
            close()
            chunk = ""
            index += 1
        else:
            chunk += here
            index += 1
    close()
    return found


def head_forms(head):
    """[(language, form, glosses, aside)] for each form a head gives, a form two languages share
    once for each; a head that opens on its form with no abbreviation is Bella Coola's, (11)
    χʷsan-im. aside is whether the form is in a bracket that names its language."""
    opened = list(OPENS.finditer(head))
    if not opened or opened[0].start() > 0:
        segments = [(L, head[:opened[0].start() if opened else len(head)])]
    else:
        segments = []
    shared = []
    for place, one in enumerate(opened):
        end = opened[place + 1].start() if place + 1 < len(opened) else len(head)
        segment = head[one.end():end].strip()
        while segment.startswith("("):
            segment = segment[bracket_end(segment, 0):].strip()
        shared.append(ABBREVIATIONS[one.group(1)])
        if segment.startswith("&") and not segment.strip("& "):
            continue
        segments.extend((language, segment) for language in shared)
        shared = []
    asides = []
    found = [(language, form, glosses, False) for language, segment in segments
             for form, glosses in forms(segment, asides)]
    return found + [(language, form, glosses, True) for language, form, glosses, _ in asides]


def content(number):
    """Whether line number holds text of the body: no footnote, page mark or running number."""
    return number not in SKIP and number not in RUNNING and not paper.lines[number][2]


def entry(start, where):
    """An entry: its head a note and its forms cited, then each comment a note."""
    label = gen.EXAMPLE.match(paper.text(start)).group(1)
    name = "%s(%s)" % (where + " " if where.startswith("§4.3") else "", label)
    head, comments, blank, number = [start], [], False, start + 1
    while number < BODY_END:
        if not content(number):
            number += 1
            continue
        text = paper.text(number)
        if not text:
            blank = True
            number += 1
            continue
        if number in HEADINGS or gen.EXAMPLE.match(text):
            break
        # The closeup read sets three spaces before (6)'s comment on page 6.
        opens_comment = re.match(r"^ +\S", RAW[number]) is not None
        if blank and not opens_comment:
            break
        if opens_comment:
            comments.append([number])
        elif comments:
            comments[-1].append(number)
        else:
            head.append(number)
        blank = False
        number += 1
    text = paper.joined(head)
    body = gen.EXAMPLE.match(text).group(2)
    paper.add(name, A, "note", text, "page %d, the entry" % paper.page(start))
    given = []
    for language, form, glosses, aside in head_forms(body):
        # The comments' prose cites anew no form its head gives, He tχ˽a˽s … of (29) among them. The
        # root of an aside, NW (L) √łq of (21), is another language's, and BC łq ‘wet’ on page 16 is
        # cited apart.
        paper.cited_done.update((form,) if aside else (form, form.strip("-√*"), form.rstrip(" …")))
        given.append(form)
        gloss = ", ".join("‘%s’" % one if not LABEL.match("x " + one) else one for one in glosses)
        paper.add(name, language, "cited form", form,
                  "page %d, entry %s%s" % (paper.page(start), label, ", " + gloss if gloss else ""))
    # An italic run the head sets outside the forms it gives, -łp in (6)'s gloss ‘salmonberry bush
    # (-łp)’, is cited too; a run only inside a form, muχʷmuχʷ of (3)'s compound, is cited from the
    # comment.
    outside = body
    for form in sorted(given, key=len, reverse=True):
        outside = outside.replace(form, " " * len(form))
    pages = sorted({paper.page(one) for one in head})
    runs = [run for page in pages for run in paper.italics().get(page, ())]
    paper.cited(name, body, pages, skip={run for run in runs if not re.search(
        r"(?<![\w’])%s(?![\w’])" % re.escape(run), outside)})
    for lines in comments:
        note = paper.joined(lines)
        pages = sorted({paper.page(one) for one in lines})
        paper.add(name, A, "note", note, "page %d, a comment on entry %s" % (pages[0], label))
        # A form the line breaks after one of its hyphens, sm̩-nm̩nm̩- at the end of a line of (16) and
        # uuc on the next, is cited whole.
        paper.cited(name, "".join(paper.text(one) + ("" if paper.text(one).endswith("-") else " ")
                                  for one in lines if paper.text(one)), pages)
        paper.mentions(name, note, NAMES, "name")
        paper.mentions(name, note, LANGUAGES, "language")
    return number


FIGURES = {"Figure 2": r"^p t c k q kʷ qʷ$", "Figure 3": r"^\*p \*t \*ts", "Figure 5": r"^Gloss Nisqaʔ",
           "Figure 6": r"^Penutian Bella Coola", "Figure 7": r"^Bella Coola Alsea Siuslaw$"}
PICTURES = ("Figure 1", "Figure 4", "Figure 8", "Figure 9")


# Figure 5 sets a form of each Tsimshianic language in a column under its name, with the gloss ahead
# and each form's sound in square brackets after it.
COLUMNS = {"Figure 5": ("Nisqaʔ", "Gitksan", "North Tsimshian")}


def figure(label):
    def write(start, where):
        """A figure set as text: a note to each printed line, then its caption. A line's forms are
        cited: a column's under its language, and in Figure 6 each under the name ahead of it."""
        count, number = 0, start
        while not paper.text(number).startswith(label + ":"):
            if content(number) and paper.text(number):
                count += 1
                name = "%s line %d" % (label, count)
                paper.add(name, A, "note", paper.text(number),
                          "page %d, a line of the figure as the text layer reads it" % paper.page(number))
                glossed = re.match(r"^\s*(‘[^’]+’)\s*(.*)$", RAW[number])
                if label in COLUMNS and glossed:
                    for language, cell in zip(COLUMNS[label], re.split(r"\s{2,}", glossed.group(2).strip())):
                        form, _, sound = cell.partition(" ")
                        paper.add(name, language, "cited form", form, "page %d, %s, %s%s" % (
                            paper.page(number), label, glossed.group(1), " " + sound if sound else ""))
                elif label == "Figure 6":
                    paper.cited(name, paper.text(number), [paper.page(number)])
            number += 1
        return caption(label)(number, where)
    return write


def caption(label):
    def write(start, where):
        """A figure's caption, a note, with the line it wraps onto."""
        lines = [start]
        if not re.search(r"\)$", paper.text(start)):
            lines.append(start + 1)
        paper.add(label, A, "note", paper.joined(lines), "page %d, the caption" % paper.page(start))
        return lines[-1] + 1
    return write


blocks = {}
previous = SECTION["2.1"]
for count in range(1, 32):
    at = paper.find(r"^\(%d\) " % count, previous)
    blocks[at] = entry
    previous = at + 1
previous = SECTION["4.3"]
for count in range(1, 11):
    at = paper.find(r"^\(%d\) BC " % count, previous)
    blocks[at] = entry
    previous = at + 1
for label, pattern in FIGURES.items():
    blocks[paper.find(pattern)] = figure(label)
for label in PICTURES:
    blocks[paper.find(r"^%s:" % label)] = caption(label)

paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=HEADINGS)
# The contact line at the foot of page 1 carries no mark, and the text layer sets it first on the
# page, ahead of the title; it is a note of its own.
CONTACT = "Contact info: hanknater@gmail.com"
title = next(row for row in paper.rows if row[2] == "title")
assert title[3].startswith(CONTACT + " ")
title[3] = title[3][len(CONTACT) + 1:]
at = next(index for index, row in enumerate(paper.rows) if row[2] == "language")
paper.rows.insert(at, ["front", A, "note", CONTACT, "page 1, the note at the foot of the page with no mark"])
paper.write()
