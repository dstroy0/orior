# Context for Alper, Basu, Bennett, Kessler, Ravana and Todd, Towards low-resource text-to-speech
# generation for Indigenous Pacific Northwest languages: The case of Haida X̱aad Kíl.

AUTHORS = ("Morris Alper, Samopriya Basu, Nathan Bennett, Ryan Kessler, S. Verlaine Ravana and "
           "Wendy F. K’ah Skáahluwaa Todd")
L = "X̱aad Kíl"

TITLE = ("Towards low-resource text-to-speech generation for Indigenous Pacific Northwest languages: "
         "The case of Haida X̱aad Kíl")

BYLINE = ("Morris Alper, Independent Scholar, Samopriya Basu, Simon Fraser University, Nathan Bennett, "
          "Future Ancestors Alliance, Ryan Kessler and S. Verlaine Ravana, X̱aadas Kíl Ḵuyáas "
          "Foundation, and Wendy F. K’ah Skáahluwaa Todd, X̱aadas Kíl Ḵuyáas Foundation & University "
          "of Alaska Southeast–Juneau")

FOUNDATION ="in X̱aadas Kíl Ḵuyáas Foundation, the affiliation of Ryan Kessler, S. Verlaine Ravana and Wendy F. K’ah Skáahluwaa Todd"

FORMS = {
    "X̱aad": ("cited form", L, "the first word of X̱aad Kíl, the Haida name of the language, in the title, the abstract and §1"),
    "Kíl": ("cited form", L, "the second word of X̱aad Kíl, the Haida name of the language; also in X̱aadas Kíl Ḵuyáas Foundation"),
    "X̱aadas": ("cited form", L, FOUNDATION),
    "Ḵuyáas": ("cited form", L, FOUNDATION),
    "K’ah": ("name", AUTHORS, "part of Wendy F. K’ah Skáahluwaa Todd's name"),
    "Skáahluwaa": ("name", AUTHORS, "part of Wendy F. K’ah Skáahluwaa Todd's name"),
    "Háw’aa": ("cited form", L, "thanks, which opens the acknowledgement, footnote *"),
    "Áljuhl": ("name", AUTHORS, "Erma Lawrence, the fluent Haida speaker whose recordings of 1974 and 2003 train the model"),
    "Áljuhl’s": ("name", AUTHORS, "Erma Lawrence's, possessive, of her voice and her family"),
    "Sámi": ("language", AUTHORS, "whose communities objected to language technology built without consent, §2.2"),
    "Māori": ("language", AUTHORS, "te reo Māori, an Indigenous Polynesian language whose online data was scraped, §2.2"),
    "ʻōlelo": ("language", AUTHORS, "the first word of ʻōlelo Hawaiʻi, Hawaiian, §2.2"),
    "Hawaiʻi": ("language", AUTHORS, "the second word of ʻōlelo Hawaiʻi, Hawaiian, §2.2"),
    "Kanienʼkéha": ("language", AUTHORS, "Mohawk, among the languages AI-generated resources misrepresent, §2.2"),
    "Diné": ("language", AUTHORS, "the first word of Diné bizaad, Navajo, §2.2"),
    "Kíilang": ("cited form", L, "the first word of Kíilang Sḵʼatʼáa, “Learning Your Language”, the title of Lawrence's book"),
    "Sḵʼatʼáa": ("cited form", L, "the second word of Kíilang Sḵʼatʼáa, “Learning Your Language”, the title of Lawrence's book"),
    "Hláa": ("cited form", L, "the first word of Hláa uu hlg̱ánggulaang, “I am working”, the phrase of Figure 2"),
    "hlg̱ánggulaang": ("cited form", L, "the last word of Hláa uu hlg̱ánggulaang, “I am working”, the phrase of Figure 2"),
    "tʰ": ("symbol note", AUTHORS, "an aspirated segment, which the IPA transcripts write ⟨t⟩"),
    "kʰ": ("symbol note", AUTHORS, "an aspirated segment, which the IPA transcripts write ⟨k⟩"),
    "á": ("symbol note", AUTHORS, "high tone as an acute over a vowel, which the transcripts replace with ꜛ before the vowel"),
    "é": ("symbol note", AUTHORS, "high tone as an acute over a vowel, which the transcripts replace with ꜛ before the vowel"),
    "í": ("symbol note", AUTHORS, "high tone as an acute over a vowel, which the transcripts replace with ꜛ before the vowel"),
    "ó": ("symbol note", AUTHORS, "high tone as an acute over a vowel, which the transcripts replace with ꜛ before the vowel"),
    "ú": ("symbol note", AUTHORS, "high tone as an acute over a vowel, which the transcripts replace with ꜛ before the vowel"),
    "ꜛ": ("symbol note", AUTHORS, "U+A71B, the IPA upstep arrow, which the transcripts set before a vowel for high tone"),
    "sg̱íw": ("cited form", L, "‘laver’, which the voice changer distorts to *spíw, §3.3.2"),
    "spíw": ("note", AUTHORS, "*spíw, the voice changer's distortion of sg̱íw ‘laver’, which does not exist in Haida; the transcript then takes ⟨p⟩"),
    "⟨ĝ⟩": ("symbol note", AUTHORS, "a rare phoneme, found only in loanwords, never in the training data, §5.1"),
    "⟨x̂⟩": ("symbol note", AUTHORS, "a rare phoneme, found only in loanwords, never in the training data, §5.1"),
    "Guzmán": ("name", AUTHORS, "D. Guzmán, an author of Pine et al., the references"),
}

DROP = ("/ʡ͡ʜ", "⟨ʡ", "arΧiv:2405.11767", "⟨g̱", "x̱⟩")

FIGURE_1 = ("page 6, a screenshot from haidalanguage.org with no text layer, six phrases in the classic "
            "Alaskan Haida orthography, each with its English: Hlk'yáawdaalw uu íijang. This is a broom. / "
            "Hlk'yáawdaalwaay í'waan-gang. The broom is big. / Hlk'yáawdaalwaay gigwáay sgíidang. The broom "
            "handle is red. / K̲'ust'áan-gyaa uu íijang. This is a crab. / K̲'ust'áan-gyaa uu táaw 'láa "
            "íijang. Crabs are good food. / K̲'ust'áan-gyaa uu isdéi díi guláagang. I like to get crabs. The "
            "K of K'ust'áan is underlined, the HTML underline §3.2 describes, read at 200 dpi")
FIGURE_3 = ("page 6, a picture with no text layer, eight lines of the LJSpeech metadata, file name | IPA "
            "transcript, each phrase repeated: wavs/el/el-what_are_you_drying_then.wav|gꜛuus tɬ'aa dꜛaŋ "
            "xilꜛaadaaŋ? (three times) / wavs/el/el-I_am_drying_black_seaweed.wav|sʔꜛiw uu ɬ xilꜛaadaaŋ. / "
            "wavs/00/0029.wav|gꜛuusgjaa uu ꜛiidʒaŋ? / wavs/00/0058.wav|tꜛaan-gjaa uu ꜛiidʒaŋ. / "
            "wavs/00/0059.wav|quŋꜛaaj uu ꜛiidʒaŋ. / wavs/00/0060.wav|k'aajɬt'ꜛaagjaa uu ꜛiidʒaŋ. / "
            "wavs/00/0061.wav|xuujꜛaaj uu ꜛiidʒaŋ. / wavs/00/0062.wav|k'ꜛaawgjaa uu ꜛiidʒaŋ. The arrow "
            "is drawn as an upward arrow in the screenshot and written here as ꜛ, read at 300 dpi")
FIGURE_4 = ("page 9, a screenshot of the prototype with no text layer. The input box holds Gám k̲íilangk "
            "k̲'áysgat-'ang., and the suggested phrase reads Gám k̲íilangk k̲'áysgat-'ang. Don't forget "
            "your own language. The special character buttons are á é í ó ú g̲ k̲ x̲ ĝ x̂, read at 220 dpi")
FIGURE_5 = ("page 11, a chart with no text layer, the average of all metrics for each of the fourteen "
            "sentences, raw against anonymized. Its labels, read turned upright at 400 dpi, where the "
            "underlines of g, k and x are hard to make out: Aadáay iik g̱at'íidang / Áyaa / Gíisdluu "
            "dajáng dáng dahgaa / Tláahl 'wáak sg̱wáansang / Dáng ḵats áakw st'i us / Ḵats jánt áatl'an "
            "ḵwáan-gang / Díi git íihlangaas uu 'wáagan / Adaahl k'íin-gan / Ts'áanuu'uu hlaa / At'án "
            "dluu tl' ḵ'áalangiidan / X̱ánjaangwaay skúnaang / Daláng x̱ánts san tl' isdáasaang / Sg̱íiwaay "
            "hlg̱álgang / G̱uhlga'áangw ts'úujuus gadáang")

TABLE_1 = [
    ("Vocal timbre 4.57/5 | 91.43% 2.07/5 | 41.42%", "vocal timbre"),
    ("Prosodic naturality 4.07/5 | 81.43% 2.93/5 | 58.57%", "prosodic naturality"),
    ("Pitch naturality 4.71/5 | 94.29% 3.5/5 | 70.00%", "pitch naturality"),
    ("Lateral accuracy 4.5/5 | 88.00% 2.2/5 | 44.00%", "the lateral obstruents"),
    ("Uvular accuracy 5/5 | 100% 2.33/5 | 46.67%", "the uvulars"),
    ("Affricate accuracy 4.56/5 | 91.11% 2.67/5 | 53.33%", "the affricates"),
    ("Epiglottal accuracy 3.83/5 | 76.66% 3.17/5 | 63.33%", "the epiglottals"),
    ("Ejective accuracy 4.5/5 | 90.00% 2.13/5 | 42.50%", "the ejectives"),
    ("Output consistency 4.29/5 | 85.71% 4.29/5 | 85.71%", "consistency"),
    ("Overall performance 88.73% 56.17%", "overall"),
]

ADD = tuple(
    [(None, ("title", AUTHORS, "title", TITLE, "the paper's title, carrying the star of the acknowledgement footnote")),
     (None, ("title", AUTHORS, "name", "Morris Alper", "author, Independent Scholar")),
     (None, ("title", AUTHORS, "name", "Samopriya Basu", "author, Simon Fraser University")),
     (None, ("title", AUTHORS, "name", "Nathan Bennett", "author, Future Ancestors Alliance")),
     (None, ("title", AUTHORS, "name", "Ryan Kessler", "author, X̱aadas Kíl Ḵuyáas Foundation")),
     (None, ("title", AUTHORS, "name", "S. Verlaine Ravana", "author, X̱aadas Kíl Ḵuyáas Foundation")),
     (None, ("title", AUTHORS, "name", "Wendy F. K’ah Skáahluwaa Todd", "author, X̱aadas Kíl Ḵuyáas Foundation and University of Alaska Southeast–Juneau")),
     (None, ("title", AUTHORS, "name", "X̱aadas Kíl Ḵuyáas Foundation", "an affiliation")),
     (None, ("title", AUTHORS, "name", "Future Ancestors Alliance", "Nathan Bennett's affiliation")),
     (None, ("title", AUTHORS, "language", "Haida", "the language of the paper, X̱aad Kíl, a critically endangered language isolate of Southeast Alaska and Haida Gwaii, fluent speakers in the single digits")),
     (None, ("title", AUTHORS, "language", "X̱aad Kíl", "the Haida name of Haida")),
     (None, ("footnote *", AUTHORS, "name", "Erma Lawrence", "Áljuhl, thanked for her dedication to preserving the Haida language; the speaker of every recording")),
     (None, ("footnote *", AUTHORS, "name", "Jordan Lachler", "thanked for documenting Haida; compiled the recordings on haidalanguage.org, Lachler 2010")),
     (None, ("footnote *", AUTHORS, "name", "Yakov Kolani", "thanked for feedback")),
     (None, ("footnote *", AUTHORS, "name", "Anthony Webster", "thanked for suggesting articles on misuse of AI")),
     (None, ("footnote *", AUTHORS, "name", "Mark Turin", "thanked for suggesting articles on misuse of AI")),
     (None, ("§1", AUTHORS, "place", "Southeast Alaska", "where Haida is spoken")),
     (None, ("§1", AUTHORS, "place", "Haida Gwaii", "where Haida is spoken, off the coast of British Columbia")),
     (None, ("§1", AUTHORS, "place", "British Columbia", "where Haida is spoken")),
     (None, ("§1", AUTHORS, "place", "Alaska", "where Haida is spoken")),
     (None, ("§1", AUTHORS, "language", "Javanese", "spoken by millions and still digitally under-resourced")),
     (None, ("§2.2", AUTHORS, "language", "te reo Māori", "an Indigenous Polynesian language whose online data was scraped")),
     (None, ("§2.2", AUTHORS, "language", "ʻōlelo Hawaiʻi", "Hawaiian, an Indigenous Polynesian language whose online data was scraped")),
     (None, ("§2.2", AUTHORS, "language", "Abenaki", "among the languages of fraudulent AI-generated resources")),
     (None, ("§2.2", AUTHORS, "language", "Mohawk", "Kanienʼkéha")),
     (None, ("§2.2", AUTHORS, "language", "Diné bizaad", "Navajo")),
     (None, ("§2.2", AUTHORS, "language", "Anishinaabemowin", "Ojibwe")),
     (None, ("§3.2", AUTHORS, "name", "haidalanguage.org", "Jordan Lachler's website, where the recordings are public")),
     (None, ("§3.2", L, "cited form", "Kíilang Sḵʼatʼáa", "“Learning Your Language”, Lawrence's second book")),
     (None, ("§3.2", AUTHORS, "place", "Kasaan", "the dialect of Lawrence's Alaskan Haida Phrasebook for Beginners, Volume 1")),
     (None, ("§3.2", L, "cited form", "Hláa uu hlg̱ánggulaang", "“I am working”, the phrase of Figure 2, repeated twice by Áljuhl")),
     (None, ("§3.2", AUTHORS, "symbol note", "⟨g̱ x̱⟩", "the underlined letters of the classic Haida orthography, for the epiglottals /ʡ͡ʜ ʜ/, displayed on haidalanguage.org with HTML underlining")),
     (None, ("§3.2", AUTHORS, "symbol note", "/ʡ͡ʜ ʜ/", "the epiglottal consonants, with U+0361 COMBINING DOUBLE INVERTED BREVE tying ʡ and ʜ")),
     (None, ("§3.3.1", AUTHORS, "symbol note", "⟨ʡ ʜ⟩", "the epiglottal symbols the transcripts use for ⟨g̱ x̱⟩")),
     (None, ("§3.3.1", AUTHORS, "symbol note", "⟨d g⟩", "“voiced” IPA symbols for the unaspirated segments [t k]")),
     (None, ("§3.3.1", AUTHORS, "symbol note", "⟨t k⟩", "“unvoiced” IPA symbols for the aspirated segments [tʰ kʰ]")),
     (None, ("§3.3.2", AUTHORS, "symbol note", "⟨p⟩", "the letter the transcript takes where the voice changer turns sg̱íw into *spíw")),
     (None, ("§4.1", AUTHORS, "symbol note", "/Cj/", "the environment where the anonymized voice fronts dorsal stops, velars sounding coronal and uvulars velar")),
     (None, ("§5.1", L, "cited affix", "-ng", "the present tense verb ending, which the model over-uses")),
     (None, ("§5.1", L, "cited affix", "-n", "the past tense verb ending, which the model under-uses")),
     (None, ("§3.2", AUTHORS, "symbol note", "Figure 1", FIGURE_1)),
     (None, ("§3.2", AUTHORS, "symbol note", "Figure 2", "page 6, a waveform in the Audacity player of Hláa uu hlg̱ánggulaang, said twice; no text layer")),
     (None, ("§3.3.1", AUTHORS, "symbol note", "Figure 3", FIGURE_3)),
     (None, ("§3.5", AUTHORS, "symbol note", "Figure 4", FIGURE_4)),
     (None, ("§4.1", AUTHORS, "symbol note", "Figure 5", FIGURE_5)),
     (None, ("§4.1", AUTHORS, "note", "Table 1: Average scores per metric used for assessment", "page 10, heads Metric, Raw audio, Anonymized audio; each cell is a score out of 5 and its percentage")),
     ]
    + [(None, ("Table 1", AUTHORS, "note", row, "page 10, the scores for %s, raw audio then anonymized audio" % what))
       for row, what in TABLE_1]
    + [(None, ("references", AUTHORS, "symbol note", "arΧiv:2405.11767", "Wang et al. 2025, written with Χ U+03A7 GREEK CAPITAL LETTER CHI"))]
)

SET = {}
REPLACE = ()

WHOSE = (
    "The Haida of this paper is Áljuhl's (Erma Lawrence), from her recordings of 1974 and 2003 on "
    "haidalanguage.org, parallel to her two books. The phrases cited in the prose, the six of Figure 1 "
    "and the fourteen test sentences of Figure 5 are hers or her books'. The IPA transcripts of "
    "Figure 3 are the authors', made by their grapheme-to-phoneme function.\n\n"
    "The who for a Haida word is X̱aad Kíl. The prose, the table, the figures, the footnote and the "
    "references carry the six authors."
)

LETTERS = (
    "The classic Alaskan Haida orthography writes high tone as an acute, á é í ó ú, the epiglottals "
    "as underlined ⟨g̱ x̱⟩ (U+0331 COMBINING MACRON BELOW in the text layer), ḵ for the uvular, "
    "the apostrophe for glottalization, hl and tl for the laterals, and ĝ x̂ in loanwords. The IPA "
    "transcripts write ꜛ U+A71B before a high-tone vowel and ʡ ʜ for the epiglottals."
)

PAGE_NOTES = (
    "The text layer spaces inside words: pro pose, low -resource, P acific, trade -off. page_text.py "
    "closed 79 lines from the glyph positions. The page sets ⟨g̱ x̱⟩ with a space, which the "
    "inserted-space repair closes; that is the page-read correction. Figures 1 to 5 are pictures with "
    "no text layer; each is written out in a symbol note, and the underlines of Figure 5 are "
    "uncertain at the chart's resolution."
)
