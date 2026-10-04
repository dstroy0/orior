"""Examples the page sets a word over its gloss, one to a line, read off the page again.

A paper that stacks each word of an example on its gloss, tsut over tsut and re=núxwenxw over
DET=woman, reaches the text layer as one line a word and one a gloss. The engine keeps some of
those examples and runs others into the prose after them, in Oliver the words of (2) and its
translation in the note that opens the next paragraph. The pairs op names the examples to read
again, and each is rebuilt from the page:

  (N) HEAD or (N) Context: ...     the sentence, or a context note and the sentence after it
  WORD / GLOSS                     a segmentation row and a gloss row for each pair of lines
  ‘TRANSLATION’ SOURCE             the translation, the source at its right a citation row
  Consultant’s comment: “...”      a speaker comment
  (sf | RI | 11.21.2023)           the tag on a line of its own, a citation row
  b. ...                           the next sub-example, (Nb)

The engine's rows for the example go, and so does the example text it left in a note: the note
keeps only the prose after the translation, and the cited forms the engine took from the leaked
words between the example and that note go with it.
"""
import os
import re

import residue

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import PRIVATE  # noqa: E402
HEAD = re.compile(r"^\((\d{1,3})\)\s+(\S.*)$")
LETTER = re.compile(r"^([a-h])\.\s+(\S.*)$")
# A translation can carry a judgment before its quote, # ‘I got stolen.’, or name the reading it
# is, Target: ‘Meat is cooked in this pot.’
TRANSLATION = re.compile(r"^(?:(?:Intended|Target|Actual):\s*)?(?:[*#?]{1,2}\s*)?[‘“]")
# A published source at the right of a translation, Kuipers (1974:117), or a bracketed tag or
# source, (sf, vt | RI | 11.21.2023) and (Kuipers 1974:86).
PUBLISHED = (r"[A-Z][A-Za-z’'-]+(?: (?:and|&) [A-Z][A-Za-z’'-]+)?(?: et al\.)?"
             r" \((?:\d{4}[a-z]?(?::[\d–-]+)?|unpublished|in prep\.)\)")
# A category in brackets and a cross-reference can stand before the tag, [noun] (sf | EP.2023/02/11)
# and (cf. (14)) (sf | EP.2022/10/29), and go with it. A category alone, [verb], is a tag too; a
# remark in brackets, [indicating their instruments], stays in the translation.
SOURCE = re.compile(r"^(.*?[’”.?!])\s*(%s|(?:(?:\[[^\[\]]*\]|\((?:[^()]|\([^()]*\))*\))\s*)*"
                    r"(?:\([^()]*\)|\[[^\[\]\s]*\]))\s*$" % PUBLISHED)
# What stands after a translation on lines of its own: a comment, a bracketed tag or remark, or a
# published source, Kuipers (1974:104) under (27).
AFTER = re.compile(r"^(?:Consultant|Comment|\(|%s$)" % PUBLISHED)
# Roots set against each other, √səq vs. √səq̓, where the text layer can close the space before vs.
VERSUS = re.compile(r"\s*vs\.\s+")
# A list of roots can run past h., (16i) √zənp̓.
ROOT_LETTER = re.compile(r"^([a-l])\.\s+(\S.*)$")
# The kinds the engine gives the candidates it takes from an example's words.
CANDIDATES = ("cited form", "root", "cited affix")
# A conversation opens each turn on its speaker's initials, (1) KBG: xwúy̓ nke ʔiƛ̓ʔiƛ̓tis.
TURN = re.compile(r"^[A-Z]{2,3}:\s")


def page_lines(stem):
    """Each page line repaired as every tool here repairs it, with the page it stands on."""
    repair = residue.paper_repair(stem)
    with open(os.path.join(PRIVATE, "pagetext", stem + ".txt"), encoding="utf-8") as handle:
        raw = handle.read().split("\n")
    out = []
    page = 0
    for one in raw:
        marker = re.match(r"^===== page (\d+) =====$", one)
        if marker:
            page = int(marker.group(1))
            out.append((page, ""))
            continue
        out.append((page, " ".join(repair(one).split())))
    return out


def source_who(source):
    """A published source written as a who, Kuipers (1974:117) as Kuipers 1974:117; a tag gives none."""
    # A category or cross-reference before it, [adjective] (Watanabe 2003:66–68), is passed over.
    while True:
        tagged = re.match(r"^(?:\[[^\[\]]*\]|\((?:[^()]|\([^()]*\))*\))\s+(?=\S)", source)
        if not tagged:
            break
        source = source[tagged.end():]
    published = re.match(r"^\(?([A-Z][^()|]*?) \(?(\d{4}[a-z]?(?::[\d–-]+)?|unpublished|in prep\.)\)?\)?$",
                         source)
    return "%s %s" % (published.group(1), published.group(2)) if published else ""


def head_at(lines, number):
    """The page line opening example number, passing over prose that opens on a reference to it,
    (2) to (6), which differ minimally, or (17) refers to a Co-initiator."""
    # A picture's number can stand alone on its line, (20).
    return next((at for at, (page, text) in enumerate(lines)
                 if (HEAD.match(text) and HEAD.match(text).group(1) == str(number)
                     and not re.match(r"^(?:to|and|or|through|refers|shows|illustrates|is|are)\b",
                                      HEAD.match(text).group(2))) or text == "(%s)" % number), None)


def blocks(lines, number, start=None, stacked=False, tiers=False):
    """The sub-examples of example number: label, page and its lines, in order.

    start is the page line of the head where the paper printed the number twice. A stacked example
    sets each word over its phonemic form and gloss, a line each, and no line of it runs on. In a
    paper whose examples are two tiers, tiers, the words over their gloss, the line after the words
    is their gloss and never the rest of a sentence."""
    start = head_at(lines, number) if start is None else start
    if start is None:
        raise SystemExit("pairs: no page line opens (%s)" % number)
    out = []
    label = "(%s)" % number
    rest = HEAD.match(lines[start][1]).group(2)
    lettered = LETTER.match(rest)
    if lettered:
        label, rest = "(%s%s)" % (number, lettered.group(1)), lettered.group(2)
    current = [label, lines[start][0], [rest], {}]
    stage = "head"
    footnote_page = None

    def goes_on(at):
        # The first line of the next page is the next sub-example: the lines between, a footnote
        # run on from the page before, (29) e. and f. of Lyon, are passed over.
        for later_page, later_text in lines[at + 1:]:
            if later_page == lines[at][0] or not later_text or later_text.isdigit():
                continue
            nxt = re.match(r"^([a-z])\.\s", later_text)
            return bool(nxt) and nxt.group(1) == chr(ord(current[0][-2]) + 1)
        return False

    for at, (page, text) in enumerate(lines[start + 1:], start + 1):
        if not text or text.isdigit():
            continue
        if HEAD.match(text):
            break
        # A footnote at the foot of the page is passed over to the next page, where a stacked
        # example can go on, b. under (4) past footnote 3. Elsewhere the footnote ends it: the b.
        # after Sardinha's (7) is a display of forms and meanings.
        closed = (stacked or tiers) and stage in ("translation", "after") and re.search(r"[’”)\]]\d?$", current[2][-1])
        if closed and (re.match(r"^\d{1,2} \S" if tiers else r"^\d{1,2} [-A-Z]", text) or
                       (tiers and not LETTER.match(text) and not AFTER.match(text) and goes_on(at))):
            footnote_page = page
            continue
        if footnote_page == page:
            continue
        lettered = LETTER.match(text)
        # A list can run past h., (73i) to (73k), each letter after the one before it.
        later = re.match(r"^([i-z])\.\s+(\S.*)$", text)
        if later and current[0][-2:-1] == chr(ord(later.group(1)) - 1):
            lettered = later
        if lettered and stage in ("translation", "after"):
            out.append(current)
            current = ["(%s%s)" % (number, lettered.group(1)), page, [lettered.group(2)], {}]
            stage = "head"
            continue
        # A second speaker's turn after the first one's translation, F: t̓anɛ́t after Me: tam č̓ɛ,
        # is its own sub-example, (62F), as the paper cites it.
        speaker = re.match(r"^([A-Z][a-z]?):\s+\S", text)
        if speaker and stage in ("translation", "after") and \
                any(re.match(r"^[A-Z][a-z]?:\s", one) for one in current[2][:2]):
            out.append(current)
            current = ["(%s%s)" % (number, speaker.group(1)), page, [text], {}]
            stage = "head"
            continue
        if stage == "after":
            # A remark in brackets the line end broke goes on to the line that closes it.
            if current[2][-1].startswith("(") and not current[2][-1].endswith(")"):
                current[2][-1] += " " + text
                continue
            # So does a comment whose quote the line end broke, "You probably could… over I mean ...".
            if current[2][-1].startswith(("Consultant", "Comment")) and \
                    current[2][-1].count("“") > current[2][-1].count("”"):
                current[2][-1] += " " + text
                continue
            if AFTER.match(text):
                current[2].append(text)
                continue
            # A comment in the language has its English on the line under it, (46) of Lyon.
            if TRANSLATION.match(text) and current[2][-1].startswith(("Comment", "Consultant")):
                current[2].append(text)
                continue
            break
        if TRANSLATION.match(text) and stage != "head":
            stage = "translation"
        elif stage == "translation" and not re.search(r"[’”)]\d?$", current[2][-1]):
            # A word the line end broke, com- ing, is one word again.
            # The engine's note keeps the break, and the text as set is kept to find it by.
            current[3][len(current[2]) - 1] = current[3].get(len(current[2]) - 1, current[2][-1]) + " " + text
            if re.search(r"[a-z]-$", current[2][-1]) and text[:1].islower():
                current[2][-1] = current[2][-1][:-1] + text
            else:
                current[2][-1] += " " + text
            continue
        elif stage == "translation":
            stage = "after"
            if AFTER.match(text):
                current[2].append(text)
                continue
            break
        elif stage == "head":
            stage = "pairs"
            if current[2][0].startswith("Context:"):
                # A context runs on to the line that closes it, and a colon closes one that sets up
                # the sentence under it, I tell him: in Huijsmans. A bare Context: heads its lines.
                if not re.search(r"[.?!)]$", current[2][0]) and \
                        not (current[2][0].endswith(":") and current[2][0] != "Context:"):
                    current[2][0] += " " + text
                    stage = "head"
                    continue
                # A context of two sentences, the second setting up the one under it, ... at the
                # store. / I decide I want one of the big blue ones and tell the lady: in (15) of
                # Huijsmans, runs on to that second sentence.
                if text.endswith(":") and len(re.findall(r"\b(?:I|you|he|she|we|they|the|a|and|to|of|it)\b",
                                                         text)) >= 3:
                    current[2][0] += " " + text
                    stage = "head"
                    continue
                # A context over the first sub-example, (60) Context: ... then a. * iʔ siwɬkʷ, opens it.
                lettered = LETTER.match(text)
                if lettered and current[0] == "(%s)" % number:
                    current[0], text = "(%s%s)" % (number, lettered.group(1)), lettered.group(2)
                # A context the pairs follow straight on, with no sentence of its own, (43).
                if not stacked and " " not in text and not re.search(r"[.?!”’\"…]$", text):
                    current[2].append("")
                    current[2].append(text)
                    continue
                current[2].append(text)
                stage = "sentence"
                continue
            # A sentence the line end broke goes on, in a line with spaces or one that closes it.
            # A turn of a conversation is set a tier a line and never breaks.
            if not stacked and not tiers and not TURN.match(current[2][0]) and \
                    not re.search(r"[.?!”’\"…:]$", current[2][0]) and \
                    (" " in text or re.search(r"[.?!”’\"…]$", text)):
                current[2][0] += " " + text
                stage = "head"
                continue
        elif stage == "sentence":
            stage = "pairs"
            if " " in text and not stacked and not tiers:
                current[2][-1] += " " + text
                stage = "sentence"
                continue
        current[2].append(text)
    out.append(current)
    return out


def rows_of(label, page, lines, context, stacked=False):
    """The oracle rows of one sub-example from its page lines."""
    authors, language = context.AUTHORS, getattr(context, "LANG", "")
    out = []
    at = 0

    def row(who, kind, form, gloss="page %d, pairs" % page, same=False):
        nonlocal at
        at += 0 if same else 1
        out.append(["%s line %d" % (label, at), who, kind, form, gloss])

    rest = list(lines)
    if rest[0].startswith("Context:") and len(rest) < 2:
        raise SystemExit("pairs: %s is a context with no sentence after it" % label)
    if rest[0].startswith("Context:"):
        row(authors, "note", rest.pop(0))
    sentence = rest.pop(0)
    pairs = []
    while rest and not TRANSLATION.match(rest[0]):
        pairs.append(rest.pop(0))
    # An example set in two tiers, the words over their gloss, has no sentence above them: the
    # first line is the words, and a long one wraps, words and gloss again under the first two.
    if sentence and len(pairs) % 2 == 1 and getattr(context, "TIERS", False) and not stacked:
        pairs.insert(0, sentence)
        while pairs:
            words = pairs.pop(0)
            row(language, "segmentation" if re.search(r"[-=√•{\[<]", words) else "transcription", words)
            row(language, "gloss", pairs.pop(0))
    elif sentence:
        row(language, "transcription", sentence)
    # A long turn wraps, its sentence, segmentation and gloss set again under the first three, and a
    # stacked example sets every word so. The stop that ends the sentence can stand on a line of its
    # own under the last word's gloss, ʔət χaƛ̓ / ʔətᶿ=x̌aƛ̓ / 1sg.poss=desire / . in (37) of
    # Huijsmans: it closes that word.
    if stacked and len(pairs) % 3 == 0 and len(pairs) > 3 and pairs[-1] in (".", "?", "!"):
        stop = pairs.pop()
        pairs[-3] += stop
    if stacked and len(pairs) % 3 != 2:
        print("pairs: %s is no word over form and gloss stack: %s" % (label, " / ".join(pairs)))
    if (TURN.match(sentence) or stacked) and len(pairs) > 2 and len(pairs) % 3 == 2:
        row(language, "segmentation", pairs[0])
        row(language, "gloss", pairs[1])
        for said, word, gloss in zip(pairs[2::3], pairs[3::3], pairs[4::3]):
            row(language, "transcription", said)
            row(language, "segmentation", word)
            row(language, "gloss", gloss)
        pairs = []
    if len(pairs) % 2:
        print("pairs: %s has an odd count of word and gloss lines: %s" % (label, " / ".join(pairs)))
    for word, gloss in zip(pairs[0::2], pairs[1::2]):
        row(language, "segmentation", word)
        row(language, "gloss", gloss)
    for text in rest:
        sourced = SOURCE.match(text)
        if TRANSLATION.match(text):
            said, source = (sourced.group(1), sourced.group(2)) if sourced else (text, "")
            cited = source_who(source)
            row(cited or authors, "translation", said,
                "page %d, pairs, cited %s" % (page, cited) if cited else "page %d, pairs" % page)
            if source:
                row(authors, "citation", source, "the tag or source at the right of the translation", True)
        elif text.startswith(("Consultant", "Comment")):
            tagged = re.match(r"^(.*[”’.])\s*(\([a-z, ]+\|[^()]*\))$", text)
            row(authors, "speaker comment", tagged.group(1) if tagged else text)
            if tagged:
                row(authors, "citation", tagged.group(2), "the tag or source at the right of the comment", True)
        elif re.match(r"^%s$" % PUBLISHED, text) or \
                (re.match(r"^\([^()]*\)$", text) and len(text) < 45 and "‘" not in text):
            row(authors, "citation", text, "page %d, pairs" % page)
        else:
            # A remark of the author's on the example, (The speaker indicated that ...).
            row(authors, "note", text)
    # A source on a line of its own is the translation's as much as one at its right.
    cited = next((source_who(one[3]) for one in out if one[2] == "citation" and source_who(one[3])), "")
    for one in out:
        if one[2] == "translation" and one[1] == authors and cited:
            one[1] = cited
            one[4] = "%s, cited %s" % (one[4], cited)
    return out, [word for one in lines for word in one.split()]


def apply(out, context, example_of):
    """Rebuild in out, in place, each example the pairs op or the lines op names.

    The lines op names examples set one tier a line, the sentence, its segmentation and its gloss,
    read the same way: the segmentation line and the gloss line under it are one pair, and the
    guard against a pair line holding a sentence is off.
    """
    named = [(spec, True, False) for spec in getattr(context, "PAIRS", ())] + \
        [(spec, False, False) for spec in getattr(context, "LINES", ())] + \
        [(spec, False, True) for spec in getattr(context, "STACKS", ())]
    if not named:
        return
    lines = page_lines(context.STEM)
    for spec, guarded, stacked in named:
        built, words, ends = [], set(), []
        # N:K is the Kth (N) where the paper printed the number twice.
        number, start, kept_pages = nth_head(lines, spec)
        found = blocks(lines, number, start, stacked, getattr(context, "TIERS", False))
        # An example set some other way, a formula over the prose after it, is left as the engine
        # read it: a pair line holding a sentence, or a word with no gloss under it.
        pair_lines = []
        for label, page, text, raw in found:
            body = text[2:] if text[0].startswith("Context:") else text[1:]
            pair_lines += [one for one in body if not TRANSLATION.match(one)
                           and not AFTER.match(one)]
        if guarded and any(one.count(" ") > 2 for one in pair_lines):
            print("pairs: (%s) is no word over gloss example, left as the engine read it" % number)
            continue
        for label, page, text, raw in found:
            rows, used = rows_of(label, page, text, context, stacked)
            built += rows
            words |= set(used) | {word for one in raw.values() for word in one.split()} | {label[-2] + "."}
            ends.append((text[-1], raw.get(len(text) - 1, text[-1])))
        # The engine can give the number to prose that cites it pages away, (119), k̓ʷmi ‘small’ (160);
        # only its rows on the example's pages, and the page after where it runs on, give way, short
        # of the page the number is printed again on.
        pages = (min(one[1] for one in found), max(one[1] for one in found) + 1)
        if kept_pages:
            pages = (pages[0], min(pages[1], kept_pages[1]))
        replace(out, number, built, words, ends, example_of, pages=pages)


def english(out, context, example_of):
    """Rebuild in out, in place, each English example the english op names, one row a sentence.

    (4) a. Hazel likes a boy. b. Hazel likes snow. is an English (4a) and (4b), a label at the right,
    [MASS], kept in the gloss, and a caption before the first letter, (1) Count nouns:, a note.
    """
    numbers = getattr(context, "ENGLISH", ())
    if not numbers:
        return
    lines = page_lines(context.STEM)
    for number in numbers:
        start = head_at(lines, number)
        if start is None:
            raise SystemExit("english: no page line opens (%s)" % number)
        page = lines[start][0]
        taken = [lines[start][1]]
        footnote_page = None
        for other, text in lines[start + 1:]:
            if not text or text.isdigit() or other == footnote_page:
                continue
            # The footnotes at the foot of the page are passed over, (6) e. goes on on the next.
            if len(taken) > 1 and re.match(r"^\d{1,2} \S", text):
                footnote_page = other
                continue
            # A caption can run over lines before the first letter, (7) Context: ... A asks:.
            lettered = any(LETTER.match(one) for one in taken[1:]) or \
                re.search(r"(?:^|\s)[a-h]\.\s", HEAD.match(taken[0]).group(2))
            # A sentence closed on its line, (1) Mt. Everest is taller than Mt. Fuji., is the example.
            if not LETTER.match(text) and (lettered or len(taken) > 3 or re.search(r"[.?!’”)]$", taken[-1])):
                break
            taken.append(text)
        joined = " ".join(taken)
        parts = re.split(r"(?:^|\s)([a-h])\.\s+", HEAD.match(joined).group(2))
        built = []
        if parts[0].strip() and len(parts) == 1:
            # An example of one English sentence and no letters, (1) Mt. Everest is taller than Mt. Fuji.
            built.append(["(%s) line 1" % number, "English", "transcription", parts[0].strip(),
                          "page %d, English" % page])
        elif parts[0].strip():
            built.append(["(%s) line 1" % number, context.AUTHORS, "note", parts[0].strip(),
                          "page %d, the caption" % page])
        for letter, item in zip(parts[1::2], parts[2::2]):
            # A label at the right, [MASS], or set apart in lower case after the sentence,
            # Somebody ate the last piece of cake. indefinite Initiator.
            labelled = re.match(r"^(.*?)\s*(\[[A-Z]+\])$", item.strip()) or \
                re.match(r"^(.*?[.!?])\s+([a-z][A-Za-z -]*)$", item.strip())
            said, label = (labelled.group(1), labelled.group(2)) if labelled else (item.strip(), "")
            said = re.sub(r"([.!?])\d{1,2}$", r"\1", said)
            built.append(["(%s%s) line 1" % (number, letter), "English", "transcription", said,
                          "page %d, English%s" % (page, ", " + label if label else "")])
        words = set(joined.split()) | {"(%s)" % number}
        replace(out, number, built, words, [(taken[-1], taken[-1])], example_of,
                opens="(%s) " % number)


def columns(out, context, example_of):
    """Rebuild in out, in place, each example the columns op names, its sub-examples side by side.

    (1) a. SȻOTI  b. TI,TOS  c. SḴÁLEX sets the letters across one line and each tier under them in
    the same columns, skʷá.ti  tiʔ.tás  sqé.ləx̌ and then ‘crazy, insane’  ‘bucking tide’. A tier
    parts at its runs of two spaces or more, or at each space where that gives a cell a letter, and
    a translation tier at each opening quote. The op names the kinds of the tiers above the
    translation, in order, and a source on a line of its own after the last group, (Montler 2018),
    is whose each translation is. A group with an empty cell is a picture and stops the example.
    """
    named = getattr(context, "COLUMNS", ())
    if not named:
        return
    lines = page_lines(context.STEM)
    repair = residue.paper_repair(context.STEM)
    with open(os.path.join(PRIVATE, "pagetext", context.STEM + ".txt"), encoding="utf-8") as handle:
        raw = [repair(one).strip() for one in handle.read().split("\n")]
    letters = re.compile(r"(?:^|\s)(\*?[a-h])\.(?:\s+|$)")
    for kinds, spec in named:
        number, start, pages = nth_head(lines, spec)
        page = lines[start][0]
        caption = HEAD.match(lines[start][1]).group(2)
        built, words, source, taken = [], set(lines[start][1].split()), "", [lines[start][1]]
        groups, group = [], None
        texts = [(lines[start][0], caption, caption)] + \
            [(page_of, text, raw[at]) for at, (page_of, text) in enumerate(lines) if at > start]
        if not letters.match(caption):
            if caption.strip():
                built.append(["(%s) line 1" % number, context.AUTHORS, "note", caption,
                              "page %d, the caption" % page])
            texts = texts[1:]
        footnote_page = None
        for page_of, text, spaced in texts:
            # A footnote at the page foot, (60) broken over the page by footnote 13, is passed over
            # to the next page.
            if page_of == footnote_page:
                continue
            if (group or groups) and re.match(r"^\d{1,2} [A-Z]", text) and not TRANSLATION.match(text):
                footnote_page = page_of
                continue
            if HEAD.match(text) and text != caption:
                break
            if not text or text.isdigit():
                if group:
                    groups.append(group)
                    group = None
                continue
            if letters.match(text):
                if group:
                    groups.append(group)
                parts = letters.split(text)
                labels, cells = parts[1::2], [one.strip() for one in parts[2::2]]
                # A row of a set whose columns are numbered, (8) a. i. SḴÁLEX ii. SPÁ,W̱EṈ, is (8a.i)
                # and (8a.ii).
                if len(labels) == 1 and re.match(r"^[ivx]+\.\s", cells[0]):
                    romans = re.split(r"(?:^|\s)([ivx]+)\.\s+", cells[0])
                    labels = ["%s.%s" % (labels[0], one) for one in romans[1::2]]
                    cells = [one.strip() for one in romans[2::2]]
                group = {"page": page_of, "letters": labels, "tiers": [cells]}
                taken.append(text)
                continue
            if re.match(r"^\([A-Z][^()]*\d{4}[a-z]?(?::[\d–-]+)?\)$", text) and (groups or group):
                source = text
                taken.append(text)
                break
            if not group:
                break
            count = len(group["letters"])
            if TRANSLATION.match(text):
                # A source at the right of the last translation, ‘skin’ (Leonard 2007), closes the
                # example.
                trailing = re.match(r"^(.*?)\s*(\([A-Z][^()]*\d{4}[a-z]?(?::[\d–-]+)?\))$", text)
                said = trailing.group(1) if trailing else text
                if trailing:
                    source = trailing.group(2)
                cells = [one.strip() for one in re.split(r"\s*(?=[‘“])", said) if one.strip()]
            else:
                cells = [one for one in re.split(r"\s{2,}", spaced) if one]
                if len(cells) != count:
                    cells = text.split()
            if len(cells) != count:
                print("columns: (%s) a tier of %d cells under %d letters: %s" % (number, len(cells), count, text))
                groups.append(group)
                group = None
                break
            group["tiers"].append(cells)
            taken.append(text)
        if group:
            groups.append(group)
        cited = source_who(source)
        for group in groups:
            if any(not cell for cell in group["tiers"][0]):
                print("columns: (%s) a group with an empty cell, a picture, stops the example" % number)
                break
            for column, letter in enumerate(group["letters"]):
                label = "(%s%s)" % (number, letter.lstrip("*"))
                at, form_tier = 0, 0
                for tier in group["tiers"]:
                    cell = tier[column]
                    at += 1
                    if TRANSLATION.match(cell):
                        built.append(["%s line %d" % (label, at), cited or context.AUTHORS, "translation", cell,
                                      "page %d, columns%s" % (group["page"], ", cited " + cited if cited else "")])
                        continue
                    kind = kinds[min(form_tier, len(kinds) - 1)]
                    form_tier += 1
                    built.append(["%s line %d" % (label, at), context.LANG, kind,
                                  ("*" if letter.startswith("*") and form_tier == 1 else "") + cell,
                                  "page %d, columns" % group["page"]])
        if source and built:
            last = re.match(r"^(\(\S+\)) line (\d+)$", built[-1][0])
            built.append(["%s line %d" % (last.group(1), int(last.group(2)) + 1), context.AUTHORS, "citation",
                          source, "the source under the example"])
        words |= {word for one in taken for word in one.split()} | {"%s." % one for one in "abcdefgh"}
        replace(out, number, built, words, [(taken[-1], taken[-1])], example_of, opens="(%s)" % number,
                pages=pages)


def nth_head(lines, spec):
    """The number, the page line opening it and the pages it may stand on, for a spec of N or N:K,
    the Kth example the page numbers (N) where the paper printed a number twice. A spec of N:K
    keeps the rebuild to the pages from that head up to the page before the next (N)."""
    number, _, nth = spec.partition(":")
    start = head_at(lines, number)
    if start is None:
        raise SystemExit("pairs: no page line opens (%s)" % number)
    for _ in range(int(nth or 1) - 1):
        found = head_at(lines[start + 1:], number)
        if found is None:
            raise SystemExit("pairs: no page line opens (%s) a %s time" % (number, nth))
        start = start + 1 + found
    if not nth:
        return number, start, None
    after = head_at(lines[start + 1:], number)
    return number, start, (lines[start][0], lines[start + 1 + after][0] - 1 if after is not None else 10 ** 6)


def display(out, context, example_of):
    """Rebuild in out, in place, each example the display op names, a line or two set apart.

    An OT ranking, (11) WSP >> TROCHEE, a constraint defined, (9) WEIGHT-TO-STRESS (WSP): Assign
    one violation ..., or a picture with no text layer and at most a caption, (17) HDA foot
    structures in SENĆOŦEN. The engine reads the prose after one of these as the example's
    lines. That prose goes back to the paragraph it opens: into the note after the example where
    that note goes on in lower case, or a note of its own in the section before. A spec of N+K
    takes the K printed lines after the head whole, a row each, the diagram of (31) in Schneider
    and Gerdts, yə= V1 (yə=) V2 | a rule of U+2014 | over what it means.
    """
    named = getattr(context, "DISPLAY", ())
    if not named:
        return
    lines = page_lines(context.STEM)
    for kind, spec in named:
        spec, _, taken = spec.partition("+")
        number, start, pages = nth_head(lines, spec)
        page = lines[start][0]
        caption = HEAD.match(lines[start][1]).group(2) if HEAD.match(lines[start][1]) else ""
        parts = [caption]
        for other, text in lines[start + 1:]:
            if taken and len(parts) <= int(taken):
                if text and not text.isdigit():
                    parts.append(" ".join(text.split()))
                continue
            if taken or not text or text.isdigit() or HEAD.match(text):
                break
            # A lettered line, or a ranking under a ranking, (65) one language's ranking a line.
            if LETTER.match(text) or (kind != "picture" and ">>" in parts[-1] and ">>" in text):
                parts.append(text)
            elif kind != "picture" and ":" in parts[-1] and not parts[-1].endswith("."):
                parts[-1] += " " + text
            else:
                break
        built = []
        if kind == "picture":
            said = re.sub(r"(?:^|\s)\*?[a-h]\.(?=\s|$)", " ", " ".join(parts)).strip()
            built.append(["(%s)" % number, context.AUTHORS, "note", said or "(%s)" % number,
                          "page %d, %s" % (page, "the caption of a picture with no text layer" if said
                                           else "drawn as a picture with no text layer")])
        else:
            counts = {}
            for text in parts:
                lettered = LETTER.match(text)
                label = "(%s%s)" % (number, lettered.group(1)) if lettered else "(%s)" % number
                counts[label] = counts.get(label, 0) + 1
                built.append(["%s line %d" % (label, counts[label]), context.AUTHORS, kind,
                              lettered.group(2) if lettered else text, "page %d, the display" % page])
        shown = [" ".join(one.split()) for one in parts]
        own = [at for at, one in enumerate(out) if re.match(r"^\(%s[a-z]?\)$" % number, example_of(one[0]))
               and in_pages(one, pages)]
        if not own:
            # The engine ran the display into a note, (10) TROCHEE: ... inside the prose after (9):
            # the note keeps what stands before it, and what stands after it is a note of its own.
            opening = "(%s) %s" % (number, shown[0]) if shown[0] else "(%s)" % number
            held = next((at for at, one in enumerate(out) if one[2] == "note" and not example_of(one[0])
                         and opening in " ".join(one[3].split()) and in_pages(one, pages)), None)
            if held is None and kind == "picture":
                # A picture the engine dropped whole stands before the note of the prose after it.
                following = next((text for other, text in lines[start + 1:]
                                  if text and not text.isdigit() and not LETTER.match(text)
                                  and not re.fullmatch(r"(?:[a-h]\.\s*)+", text)), "")
                at = next((at for at, one in enumerate(out) if one[2] == "note"
                           and following and one[3].startswith(following[:30])), None)
                if at is None:
                    raise SystemExit("display: (%s) is in no row and no note, nor the prose after it" % number)
                out[at:at] = built
                continue
            if held is None:
                raise SystemExit("display: (%s) is in no row and no note" % number)
            before, _, rest = " ".join(out[held][3].split()).partition(opening)
            rest = strip_shown(rest, shown[1:])
            tail = [[out[held][0], out[held][1], "note", rest, out[held][4]]] if rest else []
            out[held + 1:held + 1] = built + tail
            if before.strip():
                out[held][3] = before.strip()
            else:
                del out[held]
            continue
        prose = []
        for at in own:
            said = strip_shown(" ".join(out[at][3].split()), shown)
            if said and said not in ("(%s)" % number,) and not re.fullmatch(r"(?:[a-h]\.\s*)+", said):
                prose.append([out[at][0], out[at][1], out[at][2], said, out[at][4]])
        give_back(out, own, prose, built, page, context, example_of)
        words = {word for one in parts for word in one.split()}
        replace(out, number, built, words, [], example_of, opens="(%s)" % number, pages=pages)


def give_back(out, own, prose, built, page, context, example_of):
    """The prose the engine read as an example's lines, prose, given back to its paragraph: joined to
    the note after the example where that note goes on in lower case, past the cited forms the engine
    took from it, or else a note of its own in the section before, added to built."""
    if not prose:
        return
    said = " ".join(one[3] for one in prose)
    after = own[-1] + 1
    # The candidates the engine took from the paragraph stand between it and its rest.
    while after < len(out) and out[after][2] in ("cited form", "cited affix", "language", "name", "notation") \
            and not example_of(out[after][0]):
        after += 1
    if after < len(out) and out[after][2] == "note" and not example_of(out[after][0]) \
            and out[after][3][:1].islower():
        out[after][3] = said + " " + out[after][3]
    else:
        where = next((one[0] for one in reversed(out[:own[0]]) if one[0].startswith("§")), "§1")
        found = re.search(r"page \d+", prose[0][4])
        built.append([where, context.AUTHORS, "note", said, found.group(0) if found else "page %d" % page])


def table(out, context, example_of):
    """Rebuild in out, in place, each example the table op names, a caption, a header and a row a line.

    (2) Secondary labialization: over Boas APA pentl’ach gloss and koā'san → qʷasəm qwasem ‘flower’
    [YP]: each form cell a row of the kind the op names for its column, KIND@WHO where the column is
    someone's own, the gloss a translation and the source tag a citation. A kind of rule takes the
    whole line as one rule row, y/ī → y. The cells of a line share its where, (2) line 3, as the
    engine sets a table row. The rows run while a line holds an arrow or a quoted gloss.
    """
    named = getattr(context, "TABLE", ())
    if not named:
        return
    lines = page_lines(context.STEM)
    tags = getattr(context, "TABLE_TAGS", {})
    for kinds, spec in named:
        number, start, pages = nth_head(lines, spec)
        page = lines[start][0]
        caption = HEAD.match(lines[start][1]).group(2)
        built = [["(%s) line 1" % number, context.AUTHORS, "note", caption, "page %d, the caption" % page]]
        taken, at = [lines[start][1]], 1
        for other, text in lines[start + 1:]:
            if not text or text.isdigit():
                continue
            row = "→" in text or "‘" in text
            if not row and at == 1 and len(text.split()) <= 5:
                at += 1
                built.append(["(%s) line %d" % (number, at), context.AUTHORS, "note", text, "page %d, heads the rows below" % other])
                taken.append(text)
                continue
            if not row:
                break
            at += 1
            taken.append(text)
            where = "(%s) line %d" % (number, at)
            if kinds == ("rule",):
                built.append([where, context.AUTHORS, "rule", text, "page %d, the table" % other])
                continue
            tag = re.search(r"\s*\[([A-Z]{1,3})\](\d{0,2})$", text)
            said = text[:tag.start()] if tag else text
            note = ", carries footnote %s" % tag.group(2) if tag and tag.group(2) else ""
            said = re.sub(r"(?<=[:’])\d{1,2}$", "", said.strip())
            quote = said.find("‘")
            gloss = said[quote:].strip() if quote >= 0 else ""
            cells = [one for one in said[:quote if quote >= 0 else len(said)].replace("→", " ").split()]
            if len(cells) != len(kinds):
                print("table: (%s) a line of %d cells under %d kinds: %s" % (number, len(cells), len(kinds), text))
                cells = [" ".join(cells)] if len(kinds) == 1 else cells + [""] * (len(kinds) - len(cells))
            for cell, kind in zip(cells, kinds):
                kind, _, who = kind.partition("@")
                if cell:
                    built.append([where, who or context.LANG, kind, cell, "page %d, the table%s" % (other, note)])
            if gloss:
                built.append([where, context.AUTHORS, "translation", gloss, "page %d, the table" % other])
            if tag:
                built.append([where, context.AUTHORS, "citation", "[%s]" % tag.group(1),
                              tags.get(tag.group(1), "the source of the row")])
        # Engine rows holding none of the table's lines are the prose after it.
        own = [at for at, one in enumerate(out) if re.match(r"^\(%s[a-z]?\)$" % number, example_of(one[0]))
               and in_pages(one, pages)]
        flat = " ".join(" ".join(taken).split())
        prose = [out[at] for at in own if " ".join(out[at][3].split()) not in flat]
        give_back(out, own, prose, built, page, context, example_of)
        words = {word for one in taken for word in one.split()}
        replace(out, number, built, words, [(taken[-1], taken[-1])], example_of, opens="(%s)" % number, pages=pages)


def tableau(out, context, example_of):
    """Rebuild in out, in place, each example the tableau op names, each printed line a note.

    (8) saˀsqʷ ‘fly just a little bit’ over the input µ + saˀ0.5 qʷ, the constraints a line each,
    *FLOAT or INTEGRITY, the weights w=7 w=7 w=3 and the candidates a. ☞ saˀsqʷ −0.5 −1 −1 −4.5:
    a constraint is no tier and the last digit of a score no footnote's mark. The lines run from the
    head to the blank line after the last candidate.
    """
    named = getattr(context, "TABLEAU", ())
    if not named:
        return
    lines = page_lines(context.STEM)
    for spec in named:
        number, start, pages = nth_head(lines, spec)
        page = lines[start][0]
        caption = HEAD.match(lines[start][1]).group(2)
        built = [["(%s) line 1" % number, context.AUTHORS, "note", caption, "page %d, set as an example" % page]]
        taken, candidates = [lines[start][1]], False
        for other, text in lines[start + 1:]:
            if not text:
                if candidates:
                    break
                continue
            if HEAD.match(text):
                break
            candidates = candidates or bool(re.match(r"^[a-h]\.\s", text))
            taken.append(text)
            built.append(["(%s) line %d" % (number, len(taken)), context.AUTHORS, "note", text,
                          "page %d, set as an example" % other])
        own = [at for at, one in enumerate(out) if re.match(r"^\(%s[a-z]?\)$" % number, example_of(one[0]))
               and in_pages(one, pages)]
        flat = " ".join(" ".join(taken).split())
        prose = [out[at] for at in own if " ".join(out[at][3].split()) not in flat]
        give_back(out, own, prose, built, page, context, example_of)
        words = {word for one in taken for word in one.split()}
        replace(out, number, built, words, [(taken[-1], taken[-1])], example_of, opens="(%s)" % number, pages=pages)


def roots(out, context, example_of):
    """Rebuild in out, in place, each example the roots op names, a list of roots and their glosses.

    An item is a root or its variants and a gloss, a. √pəq, paq ‘white’, perhaps with a word built on
    it, (as in ƛ̓ál-ləx ‘stop (oneself)’), or roots set against each other, a. √səq vs. √səq̓, with their
    glosses on the line below in the same order, ‘split’ ‘crack’. Each root is a root row and each
    gloss a translation; the word it is found in is the item's second line, a segmentation and its
    translation. Roots set against each other take a line each.
    """
    named = getattr(context, "ROOTS", ())
    if not named:
        return
    lines = page_lines(context.STEM)
    for spec in named:
        number, start, pages = nth_head(lines, spec)
        page = lines[start][0]
        rest = HEAD.match(lines[start][1]).group(2)
        items, built, taken = [], [], [lines[start][1]]
        lettered = ROOT_LETTER.match(rest)
        if lettered:
            items.append(["(%s%s)" % (number, lettered.group(1)), page, lettered.group(2), None])
        elif rest.startswith("√"):
            items.append(["(%s)" % number, page, rest, None])
        else:
            built.append(["(%s) line 1" % number, context.AUTHORS, "note", rest, "page %d, the caption" % page])
        for other, text in lines[start + 1:]:
            if not text or text.isdigit():
                continue
            if HEAD.match(text):
                break
            lettered = ROOT_LETTER.match(text)
            if lettered:
                items.append(["(%s%s)" % (number, lettered.group(1)), other, lettered.group(2), None])
            elif text.startswith("‘") and items and VERSUS.search(items[-1][2]) and items[-1][3] is None:
                items[-1][3] = text
            else:
                break
            taken.append(text)
        for label, other, text, glosses in items:
            at = 0
            if VERSUS.search(text):
                cells = [one.strip() for one in re.split(r"\s*(?=‘)", glosses or "") if one.strip()]
                for side, cell in zip(VERSUS.split(text), cells + [""] * 5):
                    at += 1
                    for form in re.split(r",\s*|\s+~\s+", side.strip()):
                        built.append(["%s line %d" % (label, at), context.LANG, "root", form, "page %d, roots" % other])
                    said = re.match(r"^(‘.*’)\s*(.*)$", cell)
                    if said:
                        built.append(["%s line %d" % (label, at), context.AUTHORS, "translation", said.group(1),
                                      "page %d, roots%s" % (other, ", " + said.group(2).strip("()") if said.group(2) else "")])
                continue
            parsed = re.match(r"^(.*?)\s*(‘.*?(?:’|$))(\d{0,2})\s*(?:\(as in (.+?) (‘.*’)\))?\s*$", text)
            if not parsed:
                print("roots: %s is no root and gloss: %s" % (label, text))
                continue
            at += 1
            for form in re.split(r",\s*|\s+~\s+", parsed.group(1).strip()):
                built.append(["%s line %d" % (label, at), context.LANG, "root", form, "page %d, roots" % other])
            built.append(["%s line %d" % (label, at), context.AUTHORS, "translation", parsed.group(2),
                          "page %d, roots%s" % (other, ", carries footnote " + parsed.group(3) if parsed.group(3) else "")])
            if parsed.group(4):
                at += 1
                built.append(["%s line %d" % (label, at), context.LANG, "segmentation", parsed.group(4),
                              "page %d, roots, the word the root is found in" % other])
                built.append(["%s line %d" % (label, at), context.AUTHORS, "translation", parsed.group(5),
                              "page %d, roots" % other])
        own = [at for at, one in enumerate(out) if re.match(r"^\(%s[a-z]?\)$" % number, example_of(one[0]))
               and in_pages(one, pages)]
        flat = " ".join(" ".join(taken).split())
        prose = [out[at] for at in own if " ".join(out[at][3].split()) not in flat]
        if own:
            give_back(out, own, prose, built, page, context, example_of)
        words = {word for one in taken for word in one.split()}
        replace(out, number, built, words, [(taken[-1], taken[-1])], example_of, opens="(%s)" % number, pages=pages)


# A word of a list, a. dénxisa ‘drag net along shore’, its other definition in brackets before the
# gloss, n. ciá (ceyá) ‘draw water’. Two forms can stand side by side, ’iála ’eyála.
WORD_ITEM = re.compile(r"^(\*?[a-z])\.\s+(\S.*?)(?:\s+\(([^()‘’]+)\))?\s+(‘.*’)$")


def words(out, context, example_of):
    """Rebuild in out, in place, each example the words op names, a list of words, a. FORM ‘GLOSS’ a
    line, with a blank line or none between pairs.

    The head can name what the list shows, (12) -(l)ás ‘place’, an affix and its gloss, or a
    caption. The engine can read the list as other kinds and the paragraph after it as the last
    word's lines, (12h) In each of the example pairs above ...; that prose goes back to its
    paragraph, as the display op gives it back.
    """
    named = getattr(context, "WORDS", ())
    if not named:
        return
    lines = page_lines(context.STEM)
    for spec in named:
        number, start, pages = nth_head(lines, spec)
        page = lines[start][0]
        head = HEAD.match(lines[start][1]).group(2)
        items = []
        built = []
        if WORD_ITEM.match(head):
            items.append((page, head))
        else:
            named_form = re.match(r"^(\S+)\s+(‘.*’)$", head)
            if named_form:
                kind = "cited affix" if named_form.group(1).startswith("-") or \
                    named_form.group(1).endswith("-") else "cited form"
                built.append(["(%s) line 1" % number, context.LANG, kind, named_form.group(1),
                              "page %d, what the list shows" % page])
                built.append(["(%s) line 1" % number, context.AUTHORS, "translation", named_form.group(2),
                              "page %d" % page])
            else:
                built.append(["(%s) line 1" % number, context.AUTHORS, "note", head,
                              "page %d, the caption" % page])
        for other, text in lines[start + 1:]:
            if not text or text.isdigit():
                continue
            if not WORD_ITEM.match(text):
                break
            items.append((other, text))
        if not items:
            raise SystemExit("words: (%s) lists no word" % number)
        for other, text in items:
            letter, form, spelled, gloss = WORD_ITEM.match(text).groups()
            label = "(%s%s)" % (number, letter.lstrip("*"))
            # A caption setting two definitions against each other, (11) Examples of VV́ ~ əRV́, puts
            # the second of each pair beside the first, 'iála 'eyála.
            if "~" in head and len(form.split()) == 2 and not spelled:
                form, spelled = form.split()
            built.append(["%s line 1" % label, context.LANG, "transcription",
                          ("*" if letter.startswith("*") else "") + form, "page %d, words" % other])
            if spelled:
                built.append(["%s line 1" % label, context.LANG, "variant spelling", spelled,
                              "page %d, the other spelling, in brackets" % other])
            built.append(["%s line 1" % label, context.AUTHORS, "translation", gloss, "page %d, words" % other])
        shown = [head] + [text for other, text in items]
        own = [at for at, one in enumerate(out) if re.match(r"^\(%s[a-z]?\)$" % number, example_of(one[0]))
               and in_pages(one, pages)]
        prose = []
        for at in own:
            said = strip_shown(" ".join(out[at][3].split()), shown)
            if said and said != "(%s)" % number and not re.fullmatch(r"(?:[a-z]\.\s*)+", said) and \
                    not any(said in " ".join(one.split()) for one in shown):
                prose.append([out[at][0], out[at][1], out[at][2], said, out[at][4]])
        if own:
            give_back(out, own, prose, built, page, context, example_of)
        taken = {word for one in shown for word in one.split()} | {"%s." % one for one in "abcdefghijklmnopqrstuvwxyz"}
        replace(out, number, built, taken, [(shown[-1], shown[-1])], example_of, opens="(%s)" % number,
                pages=pages)


def wordlist(out, context, example_of):
    """Rebuild in out, in place, each table the wordlist op names by its caption, Table A1, a word
    and its gloss a line, under a header of column names the table prints again on each page.

    A gloss the cell wraps goes on over the lines after it, ‘dregs at the / bottom’, and a phrase can
    take two lines before its gloss, ʔe skə̣l̓na píx̣mek ʔa / ze / ‘buckskin was hung ...’. The engine
    reads the table as one note and each word as a candidate; both give way to a row a word, Table
    A1 line N, the word a transcription and its gloss a translation.
    """
    named = getattr(context, "WORDLIST", ())
    if not named:
        return
    lines = page_lines(context.STEM)
    for caption, plain in named:
        start = next((at for at, (page, text) in enumerate(lines) if text.startswith(caption + ":")), None)
        if start is None:
            raise SystemExit("wordlist: no page line opens %s:" % caption)
        header = next(text for page, text in lines[start + 1:] if text)
        page = lines[start][0]
        built = [[caption, context.AUTHORS, "note", lines[start][1], "page %d, the caption" % page],
                 [caption, context.AUTHORS, "note", header, "page %d, heads the rows below" % page]]
        items, pending, open_item = [], [], None
        for other, text in lines[start + 1:]:
            if not text or text.isdigit() or text == header:
                continue
            if re.match(r"^(?:\d+(?:\.\d+)* [A-Z]|References$|Appendix$)", text):
                break
            # A plain table ends at the next caption or at a note under it, *Speaker's translation.
            if plain and re.match(r"^(?:Table \d+:|\*)", text):
                break
            if open_item is not None:
                open_item[2] += " " + text
                if text.endswith("’"):
                    open_item = None
                continue
            if "‘" not in text and plain and " " in text:
                form, _, gloss = text.partition(" ")
                items.append([other, form, gloss.strip()])
                continue
            if "‘" not in text:
                pending.append(text)
                continue
            form, _, gloss = text.partition("‘")
            item = [other, " ".join(pending + [form.strip()]).strip(), "‘" + gloss.strip()]
            pending = []
            items.append(item)
            if not item[2].endswith("’"):
                open_item = item
        for at, (other, form, gloss) in enumerate(items, 1):
            built.append(["%s line %d" % (caption, at), context.LANG, "transcription", form,
                          "page %d, wordlist" % other])
            built.append(["%s line %d" % (caption, at), context.AUTHORS, "translation", gloss,
                          "page %d, wordlist" % other])
        taken = {word for one in [lines[start][1], header] + [one[1] + " " + one[2] for one in items]
                 for word in one.split()}
        held = next((at for at, one in enumerate(out) if one[2] == "note" and not example_of(one[0])
                     and " ".join(one[3].split()).startswith(lines[start][1])), None)
        # The engine can have read the table as rows of its own, Table 2 line 3.
        own = [at for at, one in enumerate(out) if one[0].startswith(caption + " line ")]
        if held is None and own:
            held = own[0]
        if held is None:
            raise SystemExit("wordlist: %s is in no note" % caption)
        where = out[held][0]
        # The candidates the engine took from the table's words go with it, and in a plain table
        # those of its page wherever the engine put them.
        words = taken | {part for word in taken for part in word.split()}
        gone = {held} | set(own) | {at for at, one in enumerate(out) if one[2] in CANDIDATES and one[3] in words
                                    and (one[0] == where or (plain and "page %d" % page in one[4]))}
        rest = [one for at, one in enumerate(out) if at not in gone]
        at = sum(1 for one in range(held) if one not in gone)
        out[:] = rest[:at] + built + rest[at:]


def strip_shown(text, shown):
    """text with the display lines it opens on taken off, in order, a lettered line with or without
    its letter."""
    text = text.strip()
    for one in shown:
        lettered = LETTER.match(one)
        for form in (one, lettered.group(2) if lettered else None):
            if form and text.startswith(form):
                text = text[len(form):].strip()
                break
    return text


def in_pages(row, pages):
    """Whether a row stands on the pages a spec of N:K keeps to, or any page for a spec of N."""
    if pages is None:
        return True
    page = re.search(r"page (\d+)", row[4])
    return page is None or pages[0] <= int(page.group(1)) <= pages[1]


def replace(out, number, built, words, ends, example_of, opens=None, pages=None):
    """The engine's rows for example number and the example text it left in notes give way to built.

    The notes are sought from the example's rows, or the note opening on the example's number where
    the engine gave it none, up to the next example or heading. A note of the example's words alone
    goes; a note holding the end of a sub-example's last line keeps the prose after it.
    """
    own = [at for at, one in enumerate(out)
           if re.match(r"^\(%s[a-z]?\)$" % number, example_of(one[0])) and in_pages(one, pages)]
    begin = own[0] if own else next((at for at, one in enumerate(out) if opens and one[2] == "note"
                                     and one[3].startswith(opens)), None)
    window = []
    for at, one in enumerate(out):
        if begin is not None and at < begin:
            continue
        if begin is not None and at > (own[-1] if own else begin) and \
                (example_of(one[0]) or one[2] == "heading"):
            break
        if one[2] == "note" and not example_of(one[0]):
            window.append(at)
    leaked = []
    for at in window:
        if set(out[at][3].split()) <= words:
            leaked.append(at)
            out[at][3] = ""
    for pair in ends:
        for at in window:
            one, cut = out[at], None
            if at in leaked:
                continue
            for last in pair:
                # A note that opens on the end of the last line, the translation the engine broke
                # across the page line, holds that end alone.
                opening = next((len(last) - start for start in range(len(last) - 20)
                                if one[3].startswith(last[start:])), 0)
                if opening and last not in one[3]:
                    cut = one[3][opening:]
                elif last in one[3]:
                    before, _, after = one[3].partition(last)
                    if set(before.split()) <= words:
                        cut = after
                    else:
                        print("pairs: (%s) ends in a note with prose before it: %s" % (number, one[3][:80]))
                if cut is not None:
                    break
            if cut is not None:
                # What follows is the next sub-example's opening, read again whole, or prose.
                one[3] = "" if set(cut.split()) <= words else cut.strip()
                leaked.append(at)
                break
    if os.environ.get("PAIRS_TRACE"):
        for at in leaked:
            print("pairs: (%s) takes %s, leaving: %s" % (number, out[at][0], out[at][3][:60]))
    first = own[0] if own else min(leaked, default=None)
    if first is None:
        raise SystemExit("pairs: (%s) has no rows and no note to stand at" % number)
    gone = set(own) | {at for at in leaked if not out[at][3]}
    # The cited forms the engine took from the example's words, up to the last note it ran into,
    # or in the run of cited forms straight after its rows.
    until = max(leaked, default=own[-1] if own else first)
    while until + 1 < len(out) and out[until + 1][2] in CANDIDATES:
        until += 1
    gone |= {at for at in range(first, until + 1) if out[at][2] in CANDIDATES
             and not example_of(out[at][0])
             and any(word == out[at][3] or (len(out[at][3]) > 3 and word.startswith(out[at][3]))
                     for word in words)}
    kept = [one for at, one in enumerate(out) if at not in gone and at < first]
    tail = [one for at, one in enumerate(out) if at not in gone and at >= first]
    out[:] = kept + built + tail
