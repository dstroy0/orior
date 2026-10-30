# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py container: the window's systems as containers that stay up, each named orior-ui-<system> and holding a
copy of the tree of its own, where the window is built and run on that system.

    harness.py container list                      each system window_images.tsv gives a platform, its container's state
    harness.py container build SYSTEM...           its image built for its platform, or pulled where it is built elsewhere
    harness.py container start SYSTEM [--fresh]    its container made where there is none, started, and the tree copied in;
                                                   --fresh makes it again, its volumes kept
    harness.py container sync SYSTEM               the tree's parts copied into the container's copy
    harness.py container exec SYSTEM -- COMMAND... a command run in the container's copy of ui
    harness.py container stop SYSTEM
    harness.py container remove SYSTEM [--volumes] the container taken away, and with --volumes its copy and builds
    harness.py container engine linux|windows      Docker Desktop's engine switched, and waited on until it answers

The systems are the rows of utils/test/window_images.tsv with a platform: linux, Ubuntu 24.04 on x86-64; ubuntu_arm64,
the same on arm64; rpi, Debian bookworm on arm64, as Raspberry Pi OS is; windows, Windows Server 2022; and macos.

A Linux container reads the tree at /host, read-only, and keeps its copy on a volume at /top, its builds on a second
at /top/build, and its crates on the volume the window's suite keeps them on. The tree's parts the window's build reads
are copied in with rsync, each file where its bytes differ: ui, utils, src/cu and bin_names.tsv. An arm64 container
runs under Docker's emulation, at a fraction of the machine's speed. The Windows container is the same at C:\\host and
C:\\top, copied with robocopy, and runs only while Docker's engine is Windows', which `engine` switches to and back.

macOS runs as a virtual machine under KVM in a Linux container, dockurr/macos: its viewer is at
http://localhost:8006, its disk on the volume orior-ui-macos-storage, and the tree is shared read-only at /shared,
which macOS mounts with `sudo -S mount_9p shared`. Its first start installs macOS through the viewer, and what runs in
it is reached there, never by exec or sync. Apple's license for macOS allows it on Apple's hardware alone."""

import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
BUILD = findroot.at("build")
IMAGES_TABLE = findroot.at("utils", "test", "window_images.tsv")
DOCKER_CLI = r"C:\Program Files\Docker\Docker\DockerCli.exe"
DOCKER_INSTALL_SETTINGS = r"C:\ProgramData\DockerDesktop\install-settings.json"
ENGINE_SWITCH = {"linux": "-SwitchLinuxEngine", "windows": "-SwitchWindowsEngine"}
ENGINE_WAIT_S = 300
ENGINE_POLL_S = 5
# the parts of ui each copy keeps of its own: what is built there and the suite's runs
UI_KEPT = ["cli/target", "gui/src-tauri/target", "test/runs"]
MACOS_VERSION = "15"
MACOS_RAM = "8G"
MACOS_CORES = "4"
MACOS_VIEWER = "http://localhost:8006"


def table():
    with open(IMAGES_TABLE, encoding="utf-8") as kept:
        head = kept.readline().rstrip("\n").split("\t")
        rows = [line.rstrip("\n").split("\t") for line in kept if line.strip()]
        return [dict(zip(head, cells + [""] * (len(head) - len(cells)))) for cells in rows]


def systems():
    return [row for row in table() if row["platform"]]


def system_named(wanted):
    for row in systems():
        if row["id"] == wanted:
            return row
    raise SystemExit("window_images.tsv names no system %s; it names %s" % (wanted, ", ".join(row["id"] for row in systems())))


def cargo_volume():
    for row in table():
        if row["id"] == "cargo_volume":
            return row["image"]
    raise SystemExit("window_images.tsv names no cargo_volume")


def container_of(system):
    return "orior-ui-" + system["id"].replace("_", "-")


def engine_of(system):
    return system["platform"].split("/")[0]


def is_macos(system):
    return system["id"] == "macos"


def docker(words, **kw):
    return subprocess.run(["docker", *words], stdin=subprocess.DEVNULL, **kw)


def docker_said(words):
    ran = docker(words, capture_output=True, text=True, errors="replace")
    return ran.returncode, ran.stdout.strip()


def engine_now():
    code, said = docker_said(["info", "--format", "{{.OSType}}"])
    return said if code == 0 else ""


def engine_ready(system):
    """Whether Docker's engine runs the system's containers; where it does not, what switches it is said."""
    now = engine_now()
    wanted = engine_of(system)
    if now == wanted:
        return True
    if not now:
        print("Docker's engine does not answer")
    else:
        print("Docker's engine runs %s containers here; %s's takes %s: harness.py container engine %s" % (now, system["id"], wanted, wanted))
    return False


def image_present(system):
    return docker_said(["image", "inspect", "--format", "{{.Id}}", system["image"]])[0] == 0


def state_of(name):
    code, said = docker_said(["container", "inspect", "--format", "{{.State.Status}}", name])
    return said if code == 0 else ""


def image_made(system):
    """The system's image built from its folder for its platform, with its build's arguments, or pulled where it has
    no folder; the build's whole output goes to build/container_<system>.log."""
    os.makedirs(BUILD, exist_ok=True)
    log = os.path.join(BUILD, "container_%s.log" % system["id"])
    if system["folder"]:
        words = ["build", "-t", system["image"]]
        if engine_of(system) == "linux":
            words += ["--platform", system["platform"]]
        for one in system["args"].split():
            words += ["--build-arg", one]
        words.append(findroot.at(*system["folder"].split("/")))
    else:
        words = ["pull", "--platform", system["platform"], system["image"]]
    print("%s: %s, its output to %s" % (system["id"], " ".join(["docker"] + words[:-1]), log), flush=True)
    with open(log, "wb") as out:
        made = docker(words, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
    with open(log, encoding="utf-8", errors="replace") as kept:
        lines = kept.readlines()
    print("".join(lines[-12 if made.returncode else -1:]), end="")
    return made.returncode


def linux_run_words(system, name):
    return [
        "run", "-d", "--name", name, "--platform", system["platform"], "--init", "--shm-size=1g", "--add-host", "host.docker.internal:host-gateway",
        "--mount", "type=bind,source=%s,target=/host,readonly" % ROOT,
        "-v", "%s-tree:/top" % name, "-v", "%s-build:/top/build" % name, "-v", "%s:/root/.cargo/registry" % cargo_volume(),
        "-w", "/top", "--entrypoint", "sleep", system["image"], "infinity",
    ]


def windows_run_words(system, name):
    return [
        "run", "-d", "--name", name,
        "--mount", "type=bind,source=%s,target=C:\\host,readonly" % ROOT,
        "-v", "%s-tree:C:\\top" % name, "-v", "%s-build:C:\\top\\build" % name,
        "-w", "C:\\top", "--entrypoint", "powershell", system["image"], "-NoLogo", "-Command", "Wait-Event",
    ]


def macos_run_words(system, name):
    return [
        "run", "-d", "--name", name, "-e", "VERSION=" + MACOS_VERSION, "-e", "RAM_SIZE=" + MACOS_RAM, "-e", "CPU_CORES=" + MACOS_CORES,
        "--device", "/dev/kvm", "--device", "/dev/net/tun", "--cap-add", "NET_ADMIN",
        "-p", "127.0.0.1:8006:8006", "-p", "127.0.0.1:5900:5900",
        "-v", "%s-storage:/storage" % name, "--mount", "type=bind,source=%s,target=/shared,readonly" % ROOT,
        "--stop-timeout", "120", system["image"],
    ]


def run_words(system, name):
    if is_macos(system):
        return macos_run_words(system, name)
    if engine_of(system) == "windows":
        return windows_run_words(system, name)
    return linux_run_words(system, name)


def linux_sync():
    copied = "rsync -a --checksum --no-times --delete --exclude __pycache__"
    kept = " ".join("--exclude /" + part for part in UI_KEPT)
    return " && ".join([
        "mkdir -p /top/src/cu",
        "%s %s /host/ui/ /top/ui/" % (copied, kept),
        "%s /host/utils/ /top/utils/" % copied,
        "%s /host/src/cu/ /top/src/cu/" % copied,
        "cp /host/bin_names.tsv /top/bin_names.tsv",
    ])


def windows_sync():
    """robocopy's mirror of each part, the folders each copy keeps left out on both sides; its exit codes below 8
    are copies made."""
    lines = []
    for part, kept in (("ui", UI_KEPT), ("utils", []), ("src\\cu", [])):
        left_out = ["__pycache__"]
        for one in kept:
            left_out += ["C:\\host\\%s\\%s" % (part, one.replace("/", "\\")), "C:\\top\\%s\\%s" % (part, one.replace("/", "\\"))]
        lines.append("robocopy C:\\host\\%s C:\\top\\%s /MIR /XD %s /NFL /NDL /NJH /NJS /NP | Out-Null; if ($LASTEXITCODE -ge 8) { exit $LASTEXITCODE }" % (part, part, " ".join(left_out)))
    lines.append("Copy-Item C:\\host\\bin_names.tsv C:\\top\\bin_names.tsv; exit 0")
    return "; ".join(lines)


def synced(system):
    name = container_of(system)
    if engine_of(system) == "windows":
        words = ["exec", name, "powershell", "-NoLogo", "-Command", windows_sync()]
    else:
        words = ["exec", name, "sh", "-c", linux_sync()]
    print("%s: the tree copied in" % name, flush=True)
    ran = docker(words)
    if ran.returncode != 0:
        print("%s: the copy ended %d" % (name, ran.returncode))
    return ran.returncode


def cmd_container_list(a):
    now = engine_now()
    print("Docker's engine: %s" % (now or "not answering"))
    for system in systems():
        name = container_of(system)
        if engine_of(system) != now:
            print("%-22s %-34s %-14s not reachable on this engine" % (name, system["image"], system["platform"]))
            continue
        image = "built" if image_present(system) else "not built"
        print("%-22s %-34s %-14s image %s, container %s" % (name, system["image"], system["platform"], image, state_of(name) or "not made"))
    return 0


def cmd_container_build(a):
    worst = 0
    for wanted in a.systems:
        system = system_named(wanted)
        if not engine_ready(system):
            return 2
        worst = max(worst, image_made(system))
    return worst


def cmd_container_start(a):
    system = system_named(a.system)
    if not engine_ready(system):
        return 2
    name = container_of(system)
    if a.fresh and state_of(name):
        docker(["rm", "-f", name], stdout=subprocess.DEVNULL)
    if not image_present(system):
        code = image_made(system)
        if code != 0:
            return code
    state = state_of(name)
    if not state:
        made = docker(run_words(system, name), stdout=subprocess.DEVNULL)
        if made.returncode != 0:
            return made.returncode
    elif state != "running":
        started = docker(["start", name], stdout=subprocess.DEVNULL)
        if started.returncode != 0:
            return started.returncode
    print("%s: %s" % (name, state_of(name)))
    if is_macos(system):
        print("%s: macOS's viewer is at %s; the tree is shared at /shared, mounted in macOS with sudo -S mount_9p shared" % (name, MACOS_VIEWER))
        return 0
    return synced(system)


def cmd_container_sync(a):
    system = system_named(a.system)
    if is_macos(system):
        print("macOS reads the tree shared at /shared; nothing is copied in")
        return 2
    if not engine_ready(system):
        return 2
    if state_of(container_of(system)) != "running":
        print("%s is not running: harness.py container start %s" % (container_of(system), system["id"]))
        return 2
    return synced(system)


def cmd_container_exec(a):
    system = system_named(a.system)
    if is_macos(system):
        print("macOS runs its commands through its viewer, %s" % MACOS_VIEWER)
        return 2
    if not engine_ready(system):
        return 2
    words = a.words[1:] if a.words[:1] == ["--"] else a.words
    if not words:
        print("no command given, after --")
        return 2
    folder = "C:\\top\\ui" if engine_of(system) == "windows" else "/top/ui"
    return subprocess.run(["docker", "exec", "-i", "-w", folder, container_of(system), *words]).returncode


def cmd_container_stop(a):
    system = system_named(a.system)
    if not engine_ready(system):
        return 2
    return docker(["stop", container_of(system)], stdout=subprocess.DEVNULL).returncode


def cmd_container_remove(a):
    system = system_named(a.system)
    if not engine_ready(system):
        return 2
    name = container_of(system)
    if state_of(name):
        removed = docker(["rm", "-f", name], stdout=subprocess.DEVNULL)
        if removed.returncode != 0:
            return removed.returncode
    if a.volumes:
        volumes = [name + "-storage"] if is_macos(system) else [name + "-tree", name + "-build"]
        for volume in volumes:
            if docker_said(["volume", "inspect", volume])[0] == 0:
                gone = docker(["volume", "rm", volume], stdout=subprocess.DEVNULL)
                if gone.returncode != 0:
                    return gone.returncode
    return 0


def windows_containers_refused():
    """Whether this machine refuses Windows containers, and what an administrator changes where it does: Docker
    Desktop installed with noWindowsContainers, or Windows' Containers feature off."""
    refused = False
    try:
        with open(DOCKER_INSTALL_SETTINGS, encoding="utf-8") as kept:
            if json.load(kept).get("noWindowsContainers"):
                print("Docker Desktop was installed with noWindowsContainers, in %s: an administrator sets it false and restarts Docker Desktop" % DOCKER_INSTALL_SETTINGS)
                refused = True
    except (OSError, ValueError):
        pass
    feature = subprocess.run(["powershell", "-NoProfile", "-Command", "(Get-CimInstance Win32_OptionalFeature -Filter \"Name='Containers'\").InstallState"],
                             stdin=subprocess.DEVNULL, capture_output=True, text=True)
    if feature.stdout.strip() not in ("", "1"):
        print("Windows' Containers feature is off: an administrator runs Enable-WindowsOptionalFeature -Online -FeatureName Containers -All, and restarts Windows")
        refused = True
    return refused


def cmd_container_engine(a):
    if engine_now() == a.engine:
        print("Docker's engine runs %s containers" % a.engine)
        return 0
    if not os.path.isfile(DOCKER_CLI):
        print("%s is not here: Docker Desktop's engine is switched from its own menu" % DOCKER_CLI)
        return 2
    if a.engine == "windows" and windows_containers_refused():
        return 2
    subprocess.run([DOCKER_CLI, ENGINE_SWITCH[a.engine]], stdin=subprocess.DEVNULL)
    deadline = time.time() + ENGINE_WAIT_S
    while time.time() < deadline:
        if engine_now() == a.engine:
            print("Docker's engine runs %s containers" % a.engine)
            return 0
        time.sleep(ENGINE_POLL_S)
    print("Docker's engine did not answer as %s's within %d seconds" % (a.engine, ENGINE_WAIT_S))
    return 1


def register(sub):
    """The container group's commands added to harness.py's subparsers."""
    import argparse

    group = sub.add_parser("container", help="the window's systems as containers that stay up, each with its own copy of the tree")
    container = group.add_subparsers(dest="command", required=True)
    p = container.add_parser("list", help="each system, its image and its container's state")
    p.set_defaults(fn=cmd_container_list)
    p = container.add_parser("build", help="the systems' images built for their platforms, or pulled")
    p.add_argument("systems", nargs="+", help="the systems, by their ids in window_images.tsv")
    p.set_defaults(fn=cmd_container_build)
    p = container.add_parser("start", help="the system's container made where there is none, started, and the tree copied in")
    p.add_argument("system")
    p.add_argument("--fresh", action="store_true", help="the container made again, its volumes kept")
    p.set_defaults(fn=cmd_container_start)
    p = container.add_parser("sync", help="the tree's parts copied into the container's copy")
    p.add_argument("system")
    p.set_defaults(fn=cmd_container_sync)
    p = container.add_parser("exec", help="a command run in the container's copy of ui")
    p.add_argument("system")
    p.add_argument("words", nargs=argparse.REMAINDER, help="the command and its words, after --")
    p.set_defaults(fn=cmd_container_exec)
    p = container.add_parser("stop", help="the system's container stopped")
    p.add_argument("system")
    p.set_defaults(fn=cmd_container_stop)
    p = container.add_parser("remove", help="the system's container taken away")
    p.add_argument("system")
    p.add_argument("--volumes", action="store_true", help="its copy of the tree and its builds taken away with it")
    p.set_defaults(fn=cmd_container_remove)
    p = container.add_parser("engine", help="Docker Desktop's engine switched to Linux's or Windows'")
    p.add_argument("engine", choices=sorted(ENGINE_SWITCH))
    p.set_defaults(fn=cmd_container_engine)
