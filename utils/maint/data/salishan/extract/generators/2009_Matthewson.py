"""The ops of 2009_Matthewson: Lisa Matthewson's Mood and modality in St'át'imcets and beyond. The
St'át'imcets subjunctive appears in nine environments and is not selected by attitude verbs; after
Portner (1997), it restricts the conversational background of a governing modal so as to weaken the
modal's quantificational force, the dimension along which St'át'imcets modals differ from
Indo-European ones.

Beside its interlinear examples the paper numbers four tables, (4), (6), (7) and (28), and many
displays with no forms in them: English sentences, definitions and formulas from the semantic
literature, each a note to its item and a citation to its source. The tables are read off renders
of pages 3, 4, 11 and 55 and checked letter for letter against the lines the text layer gives.
Examples the paper repeats, (17b) on page 25 and eleven more, are read again where they stand.
"""
import os
import re
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "St’át’imcets"
AUTHORS = ["Lisa Matthewson"]
paper = gen.Paper("2009_Matthewson", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Carl Alexander", "St’át’imcets consultant, the acknowledgments"),
         ("Gertrude Ned", "St’át’imcets consultant, the acknowledgments"),
         ("Laura Thevarge", "St’át’imcets consultant, the acknowledgments"),
         ("Rose Agnes Whitley", "St’át’imcets consultant, the acknowledgments"),
         ("Beverley Frank", "the late St’át’imcets consultant, the acknowledgments"),
         ("David Beaver", "thanked, footnotes 24 and 29 and the acknowledgments"),
         ("Henry Davis", "thanked; Davis (2000, 2006, 2009), the grammar of Upper St’át’imcets"),
         ("Peter Jacobs", "thanked, the acknowledgments; Jacobs (1992), Squamish subordinate clauses"),
         ("Meagan Louie", "the UBC Pragmatics Research Group, the acknowledgments"),
         ("Scott Mackie", "the UBC Pragmatics Research Group, the acknowledgments"),
         ("Amélia Reis Silva", "the UBC Pragmatics Research Group, the acknowledgments"),
         ("Ryan Waldie", "the UBC Pragmatics Research Group, the acknowledgments"),
         ("Portner", "Paul Portner (1997, 2003, 2004, 2007, 2009), moods restrict the conversational background"),
         ("Palmer", "F. R. Palmer (2001), Mood and Modality, the Indo-European and Amele data"),
         ("van Eijk", "Jan van Eijk (1997), the grammar of Lillooet and the practical orthography"),
         ("Kratzer", "Angelika Kratzer (1981, 1991, 2009), modal bases and ordering sources"),
         ("Farkas", "Donka Farkas (1992, 2003), assertion and mood choice"),
         ("Giannakidou", "Anastasia Giannakidou (1997, 1998, 2009), veridicality"),
         ("Giorgi", "Alessandra Giorgi, with Pianesi (1997)"), ("Pianesi", "Fabio Pianesi, with Giorgi (1997)"),
         ("James", "Frances James (1986), the English subjunctive"),
         ("von Fintel", "Kai von Fintel (2000), with Heim (2007) and with Iatridou (2007, 2008)"),
         ("Heim", "Irene Heim, with von Fintel (2007)"), ("Iatridou", "Sabine Iatridou, with von Fintel (2007, 2008)"),
         ("Rullmann", "Hotze Rullmann, Rullmann et al. (2008), St’át’imcets modals; thanked"),
         ("Schwager", "Magdalena Schwager (2005, 2006, 2008), imperatives as modals"),
         ("Kroeber", "Paul Kroeber (1999), the Salish language family"),
         ("Littell", "Patrick Littell (2009), conjectural questions; thanked"),
         ("Peterson", "Tyler Peterson (2009a, b), Gitksan evidentials; thanked"),
         ("Quer", "Josep Quer (1998, 2001, 2009), mood"), ("Hamblin", "C. L. Hamblin (1973), questions"),
         ("Condoravdi", "Cleo Condoravdi (2001), the temporal interpretation of modals"),
         ("Stalnaker", "Robert Stalnaker (1974), pragmatic presuppositions"),
         ("Krasikova", "Sveta Krasikova, with Zchechev (2005), the Sufficiency Modal Construction"),
         ("Zchechev", "Ventsislave Zchechev, with Krasikova (2005)"),
         ("Mitchell", "Keith Mitchell (2003), had better and might as well"),
         ("Faller", "Martina Faller (2002, 2006), evidentials in Cuzco Quechua")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish, the language of the paper, a.k.a. Lillooet"),
             ("Lillooet", "St’át’imcets, its other name"), ("Italian", "Palmer (2001), the Indo-European contrast"),
             ("Spanish", "Palmer (2001) and Quer, subjunctive complements"),
             ("Romanian", "Farkas (2003), mood choice under believe and want"),
             ("Catalan", "Quer, subjunctive relative clauses"), ("Amele", "Palmer (2001), realis and irrealis"),
             ("Modern Greek", "Giannakidou, the subjunctive under nonveridical operators"),
             ("French", "the conditional as a weak necessity modal, (74)"),
             ("Skwxwú7mesh", "Squamish, footnote 16"), ("Squamish", "Skwxwú7mesh, footnote 16"),
             ("Nɬeʔkepmxcín", "Thompson Salish, conjectural questions, footnote 38"),
             ("Gitksan", "conjectural questions, footnote 38"),
             ("Cheyenne", "reportatives in questions, footnote 43, after Murray (2009)"),("English", "the translations")]
GLUED = {"5": "ku=kéla7", "7": "lts7á=has=malh", "18": "kw=s=ts7as=Ø", "20": "1997", "21": "2.2."}
SPACED = {"27": "proposition."}

FOUND = paper.page_footnotes()
paper.page_footnotes = lambda *args, **kwargs: FOUND
SKIP = {one for parts, _ in FOUND.values() for one in parts} | set(paper.volume_header()) | paper.running_numbers_set()
EXAMPLE = gen.EXAMPLE

# (22) on page 39, (104) and (110) set their first letter against the number, (110)a.; the common
# reader takes an example's number only with a space after it.
for number in range(1, paper.last + 1):
    glued_letter = re.match(r"^(\(\d+\))([a-z]\.)", paper.spaced[number])
    if glued_letter:
        paper.spaced[number] = glued_letter.group(1) + " " + paper.spaced[number][len(glued_letter.group(1)):]
        paper.lines[number] = (paper.spaced[number],) + tuple(paper.lines[number][1:])


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in SKIP


def after(number, pattern):
    """The first line after number that matches pattern."""
    number += 1
    while not re.search(pattern, paper.text(number) or ""):
        number += 1
    return number


def printed_lines(first, stop):
    return [one for one in range(first, stop) if printed(one)]


def same_letters(cells, lines, label):
    """Check that the cells written from the printed lines hold their letters, all of them and no
    others, the example's number aside, whatever the order the columns set them in."""
    written = "".join(cells).replace(" ", "")
    source = "".join(paper.text(one) for one in lines).replace(" ", "").replace("(%s)" % label, "", 1)
    extra, missing = Counter(written) - Counter(source), Counter(source) - Counter(written)
    assert not extra and not missing, (label, "written only:", "".join(sorted(extra.elements())),
                                       "printed only:", "".join(sorted(missing.elements())))


def typology(heads, verb):
    """(4) and (6), the typology of Indo-European against St'át'imcets: under each head, the kind of
    item each language restricts it with, one set in italics and the other in bold. (6) heads its
    columns lexically restrict, and lexically encode where the conclusion repeats it on page 55."""
    rows = (("Indo-European", ("lexical", "context") if not verb else ("modals", "moods")),
            (LANGUAGE, ("context", "lexical") if not verb else ("moods", "modals")))
    faces = (("in italics", "in bold"), ("in bold", "in italics"))

    def block(first, where):
        label = EXAMPLE.match(paper.text(first)).group(1)
        stop = after(first, r"^(In this paper I extend|These results suggest|The analysis presented here)")
        page = paper.page(first)
        titles = ["%s %s" % (verb, head) if verb else head for head in heads]
        cells = titles + [one for language, items in rows for one in (language,) + items]
        same_letters(cells, printed_lines(first, stop), label)
        where = "(%s) line %%d" % label
        for title in titles:
            paper.add(where % 1, A, "note", title, "page %d, a column's head" % page)
        for index, (language, items) in enumerate(rows):
            paper.add(where % (index + 2), A, "note", language, "page %d, a row's head" % page)
            for title, item, face in zip(titles, items, faces[index]):
                paper.add(where % (index + 2), A, "note", item, "page %d, %s, %s, %s" % (page, language, title, face))
        return stop
    return block


def paradigms(first, where):
    """(7), the subject agreement paradigms of tsut ‘to say’: a head INDICATIVE over the indicative
    and nominalized columns, carrying footnote 3's mark, then the subjunctive; each person's three
    forms, the ending of each in bold, and the source under the table."""
    stop = after(first, r"^With transitive predicates")
    page = paper.page(first)
    title = "Subject agreement paradigms for the intransitive predicate tsut ‘to say’:"
    heads = ("INDICATIVE", "NOMINALIZED", "SUBJUNCTIVE")
    persons = (("1SG", "tsút=kan", "n=s=tsut", "tsút=an"), ("2SG", "tsút=kacw", "s=tsút.=su", "tsút=acw"),
               ("3SG", "tsut=Ø", "s=tsut=s", "tsút=as"), ("1PL", "tsút=kalh", "s=tsút=kalh", "tsút=at"),
               ("2PL", "tsút=kal’ap", "s=tsút=lap", "tsút=al’ap"), ("3PL", "tsút=wit", "s=tsút=i", "tsút=wit=as"))
    source = "(adapted from van Eijk 1997:146)"
    cells = [title, "INDICATIVE3"] + list(heads) + [one for row in persons for one in row] + [source]
    same_letters(cells, printed_lines(first, stop), "7")
    paper.add("(7) line 1", A, "note", title, "page %d, the table's caption, tsut in italics" % page)
    paper.add("(7) line 2", A, "note", "INDICATIVE3", "page %d, the head over the first two columns" % page)
    for head in heads:
        paper.add("(7) line 3", A, "note", head, "page %d, a column's head" % page)
    for index, (person, *forms) in enumerate(persons):
        where = "(7) line %d" % (index + 4)
        paper.add(where, L, "gloss", person, "page %d, a row's head" % page)
        for head, form in zip(heads, forms):
            paper.add(where, L, "transcription", form, "page %d, %s, %s, the agreement in bold" % (page, person, head))
    paper.add("(7) line 10", A, "citation", source, "page %d, under the table" % page)
    return stop


def environments(first, where):
    """(28), the nine uses of the subjunctive: each environment with its meaning in the indicative
    and in the subjunctive. A cell that wraps in its column runs on; t'u7 is set in italics."""
    stop = after(first, r"^These are all the cases")
    page = paper.page(first)
    heads = ("ENVIRONMENT", "INDICATIVE MEANING", "SUBJUNCTIVE MEANING")
    rows = (("plain assertion", "assertion", "wish"),
            ("deontic modal", "deontic necessity/ possibility", "wish"),
            ("deontic modal", "deontic necessity/ possibility", "‘pretend’"),
            ("imperative", "command", "polite request"),
            ("wh-question + evidential/future", "question", "uncertainty/ wondering"),
            ("yes-no question + evidential/future", "question", "uncertainty/ wondering"),
            ("wh-word + evidential", "question", "ignorance free relative"),
            ("scalar particle t’u7", "‘just/still’", "‘might as well’"),
            ("wh-word + scalar particle t’u7", "N/A", "indifference free relative"))
    same_letters(list(heads) + [one for row in rows for one in row], printed_lines(first, stop), "28")
    for head in heads:
        paper.add("(28) line 1", A, "note", head, "page %d, a column's head" % page)
    for index, row in enumerate(rows):
        for head, cell in zip(heads, row):
            face = ", t’u7 in italics" if "t’u7" in cell else ""
            paper.add("(28) line %d" % (index + 2), A, "note", cell, "page %d, %s%s" % (page, head, face))
    return stop


# A display item opens on a letter, a numbered presupposition, a line of a modal's parts, or a
# clause of a definition; a source in parentheses, on its own line or set off at an item's end,
# is a citation.
ITEM = re.compile(r"^(?:[1-9]\.\s|[ivx]{1,3}\.\s|\[\[|(?:Modal base|Ordering source|Universal quantification|"
                  r"When defined|If defined)\b)")
SOURCE = r"\((?:adapted from |cf\. )?(?:[A-Z]|von )[^()]*?\d{4}[^()]*\)\d*"
ROMAN = re.compile(r"^[ivx]{1,3}\.\s")
SOURCE_LINE = re.compile(r"^%s$" % SOURCE)
SOURCE_END = re.compile(r"(?:\s{3,}|(?<=[.!?])\s+)(%s)$" % SOURCE)


def items_of(lines, opens=None):
    """[(letter, text, line)] for the printed lines of a display, its wrapped lines run on."""
    found, letter = [], ""
    for index, number in enumerate(lines):
        text = paper.spaced[number].strip()
        if index == 0 and EXAMPLE.match(text):
            text = (EXAMPLE.match(text).group(2) or "").strip()
        # i. and ii. number the clauses of (66)'s analysis; i. is no letter.
        lettered = None if ROMAN.match(text) else gen.SUB.match(text)
        if lettered:
            letter, text = lettered.group(1), lettered.group(2).strip()
        # A formula carries on a clause left at its comma, For any reference situation r, ...,
        # context R, / [[maydep(φ)]]r,F,R is only defined if ...
        carried_on = found and found[-1][1].rstrip().endswith(",")
        if index == 0 or lettered or not carried_on and ITEM.match(text) or SOURCE_LINE.match(text) or \
                opens and re.match(opens, text):
            found.append([letter, text, number])
        else:
            found[-1][1] += " " + text
    return found


def write_items(label, items, count, gloss):
    """Write a display's items under label from line count + 1 on, each item's source a citation."""
    counts = {}
    for letter, text, number in items:
        here = label + letter
        counts[here] = counts.get(here, count if not letter else 0)
        source = SOURCE_END.search(text)
        pieces = [(A, "note", text[:source.start()] if source else text)] + ([(A, "citation", source.group(1))] if source else [])
        if SOURCE_LINE.match(text):
            pieces = [(A, "citation", text)]
        for who, kind, piece in pieces:
            counts[here] += 1
            paper.add("(%s) line %d" % (here, counts[here]), who, kind, re.sub(r"\s+", " ", piece).strip(),
                      "page %d, %s" % (paper.page(number), gloss if kind == "note" else "the source"))


def formal(end, opens=None, gloss="set as an example"):
    """A display with no forms in it, to the line before the first that matches end: an English
    sentence, a definition or a formula, an item to each part and a citation to its source."""
    def block(first, where):
        stop = after(first, end)
        label = EXAMPLE.match(paper.text(first)).group(1)
        write_items(label, items_of(printed_lines(first, stop), opens), 0, gloss)
        return stop
    return block


def analyzed(analysis, end):
    """An interlinear example with its analysis set under it, (66) and (109): the example to the line
    before the first matching analysis, then the analysis's clauses to the line before end."""
    def block(first, where):
        label = EXAMPLE.match(paper.text(first)).group(1)
        start = after(first, analysis)
        paper.example(first, last=start - 1, skip=SKIP)
        count = sum(1 for row in paper.rows if row[0].startswith("(%s) line " % label))
        stop = after(start, end)
        write_items(label, items_of(printed_lines(start, stop)), count, "the analysis set under the example")
        return stop
    return block


def prose(end):
    """A paragraph that opens on the next example's number, (62) shows how the best worlds are
    selected, to the line before the first that matches end: a note, with its forms and names."""
    def block(first, where):
        stop = after(first, end)
        lines = printed_lines(first, stop)
        body = paper.joined(lines)
        pages = sorted({paper.page(one) for one in lines})
        paper.add(where, A, "note", body, "page %d" % pages[0])
        paper.cited(where, body, pages)
        paper.mentions(where, body, NAMES, "name")
        paper.mentions(where, body, LANGUAGES, "language")
        return stop
    return block


def carried(end):
    """The end of a sentence a page break and a footnote cut off, The question in / (68) cannot be
    interpreted, run on to the paragraph it closes. The example after it, which the line's number
    names, follows; flow takes a block's opening number as set."""
    def block(first, where):
        stop = after(first, end)
        paragraph = next(row for row in reversed(paper.rows) if row[2] == "note" and row[0] == where)
        paragraph[3] += " " + paper.joined(printed_lines(first, stop))
        return paper.example(stop, skip=SKIP)
    return block


# (3), (81), (82), (96) and (97) set a category at the right of a translation or a gloss, ONLY
# EPISTEMIC or EVID + SBJN, a column the common reader takes for the rest of an open translation.
CATEGORY = re.compile(r"\s{3,}(ONLY [A-Z]+|(?:EVID|FUT) \+ (?:INDIC|SBJN)|INDIC|SBJN)$")


def categorized(end):
    """An example whose parts carry a category at the right, to the line before the first that
    matches end: each part's transcription, gloss and translation, and its category a note."""
    def block(first, where):
        stop = after(first, end)
        label, letter, counts = EXAMPLE.match(paper.text(first)).group(1), "", {}
        for index, number in enumerate(printed_lines(first, stop)):
            text = paper.spaced[number].strip()
            if index == 0:
                text = EXAMPLE.match(text).group(2).strip()
            lettered = gen.SUB.match(text)
            if lettered:
                letter, text = lettered.group(1), lettered.group(2).strip()
            here = label + letter
            counts[here] = counts.get(here, 0)
            category = CATEGORY.search(text)
            said = text[:category.start()] if category else text
            kind = "transcription" if lettered or index == 0 else "translation" if said.startswith("‘") else "gloss"
            for who, what, form, gloss in [(A if kind == "translation" else L, kind, said, "")] + \
                    ([(A, "note", category.group(1), ", the category set at the right of the %s" % kind)] if category else []):
                counts[here] += 1
                paper.add("(%s) line %d" % (here, counts[here]), who, what, form, "page %d%s" % (paper.page(number), gloss))
        return stop
    return block


SPEAKER = re.compile(r"^(A|B’?):\s*(.*)$")


def exchange(end):
    """(69), an exchange: A's sentence and B's two replies, each speaker's letter a note over the
    tiers of what they say, a sentence that wraps set over its gloss again, to the line before the
    first that matches end."""
    def block(first, where):
        stop = after(first, end)
        label, count, kind = EXAMPLE.match(paper.text(first)).group(1), 0, None
        for index, number in enumerate(printed_lines(first, stop)):
            text = paper.spaced[number].strip()
            if index == 0:
                text = EXAMPLE.match(text).group(2).strip()
            speaker = SPEAKER.match(text)
            rows = []
            if speaker:
                rows.append((A, "note", speaker.group(1) + ":", ", the speaker"))
                text, kind = speaker.group(2), None
            if text.startswith("‘"):
                rows.append((A, "translation", text, ""))
            else:
                kind = "transcription" if kind is None else "gloss" if kind in ("transcription", "segmentation") \
                    else "segmentation"
                rows.append((L, kind, text, ""))
            for who, what, form, gloss in rows:
                count += 1
                paper.add("(%s) line %d" % (label, count), who, what, form, "page %d%s" % (paper.page(number), gloss))
        return stop
    return block


def again(first, where):
    """An example the paper repeats where it takes it up again, read as the page sets it here."""
    return paper.example(first, skip=SKIP)


def start_of(pattern):
    """The first line an example opens on that matches pattern; prose can wrap to open on a
    reference, (70), we see that."""
    return next(one for one in range(1, paper.last + 1)
                if re.search(pattern, paper.text(one)) and EXAMPLE.match(paper.text(one)))


BLOCKS = {start_of(r"^\(4\)\s+quantificational"): typology(("quantificational force", "conversational background"), ""),
          start_of(r"^\(6\)\s+lexically restrict"): typology(("quantificational force", "conversational background"),
                                                             "lexically restrict"),
          start_of(r"^\(6\)\s+lexically encode"): typology(("quantificational force", "conversational background"),
                                                           "lexically encode"),
          start_of(r"^\(7\)\s+Subject agreement"): paradigms,
          start_of(r"^\(28\)\s+ENVIRONMENT"): environments,
          start_of(r"^\(3\)\s+a\. wá7=k’a"): categorized(r"^A simplified table"),
          start_of(r"^\(81\)"): categorized(r"^\(82\)"),
          start_of(r"^\(82\)"): categorized(r"^As argued in the above-mentioned"),
          start_of(r"^\(96\)"): categorized(r"^\(97\)"),
          start_of(r"^\(97\)"): categorized(r"^The contrast between the evidential"),
          start_of(r"^\(39\)"):formal(r"^Farkas provides", opens=r"^c \+"),
          start_of(r"^\(42\)"): formal(r"^According to this analysis"),
          start_of(r"^\(51\)"): formal(r"^Portner argues that"),
          start_of(r"^\(52\)"): formal(r"^Portner further argues"),
          start_of(r"^\(53\)"): formal(r"^\(54\)"),
          start_of(r"^\(54\)"): formal(r"^For Italian moods"),
          start_of(r"^\(58\)"): formal(r"^Rullmann et al\. \(2008\) argue"),
          start_of(r"^\(61\)"): formal(r"^For any worlds w1", opens=r"^∀"),
          start_of(r"^\(62\)\s+For a given"): formal(r"^The best worlds are those", opens=r"^∀"),
          start_of(r"^\(63\)"): formal(r"^The analysis of St’át’imcets normative"),
          start_of(r"^\(64\)"): formal(r"^Now for the subjunctive"),
          start_of(r"^\(65\)"): formal(r"^According to \(65\)"),
          start_of(r"^\(66\)"): analyzed(r"^\[\[ka\(h\)", r"^As above, maxg"),
          start_of(r"^\(69\)"): exchange(r"^Having established that"),
          start_of(r"^\(70\)"):formal(r"^\(71\) also illustrates"),
          start_of(r"^\(71\) a\."): formal(r"^von Fintel and Iatridou argue"),
          start_of(r"^\(72\)"): formal(r"^\(73\)"),
          start_of(r"^\(73\)"): formal(r"^As von Fintel and Iatridou \(2008:137\)"),
          start_of(r"^\(75\)"): formal(r"^A simple case"),
          start_of(r"^\(76\)\s+Get up"): formal(r"^\(76\) is true iff"),
          start_of(r"^\(77\)"): formal(r"^Under Schwager’s analysis", opens=r"^Given what"),
          start_of(r"^\(80\)"): formal(r"^The important feature"),
          start_of(r"^\(85\)"): formal(r"^But this is not the meaning"),
          start_of(r"^\(88\)"): formal(r"^\(89\)"),
          start_of(r"^\(89\)"): formal(r"^Next, we need", opens=r"^= \{"),
          start_of(r"^\(90\)"): formal(r"^I assume that the evidential"),
          start_of(r"^\(98\)"): formal(r"^Applying this analysis"),
          start_of(r"^\(100\)"): formal(r"^With ignorance free"),
          start_of(r"^\(101\)\s+There’s"): formal(r"^\(101\) presupposes"),
          start_of(r"^\(105\)"): formal(r"^There is no reason"),
          start_of(r"^\(108\)"): formal(r"^If we apply this analysis", opens=r"^\{s:"),
          start_of(r"^\(109\)"): analyzed(r"^\[\[\(109\)\]\]", r"^\(109\) is defined only"),
          start_of(r"^\(111\)"): formal(r"^The crucial elements")}
BLOCKS[paper.find(r"^\(62\) shows how")] = prose(r"^\(62\)\s+For a given")
BLOCKS[paper.find(r"^\(71\) also illustrates")] = prose(r"^\(71\) a\.")
BLOCKS[paper.find(r"^\(68\) cannot be interpreted")] = carried(r"^\(68\)\s+nilh")
for repeated in (r"^\(17\) b\.", r"^\(13\) b\.", r"^\(14\) b\.", r"^\(21\) b\.", r"^\(22\) b\.",
                 r"^\(16\) a\.\s+lts7á=malh", r"^\(21\) a\.\s+nká7", r"^\(22\) a\.\s+lán=ha", r"^\(24\) a\.",
                 r"^\(15\) a\.", r"^\(43\) e\.", r"^\(26\) a\.", r"^\(27\) c\."):
    lines = [one for one in range(1, paper.last + 1) if re.search(repeated, paper.text(one))]
    BLOCKS[lines[-1]] = again
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, glued=GLUED, spaced=SPACED)


def row_at(where):
    return next(row for row in paper.rows if row[0] == where)


# (10b)'s and (79b)'s translations stand under a label in lower case, intended: ‘May it rain today.’
for row in paper.rows:
    if row[0].startswith("(") and row[3].startswith("intended: ‘"):
        row[1:3] = [A, "translation"]
# (79)'s context closes on what the speaker says, You say / ‘Take mine!’, over the parts.
context = row_at("(79) line 1")
index = paper.rows.index(context)
assert [row[0] for row in paper.rows[index + 1:index + 4]] == ["(79) line 2", "(79) line 3", "(79) line 4"]
context[3] = " ".join([context[3]] + [row[3] for row in paper.rows[index + 1:index + 4]])
del paper.rows[index + 1:index + 4]


def split_right(where, pattern, gloss):
    """Take the piece of the row at where that pattern ends it with off to a note of its own after
    it, the rows of the part after it renumbered."""
    row = row_at(where)
    piece = re.search(r"\s+(%s)$" % pattern, row[3])
    row[3] = row[3][:piece.start()]
    index = paper.rows.index(row)
    part, line = re.match(r"^(.*) line (\d+)$", where).groups()
    for later in paper.rows[index + 1:]:
        numbered = re.match(r"^(.*) line (\d+)$", later[0])
        if not numbered or numbered.group(1) != part:
            break
        later[0] = "%s line %d" % (part, int(numbered.group(2)) + 1)
    paper.rows.insert(index + 1, ["%s line %d" % (part, int(line) + 1), A, "note", piece.group(1),
                                  row[4] + ", " + gloss])


def run_on(where):
    """Run the row after the one at where on to it, the rows of the part after them renumbered."""
    row = row_at(where)
    index = paper.rows.index(row)
    part = re.match(r"^(.*) line \d+$", where).group(1)
    row[3] += " " + paper.rows.pop(index + 1)[3]
    for later in paper.rows[index + 1:]:
        numbered = re.match(r"^(.*) line (\d+)$", later[0])
        if not numbered or numbered.group(1) != part:
            break
        later[0] = "%s line %d" % (part, int(numbered.group(2)) - 1)


# (47b)'s and (47c)'s contexts wrap to a second line, him. You say:
for letter in "bc":
    assert row_at("(47%s) line 2" % letter)[2] == "note"
    run_on("(47%s) line 1" % letter)
# (46a) and (46b) set the reading of the indefinite at the right of the gloss.
for letter in "ab":
    split_right("(46%s) line 6" % letter, r"\[(?:WIDE|NARROW)-SCOPE INDEFINITE\]", "the reading set at the right of the gloss")
# (104)'s two translations of each part carry the consultant's reaction at the right, notes.
for row in paper.rows:
    if row[0].startswith("(104") and row[3] in ("[accepted]", "[spontaneously given]"):
        row[1:3] = [A, "note"]
# (47c)'s translation is printed without its opening quote; the common reader takes it for a
# paragraph of prose.
gloss_47c = row_at("(47c) line 3")
translation_47c = paper.rows[paper.rows.index(gloss_47c) + 1]
assert translation_47c[3] == "If I had seen him, I would have told you.’", translation_47c
translation_47c[:3] = ["(47c) line 4", A, "translation"]
translation_47c[4] += ", printed without its opening quote"
# Footnote 33's (i) sets two English sentences, each with its use at the right, DESCRIPTIVE and
# PERFORMATIVE; they are notes, and the use a note of its own.
for letter in "ab":
    sentence = row_at("footnote 33 (i%s) line 1" % letter)
    use = re.search(r"\s+(DESCRIPTIVE|PERFORMATIVE)$", sentence[3])
    sentence[1:4] = [A, "note", sentence[3][:use.start()]]
    paper.rows.insert(paper.rows.index(sentence) + 1, ["footnote 33 (i%s) line 2" % letter, A, "note", use.group(1),
                                                       sentence[4] + ", the use set at the right"])
# Footnote 42 numbers Rocci's three presuppositions (i) to (iii), English notes; (iii) wraps
# over two lines, and the footnote's last sentences follow it as prose.
for number in ("i", "ii"):
    row_at("footnote 42 (%s) line 1" % number)[1:3] = [A, "note"]
third = row_at("footnote 42 (iii) line 1")
index = paper.rows.index(third)
tail = paper.rows[index + 1:index + 5]
assert [row[0] for row in tail] == ["footnote 42 (iii) line %d" % one for one in range(2, 6)], tail
third[1:4] = [A, "note", third[3] + " " + tail[0][3]]
paper.rows[index + 1:index + 5] = [["footnote 42", A, "note", " ".join(row[3] for row in tail[1:]), tail[1][4]]]
# The Romance and Amele examples name their language before the source at the right, as
# "(Italian; Palmer 2001:102)"; their forms are in that language, not St'át'imcets.
spoken = {}
for row in paper.rows:
    named = re.match(r"^\((Italian|Spanish|Romanian|Catalan|Amele); ", row[3])
    if row[2] == "citation" and named:
        spoken[row[0].split(" line")[0]] = named.group(1)
assert len(spoken) == 13, spoken
for row in paper.rows:
    if row[1] == L and row[0].split(" line")[0] in spoken:
        row[1] = spoken[row[0].split(" line")[0]]
# Footnote 16's (i) to (iii) are Skwxwú7mesh, from Peter Jacobs; footnote 13's (i) sets the two
# constraints of Farkas's ranking, English.
for row in paper.rows:
    if row[0].startswith("footnote 16 (") and row[1] == L:
        row[1] = "Skwxwú7mesh"
constraints = row_at("footnote 13 (i) line 1")
assert constraints[3] == "*SUBJ/+Decided *IND/-Assert", constraints
constraints[1:3] = [A, "note"]
paper.write()
