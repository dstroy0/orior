# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The window driven through Appium's Mac2 driver, where its web view is WKWebView, on macOS: every press and key is
XCTest's own input, and the window is read through its accessibility tree, where WebKit gives each element of the page
its HTML id as its identifier, an element's text as its value and its aria-label as its label. The window is built as
it ships and wrapped as an application of the run's own; Appium is started with its log in the run's folder, and each
file of ui/test/appium runs its tests on a session of its own, the window started fresh for each with the run's home
and tree. Appium and its driver come from npm: npm install -g appium, then appium driver install mac2."""

import http.client
import importlib.util
import json
import os
import plistlib
import shutil
import socket
import subprocess
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

SUITE = findroot.at("ui", "test", "appium")
APP = findroot.at("build", "ui_test", "appium", "orior_test.app")
BUNDLE_ID = "com.dstroy0.orior.test"
# how long a request may take, how long a session may take to start, the first of which builds XCTest's agent, and how
# long a test waits for what it looks for
WAIT = 60
SESSION_WAIT = 900
LOOK_WAIT = 30
# the key the W3C protocol names an element by
ELEMENT = "element-6066-11e4-a52e-4f735466cecf"

# XCUIKeyModifierFlags, as macos: keys takes them
SHIFT = 1 << 17
CONTROL = 1 << 18
OPTION = 1 << 19
COMMAND = 1 << 20

# XCUIKeyboardKey's special keys, as macos: keys takes them
RETURN = "\r"
ESCAPE = "\x1b"
TAB = "\t"
DELETE = "\x08"


def request(port, method, path, body=None, wait=WAIT):
    """`method` sent to `path` at the loopback's `port` with `body`; what the answer's JSON holds under value, or the
    error it names raised."""
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=wait)
    try:
        text = json.dumps(body) if body is not None else ""
        connection.request(method, path, body=text.encode() if text else None, headers={"Content-Type": "application/json; charset=utf-8", "Content-Length": str(len(text.encode()))})
        answer = connection.getresponse().read().decode(errors="replace")
    except (OSError, http.client.HTTPException) as error:
        raise RuntimeError("Appium %s %s: %s" % (method, path, error)) from None
    finally:
        connection.close()
    try:
        value = json.loads(answer).get("value")
    except ValueError as error:
        raise RuntimeError("Appium %s %s answered %s: %s" % (method, path, answer[:200], error)) from None
    if isinstance(value, dict) and isinstance(value.get("error"), str):
        raise RuntimeError("Appium %s %s: %s: %s" % (method, path, value["error"], value.get("message", "")))
    return value


def answers(port):
    try:
        request(port, "GET", "/status", wait=2)
        return True
    except RuntimeError:
        return False


def free_port():
    with socket.socket() as one:
        one.bind(("127.0.0.1", 0))
        return one.getsockname()[1]


def quoted_xpath(text):
    """`text` as an XPath string literal, whichever quotes it holds."""
    if "'" not in text:
        return "'%s'" % text
    if '"' not in text:
        return '"%s"' % text
    return "concat(%s)" % ", \"'\", ".join("'%s'" % part for part in text.split("'"))


class Window:
    """One session on the window: its elements found by identifier, by label and by the text they show, pressed and
    typed into with XCTest's own input, and waited on; and the run's tree, whose files the tests read."""

    def __init__(self, port, app, environment, tree):
        capabilities = {"platformName": "mac", "appium:automationName": "mac2", "appium:bundleId": BUNDLE_ID, "appium:appPath": app, "appium:environment": environment, "appium:serverStartupTimeout": SESSION_WAIT * 1000}
        opened = request(port, "POST", "/session", {"capabilities": {"alwaysMatch": capabilities}}, wait=SESSION_WAIT)
        self.port = port
        self.id = opened.get("sessionId") if isinstance(opened, dict) else None
        if not self.id:
            raise RuntimeError("Appium gave no session")
        self.tree = tree

    def command(self, method, path, body=None, wait=WAIT):
        return request(self.port, method, "/session/%s%s" % (self.id, path), body if method not in ("GET", "DELETE") else None, wait)

    def execute(self, method, args):
        """One of Mac2's macos: methods, given `args`; what it answers."""
        return self.command("POST", "/execute/sync", {"script": method, "args": [args]})

    def found(self, using, value):
        """The first element `using` finds by `value`, or None."""
        elements = self.command("POST", "/elements", {"using": using, "value": value})
        return elements[0][ELEMENT] if elements else None

    def waited(self, using, value, wait=LOOK_WAIT):
        """The first element `using` finds by `value`, waited on for `wait` seconds."""
        until = time.monotonic() + wait
        while True:
            element = self.found(using, value)
            if element is not None:
                return element
            if time.monotonic() >= until:
                raise AssertionError("no element is found by %s %s in %d s" % (using, value, wait))
            time.sleep(0.25)

    def by_id(self, identifier, wait=LOOK_WAIT):
        """The element of the page whose HTML id is `identifier`."""
        return self.waited("accessibility id", identifier, wait)

    def by_label(self, label, wait=LOOK_WAIT):
        """The element whose label, its aria-label in the page, is `label`."""
        return self.waited("xpath", "//*[@label=%s]" % quoted_xpath(label), wait)

    def showing(self, identifier, text, wait=LOOK_WAIT):
        """An element inside the one whose HTML id is `identifier` that shows `text` as its value, title or label."""
        said = quoted_xpath(text)
        return self.waited("xpath", "//*[@identifier=%s]//*[contains(@value, %s) or contains(@title, %s) or contains(@label, %s)]" % (quoted_xpath(identifier), said, said, said), wait)

    def gone(self, using, value, wait=LOOK_WAIT):
        """Waits until no element `using` finds by `value` is displayed."""
        until = time.monotonic() + wait
        while self.found(using, value) is not None:
            if time.monotonic() >= until:
                raise AssertionError("an element found by %s %s is still displayed after %d s" % (using, value, wait))
            time.sleep(0.25)

    def click(self, element):
        self.execute("macos: click", {"elementId": element})

    def keys(self, keys):
        """Each key of `keys` pressed in turn: a character, one of the special keys, or {key, modifierFlags}."""
        self.execute("macos: keys", {"keys": keys})

    def chord(self, key, flags):
        self.keys([{"key": key, "modifierFlags": flags}])

    def typed(self, text):
        self.keys(list(text))

    def file_text(self, path):
        with open(os.path.join(self.tree, path), encoding="utf-8", errors="replace") as handle:
            return handle.read()

    def until(self, holds, said, wait=LOOK_WAIT):
        """Waits until `holds()` is true, or fails saying `said`."""
        until = time.monotonic() + wait
        while not holds():
            if time.monotonic() >= until:
                raise AssertionError("%s, after %d s" % (said, wait))
            time.sleep(0.25)

    def end(self):
        try:
            request(self.port, "DELETE", "/session/%s" % self.id)
        except RuntimeError:
            pass


def application(program):
    """The window's program wrapped as the run's own application, by its bundle identifier, where Mac2 starts it."""
    contents = os.path.join(APP, "Contents")
    shutil.rmtree(APP, ignore_errors=True)
    os.makedirs(os.path.join(contents, "MacOS"))
    name = os.path.basename(program)
    shutil.copy2(program, os.path.join(contents, "MacOS", name))
    properties = {"CFBundleIdentifier": BUNDLE_ID, "CFBundleExecutable": name, "CFBundleName": "orior test", "CFBundlePackageType": "APPL", "CFBundleShortVersionString": "0.0.0", "NSHighResolutionCapable": True}
    with open(os.path.join(contents, "Info.plist"), "wb") as out:
        plistlib.dump(properties, out)
    return APP


class Driven:
    """Appium running, its log in the run's folder, and stopped when ended."""

    def __init__(self, run):
        self.port = free_port()
        self.log = open(os.path.join(run, "appium.log"), "wb")
        self.server = subprocess.Popen(["appium", "--port", str(self.port), "--log-no-colors"], stdin=subprocess.DEVNULL, stdout=self.log, stderr=self.log)
        until = time.monotonic() + 60
        while not answers(self.port):
            if time.monotonic() >= until or self.server.poll() is not None:
                self.close()
                raise RuntimeError("Appium did not answer; its log is %s" % os.path.join(run, "appium.log"))
            time.sleep(0.25)

    def close(self):
        self.server.terminate()
        try:
            self.server.wait(timeout=30)
        except subprocess.TimeoutExpired:
            self.server.kill()
            self.server.wait()
        self.log.close()


def suite_files(only):
    """The files of ui/test/appium, each holding tests, those whose names hold a text of `only` where it names any."""
    names = sorted(name for name in os.listdir(SUITE) if name.endswith(".py") and not name.startswith("_"))
    return [name for name in names if not only or any(one in name for one in only)]


def tests_of(path):
    """The tests a file holds, in the order it gives them: each function named test_, its name the first line of
    its docstring."""
    spec = importlib.util.spec_from_file_location("appium_" + os.path.splitext(os.path.basename(path))[0], path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    found = [value for name, value in vars(module).items() if name.startswith("test_") and callable(value)]
    return sorted(found, key=lambda test: test.__code__.co_firstlineno)


def run_files(driven, app, environment, tree, only, report, outcome):
    """Each file of the suite run on a session of its own, its outcomes put in `report` as `outcome` makes them."""
    for name in suite_files(only):
        outcomes = []
        try:
            tests = tests_of(os.path.join(SUITE, name))
            window = Window(driven.port, app, environment, tree)
        except (RuntimeError, OSError, SyntaxError, ImportError) as error:
            report.files.append(("appium/" + name, [outcome("the file", False, 0, str(error))], []))
            continue
        try:
            for test in tests:
                said = (test.__doc__ or test.__name__).strip().splitlines()[0]
                began = time.monotonic()
                try:
                    test(window)
                    outcomes.append(outcome(said, True, int((time.monotonic() - began) * 1000)))
                except Exception:  # noqa: BLE001 - whatever a test failed with is its outcome
                    outcomes.append(outcome(said, False, int((time.monotonic() - began) * 1000), traceback.format_exc(limit=4)))
                print("  %s %s" % ("ok  " if outcomes[-1].ok else "FAIL", said), flush=True)
        finally:
            window.end()
        report.files.append(("appium/" + name, outcomes, []))
