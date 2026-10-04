"""Match context onto the engine's reconstruction and write the oracle table.

usage: python finish.py <stem>

Reads <stem>.draft.tsv, written by paper_sift.py, and finish/<stem>.py, the context a person read
off the page: who and kind for the names, places and languages the draft calls cited forms, rows to
add (title, names, notations, symbol notes), and text to replace where the text layer broke a row.

Generic steps, the same for every paper, after the rules a context file sets (WHO_RULES for whose a
row is, KIND_RULES for what it is):
  - a note that is only the printed page number is dropped, and one the page number opens loses it
  - a note opening with a footnote number moves to where "footnote N"
  - a translation ending in a citation, a speaker tag or a bracketed label is split in two
  - reference section notes become one reference row per entry
  - cited form candidates are kept once per section
"""
import importlib.util
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from workdir import ORACLES, WORK  # noqa: E402

# What can close a translation's line: a source with its year, a speaker tag, a bracketed label,
# or the text and lines a sentence comes from, (Sandro Botticelli: Primavera, lines 3–4), or the
# code of a story and its line, (WSa.GE.357) or (MJJ 68), with a footnote mark after it.
# A work in preparation stands for its year, (Forbes et al. in prep.:HH: Before the people die).
# A tag can open on what the speaker did, (volunteered HH) or (volunteered HH; accepted BS, JH) in
# Hill and Matthewson. A disc recording and its time stands for a source, (694 side 2,
# 00:04:44.902 – 00:04:46.274) in the Joe Peter Chinook Transcription Project.
TRAILER = re.compile(r"^(.*?[’”.?!̓])\s*((?:\[[^\]]*\]\s*)?\((?:vt|vf|sf|volunteered\b|accepted\b"
                     r"|\d+ side \d[^()]*"
                     r"|[A-Z][^()]*(?:\d{4}|in prep\.)[^()]*"
                     r"|[A-Z][^()]*, lines? \d+(?:[–-]\d+)?"
                     r"|[A-Z]{2,}[A-Za-z]*(?:[. ][\w–-]+)*)[^()]*\)?\d{0,2}"
                     r"|\[[^\]]*\]|\((?:vt|vf|sf)[^()]*\)?)\s*$")
ENTRY = re.compile(r"(?<=[.)/]) (?=[A-Z][A-Za-z’'\-]+(?:,| \([A-Z])[ A-Z.,&]{0,40}(?:\(\d{4}\)|, [A-Z]\.)"
                   r"|[A-Z][A-Za-z’'\-]+, [A-Z][a-z]+(?:-[A-Z][a-z]+)?(?: [A-Z]\.)*(?: &| and [A-Z]|,|\.? \d{4}| \(\d{4})"
                   r"|[A-Z][A-Za-z’'\-]+, (?:[A-Z]\. ?)+and [A-Z])")


def load_context(stem):
    # finish/<stem>.py when a paper has one, with ops/<stem>.ops laid over it, the one-line ops
    # oracle.py takes on stdin.
    import ops
    path = os.path.join(HERE, "finish", stem + ".py")
    if not os.path.exists(path):
        return ops.apply(ops.blank(stem), stem)
    spec = importlib.util.spec_from_file_location("context_" + re.sub(r"\W", "_", stem), path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return ops.apply(module, stem)


def main():
    stem = sys.argv[1]
    context = load_context(stem)
    with open(os.path.join(WORK, stem + ".draft.tsv"), encoding="utf-8") as handle:
        rows = [line.rstrip("\n").split("\t") for line in handle][1:]
    # A context that reads the rest of the paper off the page again keeps the draft rows before
    # that point, in Zenk the rows before §4, whose examples the text layer ran into the prose.
    if getattr(context, "DRAFT_UNTIL", None) is not None:
        rows = rows[:context.DRAFT_UNTIL]

    printed = {}
    for where, who, kind, form, gloss in rows:
        if kind == "note" and form.isdigit():
            page = int(re.search(r"page (\d+)", gloss).group(1))
            printed[page] = form
    offsets = {int(form) - page for page, form in printed.items()}
    footnotes = {re.match(r"^(\d{1,2}) ", form).group(1) for where, who, kind, form, gloss in rows
                 if kind == "note" and re.match(r"^\d{1,2} [A-Z]", form)}
    digit_letters = any(re.search(r"\d[^\W\d_]", form) for where, who, kind, form, gloss in rows
                        if kind == "transcription")

    out = []
    seen_forms = set()
    initials = getattr(context, "INITIALS", {})

    def example_of(where):
        # (3) line 2, §3.2 (3) line 2 in a paper that numbers its examples again in each section,
        # or footnote 4 (i) line 6.
        at_example = re.match(r"^((?:§\S+ |footnote \S+ )?\([^)\s]+\))", where)
        return at_example.group(1) if at_example else ""

    # Whose each example is, from its tag wherever it stands, before or after a comment: (Name | VF)
    # names the speaker first, and (SF | BP 22 May 2025) puts the elicitation code first and the
    # speaker's initials after it.
    tag_names = {}
    for where, who, kind, form, gloss in rows:
        example = example_of(where)
        tagged = re.search(r"\(([^|()]+)\|\s*([^|()]*)\)?", form) \
            if kind in ("translation", "citation", "speaker comment") else None
        if tagged and example and example not in tag_names:
            first = tagged.group(1).strip()
            if re.fullmatch(r"[A-Z]{2}(?:, ?[A-Z]{2})*", first) and tagged.group(2).split():
                tag_names[example] = tagged.group(2).split()[0].rstrip(";,")
            elif kind != "speaker comment":
                tag_names[example] = first
    for where, who, kind, form, gloss in rows:
        example = example_of(where)
        if kind == "speaker comment":
            if who in ("Comment", "Comments", "Consultant’s", "Consultant's"):
                # A comment closing each quote on its speaker's initials, "It'd be weird." (RJ), is
                # theirs; otherwise it is the tagged speaker's.
                said = [one for group in re.findall(r"\(([A-Z]{2}(?:, ?[A-Z]{2})*)\)", form)
                        for one in re.split(r", ?", group) if one in initials]
                names = list(dict.fromkeys(initials[one] for one in said))
                if names:
                    who = names[0] if len(names) == 1 else ", ".join(names[:-1]) + " and " + names[-1]
                else:
                    who = tag_names.get(example, context.AUTHORS)
            who = initials.get(who, who)
        for old, new in getattr(context, "REPLACE", ()):
            form = form.replace(old, new)
        # The language labels of a comparative table, Th /=kʷ/, written out.
        who = getattr(context, "WHO", {}).get(who, who)
        # A who the engine took from a speaker tag, SF | BP 22 May 2025, names the person it stands for.
        for who_rule, kind_rule, new_who, rule_gloss in getattr(context, "WHO_MATCH", ()):
            if re.search(who_rule, who) and re.search(kind_rule, kind):
                who = new_who
                gloss = "%s, %s" % (gloss, rule_gloss) if rule_gloss else gloss
                break
        # A paper whose examples set a practical orthography over a phonemic line: the row holding
        # a letter only the phonemic alphabet has is the segmentation, and the row with none of
        # them is the orthography, whatever the engine called it.
        script = getattr(context, "TIER_SCRIPT", "")
        if script and where.startswith(("(", "§", "Table")) and " line " in where and \
                kind in ("transcription", "segmentation"):
            kind = "segmentation" if any(one in script for one in form) else "transcription"
        # A rule a person read off the page: the forms of these examples that match are this
        # language's, the Salish analysis set beside a Dene word.
        for where_rule, kind_rule, form_rule, rule_who, rule_gloss in getattr(context, "WHO_RULES", ()):
            if re.search(where_rule, where) and kind == kind_rule and re.search(form_rule, form):
                who = rule_who
                gloss = "%s, %s" % (gloss, rule_gloss) if rule_gloss else gloss
                break
        # A rule a person read off the page for what a row is: the lines of an example that hold a
        # formula, λ⃗v.⃗v ∈ σ(α), are the author's denotation and not a tier of the language.
        for where_rule, kind_rule, form_rule, new_kind, new_who, rule_gloss in \
                getattr(context, "KIND_RULES", ()):
            if re.search(where_rule, where) and re.search(kind_rule, kind) and \
                    re.search(form_rule, form):
                kind, who = new_kind, new_who
                gloss = "%s, %s" % (gloss, rule_gloss) if rule_gloss else gloss
                break
        page_match = re.search(r"page (\d+)", gloss)
        page = int(page_match.group(1)) if page_match else 0
        if kind == "note":
            if form.isdigit() and (int(form) - page) in offsets | {int(form) - page + 1}:
                continue
            lead = re.match(r"^(\d{1,4}) (.*)$", form)
            if lead and any((int(lead.group(1)) - one) in offsets for one in (page, page - 1)):
                form = lead.group(2)
            # The star can sit on the first word, *As ever, my deepest debt in Davis.
            footnote = re.match(r"^(\d{1,2}|\*) (?=\S)", form) or \
                (re.match(r"^(\d{1,2}|\*)(?=[A-Z][a-z])", form) if "footnote" in gloss else None)
            if footnote and not example and where not in ("appendix", "references"):
                where = "footnote %s" % footnote.group(1)
        if kind in ("transcription", "segmentation", "gloss", "phonemic"):
            marker = re.match(r"^(.*[.,;:!?])(\d{1,2})$", form)
            if marker:
                form = marker.group(1)
                gloss = "%s, carries footnote %s, written here without its digit" % (gloss, marker.group(2))
        # A tier word can carry the footnote mark itself, Indigenous-people7 or x̣ʷóx̣ʷstms8, where the
        # paper has a footnote of that number. A gloss label puts its digits first, 3SG, and is left
        # alone, and so is every orthography tier of a paper whose alphabet has a digit, Sḵwx̱wú7mesh.
        # A tier whose own words write a digit as a letter, s=7áts̓x or nká7=as in St’át’imcets, keeps
        # every digit it has, láti7 with them.
        own_digits = re.search(r"\d[^\W\d_]|[^\W\d_]\d[=\-]", form)
        if kind == "gloss" or (kind in ("transcription", "segmentation") and not digit_letters
                               and not own_digits):
            # A cell can hold two forms with a mark each, tsedutní15, dune16.
            # A gloss can close on an infix label, 3SUBJ-CS-LCRF<IPFV>2.
            marks = [one for one in re.finditer(
                r"(?:(?<=[^\W\d_]{2})|(?<=[^\W\d_][̀-ͯ])|(?<=[̀-ͯ][^\W\d_])|(?<=[A-Z]>))"
                r"(\d{1,2})(?=[\s,;]|$)", form)
                     if one.group(1) in footnotes]
            for marker in reversed(marks):
                form = form[:marker.start()] + form[marker.end():]
            if marks:
                gloss = "%s, carries footnote %s, written here without its digit" % (
                    gloss, " and ".join(one.group(1) for one in marks))
        # Where the paper says one person gave every translation, the translation is theirs.
        if kind == "translation" and getattr(context, "TRANSLATION_WHO", None) and \
                who == context.AUTHORS:
            who = context.TRANSLATION_WHO
            gloss = "%s, volunteered by the speaker" % gloss
        if kind == "translation":
            split = TRAILER.match(form)
            if split:
                # A volunteered translation is the speaker's own English.
                if "vt" in split.group(2) and getattr(context, "VT_WHO", None):
                    who = context.VT_WHO.get(example, context.VT_WHO.get("all", who))
                    gloss = "%s, volunteered by the speaker" % gloss
                # (Name | CF,VG): VG is a volunteered gloss or translation, the speaker's English.
                tagged = re.search(r"\(([^|()]+)\|\s*([^)]*)\)", split.group(2))
                if tagged and "VG" in tagged.group(2):
                    who = tagged.group(1).strip()
                    gloss = "%s, volunteered by the speaker" % gloss
                out.append([where, who, kind, split.group(1), gloss])
                out.append([where, context.AUTHORS, "citation", split.group(2), "the tag or source at the right of the translation"])
                continue
        # A section that is itself one kind of row, the English of a story set as paragraphs.
        if kind == "note" and where in getattr(context, "SECTIONS", {}) and "footnote" not in gloss:
            kind, who, gloss = context.SECTIONS[where]
            marks = re.findall(r"(?<=[.?!’”])(\d{1,2})(?=\s|$)", form)
            form = re.sub(r"(?<=[.?!’”])\d{1,2}(?=\s|$)", "", form)
            if marks:
                gloss = "%s, carries footnote %s, written here without its digit" % (
                    gloss, " and ".join(marks))
        if where == "references" and kind == "note":
            # A line break inside an entry can end a note, Fieldwork DRAWL. URL: above its URL, and
            # the URL joins the entry above it. So does a line saying where the work is found,
            # Accessed via the Kinkade Collection under Kinkade 1989 in Davis and Nederveen.
            if out and out[-1][0] == "references" and out[-1][2] == "reference" and \
                    (form.startswith("http") or out[-1][3].endswith(":") or
                     re.match(r"^(?:Accessed|Available|Retrieved|Downloaded)\b", form)):
                form = out.pop()[3] + " " + form
            pieces = ENTRY.split(form)
            for number, entry in enumerate(pieces):
                # A publisher and its place can look like an author and a name, GLSA, Amherst, MA.
                # at the end of Matthewson and Todorovic 2018; a piece with no digit in it is the
                # end of the entry before it.
                if number and not re.search(r"\d", entry) and len(entry) < 40:
                    out[-1][3] += " " + entry.strip()
                    continue
                out.append([where, context.AUTHORS, "reference", entry.strip(), ""])
            continue
        if kind == "cited form":
            if form in context.DROP:
                continue
            key = (where, form)
            if key in seen_forms:
                continue
            seen_forms.add(key)
            if form in context.FORMS:
                kind, who, gloss = context.FORMS[form]
            else:
                gloss = gloss.replace("candidate, ", "")
                # A root marked √ and an affix with its hyphen are named on their own.
                if form.startswith("√"):
                    kind = "root"
                # An optional segment can open one, (ʔa)kɬ-.
                elif re.match(r"^-\S*[^\W\d_]$|^\(?[^\W\d_]\S*-$", form) and " " not in form:
                    kind = "cited affix"
        out.append([where, who, kind, form, gloss])

    # A row the text layer ran two things into, a display and the prose after it, [DepP DEP2 …] →
    # monoclausal The second process..., is cut before the words that open the second. head and
    # tail change the fields of each half, and None drops that half, which the context adds back
    # whole where it belongs.
    # Examples the page sets a word over its gloss, read off the page again where the engine ran
    # them into the prose.
    import pairs
    pairs.apply(out, context, example_of)
    pairs.english(out, context, example_of)
    pairs.columns(out, context, example_of)
    pairs.display(out, context, example_of)
    pairs.table(out, context, example_of)
    pairs.tableau(out, context, example_of)
    pairs.roots(out, context, example_of)
    pairs.words(out, context, example_of)
    pairs.wordlist(out, context, example_of)
    # The rows read off the page again keep the footnote marks the page set on them, ‘Maybe.’8,
    # EXCM19 or ♪ súwle ke teteʔ eʔsnúk̓ʷeʔ ♪11. The digit of a footnote the paper has comes off
    # a word or a closing mark, and off a translation only at its end. A gloss label of one letter
    # and a digit, =D3 or =V2 in Kwak̕wala, keeps it.
    for one in out:
        if not re.search(r"\bpairs\b", one[4]) or one[2] not in (
                "transcription", "segmentation", "gloss", "translation"):
            continue
        if one[2] in ("transcription", "segmentation") and (
                digit_letters or re.search(r"\d[^\W\d_]|[^\W\d_]\d[=\-]", one[3])):
            continue
        pattern = r"(?<=[’”])(\d{1,2})$" if one[2] == "translation" else \
            r"(?:(?<=[^\W\d_]{2})|(?<=[A-Z]>)|(?<=[?.!♪’”)\]]))(\d{1,2})(?=[\s,;]|$)"
        marks = [found for found in re.finditer(pattern, one[3]) if found.group(1) in footnotes]
        for marker in reversed(marks):
            one[3] = one[3][:marker.start()] + one[3][marker.end():]
        if marks:
            one[4] = "%s, carries footnote %s, written here without its digit" % (
                one[4], " and ".join(found.group(1) for found in marks))
    # A comparative paper whose examples open on a code from its language key, (1) a. Lo čənúkʷ,
    # b. Up čanúkʷ: the code names the language of the sub-example's form and gloss rows and is no
    # part of the form, and the source the key gives the code is whose the translation is. A
    # sub-example with no code of its own, (7a) ii., keeps the language of the one before it.
    codes = getattr(context, "CODES", {})
    if codes:
        pattern = re.compile(r"^((?:[ivx]+\.\s+)?)(%s)\s+(\S.*)$" % "|".join(
            re.escape(code) for code in sorted(codes, key=len, reverse=True)))
        language, source, last_example = None, "", None
        for one in out:
            example = example_of(one[0])
            if not example:
                continue
            number = re.sub(r"[a-z]?\)$", "", example) if re.search(r"\(\d", example) else example
            if number != last_example:
                language, source, last_example = None, "", number
            coded = pattern.match(one[3])
            if language and not coded and re.match(r"^[ivx]+\.\s+\S", one[3]):
                # The next item of a sub-example the code opened, (7a) ii. s-√čə́n-mis-n.
                one[2] = "segmentation" if re.search(r"[-=√•{\[]", one[3]) else "transcription"
                one[1] = language
            elif coded:
                language, source = codes[coded.group(2)]
                one[3] = coded.group(1) + coded.group(3)
                one[2] = "segmentation" if re.search(r"[-=√•{\[]", one[3]) else "transcription"
                one[1] = language
            elif language and one[3].startswith(("‘", "[‘", "“")):
                one[2] = "translation"
                if source:
                    one[1] = source
                    one[4] = "%s, cited %s" % (one[4], source)
            elif language and one[2] not in ("translation", "citation", "note", "speaker comment"):
                one[2] = "gloss"
                one[1] = language
    # Rows a pattern names, where, kind and form each a regex, as the words of a story the engine
    # offered one by one where a row holds the whole passage.
    for where_rule, kind_rule, form_rule in getattr(context, "REMOVE_IF", ()):
        out = [one for one in out if not (re.search(where_rule, one[0]) and re.search(kind_rule, one[2])
                                          and re.search(form_rule, one[3]))]
    # Rows moved to another where by pattern, the notes of a references section the engine did not
    # know by its heading.
    for where_rule, kind_rule, new_where in getattr(context, "REWHERE", ()):
        for one in out:
            if re.search(where_rule, one[0]) and re.search(kind_rule, one[2]):
                one[0] = new_where
    # A run of rows moved to another where, from the row holding the first form to the row holding
    # the last: the cells of a table the engine offered as the section's cited forms.
    for where, first, last, new_where in getattr(context, "RANGE_WHERE", ()):
        start = next((at for at, one in enumerate(out) if one[0] == where and one[3] == first), None)
        end = next((at for at, one in enumerate(out) if start is not None and at >= start
                    and one[0] == where and one[3] == last), None)
        if start is None or end is None:
            raise SystemExit("RANGE_WHERE found no run %s from %s to %s" % (where, first, last))
        for one in out[start:end + 1]:
            if one[0] == where:
                one[0] = new_where
    # The language of the examples a where pattern names, given to their language rows: a paper
    # comparing languages, each example headed or introduced by the language it is from.
    for where_rule, language in getattr(context, "LANG_WHERE", ()):
        for one in out:
            if re.search(where_rule, one[0]) and one[2] in (
                    "transcription", "segmentation", "gloss", "phonemic", "practical orthography", "phonetic", "underlying", "surface",
                    "cited form", "cited affix"):
                one[1] = language
    # A who that is the initials of the speakers and the tag of the session, BP/KBG | sf | 09.29.2022,
    # written out as the speakers' names, the tag kept in the gloss.
    initials = getattr(context, "INITIALS", {})
    for one in out:
        tagged = re.match(r"^([A-Z]{2,3}(?:/[A-Z]{2,3})*)(\s*\|.*)?$", one[1])
        if tagged and all(part in initials for part in tagged.group(1).split("/")):
            names = [initials[part] for part in tagged.group(1).split("/")]
            one[4] = "%s, %s" % (one[4], one[1]) if one[4] else one[1]
            one[1] = names[0] if len(names) == 1 else ", ".join(names[:-1]) + " and " + names[-1]
    # A conversation opens each turn on its speaker's initials, (1) KBG: xwúy̓ nke ʔiƛ̓ʔiƛ̓tis; the
    # initials are parted from the line as a citation row of that speaker.
    if getattr(context, "TURNS", False):
        turned = []
        for one in out:
            turn = re.match(r"^([A-Z]{2,3}):\s+(\S.*)$", one[3])
            if turn and turn.group(1) in initials and one[2] == "transcription" and example_of(one[0]):
                page = re.search(r"page \d+", one[4])
                turned.append([one[0], initials[turn.group(1)], "citation", turn.group(1) + ":",
                               "%sthe initials of the turn's speaker" % (page.group(0) + ", " if page else "")])
                one[3] = turn.group(2)
            turned.append(one)
        out[:] = turned
    # A translation or comment printed with its source in parentheses at its right, ‘The man is
    # strong.’ (Delphine Derickson Armstrong), parted into the row and a citation row; the speakers
    # the sources of an example name by their full names are whose its translations and comments are.
    speakers = getattr(context, "SOURCE_SPEAKERS", ())
    if speakers:
        parted = []
        for one in out:
            sourced = re.match(r"^(.*?[’”.?!]\d{0,2})\s*(\((?:[^()]|\([^()]*\))*\))\s*$", one[3]) \
                if one[2] in ("translation", "speaker comment") and example_of(one[0]) else None
            parted.append(one)
            if sourced and any(name in sourced.group(2) for name in speakers):
                one[3] = sourced.group(1)
                # The footnote mark before the source, ‘This is a heavy box.’11 (Delphine ...).
                mark = re.search(r"(?<=[’”.])(\d{1,2})$", one[3])
                if mark and mark.group(1) in footnotes:
                    one[3] = one[3][:mark.start()]
                    one[4] = "%s, carries footnote %s, written here without its digit" % (one[4], mark.group(1))
                page = re.search(r"page \d+", one[4])
                parted.append([one[0], context.AUTHORS, "citation", sourced.group(2),
                               "%sthe source at the right of the %s" % (page.group(0) + ", " if page else "",
                                                                       one[2])])
        out[:] = parted
        named = {}
        for one in out:
            if one[2] == "citation" and example_of(one[0]) and example_of(one[0]) not in named:
                # A tag can name them by initials, (BP; volunteered), where the paper gives them.
                places = {name: one[3].index(name) for name in speakers if name in one[3]}
                for short, name in initials.items():
                    at = re.search(r"(?<![A-Za-z])%s(?![A-Za-z])" % re.escape(short), one[3])
                    if at and name not in places:
                        places[name] = at.start()
                names = sorted(places, key=places.get)
                if names:
                    named[example_of(one[0])] = names[0] if len(names) == 1 else \
                        ", ".join(names[:-1]) + " and " + names[-1]
        for one in out:
            if one[2] in ("translation", "speaker comment") and one[1] == context.AUTHORS \
                    and example_of(one[0]) in named:
                one[1] = named[example_of(one[0])]
    # An example whose source tag names its speakers by initials, (vf | RI | 4.4.2024) or
    # (vf | JoF 2019-02-14), has its translation and the comments on it from them. The tag stands in
    # a citation row, or in the translation or comment the engine did not part it from.
    if getattr(context, "TAG_WHO", False):
        speakers = {}
        for one in out:
            example = re.match(r"^(\([^)]+\))", one[0])
            # (20230320-VB VF) puts the session's date first.
            # A story's code, (RP.2010) or (WSa.784), opens on its teller's initials.
            tag = re.search(r"\((?:[a-z ,]+\|\s*|\d{8}-)?([A-Z][a-z]?[A-Z]{1,2}[a-z]?(?:/[A-Z][a-z]?[A-Z]{1,2})*)"
                            r"(?:\s*[|),]|\s+\d|\.\d|\s+[A-Z]{2}\))", one[3]) \
                if example and one[2] in ("citation", "translation", "speaker comment") else None
            # A tag in square brackets names its speakers before the code, [BP, KBG|SF].
            tag = tag or (re.search(r"\[([A-Z]{2,3}(?:,\s*[A-Z]{2,3})*)\|", one[3])
                          if example and one[2] in ("citation", "speaker comment") else None)
            parts = re.split(r"/|,\s*", tag.group(1)) if tag else []
            if tag and all(part in initials for part in parts) and example.group(1) not in speakers:
                names = [initials[part] for part in parts]
                speakers[example.group(1)] = names[0] if len(names) == 1 else \
                    ", ".join(names[:-1]) + " and " + names[-1]
        for one in out:
            example = re.match(r"^(\([^)]+\))", one[0])
            if example and example.group(1) in speakers and one[2] in ("translation", "speaker comment") \
                    and (one[1] in (context.AUTHORS, "sf", "vf", "Consultant", "Consultant’s")
                         or re.match(r"^[A-Z][A-Za-z]{1,3}[. ]\d", one[1])):
                one[1] = speakers[example.group(1)]
    # Sub-examples a single tag or source closes, (28a) to (28e) and (DL.10.22) under the last: a
    # translation still the authors' takes the one who its siblings give theirs.
    if getattr(context, "SIBLINGS", False):
        def set_of(where):
            within = re.match(r"^((?:§\S+ |footnote \S+ )?\(\d+)[a-z]\)", where)
            return within.group(1) if within else None
        whose = {}
        for one in out:
            if set_of(one[0]) and one[2] == "translation" and one[1] != context.AUTHORS:
                whose.setdefault(set_of(one[0]), set()).add(one[1])
        for one in out:
            given = whose.get(set_of(one[0]) or "", set())
            if one[2] in ("translation", "speaker comment") and one[1] == context.AUTHORS and len(given) == 1:
                one[1] = next(iter(given))
                one[4] = "%s, the tag or source closing the set" % one[4]
    # The forms of an example the engine left as prose, given the language its head names, (10)
    # Squamish:, until a paragraph of prose opens.
    if getattr(context, "LANG_HEADS", False):
        current = None
        for one in out:
            head = re.match(r"^(?:\(\d+\) )?([A-Z][^:]{2,40}):$", one[3]) if one[2] == "note" else None
            if head:
                current = head.group(1)
            elif one[2] in ("note", "heading") and len(one[3]) > 80 and re.match(r"^[A-Z]", one[3]):
                current = None
            elif current and one[0].startswith("§") and one[2] in ("cited form", "cited affix"):
                one[1] = current
    # A note of the prose that follows an example row straight on is that example's next line: the
    # English of an interlinear text whose free translation the engine left in the section.
    for where_rule, new_kind in getattr(context, "ABSORB", ()):
        for at in range(1, len(out)):
            one, before = out[at], out[at - 1]
            line = re.match(r"^(\(\S+\)) line (\d+)$", before[0])
            if line and one[2] == "note" and re.search(where_rule, one[0]):
                one[0] = "%s line %d" % (line.group(1), int(line.group(2)) + 1)
                one[2] = new_kind
    # A kind given by pattern after the moves above, the phonemic cells of a table so moved, and a
    # who with it where the rule names one.
    for rule in getattr(context, "REKIND", ()):
        where_rule, kind_rule, form_rule, new_kind = rule[:4]
        for one in out:
            if re.search(where_rule, one[0]) and re.search(kind_rule, one[2]) and re.search(form_rule, one[3]):
                one[2] = new_kind
                if len(rule) > 4 and rule[4]:
                    one[1] = rule[4]
    for where, marker, head, tail in getattr(context, "SPLIT", ()):
        for index, one in enumerate(out):
            if one[0] == where and marker in one[3]:
                cut = one[3].index(marker)
                halves = []
                for text, change in ((one[3][:cut].strip(), head), (one[3][cut:].strip(), tail)):
                    if change is None or not text:
                        continue
                    half = list(one)
                    half[3] = text
                    for field, value in change.items():
                        half[("where", "who", "kind", "form", "gloss").index(field)] = value
                    halves.append(half)
                out[index:index + 1] = halves
                break
        else:
            raise SystemExit("SPLIT found no row %s holding %s" % (where, marker))

    # Context a person adds: the rows the reconstruction cannot know about, placed after the row
    # whose where they name, or at the end. REMOVE drops the rows a SET joined into the row above,
    # the lines a paraphrase ran over.
    # A form ending in ... names a long row by its opening words, as in ADD.
    remove = set(getattr(context, "REMOVE", ()))
    opening = [(where, form[:-3]) for where, form in remove if form.endswith("...")]
    out = [one for one in out if (one[0], one[3]) not in remove and
           not any(one[0] == where and one[3].startswith(start) for where, start in opening)]
    # Rows a person read off the page again whole, the cells of a table set one to a line.
    remove_where = getattr(context, "REMOVE_WHERE", None)
    if remove_where:
        out = [one for one in out if not re.search(remove_where, one[0])]
    for after, row in getattr(context, "ADD", ()):
        at = len(out)
        # after is a where, or a (where, form) pair for one row among several of that where. A
        # form ending in ... names the row by its opening words.
        if after is not None:
            for index, one in enumerate(out):
                if one[0] == after or (isinstance(after, tuple) and (
                        (one[0], one[3]) == after or (after[1].endswith("...") and one[0] == after[0]
                                                      and one[3].startswith(after[1][:-3])))):
                    at = index + 1
        out.insert(at, list(row))
    # A form ending in ... names a long row by its opening words here too.
    # A paragraph the engine broke, a reference list cut at a year that opens a line, (2006), joined
    # again: the second row's form goes on after the first's, and the second row goes.
    def named(one, where, form):
        return one[0] == where and (one[3] == form or (form.endswith("...") and one[3].startswith(form[:-3])))

    # The engine can take a year opening the line for an example number; between, the text it took,
    # goes back between the two.
    for first_where, first_form, second_where, second_form, between in getattr(context, "JOIN", ()):
        first = next((one for one in out if named(one, first_where, first_form)), None)
        second = next((one for one in out if named(one, second_where, second_form)), None)
        if first is None or second is None:
            raise SystemExit("JOIN found no row %s %s or %s %s" % (first_where, first_form, second_where, second_form))
        joined = " ".join(one for one in (first[3], between) if one)
        first[3] = joined + ("" if second[3][:1] in ",;." else " ") + second[3]
        out.remove(second)
    for where_form, change in getattr(context, "SET", {}).items():
        for one in out:
            same = one[3] == where_form[1] or \
                (where_form[1].endswith("...") and one[3].startswith(where_form[1][:-3]))
            if same and where_form[0] in (None, one[0]):
                for field, value in change.items():
                    one[("where", "who", "kind", "form", "gloss").index(field)] = value

    target = os.path.join(ORACLES, stem + ".oracle.tsv")
    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write("where\twho\tkind\tform\tgloss\n")
        for one in out:
            handle.write("\t".join(" ".join(str(field).split()) for field in one) + "\n")
    print("%d rows to %s" % (len(out), target))


if __name__ == "__main__":
    main()
