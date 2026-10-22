# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The window's page as the suite drives it: the runner put into the page, each test file of ui/test/src loaded and
run with the page fresh for each, every step the runner asks for performed as real input, every file the page asks
for under /__harness/ served, and what the page threw kept. The same run through the debugging port, where the
window is WebView2, and through WebDriver, where it is WebKitGTK. Before the first file the window's comms are proven;
after each file every job it started is stopped."""

import base64
import json
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

import harness_frames  # noqa: E402
import harness_input  # noqa: E402

ROOT = findroot.root()
UI = findroot.at("ui")
SUITE = findroot.at("ui", "test")
WINDOW_FILES = findroot.at("ui", "gui", "src-js")
# how long a file's tests may run, and how long the window may take to start
FILE_WAIT = 1800
START_WAIT = 90
COMMS_WAIT = 30
TEARDOWN_WAIT = 20
STARTED = "!window.__harnessLeft && document.readyState === 'complete' && 'started' in document.documentElement.dataset"

# The expression proving the window's comms: a call answered in place, then a call answered later by its event. The
# definition is asked of a file no server holds; the event therefore carries an error where the link works, and only no event
# at all leaves the answer null.
COMMS = """(async () => {
  const { invoke, asked } = await import("/bridge.js");
  const began = performance.now();
  const call = await invoke("tree_files").then(() => null, (error) => `the call failed: ${error}`);
  const callMs = Math.round(performance.now() - began);
  const asking = performance.now();
  const event = await asked("lsp_definition", { path: "hello.py", line: 0, col: 0 }, 10000).then((answer) => (answer === null ? "no call-answered event came back in 10 s" : null), () => null);
  return { call, callMs, event, eventMs: Math.round(performance.now() - asking) };
})()"""
# The expression stopping every job the page's Run view holds as live, answering how many.
TEARDOWN = """(async () => {
  const { stopLive } = await import("/run.js");
  return await stopLive();
})()"""
# the page's side of the steps under WebDriver: each step the runner asks for kept until the harness takes it, and the
# file's end kept the same way
QUEUE = "const h = (window.__harnessDriven = { queue: [], end: null, wake: null }); window.__harnessCall = (payload) => { h.queue.push(payload); h.wake?.(); }; return true;"
RUN = "const h = window.__harnessDriven; const url = arguments[0]; (async () => { await import(url); return await window.__harness.run(); })().then((results) => { h.end = { results }; h.wake?.(); }, (error) => { h.end = { error: String(error?.stack || error) }; h.wake?.(); }); return true;"
NEXT = "const done = arguments[arguments.length - 1]; const h = window.__harnessDriven; const look = () => { if (h.queue.length) { h.wake = null; done({ step: h.queue.shift() }); } else if (h.end) { h.wake = null; done({ end: h.end }); } else { h.wake = look; } }; look();"


class Outcome:
    """A test's outcome: its name, whether it passed, how long it took, and why not."""

    def __init__(self, name, ok, ms=0, error=""):
        self.name, self.ok, self.ms, self.error = name, ok, ms, error

    def not_run(self):
        """Whether the test was not run on the window driven, for a step only another window can take."""
        return self.ok and self.error.startswith(harness_input.NOT_HERE)

    def passed(self):
        return self.ok and not self.not_run()


def served_as(spec, file):
    """Where `spec`, imported by the file at `file`, is served from: a module of the window from its own place, a file
    of the suite from under /__harness/; None where `spec` is no relative path."""
    if not spec.startswith(("./", "../")):
        return None
    path = os.path.normpath(os.path.join(os.path.dirname(file), spec))
    for folder, prefix in ((WINDOW_FILES, "/"), (UI, "/__harness/")):
        try:
            part = os.path.relpath(path, folder).replace("\\", "/")
        except ValueError:
            continue
        if not part.startswith(".."):
            return prefix + part
    return None


def rewrite_line(line, file):
    out = []
    rest = line
    while True:
        at = min((rest.find(quote) for quote in ('"', "'") if quote in rest), default=-1)
        if at < 0:
            break
        quote = rest[at]
        end = rest.find(quote, at + 1)
        if end < 0:
            break
        before = rest[:at].rstrip(" \t\n\r(")
        spec = rest[at + 1:end]
        out.append(rest[: at + 1])
        out.append((served_as(spec, file) or spec) if before.endswith(("from", "import")) else spec)
        out.append(quote)
        rest = rest[end + 1:]
    out.append(rest)
    return "".join(out)


def rewrite_imports(source, file):
    """The module at `file` with each import of a module by its path made the path the page serves it at: a test
    imports the very module the window runs. A line of a comment is left as it is."""
    lines = source.splitlines(keepends=True)
    return "".join(line if line.lstrip().startswith(("//", "/*", "*")) else rewrite_line(line, file) for line in lines)


def content_type(path):
    ending = os.path.splitext(path)[1]
    return {".js": "text/javascript; charset=utf-8", ".mjs": "text/javascript; charset=utf-8", ".json": "application/json; charset=utf-8", ".css": "text/css; charset=utf-8", ".html": "text/html; charset=utf-8"}.get(ending, "application/octet-stream")


class Page:
    """The page over the debugging port: its errors kept, whether it closed, its steps performed and its files served."""

    def __init__(self, conn, told_servers):
        self.conn = conn
        self.errors = []
        self.lock = threading.Lock()
        self.frames = harness_frames.Frames()
        self.told = told_servers
        conn.on("Runtime.bindingCalled", self.binding_called)
        conn.on("Fetch.requestPaused", self.fulfill)
        conn.on("Runtime.exceptionThrown", self.thrown)
        conn.on("Runtime.consoleAPICalled", self.console)
        conn.on("Log.entryAdded", self.logged)
        conn.on("Tracing.dataCollected", lambda params: self.frames.heard("Tracing.dataCollected", params))
        conn.on("Tracing.tracingComplete", lambda params: self.frames.heard("Tracing.tracingComplete", params))

    def closed(self):
        return self.conn.closed

    def kept(self, text):
        with self.lock:
            self.errors.append(text)

    def taken(self):
        with self.lock:
            errors, self.errors = self.errors, []
        return errors

    def binding_called(self, params):
        if params.get("name") != "__harnessCall":
            return
        step = json.loads(params.get("payload") or "{}")

        def performing():
            error = None
            try:
                harness_input.perform(self.conn, step, self.frames)
            except Exception as failed:  # noqa: BLE001 - whatever a step failed with is the test's to read
                error = str(failed)
            try:
                self.conn.evaluate("window.__harness?.answer(%s, %s)" % (json.dumps(step.get("id", 0)), json.dumps(error)), timeout=10)
            except (RuntimeError, TimeoutError, ConnectionError, OSError):
                pass

        threading.Thread(target=performing, daemon=True).start()

    def fulfill(self, params):
        """A request the page made under /__harness/ answered with the file of the suite it names."""
        url = params.get("request", {}).get("url", "")
        named = url.split("/__harness/", 1)[1].split("?")[0].split("#")[0] if "/__harness/" in url else ""
        path = os.path.normpath(os.path.join(UI, named))
        inside = path.startswith(os.path.join(UI, "test") + os.sep)
        code, body = 404, "the harness has no " + named
        if inside:
            try:
                with open(path, encoding="utf-8") as source:
                    text = source.read()
                code = 200
                body = rewrite_imports(text, path) if content_type(path).startswith("text/javascript") else text
            except OSError:
                pass
        headers = [{"name": "Content-Type", "value": content_type(path)}, {"name": "Cache-Control", "value": "no-store"}]
        self.conn.notify("Fetch.fulfillRequest", requestId=params.get("requestId", ""), responseCode=code, responseHeaders=headers, body=base64.b64encode(body.encode("utf-8")).decode())

    def thrown(self, params):
        details = params.get("exceptionDetails", {})
        self.kept("threw " + (details.get("exception", {}).get("description") or details.get("text") or "an error"))

    def console(self, params):
        if params.get("type") in ("error", "assert"):
            words = [arg.get("value") if isinstance(arg.get("value"), str) else arg.get("description") or json.dumps(arg.get("value")) for arg in params.get("args", [])]
            self.kept("console.error " + " ".join(words))

    def logged(self, params):
        entry = params.get("entry", {})
        if entry.get("level") == "error":
            self.kept(("%s %s %s" % (entry.get("source", "log"), entry.get("text", ""), entry.get("url", ""))).strip())

    def wait_started(self):
        until = time.monotonic() + START_WAIT
        while time.monotonic() < until:
            if self.closed():
                raise RuntimeError("the window closed")
            try:
                if self.conn.evaluate(STARTED, timeout=10) is True:
                    return
            except (RuntimeError, TimeoutError):
                pass
            time.sleep(0.1)
        raise RuntimeError("the window did not finish starting in %d s" % START_WAIT)

    def ready(self):
        """The port's domains the run needs turned on: events, the page, the log, the binding the runner calls, the
        files served under /__harness/, and the page told it has the focus."""
        for method in ("Runtime.enable", "Page.enable", "Log.enable"):
            self.conn.call(method, timeout=10)
        self.conn.call("Runtime.addBinding", timeout=10, name="__harnessCall")
        self.conn.call("Fetch.enable", timeout=10, patterns=[{"urlPattern": "*/__harness/*", "requestStage": "Request"}])
        self.conn.call("Emulation.setFocusEmulationEnabled", timeout=10, enabled=True)

    def evaluate(self, expression, wait):
        return self.conn.evaluate(expression, timeout=wait)

    def leave(self):
        """The page left and loaded fresh: marked as left, as the reload answers before the page it loads is there."""
        try:
            self.conn.evaluate("window.__harnessLeft = true", timeout=10)
            self.conn.call("Page.reload", timeout=30, ignoreCache=True)
        except (RuntimeError, TimeoutError, ConnectionError):
            pass

    def begin(self, runner):
        self.wait_started()
        self.conn.evaluate(runner, timeout=10)
        self.conn.evaluate("window.__harness.servers = %s" % json.dumps(self.told), timeout=10)
        self.taken()

    def run_file(self, file):
        """The file loaded and run; its outcomes."""
        expression = "(async () => { await import(%s); return await window.__harness.run(); })()" % json.dumps("/__harness/test/" + file)
        results = self.conn.evaluate(expression, timeout=FILE_WAIT)
        if not isinstance(results, list):
            return [Outcome("the file", False, 0, "the run gave %s" % json.dumps(results))]
        return outcomes_of(results)

    def release(self):
        return


def as_async_script(expression):
    """`expression` as a WebDriver async script: its promise's value handed to the script's callback, a rejection as
    `{ error }`."""
    return "const done = arguments[arguments.length - 1]; (%s).then(done, (error) => done({ error: String(error) }));" % expression


class DrivenPage:
    """The page over WebDriver: the same run, the steps taken from the page's own queue and the errors read from what
    catch.js kept."""

    def __init__(self, session, told_servers):
        self.session = session
        self.told = told_servers
        self.frames = None

    def closed(self):
        try:
            self.session.url()
            return False
        except RuntimeError:
            return True

    def wait_started(self):
        until = time.monotonic() + START_WAIT
        said = ""
        while time.monotonic() < until:
            try:
                if self.session.execute("return %s;" % STARTED, []) is True:
                    return
            except RuntimeError as error:
                said = str(error)
            time.sleep(0.1)
        raise RuntimeError("the window did not finish starting in %d s%s" % (START_WAIT, ": " + said if said else ""))

    def evaluate(self, expression, wait):
        value = self.session.execute_async(as_async_script(expression), [], wait)
        if isinstance(value, dict) and isinstance(value.get("error"), str):
            raise RuntimeError(value["error"])
        return value

    def leave(self):
        try:
            self.session.execute("window.__harnessLeft = true; location.reload(); return true;", [])
        except RuntimeError:
            pass

    def begin(self, runner):
        self.wait_started()
        kept = [line for line in runner.splitlines() if not line.lstrip().startswith("//# sourceURL")]
        self.session.execute("return (\n%s\n);" % "\n".join(kept).rstrip().rstrip(";"), [])
        self.session.execute(QUEUE, [])
        self.session.execute("window.__harness.servers = arguments[0]; if (window.__harnessErrors) window.__harnessErrors.length = 0; return true;", [self.told])

    def run_file(self, file):
        self.session.execute(RUN, ["/__harness/test/" + file])
        while True:
            answer = self.session.execute_async(NEXT, [], FILE_WAIT)
            if isinstance(answer.get("step"), str):
                step = json.loads(answer["step"])
                error = None
                try:
                    harness_input.perform_driven(self.session, step)
                except Exception as failed:  # noqa: BLE001 - whatever a step failed with is the test's to read
                    error = str(failed)
                self.session.execute("window.__harness.answer(arguments[0], arguments[1]); return true;", [step.get("id"), error])
                continue
            end = answer.get("end") or {}
            if isinstance(end.get("error"), str):
                raise RuntimeError("the file did not load: " + end["error"])
            return outcomes_of(end.get("results") or [])

    def taken(self):
        try:
            kept = self.session.execute("return window.__harnessErrors || [];", [])
            return [one for one in kept if isinstance(one, str)]
        except RuntimeError:
            return []

    def release(self):
        try:
            self.session.release()
        except RuntimeError:
            pass


def outcomes_of(results):
    return [Outcome(str(one.get("name", "")), one.get("ok") is True, int(one.get("ms") or 0), str(one.get("error", ""))) for one in results if isinstance(one, dict)]


def comms_outcome(page):
    """The window's comms proven: a call answered, and an asked call's event back; what did not come, else ok."""
    name = "the window answers a call, and an asked call's event comes back"
    try:
        value = page.evaluate(COMMS, COMMS_WAIT)
    except (RuntimeError, TimeoutError, ConnectionError) as error:
        return Outcome(name, False, 0, "the expression did not run: %s" % error)
    if not isinstance(value, dict):
        return Outcome(name, False, 0, "the expression gave %s" % json.dumps(value))
    said = [value.get(key) for key in ("error", "call", "event") if isinstance(value.get(key), str)]
    return Outcome(name, not said, int(value.get("callMs") or 0) + int(value.get("eventMs") or 0), "; ".join(said))


def torn_down(page):
    """Every job the file started stopped; how many, or the error that kept them running."""
    value = page.evaluate(TEARDOWN, TEARDOWN_WAIT)
    if isinstance(value, dict) and isinstance(value.get("error"), str):
        raise RuntimeError(value["error"])
    return int(value or 0)


def run_files(page, files, report):
    """Each test file run in the page, fresh for each; the first file waits on the comms proven, and a window that
    answers no call runs none."""
    with open(os.path.join(SUITE, "runner.js"), encoding="utf-8") as source:
        runner = source.read()
    for at, file in enumerate(files):
        if page.closed():
            report.files.append((file, [Outcome("the file", False, 0, "the window closed before it ran")], []))
            continue
        if at > 0:
            page.leave()
        outcomes = []
        try:
            page.begin(runner)
            if at == 0:
                comms = comms_outcome(page)
                print("  the window: %s" % ("answers, %d ms" % comms.ms if comms.ok else comms.error), flush=True)
                if not comms.ok:
                    report.files.append(("the window", [comms], []))
                    return
            outcomes = page.run_file(file)
            if not outcomes:
                outcomes = [Outcome("the file", False, 0, "the file gave no test")]
        except (RuntimeError, TimeoutError, ConnectionError) as error:
            outcomes.append(Outcome("the file", False, 0, "the file did not load: %s" % error))
        try:
            stopped = torn_down(page)
            if stopped:
                print("  %s: %d job%s it started stopped" % (file, stopped, "" if stopped == 1 else "s"), flush=True)
        except (RuntimeError, TimeoutError, ConnectionError) as error:
            outcomes.append(Outcome("the jobs the file started are stopped", False, 0, str(error)))
        errors = page.taken()
        threw = [one for one in errors if one.startswith("threw")]
        if threw:
            outcomes.append(Outcome("the page threw nothing", False, 0, "\n".join(threw)))
        failed = sum(1 for one in outcomes if not one.ok)
        not_run = sum(1 for one in outcomes if one.not_run())
        print("  %s: %d passed, %d failed, %d not run on this window" % (file, len(outcomes) - failed - not_run, failed, not_run), flush=True)
        for one in outcomes:
            if not one.ok:
                print("    FAILED %s: %s" % (one.name, one.error.splitlines()[0] if one.error else ""), flush=True)
        report.files.append((file, outcomes, errors))
        page.release()
