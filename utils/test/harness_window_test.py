# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py window test: the window driven as the command line drives it. ui/test/cli_sweep.py starts the released
window on a tree and a home of its own on the display --monitor numbers, DISPLAY1 where none is, and sends it every
command and option of impero, each command that acts in the window given its settle and pictured; --only retains the
forms holding its text. The run fails where the window ends under a form, a form crashes, or one does not end in its
time.

    harness.py window test [--only TEXT]... [--monitor N] [--monitors]

The report goes to build/ui_test/cli_sweep/<time>/cli_sweep.md."""

import argparse
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
SUITE = findroot.at("ui", "test")


def table(path):
    with open(path, encoding="utf-8") as retained:
        head = retained.readline().rstrip("\n").split("\t")
        rows = [line.rstrip("\n").split("\t") for line in retained if line.strip()]
        return [dict(zip(head, cells + [""] * (len(head) - len(cells)))) for cells in rows]


def bin_named(wanted):
    for row in table(findroot.at("bin_names.tsv")):
        if row.get("id") == wanted:
            return row["name"]
    raise SystemExit("bin_names.tsv names no %s" % wanted)


def options_of(words):
    parser = argparse.ArgumentParser(prog="harness.py window test", add_help=True)
    parser.add_argument("--only", action="append", default=[], help="the forms whose words hold this text")
    parser.add_argument("--monitor", type=int, default=int(os.environ.get("ORIOR_TEST_MONITOR") or 0), help="the display's number, as the system's settings give it")
    parser.add_argument("--monitors", action="store_true", help="the displays listed, by the number --monitor takes")
    return parser.parse_args(words)


def copy_tree(frm, to):
    shutil.copytree(frm, to, dirs_exist_ok=True)


def window_program(gui_name, sanitizer=""):
    """The program ui/cli/build.sh window writes for this machine, under `sanitizer` where one is named."""
    import harness_window

    folder = harness_window.host_target() + ("-" + sanitizer if sanitizer else "")
    return findroot.at("build", "cli", folder, gui_name + (".exe" if os.name == "nt" else ""))


def window_built(gui_name, sanitizer=""):
    """The window built by harness.py build gui, under `sanitizer` where one is named; or the program
    ORIOR_TEST_GUI names."""
    given = os.environ.get("ORIOR_TEST_GUI")
    if given and os.path.isfile(given):
        return given
    import harness_build

    if sanitizer:
        os.environ["ORIOR_SANITIZE"] = sanitizer
    if harness_build.cmd_build_gui(None) != 0:
        raise RuntimeError("the window did not build")
    return window_program(gui_name, sanitizer)


def test_windows_ended(program):
    """Every window running from the test program ended, on Windows: the one a run started and any a test opened
    from it, as New Window does, which outlive the run and hold the program file against the next build; how many."""
    if os.name != "nt":
        return 0
    import harness_win32 as win

    wanted = os.path.abspath(program).lower()
    running = [process_id for process_id, _, exe in win.processes() if win.image_path(process_id).lower() == wanted]
    for process_id in running:
        win.ended(process_id)
    return len(running)


def cli_driven(options):
    """The window in C driven as the command line drives it: the released window built, then ui/test/cli_sweep.py run on
    a window of its own on the display --monitor numbers, every command and option of the command line sent to it and
    each that acts in the window pictured. Fails where the window ends under a form, a form crashes, or one does not end
    in its time."""
    import json

    import harness_window

    window_built(bin_named("gui"))
    harness_window.windows_ended()
    display = "\\\\.\\DISPLAY%d" % options.monitor if options.monitor else "\\\\.\\DISPLAY1"
    words = [sys.executable, "-I", os.path.join(SUITE, "cli_sweep.py"), "--display", display]
    for only in options.only or []:
        words += ["--only", only]
    code = subprocess.run(words, stdin=subprocess.DEVNULL).returncode
    runs = findroot.at("build", "ui_test", "cli_sweep")
    latest = max((os.path.join(runs, one) for one in os.listdir(runs)), key=os.path.getmtime) if os.path.isdir(runs) else ""
    retained = os.path.join(latest, "cli_sweep.json")
    if code or not os.path.isfile(retained):
        print("the sweep did not end with its report (exit %d)" % code)
        return 1
    with open(retained, encoding="utf-8") as handle:
        records = json.load(handle)
    broken = [one for one in records if "window_ended" in one or one.get("late") or (isinstance(one.get("code"), int) and (one["code"] < 0 or one["code"] >= 0xC0000000))]
    print("window test: %d forms, %d where the window ended, crashed or did not end; %s" % (len(records), len(broken), os.path.join(latest, "cli_sweep.md")))
    return 1 if broken else 0


def cmd_window_test(a):
    words = [word for word in a.extra if word != "--"]
    options = options_of(words)
    if options.monitors:
        if os.name != "nt":
            print("the displays are listed on Windows alone")
            return 1
        import harness_win32 as win

        for device, (left, top, right, bottom) in sorted(win.displays().items()):
            print("  %s  %dx%d at %d,%d" % (device, right - left, bottom - top, left, top))
        return 0
    return cli_driven(options)


def register(window):
    """The sweep's command, added to the window group."""
    p = window.add_parser("test", help="the command line's commands swept over a window of its own; --help lists its options")
    # the sweep's options are the command's own: main passes them over unparsed, in their order
    p.set_defaults(fn=cmd_window_test, takes_extra=True)
