# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py cli: the CLI built, its checks run and its common paths timed.

    harness.py cli build                  the CLI library and programs, ui/cli/build.sh
    harness.py cli test [name...]         the CLI's Unity suites, every one where none is named
    harness.py cli coverage build|test|report  the suites built for gcov, run, and merged by gcovr into build/sonarqube/coverage.xml
    harness.py cli bench [--label L] [--repeat N]  the CLI's common paths timed over ui/cli/src, rows retained in build/harness/bench

A child's output streams through the harness's own; the invocation's log under build/harness/log therefore holds it."""

import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_unity  # noqa: E402

ROOT = findroot.root()
CLI_BUILD = "ui/cli/build.sh"
CLI_BENCH = "utils/test/src/ui/cli/cli_bench.sh"
# the lines a suite's checks print as the harness reads them: "N checks, M failed" and "test exit N"
CHECKS_LINE = re.compile(r"(\d+) checks, (\d+) failed")
EXIT_LINE = re.compile(r"test exit (\d+)")


def output_read(text):
    """(checks, failed, exits) of a suite's output: the checks and failures its check lines count, and every test exit
    it printed."""
    checks = failed = 0
    for m in CHECKS_LINE.finditer(text):
        checks += int(m.group(1))
        failed += int(m.group(2))
    return checks, failed, [int(m.group(1)) for m in EXIT_LINE.finditer(text)]


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


def processor_seconds(process, job=None):
    """The processor time a child and its descendants have used, in seconds; None where it cannot be read. On Windows
    the descendants are read through `job`, the job the child was put in as it started (harness_win32.job_joined)."""
    if os.name == "nt":
        import harness_win32 as win

        return win.job_processor_seconds(job) if job else None
    # a system with no /proc, as macOS has none, cannot say
    if not os.path.isdir("/proc"):
        return None
    # every process under the child, running or ended: a process's own stat counts its running children not at all
    parents = {}
    ticks = {}
    for name in os.listdir("/proc"):
        if not name.isdigit():
            continue
        try:
            with open("/proc/%s/stat" % name, encoding="ascii", errors="replace") as stat:
                fields = stat.read().rsplit(")", 1)[1].split()
            parents[int(name)] = int(fields[1])
            ticks[int(name)] = int(fields[11]) + int(fields[12]) + int(fields[13]) + int(fields[14])
        except (OSError, IndexError, ValueError):
            continue
    if process.pid not in ticks:
        return None
    total = 0
    for pid, ticked in ticks.items():
        above = pid
        while (above != process.pid) and (above in parents) and (above != 1):
            above = parents[above]
        total += ticked if above == process.pid else 0
    return total / os.sysconf("SC_CLK_TCK")


# how long a child may go without a line or any processor time before it is called stalled and ended, and how often
# a quiet child that still works is said to be alive
STALL_SECONDS = 90
HEARTBEAT_SECONDS = 30


def streamed(command, cwd=ROOT, env=None, stall=STALL_SECONDS, seen=None):
    """A command run with its output streamed line by line through the harness's standard output, each line added to
    `seen` where it is a list; its exit code.
    A child that writes nothing is watched: while its processor time moves a heartbeat says so every
    HEARTBEAT_SECONDS; one whose time is stationary for `stall` seconds is ended and 124 is the exit code. A child whose work
    runs inside a container shows no processor time here; such a child is therefore run with no stall, the heartbeat alone."""
    import queue
    import threading
    import time

    # on Windows the child starts suspended and joins a job before it runs, so that every process under it is counted
    job = None
    flags = 0
    if os.name == "nt":
        import harness_win32 as win

        flags = win.CREATE_SUSPENDED
    process = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1, creationflags=flags)
    if os.name == "nt":
        job = win.job_joined(process._handle)
        if win.threads_resumed(process.pid) == 0:
            process.kill()
            process.wait()
            print("not started: the child's thread could not be let run: %s" % " ".join(command), flush=True)
            return 1
    lines = queue.Queue()

    def reading():
        for line in process.stdout:
            lines.put(line)
        lines.put(None)

    threading.Thread(target=reading, daemon=True).start()
    quiet_since = time.monotonic()
    said_at = quiet_since
    cpu_was = processor_seconds(process, job)
    cpu_moved_at = quiet_since
    while True:
        try:
            line = lines.get(timeout=5)
        except queue.Empty:
            now = time.monotonic()
            cpu = processor_seconds(process, job)
            if cpu is not None and cpu_was is not None and cpu > cpu_was + 0.05:
                cpu_moved_at = now
            cpu_was = cpu if cpu is not None else cpu_was
            if (stall is not None) and (now - max(cpu_moved_at, quiet_since) >= stall):
                print("stalled: no output and no processor time for %d s, ended: %s" % (stall, " ".join(command)), flush=True)
                # the child's own children are in its job, and a stalled check is such a child of its script
                if job is not None:
                    win.job_ended(job)
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
        if seen is not None:
            seen.append(line)
        quiet_since = said_at = time.monotonic()
    return process.wait()


def cmd_cli_build(a):
    return streamed([bash(), CLI_BUILD])


def checks_env(a):
    """The checks' environment: a suite's, which leaves the fixtures' commits and tags unsigned, with the settings
    `--sanitize` and `--owners` give."""
    env = harness_unity.suite_env()
    if a.sanitize:
        env["ORIOR_SANITIZE"] = a.sanitize
    if a.owners:
        env["ORIOR_CELLS_OWNERS"] = "1"
    return env


def library_archive(folder):
    """The path of impero's archive in build/cli/<folder>."""
    with open(os.path.join(ROOT, "bin_names.tsv"), encoding="utf-8") as names:
        cli_name = next(line.split("\t")[1] for line in names if line.startswith("cli\t"))
    return os.path.join(ROOT, "build", "cli", folder, "lib" + cli_name + ".a")


def cmd_cli_test(a):
    if a.cases:
        return harness_unity.show_cases(ROOT, a.cases)
    build_env = harness_unity.sanitize_env(a.sanitize, "build/sanitizer_build", dict(checks_env(a) or os.environ))
    if a.owners:
        os.environ["ORIOR_CELLS_OWNERS"] = "1"

    def library_built(target):
        """impero's library built for `target` through ui/cli/build.sh: its archive, or None where the build failed."""
        code = streamed([bash(), CLI_BUILD, "library", target], env=build_env)
        if code != 0:
            print("the library's build ended with %d" % code, flush=True)
            return None
        return library_archive(target + (("-" + a.sanitize) if a.sanitize else ""))

    return harness_unity.run_suites(ROOT, a.names, library_built, a.sanitize)


def coverage_env():
    """The environment that makes ui/cli/build.sh build for coverage."""
    return dict(os.environ, ORIOR_COVERAGE="1")


def coverage_library_built(target):
    """impero's library built for coverage for `target`: its archive, or None where the build failed."""
    if streamed([bash(), CLI_BUILD, "library", target], env=coverage_env()) != 0:
        return None
    return library_archive(target + "-coverage")


def cmd_cli_coverage_build(a):
    # the program and the window are built too, their sources having no suite yet, for a build wrapper to record every source
    code = harness_unity.coverage_built(ROOT, a.names, coverage_library_built)
    for words in ((CLI_BUILD,), (CLI_BUILD, "window")):
        code = code or streamed([bash(), *words], env=coverage_env())
    return code


def cmd_cli_coverage_test(a):
    return harness_unity.coverage_ran(ROOT, a.names)


def cmd_cli_coverage_report(a):
    return harness_unity.coverage_report(ROOT)


def cmd_cli_bench(a):
    if not a.docker:
        return streamed([bash(), CLI_BENCH, a.label, str(a.repeat)])
    # the tree's sources copied into the container's own filesystem first; the rows therefore measure the code and not the
    # mount; the rows the run retains are appended to the host's bench.tsv under a label that says where they ran
    inside = ("mkdir -p /work /usr/src/build/harness/bench && cp -r /usr/src/ui /usr/src/src /usr/src/utils /usr/src/bin_names.tsv /work/"
              " && cd /work && bash utils/test/src/ui/cli/cli_bench.sh %s %d"
              " && cat build/harness/bench/bench.tsv >> /usr/src/build/harness/bench/bench.tsv") % ("docker-" + a.label, a.repeat)
    return streamed(["docker", "run", "--rm", "-v", ROOT + ":/usr/src", "-w", "/usr/src", "orior-test-window", "sh", "-c", inside], stall=None,
                    env=dict(os.environ, MSYS_NO_PATHCONV="1"))




def register(sub):
    """The cli group's commands added to harness.py's subparsers."""
    group = sub.add_parser("cli", help="the CLI built and its checks run")
    cli = group.add_subparsers(dest="command", required=True)
    p = cli.add_parser("build", help="the CLI library and programs, through ui/cli/build.sh")
    p.set_defaults(fn=cmd_cli_build)
    p = cli.add_parser("test", help="the CLI's Unity suites generated, built and run, every one where none is named")
    p.add_argument("names", nargs="*", help="suite names, the module of utils/test/src/ui/cli/unit/<module>/test_<module>")
    p.add_argument("--cases", default="", help="print what the generator registers in this suite and what it would skip")
    p.add_argument("--sanitize", choices=("address", "thread"), default="", help="the suites built and run under this sanitizer, a report failing its suite")
    p.add_argument("--owners", action="store_true", help="each suite's end prints MMgr's owner table, every owner's bytes now and its peak")
    p.set_defaults(fn=cmd_cli_test)
    p = cli.add_parser("coverage", help="the CLI's Unity suites built for gcov, run, and their counts merged through gcovr for the scanner")
    coverage = p.add_subparsers(dest="step", required=True)
    q = coverage.add_parser("build", help="the library, the suites, the program and the window built with --coverage under build/cli/<target>-coverage")
    q.add_argument("names", nargs="*", help="suite names, every suite where none are named")
    q.set_defaults(fn=cmd_cli_coverage_build)
    q = coverage.add_parser("test", help="the suites built for coverage run, the .gcda of any earlier run removed first")
    q.add_argument("names", nargs="*", help="suite names, every suite where none are named")
    q.set_defaults(fn=cmd_cli_coverage_test)
    q = coverage.add_parser("report", help="the .gcda merged through gcovr into build/sonarqube/coverage.xml")
    q.set_defaults(fn=cmd_cli_coverage_report)
    p = cli.add_parser("bench", help="the CLI's common paths timed over ui/cli/src, rows retained in build/harness/bench/bench.tsv")
    p.add_argument("--label", default="", help="the run's label in the retained rows, such as before or after")
    p.add_argument("--repeat", type=int, default=5, help="how many times each pass runs, the least time retained")
    p.add_argument("--docker", action="store_true", help="run in the Linux test image, on a copy of the tree inside it")
    p.set_defaults(fn=cmd_cli_bench)


