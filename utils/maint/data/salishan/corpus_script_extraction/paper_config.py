#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# One entry per paper: who spoke it, what its orthography is, and which grains its extraction has.
#
#   Usage:  from paper_config import PAPERS, marks_for, repair_for, speakers_for
#
# This file alone holds the alphabets and the speakers. A second copy drifts. A paper in one copy and not the other is a
# paper that the coverage check reads with the wrong alphabet. The check then reports it as fully covered.
#
# WHO SPOKE IT COMES FIRST
#
# These languages belong to the people who speak them, and none of this exists without them. This file names the
# speakers, because their names are a fact about the paper, as the alphabet is. A person finds them by a read of it. A
# name is also not something to derive from a column with a heuristic. Where a paper cites a published dictionary and
# never says who spoke, the entry is empty and the index says so. That is the honest answer, and the linguist does not
# go in the field.
#
# WHAT AN ALPHABET IS FOR
#
# A token that holds one of these characters is the language. Most of these papers share ʔ, ə and ɬ, and each entry adds
# what that paper has to SHARED. A set for one paper takes SHARED and adds to it, and never stands as its own alphabet.
# A checker that knows the characters of one paper reads every other paper as empty, and then reports that nothing is
# absent from it.
#
# WHAT A GRAIN IS
#
# A named type of damage that the extraction did, out of the five that refs.md measures. A paper declares the ones it
# has, and no others. The evidence for each declaration is the comment above it, because the count behind a decision is
# a fact about that paper.

from glyph_names import decoded as glyph_names_decoded
from mary_george_repair import repaired as mary_george_repaired
from repairs import composed, corrected, one_mark, sequence
from salish_marking import TEXT_SPACE
from whitespace import any_of, closed_after_bracket, closed_after_marks, stacked_but_not

# The space that every paper here uses. salish_marking holds the one definition.
SHARED = TEXT_SPACE

# The stress accents, which the general space rule leaves alone. A word can end in a stressed vowel, and a close of the
# space after one welds it to the word after it. LaFontaine and Janzen has `ntes neʔé e sqyéytn`, and a close there
# gives `neʔée`, which the language does not have.
STRESS = "́̀"

# The stacked marks plus the accents, for a paper whose own layout makes a close after an accent safe. Both papers that
# take this print two spaces at a real boundary. A lone space after an acute is then the inserted one every time.
STACKED_AND_STRESS = "̓̌́̀̕"

# The two ways a paper can write the ejective. Where one paper writes one character both ways, the two are one mark, and
# the normal form is arithmetic.
COMMA_ABOVE = "̓"
COMMA_ABOVE_RIGHT = "̕"

# The grain nearly every paper carries: a lone space left after a stacked diacritic. Counts are in
# inserted_space.py, which is where this grain's evidence lives.
INSERTED_SPACE = closed_after_marks(stacked_but_not(STRESS))

# The same grain on a paper that also breaks at an accent.
INSERTED_SPACE_AT_ACCENTS = closed_after_marks(any_of(STACKED_AND_STRESS))


class Paper(object):
    """One paper: who spoke it, what its orthography is, and what its extraction did to it.

    coverage is the repair that the coverage check applies, named where it differs from the one that the oracle check
    applies. Two papers differ today, and neither difference was deliberate: they show two hand-kept lists that drift
    apart. A reconciliation of them changes what the coverage check reports. It is a measurement to run, and not an edit
    to make quietly.
    """

    def __init__(
        self,
        stem,
        oracle,
        record,
        language,
        speakers=(),
        marks=SHARED,
        repair=None,
        coverage=None,
        note="",
        cite="",
    ):
        self.stem = stem
        self.oracle = oracle
        self.record = record
        self.language = language
        self.speakers = tuple(speakers)
        self.marks = marks
        self.repair = repair
        self.coverage = coverage if (coverage is not None) else ()
        self.note = note
        self.cite = cite


# Two speakers of two Lushootseed dialects, the only external dialect label in the archive, and the one that the border
# test scores against.
LUSHOOTSEED_STRESS = SHARED + "ǰᶻθáíúàìù" + "̌́̀"

# The 1983 typescript's damaged orthography. Nothing in the shared set appears in it: the glottal
# stop is ? and the schwa is ~, J or G.
DAMAGED = "?~JG@V%]!"

# Lyon's two Okanagan papers, checked against a drafted page text written in the shared orthography.
# The wedge over x in x̌ast and the raised dot of ya·ʕt are the two the shared set does not carry.
OKANAGAN = SHARED + "̌·"

# The 1975 typescript, whose glottal stop is ? and which retains the rest of the orthography.
HILBERT_HESS = "?ə" + "čšɬƛᶻʷ" + "̌̓"

# Robertson writes the Thompson and Shuswap forms in Americanist symbols, and says so on page 30.
ROBERTSON = SHARED + "̣čš"

# Wolfe's forms are affixes instead of words. The set must reach a suffix in plain characters with one accent on it. The
# set gives the accents both composed and as separate marks. NFC makes á one character, while ə́ has no composed form
# and keeps its acute as a separate mark.
WOLFE = SHARED + "ʸːɛεέŋᶿθǰčšĺ" + "áéíóú" + "̌́"

# Nater's voiceless words. The apostrophe is deliberately absent. It is the ejective mark of the paper, and also the
# closing quote of every gloss. With it in, every English gloss would be a word of the language.
NATER_VOICELESS = SHARED + "̩̌"

# Lyon's inchoatives, where the root sign alone makes half the roots visible at all.
LYON_INCH = SHARED + "̌́" + "áíúé" + "ɣš√"

# Kim writes the lateral fricative ɫ, a third character for it, and marks a rule-derived glottal
# stop ˀ against phonemic ʔ.
KIM = SHARED + "ɫˀščóéɔ" + "̦́ʹ"

# Footnote 9 cites four Moses-Columbian forms, and the extraction damages each of them twice. It inserts a space inside
# the onset, and it repeats the diacritic. `x̦̦` carries two commas below where the page prints one, and `ƛ̓̓` two
# commas above.
#
# That double mark is its own type, and Robertson has it too. Page 1's epigraph prints one ɬ and one ʔ, and the text
# holds four of each. The text cannot decide it, because a language may genuinely stack two marks. A person read these
# four off the page, and the list gives them one at a time.
KIM_DOUBLED = (
    ("[p ʰtíx̦̦ʷ]", "[pʰtíx̦ʷ]"),
    ("[p ətíx̦̦ʷ]", "[pətíx̦ʷ]"),
    ("[x ƛ̓̓út]", "[xƛ̓út]"),
    ("[xəƛ̓̓út]", "[xəƛ̓út]"),
)

# Nater's etymological database, which writes the schwa two ways NFC does not unify: ǝ U+01DD 183
# times and ə U+0259 52 times.
NATER_ETYM = SHARED + "ǝ√" + "áíúà" + "ᴗɢʁʒščɣλˑ"

# Hall and colleagues on the control directive. The dot below is its rounded uvular. The characters ǰ and θ are here to
# make the Comox forms visible, so that the who column can keep them out of the target stream.
HALL_CTR = SHARED + "̣" + "áéíóúè" + "́" + "ǰθ"

# Davis and Mellesmoen on St'át'imcets reduplication. The caron is its x̌, and the dot below its retracted vowels. The
# sign √ opens 27 root citations that carry nothing else that a check could see.
DAVIS_MELLESMOEN = SHARED + "̣̌́̀" + "áíú" + "ɣ√ǰθ"

# The marks that the extraction of this paper dropped outright, put back. Each of these is a word whose diacritic the
# text lost, while the space that the typesetter made room for stays. No rule reaches them. This paper writes l followed
# by a space for both l̓ and a plain l at a real boundary, and `l=ta=q̓íl q-s=a` is the second of those.
#
# Provenance is one of two things, and there is no third. symbol_sift.py read four pairs. It finds a restoration that
# the paper itself writes elsewhere, and reports the rate at which a random mark reaches it. Those are `xʷəlp`,
# `qʷal út` twice, and `Secwep emctsín`, at scores of 160.9 and 75.9 against a chance rate of 0.136. A person read the
# rest off rendered pages, and each line names its page.
#
# Order matters. The longer contexts come first, because `qʷəl -qʷal út` must go before `qʷal út`. Otherwise the second
# fires inside the first, and leaves it half repaired.
DAVIS_MELLESMOEN_DROPPED = (
    # Read by symbol_sift from the paper's own second print of the word.
    ("xʷəlp-í<p>l əx", "xʷəlp-í<p>l̓əx"),
    ("qʷə-qʷəl -qʷal út", "qʷə-qʷəl̓-qʷal̓út"),
    ("qʷəl -qʷə-qʷal út", "qʷəl̓-qʷə-qʷal̓út"),
    ("qʷəl -qʷal út", "qʷəl̓-qʷal̓út"),
    ("qʷə-qʷal út", "qʷə-qʷal̓út"),
    ("qʷal ə́<l >t", "qʷal̓ə́<l̓>t"),
    ("qʷal út", "qʷal̓út"),
    ("Secwep emctsín", "Secwepemctsín"),
    ("S kwxwú7mesh", "Skwxwú7mesh"),
    # Page 13.
    ("ti  ́pə  ḷ", "típəḷ"),
    ("tə́<t>pə  l", "tə́<t>pəḷ"),
    ("k̓ə  ́ḷən", "k̓ə́ḷən"),
    ("k̓ə̣́́<k̓>l  ən", "k̓ə́<k̓>ḷən"),
    # Page 14.
    ("s-k̓ə  ḷ-ə̣́́<l ̣́>c̓aʔ", "s-k̓əḷ-ə́<ḷ̓>c̓aʔ"),
    ("s-k̓ə  ḷ-íc̓aʔ", "s-k̓əḷ-íc̓aʔ"),
    ("qḷ-ə ̣́́<l ̣́̓>kaʔ", "qḷ-ə́<ḷ̓>kaʔ"),
    ("qḷ-a  ́kaʔ", "qḷ-ákaʔ"),
    # Page 15.
    ("ṣtə ̣́́<t>əw", "ṣtə́<t>əw"),
    ("lapḷə̣́́<l ̣́>s", "lapḷə́<ḷ̓>s"),
    ("lapḷa  ́s", "lapḷás"),
    ("kḷə̣́́<l ̣́>si", "kḷə́<ḷ̓>si"),
    ("kḷi  ́si", "kḷísi"),
    ("ka-mə́<m>l -a", "ka-mə́<m>l̓-a"),
    ("ta=ṣtu ́h=a", "ta=ṣtụ́h=a"),
    ("ta=ṣtə ́tw̓=a", "ta=ṣtə́tw̓=a"),
    ("ta=ṣtə ́t(h)=a", "ta=ṣtə́t(h)=a"),
    # Page 18.
    ("q̓ʷə<q̓ʷ>l -ən-ás", "q̓ʷə<q̓ʷ>l̓-ən-ás"),
    ("kʷu=ḷə ̣́́<ḷ>ạy̓s", "kʷu=ḷə́<ḷ>ạy̓s"),
    ("n-ká<k>əl -xal", "n-ká<k>əl̓-xal"),
    # Page 20. mil carries the quote after it, so that the pair cannot fire inside another word.
    ("q̓í<q̓>ɬil", "q̓í<q̓>ɬil̓"),
    ("ʕí<ʕ >ƛ̓-əm", "ʕí<ʕ̓>ƛ̓-əm"),
    ("ƛ̓a ̣́́<ƛ̓>l  -ən", "ƛ̓á<ƛ̓>ḷ-ən"),
    ("ƛ̓a  ́ḷ-ạn", "ƛ̓áḷ-ạn"),
    ("n-mí<m>l -ən", "n-mí<m>l̓-ən"),
    ("mil   ‘", "mil̓   ‘"),
    ("q̓əɬʔ-ál xən", "q̓əɬʔ-álxən"),
    ("la-líl təm", "la-líl̓təm"),
    # Pages 22 and 24, the double pluractionals.
    ("q̓əy-q̓ə́<q̓>əy-l əx", "q̓əy-q̓ə́<q̓>əy-l̓əx"),
    ("ɬəʕʷ-ɬʕʷ-í<ʕʷ>l əx", "ɬəʕʷ-ɬʕʷ-í<ʕʷ>l̓əx"),
    ("ɬəʕʷ-ɬʕʷí-<ʕʷ>l əx", "ɬəʕʷ-ɬʕʷí-<ʕʷ>l̓əx"),
    # Page 25, the neologisms.
    ("√ta  ́wən", "√táwən"),
    ("təw-tə̣́́<t>wən", "təw-tə́<t>wən"),
    ("pə ̣́y-pə ̣́́<p>y̓ət", "pə́y-pə́<p>y̓ət"),
    # Page 23, where the extraction breaks a lexical suffix off its own stem.
    ("ɬəʕʷ-í<ʕ>l əx", "ɬəʕʷ-í<ʕ>l̓əx"),
    ("k̓ʷs-á<s>l ic̓aʔ", "k̓ʷs-á<s>l̓ic̓aʔ"),
    ("cə́<c>l əkst", "cə́<c>l̓əkst"),
    ("məc-xə<x>əl =ɬkán", "məc-xə<x>əl̓=ɬkán"),
    ("pə mí-<m>l̓əx", "pəmí-<m>l̓əx"),
    ("pə m-ílx", "pəm-ílx"),
    ("l=ta=q̓íl q-s=a", "l=ta=q̓ílq-s=a"),
)

# Hess's Snohomish, read off the pages of the 1967 typescript. The glottal stop is ?, and the uvular is x under a dot.
# The ŋ is there once, in the Straits cognate of the suffix that the paper is about. The hand extraction counted every
# character in this set before this file wrote it here.
HESS_SNOHOMISH = "?ə" + "čšɬƛŋ" + "ʷ" + "áéíúàìù" + "̣̓́̀"

# Elmendorf's comparative vocabularies, thirteen languages in one paper. The uvular is again x under a dot. This one
# adds the Americanist small capitals ɪ and ᴀ for lax vowels, θ, a raised y, and a raised dot for length. The apostrophe
# is in the set because Boas and Haeberlin's forms carry one inside the word, as in `sča'u`. A drop of it would split
# those forms in two.
ELMENDORF_COMPARATIVE = "?ə" + "čšɬƛθɪᴀ" + "ʷʸ" + "áäéíóú" + "̣̓́" + "·'"

# Hamp on Tillamook. This paper cites no words: what it sets out is four consonant inventories and two feature matrices.
# Its language content is segments, and not forms.
#
# Reichard's chart alone of the four carries the plain lateral affricate as well as the glottalized one. The other three
# carry the glottalized one alone. That is a fact about the charts, and not about this set, because SHARED already holds
# ƛ. A name for it here as an addition would be a character that changes nothing, and a comment that reads as though it
# does.
#
# ɔ and ɨ are the two vowel colors that TT give for q and k. The ɨ is a reading from its pair with ɔ, and not from the
# glyph, which is a typed i with a raised mark. The table shows it on a row of its own, and does not promote it here.
HAMP_TILLAMOOK = SHARED + "?" + "̣" + "ɔɨæʌɪ"

# Kinkade on Columbian deictics, with Kalispel, Coeur d'Alene and three Colville forms beside it. The set comes from the
# oracle's own form column, and not from a hand composition. Past SHARED, those rows hold acute, grave, caron, dot
# below, æ and small capital ɪ.
#
# The grave is a live contrast in this paper, and not decoration. Three -ákst pairs print an acute in the base and a
# grave on that same vowel in the derived form. `scxaʔánəm` against `scxaʔànəmákst` is the clearest. A set without
# U+0300 reads half of that alternation as unmarked.
#
# æ and ɪ belong to the two authors quoted, and not to Kinkade. The letter æ is Reichard's throughout the Coeur d'Alene
# forms of that source. The vowel ɪ is the reduplicant vowel of `cɪciʔ` and `cɪciy̓æ`. The table records it as an
# assigned reading of a short curled stroke, and not as a glyph that anyone identified.
KINKADE_COLUMBIAN = SHARED + "̣" + "́̀̌" + "æɪ"

# Givens and Hall on Bev Phillips's story of The Moon and the Birchbark Canoe. The set comes from the oracle's own form
# column: past SHARED, these rows hold the acute and the dot below. There is no grave and no caron anywhere in the
# paper.
#
# Its lateral is U+026C. The file encodes that, and so does every other Nɬeʔkepmxcín row in the corpus. The embedded
# font draws that character with a bar through the stem. A reader who works from page renders sees U+0142, and writes
# it. The symbol note of the table records the trap.
#
# The paper's own footnote 1 names the orthography: a form of the North American Phonetic Alphabet, as Thompson and
# Thompson 1992 and 1996 use it. That is a paper that states what its characters are, the strongest provenance available
# for a marks set here.
GIVENS_HALL_NLEKEPMXCIN = SHARED + "́" + "̣"

# The extraction of this paper flattens its own labialization in 24 places, and gets it right in 94. The raised w is
# U+02B7 ninety four times, and a plain full size w twenty four times, for the same segment. Those are words that the
# text layer also writes correctly elsewhere. The page is consistent and the extraction is not. Where the hand
# extraction differs from it, the defect is the extraction's, and not the reading's.
#
# A person counted every site, and read each one off a page render before it went in here. corrected() asks that of a
# pair. Seven patterns cover all twenty four:
#   - `xwúy̓`: `xʷúy̓ceʔs` on page 2 lines 2, 6 and 14, and in the segmentation of examples 4, 11, 20 and 24.
#   - `xwʔít`: `xʷʔít` on page 2 lines 3 and 5, and in examples 5, 9 and 18.
#   - `cúkw`: `cúkʷsc` on page 2 line 1, and `cúkʷ-s-c` in example 2.
#   - `tox̣w`: `tox̣ʷtés` on page 2 line 1, read again at 34x in example 3, where the dot below is visible.
#   - `zxwé`: `szxʷépmx` on page 2 line 4, and `s-zxʷép-mx` in example 7.
#   - `tmixw`: `tmixʷíyxs` on page 2 line 4, and `e=tmixʷ-íyxs` in example 7.
#   - `pkw`: `npkʷə́ps` on page 2 line 7, and `s=n-pkʷ-ə́p=s` in example 13.
#
# The file writes seven patterns, and not a rule that maps every Cw to Cʷ. That rule is the guess that
# draft_page_text.py already makes, and that papers.py already warns about. Page kʷ and page wist both arrive as w, and
# a draft labializes whichever consonant takes it. Here a person counted the sites. Each pattern is evidence about one
# word, and none of them is a rule about a character.
GIVENS_HALL_FLATTENED = (
    ("xwúy̓", "xʷúy̓"),
    ("xwʔít", "xʷʔít"),
    ("cúkw", "cúkʷ"),
    ("tox̣w", "tox̣ʷ"),
    ("zxwé", "zxʷé"),
    ("tmixw", "tmixʷ"),
    ("pkw", "pkʷ"),
)

# Every paper, and whose language is in it.
#
# The speakers come from the papers themselves. Where a paper cites a published dictionary and never says who spoke, the
# list is empty, and the index prints that. A linguist's name never goes in the field: they wrote the paper down, and
# the language is not theirs.
PAPERS = (
    Paper(
        "Mellesmoen_Kye_ICSNL61",
        "Mellesmoen_Kye_ICSNL61.oracle.tsv",
        "MarthaLamont-AnnieJack_AComparativeAnalysisOfStressInNorthernAndSouthernLushootseed"
        "_MellesmoenKye_Salish_lushootseed_2026_mixed.txt",
        "Lushootseed",
        speakers=("Martha Lamont, Northern dialect", "Annie Jack, Southern dialect"),
        marks=LUSHOOTSEED_STRESS,
        repair=sequence(
            INSERTED_SPACE_AT_ACCENTS,
            closed_after_bracket(),
            one_mark(COMMA_ABOVE_RIGHT, COMMA_ABOVE),
            composed(),
        ),
        coverage=("mellesmoen",),
        note="Both recorded by Leon Metcalf in the 1950s. The only paper here that labels "
        "every form by dialect, and the border test is scored against it.",
        cite="src:Mellesmoen-and-Kye-2026",
    ),
    Paper(
        "1983_Hilbert",
        "1983_Hilbert.oracle.tsv",
        "SusieSampsonPeter-MarthaLaMont_PokingFunInLushootseed_Hilbert"
        "_Salish_lushootseed_1983_mixed.txt",
        "Lushootseed",
        speakers=("Susie Sampson Peter, Upper Skagit", "Martha LaMont, Tulalip-Skagit"),
        marks=DAMAGED,
        note="Vi taqʷšəblu Hilbert wrote the paper; the twenty-one examples were said by her aunt "
        "Susie Sampson Peter and by Martha LaMont, recorded by Leon Metcalf between 1950 and "
        "1958 and by Thom Hess in 1963. Hilbert is the author of the paper and not its "
        "speaker.",
        cite="src:Hilbert-1983",
    ),
    Paper(
        "Matthewson_Redan_ICSNL61",
        "Matthewson_Redan_ICSNL61.oracle.tsv",
        "Kweswapaw-LindaRedan_Cw7aozKati7Lati7KuNaxwit_MatthewsonRedan"
        "_Salish_statimcets_2026_mixed.txt",
        "St'át'imcets",
        speakers=(
            "K̓weswapáw̓ (Linda Redan), Qayqáyten",
            "Sam Mitchell, in van Eijk and Williams 1981",
        ),
        repair=INSERTED_SPACE,
        coverage=("inserted spaces",),
        note="K̓weswapáw̓ told the story over Zoom on 31 October 2025, three minutes twenty-eight "
        "seconds, and the audio and video are held by her. Sam Mitchell is the speaker of "
        "the earlier text the paper cites.",
        cite="src:Matthewson-and-Redan-2026",
    ),
    Paper(
        "AlexanderDavis_ICSNL61",
        "AlexanderDavis_ICSNL61.oracle.tsv",
        "Qwa7yanak-CarlAlexander_ITsicwasSQwa7yanakAku7GraveyardValley_AlexanderDavis"
        "_Salish_statimcets_2026_mixed.txt",
        "St'át'imcets",
        speakers=("Qwa7yán'ak (Carl Alexander), Nxwísten",),
        note="Recorded at Nxwísten on 7 July 2025, just over half an hour.",
        cite="src:Alexander-and-Davis-2026",
    ),
    Paper(
        "22-Nater-Bella-Coola-tale-10",
        "22-Nater-Bella-Coola-tale-10.oracle.tsv",
        "MargaretSiwallace_ABellaCoolaTale_Nater_Salish_nuxalk_2015_nomixed.txt",
        "Nuxalk",
        speakers=("Dr. Margaret Siwallace",),
        note="Recorded about 1975, published 2015.",
        cite="src:Nater-2015",
    ),
    Paper(
        "ICSNL59_LaFontaine_Janzen_final",
        "ICSNL59_LaFontaine_Janzen_final.oracle.tsv",
        "wlwlmelst-MauriceMichell_FourStoriesByWlwlmelst_LaFontaineJanzen"
        "_Salish_nlekepmxcin_2024_mixed.txt",
        "nɬeʔkepmxcín",
        speakers=("wlwlmelst (Maurice Michell), Southern yutémkt dialect",),
        repair=INSERTED_SPACE,
        coverage=("inserted spaces",),
        note="He shares his four stories freely for people connecting with the language. They "
        "came from his mother nxwelinek and his grandmother ʔústko.",
        cite="src:LaFontaine-and-Janzen-2024",
    ),
    Paper(
        "ICSNL59_Garcia_Hannon_Stacey_final",
        "ICSNL59_Garcia_Hannon_Stacey_final.oracle.tsv",
        "Kweltezetkwu-BerniceGarcia_ThreeGlossedNlekepmxcinNarratives_GarciaHannonStacey"
        "_Salish_nlekepmxcin_2024_mixed.txt",
        "nɬeʔkepmxcín",
        speakers=("Kʷəɬtəzétkʷu (Bernice Garcia), c̓əɬétkʷu (Coldwater)",),
        repair=INSERTED_SPACE,
        coverage=("inserted spaces",),
        note="She asks it be acknowledged she is a Kamloops Indian Residential School speaker "
        "re-learning her language.",
        cite="src:Garcia-Hannon-and-Stacey-2024",
    ),
    Paper(
        "ICSNL56_DavisJ_2_final-1",
        "ICSNL56_DavisJ_2_final-1.oracle.tsv",
        "MaryGeorge_MaryGeorgePersonalNarratives_JohnHamiltonDavis"
        "_Salish_ayajuthem_2021_mixed.txt",
        "Mainland Comox (ayajuthem)",
        speakers=("Mary George, Sliammon", "Noel George Harry", "Tommy Paul"),
        repair=mary_george_repaired,
        coverage=(),
        note="Recorded 1969 to 1980. The two checks of its extraction repair it differently, and "
        "their results for it are not directly comparable.",
        cite="src:John-Hamilton-Davis-2021",
    ),
    Paper(
        "HallPhillipsICSNL60",
        "HallPhillipsICSNL60.oracle.tsv",
        "BevPhillips_WhenOldOneCreatedTheEarth_HallPhillips"
        "_Salish_nlekepmxcin_2025_nomixed.txt",
        "nɬeʔkepmxcín",
        speakers=("Bev Phillips, Lytton First Nation (ƛ̓q̓əmcín)",),
        repair=INSERTED_SPACE,
        coverage=("spaces",),
        note="Her own recorded reading of the story is held with the corpus, and it checks the "
        "extraction as well as sourcing it. The two checks of its extraction close spaces "
        "differently.",
        cite="src:Hall-and-Phillips-2025",
    ),
    Paper(
        "19-Lyon_ICSNL50_final-78",
        "19-Lyon_ICSNL50_final-78.oracle.tsv",
        "GeorgeLezard-NellieGuitterez-AndrewMcGinnis_ThreeOkanaganStoriesAboutPriests_Lyon"
        "_Salish_nsyilxcen_2015_nomixed.txt",
        "Nsyilxcən",
        speakers=(
            "George Lezard, Penticton Indian Reserve",
            "Nellie Guitterez, Upper Nicola Indian Band",
            "Kiláwnaʔ (Andrew McGinnis), Penticton Indian Reserve",
        ),
        marks=OKANAGAN,
        coverage=("page", "columns"),
        note="George Lezard recorded 1966 by Randy Bouchard, transcribed by Larry Pierre 1970, "
        "updated by permission of Arnie Baptiste, his son. Nellie Guitterez recorded 1978 "
        "or 1979 by Yvonne Hébert, reprinted by permission of Lynne Jorgesen, her "
        "great-granddaughter.",
        cite="src:Lyon-2015",
    ),
    Paper(
        "2013_Lindley_Lyon",
        "2013_Lindley_Lyon.oracle.tsv",
        "LottieLindley_TwelveMoreUpperNicolaOkanaganNarratives_LindleyLyon"
        "_Salish_nsyilxcen_2013_nomixed.txt",
        "Nsyilxcən",
        speakers=("Lottie Lindley, Upper Nicola",),
        marks=OKANAGAN,
        coverage=("page", "columns"),
        cite="src:Lindley-and-Lyon-2013",
    ),
    Paper(
        "1975_Hilbert_Hess",
        "1975_Hilbert_Hess.oracle.tsv",
        "ViHilbert-ThomHess_ANoteOnAeConstructionsInLushootseed_HilbertHess"
        "_Salish_lushootseed_1975_mixed.txt",
        "Lushootseed",
        marks=HILBERT_HESS,
        note="A 1975 typescript, scanned, and its text is OCR of the scan. The paper names "
        "no speaker for its examples. The OCR carries none of the orthography: zero schwas, "
        "zero raised w, zero barred l, zero wedges, against 169, 77, 41 and 33 in the hand "
        "extraction. It writes taqWsablu for taqʷšəblu and slahal for sləhal. All 200 "
        "disagreements in which the extraction holds a form the hand extraction does not come "
        "from that, and they are faults of the source text and not of the transcriber. Until a "
        "page text is drafted for it, as for the two Lyon papers, a check against it measures "
        "nothing.",
        cite="src:Hilbert-and-Hess-1975",
    ),
    Paper(
        "2012_Robertson",
        "2012_Robertson.oracle.tsv",
        "CharleyAlexisMayoos-WilliamCelestin_BCIndigenousPeoplesChinukPipaScript_Robertson"
        "_Salish_nlekepmxcin-secwepemctsin_2012_mixed.txt",
        "nɬeʔkepmxcín and Secwepemctsín",
        speakers=("Charley Alexis Mayoos", "William Celestin"),
        marks=ROBERTSON,
        repair=glyph_names_decoded,
        coverage=("glyph names", "line joins"),
        note="Their texts are written in Chinuk pipa. Texts 3 to 6 are Chinook Jargon, which is a "
        "pidgin and is not Salish, and the speaker column retains those out.",
        cite="src:Robertson-2012",
    ),
    Paper(
        "WolfeICSNL60",
        "WolfeICSNL60.oracle.tsv",
        "unstated_LexicalSuffixesAndConnectivesInProtoCentralSalishAndBeyond_Wolfe"
        "_Salish_centralsalish_2025_mixed.txt",
        "eighteen Central Salish languages",
        marks=WOLFE,
        repair=INSERTED_SPACE,
        coverage=("inserted spaces",),
        note="Every form is cited from a published dictionary of one of eighteen languages. there "
        "is nobody this corpus is of. The speaker column carries the language instead, and the "
        "gold standard corpus for this paper is retained per language.",
        cite="src:Wolfe-2025",
    ),
    Paper(
        "ICSNL59_Nater_2_final",
        "ICSNL59_Nater_2_final.oracle.tsv",
        "unstated_VoicelessWordsInBellaCoolaFactVsFiction_Nater"
        "_Salish_nuxalk_2024_mixed.txt",
        "Nuxalk",
        marks=NATER_VOICELESS,
        repair=INSERTED_SPACE,
        coverage=("inserted spaces",),
        note="Nater's own records, from his 1990 dictionary and 1984 grammar. No speaker is "
        "named. Six entries and two tables are Heiltsuk, Oowekyala, Kwak̓wala and Haisla, "
        "which are North Wakashan and not Salish at all.",
        cite="src:Nater-2024",
    ),
    Paper(
        "LyonICSNL60_Inch-2",
        "LyonICSNL60_Inch-2.oracle.tsv",
        "DelphineDerricksonArmstrong-DaveMichele_NsyilxcnInchoativesAndTheirDistributions"
        "AcrossRootTypes_Lyon_Salish_nsyilxcen_2025_mixed.txt",
        "Nsyilxcən",
        speakers=(
            "ɬk̓mxnalqs (Delphine Derrickson-Armstrong), stq̓aʔtkʷɬniw̓t",
            "c̓əskʕáknaʔ (Dave Michele), stq̓aʔtkʷɬniw̓t",
        ),
        marks=LYON_INCH,
        repair=INSERTED_SPACE,
        coverage=("inserted spaces",),
        note="Elicited from both speakers. Max cells of its two tables are starred, which is "
        "a form the linguist built and the speakers rejected, and those are held out.",
        cite="src:Lyon-2025",
    ),
    Paper(
        "Kim_TwanaReduplication_final",
        "Kim_TwanaReduplication_final.oracle.tsv",
        "unstated_TheTruncatedReduplicationInTwana_Kim" "_Salish_twana_2017_mixed.txt",
        "Twana",
        marks=KIM,
        # Composition runs before the corrections for the reason it does everywhere: corrected()
        # composes its own patterns. A pattern meets composed text or it matches nothing.
        repair=sequence(INSERTED_SPACE, composed(), corrected(KIM_DOUBLED)),
        coverage=("inserted spaces",),
        note="Every Twana form is Drachman's, out of a 1969 dissertation the paper calls the "
        "only reliable reference in existence for this. No speaker is named. Footnote 9's "
        "four Moses-Columbian forms are the only place its extraction repeats a combining "
        "mark, and those are corrected from the page.",
        cite="src:Kim-2017",
    ),
    Paper(
        "2013_Nater",
        "2013_Nater.oracle.tsv",
        "unstated_HowSalishIsBellaCoola_Nater" "_Salish_nuxalk_2013_mixed.txt",
        "Nuxalk",
        marks=NATER_ETYM,
        # The extraction writes the paper's ’ as ‟ and its ‘ as „, and writes ’ plainly 51 times. That is one mark set
        # two ways, put back to the way the page prints it.
        repair=sequence(INSERTED_SPACE, one_mark("‟", "’"), one_mark("„", "‘")),
        coverage=("inserted spaces",),
        note="1407 numbered entries out of Nater's own 1990 dictionary. No speaker is named.",
        cite="src:Nater-2013",
    ),
    Paper(
        "Hall-et-al_-ICSNL_61-1",
        "Hall-et-al_-ICSNL_61-1.oracle.tsv",
        "unstated_CtrlAltDeleteTheControlDirectiveAndAssociatedTDeletionInNlekepmxcin"
        "_HallLuntzlaraMellesmoenReid_Salish_nlekepmxcin_2026_mixed.txt",
        "nɬeʔkepmxcín",
        speakers=(
            "Bev Phillips",
            "c̓úʔsinek (Marty Aspinall)",
            "kʷaɬtèzetkʷ (Bernice Garcia)",
        ),
        marks=HALL_CTR,
        note="The forms are cited from Thompson and Thompson's grammar and dictionary. These "
        "three are the speakers the paper thanks, two examples are Bev Phillips reading "
        "her own story, and kʷaɬtèzetkʷ introduces herself in the acknowledgement.",
        cite="src:Hall-Luntzlara-Mellesmoen-and-Reid-2026",
    ),
    Paper(
        "ICSNL58_Davis_Mellesmoen_final",
        "ICSNL58_Davis_Mellesmoen_final.oracle.tsv",
        "unstated_ANewlyDiscoveredReduplicationPatternInStatimcetsAndItsImplications"
        "_DavisMellesmoen_Salish_statimcets_2023_mixed.txt",
        "St'át'imcets",
        speakers=("Qwa7yán'ak (Carl Alexander), Nxwísten",),
        marks=DAVIS_MELLESMOEN,
        # The dropped marks go back last, after the other grains and after composition. A person read each pair off a
        # line after those steps. A pair matches the repaired text and not the raw text, and a first place in the order
        # matches nothing at all.
        repair=sequence(
            INSERTED_SPACE_AT_ACCENTS,
            closed_after_bracket(),
            one_mark(COMMA_ABOVE_RIGHT, COMMA_ABOVE),
            composed(),
            corrected(DAVIS_MELLESMOEN_DROPPED),
        ),
        coverage=("mellesmoen",),
        note="Its data has three sources: van Eijk's dictionary, Davis et al. in preparation, "
        "and elicitation with Carl Alexander. It labels forms (U) and (L) for Upper and "
        "Lower St'át'imcets, the second external dialect label in the archive.",
        cite="src:Davis-and-Mellesmoen-2023",
    ),
    Paper(
        "1967_Hamp",
        "1967_Hamp.oracle.tsv",
        "",
        "Tillamook",
        marks=HAMP_TILLAMOOK,
        note="Eric P. Hamp, Another Look at Tillamook Phonology, ICSNL 2. It cites no words. "
        "its language content is four consonant inventories set side by side and two "
        "feature matrices. No speaker is named, and the paper works throughout from "
        "Thompson and Thompson, Reichard, Kinkade, Drachman and Edel. What it does record "
        "is that a Tillamook speaker was living in 1967 and it does not say who: the "
        "urgency of the matter is put as being that there is yet a surviving speaker "
        "available for possible re-check, and page 2 grants one sense of its claim only "
        "on the assumption that a last remaining speaker is typical of a community. Both "
        "sentences are in the table. The Twana chart is used, in the page's own words, "
        "without his permission, meaning Drachman's. The dot under the uvular fricatives "
        "prints solid in some cells of these charts and as an open ring in others, and the "
        "two positions swap between the Tillamook and Twana charts. It is one mark and "
        "the variation is the typewriter.",
        cite="src:Hamp-1967",
    ),
    Paper(
        "ICSNL58_Givens_Hall_final",
        "ICSNL58_Givens_Hall_final.oracle.tsv",
        "",
        "nɬeʔkepmxcín",
        speakers=("Bev Phillips, Lytton (ƛ̓q̓əmcín) dialect",),
        marks=GIVENS_HALL_NLEKEPMXCIN,
        # Composition comes before the corrections, as it does everywhere. The function corrected() composes its own
        # patterns, and a pattern that meets decomposed text matches nothing.
        repair=sequence(INSERTED_SPACE, composed(), corrected(GIVENS_HALL_FLATTENED)),
        coverage=("inserted spaces", "flattened labialization"),
        note="Katherine Givens and Brent Hall, The Moon and the Birchbark Canoe (ɬ máʕxetn pe ɬ "
        "qʷɬinéwɬ), ICSNL 58. Seven pages, read one at a time off page renders. Page 1: the "
        "story was recounted in Nɬeʔkepmxcín by Bev Phillips, a native speaker of the Lytton "
        "dialect, who also helped with the translation, and Givens and Hall transcribed and "
        "glossed it. She is quoted on the page choosing the story and saying creation stories "
        "are not just stories to us, and the paper's first footnote thanks her for entrusting "
        "it to them. Of the three recordings, only this one has a file named for the "
        "transcribers and not the speaker, and read from that name alone it suggests a second "
        "speaker. Footnote 4 defines (VG), a volunteered gloss, as a translated sentence BP "
        "offered. The parenthesis at the right margin is the only thing separating her English "
        "from the authors', and examples 12, 13 and 14 lack it. Its section 4 repeats every "
        "sentence of section 2 in morpheme-broken form, which gave a second independent "
        "reading of every word and turned up three places where the two tiers disagree. "
        "Footnotes 2, 6, 7 and 8 each declare a mark or a parsing the authors reached by ear "
        "or by inference, which is more than any other paper here states about its own "
        "readings.",
        cite="src:Givens-and-Hall-2023",
    ),
    Paper(
        "1967_Hess",
        "1967_Hess.oracle.tsv",
        "",
        "Snohomish",
        marks=HESS_SNOHOMISH,
        note="Thom Hess, The Morph /-(ə)b/ in Snohomish, ICSNL 2. The hand extraction is checked "
        "against the paper. No speaker is named. The forms are Hess's Snohomish data. Marked "
        "characters, page 3, by eye and inside one table: xáyəb 'laugh' and xʷúyub 'sell' "
        "print a bare x where ƛ̓áɬəb 'salty' prints a barred x body with a glottalization "
        "mark, t̓ádəb 'bitter' prints the same t as the English word taste on the first line "
        "of that page with a glottalization mark added, and d̓áƛ̓əb 'cloud' carries a hook at "
        "the top of its d and a bar across the foot, one composite, where the d of t̓ádəb one "
        "column away is bare. Below sixteen times magnification either half of that composite "
        "drops out. Glyphs in the printed text unclear. Hilbert and Hess (1975) set the same "
        "character in dəxʷgʷəƛ̓əlads, and Nater's Coast Salish *ƛ̓aɬ 'bitter, salt' matches "
        "ƛ̓áɬəb segment for segment.",
        cite="src:Hess-1967",
    ),
    Paper(
        "1967_Elmendorf",
        "1967_Elmendorf.oracle.tsv",
        "",
        "Twana",
        marks=ELMENDORF_COMPARATIVE,
        note="William W. Elmendorf, Word Tabu and Change Rates, ICSNL 2. Thirteen languages are "
        "cited in it and Twana is its subject. That is the language named here; every row "
        "carries its own language in the speaker column. No speaker is named anywhere: the "
        "forms come from Boas and Haeberlin 1927, Krueger 1967, McIlwraith 1948, Walters 1938 "
        "and Ray 1932, from Warren Snyder's Suquamish list and Wayne Suttles' Squamish field "
        "notes, and from Elmendorf's own field data. This typescript writes its uvular as x "
        "under a dot where Hilbert and Hess (1975) write it under a caron, and Hess (1967) "
        "agrees with it, which is two papers of one conference against one of a later year. "
        "One mark is unresolved and marked so in the table: a short raised stroke over the s "
        "of the Columbia ska'u on page 7, which is not the wedge the Upper Chehalis sča'u "
        "carries two words earlier.",
        cite="src:Elmendorf-1967",
    ),
    Paper(
        "1967_Kinkade",
        "1967_Kinkade.oracle.tsv",
        "",
        "Columbian",
        marks=KINKADE_COLUMBIAN,
        note="M. Dale Kinkade, Deictics in Columbian: A Work Paper, ICSNL 2. Twelve pages, read one "
        "at a time off page renders. Columbian is what it is about; Kalispel comes from Vogt "
        "1940, Coeur d'Alene from Reichard 1938, and three Colville forms from an unnamed "
        "speaker who also knew Columbian. Every row carries its own language in the speaker "
        "column. No speaker is named anywhere in the paper: it says my Cm informants and says "
        "no more than that. The forms in this table were said by people it does not identify. "
        "Its typewriter has three raised marks and the table's symbol notes turn on telling "
        "them apart, a comma with a thick head and a curling tail, a V wedge, and a straight "
        "acute. čén̓ on page 10 carries all three in one word and is the control. The wedge "
        "appears only in the Kalispel and Coeur d'Alene forms, ten times for ten, and never in "
        "the Columbian ones, whose seven caron readings are assigned and are all x̌ or the č "
        "of čiílx. Whether that split is about the language or about page order is not "
        "decidable here, because the wedge first appears on page 8 and every Columbian form "
        "was typed on pages 1 to 7. Elmendorf (1967), from the same conference, distinguishes "
        "a wedge from a short raised stroke in its own table. the distinction is one these "
        "typescripts can carry.",
        cite="src:Kinkade-1967",
    ),
)


def by_stem(stem):
    """One paper's config, by the stem its text sits under in build/papers."""
    for paper in PAPERS:
        if paper.stem == stem:
            return paper
    return None


def marks_for(stem):
    """What a paper writes its language with."""
    paper = by_stem(stem)
    return paper.marks if paper else SHARED


def repair_for(stem):
    """The grains a paper's extraction carries, composed, or None where it carries none."""
    paper = by_stem(stem)
    return paper.repair if paper else None


def speakers_for(stem):
    """Whose language is in a paper, or an empty tuple where the paper never says."""
    paper = by_stem(stem)
    return paper.speakers if paper else ()
