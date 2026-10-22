# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py build and harness.py source: the window's program and MMgr's suites built, and the C sources read by
the compiler and the linker's tools.

    harness.py build gui                      the window's release program, every window running from it ended first
    harness.py build mmgr [--suffix S] [--flags F]
                                              MMgr's Unity suites under build/mmgr_tests<S>, the accuracy suites run
    harness.py build image                    the Linux test image, from ui/test/docker
    harness.py source tidy                    clang-tidy with the static analyzer, and cppcheck, in that image, the
                                              reports under build/sonarqube for the SonarQube scan
    harness.py source syntax <source>...      each C source checked by the compiler alone, nothing built
    harness.py source tests-syntax            every ui test's C file checked, the errors to build/harness/out
    harness.py source frames                  each CLI function whose stack frame is 64 KB or more
    harness.py source static-sizes <list>     each object's .bss and .data bytes, the largest last
    harness.py source static-symbols <name>...  the zero-filled statics of a CLI object, largest first
    harness.py source format                  clang-format over ui/cli/src and ui/query/src, the files changed listed

Logs and listings go under build/ and build/harness/out."""

import concurrent.futures
import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
BUILD = os.path.join(ROOT, "build")
OUT = os.path.join(BUILD, "harness", "out")
MMGR = os.path.join(ROOT, "src", "cu", "includes", "mmgr")
EMBEDDED_TYPES = os.path.join(ROOT, "src", "cu", "includes", "embedded_types")
MMGR_FLAGS = ["-I", os.path.join(MMGR, "include"), "-I", os.path.join(MMGR, "src"), "-I", os.path.join(EMBEDDED_TYPES, "include"), "-DMMGR_ENABLE_DMA=0", "-DMMGR_ENABLE_EXTRAM=0", "-DMMGR_ALIGN_BYTES=32", "-DMMGR_ENABLE_HOST=1"]
RUBY = r"C:\tools\ruby34\bin"
FRAME_LARGEST = 65536
FRAME_JOBS = 16
MIB = 1048576.0


def tail(path, count):
    with open(path, encoding="utf-8", errors="replace") as kept:
        return "".join(kept.readlines()[-count:])


def grep(path, pattern, count):
    with open(path, encoding="utf-8", errors="replace") as kept:
        return [line.rstrip("\n") for line in kept if re.search(pattern, line)][:count]


def cmd_build_gui(a):
    import harness_window

    if os.name == "nt":
        harness_window.windows_ended()
    log = os.path.join(BUILD, "gui_build.log")
    with open(log, "wb") as out:
        built = subprocess.run(["cargo", "build", "--release", "--manifest-path", os.path.join(ROOT, "ui", "gui", "src-tauri", "Cargo.toml")], cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
    if built.returncode != 0:
        print("\n".join(grep(log, r"^error", 5)))
        print(tail(log, 3), end="")
        return 1
    print(tail(log, 1), end="")
    return 0


def cmd_build_mmgr(a):
    tree = os.path.join(BUILD, "mmgr_tests" + a.suffix)
    env = dict(os.environ)
    env["PATH"] = RUBY + os.pathsep + env.get("PATH", "")
    configure_log = tree + "_configure.log"
    build_log = tree + "_build.log"
    with open(configure_log, "wb") as out:
        configured = subprocess.run(
            ["cmake", "-S", MMGR, "-B", tree, "-G", "Ninja", "-DMMGR_BUILD_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_C_FLAGS=-DMMGR_ENABLE_HOST=1 " + a.flags, "-DFETCHCONTENT_SOURCE_DIR_EMBEDDED_TYPES=" + EMBEDDED_TYPES.replace("\\", "/")],
            cwd=ROOT, env=env, stdout=out, stderr=subprocess.STDOUT,
        )
    if configured.returncode != 0:
        print(tail(configure_log, 20), end="")
        return 1
    with open(build_log, "wb") as out:
        built = subprocess.run(["cmake", "--build", tree], cwd=ROOT, env=env, stdout=out, stderr=subprocess.STDOUT)
    if built.returncode != 0:
        print("\n".join(grep(build_log, "error", 20)))
        return 1
    ran = subprocess.run(["ctest", "--test-dir", tree, "-R", "memoria_operor|cellularum_laboro", "--output-on-failure"], cwd=ROOT, env=env, capture_output=True, text=True, errors="replace")
    print("".join((ran.stdout + ran.stderr).splitlines(keepends=True)[-25:]), end="")
    return ran.returncode


TEST_IMAGE = "orior-test-window"
TIDY_CHECKS = "clang-analyzer-*,bugprone-*,cert-*,concurrency-*,performance-*,misc-*,-misc-include-cleaner,-bugprone-easily-swappable-parameters,-misc-no-recursion"
TIDY_OUT = "build/sonarqube/clang-tidy.txt"
CPPCHECK_OUT = "build/sonarqube/cppcheck.xml"


def cmd_build_image(a):
    """The Linux test image built from ui/test/docker, the one the window's suite runs in and the C analysis runs in."""
    log = os.path.join(BUILD, "image_build.log")
    with open(log, "wb") as out:
        built = subprocess.run(["docker", "build", "-t", TEST_IMAGE, os.path.join(ROOT, "ui", "test", "docker")], cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
    if built.returncode != 0:
        print(tail(log, 12), end="")
        return 1
    print(tail(log, 1), end="")
    return 0


def cmd_source_tidy(a):
    """clang-tidy, with the clang static analyzer's checks, and cppcheck over the CLI's and the query's C sources,
    inside the Linux test image with the build's own words, their reports written under build/sonarqube for the
    SonarQube scan to import."""
    os.makedirs(os.path.join(BUILD, "sonarqube"), exist_ok=True)
    words = "-std=c11 -I src/cu/includes/mmgr/include -I src/cu/includes/mmgr/src -I src/cu/includes/embedded_types/include -DMMGR_ENABLE_DMA=0 -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ENABLE_HOST=1 -DMMGR_ALIGN_BYTES=32 -DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1"
    inside = (
        "find ui/cli/src ui/query/src -name '*.c' -not -path '*/bundle/*' | sort > build/sonarqube/tidy_files.txt"
        " && xargs -a build/sonarqube/tidy_files.txt -P %d -I{} sh -c \"clang-tidy -quiet -checks='%s' {} -- %s 2>/dev/null\" > %s"
        "; cppcheck --quiet --enable=warning,style,performance,portability --std=c11 --language=c --inline-suppr --xml --xml-version=2"
        " -D__clang__=1 -D__GNUC__=13 %s --file-list=build/sonarqube/tidy_files.txt --output-file=%s; wc -l < %s"
    ) % (FRAME_JOBS, TIDY_CHECKS, words, TIDY_OUT, " ".join(word for word in words.split() if word.startswith(("-I", "-D"))), CPPCHECK_OUT, TIDY_OUT)
    top = ROOT.replace("\\", "/")
    ran = subprocess.run(["docker", "run", "--rm", "-v", top + ":/usr/src", "-w", "/usr/src", TEST_IMAGE, "sh", "-c", inside], cwd=ROOT, capture_output=True, text=True, errors="replace")
    print((ran.stdout + ran.stderr).strip()[-2000:])
    if ran.returncode != 0:
        return 1
    print("clang-tidy: %s, cppcheck: %s" % (TIDY_OUT, CPPCHECK_OUT))
    return 0


def cmd_source_syntax(a):
    status = 0
    for source in a.sources:
        said = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-fsyntax-only", *MMGR_FLAGS, os.path.join(ROOT, source)], capture_output=True, text=True, errors="replace")
        text = said.stdout + said.stderr
        if said.returncode == 0 and not text.strip():
            print("%s: clean" % source)
            continue
        print("%s:" % source)
        print("\n".join(line for line in text.splitlines() if re.search("error|warning", line))[:20 * 200])
        status = 1
    return status


def cmd_source_tests_syntax(a):
    os.makedirs(OUT, exist_ok=True)
    listed = os.path.join(OUT, "tests_syntax.txt")
    folder = os.path.join(ROOT, "utils", "test", "src", "ui", "cli")
    errors = []
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".c"):
            continue
        said = subprocess.run(["clang", "-std=c11", "-fsyntax-only", *MMGR_FLAGS, os.path.join(folder, name)], capture_output=True, text=True, errors="replace")
        errors.extend(line for line in (said.stdout + said.stderr).splitlines() if "error:" in line and "scriptura" not in line.lower())
    with open(listed, "w", encoding="utf-8", newline="\n") as out:
        out.write("".join(line + "\n" for line in errors))
    print(len(errors))
    counts = {}
    for line in errors:
        key = re.sub(r":.*error:", ": ", line)
        counts[key] = counts.get(key, 0) + 1
    for key, count in sorted(counts.items(), key=lambda item: -item[1])[:40]:
        print("%7d %s" % (count, key))
    return 0


def frame_lines(source):
    compiler = os.environ.get("CC", "clang")
    said = subprocess.run(
        [compiler, "--target=x86_64-pc-windows-msvc", "-std=c11", "-O2", "-c", "-o", os.devnull, "-Wframe-larger-than=%d" % FRAME_LARGEST, *MMGR_FLAGS, "-DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1", source],
        capture_output=True, text=True, errors="replace",
    )
    return [line for line in (said.stdout + said.stderr).splitlines() if "stack frame size" in line]


def cmd_source_frames(a):
    listed = os.path.join(BUILD, "harness", "frames.txt")
    sources = [os.path.join(folder, name) for folder, _, names in os.walk(os.path.join(ROOT, "ui", "cli", "src")) for name in names if name.endswith(".c")]
    with concurrent.futures.ThreadPoolExecutor(max_workers=FRAME_JOBS) as pool:
        found = [line for lines in pool.map(frame_lines, sorted(sources)) for line in lines]
    os.makedirs(os.path.dirname(listed), exist_ok=True)
    with open(listed, "w", encoding="utf-8", newline="\n") as out:
        out.write("".join(line + "\n" for line in found))
    print("\n".join(found))
    print("%d functions, %s" % (len(found), listed))
    return 0


def section_bytes(path):
    said = subprocess.run(["llvm-readobj", "--sections", path], capture_output=True, text=True, errors="replace")
    total = 0
    taking = False
    for line in said.stdout.splitlines():
        if re.search(r"Name: \.(bss|data)\b", line):
            taking = True
        elif "Name:" in line:
            taking = False
        elif taking and "RawDataSize:" in line:
            total += int(line.split()[1])
    return total


def cmd_source_static_sizes(a):
    os.makedirs(OUT, exist_ok=True)
    listed = os.path.join(OUT, "static_sizes.txt")
    with open(a.list, encoding="utf-8") as kept:
        objects = [line.strip() for line in kept if line.strip()]
    sized = sorted((section_bytes(path), path) for path in objects)
    with open(listed, "w", encoding="utf-8", newline="\n") as out:
        out.write("".join("%d %s\n" % pair for pair in sized))
    print("total %.1f MiB" % (sum(size for size, _ in sized) / MIB))
    for size, path in sized[-40:]:
        print("%9.1f MiB  %s" % (size / MIB, path))
    return 0


def cmd_source_static_symbols(a):
    for name in a.names:
        path = os.path.join(BUILD, "cli", "x86_64-pc-windows-msvc", "objects", "ui__cli__src__%s.c.o" % name)
        sections = subprocess.run(["llvm-objdump", "-h", path], capture_output=True, text=True, errors="replace").stdout
        end = next((int(line.split()[2], 16) for line in sections.splitlines() if len(line.split()) > 2 and line.split()[1] == ".bss"), 0)
        symbols = subprocess.run(["llvm-nm", "-n", path], capture_output=True, text=True, errors="replace").stdout
        statics = [(int(parts[0], 16), parts[2]) for parts in (line.split() for line in symbols.splitlines()) if len(parts) == 3 and parts[1] in "bB"]
        sized = [((statics[at + 1][0] if at + 1 < len(statics) else end) - start, symbol) for at, (start, symbol) in enumerate(statics)]
        print("== " + name)
        for size, symbol in sorted(sized, reverse=True)[:6]:
            print("%10.1f KiB  %s" % (size / 1024.0, symbol))
    return 0


def digest(path):
    with open(path, "rb") as kept:
        return hashlib.md5(kept.read()).hexdigest()


def cmd_source_format(a):
    os.makedirs(OUT, exist_ok=True)
    files = sorted(os.path.join(folder, name) for top in ("ui/cli/src", "ui/query/src") for folder, _, names in os.walk(os.path.join(ROOT, *top.split("/"))) for name in names if name.endswith((".c", ".h")))
    before = {path: digest(path) for path in files}
    subprocess.run(["clang-format", "-i", *files], check=True)
    changed = [path for path in files if digest(path) != before[path]]
    with open(os.path.join(OUT, "format_changed.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write("".join(findroot.rel(path) + "\n" for path in changed))
    print(len(files))
    print(len(changed))
    return 0


def register(sub):
    """The build and source groups' commands added to harness.py's subparsers."""
    group = sub.add_parser("build", help="the window's program and MMgr's suites built")
    build = group.add_subparsers(dest="command", required=True)
    p = build.add_parser("gui", help="the window's release program, every window running from it ended first")
    p.set_defaults(fn=cmd_build_gui)
    p = build.add_parser("mmgr", help="MMgr's Unity suites built and the accuracy suites run")
    p.add_argument("--suffix", default="", help="the build tree's suffix under build/mmgr_tests")
    p.add_argument("--flags", default="", help="C flags past the tree's own")
    p.set_defaults(fn=cmd_build_mmgr)
    p = build.add_parser("image", help="the Linux test image built from ui/test/docker")
    p.set_defaults(fn=cmd_build_image)

    group = sub.add_parser("source", help="the C sources read by the compiler and the linker's tools")
    source = group.add_subparsers(dest="command", required=True)
    p = source.add_parser("syntax", help="each C source checked by the compiler alone, nothing built")
    p.add_argument("sources", nargs="+", help="repository-relative")
    p.set_defaults(fn=cmd_source_syntax)
    p = source.add_parser("tests-syntax", help="every ui test's C file checked, the errors to build/harness/out/tests_syntax.txt")
    p.set_defaults(fn=cmd_source_tests_syntax)
    p = source.add_parser("frames", help="each CLI function whose stack frame is 64 KB or more, to build/harness/frames.txt")
    p.set_defaults(fn=cmd_source_frames)
    p = source.add_parser("static-sizes", help="each object's .bss and .data bytes, to build/harness/out/static_sizes.txt")
    p.add_argument("list", help="a file naming one object a line")
    p.set_defaults(fn=cmd_source_static_sizes)
    p = source.add_parser("static-symbols", help="the zero-filled statics of CLI objects, largest first")
    p.add_argument("names", nargs="+", help="the source's name under ui/cli/src, without .c")
    p.set_defaults(fn=cmd_source_static_symbols)
    p = source.add_parser("format", help="clang-format over ui/cli/src and ui/query/src, the files changed listed")
    p.set_defaults(fn=cmd_source_format)
    p = source.add_parser("tidy", help="clang-tidy with the static analyzer, and cppcheck, in the Linux image, their reports under build/sonarqube")
    p.set_defaults(fn=cmd_source_tidy)
