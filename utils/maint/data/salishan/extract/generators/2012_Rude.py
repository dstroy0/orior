"""The ops of 2012_Rude: Noel Rude's Reconstructing Proto-Sahaptian sounds.

The paper sets its forms upright; only the Latin names of plants and animals are in italics. An
example is a cognate set, each language's forms after its tag, NP kíi /kí/ ‘this’; S čí; PS *kí:
the example is a note as printed, and each form is a cited form under the language its tag names,
an underlying form between slashes a phonemic row, with the gloss the set gives it. A form in the
prose is cited under the language the text names nearest ahead of it. The underlying forms of
Nez Perce strong morphemes are underlined on the page; a phonemic row notes each underlined
morpheme in its gloss. Tables 1 to 11 are read cell by cell from the renders, each checked against
the printed lines letter for letter. Table 1 sets its central vowel as a Times i with a bar the
text layer loses, and it is written ɨ. Tables 1 and 5 and examples (133) to (135) stand beside
prose the text layer interleaves with them, and the prose is written from the renders too.
"""
import os
import re
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402
import page_text  # noqa: E402

A = gen.A
LANGUAGE = "Sahaptin"
AUTHORS = ["Noel Rude"]
STEM = "2012_Rude"
paper = gen.Paper(STEM, authors=AUTHORS[0], language=LANGUAGE)
NP, S, PS = "Nez Perce", "Sahaptin", "Proto-Sahaptian"
NAMES = [("Aoki", "Haruo Aoki (1962, 1963, 1966, 1994), Nez Perce and Northern Sahaptin, the Nez Perce dictionary"),
         ("Rigsby", "Bruce Rigsby (1965, 1969 with Michael Silverstein, 1996 with Noel Rude), Sahaptian vowels"),
         ("Silverstein", "Michael Silverstein (1969, with Bruce Rigsby), Nez Perce vowels"),
         ("Pharis", "Nicholas J. Pharris (2006), Molalla and Nez Perce, spelled Pharis in note 1"),
         ("DeLancey", "Scott DeLancey (1997, with Victor Golla), the Penutian hypothesis"),
         ("Golla", "Victor Golla (1997, with Scott DeLancey), the Penutian hypothesis"),
         ("Mithun", "Marianne Mithun (1999), the languages of native North America"),
         ("Jacobs", "Melville Jacobs (1929, 1931, 1934, 1937), Northern Sahaptin grammar and texts"),
         ("Millstein", "Henry Millstein (ca. 1990a, 1990b), Warm Springs Sahaptin"),
         ("Beavert", "Virginia Beavert (2002a, 2002b, 2005, 2009, with Sharon Hargus), Yakima Sahaptin"),
         ("Hargus", "Sharon Hargus, who read the paper and heard the Nez Perce recording; Yakima Sahaptin"),
         ("Inez Spino Reves", "a Umatilla speaker, the author's source for the Columbia River examples"),
         ("Elizabeth Wocatsie Jones", "a Walla Walla speaker, the author's source for the Northeast examples"),
         ("Crook", "Harold D. Crook (1996), Nez Perce nouns with irregular metrical behavior"),
         ("Barker", "M. A. R. Barker (1963), the Klamath dictionary"),
         ("Eugene John", "a native speaker of Nez Perce, who recorded words with glottalized resonants")]
LANGUAGES = [(S, "Sahaptian"), (NP, "Sahaptian"), ("Sahaptian", "the family"), (PS, "the reconstructed parent"),
             ("Umatilla", "Columbia River Sahaptin"), ("Tenino", "Columbia River Sahaptin"),
             ("Celilo", "Columbia River Sahaptin"), ("Klickitat", "Northwest Sahaptin"),
             ("Upper Cowlitz", "Northwest Sahaptin"), ("Yakima", "Northwest Sahaptin"),
             ("Priest Rapids", "Northeast Sahaptin"), ("Walla Walla", "Northeast Sahaptin"),
             ("Palouse", "Northeast Sahaptin"), ("Warm Springs", "Sahaptin"), ("Plateau Penutian", "the stock"),
             ("Penutian", "the macro-family"), ("Uto-Aztecan", "a family the paper links Sahaptian with"),
             ("Klamath", "Plateau Penutian"), ("Jargon", "Chinook Jargon, the source of táqmaał"),
             ("Canadian French", "the source of čalámat")]
# The tags of the paper's abbreviations, note 1; U, Umatilla, is not in the list.
TAGS = {"NP": NP, "S": S, "PS": PS, "CR": "Columbia River Sahaptin", "NE": "Northeast Sahaptin",
        "NW": "Northwest Sahaptin", "N": "Northern Sahaptin", "K": "Klikitat", "Y": "Yakima", "WS": "Warm Springs",
        "U": "Umatilla", "Klamath": "Klamath"}
FOOT = {one for parts, _ in paper.page_footnotes().values() for one in parts}
RUNNING = paper.running_numbers_set()
UNDERLINED = page_text.underlined_runs(page_text.paper_document(STEM)[0], page_text.PAPER_CIPHERS.get(STEM))


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def same_letters(cells, first, last):
    """Check that the cells written from lines first to last hold the printed letters, all of them
    and no others, whatever the order the columns set them in."""
    written = sorted("".join(cells).replace(" ", ""))
    source = sorted("".join(paper.text(one) for one in range(first, last + 1) if printed(one)).replace(" ", ""))
    assert written == source, (first, last, "".join(written), "".join(source))


def bare(text):
    return re.sub(r"[\s´]", "", text)


def underlined(page, form):
    """The morphemes of an underlying form the page underlines, the strong morphemes."""
    runs = [bare(one) for one in UNDERLINED.get(page, ())]
    inside = form.strip("/")
    if bare(inside) in runs:
        return [inside]
    return [piece for piece in re.split(r"[-\s]", inside) if len(bare(piece)) and bare(piece) in runs]


def strong(page, form):
    found = underlined(page, form) if form.startswith("/") else []
    if not found:
        return ""
    if found == [form.strip("/")]:
        return ", underlined, a strong morpheme"
    return ", %s underlined, %s" % (" and ".join(found), "a strong morpheme" if len(found) == 1 else "strong morphemes")


# English words the example reader meets among the forms: the asides in a set, from Jargon,
# purpose nominalizer, bound, the labels of (273), and the words of (310)'s comment.
ENGLISH = {"from", "Jargon", "purpose", "nominalizer", "is", "bound", "in", "imperative", "may", "be", "a",
           "reduplication", "borrowed", "e.g.", "vi.", "plural", "reflecting", "an", "earlier", "e.g"}
BRACKET_GLOSSES = {"sound of rattlesnake"}
SEPARATORS = {"~", ",", "&", "<", ">", "+", "or", "cf.", "also", "and", "-", "–"}
QUOTE_CLOSE = re.compile(r"’(?![^\W\d_])")


def split_top(text, sign=";"):
    """text split at each sign that stands outside quotes and brackets."""
    parts, depth, quoted, start = [], 0, False, 0
    for at, one in enumerate(text):
        if one == "‘" and not quoted:
            quoted = True
        elif one == "’" and quoted and QUOTE_CLOSE.match(text, at) and not text.startswith("’ woodpecker", at):
            quoted = False
        elif one == "(" and not quoted:
            depth += 1
        elif one == ")" and not quoted:
            depth -= 1
        elif one == sign and not quoted and depth == 0:
            parts.append(text[start:at])
            start = at + 1
    parts.append(text[start:])
    return [one.strip() for one in parts if one.strip()]


def tokens(text):
    """The words of a stretch, a gloss in quotes, a bracket and a form between slashes each one."""
    raw, out, at = text.split(), [], 0
    while at < len(raw):
        word = raw[at]
        if word.startswith("‘"):
            while not re.search(r"’[,.;:)]*$", word) or word in ("‘Lewis’",):
                at += 1
                word += " " + raw[at]
        elif word.startswith("("):
            while word.count("(") > word.count(")"):
                at += 1
                word += " " + raw[at]
        elif word.startswith("/") and word.rstrip(",.;:)").count("/") == 1:
            # A form between slashes runs to the closing slash, /hawl + -ʔis/ over three words.
            while word.rstrip(",.;:)").count("/") == 1 and at + 1 < len(raw):
                at += 1
                word += " " + raw[at]
        out.append(word)
        at += 1
    return out


def forms_of(text, languages):
    """[[kind, form, languages, glosses]] of a stretch of a cognate set: each form after the tag of
    its language, the forms ahead of a gloss taking it. A bracket that opens on a tag, a star or a
    slash is read the same way; another is an aside or a source."""
    items, pending, last, glossed = [], [], None, []
    for part in split_top(text):
        last = "part"
        for word in tokens(part):
            core = word.rstrip(",.:") if not word.startswith(("‘", "(")) else word
            if word.startswith("‘"):
                gloss = re.sub(r"[,.;:)]*$", "", word)
                if gloss.count("(") < gloss.count(")"):
                    gloss = gloss[:gloss.rindex(")")]
                # A second gloss after "or" goes to the forms the first went to, ‘go!’ or ‘do!’.
                glossed = glossed if last == "or" else pending or items[-1:]
                for one in glossed:
                    one[3].append(gloss)
                pending, last = [], "gloss"
                continue
            if word.startswith("("):
                inside = word.rstrip(",.;:")[1:-1]
                opens = inside.split()[0] if inside.split() else ""
                # A bracketed gloss, (‘woman’s brother’s daughter’) and (sound of rattlesnake), goes
                # with the forms ahead of it.
                if inside.startswith("‘") or inside in BRACKET_GLOSSES:
                    glossed = pending or glossed
                    for one in glossed:
                        one[3].append("(%s)" % inside)
                    pending, last = [], "gloss"
                    continue
                # A bracket of forms, (čmɨ́kʷ < čmúk ‘black’), is read like one after a tag, and so is
                # one whose forms follow a few words, (purpose nominalizer is *-ʔeš).
                if opens.rstrip(",") in TAGS or opens in ("cf.", "<", "e.g.,", "purpose", "reflecting") \
                        or inside.startswith(("*", "/")) or " < " in inside and is_form(opens):
                    inner = forms_of(inside, languages)
                    items.extend(inner)
                    if " < " not in inside:
                        pending.extend(one for one in inner if not one[3])
                last = "aside"
                continue
            if core in TAGS:
                languages = languages + [TAGS[core]] if last == "&tag" else [TAGS[core]]
                last = "tag"
                continue
            if core in SEPARATORS or word in SEPARATORS:
                last = "&tag" if core == "&" and last == "tag" else "or" if core == "or" and last == "gloss" \
                    else "separator"
                continue
            if core in ENGLISH or word.rstrip(";") in ENGLISH:
                last = "english"
                continue
            if core.startswith("/"):
                item = ["phonemic", core, list(languages), []]
            else:
                # Two words with nothing between them are one form, kex ʔíin and čná iwačáʔ.
                if last == "form" and items[-1][0] == "cited form" and not items[-1][3]:
                    items[-1][1] += " " + core
                    last = "form" if core == word else "separator"
                    continue
                # A starred form is a reconstruction, *weyélikenwi in (46) with no tag ahead of it.
                item = ["cited form", core, [PS] if core.startswith("*") else list(languages), []]
            items.append(item)
            pending.append(item)
            last = "form" if core == word else "separator"
    return items


def write_forms(where, items, page, what):
    for kind, form, languages, glosses in items:
        gloss = ", ".join(glosses)
        for language in languages:
            paper.add(where, language, kind, form, "page %d, %s%s%s" % (
                page, what, ", " + gloss if gloss else "", strong(page, form)))


# Examples.

HEADINGS = paper.headings(paper.find(r"^1 Vowels$"), paper.find(r"^References$") - 1, skip=FOOT)


def continues(text, line):
    """Whether line runs on the example whose text so far is text: the text is left open, or the
    line opens on what only a set's second line opens on."""
    if line in HEADINGS or gen.EXAMPLE.match(paper.text(line)) or not printed(line):
        return False
    following = paper.text(line)
    if re.search(r"[;,<&]$|\b(?:%s)$" % "|".join(TAGS), text) or text.count("‘") > len(QUOTE_CLOSE.findall(text)) \
            or text.count("(") > text.count(")"):
        return True
    if re.match(r"^[‘/*(\d]|^[^\W\d_A-Z]", following):
        return True
    return bool(re.match(r"^(?:%s) \S+(?:;|$)" % "|".join(TAGS), following))


def example(first, where):
    """A numbered cognate set: a note as printed, then its forms."""
    label, text = gen.EXAMPLE.match(paper.text(first)).groups()
    page = paper.page(first)
    number = after(first)
    while number <= paper.last and continues(text, number):
        text += " " + paper.text(number)
        number = after(number)
    write_example(label, text, page)
    return number


# (249) prints the Nez Perce form of (58) with no tag between two Sahaptin ones.
UNTAGGED = {("249", "k̓ócac"): NP, ("249", "/k̓ʷcc/"): NP}


def write_example(label, text, page):
    where = "(%s)" % label
    paper.add(where, A, "note", text, "page %d, the example as printed" % page)
    items = forms_of(text, [S])
    for item in items:
        if (label, item[1]) in UNTAGGED:
            item[2] = [UNTAGGED[label, item[1]]]
            item[3].append("printed with no tag, the Nez Perce form of (58)")
    write_forms(where, items, page, "example (%s)" % label)


# Front matter, Figure 1 and the tables.

def figure_1(first, where):
    """Figure 1, the family tree: the caption over it, then each node a language, row by row."""
    page = paper.page(first)
    last = paper.find(r"^Northwest Northeast$", first)
    nodes = ["Proto-Sahaptian", "Nez Perce", "Sahaptin", "Columbia River", "Northern", "Northwest", "Northeast"]
    same_letters([paper.text(first)] + nodes, first, last)
    paper.add("Figure 1", A, "note", paper.text(first), "page %d, the figure's caption" % page)
    for node, under in zip(nodes, ("the root", PS, PS, S, S, "Northern", "Northern")):
        paper.add("Figure 1", A, "note", node, "page %d, a node of the tree%s" % (
            page, ", under " + under if under != "the root" else ", its root"))
    return after(last)


def table_1(first, where):
    """The paragraph Table 1 stands beside, which the text layer interleaves with the caption, then
    the table: a row of vowels for each height under Front, Central and Back."""
    page = paper.page(first)
    last = paper.find(r"^in underlying form between slashes\.", first)
    said = ("Proto-Sahaptian *i and *u survive intact in both Nez Perce and Sahaptin. Stressed vowels are "
            "length-", "ened in Nez Perce (though with some phonological caveats). Because of the complexity of "
            "Nez Perce phonology Nez Perce examples will be provided in underlying form between slashes.")
    caption = "Table 1. Proto-Sahaptian vowels"
    heads = ("Front", "Central", "Back")
    rows = (("High", ("i", "ɨ", "u")), ("Mid", (None, None, "o")), ("Low", ("æ", None, "α")))
    cells = list(said) + [caption] + list(heads) + [one for name, row in rows for one in (name,) + row if one]
    same_letters([one.replace("ɨ", "i") for one in cells], first, last)
    body = said[0][:-1] + said[1]
    paper.add(where, A, "note", body, "page %d" % page)
    my_cited(where, body, [page])
    paper.add("Table 1", A, "note", caption, "page %d, the table's caption, set beside the paragraph" % page)
    for head in heads:
        paper.add("Table 1", A, "note", head, "page %d, a column's head" % page)
    for name, row in rows:
        paper.add("Table 1", A, "note", name, "page %d, the row's head" % page)
        for head, vowel in zip(heads, row):
            if vowel:
                paper.add("Table 1", PS, "cited form", vowel, "page %d, %s %s%s" % (
                    page, name.lower(), head.lower(),
                    ", set as a Times i with a bar the text layer loses" if vowel == "ɨ" else ""))
    return after(last)


def table_2(first, where):
    """Table 2: Sahaptin forms with no ablaut beside their long vowel forms, under each ablaut."""
    page = paper.page(first)
    last = paper.find(r"^mɨ́ł ", first)
    paper.add("Table 2", A, "note", paper.text(first), "page %d, the table's caption" % page)
    number = after(first)
    for head in re.split(r"\s{3,}", paper.spaced[number]):
        paper.add("Table 2", A, "note", head, "page %d, a column's head" % page)
    number = after(number)
    kind = None
    while number <= last:
        text = paper.text(number)
        if text.endswith("-ablaut"):
            kind = text
            paper.add("Table 2", A, "note", text, "page %d, the heading of the rows under it" % page)
        else:
            for column, cell in zip(("zero", "long vowel"), re.split(r"\s{3,}", paper.spaced[number])):
                write_forms("Table 2", forms_of(cell, [S]), page, "%s, the %s column" % (kind, column))
        number = after(number)
    return number


def cell_table(label, first, last, heads, sections, language):
    """A table whose cells are cognate sets, under the headings of sections ([(heading, [row])], a
    row a tuple of cells), each cell under its column's head in heads."""
    page = paper.page(first)
    caption = paper.text(first)
    cells = [caption] + list(heads) + [one for heading, rows in sections for one in
                                       ([heading] if heading else []) + [cell for row in rows for cell in row if cell]]
    same_letters(cells, first, last)
    paper.add(label, A, "note", caption, "page %d, the table's caption" % page)
    for head in heads:
        paper.add(label, A, "note", head, "page %d, a column's head" % page)
    for heading, rows in sections:
        if heading:
            paper.add(label, A, "note", heading, "page %d, the heading of the rows under it" % page)
        for row in rows:
            for head, cell in zip(heads, row):
                if cell:
                    paper.add(label, A, "note", cell, "page %d, %s%s, the cell as printed" % (
                        page, heading + ", " if heading else "", head.lower()))
                    write_forms(label, forms_of(cell, [language]), page, "%s%s" % (
                        heading + ", " if heading else "", head.lower()))
    return after(last)


TABLE_3 = [("aa-ablaut", [("/c̓´kn/ ‘be split, cut’ (Aoki 1994:58); cf. S č̓ɨ́x̣n ‘be cut, cracked, split’",
                           "hic̓áax̣ca /hi-c̓áax̣n-sen-s/ ‘it is splitting’ (Aoki 1994:61)"),
                          ("/k̓ppn/ ‘be round’ (Aoki 1994:266)", "k̓apáap ‘in a round manner’ (Aoki 1994:265)"),
                          ("táyam /t´yam/ ‘summer’ (Aoki 1994:697)",
                           "táayamima /táayamima/ ‘from in the summer’ (Aoki 1994:697)"),
                          ("t̓átnin̓ /t̓´tniʔns/ ‘torn’ (Aoki 1994:809)", "t̓áat /t̓áat/ ‘torn’ (Aoki 1994:810)"),
                          ("/waqlp/ ‘put the arms around’ (Aoki 1994:384)",
                           "/waqláaptan/ ‘hold in arms’ (Aoki 1994:343)"),
                          ("wáy̓at /wy̓at/ ‘far’ (Aoki 1994:839); cf. S wíyat",
                           "/wáay̓atn/ ‘go on guardian spirit quest’ (Aoki 1994:839)"),
                          ("/x̣l´p/ ‘open’ (Aoki 1994:914); cf. S x̣lɨ́p ‘open’ (vi.)",
                           "x̣aláap /x̣láap/ ‘slowly opened’ (Aoki 1994:914)")]),
           ("ee-ablaut", [("/ʔnp/ ‘get, take, hold’ (Aoki 1994:1045); cf. S nɨ́p (bound)",
                           "/ʔnéepten/ ‘hold, keep, dominate’ (Aoki 1994:1032)")]),
           ("ii-ablaut", [("/tlqn/ ‘stop’ (Aoki 1994:673) (Aoki 1994:673)",
                           "talíix̣ /tlíiq/ ‘still, quiet’ (Aoki 1994:677)"),
                          ("/tpípi/ ‘foam, be sudsy’ (Aoki 1994:753)", "tíipip /tíipip/ ‘foam’ (Aoki 1994:753)")]),
           ("oo-ablaut", [("/q̓ʷł/ bound in /nkáq̓ʷłk/ ‘remove’ (Aoki 1994:611)",
                           "q̓óoł /q̓óoł/ ‘slippingly’ (Aoki 1994:611)")]),
           ("uu-ablaut", [("/p´qʷn/ ‘go separate ways’ (Aoki 1994:560)", "púux̣ /púuq/ ‘scatteringly’ (Aoki 1994:560)"),
                          ("qúx̣ /qʷ´q/ ‘loose dirt’ (Aoki 1994:598)", "qúux̣ /qúuq/ ‘powdery’ (Aoki 1994:598)"),
                          ("/qʷ´qn/ ‘raise dust, powdery snow’ (Aoki 1994:598)",
                           "/qúux̣n/ ‘be gray (of cloud)’ (Aoki 1994:598)")])]


def table_3(first, where):
    """Table 3: Nez Perce forms with no ablaut beside their long vowel forms, with Aoki's pages."""
    return cell_table("Table 3", first, paper.find(r"^1994:598\)$", first), ("Zero", "Long Vowel"), TABLE_3, NP)


TABLE_4 = (("Absolute", ("kʷɨ́ma ‘those’", None, "kʷiiní ‘those two’", None)),
           ("Accusative", (None, "kʷaaná ‘that’ kʷaamanáy ‘those’", "kʷíinaman ‘those two’",
                           "NW kuunák ‘that’ NW kuumanák ‘those’")),
           ("Genitive", ("kʷɨnmí ‘of that’", "kʷaamíin ‘of those’", "kʷiinamí ‘of those two’", "NW kuumínk ‘of those’")),
           ("Ergative", ("kʷɨ́nɨm ‘that’", None, None, None)), ("Allative", ("íkʷɨn ‘to that’", None, None, None)),
           ("Versative", (None, "kʷáan ‘toward that’", None, "NW kuuník ‘toward that’")),
           ("Ablative", ("kʷɨ́ni ‘from that’", None, None, None)), ("Locative", ("kʷná ‘in that’", None, None, None)),
           ("Instrumental", ("kʷɨ́nki ‘with that’", None, None, None)),
           ("Distance", (None, "kʷáal ‘that long/far’", None, None)),
           ("Quantity", ("kʷɨ́ł ‘that many/much’", None, None, None)),
           ("Lateral", ("kʷníin ‘on that side’", None, None, None)))


def table_4(first, where):
    """Table 4: the distal demonstrative's cases under each ablaut. The Northwest forms set their tag
    in small capitals; ‘that many/much’ breaks after its slash with a hyphen."""
    page = paper.page(first)
    last = paper.find(r"^Lateral side’$", first)
    heads = ("zero-ablaut", "aa-ablaut", "ii-ablaut", "uu-ablaut")
    cells = [paper.text(first)] + list(heads) + [one for name, row in TABLE_4 for one in (name,) + row if one] + ["-"]
    same_letters(cells, first, last)
    paper.add("Table 4", A, "note", paper.text(first), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 4", A, "note", head, "page %d, a column's head" % page)
    for name, row in TABLE_4:
        paper.add("Table 4", A, "note", name, "page %d, the row's head" % page)
        for head, cell in zip(heads, row):
            if cell:
                paper.add("Table 4", A, "note", cell, "page %d, %s, %s, the cell as printed%s" % (
                    page, name, head, ", NW in small capitals" if "NW" in cell else
                    ", the line broken after the slash with a hyphen" if "many/much" in cell else ""))
                write_forms("Table 4", forms_of(cell, [S]), page, "%s, %s" % (name.lower(), head))
    return after(last)


def table_5(first, where):
    """The paragraph Table 5 stands beside, which the text layer interleaves with the table, then
    the table: the weak set and the strong set, the strong vowels underlined."""
    page = paper.page(first)
    last = paper.find(r"^Table 5\. The vowel /i/ occurs in both sets\.$", first) or \
        paper.find(r" e a$", first)
    said = ("Though only Nez Perce has vowel harmony, there is evidence that the phenomenon existed in "
            "Proto-Sahaptian. There are in Nez Perce two sets of vowels, a weak set and a strong set, as in "
            "Table 5. The vowel /i/ occurs in both sets.")
    caption = "Table 5. Vowel Harmony Sets"
    sets = (("Weak", ("i", "u", "e")), ("Strong", ("i", "o", "a")))
    same_letters([said, caption] + [one for head, vowels in sets for one in (head,) + vowels], first, last)
    number = after(last)
    rest = []
    while number <= paper.last and printed(number) and not gen.EXAMPLE.match(paper.text(number)):
        rest.append(number)
        number = after(number)
    body = said + " " + paper.joined(rest)
    paper.add(where, A, "note", body, "page %d" % page)
    my_cited(where, body, [page])
    paper.mentions(where, body, NAMES, "name")
    paper.add("Table 5", A, "note", caption, "page %d, the table's caption, set beside the paragraph" % page)
    for head, vowels in sets:
        paper.add("Table 5", A, "note", head, "page %d, a column's head" % page)
        for vowel in vowels:
            paper.add("Table 5", NP, "cited form", vowel, "page %d, the %s set%s" % (
                page, head.lower(), ", underlined" if head == "Strong" and vowel != "i" else
                ", the i of the strong set underlined" if head == "Strong" else ""))
    return number


def table_6(first, where):
    """Table 6: the plain sounds and their diminutives, and Sahaptin's augmentatives, the heads of
    the sub-columns in small italics."""
    page = paper.page(first)
    last = paper.find(r"^u o$", first)
    heads = [("Nez Perce", "Plain"), ("Nez Perce", "Diminutive"), ("Sahaptin", "Plain"), ("Sahaptin", "Diminutive"),
             ("Sahaptin", "Augmentative")]
    rows = (("m", "w", "m", "w", None), ("n", "l", "n", "l", None), ("s", "c", "š", "s", "ł"), (None, None, "č", "c", "ƛ"),
            ("k", "q", "q", "k", None), ("x", "x̣", "x̣", "x", None), ("C", "C̓", "C", "C̓", None), ("e", "a"), ("u", "o"))
    note = "Note that C = consonant"
    cells = [paper.text(first), "Nez Perce", "Sahaptin"] + [head for _, head in heads] + \
        [one for row in rows for one in row if one] + [note]
    same_letters(cells, first, last)
    paper.add("Table 6", A, "note", paper.text(first), "page %d, the table's caption" % page)
    for language in ("Nez Perce", "Sahaptin"):
        paper.add("Table 6", A, "note", language, "page %d, the head over its language's columns" % page)
    for language, head in heads:
        paper.add("Table 6", A, "note", head, "page %d, a column's head under %s, in small italics" % (page, language))
    for count, row in enumerate(rows, 1):
        for (language, head), cell in zip(heads, row):
            if cell:
                paper.add("Table 6", language, "cited form", cell, "page %d, row %d, %s%s" % (
                    page, count, head.lower(), ", C a consonant" if cell.startswith("C") else ""))
    paper.add("Table 6", A, "note", note, "page %d, under the Sahaptin columns" % page)
    return after(last)


def table_7(first, where):
    """Table 7: the Nez Perce diphthongs under weak and strong vowel harmony."""
    page = paper.page(first)
    last = paper.find(r"^múuyn ", first)
    paper.add("Table 7", A, "note", paper.text(first), "page %d, the table's caption" % page)
    number = after(first)
    heads = re.split(r"\s{3,}", paper.spaced[number])
    for head in heads:
        paper.add("Table 7", A, "note", head, "page %d, a column's head" % page)
    number = after(number)
    while number <= last:
        for head, cell in zip(heads, re.split(r"\s{3,}", paper.spaced[number])):
            write_forms("Table 7", forms_of(cell, [NP]), page, head.lower())
        number = after(number)
    return number


def table_8(first, where):
    """Table 8: Nez Perce diphthongs from breaking beside Sahaptin simple vowels, each group under
    its change; the second ɨw and the second ɨy are underlined, strong."""
    page = paper.page(first)
    last = paper.find(r"^sáyxsayx ", first)
    paper.add("Table 8", A, "note", paper.text(first), "page %d, the table's caption" % page)
    number = after(first)
    heads = re.split(r"\s{3,}", paper.spaced[number])
    for head in heads:
        paper.add("Table 8", A, "note", head, "page %d, a column's head" % page)
    number = after(number)
    seen = {}
    change = None
    while number <= last:
        cells = re.split(r"\s{3,}", paper.spaced[number])
        if len(cells) == 3:
            change = cells.pop(0)
            seen[change] = seen.get(change, 0) + 1
            paper.add("Table 8", A, "note", change, "page %d, the head of the group beside it%s" % (
                page, ", its %s underlined, strong" % change.split()[0] if seen[change] == 2 else ""))
        for head, cell in zip(heads, cells):
            write_forms("Table 8", forms_of(cell, [NP if head == "Nez Perce" else S]), page,
                        "%s, the %s column" % (change, head))
        number = after(number)
    return number


def table_9(first, where):
    """Table 9: the consonants, a row of the page a line of the table."""
    page = paper.page(first)
    last = paper.find(r"^w̓ y̓$", first)
    paper.add("Table 9", A, "note", paper.text(first), "page %d, the table's caption" % page)
    number, count = after(first), 0
    while number <= last:
        count += 1
        for sound in paper.text(number).split():
            paper.add("Table 9", PS, "cited form", sound, "page %d, row %d, a consonant of Nez Perce or Sahaptin" % (
                page, count))
        number = after(number)
    return number


def table_10(first, where):
    """Examples (133) to (135) and the prose between them, which the text layer interleaves with
    Table 10 beside them, then the table: each sibilant of Nez Perce and Sahaptin beside its
    Proto-Sahaptian source."""
    page = paper.page(first)
    last = paper.find(r"^PS \*č̓č̓ɨ́l/\*č̓č̓áal$", first)
    sets = (("133", "S qčáqn ‘open the mouth’; NP /qseqn/; PS *qčéqn"),
            ("134", "S ččúu ‘quiet, still’; NP /s´wn/ ‘be silent, absent’; PS *(č)čɨ́w(n)"),
            ("135", "S čč̓áal ‘noisy’; NP c̓ic̓ál /c̓c̓´l/; PS *č̓č̓ɨ́l/*č̓č̓áal"))
    said = "There are also a few examples where Sahaptin č̓ corresponds to Nez Perce c̓."
    caption = "Table 10. Sahaptian sibilants"
    heads = ("Nez Perce", "Sahaptin", "Proto-Sahaptian")
    rows = (("s", "č", "*č"), ("c̓", "č̓", "*č̓"), ("s", "š", "*š"), ("c", "s", "*s"), ("s", "ƛ", "*ƛ"),
            ("c̓", "ƛ̓", "*ƛ̓"), ("s", "ł", "*ł"))
    cells = ["(%s)%s" % pair for pair in sets] + [said, caption] + list(heads) + \
        [one for row in rows for one in row]
    same_letters(cells, first, last)
    for label, text in sets[:2]:
        write_example(label, text, page)
    paper.add(where, A, "note", said, "page %d" % page)
    my_cited(where, said, [page])
    write_example(*sets[2], page)
    paper.add("Table 10", A, "note", caption, "page %d, the table's caption, set beside (133) to (135)" % page)
    for head in heads:
        paper.add("Table 10", A, "note", head, "page %d, a column's head%s" % (
            page, ", broken over two lines" if " " in head or "-" in head else ""))
    for count, row in enumerate(rows, 1):
        for head, cell in zip(heads, row):
            paper.add("Table 10", head, "cited form", cell, "page %d, row %d" % (page, count))
    return after(last)


def table_11(first, where):
    """Table 11: a gloss beside its Nez Perce underlying form and its Sahaptin form."""
    page = paper.page(first)
    last = paper.find(r"^‘say, tell’ ", first)
    paper.add("Table 11", A, "note", paper.text(first), "page %d, the table's caption" % page)
    number = after(first)
    heads = re.split(r"\s{3,}", paper.spaced[number])
    for head in heads:
        paper.add("Table 11", A, "note", head, "page %d, a column's head" % page)
    number = after(number)
    while number <= last:
        gloss, underlying, sahaptin = re.split(r"\s{3,}", paper.spaced[number])
        write_forms("Table 11", [["phonemic", underlying, [NP], [gloss]]] + forms_of(sahaptin + " " + gloss, [S]),
                    page, "the row of %s" % gloss)
        number = after(number)
    return number


# The prose's forms, set upright.

NAMED = dict(TAGS, **{NP: NP, S: S, PS: PS, "Proto-": PS, "Sahaptian": "Sahaptian", "Nez": NP,
                      "Jargon": "Chinook Jargon", "Canadian French": "Canadian French"})
NAMED_AT = re.compile(r"(?<![\w*/-])(%s)(?![\w-])" % "|".join(re.escape(one) for one in sorted(NAMED, key=len, reverse=True)))
PLAIN = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,;:!?'\"()[]-–—‘’“”§&%+=<>…⊂é/~")
# A form between slashes spaced around a plus, /ʔ + n/, is one form, and so is a pair of
# phonetic forms, [iw]/[iy].
PROSE_FORM = re.compile(r"(?<![\w’/*\]])(?:/[^/\s]+ \+ [^/\s]+/|/[^/\s]+/|\[[^\]\s]+\](?:/\[[^\]\s]+\])?"
                        r"|\*[^\s,;.:)(’]+|[^\s,;.:()‘’“”]+)")
# The language of a prose form whose section names it where the text right ahead of the form
# names another or none: §1.2.1 and §1.4 are Nez Perce lengthening and epenthesis, §1.6 Nez Perce
# breaking, §2 Nez Perce stress, and the suffixes of §3.5 Nez Perce morphemes. A form the text
# gives to both languages is under Sahaptian, or under each where it names both.
PROSE_LANGUAGE = {"/VʔV/": NP, "/VhV/": NP, "/i/": NP, "/y/": NP, "[y]": NP, "/uu/": NP, "[ee]": NP, "[aa]": NP,
                  "/aa/": S, "/ii/": "Northwest Sahaptin", "VʔV": "Sahaptian", "/x̣´x̣aac/": NP, "x̣éx̣es": NP,
                  "/x̣´x̣es/": NP, "/héw̓yn/": NP, "hehéw̓iine": NP, "/n/": "Sahaptian", "/l/": "Sahaptian", "[a]": NP, "[e]": NP, "[u]": NP, "[o]": NP,
                  "[iw]/[iy]": NP, "[aw]/[ay]": NP, "/w/": NP, "yik̓íwn": NP, "t̓áwn": NP, "/ʔ + n/": NP, "/s/": NP,
                  "C_C+CV": NP, "C_+CV": NP, "m̓": "Sahaptian", "n̓": "Sahaptian", "l̓": "Sahaptian", "w̓": "Sahaptian",
                  "y̓": "Sahaptian", "č": "Sahaptian", "/x̣/": S, "[x]": "Sahaptian", "[x̣]": "Sahaptian",
                  "/k/": [NP, S], "/q/": [NP, S], "[uy]": NP, "[oy]": NP, "/C + ʔ/": "Sahaptian",
                  "/R + ʔ/": "Sahaptian", "/-ʔ/": NP, "/-ʔew/": NP, "/-ʔis/": NP, "/-n/": NP, "/-ʔál/": NP, "/-ʔes/": NP,
                  "/-iʔins/": NP, "-íin": NP, "-íis-": NP, "-in̓": NP, "-iʔs-": NP, "/ʔ/": NP,
                  "-aʔ": "Northeast Sahaptin", "ʔ": "Northeast Sahaptin"}
# Upright forms in plain letters, which the reader takes for English: page, form, language.
PLAIN_FORMS = {(8, "-puu"): NP, (8, "-pam"): "Northern Sahaptin", (8, "m"): "Sahaptian", (8, "w"): "Sahaptian",
               (14, "c"): "Sahaptian", (paper.page(paper.find(r"sequence VʔV or VhV:$")), "VhV"): "Sahaptian"}
# The prose's one italic form.
ITALIC = {"laymíwt"}


def is_form(word):
    if word.startswith(("/", "[", "*")) and len(word) > 1:
        return True
    return any(one not in PLAIN for one in unicodedata.normalize("NFD", word))


def my_cited(where, body, pages, skip=()):
    """A cited form row for each upright form of body: one between slashes or square brackets, a
    starred one, or a word the English alphabet cannot define, under the language named nearest
    ahead of it, or under each of two the text joins with &, and a starred form under
    Proto-Sahaptian, with the quoted gloss right after it."""
    for found in PROSE_FORM.finditer(body):
        word = found.group(0).rstrip("’")
        page = pages[0]
        plain = next((PLAIN_FORMS[one, word] for one in pages if (one, word) in PLAIN_FORMS), None)
        if not plain and (not is_form(word) or word in skip):
            continue
        if word in paper.cited_done:
            continue
        paper.cited_done.add(word)
        before = body[max(0, found.start() - 90):found.start()]
        ahead = list(NAMED_AT.finditer(before))
        if plain or word in PROSE_LANGUAGE:
            languages = plain or PROSE_LANGUAGE[word]
        elif word.startswith("*"):
            languages = PS
        elif ahead:
            languages = [NAMED[ahead[-1].group(1)]]
            joined = re.search(r"\b(%s) & $" % "|".join(TAGS), before[:ahead[-1].start()])
            if joined and ahead[-1].start() == len(before) - len(ahead[-1].group(1)) - 1:
                languages.insert(0, TAGS[joined.group(1)])
        else:
            languages = S
        kind = "phonemic" if word.startswith("/") else "cited form"
        gloss = re.match(r"\s*(‘(?:[^’]|’(?=[^\W\d_]))+’)", body[found.end():])
        for language in [languages] if isinstance(languages, str) else languages:
            paper.add(where, language, kind, word, "page %d, set %s%s%s" % (
                page, "in italics" if word in ITALIC else "upright", ", " + gloss.group(1) if gloss else "",
                strong(page, word)))


paper.cited = my_cited

BLOCKS = {}
for number in range(1, paper.last + 1):
    if printed(number) and gen.EXAMPLE.match(paper.text(number)) and number not in FOOT:
        BLOCKS[number] = example
BLOCKS.update({paper.find(r"^Figure 1\. "): figure_1, paper.find(r"^Proto-Sahaptian \*i and \*u survive"): table_1,
               paper.find(r"^Table 2\. "): table_2, paper.find(r"^Table 3\. "): table_3,
               paper.find(r"^Table 4\. "): table_4, paper.find(r"^Table 5\. Vowel$"): table_5,
               paper.find(r"^Table 6\. "): table_6, paper.find(r"^Table 7\. "): table_7,
               paper.find(r"^Table 8\. "): table_8, paper.find(r"^Table 9\. "): table_9,
               paper.find(r"^\(133\) "): table_10, paper.find(r"^Table 11\. "): table_11})
assert None not in BLOCKS, BLOCKS
paper.standard(AUTHORS, NAMES, LANGUAGES, headings=HEADINGS, blocks=BLOCKS, appendix=r"^Noel Rude$")


def moved(at, write):
    """Put the rows write adds at index at."""
    count = len(paper.rows)
    write()
    added = paper.rows[count:]
    del paper.rows[count:]
    paper.rows[at:at] = added


# The front reader takes page 1 to the first heading on page 2 a line at a time: the abstract under
# the affiliation with no label, Figure 1 and note 1 over again. The abstract is one note, its
# upright *k cited, and Figure 1 is read as a figure.
first_line = paper.find(r"^Sahaptin and the mutually unintelligible")
last_line = paper.find(r"^examples labeled Sahaptin \(S\) are from Umatilla")
front_rows = [at for at, row in enumerate(paper.rows) if row[0] == "front" and row[4] == "page 1, under Noel Rude"]
opened = next(at for at in front_rows if paper.rows[at][3].startswith("Sahaptin and the mutually"))
closed = next(at for at in front_rows if paper.rows[at][3].startswith("examples labeled Sahaptin (S)"))
del paper.rows[opened:closed + 1]
abstract = paper.joined(range(first_line, last_line + 1))
moved(opened, lambda: (paper.add("front", A, "note", abstract, "page 1, the abstract, set under the affiliation "
                                                              "with no label"),
                       my_cited("front", abstract, [1]), paper.mentions("front", abstract, NAMES, "name"),
                       paper.mentions("front", abstract, LANGUAGES, "language")))
figure = [at for at, row in enumerate(paper.rows) if row[0] == "front" and
          (row[4] == "page 1, under Noel Rude" and at > opened + 1 or row[4] == "page 1, the abstract")]
at = figure[0]
for one in reversed(figure):
    del paper.rows[one]
moved(at, lambda: figure_1(paper.find(r"^Figure 1\. "), "front"))
# The reference reader takes the last indented line of Rigsby and Rude 1996 and of Rude 2000 for a
# reference of its own; each joins the one before it.
for line in ("Washington, D.C.: Smithsonian Institution.", "Hermosillo, Sonora, México: Editorial UniSon."):
    at = next(at for at, row in enumerate(paper.rows) if row[2] == "reference" and row[3] == line)
    paper.rows[at - 1][3] += " " + line
    del paper.rows[at]
# The e-mail address is printed closed up; the text layer spaces the @.
tail = paper.find(r"^Noel Rude$", paper.find(r"^References$"))
paper.add("end", A, "note", paper.joined([tail, after(tail)]).replace(" @ ", "@"),
          "page %d, the author's name and e-mail address" % paper.page(tail))
paper.write()
