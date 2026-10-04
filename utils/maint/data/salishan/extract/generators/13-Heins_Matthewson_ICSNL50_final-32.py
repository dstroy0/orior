"""The ops of 13-Heins_Matthewson_ICSNL50_final-32: Thomas J. Heins and Lisa Matthewson on the
Gitksan particle gi: no marker of spatio-temporal distance or past tense, it signals that at least
one interlocutor had prior evidence for the proposition asserted, or for a question or its answer,
with a bias to the addressee derived from Gricean reasoning.

Most examples are exchanges. A context runs over the lines under the number to the first turn;
each turn opens on its speaker, Michael:, T.J.:, A:, a note of its own, and sets a segmentation
over its gloss, wrapping in pairs, and a translation, the speaker's initials or a source at its
right. An example with no speaker sets the same tiers. A form judged with no tiers under it is a
transcription. Consultant’s comment: and Consultant: “...” lines are speaker comments, run on to
the closing quote; a researcher's question set in brackets inside one is a note, and so are the
lines between turns, Time passes …. (4) is Nisg̲a'a, from Tarpent, and (38) Cuzco Quechua, from
Faller. (51) and (57) list the generalizations, a note to each part and to its sub-cases. The
italic forms in the prose are cited by their runs, each with its language.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Gitksan"
NISGAA = "Nisg\u0332a'a"
AUTHORS = ["Thomas J. Heins", "Lisa Matthewson"]
paper = gen.Paper("13-Heins_Matthewson_ICSNL50_final-32", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("Vincent Gogag", "Gitksan consultant, Git-anyaaw (Kitwancool); VG in the examples"),
         ("Hector Hill", "Gitksan consultant, Gijigyukwhla (Gitsegukla); HH in the examples"),
         ("Barbara Sennott", "Gitksan consultant, Ansbayaxw (Kispiox); BS in the examples"),
         ("Ray Jones", "Gitksan consultant, Prince Rupert and Gijigyukwhla; RJ"),
         ("Louise Wilson", "Gitksan consultant, Ansbayaxw and Prince Rupert; LW in the examples"),
         ("Katie Bicevskis", "UBC Gitksan Research Lab, thanked"),
         ("Kyra Borland-Walker", "UBC Gitksan Research Lab, thanked"),
         ("Colin Brown", "UBC Gitksan Research Lab, thanked"),
         ("Jason Brown", "UBC Gitksan Research Lab, thanked; Gitksan phonology (2008)"),
         ("Henry Davis", "UBC Gitksan Research Lab, thanked"),
         ("Catherine Dworak", "UBC Gitksan Research Lab, thanked"),
         ("Clarissa Forbes", "UBC Gitksan Research Lab, thanked"),
         ("Aidan Pine", "UBC Gitksan Research Lab, thanked"),
         ("Alyssa Satterwhite", "UBC Gitksan Research Lab, thanked"),
         ("Michael Schwan", "UBC Gitksan Research Lab, thanked"),
         ("Yimeng Wang", "UBC Gitksan Research Lab, thanked"),
         ("Boas", "Franz Boas, Tsimshian (1911)"),
         ("Jóhannsdóttir", "Kristín M. Jóhannsdóttir, aspect in Gitxsan (2006)"),
         ("Rigsby", "Bruce Rigsby, Gitksan grammar (1986) and language shift (1987)"),
         ("Tarpent", "Marie-Lucie Tarpent, the Nisgha evidential postclitics (1984), grammar (1987), =ga'a (1998)"),
         ("Hindle", "Lonnie Hindle and Bruce Rigsby, the practical dictionary (1973)"),
         ("Kari", "James Kari, Gitksan and Wet'suwet'en relations (1987)"),
         ("Hoard", "James E. Hoard, obstruent voicing (1978)"),
         ("Ingram", "John Ingram, obstruent voicing with Rigsby (1990)"),
         ("Grenoble", "Lenore Grenoble, documenting pragmatics (2007)"),
         ("Faller", "Martina Faller, Cuzco Quechua evidentials (2002, 2007, 2011)"),
         ("Matthewson", "Lisa Matthewson, fieldwork (2004), evidentials (2011, 2012), particles (2015)"),
         ("von Stechow", "Arnim von Stechow, wieder ‘again’ (1996)"),
         ("Grice", "the Quality maxim"),
         ("Stalnaker", "Robert Stalnaker, presuppositions (1973) and assertion (1978)"),
         ("Smith", "Marsha J. Smith, Gitxsan stories (2004)"),
         ("Zimmermann", "Malte Zimmermann, discourse particles (2011)")]
LANGUAGES = [(LANGUAGE, "Tsimshianic, Interior branch; upper Skeena River, northwestern British Columbia"),
             (NISGAA, "Tsimshianic, Interior branch; Nass River Valley; (4)"),
             ("Tsimshianic", "the family"), ("Southern Tsimshian", "Tsimshianic, Tarpent (1998)"),
             ("Cuzco Quechua", "Faller's reportative and direct evidentials; (38)"),
             ("St’át’imcets", "Lillooet Salish, the evidential lákw7a"),
             ("German", "the discourse particles ja and doch"), ("English", "the translations")]
# The examples of another language, by number.
LANGUAGE_OF = {"4": NISGAA, "38": "Cuzco Quechua"}
# The lists of generalizations set as examples.
GENERALIZATIONS = ("51", "57")
# The italic runs that are forms, by language: the Gitksan orthography defines most in plain letters,
# gi and aa. Boas's -g·ê and Tarpent's -gi are Nisg̲a'a; the italic English, again and should, is
# no form.
FORM_LANGUAGE = {"gi": L, "aa": L, "ist": L, "wa": L, "we": L, "t": L, "t'ak": L, "Ha'miiyaa": L,
                 "k̲'ap/ap": L, "-gi": NISGAA, "g·ê": NISGAA, "=ga'a": "Southern Tsimshian",
                 "=mi": "Cuzco Quechua", "lákw7a": "St’át’imcets", "ja": "German", "doch": "German"}
paper.form_language = FORM_LANGUAGE.get

RUNNING = paper.running_numbers_set()
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts} | set(paper.volume_header())
# A turn's speaker and what they say, Michael: Oo naa=hl we-n=gi?, T.J.: Gwi?, BS: Ii 'nit=hl.
TURN = re.compile(r"^((?:[A-Z][a-z]*\.)+|[A-Z][A-Za-z]*):\s+(\S.*)$")
# A consultant's comment, Consultant's comment: or Consultant's comment about gi-version:.
COMMENT = re.compile(r"^Consultant’s comment[^:“]*:")
# A researcher's question set in brackets inside a comment, [Researcher: "No."].
RESEARCHER = re.compile(r"\s*(\[Researcher: “[^”]*”\])\s*")


def printed(number):
    return paper.text(number).strip() and not paper.lines[number][2] and number not in RUNNING \
        and number not in AT_FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def text_of(number):
    return paper.text(number) if number <= paper.last else ""


def gloss_line(number):
    """Whether line number is a gloss tier: a quarter of its words or more hold a label in capitals,
    ¬PPS among them. A line of prose that names a consultant, Here, VG judges that Aidan, holds one
    in ten."""
    words = text_of(number).split()
    return bool(words) and sum(gen.is_gloss(one) or one.startswith("¬") for one in words) * 4 >= len(words)


def form_start(number):
    """Whether line number opens the tiers of a sentence with no speaker: a gloss stands under it."""
    return not text_of(number).startswith(("‘", "“")) and gloss_line(after(number))


def starred(number):
    """A form judged and given no tiers, * Stacy=hl wa/we-n=aa=gi?."""
    return text_of(number).startswith(("* ", "# ")) and not form_start(number)


def opens(number):
    """Whether line number opens a turn, a sentence's tiers, a lettered part or a judged form."""
    text = text_of(number)
    return bool(TURN.match(text) or gen.SUB.match(text)) or form_start(number) or starred(number)


def closed(text):
    """Whether a translation is whole: it ends on its closing quote after a stop, or on a stop
    where the page prints no closing quote, the source at its right set aside."""
    body = re.sub(r"\s*\([^()]*\)\s*$", "", text)
    return bool(re.search(r"(?:[.?!…)\]—]’|[.?!])\d{0,2}$", body))


def quoted(line, text):
    """A comment run on while its quotes are open, or while the line under it opens on a
    researcher's question in brackets. Returns (last line, text)."""
    while True:
        below = after(line)
        if text.count("“") > text.count("”") or text_of(below).startswith("[Researcher:"):
            line, text = below, text + " " + text_of(below)
        else:
            return line, text


def example(start, where):
    """A numbered example, whole or in lettered parts, read line by line to the prose after it."""
    opened = gen.EXAMPLE.match(paper.text(start))
    number = opened.group(1)
    who = LANGUAGE_OF.get(number, L)
    state = {"label": number, "count": 0}

    def row(by, kind, form, at, gloss=""):
        state["count"] += 1
        paper.add("(%s) line %d" % (state["label"], state["count"]), by, kind, form,
                  "page %d%s" % (paper.page(at), gloss))

    def tiers(line, text):
        """A segmentation over its gloss, wrapping in pairs, to the translation and what stands at
        its right. Returns the line after it."""
        count = 0
        while not text.startswith(("‘", "“")) and not gen.JUDGED_QUOTE.match(text) and count < 10:
            row(who, ("segmentation", "gloss")[count % 2], text, line)
            count += 1
            line = after(line)
            text = text_of(line)
        at = line
        while not closed(text) and after(line) <= paper.last:
            line = after(line)
            text += " " + text_of(line)
        said, pieces = gen.split_translation(text) or (text, [])
        row(A, "translation", said, at)
        for piece in pieces:
            row(A, "citation" if gen.is_source(piece) else "note", piece, at,
                ", the source at the right of the translation")
        return after(line)

    line, text = start, opened.group(2) or ""
    while True:
        sub = gen.SUB.match(text)
        if sub:
            state.update(label=number + sub.group(1), count=0)
            text = sub.group(2)
        turn = TURN.match(text)
        if gen.CONTEXT.match(text):
            at = line
            while not (re.search(r"[.:!?]\d{0,2}$", text) and opens(after(line))) and line - at < 8:
                line = after(line)
                text += " " + text_of(line)
            row(A, "note", text, at, ", the context")
            line = after(line)
        elif turn and turn.group(2).startswith("“"):
            at = line
            line, text = quoted(line, text)
            if turn.group(1) == "Researcher":
                row(A, "note", text, at, ", the researcher's question")
            else:
                row(A, "speaker comment", text, at, ", the consultant's answer")
            line = after(line)
        elif turn:
            row(A, "note", turn.group(1) + ":", line, ", the speaker of the turn")
            line = tiers(line, turn.group(2))
        elif COMMENT.match(text):
            at = line
            line, text = quoted(line, text)
            for piece in RESEARCHER.split(text):
                if piece.startswith("[Researcher:"):
                    row(A, "note", piece, at, ", the researcher's question inside the comment")
                elif piece:
                    row(A, "speaker comment", piece, at, ", the consultant's comment")
            line = after(line)
        elif form_start(line):
            line = tiers(line, text)
        elif starred(line):
            tagged = re.match(r"^(.*\S)\s+(\([^()]*\))$", text)
            row(who, "transcription", tagged.group(1) if tagged else text, line, ", a form judged, no tiers")
            if tagged:
                row(A, "citation", tagged.group(2), line, ", the source at its right")
            line = after(line)
        else:
            row(A, "note", text, line, ", between the turns")
            line = after(line)
        text = text_of(line)
        if line > paper.last or gen.EXAMPLE.match(text):
            return line
        if not (opens(line) or COMMENT.match(text) or gen.CONTEXT.match(text)
                or text.startswith("[") or TURN.match(text_of(after(line)))):
            return line


def generalizations(start, where):
    """(51) or (57): each lettered part a note, and the sub-cases under it a note, to the closing
    parenthesis of the last."""
    number = gen.EXAMPLE.match(paper.text(start)).group(1)
    line, text, label, count, parts = start, gen.EXAMPLE.match(paper.text(start)).group(2), None, 0, []
    while True:
        sub = gen.SUB.match(text)
        if sub:
            label, count, text = number + sub.group(1), 0, sub.group(2)
        if sub or text.startswith("(Sub-cases:"):
            count += 1
            parts.append(["(%s) line %d" % (label, count), text, line])
        else:
            parts[-1][1] += " " + text
        line = after(line)
        if parts[-1][1].startswith("(Sub-cases:") and parts[-1][1].endswith(".)") and label.endswith("b"):
            break
        text = text_of(line)
    for here, body, at in parts:
        paper.add(here, A, "note", body, "page %d, %s" % (
            paper.page(at), "its sub-cases" if body.startswith("(Sub-cases:") else "a generalization"))
    return line


blocks = {}
for number in range(1, paper.last + 1):
    opened = gen.EXAMPLE.match(paper.text(number))
    if printed(number) and opened:
        blocks[number] = generalizations if opened.group(1) in GENERALIZATIONS else example
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=blocks)

# Two entries open on no surname and initials, First Peoples' Cultural Council and von Stechow, and
# gen runs each onto the entry above it; Zimmermann's wraps onto a line that opens on Maienborn,
# and gen takes that line for an entry of its own.
for opening in (" First Peoples' Cultural Council. (2014)", " von Stechow, A. (1996)"):
    for index, row in enumerate(paper.rows):
        if row[2] == "reference" and opening in row[3]:
            first, second = row[3].split(opening)
            paper.rows[index:index + 1] = [[row[0], row[1], row[2], first, row[4]],
                                           [row[0], row[1], row[2], opening.strip() + second, row[4]]]
            break
for index, row in enumerate(paper.rows):
    if row[2] == "reference" and row[3].startswith("Zimmermann, M. (2011)"):
        row[3] += " " + paper.rows[index + 1][3]
        del paper.rows[index + 1]
        break
paper.write()
