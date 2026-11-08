# Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""A word renamed across the tree's own sources: in identifiers and in comments, never in string literals.

    harness.py source rename WORD NEW [--plural NEWS] [--write] [--folders ...]

Each C, CUDA, C++, Python and JavaScript file under the folders that carries the tree's copyright line is cut into
code, comment and string pieces. In code, an identifier part that is the word or its plural becomes the new word in
the same case: kind_of becomes type_of, KIND_X becomes TYPE_X, scrollKind becomes scrollType. In a comment and a
Python docstring, the word becomes the new word, with its case kept. A string literal keeps its text, but the code in
an f-string's braces and in a template literal's `${}` is code. A JavaScript property name, after a dot or before a
colon in an object, keeps its text, since it names a key that JSON carries.

Without --write, the command writes nothing: it lists each name the rename makes that the tree has already, each a
collision to settle first, and counts the changes by file. With --write, it writes the files, and does not write
where a collision stands.
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
    """Each whole word `word` or `plural` in `text` as `new` or `news`, its case kept."""

    def swap(match):
        found = match.group(0)
        return cased(found, news if found.lower() == plural else new)

    return re.sub(r"\b(?i:%s|%s)\b" % (re.escape(plural), re.escape(word)), swap, text)


def identifier_renamed(name, word, new, plural, news):
    """The identifier `name` with each part that is the word or its plural renamed, its case kept."""
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
            # a regular expression literal, kept as a string
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
            match = re.match(r"(?i)(rb|br|fr|rf|r|b|f|u)?('''|\"\"\"|'|\")", text[at:at + 5])
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
    """The identifiers of a run of code renamed; a JavaScript property name kept.

    In JavaScript, `brackets` is the stack of the brackets open before the run, and the run updates it. A name after
    `.` is a property and keeps its text. A name first in a pair of an object, after `{` or `,` with a `{` innermost,
    is a key: before `:` it keeps its text, and as a shorthand before `,` or `}` it becomes `key: renamed`, so the key
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
    """An f-string's or a template literal's code, inside its braces, renamed; its text kept."""
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

    In C and JavaScript, a region runs from the end of the region before it to the brace that closes a block at the
    top level, and so holds a function with its parameters, or a struct with its members. In Python, a region runs
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


def cmd_source_rename(a):
    word = a.word.lower()
    plural = (a.word + "s").lower()
    new = a.new
    news = a.plural or (a.new + "s")
    files = [name for name in tracked(a.folders) if name not in a.exclude]
    # every identifier the tree has already, by language, to find the names a rename makes that stand already
    standing = {"c": set(), "python": set(), "script": set()}  # noqa: F841
    texts = {}
    for name in files:
        path = os.path.join(ROOT, name)
        with open(path, encoding="utf-8", errors="surrogateescape", newline="") as handle:
            text = handle.read()
        language = language_of(name)
        texts[name] = (text, language)
        for piece, body in pieces(text, language):
            if piece == "code":
                standing[language].update(IDENTIFIER.findall(body))
    changed = {}
    made = {"c": set(), "python": set(), "script": set()}
    collisions = []
    for name, (text, language) in texts.items():
        if not owned(text):
            continue
        names_made = set()
        renamed = file_renamed(text, language, word, new, plural, news, names_made)
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
    report = os.path.join(HARNESS_BUILD, "rename_%s.txt" % word)
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


def register(source):
    """The rename command added to the source group's subparsers."""
    p = source.add_parser("rename", help="a word renamed in the tree's own identifiers and comments, never in strings")
    p.add_argument("word", help="the word renamed, singular")
    p.add_argument("new", help="the word it becomes, singular")
    p.add_argument("--plural", default="", help="the new word's plural, the new word and s where none")
    p.add_argument("--folders", nargs="+", default=["ui", "src", "utils", "examples", "evidence"],
                   help="the folders read, from the top of the tree")
    p.add_argument("--exclude", nargs="+", default=[], help="the files excluded, from the top of the tree")
    p.add_argument("--write", action="store_true", help="the files written; else the collisions and counts alone")
    p.add_argument("--force", action="store_true", help="the files written where collisions stand, each checked")
    p.set_defaults(fn=cmd_source_rename)
