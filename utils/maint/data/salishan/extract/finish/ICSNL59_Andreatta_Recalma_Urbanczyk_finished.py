# Context for Mathew Andreatta, Jesse Recalma and Suzanne Urbanczyk, Boas and the Babblefish Part 2:
# Making Sense of pentl'ach ¢ and ç. The paper has no numbered examples; its forms stand in nine
# tables, Boas's definitions in pentl'ach, ʔayʔaǰusəm and shashishalhem and the consonant inventories
# of the neighbouring languages. The text layer ran each table into the paragraph after it, and the
# engine read Tables 2 to 4 as examples. Each table is read off the page again one cell a row, the
# columns of Tables 6, 8 and 9 placed by glyph position.
import os
import re

from workdir import WORK

STEM = "ICSNL59_Andreatta_Recalma_Urbanczyk_finished"
AUTHORS = "Mathew Andreatta, Jesse Recalma and Suzanne Urbanczyk"
PENTLACH = "pentl’ach"
ISLAND = "ʔayʔaǰusəm"
MAINLAND = "ʔayʔaǰuθəm"
SECHELT = "shashishalhem"

TITLE = "Boas and the Babblefish Part 2: Making Sense of pentl’ach ¢ and ç"
BYLINE = "Mathew Andreatta and Jesse Recalma, Qualicum First Nation, and Suzanne Urbanczyk, University of Victoria"
VOLUME = "59"

with open(os.path.join(WORK, STEM + ".draft.tsv"), encoding="utf-8") as handle:
    DRAFT = [one.rstrip("\n").split("\t") for one in handle][1:]


def chained(anchor, rows):
    """Each row after the one before it, the first after anchor."""
    out = []
    for row in rows:
        out.append((anchor, row))
        anchor = (row[0], row[3])
    return out


LANGUAGES = {PENTLACH: PENTLACH, ISLAND: ISLAND, MAINLAND: MAINLAND, SECHELT: SECHELT, "shahishalhem": SECHELT}


def comparison(number, page, caption, source, meanings, rows):
    """A table of Boas's definitions: the caption, a meaning a column, and a row a language, its head
    and a form a cell. A cell of None is empty on the page."""
    where = "Table %d" % number
    out = [(where, AUTHORS, "note", caption, "page %d, the caption" % page)]
    out += [(where, AUTHORS, "note", meaning, "page %d, the head of a column" % page) for meaning in meanings]
    for head, *cells in rows:
        language = LANGUAGES.get(head)
        spelled = ", the page spelling shashishalhem so here" if head == "shahishalhem" else ""
        out.append(("%s %s" % (where, language or head), AUTHORS, "language", head, "page %d, the head of a row%s" % (page, spelled)))
        for meaning, cell in zip(meanings, cells):
            if cell is None:
                continue
            form, note = cell if isinstance(cell, tuple) else (cell, "")
            out.append(("%s %s" % (where, language), language, "cited form", form,
                        "page %d, %s, %s%s" % (page, meaning, source, note)))
    return out


TABLE_1 = comparison(1, 4, "Table 1: Forms from “Comparative Salishan vocabularies”",
                     "Boas's spelling, Comparative Salishan vocabularies, Pt. 1",
                     ("‘grandchild’", "‘eyebrow’", "‘mouth’", "‘beard’"), (
                         (PENTLACH, "ē’maç", "çō’man", "çō’çin", "qō’poçěn"),
                         (ISLAND, "ē’maç", ("çō’men", ", marked (pl.)"), "çō’çin", "qō’poçěn"),
                         (SECHELT, "ē’maç", "çeçōten", "çō’çin", ("kwa’yōçin", ", carries footnote 5, written here without its digit"))))
TABLE_2 = comparison(2, 4, "Table 2: Forms from “Comparative Salishan vocabularies” (Boas 1925, Pt. 3)",
                     "Boas's spelling, Comparative Salishan vocabularies, Pt. 3",
                     ("‘chisel, to’", "‘swamp’"), (
                         (PENTLACH, ("tsī’icam", ", glossed (with hammer)"), "ts’ē’ts’ēq"),
                         (ISLAND, ("çetsā’em", ", glossed (with hammer)"), "ts’ē’ts’ēq"),
                         (SECHELT, "çē’tc’Em", "ts’ē’ts’ēq")))
TABLE_3 = comparison(3, 5, "Table 3: Forms from “Comparative Salishan vocabularies” (Boas 1925, Pt. 1)",
                     "Boas's spelling, Comparative Salishan vocabularies, Pt. 1",
                     ("‘tomorrow’", "‘hat’", "‘narrow’"), (
                         (PENTLACH, "kū’içē", "çī’aqup", "çē’içō"),
                         (ISLAND, "kū’iska", "sědja’qōm", "tī’tōl"),
                         (SECHELT, "kū’isěm", "sī’aqōm", "ts’ēatE")))
TABLE_4 = comparison(4, 5, "Table 4: Forms from “Comparative Salishan vocabularies” (Boas 1925, Pt. 3)",
                     "Boas's spelling, Comparative Salishan vocabularies, Pt. 3",
                     ("‘stern (of boat)’", "‘vertebra’"), (
                         (PENTLACH, "xē’xiap", "q’ē’qoalō"),
                         (ISLAND, "qē’ap", ("xōmā’ō", ", glossed (of fish)")),
                         ("shahishalhem", "qē’qelap", ("x.au’wa", ", glossed (of fish)"))))
# Table 5 sets Boas's field notes for pentl'ach beside the modern forms of the other two, from
# FirstVoices for ʔayʔaǰuθəm and Beaumont 2011 for shashishalhem.
TABLE_5 = comparison(5, 6, "Table 5: Boas field notes (Boas ~1910)", "the row's source",
                     ("‘to fly’", "‘woman’", "‘eyelash’", "‘long’"), (
                         (PENTLACH, "lvō’lvōk", "slā’naē", "lvē’ptěn", "lvākt"),
                         (MAINLAND, "ɬuk̓ʷ", "saɬtxʷ", "ɬɛpawus", "ƛ̓aqt"),
                         (SECHELT, "sekw’", "s-lhánay", "lhíp-ten", "tl’akt")))
TABLE_5.insert(1, ("Table 5", AUTHORS, "note", "Source", "page 6, the head of the column of languages"))


def inventory(number, page, caption, columns, rows):
    """A consonant chart: the caption, the column heads, and a row a manner, each segment under the
    column its glyphs stand in."""
    where = "Table %d" % number
    out = [(where, AUTHORS, "note", caption, "page %d, the caption" % page)]
    out += [(where, AUTHORS, "note", column, "page %d, the head of a column" % page) for column in columns]
    for head, cells in rows:
        out.append(("%s %s" % (where, head), AUTHORS, "note", head, "page %d, the head of a row" % page))
        for column, segment, note in cells:
            out.append(("%s %s" % (where, head), AUTHORS, "notation", segment,
                        "page %d, %s %s%s" % (page, column.lower(), head.lower(), note)))
    return out


TABLE_6 = inventory(6, 8, "Table 6: Consonant contrasts in pentl’ach, if <ç> is /θ/", ("Dental", "Alveolar"), (
    ("Stop", (("Alveolar", "t", ", plain"), ("Alveolar", "t’", ", ejective"))),
    ("Affricate", (("Alveolar", "(ts)", ", plain, in parentheses for the doubt the prose gives"), ("Alveolar", "ts’", ", ejective"))),
    ("Fricative", (("Dental", "θ", ""), ("Alveolar", "s", "")))))
TABLE_8 = inventory(8, 9, "Table 8: Musqueam Halkomelem alveolar and dental fricatives and affricates (Suttles 2004:3)",
                    ("Plain", "Ejective", "Fricative"), (
                        ("Dental", (("plain", "tᶿ", " affricate"), ("ejective", "t̓ᶿ", " affricate"), ("fricative", "θ", ""))),
                        ("Alveolar", (("plain", "ts", " affricate"), ("ejective", "ts’", " affricate"), ("fricative", "s", "")))))
TABLE_9 = inventory(9, 9, "Table 9: Consonant contrasts in pentl’ach, if <ç> is /ts/", ("Dental", "Alveolar"), (
    ("Stop", (("Alveolar", "t", ", plain"), ("Alveolar", "t’", ", ejective"))),
    ("Affricate", (("Alveolar", "ts", ", plain"), ("Alveolar", "ts’", ", ejective"))),
    ("Fricative", (("Alveolar", "s", ", the dental column empty"),))))

TABLE_7 = [("Table 7", AUTHORS, "note", "Table 7: Central Salish alveolar obstruents", "page 9, the caption")]
TABLE_7 += [("Table 7", AUTHORS, "note", head, "page 9, the head of a column") for head in ("Language", "Plain", "Ejective", "Fricative", "Source")]
for _language, _source in (("shashishalhem", "Beaumont (1985, 2011)"), ("Halkomelem", "Suttles (2004)"),
                           ("Skwxwú7mesh", "Jacobs (2011)"), ("Nooksack", "Galloway (1984)"),
                           ("Lushootseed", "Bates et al. (1994)")):
    TABLE_7 += [("Table 7 %s" % _language, AUTHORS, "language", _language, "page 9, the head of a row"),
                ("Table 7 %s" % _language, AUTHORS, "notation", "ts", "page 9, the plain alveolar affricate"),
                ("Table 7 %s" % _language, AUTHORS, "notation", "ts’", "page 9, the ejective alveolar affricate"),
                ("Table 7 %s" % _language, AUTHORS, "notation", "s", "page 9, the alveolar fricative"),
                ("Table 7 %s" % _language, AUTHORS, "citation", _source, "page 9, the source")]

FORMS = {
    PENTLACH: ("language", AUTHORS, "Pentlatch, Central Salish, the language of the paper; the authors follow the community in writing it in lower case"),
    "Pentl’ach": ("language", AUTHORS, "the language, capitalized in the abstract"),
    "ç": ("cited form", PENTLACH, "Boas's symbol, a voiceless interdental by his key, the sound the paper asks after"),
    "<ç>": ("cited form", PENTLACH, "Boas's symbol, a voiceless interdental by his key"),
    "<ȼ>": ("cited form", "K’omoks", "the symbol of Boas's German/K’omoks word list for the same sound"),
    "θ": ("notation", AUTHORS, "the voiceless dental fricative"),
    "/θ/": ("notation", AUTHORS, "the voiceless dental fricative, a phoneme"),
    "/č/": ("notation", AUTHORS, "the voiceless palato-alveolar affricate"),
    "/tᶿ/": ("notation", AUTHORS, "the plain dental affricate"),
    "tᶿ": ("notation", AUTHORS, "the plain dental affricate"),
    "t̓ᶿ": ("notation", AUTHORS, "the ejective dental affricate"),
    "ɬ": ("notation", AUTHORS, "the lateral fricative"),
    "ƛ̓": ("notation", AUTHORS, "the ejective lateral affricate"),
    ISLAND: ("language", AUTHORS, "Island Comox, the authors' term for the dialect Boas documented"),
    MAINLAND: ("language", AUTHORS, "Mainland Comox, whose dialects have dental fricatives and affricates"),
    "ʔayaǰuθəm": ("language", AUTHORS, "ʔayʔaǰuθəm as page 9 spells it"),
    "K’omoks": ("language", AUTHORS, "the language Boas documented beside pentl’ach"),
    "Skwxwú7mesh": ("language", AUTHORS, "Squamish"),
    "Kwak’wala": ("language", AUTHORS, "a northern Wakashan language, north of pentl’ach"),
    "hul’q’umi’num": ("language", AUTHORS, "Island Halkomelem, south of pentl’ach"),
    "Hulq’umi’num": ("language", AUTHORS, "Island Halkomelem"),
    "Hul’q’umi’num’": ("language", AUTHORS, "Island Halkomelem"),
    "hənq̓əmin̓əm": ("language", AUTHORS, "Musqueam Halkomelem, on the mainland"),
    "Tla’amin": ("language", AUTHORS, "a mainland dialect of ʔayʔaǰuθəm"),
    "SENĆOŦEN": ("language", AUTHORS, "the Northern Straits dialect with dental place"),
    "k’éya-sh-t-ámin": ("cited form", SECHELT, "page 4, ‘chisel (tool)’ (Beaumont 2011:83)"),
    "s-ts’íts’ik": ("cited form", SECHELT, "page 5, ‘mud’, the modern word"),
    "Sníchim": ("cited form", "Skwxwú7mesh", "‘language’, in the title of Jacobs 2011"),
    "sníchim": ("cited form", "Skwxwú7mesh", "‘language’, in the title of Jacobs 2011"),
    "Xwelíten": ("cited form", "Skwxwú7mesh", "‘English’, in the title of Jacobs 2011"),
    "skíxwts": ("cited form", "Skwxwú7mesh", "in the title of Jacobs 2011, the Squamish-English dictionary"),
    "Honoré": ("name", AUTHORS, "Honoré Watanabe, in a reference entry"),
}
# The partial segments the engine read out of /(tᶿ), t̓ᶿ, θ/ and the letters of Tables 1 to 5, which
# are read again whole in the tables.
DROP = ("<ç", "German/K’omoks", "ᶿ", "θ/", "t̓", "/(tᶿ)", "/θ", "tᶿ/", "Küste")
_TABLE_FORMS = {row[3] for row in TABLE_1 + TABLE_2 + TABLE_3 + TABLE_4 + TABLE_5 if row[2] == "cited form"}
_TABLE_FORMS |= {"kwa’yōçin5", "çō’men", "x.au’wa"}

SPLIT = [
    ("front", "Mathew Andreatta Jesse Recalma", {"where": "title", "kind": "title", "gloss": "page 1, the title, its star the acknowledgement footnote's"}, {}),
    ("front", "Abstract:", {"where": "title", "gloss": "page 1, the authors and their affiliations"}, {"gloss": "page 1, the abstract"}),
    ("§2", "As pointed out above, neither", None, {}),
    ("§2", "What is noticeable about", None, {}),
    ("§2", "As you can see, there is an incredible", None, {}),
    ("§2", "In these words, pentl’ach has a fricative", None, {}),
    ("§2", "Boas also did not reliably distinguish between lateral", {}, {}),
    ("§2", "This shows that Boas did not", None, {}),
    ("§4", "If /ts/ is absent", None, {}),
    ("§4", "So, we can assume", None, {}),
    ("§4", "Notice that there is a contrast", None, {}),
    ("§3", "Figure 1: Salish languages adjacent", {}, {"where": "Figure 1", "gloss": "page 7, the caption of the map"}),
    ("Figure 1", "Notice that pentl’ach is to", {}, {"where": "§3"}),
]
REMOVE = [("§4", one[3]) for one in DRAFT if one[3].startswith("Table 9: Consonant contrasts")]
REMOVE += [("§2", form) for form in _TABLE_FORMS]
REMOVE_WHERE = r"^Table [234] line"

ADD = tuple(
    [(("title", "Mathew Andreatta Jesse Recalma..."), ("title", AUTHORS, "name", "Suzanne Urbanczyk", "author, University of Victoria")),
     (("title", "Mathew Andreatta Jesse Recalma..."), ("title", AUTHORS, "name", "Jesse Recalma", "author, Qualicum First Nation")),
     (("title", "Mathew Andreatta Jesse Recalma..."), ("title", AUTHORS, "name", "Mathew Andreatta", "author, Qualicum First Nation")),
     (("footnote 2", "2 Many thanks to Daniel Reisinger..."), ("footnote 2", AUTHORS, "name", "Daniel Reisinger", "thanked in footnotes 2 and 6 for his work on K’omoks and ʔayʔaǰuθəm")),
     (("§2", "k’éya-sh-t-ámin"),("§2", SECHELT, "cited form", "tsek’-t", "page 4, ‘hammer s.th. firmly into ground’ (Beaumont 2011:201)")),
     (("§5", "ʔayʔaǰuθəm has two dialect groups..."), ("§5", AUTHORS, "notation", "/(tᶿ), t̓ᶿ, θ/", "page 10, the dental series of Musqueam and of Mainland Comox, the plain affricate marginal")),
     (("§5", "ʔayʔaǰuθəm has two dialect groups..."), ("§5", AUTHORS, "notation", "/ts, ts’, s/", "page 10, the alveolar series of Island Comox")),
     (None, ("all", AUTHORS, "notation", "<…>", "a symbol as Boas wrote it")),
     (None, ("all", AUTHORS, "notation", "/…/", "a phoneme")),
     (None, ("all", AUTHORS, "notation", "[…]", "a phonetic value"))]
    + chained(("§2", "There are many forms in the “Comparative Salish vocabularies”..."), TABLE_1)
    + chained(("§2", "In addition to looking for instances of <ç>..."), TABLE_2)
    + chained(("§2", "What is noticeable about..."), TABLE_3)
    + chained(("§2", "A key articulatory difference..."), TABLE_4)
    + chained(("§2", "Boas also did not reliably distinguish between lateral..."), TABLE_5)
    + chained(("§4", "Phoneme inventories are usually symmetrical..."), TABLE_6)
    + chained(("§4", "The situation in which pentl’ach lacks..."), TABLE_7)
    + chained(("§4", "The following chart illustrates..."), TABLE_8)
    + chained(("§4", "If <ç> represents the alveolar affricate..."), TABLE_9)
)

SET = {("§2", "s-ts’íts’ik"): {"form": "s-ts’íts’ik’"}}
# The words a footnote cites, which the engine left in the section around it.
_note = None
for _where, _who, _kind, _form, _gloss in DRAFT:
    _page = re.search(r"page (\d+)", _gloss)
    _page = int(_page.group(1)) if _page else 0
    if _kind == "note" and "footnote" in _gloss:
        _mark = re.match(r"^(\d{1,2}|\*) ", _form)
        _note = (_mark.group(1), _page) if _mark else None
        continue
    if _kind != "cited form" or not _note or _page != _note[1] or (_where, _form) in REMOVE:
        _note = None
        continue
    SET.setdefault((_where, _form), {})["where"] = "footnote %s" % _note[0]

WHOSE = (
    "The forms are Boas's: his spellings of pentl'ach, ʔayʔaǰusəm (Island Comox) and shashishalhem "
    "(Sechelt) in the Comparative Salishan vocabularies and his field notes, set beside modern "
    "ʔayʔaǰuθəm forms from FirstVoices and shashishalhem forms from Beaumont 2011. Each cell of "
    "Tables 1 to 5 is given to the language of its row. The consonant charts, Tables 6 to 9, set "
    "phonemes, which are notation rows of the authors, and the prose and the notes are the authors'."
)

LETTERS = (
    "Boas wrote ç and ¢ for the sound in question, ē and ō with macrons, ě, E and the apostrophe after "
    "a stressed vowel; lv for a lateral. The modern forms write ɬ, ƛ̓, k̓ʷ and xʷ, and shashishalhem "
    "its practical orthography, lh and tl’. The phonemes are in slashes, the phonetic values in "
    "square brackets and Boas's symbols in angle brackets."
)

PAGE_NOTES = (
    "The text layer ran each table into the paragraph after it; the tables are read off the page one "
    "cell a row, and the columns of the consonant charts, where Tables 6 and 9 leave the dental stop "
    "and affricate cells empty, are placed by the glyph positions. Table 4 spells its last row "
    "shahishalhem."
)
