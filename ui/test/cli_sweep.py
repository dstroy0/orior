#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Sweeps every command and option of impero, as `imp help` lists them, on a window of the
# sweep's own (isolated.py): the released window in C on a tree and a home of the sweep's own, so a
# command that acts in the window acts in that one, on the display the sweep is given, where it is seen
# as each command lands. Each command is run bare, with each of its words filled and each option given,
# and with each choice its words offer on its own; a word to fill is given a value of the sweep's tree.
# The help's own forms, the completions for each shell, and the job lists and a job shown on the top's
# own tree, which only read, are run besides.
#
# Each form that acts in the window is given its settle and the window's frame is pictured into the
# run's shots/, then Escape closes what the form left asking. A window that ends under a form is
# recorded as crashed by it and started again on its home and tree, and the sweep goes on.
#
# A form that would reach another machine, a container or the bug tracker is not run, and the report
# names it, as is one that opens another window. Each form's record keeps its words, its exit code,
# how long it took, the ends of what it said and its picture; the report flags those that crashed
# first, then those that did not end in their time, then those that ended with an error, each with
# what it said.
#
#   Usage:  python -I ui/test/cli_sweep.py [--display \\.\DISPLAY1] [--wait 60] [--settle 1] [--only <text>]
#
# Each run is kept in the top's build/ui_test/cli_sweep/<time>/: cli_sweep.md, cli_sweep.json, window.log,
# shots/, and the window's home and tree.

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils" / "test"))
import harness_window  # noqa: E402
import isolated  # noqa: E402

HERE = Path(__file__).resolve().parent
TOP = HERE.parents[1]
CLI = TOP / "build" / "cli" / harness_window.host_target() / ("imp.exe" if os.name == "nt" else "imp")

# The forms after which the window is put back on the sweep's tree, as they open another
OPENERS = ("open-folder", "open-repository", "open-workspace", "create-repository", "add-folder", "environment")

# The value each word to fill is given, by its name; a word not named here is given VALUE
FILLED = {
    "file": "main.py", "path": "main.py", "folder": "sub", "archive": "sub/none.zip", "name": "sweep_name", "text": "greet",
    "url": "file:///nonexistent/sweep.git", "tool": "sweep-none", "container": "sweep-none", "commit": "HEAD", "branch": "main",
    "line": "3", "percent": "50", "n": "1", "count": "1", "times": "2", "key": "x", "keys": "Ctrl+Alt+Shift+F12", "job": "sweep",
    "ext,...": "swp", "a,b": "one,two", "name,...": "sweep", "word,...": "one", "q,q": "\",'", "group": "sweep_group",
    "variable": "count", "comment": "1", "stash": "0", "words": "one", "test": "test_main.py", "table": "people", "save": "1",
    "run.qry": "none.qry", "pattern": "*.py", "part": "sm_86", "pane": "explorer", "macro": "sweep", "item": "branch",
    "function": "greet", "from": "query", "to": "learn", "title": "sweep", "taking": "1", "step": "1", "slot": "0", "shell": "bash",
    "set": "sweep", "schema": "main", "ruleset": "none.krs", "plugin": "python", "open": "/*", "close": "*/", "mode": "sweep",
    "listing.sass": "none.sass", "language": "python", "kind": "error", "id": "sweep", "icon": "file", "exception": "ValueError",
    "arch": "sm_86", "cu": "none.cu", "command": "save", "connection": "sweep", "message": "sweep", "condition": "True",
    "color": "#336699", "font": "monospace", "address": "0", "bytes": "16", "frame": "0", "host:port": "127.0.0.1:9",
    "megabytes": "1", "encoding": "utf-8", "other commit": "HEAD", "other": "HEAD", "mark": "S", "subject": "sweep",
    "template": "sweep", "kept": "count", "hits": "1", "word": "sweep",
}
VALUE = "sweep"

# The forms not run, by the word that marks them, and why
BLOCKED = {
    "open-remote": "it reaches another machine",
    "profile-remote": "it reaches another machine",
    "attach-address": "it reaches another machine",
    "report": "it files a report on the bug tracker",
    "dev-container": "it starts a container",
    "clone": "it fetches a repository over the network",
    "container-run": "it runs a command in a container",
    "container-start": "it starts a container",
    "container-stop": "it stops a container",
    "container-restart": "it restarts a container",
    "container-terminal": "it opens a container's terminal",
    "container-open": "it opens a container",
    "new-window": "it opens another window",
    "open-folder-window": "it opens another window",
    "tab-new-window": "it opens another window",
    "file-open-window": "it opens another window",
    "exit": "it closes the sweep's window",
}


def usages():
    """Each usage of the help: its words, the words that fill in after them, and whether it acts in the window."""
    told = subprocess.run([str(CLI), "help"], capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
    joined = []
    for line in told.splitlines():
        if line.startswith("  imp ") or line.startswith("  orior "):
            joined.append(line.strip())
        elif line.startswith("    ") and joined and not line.strip().startswith("*") and not re.match(r"^\s{20,}", line):
            joined[-1] += " " + line.strip()
    out = []
    for line in joined:
        spec = re.split(r"\s{2,}", line, maxsplit=1)[0]
        window = "* " in line[len(spec) :][:60]
        # a usage of alternatives each its own command, as `a | b <x>`, after the menu and the command where it names them
        alternatives = split_top(" ".join(spec.split()[1:]), "|", spaced=True)
        prefix = []
        for at, alternative in enumerate(alternatives):
            words = alternative.split() if at == 0 else prefix + alternative.split()
            head = []
            while words and re.match(r"^[a-z][a-z0-9-]*$", words[0]):
                head.append(words.pop(0))
            prefix = head[:2] if (at == 0 and len(head) >= 2) else prefix
            out.append({"head": head, "spec": " ".join(words), "window": window})
    return out


def tokens(spec):
    """The spec's words at its top, a bracket's words kept together."""
    out, depth, word = [], 0, ""
    for character in spec:
        depth += character in "[<"
        depth -= character in "]>"
        if character == " " and depth == 0:
            if word:
                out.append(word)
            word = ""
        else:
            word += character
    return out + ([word] if word else [])


def filled(token, choice=0):
    """A token as the words it gives: a word to fill its value, a choice its `choice`th, an option bracket its words."""
    token = token[:-3] if token.endswith("...") and len(token) > 3 else token
    if token.startswith("<") and token.endswith(">"):
        inner = token[1:-1]
        if "|" in inner:
            picked = inner.split("|")[choice % len(inner.split("|"))]
            return [FILLED.get(picked, picked)]
        return [FILLED.get(inner, VALUE)]
    if token.startswith("[") and token.endswith("]"):
        inner = token[1:-1]
        parts = split_top(inner, "|")
        # a bracket whose choices are of its last word alone, as [--leave all|none], keeps its words before it
        if len(parts) > 1 and len(tokens(parts[0])) > 1 and all(len(tokens(part)) == 1 for part in parts[1:]):
            lead = tokens(parts[0])
            last = [lead[-1]] + parts[1:]
            return [word for one in lead[:-1] for word in filled(one)] + filled(last[choice % len(last)])
        picked = parts[choice % len(parts)]
        # a bracket of one word that names what fills it, as [folder], is given that word's value
        if picked in FILLED:
            return [FILLED[picked]]
        if picked.startswith("path[:line"):
            return ["main.py:3:5"]
        return [word for one in tokens(picked) for word in filled(one)]
    if "|" in token:
        return [token.split("|")[choice % len(token.split("|"))]]
    if token in ("...", "<file>..."):
        return []
    return [token.rstrip(".")]


def split_top(text, mark, spaced=False):
    """`text` parted at each `mark` outside its brackets, or, where `spaced`, at each with a space on either side."""
    out, depth, part = [], 0, ""
    for at, character in enumerate(text):
        depth += character in "[<"
        depth -= character in "]>"
        apart = not spaced or (text[at - 1 : at] == " " and text[at + 1 : at + 2] == " ")
        if character == mark and depth == 0 and apart:
            out.append(part.strip())
            part = ""
        else:
            part += character
    return [one for one in out + [part.strip()] if one]


def forms(usage):
    """The forms a usage is run in: bare, each word filled, and each choice of each word on its own. orior bare opens
    a window of its own and is not run; a file given alone is given with a line and a column besides."""
    if not usage["head"]:
        return [["main.py"], ["main.py:3"], ["main.py:3:5"]] if usage["spec"].startswith("<file>") else []
    parts = tokens(usage["spec"])
    out = [list(usage["head"])]
    full = [word for part in parts for word in filled(part)]
    out.append(usage["head"] + full)
    for at, part in enumerate(parts):
        choices = len(split_top(part[1:-1], "|") if part.startswith(("[", "<")) else part.split("|"))
        for choice in range(1, choices):
            out.append(usage["head"] + [word for other, one in enumerate(parts) for word in filled(one, choice if other == at else 0)])
    seen, unique = set(), []
    for one in out:
        if tuple(one) not in seen:
            seen.add(tuple(one))
            unique.append(one)
    return unique


class CliSweeper:
    def __init__(self, options):
        self.options = options
        self.stamp = time.strftime("%Y%m%d-%H%M%S")
        self.out = TOP / "build" / "ui_test" / "cli_sweep" / self.stamp
        self.out.mkdir(parents=True)
        self.shots = self.out / "shots"
        self.shots.mkdir()
        self.window = isolated.Window(self.out, options.display)
        self.records = []

    def tree_kept(self, words):
        """The window put back on the sweep's tree after a form that opens another, as the forms after it reach the
        window by its tree: the form's opener, or None where it opened none."""
        opener = next((word for word in OPENERS if word in words), None)
        if opener is None or not self.window.alive():
            return None
        subprocess.run([str(CLI), "file", "open-folder", str(self.window.tree)], cwd=self.window.tree, env=self.window.env, capture_output=True, timeout=self.options.wait, stdin=subprocess.DEVNULL)
        time.sleep(self.options.settle)
        return opener

    def pictured(self, index, record):
        """A form that acts in the window given its settle, the frame pictured, and what it left asking closed."""
        time.sleep(self.options.settle)
        path = self.shots / ("%04d.png" % index)
        if self.window.shot(path):
            record["shot"] = str(path.relative_to(self.out))
        self.window.keys("Escape")

    def ran(self, words):
        """One form run: its record."""
        record = {"words": words}
        blocked = next((why for word, why in BLOCKED.items() if word in words), None)
        if blocked:
            record["blocked"] = blocked
            return record
        began = time.time()
        try:
            done = subprocess.run([str(CLI)] + words, cwd=self.window.tree, env=self.window.env, capture_output=True, timeout=self.options.wait, stdin=subprocess.DEVNULL)
            record["code"] = done.returncode
            record["out"] = done.stdout.decode("utf-8", "replace")[-600:]
            record["err"] = done.stderr.decode("utf-8", "replace")[-600:]
        except subprocess.TimeoutExpired as late:
            record["code"] = None
            record["late"] = True
            record["err"] = (late.stderr or b"").decode("utf-8", "replace")[-600:]
        record["ms"] = round((time.time() - began) * 1000)
        return record

    def run(self):
        try:
            every = [(["help"], False)] + [(["--completions", shell], False) for shell in ("bash", "zsh", "fish", "powershell")]
            every += [(["--root", str(TOP), "run", "list"], False), (["--root", str(TOP), "list"], False)]
            for usage in usages():
                every += [(one, usage["window"]) for one in forms(usage)]
            if self.options.only:
                every = [one for one in every if self.options.only in " ".join(one[0])]
            print(f"{len(every)} forms; the window's tree is {self.window.tree}")
            for index, (words, acts) in enumerate(every, 1):
                record = self.ran(words)
                if acts and not record.get("blocked"):
                    self.pictured(index, record)
                record["tree"] = self.tree_kept(words)
                if not self.window.alive():
                    # the window ended under the form: recorded as its crash, and started again for the forms after it
                    record["window_ended"] = self.window.restarted()
                self.records.append(record)
                state = record.get("blocked") or ("did not end" if record.get("late") else f"exit {record['code']}")
                state += f", the window ended {record['window_ended']}" if "window_ended" in record else ""
                print(f"{index:4d}/{len(every)} {' '.join(words)[:70]:70s} {state}", flush=True)
        finally:
            self.write()
            self.window.close()

    def write(self):
        (self.out / "cli_sweep.json").write_text(json.dumps(self.records, indent=1), encoding="utf-8")
        crashed = [one for one in self.records if (isinstance(one.get("code"), int) and (one["code"] < 0 or one["code"] >= 0xC0000000)) or "window_ended" in one]
        late = [one for one in self.records if one.get("late")]
        failed = [one for one in self.records if isinstance(one.get("code"), int) and 0 < one["code"] < 0xC0000000]
        blocked = [one for one in self.records if one.get("blocked")]
        lines = [f"# The command line's sweep, {self.stamp}", ""]
        lines += [f"{len(self.records)} forms: {len(crashed)} crashed, {len(late)} did not end in {self.options.wait:.0f} s, {len(failed)} ended with an error, {len(blocked)} not run.", ""]

        def said(one):
            return (one.get("err") or one.get("out") or "").strip().splitlines()[-1:] or [""]

        for title, records, detail in (
            ("Crashed", crashed, lambda one: (f"the window ended with {one['window_ended']}" if "window_ended" in one else f"exit {one['code']:#x}") + f": {said(one)[0][:300]}"),
            ("Did not end", late, lambda one: said(one)[0][:300]),
            ("Ended with an error", failed, lambda one: f"exit {one['code']}: {said(one)[0][:300]}"),
            ("Not run", blocked, lambda one: one["blocked"]),
        ):
            lines += [f"## {title}", ""] + [f"- `imp {' '.join(one['words'])}`: {detail(one)}" for one in records] + [""]
        (self.out / "cli_sweep.md").write_text("\n".join(lines), encoding="utf-8")
        print(f"the report is {self.out / 'cli_sweep.md'}")


def main():
    # each line said as it is printed, a run's output read as it goes where it is kept in a file
    sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
    parser = argparse.ArgumentParser(description="Sweep every command and option of impero on a window of the sweep's own.")
    parser.add_argument("--display", default="\\\\.\\DISPLAY1", help="the display the window is put on")
    parser.add_argument("--wait", type=float, default=60.0, help="seconds a form is given to end")
    parser.add_argument("--settle", type=float, default=1.0, help="seconds a form that acts in the window is given before its picture")
    parser.add_argument("--only", help="run only the forms whose words hold this text")
    CliSweeper(parser.parse_args()).run()


if __name__ == "__main__":
    main()
