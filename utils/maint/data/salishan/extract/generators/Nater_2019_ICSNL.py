"""The ops of Nater_2019_ICSNL: Hank Nater's Old records of three contiguous Pacific Northwest
languages, the Carrier, Shuswap and Bella Coola words Alexander Mackenzie wrote down in 1793, read
against current forms: drag chain sound shifts in Carrier, a Shuswap dialect since lost, and a Bella
Coola lexicon little changed.

Tables 1 and 3 set two entries to a row, each a gloss in quotes, Mackenzie's definition and the
current or reconstructed forms in parentheses after it. Table 4 sets two to a row as Mackenzie's
definition, = and the Bella Coola form with its gloss. Table 5 gives Mackenzie's and Harmon's
definitions of three Carrier words with the forms they stand for, and Table 6 Harmon's definitions
beside the Southern and Central Carrier forms. Mackenzie's and Harmon's definitions are cited forms,
each with the word it records named in its gloss. Table 2 is a diagram of the shifts, the stages a
row each with arrows between them, and Figure 1 a tree of the Salish branches.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "Bella Coola"
AUTHORS = ["Hank Nater"]
paper = gen.Paper("Nater_2019_ICSNL", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Alexander Mackenzie", "the Scottish explorer whose journal of 1793 lists the words"),
         ("Mackenzie", "Alexander Mackenzie, Voyages from Montreal (1793, published 1801 and 1911)"),
         ("Daniel Harmon", "whose journal of 1820 lists Carrier words"),
         ("Harmon", "Daniel W. Harmon, A journal of voyages and travels (1820)"),
         ("Antoine", "F. Antoine et al., the Carrier bilingual dictionary (1974)"),
         ("Kari", "James Kari, the Ahtna Athabaskan dictionary (1990)"),
         ("Krauss", "Michael E. Krauss, Na-Dene (1976), and with Jeff Leer, Athabaskan, Eyak and Tlingit "
          "sonorants (1981)"),
         ("Kraus", "Michael E. Krauss, spelled Kraus in footnote 4"),
         ("Leer", "Jeff Leer, recent advances in AET comparison (2008)"),
         ("Kuipers", "Aert H. Kuipers, the Shuswap language (1974) and the Salish etymological dictionary (2002)"),
         ("Morice", "A. G. Morice, the Carrier language (1932)"),
         ("Lincoln & Rath", "Neville J. Lincoln and John Rath, the North Wakashan comparative root list (1980)"),
         ("Powell & Woodruff", "J. V. Powell and Fred Woodruff, Sr., the Quileute dictionary (1976)"),
         ("Seymour", "Deni J. Seymour, gateways for Athabascan migration (2012)"),
         ("Van Eijk", "Jan P. van Eijk, the Lillooet language (1997) and the Lillooet-English dictionary (2013)"),
         ("Kinkade", "M. Dale Kinkade, the Upper Chehalis dictionary (1991)"),
         ("Cook", "Eung-Do Cook, Shuswap vowels and proto-Salish (1987)"),
         ("Wilmeth", "Roscoe Wilmeth, Chilcotin archaeology (1970)"),
         ("Nater", "the author's earlier work, 1994, 2013, 2014 and 2018")]
LANGUAGES = [(LANGUAGE, "Nuxalk, the northernmost Salish language, its words from Mackenzie's list of 1793"),
             ("Nuxalk", "Bella Coola"), ("Carrier", "Dakelh, Athabascan, Mackenzie's “Nagailer”"),
             ("Dakelh", "Carrier"), ("Shuswap", "Secwepmc, Interior Salish, Mackenzie's “Atnah”"),
             ("Secwepmc", "Shuswap"), ("Salish", "the family of Bella Coola and Shuswap"),
             ("Athabascan", "the family of Carrier"), ("PA", "proto-Athabascan"),
             ("proto-Athabascan", "the reconstructed ancestor of Carrier"),
             ("Chipewyan", "Athabascan, with the same shifts"), ("Tahltan", "Athabascan, the author's field notes"),
             ("Lillooet", "Interior Salish"), ("Tillamook", "Salish"), ("proto-Salish", "*smułac ‘woman’"),
             ("proto-Interior Salish", "*nəχʷ ‘female’"), ("Interior Salish", "the branch of Shuswap"),
             ("Kwakwala", "North Wakashan"), ("Quileute", "Chimakuan"), ("North Wakashan", "a source of loans"),
             ("Tsimshianic", "a source of loans"), ("Tsamosan", "a branch of Salish"),
             ("Central Salish", "a branch of Salish"), ("Nootkan", "Wakashan"), ("Chinookan", "a source of loans"),
             ("Upper Chehalis", "Tsamosan"), ("Chilcotin", "Athabascan"), ("Eyak", "PA-Eyak"),
             ("Na-Dene", "Athabascan-Eyak-Tlingit"), ("Southern Carrier", "a Carrier dialect"),
             ("Central Carrier", "a Carrier dialect"), ("Thompson-Lillooet", "Interior Salish, the pair with /z z’/"),
             ("English", "Mackenzie's spellings")]
MARK = re.compile(r"(?<=[^\d\s])\d+(?=\s|$)")


def marked(name, entry, page):
    """An entry that carries a footnote's mark, thilisitch3 in Table 1, as a note that holds it,
    where the footnote is placed after it."""
    if MARK.search(entry):
        paper.add(name, A, "note", entry, "page %d, %s, the entry that carries footnote %s"
                  % (page, name, MARK.search(entry).group(0)))


def split_entries(text):
    """The two entries of a row of Table 1 or 3, split at the gloss that opens the second outside
    the parentheses of the first."""
    depth = 0
    for at, char in enumerate(text):
        depth += char == "("
        depth -= char == ")"
        if char == "‘" and depth == 0 and at > 0 and text[:at].rstrip()[-1:] not in ("", ","):
            return [text[:at].strip(), text[at:].strip()]
    return [text.strip()]


def parenthesized(entry, page, name, record):
    """The forms in the parentheses after a definition, each a cited form: a current form, cf. a
    related one, or a reconstruction under *."""
    for inner in re.findall(r"\(((?:[^()]|\([^()]*\))*)\)", entry):
        if inner == "?":
            paper.add(name, A, "note", "(?)", "page %d, %s, the word “%s” records is unknown" % (page, name, record))
            continue
        # A comma outside the glosses parts the forms, k’a ‘bullet’, k’az̭a ‘arrow’; a comma inside
        # one, ‘mat, mattress’, does not. A gloss closes at a ’ with no letter after it.
        parts, depth, begin = [], 0, 0
        for at, char in enumerate(inner):
            if char == "‘":
                depth += 1
            elif char == "’" and depth and not re.match(r"[^\W\d_]", inner[at + 1:at + 2]):
                depth -= 1
            elif char == "," and not depth:
                parts.append(inner[begin:at].strip())
                begin = at + 1
        parts.append(inner[begin:].strip())
        for part in parts:
            found = re.match(r"^(cf\. )?(\S+)(?: (‘.*’)(\?)?)?$", part)
            form, gloss = found.group(2), found.group(3)
            what = "a reconstruction" if form.startswith("*") else "a form compared" if found.group(1) \
                else "the form"
            paper.add(name, L, "cited form", form, "page %d, %s, %s under Mackenzie's “%s”%s%s" % (
                page, name, what, record, ", " + gloss if gloss else "", ", with a query" if found.group(4) else ""))


def word_list(start, where):
    """Table 1 or 3: the header a note, then each entry's gloss, Mackenzie's definition and the
    forms in parentheses after it."""
    name = re.match(r"^(Table \d+):", paper.text(start)).group(1)
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    paper.add(name, A, "note", paper.text(start + 1), "page %d, the table's header" % paper.page(start + 1))
    number = start + 2
    while paper.text(number).strip():
        page = paper.page(number)
        for entry in split_entries(paper.text(number)):
            marked(name, entry, page)
            found = re.match(r"^(‘[^’]+’) (\S+?)\d*(?: (\(.*))?$", entry)
            gloss, record = found.group(1), found.group(2)
            paper.add(name, L, "cited form", record, "page %d, %s, Mackenzie's spelling of %s" % (page, name, gloss))
            parenthesized(found.group(3) or "", page, name, record)
        number += 1
    return number


def diagram(start, where):
    """Table 2, the stages of the shifts from proto-Athabascan to Carrier, each row a note and each
    row of arrows between them a note; the empty boxes of the diagram are drawings."""
    name = "Table 2"
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption; a dotted empty box in a "
              "stage stands where a series has shifted out" % paper.page(start))
    number = start + 1
    while not paper.text(number).startswith("2.2 "):
        text = paper.text(number)
        what = "a row of arrows, the shifts between the stages" if not re.search(r"\w", text) else "a stage"
        paper.add(name, A, "note", text, "page %d, %s" % (paper.page(number), what))
        number += 1
    return number


def bella_coola(start, where):
    """Table 4: two entries to a row, Mackenzie's definition, his gloss where he gave one, = and the
    Bella Coola form or forms with their gloss, or a form compared with cf."""
    name = "Table 4"
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    number = start + 1
    while paper.text(number).strip():
        page = paper.page(number)
        text = paper.text(number)
        cut = re.search(r"’\d* (?=[a-z][\w-]*(?: [a-z]+)? (?:=|‘))", text)
        for entry in ([text[:cut.end()], text[cut.end():]] if cut else [text]):
            # Each entry is a note as well, its glosses in words the forms do not hold.
            mark = MARK.search(entry.strip())
            paper.add(name, A, "note", entry.strip(), "page %d, %s, an entry%s"
                      % (page, name, ", carrying footnote " + mark.group(0) if mark else ""))
            entry = re.sub(r"^(.*’)\d+$", r"\1", entry.strip())
            found = re.match(r"^(.+?)(?: (‘[^‘]+’))?(?:\d+)?(?: = (.+?) (‘.+’))?(?: \(cf\. (\S+) (‘.+’)\))?$", entry)
            record, own, forms, gloss = found.group(1), found.group(2), found.group(3), found.group(4)
            paper.add(name, L, "cited form", record, "page %d, %s, Mackenzie's spelling%s" % (
                page, name, ", his gloss " + own if own else ""))
            for form in (forms.split(", ") if forms else []):
                paper.add(name, L, "cited form", form, "page %d, %s, the Bella Coola form of Mackenzie's “%s”, %s"
                          % (page, name, record, gloss))
            if found.group(5):
                paper.add(name, L, "cited form", found.group(5), "page %d, %s, compared with Mackenzie's “%s”, %s"
                          % (page, name, record, found.group(6)))
        number += 1
    return number


def shifts(start, where):
    """Table 5: a gloss, Mackenzie's definition and the form it stands for, Harmon's and his."""
    name = "Table 5"
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    paper.add(name, A, "note", paper.text(start + 1), "page %d, the table's header" % paper.page(start + 1))
    number = start + 2
    while paper.text(number).strip():
        page = paper.page(number)
        marked(name, paper.text(number), page)
        found = re.match(r"^(‘[^’]+’) (“[^”]+”) (\S+) (“[^”]+”) (\S+?)\d*$", paper.text(number))
        gloss = found.group(1)
        for record, form, who in ((found.group(2), found.group(3), "Mackenzie"), (found.group(4), found.group(5), "Harmon")):
            paper.add(name, L, "cited form", record.strip("“”"), "page %d, %s, %s's spelling of %s" % (page, name, who, gloss))
            paper.add(name, L, "cited form", form, "page %d, %s, the form %s's %s stands for" % (page, name, who, record))
        number += 1
    return number


def dialects(start, where):
    """Table 6: a gloss, Harmon's definition, the Southern and the Central Carrier forms, a form in
    parentheses a word of another shape or meaning."""
    name = "Table 6"
    paper.add(name, A, "note", paper.text(start), "page %d, the table's caption" % paper.page(start))
    paper.add(name, A, "note", paper.text(start + 1), "page %d, the table's header" % paper.page(start + 1))
    number = start + 2
    while paper.text(number).strip():
        page = paper.page(number)
        found = re.match(r"^(‘[^’]+’) (\S+) (\S+(?: ‘[^’]+’)?) (\S+)$", paper.text(number))
        gloss, harmon, southern, central = found.groups()
        paper.add(name, L, "cited form", harmon, "page %d, %s, Harmon's spelling of %s" % (page, name, gloss))
        for form, dialect in ((southern, "Southern"), (central, "Central")):
            own = re.match(r"^(\S+) (‘.+’)$", form)
            paper.add(name, L, "cited form", own.group(1) if own else form, "page %d, %s, the %s Carrier form of %s%s"
                      % (page, name, dialect, gloss, ", " + own.group(2) if own else ""))
        number += 1
    return number


def tree(start, where):
    """Figure 1, a tree of Salish with its four branches under it, and its caption."""
    caption = paper.find(r"^Figure 1:", start)
    labels = [paper.text(one) for one in range(start, caption) if paper.text(one).strip()]
    paper.add("Figure 1", A, "note", " / ".join(labels),
              "page %d, the figure, a tree: Salish over its branches" % paper.page(start))
    paper.add("Figure 1", A, "note", paper.text(caption), "page %d, the figure's caption" % paper.page(caption))
    return caption + 1


BLOCKS = {paper.find(r"^Table 1:"): word_list, paper.find(r"^Table 2:"): diagram, paper.find(r"^Table 3:"): word_list,
          paper.find(r"^Table 4:"): bella_coola, paper.find(r"^Table 5:"): shifts, paper.find(r"^Table 6:"): dialects,
          paper.find(r"^Salish$"): tree}
# Footnote marks open lines, 1 "Nagailer" and 2 As a rule, where the heading finder takes them for
# section numbers; the headings are named here.
HEADINGS = {paper.find(r"^%s$" % re.escape(title)): label for label, title in (
    ("1", "1 Introduction"), ("2", "2 The data"), ("2.1", "2.1 The Carrier word list"),
    ("2.2", "2.2 The Shuswap word list"), ("2.3", "2.3 The Bella Coola word list"),
    ("3", "3 Remaining issues and preliminary conclusions"))}
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, headings=HEADINGS)
# The page sets PA *-ɢəŋ'₂ with the star and hyphen upright and its homophone index subscript; the
# italic reader takes the index as a plain 2 and the run no longer meets the text. It is written here,
# ahead of the -ɢəm' it equates.
at = next(index for index, row in enumerate(paper.rows) if row[2] == "cited form" and row[3] == "-ɢəm’")
paper.rows.insert(at, paper.rows[at][:3] + ["*-ɢəŋ’₂", "page 6, in italics, a Proto-Athabascan reconstruction, "
                                                       "its homophone index subscript"])
paper.write()
