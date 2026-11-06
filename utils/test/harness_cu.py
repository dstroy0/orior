# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py cu: every .cu under src/cu given a thin wrapper, and nvcc building the wrappers through CMake.

    harness.py cu wrappers          each source's wrapper written under utils/test/src/cu/unit, a wrapper whose source is gone removed
    harness.py cu build [name...]   nvcc builds the wrappers into build/cu_unit, every one where none is named, each by its source's name
    harness.py cu test [name...]    the Unity suites of the .cu sources generated, built by nvcc and run, every one where none is named

A .cu is C++ for nvcc: its static functions, kernels and device functions have no C linkage, and many of its signatures name std:: types
no C caller could write. A wrapper is a translation unit that includes its source, and a Unity suite nvcc compiles includes the wrapper:
the suite then calls every function of the source. A source with a `main` has it renamed <source>_main, the suite's runner holding the
program's main. No two sources share a header's name, as utils/maint/engine/clangd_database.py checks, and one include path serves them all."""

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
SOURCES = os.path.join(ROOT, "src", "cu")
SUITES = os.path.join("utils", "test", "src", "cu", "unit")
WRAPPERS = os.path.join(ROOT, SUITES)
BUILD = os.path.join(ROOT, "build", "cu_unit")
SUFFIX = "_wrap.cu"
HEAD = "// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>\n// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational\n"
PROGRAM_ENTRY = re.compile(r"^\s*int\s+main\s*\(", re.M)


def sources():
    """Every .cu under src/cu but MMgr's and embedded_types', which have their own suites, each by its path from src/cu with forward
    slashes, sorted."""
    found = []
    for top, directories, files in os.walk(SOURCES):
        if os.path.normpath(top) == os.path.join(SOURCES, "includes"):
            directories[:] = [name for name in directories if name not in ("mmgr", "embedded_types")]
        found += [os.path.relpath(os.path.join(top, name), SOURCES).replace("\\", "/") for name in files if name.endswith(".cu")]
    return sorted(found)


def wrapper_text(relative):
    """The wrapper of the source at `relative`, its `main` renamed where it has one."""
    name = os.path.basename(relative)[: -len(".cu")]
    with open(os.path.join(SOURCES, relative), encoding="utf-8", errors="replace") as source:
        has_entry = PROGRAM_ENTRY.search(source.read()) is not None
    lines = [HEAD + "// %s%s: src/cu/%s as a translation unit a Unity suite includes, every function of the source in reach of the suite\n" % (name, SUFFIX, relative)]
    if has_entry:
        lines.append("#define main %s_main\n" % name)
    lines.append('#include "%s"\n' % relative)
    if has_entry:
        lines.append("#undef main\n")
    return "".join(lines)


def wrapper_path(relative):
    """Where the wrapper of the source at `relative` lies."""
    return os.path.join(WRAPPERS, *relative[: -len(".cu")].split("/")[:-1], os.path.basename(relative)[: -len(".cu")] + SUFFIX)


def cmd_cu_wrappers(a):
    wanted = {}
    for relative in sources():
        wanted[os.path.normpath(wrapper_path(relative))] = wrapper_text(relative)
    written = 0
    for path, text in wanted.items():
        current = None
        if os.path.isfile(path):
            with open(path, encoding="utf-8", newline="") as old:
                current = old.read()
        if current != text:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8", newline="\n") as out:
                out.write(text)
            written += 1
    removed = 0
    for top, _, files in os.walk(WRAPPERS):
        for name in files:
            path = os.path.normpath(os.path.join(top, name))
            if name.endswith(SUFFIX) and path not in wanted:
                os.remove(path)
                removed += 1
    print("cu wrappers: %d sources, %d written, %d removed" % (len(wanted), written, removed))
    return 0


def msvc_compiler():
    """The newest MSVC cl.exe, nvcc's host compiler on Windows, or None."""
    found = []
    for base in (os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"), os.environ.get("ProgramFiles", r"C:\Program Files")):
        studio = os.path.join(base, "Microsoft Visual Studio")
        for top, directories, files in os.walk(studio):
            if "cl.exe" in files and top.lower().endswith(os.path.join("hostx64", "x64")):
                found.append(os.path.join(top, "cl.exe"))
                directories[:] = []
    return sorted(found)[-1] if found else None


def msvc_env(compiler):
    """The environment vcvars64.bat sets beside the cl.exe at `compiler`: CMake links its test programs through MSVC's linker and
    resource compiler, which read their tools and libraries from it."""
    studio = compiler[: compiler.lower().index(os.path.join("vc", "tools").lower())]
    script = os.path.join(studio, "VC", "Auxiliary", "Build", "vcvars64.bat")
    ran = subprocess.run('"%s" >nul && set' % script, shell=True, capture_output=True, text=True, check=False)
    return dict(line.split("=", 1) for line in ran.stdout.splitlines() if "=" in line)


def built(targets):
    """The CMake project configured and `targets` built, every target where none are named: the exit code. Ninja's -k 0 goes on past a
    target that does not compile, which lists every one that does not in one build."""
    cmake = shutil.which("cmake") or os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "CMake", "bin", "cmake.exe")
    configure = [cmake, "-S", WRAPPERS, "-B", BUILD, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
    env = None
    if sys.platform == "win32":
        compiler = msvc_compiler()
        if compiler is None:
            print("cu: no MSVC cl.exe for nvcc's host compiler")
            return 1
        configure += ["-DORIOR_CU_HOST=" + compiler.replace("\\", "/")]
        env = msvc_env(compiler)
    code = subprocess.run(configure, cwd=ROOT, env=env, check=False).returncode
    if code != 0:
        return code
    chosen = ["--target", *targets] if targets else []
    return subprocess.run([cmake, "--build", BUILD, *chosen, "--", "-k", "0"], cwd=ROOT, env=env, check=False).returncode


def cmd_cu_build(a):
    return built(["wrap_" + name for name in a.names])


def cmd_cu_test(a):
    harness_unity.lowered_priority()
    unity = harness_unity.unity_folder(ROOT)
    suites = harness_unity.suite_dirs(ROOT, a.names, SUITES, ".cu")
    for suite_dir in suites:
        harness_unity.generate_runner(ROOT, suite_dir, unity, ".cu")
    names = [os.path.basename(suite_dir) for suite_dir in suites]
    code = built(names)
    run = os.path.join(BUILD, "run")
    os.makedirs(run, exist_ok=True)
    env = harness_unity.suite_env()
    suffix = harness_unity.host_target()[1]
    tests = 0
    failures = 0
    failing = []
    for name in names:
        program = os.path.join(BUILD, "suites", name + suffix)
        if not os.path.isfile(program):
            print("  %s did not build" % name, flush=True)
            failing.append(name)
            continue
        ran_tests, ran_failures, failed = harness_unity.program_ran(name, program, run, env)
        tests += ran_tests
        failures += ran_failures
        if failed:
            failing.append(name)
    return harness_unity.totals_printed(len(suites), tests, failures, failing, "cu") or code


def register(sub):
    """The cu group's commands added to harness.py's subparsers."""
    group = sub.add_parser("cu", help="every .cu under src/cu given a thin wrapper, and nvcc building the wrappers through CMake")
    cu = group.add_subparsers(dest="command", required=True)
    p = cu.add_parser("wrappers", help="each source's wrapper written under utils/test/src/cu/unit, a wrapper whose source is gone removed")
    p.set_defaults(fn=cmd_cu_wrappers)
    p = cu.add_parser("build", help="nvcc builds the wrappers through CMake into build/cu_unit, every one where none is named")
    p.add_argument("names", nargs="*", help="source names without .cu, such as division")
    p.set_defaults(fn=cmd_cu_build)
    p = cu.add_parser("test", help="the Unity suites of the .cu sources generated, built by nvcc and run, every one where none is named")
    p.add_argument("names", nargs="*", help="source names without .cu, such as division")
    p.set_defaults(fn=cmd_cu_test)
