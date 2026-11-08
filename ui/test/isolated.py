# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# A window of the sweep's own: the released window started on a tree of dummy files, with a home of its own, so a
# sweep neither reads nor changes the reader's orior. On Windows it is put on the display the sweep is given,
# \\.\DISPLAY1 unless another is named, and its pixels are read off the screen there.

import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils" / "test"))
import harness_window  # noqa: E402
import walk  # noqa: E402

HERE = Path(__file__).resolve().parent
TOP = HERE.parents[1]
GUI = Path(harness_window.program())

# The seconds the window is given to show its frame once started
SHOWN_S = 120


class Window:
    """The sweep's window: its process, its home and its tree, under `run`."""

    def __init__(self, run, display):
        self.run = run
        self.home = run / "home"
        self.tree = run / "tree"
        self.home.mkdir(parents=True, exist_ok=True)
        self.tree.mkdir(parents=True, exist_ok=True)
        walk.make_tree(self.tree)
        self.display = display
        self.log = open(run / "window.log", "ab")
        self.env = dict(os.environ)
        self.env.update({"ORIOR_HOME": str(self.home), "ORIOR_ROOT": str(self.tree), "ORIOR_NO_REPORTS": "1"})
        self.start()

    def start(self):
        """The window started on the sweep's home and tree, its frame waited for, and put on its display."""
        self.process = subprocess.Popen([str(GUI)], cwd=self.tree, env=self.env, stdin=subprocess.DEVNULL, stdout=self.log, stderr=self.log)
        if os.name != "nt":
            return
        import harness_win32 as win

        until = time.time() + SHOWN_S
        self.handle = None
        while time.time() < until and not self.handle:
            if self.process.poll() is not None:
                raise SystemExit(f"the window closed as it started; its log is {self.run / 'window.log'}")
            self.handle, _ = win.main_window(self.process.pid)
            time.sleep(0.2)
        if not self.handle:
            self.close()
            raise SystemExit(f"the window showed no frame in {SHOWN_S} s; its log is {self.run / 'window.log'}")
        harness_window.window_placed(self.process.pid, self.display)
        self.handle, _ = win.main_window(self.process.pid)

    def alive(self):
        return self.process.poll() is None

    def keys(self, *names):
        """The keys named sent to the window as real input, in order."""
        if os.name == "nt" and self.alive():
            import harness_win32 as win

            win.keys_sent(self.handle, list(names))

    def shot(self, path):
        """The window's whole frame read off the screen into the PNG at `path`."""
        if os.name != "nt" or not self.alive():
            return False
        import harness_win32 as win

        left, top, right, bottom = win.window_rect(self.handle)
        win.png_written(str(path), win.screen_pixels(left, top, right - left, bottom - top))
        return True

    def ended(self):
        """The window's process ended, its exit code where it has ended on its own, else None."""
        code = self.process.poll()
        if code is None:
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
