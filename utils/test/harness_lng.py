# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py lng: the target tables of src/lng, a language's symbols each with the transpiler word it bridges to.

    harness.py lng witness              every target table's probes run, each witness cell written
    harness.py lng witness c python     the tables of those targets alone

A target table is a file src/lng/<target>_lng.tsv with a `witness` column. Each row gives a symbol of the target's
language and the transpiler word it bridges to. The bridge word's ladder anchor, the `anchor` column of
src/lng/transpiler_lng.tsv, takes the symbol to gsm. The cases of that anchor in ladder.h are its gsm, as gsm_write
writes them: 1,1->2 for add.

A relation's probe is a template that sets `answer` from `left` and `right`. The command runs it on each case of the
anchor at the ladder's word of 32 bits. A case of one word gives `left`, and `right` is 0. A predicate answers the target's true or 0, and the target's true is what its
`==` answers for 1,1, as the ladder reads SAME. A type's probe prints its expected output.

Where every answer is the case's answer, the witness cell gets the tool and its version. Else the cell is empty. That
row is a hole, and no lift goes through it. lng_check (src/lng/lng_check.sh) reads the tables and reports each hole.

The command harness.py lng prove writes the cases of ladder.h into theory/lng/Lng/ladder_cases.lean. It writes the
rows of gnascor_asm_lng.tsv into theory/lng/Lng/matrix_rows.lean. The high order words go into
theory/lng/Lng/g_rows.lean. The anchors of the transpiler's words and the cases of each .gsm file go with them. The
proofs of .g in g_lng.lean read them. Then the command builds theory/lng with lake.

Each target has a Lean file, theory/lng/Lng/<target>_lng.lean. That file has a theorem for each witnessed relation,
with the name of its bridge word. lng_check reports each relation with none."""

import glob
import os
import platform
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
TABLES = os.path.join(ROOT, "src", "lng")
TRANSPILER = os.path.join(TABLES, "transpiler_lng.tsv")
LADDER = os.path.join(ROOT, "src", "cu", "transpiler", "lstar", "protocol", "counterexample", "ladder.h")
OUT = os.path.join(ROOT, "build", "lng", "witness")

# the text a C probe goes into: the probe is the body of main
C_FRAME = "#include <stdint.h>\n#include <stdio.h>\n\nint main(void)\n{\n    %s\n    return 0;\n}\n"

# the text a relation's template goes into for one case, in C and in Python
C_CASE = 'uint32_t left = %su; uint32_t right = %su; uint32_t answer = 0u; %s printf("%%u", (unsigned)answer);'
PYTHON_CASE = "left = %s; right = %s; answer = 0; %s; print(int(answer))"

# the ladder anchors whose answer is a predicate, true or false
PREDICATES = {"same", "less", "at_max", "more", "at_least", "apart", "zero"}


def ladder():
    """Each ladder anchor's text and its cases, each case its words and its answer, read from ladder.h."""
    with open(LADDER, encoding="utf-8") as handle:
        text = handle.read()
    names = dict(re.findall(r'anchor_\((\w+), "(\w+)"', text))
    cases = {}
    for name, words, answer in re.findall(r"\{LADDER_(\w+), \{([^}]*)\}, \d+u, (0x[0-9a-fA-F]+|\d+)u", text):
        values = [int(word.strip().rstrip("u"), 0) for word in words.split(",") if word.strip()]
        cases.setdefault(names[name], []).append((values, int(answer, 0)))
    return cases


def anchors():
    """Each transpiler word with a ladder anchor, and its anchor's text."""
    with open(TRANSPILER, encoding="utf-8") as handle:
        lines = [line.split("\t") for line in handle.read().split("\n") if line != ""]
    head = lines[0]
    word, anchor, target = head.index("word"), head.index("anchor"), head.index("target")
    return {cells[word]: cells[anchor] for cells in lines[1:] if cells[anchor] != "" and cells[target] == ""}


def tables():
    """Each target table's name and path, in the order of the names."""
    found = []
    for path in sorted(glob.glob(os.path.join(TABLES, "*_lng.tsv"))):
        with open(path, encoding="utf-8") as handle:
            head = handle.readline().rstrip("\n").split("\t")
        if "witness" in head:
            found.append((os.path.basename(path)[: -len("_lng.tsv")], path))
    return found


def c_tool():
    """The C compiler's witness text, or None where cc does not run."""
    try:
        version = subprocess.run(["cc", "-dumpfullversion"], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    return "cc %s -std=c11 -O0" % version.strip()


def c_output(probe, folder, name):
    """What the C probe prints, or None where it does not compile or does not exit 0."""
    source = os.path.join(folder, name + ".c")
    binary = os.path.join(folder, name + ".exe")
    with open(source, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(C_FRAME % probe)
    built = subprocess.run(["cc", "-std=c11", "-O0", "-o", binary, source], capture_output=True, text=True)
    if built.returncode != 0:
        print("  %s: %s" % (name, built.stderr.strip().splitlines()[0] if built.stderr.strip() else "no build"))
        return None
    ran = subprocess.run([binary], capture_output=True, text=True)
    return ran.stdout.strip() if ran.returncode == 0 else None


def python_tool():
    """The Python witness text."""
    return "python %s" % platform.python_version()


def python_output(probe, folder, name):
    """What the Python probe prints, or None where it does not exit 0."""
    source = os.path.join(folder, name + ".py")
    with open(source, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(probe + "\n")
    ran = subprocess.run([sys.executable, "-I", source], capture_output=True, text=True)
    return ran.stdout.strip() if ran.returncode == 0 else None


# each target's tool: the witness text, and the run of one probe
RUNNERS = {"c": (c_tool, c_output), "python": (python_tool, python_output)}


def case_body(name, template, cases):
    """The probe body that runs `template` on each of `cases`, one answer a line. A case of one word puts 0 in
    `right`."""
    pairs = [(words[0], words[1] if len(words) > 1 else 0) for words, _ in cases]
    if name == "c":
        return " ".join("{ " + C_CASE % (left, right, template) + ' printf("\\n"); }' for left, right in pairs)
    return "\n".join(PYTHON_CASE % (left, right, template) for left, right in pairs)


def relation_answers(name, output, folder, number, template, cases):
    """What the template answers on each case, or None where the probe does not run."""
    got = output(case_body(name, template, cases), folder, "row_%d" % number)
    return None if got is None else got.split()


def witness(name, path):
    """The probes of one table run, its witness cells written. 0 where the command writes the table, and 2 where the
    host has no tool for it."""
    tool, output = RUNNERS[name]
    text = tool()
    if text is None:
        print("lng witness: %s has no tool on this host, and its witnesses stay as written" % name)
        return 2
    cases = ladder()
    anchor_of = anchors()
    with open(path, encoding="utf-8", newline="") as handle:
        lines = handle.read().split("\n")
    head = lines[0].split("\t")
    at = {column: place for place, column in enumerate(head)}
    folder = os.path.join(OUT, name)
    os.makedirs(folder, exist_ok=True)
    rows = [(number, line.split("\t")) for number, line in enumerate(lines[1:], start=1) if line != ""]
    # the target's true: what its symbol for eq answers on the first case of same, 1,1
    truth = None
    for number, cells in rows:
        if (cells[at["transpiler"]] == "eq") and (cells[at["probe"]] != ""):
            got = relation_answers(name, output, folder, number, cells[at["probe"]], cases["same"][:1])
            truth = got[0] if got else None
    witnessed = 0
    for number, cells in rows:
        probe = cells[at["probe"]]
        anchor = anchor_of.get(cells[at["transpiler"]], "")
        passed = False
        if cells[at["type"]] == "type":
            got = output(probe, folder, "row_%d" % number) if probe != "" else None
            passed = (got is not None) and (got == cells[at["expected"]])
        elif (anchor != "") and (probe != ""):
            want = [str(answer) for _, answer in cases[anchor]]
            if anchor in PREDICATES:
                want = [(truth if answer != 0 else "0") for _, answer in cases[anchor]]
            got = relation_answers(name, output, folder, number, probe, cases[anchor])
            passed = (got is not None) and (truth is not None or anchor not in PREDICATES) and (got == want)
            if (got is not None) and not passed:
                wrong = [(case, given) for case, given, wanted in zip(cases[anchor], got, want) if given != wanted]
                if wrong:
                    (words, answer), given = wrong[0]
                    print("  %s: %s answers %s for %s, and gsm's %s answers %s" % (
                        name, cells[at["word"]], given, ",".join(str(word) for word in words), anchor,
                        want[cases[anchor].index((words, answer))]))
        cells[at["witness"]] = text if passed else ""
        witnessed += 1 if passed else 0
        lines[number] = "\t".join(cells)
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write("\n".join(lines))
    print("lng witness: %s, %d rows, %d witnessed by %s" % (name, len(rows), witnessed, text))
    return 0


def cmd_witness(a):
    """The probes of each table the command names run, or of every table where it names none."""
    known = tables()
    chosen = [(name, path) for name, path in known if (not a.targets) or (name in a.targets)]
    for name in a.targets:
        if name not in {table for table, _ in known}:
            print("lng witness: no target table is named %s" % name)
            return 2
    code = 0
    for name, path in chosen:
        if name not in RUNNERS:
            print("lng witness: %s has no runner, and its witnesses stay as written" % name)
            code = 2
            continue
        code = max(code, witness(name, path))
    return code


THEORY = os.path.join(ROOT, "theory", "lng")
ASM = os.path.join(TABLES, "gnascor_asm_lng.tsv")


def lean_text(text):
    """`text` as a Lean string literal."""
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def write_ladder_cases():
    """Lng/ladder_cases.lean: every case of ladder.h as Lean data, its anchor's text, its words and its answer."""
    lines = ["/-! The cases of ladder.h, written by harness.py lng prove. Each is an anchor, its words and its answer. -/",
             "", "namespace Lng.LadderCases", "", "def cases : List (String × List Nat × Nat) := ["]
    entries = []
    for anchor, cases in ladder().items():
        for words, answer in cases:
            entries.append("  (%s, [%s], %d)" % (lean_text(anchor), ", ".join(str(word) for word in words), answer))
    lines.append(",\n".join(entries))
    lines += ["]", "", "end Lng.LadderCases", ""]
    with open(os.path.join(THEORY, "Lng", "ladder_cases.lean"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    return len(entries)


def write_matrix_rows():
    """Lng/matrix_rows.lean: every row of gnascor_asm_lng.tsv as Lean data, each cell a string."""
    with open(ASM, encoding="utf-8") as handle:
        rows = [line.split("\t") for line in handle.read().split("\n") if line != ""]
    head = rows[0]
    fields = ["mnemonic", "table", "past", "present", "past left", "past right", "present left", "present right",
              "left", "right", "arrow"]
    at = [head.index(field) if field in head else None for field in fields]
    lines = ["/-! The rows of gnascor_asm_lng.tsv, written by harness.py lng prove. Each cell is a string. -/", "",
             "namespace Lng.MatrixRows", "",
             "structure Row where",
             "  mnemonic : String", "  table : String", "  past : String", "  present : String",
             "  pastLeft : String", "  pastRight : String", "  presentLeft : String", "  presentRight : String",
             "  left : String", "  right : String", "  arrow : String", "",
             "def rows : List Row := ["]
    entries = []
    for cells in rows[1:]:
        values = [cells[place] if (place is not None and place < len(cells)) else "" for place in at]
        entries.append("  ⟨" + ", ".join(lean_text(value) for value in values) + "⟩")
    lines.append(",\n".join(entries))
    lines += ["]", "", "end Lng.MatrixRows", ""]
    with open(os.path.join(THEORY, "Lng", "matrix_rows.lean"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    return len(entries)


HOL = os.path.join(TABLES, "gnascor_hol_lng.tsv")
GSM_FILES = os.path.join(ROOT, "src", "cu", "transpiler", "lstar", "protocol", "table")


def tsv_rows(path):
    """The head and the rows of the tsv at `path`, each row a dict from the head's cells to its cells."""
    with open(path, encoding="utf-8") as handle:
        lines = [line.split("\t") for line in handle.read().split("\n") if line != ""]
    head = lines[0]
    return head, [dict(zip(head, cells + [""] * (len(head) - len(cells)))) for cells in lines[1:]]


def write_g_rows():
    """Lng/g_rows.lean: the rows of gnascor_hol_lng.tsv, each with the operators it marks. With them go the
    transpiler's own words that have an anchor, and the cases of each .gsm file of the protocol's tables."""
    head, hol = tsv_rows(HOL)
    operators = head[head.index("transpiler") + 1:]
    lines = ["/-! The high order words, the anchors of the transpiler's words and the .gsm files, written by harness.py",
             "lng prove. -/", "", "namespace Lng.GRows", "",
             "structure HolRow where", "  word : String", "  type : String", "  bits : String", "  signed : String",
             "  transpiler : String", "  symbols : List String", "", "def holRows : List HolRow := ["]
    entries = []
    for row in hol:
        marked = [operator for operator in operators if row[operator] == "1"]
        entries.append("  ⟨%s, %s, %s, %s, %s, [%s]⟩" % (lean_text(row["word"]), lean_text(row["type"]),
                                                        lean_text(row["bits"]), lean_text(row["signed"]),
                                                        lean_text(row["transpiler"]),
                                                        ", ".join(lean_text(operator) for operator in marked)))
    lines += [",\n".join(entries), "]", "", "def anchors : List (String × String) := ["]
    _, transpiler = tsv_rows(TRANSPILER)
    pairs = ["  (%s, %s)" % (lean_text(row["word"]), lean_text(row["anchor"])) for row in transpiler
             if row["target"] == "" and row["anchor"] != ""]
    lines += [",\n".join(pairs), "]", "", "def gsmFiles : List (String × List (List Nat × Nat)) := ["]
    files = []
    for path in sorted(glob.glob(os.path.join(GSM_FILES, "*.gsm"))):
        cases = []
        with open(path, encoding="utf-8") as handle:
            for line in handle.read().split("\n"):
                if "->" in line:
                    words, answer = line.split("->")
                    cases.append("([%s], %s)" % (words.replace(",", ", "), answer.strip()))
        files.append("  (%s, [%s])" % (lean_text(os.path.basename(path)), ", ".join(cases)))
    lines += [",\n".join(files), "]", "", "end Lng.GRows", ""]
    with open(os.path.join(THEORY, "Lng", "g_rows.lean"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    return len(entries), len(files)


def cmd_prove(a):
    """The ladder's cases, the matrix rows and the .g rows written as Lean data, the proofs built, and no proof left
    open."""
    cases = write_ladder_cases()
    rows = write_matrix_rows()
    words, files = write_g_rows()
    print("lng prove: %d ladder cases, %d matrix rows, %d high order words and %d .gsm files written"
          % (cases, rows, words, files))
    built = subprocess.run(["lake", "build"], cwd=THEORY)
    if built.returncode != 0:
        print("lng prove: lake build failed")
        return 1
    # lake build passes a theorem that sorry closes, and only shows a warning; this fails on it
    open_proofs = []
    for path in glob.glob(os.path.join(THEORY, "Lng", "*.lean")) + [os.path.join(THEORY, "Lng.lean")]:
        with open(path, encoding="utf-8") as handle:
            for number, line in enumerate(handle, start=1):
                if re.search(r"\bsorry\b", line):
                    open_proofs.append("%s:%d" % (os.path.relpath(path, ROOT).replace("\\", "/"), number))
    for place in open_proofs:
        print("lng prove: a proof is open at %s" % place)
    if open_proofs:
        return 1
    print("lng prove: every proof is closed")
    return 0


def register(sub):
    """The lng group's commands added to harness.py's subparsers."""
    group = sub.add_parser("lng", help="the target tables of src/lng, each symbol bridged to a transpiler word")
    lng = group.add_subparsers(dest="command", required=True)
    p = lng.add_parser("witness", help="every target table's probes run, each witness cell written")
    p.add_argument("targets", nargs="*", help="the targets whose tables run, every table where none is named")
    p.set_defaults(fn=cmd_witness)
    p = lng.add_parser("prove", help="the ladder's cases and the matrix rows written as Lean data, and the proofs of "
                       "theory/lng built with no proof open")
    p.set_defaults(fn=cmd_prove)
