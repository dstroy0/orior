# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py window: the released window started, ended, watched, pictured and measured, on Windows.

    harness.py window start [--display \\\\.\\DISPLAY3] [--id N]   one window alone, placed on the display
    harness.py window ended                                      every window running from the release program ended
    harness.py window watch [--seconds 25]                       started alone with its output kept, ended after
    harness.py window shot [--id N] [--width 480] [--height 320] its top-left pictured to build/harness/window_shot.png
    harness.py window zoom [--x --y --width --height --scale]    a part drawn larger to build/harness/window_zoom.png
    harness.py window frames [--id N] [--every 40] [--for 1200]  pictured whole every --every ms into build/harness/frames,
                             [--port P --trigger JS] [--start]   an expression run first, or started and pictured from
                                                                 its first moment
    harness.py window dump [--path FILE]                         the newest crash dump read under cdb, the faulting stack
    harness.py window eval --port P EXPRESSION                   an expression run on the window's page, its answer printed
    harness.py window type --port P [--key] TEXT                 text typed into the window key by key, or one chord
    harness.py window regions [--id N] [--count 12]              committed bytes by kind and the largest allocations
    harness.py window debug [--seconds 45]                       run under cdb, the stack where it faults
    harness.py window memory                                     its process tree's private working set and commit
    harness.py window luminance [--floor 78] [--each 6]          the dark scheme's tokens by lightness, with their rules
    harness.py window cpu [--minutes 2] [--step 5] [--spike 150] its processor time each step, its busiest threads, a
                                                                 stack of the busiest past the spike, to window_cpu.txt
    harness.py window in-job --id N                              whether a process sits in a job
    harness.py window test [arg...]                              the window's suite, ui/test/src (harness_window_test.py)

The window's program is build/cargo/release/<gui>.exe, gui as bin_names.tsv names it. Every start ends the windows
running from that program first; one window therefore runs at a time, and its process id is kept in build/harness/window.pid
for the commands that read it back."""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
HARNESS_BUILD = os.path.join(ROOT, "build", "harness")
PID_FILE = os.path.join(HARNESS_BUILD, "window.pid")
CDB = r"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
DISPLAY = r"\\.\DISPLAY3"
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


def program():
    return os.path.join(ROOT, "build", "cargo", "release", gui_name() + ".exe")


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
    title_wanted = gui_name()
    for _ in range(PLACE_TRIES):
        time.sleep(PLACE_POLL_S)
        handle, title = win.main_window(process_id)
        if not handle or title != title_wanted:
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


def window_started(stdout=None, stderr=None):
    """The release window started alone, from the repository's top, its id kept."""
    windows_ended()
    process = subprocess.Popen([program()], cwd=ROOT, stdin=subprocess.DEVNULL, stdout=stdout or subprocess.DEVNULL, stderr=stderr or subprocess.DEVNULL)
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
        process_id = window_started().pid
    return 0 if window_placed(process_id, a.display) else 1


def cmd_ended(a):
    if not windows_only():
        return 1
    print("ended %d" % windows_ended())
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
    program = os.path.join(suite.APP_TARGET, "debug", gui_name() + ".exe")
    process = subprocess.Popen([program], cwd=tree, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    until = time.monotonic() + 60
    while time.monotonic() < until and not win.main_window(process.pid)[0]:
        time.sleep(0.05)
    window_placed(process.pid, DISPLAY)
    return process


def cmd_frames(a):
    """The window pictured whole every --every milliseconds for --for milliseconds, each frame a PNG under
    build/harness/frames, numbered; where --port and --trigger are given, the expression runs on the window's page
    through its debugging port as the first frame is taken: the frames show what it sets in motion. With --start
    the suite's build of the window is started on the fixture tree and pictured from its first moment, and ended
    after."""
    if not windows_only():
        return 1
    import harness_win32 as win

    started = frames_window_started() if a.start else None
    found = window_of(started.pid if started else a.id)
    if not found:
        return 1
    process_id, _, rect = found
    handle, _ = win.main_window(process_id)
    left, top, right, bottom = win.window_rect(handle)
    folder = os.path.join(HARNESS_BUILD, "frames")
    for old in os.listdir(folder) if os.path.isdir(folder) else []:
        os.remove(os.path.join(folder, old))
    os.makedirs(folder, exist_ok=True)
    if a.port and a.trigger:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import harness_cdp

        conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
        conn.call("Runtime.enable", timeout=10)
        conn.evaluate(a.trigger, timeout=10)
    began = time.monotonic()
    grabbed = []
    while (time.monotonic() - began) * 1000.0 < a.seconds_for:
        grabbed.append((int((time.monotonic() - began) * 1000.0), win.screen_pixels(left, top, right - left, bottom - top)))
        time.sleep(a.every / 1000.0)
    if started:
        started.kill()
        started.wait()
    crop = [int(one) for one in a.crop.split(",")] if a.crop else None
    for count, (at, rows) in enumerate(grabbed):
        if crop:
            rows = [row[crop[0] * 3 : (crop[0] + crop[2]) * 3] for row in rows[crop[1] : crop[1] + crop[3]]]
            rows = win.rows_scaled(rows, a.scale)
        win.png_written(os.path.join(folder, "%03d_%04dms.png" % (count, at)), rows)
    print("window %d at %d,%d to %d,%d, %d frames every %d ms: %s" % (process_id, left, top, right, bottom, len(grabbed), a.every, folder))
    return 0


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
    symbols = ";".join([os.path.join(ROOT, "build", "cargo", "app", "debug"), os.path.join(ROOT, "build", "cargo", "release"), os.path.join(ROOT, "build", "cargo", "debug")])
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
    for (name, allocation), size in sorted(allocations.items(), key=lambda one: -one[1])[: a.count]:
        print("  %-8s %18s %6d MB" % (name, "0x%x" % allocation, size >> 20))
    return 0


def cmd_memory(a):
    if not windows_only():
        return 1
    import harness_win32 as win

    wanted = program().lower()
    roots = [process_id for process_id, _, _ in win.processes() if win.image_path(process_id).lower() == wanted]
    if not roots:
        print("no window running")
        return 1
    private_sum = commit_sum = 0
    counted = 0
    for process_id in win.process_tree(roots[0]):
        said = win.memory(process_id)
        if said is None:
            continue
        private, _, commit = said
        name = next((exe for other, _, exe in win.processes() if other == process_id), "?")
        print("%7d %-28s working %8.1f MiB  commit %8.1f MiB" % (process_id, name, private / MIB, commit / MIB))
        private_sum += private
        commit_sum += commit
        counted += 1
    print("sum working %.1f MiB, commit %.1f MiB, %d processes" % (private_sum / MIB, commit_sum / MIB, counted))
    return 0


def cmd_cpu(a):
    if not windows_only():
        return 1
    script = os.path.join(ROOT, "utils", "test", "window_cpu.ps1")
    log = os.path.join(ROOT, "build", "harness", "window_cpu.txt")
    words = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-Minutes", str(a.minutes),
             "-Step", str(a.step), "-Spike", str(a.spike), "-Log", log, "-Symbols", os.path.dirname(program())]
    return subprocess.call(words)


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
    p.set_defaults(fn=cmd_start)

    p = window.add_parser("ended", help="every window running from the release program ended")
    p.set_defaults(fn=cmd_ended)

    p = window.add_parser("watch", help="started alone with its output kept, ended after --seconds where it still runs")
    p.add_argument("--seconds", type=int, default=25)
    p.set_defaults(fn=cmd_watch)

    p = window.add_parser("shot", help="the window's top-left pictured to build/harness/window_shot.png")
    p.add_argument("--id", type=int, default=0, help="the process; window.pid's where none")
    p.add_argument("--width", type=int, default=480)
    p.add_argument("--height", type=int, default=320)
    p.set_defaults(fn=cmd_shot)

    p = window.add_parser("frames", help="the window pictured whole every --every ms for --for ms, to build/harness/frames, an expression run on its page first")
    p.add_argument("--id", type=int, default=0, help="the process; window.pid's where none")
    p.add_argument("--every", type=int, default=40)
    p.add_argument("--for", dest="seconds_for", type=int, default=1200, help="milliseconds")
    p.add_argument("--port", type=int, default=0, help="the window's debugging port, for --trigger")
    p.add_argument("--trigger", default="", help="an expression run on the page as the first frame is taken")
    p.add_argument("--start", action="store_true", help="the suite's build started on the fixture tree and pictured from its first moment")
    p.add_argument("--crop", default="", help="LEFT,TOP,WIDTH,HEIGHT of the window kept of each frame, drawn --scale times larger")
    p.add_argument("--scale", type=int, default=4)
    p.set_defaults(fn=cmd_frames)

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

    p = window.add_parser("memory", help="the window's process tree, each one's private working set and commit")
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

    p = window.add_parser("in-job", help="whether a process sits in a job")
    p.add_argument("--id", type=int, required=True)
    p.set_defaults(fn=cmd_in_job)

    import harness_window_test

    harness_window_test.register(window)
