#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Sweeps every control of orior's window, in a window of the sweep's own (isolated.py): each button,
# link, tab, menu item, option and tree item pressed, each field typed into and its Enter pressed,
# each check turned and turned back, each choice set to each of its options and put back, and each
# slider set to its ends and put back, all as real input through the window's debugging port. What a
# press shows that was not shown before, a menu, a submenu, a sheet, a prompt, a view, is swept in
# its turn, reached again from the edit view by the presses that showed it; a tree item and a tab
# are pressed with the right button too, for the menu that shows. Each control is swept once, by the
# key sweep.js names it by.
#
# monitor.js keeps every call inside the window as walk.py's does. A control's record says what it
# was, what was done, and each error the page threw, each call that failed, each link it would have
# followed and how many controls it showed. A control after which the page stops answering or the
# window closes is recorded so, the window is started again on its home and tree, and the sweep goes
# on. The report flags those first, then the controls that threw, then the calls that failed, then
# the controls that could not be reached again, and lists those not pressed: the ones the window shows
# as not to be pressed, and a file's field, which opens the system's picker.
#
# The sweep runs in --windows windows at once, four unless another count is given, each taking every
# so many of the edit view's controls and all each of them shows, their records joined into one report.
#
#   Usage:  python -I ui/test/sweep.py [--display \\.\DISPLAY3] [--windows 4] [--depth 4] [--settle 3]
#                                      [--limit N] [--reach 10]
#
# Each run is kept in the top's build/ui_test/sweep/<time>/: sweep.md and sweep.json, and each window's
# own run, its records, window.log, home and tree, in window_<n>/ beside its output in window_<n>.txt.
# A run with --limit is one window's, kept in the run's folder itself.

import argparse
import collections
import json
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils" / "test"))
import harness_cdp as cdp  # noqa: E402
import isolated  # noqa: E402
import walk  # noqa: E402

HERE = Path(__file__).resolve().parent
TOP = HERE.parents[1]
HELD = "held by the walker"

# How long the window stays still before a press is taken as done, in milliseconds: longer than a menu's or a sheet's
# opening runs
QUIET = 250

# What a field is given before its Enter
TYPED = "sweep"


class Hung(Exception):
    """The page stopped answering."""


class Sweeper:
    def __init__(self, options):
        self.options = options
        self.stamp = time.strftime("%Y%m%d-%H%M%S")
        self.out = Path(options.into) if options.into else TOP / "build" / "ui_test" / "sweep" / self.stamp
        self.out.mkdir(parents=True)
        self.window = isolated.Window(self.out, options.display)
        self.dialogs = []
        self.conn = None
        self.connect()
        self.done = set()
        self.records = []
        self.unreached = []
        self.surfaces = collections.deque([{"name": "the edit view", "steps": []}])
        self.queued = set()

    def connect(self):
        """A connection to the window's page, its dialogs answered; and, where it is a window started again,
        monitor.js and sweep.js put in it and the edit view shown."""
        again = self.conn is not None
        if again:
            self.conn.close()
        self.conn = cdp.Connection(self.window.page)
        self.conn.on("Page.javascriptDialogOpening", self.dialog)
        self.conn.call("Page.enable")
        if again:
            self.prepare()
            self.baseline()

    def dialog(self, params):
        """A dialog of the page's own, an alert or a question, closed unanswered on the reading thread."""
        self.dialogs.append(params.get("message", "")[:120])
        self.conn.notify("Page.handleJavaScriptDialog", accept=False)

    def js(self, expression, timeout=60):
        """The value of `expression` in the page; a page held by something over it is sent an Escape and
        asked again, and one that still does not answer stops the sweep."""
        try:
            return self.conn.evaluate(expression, timeout=timeout)
        except TimeoutError:
            self.key("Escape", "Escape", 27)
            try:
                return self.conn.evaluate(expression, timeout=15)
            except TimeoutError:
                raise Hung(f"the page stopped answering: {expression[:80]}") from None

    def prepare(self):
        """monitor.js and sweep.js put into the page, the sweep stopped unless the watch holds every call."""
        self.js((HERE / "monitor.js").read_text(encoding="utf-8"))
        holds = self.js(
            "(async () => { if (!(await window.__walk.guarded)) return false;"
            " try { await (await import('/bridge.js')).invoke('walker_probe'); return false; }"
            " catch (error) { return String(error).includes('held by the walker'); } })()"
        )
        if holds is not True:
            raise SystemExit("the page's calls are not held by the walker's watch: nothing is pressed")
        self.js((HERE / "sweep.js").read_text(encoding="utf-8"))

    def key(self, key, code, virtual, modifiers=0):
        for kind in ("rawKeyDown", "keyUp"):
            self.conn.call("Input.dispatchKeyEvent", timeout=5, type=kind, key=key, code=code, windowsVirtualKeyCode=virtual, nativeVirtualKeyCode=virtual, modifiers=modifiers)

    def click(self, x, y, button="left"):
        self.conn.call("Input.dispatchMouseEvent", type="mouseMoved", x=x, y=y, button="none")
        for kind in ("mousePressed", "mouseReleased"):
            self.conn.call("Input.dispatchMouseEvent", type=kind, x=x, y=y, button=button, clickCount=1)

    def settle(self, cap):
        """Waits until the window has been still for QUIET, or `cap` seconds; says whether it was."""
        start = time.time()
        while time.time() - start < cap:
            quiet = self.js("window.__walk && window.__sweep ? window.__walk.quietOwn() : -1")
            if quiet == -1:
                self.prepare()
            elif quiet >= QUIET:
                return True
            time.sleep(0.08)
        return False

    def listing(self):
        return self.js("window.__sweep.list()")

    def baseline(self):
        """The tree's dummy files as made, every overlay closed, and the edit view with main.py open in it."""
        for name, text in walk.DUMMIES.items():
            path = self.window.tree / name
            if not path.is_file():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8", newline="\n")
        self.js("window.__walk.clear()")
        self.js(
            "(async () => { (await import('/views.js')).showView('edit'); await (await import('/edit.js')).openFile('main.py');"
            " document.querySelector('#editor .ed-input')?.focus(); return 1; })()"
        )
        self.settle(self.options.settle)

    def replay(self, surface):
        """The surface reached again from the edit view by the presses that showed it: whether it was."""
        self.baseline()
        for one in surface["steps"]:
            found = self.js(f"window.__sweep.locate({json.dumps(one['key'])})")
            if found is None:
                return False
            self.click(found["x"], found["y"], one["button"])
            self.settle(self.options.settle)
        return True

    def reach(self, surface, key):
        """The control of `key` as the surface shows it now, the surface reached again where it is gone, and its last
        press made once more where that closed what an earlier press left open, as a pane's own button does."""
        found = self.js(f"window.__sweep.locate({json.dumps(key)})")
        if found is None and self.replay(surface):
            found = self.looked_for(key)
            last = surface["steps"][-1] if surface["steps"] else None
            again = self.js(f"window.__sweep.locate({json.dumps(last['key'])})") if (found is None and last) else None
            if again is not None:
                self.click(again["x"], again["y"], last["button"])
                self.settle(self.options.settle)
                found = self.looked_for(key)
        return found

    def looked_for(self, key):
        """The control of `key` once it shows, looked for until --reach seconds have gone, as a list the window reads
        on its own, the tree's files or the toolchains' versions, shows after the window has come to rest: or None."""
        until = time.time() + self.options.reach
        found = self.js(f"window.__sweep.locate({json.dumps(key)})")
        while found is None and time.time() < until:
            time.sleep(0.25)
            found = self.js(f"window.__sweep.locate({json.dumps(key)})")
        return found

    def act(self, control):
        """What is done to a control by its kind, and the state put back after; says what was done."""
        kind = control["kind"]
        before = self.js(f"window.__sweep.state({control['index']})")
        if kind in ("button", "tab", "item", "tree"):
            self.click(control["x"], control["y"])
            return "pressed"
        if kind == "check":
            self.click(control["x"], control["y"])
            self.settle(self.options.settle)
            again = self.js(f"window.__sweep.locate({json.dumps(control['key'])})")
            if again is not None and self.js(f"window.__sweep.state({again['index']})") != before:
                self.click(again["x"], again["y"])
                return "turned and turned back"
            return "turned"
        if kind == "field":
            self.click(control["x"], control["y"])
            self.key("a", "KeyA", 65, modifiers=2)
            self.conn.call("Input.insertText", text=TYPED)
            self.key("Enter", "Enter", 13)
            self.settle(self.options.settle)
            again = self.js(f"window.__sweep.locate({json.dumps(control['key'])})")
            if again is not None and before is not None:
                self.js(f"window.__sweep.set({again['index']}, {json.dumps(before)})")
            return "typed into, Enter pressed, put back"
        if kind == "choice":
            for option in range(control["options"]):
                self.js(f"window.__sweep.set({control['index']}, {option})")
                self.settle(self.options.settle)
                again = self.js(f"window.__sweep.locate({json.dumps(control['key'])})")
                if again is None:
                    return f"set to {option + 1} of {control['options']} options, then gone"
                control = again
            self.js(f"window.__sweep.set({control['index']}, {json.dumps(before)})")
            return f"set to each of {control['options']} options, put back"
        if kind == "range":
            least, most = self.js(f"window.__sweep.bounds({control['index']})")
            for value in (least, most):
                self.js(f"window.__sweep.set({control['index']}, {json.dumps(value)})")
                self.settle(self.options.settle)
            self.js(f"window.__sweep.set({control['index']}, {json.dumps(before)})")
            return "set to its ends, put back"
        return "not pressed"

    def sweep_control(self, surface, control, shown):
        """One control swept: its record, and a surface of what it showed queued, and of the menu its
        right button shows for a tree item or a tab."""
        record = {"surface": surface["name"], "key": control["key"], "kind": control["kind"], "label": control["label"], "covered": control["covered"]}
        if control["disabled"]:
            record["did"] = "not pressed: shown as not to be pressed"
            return record
        if control["kind"] == "file":
            record["did"] = "not pressed: it opens the system's picker"
            return record
        self.js("window.__walk.begin(); window.__sweep.navigations = []; 1")
        record["did"] = self.act(control)
        record["settled"] = self.settle(self.options.settle)
        self.js("window.__walk.answer(null)")
        report = self.js("window.__walk.report()")
        record["errors"] = [one["text"].splitlines()[0][:300] for one in report["errors"]]
        record["failed"] = [f"{one['name']}: {one.get('error', '')[:200]}" for one in report["calls"] if not one["ok"] and HELD not in one.get("error", "")]
        record["held"] = sorted({one["name"] for one in report["held"]})
        record["navigations"] = self.js("window.__sweep.navigations")
        record["revealed"] = self.queue(surface, control, "left", shown)
        if control["kind"] in ("tree", "tab"):
            self.js("window.__walk.clear()")
            again = self.reach(surface, control["key"])
            if again is not None:
                self.click(again["x"], again["y"], "right")
                self.settle(self.options.settle)
                record["revealed"] += self.queue(surface, control, "right", shown)
        self.js("window.__walk.clear()")
        return record

    def queue(self, surface, control, button, shown):
        """The controls a press showed that were not shown before nor swept, queued as a surface of
        their own reached by the surface's presses and this one: how many."""
        fresh = [one["key"] for one in self.listing() if one["key"] not in shown and one["key"] not in self.done and one["key"] not in self.queued]
        if not fresh or len(surface["steps"]) >= self.options.depth:
            return len(fresh)
        self.queued.update(fresh)
        name = f"{surface['name']} > {control['label'] or control['kind']}" + (" (right button)" if button == "right" else "")
        self.surfaces.append({"name": name, "steps": surface["steps"] + [{"key": control["key"], "button": button}], "keys": fresh})
        return len(fresh)

    def run(self):
        try:
            self.prepare()
            self.baseline()
            swept = 0
            while self.surfaces:
                surface = self.surfaces.popleft()
                if not self.replay(surface):
                    self.unreached.append({"surface": surface["name"], "keys": surface.get("keys", [])})
                    print(f"not reached again: {surface['name']}")
                    continue
                # each control the surface showed, reached through reach() whether it shows now or not
                wanted = surface.get("keys")
                keys = [one["key"] for one in self.listing()] if wanted is None else list(wanted)
                keys = [key for key in keys if key not in self.done]
                # a window of several takes its share of the edit view's controls, and all each of them shows
                if not surface["steps"] and self.options.of > 1:
                    keys = [key for at, key in enumerate(keys) if at % self.options.of == self.options.shard]
                print(f"{surface['name']}: {len(keys)} controls")
                for key in keys:
                    if key in self.done:
                        continue
                    if self.options.limit and swept >= self.options.limit:
                        return
                    self.done.add(key)
                    control = {"key": key, "kind": "", "label": key.split(" | ")[-1]}
                    began = time.time()
                    try:
                        found = self.reach(surface, key)
                        if found is None:
                            self.unreached.append({"surface": surface["name"], "keys": [key], "seconds": round(time.time() - began, 2)})
                            continue
                        control = found
                        shown = {one["key"] for one in self.listing()}
                        record = self.sweep_control(surface, found, shown)
                    except (Hung, RuntimeError, TimeoutError, ConnectionError, OSError) as error:
                        # a page that stopped answering, or a window that closed, is a finding: the window is started
                        # again on its home and tree and the sweep goes on from the next control
                        code = self.window.restarted()
                        ended = "the window ended" + (f", its exit {code:#x}" if code is not None else " as the sweep ended it")
                        record = {"surface": surface["name"], "key": key, "kind": control["kind"], "label": control["label"], "did": "hung the page" if isinstance(error, Hung) else "the window closed", "errors": [f"{str(error)[:240]}; {ended}"]}
                        self.connect()
                        print(f"  {control['label'][:40]!r}: {record['did']}; {ended}; the window is started again")
                    record["seconds"] = round(time.time() - began, 2)
                    self.records.append(record)
                    swept += 1
                    flag = "THREW" if record.get("errors") else ("failed call" if record.get("failed") else "")
                    print(f"  {swept:5d} {record['kind']:6s} {record['label'][:40]:40s} {record['did'][:30]:30s} {flag}")
        finally:
            self.write()
            self.conn.close()
            self.window.close()

    def write(self):
        written(self.out, self.stamp, self.records, self.unreached, self.dialogs)


def written(out, stamp, records, unreached, dialogs):
    """The sweep's records kept in sweep.json and told in sweep.md under `out`."""
    (out / "sweep.json").write_text(json.dumps({"records": records, "unreached": unreached, "dialogs": dialogs}, indent=1), encoding="utf-8")
    kinds = collections.Counter(one["kind"] for one in records)
    stopped = [one for one in records if one.get("did") in ("hung the page", "the window closed")]
    threw = [one for one in records if one.get("errors") and one not in stopped]
    failed = [one for one in records if one.get("failed") and not one.get("errors")]
    held_back = [one for one in records if one.get("did", "").startswith("not pressed")]
    lines = [f"# The window's sweep, {stamp}", "", f"{len(records)} controls swept: " + ", ".join(f"{count} {kind}" for kind, count in sorted(kinds.items())) + ".", ""]
    lines += [f"{len(stopped)} hung the page or closed the window, {len(threw)} threw, {len(failed)} made a call that failed, {sum(len(one['keys']) for one in unreached)} could not be reached again, {len(held_back)} not pressed.", ""]

    def section(title, chosen, detail):
        lines.extend([f"## {title}", ""])
        lines.extend(f"- {one['surface']} / {one['kind']} {one['label']!r}: {detail(one)}" for one in chosen)
        lines.append("")

    section("Hung the page or closed the window", stopped, lambda one: "; ".join(one["errors"])[:500])
    section("Threw", threw, lambda one: "; ".join(one["errors"])[:500])
    section("A call failed", failed, lambda one: "; ".join(one["failed"])[:500])
    lines.extend(["## Not reached again", ""])
    lines.extend(f"- {one['surface']}: {len(one['keys'])} controls, the first {one['keys'][0]!r}" for one in unreached if one["keys"])
    lines.append("")
    section("Not pressed", held_back, lambda one: one["did"])
    spent = sum(one.get("seconds", 0) for one in records)
    lost = sum(one.get("seconds", 0) for one in unreached)
    lines.extend(["## Time", "", f"{spent:.0f} s sweeping controls, {lost:.0f} s looking for those not reached again.", ""])
    deepest = collections.defaultdict(lambda: [0, 0.0])
    for one in records:
        depth = deepest[one["surface"].count(" > ")]
        depth[0] += 1
        depth[1] += one.get("seconds", 0)
    lines.extend(f"- {count} controls {depth} presses in: {seconds:.0f} s, {seconds / count:.1f} s each" for depth, (count, seconds) in sorted(deepest.items()))
    lines.append("")
    section("The slowest", sorted(records, key=lambda one: -one.get("seconds", 0))[:20], lambda one: f"{one.get('seconds', 0)} s")
    lines.extend(["## Dialogs of the page's own, closed unanswered", ""] + [f"- {one!r}" for one in dialogs] + [""])
    (out / "sweep.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"the report is {out / 'sweep.md'}")


def swept_in_windows(options):
    """The sweep run in `options.windows` windows at once, each a process of its own taking its share of the edit view's
    controls and all they show, their records joined into one report, each control once."""
    stamp = time.strftime("%Y%m%d-%H%M%S")
    out = TOP / "build" / "ui_test" / "sweep" / stamp
    out.mkdir(parents=True)
    common = ["--display", options.display, "--depth", str(options.depth), "--settle", str(options.settle), "--reach", str(options.reach)]
    shares = []
    for shard in range(options.windows):
        into = out / f"window_{shard}"
        told = open(out / f"window_{shard}.txt", "wb")
        words = [sys.executable, "-I", str(Path(__file__).resolve()), *common, "--windows", "1", "--of", str(options.windows), "--shard", str(shard), "--into", str(into)]
        shares.append((into, subprocess.Popen(words, stdout=told, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL), told))
    records, unreached, dialogs, seen = [], [], [], set()
    for into, process, told in shares:
        process.wait()
        told.close()
        kept = json.loads((into / "sweep.json").read_text(encoding="utf-8")) if (into / "sweep.json").is_file() else {"records": [], "unreached": [], "dialogs": []}
        for one in kept["records"]:
            if one["key"] not in seen:
                seen.add(one["key"])
                records.append(one)
        unreached += kept["unreached"]
        dialogs += kept["dialogs"]
    unreached = [{"surface": one["surface"], "keys": [key for key in one["keys"] if key not in seen], "seconds": one.get("seconds", 0)} for one in unreached]
    written(out, stamp, records, [one for one in unreached if one["keys"]], dialogs)


def main():
    # each line said as it is printed, a run's output read as it goes where it is kept in a file
    sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
    parser = argparse.ArgumentParser(description="Sweep every control of orior's window in a window of the sweep's own.")
    parser.add_argument("--display", default="\\\\.\\DISPLAY3", help="the display the window is put on")
    parser.add_argument("--depth", type=int, default=4, help="the most presses a surface is reached by")
    parser.add_argument("--settle", type=float, default=3.0, help="seconds a press is given to come to rest")
    parser.add_argument("--limit", type=int, default=0, help="the most controls swept, none where 0")
    parser.add_argument("--reach", type=float, default=10.0, help="seconds a control reached again is looked for")
    parser.add_argument("--windows", type=int, default=4, help="the windows swept at once, each a share of the controls")
    parser.add_argument("--of", type=int, default=1, help=argparse.SUPPRESS)
    parser.add_argument("--shard", type=int, default=0, help=argparse.SUPPRESS)
    parser.add_argument("--into", help=argparse.SUPPRESS)
    options = parser.parse_args()
    if options.windows > 1 and not options.limit:
        swept_in_windows(options)
    else:
        Sweeper(options).run()


if __name__ == "__main__":
    main()
