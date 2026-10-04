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
    return SPACE_AFTER_STACK.sub("", SPACE_MORA.sub("", SPACE_BEFORE_MARK.sub(r"\1", line)))


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
    spaces, and closing the space after a stacked mark there joins two words, xin̓ te.
    """
    if os.path.isfile(os.path.join(PAGE_TEXT, stem + ".rows")):
        return sequence(reopened, attached, lettered, composed(),
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
CORRECTIONS = {
    # Martin's ⟨e⟩ on page 8 is set in CMSY10, whose angle brackets the layer reads as its letters h
    # and i. Read off the glyph fonts. The bold ? on page 3 stands 3.5 points right of and, a word
    # space the layer drops.
    "Martin_2019_ICSNL": (("type hei.", "type ⟨e⟩."), ("and? indicates", "and ? indicates")),
    # Blamire's Times New Roman maps the quotes ‘ ’ to „ ‟, and the layer sets a space before each ‟
    # and after it where a letter follows, ts ‟ -iɬ for ts’-iɬ and Cinque ‟ s for Cinque’s. Read off
    # 160 dpi crops of pages 2 and 3.
    "2011_Blamire": ((" ‟ ", "’"), (" ‟", "’"), ("„ ", "‘"), ("„", "‘"), ("‟", "’")),
    # Stewart's closeup reading keeps a space inside dɑ in (18a) and (48), inside gɑʔgəʔo in (49),
    # and before the last hyphen of Table 1's stem form nu-…-, where the glyph rows set the dot
    # below of x̣ out of order and no row matches the line. Read off 400 dpi line crops of pages 7,
    # 10, 15 and 16.
    "2011_Stewart": (("d ɑ", "dɑ"), ("g ɑʔgəʔo", "gɑʔgəʔo"), ("nu-… -", "nu-…-")),
    # Mayer's references set Springer- / Verlag with the V kerned under the e, and the layer puts a
    # space between them. Read off a 400 dpi line crop of page 11.
    "2010_Mayer": (("V erlag", "Verlag"),),
    # Turner's page text is closed up from its glyph rows, and the inserted-space repair then closes
    # the printed space after an underlined W̱ as well: HÍ SW̱ KE SIÁM on the title's note, TEȻNOW̱
    # təkʷnaxʷ on page 9 and NEḰNOW̱ SEN in (2). The layer's Rules .6 on page 4 is printed Rules.6.
    # Read off line crops of pages 1, 4, 9 and 11.
    "2011_Turner": (("SW̱KE", "SW̱ KE"), ("TEȻNOW̱təkʷnaxʷ", "TEȻNOW̱ təkʷnaxʷ"), ("NEḰNOW̱SEN", "NEḰNOW̱ SEN"),
                    ("Rules .6", "Rules.6")),
    # Miyashita and Many Bears's Figure 4 sets its labels over one another: the label Collaboration
    # Skill Share turned on its side, over Language Documentation and Linguistic Analysis. The text
    # layer interleaves their letters on one line. Read off a 300 dpi crop turned upright.
    "2009_Miyashita_ManyBears": (("Saehr Skill aotin oabCll   LDaoncguumaegentation   ALinnagluyissitsic",
                                  "Collaboration Skill Share   Language Documentation   Linguistic Analysis"),),
    # Herrling's Summertime of 2010 draws the underline of x̱ as a rule the text layer does not
    # carry, found by the paths under each letter: the byline's Th'athelex̱wot, qex̱-s in (1), qex̱ in
    # (2), x̱et'estexw-es in (3), qe::x̱ in (22) and x̱ete in (30). The layer gives (2)'s Ō as a
    # grave accent, and sets the underscore of (12)'s qw'o_l on a line of its own under the word.
    # Read off 110 dpi pages and 600 dpi line crops.
    "2010_Herrling": (("Th'athelexwot", "Th'athelex̱wot"), ("qex-s", "qex̱-s"),
                      ("`   ts'ats'el   ew   qex   te", "Ō   ts'ats'el   ew   qex̱   te"),
                      ("xet'estexw-es", "x̱et'estexw-es"), ("qe::x", "qe::x̱"),
                      ("ey   xete", "ey   x̱ete"), ("_", ""), ("qw'o l teli", "qw'o_l teli")),
    # Van Eijk's Súnułqaz’ paper of 2007 is a scan, its text layer an OCR, and each pair sets a
    # misread word as printed, read off 130 dpi pages and 600 dpi line crops: the title and the
    # references' Súnułqaz’, (5) súnułqaz’ in Lillooet, Secwépemc twice, the form rn misread as nn,
    # Klamath biblant n’os gitk and la·ba n’os gitk with note 1's mark, Quileute t’abale, kind of,
    # the three web addresses and Bamum, of in two titles, /n/ and the mark 1 of note 1, and the
    # curly quotes and apostrophes of the serif text.
    "Eijk_2007": (("Sunulqaz': The", "Súnułqaz’: The"), ("fonn", "form"),
                  ("(simulqaz' in", "(súnułqaz’ in"), ("Secwepemc", "Secwépemc"),
                  ("kind-of pastiche", "kind of pastiche"),
                  ("bib/ant n 'os gitk \"having-", "biblant n’os gitk “having-"),
                  ("ends\" and la'ba n 'os gitk \"having-two-heads. \" · The",
                   "ends” and la·ba n’os gitk “having-two-heads.”1 The"),
                  ("t 'abale", "t’abale"), ("calciviVmmp 15engl", "ca/civil/mmp15engl"), ("Barnum", "Bamum"),
                  ("nbm.org", "nhm.org"), ("cameroonl014", "cameroon/014"),
                  ("saveonpoolsupplies.com. although", "saveonpoolsupplies.com, although"),
                  ("• In Barker's", "1 In Barker’s"), ("above the In!,", "above the /n/,"),
                  ("Legends o/Vancouver", "Legends of Vancouver"), ("0/La Push", "of La Push"),
                  ("'Who is Sunulqaz'?: A Salish Quest.'", "‘Who is Súnułqaz’?: A Salish Quest.’"),
                  ("43: 177", "43:177"),
                  ("authors'", "authors’"), ("man's", "man’s"), ("Barker's", "Barker’s"),
                  ("\"The Sea-Serpent,\"", "“The Sea-Serpent,”"), ("'two-headed snake'", "‘two-headed snake’"),
                  ("comments: \"This", "comments: “This"), ("'tule'", "‘tule’"), ("\"twist\"", "“twist”"),
                  ("\"vicious guardian spirit.\"", "“vicious guardian spirit.”"),
                  ("'two-headed,' 'double-", "‘two-headed,’ ‘double-"),
                  ("headed,' 'snake' and 'serpent'", "headed,’ ‘snake’ and ‘serpent’"),
                  ("1993. 'Black", "1993. ‘Black"), ("Ethnoherpetology.' MS.", "Ethnoherpetology.’ MS.")),
    # Kinley's Northern Straits paper is a scan, its text layer an OCR. Figure 1 on page 1 is a
    # family tree whose rules the OCR reads as letters and punctuation before each name; the pairs
    # take the rules off and set each name as printed, read off a 130 dpi render. Page 1 prints Al
    # Charles; page 2 the mark 1 after field notes, Samish, Tommy Bob, of ICSNL 36, lh and In in
    # note 1, and the language name Ləkʷəŋínəŋ three times, its í a tall dotless i with the acute,
    # read off 500 and 600 dpi renders. Nothern in the abstract and in Suttles (2001) is printed.
    "2001_Kinley": (("So-cbew-sum", "So-chew-sum"), ("I!!:::::I!:;::: ==xx", "xx"),
                    ("~-Scbe-Iayok Washington", "Sche-layok Washington"), ("I---Se-o-won", "Se-o-won"),
                    ("fL---So-sow-do-maut", "So-sow-do-maut"), ("n   Jack ChiElhEq", "Jack ChiElhEq"),
                    ("L....---Boston Tom", "Boston Tom"), ("l!::':;:==Whee-wel-so", "Whee-wel-so"),
                    ("I----David Torn", "David Tom"), ("Ceci Iia Sam Torn", "Cecilia Sam Tom"),
                    ("'----Elizabeth Tom", "Elizabeth Tom"), ("--Victor Underwood", "Victor Underwood"),
                    ("L - - - -t ecilia Tom", "Cecilia Tom"), ("n   Harry Steele", "Harry Steele"),
                    ("L....---Lena Harry", "Lena Harry"), ("L....---Deeamia", "Deeamia"),
                    ("11. . ---Jack Edwards", "Jack Edwards"), ("n   Tskwsa'ba:lh", "Tskwsa'ba:lh"),
                    (".1.... --cCharley Edwards", "Charley Edwards"), ("II   Xdi'chElwEt", "Xdi'chElwEt"),
                    ("..I... --Bob", "Bob"), (".1.... --Tommy Bob", "Tommy Bob"),
                    ("AI Charles", "Al Charles"), ("field notes}", "field notes1"),
                    ("the Sarnish", "the Samish"), ("~kw~1Jfn~1J. 'Tommy", "Ləkʷəŋínəŋ. Tommy"),
                    ("language ~kw~1Jfn~1J.", "language Ləkʷəŋínəŋ."), ("of I..dkwa1Jfn~1J ..", "of Ləkʷəŋínəŋ."),
                    ("oflCSNL", "of ICSNL"), ("fricative. I n", "fricative. In"),
                    # The serif text prints curly quotes and apostrophes where the OCR has straight
                    # ones, and the dash of La Qualan(U+2014)a; the tree's sans names keep their straight
                    # apostrophes. Read at 300 and 400 dpi.
                    ("Suttles'", "Suttles’"), ("'Some", "‘Some"), ("Straits'", "Straits’"),
                    ("mother's", "mother’s"), ("\"was a La Qualan-a small band near Lummi.\"",
                                               "“was a La Qualan—a small band near Lummi.”"),
                    ("\"Lummi\".", "“Lummi”."), ("\"E\"", "“E”"), ("\"lb\"", "“lh”"), ("\"ch\"", "“ch”")),
    # Page 1 sets note 2's mark on the comma after kinship terms, and both page_text modes set it a
    # space off; page 19 prints prepositions whole, the justified line's gap falling inside it.
    "14-Davis_Forbes_ICSNL50_final-32": (("kinship terms, 2", "kinship terms,2"),
                                         ("prepos itions", "prepositions")),
    # Page 1, the note on the title, prints SSHRC whole twice; the glyph rows set a space after its
    # first S. Read at 300 dpi.
    "5_Lyon-Davis_2018": (("a S SHRC", "a SSHRC"),),
    # Page 12, (36), prints Ga-qˀaw-tk whole, the raised ˀ close on its q; the closeup read sets a
    # space before the ˀ. Read at 400 dpi.
    "03-Dunn-Tsimshian-14": (("Ga-q ˀaw-tk", "Ga-qˀaw-tk"),),
    # Page 2 (b) prints appearance-…like closed up and page 16 Lillooet -Vn/-Vn’; (4) and (5) on page 5
    # set the star against the quote, (NOT *‘, twice; the two cells of Figure 11 on page 13 come off
    # the text layer interleaved. Figure 1 on page 2 sets its slot labels aspect, RDR and voice turned
    # a quarter, and the glyph rows take their letters one by one onto the rows beside them; the eight
    # pairs after Lillooet set each label back on its slot's first row. Each read off a render by the
    # Sardinha2 helper.
    "11-Nater-Complex-predicate-18": (
        ("appearance- …like", "appearance-…like"),
        ("(NOT * ‘", "(NOT *‘"),
        ("-(b(se)ntue-f,ac-(tsiv)tex)ʷ’ ‘control CAUS", "-(s)tu-, -(s)txʷ ‘control CAUS (benefactive)’"),
        ("--m(a)i(mn)in‘r‘eOlBatLioOnBaJl, amppealincsa’tive’", "-mi(n) ‘relational applicative’ -(a)min ‘OBL OBJ, means’"),
        ("Lillooet - Vn/- Vn’", "Lillooet -Vn/-Vn’"),
        ("tc   transition – development", "2   aspect   transition – development"),
        ("2   p", ""),
        ("a   stative – completive", "stative – completive"),
        ("R   TR -m, -amk applicative", "3   RDR   TR -m, -amk applicative"),
        ("R   -nix NC causative", "-nix NC causative"),
        ("ec   transitive, medium", "4   voice   transitive, medium"),
        ("4   io", ""),
        ("v   reflexive, reciprocal", "reflexive, reciprocal")),
    # Note 5 on page 4 prints ʔac-xʷə́n̓=yəlps whole, and note 9 on page 5 prints HYPOCORISTIC whole in
    # small capitals; the rows read sets a space in each. Read at 400 dpi.
    "12_The-flip-side-of-lexical-tabooing": (("xʷə́n̓ =yəlps", "xʷə́n̓=yəlps"),
                                             ("HYPOCORIS TIC", "HYPOCORISTIC")),
    # Page 1 footnote and page 9 prose and reference: the page prints the raised w, the text layer w.
    "Gilmour_ICSNL61": (("kwaɬtèzetkw", "kʷaɬtèzetkʷ"),),
    # Page 3, (2a): the page sets =elik apart from its gloss, the text layer as = elik‘creative.
    "Ikonnikova_ICSNL61": (("= elik‘creative", "=elik ‘creative"),),
    # Table 1 and (7) print a real space after t̓ that the inserted-space repair closes, and §4 prints
    # schwa-glottalized where the text layer sets schwa - at the line end.
    "Janzen_ICSNL61-1": (
        ("p̓  t̓c̓  ƛ̓", "p̓  t̓  c̓  ƛ̓"),
        ("c̓k̓vxt̓c̓k̓ʷxt̓short", "c̓k̓vxt̓ c̓k̓ʷxt̓ short"),
        ("and schwa -", "and schwa-"),
    ),
    # Page 6 prints (e.g., məq̓ ‘full’) and page 17 In press. χʷaχʷaǰɩm with their spaces; both the
    # text layer and the glyph positions run them together. Read at 400 dpi.
    "Mellesmoen_Trotter_ICSNL61": (
        ("press.χʷaχʷaǰɩm", "press. χʷaχʷaǰɩm"),
    ),
    # §1.1, §2, Koch 2011 and Garcia et al. 2024 print the language name whole.
    "Phillips_et_al_ICSNL61-1": (("nɬeʔkepmxc ín", "nɬeʔkepmxcín"), ("n ɬeʔkepmxcín", "nɬeʔkepmxcín"),
                                 ("N ɬeʔkepmxcín", "Nɬeʔkepmxcín")),
    # The text layer sets a space before a letter from the IPA font, Secwepemcts ín, / əy, and
    # inside the closing bracket of ⟨s, j, sh, z ⟩. The page prints each word whole.
    "Pincott_ICSNL61-1": (("Secwepemcts ín", "Secwepemctsín"), ("n ɬeʔkepmxcín", "nɬeʔkepmxcín"),
                          ("St’ át’imcets", "St’át’imcets"), ("nsyilxc ən", "nsyilxcən"),
                          ("Secw épemc", "Secwépemc"), ("Mose s-", "Moses-"), ("C₁ əC₂", "C₁əC₂"),
                          ("/xt éwméɬxʷ/", "/xtéwméɬxʷ/"), ("/xtewm éɬxʷ/", "/xtewméɬxʷ/"),
                          ("/xt əwméɬxʷ/", "/xtəwméɬxʷ/"), ("/m út-emíʔ/", "/mút-emíʔ/"),
                          ("/ əy", "/əy"), ("[ əy", "[əy"), ("z ⟩", "z⟩"), ("j ⟩", "j⟩"),
                          ("*yukʷa ʔ", "*yukʷaʔ"), ("{sihshinim}sextsíne", "{sihshinim} sextsíne")),
    # Page 36 prints təsqanaməs in the title of Louie et al. 2025, read off the glyph positions, and
    # page 32 prints /x̌ʷit/ whole.
    "Reisinger_ICSNL61-1": (("t əsqanaməs", "təsqanaməs"), ("/ x̌ʷit/", "/x̌ʷit/")),
    # Read at 300 and 400 dpi: page 1 prints Teit's, page 2 St'át'imcets, page 3 petə́le(ʔ), page 4
    # s-/mələ́q=amxʷ, page 5 /tə̣́mɬ with the dot under its stressed schwa and 'LIGATURE-', page 7
    # <taki΄nktcîn>, each whole.
    "Robertson_ICSNL61-1": (("Tei t’s", "Teit’s"), ("St ’át’imcets", "St’át’imcets"),
                            ("petə́ le(ʔ)", "petə́le(ʔ)"),
                            ("s-/mələ́ q=a", "s-/mələ́q=a"),
                            ("/tə̣́́mɬ", "/tə̣́mɬ"),
                            ("LIGA TURE", "LIGATURE"), ("< taki΄nktcîn>", "<taki΄nktcîn>")),
    # Footnote 2 on page 2 prints the name kʷaɬtèzetkʷ whole where the text layer breaks it after kʷ.
    "Steiner_Matthewson_ICSNL61-1": (("kʷ aɬtèzetkʷ", "kʷaɬtèzetkʷ"),),
    # Pages 6 and 7: the page sets the two letters apart, ⟨g̱ x̱⟩, and the inserted-space repair
    # closes the space after the macron below.
    "AlperetalICSNL60": (("⟨g̱x̱⟩", "⟨g̱ x̱⟩"),),
    # Read at 600 dpi: the Huijsmans 2023 entry on page 20 prints ʔayʔajuθəm, whose glottal stop the
    # TIPA font sets on the code of P.
    "BrownICSNL60": (("PayPajuθəm", "ʔayʔajuθəm"),),
    # The Symbol font's codes in the Private Use Area of the text layer: the star of the title and
    # its footnote on page 1 as U+F02A, the lambdas of (116), (118), (119), (121) and (126) as
    # U+F06C, and the iota of the definite description ιz P(z) as U+F069.
    # Read at 600 dpi: the text layer drops the acute of a stressed schwa and leaves a space in its
    # place, sxə́<x>əm̓' of (12), kə́laʔ of (28), stə́x̌ʷ and s-pə́plaʔ of (30), ʕə́lʕəl of (65) and
    # (67), qəɬmə́<m>ən̓ of footnote 21, k̓ʷə́n, lə́ŋ and swə́yqəʔ of (95) to (105), kʷə́nt̕ of (107),
    # t̕ᶿətθə́mq̓ən of (108), ŋə́nəʔ of (113), sə́lč̕ of (114) and ʔə́wə of (104) and (115); the
    # spaced qʷal̕ə <l̕>t of (27) is the same.
    # The text layer sets spaces inside words the page prints whole, by the glyph-gap census and the
    # page: ʔayʔaǰuθəm, ʔaƛ̓əm and qʷaqʷatχʷaθotoɬ of §2.1, Church House, above, English, coo.
    "ICSNL57_Francis_et_al": (("ʔayʔa ǰuθəm", "ʔayʔaǰuθəm"), ("ISO -639-3: c oo", "ISO-639-3: coo"),
                              ("Church Ho use", "Church House"), ("abo ve", "above"), ("En glish", "English"),
                              ("(1956 –2022)", "(1956–2022)"), ("ʔa ƛ̓əm", "ʔaƛ̓əm"),
                              ("qʷaqʷat χʷaθotoɬ", "qʷaqʷatχʷaθotoɬ"), ("garbage .", "garbage."),
                              ("(i.e. , maʔtčɛn)", "(i.e., maʔtčɛn)"), ("109 –110", "109–110"),
                              ("non -", "non-"), ("sound s like", "sounds like"), ("texts .", "texts."),
                              ("Languages 57 .", "Languages 57.")),
    # Table 2 on page 6 sets the moras of its Quantity column as subscripts, Vμ > Vμμ, and the text
    # layer puts them on a line of their own under each row.
    "ICSNL57_Mellesmoen": (("/u/   V > V   u > o", "/u/   Vμ > Vμμ   u > o"),
                           ("/i/   V > V   i > e", "/i/   Vμ > Vμμ   i > e"),
                           ("/a/   V > V   N/A", "/a/   Vμ > Vμμ   N/A"),
                           ("/ə/   V > V   ə > e", "/ə/   V > Vμ   ə > e"), ("μ   μμ", "")),
    # The wordlists raise a footnote number straight after a form's closing dash, čɩs(U+2014)23, and the
    # text layer runs the two together.
    # Read by glyph rows, the gloss tiers keep the stream's space after the period of a subject clitic's
    # person, 1SG. SUB, where the page sets 1SG.SUB whole.
    # The phonetic tier's labialized k̀ʷ and q̀ʷ, glottalized with a grave, take a space after the mark
    # in the text layer that the page does not set; the raised ʷ is page_text.raise_letters's.
    "ICSNL56_DavisJ_1_final": (("k̀ ʷ", "k̀ʷ"), ("q̀ ʷ", "q̀ʷ")),
    # Footnote 1's number, raised after (English)., runs into the sentence after it.
    "ICSNL57_Sullivan": (("(English) .1How", "(English).1 How"),),
    "ICSNL57_Schneider": (("1SG. SUB", "1SG.SUB"), ("2SG. SUB", "2SG.SUB"), ("1PL. SUB", "1PL.SUB"),
                          ("2PL. SUB", "2PL.SUB")),
    "ICSNL57_Reisinger_Griffin": (("čɩs—23", "čɩs— 23"), ("qʷop—24", "qʷop— 24"), ("kʷʊs—133", "kʷʊs— 133"),
                                  ("ʔə—151", "ʔə— 151"), ("ʊt—168", "ʊt— 168")),
    # A word ending in ḥ, its dot below a combining mark, loses the space after it to the closing of
    # spaces after marks: ʔanaḥ given, ʔačknaḥ would.
    "ICSNL56_Inman_correction": (("ʔanaḥgiven", "ʔanaḥ given"), ("ʔačknaḥwould", "ʔačknaḥ would")),
    # Read by glyph rows. The text layer sets a space inside the article =ʔiˑ before its length mark
    # (page 1, §1, and page 4, §3), inside the NOW clitic =!aƛ after its !, as in ʔu-L.waƛ=!aƛ of (27)
    # and (28) on page 9, qii-qḥ=!aƛ=qač̓a of (40) on page 12 and qii-qḥ=!aƛ=s of (43) on page 13,
    # and inside ʔu-!aałuk of (44) on page 13. The null sign of ʔuḥ(=∅) in (14) on page 5 is a 0 and
    # a slash, and page 9 prints *He/she/they found something. with its space. Each read off a
    # 160 to 240 dpi render of its page.
    "3_Inman_2018": (("=ʔi ˑ", "=ʔiˑ"), ("=! aƛ", "=!aƛ"), ("ʔu-! aałuk", "ʔu-!aałuk"),
                     ("ʔuḥ(=0/)", "ʔuḥ(=∅)"), ("theyfound", "they found")),
    # The Symbol font's double arrow of -s ‘his’ ⇒ -ows on page 3, U+F0DE in the text layer. The
    # bracketed forms print raised ʸ, ʷ, ᶿ, ᵋ, ᵒ and ᵃ that the text layer sets on the line, some of
    # them a space apart; each is read off a 300 dpi render of its page.
    "ICSNL57_JDavis": (("", "⇒"), ("moçw iyεlᴧs", "moçʷiyεlᴧs"), ("čyεεl", "čʸεᵋl"),
                       ("čyεεn", "čʸεᵋn"), ("čyε", "čʸε"), ("č̀yε", "č̀ʸε"), ("çw", "çʷ"),
                       ("t̀θ", "t̀ᶿ"), ("t̀ᶿoočɪs", "t̀ᶿoᵒčɪs"), ("p’aalᴧ", "p’aᵃlᴧ"),
                       ("θaam", "θaᵃm"), ("maatᴧs", "maᵃtᴧs"),
                       ("ʔaa ǰ y εǰ y εts kwʊk w təm", "ʔaᵃǰʸεǰʸεts kʷʊkʷtəm"), ("kwɪs", "kʷɪs"),
                       (" kw ʔaɪhos]", " kʷ ʔaɪhos]"), ("[ho ga kw paʔa", "[ho ga kʷ paʔa"),
                       ("χ w awɪq wojy ε]", "χʷawɪqʷojʸε]"), ("t̀ɔq̀ wtəm", "t̀ɔq̀ʷtəm"),
                       ("sǰyεsoɬ", "sǰʸεsoɬ"), ("t̀ᶿok̀ w]", "t̀ᶿok̀ʷ]"),
                       ("t̀ᶿot̀ᶿok̀ wok̀ w]", "t̀ᶿot̀ᶿok̀ʷok̀ʷ]"),
                       ("C 1V1-", "C₁V₁-"), ("-V1C 2", "-V₁C₂"), ("s.yél. (ʔ)áw", "s.yél.(ʔ)áw"),
                       ("he use d", "he used"), ("Tommy P aul", "Tommy Paul"), ("Sl iammon", "Sliammon"),
                       ("Isla nd", "Island"), ("the word s for", "the words for"),
                       ("Noel Harry ;", "Noel Harry;"), ("rights ,", "rights,"), ("River” .", "River”."),
                       ("[ Pentlatch Vocabulary ]", "[Pentlatch Vocabulary]"), ("Ms #711 -a", "Ms #711-a"),
                       ("Language .", "Language."), ("San Diego ,", "San Diego,")),
    "ICSNL57_HDavis": (("", "*"), ("", "λ"), ("", "ι"),
                       ("sxə <x>", "sxə́<x>"), ("qʷal̕ə <l̕>t", "qʷal̕ə́<l̕>t"), ("kə laʔ", "kə́laʔ"),
                       ("stə x̌ʷ", "stə́x̌ʷ"), ("s-pə plaʔ", "s-pə́plaʔ"), ("ʕə lʕəl", "ʕə́lʕəl"),
                       ("qəɬmə <m>", "qəɬmə́<m>"), ("k̓ʷə n-", "k̓ʷə́n-"), ("lə ŋ-", "lə́ŋ-"),
                       ("swə y", "swə́y"), ("kʷə nt̕", "kʷə́nt̕"), ("t̕ᶿətθə mq̓ən", "t̕ᶿətθə́mq̓ən"),
                       ("ŋə nəʔ", "ŋə́nəʔ"), ("ʔə wə", "ʔə́wə"), ("sə lč̕", "sə́lč̕"),
                       # Page 7 sets St’át’imcets apart after its first apostrophe.
                       ("St’ át’imcets", "St’át’imcets")),
    # Read at 600 dpi: the Symbol font's codes sit in the Private Use Area of the text layer, the
    # star of the title and its footnote on page 1 as U+F02A, and the lambda of λx[x claimed ...]
    # in (49) and of the λ-operator in footnote 14 as U+F06C.
    "DavisICSNL60": (("", "*"), ("", "λ")),
    # Read at 600 dpi: the null third person subject of sul-t=∅ on page 14 and every other is the
    # Symbol font's empty set, U+F0C6 in the text layer.
    # The existential of the formulas (24) to (27) on pages 18 and 20 is ∃, set as the katakana ﾖ,
    # U+FF96, and in (31) as the Latin Ǝ, U+018E.
    # Read at 600 dpi: the dot below is dropped from the text layer, most often as a space, in
    # sə̣́n<sə̣n>-t and √sə̣n of (1a) on page 5, xʷʔạ́z of (3a) on page 6, x̣̌əs-t of (4a) on page 7,
    # ʔạ́y of (9) on page 13, xʷạ́z of (28b) on page 19, sx̣án̓i of (33a) and kạ́h of (34a) on page 21,
    # and pə̣tạ́k of (38) on page 23.
    "Davis-NederveenICSNL60": (("sə  ́n<sə n>-t", "sə̣́n<sə̣n>-t"), ("√sə n", "√sə̣n"),
                               ("xʷʔa  ́z=as", "xʷʔạ́z=as"), ("x̌  əs-t", "x̣̌əs-t"),
                               ("ʔá  y=Ø", "ʔạ́y=Ø"), ("xʷa  ́z=a=Ø", "xʷạ́z=a=Ø"),
                               ("ʔə=sx án̓i", "ʔə=sx̣án̓i"), ("n-ka  ́h=a", "n-kạ́h=a"),
                               ("pə ta  ́k=a", "pə̣tạ́k=a"),
                               ("ﾖ", "∃"), ("Ǝ", "∃"), ("", "∅"),),
    # Read at 200 and 300 dpi: footnote 1 on page 2 prints from Thompson and Thompson (1996:45) and
    # defines it scew̓exmxcín with their spaces, and page 4 the greeting hén̕ ɬeʔ kʷ as three words
    # and like ɬɛn̓ in ʔayʔaǰuθəm with its space after the glottalized n.
    "Steiner_ICSNL61-1": (("fromThompson and Thompson(1996:45)", "from Thompson and Thompson (1996:45)"),
                          ("itscew̕exmxcín", "it scew̕exmxcín"), ("hén̕ɬeʔ", "hén̕ ɬeʔ"),
                          ("ɬɛn̓in", "ɬɛn̓ in")),
    # Read at 200 dpi: the violation marks of tableaux (69), (72) and (73) on pages 23 and 24 set
    # some stars and exclamation marks in the Symbol font, U+F02A and U+F021 in the text layer. Read
    # at 600 dpi: page 4 and every page after print tí͜y with one acute where the layer doubles it.
    # Read at 800 dpi: footnote 25 on page 34 prints sx̣ə́k̓iʔt with the dot under its x, where the
    # layer leaves a space. Read at 300 dpi: page 8 prints *[-tn], *[máʕ.xetn], *[-nm] and *[kénm]
    # with no space inside the brackets, page 6 *[ʔes.kɬxə́n] and page 7 *[mƛ̓-] and *[mƛ̓ə́q̓ʷ],
    # and the glyph positions of Table 1 on page 2 set ə̣ and o in two cells 103 points apart,
    # and every soft hyphen of the text layer, U+00AD, is a
    # hyphen on the page: [kɬx-], [-OR], DEP-IO.
    "Hall-Luntzlara-Mellesmoen-Reid-ICSNL60": (("", "*"), ("", "!"),
                                               ("í́", "í"), ("í́", "í"),
                                               ("sx ə̣́k̓iʔt", "sx̣ə̣́k̓iʔt"),
                                               ("*[m áʕ.xetn]", "*[máʕ.xetn]"), ("*[k énm]", "*[kénm]"),
                                               ("*[ ʔes.kɬxə́n]", "*[ʔes.kɬxə́n]"), ("*[m ƛ̓", "*[mƛ̓"),
                                               ("ə̣o", "ə̣ o"),
                                               ("­-", "-"), ("­", "-"), ("[ -", "[-"),
                                               (" -]", "-]")),
    # Read at 600 dpi: the null morpheme of (1a) on page 3 and every other is ∅, set as a 0 with a
    # / drawn over it.
    # The glyph positions at a 0.3 em word gap (table_cells.py) set the deleted segments of a
    # segmentation against their hyphens, nés-[n]-[t]-si-n on page 4 and wéw-n-t-s[e]m-[e]s on page
    # 7, and = against the glottalized y̓ before it, xʷúy̓=kʷ; the brackets and the comma above stand
    # wider than their boxes and the rows read a space beside them.
    # Read at 300 dpi: the denotations (22) on page 10 and (49) on page 19 open on the brackets ⟦
    # and ⟧, which the text layer types as J and K.
    "HannonICSNL60": (("0/", "∅"), ("- [", "-["), ("] -", "]-"), ("∅ -", "∅-"), ("∅ =", "∅="),
                      ("y̓ =", "y̓="), ("s [e]m", "s[e]m"), ("[e] s", "[e]s"), ("[en] e", "[en]e"),
                      ("s [i]", "s[i]"), ("Jxʷúy̓K", "⟦xʷúy̓⟧"), ("JMODK", "⟦MOD⟧")),
    # Pages 1, 3, 14 and 16 print ʔayʔaǰuθəm whole where the text layer breaks it after ʔayʔa, and
    # footnote 9 prints Sḵwx̱wú7mesh whole, and page 14 Tla'amin, read at 300 dpi.
    "Kelly_Huijsmans_McCarthy_ICSNL61-1": (
        ("ʔayʔa ǰuθəm", "ʔayʔaǰuθəm"),
        ("S ḵwx̱wú7mesh", "Sḵwx̱wú7mesh"),
        ("Tl a’amin", "Tla’amin"),
    ),
    # The glyph rows against the text layer (layer_diff.py): the opening quote sits after the first
    # letter in the glyph stream, A‘nd for ‘And; the rows read a space inside cedar.shakes and
    # coho.salmon and around <STAT>, and none between NEG and ???, after F: before ..., or in three
    # tightly set lines of Freddie's English, all of which the text layer holds.
    "LouieICSNL60": (
        ("A‘nd", "‘And"), ("f‘at", "‘fat"), ("F:...", "F: ..."), ("NEG???", "NEG ???"),
        ("cedar. shakes", "cedar.shakes"), ("coho. salmon", "coho.salmon"), ("< STAT>", "<STAT>"),
        ("<STAT> -3ERG", "<STAT>-3ERG"), ("<STAT> =3SBJV", "<STAT>=3SBJV"),
        ("andformed", "and formed"), ("ofshiny", "of shiny"), ("leadfor", "lead for"),
        ("ofSliammon", "of Sliammon"),
    ),
    "LyonICSNL60_CS": (
        ("DETrope", "DET rope"), ("<INCH>DETwind", "<INCH> DET wind"),
    ),
    # Read at 300 dpi: the Symbol font's radical and bullet, which the text layer holds in the
    # private use area: the radical marking a root, √smiw on page 3, and the diamond bullets of
    # §4.2 and §5.
    "NaterICSNL60_BellaCoola": (
        ("", "√"), ("", "♦"),
    ),
    # Read at 400 dpi: Table 8's nɬeʔkepmxcín weak grade k̓lə̣́m carries a dot below its schwa, which
    # the text layer drops, and its acute sits apart from the vowel. Table 6's ‘transverse’ row
    # spaces its three cells, *xət̓ √xét̓ √xə́ƛ̓, which closing the space after the comma above runs
    # together. The glyph positions set the forms of pages 5 and 11 between slashes as one word each,
    # /é/, /c-k̓éʔ/, /k̓lám/, /cíʕəns/, /í/, where the text layer spaces them.
    "PincottICSNL60": (
        ("/ é/", "/é/"), ("/c -k̓éʔ/", "/c-k̓éʔ/"), ("/ k̓", "/k̓"), ("/c íʕəns/", "/cíʕəns/"), ("/ í/", "/í/"),
        ("k̓lǝ  ́m", "k̓lǝ̣́m"),
        ("*xət̓√x\xe9t̓√xə́ƛ̓",
         "*xət̓ √x\xe9t̓ √xə́ƛ̓"),
    ),
    # Read at 400 dpi and off the glyph positions: closing the space after a mark below or above
    # runs words together that the page sets apart, SENĆOŦEN SW̱ YÁ¸ of (36) and (38), whose second
    # line segments =sxʷ yéʔ, and ʔayʔaǰuθəm qayx̣ kʷum of (24) and Qayx̣ (Mink) on page 3.
    "SchneiderGriffinICSNL60": (
        ("SW̱YÁ¸", "SW̱ YÁ¸"), ("qayx̣kʷum", "qayx̣ kʷum"), ("Qayx̣(Mink", "Qayx̣ (Mink"),
    ),
    # The glyph positions set sɣə́p of (2), cʕə́p of (5a) and c̓k̓ʷə́m of (46) and (54) as one word
    # each, where the text layer leaves a space after the accented schwa.
    "SteinerICSNL60": (
        ("sɣə́ p", "sɣə́p"), ("cʕə́ p", "cʕə́p"), ("ʷə́ m", "ʷə́m"),
    ),
    # Footnote 1 on page 1 at 400 dpi prints ⱡ = [ɬ] and ȼ = [t͡s]; the text layer codes the belted
    # l as ì and drops the tie bar.
    "ICSNL59_Diep_Xu_Babel_Bochnak_final": (("ⱡ = [ì], ȼ = [ts]", "ⱡ = [ɬ], ȼ = [t͡s]"),),
    # Page 3 sets the name whole, the two glyph runs -0.004 em apart; footnote 7's space is wider.
    "ICSNL59_Hannon_final": (("K̓ʷəɬtə̀ zétkʷu (Bernice", "K̓ʷəɬtə̀zétkʷu (Bernice"),),
    # Page 6 sets 'ux̄ʷ whole, its ʷ -0.029 em from the x̄; the text layer puts a space before it.
    "ICSNL59_Murphey_Jo_final": (("’ux̄ ʷ", "’ux̄ʷ"), ("’ux̄  ʷ", "’ux̄ʷ")),
    # Page 6 sets a space between xelnwélln̓ and its gloss (able to); the repair closes it after the mark.
    "ICSNL59_Oliver_final": (("xelnwélln̓(able", "xelnwélln̓ (able"),),
    # Footnote 4 sets a space between pal̓ and for; the repair closes it after the mark.
    "ICSNL58_Andreatta_Recalma_Urbanczyk_final": (("pal̓for", "pal̓ for"),),
    # Table A1 on pages 18 to 20 prints a stressed schwa, ə́, whose acute is a glyph with no text;
    # the text layer and pdfium set a space where it stands. Each read off a 250 dpi render.
    "ICSNL58_Khalaji_final": (("cə x̣ ‘dripping’", "cə́x̣ ‘dripping’"), ("kə st ‘bad’", "kə́st ‘bad’"),
                              ("ɬmə k ‘hole in socket’", "ɬmə́k ‘hole in socket’"),
                              ("ntəkʷpə n̓i ‘to become deaf’", "ntəkʷpə́n̓i ‘to become deaf’"),
                              ("pxʷə p ‘inflate’", "pxʷə́p ‘inflate’"), ("skə kn̓ ‘companion’", "skə́kn̓ ‘companion’"),
                              ("skə kn̓  ‘companion’", "skə́kn̓ ‘companion’"),
                              ("x̣əcə m ‘bet or gamble’", "x̣əcə́m ‘bet or gamble’"),
                              ("x̣ətqə m ‘making a hole’", "x̣ətqə́m ‘making a hole’"),
                              ("ʔeskəłxə n ‘barefoot’", "ʔeskəłxə́n ‘barefoot’")),
    # Pages 25, 51 and 53 set St'át'imcets whole and category-neutral with its hyphen closed, and
    # footnote 48 on page 32 sets istəmtímaʔ and strong whole, read off 300 dpi renders; the text
    # layer puts a space inside each.
    # Footnote * on page 1 sets Bernice Garcia's introduction closed up, ncitxʷ. and netíyxs, tékm and
    # nɬeʔkepmx and tmixʷs., read off a 400 dpi render; the text layer spaces inside the words.
    "ICSNL58_Matthewson_final": (("ncitxʷ . ƛ̓uʔ", "ncitxʷ. ƛ̓uʔ"), ("net íyxs", "netíyxs"),
                                 ("ƛ̓uʔ t ékm", "ƛ̓uʔ tékm"),
                                 ("ne n ɬeʔkepmx e tmixʷs .", "ne nɬeʔkepmx e tmixʷs.")),
    # The text layer keeps the typesetter's ligatures, ﬁnds and diﬀerence, as single glyphs.
    "ICSNL58_Menon_final": (("ﬃ", "ffi"), ("ﬀ", "ff"), ("ﬁ", "fi"), ("ﬂ", "fl")),
    # Pages 3, 6 and 11 print x̌ and č̓ with a caron, read off 200 to 400 dpi renders; the text layer
    # drops the caron of x̌ and leaves a space, s-x ʷusm and pix -m-wn, and writes č̓ as c and a
    # comma set apart, [č c ̓  š] and [sí.c ̓ e.n̩], which the repairs before these close up to čc̓ and c̓e.
    "ICSNL58_Schillo_final": (("[čc̓  š]", "[č č̓ š]"), ("sí.c̓e", "sí.č̓e"), ("s-x ʷ", "s-x̌ʷ"),
                              ("√x ʷ", "√x̌ʷ"), ("sx ú", "sx̌ú"), ("pix -", "pix̌-"), ("pí.x e", "pí.x̌e"),
                              ("píx .", "píx̌."),
                              # Page 2 sets Secwepemctsín and /-úl̕əxʷ/ closed up, read off a 220 dpi render.
                              ("Secwepemcts ín", "Secwepemctsín"), ("is / -úl̕əxʷ/", "is /-úl̕əxʷ/"),
                              # Pages 3, 6 and 7 set each pair of segments apart, [ʕ̕ ʕ̕ʷ], [m̩ m̩̓], [n̩ n̩̓]
                              # and [w̩ w̩̓]; the repair closing the space after a mark joins them.
                              ("ʕ̕ʕ̕ʷ", "ʕ̕ ʕ̕ʷ"), ("m̩m̩̓", "m̩ m̩̓"), ("n̩n̩̓", "n̩ n̩̓"), ("w̩w̩̓", "w̩ w̩̓")),
    # Page 5 prints the C of a C₁ reduplicated cognate with its 1 as a subscript, which the text
    # layer sets as a plain 1 (150 dpi).
    "2010_Denzer-King": (("C1 reduplicated", "C₁ reduplicated"),),
    # Page 31 prints the indices of (95) as subscripts, [səntumisten]₂ [iʔ tl t₂]₁ and t₁, which the text
    # layer sets as plain digits (200 dpi).
    "2010_Lyon": (("[səntumisten]2", "[səntumisten]₂"), ("t2]1", "t₂]₁"), ("   t1   axàʔ", "   t₁   axàʔ")),
    # Read off 600 dpi renders: pages 1 and 2 print *…an#, page 3 […lɑⁿ], "smosledgensk" and
    # proto-Salish, page 4 Harmon 1820:403 with no space, and page 6 the homophone index of PA
    # *-ɢəŋ'₂ as a subscript, which the text layer sets as a plain 2. Page 2's drag chain and Table 2
    # on page 3 subscript the series indices, /c₁/, /č₁/, /č₂/, /c₂/, /č₃/ and /c₃/ (500 dpi).
    # Footnote 15 on page 6 draws a rule under the c and the s of Lillooet's /c̱ s̱/, which the layer
    # does not carry, written with the macron below as in Herrling's x̱ (300 dpi).
    "Nater_2019_ICSNL": (("*… an#", "*…an#"), ("[…l ɑⁿ]", "[…lɑⁿ]"), ("“ smosledgensk”", "“smosledgensk”"),
                         ("proto -Salish", "proto-Salish"), ("Harmon 1820 :403", "Harmon 1820:403"),
                         ("*-ɢəŋ’2", "*-ɢəŋ’₂"), ("/c1/", "/c₁/"), ("/č1/", "/č₁/"), ("/c2/", "/c₂/"),
                         ("/č2/", "/č₂/"), ("/c3/", "/c₃/"), ("/č3/", "/č₃/"),
                         ("along with /c s/", "along with /c̱ s̱/")),
    # The layer sets a space after a glottal mark and before a raised w that pages 2 to 13 print closed
    # up. (3b)'s k̓̓ʷɫ prints two glottal marks, the second 2.8 points over the first, where (4b)'s
    # k̓ʷɫ prints one (renders at 1200 dpi); the marks drawn above the
    # Mengarini lines of (3b), (4b) and (12) are the ones of the segmentation under them. Pages 5, 9,
    # 11 and 12 print ⟦ʕác-m⟧ and ʕay̓p closed up, page 1 in Montana and page 12 nouns and ɫp̓úlexʷtn.
    "McKay_2019_ICSNL": (("k̓̓ ʷɫ", "k̓̓ʷɫ"), ("k̓ ʷɫ", "k̓ʷɫ"), ("háʕ̓ ʷ", "háʕ̓ʷ"), ("k̓ ʷúl̓ -mín", "k̓ʷúl̓-mín"),
                         ("c̓ óq̓ ʷ", "c̓óq̓ʷ"), ("láq̓ ʷ", "láq̓ʷ"), ("q̓ ʷéyɫ", "q̓ʷéyɫ"), ("c̓ wét", "c̓wét"),
                         ("č̓ éxʷ", "č̓éxʷ"), ("ʕá c-m", "ʕác-m"), ("ʕay̓ p", "ʕay̓p"), ("i n Montana", "in Montana"),
                         ("nou ns", "nouns"), ("ɫp̓ úlexʷtn", "ɫp̓úlexʷtn")),
    # The layer sets a space after a glottal mark that page 11 prints closed up (400 dpi):
    # (6)'s ʔíc̓amin and c̓amqɬ, /č̓/, /c̓/, *k̓, and footnote 10's /t̓ᶿ/. Pages 2, 5 and 10 print
    # informed, suggests and realized as one word each.
    "Mellesmoen_2019_ICSNL": (("ʔíc̓ amin", "ʔíc̓amin"), ("c̓ amqɬ", "c̓amqɬ"), ("/č̓ /", "/č̓/"), ("/c̓ /", "/c̓/"),
                              ("*k̓ ,", "*k̓,"), ("/t̓ ᶿ/", "/t̓ᶿ/"), ("informe d", "informed"),
                              ("sugge sts", "suggests"), ("re alized", "realized")),
    # The title's note mark, Symbol's ∗, stands on St'át'imcets, which the residue would read as one
    # token of the language; set apart, the title row holds the name. Page 1 prints wisdom as one word.
    "HDavis_2019_ICSNL": (("St’át’imcets∗", "St’át’imcets ∗"), ("wi sdom", "wisdom")),
    # Page 18 prints wog̲-an closed up, italic, over the underline the layer spaces after.
    "Forbes_2019_ICSNL": (("wog̲ -an", "wog̲-an"),),
    # The TIPA font's @ is ə and its P ʔ, found by tipa_pairs.py and read off 300 dpi renders of
    # pages 8, 10 and 12, where page 12 prints /ə/ closed up. Page 17 sets the italic k̓ak̓pit with its
    # first mark raised, which the layer spaces off the k. The glyph rows space a long vowel's colon,
    # the italic -Vm and a bracket's subscript stem off what pages 3, 5, 6, 16, 20 and 21 print closed
    # up (300 dpi); (17a) prints ☹ where the layer has a slash.
    "MellesmoenHuijsmans_2019_ICSNL": (("NxaPamxcín", "Nxaʔamxcín"), ("/ @/", "/ə/"), ("/@/", "/ə/"),
                                       ("k ̓ak̓pit", "k̓ak̓pit"), ("ju: θut", "ju:θut"),
                                       ("[ǰa: qʷɛt]", "[ǰa:qʷɛt]"), ("[ǰu: θot]", "[ǰu:θot]"),
                                       ("(- Vm)", "(-Vm)"), ("] stem", "]stem"), ("/ k̓əpit2", "☹ k̓əpit2")),
    # Pages 8 to 10 print Beaumont's Sechelt with an underlined k̲ and x̲, drawn as a rule under the
    # letter the text layer does not carry; underline_probe.py found each rule under a letter, and 150
    # dpi renders of pages 7 to 10 agree. Page 3 (8) prints a lot of forest fires; the layer closes the
    # space after of.
    "ReisingerHuijsmans_2019_ICSNL": (("-ka", "-k̲a"), ("yáka", "yák̲a"), ("Yáka", "Yák̲a"), ("yéká", "yék̲á"),
                                      ("kél-álh", "k̲él-álh"), ("hákw-nu", "hák̲w-nu"), ("kéyi-la", "k̲éyi-la"),
                                      ("xét-át", "x̲ét-át"), ("téʔáxa", "téʔáx̲a"), ("lot offorest", "lot of forest")),
    # Page 15 prints ɫəɫə́l̓ət ‘bailing it out’ closed up; the layer sets a space after the accent.
    "15_ICSNL55_Mellesmoen_Urbanczyk_final": (("ɫəɫə́ l̓ət", "ɫəɫə́l̓ət"),),
    # The layer sets a space inside words that pages 3 and 8 print closed up (400 dpi): distinctive,
    # ich-laut [çʷ], Ko-mookhs, [θaɬoɬtçʷ] and Gibbs's /qayməçʷs/, /qayməçʷ/ and [mʊçʷs].
    "JDavis_2019_ICSNL": (("di stinctive", "distinctive"), ("ich -laut [ç ʷ]", "ich-laut [çʷ]"),
                          ("Ko -mookhs", "Ko-mookhs"), ("[ θaɬoɬtçʷ]", "[θaɬoɬtçʷ]"), ("/qaym əçʷs/", "/qayməçʷs/"),
                          ("[qaym ʊçʷs]", "[qaymʊçʷs]"), ("/qaym əçʷ/", "/qayməçʷ/"), ("[m ʊçʷs]", "[mʊçʷs]")),
    # Pages 1 to 10 print every k, q, χ and x of a labialized pair with the raised w, read off renders
    # of pages 1, 2, 3, 5, 6, 8 and 10; the text layer writes a plain w, set apart where the example
    # is italic. Page 8 (29) closes ’aχʷtχʷ up. Page 17 (14) prints tχw with a plain w, and the last
    # pair puts it back after the general one. Page 10 footnote 9 prints *c’ən ‘tight’ on one line; the
    # layer drops the ə and sets n ‘tight’ on a line of its own, which the first two pairs join back.
    "ICSNL58_Nater_final2": (("proto-Interior Salish *c’", "proto-Interior Salish *c’ən ‘tight’ (Kuipers 2002)."),
                             (" n ‘tight’ (Kuipers 2002).", ""),("’aχ w tχ w ˬ", "’aχʷtχʷˬ"), ("sq w lχw uɬ", "sqʷlχʷuɬ"),
                             ("tux w !", "tuxʷ!"), ("ick w iχ", "ickʷiχ"), ("k w ˬ", "kʷˬ"),
                             ("χ w ˬ", "χʷˬ"), ("kw", "kʷ"), ("qw’", "qʷ’"), ("χw", "χʷ"),
                             ("ˬkʷuˬtχʷ ’ulaˬ", "ˬkʷuˬtχw ’ulaˬ")),
    # The repair closing the space after a mark or a glyph gap also joins real word breaks in the APA
    # lines of (3), (4a), (15), (26) and (38); each gloss line counts one word more. Page 11 carries
    # [[yee]], a note left in the layer, and page 14 sets a stray s before (26)'s translation and tag.
    "ICSNL58_Schneider_final": (("ʔiməšyə=t̓", "ʔiməš yə=t̓"), ("ʔimǝštθǝ", "ʔimǝš tθǝ"),
                                ("kʷən̓-atəl̓DYN=", "kʷən̓-atəl̓ DYN="), ("ʔəl̓ʔəncə", "ʔəl̓ ʔəncə"),
                                ("c̓iməl̓ʔiməštəw̓nił", "c̓iməl̓ ʔiməš təw̓nił"), ("[[yee]]", ""),
                                ("s‘That was when", "‘That was when"), ("s  (WSa 1977: line 426)", "(WSa 1977: line 426)")),
    # The layer sets the language names of three titles apart letter by letter and drops the fi
    # ligature of Infixing and Griffin.
    "ICSNL57_program": (("ʔ ay ʔ a ǰ u θ ə m", "ʔayʔaǰuθəm"), ("nsyilxc ə n", "nsyilxcən"), ("Kwa k̓wala", "Kwak̓wala"),
                        ("as Inxing", "as Infixing"), ("Laura Grin", "Laura Griffin")),
    "ICSNL58_Lyon_final": (("St’ át’imcets", "St’át’imcets"), ("category -neutral", "category-neutral"),
                           ("ist əmtímaʔ ‘My grandmother got", "istəmtímaʔ ‘My grandmother got"),
                           ("are str ong.", "are strong.")),
    # Page 13 sets f. * ǰɛhɛƛ̓ */ǰi<hi>ƛ̓/ with a space before the star of the phonemic form, as d. and e.
    # do; the repair closes it after the comma above.
    "ICSNL58_Huijsmans_final2": (("ǰɛhɛƛ̓*/ǰi<hi>ƛ̓/", "ǰɛhɛƛ̓ */ǰi<hi>ƛ̓/"),),
    # The text layer sets a space after the K̀ of Bernice Garcia's name in footnote *, as in ICSNL59_Hannon_final.
    "ICSNL58_Hannon_Smith_final": (("K̀ wəłtèzetkwu (Bernice", "K̀wəłtèzetkwu (Bernice"),),
    # The text layer drops the caron of x̌ and sets a space where it stood; the glyph gap there is
    # under a twentieth of an em, and the page at 250 dpi prints x̌ in (11), (12), (17), (18), (22),
    # (23), (25b), (26), (30), (31), (33) and (34). (17) runs θat-íl̓ into ta= once the space after
    # the glottal mark is closed; the glyphs set them 34 points apart. Footnote 33 on page 24 prints
    # /k̓/ and /ky/ with no space inside the slashes.
    "ICSNL59_Davis_final": (("x ə́ƛ̓-ən-əm", "x̌ə́ƛ̓-ən-əm"), ("x áyk̓ʷ", "x̌áyk̓ʷ"), ("ʔúx ʷ", "ʔúx̌ʷ"),
                            ("ʔux ʷ", "ʔux̌ʷ"), ("x ɬ-ət-Ø", "x̌ɬ-ət-Ø"), ("ʔác̓x -ən", "ʔác̓x̌-ən"),
                            ("n-x ʷáz", "n-x̌ʷáz"), ("qʷənúx ʷ", "qʷənúx̌ʷ"), ("ʔá<ʔa>x ič", "ʔá<ʔa>x̌ič"),
                            ("θat-íl̓ta=cux ácut", "θat-íl̓ ta=cux̌ácut"), ("/ k̓/ /x/ to / ky/", "/k̓/ /x/ to /ky/")),
    # The glyph positions on page 8 set 'Wùik̓ala. as one word from 278 to 318 points, and the page
    # at 300 dpi prints it whole; the text layer sets a space after Wùi.
    "ICSNL59_Baleno_Janzen_Yoder_final": (("Wùi k̓ala", "Wùik̓ala"),),
    # The glyph positions of the CWDP alphabet on page 34 set x̣ at 119 points and [χ] at 255, in
    # two columns; closing the space after the dot below runs them together. Read at 300 dpi: (1a)
    # and (1b) on page 15 print gatgíyamx̣ and qáx̣ba as two words, the field set 25 points apart;
    # Appendix 5, sets 6 and 14 on pages 42 and 43, print Taï patlach and Taï pous, and Appendix 6
    # on page 44 prints the syllabic n̩ apart from ˈsaika.
    "ZenkICSNL60": (("x̣[χ]", "x̣ [χ]"), ("ɡɑtɡíyɑmx̣qɑ́ˑx̣bɑ", "ɡɑtɡíyɑmx̣ qɑ́ˑx̣bɑ"),
                    ("gatgíyamx̣qáx̣ba", "gatgíyamx̣ qáx̣ba"), ("Taïpatlach", "Taï patlach"),
                    ("Taïpous", "Taï pous"), ("n̩ˈsaika", "n̩ ˈsaika")),
    # Read at 300 dpi: the same radical before seven roots of pages 4 and 5, Carrier √k'ʷaʔ, *√ɣ̌ʷəǯ.
    # The glyph positions space the members of a list of sounds, ʕ̞ ʕ̞ʷ, ḥ ḥʷ, ḥ ḥw and č č', which
    # closing the space after a mark below or above runs together.
    "Nater_ICSNL60_resonants": (
        ("", "√"), ("ʕ̞ʕ̞ʷ", "ʕ̞ ʕ̞ʷ"), ("ḥḥʷ", "ḥ ḥʷ"), ("ḥḥw", "ḥ ḥw"), ("čč’", "č č’"),
    ),
    # Read at 300 dpi: page 12 sets the Squamish cognate of 0233 against the quote opening its gloss,
    # -(a)xʷ'2SG.SUBJ', and the form is parted from the gloss to be a word of its own.
    "Nater-final": (("-(a)xʷ‘2SG.SUBJ’", "-(a)xʷ ‘2SG.SUBJ’"),),
    # Read at 300 dpi: the underlying lines of (15d) on page 14 and (18b) on page 15 print qəx̣ tə=ʔasxʷ
    # and ʔə=qʷəl̓ t̓əq̓-aš-uɬ as two words each, over the glosses lots and come; closing the space after
    # the mark below and the mark above runs them together.
    "ICSNL56_Reisinger_Huijsmans_v1.2-1": (("qəx̣tə=ʔasxʷ", "qəx̣ tə=ʔasxʷ"),
                                           ("ʔə=qʷəl̓t̓əq̓-aš-uɬ", "ʔə=qʷəl̓ t̓əq̓-aš-uɬ")),
    # Read at 200 dpi: the underlying lines of (6c), (7), (8), (9a) and (9c) on pages 4 and 5 print
    # nem̓ ʔiməš, č nemǝstǝxʷ, ʔimǝš tθǝ and ʔimǝš (RP apart, each word over its own gloss (go walk,
    # 2SG go.CS, walk DT); (17) on page 9 sets appear(PFV) and DT under wil̓ and tey̓. Closing the
    # space after the mark runs them together.
    "ICSNL56_Schneider_final": (("nem̓ʔimǝš", "nem̓ ʔimǝš"), ("čnemǝstǝxʷ", "č nemǝstǝxʷ"),
                                ("ʔimǝštθǝ", "ʔimǝš tθǝ"), ("ʔimǝš(RP", "ʔimǝš (RP"), ("wil̓tey̓", "wil̓ tey̓")),
    # The TIPA font's codes, read by tipa_pairs.py from each glyph's font and checked against a
    # 250 dpi render of pages 1, 2, 3, 5, 9 and 12: @ is ə, P ʔ, T θ, X χ, A ɑ, ì ɬ, ň ƛ, ˇȷ ǰ, and a w or
    # h set small after a letter the raised ʷ or ʰ; the text layer's space at a change of font goes
    # where the glyphs touch. The formulas of (59) to (69), read off the render of pages 11 to 14,
    # print ⟦ ⟧ where the layer has J K, and the λ binders the layer drops, with their type subscripts.
    "ICSNL56_Sobolak_final": (
        ('JhaveFULLK = yexees[R(x)(e)&POSS( y)(e)]', '⟦haveFULL⟧ = λyₑλxₑλeₛ[R(x)(e)&POSS(y)(e)]'),
        ('JhaveFULLK = yxe[R(x)(e)&POSS( y)(e)]', '⟦haveFULL⟧ = λyλxλe[R(x)(e)&POSS(y)(e)]'),
        ('JhaveK= yexees[R(x)(e)&POSS( y)(e)]', '⟦have⟧ = λyₑλxₑλeₛ[R(x)(e)&POSS(y)(e)]'),
        ('JhaveLIGHTK = xees[R(x)(e)]', '⟦haveLIGHT⟧ = λxₑλeₛ[R(x)(e)]'),
        ('JhaveK= xees [have(x)(e)]', '⟦have⟧ = λxₑλeₛ [have(x)(e)]'),
        ('JhaveLIGHTK = xe[R(x)(e)]', '⟦haveLIGHT⟧ = λxλe[R(x)(e)]'),
        ('epì-X wP-X wPéy', 'epɬ-χʷʔ-χʷʔéy'),
        ('*ep ì-pus-nt-n', '*epɬ-pus-nt-n'),
        ('c-m @q’méq’-@m', 'c-məq’méq’-əm'),
        ('PayPa  ȷuT@m', 'ʔayʔaǰuθəm'),
        ('q w’umqn-átkw', 'qʷ’umqn-átkʷ'),
        ("kì-sň'aPcin@m", 'kɬ-sƛ’aʔcínəm'),
        ('Paws-píx̌-@m', 'ʔaws-píx̌-əm'),
        ('epì-Pewtús-m', 'epɬ-ʔewtús-m'),
        ('p’@q’ swet@', 'p’əq’ swetə'),
        ('Pil@q@-t-@s', 'ʔiləqə-t-əs'),
        ('X wa-X wPéy', 'χʷa-χʷʔéy'),
        ('tx w-lel@m’', 'txʷ-leləm’'),
        ('Nsyílxc @n', 'Nsyílxcən'),
        ('kì-qwácq@n', 'kɬ-qʷácqən'),
        ('epì-esxmíp', 'epɬ-esxmíp'),
        ('p @l-cítxw', 'pəl-cítxʷ'),
        ('epì-Pék’wn', 'epɬ-ʔék’wn'),
        ('nyoPnuntn:', 'nyoʔnuntn:'),
        ('k-sílxwaP', 'k-sílxʷaʔ'),
        ('p@l-cítxw', 'pəl-cítxʷ'),
        ('ep ì-xwúy', 'epɬ-xʷúy'),
        ('epì-síc’m', 'epɬ-síc’m'),
        ('sp’iqáìq', 'sp’iqáɬq'),
        ('sw@y’qeP', 'swəy’qeʔ'),
        ('haq w-@m', 'haqʷ-əm'),
        ('kì-síyaP', 'kɬ-síyaʔ'),
        ('ì-s@plil', 'ɬ-səplil'),
        ("c-p’@q'", 'c-p’əq’'),
        ('(Pa)kì-', '(ʔa)kɬ-'),
        ('epì-pus', 'epɬ-pus'),
        ('txw-ka:', 'txʷ-ka:'),
        ('lem- @t', 'lem-ət'),
        ('c-haq w', 'c-haqʷ'),
        ('c-tel @', 'c-telə'),
        ('ik’lí P', 'ik’líʔ'),
        ('qwácq@n', 'qʷácqən'),
        ('c-pìet', 'c-pɬet'),
        ('swet@.', 'swetə.'),
        ('swet@?', 'swetə?'),
        ('*Papì-', '*ʔapɬ-'),
        ('ik’líP', 'ik’líʔ'),
        ('*Papì', '*ʔapɬ'),
        ("p’@q'", 'p’əq’'),
        ('xň’ut', 'xƛ’ut'),
        ('Pil@q', 'ʔiləq'),
        ('epì-.', 'epɬ-.'),
        ('x wúy', 'xʷúy'),
        ('k @n', 'kən'),
        ("P@w'", 'ʔəw’'),
        ('epì-', 'epɬ-'),
        ('kwT@', 'kʷθə'),
        ('ni P', 'niʔ'),
        ('txw-', 'txʷ-'),
        ('haqw', 'haqʷ'),
        ('lıkh', 'lıkʰ'),
        ('epì', 'epɬ'),
        ('k@n', 'kən'),
        ('p@l', 'pəl'),
        ('xAt', 'xɑt'),
        ('P@', 'ʔə'),
        ('Pi', 'ʔi'),
        ('iP', 'iʔ'),
        ('kì', 'kɬ'),
        ('ì', 'ɬ'),
    ),
    # Read at 200 to 220 dpi: ʔayʔaǰuθəm on pages 1, 10 and 15 is set in the xipa font, whose codes the
    # text layer reads as P, @ and T and whose ǰ it gives as a caron alone; the formulas of (26), (27),
    # (29) and (32) on pages 11 to 13 print ⟦ ⟧ where the layer has J K; the small capitals of the
    # glosses print NMLZ-tall-3POSS, DET.OBL, ⟨INTS⟩ and (EXCLAM) whole, the layer spacing each change
    # of font; Table 1 on page 10 prints Yes; the tree of (30) on page 12 prints te with ∅ under it.
    "ICSNL56_Suharwardy_final": (
        ('J-erclausal K', '⟦-erclausal⟧'),
        ('PayPaˇ uT@m', 'ʔayʔaǰuθəm'),
        ('N-sqé ⟨q⟩xe', 'N-sqé⟨q⟩xe'),
        ('1SG.POSS -', '1SG.POSS-'),
        ('NCTRL .MID', 'NCTRL.MID'),
        ('Jp′7e7cwK', '⟦p′7e7cw⟧'),
        ('(EXCLAM )', '(EXCLAM)'),
        ('DET. OBL', 'DET.OBL'),
        ('DEM .OBL', 'DEM.OBL'),
        ('⟨INTS ⟩', '⟨INTS⟩'),
        ('NMLZ -', 'NMLZ-'),
        ('⟨DIM ⟩', '⟨DIM⟩'),
        ('te / 0', 'te ∅'),
        ('Y es', 'Yes'),
    ),
    # Read at 220 dpi: the line of page 17 that carries footnote mark 13 prints "the gesture patterns"
    # and "great detail." with the raised 13 after the stop; the text layer splits the word and spaces
    # the stop off at a kerning gap.
    "ICSNL56_Webb_final": (
        ('gestur e patterns', 'gesture patterns'),
        ('great detail .13', 'great detail.13'),
    ),
    # The text layer's spaces inside two words the glyph rows did not match: the stop the glyph
    # row of §4.4 sets on less, and the title in the Beavert & Hargus entry, which the page's own
    # character stream gives as Sínwit.
    # The glyph rows of the schedule's page 2 against a 400 dpi render: 11:00 set whole, the
    # comma above set once on m of K̓ninm̓tm̓, Q̓ʷłtal'qs with no gap, and the comma above on the k of
    # lk̓ and the N of N̓ syilxčn̓, each a word space before the next word.
    "ICSNL55_program": (
        ('1 1:00 -', '11:00 -'),
        ('K̓ninm̓̓tm̓', 'K̓ninm̓tm̓'),
        ("Q̓ʷłtal 'qs", "Q̓ʷłtal'qs"),
        ('sisp̓ lk iʔ', 'sisp̓ lk̓ iʔ'),
        ('N syilx̓čn̓', 'N̓ syilxčn̓'),
    ),
    "ICSNL56_Zenk_final": (
        ('more or less .', 'more or less.'),
        ('Ichishkíin S ínwit', 'Ichishkíin Sínwit'),
    ),
    # Tables 2 and 3 on pages 15 and 24 are set a quarter turn round and kept as the text layer
    # reads them, which puts a space where a word turns bold, gub isi'm for gubisi'm. Each word
    # read off a 200 dpi render of the page turned upright.
    "02_ICSNL55_Brown_Forbes_Schwan_final": (
        ('gub isi’m', 'gubisi’m'),
        ('gin isi’m', 'ginisi’m'),
        ('hlimoo yisi’m', 'hlimooyisi’m'),
        ('hlimoo yin', 'hlimooyin'),
        ('jakw disi’m', 'jakwdisi’m'),
        ('siwad it', 'siwadit'),
        ('siwa tdisi’m', 'siwatdisi’m'),
        ('siwa tdin', 'siwatdin'),
        ('siwa tdiit', 'siwatdiit'),
        ('’nax’nuuy ism', '’nax’nuuyism'),
        ('’nax’nuuy in', '’nax’nuuyin'),
        ('dzakw dism', 'dzakwdism'),
    ),
    # The glyph rows set the mark of footnote 11 apart, 1 1, at the note and in the text, where the
    # text layer and the page have 11.
    "03_ICSNL55_HDavis2_revised_final": (
        ('1 1 The difficulty', '11 The difficulty'),
        ('investigated.1 1', 'investigated.11'),
    ),
    # Appendix II's conversion chart sets each cell apart; the closed space after a marked letter
    # joins two cells (page 25 render). The title's note mark is an asterisk in a symbol font, which
    # the text layer holds as U+F02A (page 1 render).
    # The Symbol font's double arrow of footnotes 3 and 9, /ʔəm/ ⇒ [ʔam], U+F0DE in the text layer
    # (page 4 and 9 renders).
    "07_ICSNL55_JDavis1_final": (("", "⇒"),),
    # The same arrow in footnotes 6, 13 and 15 (page 4 and 6 renders).
    "08_ICSNL55_JDavis_2_final": (("", "⇒"),),
    # Watanabe 2003's italic title sets of Sliammon on a tight justified line, the word space under
    # the paper's threshold (page 4 render).
    "09_ICSNL55_Galligos_et_al._final": (("ofSliammon", "of Sliammon"),),
    # Jeff's quotation on page 8 prints going whole, and page 9's translation fences' with its s in
    # italics; the text layer spaces both (page 8 and 9 renders).
    "10_ICSNL55_Greymorning_final": (("go ing", "going"), ("fence s’", "fences’")),
    # The story's iʔ k̓l̓ siw̓łkʷ after tm̓xʷúlaʔxʷ on page 4 prints one comma over its k; the text
    # layer holds two, set on the same spot (page 4 render).
    "13_ICSNL55_Johnson_Barnes_Hardwick_final": (("k̓̓l̓", "k̓l̓"),),
    # (80)'s source on page 33 prints (ECH.ED.90.CD.l49) whole; the text layer spaces it after 90.
    # (page 33 render).
    "14_ICSNL55_Lyon_Czaykowska-Higgins_final": (("ECH.ED.90. CD.l49", "ECH.ED.90.CD.l49"),),
    "04_ICSNL55_HDavis1_final": (
        ('', '*'),
        ('ts c̣g ʕ', 'ts c̣ g ʕ'),
        ('ts’ c̣̓gw ʕʷ', 'ts’ c̣̓ gw ʕʷ'),
        ('l’ l̕h h', 'l’ l̕ h h'),
        ('l’ ḷ̕a a', 'l’ ḷ̕ a a'),
    ),
    # Appendix I on page 38 sets each cell of the conversion chart apart: ts c̣ g ʕ, ts' c̣̓ gw ʕʷ,
    # s ṣ w w, l' l̕ h h, l ḷ 7 ʔ, l' ḷ̕ a a. The inserted-space repair closes the space after
    # the stacked mark (renders of page 38).
    "AlexanderDavis_ICSNL61": (
        ("ts c̣g ʕ", "ts c̣ g ʕ"),
        ("ts’ c̣̓gw ʕʷ", "ts’ c̣̓ gw ʕʷ"),
        ("s ṣw w", "s ṣ w w"),
        ("l’ l̕h h", "l’ l̕ h h"),
        ("l ḷ7 ʔ", "l ḷ 7 ʔ"),
        ("l’ ḷ̕a a", "l’ ḷ̕ a a"),
    ),
    # Page 1's abstract sets a stray dot below between "such as" and x̌aƛ̓; the attached repair
    # hangs it on the s. The page prints tqalk̓ without in footnote 8 (page 5), ʔi=x̌ʷiƛ̓áz̓ =a in (38)
    # (page 13), píx̌əm̓ [kʷa (pages 15, 26, 28) and x̌aƛ̓ [kʷu (page 22) with their spaces, which the
    # inserted-space repair closes. The raised w of qʷám<qʷm> (page 8), [kʷasu (page 9) and
    # [kʷu=waʔ (pages 26, 28) is a plain w in the text layer, and ∀x in (54) and (55) (pages 19 and
    # 20) a straight double quote (renders of each page).
    # The Symbol font's empty set of the allomorph -∅ on pages 1, 9, 16, 18, 19, 20 and 22, its
    # proper subset of I ⊂ T and its negation of ¬ϕ in footnote 8 (page 6) sit in the text layer as
    # U+F0C6, U+F0CC and U+F0D8. Page 15 raises the h of -ʰx̱dła. The inserted-space repair
    # closes the page's space between the cells x̱ and x̱w of Table 2 (page 24), a line page_text.py
    # keeps as the layer has it (renders of each page).
    # The text layer sets a private-use space after Jason Brown and before Contact info (page 1)
    # where the page prints none, and the Symbol font's σμμ (page 4) as U+F073 and U+F06D; the
    # subscript is flattened, as on every σμ line. The closeup takes out the space of century of
    # phonology at the italic's start (page 10) (renders of each page).
    "02-Brown-heavy-syllables-12": (
        ("Jason Brown", "Jason Brown"), (" Contact info", "Contact info"),
        ("()", "(σμμ)"), ("century ofphonology", "century of phonology"),
    ),
    # The Symbol font's round bullet of the two lists on pages 3 and 4, seven in all, sits in the
    # text layer as U+F0B7 (renders of both pages).
    "04-MiyashitaChen_ICSNL50_FINAL-8": (
        ("", "•"),
    ),
    # The Symbol font's letters in private use, each read off a 300 dpi render of its line: U+F071 θ
    # at 13 places on pages 2, 3, 4 and 7, U+F065 ε at 11 on pages 3, 5, 6 and 8 (the Greek letter,
    # as in ICSNL57_JDavis; the 4 IPA ɛ of the paper's text font stay as printed), U+F063 χ at 4 on
    # pages 2 and 6, and U+F0DE ⇒ on pages 4 and 5. Two raised letters the layer sets on the line: the
    # ᵊ after ε in (20) and (21) on page 6, taken before the bare ε, and the ʷ of [χʷ] on page 2 and
    # /p̕uχʷ/ [p̕oχʷ] in (19). The page prints the spaces of "pileq ?" on page 3, snow' ; in (25) on page
    # 7 and the italic Journal of American at five places on pages 9 and 10.
    "05-DavisJ-ICSNL50_final-10": (("", "θ"), ("əq", "εᵊq"), ("", "ε"),
                                   ("w", "χʷ"), ("[]", "[χ]"), ("", "⇒"),
                                   ("“pileq?”", "“pileq ?”"), ("snow’;", "snow’ ;"),
                                   ("ofAmerican", "of American")),
    "17_ICSNL55_Sardinha1_final": (
        ("", "∅"), ("", "⊂"), ("", "¬"),
        ("-hx̱dła", "-ʰx̱dła"), ("x̱x̱w h", "x̱ x̱w h"),
    ),
    # Sardinha's second paper, page 8: the layer sets a space after the opening slash of each suffix in
    # Table 2's derivations, / -(g̱)a̱m/ for the printed /-(g̱)a̱m/, and the repair closes the printed space
    # of Loses initial g̱ after non- in Table 2's first row.
    "18_ICSNL55_Sardinha2_final": (("/ -", "/-"), ("g̱after", "g̱ after")),
    # Nater's Tsimshianic vestiges: the arrow font's codes U+F022 and U+F021 print → and ←, Tsimshianic
    # → Bella Coola on page 3, (← *wiʔq ← **wiq') on page 10 and Wiłpun ← NT W'ii Łpuun on page 16,
    # and U+F0D6 the root sign of (5)'s BC √pakʷ on page 6, read off 200 and 300 dpi renders. (27) on
    # page 9 prints He q'ʷḿ̩xsiwa and Ha q'ʷm̩̀ksiwa with no space in the word and two syllabic marks
    # under the m, read off a 900 dpi render.
    "9_vestiges-of-tsimshianic": (("", "→"), ("", "←"), ("", "√"),
                                  ("m̩ ̩́ x", "ḿ̩̩x"),
                                  ("m̩ ̩̀ k", "m̩̩̀k")),
    # Sardinha's eventuality types paper: Symbol-font codes read off 300 dpi renders, √ before each root
    # in (9) to (12) on pages 6 and 7, ∅ for the gloss of ʔəx̌- in (12a) and (12c) on page 7, and ∃ and
    # ⊆ in the formula (15) on page 11. Page 9 prints (13)'s italic caption Excerpt from with its space,
    # page 8 the justified head Analysis in Greene (2013) of Table 2 whole, and page 28 the italic
    # Handbook of American Indian Languages with its space.
    "13_KSardinha_Deriving-Eventual-ity-Types-in-Kwak’wala": (("", "√"), ("", "∅"),
                                                                ("", "∃"), ("", "⊆"),
                                                                ("Excerptfrom", "Excerpt from"),
                                                                ("in G reene", "in Greene"),
                                                                ("ofAmerican", "of American")),
    # McClay's Birdstone paper: page 14 prints the italic International Journal of American Linguistics
    # of Boas (1927a) with its space, read off a 400 dpi render. The gloss tiers print an unknown
    # morpheme's ? apart from the word before it, COMP ?-say in (9b) and (10) on page 5, deer-OBV ?-CONT
    # in (22a) on page 10 and Mary ?-CONT in (22b) on page 11, and close good(-IND) in both on a right
    # parenthesis the layer mirrors, read off 400 dpi renders.
    "10-McClay_Birdstone_ICSNL50_final": (("ofAmerican", "of American"), ("COMP?-say", "COMP ?-say"),
                                          ("deer-OBV?-CONT", "deer-OBV ?-CONT"),
                                          ("Mary?-CONT", "Mary ?-CONT"), ("good(-IND(", "good(-IND)")),
    # Mellesmoen's [SV] in Comox-Sliammon: the font's comma above reads as caron and comma, U+030C
    # U+0313, and each letter the page prints with the comma alone takes a caron: t̓ and k̓ of (2), q̓ of
    # Table 2, l̓, m̓, n̓, w̓, y̓ and g̓ of (3) to (5) and note 7, ƛ̓ of (13), ɣ̓ of page 14, z̓ of note 14;
    # č̓ of (3b) and (13a) comes out with two carons. The caron of ǰ is a glyph of its own the page text
    # drops, and ǰ̓ reads right; the dropped glyph leaves a space in taǰ̓•aǰ̓ of (1d), (6a) and (12a),
    # which the page prints whole. Read off 150 to 300 dpi renders and 40x crops of page 7.
    "7_Mellesmoen_SV_ComoxSliammon": (
        ("č̌̓", "č̓"), ("q̌̓", "q̓"), ("ť̓", "t̓"), ("ǩ̓", "k̓"), ("ľ̓", "l̓"), ("ň̓", "n̓"), ("m̌̓", "m̓"),
        ("w̌̓", "w̓"), ("y̌̓", "y̓"), ("ƛ̌̓", "ƛ̓"), ("ɣ̌̓", "ɣ̓"), ("ǧ̓", "g̓"), ("ž̓", "z̓"),
        ("taǰ̓ •aǰ̓", "taǰ̓•aǰ̓"),
    ),
    # Lonsdale's Seal Hunters: the Symbol font's space U+F020 after the author's name and before
    # Contact info on page 1, blank on the page, as in Koch's; the Symbol round bullet U+F0B7 of the
    # two dialect items on page 2, read off 300 dpi renders.
    "20-Lonsdale_ICSNL50_final-10": (("Deryle Lonsdale\uf020", "Deryle Lonsdale"),
                                     ("\uf020Contact info", "Contact info"),
                                     ("\uf0b7", "•")),
    # Abraham's Sasquatch story: page 1 prints kent7ú with a space before its comma, and the ! after
    # . . . spaced like the dots, read off 400 dpi renders.
    "21-Abraham_ICSNL50_final-4": (("kent7ú,", "kent7ú ,"), (". . .!", ". . . !")),
    # Davis, Huijsmans and Mellesmoen's second paper, page 8: the text layer opens spaces the page
    # does not print in the two lines before (14), ʔɛ:mɛ́t twice, As seen in (14), and
    # maʔt̓ɛ́k̓ /mat̓<í>k̓/, read off a 300 dpi render.
    "ICSNL55_Davis_Huijsmans_Mellesmoen_final2": (
        ("\u0294\u025b:m\u025b\u0301 t", "\u0294\u025b:m\u025b\u0301t"), ("As s een", "As seen"),
        ("( 14)", "(14)"),
        ("ma\u0294t\u0313\u025b\u0301 k\u0313  /mat\u0313<i\u0301>k\u0313 /",
         "ma\u0294t\u0313\u025b\u0301k\u0313 /mat\u0313<i\u0301>k\u0313/"),
    ),
    # The Symbol font's space U+F020 after the author's name and before Contact info on page 1, blank on
    # the page, as in Brown's entry. The Wingdings round bullet of (16)'s seven items on page 9 sits in the
    # text layer as U+F09F. The layer's soft hyphen U+00AD is a hyphen on the page, Moses-Columbia twice
    # and E. Czaykowska-Higgins on page 21, Bar-el on page 24. Table 1 on page 2 prints ʕ'ʷ with its w
    # raised, the only raised w of the page's eight the layer sets on the line. (14) on page 8 prints its
    # first bracket ( )p-ph with a space, and the italic of America prints its space at four places on
    # pages 22 and 23.
    "06-Koch_ICSNL50_final-24": (("Karsten A. Koch", "Karsten A. Koch"), (" Contact info", "Contact info"),
                                 ("", "•"), ("­", "-"), ("ʕ’w", "ʕ’ʷ"), ("()p-ph", "( )p-ph"),
                                 ("ofAmerica", "of America")),
    "Davis_H_ICSNL61": (
        ("aṣx̌aƛ̓", "as ̣x̌aƛ̓"),
        ("tqalk̓without", "tqalk̓ without"),
        ("qʷám<qwm>", "qʷám<qʷm>"),
        ("[kwasu", "[kʷasu"),
        ("ʔi=x̌ʷiƛ̓áz̓=a PL.DET", "ʔi=x̌ʷiƛ̓áz̓ =a PL.DET"),
        ("píx̌əm̓[kwa", "píx̌əm̓ [kʷa"),
        ("píx̌əm̓[kʷa", "píx̌əm̓ [kʷa"),
        ("x̌aƛ̓[kʷu", "x̌aƛ̓ [kʷu"),
        ("[kwu=waʔ", "[kʷu=waʔ"),
        ('"x', "∀x"),
    ),
    # Boas's raised dot, x· and g·: the text layer reads it as ∛ in note 2 on page 1 and in tiers a. and
    # f. on pages 8 to 28, as ∙ on page 8 and as a private use glyph in the italic f. of pages 14 and 15.
    # Page 15 sets the two superscript ε of unit 34's f., Aᵋyaax·siweᵋ, on a line of their own above it.
    "15-Frim_ICSNL50_final-34": (("∛", "·"), ("dÉmsx∙e", "dÉmsx·e"), ("Kā́waq", "K·ā́waq"),
                                 ("A yaaxsiwe", "Aᵋyaax·siweᵋ"), ("   ", "")),
    # Three italic forms the page sets closed up and the page text reads with a space inside, note
    # 10's posttonic k>(k)x, note 14's unattested s-√x̣il=á=q=mi(n) and Table 10's Upper Chehalis
    # (s-)ʔac(‘)-=íl(‘)als ‘inside’.
    "16-Robertson_ICSNL_final-34": (("k> (k)x", "k>(k)x"), ("s-√x̣il=á =q=mi(n)", "s-√x̣il=á=q=mi(n)"),
                                    ("(s-)ʔac(‘)- =íl(‘)als", "(s-)ʔac(‘)-=íl(‘)als")),
    # The page prints x̂, the x under a circumflex, in every Nez Perce form ((1) on page 1, (1a) on
    # page 17), and the text layer sets the circumflex as a letter after it, xˆ. Tables 1 and 2 on
    # pages 10 and 11 set their cells closed up, 'e-nees-•-se and 3O-O.PL-•, and the text layer
    # spaces inside them, around the small capitals of Table 2 and inside the words of the captions
    # and the notes; it keeps the ﬁ and ﬂ ligatures of the notes.
    "24-Deal-ICSNL50_final-26": (("xˆ", "x̂"), ("T able", "Table"), (":Results", ": Results"),
                                 ("[mo rphemes]", "[morphemes]"), ("[gl osses]", "[glosses]"),
                                 ("indicat ed", "indicated"), ("( •)", "(•)"), ("pre ﬁx", "preﬁx"),
                                 ("ﬁ", "fi"), ("ﬂ", "fl"), ("- •", "-•"), ("3 O", "3O"), ("3 S", "3S"),
                                 ("L -", "L-"), ("- S", "-S"), ("(REC )", "(REC)")),
    # Page 5 prints ˬkɬ and ˬtuɁ with their underloops in the line, and the page text reads the two
    # underloops as a line of their own under it. The raised w of (k̓ʷzúsəm) on page 5 and of
    # (kə́m̓xʷyəqs) on page 6 follows a k̓ or kə́m̓ the page sets as an image, and the page text reads
    # it as a plain w there.
    "08-vanEijk_ICSNL50_FINAL-10": (("kɬ is combined with tuɁ", "ˬkɬ is combined with ˬtuɁ"), ("ˬ   ˬ", ""),
                                    ("(k̓wzúsəm)", "(k̓ʷzúsəm)"), ("(kə́m̓xwyəqs)", "(kə́m̓xʷyəqs)")),
    # Read at 250 dpi off pages 2 to 12: the page sets č with its caron over the c, and the text
    # layer sets the caron as a letter after it, cˇ, as it does the acute of Despić. The empty set,
    # in Tables 1, 2 and 4, note 3's -∅-t and the rule of (21), reads 0/. The segmentation lines of
    # (2), (3), (5), (10) and (23) drop the raised w of kʷu kʷ'eʔ-; page_text raises the w of kʷ'eʔ
    # where it reads Sobolak's TeX-xipa10 codes as their letters, and then leaves kʷu flat there
    # and in (4). Rule (21) prints [PART+SPEAKER]
    # → ∅ / ___ [+SG] on one line, its PART set low and the text layer reading it with the blank on
    # a line under it; the probe [PART___] on pages 9 to 11 draws its blank as a rule.
    # Read at 300 dpi off pages 3, 5 and 7: Figure 1, Table 1 and Table 3 draw the raised w of kʷ,
    # qʷ, k'ʷ, q'ʷ, xʷ, χʷ and txʷ and the raised θ of t'ᶿ small by the text matrix at the body's
    # size, and the text layer sets them on the line. The captions of Examples 1 and 4 print k'w.
    # Figure 1's y’ has a small dot set low between the y and the U+2019, private-use U+F027 in the
    # layer, a mark and no letter (900 dpi render of page 3).
    "2012_Bird": (("kw   q   qw", "kʷ   q   qʷ"), ("y’", "y’"), ("k’w   q’   q’w", "k’ʷ   q’   q’ʷ"),
                  ("xw   χ   χw", "xʷ   χ   χʷ"), ("t’θ", "t’ᶿ"), ("/sqimək’w/ octopus", "/sqimək’ʷ/ octopus"),
                  ("txw/", "txʷ/")),
    # John Hamilton Davis sets the glottal stop of ’A’jia kerned into the j after it; the text layer
    # reads the apostrophe after the j and a space either side, ’A j’ ia, in (28), (35) and the
    # greetings of pages 7 and 8 (600 dpi render of page 7).
    "2012_Davis_J": (("A j’ ia", "A’jia"), ("a j’ ia", "a’jia")),
    # Davis and Van Eijk set the double pipe of an entry's comment as two bars close together, and
    # m̓ǝ̣́ṣ:m̓ǝ̣ṣ closed up at its colon; the glyph-row reader opens a word space in both (300 and 400
    # dpi renders of pages 6, 7 and 9).
    "2012_Davis_H_vanEijk": (("| |", "||"), ("m̓ǝ̣́ṣ: m̓ǝ̣ṣ", "m̓ǝ̣́ṣ:m̓ǝ̣ṣ")),
    # JeongEun Lee sets nit’sáapino’tok closed up, its apostrophe kerned back over the s, and
    # Wiltschko's wı´l with a spacing acute after the dotless i; the reader opens word spaces in
    # both. Over (18)'s rabahbót stands a stray stroke of accents rising into the gloss line above,
    # which the layer lays on the b and on the y of so.it.year.numbers (400 dpi renders of pages
    # 10, 11, 13 and 21).
    "2012_Lee": (("nit s’ áapino’tok", "nit’sáapino’tok"), ("wı ´l", "wı´l"), ("so.it.ýear", "so.it.year"),
                 ("rabahb́́ót", "rabahbót")),
    # Clarissa Forbes sets these closed up where the glyph-row reader opens a word space at a
    # straight quote or a kerned capital: italic t'uuts'xwit on page 16, can't in VG's comment on
    # page 9, AUX in Renker's title, and the closing quotes of (9), (16b)'s Intended: and (46) (300
    # and 400 dpi renders of pages 5, 7, 9, 16 and 20).
    "2012_Forbes": (("t'uuts 'xwit", "t'uuts'xwit"), ("can 't", "can't"), ("of A UX", "of AUX"),
                    ("name … '", "name…'"), ("white one. '", "white one.'"), ("barking. '", "barking.'")),
    "20_ICSNL55_Sobolak_final": (("cˇ", "č"), ("Despic´", "Despić"), ("kwu   kw’eʔ", "kʷu   kʷ’eʔ"),
                                 ("kwu   kʷ’eʔ", "kʷu   kʷ’eʔ"), ("kwu   čɬp-", "kʷu   čɬp-"),
                                 ("(21) [   +SPEAKER] → 0/ /   [+SG]", "(21) [PART+SPEAKER] → ∅ / ___ [+SG]"),
                                 ("PART   ___", ""), ("0/ /", "∅ /"), ("0/", "∅"),
                                 ("[PART   ]", "[PART___]"), ("ϕ[PART]", "ϕ[PART___]")),
    # Read off renders of pages 1 to 27. The two bulleted lists on pages 7 and 8 set a round bullet
    # the text layer reads as the Symbol font's private use glyph. The page closes up the quotation
    # mark after &c. on page 1, the ǃ of (22)'s (this)ǃ' on page 11, and the stress and length marks
    # of the appendix's pīˊcak (page 16), caplīˊl (17), mʊ́kamʊk (18, 19), itsʊ́ktʊk (20), hiˑluˑ in
    # its note 3 (22), Yeeˊnis (25) and kʚ̌mʚ̌ˊnʚ̌k (27), and the text layer spaces inside each.
    # Page 27 prints its number, 281, turned in the left margin beside the yellow entry, and the text
    # layer sets two of its digits inside the entry's (Oregon grape)8; page numbers are no row's.
    "17-Zenk_revised": (("", "•"), ("&c. \"", "&c.\""), ("(this) ǃ’", "(this)ǃ’"), ("pī ˊcak", "pīˊcak"),
                        ("caplī ˊl", "caplīˊl"), ("mʊ́kam ʊk", "mʊ́kamʊk"), ("itsʊ́kt ʊk", "itsʊ́ktʊk"),
                        ("hi ˑlu ˑ", "hiˑluˑ"), ("Yee ˊnis", "Yeeˊnis"), ("kʚ̌mʚ̌ ˊnʚ̌k", "kʚ̌mʚ̌ˊnʚ̌k"),
                        ("82   grape)8", "grape)8")),
    # Figure 6 on page 9 prints its x axis's title VarcoV closed up, in a sans face, and the text
    # layer reads a space inside it.
    "6_ICSNL2018_MarshallBird": (("Va rcoV", "VarcoV"),),
    # The text layer spaces the small capitals of footnote 2 on page 2, DISTRIB, POSS and PST, and the
    # infix -Vʔ- after its hyphen, from the title on page 1 on; page 14 prints because in (33) whole.
    # Page 9 sets the subscript m of the last line of (16) under it, as in (25) to (27) on page 12.
    "8_Mellesmoen_ICSNLinfixpaper": (("DIS TRIB", "DISTRIB"), ("POS S", "POSS"), ("PS T", "PST"),
                                     ("- Vʔ-", "-Vʔ-"), ("becaus e", "because"), ("𝜏 (𝑒)]]]", "𝜏 (𝑒𝑚)]]]")),
    # Page 3 sets the correspondence a : u in italics, spaced on both sides of the colon; the text layer
    # drops the space before it. Read at 4x.
    "10_Central-Salish-words-for-salmon": (("a: u", "a : u"),),
    # Teit's Wewêi´.tc in (1) on page 3 and Chief Waxane´ in (2) on page 6 set the accent against the
    # letter before it; the text layer spaces it off. Title 4 of (2) on page 6 breaks daughter-in-law
    # at its last hyphen at the foot of the left column; the check joins a hyphen at a line's end and
    # cannot see one at a column's, and the word's last piece goes up to the line that opens it.
    "Bischoff-etal-final": (("Wewêi ˊ.tc", "Wewêiˊ.tc"), ("Waxane ˊ", "Waxaneˊ"),
                            ("daughter-in-   26.", "daughter-in-law   26."), ("law   (Gift Test)", "(Gift Test)")),
    # Table 5 on page 9 justifies the italic translation of ʔakaⱡxa, and the text layer spaces the
    # apostrophe of one's off the word.
    "Guntly-final": (("in one 's", "in one's"),),
    # Page 4, (3), sets the language name at the right margin of each example's first line, apart
    # from its form; the text layer runs it onto the form's first word, Nłeʔkepmxcín*ʔúp(i). Read at
    # 150 dpi. The 21 heads of (3) to (15) take a space after the name.
    "ICSNL59_Nederveen_final": (("a. Nłeʔkepmxcín", "a. Nłeʔkepmxcín "), ("b. Nłeʔkepmxcín", "b. Nłeʔkepmxcín "),
                                ("c. Nłeʔkepmxcín", "c. Nłeʔkepmxcín "), ("(15) Nłeʔkepmxcín", "(15) Nłeʔkepmxcín ")),
    # Page 10 raises footnote 7's number after tspaq̓tu7semi7 in (13), whose 7 is the glottal stop, and
    # the glyph rows run the two together. Read at 200 dpi.
    "IgnaceIgnaceLyon_w7eyle_final": (("tspaq̓tu7semi77", "tspaq̓tu7semi7 7"),),
    # The glyph rows set a space after each stress mark of (1)'s phonetic forms and on either side of
    # the floating stress [́] of (2b), (4c) and (6b) and the gloss of (4a); the pages print them closed
    # up, [ˈqʌm.č’oˌθɛn] and qʷum-[́]-ut; and they close up the label (b) of (6b) on its form, which the
    # page sets apart. Read at 170 dpi.
    "MellesmoenAndreotti_NTrStative_final": (
        ("(b)??qʷaqʷ", "(b) ??qʷaqʷ"),
        ("[ˈ qʌm.č’o ˌ θɛn]", "[ˈqʌm.č’oˌθɛn]"), ("[ˈ qʌm.č’o ˌ θɛ.nəm]", "[ˈqʌm.č’oˌθɛ.nəm]"),
        ("[ˈ yɑ.ɬɑ ˌ tʌ.soɬ]", "[ˈyɑ.ɬɑˌtʌ.soɬ]"), ("qʷum-[́] -ut", "qʷum-[́]-ut"),
        ("xʷətm- [́]-əxʷ-an", "xʷətm-[́]-əxʷ-an"), ("qʷaqʷ-[́] -əxʷ-an", "qʷaqʷ-[́]-əxʷ-an"),
        ("break-[STV] -NTR-3ERG", "break-[STV]-NTR-3ERG")),
    # The right column of (41c) on page 22, (43e) on page 23 and (44b) on page 24 breaks a gloss
    # before a hyphen and sets the rest on a line of its own under it, and (41c)'s translation breaks
    # end-to-end the same way, with no closing quote on the page. Each piece goes up to the line that
    # opens it; the check joins a hyphen at a line's end and cannot see one at a line's start.
    # Read at 130 dpi.
    "2012_Jacobs": (("RED-connect-DIR", "RED-connect-DIR-LCRECIP-DIR"), ("(sticks) end", "(sticks) end-to-end"),
                    ("RED-get.hit-CAUS-TR", "RED-get.hit-CAUS-TR-CAUS.REFL"),
                    ("RED-submerge-V-DIR-TR", "RED-submerge-V-DIR-TR-CREFL")),
    # Page 6 sets the first ʔ of ʔiʔab, (in Lushootseed, ʔiʔab), in Times, at the code of a question
    # mark, a narrow space after the comma the glyph rows close up; the bold heading 2 on page 3
    # prints taqʷšəblu whole, which the glyph rows space where the Lushootseed font meets the bold.
    # Read at 300 dpi.
    # The correspondence address on page 1 sets its two stops at the end of a glyph row, and the
    # rows take the gap after each for a word space; the text layer reads it whole. The author line
    # raises every affiliation mark, and the rows raise only the a set after a name. The last row of
    # Figure 1 on page 2 prints _S and _ᐧ (_s_), and the rows read its four low lines as a line of
    # their own under it. Sentence 7 of the syllabic and the Roman telling, pages 3 and 4, opens on
    # its number set against its first word, and 7 is a letter to the token reader.
    "2013_Aistainskiaakii": (("david. osgarby@uqconnect. edu.au", "david.osgarby@uqconnect.edu.au"),
                             ("Áístainskiaakiiᵃ,b, Issapóíkoanᵃ, Áínnootaaᵃ, and David Osgarby*b,c",
                              "Áístainskiaakiiᵃ,ᵇ, Issapóíkoanᵃ, Áínnootaaᵃ, and David Osgarby*ᵇ,ᶜ"),
                             ("S   ᐧ (s)", "_S   _ᐧ (_s_)"), ("_   _ _ _", ""), ("7ᖹᖽᐧ ", "7 ᖹᖽᐧ "),
                             ("7Nííksi ", "7 Nííksi ")),
    # The gloss of (9c) on page 5 prints club-TR:REFL whole; the rows set a word space after the
    # colon, where the italic face meets the upright small capitals. Read at 300 dpi.
    "2010_Turner": (("club-TR: REFL", "club-TR:REFL"),),
    # Noguchi's Align(PPh, L; XPLEX, L) on pages 11 and 26 raises the LEX over the XP, and the rows set
    # it on a line of its own, leaving a column's gap in its place. Read at 200 dpi.
    "2011_Noguchi": (("XP   , L)", "XPLEX, L)"),),
    "2013_Palmer": (("Lushootseed,?iʔab)", "Lushootseed, ʔiʔab)"), ("taqʷš əb lu", "taqʷšəblu")),
    # The second Lillooet line of (13) on page 9 sets its four clitic underloops low, and the glyph
    # rows read them as a line of their own; each joins its clitic, as the 600 dpi render prints them.
    # Page 1 underlines the k and x of Sḵwx̱wu7mesh with a drawn rule the text layer leaves out.
    "2013_van_Eijk": (("Skwxwu7mesh", "Sḵwx̱wu7mesh"), ("-n-ǝ́m kʷuɁ   Ɂǝ ki Ɂuxʷalmíxʷ a",
                       "-n-ǝ́mˬkʷuɁ   ɁǝˬkiˬɁuxʷalmíxʷˬa"),
                      ("ˬ   ˬ ˬ   ˬ", "")),
    # The Straight face sets its f with an advance past the glyph, and the glyph rows read a space
    # after each f of a Hul'q'umi'num' form; the 300 dpi renders of pages 3 and 7 print floyt, flala
    # and flaʔ whole.
    "2011_Gerdts_Peter": tuple((spaced, spaced.replace("f ", "f")) for spaced in (
        "f loyt", "f lensəs", "f aylət", "ʔif a", "kelf ən", "ʔaf ət", "klistəf ə", "f anən",
        "ʔef lən", "f lala", "f laʔ")),
    # The text layer runs together words the italic contexts of (18), (31) and (38) and a title of
    # the references space; the 200 dpi renders of pages 6, 9 and 16 print them apart.
    "2011_Littell_Mackie": (("offish", "of fish"), ("afeeling", "a feeling"), ("anglingfor", "angling for"),
                            ("thatjob", "that job"), ("offlowers", "of flowers"), ("Evidencefrom", "Evidence from")),
    # Urbanczyk sets the English of her forms in Straight too, and the glyph rows read two of its
    # glosses letter-spaced; the 300 dpi renders of pages 2 and 15 print them whole.
    "2011_Urbanczyk": (("pull of f a layer; cut slabs f ro m wood", "pull off a layer; cut slabs from wood"),
                       ("‘bo tto m’", "‘bottom’")),
    # Rude's Table 4 on page 7 sets the second line of a cell beside the first line of the next, and
    # the text layer interleaves the letters of the Accusative uu cell and of three Genitive cells.
    # Read off a 300 dpi crop of the table. Page 33 prints the author's e-mail address closed up,
    # and the layer spaces its @.
    "2012_Rude": (("NoelRude @ CTUIR.com", "NoelRude@CTUIR.com"), ("N‘tWhokseu’umanák", "NW kuumanák ‘those’"),
                  ("kʷthoasaem’íin ‘of   kʷtwioin’ amí ‘of those   NthWoskeu’ umínk ‘of",
                   "kʷaamíin ‘of those’   kʷiinamí ‘of those two’   NW kuumínk ‘of those’")),
}

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
    for token, number in list(held.items()):
        marked = re.match(r"^(.+)'\d{1,2}$", token)
        if marked:
            held.setdefault(marked.group(1), number)
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
    joined = sequence(*(head + (reopened, attached, lettered, composed())))
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
