# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# A window of the sweep's own: the released window started on a tree of dummy files, with a home of its own, so a
# sweep neither reads nor changes the reader's orior. On Windows it is put on the display the sweep is given,
# \\.\DISPLAY1 unless another is named, and its pixels are read off the screen there.

import json
import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "utils" / "test"))
import harness_window  # noqa: E402

HERE = Path(__file__).resolve().parent
TOP = HERE.parents[1]
GUI = Path(harness_window.program())

# The seconds the window is given to show its frame once started
DISPLAYED_S = 120

# The dummy files of the sweep's tree
DUMMIES = {
    "main.py": '"""A module the sweep opens."""\n\nimport os\n\n\ndef greet(name):\n    """Say hello."""\n    message = f"hello {name}"\n    # TODO: say it louder\n    return message\n\n\nclass Counter:\n    def __init__(self):\n        self.count = 0\n\n    def add(self, by=1):\n        self.count += by\n        return self.count\n\n\nif __name__ == "__main__":\n    print(greet(os.environ.get("USER", "sweep")))\n',
    "app.js": "export function sum(values) {\n  let total = 0;\n  for (const value of values) {\n    total += value;\n  }\n  return total;\n}\n\nconsole.log(sum([1, 2, 3]));\n",
    "lib.rs": "pub fn twice(n: i32) -> i32 {\n    n * 2\n}\n\n#[cfg(test)]\nmod tests {\n    #[test]\n    fn doubles() {\n        assert_eq!(super::twice(2), 4);\n    }\n}\n",
    "README.md": "# Sweep\n\nA tree the sweep makes.\n\n- one\n- two\n\n```python\nprint('hi')\n```\n",
    "data.json": '{\n  "name": "sweep",\n  "items": [1, 2, 3],\n  "nested": {"on": true}\n}\n',
    "table.csv": "id,name,score\n1,ada,90\n2,bob,85\n3,cy,77\n",
    "style.css": "body {\n  margin: 0;\n  color: #222;\n}\n",
    "index.html": "<!doctype html>\n<html>\n  <head><link rel=\"stylesheet\" href=\"style.css\"></head>\n  <body><h1>Sweep</h1></body>\n</html>\n",
    "query.sql": "SELECT 1;\n\nSELECT name, score FROM people WHERE score > 80;\n",
    "test_main.py": "from main import greet\n\n\ndef test_greet():\n    assert greet('a') == 'hello a'\n",
    "sub/inner.txt": "a file one folder down\n",
    ".gitignore": "__pycache__/\n",
    "sweep.code-workspace": '{"folders": [{"path": "."}]}\n',
    "notes.ipynb": json.dumps({"cells": [{"cell_type": "markdown", "id": "m1", "metadata": {}, "source": ["# Notes"]}, {"cell_type": "code", "execution_count": None, "id": "c1", "metadata": {}, "outputs": [], "source": ["1 + 1"]}], "metadata": {"kernelspec": {"display_name": "Python 3", "language": "python", "name": "python3"}}, "nbformat": 4, "nbformat_minor": 5}, indent=1),
}


def make_tree(folder):
    """The sweep's tree: the dummy files, committed to a repository of its own, one changed since."""
    for name, text in DUMMIES.items():
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
    git = ["git", "-c", "user.name=sweep", "-c", "user.email=sweep@localhost", "-c", "commit.gpgsign=false"]
    subprocess.run(git + ["init", "-q", "-b", "main"], cwd=folder, check=True)
    subprocess.run(git + ["add", "-A"], cwd=folder, check=True)
    subprocess.run(git + ["commit", "-q", "-m", "first"], cwd=folder, check=True)
    with open(folder / "main.py", "a", encoding="utf-8", newline="\n") as handle:
        handle.write("\n\ndef added():\n    return 1\n")


class Window:
    """The sweep's window: its process, its home and its tree, under `run`."""

    def __init__(self, run, display):
        self.run = run
        self.home = run / "home"
        self.tree = run / "tree"
        self.home.mkdir(parents=True, exist_ok=True)
        self.tree.mkdir(parents=True, exist_ok=True)
        make_tree(self.tree)
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

        until = time.time() + DISPLAYED_S
        self.handle = None
        while time.time() < until and not self.handle:
            if self.process.poll() is not None:
                raise SystemExit(f"the window closed as it started; its log is {self.run / 'window.log'}")
            self.handle, _ = win.main_window(self.process.pid)
            time.sleep(0.2)
        if not self.handle:
            self.close()
            raise SystemExit(f"the window showed no frame in {DISPLAYED_S} s; its log is {self.run / 'window.log'}")
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
