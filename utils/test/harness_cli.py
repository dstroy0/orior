# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py cli and harness.py window test: the CLI built and its suite run, and the window's suite run.

    harness.py cli build                  the CLI library and programs, ui/cli/build.sh
    harness.py cli test [name...]         the CLI's checks, every one where none is named
    harness.py cli bench [--label L] [--repeat N]  the CLI's common paths timed over ui/cli/src, rows kept in build/harness/bench
    harness.py window test [arg...]       the window's suite, ui/test, the arguments handed to it

A child's output streams through the harness's own; the invocation's log under build/harness/log therefore holds it."""

import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
WINDOW_TESTS = os.path.join(ROOT, "ui", "test", "Cargo.toml")
CLI_BUILD = "ui/cli/build.sh"
CLI_TESTS = "utils/test/src/ui/cli/cli_test.sh"
CLI_BENCH = "utils/test/src/ui/cli/cli_bench.sh"


def bash():
    """The bash the scripts run under: on Windows, Git's, not the System32 one that opens WSL."""
    if os.name != "nt":
        return "bash"
    git = shutil.which("git")
    homes = [os.path.dirname(os.path.dirname(git))] if git else []
    homes += [os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "Git")]
    for home in homes:
        for inside in (("bin", "bash.exe"), ("usr", "bin", "bash.exe")):
            path = os.path.join(home, *inside)
            if os.path.isfile(path):
                return path
    raise SystemExit("Git's bash was not found")


def processor_seconds(process):
    """The processor time a child and its descendants have used, in seconds; None where it cannot be read."""
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes

        kernel = ctypes.windll.kernel32
        times = [wintypes.FILETIME() for _ in range(4)]
        handle = wintypes.HANDLE(process._handle)
        if not kernel.GetProcessTimes(handle, *[ctypes.byref(one) for one in times]):
            return None
        total = 0
        for one in times[2:]:
            total += (one.dwHighDateTime << 32) + one.dwLowDateTime
        return total / 10_000_000
    try:
        with open("/proc/%d/stat" % process.pid, encoding="ascii") as stat:
            fields = stat.read().rsplit(")", 1)[1].split()
        return (int(fields[11]) + int(fields[12]) + int(fields[13]) + int(fields[14])) / os.sysconf("SC_CLK_TCK")
    except (OSError, IndexError, ValueError):
        return None


# how long a child may go without a line or any processor time before it is called stalled and ended, and how often
# a quiet child that still works is said to be alive
STALL_SECONDS = 90
HEARTBEAT_SECONDS = 30


def streamed(command, cwd=ROOT, env=None, stall=STALL_SECONDS):
    """A command run with its output streamed line by line through the harness's standard output; its exit code.
    A child that writes nothing is watched: while its processor time moves a heartbeat says so every
    HEARTBEAT_SECONDS; one whose time stands still for `stall` seconds is ended and 124 is the exit code. A child whose work
    runs inside a container shows no processor time here; such a child is therefore run with no stall, the heartbeat alone."""
    import queue
    import threading
    import time

    process = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1)
    lines = queue.Queue()

    def reading():
        for line in process.stdout:
            lines.put(line)
        lines.put(None)

    threading.Thread(target=reading, daemon=True).start()
    quiet_since = time.monotonic()
    said_at = quiet_since
    cpu_was = processor_seconds(process)
    cpu_moved_at = quiet_since
    while True:
        try:
            line = lines.get(timeout=5)
        except queue.Empty:
            now = time.monotonic()
            cpu = processor_seconds(process)
            if cpu is not None and cpu_was is not None and cpu > cpu_was + 0.05:
                cpu_moved_at = now
            cpu_was = cpu if cpu is not None else cpu_was
            if (stall is not None) and (now - max(cpu_moved_at, quiet_since) >= stall):
                print("stalled: no output and no processor time for %d s, ended: %s" % (stall, " ".join(command)), flush=True)
                process.kill()
                process.wait()
                return 124
            if now - said_at >= HEARTBEAT_SECONDS:
                said_at = now
                print("alive: quiet %d s, processor %.1f s" % (now - quiet_since, cpu or 0.0), flush=True)
            continue
        if line is None:
            break
        print(line, end="", flush=True)
        quiet_since = said_at = time.monotonic()
    return process.wait()


def cmd_cli_build(a):
    return streamed([bash(), CLI_BUILD])


def cmd_cli_test(a):
    return streamed([bash(), CLI_TESTS, *a.names])


def cmd_cli_bench(a):
    if not a.docker:
        env = dict(os.environ, LTO="1") if a.lto else None
        return streamed([bash(), CLI_BENCH, a.label, str(a.repeat)], env=env)
    # the tree's sources copied into the container's own filesystem first; the rows therefore measure the code and not the
    # mount; the rows the run keeps are appended to the host's bench.tsv under a label that says where they ran
    inside = ("mkdir -p /work /usr/src/build/harness/bench && cp -r /usr/src/ui /usr/src/src /usr/src/utils /usr/src/bin_names.tsv /work/"
              " && cd /work && bash utils/test/src/ui/cli/cli_bench.sh %s %d"
              " && cat build/harness/bench/bench.tsv >> /usr/src/build/harness/bench/bench.tsv") % ("docker-" + a.label, a.repeat)
    return streamed(["docker", "run", "--rm", "-v", ROOT + ":/usr/src", "-w", "/usr/src", "orior-test-window", "sh", "-c", inside], stall=None,
                    env=dict(os.environ, MSYS_NO_PATHCONV="1"))


def cmd_window_test(a):
    words = [word for word in a.extra if word != "--"]
    return streamed(["cargo", "run", "--manifest-path", WINDOW_TESTS, "--release", "--", *words], stall=None)


def register(sub):
    """The cli group's commands added to harness.py's subparsers."""
    group = sub.add_parser("cli", help="the CLI built and its checks run")
    cli = group.add_subparsers(dest="command", required=True)
    p = cli.add_parser("build", help="the CLI library and programs, through ui/cli/build.sh")
    p.set_defaults(fn=cmd_cli_build)
    p = cli.add_parser("test", help="the CLI's checks, every one where none is named")
    p.add_argument("names", nargs="*", help="check names, as cli_test.sh takes them")
    p.set_defaults(fn=cmd_cli_test)
    p = cli.add_parser("bench", help="the CLI's common paths timed over ui/cli/src, rows kept in build/harness/bench/bench.tsv")
    p.add_argument("--label", default="", help="the run's label in the kept rows, such as before or after")
    p.add_argument("--repeat", type=int, default=5, help="how many times each pass runs, the least time kept")
    p.add_argument("--docker", action="store_true", help="run in the Linux test image, on a copy of the tree inside it")
    p.add_argument("--lto", action="store_true", help="the library built as bitcode and the bench linked whole, build/cli/<host>-lto")
    p.set_defaults(fn=cmd_cli_bench)


def register_window_test(window):
    """The window suite's command, added to the window group."""
    p = window.add_parser("test", help="the window's suite, ui/test, every argument handed to it")
    # the suite's arguments, such as --only <name> --no-servers --no-rust --monitor 3, are not the harness's: main
    # hands them over unparsed, in their order
    p.set_defaults(fn=cmd_window_test, takes_extra=True)
