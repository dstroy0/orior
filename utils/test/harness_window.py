# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py window: the released window started, ended, watched, pictured and measured, on Windows.

    harness.py window start [--display \\\\.\\DISPLAY1] [--id N]   one window alone, placed on the display, its debugging
                            [--port P]                           port on P where one is given
    harness.py window ended                                      every window running from the release program ended
    harness.py window watch [--seconds 25]                       started alone with its output kept, ended after
    harness.py window shot [--id N] [--width 480] [--height 320] its top-left pictured to build/harness/window_shot.png
    harness.py window zoom [--x --y --width --height --scale]    a part drawn larger to build/harness/window_zoom.png
    harness.py window record [--for 3000] [--start]              recorded as a video at the display's rate, lossless, into
                             [--port P --trigger JS]             build/harness/recordings, an expression run first, or
                                                                 started and recorded from before its first frame
    harness.py window slide [REC] [--from S] [--to S] [--crop]   a recording slid through a frame at a time, each against
                                                                 the one before, into build/harness/slide (harness_record.py)
    harness.py window frames [VIDEO] [--thumb X] [--rate R]      each frame's text blank, plain or colored, and where it
                                                                 stood still as the scrollbar's thumb moved, to
                                                                 build/harness/frames.tsv (harness_record.py)
    harness.py window dump [--path FILE]                         the newest crash dump read under cdb, the faulting stack
    harness.py window eval --port P EXPRESSION                   an expression run on the window's page, its answer printed
    harness.py window type --port P [--key] TEXT                 text typed into the window key by key, or one chord
    harness.py window drag --port P [--steps 120] [--rounds 1]   the editor's scrollbar dragged by its thumb end to end
                                                                 and back
    harness.py window profile --port P [--no-drag]               the page's processor time sampled over such a drag, its
                                                                 late frames and the functions that spent the most
    harness.py window regions [--id N] [--count 12]              committed bytes by kind and the largest allocations
    harness.py window debug [--seconds 45]                       run under cdb, the stack where it faults
    harness.py window memory [--id N] [--port P]                 its process tree's working set, private working set and
                                                                 commit, by kind and with the page's heap through a port
    harness.py window luminance [--floor 78] [--each 6]          the dark scheme's tokens by lightness, with their rules
    harness.py window cpu [--minutes 2] [--step 5] [--spike 150] its processor time each step, its busiest threads, a
                                                                 stack of the busiest past the spike, to window_cpu.txt
    harness.py window gpu [--seconds 50] [--id N]                its process tree's GPU 3D engine share each second and
                                                                 its processor time, to window_gpu.txt
    harness.py window in-job --id N                             whether a process sits in a job
    harness.py window test [arg...]                              the window's suite, ui/test/src (harness_window_test.py)
    harness.py window motion [--port P] [--edges E] [--rounds N] the window's motions as each edge's part comes and goes,
                                                                 what is not smooth and what does not match, ranked, to
                                                                 build/harness/motion (harness_motion.py)

The window's program is build/cargo/release/<gui>.exe, gui as bin_names.tsv names it. Every start ends the windows
running from that program first; one window therefore runs at a time, and its process id is kept in build/harness/window.pid
for the commands that read it back."""

import os
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
HARNESS_BUILD = os.path.join(ROOT, "build", "harness")
PID_FILE = os.path.join(HARNESS_BUILD, "window.pid")
CDB = r"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
DISPLAY = r"\\.\DISPLAY1"
PLACE_TRIES = 100
PLACE_POLL_S = 0.1
MIB = 1048576.0


def bin_named(wanted):
    """The name bin_names.tsv gives the program with this id."""
    with open(os.path.join(ROOT, "bin_names.tsv"), encoding="utf-8") as table:
        head = table.readline().rstrip("\n").split("\t")
        for line in table:
            row = dict(zip(head, line.rstrip("\n").split("\t")))
            if row.get("id") == wanted:
                return row["name"]
    raise SystemExit("bin_names.tsv names no %s" % wanted)


def gui_name():
    return bin_named("gui")


def host_target():
    """The target triple ui/cli/build.sh builds for on this machine."""
    if os.name == "nt":
        return "x86_64-pc-windows-msvc"
    import platform

    return "%s-%s" % (platform.machine(), "apple-darwin" if sys.platform == "darwin" else "unknown-linux-gnu")


def program():
    return os.path.join(ROOT, "build", "cli", host_target(), gui_name() + (".exe" if os.name == "nt" else ""))


def windows_only():
    if os.name != "nt":
        print("the window commands run on Windows alone")
        return False
    return True


def pid_written(process_id):
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    with open(PID_FILE, "w", encoding="utf-8") as out:
        out.write("%d\n" % process_id if process_id else "")


def pid_read():
    try:
        with open(PID_FILE, encoding="utf-8") as kept:
            return int(kept.read().strip() or 0)
    except (OSError, ValueError):
        return 0


def windows_ended():
    """Every window running from the release program ended, whatever started it; how many were."""
    import harness_win32 as win

    wanted = program().lower()
    name = (gui_name() + ".exe").lower()
    running = [process_id for process_id, _, exe in win.processes() if exe.lower() == name and win.image_path(process_id).lower() == wanted]
    for process_id in running:
        win.ended(process_id)
    pid_written(0)
    return len(running)


def window_placed(process_id, display):
    """The process's window waited for and moved onto the display's working area; True once it stands there."""
    import harness_win32 as win

    areas = win.displays()
    if display not in areas:
        print("no display %s (attached: %s)" % (display, " ".join(sorted(areas))))
        return False
    left, top, right, bottom = areas[display]
    titles_wanted = (gui_name(), "orior")
    for _ in range(PLACE_TRIES):
        time.sleep(PLACE_POLL_S)
        handle, title = win.main_window(process_id)
        if not handle or title not in titles_wanted:
            continue
        rect = win.window_rect(handle)
        centre_x = (rect[0] + rect[2]) // 2
        centre_y = (rect[1] + rect[3]) // 2
        if left <= centre_x < right and top <= centre_y < bottom:
            print("window %d on %s at %d,%d" % (process_id, display, rect[0], rect[1]))
            return True
        win.window_moved(handle, left, top)
    print("the window of %d did not stand on %s" % (process_id, display))
    return False


def browser_arguments(port):
    """The web view's arguments as tauri.conf.json gives them, with its debugging port on `port`."""
    import json

    with open(findroot.at("ui", "gui", "src-tauri", "tauri.conf.json"), encoding="utf-8") as settings:
        given = json.load(settings)["app"]["windows"][0].get("additionalBrowserArgs", "")
    return ("%s --remote-debugging-port=%d" % (given, port)).strip()


WINDOW_ERRORS = os.path.join(HARNESS_BUILD, "window_errors.txt")


def window_started(stdout=None, stderr=None, port=0):
    """The release window started alone, from the repository's top, its id kept, its debugging port on `port` where
    one is given. Its error stream, where the cells' log is written, goes to build/harness/window_errors.txt where the
    caller names none."""
    windows_ended()
    env = dict(os.environ)
    if port:
        env["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"] = browser_arguments(port)
    errors = stderr or open(WINDOW_ERRORS, "wb")
    process = subprocess.Popen([program()], cwd=ROOT, env=env, stdin=subprocess.DEVNULL, stdout=stdout or subprocess.DEVNULL, stderr=errors)
    pid_written(process.pid)
    return process


def cmd_start(a):
    if not windows_only():
        return 1
    import harness_win32 as win

    if a.id:
        if not win.alive(a.id):
            print("no process %d" % a.id)
            return 1
        process_id = a.id
    else:
        process_id = window_started(port=a.port).pid
    placed = window_placed(process_id, a.display)
    if placed and a.port:
        print("its debugging port %d" % a.port)
    return 0 if placed else 1


def cmd_ended(a):
    if not windows_only():
        return 1
    print("ended %d" % windows_ended())
    return 0


def cmd_keys(a):
    """The keys named sent to the window as real input, in order."""
    if not windows_only():
        return 1
    import harness_win32 as win

    found = window_of(a.id)
    if not found:
        return 1
    process_id, handle, _ = found
    if not win.keys_sent(handle, a.names):
        print("window %d could not be brought to the front; nothing sent" % process_id)
        return 1
    print("window %d sent %d keys" % (process_id, len(a.names)))
    return 0


def cmd_click(a):
    """A press at x, y of the window's client posted to its queue, dragged to --to where one is given, and released."""
    if not windows_only():
        return 1
    import harness_win32 as win

    found = window_of(a.id)
    if not found:
        return 1
    process_id, handle, _ = found
    moved = [(a.to[0], a.to[1])] if a.to else []
    for _ in range(a.count):
        win.pointer_sent(handle, a.x, a.y, True, True, moved)
    print("window %d pressed %d times at %d,%d%s" % (process_id, a.count, a.x, a.y, (" to %d,%d" % tuple(a.to)) if a.to else ""))
    return 0


def cmd_move(a):
    """The pointer moved to x, y of the window's client with no button down, through the places --via on its way, a
    --pause between the moves; a move that ends in an edge's band and rests there reveals the edge's part."""
    if not windows_only():
        return 1
    import harness_win32 as win

    found = window_of(a.id)
    if not found:
        return 1
    process_id, handle, _ = found
    via = [(a.via[at], a.via[at + 1]) for at in range(0, len(a.via or []), 2)]
    win.pointer_moved(handle, via + [(a.x, a.y)], a.pause)
    print("window %d pointer moved to %d,%d through %d places" % (process_id, a.x, a.y, len(via)))
    return 0


GUI_CHECKS = os.path.join(ROOT, "utils", "test", "src", "ui", "gui", "gui_test.sh")


def cmd_check(a):
    """The window's C checked without a display: every *_test.c under utils/test/src/ui/gui, or those named."""
    import harness_cli

    seen = []
    code = harness_cli.streamed([harness_cli.bash(), GUI_CHECKS, *a.names], seen=seen, env=harness_cli.checks_env(a))
    checks, failed, exits = harness_cli.output_read("".join(seen))
    # a file ended as stalled printed no exit of its own
    failing = sum(1 for one in exits if one != 0) + (1 if code == 124 else 0)
    print("window check: %d checks, %d failed, %d of %d files failing" % (checks, failed, failing, len(exits)), flush=True)
    return code or (1 if (failed or failing) else 0)


def cmd_bench(a):
    """The frame bench, utils/test/src/ui/gui/frame_bench.c, built as a check is and run: the milliseconds a frame of
    the window takes at a monitor's size, and a blend of the whole raster alone."""
    import harness_cli

    return harness_cli.streamed([harness_cli.bash(), GUI_CHECKS, "frame_bench"])


def cmd_icon(a):
    """The program's icon file: utils/test/src/ui/gui/icon_file.c built as a check is and run, its orior.ico copied to
    ui/gui/src beside orior.rc, which the window's build on Windows compiles into the program."""
    import harness_cli
    import shutil

    code = harness_cli.streamed([harness_cli.bash(), GUI_CHECKS, "icon_file"])
    if code:
        return code
    written = os.path.join(ROOT, "build", "gui_test", "orior.ico")
    kept = os.path.join(ROOT, "ui", "gui", "src", "orior.ico")
    shutil.copyfile(written, kept)
    print("icon: %s, %d bytes" % (os.path.relpath(kept, ROOT), os.path.getsize(kept)), flush=True)
    return 0


def orior_home():
    """orior's own folder, where it keeps its state: ORIOR_HOME where set, else the user's roaming folder on Windows and
    ~/.orior elsewhere."""
    if os.environ.get("ORIOR_HOME"):
        return os.environ["ORIOR_HOME"]
    if os.name == "nt":
        return os.path.join(os.environ.get("APPDATA", os.path.expanduser("~")), "orior")
    return os.path.join(os.path.expanduser("~"), ".orior")


def cmd_session(a):
    """The tree's kept session in orior's state written as given: the tabs in order and the one shown, under the key
    the window and the page keep it by, the rest of the state left as it stands; or read and printed where no tabs
    are given."""
    import json

    path = os.path.join(orior_home(), "state.json")
    key = "session." + os.path.normpath(ROOT)
    state = {}
    if os.path.exists(path):
        with open(path, encoding="utf-8") as kept:
            state = json.load(kept)
    if not a.tabs:
        print(json.dumps(state.get(key), indent=2))
        return 0
    active = a.active or a.tabs[-1]
    state[key] = {"tabs": list(a.tabs), "main": list(a.tabs), "active": active, "split": None, "groups": []}
    with open(path, "w", encoding="utf-8") as kept:
        json.dump(state, kept, indent=2)
    print("kept %d tabs for %s, %s shown, in %s" % (len(a.tabs), ROOT, active, path))
    return 0


def cmd_watch(a):
    if not windows_only():
        return 1
    import harness_win32 as win

    out_path = os.path.join(HARNESS_BUILD, "window_watch.out")
    err_path = os.path.join(HARNESS_BUILD, "window_watch.err")
    with open(out_path, "wb") as out, open(err_path, "wb") as err:
        process = window_started(out, err)
        try:
            code = process.wait(a.seconds)
            print("ended with %d within %d s" % (code, a.seconds))
        except subprocess.TimeoutExpired:
            print("still running after %d s, process %d" % (a.seconds, process.pid))
            win.ended(process.pid)
            pid_written(0)
    for label, path, count in (("out", out_path, 15), ("err", err_path, 25)):
        print("--- " + label)
        with open(path, encoding="utf-8", errors="replace") as kept:
            print("".join(kept.readlines()[-count:]), end="")
    return 0


def window_of(process_id):
    """(id, handle, rect) of the window of a process, the one window.pid names where none is given."""
    import harness_win32 as win

    process_id = process_id or pid_read()
    if not process_id or not win.alive(process_id):
        print("no process %d" % process_id)
        return None
    handle, _ = win.main_window(process_id)
    if not handle:
        print("the process %d has no window" % process_id)
        return None
    return process_id, handle, win.window_rect(handle)


def cmd_shot(a):
    if not windows_only():
        return 1
    import harness_win32 as win

    found = window_of(a.id)
    if not found:
        return 1
    process_id, _, rect = found
    path = os.path.join(HARNESS_BUILD, "window_shot.png")
    win.png_written(path, win.screen_pixels(rect[0], rect[1], a.width, a.height))
    print("window %d at %d,%d to %d,%d, shot %d x %d: %s" % (process_id, *rect, a.width, a.height, path))
    return 0


def frames_window_started():
    """The suite's build of the window started on a copy of the fixture tree with a home and a web view profile of
    its own under build/harness/frames_run, placed on the harness's display, for frames from its first moment.
    Returns the process."""
    import shutil

    import harness_win32 as win
    import harness_window_test as suite

    run = os.path.join(HARNESS_BUILD, "frames_run")
    shutil.rmtree(run, ignore_errors=True)
    tree = os.path.join(run, "tree")
    suite.copy_tree(os.path.join(suite.SUITE, "fixtures", "tree"), tree)
    os.makedirs(os.path.join(run, "home"), exist_ok=True)
    env = dict(os.environ)
    env.update({"ORIOR_HOME": os.path.join(run, "home"), "ORIOR_ROOT": tree, "ORIOR_NO_REPORTS": "1", "WEBVIEW2_USER_DATA_FOLDER": os.path.join(run, "webview")})
    program = os.path.join(suite.APP_TARGET, suite.APP_PROFILE, gui_name() + ".exe")
    process = subprocess.Popen([program], cwd=tree, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    until = time.monotonic() + 60
    while time.monotonic() < until and not win.main_window(process.pid)[0]:
        time.sleep(0.05)
    window_placed(process.pid, DISPLAY)
    return process


def cmd_eval(a):
    """An expression run on the window's page through its debugging port, its answer printed: a promise is
    awaited, and an object is printed as JSON."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import harness_cdp

    conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
    conn.call("Runtime.enable", timeout=10)
    print(conn.evaluate(a.expression, timeout=a.seconds))
    return 0


def cmd_type(a):
    """Text typed into the window through its debugging port, key by key as the suite types, a chord of the form
    Ctrl+S with --key."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import harness_cdp
    import harness_input

    conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
    if a.key:
        harness_input.press(conn, a.text)
    else:
        harness_input.type_text(conn, a.text)
    return 0


def thumb_of(conn):
    """The active editor's scrollbar as the page lays it out: the column it runs down, the middle of its thumb, and the
    farthest the thumb's middle goes up and down, its arrows at each end; None where no editor with a scrollbar is in
    front."""
    return conn.evaluate(
        "(async () => { const e = await import(location.origin + '/edit.js'); const v = e.editing().editor; if (!v) return null;"
        " const sc = v.scroller; const box = sc.getBoundingClientRect(); const bar = sc.offsetWidth - sc.clientWidth;"
        " if (bar <= 0) return null; const track = sc.clientHeight - 2 * bar;"
        " const thumb = Math.max(bar, track * sc.clientHeight / sc.scrollHeight);"
        " const room = Math.max(1, sc.scrollHeight - sc.clientHeight);"
        " const top = box.top + bar + thumb / 2;"
        " return { x: box.right - bar / 2, y: top + (track - thumb) * sc.scrollTop / room, top: top + 1, bottom: top + track - thumb - 1 }; })()",
        timeout=10,
    )


def cmd_drag(a):
    """The editor's scrollbar dragged through the window's debugging port by its thumb, a person's drag: pressed on the
    thumb where it stands, carried to the bottom end, up to the top end and back to where it began, --rounds times, --steps
    moves each way, a move a frame."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import harness_cdp
    import harness_input

    conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
    conn.call("Runtime.enable", timeout=10)
    where = thumb_of(conn)
    if not where:
        print("no editor with a scrollbar is in front")
        return 1
    x, y = where["x"], where["y"]
    for _ in range(a.rounds):
        for to in (where["bottom"], where["top"], where["y"]):
            harness_input.perform(conn, {"op": "drag", "x": x, "y": y, "toX": x, "toY": to, "steps": a.steps}, None)
            y = to
    print("dragged the scrollbar's thumb from %d to the bottom, the top and back %d time%s, %d moves each way" % (where["y"], a.rounds, "" if a.rounds == 1 else "s", a.steps))
    return 0


def timeline_started(conn):
    """The browser's timeline begun through `conn`: the events it hands on kept in the list given back, and an event set
    once the timeline is whole."""
    kept = {"events": [], "whole": threading.Event()}
    conn.on("Tracing.dataCollected", lambda params: kept["events"].extend(params.get("value", [])))
    conn.on("Tracing.tracingComplete", lambda _params: kept["whole"].set())
    conn.call("Tracing.start", timeout=10, transferMode="ReportEvents", traceConfig={"includedCategories": ["devtools.timeline", "disabled-by-default-devtools.timeline", "blink", "v8"]})
    return kept


def timeline_read(conn, kept, count):
    """The timeline begun by timeline_started ended and read: the renderer's main thread's work by its kind, each event's
    own time, the time of the events within it taken off, as lines of the profile."""
    conn.call("Tracing.end", timeout=10)
    kept["whole"].wait(60)
    events = kept["events"]
    main = {(event.get("pid"), event.get("tid")) for event in events if event.get("ph") == "M" and event.get("name") == "thread_name" and event.get("args", {}).get("name") == "CrRendererMain"}
    spans = sorted(((event["ts"], event["ts"] + event["dur"], event.get("name", "")) for event in events if event.get("ph") == "X" and "dur" in event and (event.get("pid"), event.get("tid")) in main), key=lambda span: (span[0], -span[1]))
    # each kind's own time: its spans' lengths less the spans that begin and end within them, the nearest enclosing span
    # giving up each
    own = {}
    stack = []
    for begun, ended, name in spans:
        while stack and stack[-1][1] <= begun:
            stack.pop()
        if stack:
            own[stack[-1][2]] = own.get(stack[-1][2], 0) - (ended - begun)
        stack.append((begun, ended, name))
        own[name] = own.get(name, 0) + (ended - begun)
    selves = own
    whole = sum(selves.values()) or 1
    lines = ["the renderer's main thread, %.0f ms of events, by the kind of work, each kind's own time:" % (whole / 1000.0)]
    for name, micro in sorted(selves.items(), key=lambda one: -one[1])[:count]:
        lines.append("  %7.1f ms %5.1f%%  %s" % (micro / 1000.0, 100.0 * micro / whole, name))
    return lines


def cmd_profile(a):
    """The page's processor time sampled through its debugging port while the editor's scrollbar is dragged by its
    thumb, or for --seconds where --no-drag is given: the frames the page drew and those that came late, and the
    functions that took the most time themselves, to build/harness/window_profile.txt and the screen."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import harness_cdp
    import harness_input

    conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
    conn.call("Runtime.enable", timeout=10)
    # each frame's time kept on the page while the profile runs, and how many of the editor's frames drew and how many
    # kept the last frame drawn, its view past the lines its file's window holds
    conn.evaluate("(() => { const kept = []; window.__profileFrames = kept; const each = (now) => { kept.push(now); if (window.__profileFrames === kept) requestAnimationFrame(each); }; requestAnimationFrame(each); return kept.length; })()", timeout=10)
    # the browser's own account of each frame later than --late: each script it ran with the style and layout the
    # script forced, the style and layout after them, and the rest of its drawing
    conn.evaluate(
        "(() => { const long = []; window.__profileLong = long; const late = %f;"
        " window.__profileObserver = new PerformanceObserver((list) => { for (const entry of list.getEntries()) { if (entry.duration <= late) continue;"
        " const end = entry.startTime + entry.duration; const layout = entry.styleAndLayoutStart ? end - entry.styleAndLayoutStart : 0;"
        " const render = entry.renderStart ? end - entry.renderStart - layout : 0;"
        " const scripts = entry.scripts.map((one) => `${one.sourceFunctionName || one.invoker} (${one.sourceURL.split('/').pop()}:${one.sourceCharPosition}) ${Math.round(one.duration)} forced ${Math.round(one.forcedStyleAndLayoutDuration)}`).join(', ');"
        " long.push(`${Math.round(entry.duration)} ms at ${Math.round(entry.startTime)}: ${scripts || 'no script'}; style and layout ${Math.round(layout)}, drawing ${Math.round(render)}`); } });"
        " window.__profileObserver.observe({ type: 'long-animation-frame' }); return true; })()" % a.late,
        timeout=10,
    )
    conn.evaluate(
        "(async () => { const e = await import(location.origin + '/edit.js'); const v = e.editing().editor; if (!v) return null;"
        " const painted = v.painted; const told = { drawn: 0, held: 0, shown: new Set(), reports: 0, answered: 0, letters: 0 }; window.__profilePaints = told; window.__profilePainted = painted;"
        " v.painted = function (...given) { const result = painted.apply(this, given); if (this.heldFrame === this.s && this.lastTop !== undefined && this.pastWindow?.(this.lastTop)) told.held += 1; else told.drawn += 1;"
        " const rows = this.rows(); const first = Math.max(0, Math.floor(this.scrollYKnown() / this.lineHeight)); const last = Math.min(rows.size - 1, first + Math.ceil(this.viewSize().height / this.lineHeight));"
        " for (let row = first; row <= last; row += 1) told.shown.add(this.s.base + rows.lineOf(row)); return result; };"
        " if (!window.__profileHeard) { window.__profileHeard = true; const b = await import(location.origin + '/bridge.js');"
        " b.onCalls((command, args, promise) => { const kept = window.__profilePaints; if (!kept || command !== 'pages_view') return; kept.reports += 1;"
        " promise.then((answer) => { kept.answered += answer?.lines?.length ?? 0; kept.letters += (answer?.lines ?? []).reduce((sum, line) => sum + line.length, 0); }, () => {}); }); }"
        " return true; })()",
        timeout=10,
    )
    conn.call("Profiler.enable", timeout=10)
    conn.call("Profiler.setSamplingInterval", timeout=10, interval=a.interval)
    traced = timeline_started(conn) if a.trace else None
    conn.call("Profiler.start", timeout=10)
    # the page's clock as the profile begins, which lays the profile's samples against its frames
    began = conn.evaluate("performance.now()", timeout=10)
    where = thumb_of(conn) if not (a.no_drag or a.wheel) else None
    if a.wheel:
        # the wheel turned over the editor's middle, a notch a frame, down and back up
        middle = conn.evaluate("(async () => { const e = await import(location.origin + '/edit.js'); const box = e.editing().editor.scroller.getBoundingClientRect(); return { x: box.left + box.width / 2, y: box.top + box.height / 2 }; })()", timeout=10)
        for delta in (a.delta, -a.delta):
            for _ in range(a.wheel):
                harness_input.perform(conn, {"op": "wheel", "x": middle["x"], "y": middle["y"], "deltaX": 0, "deltaY": delta}, None)
                time.sleep(0.016)
    elif where:
        x, y = where["x"], where["y"]
        for to in (where["bottom"], where["top"], where["y"]):
            harness_input.perform(conn, {"op": "drag", "x": x, "y": y, "toX": x, "toY": to, "steps": a.steps}, None)
            y = to
    else:
        time.sleep(a.seconds)
    profile = conn.call("Profiler.stop", timeout=60)["profile"]
    frames = conn.evaluate("(() => { const kept = window.__profileFrames || []; window.__profileFrames = null; return kept; })()", timeout=10) or []
    long = conn.evaluate("(() => { window.__profileObserver?.disconnect(); const long = window.__profileLong || []; window.__profileLong = null; return long; })()", timeout=10) or []
    paints = conn.evaluate(
        "(async () => { const e = await import(location.origin + '/edit.js'); const v = e.editing().editor; if (v && window.__profilePainted) v.painted = window.__profilePainted;"
        " window.__profilePainted = null; const told = window.__profilePaints; window.__profilePaints = null;"
        " return told ? { ...told, shown: told.shown.size } : null; })()",
        timeout=10,
    )
    gaps = [later - earlier for earlier, later in zip(frames, frames[1:])]
    late = sorted((gap for gap in gaps if gap > a.late), reverse=True)
    # each node's own time: its samples by the time between samples, each sample also laid on the page's clock
    spent = {}
    deltas = profile.get("timeDeltas", [])
    by_id = {node["id"]: node for node in profile["nodes"]}
    at = 0.0
    timed = []
    for sample, delta, after in zip(profile.get("samples", []), deltas, deltas[1:] + [0]):
        at += delta
        frame = by_id[sample]["callFrame"]
        url = frame.get("url", "").rsplit("/", 1)[-1]
        name = "%s %s:%d" % (frame.get("functionName") or "(anonymous)", url, frame.get("lineNumber", -1) + 1) if url else (frame.get("functionName") or "(program)")
        spent[name] = spent.get(name, 0) + after
        timed.append((began + at / 1000.0, name, after))
    whole = sum(spent.values()) or 1
    # each node's caller, for the time a function spent as each caller called it
    parent = {}
    for node in profile["nodes"]:
        for child in node.get("children", []):
            parent[child] = node["id"]

    def named_node(node_id):
        frame = by_id[node_id]["callFrame"]
        url = frame.get("url", "").rsplit("/", 1)[-1]
        return "%s %s:%d" % (frame.get("functionName") or "(anonymous)", url, frame.get("lineNumber", -1) + 1) if url else (frame.get("functionName") or "(program)")

    callers = {}
    for sample, after in zip(profile.get("samples", []), deltas[1:] + [0]):
        up = parent.get(sample)
        key = (named_node(sample), named_node(up) if up is not None else "(root)")
        callers[key] = callers.get(key, 0) + after
    lines = ["%d frames, %d later than %d ms%s" % (len(frames), len(late), a.late, (": " + ", ".join("%.0f" % gap for gap in late[:12]) + " ms") if late else "")]
    if paints:
        lines.append("the editor drew %d frames and kept the last one drawn in %d, its view past its file's window" % (paints["drawn"], paints["held"]))
        lines.append("a paged file's %d reports answered %d lines, %d letters; the frames showed %d lines of the file"
                     % (paints["reports"], paints["answered"], paints["letters"], paints["shown"]))
    lines.append("the page's time, %.0f ms sampled, by the function that spent it itself:" % (whole / 1000.0))
    for name, micro in sorted(spent.items(), key=lambda one: -one[1])[: a.count]:
        lines.append("  %7.1f ms %5.1f%%  %s" % (micro / 1000.0, 100.0 * micro / whole, name))
    lines.append("the same, by the function and what called it:")
    for (name, caller), micro in sorted(((key, value) for key, value in callers.items() if key[0] not in ("(idle)", "(program)")), key=lambda one: -one[1])[: a.count]:
        lines.append("  %7.1f ms  %s  <-  %s" % (micro / 1000.0, name, caller))
    # what ran in the latest frames' gaps, idle left out
    gaps_at = sorted(((later - earlier, earlier, later) for earlier, later in zip(frames, frames[1:]) if later - earlier > a.late), reverse=True)
    for gap, earlier, later in gaps_at[:5]:
        inside = {}
        for when, name, micro in timed:
            if earlier <= when < later and name != "(idle)":
                inside[name] = inside.get(name, 0) + micro
        top = sorted(inside.items(), key=lambda one: -one[1])[:6]
        lines.append("a frame %.0f ms late, at %.0f ms: %s" % (gap, earlier - began, ", ".join("%s %.1f ms" % (name, micro / 1000.0) for name, micro in top) or "nothing sampled"))
    lines.append("the browser's frames longer than %d ms, %d of them:" % (a.late, len(long)))
    lines += ["  " + one for one in long[: a.count]]
    if traced is not None:
        lines += timeline_read(conn, traced, a.count)
    text = "\n".join(lines)
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    with open(os.path.join(HARNESS_BUILD, "window_profile.txt"), "w", encoding="utf-8") as out:
        out.write(text + "\n")
    print(text)
    return 0


def cmd_presses(a):
    """Down, or Up with --up, pressed --count times in the editor of the window behind --port as fast as the page takes
    them, from --line where it is given and else from the line the cursor stands on: each press that left the cursor
    where it was or sent it back, with the window of a paged file as it stood, its reports in flight and whether the calls about the
    file before them were answered; the calls unanswered as the presses begin; and the slowest of a paged file's
    reports, to build/harness/window_presses.txt and the screen."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import harness_cdp
    import harness_input

    step = -1 if a.up else 1
    key = "ArrowUp" if a.up else "ArrowDown"
    conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
    conn.call("Runtime.enable", timeout=10)
    # every call the page sends kept until it is answered, and each report of a paged file's view timed with its
    # answer, by a listener the bridge is given once in the page's life
    conn.evaluate(
        "(async () => { if (window.__pressesHeard) return true; window.__pressesHeard = true; const b = await import(location.origin + '/bridge.js');"
        " window.__pressesOpen = new Map(); window.__pressesReports = []; let number = 0;"
        " b.onCalls((command, args, promise) => { const sent = performance.now(); const mine = number++;"
        " window.__pressesOpen.set(mine, [command, sent, JSON.stringify(args).slice(0, 160)]);"
        " promise.finally(() => window.__pressesOpen.delete(mine)).catch(() => {});"
        " if (command === 'pages_view') promise.then((answer) => window.__pressesReports.push([sent | 0, (performance.now() - sent) | 0, args.top, args.held_from, args.held_to,"
        " answer?.from ?? -1, answer?.lines?.length ?? 0, (answer?.lines ?? []).reduce((sum, line) => sum + line.length, 0)]), () => {}); });"
        " return true; })()",
        timeout=10,
    )
    began = conn.evaluate(
        "(async () => { const e = await import(location.origin + '/edit.js'); if (%d >= 0) await e.openAt(e.editing().active, %d, 0);"
        " const v = e.editing().editor; if (!v?.s) return null; v.focus(); window.__pressesRead = []; window.__pressesReports = [];"
        " const p = v.s.window?.paged; const now = performance.now();"
        " const answered = p ? await Promise.race([p.chain.then(() => true), new Promise((done) => setTimeout(() => done(false), 200))]) : null;"
        " return { path: e.editing().active, line: v.s.base + v.head().line, col: v.head().col, paged: Boolean(p), flying: p?.flying ?? 0, answered,"
        " open: [...window.__pressesOpen.values()].map(([command, sent, args]) => `${command} sent ${(now - sent) | 0} ms before: ${args}`) }; })()" % (a.line, a.line),
        timeout=60,
    )
    if not began:
        print("no editor with a file open in the window")
        return 1
    time.sleep(a.rest)
    for _ in range(a.count):
        harness_input.press(conn, key)
        conn.evaluate(
            "(async () => { const e = await import(location.origin + '/edit.js'); const v = e.editing().editor; const p = v.s.window?.paged;"
            " window.__pressesRead.push([performance.now() | 0, v.s.base + v.head().line, v.head().col, v.s.base, v.s.base + v.s.doc.count, p?.flying ?? null, p?.resting ?? null, p?.wanted ?? null, window.__pressesAnswered ?? null]);"
            " window.__pressesAnswered = p ? false : null; p?.chain.then(() => { window.__pressesAnswered = true; }); return 0; })()",
            timeout=10,
        )
    time.sleep(a.rest)
    read = conn.evaluate("(() => window.__pressesRead)()", timeout=10) or []
    reports = conn.evaluate("(() => window.__pressesReports)()", timeout=10) or []
    lines = ["%s from line %d, %s: %s pressed %d times" % (began["path"], began["line"], "paged" if began["paged"] else "held whole", key, a.count)]
    if began["paged"]:
        lines.append("as the presses began: %d reports in flight, the calls about the file before them %s" % (began["flying"], "answered" if began["answered"] else "not answered in 200 ms"))
    lines.append("calls unanswered as the presses began: %s" % ("; ".join(began["open"]) or "none"))
    # a press moves the cursor on the way it goes, a line or, where lines wrap, a row: one that leaves it where it was,
    # or sends it back, did not
    before = [(began["line"], began["col"])] + [(one[1], one[2]) for one in read[:-1]]
    odd = [(was, one) for was, one in zip(before, read) if ((one[1], one[2]) > was) != (step > 0) or (one[1], one[2]) == was]
    last = read[-1][1] if read else began["line"]
    lines.append("the cursor on line %d, %d lines on; %d presses that left it where it was or sent it back" % (last, (last - began["line"]) * step, len(odd)))
    for was, one in odd[: a.listed]:
        lines.append("  at %d ms from line %d col %d to line %d col %d: the window %d to %d, reports in flight %s, at rest %s, another wanted %s, the calls before them answered %s"
                     % ((one[0],) + was + tuple(one[1:])))
    if began["paged"]:
        lines.append("%d reports of where the view stands; the slowest:" % len(reports))
        for one in sorted(reports, key=lambda one: -one[1])[: a.listed]:
            lines.append("  sent at %d ms, answered in %d ms: top %d, the window %d to %d; lines from %d, %d of them, %d letters" % tuple(one))
    text = "\n".join(lines)
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    with open(os.path.join(HARNESS_BUILD, "window_presses.txt"), "w", encoding="utf-8") as out:
        out.write(text + "\n")
    print(text)
    return 1 if odd else 0


def cmd_edges(a):
    """The claim's edges benched behind --port: the editor given the keys so the claim holds, then for each edge, each
    stopping distance from it and each approach speed, --trials approaches from the window's middle that stop there;
    each trial whether the edge's part came within --wait milliseconds and how long after the pointer stopped, and
    for the controls' corner how many times the ghost went and came; to build/harness/window_edges.txt and the
    screen."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import statistics

    import harness_cdp
    import harness_input

    conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
    conn.call("Runtime.enable", timeout=10)
    size = conn.evaluate(
        "(async () => { const e = await import(location.origin + '/edit.js'); const v = e.editing().editor; if (!v) return null; v.focus();"
        " await new Promise((done) => setTimeout(done, 300)); return { width: window.innerWidth, height: window.innerHeight, claim: document.body.hasAttribute('data-claim') }; })()",
        timeout=10,
    )
    if not size or not size["claim"]:
        print("no editor holds the keys in the window and the claim does not hold: open a file and try again")
        return 1
    width, height = size["width"], size["height"]
    middle = (width / 2.0, height / 2.0)
    # where a trial stops, by the edge and the pixels from it; and the zone the claim names for that edge
    aims = {
        "top": lambda stop: (middle[0], stop),
        "bottom": lambda stop: (middle[0], height - stop),
        "left": lambda stop: (stop, middle[1]),
        "right": lambda stop: (width - stop, middle[1]),
        "controls": lambda stop: (width - 40, stop),
    }
    state = ("(() => ({ reveal: document.body.dataset.reveal ?? '', near: document.body.hasAttribute('data-near') }))()")

    def settled():
        # the pointer back in the middle and every part gone: the next trial starts from rest
        harness_input.mouse(conn, "mouseMoved", middle[0], middle[1], "none", 0, 0, 0)
        for _ in range(60):
            if not conn.evaluate(state, timeout=10)["reveal"]:
                break
            time.sleep(0.05)
        time.sleep(0.4)

    def trial(edge, stop, speed):
        settled()
        x, y = aims[edge](stop)
        # the approach: a move every STEP milliseconds, each as far as the speed carries it, the last landing on the aim
        step = 0.008
        dx, dy = x - middle[0], y - middle[1]
        far = max(1.0, (dx * dx + dy * dy) ** 0.5)
        at = 0.0
        while at < far:
            at = min(far, at + speed * step * 1000.0)
            harness_input.mouse(conn, "mouseMoved", middle[0] + dx * at / far, middle[1] + dy * at / far, "none", 0, 0, 0)
            time.sleep(step)
        stopped = time.monotonic()
        came = None
        flickers = 0
        near_was = None
        while time.monotonic() - stopped < a.wait / 1000.0:
            seen = conn.evaluate(state, timeout=10)
            if near_was is not None and seen["near"] != near_was:
                flickers += 1
            near_was = seen["near"]
            if seen["reveal"] == edge:
                came = (time.monotonic() - stopped) * 1000.0
                break
            time.sleep(0.016)
        return came, flickers

    edges = [one for one in a.edges.split(",") if one in aims]
    stops = [float(one) for one in a.stops.split(",")]
    speeds = [float(one) for one in a.speeds.split(",")]
    lines = ["the claim's edges, %d trials each, the part waited for %d ms after the pointer stops" % (a.trials, a.wait)]
    lines.append("%-9s %5s %8s   %7s  %9s  %s" % ("edge", "px", "px/ms", "came", "delay ms", "ghost went and came"))
    for edge in edges:
        for stop in stops:
            for speed in speeds:
                outcomes = [trial(edge, stop, speed) for _ in range(a.trials)]
                delays = [came for came, _ in outcomes if came is not None]
                flickers = sum(one for _, one in outcomes)
                lines.append("%-9s %5.0f %8.2f   %3d/%-3d  %9s  %s" % (edge, stop, speed, len(delays), a.trials, ("%.0f" % statistics.median(delays)) if delays else "-", flickers if edge == "controls" else ""))
    settled()
    text = "\n".join(lines)
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    with open(os.path.join(HARNESS_BUILD, "window_edges.txt"), "w", encoding="utf-8") as out:
        out.write(text + "\n")
    print(text)
    return 0


def cmd_heap(a):
    """The page's heap behind --port as a snapshot takes it, its garbage collected first: its objects by name, each
    name's count and the bytes its objects take themselves, the largest first, and the DOM nodes no document holds, to
    build/harness/window_heap.txt and the screen."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import json

    import harness_cdp

    conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
    chunks = []
    conn.on("HeapProfiler.addHeapSnapshotChunk", lambda params: chunks.append(params.get("chunk", "")))
    try:
        conn.call("HeapProfiler.enable", timeout=10)
        conn.call("HeapProfiler.collectGarbage", timeout=60)
        conn.call("HeapProfiler.takeHeapSnapshot", timeout=600, reportProgress=False, captureNumericValue=False)
    finally:
        conn.close()
    snapshot = json.loads("".join(chunks))
    meta = snapshot["snapshot"]["meta"]
    fields = meta["node_fields"]
    width = len(fields)
    name_at = fields.index("name")
    size_at = fields.index("self_size")
    detached_at = fields.index("detachedness") if "detachedness" in fields else -1
    strings = snapshot["strings"]
    nodes = snapshot["nodes"]
    by_name = {}
    detached = {}
    whole = 0
    for at in range(0, len(nodes), width):
        name = strings[nodes[at + name_at]]
        size = nodes[at + size_at]
        whole += size
        count, total = by_name.get(name, (0, 0))
        by_name[name] = (count + 1, total + size)
        if detached_at >= 0 and nodes[at + detached_at] == 2:
            count, total = detached.get(name, (0, 0))
            detached[name] = (count + 1, total + size)
    lines = ["the page's heap, its garbage collected: %d objects, %.1f MiB of their own" % (len(nodes) // width, whole / MIB)]
    lines.append("by name, the most bytes first:")
    for name, (count, total) in sorted(by_name.items(), key=lambda one: -one[1][1])[: a.count]:
        lines.append("  %9.1f KiB %8d  %s" % (total / 1024.0, count, name[:120]))
    lines.append("held by no document: %d objects, %.1f KiB" % (sum(one[0] for one in detached.values()), sum(one[1] for one in detached.values()) / 1024.0))
    for name, (count, total) in sorted(detached.items(), key=lambda one: -one[1][1])[: a.count]:
        lines.append("  %9.1f KiB %8d  %s" % (total / 1024.0, count, name[:120]))
    text = "\n".join(lines)
    os.makedirs(HARNESS_BUILD, exist_ok=True)
    with open(os.path.join(HARNESS_BUILD, "window_heap.txt"), "w", encoding="utf-8") as out:
        out.write(text + "\n")
    print(text)
    return 0


def cmd_zoom(a):
    if not windows_only():
        return 1
    import harness_win32 as win

    found = window_of(a.id)
    if not found:
        return 1
    _, _, rect = found
    path = os.path.join(HARNESS_BUILD, "window_zoom.png")
    rows = win.screen_pixels(rect[0] + a.x, rect[1] + a.y, a.width, a.height)
    win.png_written(path, win.rows_scaled(rows, a.scale))
    print("zoomed %d,%d %d x %d by %d: %s" % (a.x, a.y, a.width, a.height, a.scale, path))
    return 0


def cmd_dump(a):
    """The newest crash dump under build/harness/dumps, or the one --path names, read under cdb with the suite build's
    symbols: the faulting stack with its lines, to build/harness/window_dump.txt and the screen."""
    if not windows_only():
        return 1
    folder = os.path.join(HARNESS_BUILD, "dumps")
    dumps = sorted((os.path.join(folder, name) for name in os.listdir(folder) if name.endswith(".dmp")), key=os.path.getmtime) if os.path.isdir(folder) else []
    path = a.path or (dumps[-1] if dumps else "")
    if not path:
        print("no dump under %s: turn on Windows Error Reporting's LocalDumps for %s.exe into it" % (folder, gui_name()))
        return 1
    import harness_window_test as suite

    symbols = ";".join([os.path.join(suite.APP_TARGET, suite.APP_PROFILE), os.path.join(ROOT, "build", "cargo", "release"), os.path.join(ROOT, "build", "cargo", "debug")])
    log = os.path.join(HARNESS_BUILD, "window_dump.txt")
    with open(log, "wb") as out:
        subprocess.run([CDB, "-z", path, "-y", symbols, "-c", ".lines; .ecxr; kc 40; ~*kc 12; q"], cwd=ROOT, stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT)
    with open(log, "r", encoding="utf-8", errors="replace") as read:
        text = read.read()
    print("%s read under cdb: %s" % (path, log))
    print(text[text.find("ExceptionAddress") - 200 if "ExceptionAddress" in text else 0 :][:6000])
    return 0


def cmd_debug(a):
    if not windows_only():
        return 1
    log = os.path.join(HARNESS_BUILD, "window_debug.txt")
    release = os.path.join(ROOT, "build", "cargo", "release")
    windows_ended()
    with open(log, "wb") as out:
        process = subprocess.Popen([CDB, "-y", release, "-g", "-G", "-c", "g; .lines; kc 25; q", program()], cwd=ROOT, stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT)
        try:
            process.wait(a.seconds)
        except subprocess.TimeoutExpired:
            process.kill()
            windows_ended()
            print("no fault within %d s" % a.seconds)
    telling = re.compile(r"Access violation|orior|Call Site|^[0-9a-f]{2} ")
    with open(log, encoding="utf-8", errors="replace") as kept:
        lines = [line.rstrip("\n") for line in kept if telling.search(line)]
    print("\n".join(lines[:40]))
    return 0


def cmd_regions(a):
    """The window's own process's committed bytes by kind, private, mapped and image, and its largest committed
    allocations of each kind, every region of one allocation summed: where the process grows outside the cellblock
    and the Rust allocator, this says whether it grew in private heaps or in mapped files."""
    if not windows_only():
        return 1
    import harness_win32 as win

    found = window_of(a.id)
    if not found:
        return 1
    process_id = found[0]
    kinds = {win.MEM_PRIVATE: "private", win.MEM_MAPPED: "mapped", win.MEM_IMAGE: "image"}
    totals = {}
    allocations = {}
    for _, allocation, size, state, kind, _ in win.regions(process_id):
        if state != win.MEM_COMMIT:
            continue
        name = kinds.get(kind, "other")
        totals[name] = totals.get(name, 0) + size
        allocations[(name, allocation)] = allocations.get((name, allocation), 0) + size
    print("process %d committed: %s" % (process_id, ", ".join("%s %d MB" % (name, bytes_ >> 20) for name, bytes_ in sorted(totals.items()))))
    largest = sorted(allocations.items(), key=lambda one: -one[1])[: a.count]
    # a mapped allocation named by the file behind it; one the paging file backs has no name
    files = win.mapped_names(process_id, [allocation for (name, allocation), _ in largest if name == "mapped"])
    for (name, allocation), size in largest:
        print("  %-8s %18s %6d MB  %s" % (name, "0x%x" % allocation, size >> 20, files.get(allocation, "")))
    return 0


def kinds_on_port(port):
    """The window behind a debugging port and its web view's processes by kind, as the browser names them: the
    window's process id, and each process id's kind, the window's own named window."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import harness_cdp
    import harness_win32 as win

    conn = harness_cdp.Connection(harness_cdp.browser(port))
    try:
        listed = conn.call("SystemInfo.getProcessInfo", timeout=10).get("processInfo", [])
    finally:
        conn.close()
    kinds = {one["id"]: one["type"].lower() for one in listed}
    browser_id = next((process_id for process_id, kind in kinds.items() if kind == "browser"), 0)
    window_id = next((parent for process_id, parent, _ in win.processes() if process_id == browser_id), 0)
    kinds[window_id] = "window"
    return window_id, kinds


def page_memory(port):
    """The page's own memory behind a debugging port, its garbage collected first: its script heap and what Blink
    counts of its documents, nodes and listeners."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import harness_cdp

    conn = harness_cdp.Connection(harness_cdp.window_page(port))
    try:
        conn.call("HeapProfiler.collectGarbage", timeout=30)
        heap = conn.call("Runtime.getHeapUsage", timeout=10)
        counters = conn.call("Memory.getDOMCounters", timeout=10)
    finally:
        conn.close()
    return heap, counters


def memory_held(port):
    """What each of the web view's processes holds as the browser counts it behind its debugging port: a memory-infra
    dump taken through the browser's own target, each process's allocators at the dump's top level and the bytes each
    holds, by the process's id."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import json
    import urllib.request

    import harness_cdp

    with urllib.request.urlopen("http://127.0.0.1:%d/json/version" % port, timeout=5) as answer:
        browser = harness_cdp.Connection(json.load(answer)["webSocketDebuggerUrl"])
    events = []
    whole = threading.Event()
    browser.on("Tracing.dataCollected", lambda params: events.extend(params.get("value", [])))
    browser.on("Tracing.tracingComplete", lambda _params: whole.set())
    try:
        browser.call("Tracing.start", timeout=10, transferMode="ReportEvents",
                     traceConfig={"recordMode": "recordAsMuchAsPossible", "includedCategories": ["disabled-by-default-memory-infra"], "memoryDumpConfig": {"triggers": []}})
        browser.call("Tracing.requestMemoryDump", timeout=60, deterministic=True, levelOfDetail="detailed")
        browser.call("Tracing.end", timeout=10)
        whole.wait(60)
    finally:
        browser.close()
    held = {}
    for event in events:
        if event.get("ph") != "v":
            continue
        for name, entry in event.get("args", {}).get("dumps", {}).get("allocators", {}).items():
            size = entry.get("attrs", {}).get("size", {}).get("value")
            if "/" not in name and size is not None:
                held.setdefault(event.get("pid"), {})[name] = int(size, 16)
    return held


def cmd_memory(a):
    if not windows_only():
        return 1
    import harness_win32 as win

    if a.port and a.pressure:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import harness_cdp

        # the page told memory runs short, as the system tells a browser, and given time to give back what it can
        conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
        try:
            conn.call("Memory.simulatePressureNotification", timeout=10, level="critical")
        finally:
            conn.close()
        time.sleep(3)
    kinds = {}
    if a.port:
        try:
            root, kinds = kinds_on_port(a.port)
        except (ConnectionError, OSError, TimeoutError) as error:
            print(error)
            return 1
        roots = [root] if root else []
    else:
        wanted = program().lower()
        roots = [a.id] if a.id else [process_id for process_id, _, _ in win.processes() if win.image_path(process_id).lower() == wanted]
    if not roots:
        print("no window running")
        return 1
    every = win.processes()
    working_sum = private_sum = commit_sum = 0
    counted = 0
    for process_id in win.process_tree(roots[0]):
        said = win.memory(process_id)
        if said is None:
            continue
        private, working, commit = said
        name = kinds.get(process_id) or next((exe for other, _, exe in every if other == process_id), "?")
        print("%7d %-28s working %8.1f MiB  private %8.1f MiB  commit %8.1f MiB" % (process_id, name, working / MIB, private / MIB, commit / MIB))
        working_sum += working
        private_sum += private
        commit_sum += commit
        counted += 1
    print("sum working %.1f MiB, private %.1f MiB, commit %.1f MiB, %d processes" % (working_sum / MIB, private_sum / MIB, commit_sum / MIB, counted))
    if a.port:
        heap, counters = page_memory(a.port)
        print("page, its garbage collected: heap %.1f MiB used of %.1f MiB, %d documents, %d nodes, %d listeners" % (heap.get("usedSize", 0) / MIB, heap.get("totalSize", 0) / MIB, counters.get("documents", 0), counters.get("nodes", 0), counters.get("jsEventListeners", 0)))
    if a.port and a.infra:
        for process_id, allocators in sorted(memory_held(a.port).items(), key=lambda one: -sum(one[1].values())):
            name = kinds.get(process_id) or next((exe for other, _, exe in every if other == process_id), "?")
            largest = sorted(allocators.items(), key=lambda one: -one[1])
            print("%7s %-28s %8.1f MiB: %s" % (process_id, name, sum(allocators.values()) / MIB, ", ".join("%s %.1f MiB" % (allocator, size / MIB) for allocator, size in largest if size >= MIB / 10)))
    return 0


def cmd_cpu(a):
    if not windows_only():
        return 1
    script = os.path.join(ROOT, "utils", "test", "window_cpu.ps1")
    log = os.path.join(ROOT, "build", "harness", "window_cpu.txt")
    words = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-Minutes", str(a.minutes),
             "-Step", str(a.step), "-Spike", str(a.spike), "-Log", log, "-Symbols", os.path.dirname(program())]
    return subprocess.call(words)


def cmd_gpu(a):
    """The released window's process tree over --seconds: the processor time its processes used, and each second the
    share of the GPU's 3D engine they kept busy as the system counts it (typeperf over the GPU Engine counters), their
    mean and their most, to build/harness/window_gpu.txt."""
    if not windows_only():
        return 1
    import csv
    import harness_win32 as win

    process_id = a.id or pid_read()
    if not process_id or not win.alive(process_id):
        print("no window running: harness.py window start")
        return 1
    tree = win.process_tree(process_id)
    before = {one: win.processor_seconds(one) or 0.0 for one in tree}
    sampled = os.path.join(HARNESS_BUILD, "window_gpu.csv")
    subprocess.run(["typeperf", r"\GPU Engine(*)\Utilization Percentage", "-si", "1", "-sc", str(a.seconds), "-f", "CSV", "-o", sampled, "-y"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    each = {one: (win.processor_seconds(one) or 0.0) - before.get(one, 0.0) for one in win.process_tree(process_id)}
    used = sum(each.values())
    names = {one: exe for one, _, exe in win.processes()}
    named = re.compile(r"pid_(\d+)_.*engtype_3D\)")
    with open(sampled, encoding="utf-8", errors="replace", newline="") as kept:
        rows = list(csv.reader(kept))
    columns = [at for at, head in enumerate(rows[0]) if (found := named.search(head)) and int(found.group(1)) in tree] if rows else []
    seconds = []
    for row in rows[1:]:
        values = [row[at].strip() for at in columns if at < len(row)]
        if values and all(value for value in values):
            seconds.append(sum(float(value) for value in values))
    if not seconds:
        print("no GPU Engine sample named the window's processes")
        return 1
    lines = ["window %d, %d processes, %d s" % (process_id, len(tree), len(seconds)),
             "3D engine busy: mean %.2f%%, most %.2f%%" % (sum(seconds) / len(seconds), max(seconds)),
             "processor: %.2f s over %d s, %.1f%% of one core" % (used, a.seconds, 100.0 * used / a.seconds)]
    lines += ["  %7d %-28s %7.2f s" % (one, names.get(one, "?"), spent) for one, spent in sorted(each.items(), key=lambda kept: -kept[1]) if spent > 0.0]
    with open(os.path.join(HARNESS_BUILD, "window_gpu.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0


def cmd_in_job(a):
    if not windows_only():
        return 1
    import harness_win32 as win

    asked, inside = win.in_job(a.id)
    print("process %d asked %s in a job %s" % (a.id, asked, inside))
    return 0


STYLE = os.path.join(ROOT, "ui", "gui", "src-js", "style.css")


def srgb_channel(value):
    value /= 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def lightness(r, g, b):
    """CIE lightness, 0 to 100, of an sRGB color."""
    y = 0.2126 * srgb_channel(r) + 0.7152 * srgb_channel(g) + 0.0722 * srgb_channel(b)
    return 116.0 * (y ** (1.0 / 3.0)) - 16.0 if y > 0.008856 else 903.3 * y


def color_parsed(text):
    hexed = re.match(r"#([0-9a-fA-F]{6})\b", text)
    if hexed:
        value = hexed.group(1)
        return tuple(int(value[at : at + 2], 16) for at in (0, 2, 4))
    rgbed = re.match(r"rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)", text)
    if rgbed:
        return tuple(int(part) for part in rgbed.groups())
    return None


def cmd_luminance(a):
    """Every color token of the dark scheme ranked by its CIE lightness, from --floor down, with how many rules of
    the stylesheet read it and the first of them: the tokens standing as bright as the operators show beside what
    they paint."""
    with open(STYLE, encoding="utf-8") as kept:
        css = kept.read()
    root = css[: css.index('[data-scheme="light"]')]
    tokens = {}
    for name, value in re.findall(r"(--[\w-]+):\s*([^;]+);", root):
        color = color_parsed(value.strip())
        if color and name not in tokens:
            tokens[name] = (lightness(*color), value.strip())
    uses = {}
    for selector, body in re.findall(r"([^{}]+)\{([^{}]*)\}", css):
        for line in body.split(";"):
            for name in tokens:
                if "var(%s)" % name in line:
                    uses.setdefault(name, []).append("%s %s" % (selector.strip().split("\n")[-1].strip()[:48], line.split(":")[0].strip()))
    for name, (light, value) in sorted(tokens.items(), key=lambda one: -one[1][0]):
        if light < a.floor:
            break
        where = uses.get(name, [])
        print("%5.1f  %-22s %-26s %3d uses" % (light, name, value, len(where)))
        for one in where[: a.each]:
            print("           %s" % one)
    return 0


def register(sub):
    """The window group's commands added to harness.py's subparsers."""
    group = sub.add_parser("window", help="the released window started, ended, watched, pictured and measured (Windows)")
    window = group.add_subparsers(dest="command", required=True)

    p = window.add_parser("start", help="one window alone, placed on the display; or the process --id placed")
    p.add_argument("--display", default=DISPLAY, help="the display's device name")
    p.add_argument("--id", type=int, default=0, help="place this process's window instead of starting one")
    p.add_argument("--port", type=int, default=0, help="the started window's debugging port, none where 0")
    p.set_defaults(fn=cmd_start)

    p = window.add_parser("ended", help="every window running from the release program ended")
    p.set_defaults(fn=cmd_ended)
    p = window.add_parser("keys", help="keys sent to the window as real input, each named as the editor names a key")
    p.add_argument("names", nargs="+", help="the keys in order: Right, Mod+Z, Shift+End, or one character")
    p.add_argument("--id", type=int, default=0, help="the window's process, the started one where 0")
    p.set_defaults(fn=cmd_keys)
    p = window.add_parser("click", help="a press of the pointer posted to the window at a pixel of its client, dragged and released")
    p.add_argument("x", type=int)
    p.add_argument("y", type=int)
    p.add_argument("--to", type=int, nargs=2, metavar=("X", "Y"), help="where the press is dragged to before the release")
    p.add_argument("--count", type=int, default=1, help="how many presses in a row, two for a word, three for a line")
    p.add_argument("--id", type=int, default=0, help="the window's process, the started one where 0")
    p.set_defaults(fn=cmd_click)
    p = window.add_parser("move", help="the pointer moved to a pixel of the window's client with no button down")
    p.add_argument("x", type=int)
    p.add_argument("y", type=int)
    p.add_argument("--via", type=int, nargs="+", metavar="N", help="the places passed through on the way, x y pairs")
    p.add_argument("--pause", type=float, default=0.02, help="the seconds between one move and the next")
    p.add_argument("--id", type=int, default=0, help="the window's process, the started one where 0")
    p.set_defaults(fn=cmd_move)
    p = window.add_parser("check", help="the window's C checked without a display, every check where none is named")
    p.add_argument("names", nargs="*", help="checks by name, raster or glyphs, each a *_test.c under utils/test/src/ui/gui")
    p.add_argument("--sanitize", choices=("address", "thread"), default="", help="the checks built and run under this sanitizer, a report failing its check")
    p.add_argument("--owners", action="store_true", help="each check's end prints MMgr's owner table, every owner's bytes now and its peak")
    p.set_defaults(fn=cmd_check)
    p = window.add_parser("bench", help="the frame bench: a monitor-sized frame of the window drawn without a display, timed")
    p.set_defaults(fn=cmd_bench)
    p = window.add_parser("icon", help="the program's icon file written from the eye: ui/gui/src/orior.ico, built into the window on Windows")
    p.set_defaults(fn=cmd_icon)
    p = window.add_parser("session", help="the tree's kept session in orior's state: written from the tabs given, or printed")
    p.add_argument("tabs", nargs="*", help="the tabs' paths from the tree's top, in order")
    p.add_argument("--active", help="the tab shown; the last given where none")
    p.set_defaults(fn=cmd_session)

    p = window.add_parser("watch", help="started alone with its output kept, ended after --seconds where it still runs")
    p.add_argument("--seconds", type=int, default=25)
    p.set_defaults(fn=cmd_watch)

    p = window.add_parser("shot", help="the window's top-left pictured to build/harness/window_shot.png")
    p.add_argument("--id", type=int, default=0, help="the process; window.pid's where none")
    p.add_argument("--width", type=int, default=480)
    p.add_argument("--height", type=int, default=320)
    p.set_defaults(fn=cmd_shot)

    p = window.add_parser("eval", help="an expression run on the window's page through its debugging port, its answer printed")
    p.add_argument("--port", type=int, required=True, help="the window's debugging port, as window test --keep-window prints it")
    p.add_argument("--seconds", type=int, default=30, help="how long the answer is waited for")
    p.add_argument("expression")
    p.set_defaults(fn=cmd_eval)

    p = window.add_parser("type", help="text typed into the window through its debugging port, key by key, or one chord with --key")
    p.add_argument("--port", type=int, required=True, help="the window's debugging port")
    p.add_argument("--key", action="store_true", help="the text is one chord, as Ctrl+S or Enter")
    p.add_argument("text")
    p.set_defaults(fn=cmd_type)

    p = window.add_parser("drag", help="the editor's scrollbar dragged by its thumb through the window's debugging port, end to end and back")
    p.add_argument("--port", type=int, required=True, help="the window's debugging port")
    p.add_argument("--steps", type=int, default=120, help="the moves each way, a frame apart")
    p.add_argument("--rounds", type=int, default=1, help="how many times end to end and back")
    p.set_defaults(fn=cmd_drag)

    p = window.add_parser("profile", help="the page's processor time sampled while the editor's scrollbar is dragged: late frames and the "
                                           "functions that spent the most, to build/harness/window_profile.txt")
    p.add_argument("--port", type=int, required=True, help="the window's debugging port")
    p.add_argument("--steps", type=int, default=60, help="the drag's moves each way, a frame apart")
    p.add_argument("--no-drag", action="store_true", help="sampled for --seconds as the window stands, with no drag")
    p.add_argument("--wheel", type=int, default=0, help="the wheel turned this many notches down and as many up over the editor, a notch a frame, in place of the drag")
    p.add_argument("--delta", type=float, default=120.0, help="the pixels each notch of --wheel turns")
    p.add_argument("--seconds", type=float, default=5.0)
    p.add_argument("--interval", type=int, default=250, help="microseconds between samples")
    p.add_argument("--late", type=float, default=25.0, help="a frame later than this many milliseconds after the one before is late")
    p.add_argument("--count", type=int, default=30, help="how many functions are listed")
    p.add_argument("--trace", action="store_true", help="the browser's timeline taken as well: the renderer's main thread by the kind of work it did, "
                                                         "style, layout, paint, script and the rest, each kind's own time")
    p.set_defaults(fn=cmd_profile)

    p = window.add_parser("presses", help="Down or Up pressed in the editor as fast as the page takes them: each press that left the cursor where it "
                                          "was or sent it back, a paged file's window and reports as it stood, to build/harness/window_presses.txt")
    p.add_argument("--port", type=int, required=True, help="the window's debugging port")
    p.add_argument("--count", type=int, default=300, help="how many presses")
    p.add_argument("--line", type=int, default=-1, help="the line of the open file the cursor is put on first; where none, the line it stands on")
    p.add_argument("--up", action="store_true", help="Up pressed in place of Down")
    p.add_argument("--rest", type=float, default=2.0, help="seconds the window is let stand before the presses and after them")
    p.add_argument("--listed", type=int, default=20, help="how many presses and reports are listed")
    p.set_defaults(fn=cmd_presses)

    p = window.add_parser("edges", help="the claim's edges benched: approaches to each edge at each speed, stopping at each distance from it, "
                                        "how often the edge's part comes and how long after, to build/harness/window_edges.txt")
    p.add_argument("--port", type=int, required=True, help="the window's debugging port; the editor must hold a file")
    p.add_argument("--edges", default="top,bottom,left,right,controls", help="the edges tried, of top, bottom, left, right and controls")
    p.add_argument("--stops", default="4,12,20,28,36", help="the pixels from the edge each approach stops at")
    p.add_argument("--speeds", default="0.3,1,3", help="the speeds approached at, in pixels a millisecond")
    p.add_argument("--trials", type=int, default=5, help="approaches at each edge, stop and speed")
    p.add_argument("--wait", type=int, default=1500, help="milliseconds the part is waited for after the pointer stops")
    p.set_defaults(fn=cmd_edges)

    p = window.add_parser("heap", help="the page's heap as a snapshot takes it, its garbage collected first: its objects by name, the bytes each name "
                                       "takes, and the DOM nodes no document holds, to build/harness/window_heap.txt")
    p.add_argument("--port", type=int, required=True, help="the window's debugging port")
    p.add_argument("--count", type=int, default=30, help="how many names are listed")
    p.set_defaults(fn=cmd_heap)

    p = window.add_parser("zoom", help="a part of the window drawn larger to build/harness/window_zoom.png")
    p.add_argument("--id", type=int, default=0, help="the process; window.pid's where none")
    p.add_argument("--x", type=int, default=0)
    p.add_argument("--y", type=int, default=0)
    p.add_argument("--width", type=int, default=160)
    p.add_argument("--height", type=int, default=100)
    p.add_argument("--scale", type=int, default=5)
    p.set_defaults(fn=cmd_zoom)

    p = window.add_parser("dump", help="the newest crash dump under build/harness/dumps read under cdb, the faulting stack to build/harness/window_dump.txt")
    p.add_argument("--path", default="", help="a dump file; the newest under build/harness/dumps where none")
    p.set_defaults(fn=cmd_dump)

    p = window.add_parser("debug", help="run under cdb, its stack where it faults, to build/harness/window_debug.txt")
    p.add_argument("--seconds", type=int, default=45)
    p.set_defaults(fn=cmd_debug)

    p = window.add_parser("memory", help="the window's process tree, each one's working set, private working set and commit, "
                                          "and with --port each one's kind and the page's heap, its garbage collected")
    p.add_argument("--id", type=int, default=0, help="the window's process, as the suite's; the released window's where none")
    p.add_argument("--port", type=int, default=0, help="the debugging port of the window measured, as a kept test window's")
    p.add_argument("--pressure", action="store_true", help="with --port, the page told first that memory runs short, as the system tells a browser, "
                                                            "and measured once it has had time to give back what it can")
    p.add_argument("--infra", action="store_true", help="with --port, the browser's memory-infra dump as well: each web view process's memory by the allocators "
                                                         "that hold it, the largest first")
    p.set_defaults(fn=cmd_memory)

    p = window.add_parser("regions", help="the window's committed bytes by kind, private, mapped and image, and its largest allocations")
    p.add_argument("--id", type=int, default=0, help="the process; window.pid's where none")
    p.add_argument("--count", type=int, default=12, help="how many of the largest allocations are listed")
    p.set_defaults(fn=cmd_regions)

    p = window.add_parser("luminance", help="the dark scheme's color tokens by CIE lightness, from --floor down, with the rules that paint them")
    p.add_argument("--floor", type=float, default=78.0, help="the lightness below which tokens are left out")
    p.add_argument("--each", type=int, default=6, help="how many of a token's rules are listed")
    p.set_defaults(fn=cmd_luminance)

    p = window.add_parser("cpu", help="the window's processor time each --step s for --minutes, its busiest threads, "
                                       "and a stack of the busiest on a step past --spike ms, to build/harness/window_cpu.txt")
    p.add_argument("--minutes", type=int, default=2)
    p.add_argument("--step", type=int, default=5)
    p.add_argument("--spike", type=int, default=150)
    p.set_defaults(fn=cmd_cpu)

    p = window.add_parser("gpu", help="the window's process tree over --seconds, the GPU's 3D engine it kept busy and the "
                                       "processor time it used, to build/harness/window_gpu.txt")
    p.add_argument("--seconds", type=int, default=50, help="how long it is watched, the lattice's whole loop where none is given")
    p.add_argument("--id", type=int, default=0, help="the process; window.pid's where none")
    p.set_defaults(fn=cmd_gpu)

    p = window.add_parser("in-job", help="whether a process sits in a job")
    p.add_argument("--id", type=int, required=True)
    p.set_defaults(fn=cmd_in_job)

    import harness_motion
    import harness_record
    import harness_window_test

    harness_window_test.register(window)
    harness_motion.register(window)
    harness_record.register(window)
