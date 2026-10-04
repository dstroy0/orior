# Nater-final.oracle.tsv

Extraction of The position of Bella Coola within Salish: bound morphemes by Hank Nater, ICSNL 49.

Drafted by the anchor_sift engine and read against the page by a person. The engine sorted every
line of the text layer with english_sift.sorted_into, first against its English reference and the
pure corpus, then against this paper's own English laid over that reference. What the paper's
English did not account for became the example tiers and the cited forms. The alphabet was taken
from the characters that sit outside that English, and word_web.web() built 312 edges over the
language forms. A person then matched the context: who, kind and gloss for each row, the names,
places and languages, and the notations, read off the page. This file is the control. The reader in
corpus_script_extraction is checked against it, and where they disagree the reader is wrong until
someone reads the paper again and says otherwise.

WHOSE WORDS THESE ARE

The Bella Coola forms are the author's, from his dictionary, Nater (1990), whose entry numbers the
entries keep. Each cognate is the language its abbreviation names, from the source in parentheses
after it: Squamish from Kuipers (1967, 1969), Shuswap from Kuipers (1974), Upper Chehalis from
Kinkade (1991), Lillooet from Van Eijk (1985, 2013), Heiltsuk and North Wakash from Rath (2010) and
Lincoln and Rath (1980), and the proto-Salish reconstructions from Kuipers (2002). A starred form
with no stage named is the earlier Bella Coola form the author reconstructs.

THE LETTERS

Outside the paper's English the engine found these letters and marks: Ø á é í š ƛ ǝ ɬ ʔ ʷ ˽ χ ṇ. The
Bella Coola forms are in the author's Americanist orthography: ’ for the glottal stop's release,
written after the letter, t’ and k’ʷ, ʔ for the glottal stop, ʷ for rounding, ɬ, ƛ’, x, χ and c, ˑ
for length, and ˽ (U+02FD) for the boundary of a clitic, ʔaɬ˽ and ˽tχ. The cognates keep their
sources' letters, ǝ (U+01DD) for schwa, š, č, the acute of stress, and ṇ in Kwakiutl maq’ʷṇs.

THE PAGE AND THE TEXT LAYER

The forms are in NFC, and the check puts the text layer through the same repair: the space the PDF
sets after a stacked mark is closed, except before an opening quote, then NFC, then 1 page-read
correction.

anchor_sift's hand_extraction/oracle_check.py was run on this table through residue.py, leaving out
the notation and symbol note rows, whose form is a label and not a string the paper prints. Of 1233
rows asked, 0 hold a form the repaired paper does not, and 0 a form the repair took out. Of the
11220 distinct tokens in the paper, 0 language tokens are held by no row.

where    the paper's locator: the title, the front matter, a section, a footnote, an example
         line such as (3) line 2, the references, or all for a note about the whole paper
who      the language for an example tier and a cited form; the work cited on an example's
         line, or the speaker for a volunteered translation, for its English; the authors for
         the prose, the tables and the notes
kind     rule           a line of a rule, a derivation or a tree the authors display
         cited form     a word of the language named in the prose, a note or a table
         cited affix    an affix named on its own
         root           a root named on its own
         language       a language name
         name           a person or a proper name
         note           a paragraph, a context, a table, a footnote or a word that is not the language
         reference      an entry of the reference list, whole
         heading        a section heading
         title          the title
form     as printed, joined where the text layer breaks a word, with no footnote digits
gloss    the page, the paper's English for a form, and what the reader needs to know

Only transcription, segmentation, phonemic, cited form, cited affix and root rows whose who is a
language are the language. The gloss tiers are the authors' analysis in English and labels.
