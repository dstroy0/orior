# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py window motion: every motion the window makes as a person rests the pointer on each edge and leaves it,
recorded on each frame by ui/test/motion.js and read by motion_read: each motion that is not smooth and each pair of
motions that should match and does not, ranked, the most first, with a picture of each, written by motion_report into
build/harness/motion/<stamp>/ beside the recording itself, recording.json.

    harness.py window motion                          the suite's build started on the fixture tree, on --monitor
    harness.py window motion --port 49398             a window running with its debugging port, as window test
                                                      --keep-window leaves one
    harness.py window motion --edges top,left --rounds 3 --keep-window
    harness.py window motion --read build/harness/motion/<stamp>    a recording kept, read and reported again

The claim holds while the editor has the keys. A file of the tree is opened and the editor pressed first. Then,
--rounds times, for each edge of --edges: the pointer rests in the edge's gap until its part has come and the work has
taken its room, and moves to the middle of the work until the part has gone. Last, the pointer rests on the head and
moves from it straight to the left edge's gap, the switch from one edge to another. Each step is marked on the
recording as it starts, by its edge and whether the part comes in or goes out."""

import json
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

import harness_cdp  # noqa: E402
import harness_input  # noqa: E402
import harness_page  # noqa: E402
import motion_read  # noqa: E402
from utils.tools import findroot  # noqa: E402

RECORDER = findroot.at("ui", "test", "motion.js")
MOTION = findroot.at("build", "harness", "motion")
EDGES = ["top", "left", "right", "bottom", "controls"]
# How long a step waits: coming in, the rest, the part's slide, the work's slide a slide later and a margin; going
# out, the linger, both slides back and a margin. In milliseconds.
IN_MS = 1300
OUT_MS = 1500
# The file opened for the editor to take the keys.
OPENED = "hello.py"


def edge_point(edge, size):
    """The point in an edge's gap the pointer rests on, as the suite's runner rests it."""
    width, height = size["width"], size["height"]
    return {
        "top": (width // 2, 4),
        "left": (4, height // 2),
        "right": (width - 4, height // 2),
        "bottom": (width // 2, height - 4),
        "controls": (width - 40, 4),
    }[edge]


def moved(conn, point):
    harness_input.mouse(conn, "mouseMoved", float(point[0]), float(point[1]), "none", 0, 0, 0)


def pressed(conn, point):
    for kind, buttons in (("mouseMoved", 0), ("mousePressed", 1), ("mouseReleased", 0)):
        harness_input.mouse(conn, kind, float(point[0]), float(point[1]), "left" if kind != "mouseMoved" else "none", buttons, 1 if kind != "mouseMoved" else 0, 0)


def waited_for(conn, expression, seconds, what):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        if conn.evaluate(expression, timeout=10) is True:
            return
        time.sleep(0.05)
    raise RuntimeError("%s did not come within %d s" % (what, seconds))


def claimed(conn):
    """A file opened and the editor pressed, the claim holding; the middle of the work, where the pointer rests
    between steps."""
    conn.evaluate("(async () => { const { openFile } = await import('/edit.js'); await openFile(%s); return true; })()" % json.dumps(OPENED), timeout=30)
    waited_for(conn, "!!document.querySelector('#mode-edit .editor')?.getClientRects().length", 15, "the editor")
    box = conn.evaluate("(() => { const box = document.querySelector('#mode-edit .editor').getBoundingClientRect(); return [box.left + box.width / 2, box.top + box.height / 2]; })()", timeout=10)
    pressed(conn, box)
    waited_for(conn, "document.body.hasAttribute('data-claim')", 5, "the claim")
    time.sleep(1.0)
    return box


def driven(conn, middle, edges, rounds, size):
    """The steps, each marked as it starts."""

    def step(label, point, wait_ms):
        conn.evaluate("window.__motion.mark(%s)" % json.dumps(label), timeout=10)
        moved(conn, point)
        time.sleep(wait_ms / 1000.0)

    moved(conn, middle)
    time.sleep(OUT_MS / 1000.0)
    for _ in range(rounds):
        for edge in edges:
            step("%s in" % edge, edge_point(edge, size), IN_MS)
            step("%s out" % edge, middle, OUT_MS)
    if "top" in edges and "left" in edges:
        step("top in", edge_point("top", size), IN_MS)
        step("top to left", edge_point("left", size), IN_MS + 600)
        step("left out", middle, OUT_MS)


def recorded(conn, edges, rounds):
    """The recorder put into the page, the claim made, the steps taken; the recording."""
    harness_page.Page(conn, {}).ready()
    harness_page.Page(conn, {}).wait_started()
    with open(RECORDER, encoding="utf-8") as kept:
        conn.evaluate(kept.read(), timeout=10)
    middle = claimed(conn)
    size = conn.evaluate("({ width: innerWidth, height: innerHeight })", timeout=10)
    conn.evaluate("window.__motion.start()", timeout=10)
    try:
        driven(conn, middle, edges, rounds, size)
    finally:
        taken = conn.evaluate("window.__motion.stop()", timeout=120)
    return taken


def started(run, monitor):
    """The suite's build started on a copy of the fixture tree in `run`, placed on `monitor`: the window and its
    program."""
    import harness_window
    import harness_window_test as suite

    tree = os.path.join(run, "tree")
    suite.copy_tree(os.path.join(suite.SUITE, "fixtures", "tree"), tree)
    suite.own_repository(tree)
    os.makedirs(os.path.join(run, "home"), exist_ok=True)
    harness_window.windows_ended()
    gui_name = suite.bin_named("gui")
    suite.test_windows_ended(suite.window_program(gui_name))
    program = suite.window_built(gui_name)
    window = suite.Window(program, run, tree, [])
    window.placed(monitor)
    return window, program


def reported(taken, run):
    """A recording read and reported into `run`, the findings said, the most first. motion_report is imported here
    and not at the top, because it needs Pillow and the harness's other commands do not."""
    import motion_report

    reading = motion_read.read(taken)
    report = motion_report.written(reading, taken, run)
    kinds = {}
    for one in reading.findings:
        kinds[one.kind] = kinds.get(one.kind, 0) + 1
    print("%d frames at %.1f ms, %d motions, %d findings%s" % (len(taken.get("frames", [])), reading.frame_ms, len(reading.motions), len(reading.findings), (": " + ", ".join("%d %s" % (count, kind) for kind, count in sorted(kinds.items(), key=lambda one: -one[1]))) if kinds else ""))
    for rank, one in enumerate(reading.findings[:12], 1):
        print("  %2d. %-18s %-10s %-16s %6.2f  %s" % (rank, one.kind, one.part, one.step, one.measure, one.said))
    print("report: %s" % report)
    return 0


def cmd_window_motion(a):
    if a.read:
        path = os.path.join(a.read, "recording.json") if os.path.isdir(a.read) else a.read
        with open(path, encoding="utf-8") as kept:
            taken = json.load(kept)
        return reported(taken, os.path.dirname(os.path.abspath(path)))
    edges = [edge.strip() for edge in a.edges.split(",") if edge.strip()]
    wrong = [edge for edge in edges if edge not in EDGES]
    if wrong:
        print("no edge %s: the edges are %s" % (", ".join(wrong), ", ".join(EDGES)))
        return 1
    run = os.path.join(MOTION, time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(run, exist_ok=True)
    window = program = None
    try:
        if a.port:
            url = harness_cdp.window_page(a.port)
        else:
            window, program = started(run, a.monitor)
            url = window.page
        conn = harness_cdp.Connection(url)
        try:
            taken = recorded(conn, edges, a.rounds)
        finally:
            conn.close()
    finally:
        if window is not None:
            if a.keep_window:
                print("the window is kept open on port %d" % window.port, flush=True)
            else:
                import harness_window_test as suite

                window.close()
                suite.test_windows_ended(program)
        shutil.rmtree(os.path.join(run, "tree"), ignore_errors=True)
    with open(os.path.join(run, "recording.json"), "w", encoding="utf-8", newline="\n") as out:
        json.dump(taken, out)
    return reported(taken, run)


def register(window):
    """The motion command, added to the window group."""
    p = window.add_parser("motion", help="the window's motions as each edge's part comes and goes, read for what is not smooth and what does not match, to build/harness/motion")
    p.add_argument("--port", type=int, default=0, help="a running window's debugging port; the suite's build is started where none")
    p.add_argument("--monitor", type=int, default=2, help="the display the started window is placed on")
    p.add_argument("--edges", default=",".join(EDGES), help="the edges rested on, of %s" % ", ".join(EDGES))
    p.add_argument("--rounds", type=int, default=2, help="how many times each edge is rested on and left")
    p.add_argument("--keep-window", action="store_true", help="the started window left open")
    p.add_argument("--read", default="", help="a recording kept, its folder or its recording.json, read and reported again beside it, no window started")
    p.set_defaults(fn=cmd_window_motion)
