"""The ops of ICSNL56_Zenk_final: Henry B. Zenk on Joe Peter's Chinuk Wawa translations of John
Paul Marr's English prompts, recorded in 1941, with the transcript of one sound file appended.

The page text is the text layer closed up from the glyph rows (page_text.py closeup): the layer
sets spaces inside words, associa te, and the glyph rows break words at the stress mark's narrow
box, mamuk- ˈhihi. Each appendix set is its number and Marr's prompt, Joe Peter's Chinuk Wawa over
its gloss, and the notes; the time stamps between the sets are notes of their own. The examples of
§1 and §4 are laid out as the sets are, with a translation and the set they come from.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import italic_runs  # noqa: E402

A, L = gen.A, gen.L
paper = gen.Paper("ICSNL56_Zenk_final", authors="Henry B. Zenk", language="Chinuk Wawa")
add, text, page = paper.add, paper.text, paper.page
DEBUG = "debug" in sys.argv

HEADINGS = {paper.find(r"^%s " % re.escape(one)): one for one in ("1", "2", "3", "4", "4.1", "4.2", "4.3", "4.4", "4.5", "5")}
HEADINGS = {number: label for number, label in HEADINGS.items() if number and number < paper.find(r"^References$")}
# 4.2 and 4.3 wrap onto a second line.
WRAPPED = {paper.find(r"^4\.2 "), paper.find(r"^4\.3 ")}
REFERENCES = paper.find(r"^References$")
APPENDIX = paper.find(r"^Appendix: ")
SIDE = paper.find(r"^sinaa_694_side 1$")
FOOTNOTES = paper.footnotes(["*", "1"], start=40)
FOOTNOTE_LINES = {one for parts, _ in FOOTNOTES.values() for one in parts}
RUNNING = paper.running_numbers()

# The quotations set off from the text, from their first words to the line that ends them; each
# of Harrington's and Marr's letters closes on its citation.
QUOTES = [("I’m assuming that Harrington", "“Chinook” (Chinookan), though"),
          ("Have you received the boas", "popularly called CHINOOK."),
          ("Have finally traced Joe Peter", "Chinook will be well worth"),
          ("God has graced us at last!", "sentence. That is, what he can"),
          ("After Boas got the wonderful", "1941:mf9_r14_0229)"),
          ("I caught him [Joe Peter]", "1941:mf9_r7_0642)"),
          ("Do you in your conviction", "1941:mf9_r14_0253)")]
QUOTE_AT = {}
for first, last in QUOTES:
    start = paper.find(r"^" + re.escape(first))
    end = paper.find(re.escape(last), start)
    QUOTE_AT[start] = end
CITATION = re.compile(r"\s*(\((?:Harrington|Harrin gton) 1941:mf9_r\d+_[\d–]+\))$")

# The forms the prose cites in italics, with the gloss the prose gives them.
CITED = {"miɬayt": "‘sit, be there, remain’", "iskam": "‘take, get’",
         "yaka": "‘3SG’, the usual 3SG pronoun", "ya": "a short form of the 3SG pronoun",
         "ɬ-": "the Chinookan neuter collective element", "ɬaska": "‘3PL’", "ɬaksta": "‘who?; someone’",
         "swawa": "Joe Peter's term for ‘cougar’, as in Cowlitz", "yax̣ka": "the 3SG emphasis form",
         "kənim": "‘canoe(s)’", "munk-": "the CWDP short form of the causative auxiliary",
         "mamuk-": "the usual regional form of the causative auxiliary", "uk": "the short form of the demonstrative",
         "ukuk": "the full form of the demonstrative ‘that; that one’", "hayu-": "a marker of durative aspect",
         "hayú": "‘much, many; often’", "mamuk-wax": "‘spill.it’", "ɬush-iliʔi": "‘prairie’",
         "chaku-pulakʰli": "‘become-night’", "kʰaku-dlet": "‘like/as-true’"}
italic = {}
for where_page, run in italic_runs.italic_runs(paper.stem):
    italic.setdefault(where_page, []).append(run.strip())
cited_done = set()


def cited_in(where, body, first_page, last_page):
    for number in range(first_page, last_page + 1):
        for run in italic.get(number, ()):
            if run in CITED and run not in cited_done and re.search(r"(?<![\w-])%s(?![\w])" % re.escape(run), body):
                cited_done.add(run)
                add(where, L, "cited form", run, "page %d, in italics; %s" % (number, CITED[run]))


NAMES = [("Tony Johnson", "Tony A. Johnson, the author's co-presenter at ICSNL 37 and co-author of Zenk & Johnson 2003, 2005"),
         ("John Paul Marr", "‘Jack’ Marr, J. P. Harrington's assistant, who recorded Joe Peter in 1941 (JPM)"),
         ("John P. Harrington", "John Peabody Harrington"),
         ("Joe Peter", "1894–1959, the speaker of the recordings (JP), a Cowlitz-identified resident of Yakama Reservation"),
         ("Ives Goddard", "of the Smithsonian Institution"), ("Daisy Njoku", "of the National Anthropological Archives"),
         ("Rick McClure", "of the Taytnapam History Project"), ("Tom Larsen", "commented on an earlier draft"),
         ("David Robertson", "commented on an earlier draft; Robertson 2018"),
         ("Charles Cultee", "Boas's speaker of Chinook proper (Lower Chinook)"), ("John Clipp", "reputed to know pure Chinook"),
         ("Fred Yelkes", "a Molala-speaking resident of Portland Marr recorded"), ("Kate Charley", "named in Marr's letter of 6/3/41"),
         ("Agnes Peter", "Joe Peter's wife, perhaps a Sahaptin speaker"),
         ("Peter Wyanneshut", "Captain Peter Wyanneshut, Joe Peter's father"), ("Robert Boyd", "personal communication 2019"),
         ("Tenas McKay", "perhaps the older Wyanneshut, Joe Peter's paternal grandfather")]
LANGUAGES = [("Chinuk Wawa", "Chinook Jargon; the lower Columbia variety of Joe Peter and the Grand Ronde variety of CWDP"),
             ("Kathlamet Chinook", "Chinookan, as the National Anthropological Archives identified the recordings"),
             ("Chinook proper", "Lower Chinook, of Boas's Chinook Texts; the Baker's Bay ‘pure Chinook’"),
             ("Cowlitz", "Salishan; Kinkade's Cowlitz Dictionary"), ("Taytnapam", "Upper Cowlitz Sahaptin"),
             ("Yakama", "Sahaptin; Beavert & Hargus 2009"),
             ("Nisqually", "named in Harrington's note to set 7")]
names_done = set()


def names_in(where, body):
    for name, why in NAMES:
        if name not in names_done and name in body:
            names_done.add(name)
            add(where, A, "name", name, why)


def footnote(mark):
    parts, number_page = FOOTNOTES[mark]
    body = gen.Paper.body_of(paper.joined(parts), mark)
    where = "footnote %s" % mark
    add(where, A, "note", body, "page %d, footnote%s" % (number_page, " *, on the title" if mark == "*" else ""))
    names_in(where, body)
    cited_in(where, body, number_page, number_page)


# The front matter.
TITLE = paper.find(r"^Revisiting Joe Peter")
add("front", A, "title", text(TITLE).rstrip("*"), "page 1, carries footnote *")
add("front", A, "name", text(TITLE + 1), "author")
add("front", A, "note", text(TITLE + 2), "page 1, the author's affiliation")
KEYWORDS = paper.find(r"^Keywords:")
add("front", A, "note", paper.joined(range(TITLE + 3, KEYWORDS)), "page 1, the abstract")
add("front", A, "note", text(KEYWORDS), "page 1, the keywords")
HEADER = [one for one in range(1, TITLE) if text(one)]
add("front", A, "note", paper.joined(HEADER), "page 1, the volume's header")
for name, why in LANGUAGES:
    add("front", A, "language", name, why)
footnote("*")

starts = paper.paragraph_starts()
where, para = "§1", []
placed = {"*"}


def flush():
    global para
    if para:
        body = " ".join(one for one, _ in para)
        add(where, A, "note", body, "page %d" % para[0][1])
        names_in(where, body)
        cited_in(where, body, para[0][1], para[-1][1])
        if "tenuous.1 " in body or body.endswith("tenuous.1"):
            if "1" not in placed:
                placed.add("1")
                footnote("1")
    para = []


def quote(start):
    end = QUOTE_AT[start]
    body = paper.joined(range(start, end + 1))
    found = CITATION.search(body)
    if found:
        body = body[:found.start()]
    quotes_seen[where] = quotes_seen.get(where, 0) + 1
    label = "%s quote %d" % (where, quotes_seen[where])
    add(label, A, "note", body, "page %d, a quotation set off from the text" % page(start))
    if found:
        add(label, A, "citation", found.group(1), "page %d" % page(end))
    names_in(label, body)
    return end


quotes_seen = {}
# An example's number: (1) to (14), where a year in parentheses, (1894), opens a prose line.
EXAMPLE = re.compile(r"^\((\d{1,2})\) ")
SET_TAIL = re.compile(r"\s*(\((?:sets?|appendix set) [\d –\-]+\)\.?)$")


def example(start):
    """(N) and its lines up to the prose after it; returns the last line read."""
    head = re.match(r"^\((\d+)\) (.*)$", text(start))
    label, prompt = "(%s)" % head.group(1), head.group(2)
    count = [0]

    def row(who, kind, form, gloss):
        count[0] += 1
        add("%s line %d" % (label, count[0]), who, kind, form, gloss)

    first = page(start)
    if prompt.startswith("JPM:"):
        row(A, "note", prompt[4:].strip(), "page %d, marked JPM:, Marr's prompt" % first)
    elif prompt.startswith("(from"):
        row(A, "citation", prompt, "page %d, the example's source" % first)
    else:
        row(A, "note", prompt, "page %d, Marr's English prompt" % first)
    number = start + 1
    tier = 0
    while number <= paper.last:
        line = text(number)
        if paper.lines[number][2] or number in RUNNING or not line or number in FOOTNOTE_LINES:
            number += 1
            continue
        # The prose after an example opens on an English word with a capital; the tiers, the
        # translation in quotes and the notes in parentheses do not.
        if number in HEADINGS or EXAMPLE.match(line) or re.match(r"^[A-Z][a-z]", line):
            break
        at = page(number)
        tail = SET_TAIL.search(line)
        body = line[:tail.start()] if tail else line
        if body.startswith(("JPM:", "JP:")):
            speaker, said = body.split(":", 1)
            if speaker == "JPM":
                row(A, "note", said.strip(), "page %d, marked JPM:, Marr's prompt" % at)
            else:
                row(L, "transcription", said.strip(), "page %d, marked JP:, Joe Peter" % at)
                tier = 1
        elif body.startswith("‘"):
            row(A, "translation", body, "page %d" % at)
        elif body.startswith("("):
            row(A, "note", body, "page %d, a note to the example" % at)
        elif body:
            if tier == 0:
                row(L, "segmentation", body, "page %d, each word over its gloss" % at)
                tier = 1
            else:
                row(A, "gloss", body, "page %d" % at)
                tier = 0
        if tail:
            row(A, "citation", tail.group(1), "page %d, the appendix set it comes from" % at)
        number += 1
    return number - 1


number = HEADINGS and min(HEADINGS) or 1
while number < REFERENCES:
    line = text(number)
    if paper.lines[number][2] or number in RUNNING or not line or number in FOOTNOTE_LINES:
        number += 1
        continue
    if number in HEADINGS:
        flush()
        label = HEADINGS[number]
        heading = line
        if number in WRAPPED:
            heading += " " + text(number + 1)
            number += 1
        where = "§" + label
        add(where, A, "heading", heading, "page %d" % page(number))
        number += 1
        continue
    if number in QUOTE_AT:
        flush()
        number = quote(number) + 1
        continue
    if EXAMPLE.match(line):
        flush()
        number = example(number) + 1
        continue
    if para and number in starts:
        flush()
    para.append((line, page(number)))
    number += 1
flush()
for mark in FOOTNOTES:
    if mark not in placed:
        print("# footnote not placed:", mark, file=sys.stderr)

add("references", A, "heading", "References", "page %d" % page(REFERENCES))
for entry, at in paper.references(REFERENCES + 1, APPENDIX - 1):
    add("references", A, "reference", entry, "page %d" % at)

# The appendix: its heading, the template, the preface, the alphabet and the abbreviations.
add("appendix", A, "heading", text(APPENDIX), "page %d" % page(APPENDIX))
TEMPLATE = paper.find(r"^Template:$", APPENDIX)
add("appendix template", A, "note", text(TEMPLATE), "page %d" % page(TEMPLATE))
for one in range(TEMPLATE + 1, TEMPLATE + 5):
    add("appendix template", A, "note", text(one), "page %d, a line of the template for each set" % page(one))
PREFACE = paper.find(r"^JPM’s prompts are reproduced", APPENDIX)
STAR = paper.find(r"^\*CWDP: ", APPENDIX)
add("appendix", A, "note", paper.joined(range(PREFACE, STAR)), "page %d" % page(PREFACE))
TABLE = paper.find(r"^CWDP IPA$", APPENDIX)
add("appendix", A, "note", gen.Paper.body_of(paper.joined(range(STAR, TABLE)), "*"),
    "page %d, the note on CWDP marked * in the template" % page(STAR))
count = 0
for one in range(TABLE, SIDE):
    line = text(one)
    if not line or paper.lines[one][2] or one in RUNNING:
        continue
    count += 1
    if line.endswith(":"):
        add("appendix symbols line %d" % count, A, "note", line, "page %d, a heading of the list" % page(one))
    elif one == TABLE:
        add("appendix symbols line %d" % count, A, "note", line, "page %d, the column heads" % page(one))
    else:
        add("appendix symbols line %d" % count, A, "note", line, "page %d, a symbol and what it stands for, a printed row" % page(one))

add("appendix", A, "heading", text(SIDE), "page %d, the sound file" % page(SIDE))
TIME = re.compile(r"^(\d:\d\d:\d\d\.\d)(?: (.*))?$")
expect, current, tier = 1, "appendix", 0
number = SIDE + 1
while number <= paper.last:
    line = text(number)
    if not line or paper.lines[number][2] or number in RUNNING:
        number += 1
        continue
    at = page(number)
    head = re.match(r"^(\d{1,3}) (.*)$", line)
    if head and int(head.group(1)) == expect:
        current = "set %d" % expect
        prompt = head.group(2)
        if expect == 122:
            prompt += " " + text(number + 1)
            number += 1
        add(current, A, "note", prompt, "page %d, the set's number and Marr's English prompt (JPM)" % at)
        expect += 1
        tier = 0
        number += 1
        continue
    stamp = TIME.match(line)
    if stamp:
        add(current, A, "note", line, "page %d, the time into the sound file" % at)
        number += 1
        continue
    if line.startswith("(") or line == "[gap]":
        note, depth = line, line.count("(") - line.count(")")
        while depth > 0 and number < paper.last:
            number += 1
            if text(number):
                note += " " + text(number)
                depth += text(number).count("(") - text(number).count(")")
        add(current, A, "note", note, "page %d, %s" % (at, "a gap in the recording" if line == "[gap]" else "a note to the set"))
        number += 1
        continue
    # The transcriber's remark after set 163, note: JP's normally rapid delivery ..., runs to the
    # time stamp under it.
    if line.startswith("note:"):
        note = line
        while number + 1 <= paper.last and text(number + 1) and not TIME.match(text(number + 1)):
            number += 1
            note += " " + text(number)
        add(current, A, "note", note, "page %d, the transcriber's note to the set" % at)
        number += 1
        continue
    if tier == 0:
        add(current, L, "segmentation", line, "page %d, Joe Peter's Chinuk Wawa, each word over its gloss (JP)" % at)
        tier = 1
    else:
        add(current, A, "gloss", line, "page %d" % at)
        tier = 0
    number += 1
if expect != 170:
    print("# sets read:", expect - 1, file=sys.stderr)

if DEBUG:
    for row in paper.rows:
        print(" | ".join(row)[:200])
paper.write()
