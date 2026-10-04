"""Build a paper's oracle from one-line ops piped in, and print only what changed.

usage:
  python oracle.py STEM start "AUTHORS" "LANGUAGE"   the engine passes, the draft, a rerun.sh line
  printf '%s\\n' 'OP' 'OP' | python oracle.py STEM ops  keep the ops, rebuild, print changed rows
  python oracle.py STEM show [REGEX]                 oracle rows whose line matches, compact
  python oracle.py STEM draft [REGEX]                draft rows whose line matches, compact
  python oracle.py STEM build                        rebuild from the ops as they stand
  python oracle.py STEM undo [N]                     drop the last N ops (1), rebuild
  python oracle.py STEM done                         md, prose check, baseline, the log line counts

The ops are the one-line context ops.py reads, kept in ops/<stem>.ops. A rebuild runs finish.py and
residue.py and prints the rows that differ from the build before, - for a row gone and + for a row
new, then the residue verdict. stdin is read only by ops, since the shell here never closes it.
"""
import difflib
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from workdir import FINISH, ORACLES, WORK  # noqa: E402
import ops  # noqa: E402

ENV = dict(os.environ, PYTHONIOENCODING="utf-8")
sys.stdout.reconfigure(encoding="utf-8")
sys.stdin.reconfigure(encoding="utf-8")
BANNED = re.compile(r"rather|, so\b|so a |which is the|is the one|—| is what |nothing else")


def run(*command, allowed=(0,)):
    done = subprocess.run([sys.executable] + list(command), cwd=HERE, env=ENV, capture_output=True,
                          text=True, encoding="utf-8")
    if done.returncode not in allowed:
        sys.stdout.write(done.stdout[-2000:] + done.stderr[-2000:])
        raise SystemExit("%s failed" % command[0])
    return done.stdout


def compact(row, width=110):
    where, who, kind, form, gloss = (row + [""] * 5)[:5]
    return "%s | %s | %s | %s || %s" % (where, who[:16], kind, form[:width], gloss[:60])


def table(stem):
    path = os.path.join(ORACLES, stem + ".oracle.tsv")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as handle:
        return [line.rstrip("\n").split("\t") for line in handle][1:]


def rebuild(stem):
    before = [compact(one) for one in table(stem)]
    built = run("finish.py", stem).strip().split("\n")[-1]
    after = [compact(one) for one in table(stem)]
    for line in difflib.unified_diff(before, after, lineterm="", n=0):
        if line[:1] in "+-" and not line.startswith(("+++", "---")):
            print(line)
    verdict = run("residue.py", stem, allowed=(0, 1)).strip().split("\n")
    print(built.split(" to ")[0] + "; " + verdict[-1].strip())
    for line in verdict:
        if re.match(r"^\s+[1-9]\d* (?:forms|written|language)", line) or line.startswith("      "):
            print(line.rstrip())


def show(rows, pattern):
    for number, row in enumerate(rows, 1):
        line = compact(row)
        if not pattern or re.search(pattern, "\t".join(row)):
            print(number, line)


def main():
    stem, command = sys.argv[1], sys.argv[2]
    rest = sys.argv[3:]
    path = ops.path_of(stem)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if command == "start":
        authors, language = rest
        for tool in ("space_census.py", "stacked_census.py", "page_text.py"):
            run(tool, stem)
        run("paper_sift.py", stem, authors, language)
        print(run("draft_check.py", stem).strip().split("\n")[-1])
        if not os.path.exists(path):
            with open(path, "w", encoding="utf-8") as handle:
                handle.write("# start %s\nmeta authors %s\nmeta lang %s\n"
                             % (subprocess.run(["date", "+%H:%M"], capture_output=True, text=True).stdout.strip(),
                                authors, language))
        with open(os.path.join(HERE, "rerun.sh"), encoding="utf-8") as handle:
            listed = "run %s " % stem in handle.read()
        if not listed:
            with open(os.path.join(HERE, "rerun.sh"), "a", encoding="utf-8") as handle:
                handle.write('run %s "%s" "%s"\n' % (stem, authors, language))
        rebuild(stem)
    elif command == "ops":
        lines = [one.rstrip() for one in sys.stdin.read().split("\n") if one.strip()]
        with open(path, "a", encoding="utf-8") as handle:
            handle.write("".join(one + "\n" for one in lines))
        rebuild(stem)
    elif command == "undo":
        with open(path, encoding="utf-8") as handle:
            lines = handle.readlines()
        count = int(rest[0]) if rest else 1
        if count < 1:
            raise SystemExit("undo takes a count of 1 or more")
        for one in lines[-count:]:
            print("undone:", one.rstrip())
        with open(path, "w", encoding="utf-8") as handle:
            handle.writelines(lines[:-count])
        rebuild(stem)
    elif command == "build":
        rebuild(stem)
    elif command == "summary":
        # Each where once, in order, with the count of each kind in it: the shape of a build
        # without its rows.
        counts = {}
        for row in table(stem):
            where = "(examples)" if row[0].startswith("(") else re.sub(r" line \d+$", "", row[0])
            counts.setdefault(where, {}).setdefault(row[2], 0)
            counts[where][row[2]] += 1
        print("; ".join("%s %s" % (where, " ".join("%s:%d" % (kind[:5], n) for kind, n in kinds.items()))
                        for where, kinds in counts.items()))
    elif command == "show":
        show(table(stem), rest[0] if rest else "")
    elif command == "draft":
        with open(os.path.join(WORK, stem + ".draft.tsv"), encoding="utf-8") as handle:
            show([line.rstrip("\n").split("\t") for line in handle][1:], rest[0] if rest else "")
    elif command == "done":
        print(run("make_md.py", stem).strip().split("\n")[-1])
        for name in (path, os.path.join(FINISH, stem + ".py")):
            if os.path.exists(name):
                with open(name, encoding="utf-8") as handle:
                    for number, line in enumerate(handle, 1):
                        if BANNED.search(line):
                            print("prose %s:%d: %s" % (os.path.basename(name), number, line.rstrip()))
        os.makedirs(os.path.join(WORK, "baseline"), exist_ok=True)
        shutil.copy(os.path.join(ORACLES, stem + ".oracle.tsv"), os.path.join(WORK, "baseline"))
        counts = {}
        if os.path.exists(path):
            with open(path, encoding="utf-8") as handle:
                for line in handle:
                    op = line.split(" ", 1)[0].strip()
                    if op and not op.startswith("#") and op != "meta":
                        counts[op] = counts.get(op, 0) + 1
        print("rows %d; ops %s" % (len(table(stem)), ", ".join("%d %s" % (n, op) for op, n in sorted(counts.items()))))
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
