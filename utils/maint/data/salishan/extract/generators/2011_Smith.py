"""The ops of 2011_Smith: Sarah Smith's Effects of articulatory training on English middle-school
learners' production and perception of SENĆOŦEN uvular stops. Fifteen learners took a survey, a
production task and a perception task before and after one training session on the articulation of
the velar and uvular stops; the training raised perception of uvulars, and the learners produced
more uvulars and ejectives afterwards, while attitude and motivation did not change.

The paper has no glossed examples. Its SENĆOŦEN words stand in Table 1, Table 6 and the word lists
of Appendices A to C, each form in the APA transcription and the SENĆOŦEN orthography with its
English gloss. The survey tables, the tables of percent correct and the scales of Figure 2 are read
cell by cell from renders of pages 6 and 11 to 19, and each is checked letter for letter against
the printed lines: the survey tables set a question's wrapped lines over its label cells, and the
text layer runs them together. Figures 3 to 10 are bar charts; each chart's title, the title of its
y axis set on its side, the values up the side, the legend and the participants along the foot
are notes, the bars an image. The Praat script of Appendix G is written a printed line to a row,
the underscores _ the text layer sets on a line of their own put back in the names they join.

Page text read by glyph rows; page_text's PAPER_MARK_BASE raises the ʷ after the apostrophe of the
ejectives, [k’ʷ] and t’ᶱaqʷət.
"""
import os
import re
from collections import Counter
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

A, L = gen.A, gen.L
LANGUAGE = "SENĆOŦEN"
AUTHORS = ["Sarah Smith"]
paper = gen.Paper("2011_Smith", authors=AUTHORS[0], language=LANGUAGE)
NAMES = [("Sonya Bird", "thanked, footnote 1; Bird & Leonard (2009a)"), ("Li-Shih Huang", "thanked, footnote 1"),
         ("Montler", "Timothy Montler (1986), the outline of Saanich and the word list of the stimuli"),
         ("Werker", "Janet Werker et al. (1981), cross-language speech perception"),
         ("Best", "Catherine Best and Gerald McRoberts (2003), non-native consonant contrasts"),
         ("Lenneberg", "Eric Lenneberg (1967), the Critical Period Hypothesis, printed Lennenberg in the text"),
         ("Wang", "Yue Wang, Dawn Behne and Haisheng Jiang (2009), L1 experience in audio-visual perception"),
         ("Wipf", "Joseph Wipf (1985), second-language pronunciation"),
         ("Esling", "John Esling and Rita Wong (1983), voice quality settings"),
         ("Flege", "James Flege, Ocke-Schwen Bohn and Sunyoung Jang (1997), experience and English vowels"),
         ("Hardison", "Debra Hardison (2003), training types for English /r/ and /l/"),
         ("Kolb", "David Kolb (1984), the Experiential Learning Model"),
         ("Sewchuk", "D. H. Sewchuk (2005), experiential learning, printed Sewshuk in the references"),
         ("Whitright-Falcon", "L. Whitright-Falcon (2004), renewal of indigenous languages"),
         ("Hinton", "Leanne Hinton (2003), language revitalization"),
         ("Schmakel", "P. O. Schmakel (2008), adolescents' motivation, the model of the survey"),
         ("Lord", "Gillian Lord (2005), teaching foreign language pronunciation"),
         ("Gardner", "Robert Gardner and Wallace Lambert (1972), attitudes and motivation"),
         ("Boersma", "Paul Boersma and David Weenink (2008), Praat"),
         ("Katherine Crosswhite", "the author of the Praat script of Appendix G")]
LANGUAGES = [(LANGUAGE, "Northern Straits Salish (Central Salish), spoken around the southern tip of Vancouver Island"),
             ("Central Salish", "SENĆOŦEN"), ("Northern Straits", "SENĆOŦEN, its dialect area"),
             ("Saanich", "Montler (1986)"), ("English", "the learners' first language"),
             ("Korean", "Wang et al. (2009) and Hardison (2003)"), ("Mandarin", "Wang et al. (2009)"),
             ("Japanese", "Hardison (2003)"), ("German", "Flege et al. (1997)"), ("Spanish", "Flege et al. (1997)")]
# The page numbers run 398 to 436; the questions 22 and 24 of Table 4 and the axis values of the
# charts, 100, 90 and 0, stand alone on their lines and read as page numbers too.
paper._running = {one for one in paper.running_numbers_set() if paper.text(one) == str(paper.page(one) + 397)}
# The paper's only note is footnote 1 on page 1; the tables of pages 12 to 17 and the survey of
# Appendix D are set small and read as notes 2 to 6, and the notes are read on page 1 alone.
# Footnote 1 is set at the size of the body over it and reads as a small note only when named.
NOTE_1 = paper.find(r"^1A special thanks to Dr\. Sonya Bird")
paper.body_size_notes = range(NOTE_1, paper.find(r"^constructive feedback\.$", NOTE_1) + 1)
FOUND = paper.page_footnotes(stops=[one for one in range(1, paper.last + 1) if paper.page(one) > 1])
paper.page_footnotes = lambda *args, **kwargs: FOUND
FOOT = {one for parts, _ in FOUND.values() for one in parts}
RUNNING = paper.running_numbers_set()


def printed(number):
    return bool(paper.text(number).strip()) and not paper.lines[number][2] and number not in RUNNING \
        and number not in FOOT


def after(number):
    number += 1
    while number <= paper.last and not printed(number):
        number += 1
    return number


def same_letters(cells, first, last):
    """Check that the cells written from lines first to last hold the printed letters, all of them
    and no others, whatever the order the columns set them in."""
    written = sorted("".join(cells).replace(" ", ""))
    source = sorted("".join(paper.text(one) for one in range(first, last + 1) if printed(one)).replace(" ", ""))
    extra, missing = Counter(written) - Counter(source), Counter(source) - Counter(written)
    assert not extra and not missing, (first, last, "written only:", "".join(sorted(extra.elements())),
                                       "printed only:", "".join(sorted(missing.elements())))


def figure_1(first, where):
    """Figure 1, Kolb's model, an image the text layer holds nothing of: its caption."""
    last = after(first)
    paper.add("Figure 1", A, "note", paper.joined((first, last)), "page %d, the caption under the image" % paper.page(first))
    return after(last)


def cells_of(number):
    return re.split(r"\s{3,}", paper.spaced[number].strip())


def table_1(first, where):
    """Table 1: each stop in brackets, its description and its letter in the SENĆOŦEN orthography."""
    page = paper.page(first)
    heads = ("IPA", "Description", "SENĆOŦEN orthography")
    last = after(first)
    paper.add("Table 1", A, "note", paper.joined((first, last)), "page %d, the table's caption, over it" % page)
    line = after(last)
    assert tuple(cells_of(line)) == heads, paper.text(line)
    for head in heads:
        paper.add("Table 1", A, "note", head, "page %d, a column's head" % page)
    line = after(line)
    while paper.text(line).startswith("["):
        stop, description, letter = cells_of(line)
        paper.add("Table 1", L, "transcription", stop, "page %d, under IPA" % page)
        paper.add("Table 1", A, "note", description, "page %d, %s, the description" % (page, stop))
        paper.add("Table 1", L, "transcription", letter, "page %d, %s, in the SENĆOŦEN orthography" % (page, stop))
        line = after(line)
    return line


# The survey tables off renders of pages 12 to 14: a question's number and wording, then its average
# and the scale label the average falls under before and after the training, and each standard
# deviation.
SURVEY_HEADS = ("#", "Question", "Pretest", "Survey Scale Label", "Posttest", "Survey Scale Label")
TABLE_2 = (("1", "I want to come to school.", "2.3", "Some of the Time", "2.1", "Some of the Time", "1.1", "1.2"),
           ("2", "I like doing school work.", "3.1", "Most of the Time", "2.9", "Most of the Time", "1.1", "1.3"),
           ("6", "I like going to my SENĆOŦEN class.", "1.3", "Always", "1.7", "Some of the Time", "0.5", "0.7"),
           ("7", "I like learning SENĆOŦEN.", "1.5", "Some of the Time", "1.6", "Some of the Time", "0.9", "0.7"),
           ("8", "I like speaking SENĆOŦEN.", "1.9", "Some of the Time", "1.9", "Some of the Time", "1.2", "0.7"),
           ("9", "SENĆOŦEN is important to my community", "1.4", "Strongly Agree", "1.7", "Somewhat Agree", "0.6", "0.9"),
           ("10", "It is important that I learn SENĆOŦEN.", "1.6", "Somewhat Agree", "1.8", "Somewhat Agree", "1.0", "1.1"),
           ("11", "Learning SENĆOŦEN is important for my future.", "2.0", "Somewhat Agree", "2.1", "Somewhat Agree",
            "0.9", "1.2"),
           ("15", "I want to get good grades", "1.8", "Somewhat Agree", "1.7", "Somewhat Agree", "1.3", "1.0"),
           ("16", "I want to learn about my culture and others.", "1.7", "Somewhat Agree", "2.0", "Somewhat Agree",
            "1.1", "1.2"),
           ("27", "I like doing schoolwork on the computer.", "2.7", "Somewhat Disagree", "2.1", "Somewhat Agree",
            "1.6", "1.1"))
TABLE_3 = (("4", "I feel a part of my school.", "2.3", "Some of the Time", "2.1", "Some of the Time", "1.2", "1.2"),
           ("12", "I feel the same about school now as I did in elementary school.", "2.6", "Somewhat Disagree", "2.7",
            "Somewhat Disagree", "1.2", "1.3"),
           ("13", "I care about my grades.", "1.4", "Strongly Agree", "1.9", "Somewhat Agree", "0.8", "1.0"),
           ("14", "My friends care about their grades.", "1.9", "Somewhat Agree", "2.4", "Somewhat Agree", "1.2", "1.4"),
           ("17", "I think learning my ancestor’s language is important.", "1.5", "Somewhat Agree", "1.7",
            "Somewhat Agree", "1.0", "1.2"),
           ("19", "My family values learning SENĆOŦEN.", "2.3", "Somewhat Agree", "2.5", "Somewhat Disagree", "1.5", "1.6"),
           ("20", "I value learning SENĆOŦEN..", "1.9", "Somewhat Agree", "2.4", "Somewhat Agree", "1.2", "1.1"),
           ("21", "I am worried about losing SENĆOŦEN as a spoken language.", "2.5", "Somewhat Disagree", "2.4",
            "Somewhat Agree", "1.5", "1.5"),
           ("24", "I feel like I should learn SENĆOŦEN.", "1.6", "Somewhat Agree", "2.1", "Somewhat Agree", "0.9", "1.4"),
           ("26", "I find learning SENĆOŦEN frustrating.", "3.8", "Strongly Disagree", "4.1", "Strongly Disagree",
            "1.0", "1.2"),
           ("28", "I feel comfortable saying SENĆOŦEN words out loud in class", "3.2", "Somewhat Disagree", "3.3",
            "Somewhat Disagree", "1.1", "1.4"),
           ("29", "I feel confident in my ability to say SENĆOŦEN words.", "2.6", "Somewhat Disagree", "2.3",
            "Somewhat Agree", "1.3", "1.3"))
TABLE_4 = (("3", "I learn when I am at school.", "2.1", "Most of the Time", "2.1", "Most of the Time", "1.0", "1.2"),
           ("5", "My school appreciates everyone’s cultures.", "1.5", "Most of the Time", "1.7", "Most of the Time",
            "0.6", "0.9"),
           ("18", "Learning SENĆOŦEN is difficult.", "3.0", "Somewhat Disagree", "4.0", "Somewhat Disagree", "1.3", "1.2"),
           ("22", "I know more SENĆOŦEN words than I did in elementary school.", "2.2", "Somewhat Agree", "2.3",
            "Somewhat Agree", "1.6", "1.4"),
           ("24", "Other kids in the school think learning SENĆOŦEN is interesting.", "1.9", "Somewhat Agree", "2.1",
            "Somewhat Agree", "1.1", "1.2"))


def survey(label, rows, ends, pages):
    """Tables 2 to 4: the caption, the heads, then each question's number and wording, its AVG row
    and its SD row. Pretest and Posttest are set over two lines each, Pret est and Postt est.
    pages gives the page of each question's rows."""
    def block(first, where):
        page = paper.page(first)
        last = paper.find(ends, first) - 1
        while not printed(last):
            last -= 1
        cells = list(SURVEY_HEADS) + [cell for row in rows for cell in row] + ["AVG", "SD"] * len(rows)
        same_letters(cells, after(first), last)
        paper.add(label, A, "note", paper.text(first), "page %d, the table's caption" % page)
        for head in SURVEY_HEADS:
            broken = {"Pretest": ", set Pret over est", "Posttest": ", set Postt over est"}.get(head, "")
            paper.add(label, A, "note", head, "page %d, a column's head%s" % (page, broken))
        for number, question, pre, pre_label, post, post_label, pre_sd, post_sd in rows:
            at = "page %d, question %s" % (pages(number), number)
            paper.add(label, A, "note", number, "page %d, the question's number" % pages(number))
            paper.add(label, A, "note", question, "%s, the question" % at)
            paper.add(label, A, "note", "AVG", "%s, the head of the row of averages" % at)
            for head, cell in (("Pretest", pre), ("the pretest's Survey Scale Label", pre_label), ("Posttest", post),
                               ("the posttest's Survey Scale Label", post_label)):
                paper.add(label, A, "note", cell, "%s, AVG, %s" % (at, head))
            paper.add(label, A, "note", "SD", "%s, the head of the row of standard deviations" % at)
            for head, cell in (("Pretest", pre_sd), ("Posttest", post_sd)):
                paper.add(label, A, "note", cell, "%s, SD, %s" % (at, head))
        return after(last)
    return block


def figure_2(first, where):
    """Figure 2: two scales, each a row of responses set on their side over the numerical value
    each is given, and the questions the scale applies to under it."""
    page = paper.page(first)
    scales = ((("Always", "Some of the Time", "Most of the Time", "Not Usually", "Never", "Not Sure"),
               ("1", "2", "3", "4", "5", "0"), "Applies to Questions # 1-8"),
              (("Strongly Agree", "Somewhat Agree", "Somewhat Disagree", "Strongly Disagree", "Not Sure"),
               ("1", "2", "3", "4", "5"), "Applies to Questions # 9-29"))
    last = paper.find(r"^Applies to Questions # 9-29$", first)
    cells = [one for responses, values, applies in scales for one in
             list(responses) + list(values) + ["Response", "Numerical Value:", applies]]
    same_letters(cells, after(first), last)
    paper.add("Figure 2", A, "note", paper.text(first), "page %d, the figure's caption, over it" % page)
    for count, (responses, values, applies) in enumerate(scales, 1):
        where = "Figure 2, scale %d" % count
        paper.add(where, A, "note", "Response", "page %d, the head of the row of responses" % page)
        for response, value in zip(responses, values):
            paper.add(where, A, "note", response, "page %d, a response, set on its side" % page)
            paper.add(where, A, "note", value, "page %d, the numerical value of %s" % (page, response))
        paper.add(where, A, "note", "Numerical Value:", "page %d, the head of the row of values" % page)
        paper.add(where, A, "note", applies, "page %d, under the scale" % page)
    return after(last)


def table_5(first, where):
    """Table 5: each participant's percent correct before and after the training, whether it
    improved and the change; the average and the standard deviation under them."""
    page = paper.page(first)
    heads = ("Participant #", "Pretest (%)", "Posttest (%)", "Improvement?", "Change (%)")
    last = paper.find(r"^12\.8\s+8\.7$", first)
    rows = []
    line = after(after(after(first)))
    while line < last:
        text = paper.text(line)
        if text != "SD":
            rows.append(cells_of(line))
        line = after(line)
    assert len(rows) == 16 and rows[-1][0] == "Average", rows
    same_letters(list(heads) + [cell for row in rows for cell in row] + ["SD", "12.8", "8.7"], after(first), last)
    paper.add("Table 5", A, "note", paper.text(first), "page %d, the table's caption" % page)
    for head in heads:
        paper.add("Table 5", A, "note", head, "page %d, a column's head, the (%%) under it" % page
                  if "(%)" in head else "page %d, a column's head" % page)
    for row in rows[:-1]:
        paper.add("Table 5", A, "note", row[0], "page %d, the participant's number" % paper.page(first))
        for head, cell in zip(heads[1:], row[1:]):
            paper.add("Table 5", A, "note", cell, "page %d, participant %s, %s" % (page, row[0], head))
    paper.add("Table 5", A, "note", "Average", "page %d, the head of the row of averages" % page)
    for head, cell in zip((heads[1], heads[2], heads[4]), rows[-1][1:]):
        paper.add("Table 5", A, "note", cell, "page %d, Average, %s" % (page, head))
    paper.add("Table 5", A, "note", "SD", "page %d, the head of the row of standard deviations" % page)
    for head, cell in zip(heads[1:3], ("12.8", "8.7")):
        paper.add("Table 5", A, "note", cell, "page %d, SD, %s" % (page, head))
    return after(last)


WORD_HEADS = ("Pretest (% Correct)", "Posttest (% Correct)", "Improvement?")


def table_6(first, where):
    """Table 6: the velar words and then the uvular words, each word's percent correct before and
    after the training and whether it improved, the average and the standard deviation under
    them."""
    page = paper.page(first)
    last = paper.find(r"^SD\s+13\.5\s+15\.2$", first)
    caption_end = after(first)
    paper.add("Table 6", A, "note", paper.joined((first, caption_end)), "page %d, the table's caption" % page)
    cells = []
    line = after(caption_end)
    for group, word_head in (("Velars", "Word (IPA)"), ("Uvulars", "Word#")):
        assert paper.text(line) == group, paper.text(line)
        at = paper.page(line)
        paper.add("Table 6", A, "note", group, "page %d, the head of the rows under it" % at)
        cells.append(group)
        for head in (word_head,) + WORD_HEADS:
            paper.add("Table 6", A, "note", head, "page %d, %s, a column's head" % (at, group))
            cells.append(head)
        line = after(after(after(line)))
        while not paper.text(line).startswith("Average"):
            word, pre, post, improved = cells_of(line)
            paper.add("Table 6", L, "transcription", word, "page %d, %s, the word in IPA" % (at, group))
            for head, cell in zip(WORD_HEADS, (pre, post, improved)):
                paper.add("Table 6", A, "note", cell, "page %d, %s, %s, %s" % (at, group, word, head))
            cells.extend((word, pre, post, improved))
            line = after(line)
        for _ in range(2):
            row = cells_of(line)
            paper.add("Table 6", A, "note", row[0], "page %d, %s, the head of the row" % (at, group))
            for head, cell in zip(WORD_HEADS, row[1:]):
                paper.add("Table 6", A, "note", cell, "page %d, %s, %s, %s" % (at, group, row[0], head))
            cells.extend(row)
            line = after(line)
    same_letters(cells, after(caption_end), last)
    return line


def groups(text):
    """A side of a table as printed, its rows set off by |: a row's cells by spaces, - for an
    empty cell."""
    return [[cell if cell != "-" else "" for cell in row.split()] for row in text.split("|")]


# Tables 7 to 9 off renders of pages 16 to 18: for each section, the velar words beside the uvular
# words, each word's number in the lists of Appendix B, its percent correct before and after the
# training and whether it improved, then AVG and SD.
TABLE_7 = (("Initial",
            groups("1 46.7 53.3 Yes|3 73.3 46.7 No|11 73.3 46.7 No|13 26.7 33.3 Yes|18 40.0 33.3 No|"
                   "AVG 52.0 42.7 No|SD 20.8 8.9 -"),
            groups("4 40.0 86.7 Yes|5 46.7 60.0 Yes|7 60.0 66.7 Yes|9 73.3 73.3 No|12 33.3 40.0 Yes|"
                   "17 66.7 60.0 No|AVG 53.3 64.4 Yes|SD 15.8 15.6 -"),
            ("Improve -ment?", "Improve- ment?")),
           ("Intervocalic",
            groups("6 46.7 60.0 Yes|8 53.3 66.7 Yes|10 60.0 60.0 No|15 53.3 40.0 No|AVG 53.3 56.7 Yes|SD 4.7 10.0 -"),
            groups("2 40.0 53.3 Yes|14 60.0 60.0 No|16 66.7 33.3 No|AVG 55.6 48.9 No|SD 11.3 11.3 -"),
            ("Improve -ment?", "Improve- ment?")))
TABLE_8 = (("Plain",
            groups("3 73.3 46.7 No|8 53.3 66.7 Yes|15 53.3 40 No|AVG 60.0 51.1 -|SD 11.5 13.9 -"),
            groups("4 40 86.7 Yes|7 60 66.7 Yes|16 66.7 33.3 No|AVG 55.6 62.2 -|SD 13.9 27.0 -"),
            ("Improve -ment?", "Improve- ment?")),
           ("Ejective",
            groups("1 46.7 53.3 Yes|10 60 60 No|18 40 33.3 No|AVG 48.9 48.9 -|SD 10.2 13.9 -"),
            groups("2 40 53.3 Yes|9 73.3 73.3 No|17 66.7 60 No|AVG 60 62.2 -|SD 17.6 10.2 -"),
            ("Improve- ment?", "Improve- ment?")))
TABLE_9 = (("Unrounded",
            groups("3 73.3 46.7 No|8 53.3 66.7 Yes|15 53.3 40.0 No|AVG 60.0 51.1 -|SD 11.5 13.9 -"),
            groups("4 40.0 86.7 Yes|7 60.0 66.7 Yes|16 66.7 33.3 No|AVG 55.6 62.2 -|SD 13.9 27.0 -"),
            ("Improve- ment?", "Improve -ment?")),
           ("Rounded",
            groups("6 46.7 60.0 Yes|11 73.3 46.7 No|13 26.7 33.3 Yes|AVG 48.9 46.7 -|SD 23.4 13.4 -"),
            groups("5 46.7 60.0 Yes|12 33.3 40.0 Yes|14 60.0 60.0 No|AVG 46.7 53.3 -|SD 13.4 11.5 -"),
            ("Improve- ment?", "Improve -ment?")))


def word_table(label, sections, ends, over=None, first_word=("Word #", "Word #")):
    """Tables 7 to 9: over the sections a head, where the table has one; in each section the head
    of the section, then Velars and Uvulars side by side, each with its four columns. The column
    heads wrap: Word # and Improvement? each over two lines, Improve- ment? or Improve -ment?."""
    def block(first, where):
        page = paper.page(first)
        last = paper.find(ends, first)
        paper.add(label, A, "note", paper.text(first), "page %d, the table's caption" % page)
        printed_cells = []
        if over:
            paper.add(label, A, "note", over, "page %d, the head of the table" % page)
            printed_cells.append(over)
        for section, velars, uvulars, improvements in sections:
            where = "%s, %s" % (label, section)
            paper.add(where, A, "note", section, "page %d, the head of the section" % page)
            printed_cells.append(section)
            for group, rows, improvement, word in (("Velars", velars, improvements[0], first_word[0]),
                                                   ("Uvulars", uvulars, improvements[1], first_word[1])):
                paper.add(where, A, "note", group, "page %d, %s, the head of the four columns under it" % (page, section))
                printed_cells.append(group)
                for head, as_printed in (("Word #", word), (WORD_HEADS[0], WORD_HEADS[0]),
                                         (WORD_HEADS[1], WORD_HEADS[1]), (WORD_HEADS[2], improvement)):
                    wrap = ", set %s over two lines" % " over ".join(as_printed.split()) if head in ("Word #", WORD_HEADS[2]) else ""
                    paper.add(where, A, "note", head, "page %d, %s, %s, a column's head%s" % (page, section, group, wrap))
                    printed_cells.append(as_printed)
                for row in rows:
                    number = row[0]
                    what = "the word's number" if number.isdigit() else "the head of the row"
                    paper.add(where, A, "note", number, "page %d, %s, %s, %s" % (page, section, group, what))
                    for head, cell in zip(WORD_HEADS, row[1:]):
                        if cell:
                            paper.add(where, A, "note", cell, "page %d, %s, %s, %s, %s" % (page, section, group, number, head))
                    printed_cells.extend(row)
        same_letters(printed_cells, after(first), last)
        return after(last)
    return block


def table_10(first, where):
    """Table 10: the average and the standard deviation of the overall percent correct, before and
    after the training."""
    page = paper.page(first)
    last = paper.find(r"^SD\s+6\.3\s+7$", first)
    same_letters(["Overall (% correct)", "Pretest", "Posttest", "AVERAGE", "75.4", "76.4", "SD", "6.3", "7"],
                 after(first), last)
    paper.add("Table 10", A, "note", paper.text(first), "page %d, the table's caption" % page)
    paper.add("Table 10", A, "note", "Overall (% correct)", "page %d, the head over the two columns" % page)
    for head in ("Pretest", "Posttest"):
        paper.add("Table 10", A, "note", head, "page %d, a column's head, under Overall (%% correct)" % page)
    for row in ("AVERAGE", "75.4", "76.4"), ("SD", "6.3", "7"):
        paper.add("Table 10", A, "note", row[0], "page %d, the row's head" % page)
        for head, cell in zip(("Pretest", "Posttest"), row[1:]):
            paper.add("Table 10", A, "note", cell, "page %d, %s, %s" % (page, row[0], head))
    return after(last)


# Each chart's title, the lines of its caption and title, and the title of its y axis, set on its
# side, off renders of pages 19 to 24; the values up the side from the top.
CHARTS = {"3": (2, "Overall Production Scores", "Percent Correct (%)", "100 80 60 40 20 0"),
          "4": (1, "Place of Articulation (Velar or Uvular) Production Task", "Percent Correct (%)", "100 80 60 40 20 0"),
          "5": (2, "Rounded or Unrounded Production Task", "Percent Correct (%)", "120 100 80 60 40 20 0"),
          "6": (1, "Ejective or Non-Ejective Production Task", "Percent Correct", "120 100 80 60 40 20 0"),
          "7": (1, "Production Task - Ejectives", "Percent Correct (%)", "100 90 80 70 60 50 40 30 20 10 0"),
          "8": (1, "Production Task - Non-Ejectives", "Percent Correct (%)", "120 100 80 60 40 20 0"),
          "9": (1, "Production Task - Uvulars", "Percent Correct (%)", "90 80 70 60 50 40 30 20 10 0"),
          "10": (1, "Production Task - Velars", "Percent Correct (%)", "120 100 80 60 40 20 0")}


def chart(first, where):
    """Figures 3 to 10, bar charts of each participant's percent correct before and after the
    training: the caption over the chart, the chart's title, the title of the y axis set on its
    side, the values up the side, the legend, and the participants' numbers and the axis title
    along the foot. The text layer reads the chart across, the letters of the side title one or
    two to a line among the values and the legend."""
    number = re.match(r"^Figure (\d+)\.", paper.text(first)).group(1)
    wrapped, title, side, values = CHARTS[number]
    page = paper.page(first)
    lines = [first]
    while len(lines) < wrapped:
        lines.append(after(lines[-1]))
    last = paper.find(r"^Participant #$", first)
    label = "Figure " + number
    foot = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"
    same_letters([title, side, values, "Pretest", "Posttest", foot, "Participant #"], after(lines[-1]), last)
    paper.add(label, A, "note", paper.joined(lines), "page %d, the figure's caption, over the chart" % page)
    paper.add(label, A, "note", title, "page %d, the chart's title" % page)
    paper.add(label, A, "note", side, "page %d, the title of the y axis, set on its side" % page)
    paper.add(label, A, "note", values, "page %d, the values up the y axis, from the top" % page)
    paper.add(label, A, "note", "Pretest", "page %d, the legend, beside a blue square" % page)
    paper.add(label, A, "note", "Posttest", "page %d, the legend, beside a red square" % page)
    paper.add(label, A, "note", foot, "page %d, the participants' numbers along the x axis, a pair of bars over each"
              % page)
    paper.add(label, A, "note", "Participant #", "page %d, the title of the x axis" % page)
    return after(last)


def word_list(label, heads, ends):
    """Appendices A to C: a heading, the column heads, then each word's number, its APA
    transcription, its SENĆOŦEN orthography and its English gloss."""
    def block(first, where):
        page = paper.page(first)
        paper.add(label, A, "heading", paper.text(first), "page %d" % page)
        line = after(first)
        head_lines = []
        while not re.match(r"^1\s", paper.text(line)):
            head_lines.append(line)
            line = after(line)
        same_letters(heads, head_lines[0], head_lines[-1])
        for head in heads:
            paper.add(label, A, "note", head, "page %d, a column's head" % page)
        while not re.match(ends, paper.text(line)):
            cells = cells_of(line)
            if len(cells) == 3:
                # The APA form stands a single space from its orthography, mətaqʷəŋ METOḰEN.
                cells[1:2] = cells[1].split(" ")
            number, apa, orthography, english = cells
            at = paper.page(line)
            paper.add(label, A, "note", number, "page %d, the word's number" % at)
            paper.add(label, L, "transcription", apa, "page %d, word %s, under APA" % (at, number))
            paper.add(label, L, "transcription", orthography, "page %d, word %s, in the SENĆOŦEN orthography" % (at, number))
            paper.add(label, A, "translation", english, "page %d, word %s, the English gloss" % (at, number))
            line = after(line)
        return line
    return block


def survey_form(first, where):
    """Appendix D, the survey: two grids of check boxes, the responses set on their side over the
    boxes, each question beside its row of boxes and wrapped over as many lines as it takes."""
    page = paper.page(first)
    paper.add("Appendix D", A, "heading", paper.text(first), "page %d" % page)
    scales = (("Always", "Most of the time", "Sometimes", "Not usually", "Never", "Not Sure"),
              ("Strongly Agree", "Somewhat agree", "Somewhat disagree", "Strongly disagree", "Not sure"))
    last = paper.find(r"^SENĆOŦEN words\.$", first)
    questions, line, open_question = [], after(first), False
    printed_cells = list(scales[0]) + list(scales[1])
    while line <= last:
        opened = re.match(r"^(\d+)\.\s+(.*)$", paper.text(line))
        # A question wraps onto the line under it; the responses set on their side over the second
        # grid, lAeerggy hteergaaw, open on a lone letter and end the question over them.
        if opened:
            questions.append([opened.group(1), opened.group(2), paper.page(line)])
        elif open_question and len(paper.text(line)) > 1 and (paper.text(line)[0].islower() or paper.text(line).startswith("SENĆOŦEN")):
            questions[-1][1] += " " + paper.text(line)
        open_question = bool(opened) or open_question and len(paper.text(line)) > 1
        line = after(line)
    assert [one[0] for one in questions] == [str(one) for one in range(1, 30)], questions
    printed_cells += ["%s. %s" % (one[0], one[1]) for one in questions]
    same_letters(printed_cells, after(first), last)
    for count, scale in enumerate(scales):
        for response in scale:
            paper.add("Appendix D", A, "note", response, "page %d, a column's head, set on its side over the check boxes"
                      % paper.page(first if count == 0 else paper.find(r"^9\.\s", first)))
        for number, question, at in questions:
            if (int(number) < 9) == (count == 0):
                paper.add("Appendix D", A, "note", "%s. %s" % (number, question),
                          "page %d, a question, beside a check box under each response" % at)
    return after(last)


# The lines of the Praat script the text layer sets with their underscores on a line of their own
# under them, and each as the render of page 37 prints it.
PRAAT_JOINED = {"number of files = Get number of strings": "number_of_files = Get number of strings",
                "print number of files: 'number of files' 'newline$'":
                    "print number of files: 'number_of_files' 'newline$'",
                "for x from 1 to number of files": "for x from 1 to number_of_files",
                "# Now we will set up a string variable called \"current file$\" and use it to store":
                    "# Now we will set up a string variable called \"current_file$\" and use it to store",
                "printline DEBUG: 'current file$'": "printline DEBUG: 'current_file$'",
                "# Now I am setting up a variable called \"object name$\" that will have the":
                    "# Now I am setting up a variable called \"object_name$\" that will have the",
                "object name$ = selected$ (\"Sound\")": "object_name$ = selected$ (\"Sound\")"}


def praat(first, where):
    """Appendix G: the Praat script, a printed line to a row; a comment the page wraps runs on
    to the next line, which is its own row as printed."""
    page = paper.page(first)
    paper.add("Appendix G", A, "heading", paper.text(first), "page %d" % page)
    last = paper.find(r"^## edited by Marion", first)
    written, count, line = [], 0, after(first)
    while line <= last:
        text = paper.text(line)
        if re.fullmatch(r"_(?: _)*", text):
            line = after(line)
            continue
        fixed = PRAAT_JOINED.get(text, text)
        count += 1
        why = "page %d, a line of the script" % page
        if fixed != text:
            why += ", its underscores set under it by the text layer and put back from the render"
        paper.add("Appendix G line %d" % count, A, "note", fixed, why)
        written.append(fixed)
        line = after(line)
    same_letters(written, after(first), last)
    return after(last)


BLOCKS = {paper.find(r"^Figure 1\. Kolb"): figure_1,
          paper.find(r"^Table 1\. Phonetic features"): table_1,
          paper.find(r"^Figure 2\. Survey scales"): figure_2,
          paper.find(r"^Table 2\. Pre- and post-survey"): survey("Table 2", TABLE_2, r"^Table 3 depicts", lambda number: 12),
          paper.find(r"^Table 3\. Pre- and post-survey"): survey("Table 3", TABLE_3, r"^Table 4 depicts", lambda number: 13),
          paper.find(r"^Table 4\. Pre- and post-survey"): survey("Table 4", TABLE_4, r"^Overall there were no",
                                                                 lambda number: 13 if number == "3" else 14),
          paper.find(r"^Table 5\. Average"): table_5,
          paper.find(r"^Table 6\. Average"): table_6,
          paper.find(r"^Table 7\. Average"): word_table("Table 7", TABLE_7, r"^SD\s+4\.7\s+10\.0$"),
          paper.find(r"^Table 8\. Average"): word_table("Table 8", TABLE_8, r"^SD\s+10\.2\s+13\.9\s+SD",
                                                         over="Plain versus Ejectives"),
          paper.find(r"^Table 9\. Average"): word_table("Table 9", TABLE_9, r"^SD\s+23\.4",
                                                         over="Rounded vs. Unrounded", first_word=("Wor d#", "Word #")),
          paper.find(r"^Table 10\. Overall"): table_10,
          paper.find(r"^Appendix A:"): word_list("Appendix A", ("Production Task #", "APA", "Orthography", "English Gloss"),
                                                 r"^Appendix B:"),
          paper.find(r"^Appendix B:"): word_list("Appendix B", ("Perception Task #", "APA", "Orthography", "English Gloss"),
                                                 r"^Appendix C:"),
          paper.find(r"^Appendix C:"): word_list("Appendix C", ("Training Sound #", "APA", "Orthography", "English Gloss"),
                                                 r"^Appendix D:"),
          paper.find(r"^Appendix D:"): survey_form,
          paper.find(r"^Appendix G:"): praat}
for number in CHARTS:
    BLOCKS[paper.find(r"^Figure %s\. " % number)] = chart
paper.standard(AUTHORS, NAMES, LANGUAGES, blocks=BLOCKS, appendix=r"^Sarah C\. Smith$")
tail = paper.find(r"^Sarah C\. Smith$")
line = tail
for what in ("the author's name", "the street address", "the city", "the postal code", "the e-mail address"):
    paper.add("end", A, "note", paper.text(line), "page %d, %s" % (paper.page(line), what))
    line = after(line)
paper.write()
