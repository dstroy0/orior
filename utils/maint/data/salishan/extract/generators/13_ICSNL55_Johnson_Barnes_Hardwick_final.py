"""The ops of 13_ICSNL55_Johnson_Barnes_Hardwick_final: Sʔímlaʔxʷ Michele Johnson, K̓ninm̓tm̓ taʔ
n̓q̓ʷic̓tn̓ Grouse Barnes and Q̓ʷłm̓tal̓qs Christina Hardwick, a N̓syilxčn̓ literary contribution from the
Syilx Language House and the Salish School of Spokane: sisp̓lk̓ iʔ sʕax̌ʷíptət, a prayer for seven
generations told by K̓ninm̓tm̓ taʔ n̓q̓ʷic̓tn̓ Grouse Barnes in 2016.

Two passages in N̓syilxčn̓ carry their English after a dash, Sʔímlaʔxʷ's introduction and pútiʔ kʷu
sʔalá, We are still here: a transcription row to each sentence and a translation row to the English.
The story (§2.1) is set in the orthography with no translation: its heading and the five lines of
its metadata a note each, then a transcription row to each sentence, the minutes and seconds of the
recording the paper sets between them a note. The vocabulary (§2.2) is a cited form and its
translation to each line. The paper closes on ixíʔ and a photograph captioned with the teller's name.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A = gen.A
LANGUAGE = "N̓syilxčn̓"
AUTHORS = ["Sʔímlaʔxʷ Michele Johnson", "K̓ninm̓tm̓ taʔ n̓q̓ʷic̓tn̓ Grouse Barnes", "Q̓ʷłm̓tal̓qs Christina Hardwick"]
paper = gen.Paper("13_ICSNL55_Johnson_Barnes_Hardwick_final", authors=", ".join(AUTHORS), language=LANGUAGE)

NAMES = [("K̓ninm̓tm̓ taʔ n̓q̓ʷic̓tn̓", "Grouse Barnes, the teller of the story"),
         ("Grouse Barnes", "the teller, a N̓syilxčn̓ speaker of Westbank First Nation"),
         ("Sʔímlaʔxʷ", "Michele Johnson, the first author"), ("Michele Johnson", "the first author, Syilx Language House"),
         ("Q̓ʷłm̓tal̓qs", "Christina Hardwick, the transcriber"),
         ("Christina Hardwick", "Salish School of Spokane, the transcriber"),
         ("Xatma Sqilxʷ Flynn Wetton", "recorded the story"), ("Flynn Wetton", "recorded the story"),
         ("Lucy Simla", "great-great-grandmother of the first author"),
         ("Francis Xavier Richter", "great-great-grandfather of the first author"),
         ("Emily Michelle", "a parent of Grouse Barnes"), ("Dennis Barnes", "a parent of Grouse Barnes"),
         ("Wilfred Barnes", "the teller's English name, in the story"),
         ("Q̓iyusálxqn̓", "Herman Edward, a N̓syilxčn̓ speaker"), ("St̓aʔqʷálqs", "Grouse Barnes's daughter"),
         ("Johnson", "Sʔímlaʔxʷ Michele K. Johnson, the curriculum and the Elders' Stories"),
         ("Peterson", "Sʕam̓tíc̓aʔ Sarah Peterson, LaRae Wiley and Christopher Parkin, the textbooks (2020)")]
LANGUAGES = [(LANGUAGE, "Okanagan-Colville, Southern Interior Salish"), ("N̓səl̓xčin̓", "N̓syilxčn̓"),
             ("n̓qilxʷčn̓", "N̓syilxčn̓"), ("Nsyilxcn", "N̓syilxčn̓"), ("Okanagan-Colville", "N̓syilxčn̓"),
             ("Colville-Okanagan", "N̓syilxčn̓"), ("Okanagan", "N̓syilxčn̓"), ("Interior Salish", "the division"),
             ("Salish", "the family, and a name of N̓syilxčn̓"), ("Tlingit", "Johnson 2017a"),
             ("English", "the translations")]
FOOTNOTES = paper.page_footnotes()
AT_FOOT = {one for parts, _ in FOOTNOTES.values() for one in parts}
RUNNING = paper.running_numbers_set()


def printed(start, end):
    """The lines from start up to end that hold the text, the page breaks, running numbers and
    footnotes left out."""
    return [one for one in range(start, end) if paper.text(one).strip() and not paper.lines[one][2]
            and one not in RUNNING and one not in AT_FOOT]


def dashed(start, where):
    """A passage in N̓syilxčn̓ with its English after a dash, to the blank line under it."""
    end = start
    while paper.text(end).strip():
        end += 1
    body = re.sub(r"\s+", " ", paper.joined(printed(start, end)))
    said, english = body.split(" — ")
    for index, sentence in enumerate(gen.sentences(said), 1):
        paper.add("%s sentence %d" % (where, index), LANGUAGE, "transcription", sentence,
                  "page %d, in N̓syilxčn̓ with its English after the dash" % paper.page(start))
    paper.add("%s translation" % where, A, "translation", english, "page %d, the English" % paper.page(start))
    return end


TIME = re.compile(r"\((?:\d+ mins?\. \d+ sec\.|\d+ \d+)\)")


def story(start, where):
    """§2.1: the heading, the metadata and the story, to the vocabulary's heading."""
    here = "§2.1"
    paper.add(here, A, "heading", re.sub(r"\s+", " ", paper.text(start)), "page %d" % paper.page(start))
    number = start + 1
    while paper.text(number).strip():
        paper.add(here, A, "note", re.sub(r"\s+", " ", paper.text(number)),
                  "page %d, the story's metadata" % paper.page(number))
        number += 1
    end = paper.find(r"^2\.2 Vocabulary$", number)
    body, pages = "", []
    for line in printed(number, end):
        pages.append((len(body), paper.page(line)))
        body += re.sub(r"\s+", " ", paper.text(line)).strip() + " "
    counts = {"sentence": 0, "time": 0}

    def page_at(offset):
        return max(page for at, page in pages if at <= offset)

    at = 0
    for piece in re.split(r"(%s)" % TIME.pattern, body):
        if TIME.fullmatch(piece):
            counts["time"] += 1
            paper.add("%s time %d" % (here, counts["time"]), A, "note", piece,
                      "page %d, the minutes and seconds of the recording" % page_at(at))
        else:
            for sentence in gen.sentences(piece):
                counts["sentence"] += 1
                paper.add("%s sentence %d" % (here, counts["sentence"]), LANGUAGE, "transcription", sentence,
                          "page %d, the story" % page_at(at + piece.find(sentence[:10])))
        at += len(piece)
    return end


def vocabulary(start, where):
    """§2.2: a form and its gloss in quotes to each line, to the notes' heading."""
    end = paper.find(r"^2\.3 Notes$", start)
    for index, line in enumerate(printed(start, end), 1):
        text = re.sub(r"\s+", " ", paper.text(line)).strip()
        form, gloss = re.match(r"^(.+?) (‘.*)$", text).groups()
        paper.add("§2.2 line %d" % index, LANGUAGE, "cited form", form, "page %d, the vocabulary" % paper.page(line))
        paper.add("§2.2 line %d" % index, A, "translation", gloss, "page %d, the vocabulary" % paper.page(line))
    return end


def closing(start, where):
    """The closing word, ixíʔ, and the caption of the photograph under it."""
    paper.add("§3 close", LANGUAGE, "transcription", paper.text(start).strip(), "page %d, the closing word" % paper.page(start))
    caption = printed(start + 1, paper.find(r"^References$", start))[0]
    paper.add("§3 photograph", A, "note", re.sub(r"\s+", " ", paper.text(caption)).strip(),
              "page %d, the photograph's caption" % paper.page(caption))
    return caption + 1


blocks = {paper.find(r"^way̓\s+iskʷíst Sʔímlaʔxʷ\."): lambda start, where: dashed(start, "§1 Sʔímlaʔxʷ"),
          paper.find(r"^pútiʔ kʷu sʔalá\."): lambda start, where: dashed(start, "§1 pútiʔ kʷu sʔalá"),
          paper.find(r"^2\.1 sisp̓lk̓"): story,
          paper.find(r"^2\.2 Vocabulary$") + 1: vocabulary,
          paper.find(r"^ixíʔ\.$"): closing}
# The authors' introductions stand in a column beside their photographs, a paragraph set with no
# indent after a blank line: Q̓ʷłm̓tal̓qs opens her own under Grouse Barnes's.
paper.paragraph_starts().add(paper.find(r"^way̓\s+p yʕaʕát, My name is Christina Hardwick"))
# 2.1 titles the story in N̓syilxčn̓, in lower case, and the story's block writes its heading; 2.2
# and 2.3 follow it.
REFERENCES = paper.find(r"^References$")
headings = paper.headings(1, REFERENCES - 1, skip=AT_FOOT)
headings.update({paper.find(r"^2\.2 Vocabulary$"): "2.2", paper.find(r"^2\.3 Notes$"): "2.3"})
# Page 3 names the school's course books in italics with their volume numbers, Čaptíkʷł 1,
# N̓səl̓xčin 2 and Čaptíkʷł 2: titles, and no cited forms.
paper.form_language = lambda run: None if re.search(r" \d$", run) else gen.L if gen.orthographic(run) else None
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, blocks=blocks, headings=headings)
paper.write()
