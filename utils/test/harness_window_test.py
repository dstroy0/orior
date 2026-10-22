# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py window test: the window's suite, ui/test/src, each module of the window tested by its own file, run in
a window of the run's own, started on a copy of ui/test/fixtures/tree with a home of its own, its page loaded fresh
for each file, and every press, key, drag and wheel sent as real input. The window is driven through its debugging
port where its web view is WebView2, on Windows, and through WebDriver where it is WebKitGTK, on Linux; --docker runs
the suite in the harness's Linux image on this machine.

    harness.py window test [--only TEXT]... [--driver cdp|webdriver] [--docker] [--sanitize address|thread]
                           [--monitor N] [--monitors] [--no-servers] [--keep-window] [--stop-servers]

--sanitize builds the window's C side and links the window under that sanitizer, with clang's runtime, and the window
runs with its reports written to sanitizer.<pid> in the run's folder; a report fails the run. On Windows the released
compiler's runtime is not linked by cargo: the option runs on Linux, in the image or on the machine.

Before the suite, the system is read: each program the tests drive found on PATH with its version, the run's folder
writable, the display set where one is needed. Before the first file the window's comms are proven, and after each
file every job it started is stopped. The test servers the files reach are started as window_servers.tsv names them
and told to the tests. The report goes to build/ui_test/runs/<time>/report.md; the run fails where any test fails,
the page throws, or a module of the window has no test file."""

import argparse
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

import harness_cdp  # noqa: E402
import harness_input  # noqa: E402
import harness_page  # noqa: E402
import harness_servers  # noqa: E402
import harness_webdriver  # noqa: E402

ROOT = findroot.root()
SUITE = findroot.at("ui", "test")
RUNS = findroot.at("build", "ui_test", "runs")
BENCH_FOLDER = findroot.at("build", "ui_test", "bench")
QRY_FOLDER = findroot.at("build", "ui_test", "qry")
APP_TARGET = findroot.at("build", "cargo", "app")
WINDOW_FILES = findroot.at("ui", "gui", "src-js")
PROGRAMS_TABLE = findroot.at("utils", "test", "window_programs.tsv")
BENCH_TABLE = findroot.at("utils", "test", "window_bench.tsv")
IMAGES_TABLE = findroot.at("utils", "test", "window_images.tsv")
# the file made for the paged tests: how many lines, how many bytes each
MADE = "huge.log"
MADE_LINES = 3000000
MADE_WIDTH = 100
WATCH = "http://localhost:6080/vnc.html?autoconnect=true&resize=scale"


def table(path):
    with open(path, encoding="utf-8") as kept:
        head = kept.readline().rstrip("\n").split("\t")
        rows = [line.rstrip("\n").split("\t") for line in kept if line.strip()]
        return [dict(zip(head, cells + [""] * (len(head) - len(cells)))) for cells in rows]


def bin_named(wanted):
    for row in table(findroot.at("bin_names.tsv")):
        if row.get("id") == wanted:
            return row["name"]
    raise SystemExit("bin_names.tsv names no %s" % wanted)


def files_under(folder, ending):
    found = []
    for dirpath, _, names in os.walk(folder):
        for name in names:
            if name.endswith(ending):
                found.append(os.path.relpath(os.path.join(dirpath, name), folder).replace("\\", "/"))
    return sorted(found)


def stamp():
    return time.strftime("%Y-%m-%d-%H%M%S", time.gmtime())


class Report:
    """What each part of the run found."""

    def __init__(self):
        self.servers = []
        self.files = []
        self.untested = []
        self.orphans = []
        self.said = []

    def outcomes(self):
        return [one for _, outcomes, _ in self.files for one in outcomes]

    def failed(self):
        return sum(1 for one in self.outcomes() if not one.ok)

    def passed(self):
        return sum(1 for one in self.outcomes() if one.passed())

    def not_run(self):
        return sum(1 for one in self.outcomes() if one.not_run())

    def summary(self):
        return "%d passed, %d failed, %d not run on this window, %d parts with no test, %d tests with no part" % (self.passed(), self.failed(), self.not_run(), len(self.untested), len(self.orphans))

    def write(self):
        out = ["# Test run", "", self.summary() + ".", ""]
        if self.said or self.servers:
            out += ["## Servers and the window", ""] + ["- " + line for line in self.servers + self.said] + [""]
        out += ["## The window", ""]
        for file, outcomes, errors in self.files:
            failed = sum(1 for one in outcomes if not one.ok)
            not_run = sum(1 for one in outcomes if one.not_run())
            out.append("- %s: %d passed, %d failed, %d not run on this window" % (file, len(outcomes) - failed - not_run, failed, not_run))
            out += ["  - not run here: %s: %s" % (one.name, one.error) for one in outcomes if one.not_run()]
            for one in outcomes:
                if not one.ok:
                    out.append("  - FAILED %s (%d ms)\n\n    ```text\n%s\n    ```" % (one.name, one.ms, "\n".join("    " + line for line in one.error.splitlines())))
            out += ["  - the page wrote: " + error.replace("\n", " ") for error in errors]
        if self.untested:
            out += ["", "## No test", ""] + ["- " + part for part in self.untested]
        if self.orphans:
            out += ["", "## Tests whose part is gone", ""] + ["- " + part for part in self.orphans]
        return "\n".join(out) + "\n"


def options_of(words):
    parser = argparse.ArgumentParser(prog="harness.py window test", add_help=True)
    parser.add_argument("--only", action="append", default=[], help="the test files whose paths hold this text")
    parser.add_argument("--driver", choices=("cdp", "webdriver"), default="cdp" if os.name == "nt" else "webdriver")
    parser.add_argument("--docker", action="store_true", help="the suite in the harness's Linux image on this machine")
    parser.add_argument("--sanitize", choices=("address", "thread"), default="", help="the window built and run under this sanitizer, its reports in the run's folder")
    parser.add_argument("--monitor", type=int, default=int(os.environ.get("ORIOR_TEST_MONITOR") or 0), help="the display's number, as the system's settings give it")
    parser.add_argument("--monitors", action="store_true", help="the displays listed, by the number --monitor takes")
    parser.add_argument("--no-servers", action="store_true", help="no test server started here: each is told in the environment")
    parser.add_argument("--keep-window", action="store_true")
    parser.add_argument("--stop-servers", action="store_true", help="every test server taken away")
    return parser.parse_args(words)


def copy_tree(frm, to):
    shutil.copytree(frm, to, dirs_exist_ok=True)


def own_repository(tree):
    """The run's tree a git repository of its own, its files committed: a test's git acts on it alone. What bench
    places under bench/ is left out."""
    git = ["git", "-c", "user.name=orior harness", "-c", "user.email=harness@localhost", "-c", "commit.gpgsign=false"]
    for words in (["init", "-q", "-b", "main"], ["add", "-A"], ["commit", "-q", "-m", "The run's tree."]):
        if words[0] == "add":
            with open(os.path.join(tree, ".git", "info", "exclude"), "w", encoding="utf-8") as out:
                out.write("bench/\n")
        ran = subprocess.run(git + words, cwd=tree, stdin=subprocess.DEVNULL, capture_output=True)
        if ran.returncode != 0:
            raise RuntimeError("git %s in the run's tree ended %d" % (" ".join(words), ran.returncode))


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def whole(path, size, sum_wanted):
    return os.path.isfile(path) and os.path.getsize(path) == size and sha256_of(path) == sum_wanted


def made_line(number):
    line = "%09d a line of the paged file, the same width as every other, read from anywhere in it" % number
    return line.ljust(MADE_WIDTH - 1) + "\n"


def bench_fetched():
    """The files the window is measured on, each fetched where it is not there whole, and the paged file made; the
    folder, or the error that kept one from it."""
    os.makedirs(BENCH_FOLDER, exist_ok=True)
    for row in table(BENCH_TABLE):
        path = os.path.join(BENCH_FOLDER, row["name"])
        if whole(path, int(row["bytes"]), row["sha256"]):
            continue
        part = path + ".part"
        got = subprocess.run(["curl", "-sfL", "--retry", "3", "-o", part, row["url"]], stdin=subprocess.DEVNULL)
        if got.returncode != 0 or not whole(part, int(row["bytes"]), row["sha256"]):
            if os.path.exists(part):
                os.remove(part)
            raise RuntimeError("%s was not fetched whole from %s" % (row["name"], row["url"]))
        os.replace(part, path)
    made = os.path.join(BENCH_FOLDER, MADE)
    if not (os.path.isfile(made) and os.path.getsize(made) == MADE_LINES * MADE_WIDTH):
        with open(made + ".part", "w", encoding="utf-8", newline="\n") as out:
            for number in range(MADE_LINES):
                out.write(made_line(number))
        os.replace(made + ".part", made)
    return BENCH_FOLDER


def bench_placed(tree):
    into = os.path.join(tree, "bench")
    os.makedirs(into, exist_ok=True)
    for name in [row["name"] for row in table(BENCH_TABLE)] + [MADE]:
        frm, to = os.path.join(BENCH_FOLDER, name), os.path.join(into, name)
        try:
            os.link(frm, to)
        except OSError:
            shutil.copyfile(frm, to)


def qry_writer_built():
    """The query run's writer built with the system's C compiler where its sources are newer, for the tests of runs to
    start runs with apart from the window; None where its sources or a compiler are missing."""
    defs = findroot.at("src", "cu", "includes", "file_defs")
    qry = os.path.join(defs, "qry")
    writer = os.path.join(qry, "writer")
    if not os.path.isfile(os.path.join(qry, "qry_writer.c")):
        return None
    compiled = [os.path.join(qry, "qry_writer.c"), os.path.join(qry, "qry_read.c"), os.path.join(defs, "ksg", "ksg_seal.c"), os.path.join(defs, "ksg", "ksg_writing.c")] + [os.path.join(writer, file) for file in files_under(writer, ".c")]
    read = [os.path.join(qry, "qry.h"), os.path.join(qry, "qry_read.h")] + [os.path.join(writer, file) for file in files_under(writer, ".h")]
    os.makedirs(QRY_FOLDER, exist_ok=True)
    binary = os.path.join(QRY_FOLDER, bin_named("qry_writer") + (".exe" if os.name == "nt" else ""))
    newest = max(os.path.getmtime(file) for file in compiled + read if os.path.exists(file))
    if not os.path.exists(binary) or newest > os.path.getmtime(binary):
        made = subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-o", binary, *compiled], stdin=subprocess.DEVNULL)
        if made.returncode != 0:
            return None
    return binary


def told_servers(names, read):
    """The servers' settings named in `names`, as ORIOR_TEST_SERVERS gives them, each with its value as `read` gives
    it; a name with no value is left out."""
    return [(name, read(name)) for name in (names or "").split(",") if name and read(name) is not None]


def version_of(program):
    try:
        out = subprocess.run([program, "--version"], stdin=subprocess.DEVNULL, capture_output=True, text=True, errors="replace", timeout=20)
        text = out.stdout or out.stderr
        return text.strip().splitlines()[0] if text.strip() else ""
    except (OSError, subprocess.TimeoutExpired) as error:
        return "--version did not run: %s" % error


def environment(run, tree):
    """The system read: an outcome for each program the tests drive, for the run's folder, and for the display."""
    outcomes = []
    posix = os.name != "nt"
    for row in table(PROGRAMS_TABLE):
        if row["systems"] == "posix" and not posix:
            continue
        found = shutil.which(row["program"])
        outcomes.append(harness_page.Outcome("%s is on PATH" % row["program"], found is not None, 0, "%s, %s" % (found, version_of(found)) if found else "not found on PATH, needed for %s" % row["for"]))
    try:
        os.makedirs(run, exist_ok=True)
        with open(os.path.join(run, ".guest"), "w", encoding="utf-8") as out:
            out.write("")
        os.remove(os.path.join(run, ".guest"))
        outcomes.append(harness_page.Outcome("the run's folder is writable", True))
    except OSError as error:
        outcomes.append(harness_page.Outcome("the run's folder is writable", False, 0, "%s: %s" % (run, error)))
    inside = os.path.abspath(tree).startswith(os.path.abspath(run))
    outcomes.append(harness_page.Outcome("the tests write into the run's own copy of the tree", inside, 0, "" if inside else "%s lies outside %s" % (tree, run)))
    if posix and sys.platform != "darwin":
        display = os.environ.get("DISPLAY", "")
        outcomes.append(harness_page.Outcome("a display is set", bool(display), 0, display or "DISPLAY is not set"))
    for one in outcomes:
        print("  %s%s%s" % ("" if one.ok else "FAILED ", one.name, ": " + one.error if one.error else ""), flush=True)
    return outcomes


def free_port():
    with socket.socket() as one:
        one.bind(("127.0.0.1", 0))
        return one.getsockname()[1]


def browser_arguments(port):
    with open(findroot.at("ui", "gui", "src-tauri", "tauri.conf.json"), encoding="utf-8") as settings:
        given = json.load(settings)["app"]["windows"][0].get("additionalBrowserArgs", "")
    return ("%s --remote-debugging-port=%d" % (given, port)).strip()


def window_built(gui_name, sanitizer=""):
    """The window built into build/cargo/app, apart from the released one, under `sanitizer` where one is named and
    into a target folder of that sanitizer's own; or the program ORIOR_TEST_GUI names."""
    given = os.environ.get("ORIOR_TEST_GUI")
    if given and os.path.isfile(given):
        return given
    target = APP_TARGET + ("-" + sanitizer if sanitizer else "")
    env = dict(os.environ, **harness_webdriver.sanitizer_build_env(sanitizer))
    built = subprocess.run(["cargo", "build", "--manifest-path", findroot.at("ui", "gui", "src-tauri", "Cargo.toml"), "--target-dir", target], cwd=ROOT, env=env, stdin=subprocess.DEVNULL)
    if built.returncode != 0:
        raise RuntimeError("the window did not build")
    return os.path.join(target, "debug", gui_name + (".exe" if os.name == "nt" else ""))


def sanitizer_settings(sanitizer, run):
    """The settings the window runs under `sanitizer` with: its reports written to sanitizer.<pid> in the run's
    folder, no report ending the window, every one told and counted, and the leaks of a window the run kills
    not counted."""
    if not sanitizer:
        return []
    into = os.path.join(run, "sanitizer").replace("\\", "/")
    if sanitizer == "thread":
        return [("TSAN_OPTIONS", "log_path=%s:halt_on_error=0" % into)]
    return [("ASAN_OPTIONS", "log_path=%s:halt_on_error=0:detect_leaks=0" % into)]


def sanitizer_said(run, report):
    """Each file the sanitizer wrote in the run's folder: how many reports it holds, by kind, into the report's
    sayings."""
    for name in sorted(os.listdir(run)):
        if not name.startswith("sanitizer."):
            continue
        kinds = {}
        with open(os.path.join(run, name), encoding="utf-8", errors="replace") as kept:
            for line in kept:
                if "ERROR: " in line or "WARNING: " in line:
                    kind = line.split(": ", 2)[-1].split(" ")[0].strip()
                    kinds[kind] = kinds.get(kind, 0) + 1
        told = ", ".join("%d %s" % (count, kind) for kind, count in sorted(kinds.items(), key=lambda one: -one[1]))
        report.said.append("the sanitizer wrote %s: %s" % (os.path.join(run, name), told or "no report"))


class Window:
    """The window started for the run on its debugging port, with a home, a tree and a web view profile of its own."""

    def __init__(self, program, run, tree, extra):
        self.run = run
        self.port = free_port()
        env = dict(os.environ)
        env.update({"ORIOR_HOME": os.path.join(run, "home"), "ORIOR_ROOT": tree, "ORIOR_NO_REPORTS": "1", "GIT_CONFIG_COUNT": "2", "GIT_CONFIG_KEY_0": "commit.gpgsign", "GIT_CONFIG_VALUE_0": "false", "GIT_CONFIG_KEY_1": "tag.gpgsign", "GIT_CONFIG_VALUE_1": "false", "WEBVIEW2_USER_DATA_FOLDER": os.path.join(run, "webview"), "WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS": browser_arguments(self.port)})
        env.update(dict(extra))
        self.log = open(os.path.join(run, "window.log"), "wb")
        self.process = subprocess.Popen([program], cwd=tree, env=env, stdin=subprocess.DEVNULL, stdout=self.log, stderr=self.log)
        until = time.monotonic() + 120
        self.page = None
        while time.monotonic() < until and self.page is None:
            if self.process.poll() is not None:
                raise RuntimeError("the window closed as it started (%d); its log is %s" % (self.process.returncode, os.path.join(run, "window.log")))
            try:
                self.page = harness_cdp.window_page(self.port)
            except ConnectionError:
                time.sleep(0.1)
        if self.page is None:
            self.close()
            raise RuntimeError("the window's page did not open in 120 s; its log is %s" % os.path.join(run, "window.log"))

    def placed(self, monitor):
        """On Windows, the window moved onto the display `monitor` numbers, else the harness's own display."""
        if os.name != "nt":
            return
        import harness_win32 as win
        import harness_window

        displays = win.displays()
        wanted = "\\\\.\\DISPLAY%d" % monitor if monitor else harness_window.DISPLAY
        if wanted not in displays and displays:
            wanted = sorted(displays)[-1]
        harness_window.window_placed(self.process.pid, wanted)

    def ended(self):
        code = self.process.poll()
        return "the window ended with %d" % code if code is not None else "the window's page closed while its process still runs"

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        self.log.close()


def untested(report):
    """What has no test, and what tests nothing: each module of the window with no file in the suite, and each test
    file whose module is gone."""
    tested = {file[: -len(".test.js")] for file in files_under(os.path.join(SUITE, "src"), ".test.js")}
    modules = {file[: -len(".js")] for file in files_under(WINDOW_FILES, ".js")}
    report.untested += ["gui/src-js/%s.js" % module for module in sorted(modules - tested)]
    report.orphans += ["test/src/%s.test.js" % test for test in sorted(tested - modules)]


def image_named(wanted):
    for row in table(IMAGES_TABLE):
        if row["id"] == wanted:
            return row
    raise SystemExit("window_images.tsv names no %s" % wanted)


def inside_container(passed):
    """What the container runs: the tree's parts brought in from /host to the volume at /top, each file where its bytes
    differ and with the container's own time, then the harness's window test on X's display in memory at 96 dots an
    inch, at which WebKitWebDriver puts each press where it is sent."""
    words = " ".join("'%s'" % word.replace("'", "'\\''") for word in passed if word != "--docker")
    sync = "rsync -a --checksum --no-times --delete --exclude /test/runs --exclude /cli/target --exclude /gui/src-tauri/target --exclude __pycache__ /host/ui/ /top/ui/"
    sync += " && rsync -a --checksum --no-times --delete --exclude __pycache__ /host/utils/ /top/utils/ && mkdir -p /top/src && rsync -a --checksum --no-times --delete /host/src/cu/ /top/src/cu/ && cp /host/bin_names.tsv /top/bin_names.tsv"
    run = "dbus-run-session -- xvfb-run -a -s '-screen 0 1920x1080x24 -dpi 96' orior-watch python3 /top/utils/test/harness.py window test --driver webdriver --no-servers %s" % words
    return (sync + " && " + run).strip()


def docker_words(passed, env, keys):
    """The words of docker run for the Linux image: the tree's parts read at /host, the volumes that keep the
    container's copy, its builds and its crates, the runs' folder here where its reports go, the servers' settings as a
    container reaches this machine, and the ssh key's folder where the ssh server came up."""
    words = ["run", "--rm", "--init", "--shm-size=1g", "--add-host", "host.docker.internal:host-gateway", "-p", "127.0.0.1:6080:6080"]
    for key, value in env:
        words += ["-e", "%s=%s" % (key, value)]
    mounts = [(findroot.at("ui"), "/host/ui", True), (findroot.at("utils"), "/host/utils", True), (findroot.at("src", "cu"), "/host/src/cu", True), (findroot.at("bin_names.tsv"), "/host/bin_names.tsv", True), (RUNS, "/top/build/ui_test/runs", False)]
    if keys:
        mounts.append((keys, "/host/ssh", True))
    for source, target, read_only in mounts:
        words += ["--mount", "type=bind,source=%s,target=%s%s" % (source, target, ",readonly" if read_only else "")]
    words += ["-v", "%s:/top" % image_named("tree_volume")["image"], "-v", "%s:/root/.cargo/registry" % image_named("cargo_volume")["image"]]
    return words + [image_named("linux")["image"], "sh", "-c", inside_container(passed)]


def run_in_docker(passed, needed):
    """The suite run in the Linux image, the servers `needed` names started here for it and taken away after."""
    os.makedirs(RUNS, exist_ok=True)
    daemon = subprocess.run(["docker", "info", "--format", "{{.OSType}}"], stdin=subprocess.DEVNULL, capture_output=True, text=True)
    if daemon.stdout.strip() == "windows":
        print("Docker runs Windows containers here; the window's suite runs in the Linux image alone")
        return 2
    image = image_named("linux")
    print("the window's tests in Docker, image %s; the window shows at %s as they run" % (image["image"], WATCH), flush=True)
    built = subprocess.run(["docker", "build", "-t", image["image"], findroot.at(*image["folder"].split("/"))], stdin=subprocess.DEVNULL)
    if built.returncode != 0:
        print("the image %s did not build" % image["image"])
        return 1
    up = harness_servers.start(needed)
    for said in up.said:
        print("  " + said)
    env = harness_servers.seen_from_a_container(up.env)
    keys = None
    if any(key == "ORIOR_TEST_SSH" for key, _ in up.env):
        port = harness_servers.ssh_port(os.path.join(harness_servers.SSH_FOLDER, "config"))
        if port:
            env.append(("ORIOR_TEST_SSH_PORT", str(port)))
            keys = harness_servers.SSH_FOLDER
    env.append(("ORIOR_TEST_SERVERS", ",".join(key for key, _ in env)))
    try:
        ran = subprocess.run(["docker", *docker_words(passed, env, keys)], stdin=subprocess.DEVNULL)
    finally:
        harness_servers.stop(up.started)
    return ran.returncode


def cmd_window_test(a):
    words = [word for word in a.extra if word != "--"]
    options = options_of(words)
    if options.stop_servers:
        harness_servers.stop(harness_servers.names())
        print("the test servers are stopped")
        return 0
    if options.monitors:
        if os.name != "nt":
            print("the displays are listed on Windows alone")
            return 1
        import harness_win32 as win

        for device, (left, top, right, bottom) in sorted(win.displays().items()):
            print("  %s  %dx%d at %d,%d" % (device, right - left, bottom - top, left, top))
        return 0
    tests = ["src/" + file for file in files_under(os.path.join(SUITE, "src"), ".test.js") if not options.only or any(only in file for only in options.only)]
    needed = harness_servers.reached_in([os.path.join(SUITE, file) for file in tests])
    if options.docker:
        return run_in_docker(words, needed)
    run = os.path.join(RUNS, stamp())
    tree = os.path.join(run, "tree")
    os.makedirs(run, exist_ok=True)
    report = Report()
    env = [("ORIOR_TEST_HTTP", harness_servers.echo_server())]
    writer = qry_writer_built()
    if writer:
        env.append(("ORIOR_TEST_QRY_WRITER", writer.replace("\\", "/")))
    benched = None
    try:
        benched = bench_fetched()
        env.append(("ORIOR_TEST_BENCH", "bench"))
    except (RuntimeError, OSError) as error:
        report.said.append("the benchmark files: %s" % error)
    started = []
    if options.no_servers:
        env += told_servers(os.environ.get("ORIOR_TEST_SERVERS"), os.environ.get)
        port = os.environ.get("ORIOR_TEST_SSH_PORT")
        if port and port.isdigit():
            try:
                env += harness_servers.ssh_inside("/host/ssh", findroot.at("build", "ui_test", "ssh-inside"), int(port))
            except (RuntimeError, OSError) as error:
                report.said.append("the ssh server: %s" % error)
    else:
        print("servers", flush=True)
        up = harness_servers.start(needed)
        started = up.started
        for key, value in up.env:
            if not key.endswith("PASSWORD"):
                print("  %s=%s" % (key, value), flush=True)
                report.servers.append("%s=%s" % (key, value))
        for said in up.said:
            print("  " + said, flush=True)
            report.said.append(said)
        env += up.env
    code = 1
    try:
        print("guest", flush=True)
        report.files.append(("the system", environment(run, tree), []))
        print("window, through %s" % ("WebDriver" if options.driver == "webdriver" else "the debugging port"), flush=True)
        if tests:
            drive(options, run, tree, env, benched, report)
        if not options.only:
            untested(report)
        with open(os.path.join(run, "report.md"), "w", encoding="utf-8") as out:
            out.write(report.write())
        print(report.summary(), flush=True)
        print("report: %s" % os.path.join(run, "report.md"), flush=True)
        code = 0 if report.failed() == 0 and not report.untested and not report.orphans and not report.said else 1
    finally:
        harness_servers.stop(started)
    return code


def drive(options, run, tree, env, benched, report):
    """The window built and started on the run's tree, the files run in it, and the window ended."""
    gui_name = bin_named("gui")
    told = env + [("ORIOR_TEST_HOME", os.path.join(run, "home")), ("ORIOR_TEST_TREE", tree)]
    extra = [("ORIOR_SSH", value) for key, value in env if key == "ORIOR_TEST_SSH"] + sanitizer_settings(options.sanitize, run)
    tests = ["src/" + file for file in files_under(os.path.join(SUITE, "src"), ".test.js") if not options.only or any(only in file for only in options.only)]
    try:
        copy_tree(os.path.join(SUITE, "fixtures", "tree"), tree)
        own_repository(tree)
        if benched:
            bench_placed(tree)
        os.makedirs(os.path.join(run, "home"), exist_ok=True)
        if options.driver == "webdriver":
            program = harness_webdriver.build(gui_name, options.sanitize)
            told.append(("ORIOR_TEST_PROGRAM", program))
            driven = harness_webdriver.start(program, run, tree, extra)
            try:
                harness_page.run_files(harness_page.DrivenPage(driven.session, dict(told)), tests, report)
            finally:
                harness_input.stop_started()
                driven.close()
                sanitizer_said(run, report)
            return
        program = window_built(gui_name, options.sanitize)
        told.append(("ORIOR_TEST_PROGRAM", program))
        # a window left by a run ended from outside would hold the display and the port this run takes
        import harness_window

        harness_window.windows_ended()
        window = Window(program, run, tree, extra)
    except (RuntimeError, OSError) as error:
        print("  %s" % error, flush=True)
        report.files.append(("the window", [harness_page.Outcome("it starts", False, 0, str(error))], []))
        return
    try:
        window.placed(options.monitor)
        conn = harness_cdp.Connection(window.page)
        page = harness_page.Page(conn, dict(told))
        page.ready()
        harness_page.run_files(page, tests, report)
        if page.closed():
            report.said.append(window.ended())
    except (RuntimeError, TimeoutError, ConnectionError, OSError) as error:
        report.said.append("the window's debugging port: %s" % error)
    finally:
        harness_input.stop_started()
        if options.keep_window:
            print("  the window is kept open on port %d" % window.port, flush=True)
        else:
            window.close()
        sanitizer_said(run, report)


def register(window):
    """The window suite's command, added to the window group."""
    p = window.add_parser("test", help="the window's suite, ui/test/src, run in a window of the run's own; --help lists its options")
    # the suite's options are the command's own: main hands them over unparsed, in their order
    p.set_defaults(fn=cmd_window_test, takes_extra=True)
