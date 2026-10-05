"""Run anchor_sift's oracle_check on salishan_corpus papers without registering them.

usage: python residue.py <stem> [stem ...]

Points the library's own check at the private corpus: the paper's text layer under papers/, the
hand table under oracles/. The repair nearly every paper takes in paper_config is the space
closed after a stacked mark and then NFC. The letters counted as the language are TEXT_SPACE plus
every non-ASCII letter or mark the table's language rows use, the way paper_config reads a marks set
off an oracle's own form column.
"""
import io
import os
import re
import sys
import unicodedata

from workdir import SALISHAN as LIBRARY  # noqa: E402
from workdir import PRIVATE, ORACLES  # noqa: E402
from workdir import CORPUS  # noqa: E402
import tables  # noqa: E402
sys.path.insert(0, os.path.join(LIBRARY, "corpus_script_extraction"))
sys.path.insert(0, os.path.join(LIBRARY, "hand_extraction"))

import oracle_check  # noqa: E402
from paper_config import INSERTED_SPACE, SHARED  # noqa: E402
from repairs import composed, corrected, sequence  # noqa: E402

QUOTE_AFTER_MARK = re.compile("([̀-ͯ])(‘)")


def reopened(line):
    """The space before an opening quote put back where the inserted-space repair closed it.

    closed_after_marks takes a lone space after a stacked mark as inserted, and məq̓ ‘full’ becomes
    məq̓‘full’. No word carries an opening quote inside it, and a mark followed by ‘ is a word
    boundary every time.
    """
    line = QUOTE_AFTER_MARK.sub(r"\1 \2", line)
    # The same for a slash that opens a phonemic form, t̓ /t̓/. A slash that closes one, /c̓ /, has an
    # odd number of slashes before it on the line, and the space before it is the text layer's. A
    # slash that joins two forms, cəm̓/cmay, has no closing slash after it, and the page sets no space.
    out = []
    for at, symbol in enumerate(line):
        if symbol == "/" and at and unicodedata.combining(line[at - 1]) and \
                line[:at].count("/") % 2 == 0 and at + 1 < len(line) and \
                re.match(r"/[^\s/]+/", line[at:]):
            out.append(" ")
        out.append(symbol)
    return "".join(out)


SPACE_BEFORE_MARK = re.compile("(?<=[^\\W\\d_]|[̀-ͯ]) ([̀-ͯ])")


def attached(line):
    """A combining mark the text layer set after a space, k̓ə ̣pqns, put back on its letter.

    A combining mark cannot open a word, and the space before one is the text layer's. The letter
    can carry a mark already, √ʔə́ ̣sxe in Hall et al.

    A mora's µ subscript set after a stressed vowel's acute sits a space apart in the text layer,
    [kə́ µɬµpµ] in the tableaux of Hall et al., and joins its vowel inside the brackets.
    """
    return SPACE_AFTER_STACK.sub("", placed(line))


def placed(line):
    """attached without closing the space after a stacked mark: a page read from glyph positions
    sets that space where the page does, Cable's struck [Déix̱ x̱áat] in (47) and Chinookan -x̣̣
    would in Zenk."""
    return SPACE_MORA.sub("", SPACE_BEFORE_MARK.sub(r"\1", line))


SPACE_MORA = re.compile("(?<=[̀-ͯ]) (?=[µμ][^\\s\\[\\]]*\\])")
# A vowel carrying two marks, the dot below and the acute of a stressed retracted schwa, stands
# wider than its box, and the text layer sets a space after it inside the word: petə̣́ leʔ, stə̣́ nwn
# in Hall et al.
SPACE_AFTER_STACK = re.compile("(?<=[̀-ͯ]{2}) (?=[^\\W\\d_])")


SUB_LETTER_ON_MARK = re.compile(r"^(\s*[a-j]\.)(?=[*#?]\S)")


def lettered(line):
    """The space after a sub-example's letter put back where the page sets the judgement mark
    against it, b.*xʷúy̓ for b. *xʷúy̓. The letter is no part of the form."""
    return SUB_LETTER_ON_MARK.sub(r"\1 ", line)


def paper_repair(stem):
    """The repair every tool here applies to a paper's text before it reads a word of it.

    A page read from glyph positions (page_text.py leaves a .rows file beside it) has no inserted
    spaces, and closing the space after a stacked mark there joins two words, xin̓ te. Nor has a
    page text a person transcribed from a scan.
    """
    if os.path.isfile(os.path.join(PAGE_TEXT, stem + ".rows")) or \
            getattr(tables.of(stem), "TRANSCRIBED_FROM_SCAN", False):
        return sequence(reopened, placed, lettered, composed(),
                        corrected(CORRECTIONS.get(stem, ())))
    return sequence(inserted_space(stem), reopened, attached, lettered, composed(),
                    corrected(CORRECTIONS.get(stem, ())))


def inserted_space(stem):
    """The inserted-space repair for a paper. A page text closed up from its glyph rows (page_text.py
    leaves a .layer file of the lines it kept as the layer has them) takes it on those lines alone:
    the spaces of every other line are the glyph row's, =ox̱ =da in Sardinha's (6)."""
    layer_file = os.path.join(PAGE_TEXT, stem + ".layer")
    if not os.path.isfile(layer_file):
        return INSERTED_SPACE
    with open(layer_file, encoding="utf-8") as handle:
        kept = set(handle.read().split("\n")) - {""}

    def repair(text):
        return INSERTED_SPACE(text) if text.strip() in kept else text
    return repair


PAGE_TEXT = os.path.join(PRIVATE, "pagetext")


def source_dir(stem):
    """Where a paper's text is read from: the respaced page text where page_text.py wrote one."""
    if os.path.isfile(os.path.join(PAGE_TEXT, stem + ".txt")):
        return PAGE_TEXT
    return os.path.join(CORPUS, "papers")


def source_path(stem):
    return os.path.join(source_dir(stem), stem + ".txt")


LANGUAGE_KINDS = ("transcription", "segmentation", "phonemic", "cited form", "cited affix", "root",
                  "running speech")


def marks_of(table):
    held = set(SHARED)
    # 7 is the glottal stop only in the van Eijk orthography. A paper whose language rows never
    # write it holds 7 in page numbers, years and URLs, and counting it there asks for rows nobody
    # should write.
    held.discard("7")
    for where, who, kind, form, gloss in oracle_check.oracle_rows(table):
        if kind not in LANGUAGE_KINDS:
            continue
        for symbol in unicodedata.normalize("NFC", form):
            if symbol == "7":
                held.add(symbol)
            if symbol.isascii():
                continue
            if symbol.isalpha() or unicodedata.combining(symbol) or unicodedata.category(symbol) == "Lm":
                held.add(symbol)
    return "".join(sorted(held))


# Marks the text layer flattened, put back from the page with repairs.corrected(). Each pair was read
# off a render of the page named beside it, the provenance corrected() asks for.
CORRECTIONS = tables.gather("CORRECTIONS")

# Rows that are notes about the paper and hold a label, not a string the paper prints. reader_check
# does not ask a reader for them either (NOT_ASKED holds notation).
NOT_PRINTED = ("notation", "symbol note")

read_rows = oracle_check.oracle_rows


def printed_rows(path):
    return [one for one in read_rows(path) if one[2] not in NOT_PRINTED]


read_sources = oracle_check.source_forms


def quoted_sources(path, repair=None, pieces=2, line_joins=False):
    """source_forms, plus each token that ends a multi-word gloss offered without its closing quote.

    bare() takes ’ off a token only where the opening ‘ is on the same token, which is right for a
    one-word gloss and leaves top’ whole at the end of ‘water standing up on top’. The row holds top.
    """
    held, printed, welds = read_sources(path, repair, pieces, line_joins)
    # A footnote mark after the closing quote, ‘I want’16: the last word of the gloss is the token.
    # Where the ’ is a letter, the glottal stop of gwalg̱a’30 in Matthewson's (119a), a row holds the
    # whole token, and the word without it is offered as a weld of that token.
    for token, number in list(held.items()):
        marked = re.match(r"^(.+)'\d{1,2}$", token)
        if marked:
            held.setdefault(marked.group(1), number)
            welds.setdefault(marked.group(1), set()).add(token)
    for token in list(printed):
        marked = re.match(r"^(.+)'\d{1,2}$", token)
        if marked:
            printed.discard(token)
            printed.add(marked.group(1))
    # A bracketed tag set against the closing bracket of a form, <tsik-hi>[etc.]: the form is a
    # token of its own.
    # A footnote mark after the infix label that closes a gloss, LCRF<IPFV>2.
    for token, number in list(held.items()):
        marked = re.match(r"^(.+[A-Z]>)\d{1,2}$", token)
        if marked:
            held.setdefault(marked.group(1), number)
    # A footnote mark after the hyphen that closes a stem, wəlí-7 in Robertson's puns.
    for token, number in list(held.items()):
        marked = re.match(r"^(.+[^\W\d_]-)\d{1,2}$", token)
        if marked:
            held.setdefault(marked.group(1), number)
    # The stem is offered as a weld, and the token stays printed: a row may keep the mark, as hiɬ-10
    # does in Louie's transcription.
    for token in list(printed):
        marked = re.match(r"^(.+[^\W\d_]-)\d{1,2}$", token)
        if marked:
            welds.setdefault(token, set()).add(marked.group(1))
    # A form quoted inside an ellipsis, “...p̓áƛ̓aƛ̓...which, and a form after the sign of its
    # polarity, set in parentheses or not, (?)-čát-t̓iqi-m̓ɬ, (?)+kʷáɬ and +qʷáɬ[=]iš- in Robertson's
    # puns; a + is no letter of any orthography here.
    for token, number in list(held.items()):
        for piece in re.split(r"\.\.\.|…", token):
            if piece and piece != token:
                held.setdefault(piece.strip("“”\"'"), number)
        signed = re.search(r"\([?+\-]\)[+\-]?(.+)$|^\+(.+)$", token)
        if signed:
            held.setdefault(signed.group(1) or signed.group(2), number)
    # A form the page sets in angle brackets against the bracket, <án7ma> in Galloway's Nooksack, or
    # open at a line's end, <tskwám tále>, or closed up on the IPA before it,
    # p'əḵʷp'ɛ́·ɛḵʷ<pekw'pá7akw'>: each form inside is a token of its own, offered as a weld.
    for token, number in list(held.items()):
        if token.startswith("<") or token.endswith(">") or re.search(r"[^\W\d_]<", token):
            for piece in re.split(r"[<>]", token):
                if piece and piece != token:
                    held.setdefault(piece, number)
                    if token in printed:
                        welds.setdefault(token, set()).add(piece)
    for token, number in list(held.items()):
        tagged = re.match(r"^(.+[>⟩/])\[[^\]]*\]?$", token)
        if tagged:
            held.setdefault(tagged.group(1), number)
    # A page can set a word against the next after a comma, prefix es-,the, and a translation
    # against the source after it, pow-wow.’(Camp 2007:43): each half is a token of its own. A
    # name printed in the possessive, Margaret Sherwood’s, holds the name, and so does one the page
    # sets with a prime for its apostrophe, Victoria Howardʹs in Robertson's CJ loans.
    for token, number in list(held.items()):
        for piece in re.split(r"(?<=[^\s,]),(?=[^\s\d,])|(?<=[.!?]')(?=\()", token):
            if piece and piece != token:
                held.setdefault(piece, number)
                held.setdefault(piece.rstrip(".,;:!?'"), number)
        possessive = re.match(r"^([A-Z]\w+?)['ʹ]s$", token)
        if possessive:
            held.setdefault(possessive.group(1), number)
    # A form set against an English word by an en dash, qʷ–series in Pincott: the form is a token
    # of its own.
    for token, number in list(held.items()):
        compound = re.match(r"^(.+?)–[a-z]{4,}$", token)
        if compound:
            held.setdefault(compound.group(1), number)
    for token, number in list(held.items()):
        if token.endswith("'") and len(token) > 1:
            held.setdefault(token[:-1], number)
        # The other way round: ‘Prettys’ Bay opens a gloss with a possessive, and the paired strip
        # takes the apostrophe off as the gloss's closing quote. The row holds Prettys'.
        else:
            held.setdefault(token + "'", number)
    return held, printed, welds


read_language = oracle_check.is_language_token
EXACT = set()


def not_already_written(token, marks):
    """is_language_token, false for a token some row writes exactly as printed.

    Direction two asks whether the token lowercased, or each half of it split at a slash, is
    written. It never asks for the token itself, and əc-Ipfv/Stat, written in a row exactly as
    printed, has capitals and a slash both.
    """
    if token in EXACT:
        return False
    return read_language(token, marks)


def log_repairs(stem):
    """Write to defects.tsv each page line this repair changes past the library's own steps: a mark
    or a mora set apart from its letter (attached) and a pair of CORRECTIONS read off the page.

    The library's inserted-space repair logs nothing here; its work is the text layer's spaces in
    general, not a defect of this paper."""
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "defects", os.path.join(os.path.dirname(os.path.abspath(__file__)), "defects.py"))
    defects = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(defects)
    glyph_read = os.path.isfile(os.path.join(PAGE_TEXT, stem + ".rows"))
    head = () if glyph_read else (inserted_space(stem),)
    plain = sequence(*(head + (reopened, lettered, composed())))
    joined = sequence(*(head + (reopened, placed if glyph_read else attached, lettered, composed())))
    full = paper_repair(stem)
    entries = []
    page = 1
    with open(source_path(stem), encoding="utf-8") as handle:
        for text in handle.read().split("\n"):
            found = re.match(r"^===== page (\d+) =====", text)
            if found:
                page = int(found.group(1))
                continue
            before, middle, after = plain(text), joined(text), full(text)
            if middle != before:
                entries.append((page, "mark or mora set apart from its letter", before, middle))
            if after != middle:
                entries.append((page, "page-read correction", middle, after))
    defects.write(stem, "residue.py", entries)


def main():
    for stem in sys.argv[1:]:
        log_repairs(stem)
    oracle_check.source_forms = quoted_sources
    oracle_check.is_language_token = not_already_written
    for stem in sys.argv[1:]:
        table = os.path.join(ORACLES, "%s.oracle.tsv" % stem)
        if os.path.isfile(table):
            for row in printed_rows(table):
                EXACT.update(oracle_check.pieces(row[3]))
    oracle_check.oracle_rows = printed_rows
    oracle_check.ORACLES = ORACLES
    # One paper at a time, because a paper with a respaced page text is read from another
    # directory and oracle_check takes one directory for every paper it is given.
    failed = 0
    for stem in sys.argv[1:]:
        name = "%s.oracle.tsv" % stem
        table = os.path.join(ORACLES, name)
        marks = marks_of(table) if os.path.isfile(table) else SHARED
        repair = paper_repair(stem)
        oracle_check.EVERY = ((name, stem, "", repair, marks, False),)
        oracle_check.PAPERS = source_dir(stem)
        # oracle_check wraps stdout's buffer and closes it when the wrapper goes. Each call gets
        # its own duplicate of the handle to close.
        sys.stdout.flush()
        kept = sys.stdout
        sys.stdout = io.TextIOWrapper(open(os.dup(kept.fileno()), "wb"), encoding="utf-8")
        try:
            failed |= oracle_check.main()
        finally:
            sys.stdout = kept
    return failed


if __name__ == "__main__":
    raise SystemExit(main())
