# 1990_Kuipers.oracle.tsv

Extraction of Corrections and additions to "A report on Shuswap" (Paris 1989) by A. H. Kuipers,
ICSNL 25.

Drafted by the anchor_sift engine and read against the page by a person. The engine sorted every
line of the text layer with english_sift.sorted_into, first against its English reference and the
pure corpus, then against this paper's own English laid over that reference. What the paper's
English did not account for became the example tiers and the cited forms. The alphabet was taken
from the characters that sit outside that English, and word_web.web() built 70 edges over the
language forms. A person then matched the context: who, kind and gloss for each row, the names,
places and languages, and the notations, read off the page. This file is the control. The reader in
corpus_script_extraction is checked against it, and where they disagree the reader is wrong until
someone reads the paper again and says otherwise.

WHOSE WORDS THESE ARE

The Shuswap forms are from Kuipers's own A Report on Shuswap (Paris 1989), its texts and its
dictionary, the misprints there and their readings; the form of Text 28 is of the Enderby (ES)
dialect.

THE LETTERS

Outside the paper's English the engine found these letters and marks: é ə ʔ ̌ ̓ λ. Kuipers's
orthography for Shuswap, typed with its marks added by hand: ə, λ, γ, x̌, ʔ, the comma above for
glottalization and the acute of stress.

THE PAGE AND THE TEXT LAYER

The forms are in NFC. The page is typed and scanned, and its text layer is OCR that holds none of
the orthography; the check reads the forms against a page text a person transcribed from the scan, a
line for each printed line. The page sets its corrections in two columns, read down the left and
then the right; the marks over k, q and t and the acutes are written in by hand.

anchor_sift's hand_extraction/oracle_check.py was run on this table through residue.py, leaving out
the notation and symbol note rows, whose form is a label and not a string the paper prints. Of 35
rows asked, 0 hold a form the repaired paper does not, and 0 a form the repair took out. Of the 724
distinct tokens in the paper, 0 language tokens are held by no row.

where    the paper's locator: the title, the front matter, a section, a footnote, an example
         line such as (3) line 2, the references, or all for a note about the whole paper
who      the language for an example tier and a cited form; the work cited on an example's
         line, or the speaker for a volunteered translation, for its English; the authors for
         the prose, the tables and the notes
kind     cited form     a word of the language named in the prose, a note or a table
         cited affix    an affix named on its own
         root           a root named on its own
         language       a language name
         name           a person or a proper name
         note           a paragraph, a context, a table, a footnote or a word that is not the language
         title          the title
form     as printed, joined where the text layer breaks a word, with no footnote digits
gloss    the page, the paper's English for a form, and what the reader needs to know

Only transcription, segmentation, phonemic, cited form, cited affix and root rows whose who is a
language are the language. The gloss tiers are the authors' analysis in English and labels.
