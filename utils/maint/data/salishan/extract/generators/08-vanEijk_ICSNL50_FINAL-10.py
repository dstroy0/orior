"""The ops of 08-vanEijk_ICSNL50_FINAL-10: Jan P. van Eijk on the irrealis in Lillooet
(St’át’imcets), the term's critics and its use, the three ways the Lillooet subjunctive is used and
the enclitics ˬkɬ, ˬka and ˬk̓a, and the link of irrealis to tense and aspect.

The paper is prose with its forms cited in italics; page_text.py reads it by glyph rows, with the
words the page sets as images put back from PAPER_IMAGES. Trask's definition on page 1 and
Palmer's words on page 2 are set as block quotations, a note each.
"""
import os
import re
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
STEM = "08-vanEijk_ICSNL50_FINAL-10"
LANGUAGE = "Lillooet"
paper = gen.Paper(STEM, authors="Jan P. van Eijk", language=LANGUAGE)

NAMES = [("Christina Mickleborough", "thanked on the title for priming the author's interest in the irrealis"),
         ("Trask", "R. L. Trask, the definition of irrealis (1993:147) quoted on page 1"),
         ("Palmer", "F. R. Palmer, Mood and modality (1968; 2001), quoted on page 2"),
         ("Bybee", "J. L. Bybee (1998:267), on irrealis as too abstract"),
         ("Martin", "L. Martin (1998:198), on the irrealis in Mocho"),
         ("Vidal", "E. Vidal, with Manelis Klein (1998), on Pilagá and Toba"),
         ("Manelis Klein", "H. E. Manelis Klein, with Vidal (1998), on Pilagá and Toba"),
         ("Kinkade", "M. D. Kinkade (1998, 2001), on irrealis in Upper Chehalis and Proto-Salish"),
         ("Van Eijk", "the author's own work, Van Eijk and Hess (1986), Van Eijk (1997, 2013)"),
         ("Hess", "T. Hess, with Van Eijk (1986), on noun and verb in Salish"),
         ("Matthewson", "L. Matthewson (2010), on the St’át’imcets subjunctive"),
         ("Jensen", "J. T. Jensen (1990), the source of the Sanskrit and Georgian examples"),
         ("Bill Edwards", "his story ‘The man who stayed with the bear’ gives c̓aqʷan̓ásˬƛ̓uɁ"),
         ("Leech", "G. N. Leech (1971:110), on real and unreal conditions"),
         ("Steele", "S. Steele (1975), on past and irrealis in Uto-Aztecan"),
         ("Rullmann", "H. Rullmann, with Matthewson and Davis (2005, 2006)"),
         ("Davis", "H. Davis, with Matthewson (2003) on ˬtuɁ and with Matthewson and Rullmann (2005, 2006)"),
         ("Seiler", "H. Seiler (1971), on the dissociative in Greek"),
         ("Hofling", "C. A. Hofling (1993, 1998), on Itzaj Maya"),
         ("Whorf", "B. L. Whorf (1956:63), on distance in Hopi"),
         ("Glougie", "J. Glougie (2007), on xʷuz̓ and ˬkɬ"),
         ("Baier", "N. Baier (2010), on irrealis in Montana Salish")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salishan, St’át’imcets, the paper's subject"),
             ("Russian", "sets the object of a negative construction in the genitive"),
             ("German", "sets the object of a negative construction in the genitive only in archaic expressions"),
             ("Upper Chehalis", "marks grammatical unreality with the particle q’aɬ, after Kinkade (1998)"),
             ("Latin", "a formally distinct subjunctive paradigm"),
             ("Sanskrit", "past and future markers combined in one form, after Jensen (1990)"),
             ("Georgian", "past and future markers combined in one form, after Jensen (1990)"),
             ("Italian", "andiamo, indicative and subjunctive alike"),
             ("Mocho", "a Mayan language, Martin (1998) on its irrealis"),
             ("Uto-Aztecan", "past and irrealis linked, after Steele (1975)"),
             ("Greek", "the optative and the preterit, after Seiler (1971)"),
             ("Pilagá", "the particle ga’, after Vidal and Manelis Klein (1998)"),
             ("Toba", "the particle ka, after Vidal and Manelis Klein (1998)"),
             ("Itzaj Maya", "past-perfect and future-irrealis marked alike, after Hofling (1998)"),
             ("Hopi", "distance in location as distance in time, after Whorf (1956)"),
             ("Dutch", "vannacht, ‘last night’ and ‘tonight’"),
             ("Montana Salish", "irrealis morphology, after Baier (2010)")]

# The italic reader loses the words the page sets as images and breaks the rest at a raised w and at
# a clitic's underloop, x wɁạz kw for xʷɁạz kʷˬs.ƛ̓íq-i on page 5; it also takes the italic titles of
# the references and ad hoc on page 1, which are no forms. Each page's italic forms here are read
# off the render, a run of words the page sets in italics as one, with the stops around it upright.
ITALICS = {
    2: ["ja ne znaju etogu čeloveka"],
    3: ["Ich kenne des Menschen nicht", "q’aɬ", "moneam", "moneō", "a-tar-isy-at", "tar", "a-dhar-isy-at",
        "dhar", "a-…-at", "-isy", "da-v-c’er-di", "v-", "c’er", "da-", "-di"],
    4: ["andiamo", "Andiam! Andiam!", "c̓áqʷan̓as", "c̓aqʷan̓ásˬƛ̓uɁ", "ˬƛ̓uɁ"],
    5: ["s.Ɂə́ncˬa", "qʷúsxit", "sɁə́ncˬa", "s.Ɂənc-ás kʷuˬnás", "nas", "-as", "s.Ɂənc", "xʷɁạz kʷˬs.ƛ̓íq-i",
        "xʷɁạz", "ƛ̓iq", "s-", "-i", "xʷɁạ́z-as kʷˬs.q̓ʷə́ɬp-su", "-su", "wáɁˬƛ̓uɁ Ɂíƛ̓əm", "Ɂíƛ̓əm",
        "wáɁ-asˬƛ̓uɁ Ɂíƛ̓əm", "ˬan̓", "tayt-áxʷ-an", "-axʷ", "tayt", "tayt-káxʷˬha", "-kaxʷ",
        "plán-atˬan̓ waɁ pəl̓p", "-at", "plán-ɬkaɬ waɁ pəl̓p", "-kaɬ", "wáɁ-asˬan̓ k̓ʷzúsəm", "k̓ʷzúsəm",
        "wáɁ k̓ʷzúsəm", "ɬˬ", "Ɂiˬ",
        "ɬˬɁiɁwaɁ-mín-c-axʷ, ɬˬs.zaytən-mín-axʷ [ɬˬ]s.tám̓-as kʷˬs.cún-ci-n, húy̓-ɬkan cunám̓ən-ci-n kʷaˬpíx̌əm̓",
        "ɁíɁwaɁ", "-min", "-c", "s.záytən-min", "cun", "cunám̓ən", "píx̌əm̓", "Ɂiˬxín̓-as", "xin̓",
        "Ɂiˬsítst-as", "Ɂiˬcíxʷ-wit-as, s.x̌áw̓ˬtiɁˬƛ̓uɁ", "s.x̌aw̓", "cixʷ", "ˬkɬ", "ˬkə́ɬ",
        "Ɂac̓x̌ən-cí-ɬkanˬkɬ mútaɁ", "-ɬkan", "Ɂác̓x̌ən", "-ci", "mútaɁ", "ƛ̓ạḷạn-c-ásˬkɬ tiˬs.qax̌aɁ-lápˬa",
        "s.qáx̌aɁ", "-lap", "ƛ̓ạ́ḷạn", "ˬtuɁ", "c̓ə́kˬtuɁ"],
    6: ["ˬkɬˬtuɁ", "qlil-min̓-cih-asˬkə́ɬˬtuɁ", "qlil", "-min̓", "ˬka", "cukʷun̓-ɬkánˬkaˬtiɁ", "cúkʷun̓", "tiɁ",
        "x̌zúmˬkaˬhəm̓ kʷuˬkəm̓xʷyəqs-káɬ", "x̌zum", "kə́m̓xʷyəqs", "ˬhəm̓", "ˬk̓a", "sámaɁˬk̓a kʷuˬs.qʷal̓ən-táli",
        "sámaɁ", "s.qʷál̓ən", "xʷɁạ́zˬk̓a kʷasˬxʷɁít kʷuˬwaɁˬs.təm̓tə́təm̓-s", "xʷɁit", "s.təm̓tə́təm̓", "-s",
        "wáɁˬk̓a k̓ʷzúsəm", "Ɂinwat-wít-asˬkɬ", "-wit-as", "Ɂínwat", "Ɂinwat-wítˬkɬ", "-wit",
        "plan-atˬkáˬtuɁ waɁ cixʷ", "plan", "plan-ɬkaɬˬkáˬtuɁ", "-ɬkaɬ", "kanm-ánˬk̓a", "-an", "kánəm",
        "kanəm-ɬkánˬk̓a", "ka-…ˬa", "ˬkʷuɁ"],
    7: ["ga’", "ka"],
    8: ["vannacht", "natxʷ", "Ɂiˬnátxʷ-as", "xʷuz̓"]}
FORM_LANGUAGE = {"ja ne znaju etogu čeloveka": "Russian", "Ich kenne des Menschen nicht": "German",
                 "q’aɬ": "Upper Chehalis", "moneam": "Latin", "moneō": "Latin",
                 "a-tar-isy-at": "Sanskrit", "tar": "Sanskrit", "a-dhar-isy-at": "Sanskrit", "dhar": "Sanskrit",
                 "a-…-at": "Sanskrit", "-isy": "Sanskrit", "da-v-c’er-di": "Georgian", "v-": "Georgian",
                 "c’er": "Georgian", "da-": "Georgian", "-di": "Georgian", "andiamo": "Italian",
                 "Andiam! Andiam!": "Italian", "ga’": "Pilagá", "ka": "Toba", "vannacht": "Dutch"}
paper._italics = {page: [unicodedata.normalize("NFC", run) for run in runs] for page, runs in ITALICS.items()}
paper.form_language = lambda run: FORM_LANGUAGE.get(run, L)


def quotation(start, where):
    """A block quotation, one note, to the paragraph that opens it."""
    end = {paper.find(r"^A label often applied"): paper.find(r"^\*As before"),
           paper.find(r"^Although they are transparent"): paper.find(r"^Palmer’s misgivings")}[start]
    body = paper.joined(range(start, end))
    source = "Trask (1993:147)" if "label" in body else "Palmer (2001:148)"
    paper.add(where, A, "note", body, "page %d, a block quotation of %s" % (paper.page(start), source))
    paper.mentions(where, body, NAMES, "name")
    paper.mentions(where, body, LANGUAGES, "language")
    return end


# Page 1 sets Trask's definition small over the note on the title, and the notes read from the type
# size take it for a note with no mark; it is the block quotation of §2.
page_footnotes = paper.page_footnotes
paper.page_footnotes = lambda *args, **kwargs: {mark: parts for mark, parts in page_footnotes(*args, **kwargs).items()
                                                if mark != gen.UNMARKED}
paper.standard(["Jan P. van Eijk"], NAMES, LANGUAGES,
               blocks={paper.find(r"^A label often applied"): quotation,
                       paper.find(r"^Although they are transparent"): quotation})

# A form takes the gloss that follows it where it stands on its own. The first place the paper
# sets a word can be inside a longer form, Ɂíƛ̓əm in wáɁˬƛ̓uɁ Ɂíƛ̓əm ‘he is singing’ and the -s of
# s.təm̓tə́təm̓-s, and the gloss there is the longer form's. A gloss runs on past an apostrophe
# inside a word, ‘it’s all gone, finished’.
EDGE = "\\w\u0300-\u036f’ˬ.\\-"
# A form's paragraph is the last note of its section, a footnote placed after it passed over.
notes = {}
for row in paper.rows:
    if row[2] == "note":
        notes[row[0]] = row[3]
    if row[2] != "cited form":
        continue
    note = notes.get(row[0], "")
    forms = [one[3] for one in paper.rows if one[2] == "cited form" and len(one[3]) > len(row[3])]
    covered = [(found.start(), found.end()) for form in forms for found in re.finditer(re.escape(form), note)]
    places = [found for found in re.finditer(r"(?<![%s])%s(?![%s])" % (EDGE, re.escape(row[3]), EDGE.replace(".", "")),
                                             note)]
    alone = [found for found in places if not any(start <= found.start() and found.end() <= end
                                                   for start, end in covered)]
    if alone or places:
        gloss = re.match(r"\s*(‘(?:[^’]|’(?=\w))+’)", note[(alone or places)[0].end():])
        row[4] = re.sub(r", ‘.*$", "", row[4]) + (", " + gloss.group(1) if gloss else "")

# Van Eijk, J. P. opens an entry on page 10 with no comma after its first word, and the entries
# from Trask's run on into one.
entry = next(row for row in paper.rows if row[2] == "reference" and row[3].startswith("Trask, R. L."))
first, *rest = re.split(r" (?=Van Eijk[,.] J\. P\.)", entry[3])
entry[3] = first
at = paper.rows.index(entry)
for offset, text in enumerate(rest):
    paper.rows.insert(at + 1 + offset, ["references", A, "reference", text, entry[4]])

paper.write()
