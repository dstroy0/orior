"""One paper through the anchor_sift engine: its own English as the target, then alphabet, web, residue.

usage: python paper_sift.py <stem> <authors> <language>

1. Every repaired line of the text layer is sorted by english_sift.sorted_into against the English
   reference and the pure corpus. The lines it calls English are this paper's English.
2. That English is the target. Its byte pairs become the English anchor and its words the English
   vocabulary, and every line is sorted again against it. What the paper's own English does not
   account for is the language, a gloss tier, or residue.
3. The alphabet is every character that sits mostly outside the paper's English: letters of the
   language on one side and gloss-label small capitals on the other.
4. The word web is built with word_web.web() over the language tokens, each carrying the English
   translation or quoted gloss beside it as its concept, the example it sits in as its context.
5. The draft table is written: English lines grouped as notes, language and gloss lines one row each,
   and every language token outside an example block as a cited form candidate. Tokens that are
   neither the paper's English nor carry the alphabet, in lines the engine would not call English,
   are listed as residue for a person to read against the page.

Writes <stem>.draft.tsv, <stem>.alphabet.txt, <stem>.web.tsv and <stem>.residue.tsv to workdir.WORK.
"""
import collections
import difflib
import os
import re
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import SALISHAN as LIBRARY  # noqa: E402
from workdir import INSTRUMENT  # noqa: E402
sys.path.insert(0, HERE)
sys.path.insert(0, INSTRUMENT)
sys.path.insert(0, os.path.join(LIBRARY, "word_web"))

import residue  # noqa: E402
from english_sift import (CORPORA, counts_of, english_reference, language_reference,  # noqa: E402
                          looks_like_writing, sorted_into)
from paper_config import INSERTED_SPACE  # noqa: E402
from repairs import composed, corrected, sequence  # noqa: E402
from word_web import web  # noqa: E402
import defects  # noqa: E402
from workdir import WORK  # noqa: E402

PAGE = re.compile(r"^===== page (\d+) =====$")
EXAMPLE = re.compile(r"^\((\d+|[ivx]+)\)\s*(.*)$")
HEADING = re.compile(r"^(\d+(?:\.\d+)*)\.?\s+((?:\(|[^\W\d_])\S*.*)$")
QUOTED = re.compile(r"‘([^’]+)’")
EDGES = "‘’“”\"'()[]{},.;:!?*"
WORD = re.compile(r"[A-Za-z]+")
CAPS_LABEL = re.compile(r"[A-Z]{2,}|\b[123](?:SG|PL|DU|sg|pl)\b|(?:^|[-=.])[123]?[A-Z]{2,}")
# The judgement mark can sit against the letter, b.*xʷúy̓ in Steiner and Matthewson's (7).
SUB = re.compile(r"^([a-mA-D])\.(?:(?:\s+|(?=[*#?]))(.*))?$")
# The cells of a comparative table row: ⟨definition⟩, {romanized shorthand}, [*reconstruction] or
# [phonetic], /phonemic/, ‘gloss’, and a parenthetical. What sits between cells is a bare form, a
# language label such as Th, or a dash for an empty column.
# An infix inside a word, t̓<ʔ>ʕas in Lyon's tables, is part of the form: a cell in angle brackets
# opens after a space.
# A gloss runs to the quote that no letter follows: ‘herbalist’s song’ is one cell, and a gloss in
# straight quotes, 'idem', stands between spaces. A parenthesis is a cell where it stands apart, and
# the (ə) of kw(ə)l is part of the form.
CELL = re.compile(r"(?<!\S)<[^<>]*>|⟨[^⟩]*⟩|\{[^}]*\}|(?<![^\W\d_])\[[^\]]*\]|\[[^\]]*\](?![^\W\d_/])|/[^/‘’“”]+/"
                  r"|[‘“](?:[^’”]|[’”](?=[^\W\d_]))*[’”]|(?<!\S)'[^']+'(?!\S)"
                  r"|(?<!\S)\((?:[^()]|\([^()]*\))*\)(?![^\W\d_•])")
# A cell stands between spaces. The brackets inside qʷincút-m[in]-[t]-Ø-[e]ne mark sounds of a
# segmentation, and a line of those is a tier.
NOTATION_CELL = re.compile(r"(?<![^\s(])(?:<[^<>]*>|⟨[^⟩]*⟩|\{[^}]*\}|\[[^\]]*\]|/[^/‘’“”\s][^/‘’“”]*/)(?=[\s,;.\d)’]|$)")
TABLE = re.compile(r"^(Table \d+):\s*(.*)$")
LABEL = re.compile(r"^[A-Z][a-zA-Z]$")
STAMP = re.compile(r"\s*\[(\d{1,2}:\d{2}(?::\d{2})?)\]\s*$")
STAMP_ONLY = re.compile(r"^\s*\[(\d{1,2}:\d{2}(?::\d{2})?)\]\s*$")
# What opens a display the authors analyze with and nobody said: a rule, a tree, a derivation.
# A denotation, ⟦√ɬʕat̓⟧ = m = λdλxλs.[wet(x,s)] ≥ d, is a display of the author's, and so is a
# lambda term standing alone, a. λsλe.[BECOME(e,s) ∧ P(x,s)]. A reading written with a lambda
# inside a sentence, Only Peter λx[x claimed x is the winner] in Davis, is no denotation.
DENOTATION = re.compile(r"⟦|^(?:[a-j]\.\s*)?λ")
# A denotation goes on over a line that closes its bracket, CS⟧ = inch = λg ∈ DmΔ ..., or carries
# its value, = λt.∃e∃s.cutΔ(x,e,s) = max(cutΔ).
DENOTATION_GOES_ON = re.compile(r"⟧|=\s*λ|^[↑=]")
DISPLAY = re.compile(r"[⇔→⟶]|\b[A-Z][a-z]*P\b|^\s*√\s*$|\]\s*ω|^\[")
# Labels an author sets before an English reading of an example instead of speakers.
TRANSLATION_LABELS = ("Target", "Intended", "Actual", "Literally", "Lit", "Literal", "Gloss")
# A speaker's own comment after an example: Dave Michel: ... or DD Comment: ...
# A comment can name the example it is on, Comment on (9)A3: or Comments on versions of (6): in Hill
# and Matthewson, and a heading like that one opens a list of quotes on the lines below.
SPEAKER = re.compile(r"^([A-Z][A-Za-z’'.]+(?: [A-Z][A-Za-z’'.-]+){0,3})"
                     r"(?: [Cc]omment(?: for [Cc]ontext \w+| on \([a-j]\))?|’s question"
                     r"|(?<=Comment|omments) (?:on|about) (?:versions of )?\(\d+[a-z]?\)(?:[a-z]|A\d)?)?"
                     r":(?:\s+(.*))?$")
# A line of a list of words: the form, the root after < where the list gives one, and the gloss in
# quotes, c̓ál̓<c̓al̓>-t < √c̓al̓ ‘shady’ or maʕ̓ʷ-t ‘break’. A footnote mark can follow the gloss.
WORD_ROW = re.compile(r"^([^‘\s](?:[^‘]{0,40}?[^‘\s])?)\s+(?:<\s*(√[^‘]+?)\s+)?(‘[^‘]*?)\s*\d{0,2}$")

# How many times the paper's own English counts against one line of the general reference.
WEIGHT = 20


_ORDINARY = []


def ordinary_english():
    """Every word of cc_english.txt, the ordinary English the engine's English reference reads."""
    if not _ORDINARY:
        with open(os.path.join(CORPORA, "cc_english.txt"), encoding="utf-8", errors="replace") as handle:
            _ORDINARY.append(set(WORD.findall(handle.read().lower())))
    return _ORDINARY[0]


def same_letters(one, other, ratio=0.8):
    """True where two lines define nearly the same letters once marks, breaks and spaces are set aside.

    A segmentation can write an allomorph its tier does not, tue squamish over tw=e=squamish, and
    four letters in five alike is enough.
    """
    def letters(text):
        return "".join(symbol for symbol in unicodedata.normalize("NFD", text.lower())
                       if symbol.isalpha())
    first, second = letters(one), letters(other)
    return len(first) >= 2 and difflib.SequenceMatcher(None, first, second).ratio() >= ratio


def label_char(symbol):
    name = unicodedata.name(symbol, "")
    return ("SMALL CAPITAL" in name) or name.startswith("MATHEMATICAL")


def main():
    stem, authors, language = sys.argv[1], sys.argv[2], sys.argv[3]
    repair = residue.paper_repair(stem)
    with open(residue.source_path(stem), encoding="utf-8") as handle:
        raw_lines = handle.read().split("\n")

    lines = []
    page = 0
    for number, raw in enumerate(raw_lines, 1):
        text = unicodedata.normalize("NFC", repair(raw.rstrip())).strip()
        found = PAGE.match(text)
        if found:
            page = int(found.group(1))
            continue
        # The printed page number on a line of its own is the running foot instead of text. Kept, it
        # ended every example that crossed a page.
        if text and not re.match(r"^\d{1,4}$", text):
            lines.append((number, page, text))

    # 1. The engine, with the general anchors, finds the paper's English.
    english = english_reference()
    pure = language_reference()
    first = {number: sorted_into(text, english, pure) for number, page, text in lines}
    paper_english = [text for number, page, text in lines if first[number] == "english"]

    # 2. The paper's English is the target now. Alone it is too small an anchor: surprise() smooths
    # over all 2^16 cells, and a 30 kB reference charges an unseen pair little more than a seen one,
    # which sorted the gloss tiers as English. It is laid over the general reference instead,
    # weighted so the paper's own words and typography dominate.
    own, own_total = counts_of(paper_english)
    merged = dict(english[0])
    for pair, seen in own.items():
        merged[pair] = merged.get(pair, 0) + WEIGHT * seen
    target = (merged, english[1] + WEIGHT * own_total, english[2] + len(paper_english))
    vocabulary = collections.Counter(one.lower() for text in paper_english
                                     for one in WORD.findall(text))
    second = {number: (sorted_into(text, target, pure) if looks_like_writing(text) else "residue")
              for number, page, text in lines}

    # 3. The alphabet: characters mostly outside the paper's English.
    inside = collections.Counter()
    outside = collections.Counter()
    for number, page, text in lines:
        bucket = inside if second[number] == "english" else outside
        for symbol in text:
            # ∅ is the null morpheme several authors write inside a segmentation.
            if not symbol.isascii() and (symbol.isalpha() or unicodedata.combining(symbol)
                                         or unicodedata.category(symbol) in ("Lm", "Sk")
                                         or symbol == "∅"):
                bucket[symbol] += 1
    letters = []
    labels = []
    for symbol in sorted(set(inside) | set(outside)):
        share = outside[symbol] / float(outside[symbol] + inside[symbol])
        (labels if label_char(symbol) else letters).append((symbol, outside[symbol],
                                                            inside[symbol], share))
    alphabet = "".join(one[0] for one in letters)
    label_set = set(one[0] for one in labels)

    def token_kind(token):
        plain = token.strip(EDGES)
        if not plain or not any(one.isalpha() for one in plain):
            return "", plain
        marked = [one for one in plain if not one.isascii()]
        if marked and sum(1 for one in marked if one in label_set) * 2 >= len(marked):
            return "label", plain
        if any(one in alphabet for one in plain):
            return "language", plain
        # A gloss label in plain capitals: PFV, 1SG.ERG, N.PROS-PFV-eat-2SG.OBJ.
        if CAPS_LABEL.search(plain):
            return "label", plain
        if all(one.lower() in vocabulary for one in WORD.findall(plain)):
            return "english", plain
        return "unknown", plain

    def cell_where(example):
        label = example[0] if example[0].startswith("Table") else "(%s)" % example[0]
        return "%s line %d" % (label, example[1])

    def is_language_name(text):
        # A column head naming a language the paper compares with, Dakelh, is one capitalized
        # word, and English or Gloss heads the column of glosses.
        return text == language or any(one in alphabet for one in text) or (bool(
            re.fullmatch(r"[A-Z][a-z]{3,}", text)) and text not in (
                "English", "Gloss", "Glosses", "Meaning", "Translation", "Source", "Notes"))

    def language_head(text):
        """The language an example's heading names before its colon, or None.

        Davis and Nederveen head each example with the language it is from, (13) nɬeʔkempxcín:
        t-marked COS verbs are unaccusative. The name carries a letter of the alphabet, which keeps
        Intended: and Context: out.
        """
        named = re.match(r"^([^\W\d_][^\s:‘“]*):\s+[^‘“'\s]", text)
        if named and (named.group(1) == language or any(one in alphabet for one in named.group(1))):
            return named.group(1)
        return None

    def list_resumes(at_line):
        """True where the first line of the next page is a word of a list, kʷax̌ʷ-t ‘wake’ at the
        top of Davis and Nederveen's page 8, the rest of (4a)."""
        page = lines[at_line][1]
        for one in lines[at_line + 1:]:
            if one[1] == page or re.fullmatch(r"\d{1,3}", one[2].strip()):
                continue
            return bool(WORD_ROW.match(one[2].strip()))
        return False

    CELL_KINDS = (("<", "transcription", "in < >"), ("⟨", "transcription", "in ⟨ ⟩"),("{", "transcription", "in { }"),
                  ("[*", "phonemic", "reconstructed, in [* ]"), ("[", "phonemic", "phonetic, in [ ]"),
                  ("/", "phonemic", "phonemic, in / /"))

    def emit_cells(example, body, page, verdict):
        """One line of a comparative table as a row per cell. False where the table has ended.

        example[4] holds the column heads, each a language name or None, and example[5] the first
        half of a row whose ⟨ or { closes on the next line.
        """
        if example[5]:
            # The wrapped half closes the cell it left open: <shna-hle ‘a lazy woman’ over
            # t sek-ha> is <shna-hle t sek-ha> and then the rest of the row.
            stash = example[5]
            example[5] = ""
            # The opener left open instead of the last one: <rapentle’he ‘a lazy man’ closes its gloss.
            pairs = {"<": ">", "⟨": "⟩", "{": "}", "(": ")"}
            unclosed = [stash.rfind(one) for one, shut in pairs.items()
                        if stash.count(one) > stash.count(shut)]
            quote = re.search(r"(?<!\S)‘(?:[^’]|’(?=[^\W\d_]))*$", stash)
            if quote:
                unclosed.append(quote.start())
            opens = max(unclosed) if unclosed else max(stash.rfind(one) for one in "<⟨{(‘")
            closer = dict(pairs, **{"‘": "’"})[stash[opens]]
            end = re.search(r"\s{2,}|\s[‘“]", stash[opens:])
            end = opens + end.start() if end else len(stash)
            close = body.find(closer)
            if close >= 0:
                body = "%s %s%s %s" % (stash[:end], body[:close + 1], stash[end:], body[close + 1:])
            else:
                body = stash + " " + body
        # A parenthesis wraps too, thu sta'lusthulh (the / former wife), where a row of the table
        # is under way.
        if body.count("⟨") > body.count("⟩") or body.count("{") > body.count("}") or \
                body.count("<") > body.count(">") or \
                (example[3] and body.count("(") > body.count(")")) or \
                (example[3] and re.search(r"(?<!\S)‘(?:[^’]|’(?=[^\W\d_]))*$", body)):
            example[5] = body
            return True
        # A line of a list of morphemes: cf. (i) /tə̣́mɬ ‘red ochre’, then or, then the next.
        body = re.sub(r"^cf\.\s*", "", body)
        if body.strip() == "or":
            return True
        # A row can stand for two lines of the example, (c) & (f) √lheel hun'lheelt.
        label = re.match(r"^\(([a-n]|[ivx]+)\)(?:\s*&\s*\([a-n]\))*\s*", body)
        if label:
            body = body[label.end():]
            letter = label.group(1)
            # (i) after (h) is a letter. Under a letter it numbers the morphemes of one form.
            # A table can name one line twice, (a) √q'ay then (a) √q'ay, or come back to it after
            # another, and its rows count on under that letter.
            if len(letter) == 1 and (letter != "i" or example[6] == "h") and letter != example[6]:
                if len(example) == 7:
                    example.append({})
                if example[6]:
                    example[7][example[6]] = example[1]
                example[0] = re.sub(r"[a-n]$", "", example[0]) + letter
                example[1] = example[7].get(letter, 0)
                example[6] = letter
            # The label opens a row, and the row can go on from the next line, (a) over
            # √ts'ew ts'uwuthamu c̓əw-əθamə.
            example[3] = True
            if not body:
                if len(example) == 7:
                    example.append({})
                example[7]["open"] = True
                return True
        row_below_label = len(example) > 7 and example[7].pop("open", False)
        # The morphemes of a row of / /, [ ] and its translation, on the line under it with the
        # source at the right after a wide gap: turn-CTR-TR-3ERG   (T&T 1996:321), or
        # STAT-separate-foot   BP in Hall et al.
        # A source with its year needs no wide gap, STAT-use~COS (Hall & Phillips this volume).
        # The form the example is not can sit between them, fast-IMM   (not [x̣ʷə.nə́t])   (Hall
        # & Phillips this volume).
        aside = re.search(r"\s+(\(not [^()]*\))(?=\s{2,}|\s*$)", body)
        core = (body[:aside.start()] + body[aside.end():] if aside else body).strip()
        under = re.match(r"^(\S+(?: \S+){0,5}?)(?:\s{2,}(\([^()]*\)|[A-Z]{2,4}(?:, [A-Z]{2,4})*)"
                         r"|\s(\([^()]*(?:\d{4}|this volume)[^()]*\)))?$", core)
        if under and under.group(3):
            under = re.match(r"^(.*?)\s(\(.*\))$", core)
        row_above = example[3] and rows and rows[-1][2] == "translation" and \
            rows[-1][0] == cell_where(example)
        # Every piece between the boundaries is a gloss label or an English word; a row of forms,
        # √kwun kwunnuhwut kʷən-nəxʷ-ət in Schneider-Gerdts, is not.
        # A plain word the paper's English does not hold is a gloss word too, sleep<DIM>.
        glossed = under and all(
            token_kind(piece)[0] in ("english", "label") or not re.search(r"[^\W\d_]", piece) or
            re.fullmatch(r"[a-z]+", piece)
            for piece in re.split(r"[\s\-=.<>~{}+/]+", under.group(1)) if piece) and \
            "√" not in under.group(1)
        if row_above and glossed and not re.search(r"(?<!\S)[/\[‘“]", under.group(1)) and \
                (under.group(2) or any(one in under.group(1) for one in "-=.<~")):
            example[1] += 1
            where = cell_where(example)
            rows.append((where, language, "gloss", under.group(1),
                         "page %d, the morphemes of the row above" % page))
            if aside:
                rows.append((where, authors, "note", aside.group(1),
                             "page %d, the surface form the example does not have" % page))
            if under.group(2):
                rows.append((where, authors, "citation", under.group(2),
                             "page %d, the source of the row above" % page))
            return True
        words = body.split()
        kinds = [token_kind(one)[0] for one in words]
        # A dash for an empty cell, alone on its line.
        if example[3] and not any(one.isalpha() for one in body):
            return True
        # A line opening with its row label, (a) kwunutus kʷən-ət-əs, is a row whatever it holds.
        if label:
            example[3] = True
        if not label and not row_below_label and not NOTATION_CELL.search(body) and \
                not re.search(r"[‘“]|(?<!\S)'[^']+'(?!\S)", body) and \
                not (example[3] and re.search(r"(?<!\S)\([^()]+\)(?!\S)", body)):
            if not example[3] and len(words) <= 8 and not body.endswith("."):
                example[1] += 1
                rows.append((cell_where(example), authors, "note", body,
                             "page %d, heads the rows below" % page))
                # A head can carry a footnote mark, Dakelh10.
                heads = [re.sub(r"(?<=[^\W\d_])\d{1,2}$", "", one) for one in re.split(r"\s{2,}", body)]
                example[4] = [one if is_language_name(one) else None for one in heads]
                return True
            # The morpheme gloss under a row, LOC-look-house, or the end of a row wrapped onto a
            # line of its own, qw.
            if example[3] and len(words) <= 8 and "label" in kinds and \
                    kinds.count("label") >= kinds.count("english"):
                example[1] += 1
                rows.append((cell_where(example), language, "gloss", body,
                             "page %d, engine %s" % (page, verdict)))
                return True
            # A gloss of plain words, call, above the translation of its row.
            if example[3] and len(words) <= 3 and at_line + 1 < len(lines) and \
                    re.match(r"^[‘“]", lines[at_line + 1][2]):
                example[1] += 1
                rows.append((cell_where(example), language, "gloss", body,
                             "page %d, engine %s" % (page, verdict)))
                return True
            if example[3] and len(words) <= 3 and "english" not in kinds:
                rows.append((cell_where(example), language, "transcription", body,
                             "page %d, the end of the row above, set on its own line" % page))
                return True
            return False
        # A sentence of prose that happens to cite two forms is not a row.
        outside = CELL.sub(" ", body)
        if body.rstrip().endswith(".") or sum(
                1 for one in outside.split() if re.fullmatch(r"[A-Za-z’']{3,}[,.;:]?", one)
                and token_kind(one)[0] == "english") > 3:
            return False
        example[1] += 1
        example[3] = True
        where = cell_where(example)
        # The items of the row, each with the text before it: cells, and the tokens between them.
        items = []
        at = 0
        for found in CELL.finditer(body):
            for gap in re.finditer(r"(\s*)(\S+)", body[at:found.start()]):
                items.append((gap.group(1), gap.group(2), None))
            # The space just before the cell, or the whole gap where it is only punctuation, and
            # /xéʔ(e)/, /xʔe/ keeps its comma.
            gap = body[at:found.start()]
            separator = gap if not gap.strip(" ,;") else gap[len(gap.rstrip()):]
            after = re.match(r"(\d{1,2})(?=[\s,;]|$)", body[found.end():])
            items.append((separator, found.group(0), after.group(1) if after else None))
            at = found.end() + (len(after.group(1)) if after else 0)
        for gap in re.finditer(r"(\s*)(\S+)", body[at:]):
            items.append((gap.group(1), gap.group(2), None))

        column = 0
        label = None
        previous = None
        run = []

        def who_now():
            if label:
                return label
            if example[4] and column < len(example[4]) and example[4][column]:
                return example[4][column]
            return language

        def place(kind, separator):
            nonlocal column
            wide = bool(re.search(r"\s{2,}", separator))
            # A form after the English column opens the next column however close it is set:
            # ‘strap or band for packing’ tl’oolh12.
            after_english = previous == "‘" and kind != "‘" and example[4] and \
                column < len(example[4]) and example[4][column] is None
            if previous is not None and (wide or after_english or (kind == "/" and previous == "/"
                                                                   and "," not in separator)):
                column += 1

        def close_run():
            if not run:
                return
            # A practical orthography and a phonemic column one space apart, q’aytum q̓ay-t-əm, are
            # two cells: the run breaks where its words start carrying letters of the phonemic
            # alphabet. An accented vowel, í of tsedutní, is not one of those.
            def phonemic(token):
                return any(one in alphabet and not unicodedata.normalize("NFD", one)[0].isascii()
                           for one in token)
            # A root one space before its form, √tth'e' tth'utth'e't, is a cell of its own.
            if len(run) > 1 and run[0].startswith("√"):
                tail = run[1:]
                del run[1:]
                close_run()
                run.extend(tail)
            marked = [phonemic(one) for one in run]
            if len(run) > 1 and not marked[0] and any(marked):
                turn = marked.index(True)
                if all(marked[turn:]):
                    tail = run[turn:]
                    del run[turn:]
                    close_run()
                    run.extend(tail)
            text = re.sub(r"^cf\.\s*", "", " ".join(one for one in run).strip(" ,;"))
            del run[:]
            if not text or not any(one.isalpha() for one in text):
                return
            # A run is prose where two of its words are English, / doubling of vowel 7, or one long
            # one, perhaps ~. Anything else between the cells is a form: ren púp̓smen, -éc ~ -c,
            # ɣ : x, and ll or kw.
            english_words = [one for one in text.split() if re.fullmatch(r"[A-Za-z]{2,}", one)
                             and token_kind(one)[0] == "english"]
            # The run after a root in its row is the form built on it, √thuy thuytus.
            is_form = text.startswith("√") or (rows and rows[-1][2] == "root" and rows[-1][0] == where) \
                or (len(english_words) < 2 and not any(len(one) >= 5 for one in english_words))
            kind = "note" if not is_form else ("root" if text.startswith("√") else "transcription")
            rows.append((where, who_now() if is_form else authors, kind, text,
                         "page %d, %s" % (page, "the orthography" if is_form else "between the cells")))

        for separator, text, note in items:
            cell = CELL.fullmatch(text) if text[0] in "<⟨{[/‘“('" else None
            if cell is None:
                plain = text.strip(",;")
                if LABEL.match(plain) and not any(one in alphabet for one in plain):
                    close_run()
                    label = plain
                    continue
                if plain in ("—", "–", "-"):
                    close_run()
                    place("—", separator)
                    previous = "—"
                    continue
                if run and re.search(r"\s{2,}", separator):
                    close_run()
                if not run:
                    if not any(one.isalpha() for one in plain):
                        continue
                    place("bare", separator)
                    previous = "bare"
                run.append(text)
                continue
            close_run()
            mark = ", carries footnote %s, written here without its digit" % note if note else ""
            if text[0] in "‘“'":
                # A column of English between columns of forms, Nicola  English  Dakelh, counts.
                # A gloss one space after its form still fills the English column.
                if previous not in (None, "‘") and example[4] and column + 1 < len(example[4]) and \
                        example[4][column + 1] is None:
                    column += 1
                else:
                    place("‘", separator)
                previous = "‘"
                rows.append((where, authors, "translation", text, "page %d%s" % (page, mark)))
                continue
            if text[0] == "(":
                rows.append((where, authors, "note", text, "page %d%s" % (page, mark)))
                continue
            for opens, kind, described in CELL_KINDS:
                if text.startswith(opens):
                    place(opens[0], separator)
                    previous = opens[0]
                    rows.append((where, who_now(), kind, text,
                                 "page %d, %s%s" % (page, described, mark)))
                    web_rows.append([where, who_now(), kind, text, ""])
                    break
        close_run()
        return True

    # 4 and 5. Rows, the web and the residue.
    rows = []
    # The language of each list of words, by its label.
    word_lists = {}
    # The language each example number's heading names, by the number: (10) Secwepemctsín: over
    # (10a) and (10b).
    example_languages = {}
    web_rows = []
    leftover = []
    section = "front"
    example = None
    paragraph = []
    # The source line number of the last line a speaker comment took.
    comment_number = -2
    paragraph_at = (section, 0)

    def glossed_below(at_line, reach=6):
        """True where one of the next lines is shaped as a gloss tier or opens a translation.

        A language written in plain letters, Gitksan's dim hadiks ’nii’y, reads as English line by
        line, and an example of it is known by the author's format instead: a line of gloss labels,
        PROSP swim 1SG.PRON, a third of its pieces capital labels, or a line opening on a quote."""
        for one in lines[at_line + 1:at_line + 1 + reach]:
            if EXAMPLE.match(one[2]):
                return False
            if gloss_shaped(one[2]) or re.match(r"^\s*[#?*%]*[‘“]", one[2]):
                return True
        return False

    def tiers_below(at_line):
        """True where the next three lines are a sentence, its segmentation and its glosses.

        The sentence can carry its sub-letter, a. Hlaa yukw dim hlisxwi'y., and its segmentation
        define the stems apart, yugwit over yukw-@t."""
        if at_line + 3 >= len(lines):
            return False
        sentence = lines[at_line + 1][2]
        return not EXAMPLE.match(sentence) and same_letters(re.sub(r"^[a-j]\.\s+[*#?]*", "", sentence), lines[at_line + 2][2], 0.6) and \
            gloss_shaped(lines[at_line + 3][2], 4) and not gloss_shaped(sentence)

    def gloss_shaped(text, share=3):
        """A line of gloss labels, a third of its pieces capital labels: PROSP swim 1SG.PRON. Under a
        sentence and its segmentation a quarter is enough, not.exist =CN fish."""
        pieces = [piece for piece in re.split(r"[\s=.\-\[\]]+", text) if piece]
        capitals = [piece for piece in pieces if re.fullmatch(r"[0-9]*[A-Z][A-Z0-9]*", piece)]
        return bool(pieces) and len(capitals) * share >= len(pieces) and len(text) < 120 and \
            any(len(piece) >= 2 and not piece.isdigit() for piece in capitals)

    def closing_ahead(at_line):
        """True where one of the next three lines closes a translation, before a new letter."""
        for one in lines[at_line + 1:at_line + 4]:
            if SUB.match(one[2]) or EXAMPLE.match(one[2]):
                return False
            if re.search(r"[’”]\W*(?:\([^()]*\)?\S*\s*)?$", one[2]):
                return True
        return False

    def flush():
        if paragraph:
            rows.append((paragraph_at[0], authors, "note", " ".join(paragraph),
                         "page %d" % paragraph_at[1]))
            del paragraph[:]

    offered = set()
    def offer(text, body, kinds, number, page, verdict):
        """Offer the language forms of one line of prose or of a footnote as cited form candidates."""
        for raw, (kind, plain) in zip(body.split(), kinds):
            # A form of a practical orthography in plain letters, cited in the prose: a root,
            # √qw’aqw, or a word carrying ’ for the glottal stop, qw’qwuy’al’stun. An English
            # contraction, don’t or Hukari’s, is not one. Its ’ at either end is a letter,
            # √nuw’ and ’i’mush, and a parenthesis in it is part of it, √t’il(um).
            if kind != "language" and section != "references" and (
                    re.match(r"^√\S", plain) or
                    re.search(r"(?:^|[^\W\d_])’(?!(?:s|t|re|ll|ve|d|m)$)[^\W\d_]", plain)):
                kind = "language"
                plain = raw.strip("“”\"[]{},.;:!?*‘")
                if plain.count(")") > plain.count("("):
                    plain = plain.rstrip(")")
                if plain.count("(") > plain.count(")"):
                    plain = plain.lstrip("(")
                plain = plain.rstrip(",.;:!?")
            # A form with no mark at all, glossed where it stands, snuhwulhshun ‘car tire’: a word
            # in lower case the paper's English barely uses, its gloss in quotes right after it.
            # English sets means ‘mat’ and hyphen ‘-’ the same way. A word ordinary English writes,
            # in the four megabytes of it the engine's English reference is built from, is English.
            elif kind in ("unknown", "english") and section != "references" and \
                    plain.lower() not in ordinary_english() and \
                    re.fullmatch(r"[a-z][a-z.\-]{2,}", plain) and vocabulary[plain.lower()] <= 3 and \
                    re.search(r"(?<!\S)%s[,]?\s{1,2}‘" % re.escape(plain), text):
                kind = "language"
            # A form that opens on an optional segment keeps its parenthesis, (u)ʔéx or (ʔa)kɬ-.
            if kind == "language" and plain.count(")") > plain.count("(") and \
                    raw.lstrip("‘“\"[*").startswith("(") and not plain.startswith("("):
                plain = "(" + plain
            # A form that closes on an optional segment keeps its parenthesis, u(ʔ) or ˬ…k(a). An
            # example number, √a̱x̱(44), and a formula, HDA(μ) or λxλs.P(x)(s), are no segment.
            if kind == "language" and plain.count("(") > plain.count(")") and \
                    raw.rstrip("“”\"’.,;:!?*").endswith(")") and not plain.endswith(")") and \
                    re.search(r"\((?:[-/]?[^\W\d_][̀-ͯ’ʷ]*){1,3}$", plain) and \
                    not re.search(r"[Ͱ-Ͽ∃∀Δ]", plain) and \
                    not re.match(r"^[A-Za-z]{2,}\(", plain):
                plain = plain + ")"
            # And one that opens on a deleted segment keeps its bracket, [e]=sk̓ʷák̓ʷest.
            if kind == "language" and plain.count("]") > plain.count("[") and \
                    raw.lstrip("‘“\"(*").startswith("[") and not plain.startswith("["):
                plain = "[" + plain
            if kind == "language" and (section, plain) not in offered:
                offered.add((section, plain))
                following = ""
                at = text.find(plain)
                after = QUOTED.search(text, at + len(plain)) if at >= 0 else None
                if after and after.start() - (at + len(plain)) < 3:
                    following = after.group(1)
                rows.append((section, language, "cited form", plain,
                             "candidate, page %d%s" % (page, (", ‘%s’" % following)
                                                       if following else "")))
                web_rows.append([section, language, "cited form", plain, following])
            if kind == "unknown" and verdict != "english":
                leftover.append((number, page, verdict, plain, text))

    # Every text layer defect this run repairs or finds, for defects.tsv.
    fixed = []
    last_example = None
    last_letter = {}
    footnote_page = None
    footnote_row = None
    footnote_last = 0
    caption_row = None
    opened_numbers = []
    footnote_examples = []
    interrupted = None
    for at_line, (number, page, text) in enumerate(lines):
        verdict = second[number]
        if interrupted is not None and page > interrupted[1]:
            if example is not None and re.fullmatch(r"[ivx]+", example[0]):
                flush()
                example = interrupted[0]
                fixed.append((page, "example cut by a footnote's example, resumed on the next page",
                              text, "(%s) goes on" % example[0]))
            interrupted = None
        # A story told as running lines, each closed by its timestamp: xʷúy̓ qʷincútmne ... [00:04].
        # The stamp sits at the end of the line or alone on the next one. A bare stamp is a
        # locator and not text.
        if STAMP_ONLY.match(text):
            continue
        stamp = STAMP.search(text)
        following = lines[at_line + 1][2] if at_line + 1 < len(lines) else ""
        if not stamp and STAMP_ONLY.match(following):
            stamp = STAMP_ONLY.match(following)
        if example is None and stamp and verdict != "english" and section != "references":
            flush()
            spoken = STAMP.sub("", text).strip()
            rows.append(("%s [%s]" % (section, stamp.group(1)), language, "transcription", spoken,
                         "page %d, engine %s, a line of the story" % (page, verdict)))
            web_rows.append([section, language, "transcription", spoken, ""])
            continue
        if example is not None:
            last_example = example
        heading = HEADING.match(text)
        # A heading can be in the language, 3 nɬeʔkepmxcín. It is known by its number coming
        # after the section before it, which keeps a footnote or an abbreviation row out.
        if heading:
            order = tuple(int(one) for one in heading.group(1).split("."))
            current = tuple(int(one) for one in section[1:].split(".")) \
                if re.match(r"^§[\d.]+$", section) else (0,)
            # A long heading mostly in the language, 4.2.2 xʷúy̓ and ʔe stx̣ә́pe neʔ, is let through
            # when its number comes next: the next section at some depth, or the
            # first subsection of this one.
            following = {current[:depth] + (current[depth] + 1,) for depth in range(len(current))}
            following.add(current + (1,))
            if order <= current or order[0] > current[0] + 1 or \
                    (verdict != "english" and len(text.split()) > 4 and order not in following):
                heading = None
            # A numbered list, 3. Sandro Botticelli: Calumny of Apelles, has its neighbors numbered
            # one less and one more in the same way.
            if heading and len(order) == 1 and re.match(r"^\d+\.\s", text):
                beside = [one[2] for one in lines[max(0, at_line - 1):at_line + 2] if one[2] != text]
                if any(re.match(r"^%d\.\s" % (order[0] + step), one) for one in beside
                       for step in (-1, 1)):
                    heading = None
        # Under an appendix, a numbered row of a list, 1 ˬ(')iˬks F-L WHQ-UNSPEC, has its neighbors
        # numbered one less and one more with no period after the number.
        if heading and section == "appendix" and len(order) == 1:
            beside = [one[2] for one in lines[max(0, at_line - 1):at_line + 2] if one[2] != text]
            if any(re.match(r"^%d\s" % (order[0] + step), one) for one in beside for step in (-1, 1)):
                heading = None
        if heading and len(text) < 90 and not text.endswith((".", ":", ",")):
            flush()
            example = None
            section = "§" + heading.group(1)
            rows.append((section, authors, "heading", text, "page %d" % page))
            continue
        # A sentence wrapped onto a line of its own, appendix., opens nothing.
        if re.match(r"^(?:Appendix|APPENDIX)\b", text) and len(text) < 90:
            flush()
            example = None
            section = "appendix"
            rows.append((section, authors, "heading", text, "page %d" % page))
            continue
        if text.lower().rstrip(": ") == "references":
            flush()
            example = None
            section = "references"
            rows.append((section, authors, "heading", text, "page %d" % page))
            continue
        opened = EXAMPLE.match(text) if section != "references" else None
        opened = opened or (TABLE.match(text) if section != "references" else None)
        # Under the tiers, (i) and (ii) label the readings of the sentence, (i) # ‘I asked how to
        # fix my car.’ in Davis's (35), and open nothing.
        if opened and example is not None and example[2] in ("tiers", "after") and \
                re.fullmatch(r"[ivx]+", opened.group(1)) and \
                re.match(r"^[#?*%]*\s*[‘“]", opened.group(2)):
            opened = None
        # Inside a table, (i) labels a row or a morpheme and opens nothing.
        if opened and example is not None and example[2] == "cells" and \
                not opened.group(1).isdigit() and not TABLE.match(text):
            opened = None
        body = text
        # A caption over a comparative table, (1) Cognates showing Secwepemctsín deglottalization,
        # is English and opens an example all the same: one of the next lines is a row of cells, or
        # a form with its gloss, cf. (i) /tə̣́mɬ ‘red ochre’. A first line that is itself a row,
        # (6) <êstahi΄tz> ‘elk’ cf. s-/txéc’ ‘elk’, opens one too.
        below = lines[at_line + 1:at_line + 5]
        row_opens = bool(opened) and bool(NOTATION_CELL.search(opened.group(2))) and \
            bool(re.search(r"[‘“][^’”]*[’”]", opened.group(2)))
        caption = bool(opened) and (verdict == "english" or sum(
            1 for one in opened.group(2).split() if token_kind(one)[0] == "english") >= 3 or not any(
            token_kind(one)[0] == "language" for one in opened.group(2).split()))
        # A list of one language's words is no table, a. Secwepemctsín adjectives formed with -t
        # over ƛ̓əx-t ‘sweet’ in Davis and Nederveen's (2).
        listed_below = bool(opened) and at_line + 1 < len(lines) and \
            bool(WORD_ROW.match(lines[at_line + 1][2].strip())) and bool(re.match(
                r"^(?:[a-j]\.\s+)?[^\W\d_]*[%s][^\s()]*\s" % re.escape(alphabet), opened.group(2)))
        table_below = bool(opened) and not listed_below and (caption and (any(
                len(NOTATION_CELL.findall(one[2])) >= 2 for one in below[:3]) or sum(
                1 for one in below if re.search(r"\S\s+[‘“][^’”]*[’”]", one[2])
                and not one[2].rstrip().endswith(".")) >= 2) or row_opens)
        # A table set one cell to a line, Table 1: Root repetitions in example (2), has a gloss in
        # quotes on every few lines below its caption. Its column heads can come first, six lines
        # of Point / n-initial / Area / t-initial / Goal / w-initial in Steiner's Table 3.
        if opened and TABLE.match(text) and caption and not table_below:
            table_below = sum(1 for one in lines[at_line + 1:at_line + 15]
                              if re.search(r"[‘“][^’”]*[’”]", one[2])) >= 2 or any(
                re.match(r"^\(a\)\s", one[2]) for one in lines[at_line + 1:at_line + 4])
        if opened and TABLE.match(text) and not table_below:
            opened = None
        # An enumeration in the prose, (ii) cakʷ is unnecessary..., opens a line the way an example
        # does. The engine calls it English and it has no context, no letter and little language.
        if opened and verdict == "english" and not DISPLAY.search(opened.group(2)) and \
                not any(DENOTATION.search(one[2]) for one in lines[at_line:at_line + 3]) and \
                not table_below and \
                not re.match(r"^[√#?*%]*\s*(context|[a-j]\.)", opened.group(2).lower()) and \
                not any(second[one[0]] != "english" for one in lines[at_line + 1:at_line + 4]) and \
                not glossed_below(at_line):
            share = [token_kind(one)[0] for one in opened.group(2).split()]
            if share.count("language") < 0.3 * max(1, len(share)):
                opened = None
        # "in (14)." at the start of a line closes a sentence and opens nothing, and neither does
        # (5): 17, a reference carrying a footnote mark.
        # A line that opens on a reference and goes on with the sentence, (24), which takes the long
        # form, opens nothing either.
        if opened and opened.group(2).strip() and (
                re.match(r"^[.,;:)\]]*\s*\d*\s*$", opened.group(2)) or
                (re.match(r"^[.,;:)\]]", opened.group(2)) and verdict == "english")):
            opened = None
        # (1) tabulates these: in the middle of a sentence refers to the example; it is not one.
        # An example in a practical orthography opens in lower case too, (2) a. ’e.e.ey’ tthu, set
        # with its letter or a wide gap after the number, and the line above can close on a
        # footnote mark, bolded.3.
        # A heading in lower case over its tiers opens one after a tag, (30) dim > ii under (BS).
        if opened and at_line and opened.group(2)[:1].islower() and verdict == "english" and \
                not tiers_below(at_line) and not language_head(opened.group(2)) and \
                not re.match(r"^[a-j]\.\s", opened.group(2)) and \
                not re.match(r"^\(\w+\)\s{2,}", text) and \
                second[lines[at_line - 1][0]] == "english" and \
                not re.sub(r"(?<=[.:!?])\d{1,2}$", "", lines[at_line - 1][2].rstrip()).endswith(
                    (".", ":", "!", "?")):
            opened = None
        if opened:
            flush()
            opened_numbers.append(opened.group(1))
            # An example a footnote sets out, (i) under footnote 2, is that footnote's.
            # A footnote at the foot of one page can go on at the foot of the next, and its (i)
            # sits there under the paper's own examples: Steiner's footnotes 2 and 12.
            footnote_at = re.search(r"page (\d+)", rows[footnote_row][4]) if footnote_row is not None else None
            over_page = footnote_at is not None and int(footnote_at.group(1)) == page - 1 and \
                re.fullmatch(r"[ivx]+", opened.group(1))
            if (footnote_page == page or over_page) and footnote_row is not None and \
                    not opened.group(1).isdigit():
                owner = re.match(r"^(\d{1,2})", rows[footnote_row][3])
                if owner:
                    footnote_examples.append((len(rows), "(%s)" % opened.group(1), owner.group(1)))
                    # The paper's own example the footnotes cut into goes on at the top of the
                    # next page: (29a) of Steiner and Matthewson under footnote 17's (i).
                    if example is not None and not re.fullmatch(r"[ivx]+", example[0]) and \
                            example[2] != "cells" and interrupted is None:
                        interrupted = (example, page)
            elif not re.fullmatch(r"[ivx]+", opened.group(1)):
                interrupted = None
            # [label, line count, state, translation row index]. The state follows the cycle this
            # author sets every example in: context, then tiers, then what follows the translation.
            # A rule, a derivation or a tree is a display, and its short lines are rule rows.
            example = [opened.group(1), 0, "context", None]
            body = opened.group(2)
            if table_below:
                # [.., columns, the half of a row whose ⟨ or { closes on the next line, the last
                # letter of a row labeled (a), (b)]
                example = [opened.group(1), 0, "cells", None, [], "", None]
                # A caption set in columns heads them, (4)   Nicola  English    Dakelh10.
                if row_opens or (body and len(re.split(r"\s{2,}", body.strip())) >= 2):
                    emit_cells(example, body.strip(), page, verdict)
                elif body:
                    example[1] += 1
                    rows.append((cell_where(example), authors, "note", body,
                                 "page %d, the caption" % page))
                continue
            if DISPLAY.search(body) or DENOTATION.search(body) or re.match(r"^(?:[a-j]\.)?\s*$", body):
                example[2] = "display" if DISPLAY.search(body) or DENOTATION.search(body) else "context"
            # A caption in English over denotations, (38) Nsyilxcn null v structure, heads them.
            elif body and verdict == "english" and \
                    any(DENOTATION.search(one[2]) for one in lines[at_line + 1:at_line + 3]):
                example[1] += 1
                rows.append(("(%s) line %d" % (example[0], example[1]), authors, "note", body,
                             "page %d, the caption" % page))
                example[2] = "display"
                continue
            if not body:
                continue
        # A caption can head the denotations, (38) Nsyilxcn null v structure over a. ⟦∅v⟧ = ...
        if example is not None and example[2] == "context" and \
                (example[1] == 0 and DISPLAY.search(body) or DENOTATION.search(body)):
            example[2] = "display"
        if example is not None and example[2] == "display":
            # A denotation runs as long as its formula, ⟦əc- TRGT.STAT⟧ = Stat = λg ∈ DmΔ ... target
            # stativizer in Lyon, and a lettered one, (52) b. ⟦∅⟧ = pos, is its own sub-example.
            denotation = DENOTATION.search(body)
            if denotation or DENOTATION_GOES_ON.search(body) or len(body.split()) <= 8 and not re.match(r"^\d{1,2} [A-Z]", body) and \
                    max(len(one) for one in body.split()) <= 30:
                sub = SUB.match(body) if denotation or DENOTATION_GOES_ON.search(body) else None
                if sub and sub.group(2):
                    base = re.sub(r"[a-mA-D]$", "", example[0])
                    example = [base + sub.group(1), 0, "display", None]
                    last_letter.pop(base, None)
                    last_letter[base] = sub.group(1)
                    body = sub.group(2)
                example[1] += 1
                rows.append(("(%s) line %d" % (example[0], example[1]), authors, "rule", body,
                             "page %d, engine %s" % (page, verdict)))
                continue
            example = None
        # A lettered sub-example opens its own cycle: (3) a., then b., under one number. The next
        # letter after the last sub-example reopens it, where a page break or a comment closed it.
        # Comments set as a paragraph between two sub-examples, Comments on versions of (6): in
        # Hill and Matthewson, close the example; the next letter reopens it when its segmentation
        # and its glosses are the two lines below it.
        if example is None and last_example is not None:
            sub = SUB.match(body)
            if sub and chr(ord(sub.group(1)) - 1) in last_letter.values() and (
                    not paragraph or (at_line + 2 < len(lines) and same_letters(
                        re.sub(r"^[*#?/\s]*", "", sub.group(2) or ""),
                        re.sub(r"^[*#?/\s]*", "", lines[at_line + 1][2]), 0.6) and
                        gloss_shaped(lines[at_line + 2][2], 4))):
                flush()
                example = last_example
        if example is not None:
            # Under a footnote a line can open B. Gerdts, part of the footnote.
            sub = SUB.match(body) if footnote_page != page else None
            # i. under a. in a table is a roman numeral, (6) a. strong suffix > strong root, i. ...
            if sub and example[2] == "cells" and sub.group(1) == "i" and \
                    last_letter.get(re.sub(r"[a-mA-D]$", "", example[0])) != "h":
                body = sub.group(2) or ""
                sub = None
                if not body:
                    continue
            if sub:
                number_only = re.sub(r"[a-mA-D]$", "", example[0])
                letter = sub.group(1)
                # The example whose last letter comes just before this one. Lyon sets (ii) between
                # (102d) and (102e), and e. belongs to (102).
                if letter != "a" and last_letter.get(number_only) != chr(ord(letter) - 1):
                    for base in reversed(list(last_letter)):
                        if last_letter[base] == chr(ord(letter) - 1):
                            number_only = base
                            break
                last_letter.pop(number_only, None)
                last_letter[number_only] = letter
                if example[2] == "cells":
                    example = [number_only + letter, 0, "cells", None, example[4], "", letter]
                else:
                    example = [number_only + letter, 0, "context", None]
                body = sub.group(2) or ""
                if not body:
                    continue
            spoken = SPEAKER.match(body) if example[2] != "cells" else None
            if spoken and spoken.group(1) in TRANSLATION_LABELS:
                spoken = None
            # Only a comment heading stands with nothing after its colon.
            if spoken and spoken.group(2) is None and not spoken.group(1).startswith("Comment"):
                spoken = None
            if spoken and example[2] in ("after", "tiers") and example[1] >= 2 \
                    and not body.lower().startswith("context"):
                example[1] += 1
                rows.append(("(%s) line %d" % (example[0], example[1]),
                             re.sub(r" [Cc]omment$", "", spoken.group(1)),
                             "speaker comment", body, "page %d, engine %s" % (page, verdict)))
                comment_number = number
                for kind, plain in (token_kind(one) for one in (spoken.group(2) or "").split()):
                    if kind == "language":
                        rows.append(("(%s)" % example[0], language, "cited form", plain,
                                     "in the speaker comment, page %d" % page))
                # A speaker's comment closes the tiers the way a translation does: Steiner's
                # rejected sentences have the comment and no translation, and the prose after
                # it is no gloss.
                example[2] = "after"
                continue
        # The footnotes at the foot of a page can fall between two tiers of one example, as in (2)
        # of Phillips et al., or inside a paragraph whose line above carries the mark, language.4.
        # They are notes, and the example or the paragraph goes on over the page break.
        if footnote_page is not None and page != footnote_page:
            footnote_page = None
        if example is None or example[2] in ("tiers", "cells", "after"):
            # Under a paragraph the text layer can drop the space after the number, 1Fieldwork, or
            # set two, 22  The translations.
            # A footnote on a morpheme can open with it, 23 =ének surfaces as =ánek.
            mark = re.match(r"^(\d{1,2}) +(?:[A-Z]|[=/*√-]\S)" if example is not None
                            else r"^(\d{1,2})(?:\s*[A-Z][a-z]|\s+(?:[A-Z]\.|[AI] |[=/*√-]\S))", body)
            # The mark in the text above: language.4, languages2, ‘here’22, /*nəw- ~ nəxʷ/7,
            # morphemes 4, where the text layer set a space before it, and (p.537). 23 It, where it
            # set one after the sentence.
            marked = mark and any(
                re.search(r"(?:(?:[^\W\d\s]|[.,;:!?’”)/⟩}\]>])[.,;:!?’”)]*%s(?=[\s,.;:)⟩]|$)"
                          r"|[^\W\d\s] %s(?=[,.;])|[.’”)] %s(?= [A-Z]))"
                          % (mark.group(1), mark.group(1), mark.group(1)), one[2])
                for one in lines[max(0, at_line - 80):at_line] if one[1] in (page, page - 1))
            # The next footnote in the paper's count, marked in the text above, opens one after a
            # blank line as well as under a paragraph: 1 Thanks to Linda Smith, below the title's
            # Dene1. One the engine missed, 5 A possible argument, leaves the next one a step past
            # the count, and it is still the next footnote.
            expected = bool(marked) and footnote_last < int(mark.group(1)) <= footnote_last + 2
            # The next number in the count can open on a lowercase name, 30 c̕úʔsinek does often use.
            loose = re.match(r"^(\d{1,2}) +[^\W\d_]", body) if not mark else None
            if loose and int(loose.group(1)) == footnote_last + 1 and any(
                    re.search(r"(?:[^\W\d\s]|[)\]])[.,;:!?’”)]*%s(?=[\s,.;:)]|$)" % loose.group(1), one[2])
                    for one in lines[max(0, at_line - 80):at_line] if one[1] in (page, page - 1)):
                mark = marked = loose
                expected = True
            # Below one footnote, a line opening with the next number is the next footnote, however
            # it opens: 7 I.e. kinnikinnik, 17  Nɬeʔkepmxcín, 24 =cin regularly surfaces.
            if footnote_page == page and footnote_row is not None:
                before = re.match(r"^(\d{1,2})", rows[footnote_row][3])
                following = re.match(r"^(\d{1,2})\s+\S", body)
                if before and following and int(following.group(1)) == int(before.group(1)) + 1:
                    mark = marked = following
            # A footnote marked with a star, the acknowledgement under a title that ends Kíl*: the
            # line opens * Háw'aa, and a line above it on its page carries the star.
            starred = re.match(r"^\* *[A-Z]", body) and any(
                re.search(r"[^\W\d\s][.,;:!?’”)]*\*(?=[\s,.;:)]|$)", one[2])
                for one in lines[max(0, at_line - 80):at_line] if one[1] == page)
            if starred and footnote_row is None or starred and not rows[footnote_row][3].startswith("*"):
                footnote_page = page
                rows.append((section, authors, "note", body, "page %d, footnote" % page))
                footnote_row = len(rows) - 1
                offer(body, body, [token_kind(one) for one in body.split()], number, page, verdict)
                continue
            # Below one footnote, a line opening with a number is the next footnote.
            if mark and ((example is not None and example[2] != "after" and verdict == "english") or (paragraph and marked)
                         or (footnote_page == page and marked) or expected):
                footnote_page = page
                footnote_last = int(mark.group(1))
                rows.append((section, authors, "note", body, "page %d, footnote" % page))
                footnote_row = len(rows) - 1
                # A footnote cites forms, names and languages as the prose does.
                offer(body, body, [token_kind(one) for one in body.split()], number, page, verdict)
                if example is not None:
                    fixed.append((page, "footnote inside an example", body,
                                  "(%s) goes on below it" % example[0]))
                continue
            # The footnotes close the page, and what follows one on its page is more of it, save an
            # example the footnote sets out, (i) under footnote 3 of Kelly et al.
            if footnote_page == page and footnote_row is not None and not EXAMPLE.match(body) and \
                    not any(" line " in one[0] for one in rows[footnote_row + 1:]):
                last = rows[footnote_row]
                rows[footnote_row] = (last[0], last[1], last[2], last[3] + " " + body, last[4])
                offer(body, body, [token_kind(one) for one in body.split()], number, page, verdict)
                continue
        closes = r"[’”]\W*(?:\([^()]*\)?\S*\s*)?$"
        # The translation still open at the foot of a page, with only the page's footnotes and a
        # figure's caption set after it: (68a) ‘... Return true iff ⃗v is in the / 26 Zwarts and
        # Winter ... / Figure 5: A visualization ... / spatial representation of α.’
        held = None
        if example is not None and example[2] == "after" and example[3] is not None and \
                example[3] < len(rows) and all(one[2] in ("note", "cited form") for one in rows[example[3] + 1:]):
            opened_quote = re.match(r"^[#?*%]*([‘“])", rows[example[3]][3])
            if opened_quote and not re.search(
                    ("’" if opened_quote.group(1) == "‘" else "”") + r"(?![^\W\d_])",
                    rows[example[3]][3][opened_quote.end():]):
                held = example[3]
        # The caption is a note of its own, and the translation closes below it.
        if held is not None and re.match(r"^(?:Figure|Fig\.) \d+[:.]", body):
            rows.append((section, authors, "note", body, "page %d, a figure caption" % page))
            caption_row = len(rows) - 1
            continue
        if caption_row is not None:
            if held is not None and caption_row == len(rows) - 1 and \
                    not SUB.match(body) and not EXAMPLE.match(body):
                if re.search(closes, body) and body[:1].islower():
                    last = rows[held]
                    rows[held] = (last[0], last[1], last[2], last[3] + " " + body, last[4])
                    fixed.append((page, "figure caption inside a translation", body, rows[held][3]))
                    caption_row = None
                else:
                    last = rows[caption_row]
                    rows[caption_row] = last[:3] + (last[3] + " " + body, last[4])
                continue
            caption_row = None
        if example is not None and example[2] == "cells":
            if emit_cells(example, body, page, verdict):
                # A row head holds forms as the prose does: Feminine SG ɬə ɬ U+2014 in Kelly et al.
                if rows and rows[-1][3] == body and rows[-1][4].endswith("heads the rows below"):
                    offer(body, body, [token_kind(one) for one in body.split()], number, page, verdict)
                continue
            example = None
        kinds = [token_kind(one) for one in body.split()]
        counted = collections.Counter(kind for kind, plain in kinds if kind)
        gloss_line = counted["label"] >= 1 and counted["label"] >= counted["language"]
        tier_like = (counted["language"] + counted["label"]) > 0 or verdict != "english"
        # A translation whose closing quote is on the next line: ... and what something seems / like.'
        # A translation of three lines has a middle line with no quote at all, and the closing
        # quote on the line after it.
        closes = r"[’”']\W*(?:\([^()]*\)?\S*\s*)?$"
        # The translation is open until the quote it opened with closes: ‘"I'm helping you!" is
        # still open after the speech inside it closes. A straight quote closes itself.
        opener = re.match(r"^[#?*%]*([‘“'])", rows[-1][3]) if rows else None
        # The quote left open is the last one opened: a literal reading after the free one,
        # `‘I jumped over the log in order to get my axe.’` (More literally: ‘I jumped over the
        # log so, wraps with its own quote open in Davis's (58).
        last_open = rows[-1][3].rfind(opener.group(1)) + 1 if opener else 0
        if example is not None and example[2] == "after" and example[3] == len(rows) - 1 and \
                opener and not re.search(
                    {"‘": "’", "“": "”", "'": "'"}[opener.group(1)] + r"(?![^\W\d_])",
                    rows[-1][3][max(opener.end(), last_open):]) and not EXAMPLE.match(body) and \
                not SUB.match(body) and (re.search(closes, body) or (
                    verdict == "english" and closing_ahead(at_line))):
            last = rows.pop()
            rows.append((last[0], last[1], last[2], last[3] + " " + body, last[4]))
            fixed.append((page, "translation broken at a line end", body, rows[-1][3]))
            continue
        # A speaker's comment runs on until its quote closes: BP: "I think it's okay, ... / what the
        # thing you're eating is." (SF | BP 17 April 2025)
        # The forms the comment cites sit after it as cited form rows of the example.
        comment_at = len(rows) - 1
        while example is not None and comment_at > 0 and rows[comment_at][2] == "cited form" and \
                rows[comment_at][0] == "(%s)" % example[0]:
            comment_at -= 1
        # A comment heading, Comments on versions of (6):, gathers the quotes set on the lines
        # right under it, each closing on its speaker's initials, (BS), and a line holding only
        # the initials joins the quote above it. A blank line ends the comment, and a comment
        # closed on initials ends there even with a quote the page leaves open, "It's saying "Oh,
        # is there no good restaurant here?" (BS).
        # A tag with its fields, (SF | KBG 12 Nov 2025), is the example's own row and joins nothing.
        adjacent = number == comment_number + 1
        said = rows[comment_at][3] if rows else ""
        bracket_open = said.count("[") > said.count("]")
        if example is not None and rows and rows[comment_at][2] == "speaker comment" and \
                rows[comment_at][0].startswith("(%s) line" % example[0]) and \
                ((said.count("“") > said.count("”") and (
                    adjacent or not re.search(r"\([A-Z]{2}(?:, ?[A-Z]{2})*\)$", said))) or
                 (bracket_open and adjacent) or
                 (adjacent and (said.endswith(":") or body.startswith("“") or
                                re.fullmatch(r"\([A-Z]{2}(?:, ?[A-Z]{2})*\)", body)))) and \
                (not EXAMPLE.match(body) or bracket_open) and not SUB.match(body):
            last = rows[comment_at]
            rows[comment_at] = (last[0], last[1], last[2], last[3] + " " + body, last[4])
            fixed.append((page, "speaker comment broken at a line end", body, rows[comment_at][3]))
            comment_number = number
            continue
        # The source after a translation broken inside its parentheses, (MJJ / 8–9).
        if example is not None and example[2] == "after" and example[3] == len(rows) - 1 and \
                rows[-1][3].count("(") > rows[-1][3].count(")") and \
                re.match(r"^[^()]{0,20}\)\S*$", body):
            last = rows.pop()
            rows.append((last[0], last[1], last[2], last[3] + " " + body, last[4]))
            fixed.append((page, "source broken inside its parentheses", body, rows[-1][3]))
            continue
        # An example the page draws as a picture, the tree of Steiner and Matthewson's (44), has no
        # line in the text layer, and the paragraph after it is prose. A context set with no
        # Context: label, Hannon's (28), has its tiers a few lines below.
        if example is not None and example[2] == "context" and example[1] == 0 and \
                re.fullmatch(r"\d+", example[0]) and \
                verdict == "english" and len(body) > 60 and not SUB.match(body) and \
                not re.match(r"^[√#?*%]*\s*context", body.lower()) and \
                not language_head(body) and \
                not any(tiers_below(one) for one in range(at_line, min(at_line + 5, len(lines)))) and sum(
                    1 for one in re.findall(r"[A-Za-z]+", body)
                    if one.lower() in ("the", "and", "of", "in", "is", "to", "a", "that", "are",
                                       "why", "for", "with", "which", "it")) >= 3:
            fixed.append((page, "example with no line in the text layer, a picture",
                          "(%s)" % example[0], body))
            rows.append(("(%s)" % example[0], authors, "note",
                         "(%s)" % example[0], "page %d, drawn as a picture with no text layer" % page))
            example = None
        # A paragraph under the last word of a list ends it, As mentioned above, under (4d) of
        # Davis and Nederveen.
        if example is not None and "(%s)" % example[0] in word_lists and example[2] == "context" and \
                example[1] > 0 and verdict == "english" and len(body) > 60 and \
                not WORD_ROW.match(body) and not SUB.match(body) and not EXAMPLE.match(body) and \
                not re.match(r"^\d{1,2}\s", body) and not list_resumes(at_line):
            example = None
        if example is not None:
            label = "(%s)" % example[0]
            state = example[2]
            kind = None
            previous = rows[-1][2] if rows and rows[-1][0].startswith(label + " line") else None
            # A list of words by language, Davis and Nederveen's (1): a sub-letter names the
            # language and its source, b. Secwepemctsín (Kuipers 1974:54-55), and each line under
            # it is a form, the root it is built on after <, and its gloss, páḷ<pəḷ>-t < √paḷ
            # ‘stubborn’. The forms are that language's.
            # The label can go on to say what the words are, a. Secwepemctsín adjectives formed
            # with -t but without C1C2 reduplication in their (2); the name then carries a letter
            # of the alphabet.
            named = re.match(r"^([^\W\d_][^\s()]*?)\d{0,2}\s*(?:\([^()]*\d{4}[^()]*\))?\s*$", body)
            captioned = re.match(r"^([^\W\d_][^\s()]*)\s+[a-z][^‘“]*$", body)
            if not named and captioned and any(one in alphabet for one in captioned.group(1)):
                named = captioned
            if named and state == "context" and example[1] == 0 and \
                    is_language_name(named.group(1)) and at_line + 1 < len(lines) and \
                    WORD_ROW.match(lines[at_line + 1][2].strip()):
                word_lists[label] = named.group(1)
                example[1] += 1
                rows.append(("%s line %d" % (label, example[1]), authors, "note", body,
                             "page %d, the language of the words below" % page))
                continue
            # The footnotes at the foot of a page can fall inside a list: the end of footnote 5 and
            # footnote 6 under the first three words of Davis and Nederveen's (4a), which goes on
            # at the top of the next page. A numbered footnote under the last word of a list, 12
            # under (5d), ends the list.
            if label in word_lists and state == "context" and example[1] > 0 and \
                    not WORD_ROW.match(body) and not named:
                numbered = re.match(r"^(\d{1,2})\s+\S", body)
                if numbered and footnote_last < int(numbered.group(1)) <= footnote_last + 2:
                    footnote_page = page
                    footnote_last = int(numbered.group(1))
                    rows.append((section, authors, "note", body, "page %d, footnote" % page))
                    footnote_row = len(rows) - 1
                    offer(body, body, [token_kind(one) for one in body.split()], number, page, verdict)
                    fixed.append((page, "footnote inside a list of words", body,
                                  "%s goes on below it" % label if list_resumes(at_line)
                                  else "%s ends above it" % label))
                    if not list_resumes(at_line):
                        example = None
                    continue
                if footnote_row is not None and list_resumes(at_line):
                    last = rows[footnote_row]
                    rows[footnote_row] = (last[0], last[1], last[2], last[3] + " " + body, last[4])
                    offer(body, body, [token_kind(one) for one in body.split()], number, page, verdict)
                    fixed.append((page, "footnote carried over inside a list of words", body,
                                  "%s goes on on the next page" % label))
                    continue
            listed = WORD_ROW.match(body) if label in word_lists else None
            if listed:
                example[1] += 1
                where = "%s line %d" % (label, example[1])
                form, root, meaning = listed.group(1), listed.group(2), listed.group(3)
                # A gloss can carry a footnote mark, `‘dark colour’`4.
                marker = re.search(r"(?<=[’\w)])(\d{1,2})\s*$", body)
                rows.append((where, word_lists[label], "cited form", form,
                             "page %d, %s%s%s" % (page, meaning, ", from %s" % root if root else "",
                                                  ", carries footnote %s, written here without its digit"
                                                  % marker.group(1) if marker else "")))
                if root:
                    rows.append((where, word_lists[label], "root", root,
                                 "page %d, the root of %s" % (page, form)))
                continue
            # A line in a practical orthography of plain letters and apostrophes, suw’ thu.u.uytus
            # ’imush, which the engine cannot tell from English: no letter of the phonemic
            # alphabet, and its words carry the glottal stop as ’ or are no English word. It is the
            # first line of an example, with a tier or a translation below it, or the next line
            # of an orthography line that wraps, ’u kwsus wulh kwunnuhwus. A first line can open
            # on the quote of what someone says, “’a.a.a, me’.
            # A wrapped line can be one or two words, thuyumun. or hwu saay’.
            # A sentence of the prose citing forms, Here the passive and active forms of the verb
            # root √tth’as, carries the words English cannot do without.
            function_words = sum(1 for one in re.findall(r"[A-Za-z]+", body)
                                 if one.lower() in ("the", "and", "of", "in", "is", "to", "a",
                                                    "that", "with", "by", "for", "as", "are",
                                                    "was", "which", "this", "it", "an", "be"))
            # A line of gloss labels, 𝟣ꜱɢ.ᴘᴏꜱꜱ=forget-ɴᴄᴛʀ, is no orthography.
            plain_letters = counted["language"] == 0 and counted["label"] == 0 and \
                not re.match(r"^[#?*%]*‘", body) and function_words < 2
            stops = len(re.findall(r"(?<![^\s“¶(])’\w|\w’(?!(?:s|t|re|ll|ve|d|m)\b)", body))
            # One is enough on the opening line of quoted speech, "lheel qwul'ilh lheel.
            glottal = plain_letters and (stops >= 2 or (
                stops == 1 and body.startswith("“") and state == "after"))
            orthographic = glottal or (plain_letters and counted["unknown"] * 2 >= max(1, len(kinds)))
            # After a translation, more of the text under the same letter: the song of (32b),
            # "lheel qwul'ilh lheel.
            if glottal and state == "after" and not re.search(r"[‘“][^’”]*$", rows[-1][3]):
                orthographic = "after"
            orthographic = orthographic == "after" or (orthographic and state == "context" and example[1] == 0 and any(
                second[one[0]] != "english" or re.match(r"^\s*[‘“]", one[2])
                for one in lines[at_line + 1:at_line + 5])) or (
                state == "tiers" and previous == "transcription" and plain_letters and
                (orthographic or len(body.split()) <= 2))
            # A tier and its segmentation define the same letters, e nicola lake over e=nicola lake,
            # whatever the words are: a name in a tier reads as English and is still a tier.
            following_line = lines[at_line + 1][2] if at_line + 1 < len(lines) else ""
            spells_next = state in ("context", "tiers") and "=" not in body and \
                re.search(r"\S[=-]\S", following_line) and \
                same_letters(body, following_line) and not re.match(r"^[#?*%]*[‘“]", body)
            # Over a line of glosses the stems can be defined apart, yukw-@t under yugwit.
            spells_previous = state == "tiers" and previous == "transcription" and \
                re.search(r"\S[=-]\S", body) and (same_letters(body, rows[-1][3]) or (
                    same_letters(body, rows[-1][3], 0.6) and at_line + 1 < len(lines) and
                    gloss_shaped(lines[at_line + 1][2], 4)))
            # A segmentation of words with no boundary in them, bax ’nii’y under Bax ’nii’y., is
            # the sentence again without its capital and full stop, with the glosses below it.
            if state == "tiers" and previous == "transcription" and not spells_previous and \
                    body != rows[-1][3] and same_letters(body, rows[-1][3]) and \
                    at_line + 1 < len(lines):
                below = collections.Counter(
                    token_kind(one)[0] for one in lines[at_line + 1][2].split())
                spells_previous = below["label"] >= 1 and below["label"] >= below["language"]
            # A context whose sentence the line end broke goes on, whatever it quotes: ask / zuzúʔt
            # kʷn̕ ‘Aren't you late?’ He responds: in Steiner and Matthewson's (33).
            # A new sentence of the context can quote a form too, There are no other salmon in the
            # room. Ella says k̓ʷén̓ete xéʔe tk sqyéytn ‘Look at that salmon’, where the context above
            # does not end on the colon that introduces the example.
            # The author's format where the words read as English: the sentence, its segmentation
            # definition the same letters, and a line of gloss labels, Luu amhl goodi’y dim wil wis. /
            # luu am =hl goot-’y [dim wil wis] / in good =CN heart-1SG.II PROSP COMP rain in Brown's (2).
            # The segmentation can define the stems apart, Guhl yugwit jebin? over gu =hl yukw-@t
            # [jep-@-n ], and the glosses be mostly English words, not.exist =CN fish.
            shaped = state == "context" and at_line + 2 < len(lines) and \
                gloss_shaped(lines[at_line + 2][2], 4) and not gloss_shaped(body) and \
                (same_letters(body, lines[at_line + 1][2]) or (
                    same_letters(re.sub(r"^[a-j]\.\s+[*#?]*", "", body), lines[at_line + 1][2], 0.6) and
                    re.search(r"\S[=-]\S|\[", lines[at_line + 1][2]))) and \
                not re.match(r"^[#?*%]*[‘“']", body)
            # A sentence with its tiers under it is no context going on, Nee diit jeps Cindyhl
            # ha'niit'aa. under Dependent clause: no transitive suffix in Brown's (8b).
            context_open = state == "context" and rows and rows[-1][2] == "note" and \
                rows[-1][0].startswith(label + " line") and not shaped and \
                not re.search(r"\S[=-]\S|∅", body) and (
                    (body[:1].islower() and re.search(r"[^\W\d_]$", rows[-1][3].rstrip())
                     and counted["english"] >= 3) or
                    (body[:1].isupper() and counted["english"] >= 5
                     and not rows[-1][3].rstrip().endswith(":")
                     and not re.match(r"^context\b", body.lower())))
            if context_open:
                last = rows.pop()
                rows.append((last[0], last[1], last[2], last[3] + " " + body, last[4]))
                fixed.append((page, "context broken at a line end", body, rows[-1][3]))
                continue
            # The heading an example opens with, dim precedes complementizer wil:, over its tiers.
            # One with no colon heads them where the tiers start right under it, Yukw selects
            # complement headed by =hl over Yukwhl dim wis. / yukw [=hl dim wis] / PROG =CN PROSP rain.
            tiers_under = tiers_below(at_line)
            # A heading of two particles in their order, dim > ii, reads as no English.
            heads = state == "context" and example[1] == 0 and len(body) < 90 and \
                (verdict == "english" or (tiers_under and ">" in body)) and \
                not re.match(r"^[#?*%]*[‘“']", body) and not same_letters(body, lines[at_line + 1][2]) and \
                ((body.rstrip().endswith(":") and glossed_below(at_line)) or tiers_under)
            named_head = language_head(body) if state == "context" and example[1] == 0 else None
            if named_head:
                kind = "context"
                example_languages[re.match(r"\d*", example[0]).group(0) or example[0]] = named_head
            elif shaped:
                kind = "transcription"
            elif heads:
                kind = "context"
            elif spells_next:
                kind = "transcription"
            elif spells_previous:
                kind = "segmentation"
            elif orthographic:
                kind = "transcription"
            # A translation, or a reading the speaker rejected, marked #‘ or ?‘ or *‘. Some pages set
            # it in straight quotes, 'This bag will rip if I fill it up.'
            # A quote holding clitic boundaries is speech in the language, the first tier of
            # Davis's (50e), "waʔ=ɬkan=á=qaʔ ƛ̓it zəwát-ən-Ø.
            elif re.match(r"^[#?*%]*(?:[‘“]|'(?=[#*?]*[A-Z]))", body) and \
                    (state != "after" or body[0] in "#?*%") and \
                    not re.search(r"[^\W\d_]=[^\W\d_]", body):
                kind = "translation"
            # Straight quotes right under the tiers open a translation whatever its first letter,
            # 'they were all very friendly together' in wlwlmelst and Janzen's (3).
            elif state == "tiers" and previous in ("gloss", "word gloss") and \
                    re.match(r"^'[^\W\d_]", body):
                kind = "translation"
            # An English tier under the gloss, word for word, with another tier or the translation
            # on the line after it: the spouse of coyote and, under REM DET AUG married both OBL
            # coyote ADD in wlwlmelst and Janzen's (1).
            # A line with a quote, a colon, a speaker's tag or a closing full stop is a translation,
            # Intended: ‘We are here.’ or The berries might be ripe now. (Dave Michele | CF).
            elif state == "tiers" and previous == "gloss" and verdict == "english" and \
                    len(body) < 80 and not re.search(r"[‘’“”'≠|:]|[.?!]\s*(?:\(|$)", body) and \
                    at_line + 1 < len(lines) and (
                        second[lines[at_line + 1][0]] != "english" or
                        re.match(r"^[#?*%]*[‘“']", lines[at_line + 1][2]) or
                        # A short tier reads as English, he y̓e te steʔ., and the segmentation
                        # under it has its boundaries, he= y̓e tə= s- teʔ.
                        (at_line + 2 < len(lines) and re.search(
                            r"[^\W\d_][=-](?:\s|$)|(?:^|\s)[=-][^\W\d_]", lines[at_line + 2][2]))):
                kind = "word gloss"
            # A reading the sentence does not have, ≠ ‘You’ll get sick if you eat that.’
            elif re.match(r"^≠\s*[‘“']", body) and state in ("tiers", "after"):
                kind = "translation"
            # A label over its translation with no colon, Intended ‘The man is dead [now].’
            elif re.match(r"^(?:%s):?\s*[‘“']" % "|".join(TRANSLATION_LABELS), body) and \
                    state in ("tiers", "after"):
                kind = "translation"
            # The heading of a sub-example, a. Plain DP: or b. Demonstrative + oblique (k=
            # determiner):, names the construction and is no tier.
            # Brown sets what the sub-example shows after the colon, b. Dependent intransitive:
            # Series II marks S.
            elif state in ("context", "tiers") and example[1] == 0 and len(body) < 90 and \
                    (re.match(r"^[A-Z][^:‘“]*:$", body) or
                     re.match(r"^[A-Z][A-Za-z ()/,.-]*:\s[^‘“]*$", body)) and verdict == "english":
                kind = "context"
            # A second reading right under the first, whole on its line: Steiner's (7) sets
            # ‘#You’ll get sick if you eat that.’ over ‘You’ll get sick if you eat it.’
            elif state == "after" and example[3] == len(rows) - 1 and \
                    re.match(r"^‘[^‘’]*(?:’\w[^‘’]*)*’\s*(?:\([^()]*\))?\s*$", body):
                kind = "translation"
            # One the page opens with the wrong quote, ’What is that bug right here on my hand?’
            # in Steiner's (82), under the last gloss.
            elif state == "tiers" and previous == "gloss" and re.match(r"^’[^’]+[.?!]’$", body):
                kind = "translation"
            # A label can be set in parentheses with the word meaning, (Intended meaning: ‘Tell him
            # where to go.’) in Davis's (26b).
            elif re.sub(r"^\(|\s+meaning$", "", body.split(":")[0]) in TRANSLATION_LABELS and \
                    ":" in body and state in ("tiers", "after"):
                kind = "translation"
            # A reading labeled under the tiers, (ii)# ‘Mary figured out that John already knew she
            # could drum.’
            elif state in ("tiers", "after") and re.match(r"^\([ivx]+\)\s*[#?*%]*\s*[‘“]", body):
                kind = "translation"
            # The literal reading under the free one, lit. ‘It is good that you will sit down.’
            elif state == "after" and example[3] == len(rows) - 1 and \
                    re.match(r"^(?:lit\.|literally)\s*[‘“]", body):
                kind = "translation"
            elif state == "context" and re.match(r"^[√#?*%]*\s*context", body.lower()):
                kind = "context"
            elif state == "context" and verdict == "english" and example[1] \
                    and counted["language"] < 0.3 * max(1, len(kinds)) \
                    and rows[-1][2] == "note" and rows[-1][0].startswith(label):
                # The context wraps onto a second line.
                last = rows.pop()
                rows.append((last[0], last[1], last[2], last[3] + " " + body, last[4]))
                fixed.append((page, "context broken at a line end", body, rows[-1][3]))
                continue
            elif state in ("context", "tiers") and tier_like and not (
                    previous == "segmentation" and (verdict == "english"
                                                    or counted["language"] == 0)):
                # The tier under an orthography line is its segmentation, whether the
                # word carries a boundary: ƛ̓ʊxʷegən above ƛ̓əxʷigan.
                # Two tiers the page wraps together go on in turn, a segmentation and its glosses
                # and then the rest of each: [kʷa cəkláw̓sxən]] in Davis's (23), where the
                # segmentation above and its glosses both left a bracket open. A new sentence
                # after the glosses starts with every bracket closed, and so does the rest of a
                # formula, p]] under Steiner's (73a).
                wrapped_pair = previous == "gloss" and len(rows) >= 2 and \
                    rows[-2][2] == "segmentation" and rows[-2][0].startswith(label) and \
                    all(one[3].count("[") > one[3].count("]") for one in rows[-2:])
                kind = "gloss" if gloss_line else (
                    "segmentation" if (previous == "transcription" or wrapped_pair
                                       or re.search(r"[=~<>]|\S-\S", body)) else "transcription")
                # A segmentation of labels and affixes, n- CVC- zuʔ -tn -s under nzuʔzuʔtəns in
                # wlwlmelst and Janzen's (13), reads as a gloss line; its own gloss is the next line.
                if kind == "gloss" and previous == "transcription" and \
                        re.search(r"(?:^|\s)\S+-(?:\s|$)|(?:^|\s)-\S", body) and \
                        at_line + 1 < len(lines):
                    below = collections.Counter(
                        token_kind(one)[0] for one in lines[at_line + 1][2].split())
                    if below["label"] >= 1 and below["label"] >= below["language"]:
                        kind = "segmentation"
            elif state == "tiers" and previous == "segmentation":
                # The line under a segmentation is its gloss, labels or not: envious under
                # ƛ̓əxʷigan=č.
                kind = "gloss"
            elif state == "after" and len(body) < 70 and body.startswith(("[", "(")) and \
                    not EXAMPLE.match(body):
                kind = "citation"
            elif state == "after" and verdict != "english" and counted["language"] and \
                    at_line + 1 < len(lines) and re.search(r"\S[-=~]\S", lines[at_line + 1][2]):
                # A second sentence under the same number, with its own tiers below it: (3) of
                # Reisinger §4.1 sets two sentences from a text one after the other.
                kind = "transcription"
            elif state == "tiers" and not tier_like and example[3] is not None and len(body) < 40:
                kind = "citation"
            elif state == "context" and example[1] == 0 and verdict == "english" and \
                    at_line + 1 < len(lines) and (second[lines[at_line + 1][0]] != "english" or any(
                        tiers_below(one) for one in range(at_line, min(at_line + 5, len(lines))))):
                # An English line the example opens with, Repeated from (7), with the tiers below.
                # A context with no Context: label runs over lines of its own first, Your daughter
                # has a lovely voice. in Hannon's (28).
                kind = "context"
            if kind is not None:
                example[1] += 1
                where = "%s line %d" % (label, example[1])
                who = example_languages.get(re.match(r"\d*", example[0]).group(0) or example[0],
                                            language) \
                    if kind in ("gloss", "segmentation", "transcription") else authors
                rows.append((where, who, "note" if kind == "context" else kind, body,
                             "page %d, engine %s" % (page, verdict)))
                if kind in ("transcription", "segmentation"):
                    example[2] = "tiers"
                    web_rows.append([label, language, kind, body, ""])
                elif kind == "gloss":
                    example[2] = "tiers"
                elif kind == "translation":
                    example[2] = "after"
                    example[3] = len(rows) - 1
                    concept = " ".join(QUOTED.findall(body))
                    for one in web_rows:
                        if one[0] == label:
                            one[4] = concept
                # The source cited on an example's lines is whose English its translation is.
                # A work in preparation stands for its year, (Davis et al. in prep.).
                source = re.search(r"\(([A-Z][^()]*(?:\d{4}|in prep\.)[^()]*)\)", body)
                if source and example[3] is not None:
                    at = example[3]
                    rows[at] = (rows[at][0], source.group(1), rows[at][2], rows[at][3],
                                rows[at][4] + ", cited " + source.group(1))
                continue
            example = None
        if not paragraph:
            paragraph_at = (section, page)
        # A word the line end broke with a hyphen, subdi- / vided. The note keeps it as printed.
        if paragraph and re.search(r"[^\W\d_]-$", paragraph[-1]) and text[:1].islower():
            fixed.append((page, "word broken by a hyphen at a line end, kept",
                          paragraph[-1].split()[-1] + " " + text.split()[0],
                          paragraph[-1].split()[-1][:-1] + text.split()[0]))
        paragraph.append(text)
        offer(text, body, kinds, number, page, verdict)
        if text.endswith((".", "!", "?", ":")) and len(text) < 80:
            flush()
    flush()

    # Each footnote numbers its examples from (i) again, and its where carries the footnote:
    # footnote 2 (i) line 3.
    for start, label, owner in footnote_examples:
        for at in range(start, len(rows)):
            if rows[at][0] == label or rows[at][0].startswith(label + " line"):
                rows[at] = ("footnote %s %s" % (owner, rows[at][0]),) + tuple(rows[at][1:])
            # The next footnote starts its own (i): footnotes 12 and 13 of Steiner and Matthewson.
            elif rows[at][0].startswith("(") or (rows[at][2] == "note" and
                                                  rows[at][4].endswith(", footnote")):
                break

    # The prose of a footnote goes on under its example to the foot of the page: Lit. ‘Where a
    # fire started under the fruits was there.’ under footnote 12's (i) in Steiner.
    def page_of(row):
        found = re.search(r"page (\d+)", row[4])
        return int(found.group(1)) if found else None
    for start, label, owner in footnote_examples:
        last = None
        for at in range(start, len(rows)):
            if rows[at][0].startswith("footnote %s %s" % (owner, label)):
                last = at
            elif last is not None:
                break
        if last is None:
            continue
        for at in range(last + 1, len(rows)):
            row = rows[at]
            if page_of(row) != page_of(rows[last]) or " line " in row[0] or \
                    row[0].startswith(("(", "footnote", "Table")) or row[2] == "heading" or \
                    (row[2] == "note" and "footnote" in row[4]):
                break
            rows[at] = ("footnote %s" % owner,) + tuple(row[1:])
            fixed.append((str(page_of(row)), "footnote prose under its example, read as body text",
                          row[3][:80], "footnote %s" % owner))

    # An author sets the tiers one way throughout. Where nearly every segmentation in the paper
    # has its gloss on the line below, a line under a segmentation that reads as a transcription
    # is a gloss that repeats the word: kéʔe / kéʔe / kéʔe in Steiner's (i).
    # The line has to repeat the segmentation itself. Kelly et al. (7) sets its second word out of
    # order, tᶿ=niy-əxʷ above tᶿ niyʊxʷ., and that line is the transcription.
    def example_of(row):
        return row[0].split(" line ")[0] if " line " in row[0] else None
    below_segmentation = collections.Counter(
        two[2] for one, two in zip(rows, rows[1:])
        if one[2] == "segmentation" and example_of(one) and example_of(one) == example_of(two))
    if below_segmentation["gloss"] >= 20 and \
            below_segmentation["gloss"] >= 0.9 * (below_segmentation["gloss"]
                                                  + below_segmentation["transcription"]):
        for at in range(1, len(rows)):
            if rows[at][2] == "transcription" and rows[at - 1][2] == "segmentation" and \
                    rows[at][3] == rows[at - 1][3] and \
                    example_of(rows[at]) and example_of(rows[at]) == example_of(rows[at - 1]):
                rows[at] = rows[at][:2] + ("gloss",) + rows[at][3:]
                fixed.append((re.search(r"page (\d+)", rows[at][4]).group(1),
                              "gloss that repeats its word, read as a transcription",
                              rows[at][3], rows[at][0] + " gloss"))
        # A second segmentation under a segmentation that puts a gloss label where the first has
        # the morpheme is the gloss: DET=nɬeʔképmx under e=nɬeʔképmx, a name glossed as itself.
        for at in range(1, len(rows)):
            if rows[at][2] == "segmentation" and rows[at - 1][2] == "segmentation" and \
                    example_of(rows[at]) and example_of(rows[at]) == example_of(rows[at - 1]) and \
                    re.search(r"(?:^|[=\-])(?:[A-Z]{2,}[\w.]*|[A-Z]/[A-Z])(?=[=\-]|$)", rows[at][3]) and \
                    not re.search(r"[A-Z]{2,}|[A-Z]/[A-Z]", rows[at - 1][3]):
                rows[at] = rows[at][:2] + ("gloss",) + rows[at][3:]
                fixed.append((re.search(r"page (\d+)", rows[at][4]).group(1),
                              "gloss read as a second segmentation", rows[at][3], rows[at][0] + " gloss"))

    # A paper that numbers its examples again in every section, (1) to (17) in §3.1 and (1) again in
    # §3.2, gets the section in front of each example's where: §3.2 (1) line 2.
    # A reference in the prose that opens a line, (13), the overt NPs or (3) never saw the object,
    # is not a second (13): only the examples the engine opened are counted, and a paper that
    # numbers again in every section starts over at (1).
    if opened_numbers.count("1") > 1:
        current = "front"
        for at, row in enumerate(rows):
            if row[2] == "heading":
                current = row[0]
            elif row[0].startswith("("):
                rows[at] = ("%s %s" % (current, row[0]),) + tuple(row[1:])

    defects.write(stem, "paper_sift.py", fixed)

    forms, edges = web(web_rows, alphabet + "".join(label_set), language)

    base = os.path.join(WORK, stem)
    with open(base + ".draft.tsv", "w", encoding="utf-8", newline="") as handle:
        handle.write("where\twho\tkind\tform\tgloss\n")
        for row in rows:
            handle.write("\t".join(" ".join(str(one).split()) for one in row) + "\n")
    with open(base + ".alphabet.txt", "w", encoding="utf-8", newline="") as handle:
        handle.write("char\tcodepoint\toutside_english\tinside_english\tshare\trole\tname\n")
        for role, held in (("letter", letters), ("label", labels)):
            for symbol, out_count, in_count, share in held:
                handle.write("%s\tU+%04X\t%d\t%d\t%.2f\t%s\t%s\n"
                             % (symbol, ord(symbol), out_count, in_count, share, role,
                                unicodedata.name(symbol, "?")))
    with open(base + ".web.tsv", "w", encoding="utf-8", newline="") as handle:
        handle.write("first\tsecond\tkind\tseen\n")
        for (first_form, second_form, kind), seen in sorted(edges.items()):
            handle.write("%s\t%s\t%s\t%d\n" % (first_form, second_form, kind, seen))
    with open(base + ".residue.tsv", "w", encoding="utf-8", newline="") as handle:
        handle.write("line\tpage\tengine\ttoken\ttext\n")
        for one in leftover:
            handle.write("%d\t%d\t%s\t%s\t%s\n" % one)

    verdicts = collections.Counter(second.values())
    print("lines %d: first pass %s" % (len(lines), dict(collections.Counter(first.values()))))
    print("against the paper's own English (%d lines, %d words): %s"
          % (len(paper_english), len(vocabulary), dict(verdicts)))
    print("alphabet %d letters: %s" % (len(letters), alphabet))
    print("labels %d: %s" % (len(labels), "".join(sorted(label_set))))
    edge_kinds = collections.Counter(kind for (_, _, kind) in edges.elements())
    print("web %d forms, edges %s" % (len(forms), dict(edge_kinds)))
    print("draft %d rows %s" % (len(rows), dict(collections.Counter(one[2] for one in rows))))
    print("residue %d tokens neither the paper's English nor the alphabet" % len(leftover))


if __name__ == "__main__":
    main()
