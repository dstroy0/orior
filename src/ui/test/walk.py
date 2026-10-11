#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Walks every command of orior's menus in the running window, as a press of its item does, on a
# scratch tree of dummy files of its own, and says what each did: how long it worked, each call it
# made and how long that took, each frame and the long ones, each element that moved and how far it
# went from frame to frame, each layout shift, each error, and what it left open. The screen is kept
# as the command left it; what moved by jumps, shifted, failed or stayed open is flagged in
# report.md, the worst first, beside its pictures.
#
# A command is measured with nothing else at work in the window. One that moved is pressed again
# with the screen kept frame by frame, which takes the window's time, and its frames are kept from
# that second press. With --memory, or from memory.py, each command is pressed once more, or only,
# under V8's sampling heap profiler: the bytes the page still holds after it, once it is closed and
# put back and the heap collected, by module and by function, the heap's growth, each process's, and
# the bytes each call of the tree's side took and kept, which the window counts as its allocator
# gives them. That pass writes memory.md.
#
# The window is reached through its debugging port, which the dev app opens with
# WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port=9222. monitor.js goes into the page
# and keeps every command inside the window: a call that would quit, open a window, a picker, the
# browser or the network, file a report or install a tool is answered there and never made. A
# setting that turns on and off is pressed again to turn it back, and what opens a panel is followed
# by what closes it. The reader's settings, the page's kept entries, the clipboard and the tree open
# are put back at the end, the walk stopped or not.
#
#   Usage:  python -I src/ui/test/walk.py [--only <command,...>] [--menus <title,...>] [--skip <command,...>]
#                                          [--port 9222] [--wait 8] [--frames all|moved|none]
#                                          [--memory] [--sampling 64]
#
# Each run is kept in src/ui/test/runs/<time>/: report.md, report.json, shots/ and frames/, and
# memory.md and memory.json from the memory pass.

import argparse
import base64
import collections
import json
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cdp  # noqa: E402

HERE = Path(__file__).resolve().parent
UI = HERE.parent
COMMANDS = UI / "cli" / "commands.json"

# How long the window stays still before a command is taken as done, in milliseconds.
QUIET = 450

# The answers given to the prompts some commands ask; every other prompt is closed unanswered.
ANSWERS = {
    "create-file": "walk_new.txt",
    "create-folder": "walk_folder",
    "new-branch": "walk-branch",
    "stash": "walk stash",
    "macro-keep": "walk macro",
}

# What puts the window back after a command: its own press again for a setting that turns on and
# off, and the command that closes what it opened.
AFTER = {
    "zoom-in": "zoom-reset",
    "zoom-out": "zoom-reset",
    "split-right": "unsplit",
    "split-down": "unsplit",
    "new": "kill",
    "split": "kill",
    "debug": "stop-debug",
    "debug-mixed": "stop-debug",
    "record": "stop-debug",
    "profile": "stop-profile",
    "run-tests": "stop-tests",
    "cover-tests": "stop-tests",
    "run-failed-tests": "stop-tests",
    "macro-record": "macro-record",
    "scheme": "scheme",
    "side-bar": "side-bar",
    "terminal-view": "terminal-view",
    "toggle": "toggle",
    "debug-view": "debug-view",
    "profile-view": "profile-view",
    "containers-view": "containers-view",
    "mark-file": "mark-file",
    "bookmark": "bookmark",
    "breakpoint": "breakpoint",
}

DUMMIES = {
    "main.py": '"""A module the walker opens."""\n\nimport os\n\n\ndef greet(name):\n    """Say hello."""\n    message = f"hello {name}"\n    # TODO: say it louder\n    return message\n\n\nclass Counter:\n    def __init__(self):\n        self.count = 0\n\n    def add(self, by=1):\n        self.count += by\n        return self.count\n\n\nif __name__ == "__main__":\n    print(greet(os.environ.get("USER", "walker")))\n',
    "app.js": "export function sum(values) {\n  let total = 0;\n  for (const value of values) {\n    total += value;\n  }\n  return total;\n}\n\nconsole.log(sum([1, 2, 3]));\n",
    "lib.rs": "pub fn twice(n: i32) -> i32 {\n    n * 2\n}\n\n#[cfg(test)]\nmod tests {\n    #[test]\n    fn doubles() {\n        assert_eq!(super::twice(2), 4);\n    }\n}\n",
    "README.md": "# Walk\n\nA tree the walker makes.\n\n- one\n- two\n\n```python\nprint('hi')\n```\n",
    "data.json": '{\n  "name": "walk",\n  "items": [1, 2, 3],\n  "nested": {"on": true}\n}\n',
    "table.csv": "id,name,score\n1,ada,90\n2,bob,85\n3,cy,77\n",
    "style.css": "body {\n  margin: 0;\n  color: #222;\n}\n",
    "index.html": "<!doctype html>\n<html>\n  <head><link rel=\"stylesheet\" href=\"style.css\"></head>\n  <body><h1>Walk</h1></body>\n</html>\n",
    "query.sql": "SELECT 1;\n\nSELECT name, score FROM people WHERE score > 80;\n",
    "test_main.py": "from main import greet\n\n\ndef test_greet():\n    assert greet('a') == 'hello a'\n",
    "sub/inner.txt": "a file one folder down\n",
    ".gitignore": "__pycache__/\n",
    "walk.code-workspace": '{"folders": [{"path": "."}]}\n',
    "notes.ipynb": json.dumps({"cells": [{"cell_type": "markdown", "id": "m1", "metadata": {}, "source": ["# Notes"]}, {"cell_type": "code", "execution_count": None, "id": "c1", "metadata": {}, "outputs": [], "source": ["1 + 1"]}], "metadata": {"kernelspec": {"display_name": "Python 3", "language": "python", "name": "python3"}}, "nbformat": 4, "nbformat_minor": 5}, indent=1),
}


def menu_commands(menus_wanted):
    """Each command the menus hold, in their order: its menu path, its name and its needs."""
    out = []

    def walk(items, trail):
        for item in items:
            if not isinstance(item, dict):
                continue
            if "items" in item:
                walk(item["items"], trail + [item.get("label", "")])
            elif item.get("command"):
                out.append({"path": " / ".join(trail + [item["label"].rstrip("…")]), "command": item["command"], "needs": item.get("needs", ""), "args": item.get("args", "")})

    for menu in json.loads(COMMANDS.read_text(encoding="utf-8"))["menus"]:
        if not menus_wanted or menu["title"] in menus_wanted:
            walk(menu.get("items", []), [menu["title"]])
    return out


def make_tree(folder):
    """The scratch tree: the dummy files, committed to a repository of its own, one changed since."""
    for name, text in DUMMIES.items():
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
    git = ["git", "-c", "user.name=walker", "-c", "user.email=walker@localhost", "-c", "commit.gpgsign=false"]
    subprocess.run(git + ["init", "-q", "-b", "main"], cwd=folder, check=True)
    subprocess.run(git + ["add", "-A"], cwd=folder, check=True)
    subprocess.run(git + ["commit", "-q", "-m", "first"], cwd=folder, check=True)
    with open(folder / "main.py", "a", encoding="utf-8", newline="\n") as handle:
        handle.write("\n\ndef added():\n    return 1\n")


def kept_by(profile):
    """The bytes a sampling heap profile holds, by module, the file a script came from, and by
    function in it. monitor.js names itself walk-monitor.js, and what it holds is the walker's own;
    a frame with no file is the engine's, its built-ins and its own work."""
    modules, functions = collections.Counter(), collections.Counter()
    waiting = [profile["head"]]
    while waiting:
        node = waiting.pop()
        waiting.extend(node.get("children", []))
        size = node.get("selfSize", 0)
        if not size:
            continue
        frame = node["callFrame"]
        url = frame.get("url", "")
        module = "(the engine's)" if not url else "(the walker's own)" if url.endswith("walk-monitor.js") else url.rsplit("/", 1)[-1]
        modules[module] += size
        functions[f"{module}:{frame.get('lineNumber', -1) + 1} {frame.get('functionName') or '(anonymous)'}"] += size
    return modules, functions


def orior_home():
    named = os.environ.get("ORIOR_HOME")
    if named:
        return Path(named)
    if os.name == "nt":
        return Path(os.environ["APPDATA"]) / "orior"
    return Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") / "orior"


def keep_home(home):
    """The files at the top of orior's own folder, by name, to put back."""
    kept = {}
    if home.is_dir():
        for path in home.iterdir():
            if path.is_file() and path.stat().st_size < 8 << 20:
                kept[path.name] = path.read_bytes()
    return kept


def put_home_back(home, kept):
    for path in home.iterdir() if home.is_dir() else []:
        if path.is_file() and path.name not in kept:
            path.unlink()
    for name, data in kept.items():
        path = home / name
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)


class Walker:
    def __init__(self, options):
        self.options = options
        self.conn = cdp.Connection(cdp.window_page(options.port))
        self.stamp = time.strftime("%Y%m%d-%H%M%S")
        self.out = HERE / "runs" / self.stamp
        (self.out / "shots").mkdir(parents=True)
        (self.out / "frames").mkdir()
        self.scratch = Path(tempfile.gettempdir()) / "orior-walk" / self.stamp
        self.tree = self.scratch / "tree"
        self.tree.mkdir(parents=True)
        self.frames = []
        self.records = []
        self.interval = 1000 / 60
        self.motion = not getattr(options, "memory_only", False)
        self.memory = bool(getattr(options, "memory", False))
        self.conn.on("Page.screencastFrame", self.framed)

    def js(self, expression, timeout=60):
        return self.conn.evaluate(expression, timeout=timeout)

    def watch(self):
        """Puts monitor.js into the page, and stops the walk unless its watch holds every call: a call
        held says so, where one let through would only be refused by the server as no command."""
        self.js((HERE / "monitor.js").read_text(encoding="utf-8"))
        holds = self.js(
            "(async () => { if (!(await window.__walk.guarded)) return false;"
            " try { await (await import('/bridge.js')).invoke('walker_probe'); return false; }"
            " catch (error) { return String(error).includes('held by the walker'); } })()"
        )
        if holds is not True:
            raise SystemExit("the page's calls are not held by the walker's watch: no command is pressed")

    def framed(self, params):
        """Keeps a frame of the screen, on the connection's reading thread, and asks for the next."""
        self.frames.append((params["metadata"].get("timestamp", time.time()), params["data"]))
        if len(self.frames) > 400:
            del self.frames[:200]
        self.conn.notify("Page.screencastFrameAck", sessionId=params["sessionId"])

    def baseline(self):
        """The edit view, main.py open in it, its editor holding the keys."""
        self.js(
            "(async () => { (await import('/views.js')).showView('edit'); await (await import('/edit.js')).openFile('main.py');"
            " document.querySelector('#editor .ed-input')?.focus(); return 1; })()"
        )

    def settle(self, cap):
        """Waits until the window has been still for QUIET, or `cap` seconds; says whether it was."""
        start = time.time()
        while time.time() - start < cap:
            quiet = self.js("window.__walk ? window.__walk.quiet() : -1")
            if quiet == -1:
                self.watch()
            elif quiet >= QUIET:
                return True
            time.sleep(0.08)
        return False

    def shot(self, name):
        path = self.out / "shots" / f"{name}.jpg"
        data = self.conn.call("Page.captureScreenshot", format="jpeg", quality=60)["data"]
        path.write_bytes(base64.b64decode(data))
        return path.relative_to(self.out).as_posix()

    def press(self, command, wait):
        began_wall = time.time()
        self.frames.clear()
        self.js("window.__walk.begin()")
        ran = self.js(f"window.__walk.run({json.dumps(command)}, [], {int(wait * 1000)})", timeout=wait + 30)
        if ran.get("missing"):
            return ran, None, began_wall
        settled = self.settle(wait)
        if self.js(f"window.__walk.answer({json.dumps(ANSWERS.get(command))})"):
            ran["prompt"] = ANSWERS.get(command) or "closed"
            settled = self.settle(wait) and settled
        report = self.js("window.__walk.report()")
        report["settled"] = settled
        return ran, report, began_wall

    def keep_frames(self, index, command, began_wall):
        frames = [(stamp, data) for stamp, data in self.frames if stamp >= began_wall - 0.05]
        if not frames:
            return None
        folder = self.out / "frames" / f"{index:03d}-{command}"
        folder.mkdir(exist_ok=True)
        for at, (stamp, data) in enumerate(frames[:150]):
            (folder / f"{at:03d}-{int((stamp - began_wall) * 1000):05d}ms.jpg").write_bytes(base64.b64decode(data))
        return folder.relative_to(self.out).as_posix()

    def put_back(self, item, ran):
        """Closes what a command left over the views and presses what turns the window back; gives
        what stayed open once all was closed."""
        command = item["command"]
        left = self.js("window.__walk.clear()")
        after = AFTER.get(command) or (command if item["args"].startswith("[on|off]") else None)
        if after and not ran.get("missing"):
            self.js(f"window.__walk.run({json.dumps(after)}, [], 4000)", timeout=40)
            self.settle(3)
            self.js("window.__walk.clear()")
        return left

    def film(self, index, item):
        """Presses a command again with the screen kept frame by frame, and turns the window back."""
        self.frames.clear()
        self.conn.call("Page.startScreencast", format="jpeg", quality=55, maxWidth=1280, maxHeight=800, everyNthFrame=1)
        try:
            self.baseline()
            self.settle(2)
            ran, _, began_wall = self.press(item["command"], self.options.wait)
            folder = self.keep_frames(index, item["command"], began_wall)
            self.put_back(item, ran)
            return folder
        finally:
            self.conn.call("Page.stopScreencast")

    def collect(self):
        for _ in range(2):
            self.conn.call("HeapProfiler.collectGarbage")

    def processes(self):
        """Each process of the app by its name and id, and the bytes it has committed."""
        read = self.js("window.__TAURI_INTERNALS__.invoke('memory_use').catch(() => null)")
        return {f"{part['name']} {part['pid']}": part["commit"] for part in (read or {}).get("parts", [])}

    def weigh(self, item):
        """Presses a command under the sampling heap profiler and gives what it left: the page's bytes
        still held once it was closed, put back and the heap collected, by module and function; the
        heap's growth; each process's; and what each call of the tree's side took and kept."""
        self.baseline()
        self.settle(2)
        self.collect()
        before = self.conn.call("Runtime.getHeapUsage")["usedSize"]
        processes = self.processes()
        self.js("window.__TAURI_INTERNALS__.invoke('memory_calls', { reset: true })")
        self.conn.call("HeapProfiler.startSampling", samplingInterval=self.options.sampling, includeObjectsCollectedByMajorGC=False, includeObjectsCollectedByMinorGC=False)
        try:
            ran, report, _ = self.press(item["command"], self.options.wait)
            left = self.put_back(item, ran) if report is not None else []
            self.settle(3)
        finally:
            self.collect()
            profile = self.conn.call("HeapProfiler.stopSampling")["profile"]
        after = self.conn.call("Runtime.getHeapUsage")["usedSize"]
        grown = self.processes()
        calls = self.js("window.__TAURI_INTERNALS__.invoke('memory_calls', { reset: true })") or {}
        modules, functions = kept_by(profile)
        return ran, report, left, {
            "heapBefore": before,
            "heapAfter": after,
            "kept": after - before,
            "modules": dict(modules.most_common(20)),
            "functions": dict(functions.most_common(30)),
            "processes": {name: grown.get(name, 0) - processes.get(name, 0) for name in sorted(set(processes) | set(grown))},
            "calls": calls,
        }

    def judge_memory(self, memory):
        """What a command's memory says needs a look: the page's bytes it left held, a call that kept
        what it took, and a process that grew."""
        flags = []
        if memory["kept"] > 1 << 20:
            top = ", ".join(f"{name} {size / 1024:.0f} KiB" for name, size in list(memory["functions"].items())[:3])
            flags.append((48, "kept memory", f"the page holds {memory['kept'] / 1024:.0f} KiB more once it is closed: {top}"))
        for name, calls in memory["calls"].items():
            if calls["kept"] > 1 << 20:
                flags.append((46, "call kept memory", f"{name} kept {calls['kept'] / 1024:.0f} KiB of the {calls['taken'] / 1024:.0f} KiB its {calls['count']} calls took"))
        for name, grown in memory["processes"].items():
            if grown > 32 << 20:
                flags.append((44, "process grew", f"{name} committed {grown / (1 << 20):.0f} MiB more"))
        return flags

    def judge(self, ran, report, left):
        """What a command's record says needs a look, each as (weight, kind, text)."""
        flags = []
        if ran.get("missing"):
            return [(100, "missing", "no command of the window answers this item")]
        if ran.get("threw") and "held by the walker" not in ran["threw"]:
            flags.append((90, "threw", ran["threw"].splitlines()[0][:300]))
        for error in report["errors"]:
            flags.append((85, error["kind"], error["text"].splitlines()[0][:300]))
        for call in report["calls"]:
            if not call["ok"]:
                flags.append((60, "call failed", f"{call['name']}: {call.get('error', '')[:200]}"))
        if not ran.get("ended", True):
            flags.append((55, "never ended", f"its work was still going after {self.options.wait} s"))
        elif not report.get("settled"):
            flags.append((50, "never still", f"the window kept moving for {self.options.wait} s"))
        if left:
            flags.append((45, "left open", "still open once Escape was pressed and every sheet, menu and diff closed: " + "; ".join(left)[:300]))
        if not self.motion:
            return flags
        moving = bool(report["moves"]) or bool(report["shifts"])
        long = [gap for gap in report["gaps"] if gap > 2.5 * self.interval]
        if long and moving:
            flags.append((40, "dropped frames", f"{len(long)} frames over {2.5 * self.interval:.0f} ms while it moved, the longest {max(long):.0f} ms"))
        for loaf in report.get("loafs") or []:
            if loaf["blocking"] > 100:
                scripts = ", ".join(f"{one['fn'] or one['invoker']} {one['at']} {one['ms']} ms" for one in sorted(loaf["scripts"], key=lambda one: -one["ms"])[:3])
                flags.append((38, "blocked", f"the page was held {loaf['blocking']:.0f} ms at {loaf['t']} ms: {scripts}"))
        score = sum(shift["value"] for shift in report["shifts"])
        if score > 0.005:
            moved = []
            for shift in report["shifts"]:
                for source in shift["sources"][:2]:
                    if source["from"] and source["to"]:
                        dx, dy = source["to"][0] - source["from"][0], source["to"][1] - source["from"][1]
                        moved.append(f"{source['node']} by ({dx:+d}, {dy:+d})")
            flags.append((35, "layout shift", f"{score:.3f}: " + "; ".join(dict.fromkeys(moved))[:400]))
        for series in report["moves"]:
            points = series["points"]
            if len(points) < 2:
                continue
            steps = []
            for one, two in zip(points, points[1:]):
                if one["box"] and two["box"]:
                    steps.append((max(abs(a - b) for a, b in zip(one["box"], two["box"])), two["t"] - one["t"]))
            moving_steps = [step for step, _ in steps if step > 0.01]
            if not moving_steps:
                continue
            typical = statistics.median(moving_steps)
            worst = max(moving_steps)
            slow = [gap for step, gap in steps if step > 0.01 and gap > 2.5 * self.interval]
            if worst > max(40, 4 * typical):
                flags.append((42, "jumps", f"{series['node']} ({series['name']}) moved {worst:.0f} px in one frame, its steps mostly {typical:.0f} px"))
            elif slow:
                flags.append((30, "uneven motion", f"{series['node']} ({series['name']}) waited {max(slow):.0f} ms between frames {len(slow)} times"))
        took = max([report.get("active", 0)] + [call["t"] + call["ms"] for call in report["calls"]])
        if took > 1500:
            flags.append((20, "slow", f"worked for {took} ms"))
        slowest = max(report["calls"], key=lambda call: call["ms"], default=None)
        if slowest and slowest["ms"] > 800:
            flags.append((18, "slow call", f"{slowest['name']} took {slowest['ms']} ms"))
        return flags

    def run(self):
        commands = menu_commands(set(self.options.menus.split(",")) if self.options.menus else None)
        if self.options.only:
            wanted = self.options.only.split(",")
            commands = [one for one in commands if one["command"] in wanted]
        skipped = set(self.options.skip.split(",")) if self.options.skip else set()
        home = orior_home()
        kept_home = keep_home(home)
        original = self.js("document.getElementById('tree-path')?.textContent ?? ''")
        storage = self.js("JSON.stringify(Object.entries(localStorage))")
        watched = False
        clip = None
        try:
            make_tree(self.tree)
            self.watch()
            watched = True
            clip = self.js("window.__TAURI_INTERNALS__.invoke('clip_read').catch(() => null)")
            self.js(f"(async () => {{ await (await import('/menubar.js')).runCommand('open-folder', [{json.dumps(str(self.tree))}]); return 1; }})()")
            time.sleep(1.5)
            self.baseline()
            self.settle(5)
            self.js("window.__walk.begin()")
            time.sleep(1.0)
            idle = [gap for gap in self.js("window.__walk.report()")["gaps"] if gap > 0]
            if idle:
                self.interval = statistics.median(idle)
            if self.memory:
                self.conn.call("HeapProfiler.enable")
                self.js("window.__TAURI_INTERNALS__.invoke('memory_calls', { on: true, reset: true })")
            passes = " and ".join(name for name, on in (("motion", self.motion), ("memory", self.memory)) if on)
            print(f"walking {len(commands)} commands for {passes}; a frame every {self.interval:.1f} ms; the tree is {self.tree}")
            for index, item in enumerate(commands, 1):
                command = item["command"]
                if command in skipped:
                    self.records.append({**item, "index": index, "skipped": True})
                    continue
                record = {**item, "index": index}
                try:
                    flags = []
                    if self.motion:
                        self.baseline()
                        self.settle(2)
                        ran, report, _ = self.press(command, self.options.wait)
                        record["ran"] = ran
                        left = []
                        if report is not None:
                            record["report"] = report
                            record["shot"] = self.shot(f"{index:03d}-{command}")
                            left = self.put_back(item, ran)
                        record["leftAfterClearing"] = left
                        flags += self.judge(ran, report or {}, left)
                        moved = report is not None and (report["moves"] or report["shifts"])
                        if report is not None and (self.options.frames == "all" or (self.options.frames == "moved" and moved)):
                            record["frames"] = self.film(index, item)
                    if self.memory:
                        ran, report, left, memory = self.weigh(item)
                        record["memory"] = memory
                        if not self.motion:
                            record["ran"] = ran
                            record["leftAfterClearing"] = left
                            flags += self.judge(ran, report or {}, left)
                        flags += self.judge_memory(memory)
                    record["flags"] = [{"weight": weight, "kind": kind, "text": text} for weight, kind, text in sorted(flags, reverse=True)]
                except (RuntimeError, TimeoutError) as error:
                    record["flags"] = [{"weight": 95, "kind": "walker", "text": str(error)[:300]}]
                    if self.conn.closed:
                        self.records.append(record)
                        print(f"the window closed during {command}")
                        break
                    self.watch()
                self.records.append(record)
                worst = record.get("flags", [{}])[0].get("kind", "") if record.get("flags") else ""
                kept = f"{record['memory']['kept'] / 1024:+8.0f} KiB" if record.get("memory") else ""
                print(f"{index:3d}/{len(commands)} {command:24s} {record.get('report', {}).get('active', '')!s:>6} ms {kept}  {worst}")
        finally:
            if not self.conn.closed:
                for closing in ("stop-debug", "stop-profile", "stop-tests", "zoom-reset"):
                    try:
                        self.js(f"window.__walk ? window.__walk.run({json.dumps(closing)}, [], 3000) : null", timeout=30)
                    except (RuntimeError, TimeoutError):
                        pass
                if self.memory:
                    try:
                        self.js("window.__TAURI_INTERNALS__.invoke('memory_calls', { on: false, reset: true })")
                    except (RuntimeError, TimeoutError):
                        pass
                if clip is not None:
                    self.js(f"window.__TAURI_INTERNALS__.invoke('clip_write', {{ text: {json.dumps(clip)} }}).catch(() => null)")
                self.js(f"(() => {{ localStorage.clear(); for (const [key, value] of JSON.parse({json.dumps(storage)})) localStorage.setItem(key, value); return 1; }})()")
                if original:
                    self.js(f"(async () => {{ await (await import('/menubar.js')).runCommand('open-folder', [{json.dumps(original)}]); return 1; }})()", timeout=60)
                if watched:
                    self.js("window.__walk ? window.__walk.restore() : null")
                time.sleep(1.5)
            put_home_back(home, kept_home)
            self.conn.close()
            self.write()
            shutil.rmtree(self.scratch, ignore_errors=True)

    def write_memory(self):
        weighed = [record for record in self.records if record.get("memory")]
        modules, functions, calls = collections.Counter(), collections.Counter(), {}
        for record in weighed:
            memory = record["memory"]
            modules.update(memory["modules"])
            functions.update(memory["functions"])
            for name, one in memory["calls"].items():
                total = calls.setdefault(name, {"count": 0, "taken": 0, "kept": 0, "most": 0})
                total["count"] += one["count"]
                total["taken"] += one["taken"]
                total["kept"] += one["kept"]
                total["most"] = max(total["most"], one["most"])
        commands = [{key: record[key] for key in ("index", "command", "path", "memory")} for record in weighed]
        (self.out / "memory.json").write_text(json.dumps({"sampling": self.options.sampling, "commands": commands, "modules": modules, "functions": functions, "calls": calls}, indent=1), encoding="utf-8")

        def kib(size):
            return f"{size / 1024:,.1f}"

        lines = [
            f"# Memory of the walk of {self.stamp}",
            "",
            f"{len(weighed)} commands, each pressed under V8's sampling heap profiler, one sample each {self.options.sampling} bytes. "
            "What the page holds is what it still held once the command was closed, put back and its heap collected. "
            "A call of the tree's side is counted on the thread that answered it, as the window's allocator gave and took back its bytes.",
            "",
            "## What each command left held",
            "",
            "| # | command | menu | page KiB kept | heap after KiB | held the most | processes that grew | calls KiB taken | calls KiB kept |",
            "| ---: | --- | --- | ---: | ---: | --- | --- | ---: | ---: |",
        ]
        for record in sorted(weighed, key=lambda one: -one["memory"]["kept"]):
            memory = record["memory"]
            top = next(iter(memory["functions"].items()), None)
            grew = ", ".join(f"{name} {delta / (1 << 20):+.1f} MiB" for name, delta in memory["processes"].items() if abs(delta) >= 1 << 20)
            taken = sum(one["taken"] for one in memory["calls"].values())
            kept = sum(one["kept"] for one in memory["calls"].values())
            lines.append(
                f"| {record['index']} | `{record['command']}` | {record['path']} | {kib(memory['kept'])} | {kib(memory['heapAfter'])} | "
                f"{(top[0] + ' ' + kib(top[1])) if top else ''} | {grew} | {kib(taken)} | {kib(kept)} |"
            )
        lines += ["", "## Held by module, over the walk", "", "| module | KiB |", "| --- | ---: |"]
        lines += [f"| {name} | {kib(size)} |" for name, size in modules.most_common(40)]
        lines += ["", "## Held by function, over the walk", "", "| function | KiB |", "| --- | ---: |"]
        lines += [f"| `{name}` | {kib(size)} |" for name, size in functions.most_common(80)]
        lines += ["", "## The tree's side, by call", "", "| call | calls | KiB taken | KiB kept | KiB the most one took |", "| --- | ---: | ---: | ---: | ---: |"]
        for name, one in sorted(calls.items(), key=lambda item: -item[1]["taken"]):
            lines.append(f"| `{name}` | {one['count']} | {kib(one['taken'])} | {kib(one['kept'])} | {kib(one['most'])} |")
        (self.out / "memory.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"memory: {self.out / 'memory.md'}")

    def write(self):
        if self.memory:
            self.write_memory()
        (self.out / "report.json").write_text(json.dumps({"interval": self.interval, "tree": str(self.tree), "commands": self.records}, indent=1), encoding="utf-8")
        flagged = sorted(((flag["weight"], record, flag) for record in self.records for flag in record.get("flags", [])), key=lambda one: -one[0])
        lines = [f"# The walk of {self.stamp}", "", f"{len(self.records)} commands; a frame every {self.interval:.1f} ms with nothing moving; {len(flagged)} flags.", ""]
        lines += ["## Flags, the worst first", "", "| weight | command | menu | kind | what | shot | frames |", "| ---: | --- | --- | --- | --- | --- | --- |"]
        for weight, record, flag in flagged:
            text = flag["text"].replace("|", "\\|").replace("\n", " ")
            shot = f"[shot]({record['shot']})" if record.get("shot") else ""
            frames = f"[frames]({record['frames']})" if record.get("frames") else ""
            lines.append(f"| {weight} | `{record['command']}` | {record['path']} | {flag['kind']} | {text} | {shot} | {frames} |")
        lines += ["", "## Every command", "", "| # | command | menu | worked ms | calls | slowest call | frames | longest frame | moved | shift | held |", "| ---: | --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: | --- |"]
        for record in self.records:
            report = record.get("report")
            if not report:
                lines.append(f"| {record['index']} | `{record['command']}` | {record['path']} | {'skipped' if record.get('skipped') else ''} |  |  |  |  |  |  |  |")
                continue
            slowest = max(report["calls"], key=lambda call: call["ms"], default=None)
            shift = sum(one["value"] for one in report["shifts"])
            held = ", ".join(sorted({one["name"] for one in report["held"]}))
            lines.append(
                f"| {record['index']} | `{record['command']}` | {record['path']} | {report.get('active', 0)} | {len(report['calls'])} | "
                f"{(slowest['name'] + ' ' + str(slowest['ms']) + ' ms') if slowest else ''} | {report['frames']} | {max(report['gaps'], default=0):.0f} | "
                f"{len(report['moves'])} | {shift:.3f} | {held} |"
            )
        (self.out / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"report: {self.out / 'report.md'}")


def options(description, memory_only=False):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--only", help="the commands to walk, by name, comma-separated")
    parser.add_argument("--menus", help="the menus to walk, by title, comma-separated")
    parser.add_argument("--skip", help="commands not to press, comma-separated")
    parser.add_argument("--port", type=int, default=9222, help="the window's debugging port")
    parser.add_argument("--wait", type=float, default=8.0, help="seconds a command is given to finish and come to rest")
    parser.add_argument("--frames", choices=["all", "moved", "none"], default="moved", help="which commands keep every frame of the screen")
    parser.add_argument("--sampling", type=int, default=64, help="the bytes between the heap profiler's samples")
    if not memory_only:
        parser.add_argument("--memory", action="store_true", help="press each command once more under the heap profiler and write memory.md")
    given = parser.parse_args()
    if memory_only:
        given.memory = True
        given.memory_only = True
        given.frames = "none"
    return given


def main():
    Walker(options("Walk every command of orior's menus in the running window and flag what moved, failed or stayed open.")).run()


if __name__ == "__main__":
    main()
