r"""Context written as one-line ops, the form oracle.py takes on stdin.

A paper's ops live in ops/<stem>.ops, one to a line, and load_context in finish.py lays them over
the closed corpus's finish/<stem>.py when there is one, or over a blank context when there is none. Fields part at a | with space on both sides.
A field holding a | of its own writes it \|, as a tag (sf \| EP.2021/07/10) does.
A who of @a is the authors and @l the paper's language.

  meta authors|lang|title|byline|volume|whose|letters|notes TEXT
  form FORM | kind | who | gloss        a cited form candidate's kind, who and gloss
  drop FORM                             a candidate that is no form
  set WHERE | FORM | field=value; ...   fields of the row at WHERE holding FORM; a WHERE of * is any
  split WHERE | MARKER | head | tail    head and tail are field=value; ... pairs, - drops that half
  add AWHERE | AFORM | where | who | kind | form | gloss
                                        a row after the row AWHERE, AFORM (AFORM ending ... is its
                                        opening words, and an empty AFORM names the where alone)
  join WHERE | FORM | WHERE2 | FORM2 [| BETWEEN]  the second row's form run on after the first's,
                                        BETWEEN set between them, the second row gone, before SET;
                                        FORM ending ... names a row by its opening words
  remove WHERE | FORM                   FORM ending ... names the row by its opening words
  removewhere REGEX
  removeif WHERE_RE | KIND_RE | FORM_RE rows all three match, before SPLIT; the words of a story
                                        the engine offered one by one
  rewhere WHERE_RE | KIND_RE | WHERE    rows moved to WHERE, before SPLIT
  rangewhere WHERE | FIRST | LAST | NEW the run of WHERE rows from the one holding FIRST to the one
                                        holding LAST moved to NEW, before SPLIT
  rekind WHERE_RE | KIND_RE | FORM_RE | KIND [| WHO]  rows given KIND (and WHO) after the moves
  absorb WHERE_RE | KIND                 a note straight after an example row made its next line, of KIND
  initials XX | NAME                    a speaker's initials, for speaker comments and tagged whos
  pairs N N ...                         examples set a word over its gloss, read off the page again
  lines N N ...                         examples set one tier a line, read off the page again
  stacks N N ...                        examples set word by word, each word over its phonemic form and
                                        its gloss, a line each, read off the page again; for pairs,
                                        lines and stacks, N:K is the Kth (N) where the paper printed
                                        a number twice
  twotiers                              the examples pairs and lines read are set in two tiers, the words
                                        over their gloss, a long one wrapping to words and gloss again
  columns KIND, KIND | N N ...          examples set side by side, a. b. c. across a line and each tier
                                        in columns under them, the tiers above the translation of KINDs
  table KIND[@WHO], ... | N N ...      examples set as a table, a header and a row a line, each form cell
                                        of its column's KIND, the gloss and the source tag rows of their own;
                                        a KIND of rule takes each line whole
  tableau N N ...                       examples set as a tableau, the input, constraints, weights and
                                        candidates each printed line a note, from the head to the blank
                                        line after the last candidate
  roots N N ...                         examples listing roots and glosses, a. √pəq, paq ‘white’ (as in ...),
                                        or roots set against each other, a. √səq vs. √səq̓ over ‘split’ ‘crack’
  words N N ...                         examples listing words, a. dénxisa ‘drag net along shore’ a line, the
                                        head an affix and its gloss or a caption, a definition in brackets
                                        a variant definition row
  wordlist CAPTION                      a table of words and glosses, a word a line under a header of
                                        column names, named by its caption, Table A1; rows Table A1 line N;
                                        wordlist CAPTION | plain for a table whose glosses have no quotes,
                                        /č̓əχ/ cook, the word the line's first and the gloss the rest
  tabletag TAG | GLOSS               what a source tag of a table row, [YP], stands for
  display KIND | N N:K ...              examples set apart on a line or two, a ranking or a constraint
                                        of KIND, or a picture (KIND picture), the prose the engine
                                        read as their lines given back to its paragraph; N:K is the
                                        Kth (N) where the paper printed a number twice
  code CODE | LANGUAGE [| SOURCE]     an example form opening on CODE is LANGUAGE's, its translation SOURCE's
  english N N ...                     English examples, a. and b. on one line or several, read off the page again
  siblings                              a sub-example with no tag of its own takes the one who its siblings share
  tagwho                                an example tagged with known initials gives its translation and comments to them
  turns                                 a conversation's lines opening on known initials, KBG: ..., the initials
                                        parted as a citation row of that speaker
  speakers NAME | NAME ...              speakers the sources of the examples name in full; a translation or
                                        comment with its source in parentheses at its right, ‘…’ (NAME),
                                        parted into a citation row, and an example's translations and
                                        comments given to the speakers its sources name
  lang WHERE_RE | LANGUAGE              the language rows of the examples WHERE_RE names given LANGUAGE
  langheads                             forms of an example left as prose given the language of its head
  replace OLD | NEW                     text replaced in every form
  who OLD | NEW                         a who the engine gave, written out
  whorule WHERE_RE | KIND | FORM_RE | who | gloss
  kindrule WHERE_RE | KIND_RE | FORM_RE | kind | who | gloss
  tier SCRIPT                           the letters only the phonemic line of an example writes
  # anything                            a comment
"""
import os
import re
import types

HERE = os.path.dirname(os.path.abspath(__file__))
from workdir import PRIVATE  # noqa: E402
FIELDS = ("where", "who", "kind", "form", "gloss")
META = {"authors": "AUTHORS", "lang": "LANG", "title": "TITLE", "byline": "BYLINE",
        "volume": "VOLUME", "whose": "WHOSE", "letters": "LETTERS", "notes": "PAGE_NOTES"}


def path_of(stem):
    return os.path.join(PRIVATE, "ops", stem + ".ops")


def blank(stem):
    module = types.ModuleType("context_" + re.sub(r"\W", "_", stem))
    module.STEM = stem
    module.AUTHORS = module.LANG = module.TITLE = module.BYLINE = module.VOLUME = ""
    module.WHOSE = module.LETTERS = module.PAGE_NOTES = ""
    module.FORMS = {}
    module.DROP = ()
    return module


def changes(text, who):
    if text.strip() == "-":
        return None
    out = {}
    for pair in filter(None, (one.strip() for one in text.split(";"))):
        field, _, value = pair.partition("=")
        field = field.strip()
        if field not in FIELDS:
            raise SystemExit("ops: no field %r in %r" % (field, text))
        out[field] = who(value.strip()) if field == "who" else value.strip()
    return out


def apply(module, stem):
    path = path_of(stem)
    if not os.path.exists(path):
        return module
    with open(path, encoding="utf-8") as handle:
        lines = [one.rstrip("\n") for one in handle]

    def who(text):
        return {"@a": module.AUTHORS, "@l": getattr(module, "LANG", "")}.get(text, text)

    def grow(name, value):
        setattr(module, name, tuple(getattr(module, name, ())) + (value,))

    for number, line in enumerate(lines, 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        op, _, rest = line.strip().partition(" ")
        parts = [one.strip().replace("\\|", "|") for one in re.split(r"(?<=\s)\|(?=\s|$)", rest)]
        try:
            if op == "meta":
                key, _, text = rest.partition(" ")
                setattr(module, META[key], text.strip())
            elif op == "form":
                module.FORMS = dict(getattr(module, "FORMS", {}))
                module.FORMS[parts[0]] = (parts[1], who(parts[2]), parts[3] if len(parts) > 3 else "")
            elif op == "drop":
                grow("DROP", rest.strip())
            elif op == "set":
                module.SET = dict(getattr(module, "SET", {}))
                key = (None if parts[0] == "*" else parts[0], parts[1])
                module.SET.setdefault(key, {}).update(changes(parts[2], who))
            elif op == "split":
                grow("SPLIT", (parts[0], parts[1], changes(parts[2] if len(parts) > 2 else "", who),
                               changes(parts[3] if len(parts) > 3 else "", who)))
            elif op == "add":
                after = parts[0] if not parts[1] else (parts[0], parts[1])
                grow("ADD", (after, (parts[2], who(parts[3]), parts[4], parts[5],
                                     parts[6] if len(parts) > 6 else "")))
            elif op == "join":
                grow("JOIN", (parts[0], parts[1], parts[2], parts[3], parts[4] if len(parts) > 4 else ""))
            elif op == "remove":
                module.REMOVE = set(getattr(module, "REMOVE", ())) | {(parts[0], parts[1])}
            elif op == "removeif":
                grow("REMOVE_IF", (parts[0], parts[1], parts[2] if len(parts) > 2 else ""))
            elif op == "langheads":
                module.LANG_HEADS = True
            elif op == "lang":
                grow("LANG_WHERE", (parts[0], parts[1]))
            elif op == "rekind":
                grow("REKIND", (parts[0], parts[1], parts[2], parts[3], who(parts[4]) if len(parts) > 4 else ""))
            elif op == "pairs":
                module.PAIRS = tuple(getattr(module, "PAIRS", ())) + tuple(rest.split())
            elif op == "lines":
                module.LINES = tuple(getattr(module, "LINES", ())) + tuple(rest.split())
            elif op == "stacks":
                module.STACKS = tuple(getattr(module, "STACKS", ())) + tuple(rest.split())
            elif op == "columns":
                kinds = tuple(one.strip() for one in parts[0].split(","))
                module.COLUMNS = tuple(getattr(module, "COLUMNS", ())) + \
                    tuple((kinds, number) for number in parts[1].split())
            elif op == "table":
                kinds = tuple(one.strip() for one in parts[0].split(","))
                module.TABLE = tuple(getattr(module, "TABLE", ())) + \
                    tuple((kinds, number) for number in parts[1].split())
            elif op == "tableau":
                module.TABLEAU = tuple(getattr(module, "TABLEAU", ())) + tuple(rest.split())
            elif op == "roots":
                module.ROOTS = tuple(getattr(module, "ROOTS", ())) + tuple(rest.split())
            elif op == "words":
                module.WORDS = tuple(getattr(module, "WORDS", ())) + tuple(rest.split())
            elif op == "wordlist":
                module.WORDLIST = tuple(getattr(module, "WORDLIST", ())) + \
                    ((parts[0], len(parts) > 1 and parts[1] == "plain"),)
            elif op == "tabletag":
                module.TABLE_TAGS = dict(getattr(module, "TABLE_TAGS", {}))
                module.TABLE_TAGS[parts[0]] = parts[1]
            elif op == "display":
                module.DISPLAY = tuple(getattr(module, "DISPLAY", ())) + \
                    tuple((parts[0], number) for number in parts[1].split())
            elif op == "code":
                module.CODES = dict(getattr(module, "CODES", {}))
                module.CODES[parts[0]] = (parts[1], parts[2] if len(parts) > 2 else "")
            elif op == "english":
                module.ENGLISH = tuple(getattr(module, "ENGLISH", ())) + tuple(rest.split())
            elif op == "siblings":
                module.SIBLINGS = True
            elif op == "tagwho":
                module.TAG_WHO = True
            elif op == "turns":
                module.TURNS = True
            elif op == "twotiers":
                module.TIERS = True
            elif op == "speakers":
                module.SOURCE_SPEAKERS = tuple(getattr(module, "SOURCE_SPEAKERS", ())) + tuple(parts)
            elif op == "initials":
                module.INITIALS = dict(getattr(module, "INITIALS", {}))
                module.INITIALS[parts[0]] = parts[1]
            elif op == "absorb":
                grow("ABSORB", (parts[0], parts[1]))
            elif op == "rangewhere":
                grow("RANGE_WHERE", (parts[0], parts[1], parts[2], parts[3]))
            elif op == "rewhere":
                grow("REWHERE", (parts[0], parts[1], parts[2]))
            elif op == "removewhere":
                old = getattr(module, "REMOVE_WHERE", None)
                module.REMOVE_WHERE = rest.strip() if not old else "(?:%s)|(?:%s)" % (old, rest.strip())
            elif op == "replace":
                grow("REPLACE", (parts[0], parts[1]))
            elif op == "who":
                module.WHO = dict(getattr(module, "WHO", {}))
                module.WHO[parts[0]] = who(parts[1])
            elif op == "whorule":
                grow("WHO_RULES", (parts[0], parts[1], parts[2], who(parts[3]), parts[4]))
            elif op == "kindrule":
                grow("KIND_RULES", (parts[0], parts[1], parts[2], parts[3], who(parts[4]), parts[5]))
            elif op == "tier":
                module.TIER_SCRIPT = rest.strip()
            else:
                raise SystemExit("ops: %s line %d: no op %r" % (path, number, op))
        except (IndexError, KeyError) as error:
            raise SystemExit("ops: %s line %d: %r is short a field (%s)" % (path, number, line, error))
    return module
