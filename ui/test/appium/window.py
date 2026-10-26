# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The window as a person uses it on macOS, through Appium's Mac2 driver: it starts and shows its parts, Go to File
opens a file of the tree in a tab of its own, the command palette opens on its keys and closes on Escape, and what is
typed into a file and saved is in the file. The page binds Ctrl, not Command, on every system."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))), "utils", "test"))

from harness_appium import CONTROL, ESCAPE, RETURN, SHIFT  # noqa: E402


def opened(window, path):
    """`path` opened through Go to File, waited on until its tab shows its name."""
    window.chord("p", CONTROL)
    window.click(window.by_label("Quick open"))
    window.typed(path)
    window.keys([RETURN])
    window.showing("tabs", os.path.basename(path))


def test_starts(window):
    """the window starts and shows the strip, the editor and the status bar"""
    for identifier in ("strip", "editor", "statusbar"):
        window.by_id(identifier)


def test_go_to_file(window):
    """Go to File, Ctrl+P, opens hello.py, and its tab shows its name"""
    opened(window, "hello.py")


def test_palette(window):
    """the command palette opens on Ctrl+Shift+P and closes on Escape"""
    window.chord("p", CONTROL | SHIFT)
    window.by_label("Quick open")
    window.keys([ESCAPE])
    window.gone("xpath", "//*[@label='Quick open']")


def test_typed_and_saved(window):
    """text typed into hello.py and saved with Ctrl+S is in the file"""
    opened(window, "hello.py")
    window.click(window.by_id("editor"))
    window.typed("# typed through Mac2")
    window.keys([RETURN])
    window.chord("s", CONTROL)
    window.until(lambda: "# typed through Mac2" in window.file_text("hello.py"), "hello.py does not hold the text typed and saved")
