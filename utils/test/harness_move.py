# Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""A file or a folder of the tree moved, with each include that reaches it, or that it makes, written again.

    harness.py source move FROM TO [--write]

FROM is a tracked file or folder, and TO is its new path, both from the top of the tree. Each tracked file under FROM
goes to the same place under TO, through git mv. An include in quotes resolves from the folder of the file that holds
it, as the compiler resolves it first. That path may be a tracked file, and the file that includes or the file that
the include reaches may move. The include then gets the relative path between their new places. An include that only
a folder of -I resolves keeps its text.

The command also lists each line of a script, a table or a document that names the old path. Those lines name a path
in too many forms for a rewrite, and the caller writes each one.

Without --write, the command writes nothing: it lists the moves, counts the includes by file and lists the lines that
name the old path. With --write, it moves the files and writes the includes.
"""

import os
import re
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HARNESS_BUILD = os.path.join(ROOT, "build", "harness")

C_LIKE = (".c", ".h", ".cu", ".cuh", ".cpp", ".hpp", ".m", ".mm", ".inc", ".def", ".rc")
NAMED = (".sh", ".py", ".json", ".ps1", ".cmake", ".txt", ".toml", ".yml", ".yaml", ".md", ".tsv", ".bat")
INCLUDE = re.compile(r'^([ \t]*#[ \t]*include[ \t]*")([^"\n]+)(")', re.M)
# the trees under these folders are libraries of their own, and the command reads none of them
LIBRARIES = ("includes/mmgr/", "includes/embedded_types/")


def tracked():
    """Every tracked file of the tree, from the top, with forward slashes."""
    listed = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True).stdout
    return [name for name in listed.decode("utf-8", "replace").split("\0") if name]


def moves_of(files, source, target):
    """Each tracked file under `source` with its place under `target`. A file `source` maps to `target` itself."""
    source = source.strip("/")
    target = target.strip("/")
    if source in files:
        return {source: target}
    prefix = source + "/"
    return {name: target + "/" + name[len(prefix):] for name in files if name.startswith(prefix)}


def joined(folder, quoted):
    """The path from the top that `quoted` names from `folder`, or None where it climbs out of the tree."""
    path = os.path.normpath(os.path.join(folder, quoted)).replace("\\", "/")
    return None if path.startswith("../") or path == ".." else path


def relative(folder, path):
    """The path of `path` from `folder`, with forward slashes."""
    return os.path.relpath(path, folder or ".").replace("\\", "/")


def includes_written(name, text, files, moves):
    """The text of `name` with each include that a move reaches written again, and the count of them."""
    place = moves.get(name, name)
    count = 0

    def swap(match):
        nonlocal count
        reached = joined(os.path.dirname(name), match.group(2))
        if (reached is None) or (reached not in files):
            return match.group(0)
        if (reached not in moves) and (name not in moves):
            return match.group(0)
        quoted = relative(os.path.dirname(place), moves.get(reached, reached))
        if quoted == match.group(2):
            return match.group(0)
        count += 1
        return match.group(1) + quoted + match.group(3)

    return INCLUDE.sub(swap, text), count


def named_lines(files, moves, source):
    """Each line of a script, a table or a document that names the old path, as (file, line, text)."""
    marks = {source.strip("/")}
    parts = source.strip("/").split("/")
    if len(parts) > 1:
        marks.add("/".join(parts[-2:]))
    found = []
    for name in files:
        if not name.endswith(NAMED) or name.startswith(LIBRARIES) or name.startswith("theory/"):
            continue
        with open(os.path.join(ROOT, name), encoding="utf-8", errors="replace") as handle:
            for number, line in enumerate(handle, 1):
                if any(mark in line for mark in marks):
                    found.append((name, number, line.rstrip("\n")))
    return found


def pairs_read(path):
    """The (from, to) pairs of a file of moves, one pair a line, the two paths split by a tab."""
    with open(path, encoding="utf-8") as handle:
        return [tuple(line.rstrip("\n").split("\t")[:2]) for line in handle if "\t" in line]


def cmd_source_move(a):
    files = tracked()
    present = set(files)
    pairs = pairs_read(a.pairs) if a.pairs else [(a.source, a.target)]
    moves = {}
    for source, target in pairs:
        moves.update(moves_of(present, source, target))
    a.source = pairs[0][0]
    if not moves:
        print("nothing moves: %s is no tracked file or folder" % a.source)
        return 1
    taken = [place for place in moves.values() if place in present and place not in moves]
    if taken:
        print("nothing moves: these places hold a tracked file already")
        for place in taken[:40]:
            print("  " + place)
        return 1
    changed = {}
    counts = {}
    for name in files:
        if not name.endswith(C_LIKE) or name.startswith(LIBRARIES):
            continue
        with open(os.path.join(ROOT, name), encoding="utf-8", errors="surrogateescape", newline="") as handle:
            text = handle.read()
        written, count = includes_written(name, text, present, moves)
        if count:
            changed[name] = written
            counts[name] = count
    lines = named_lines(files, moves, a.source)
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    report = os.path.join(HARNESS_BUILD, "move_%s.txt" % a.source.strip("/").replace("/", "_"))
    with open(report, "w", encoding="utf-8") as out:
        for name in sorted(moves):
            out.write("move %s -> %s\n" % (name, moves[name]))
        for name in sorted(counts):
            out.write("include %s %d\n" % (name, counts[name]))
        for name, number, line in lines:
            out.write("named %s:%d: %s\n" % (name, number, line.strip()))
    print("%d files move, %d files get %d includes, %d lines name the old path: %s" %
          (len(moves), len(counts), sum(counts.values()), len(lines), report))
    if not a.write:
        return 0
    for name, written in changed.items():
        with open(os.path.join(ROOT, name), "w", encoding="utf-8", errors="surrogateescape", newline="") as handle:
            handle.write(written)
    for name in sorted(moves):
        os.makedirs(os.path.dirname(os.path.join(ROOT, moves[name])), exist_ok=True)
        moved = subprocess.run(["git", "mv", name, moves[name]], cwd=ROOT, capture_output=True, text=True)
        if moved.returncode != 0:
            print("git mv %s failed: %s" % (name, moved.stderr.strip()))
            return 1
    print("written")
    return 0


def register(source):
    """The move command added to the source group's subparsers."""
    p = source.add_parser("move", help="a file or folder moved, with each include that reaches it written again")
    p.add_argument("source", nargs="?", default="", help="the tracked file or folder, from the top of the tree")
    p.add_argument("target", nargs="?", default="", help="its new path, from the top of the tree")
    p.add_argument("--pairs", default="", help="a file of moves, each line a from path, a tab and a to path")
    p.add_argument("--write", action="store_true", help="the files moved and written; else the counts alone")
    p.set_defaults(fn=cmd_source_move)
