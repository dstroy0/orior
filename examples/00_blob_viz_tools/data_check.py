"""Does the page's script read a key that its own data does not carry?

    python examples/00_blob_viz_tools/data_check.py build/view/sha_clock_view.html
    python examples/00_blob_viz_tools/data_check.py --check

THE DEFECT THIS EXISTS FOR

Four builders share `room_view_template.html`, and only `build_sha_clock_view.py` puts a `clock` key in the JSON that it
writes. Nothing else in the tree says which builder's data satisfies which template's reads. A builder can therefore
drop a key that the template needs, and no gate notices. Every gate here asks whether the code runs, and none asks
whether the data is there.

THE CLOCK READS HAVE GUARDS

Every one of the eight reads of `DATA.clock` in that template has a guard: two `if (DATA.clock)` blocks, four ternaries
with defaults, one short circuit, and the assignment `var CLOCK = DATA.clock || null;`. `DATA.sources` has `|| 2`. The
template therefore supports a page with no clock deliberately. `CLOCK` becomes `null` instead of `undefined`,
`panelReady` returns false, and the panels are correctly absent instead of dead. Such a page can be the wrong page to
serve, and it is still not a broken one.

A claim about a read must come from the whole line. A pattern that stops at the first space shows `CLOCK = DATA.clock`,
and hides the `|| null` after it. The quantity that such a pattern reports is then not the quantity intended.

No page in the tree has an unguarded absent key. That is a useful fact about the templates, and not a reason to skip the
gate. The gate errors the next builder, and the next template read that has no default.

WHAT THIS ASKS INSTEAD

Take the built page, which carries both halves in one file. Pull the JSON literal out of `var DATA = {...}`, collect
every `DATA.<path>` that the script reads, and walk each path into the parsed data.

A path is no longer a data path as soon as it leaves the dictionaries. `DATA.sources.length` is a `sources` key followed
by a JavaScript property. The walk descends while the value is a mapping, and stops when it is not. Anything past that
point is the language's business.

GUARDED AND UNGUARDED, BECAUSE ONLY ONE OF THEM IS A DEFECT

A template legitimately asks whether an optional key is present. `if (DATA.things)` is a question, and an absent
`things` is the answer. `CLOCK = DATA.clock` is not a question.

The report therefore lists an absent key either way, and **fails** only when nothing guards it. A guard on a parent
covers its children. A page that asks whether `clock` is there owns what it does underneath `clock`. That spares this
tool a brace count to find the block that encloses the read.

WHAT `--check` GRADES

Eight guard shapes, one at a time. A mark of guarded on every one of them would pass anything. The prefix rule, both
ways: a guard on the parent covers the child, and covers nothing else. The brace counter, against a literal with braces
inside its strings. The walk, which must leave the dictionaries at `sources.length` and stay inside them at
`clock.hold`.

Then a written sample with one unguarded absent key, because the tree has none. A checker that never fires is a checker
that nobody tested. Then both real pages, which must come back clean for their two different reasons.
"""

import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

# The assignment that the builders write. This file names it once. A change to the builders is then a change here, and
# never a silent pass on a page whose data this tool could not find.
OPENER = "var DATA = "

# A read of the data, with as many dotted segments as the source defines. The walk below decides how
# many of them are data and how many are the language's.
READ = re.compile(r"\bDATA((?:\.[A-Za-z_][A-Za-z0-9_]*)+)")

# A read that sits in the test of a conditional, on this line.
TESTED = re.compile(r"\s*(\}\s*)?(else\s+)?(if|while)\s*\(")

# A read with a negation or a type test immediately before it.
DENIED = re.compile(r"(!|typeof\s+)$")

# A read that is an operand of a short circuit or a ternary, on either side of it. `DATA.x || 2`
# carries its own default and `DATA.x ? a : b` asks the question outright.
LEADS = re.compile(r"^\s*(&&|\|\||\?)")
FOLLOWS = re.compile(r"(\?|&&|\|\|)\s*$")

# A proximity test is useless here. It looks for any of `&&`, `||`, `?`, `if (` or `typeof` within 120 characters either
# side, and JavaScript carries those everywhere. `var CLOCK = DATA.clock;` comes back guarded when an unrelated `if`
# sits thirty characters earlier. A guard must be a syntactic relationship with this read, and never a token that sits
# nearby.


def strip_strings(text, start=0):
    """The text from `start` with the contents of JSON strings blanked. Only structure is visible.

    A brace inside a string value must not count, and JSON has exactly one string delimiter.

    A blank on any of the quote, the apostrophe and the backtick is right for JavaScript source. It is wrong for a JSON
    literal, because an apostrophe there is an ordinary character inside a value.

    A scan from the top of the file is worse. An apostrophe in a comment above the data opens a string that never
    closes. The blank then runs to the end of the page, and takes the data's own braces with it. The depth counter never
    leaves zero, and the tool reports `DATA DOES NOT PARSE AS JSON` for a page whose data is perfectly well formed. The
    page step_view.html carries 25 apostrophes in its data, and voxel_view.html carries one, and one is enough.

    The scan therefore begins at the literal, and admits one delimiter. A tool that blames its input for its own defect
    sends the next person to fix a file that is fine.
    """
    out = []
    inside = False
    escaped = False
    for one in text[start:]:
        if inside:
            out.append(" " if one != "\n" else "\n")
            if escaped:
                escaped = False
            elif one == "\\":
                escaped = True
            elif one == '"':
                inside = False
                out[-1] = one
        else:
            out.append(one)
            if one == '"':
                inside = True
    return "".join(out)


def data_literal(text):
    """The JSON that the page carries, as a string, or None with a reason.

    The function counts braces on a copy with blanks for the string contents, and takes the slice from the original. The
    returned text is the real literal, and a brace in a value does not fool the count.
    """
    at = text.find(OPENER)
    if at < 0:
        return None, "no '%s' in the page" % OPENER.strip()
    start = at + len(OPENER)
    if start >= len(text) or text[start] != "{":
        return None, "the data does not open with a brace"
    # Blanked from the literal onward, and indexed from there. Nothing above the data can reach
    # into the count.
    blanked = " " * start + strip_strings(text, start)

    depth = 0
    for here in range(start, len(blanked)):
        if blanked[here] == "{":
            depth += 1
        elif blanked[here] == "}":
            depth -= 1
            if depth == 0:
                return text[start:here + 1], ""
    return None, "the data's braces never close"


def guarded_at(text, start, end):
    """Whether this one read is a question about the key, and not an assumption that it is there.

    The function judges the line that holds the read, and the position of the read within it. The relationship is
    syntactic. Four shapes count, and each is a way to write "if this is absent, carry on":
      - `if (DATA.x)`: the test of a conditional.
      - `!DATA.x`: negated, or passed to typeof.
      - `DATA.x || 2`: given a default, or at the head of a short circuit or a ternary.
      - `cond ? DATA.x : 2`: reached only when something already asked.
    """
    line_start = text.rfind("\n", 0, start) + 1
    line_end = text.find("\n", end)
    if line_end < 0:
        line_end = len(text)
    before = text[line_start:start]
    after = text[end:line_end]

    if TESTED.match(text[line_start:line_end]) and "(" in before:
        return True
    if DENIED.search(before):
        return True
    if LEADS.match(after):
        return True
    if FOLLOWS.search(before):
        return True
    return False


def reads_of(text):
    """Every dotted read of the data, as {path tuple: [(offset, guarded)]}.

    The assignment itself is not a read, because `var DATA = ` has no dotted segment.
    """
    out = {}
    for found in READ.finditer(text):
        path = tuple(found.group(1).lstrip(".").split("."))
        out.setdefault(path, []).append((found.start(), guarded_at(text, found.start(), found.end())))
    return out


def protected(path, found):
    """Whether a guard on the path itself, or on anything that it hangs from, covers the path.

    A line test cannot see the block that encloses a read. The template writes:

        if (DATA.clock) {
          opBox.max = String(DATA.clock.ticks);

    The inner read there carries no guard of its own, and needs none. The rule replaces a brace count: a guarded read of
    `clock` covers `clock.ticks`. Once the page demonstrably asks whether a key is there, what it does underneath that
    key is its own business.

    This is the deliberate looseness in the tool. It cannot catch a page that tests for a key and then reads a child of
    it outside the test. That shape is rare enough to trade for a tool that never fails a correct builder.
    """
    for depth in range(len(path), 0, -1):
        for _, guard in found.get(path[:depth], []):
            if guard:
                return True
    return False


def walk(data, path):
    """How far `path` gets into `data`, and whether it got there.

    Descends while the value is a mapping. The first segment that is not a key of a mapping ends the walk. If the value
    at that depth is a mapping, the key is absent. If not, the segments after it are JavaScript property access, and the
    data satisfies the path.
    """
    here = data
    for depth, segment in enumerate(path):
        if not isinstance(here, dict):
            return True, depth, None
        if segment not in here:
            return False, depth, sorted(here.keys())
        here = here[segment]
    return True, len(path), None


def check(path):
    with io.open(path, encoding="utf-8", newline="") as handle:
        text = handle.read()

    lines = ["  %s" % os.path.basename(path)]
    failed = 0

    literal, why = data_literal(text)
    if literal is None:
        lines.append("  NO DATA FOUND: %s" % why)
        sys.stdout.write("\n".join(lines) + "\n\n1 check(s) failed\n")
        return 1
    try:
        data = json.loads(literal)
    except ValueError as broken:
        lines.append("  DATA DOES NOT PARSE AS JSON: %s" % broken)
        sys.stdout.write("\n".join(lines) + "\n\n1 check(s) failed\n")
        return 1

    lines.append("  data carries %d top level key(s), %d bytes" % (len(data), len(literal)))

    found = reads_of(text)
    lines.append("  script reads %d distinct path(s)" % len(found))

    missing = []
    for path_bits in sorted(found):
        ok, depth, near = walk(data, path_bits)
        if not ok:
            missing.append((path_bits, depth, near, protected(path_bits, found)))

    for path_bits, depth, near, covered in missing:
        defined = "DATA." + ".".join(path_bits)
        absent = ".".join(path_bits[:depth + 1])
        if covered:
            lines.append("  optional %s: absent, and the page asks before reading it" % defined)
        else:
            lines.append("  MISSING %s: no '%s' in the data, and nothing guards the read"
                         % (defined, absent))
            lines.append("    whatever depends on it renders empty, and nothing throws")
            if near:
                lines.append("    the data has: %s" % ", ".join(near[:12]))
            failed += 1

    if not missing:
        lines.append("  every path the script reads is carried by the data")

    sys.stdout.write("\n".join(lines) + "\n\n")
    sys.stdout.write("%d check(s) failed\n" % failed)
    return failed


# The real pages that the gate grades this tool on. Both must come back clean, for different reasons. The clock page
# carries every key that it reads. The room page lacks four of them, and asks before it reads every one. A finding on
# either is a defect in this file.
KNOWN_CLEAN = ("sha_clock_view.html", "sha_room_view.html")

# The known positive, written here because the tree does not contain one. Every template read of an absent key has a
# guard. That is a fact about the templates, and not a reason to ship a checker that never fires. The shape below is
# what the gate exists to error: a key that the data lacks, read with no question about it.
KNOWN_BROKEN = "\n".join((
    'var DATA = {"shell": "sphere", "clock": {"ticks": 512}};',
    'var CLOCK = DATA.clock;',
    'var TICKS = DATA.clock.ticks;',
    'var SPIN = DATA.spin;',
    'if (DATA.things) { paint(DATA.things.count); }',
))

# `spin` is absent and unguarded. One finding. `things` is absent and asked about. None. The
# clock and its ticks are present. One finding is therefore the expected answer entire.
KNOWN_BROKEN_FINDINGS = 1


def _check():
    lines = []
    failed = 0

    # The brace counter, against a literal whose values hold braces. A page's data is mostly strings, and an early stop
    # would truncate the JSON and report every key as absent.
    tricky = 'var DATA = {"a": "} not the end {", "b": {"c": 1}}\nvar OTHER = 2;'
    literal, why = data_literal(tricky)
    lines.append("  a brace inside a string does not end the scan: %s"
                 % ("held" if literal and json.loads(literal).get("b") == {"c": 1} else "FAILED " + why))
    if not literal or json.loads(literal).get("b") != {"c": 1}:
        failed += 1

    # The apostrophe, in both places that it appears. A delimiter rule on it loses the data's braces to it. The page
    # step_view.html has 25 in its values.
    quoted = "// the page's own comment\nvar DATA = {\"who\": \"Douglas's\", \"n\": {\"k\": 2}}\nvar X = 1;"
    literal, why = data_literal(quoted)
    got = json.loads(literal) if literal else {}
    lines.append("  an apostrophe in a value and in a comment above it: %s"
                 % ("held" if got.get("who") == "Douglas's" and got.get("n") == {"k": 2}
                    else "FAILED " + why))
    if got.get("who") != "Douglas's" or got.get("n") != {"k": 2}:
        failed += 1

    # The walk must stop when it leaves the dictionaries. Otherwise the tool reports every `.length` in the source as an
    # absent key, and the real findings arrive buried.
    sample = {"sources": [1, 2, 3], "clock": {"tau": 0.5}}
    ok, _, _ = walk(sample, ("sources", "length"))
    lines.append("  a property past the data reads as satisfied: %s" % ("yes" if ok else "NO"))
    if not ok:
        failed += 1
    ok, depth, near = walk(sample, ("clock", "hold"))
    lines.append("  a key absent inside a carried mapping is caught: %s at depth %d"
                 % ("yes" if not ok else "NO", depth))
    if ok or depth != 1:
        failed += 1

    # Each guard shape has to read as a guard, and a bare assignment has to not. A 120 character
    # window calls every one of them guarded.
    shapes = (
        ("if (DATA.a) { go(); }", ("a",), True),
        ("var x = !DATA.b;", ("b",), True),
        ("var x = DATA.c || 2;", ("c",), True),
        ("var x = ready ? DATA.d : 0;", ("d",), True),
        ("var x = typeof DATA.e;", ("e",), True),
        ("var CLOCK = DATA.f;", ("f",), False),
        ("paint(DATA.g);", ("g",), False),
        ("var n = DATA.h + 1;", ("h",), False),
    )
    wrong = []
    for source, path, wanted in shapes:
        got = reads_of(source).get(path, [(0, None)])[0][1]
        if got != wanted:
            wrong.append("%s read as %s" % (source, "guarded" if got else "unguarded"))
    lines.append("  8 guard shapes, each read for its syntax: %d wrong" % len(wrong))
    for one in wrong:
        lines.append("    FAIL %s" % one)
    failed += len(wrong)

    # The prefix rule. It covers a read inside `if (DATA.clock) {` without a brace count. A guard on the parent must
    # cover the child, and a guard on nothing must cover nothing.
    found = reads_of("if (DATA.clock) { go(DATA.clock.ticks); }\nvar S = DATA.spin;")
    covered = protected(("clock", "ticks"), found)
    bare = protected(("spin",), found)
    lines.append("  a guard on the parent covers the child: %s, and covers nothing else: %s"
                 % ("yes" if covered else "NO", "yes" if not bare else "NO"))
    if not covered or bare:
        failed += 1

    lines.append("")

    # The known positive, written above because the tree has none. A checker that never fires is a checker that nobody
    # tested.
    made = os.path.join(ROOT, "build", "view", "_data_check_sample.html")
    os.makedirs(os.path.dirname(made), exist_ok=True)
    with io.open(made, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(KNOWN_BROKEN)
    lines.append("  the written sample, expecting %d finding(s)" % KNOWN_BROKEN_FINDINGS)
    sys.stdout.write("\n".join(lines) + "\n")
    lines = []
    got = check(made)
    os.unlink(made)
    if got != KNOWN_BROKEN_FINDINGS:
        lines.append("    FAIL got %d, so the tool does not catch what it exists for" % got)
        failed += 1

    # The real pages, both of which have to come back clean.
    for name in KNOWN_CLEAN:
        where = os.path.join(ROOT, "build", "view", name)
        if not os.path.exists(where):
            lines.append("  %s is not built, so the tool was not graded on it" % name)
            failed += 1
            continue
        lines.append("  grading against %s, expecting 0" % name)
        sys.stdout.write("\n".join(lines) + "\n")
        lines = []
        got = check(where)
        if got:
            lines.append("    FAIL got %d on a page that should pass" % got)
            failed += 1

    sys.stdout.write("\n".join(lines) + "\n")
    sys.stdout.write("%d gate check(s) failed\n" % failed)
    return failed


if __name__ == "__main__":
    argv = sys.argv[1:]
    if "--check" in argv:
        sys.exit(1 if _check() else 0)
    if argv:
        sys.exit(1 if check(argv[0]) else 0)
    sys.stdout.write(__doc__)
    sys.exit(2)
