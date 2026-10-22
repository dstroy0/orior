# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# A window of the sweep's own: the released window started on a tree of dummy files, with a home, a
# web view profile and a debugging port of its own, so a sweep neither reads nor changes the
# reader's orior and runs beside an orior already open. On Windows it is put on the display the
# sweep is given, \\.\DISPLAY3 unless another is named.

import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cdp  # noqa: E402
import walk  # noqa: E402

HERE = Path(__file__).resolve().parent
TOP = HERE.parents[1]
GUI = TOP / "build" / "cargo" / "release" / ("orior_gui.exe" if os.name == "nt" else "orior_gui")
HARNESS = TOP / "utils" / "test" / "harness.py"


def free_port():
    """A port nothing listens on, of those the system gives out."""
    with socket.socket() as one:
        one.bind(("127.0.0.1", 0))
        return one.getsockname()[1]


def browser_arguments(port):
    """The browser's arguments the window's settings give it, the debugging port at `port` after them."""
    settings = json.loads((HERE.parent / "gui" / "src-tauri" / "tauri.conf.json").read_text(encoding="utf-8"))
    given = settings["app"]["windows"][0].get("additionalBrowserArgs", "")
    return f"{given} --remote-debugging-port={port}".strip()


class Window:
    """The sweep's window: its process, its port, its home and its tree, under `run`."""

    def __init__(self, run, display):
        self.run = run
        self.home = run / "home"
        self.tree = run / "tree"
        self.home.mkdir(parents=True, exist_ok=True)
        self.tree.mkdir(parents=True, exist_ok=True)
        walk.make_tree(self.tree)
        self.display = display
        self.log = open(run / "window.log", "ab")
        self.start()

    def start(self):
        """The window started on its port, its page waited for, and put on its display."""
        run = self.run
        self.port = free_port()
        self.env = dict(os.environ)
        self.env.update(
            {
                "ORIOR_HOME": str(self.home),
                "ORIOR_ROOT": str(self.tree),
                "ORIOR_NO_REPORTS": "1",
                "WEBVIEW2_USER_DATA_FOLDER": str(run / "webview"),
                "WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS": browser_arguments(self.port),
            }
        )
        self.process = subprocess.Popen([str(GUI)], cwd=self.tree, env=self.env, stdin=subprocess.DEVNULL, stdout=self.log, stderr=self.log)
        until = time.time() + 120
        self.page = None
        while time.time() < until and self.page is None:
            if self.process.poll() is not None:
                raise SystemExit(f"the window closed as it started; its log is {run / 'window.log'}")
            try:
                self.page = cdp.window_page(self.port)
            except ConnectionError:
                time.sleep(0.2)
        if self.page is None:
            self.close()
            raise SystemExit(f"the window's page did not open in 120 s; its log is {run / 'window.log'}")
        if os.name == "nt":
            subprocess.run([sys.executable, str(HARNESS), "window", "start", "--display", self.display, "--id", str(self.process.pid)], stdin=subprocess.DEVNULL, capture_output=True)

    def ended(self):
        """The window's process ended, its exit code where it has ended on its own, else None."""
        code = self.process.poll()
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=30)
        return code

    def restarted(self):
        """The window ended and started again on the same home and tree: the exit code it had ended with, or None
        where it was still running."""
        code = self.ended()
        self.start()
        return code

    def close(self):
        self.ended()
        self.log.close()
