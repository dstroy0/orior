# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py window check --on HOST | --docker: the window's checks run on a Linux host.
    harness.py window check --on user@host [NAME...] [--only ...]   the host reached over ssh, as the window's own link
                                                                   reaches one: the host and leave to reach it are the
                                                                   only things the reader gives, no path is kept, and
                                                                   the host's own PATH resolves each program
    harness.py window check --on [NAME...]                         the host the preference checks.host names, set in
                                                                   Preferences or at install with --checks-host
    harness.py window check --docker [NAME...]                     the host that is the image, the container
                                                                   orior-ui-linux, started and synced as harness.py
                                                                   container does
The host is read first, against window_programs.tsv and toolchains.json by workflow, and the answer comes in three
parts: what a workflow needs and the host lacks, which stops that workflow; what would help it; and, for a host that is
not the image, the steps that set the bare machine up to the sheet, said for that host's package manager through
window_packages.tsv. A host the checks workflow cannot run on ends the run there, with the steps printed.
Then the tree's parts the window's build reads come over as the Linux image takes them, with rsync where this machine
has one that reaches ssh and as a tar stream over ssh elsewhere, into ~/orior_checks/tree on the host; the window and
its checks are built there and run under xvfb-run, X's display in memory, each line coming back as it is printed. The
report goes to build/ui_test/runs/<time>/report.md: the host's reading, the checks' summary and the checks' own lines.
WSL is a host like any other: its sshd reached by the distro's address."""
import json
import os
import shutil
import subprocess
import sys
import time
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from utils.tools import findroot  # noqa: E402
ROOT = findroot.root()
RUNS = findroot.at("build", "ui_test", "runs")
PROGRAMS_TABLE = findroot.at("utils", "test", "window_programs.tsv")
PACKAGES_TABLE = findroot.at("utils", "test", "window_packages.tsv")
TOOLCHAINS = findroot.at("ui", "cli", "toolchains.json")
PROBE = findroot.at("utils", "test", "window_host_probe.sh")
# the parts of the tree the window's build reads, as the Linux image copies them in
PARTS = ["ui", "utils", "src/cu", "includes", "LICENSES", "LICENSE", "bin_names.tsv"]
# what a copy leaves out of the parts: a library's own builds and checkouts, the window suite's runs and Python's caches
LEFT_OUT = ["build", ".git", "__pycache__", "ui/test/runs"]
# the folder of the host's own that holds its copy of the tree
HOST_TREE = "orior_checks/tree"
# the setting that names the host, as Preferences and the installer write it
HOST_SETTING = "checks.host"
# the workflow the checks cannot run without; the others fail checks of their own where they lack a program
CHECKS_WORKFLOW = "checks"
XVFB = "xvfb-run -a -s '-screen 0 1920x1080x24 -dpi 96'"


def table(path):
    """The rows of a tab-separated table with a head line, each a dict by the head's names."""
    with open(path, encoding="utf-8") as read:
        head = read.readline().rstrip("\n").split("\t")
        return [dict(zip(head, line.rstrip("\n").split("\t"))) for line in read if line.strip()]


def host_from_preference():
    """The host the reader's settings.json names under checks.host, or "" where it names none."""
    import harness_window

    path = os.path.join(harness_window.orior_home(), "settings.json")
    try:
        with open(path, encoding="utf-8") as read:
            settings = json.load(read)
    except (OSError, ValueError):
        return ""
    value = settings.get(HOST_SETTING)
    if value is None and isinstance(settings.get("checks"), dict):
        value = settings["checks"].get("host")
    return value if isinstance(value, str) else ""


def quoted(word):
    return "'" + word.replace("'", "'\\''") + "'"


class SshHost:
    """A Linux host reached over ssh: a command run there through its login shell, and the tree's parts copied in."""

    def __init__(self, host):
        self.host = host
        self.name = host
        self.tree = "$HOME/" + HOST_TREE
        self.is_image = False

    def ssh_words(self):
        return ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=20", self.host]

    def run(self, command, capture=False, stdin=None):
        """`command` run on the host under its login shell: its exit code, with its output where captured."""
        words = self.ssh_words() + ["bash", "-lc", quoted(command)]
        if capture:
            ran = subprocess.run(words, stdin=stdin or subprocess.DEVNULL, capture_output=True, text=True, errors="replace")
            return ran.returncode, ran.stdout, ran.stderr
        return subprocess.run(words, stdin=stdin or subprocess.DEVNULL).returncode, "", ""

    def probe(self, programs):
        """The probe script run on the host over its standard input, the programs as its words."""
        with open(PROBE, encoding="utf-8") as script:
            words = self.ssh_words() + ["bash", "-s", "--"] + programs
            ran = subprocess.run(words, stdin=script, capture_output=True, text=True, errors="replace")
        return ran.returncode, ran.stdout, ran.stderr

    def copied(self):
        """The parts brought into the host's copy: rsync where this machine's rsync reaches ssh, else one tar stream."""
        if os.name != "nt" and shutil.which("rsync"):
            left_out = []
            for one in LEFT_OUT:
                left_out += ["--exclude", one]
            code = self.run("mkdir -p " + self.tree)[0]
            for part in PARTS:
                source = os.path.join(ROOT, part)
                target = "%s:%s/%s" % (self.host, HOST_TREE, part)
                words = ["rsync", "-a", "--delete", "--checksum", "--no-times", "-e", "ssh -o BatchMode=yes"] + left_out
                words += [source + ("/" if os.path.isdir(source) else ""), target]
                code = code or subprocess.run(words, stdin=subprocess.DEVNULL).returncode
            return code
        left_out = []
        for one in LEFT_OUT:
            left_out += ["--exclude=" + one]
        tar = subprocess.Popen(["tar", "-cf", "-", *left_out, *PARTS], cwd=ROOT, stdout=subprocess.PIPE)
        unpack = "rm -rf %s && mkdir -p %s && tar -xf - -C %s" % (self.tree, self.tree, self.tree)
        code, _, err = self.run(unpack, capture=True, stdin=tar.stdout)
        tar.stdout.close()
        tar.wait()
        if code != 0:
            print(err.strip(), flush=True)
        return code or tar.returncode


class ImageHost:
    """The host that is the image: the container orior-ui-linux, started and synced as harness.py container does."""

    def __init__(self):
        import harness_container

        self.container = harness_container
        self.system = harness_container.system_named("linux")
        self.name = harness_container.container_of(self.system)
        self.tree = "/top"
        self.is_image = True

    def run(self, command, capture=False, stdin=None):
        words = ["docker", "exec", "-i", self.name, "bash", "-lc", command]
        if capture:
            ran = subprocess.run(words, stdin=stdin or subprocess.DEVNULL, capture_output=True, text=True, errors="replace")
            return ran.returncode, ran.stdout, ran.stderr
        return subprocess.run(words, stdin=stdin or subprocess.DEVNULL).returncode, "", ""

    def probe(self, programs):
        with open(PROBE, encoding="utf-8") as script:
            words = ["docker", "exec", "-i", self.name, "bash", "-s", "--"] + programs
            ran = subprocess.run(words, stdin=script, capture_output=True, text=True, errors="replace")
        return ran.returncode, ran.stdout, ran.stderr

    def copied(self):
        """The container started where it is not, and the tree's parts synced in from the host mount."""
        class Words:
            system = "linux"
            fresh = False

        return self.container.cmd_container_start(Words())


def workflows():
    """The programs sheet by workflow: {workflow: [row]}, each row its program, systems and purpose."""
    grouped = {}
    for row in table(PROGRAMS_TABLE):
        grouped.setdefault(row["workflow"], []).append(row)
    return grouped


def needed_tools():
    """The tools toolchains.json marks needed, each with its programs: the window runs on none of them absent."""
    with open(TOOLCHAINS, encoding="utf-8") as read:
        tools = json.load(read)["tools"]
    return [tool for tool in tools if tool.get("needed")]


def probe_read(text):
    """The probe's lines as a dict: system, manager, home, headers {name: yes/no} and programs {name: (path, version)}."""
    read = {"programs": {}, "headers": {}, "system": "", "manager": "none", "home": ""}
    for line in text.splitlines():
        cells = line.rstrip("\n").split("\t")
        if cells[0] == "system" and len(cells) >= 3:
            read["system"] = "%s %s" % (cells[1], cells[2])
        elif cells[0] == "manager" and len(cells) >= 2:
            read["manager"] = cells[1]
        elif cells[0] == "program" and len(cells) >= 4:
            read["programs"][cells[1]] = (cells[2] if cells[2] != "-" else "", cells[3] if cells[3] != "-" else "")
        elif cells[0] == "header" and len(cells) >= 3:
            read["headers"][cells[1]] = cells[2] == "yes"
        elif cells[0] == "home" and len(cells) >= 2:
            read["home"] = cells[1]
    return read


def packages_for(manager, names):
    """The packages `manager` installs for the programs and headers `names`, by window_packages.tsv; a name the sheet
    does not hold is given as itself."""
    rows = {row["program"]: row for row in table(PACKAGES_TABLE)}
    packages = []
    for name in names:
        package = rows.get(name, {}).get(manager, "") or name
        if package not in packages:
            packages.append(package)
    return packages


INSTALL_LINE = {
    "apt": "sudo apt-get update && sudo apt-get install -y --no-install-recommends %s",
    "dnf": "sudo dnf install -y %s",
    "pacman": "sudo pacman -S --needed --noconfirm %s",
    "apk": "sudo apk add %s",
    "zypper": "sudo zypper install -y %s",
}


def answered(read, is_image):
    """The three parts: (stops, helps, steps). `stops` names what the checks workflow and the needed tools lack, which
    ends the run; `helps` names what the other workflows lack, each with the workflow it stops; `steps` is the line that
    sets the host up, empty for the image."""
    stops = []
    helps = []
    missing = []
    for workflow, rows in workflows().items():
        for row in rows:
            path, _ = read["programs"].get(row["program"], ("", ""))
            if path:
                continue
            missing.append(row["program"])
            line = "%s: %s is not on the host's PATH, for %s" % (workflow, row["program"], row["for"])
            (stops if workflow in (CHECKS_WORKFLOW, "host") else helps).append(line)
    for tool in needed_tools():
        for program in tool.get("programs", []):
            path, _ = read["programs"].get(program, ("", ""))
            if not path and program not in missing:
                missing.append(program)
                stops.append("%s: %s is not on the host's PATH, for %s" % (tool["name"], program, tool.get("for", "")))
    for header, what in (("X11/Xlib.h", "the X11 headers"), ("X11/extensions/shape.h", "the X11 shape extension's headers"),
                         ("X11/extensions/XInput2.h", "the X input extension's headers")):
        if not read["headers"].get(header, True):
            missing.append(header)
            stops.append("%s: %s are not installed, which the window's system body is built against"
                         % (CHECKS_WORKFLOW, what))
    steps = ""
    if missing and not is_image:
        manager = read["manager"]
        if manager in INSTALL_LINE:
            steps = INSTALL_LINE[manager] % " ".join(packages_for(manager, missing))
        else:
            steps = "no package manager of apt, dnf, pacman, apk or zypper answers on the host; install: " + " ".join(missing)
    return stops, helps, steps


def programs_named():
    """Every program the sheet and the needed tools name, once each, the probe's words."""
    names = []
    for rows in workflows().values():
        for row in rows:
            if row["program"] not in names:
                names.append(row["program"])
    for tool in needed_tools():
        for program in tool.get("programs", []):
            if program not in names:
                names.append(program)
    return names


def check_command(tree, names, only, sanitize, owners):
    """The checks' command on the host, under X's display in memory, the harness there given the same words."""
    words = ["python3", "utils/test/harness.py", "window", "check"] + list(names)
    if only:
        words += ["--only", only]
    if sanitize:
        words += ["--sanitize", sanitize]
    if owners:
        words.append("--owners")
    return "cd %s && %s %s" % (tree, XVFB, " ".join(quoted(word) for word in words))


def streamed_run(host, command, lines):
    """`command` run on the host with each output line printed here and added to `lines`: the exit code."""
    if isinstance(host, ImageHost):
        words = ["docker", "exec", "-i", host.name, "bash", "-lc", command]
    else:
        words = host.ssh_words() + ["bash", "-lc", quoted(command)]
    child = subprocess.Popen(words, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    for line in child.stdout:
        line = line.rstrip("\n")
        print(line, flush=True)
        lines.append(line)
    return child.wait()


def report_written(run, host, read, stops, helps, steps, lines, code):
    """The run's report.md: the host, its reading in three parts, the summary line and every line the checks printed."""
    os.makedirs(run, exist_ok=True)
    summary = next((line for line in reversed(lines) if line.startswith("window check:")), "the checks printed no summary")
    out = ["# The window's checks on %s" % host.name, "", "System: %s. Package manager: %s." % (read["system"] or "not read", read["manager"]), ""]
    out += ["## What stops a workflow", ""] + (["- " + line for line in stops] or ["- nothing: the checks workflow has every program"]) + [""]
    out += ["## What would help", ""] + (["- " + line for line in helps] or ["- nothing: every workflow has its programs"]) + [""]
    if steps:
        out += ["## Setting the host up", "", "    " + steps, ""]
    out += ["## Programs read", ""]
    for name, (path, version) in sorted(read["programs"].items()):
        out.append("- %s: %s" % (name, ("%s, %s" % (path, version)) if path else "not on PATH"))
    out += ["", "## The checks", "", summary, "", "Exit %d." % code, "", "```", *lines, "```", ""]
    path = os.path.join(run, "report.md")
    with open(path, "w", encoding="utf-8", newline="\n") as write:
        write.write("\n".join(out))
    return path


def checks_on_host(a):
    """The window's checks on the host --on names, the preference's where it names none, or the image with --docker:
    the host read and answered in three parts, then the tree copied and the checks run and reported."""
    if a.docker:
        host = ImageHost()
    else:
        named = a.on if a.on not in (None, "") else host_from_preference()
        if not named:
            print("no host: give --on user@host, or set checks.host in Preferences or at install with --checks-host")
            return 2
        host = SshHost(named)
    run = os.path.join(RUNS, time.strftime("%Y%m%d_%H%M%S"))
    if host.is_image:
        # the image's container must run before it is read
        code = host.copied()
        if code != 0:
            return code
    print("reading %s" % host.name, flush=True)
    code, out, err = host.probe(programs_named())
    if code != 0:
        print("%s could not be read: %s" % (host.name, err.strip() or out.strip()))
        return 2
    read = probe_read(out)
    stops, helps, steps = answered(read, host.is_image)
    print("  %s, package manager %s" % (read["system"], read["manager"]))
    for line in stops:
        print("  stops " + line)
    for line in helps:
        print("  would help " + line)
    if steps:
        print("  set the host up: " + steps)
    if stops:
        report = report_written(run, host, read, stops, helps, steps, [], 2)
        print("the checks cannot run on %s; the report is %s" % (host.name, os.path.relpath(report, ROOT)))
        return 2
    if not host.is_image:
        print("copying the tree's parts to %s" % host.name, flush=True)
        code = host.copied()
        if code != 0:
            print("the copy ended %d" % code)
            return code
    lines = []
    code = streamed_run(host, check_command(host.tree, a.names, a.only, a.sanitize, a.owners), lines)
    report = report_written(run, host, read, stops, helps, steps, lines, code)
    print("the report is %s" % os.path.relpath(report, ROOT), flush=True)
    return code
