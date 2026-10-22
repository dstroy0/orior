# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The window driven through WebDriver, where its web view is WebKitGTK and has no debugging port: on Linux, and in
the harness's Linux image. A session of the harness's own is spoken over HTTP to WebKitWebDriver: scripts run in the
page and answered as JSON, and input sent as WebDriver's actions. The window is built from a page of the harness's
staging: the window's files as they are, catch.js first in the page's head, and every file of the suite under
/__harness/, each import made the path the page serves it at. WebKitWebDriver starts the window through a launch
script with a home, a tree and a log of the run's own."""

import http.client
import json
import os
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
SUITE = findroot.at("ui", "test")
WINDOW_FILES = findroot.at("ui", "gui", "src-js")
STAGING = findroot.at("build", "ui_test", "driven", "page")
DRIVEN_TARGET = findroot.at("build", "ui_test", "driven", "app")
WAIT = 30


def request(port, method, path, body=None, wait=WAIT):
    """`method` sent to `path` at the loopback's `port` with `body`; what the answer's JSON holds under value, or the
    error it names raised."""
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=wait)
    try:
        text = json.dumps(body) if body is not None else ""
        connection.request(method, path, body=text.encode() if text else None, headers={"Content-Type": "application/json; charset=utf-8", "Content-Length": str(len(text.encode()))})
        answer = connection.getresponse().read().decode(errors="replace")
    except (OSError, http.client.HTTPException) as error:
        raise RuntimeError("WebDriver %s %s: %s" % (method, path, error)) from None
    finally:
        connection.close()
    try:
        value = json.loads(answer).get("value")
    except ValueError as error:
        raise RuntimeError("WebDriver %s %s answered %s: %s" % (method, path, answer[:200], error)) from None
    if isinstance(value, dict) and isinstance(value.get("error"), str):
        raise RuntimeError("WebDriver %s %s: %s: %s" % (method, path, value["error"], value.get("message", "")))
    return value


def answers(port):
    try:
        request(port, "GET", "/status", wait=2)
        return True
    except RuntimeError:
        return False


class Session:
    """One WebDriver session on the window."""

    def __init__(self, port, binary):
        asked = {"capabilities": {"alwaysMatch": {"webkitgtk:browserOptions": {"binary": binary, "args": []}}}}
        opened = request(port, "POST", "/session", asked, wait=180)
        self.port = port
        self.id = opened.get("sessionId") if isinstance(opened, dict) else None
        if not self.id:
            raise RuntimeError("WebDriver gave no session")
        self.command("POST", "/timeouts", {"script": 1800000, "pageLoad": 120000})

    def command(self, method, path, body=None, wait=WAIT):
        return request(self.port, method, "/session/%s%s" % (self.id, path), body if method not in ("GET", "DELETE") else None, wait)

    def execute(self, script, args):
        """`script`, a function's body given `args`, run in the page; what it returns."""
        return self.command("POST", "/execute/sync", {"script": script, "args": args})

    def execute_async(self, script, args, wait):
        """`script` run with a callback as its last argument; what it is called with, within `wait` seconds."""
        return self.command("POST", "/execute/async", {"script": script, "args": args}, wait)

    def actions(self, sources):
        self.command("POST", "/actions", {"actions": sources})

    def release(self):
        self.command("DELETE", "/actions")

    def url(self):
        return self.command("GET", "/url")

    def end(self):
        try:
            request(self.port, "DELETE", "/session/%s" % self.id)
        except RuntimeError:
            pass


def with_catch(index):
    """The page with catch.js first in its head, ahead of every script it loads."""
    at = index.find("<head>")
    if at < 0:
        return index
    at += len("<head>")
    return index[:at] + '\n    <script src="/__harness/catch.js"></script>' + index[at:]


def files_under(folder, ending):
    found = []
    for dirpath, _, names in os.walk(folder):
        for name in names:
            if name.endswith(ending):
                found.append(os.path.relpath(os.path.join(dirpath, name), folder).replace("\\", "/"))
    return sorted(found)


def written_if_changed(path, data):
    try:
        with open(path, "rb") as held:
            if held.read() == data:
                return False
    except OSError:
        pass
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as out:
        out.write(data)
    return True


def stage():
    """The page staged: the window's files with catch.js first in the page's head, and the suite's files under
    __harness/, each import made the path the page serves it at; what the stage kept that the page no longer has
    taken away."""
    import harness_page

    kept = set()
    for file in files_under(WINDOW_FILES, ""):
        with open(os.path.join(WINDOW_FILES, file), "rb") as source:
            data = source.read()
        if file == "index.html":
            data = with_catch(data.decode("utf-8")).encode("utf-8")
        written_if_changed(os.path.join(STAGING, file), data)
        kept.add(file)
    with open(os.path.join(SUITE, "catch.js"), "rb") as source:
        written_if_changed(os.path.join(STAGING, "__harness", "catch.js"), source.read())
    kept.add("__harness/catch.js")
    top_files = [name for name in os.listdir(SUITE) if name.endswith(".js") and os.path.isfile(os.path.join(SUITE, name))]
    for file in sorted(top_files) + ["src/" + name for name in files_under(os.path.join(SUITE, "src"), ".js")]:
        path = os.path.join(SUITE, file)
        with open(path, encoding="utf-8") as source:
            text = source.read()
        shown = "__harness/test/" + file
        written_if_changed(os.path.join(STAGING, shown), harness_page.rewrite_imports(text, path).encode("utf-8"))
        kept.add(shown)
    for file in files_under(STAGING, ""):
        if file not in kept:
            os.remove(os.path.join(STAGING, file))


def build(gui_name):
    """The page staged and the window built with it in place of its own files; the program built."""
    stage()
    env = dict(os.environ, TAURI_CONFIG=json.dumps({"build": {"frontendDist": STAGING}}))
    built = subprocess.run(["cargo", "build", "--manifest-path", findroot.at("ui", "gui", "src-tauri", "Cargo.toml"), "--target-dir", DRIVEN_TARGET], cwd=ROOT, env=env, stdin=subprocess.DEVNULL)
    if built.returncode != 0:
        raise RuntimeError("the window did not build")
    return os.path.join(DRIVEN_TARGET, "debug", gui_name + (".exe" if os.name == "nt" else ""))


def quoted(text):
    return "'" + text.replace("'", "'\\''") + "'"


def launch_script(program, run, tree, extra):
    """The script WebKitWebDriver starts the window through: in the run's tree, with its home, the settings `extra`
    gives, and the web view's automation allowed, its output in the run's log."""
    settings = [("ORIOR_HOME", os.path.join(run, "home")), ("ORIOR_ROOT", tree), ("ORIOR_NO_REPORTS", "1"), ("TAURI_WEBVIEW_AUTOMATION", "true")] + list(extra)
    exports = "\n".join("export %s=%s" % (key, quoted(value)) for key, value in settings)
    return "#!/bin/sh\n# The window, as WebKitWebDriver starts it for the harness; the words it is given are WebDriver's own.\ncd %s\n%s\nexec %s >> %s 2>&1\n" % (quoted(tree), exports, quoted(program), quoted(os.path.join(run, "window.log")))


def free_port():
    with socket.socket() as one:
        one.bind(("127.0.0.1", 0))
        return one.getsockname()[1]


class Driven:
    """The window open under WebDriver, closed with its driver when ended."""

    def __init__(self, driver, session):
        self.driver = driver
        self.session = session

    def close(self):
        self.session.end()
        self.driver.kill()
        self.driver.wait()


def start(program, run, tree, extra):
    """WebKitWebDriver started and a session opened on the window `program` gives, with `run` as the run's folder,
    `tree` as its tree and the settings `extra` besides."""
    launch = os.path.join(run, "launch.sh")
    with open(launch, "w", encoding="utf-8", newline="\n") as out:
        out.write(launch_script(program, run, tree, extra))
    os.chmod(launch, 0o755)
    port = free_port()
    log = open(os.path.join(run, "webdriver.log"), "wb")
    driver = subprocess.Popen(["WebKitWebDriver", "--port=%d" % port], stdin=subprocess.DEVNULL, stdout=log, stderr=log)
    until = time.monotonic() + 30
    while not answers(port):
        if time.monotonic() >= until or driver.poll() is not None:
            driver.kill()
            raise RuntimeError("WebKitWebDriver did not answer; its log is %s" % os.path.join(run, "webdriver.log"))
        time.sleep(0.1)
    try:
        return Driven(driver, Session(port, launch))
    except RuntimeError as error:
        driver.kill()
        driver.wait()
        raise RuntimeError("%s; the window's log is %s" % (error, os.path.join(run, "window.log"))) from None
