"""The ops of Nater-final: Hank Nater's The position of Bella Coola within Salish: bound morphemes,
the Bella Coola fossilized roots, proclitics, enclitics, prefixes and suffixes of Nater (1990) sorted
by origin, proto-Salish, pre-Coastal Salish, non-Salish (mainly North Wakash) or, in the paper's
term, `innovative`.

§5 lists the morphemes as entries under the number each has in Nater (1990), 0001 √a 'distal': the
entry's Bella Coola forms, their gloss and a remark, and on the lines indented under it the
cognates and reconstructions it is compared with, each cognate after the abbreviation of its
language, Sq, Ch, Li, Sh or He, and each reconstruction after a star. Each entry is a note as
printed under the label entry and its number, each Bella Coola form of its head a row of the kind
its section lists, a root, a clitic as a cited form or an affix, and each form of its indented
lines a cited form of the language its abbreviation names. A starred form after PS or pre-CS is
that stage's, a starred form glossed by Kuipers (2002) is proto-Salish, and any other starred form
is the earlier Bella Coola form the paper reconstructs. The shifts of §3, (a) to (h), are read the
same way. Figure 1's tree, Figure 2's table of deictic components, Figure 3's modified pronominal
suffixes and Figure 4's table of counts are a note to each printed line.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import residue  # noqa: E402

A, L = gen.A, gen.L
STEM = "Nater-final"
LANGUAGE = "Bella Coola"
AUTHORS = ["Hank Nater"]
paper = gen.Paper(STEM, authors=AUTHORS[0], language=LANGUAGE)
# Every hyphen the paper ends a line on is printed, North Wakash- / induced and pre- / CS.
paper.hyphen_ends_join = True
RAW = [""] + open(residue.source_path(STEM), encoding="utf-8").read().split("\n")
NAMES = [("Nater", "Hank Nater, the author; Nater (1984, 1990, 1994, 2010, 2013) and his Southern Carrier fieldnotes"),
         ("Kuipers", "Aert H. Kuipers (1967, 1969, 1974, 2002), Squamish, Shuswap and the Salish etymological dictionary"),
         ("Kinkade", "M. Dale Kinkade (1991), the Upper Chehalis dictionary"),
         ("Van Eijk", "Jan P. van Eijk (1985, 2013), the Lillooet language and dictionary"),
         ("Lincoln", "Neville J. Lincoln, with Rath (1980), the North Wakashan comparative root list"),
         ("Rath", "John C. Rath (2010), the Heiltsuk-English dictionary; with Lincoln (1980)")]
LANGUAGES = [(LANGUAGE, "the language of the paper, Nuxalk"), ("Nuxalk", "Bella Coola, its other name"),
             ("Salish", "the family Bella Coola belongs to"), ("Coastal Salish", "CS, a branch of Salish"),
             ("Interior Salish", "a branch of Salish"), ("proto-Salish", "PS, the family's ancestor"),
             ("pre-Coastal Salish", "pre-CS, the ancestor of Bella Coola and Coastal Salish"),
             ("North Wakash", "the Wakashan languages north of Bella Coola"), ("Oowekyala", "North Wakash"),
             ("Heiltsuk", "He, North Wakash"), ("Kwakiutl", "North Wakash"), ("Dakelh", "southern Dakelh, Athabascan"),
             ("Athabascan", "a northern neighbor"), ("Upper Chehalis", "Ch, Tsamosan"), ("Chehalis", "Upper Chehalis"),
             ("Squamish", "Sq, Coastal Salish"), ("Lillooet", "Li, Interior Salish"), ("Shuswap", "Sh, Interior Salish"),
             ("Colville", "Interior Salish"), ("Chinook", "the source of -uks")]
TAGS = {"Sq": "Squamish", "Ch": "Upper Chehalis", "Li": "Lillooet", "Sh": "Shuswap", "He": "Heiltsuk",
        "Colville": "Colville", "Kwakiutl": "Kwakiutl", "Oowekyala": "Oowekyala", "Chinook": "Chinook",
        "North Wakash": "North Wakash", "PS": "Proto-Salish", "pre-CS": "pre-Coastal Salish"}
KINDS = {"5.1": "root", "5.2": "cited form", "5.3": "cited form", "5.4": "cited affix", "5.5": "cited affix"}
# The subheadings of §5, each a class of morpheme over the entries under it.
SUBHEADS = {"Prepositions", "Articles", "Pre-predicatives", "Other", "Adverbs", "Imperative markers",
            "Wh-question markers", "Yes-no question markers", "Deictics", "Reduced articles",
            "Verbalizers and adjectivizers", "Somatic prefixes", "Grammatical prefixes", "Aspectual prefixes",
            "Spatial A", "Spatial B", "Circumfixes", "Pronominal suffixes", "Declarative (in)transitive", "Imperative",
            "Declarative transitive/causative", "Transitive participial", "Transitive passive",
            "Declarative causative", "Causative passive", "Modifying suffixes", "Verbal suffixes",
            "Lexical suffixes", "Nominalizing suffixes", "Formative suffixes"}
# 663 -liwa stands two spaces in from the margin where every other entry stands at it.
# A line wrapped from Rath 2010 Excel suffix list opens on a year and is no entry.
ENTRY = re.compile(r"^(\d{3,4}(?:, \d{3,4})?)\s+(?!Excel\b)(\S.*)$")
TAG = re.compile(r"(?<![\w*-])(%s)\s+(?=\S)" % "|".join(re.escape(one) for one in sorted(TAGS, key=len, reverse=True)))
GLOSS = re.compile(r"^\s*(‘[^’]*(?:’[^\s,;)][^’]*)*’)")
# The source is the citation alone; Kuipers's own form can follow it in the bracket, (Ku02:210 -wil 'canoe').
SOURCE = re.compile(r"^\s*\(((?:Ku\d\d|Ki|VE\d\d|Na\d\d|Ku\d\d\d\d)\d*(?::\s?[\d,\-– ]*\d)?)(?=\)|\s[^\d\s])")
# A token that is no form: an English word, a source, a cross-reference number.
WORDS = {"id.", "in", "and", "or", "cf.", "petrified", "suffix", "found", "beside", "as", "with", "see", "the", "of",
         "is", "if", "but", "also"}


def is_form(token):
    token = token.strip(",;")
    if not token or token in WORDS or token in TAGS or re.fullmatch(r"\d+", token) or token.startswith(("(", "‘")):
        return False
    if token.endswith(")") and token.count("(") < token.count(")"):
        return False
    return not re.fullmatch(r"[A-Za-z]{3,}\.?", token) or token in ("ta", "ti", "ci", "na")


def gloss_after(text, at):
    """The gloss in quotes standing right after position at, and the source in parentheses after it."""
    found, rest = GLOSS.match(text[at:]), text[at:]
    gloss = found.group(1) if found else ""
    source = SOURCE.match(rest[found.end():] if found else rest)
    return gloss, source.group(1) if source else ""


def italic_pieces(pages):
    """The words the paper sets in italics on the pages, each a form: a run parted at its spaces
    and the = of ʔiɬ˽ = ɬa˽, a piece of one letter or with a bracket left open, q(ʷ of *q(ʷ),
    passed over."""
    pieces = []
    for page in pages:
        for run in paper.italics().get(page, []):
            for piece in run.split():
                piece = piece.strip(",;:")
                if len(piece) < 2 or not re.search(r"[^\W\d_]", piece) or piece.count("(") != piece.count(")"):
                    continue
                if piece not in pieces:
                    pieces.append(piece)
    return pieces


def forms_in(text, pages=()):
    """[(form, language, gloss)] for the forms of a note in the order printed: the forms after an
    abbreviation, a list parted by commas, given to its language; a starred form to its stage; a
    form glossed in quotes with no abbreviation before it to Bella Coola; and any other word of the
    pages' italics, ta˽ in cf. 1516 ta˽ and ʔa-ɬay after <, to Bella Coola."""
    found, taken, spans = [], set(), []
    for tag in TAG.finditer(text):
        at = tag.end()
        while True:
            token = re.match(r"(\*{0,2}[^\s,;‘()]+(?:\([^)\s]*\)[^\s,;‘()]*)*)", text[at:])
            if not token or not is_form(token.group(1)):
                break
            form = token.group(1)
            gloss, source = gloss_after(text, at + token.end())
            found.append((at, form, TAGS[tag.group(1)], ", ".join(one for one in (gloss, source) if one)))
            taken.add(at)
            spans.append((at, at + token.end()))
            at += token.end()
            # A list goes on past a gloss and a source to the next form of the same language,
            # Sq ɬa 'DEF.PRES.WEAK.FEM', ʔaɬi 'DEF.PRESENT.STRONG.DISTAL.FEM', and stops at a tag.
            after = re.match(r"\s*‘[^’]*(?:’[^\s,;)][^’]*)*’", text[at:])
            if after:
                at += after.end()
                cited = SOURCE.match(text[at:])
                at += cited.end() if cited else 0
            gap = re.match(r",\s+(?=[^\s‘(])", text[at:])
            if not gap or TAG.match(text[at + gap.end():]):
                break
            at += gap.end()
    for star in re.finditer(r"(?<![\w*])(\*{1,2}[^\s,;‘()]*(?:\([^)\s]*\)[^\s,;‘()]*)*)", text):
        if len(star.group(1).strip("*")) < 1:
            continue
        if star.start() in taken:
            continue
        gloss, source = gloss_after(text, star.end())
        # The source after a list of starred forms is each one's, *wəs, *wis 'high, above' (Ku02:116).
        listed = re.match(r"(?:,\s*\*[^\s,;‘()]+)+", text[star.end():])
        if listed and not source:
            source = gloss_after(text, star.end() + listed.end())[1]
        before = text[:star.start()]
        stage = "pre-Coastal Salish" if re.search(r"pre-CS\s*(?:\(|:)?\s*$", before) or text.startswith("pre-CS") \
            else "Proto-Salish" if re.search(r"\bPS\s*$", before) or source.startswith("Ku02") else L
        # The colon after a reconstruction opens its cognates, *t-…: Sh t(k)- and is no part of it.
        found.append((star.start(), star.group(1).rstrip(":"), stage, ", ".join(one for one in (gloss, source) if one)))
        taken.add(star.start())
        spans.append(star.span())
    for quoted in re.finditer(r"(?<!\S)([^\s,;:‘’<=*+(][^\s,;:‘’<=*+]*)\s+‘", text):
        form = quoted.group(1)
        if quoted.start() in taken or not is_form(form) or re.search(r"(?:%s)\s+$" % "|".join(map(re.escape, TAGS)),
                                                                  text[:quoted.start()]):
            continue
        if re.fullmatch(r"\d+", text[:quoted.start()].split()[-1] if text[:quoted.start()].split() else ""):
            continue
        gloss, source = gloss_after(text, quoted.end() - 1)
        found.append((quoted.start(1), form, language_at(text, quoted.start(1)),
                      ", ".join(one for one in (gloss, source) if one)))
        spans.append(quoted.span(1))
    # The Bella Coola form a cognate in (= Ch …) is set beside, ʔaɬ˽ (= Ch ʔaɬ) and x˽ (= Ch š).
    for pair in re.finditer(r"(?<![^\s(])([^\s,;:‘’(=][^\s,;:‘’]*)\s+(?=(?:‘[^’]*’\s+)?\(=\s)", text):
        if any(start < pair.end(1) and pair.start(1) < end for start, end in spans):
            continue
        gloss, source = gloss_after(text, pair.end(1))
        found.append((pair.start(1), pair.group(1), L, ", ".join(one for one in (gloss, source) if one)))
        spans.append(pair.span(1))
    for piece in italic_pieces(pages):
        # A piece of a form set partly in italics, in of -…in, is no word of its own in the prose,
        # nor is a capitalized word, the Bella Coola of the closing list set in italics.
        if piece in WORDS or piece[:1].isupper():
            continue
        # A whole word: no star or root sign before it, √a being read above, no
        # letter before the bracket it opens on, an of -m(an)-, and an affix's hyphen the italics
        # leave upright, -ʔiɬ in (= -ʔiɬ) and ʔix- in cf. 261 ʔix-, taken in.
        for hit in re.finditer(r"(?<![^\s(<+,;:\[])(?<!\S\()-?%s-?(?![^\s,;:)\]’.])" % re.escape(piece), text):
            if any(start < hit.end() and hit.start() < end for start, end in spans):
                continue
            form, end = hit.group(0), hit.end()
            # A ’ after the run that closes no quote is the word's glottal mark, set upright, ɬq’ in
            # cf. ɬq’ 'to slap'.
            if text[end:end + 1] == "’" and text[:hit.start()].count("‘") <= text[:hit.start()].count("’"):
                form, end = form + "’", end + 1
            gloss, source = gloss_after(text, end)
            found.append((hit.start(), form, language_at(text, hit.start()),
                          ", ".join(one for one in (gloss, source) if one)))
            spans.append((hit.start(), end))
    return [one[1:] for one in sorted(found, key=lambda one: one[0])]


def language_at(text, at):
    """The language of an unstarred form at position at that no abbreviation stands right before.
    A word after an abbreviation is that language's, North Wakash -inuχʷ , -iniχʷ, until a
    cross-reference, a cf., a <, a semicolon or a bracket opened after a closed one turns back to
    Bella Coola: Sq -numut (Ku67:95) (for -cut ~ -mut,
    where -cut is entry 1737's. PS and pre-CS name a stage in the prose, of PS origin, and a stage's
    forms are starred. A form after Kuipers's (2002) page in the bracket is his reconstruction,
    (Ku02:210 -wil)."""
    if re.search(r"\(Ku02:[\d,\-–]+\s+$", text[:at]):
        return "Proto-Salish"
    tags = [tag for tag in TAG.finditer(text) if tag.end() <= at and tag.group(1) not in ("PS", "pre-CS")]
    between = text[tags[-1].end():at] if tags else ""
    # An abbreviation inside a bracket names nothing past its close, -tuɬ-/-muɬ- (= Ch -tul-/-mul-),
    # -c(an)-/-m(an)-.
    closed = between.count(")") > between.count("(")
    if tags and not closed and not re.search(r"\bcf\.|<|;|\)\s+\(|\b\d{1,4}\s+$", between):
        return TAGS[tags[-1].group(1)]
    return L


def kind_of(form, language):
    if form.startswith("√"):
        return "root"
    if language == L and (form.startswith("-") or form.endswith("-")):
        return "cited affix"
    return "cited form"


def add_forms(where, text, page, said, pages=None):
    for form, language, gloss in forms_in(text, pages if pages is not None else (page,)):
        paper.add(where, language, kind_of(form, language), form, "page %d, %s%s" % (page, said, ", " + gloss if gloss else ""))


def indented(number):
    return RAW[number].startswith(" ") and RAW[number].strip() != ""


def head_forms(head):
    """The Bella Coola forms an entry opens on, and the rest of its head."""
    forms, rest = [], head
    while rest:
        token = re.match(r"([^\s‘]+?)(,|\s=)?(?=\s|‘|$)", rest)
        # A form can open on its optional part in brackets, (ka)nus-…-m; a remark in brackets is no form.
        bracketed = token and token.group(1).startswith("(") and not re.match(r"\([^)\s]*\)[^\s)]", token.group(1))
        if not token or bracketed or token.group(1).startswith(("<", "‘")) or token.group(1) in ("various",):
            break
        forms.append(token.group(1))
        rest = rest[token.end():].lstrip()
        if rest.startswith("= "):
            rest = rest[2:]
        if not token.group(2) and not rest.startswith("="):
            break
    return forms, rest


def skipped(number):
    text = paper.text(number)
    return paper.lines[number][2] or not text or re.fullmatch(r"\d{2,3}", text)


def entries(start, where):
    """§5.1 to §5.5: the headings, the subheadings, each section's opening and closing paragraphs,
    Figure 2, and each entry with the lines under it."""
    end = paper.find(r"^6 Concluding notes")
    section, state, number = "5.1", {"entry": None}, start
    paragraph = []

    def flush():
        if paragraph:
            body = paper.joined(paragraph)
            paper.add("§" + section, A, "note", body, "page %d" % paper.page(paragraph[0]))
            add_forms("§" + section, body, paper.page(paragraph[0]), "in the prose",
                      sorted({paper.page(one) for one in paragraph}))
            paragraph.clear()

    def close():
        entry = state["entry"]
        if not entry:
            return
        label, head, notes, page = entry
        where = "entry %s" % label
        forms, rest = head_forms(head)
        paper.add(where, A, "note", " ".join([label, head] + notes), "page %d, the entry as printed" % page)
        for form in forms:
            paper.add(where, L, KINDS[section], form, "page %d, %s" % (page, rest or "its head"))
        # An entry can run on over the foot of its page.
        add_forms(where, rest, page, "in the head of the entry", (page, page + 1))
        for note in notes:
            add_forms(where, note, page, "under the entry", (page, page + 1))
        state["entry"] = None

    def open_bracket():
        """Whether the line above leaves a parenthesis open: a line opening on a number under
        one, 686 -m) after (cf., is a cross-reference wrapped and no entry. The line and not the
        entry, since 1807 leaves its (cf. open for good."""
        above = next((paper.text(one) for one in range(number - 1, start - 1, -1) if not skipped(one)), "")
        return above.count("(") > above.count(")")

    while number < end:
        text = paper.text(number)
        if skipped(number):
            number += 1
            continue
        heading = re.match(r"^(5\.\d) \S", text)
        if heading:
            close(), flush()
            section = heading.group(1)
            paper.add("§" + section, A, "heading", text, "page %d" % paper.page(number))
        elif text in SUBHEADS:
            close(), flush()
            paper.add("§" + section, A, "heading", text, "page %d, a class of morpheme under §%s" % (paper.page(number),
                                                                                                section))
        elif text.startswith("t-/c- ʔiɬ-"):
            close(), flush()
            number = figure(number, "§" + section, r"^Figure 2 ")
            continue
        elif ENTRY.match(text) and (not indented(number) or text.startswith("663 ")) and not open_bracket():
            close(), flush()
            matched = ENTRY.match(text)
            state["entry"] = [matched.group(1), matched.group(2), [], paper.page(number)]
        elif state["entry"] and indented(number) and re.match(r"^\(\d\) ", text):
            state["entry"][1] += " " + text
        elif state["entry"] and indented(number) and not re.match(r"^(?:Of |These |Below)", text):
            state["entry"][2].append(text)
        elif state["entry"] and not indented(number) and not paragraph:
            notes = state["entry"][2]
            if notes:
                notes[-1] += " " + text
            else:
                state["entry"][1] += " " + text
        else:
            close()
            if indented(number) and paragraph:
                flush()
            paragraph.append(number)
        number += 1
    close(), flush()
    return end


def figure(start, where, caption):
    """A figure a note to each printed line, to its caption, which is a note too."""
    end = paper.find(caption, start)
    for number in range(start, end + 1):
        if not skipped(number):
            paper.add(where, A, "note", paper.text(number), "page %d, %s" % (
                paper.page(number), "the caption" if number == end else "a printed line of %s" % caption[1:-1].strip()))
    return end + 1


def shifts(start, where):
    """§3's shifts (a) to (h), each a note and its forms, and the paragraph after them."""
    end = paper.find(r"^4 Data organization")
    items, number = [], start
    while number < end:
        if not skipped(number):
            if re.match(r"^\([a-h]\d?\) ", paper.text(number)) or paper.text(number).startswith("Of these"):
                items.append([number])
            else:
                items[-1].append(number)
        number += 1
    for lines in items:
        body = paper.joined(lines)
        label = re.match(r"^\(([a-h]\d?)\)", body)
        at = "§3 (%s)" % label.group(1) if label else "§3"
        paper.add(at, A, "note", body, "page %d" % paper.page(lines[0]))
        pages = sorted({paper.page(one) for one in lines})
        # A shift is a rule, its statement up to the examples it gives, (h) *ns > nc; the starred
        # sequences of the statement, *ən# and *#yə, are no forms.
        example = body.find("(e.g.")
        if label and example > 0:
            paper.add(at, LANGUAGE, "rule", body[label.end():example].strip(), "page %d, shift (%s) of §3" % (
                paper.page(lines[0]), label.group(1)))
            add_forms(at, body[example:], paper.page(lines[0]), "a shift of §3", pages)
        else:
            add_forms(at, body, paper.page(lines[0]), "a shift of §3" if label else "in the prose", pages)
    return end


def abbreviations(start, where):
    """§4's list of abbreviations, a note, whose (1974), wrapped to open a line, is no example."""
    end = paper.find(r"^singular, SUBJ = subject", start)
    paper.add("§4", A, "note", paper.joined(range(start, end + 1)), "page %d, the abbreviations" % paper.page(start))
    return end + 1


BLOCKS = {paper.find(r"^proto-Salish$"): lambda start, where: figure(start, "§1", r"^Figure 1 "),
          paper.find(r"^\(a\) pre-suffix"): shifts,
          paper.find(r"^Abbreviations used in this report"): abbreviations,
          paper.find(r"^5\.1 Fossilized roots"): entries,
          paper.find(r"^Contemporary Origin Modification"): lambda start, where: figure(start, "§6", r"^Figure 3 "),
          paper.find(r"^Category Provenience"): lambda start, where: figure(start, "§6", r"^Figure 4 ")}
paper.reference_lines_run_on = [paper.find(r"^Amsterdam, Netherlands\.$", paper.find(r"^References$")),
                                paper.find(r"^Heiltsuk Cultural Education Centre", paper.find(r"^References$"))]
paper.meta = {
    "title": "The position of Bella Coola within Salish: bound morphemes",
    "byline": "Hank Nater",
    "volume": "49",
    "whose": "The Bella Coola forms are the author's, from his dictionary, Nater (1990), whose entry numbers the "
             "entries keep. Each cognate is the language its abbreviation names, from the source in parentheses "
             "after it: Squamish from Kuipers (1967, 1969), Shuswap from Kuipers (1974), Upper Chehalis from "
             "Kinkade (1991), Lillooet from Van Eijk (1985, 2013), Heiltsuk and North Wakash from Rath (2010) and "
             "Lincoln and Rath (1980), and the proto-Salish reconstructions from Kuipers (2002). A starred form "
             "with no stage named is the earlier Bella Coola form the author reconstructs.",
    "letters": "The Bella Coola forms are in the author's Americanist orthography: ’ for the glottal stop's "
               "release, written after the letter, t’ and k’ʷ, ʔ for the glottal stop, ʷ for rounding, ɬ, ƛ’, "
               "x, χ and c, ˑ for length, and ˽ (U+02FD) for the boundary of a clitic, ʔaɬ˽ and ˽tχ. The "
               "cognates keep their sources' letters, ǝ (U+01DD) for schwa, š, č, the acute of stress, and ṇ "
               "in Kwakiutl maq’ʷṇs.",
}
# §2's heading holds the stop of vs., which gen.HEADING reads as no heading, and the headings after
# it then fail to follow one; the six are named here, and §5.1 to §5.5 are the entries block's.
HEADINGS = {paper.find(r"^%s " % label): label for label in ("1", "2", "3", "4", "5", "6")}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, headings=HEADINGS)

# §6's paragraphs are read as §5's are, each cognate given to the language its abbreviation names,
# the preposition š of (= Ch š) to Upper Chehalis; the forms of its figures are their notes.
rebuilt = []
for row in paper.rows:
    if row[0] == "§6" and row[2] == "cited form" and "in italics" in row[4]:
        continue
    rebuilt.append(row)
    if row[0] == "§6" and row[2] == "note" and re.fullmatch(r"page \d+", row[4]):
        page = int(row[4].split()[1])
        for form, language, gloss in forms_in(row[3], (page,)):
            rebuilt.append(["§6", language, kind_of(form, language), form,
                            "page %d, in the prose%s" % (page, ", " + gloss if gloss else "")])
paper.rows = rebuilt

# The note on the author's name is marked by a private-use glyph, U+F020, which page_footnotes
# finds no mark in, and the paragraph over it at the foot of page 1 runs on into it.
CONTACT = " Contact info: "
at = next(at for at, row in enumerate(paper.rows) if row[2] == "note" and CONTACT in row[3])
paper.rows[at][3], contact = paper.rows[at][3].split(CONTACT)
paper.rows[at][3] = paper.rows[at][3].replace("", "").rstrip()
paper.rows.insert(at + 1, ["footnote", A, "note", "Contact info: " + contact,
                           "page 1, the footnote on the author's name, its mark the private-use glyph U+F020"])
# The author line holds the name and that mark.
paper.rows = [row for row in paper.rows if not (row[2] == "note" and row[4] == "page 1, the author line")]


def split_reference(opening):
    """Part the reference row that runs the entry opening onto the entry before it: the reader
    opens no entry on Van Eijk, whose name is two words."""
    at = next(at for at, row in enumerate(paper.rows) if row[2] == "reference" and " " + opening in row[3])
    row = paper.rows[at]
    row[3], after = row[3].split(" " + opening, 1)
    paper.rows.insert(at + 1, [row[0], row[1], "reference", opening + after, row[4]])


split_reference("Van Eijk, Jan P. (1985).")
split_reference("Van Eijk, Jan P. (2013).")
paper.write()
