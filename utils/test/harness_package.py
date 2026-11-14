# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py package: the install bundle of a target, impero and tabvla with the license texts in one archive.
    harness.py package                    the host's target built and bundled
    harness.py package --target T         the target T, as ui/cli/build.sh names one
    harness.py package --no-build         the programs as they stand in build/cli/<target>, nothing built
The bundle is build/package/orior-<version>-<target>/ with imp, tab, LICENSE, LICENSES/ and INSTALL.txt, and beside it
the archive of that folder: a .zip for a Windows target, a .tar.gz for every other. Unpacked anywhere, `imp file
install` in the folder installs it; `imp file uninstall` takes it out again.
"""
import os
import re
import shutil
import subprocess
import sys
import tarfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
VERSION_HEADER = os.path.join(ROOT, "ui", "cli", "src", "command_line", "cli_internal.h")
INSTALL_TEXT = (
    "orior: impero (imp) and tabvla (tab)\n"
    "\n"
    "Windows: open a terminal in this folder and run  imp.exe file install\n"
    "Linux and macOS: open a terminal in this folder and run  ./imp file install\n"
    "\n"
    "The install shows the license and asks you to accept it, copies the programs into\n"
    "your programs folder or the folder you name, and puts imp and tab on your PATH\n"
    "where neither name is taken. --no-imp and --no-tab leave a name out, --aliases\n"
    "writes shell aliases in place of links, and --debug-port sets the window's\n"
    "debugging port. imp file uninstall takes everything out again; with --files it\n"
    "removes your settings too.\n"
)


def version():
    """The command line's version, as cli_internal.h defines it."""
    with open(VERSION_HEADER, encoding="utf-8") as header:
        found = re.search(r'#define CLI_VERSION "([^"]+)"', header.read())
    return found.group(1) if found else "0"


def program_names():
    """The two programs' file names, cli's then gui's, from bin_names.tsv."""
    names = {}
    with open(os.path.join(ROOT, "bin_names.tsv"), encoding="utf-8") as table:
        for line in table:
            parts = line.rstrip("\n").split("\t")
            if len(parts) > 1:
                names[parts[0]] = parts[1]
    return names["cli"], names["gui"]


def libraries_resolved(into, cli_name, gui_name):
    """Every shared library the installed programs load found on this system, read with ldd on Linux and otool on
    macOS, each program's libraries printed: 1, or 0 with the ones not found printed."""
    reader = ["otool", "-L"] if sys.platform == "darwin" else ["ldd"]
    if shutil.which(reader[0]) is None:
        print("%s is not on this system: the libraries are not read" % reader[0], flush=True)
        return 1
    whole = 1
    for program in (cli_name, gui_name):
        path = os.path.join(into, program)
        if not os.path.isfile(path):
            continue
        read = subprocess.run(reader + [path], capture_output=True, text=True)
        lines = [line.strip() for line in (read.stdout + read.stderr).splitlines() if line.strip()]
        print("%s loads:" % program, flush=True)
        for line in lines:
            print("  " + line, flush=True)
        lost = [line for line in lines if "not found" in line]
        if read.returncode != 0 or lost:
            print("%s: libraries not found: %s" % (program, ", ".join(lost) or "the reader failed"), flush=True)
            whole = 0
    return whole


def install_checked(bundle, cli_name, gui_name, suffix, windows):
    """The bundle installed into build/package/check_install with every question answered on the command line, the
    install folder read for what the install says it places, and the install taken out again: 0, or 1 with what is
    missing printed. The uninstall on Windows removes the folder a moment after its program ends, so the folder is
    read again for a few seconds."""
    import time

    into = os.path.join(ROOT, "build", "package", "check_install")
    shutil.rmtree(into, ignore_errors=True)
    program = os.path.join(bundle, cli_name + suffix)
    env = dict(os.environ)
    env["ORIOR_HOME"] = os.path.join(ROOT, "build", "package", "check_home")
    shutil.rmtree(env["ORIOR_HOME"], ignore_errors=True)
    words = [program, "file", "install", into, "--accept", "--yes", "--debug-port", "off"]
    installed = subprocess.run(words, cwd=ROOT, env=env, capture_output=True, text=True)
    print(installed.stdout, end="", flush=True)
    if installed.returncode != 0:
        print(installed.stderr, end="", flush=True)
        print("the install ended with %d" % installed.returncode, flush=True)
        return 1
    expected = [cli_name + suffix, "LICENSE", os.path.join("LICENSES", "AGPL-3.0-or-later.txt"), "install.tsv"]
    if os.path.isfile(os.path.join(bundle, gui_name + suffix)):
        expected.append(gui_name + suffix)
    if windows:
        expected.append(os.path.join("bin", cli_name + ".cmd"))
    missing = [one for one in expected if not os.path.isfile(os.path.join(into, one))]
    if missing:
        print("the install left out: " + ", ".join(missing), flush=True)
        return 1
    if not windows and not libraries_resolved(into, cli_name, gui_name):
        return 1
    state = os.path.join(env["ORIOR_HOME"], "state.json")
    if not os.path.isfile(state):
        print("the install wrote no %s" % state, flush=True)
        return 1
    with open(state, encoding="utf-8") as kept:
        if "debug-port" not in kept.read():
            print("the install wrote no debugging port default", flush=True)
            return 1
    removed = subprocess.run([os.path.join(into, cli_name + suffix), "file", "uninstall", "--yes"], cwd=ROOT, env=env, capture_output=True, text=True)
    print(removed.stdout, end="", flush=True)
    if removed.returncode != 0:
        print(removed.stderr, end="", flush=True)
        print("the uninstall ended with %d" % removed.returncode, flush=True)
        return 1
    for _ in range(40):
        if not os.path.isdir(into):
            break
        time.sleep(0.25)
    if os.path.isdir(into):
        print("the uninstall left %s" % into, flush=True)
        return 1
    shutil.rmtree(env["ORIOR_HOME"], ignore_errors=True)
    print("installed into and removed from %s" % into, flush=True)
    return 0


def cmd_package(a):
    import harness_cli
    import harness_window

    target = a.target or harness_window.host_target()
    windows = "windows" in target
    suffix = ".exe" if windows else ""
    if not a.no_build:
        code = harness_cli.streamed([harness_cli.bash(), harness_cli.CLI_BUILD, target])
        if code != 0:
            print("the build ended with %d" % code, flush=True)
            return code
        # the window is not built for every system; a bundle without it installs the command line alone
        if harness_cli.streamed([harness_cli.bash(), harness_cli.CLI_BUILD, "window", target]) != 0:
            print("the window did not build for %s: the bundle carries the command line alone" % target, flush=True)
    built = os.path.join(ROOT, "build", "cli", target)
    cli_name, gui_name = program_names()
    name = "orior-%s-%s" % (version(), target)
    bundle = os.path.join(ROOT, "build", "package", name)
    shutil.rmtree(bundle, ignore_errors=True)
    os.makedirs(bundle)
    for program in (cli_name, gui_name):
        source = os.path.join(built, program + suffix)
        if not os.path.isfile(source):
            if program == cli_name:
                print("%s is not built" % source, flush=True)
                return 1
            continue
        shutil.copy2(source, os.path.join(bundle, program + suffix))
    shutil.copy2(os.path.join(ROOT, "LICENSE"), os.path.join(bundle, "LICENSE"))
    shutil.copytree(os.path.join(ROOT, "LICENSES"), os.path.join(bundle, "LICENSES"))
    with open(os.path.join(bundle, "INSTALL.txt"), "w", encoding="utf-8", newline="\r\n" if windows else "\n") as text:
        text.write(INSTALL_TEXT)
    if windows:
        archive = bundle + ".zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as out:
            for folder, _, files in os.walk(bundle):
                for file in files:
                    path = os.path.join(folder, file)
                    out.write(path, os.path.join(name, os.path.relpath(path, bundle)))
    else:
        archive = bundle + ".tar.gz"
        with tarfile.open(archive, "w:gz") as out:
            out.add(bundle, arcname=name)
    print("%s: %d bytes" % (os.path.relpath(archive, ROOT), os.path.getsize(archive)), flush=True)
    if a.check:
        return install_checked(bundle, cli_name, gui_name, suffix, windows)
    return 0


def register(sub):
    """The package command added to harness.py's subparsers."""
    p = sub.add_parser("package", help="the install bundle of a target: imp, tab and the license texts archived under build/package")
    p.add_argument("--target", default=None, help="the target built and bundled, the host's where none")
    p.add_argument("--no-build", action="store_true", help="the programs as they stand in build/cli/<target>, nothing built")
    p.add_argument("--check", action="store_true", help="the bundle installed into build/package/check_install, read, and taken out again")
    p.set_defaults(fn=cmd_package)
