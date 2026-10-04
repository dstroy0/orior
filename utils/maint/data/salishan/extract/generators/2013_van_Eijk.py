"""The ops of 2013_van_Eijk: Jan P. van Eijk's Non-concatenative morphology and interlinear
translations: a Lillooet example. The paper sets out how an interlinear gloss can show ablaut,
infixation and reduplication, with angle brackets for diminutive reduplication, swing brackets for
the inchoative infix and square brackets for an IC structure, and applies it to Bill Edwards's The
two coyotes, (8) to (19): each sentence the Lillooet, its gloss a morpheme a word, and its
translation.

Page text read by glyph rows; page_text's PRIVATE_USE reads the AboriginalSans codes. The forms are
set upright in AboriginalSans against the AboriginalSerif of the prose, and italic_runs reads that
face as their italics.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Lillooet"
AUTHORS = ["Jan P. van Eijk"]
paper = gen.Paper("2013_van_Eijk", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Bill Edwards", "the storyteller of The two coyotes, recorded in 1973"),
         ("Van Eijk", "Jan P. van Eijk, the author, cited for his grammar of Lillooet (1997), CVC "
                      "reduplication (1993), locus and ordering (2004) and The Two Coyotes (2008)"),
         ("Charles Hill-Tout", "published texts in about a dozen Salish languages around 1900"),
         ("Hockett", "Charles F. Hockett, two models of grammatical description (1954)"),
         ("Matthewson", "Lisa Matthewson, St’át’imc oral narratives (2005) and texts (2008)"),
         ("Kuipers", "Aert H. Kuipers, Squamish (1967, 1969), Shuswap (1974, 1989) and the Salish "
                     "etymological dictionary (2002)"),
         ("Pustet", "Regina Pustet, split intransitivity in Lakota and Osage (2002)"),
         ("Lindley and Lyon", "Lottie Lindley and John Lyon, Upper Nicola Okanagan texts (2012)"),
         ("Mattina and DeSautel", "Anthony Mattina and Dora Noyes DeSautel, Okanagan texts (2002)")]
LANGUAGES = [(LANGUAGE, "Northern Interior Salish, spoken in British Columbia"),
             ("St’át’imcets", "the language's own name, given with Lillooet"),
             ("Bella Coola", "Nuxalk, one of the languages with bilingual texts"),
             ("Squamish", "Sḵwx̱wu7mesh, one of the languages with bilingual texts"),
             ("Lushootseed", "one of the languages with bilingual texts"),
             ("Shuswap", "Secwepemctsín, one of the languages with bilingual texts"),
             ("Okanagan", "one of the languages with bilingual texts"),
             ("Kalispel", "one of the languages with bilingual texts"),
             ("Dutch", "the language of (1)"),
             ("Thompson", "Nlaka’pamux, the language of (2)"),
             ("English", "the language of (3) and (4) and of the translations"),
             ("Lakota", "its infixed person markers, from Pustet 2002")]
OTHER = {"na-ma-ya-x’ų": "Lakota", "ma-": "Lakota", "ya-": "Lakota", "nax’ų": "Lakota",
         "*k̓i/amǝl": "Proto-Salish", "k̓i/amǝl": "Proto-Salish", "/k[Ɂ]éw": "Thompson",
         "/q̓á[•q̓]y̓-m̓": "Thompson", "/q̓ay̓": "Thompson", "arbeid-er-s": "Dutch"}
# The language of each example that is not Lillooet's, and each example's tiers as the page sets
# them: (2) a surface form over its underlying form, the rest a form over its gloss. The text of (8)
# to (19) wraps each sentence onto more lines, a Lillooet line over its gloss each time.
EXAMPLE_LANGUAGES = {"1": "Dutch", "2": "Thompson", "3": "English", "4": "English"}
# The sounds and marks the prose discusses, set in the face of the forms: no form of Lillooet.
SOUNDS = {"á", "ǝ́", "Ɂ", "ǝ", "ˬ", "+aɬ+"}
paper.form_language = lambda run: None if run in SOUNDS else OTHER.get(run, L)
# The square and swing brackets of a form stand in the serif face of the prose, and the form's runs
# break at them: ká{Ɂǝ}w̓ reads as ká, Ɂǝ and w̓. A word of the page made of runs and brackets is
# one run.
italics = paper.italics()
by_page = {}
for number in range(1, paper.last + 1):
    by_page.setdefault(paper.page(number), []).append(paper.text(number) or "")
for page, runs in italics.items():
    for token in " ".join(by_page.get(page, [])).split():
        token = token.strip(",.;:‘’“”()")
        if not re.search(r"[\[\]{}]", token):
            continue
        # A run can hold a bracket of its own, lǝp̓-xál]-tǝn of n.[lǝp̓-xál]-tǝn: each stretch of the
        # word between two bracket edges, a run's trailing stop or colon aside, is tried.
        edges = sorted({0, len(token)} | {one.start() for one in re.finditer(r"[\[\]{}]", token)} |
                       {one.end() for one in re.finditer(r"[\[\]{}]", token)})
        stretches = {token[low:high] for low in edges for high in edges if low < high}
        held = [one for one in runs if one in stretches or one + "." in stretches or one + ":" in stretches]
        if len(held) < 2:
            continue
        at = runs.index(held[0])
        for one in held:
            runs.remove(one)
        runs.insert(at, token)
paper.standard(AUTHORS, NAMES, LANGUAGES, front_languages=1, appendix=r"^jvaneijk@")
# The references open on an author's surname or, for the next work of the same author, on U+2014. A line
# wrapped at Aert H. Kuipers's name in Dixon and Palmantier's entry opens none.
paper.rows = [row for row in paper.rows if row[0] != "references"]
end = paper.find(r"^References$")
tail = paper.find(r"^jvaneijk@")
paper.add("references", A, "heading", paper.text(end), "page %d" % paper.page(end))
opens = r"^(?:Bierwert|Davis|Dixon|Elliott|Hess|Hockett|Kuipers|Lindley|Matthewson|Mattina|Maud|Pustet|" \
        r"Thompson|Van Eijk|Vogt), |^—\."
for text, at in paper.references(end + 1, tail - 1, opens=opens):
    paper.add("references", A, "reference", text, "page %d" % at)
paper.add("end", A, "note", paper.text(tail), "page %d, the author's e-mail, under the references" % paper.page(tail))
examples = {}
for row in paper.rows:
    found = re.match(r"^\((\d+)\) line (\d+)$", row[0])
    if found:
        examples.setdefault(found.group(1), []).append(row)
for number, rows in examples.items():
    for index, row in enumerate(rows):
        if index == len(rows) - 1:
            row[2] = "translation"
        elif number == "2":
            row[2] = ("transcription", "segmentation", "gloss")[index]
        else:
            row[2] = ("transcription", "gloss")[index % 2]
        if row[2] != "translation" and number in EXAMPLE_LANGUAGES:
            row[1] = EXAMPLE_LANGUAGES[number]
# The gloss of lǝp̓ on page 11 opens a quote the paper never closes; the gloss stops at the comma.
for row in paper.rows:
    if row[2] == "cited form" and row[3] == "lǝp̓" and "‘to plant," in row[4]:
        row[4] = row[4][:row[4].index("‘to plant,") + len("‘to plant,")] + " a quote the paper never closes"
paper.write()
