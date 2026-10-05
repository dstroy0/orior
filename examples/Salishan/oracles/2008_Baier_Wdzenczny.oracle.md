# 2008_Baier_Wdzenczny.oracle.tsv

Extraction of Negation in Montana Salish by Nico Baier & Di Wdzenczny, University of Michigan, ICSNL
43.

Drafted by the anchor_sift engine and read against the page by a person. The engine sorted every
line of the text layer with english_sift.sorted_into, first against its English reference and the
pure corpus, then against this paper's own English laid over that reference. What the paper's
English did not account for became the example tiers and the cited forms. The alphabet was taken
from the characters that sit outside that English, and word_web.web() built 555 edges over the
language forms. A person then matched the context: who, kind and gloss for each row, the names,
places and languages, and the notations, read off the page. This file is the control. The reader in
corpus_script_extraction is checked against it, and where they disagree the reader is wrong until
someone reads the paper again and says otherwise.

WHOSE WORDS THESE ARE

The Montana Salish examples come from the field data and texts Prof. Sally Thomason of the
University of Michigan made available (footnote 1); the Tillamook example (20) is cited from Davis
(2005:40).

THE LETTERS

Outside the paper's English the engine found these letters and marks: á é í ó ú Č č ł š ƛ ə ʔ ʷ χ.
The forms are as the paper prints them, the ʷ raised over its letter in the examples and set as a
plain w in the prose, kw esxwúyi and skw’łpaχém. The gloss labels are small capitals on the page and
capitals in the table.

THE PAGE AND THE TEXT LAYER

The forms are in NFC, and the check puts the page text, read from the glyph positions, through the
same repair: a mark the text sets after a space is put back on its letter, then NFC, then 1
page-read correction. The fonts are CID TrueType subsets with no ToUnicode map; their glyphs are
named by outline, the Times-Roman ones by raster against TeX Gyre Termes, and the paper is read by
glyph rows. The translation of (11a) is set without its quotes and that of (1c) without its closing
one.

anchor_sift's hand_extraction/oracle_check.py was run on this table through residue.py, leaving out
the notation and symbol note rows, whose form is a label and not a string the paper prints. Of 298
rows asked, 0 hold a form the repaired paper does not, and 0 a form the repair took out. Of the 6748
distinct tokens in the paper, 0 language tokens are held by no row.

where    the paper's locator: the title, the front matter, a section, a footnote, an example
         line such as (3) line 2, the references, or all for a note about the whole paper
who      the language for an example tier and a cited form; the work cited on an example's
         line, or the speaker for a volunteered translation, for its English; the authors for
         the prose, the tables and the notes
kind     transcription  an example tier in the orthography
         segmentation   an example tier broken into morphemes
         gloss          the morpheme gloss tier of an example
         translation    the English of an example
         cited form     a word of the language named in the prose, a note or a table
         language       a language name
         name           a person or a proper name
         note           a paragraph, a context, a table, a footnote or a word that is not the language
         citation       a work cited in the text, or the tag at the right of an example
         reference      an entry of the reference list, whole
         heading        a section heading
         title          the title
form     as printed, joined where the text layer breaks a word, with no footnote digits
gloss    the page, the paper's English for a form, and what the reader needs to know

Only transcription, segmentation, phonemic, cited form, cited affix and root rows whose who is a
language are the language. The gloss tiers are the authors' analysis in English and labels.
