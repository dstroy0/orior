# Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""A word renamed across the tree's own sources: in identifiers and in comments, never in string literals.

    harness.py source rename WORD NEW [--plural NEWS] [--whole] [--write] [--folders ...]
    harness.py source rename --pairs FILE [--whole] [--write] [--folders ...]

With --pairs, FILE holds one rename a line, the old word and the new one apart by a tab. The command reads and cuts the
tree once for all of them.

The command reads each C, CUDA, C++, Python and JavaScript file under the folders that carries the tree's copyright
line. It cuts each into code, comment and string pieces. In code, an identifier part that is the word or its plural
becomes the new word in the same case. With WORD count and NEW total, count_of becomes total_of, and COUNT_X becomes
TOTAL_X.

In a comment and a Python docstring, the word becomes the new word, with its case retained. A string literal
retains its text, but the code in an f-string's braces and in a template literal's `${}` is code. A JavaScript property
name, after a dot or before a colon in an object, retains its text, since it names a key that JSON carries.

    harness.py source call FUNCTION NEW TEMPLATE [--write] [--folders ...]

Each call of the C function FUNCTION becomes a call of NEW, in code and never in a comment or a string. TEMPLATE makes
its arguments of the old ones: {0} is the first as written, {1} the second, and so on. A declaration or a definition
of FUNCTION retains its text. Without --write, it writes the report of each line it changes alone.

With --whole, WORD is a whole identifier. It becomes NEW only where an identifier in code, or a word in a comment or a
docstring, is WORD exactly, in its own case. With WORD g_cli_guard and NEW g_platform_guard, g_cli_guard_set retains
its text.

Without --write, the command writes nothing. It lists each name the rename makes that the tree has already, each a
collision to settle first, and counts the changes by file. With --write, it writes the files, and does not write where
a collision is.
"""

import os
import re
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HARNESS_BUILD = os.path.join(ROOT, "build", "harness")

C_LIKE = (".c", ".h", ".cu", ".cuh", ".cpp", ".hpp")
PYTHON = (".py",)
SCRIPT = (".js", ".mjs")
OWNED = ("dstroy0", "Douglas Quigg")
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
PART = re.compile(r"[A-Z]+(?![a-z])|[A-Z]?[a-z]+|[0-9]+|_+")


def cased(old, new):
    """`new` in the case of `old`: all capitals, a first capital, or lower case."""
    if old.isupper() and len(old) > 1:
        return new.upper()
    if old[:1].isupper():
        return new[:1].upper() + new[1:]
    return new


def word_renamed(text, word, new, plural, news):
    """Each whole word `word` or `plural` in `text` as `new` or `news`, its case retained. Where `plural` is None, `word`
    is a whole identifier, and only that identifier, in its own case, becomes `new`."""
    if plural is None:
        return re.sub(r"(?<![A-Za-z0-9_])%s(?![A-Za-z0-9_])" % re.escape(word), new, text)

    def swap(match):
        found = match.group(0)
        return cased(found, news if found.lower() == plural else new)

    return re.sub(r"\b(?i:%s|%s)\b" % (re.escape(plural), re.escape(word)), swap, text)


def identifier_renamed(name, word, new, plural, news):
    """The identifier `name` with each part that is the word or its plural renamed, its case retained. Where `plural` is
    None, `word` is a whole identifier, and `name` becomes `new` only where it is `word`."""
    if plural is None:
        return new if name == word else name
    parts = PART.findall(name)
    if "".join(parts) != name:
        return name
    changed = [cased(part, news if part.lower() == plural else new) if part.lower() in (word, plural) else part
               for part in parts]
    return "".join(changed)


def pieces(text, language):
    """`text` cut into (piece, text) pairs, a piece code, comment, string or docstring, in order."""
    out = []
    at = 0
    code_start = 0
    length = len(text)
    previous = ""

    def flush(end):
        if end > code_start:
            out.append(("code", text[code_start:end]))

    while at < length:
        two = text[at:at + 2]
        three = text[at:at + 3]
        char = text[at]
        if language != "python" and two == "//":
            flush(at)
            end = text.find("\n", at)
            end = length if end < 0 else end
            out.append(("comment", text[at:end]))
            at = code_start = end
            continue
        if language != "python" and two == "/*":
            flush(at)
            end = text.find("*/", at + 2)
            end = length if end < 0 else end + 2
            out.append(("comment", text[at:end]))
            at = code_start = end
            continue
        if language == "python" and char == "#":
            flush(at)
            end = text.find("\n", at)
            end = length if end < 0 else end
            out.append(("comment", text[at:end]))
            at = code_start = end
            continue
        if language == "script" and char == "/" and previous in ("", "(", ",", "=", ":", "[", "!", "&", "|", "?", "{", "}", ";"):
            # a regular expression literal, retained as a string
            flush(at)
            end = at + 1
            in_class = False
            while end < length and text[end] != "\n":
                if text[end] == "\\":
                    end += 2
                    continue
                if text[end] == "[":
                    in_class = True
                elif text[end] == "]":
                    in_class = False
                elif text[end] == "/" and not in_class:
                    break
                end += 1
            end += 1
            while end < length and text[end].isalpha():
                end += 1
            out.append(("string", text[at:end]))
            at = code_start = end
            previous = "x"
            continue
        prefix = ""
        if language == "python":
            # three quotes as a count of one quote, so that the pattern holds no run of three quotes itself
            match = re.match(r"(?i)(rb|br|fr|rf|r|b|f|u)?('{3}|\"{3}|'|\")", text[at:at + 5])
            if match and (at == 0 or not (text[at - 1].isalnum() or text[at - 1] == "_")):
                prefix = match.group(1) or ""
                quote = match.group(2)
            else:
                quote = None
        else:
            quote = char if char in ("'", '"', "`") else None
            if quote == "'" and language == "c" and at > 0 and (text[at - 1].isalnum()):
                quote = None
        if quote:
            flush(at)
            start = at
            at += len(prefix) + len(quote)
            while at < length:
                if text[at] == "\\":
                    at += 2
                    continue
                if text.startswith(quote, at):
                    at += len(quote)
                    break
                if len(quote) == 1 and quote != "`" and text[at] == "\n":
                    break
                at += 1
            body = text[start:at]
            if language == "python" and len(quote) == 3 and "f" not in prefix.lower():
                out.append(("docstring", body))
            elif (language == "python" and "f" in prefix.lower()) or quote == "`":
                out.append(("formatted", body))
            else:
                out.append(("string", body))
            code_start = at
            previous = "x"
            continue
        if not char.isspace():
            previous = char
        at += 1
    flush(length)
    return out


def code_renamed(code, word, new, plural, news, language, names_made, brackets=None):
    """The identifiers of a run of code renamed; a JavaScript property name retained.

    In JavaScript, `brackets` is the stack of the brackets open before the run, and the run updates it. A name after
    `.` is a property and retains its text. A name first in a pair of an object, after `{` or `,` with a `{` innermost,
    is a key. Before `:` it retains its text. As a shorthand before `,` or `}` it becomes `key: renamed`, and the key
    stays the same.
    """
    brackets = [] if brackets is None else brackets
    out = []
    at = 0
    for match in IDENTIFIER.finditer(code):
        for char in code[at:match.start()]:
            if char in "([{":
                brackets.append(char)
            elif char in ")]}" and brackets:
                brackets.pop()
        out.append(code[at:match.start()])
        at = match.end()
        name = match.group(0)
        renamed = identifier_renamed(name, word, new, plural, news)
        if renamed == name:
            out.append(name)
            continue
        if language == "script":
            before = code[:match.start()].rstrip()
            after = code[match.end():].lstrip()
            if before.endswith("."):
                out.append(name)
                continue
            first_in_pair = bool(brackets) and brackets[-1] == "{" and (before.endswith("{") or before.endswith(","))
            if first_in_pair and after.startswith(":") and not after.startswith("::"):
                out.append(name)
                continue
            if first_in_pair and (after.startswith(",") or after.startswith("}")):
                names_made.add((name, renamed))
                out.append("%s: %s" % (name, renamed))
                continue
        names_made.add((name, renamed))
        out.append(renamed)
    for char in code[at:]:
        if char in "([{":
            brackets.append(char)
        elif char in ")]}" and brackets:
            brackets.pop()
    out.append(code[at:])
    return "".join(out)


def formatted_renamed(body, word, new, plural, news, language, names_made):
    """An f-string's or a template literal's code, inside its braces, renamed; its text retained."""
    opener = "${" if language == "script" else "{"
    out = []
    at = 0
    while True:
        start = body.find(opener, at)
        if start < 0 or (language == "python" and body.startswith("{{", start)):
            if start >= 0:
                out.append(body[at:start + 2])
                at = start + 2
                continue
            out.append(body[at:])
            return "".join(out)
        end = body.find("}", start)
        if end < 0:
            out.append(body[at:])
            return "".join(out)
        out.append(body[at:start + len(opener)])
        out.append(code_renamed(body[start + len(opener):end], word, new, plural, news, language, names_made))
        at = end


def language_of(path):
    if path.endswith(C_LIKE):
        return "c"
    if path.endswith(PYTHON):
        return "python"
    if path.endswith(SCRIPT):
        return "script"
    return None


def owned(text):
    head = text[:600]
    return any(mark in head for mark in OWNED)


def file_renamed(text, language, word, new, plural, news, names_made):
    """The file's text with the word renamed in its code, comments and docstrings."""
    out = []
    brackets = []
    for piece, body in pieces(text, language):
        if piece == "code":
            out.append(code_renamed(body, word, new, plural, news, language, names_made, brackets))
        elif piece in ("comment", "docstring"):
            out.append(word_renamed(body, word, new, plural, news))
        elif piece == "formatted":
            out.append(formatted_renamed(body, word, new, plural, news, language, names_made))
        else:
            out.append(body)
    return "".join(out)


def bare_names(code, language):
    """The identifiers of a run of code that name a variable, a parameter, a function or a member declared.

    A name after `.` or `->` is a member access, and a name before `:` in a JavaScript object is a key. Neither is
    bare, because neither can collide with a local name.
    """
    names = set()
    for match in IDENTIFIER.finditer(code):
        before = code[:match.start()].rstrip()
        after = code[match.end():].lstrip()
        if before.endswith(".") or before.endswith("->"):
            continue
        if language == "script" and after.startswith(":") and not after.startswith("::"):
            continue
        names.add(match.group(0))
    return names


def regions(text, language):
    """The file cut into regions, each a (first line, bare names) pair.

    In C and JavaScript, a region runs from the end of the region before it to a top-level brace that closes a block.
    It holds a function with its parameters, or a struct with its members. In Python, a region runs
    from one line at the left margin to the next. The first region also holds the names of the top level.
    """
    out = []
    names = set()
    line = 1
    first = 1
    depth = 0
    for piece, body in pieces(text, language):
        if piece == "formatted":
            opener = "${" if language == "script" else "{"
            for inner in re.findall(re.escape(opener) + r"([^}]*)\}", body):
                names.update(bare_names(inner, language))
            line += body.count("\n")
        elif piece == "code":
            if language == "python":
                for row in body.split("\n"):
                    if row[:1] not in ("", " ", "\t", ")", "]", "}") and names:
                        out.append((first, names))
                        names = set()
                        first = line
                    names.update(bare_names(row, language))
                    line += 1
                line -= 1
            else:
                start = 0
                for at, char in enumerate(body):
                    if char == "{":
                        depth += 1
                    elif char == "}":
                        depth = max(0, depth - 1)
                        if depth == 0:
                            names.update(bare_names(body[start:at + 1], language))
                            line += body.count("\n", start, at + 1)
                            start = at + 1
                            out.append((first, names))
                            names = set()
                            first = line
                names.update(bare_names(body[start:], language))
                line += body.count("\n", start)
        else:
            line += body.count("\n")
    out.append((first, names))
    return out


def tracked(folders):
    listed = subprocess.run(["git", "ls-files", "-z", "--"] + list(folders), cwd=ROOT, capture_output=True).stdout
    return [name for name in listed.decode("utf-8", "replace").split("\0") if name and language_of(name)]


def renames_of(a):
    """The renames of one run, each (word, new, plural, news): every line `old<TAB>new` of --pairs, or WORD and NEW. A
    whole identifier matches in its own case and has no plural."""
    if a.pairs:
        with open(a.pairs, encoding="utf-8") as listed:
            pairs = [line.rstrip("\r\n").split("\t") for line in listed if line.strip() and not line.startswith("#")]
    else:
        pairs = [[a.word, a.new]]
    renames = []
    for old, new in pairs:
        if a.whole:
            renames.append((old, new, None, None))
        else:
            renames.append((old.lower(), new, (old + "s").lower(), a.plural or (new + "s")))
    return renames


def cmd_source_rename(a):
    if not a.pairs and not (a.word and a.new):
        print("rename: WORD and NEW, or --pairs FILE")
        return 2
    renames = renames_of(a)
    files = [name for name in tracked(a.folders) if name not in a.exclude]
    texts = {}
    for name in files:
        path = os.path.join(ROOT, name)
        with open(path, encoding="utf-8", errors="surrogateescape", newline="") as handle:
            texts[name] = (handle.read(), language_of(name))
    changed = {}
    made = {"c": set(), "python": set(), "script": set()}
    collisions = []
    for name, (text, language) in texts.items():
        if not owned(text):
            continue
        names_made = set()
        renamed = text
        for word, new, plural, news in renames:
            # the loop cuts no file that holds no form of the word
            held = renamed if plural is None else renamed.lower()
            if word in held:
                renamed = file_renamed(renamed, language, word, new, plural, news, names_made)
        if renamed == text:
            continue
        changed[name] = renamed
        made[language].update(names_made)
        # a name the rename makes that a region of the file has already, where the region also has the old name
        olds = {old for old, _ in names_made}
        for line, region in regions(text, language):
            for old, new_name in sorted(names_made):
                if (old in region) and (new_name in region) and (new_name not in olds):
                    collisions.append((language, "%s:%d: %s" % (name, line, old), new_name))
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    report = os.path.join(HARNESS_BUILD, "rename_%s.txt" % (os.path.splitext(os.path.basename(a.pairs))[0] if a.pairs else a.word))
    with open(report, "w", encoding="utf-8") as out:
        for language, old, renamed in collisions:
            out.write("collision %s %s -> %s\n" % (language, old, renamed))
        for language in sorted(made):
            for old, renamed in sorted(made[language]):
                out.write("name %s %s -> %s\n" % (language, old, renamed))
        for name in sorted(changed):
            before = texts[name][0].splitlines()
            after = changed[name].splitlines()
            count = sum(1 for one, other in zip(before, after) if one != other)
            out.write("file %s %d\n" % (name, count))
    print("%d files change, %d names made, %d collisions: %s" %
          (len(changed), sum(len(names) for names in made.values()), len(collisions), report))
    for language, old, renamed in collisions[:60]:
        print("  collision %s %s -> %s" % (language, old, renamed))
    if not a.write:
        return 0
    if collisions and not a.force:
        print("nothing written: settle the collisions, or pass --force where each is checked")
        return 1
    for name, renamed in changed.items():
        with open(os.path.join(ROOT, name), "w", encoding="utf-8", errors="surrogateescape", newline="") as handle:
            handle.write(renamed)
    print("written")
    return 0


def masked(text, language):
    """`text` with each comment and string blanked to spaces of the same length, its line ends kept. A search of the
    code then finds no word inside one, and each place in it is the same place in `text`."""
    return "".join(body if piece == "code" else re.sub(r"[^\n]", " ", body) for piece, body in pieces(text, language))


def call_arguments(hidden, opening):
    """The arguments of the call whose `(` is at `opening` in the masked text `hidden`. They are the (start, end) span
    of each, split at the commas of the call's own depth, and the place after its `)`. None where the brackets do not
    close."""
    depth = 0
    spans = []
    start = opening + 1
    for at in range(opening, len(hidden)):
        char = hidden[at]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
            if depth == 0:
                if hidden[start:at].strip() or spans:
                    spans.append((start, at))
                return spans, at + 1
        elif char == "," and depth == 1:
            spans.append((start, at))
            start = at + 1
    return None


# the words that come before a call and not before a declaration's name
CALL_KEYWORDS = ("return", "else", "case", "do", "sizeof")


def file_recalled(text, language, function, new, template):
    """The file's text with each call of `function` written as a call of `new`, its arguments recast by `template`. In
    `template`, {0}, {1} and the rest are the call's arguments as written. A declaration or a definition of `function`,
    its name after a type, retains its text. The text and the count of calls recast."""
    hidden = masked(text, language)
    out = []
    at = 0
    count = 0
    for match in re.finditer(r"(?<![A-Za-z0-9_.>])%s[ \t]*\(" % re.escape(function), hidden):
        before = re.search(r"([A-Za-z0-9_]+)\s*$", hidden[:match.start()])
        if before is not None and before.group(1) not in CALL_KEYWORDS:
            continue
        found = call_arguments(hidden, match.end() - 1)
        if found is None:
            continue
        spans, end = found
        words = [text[start:stop].strip() for start, stop in spans]
        out.append(text[at:match.start()])
        out.append("%s(%s)" % (new, template.format(*words)))
        at = end
        count += 1
    out.append(text[at:])
    return "".join(out), count


def cmd_source_call(a):
    files = [name for name in tracked(a.folders) if name not in a.exclude and language_of(name) == "c"]
    changed = {}
    report_lines = []
    for name in files:
        path = os.path.join(ROOT, name)
        with open(path, encoding="utf-8", errors="surrogateescape", newline="") as handle:
            text = handle.read()
        if not owned(text):
            continue
        try:
            recast, count = file_recalled(text, "c", a.function, a.new, a.template)
        except IndexError:
            print("  %s: a call has fewer arguments than the template names" % name)
            return 1
        if count:
            changed[name] = recast
            report_lines.append("file %s %d" % (name, count))
            before = text.splitlines()
            after = recast.splitlines()
            report_lines.extend("  - %s\n  + %s" % (one.strip(), other.strip())
                                for one, other in zip(before, after) if one != other)
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    report = os.path.join(HARNESS_BUILD, "call_%s.txt" % a.function)
    with open(report, "w", encoding="utf-8") as out:
        out.write("\n".join(report_lines) + "\n")
    calls = sum(int(line.rsplit(" ", 1)[1]) for line in report_lines if line.startswith("file "))
    print("%d calls in %d files recast: %s" % (calls, len(changed), report))
    if not a.write:
        return 0
    for name, recast in changed.items():
        with open(os.path.join(ROOT, name), "w", encoding="utf-8", errors="surrogateescape", newline="") as handle:
            handle.write(recast)
    print("written")
    return 0


def register(source):
    """The rename and call commands added to the source group's subparsers."""
    p = source.add_parser("call", help="each call of a C function written as a call of another, its arguments recast")
    p.add_argument("function", help="the function whose calls are recast")
    p.add_argument("new", help="the function that each call becomes")
    p.add_argument("template", help="the new call's arguments, {0}, {1} and the rest the old call's, as written")
    p.add_argument("--folders", nargs="+", default=["ui", "src", "utils", "examples", "includes"],
                   help="the folders read, from the top of the tree")
    p.add_argument("--exclude", nargs="+", default=[], help="the files excluded, from the top of the tree")
    p.add_argument("--write", action="store_true", help="the files written; else the report alone")
    p.set_defaults(fn=cmd_source_call)
    p = source.add_parser("rename", help="a word renamed in the tree's own identifiers and comments, never in strings")
    p.add_argument("word", nargs="?", default="", help="the word renamed, singular")
    p.add_argument("new", nargs="?", default="", help="the word it becomes, singular")
    p.add_argument("--pairs", default="", metavar="FILE",
                   help="a file of renames, one `old<TAB>new` a line, every one of them made in one pass of the tree")
    p.add_argument("--plural", default="", help="the new word's plural, the new word and s where none")
    p.add_argument("--whole", action="store_true",
                   help="the word is a whole identifier, renamed only where an identifier or a comment's word is it exactly")
    p.add_argument("--folders", nargs="+", default=["ui", "src", "utils", "examples", "evidence"],
                   help="the folders read, from the top of the tree")
    p.add_argument("--exclude", nargs="+", default=[], help="the files excluded, from the top of the tree")
    p.add_argument("--write", action="store_true", help="the files written; else the collisions and counts alone")
    p.add_argument("--force", action="store_true", help="the files written where collisions are, each checked")
    p.set_defaults(fn=cmd_source_rename)
