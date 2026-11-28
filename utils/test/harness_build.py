# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py build and harness.py source: the window's program and MMgr's suites built, and the C sources read by
the compiler and the linker's tools.

    harness.py build gui                      the window's release program, every window running from it ended first
    harness.py build cli [TARGET]             the command line's program, its debug information next to it
    harness.py build mmgr [--suffix S] [--flags F] [--tests R]
                                              MMgr's Unity suites under build/mmgr_tests<S>, the owner queue's room
                                              as orior builds MMgr, the accuracy suites run, or those R matches
    harness.py build image                    the Linux test image, from ui/test/docker
    harness.py source tidy                    clang-tidy with the static analyzer, and cppcheck, in that image, the
                                              reports under build/sonarqube for the SonarQube scan
    harness.py source analyze <source>...     the clang static analyzer over C sources or folders, its findings to
                                              build/harness/out/analyze.txt
    harness.py source syntax <source>...      each C source checked by the compiler alone, nothing built
    harness.py source tests-syntax            every ui test's C file checked, the errors to build/harness/out
    harness.py source frames                  each CLI function whose stack frame is 64 KB or more
    harness.py source static-sizes <list>     each object's .bss and .data bytes, the largest last
    harness.py source static-symbols <name>...  the zero-filled statics of a CLI object, largest first
    harness.py source format                  clang-format over ui/gui/src, ui/cli/src, ui/query/src and includes/platform, the files changed listed
    harness.py source repeats [folder] [lines]  the runs of C lines located in two or more places under the folder,
                                              blank and comment lines dropped, the longest first
    harness.py source cells [folder]          every persistent take under the folder, with the function it is in,
                                              what it is assigned to, where the function releases it and its escape
                                              class, as build/harness/out/cells.tsv, the classes counted

Logs and listings go under build/ and build/harness/out."""

import concurrent.futures
import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

import harness_move  # noqa: E402
import harness_rename  # noqa: E402

ROOT = findroot.root()
BUILD = os.path.join(ROOT, "build")
OUT = os.path.join(BUILD, "harness", "out")
MMGR = os.path.join(ROOT, "includes", "mmgr")
EMBEDDED_TYPES = os.path.join(ROOT, "includes", "embedded_types")
MMGR_FLAGS = ["-I", os.path.join(MMGR, "include"), "-I", os.path.join(MMGR, "src"), "-I", os.path.join(EMBEDDED_TYPES, "include"), "-DMMGR_ENABLE_DMA=0", "-DMMGR_ENABLE_EXTRAM=0", "-DMMGR_ALIGN_BYTES=32", "-DMMGR_CARCER_OWNERS=1024u",
              "-DMMGR_CARCER_REGIONS=16u", "-DMMGR_CARCER_REGION_BYTES=4194304u", "-DMMGR_CARCER_LARGE_REGION_BYTES=68157440u"]
RUBY = r"C:\tools\ruby34\bin"
FRAME_LARGEST = 65536
FRAME_JOBS = 16
MIB = 1048576.0


def tail(path, count):
    with open(path, encoding="utf-8", errors="replace") as retained:
        return "".join(retained.readlines()[-count:])


def grep(path, pattern, count):
    with open(path, encoding="utf-8", errors="replace") as retained:
        return [line.rstrip("\n") for line in retained if re.search(pattern, line)][:count]


def cmd_build_gui(a):
    import harness_window

    if os.name == "nt":
        harness_window.windows_ended()
    import harness_cli

    log = os.path.join(BUILD, "gui_build.log")
    with open(log, "wb") as out:
        built = subprocess.run([harness_cli.bash(), os.path.join(ROOT, "ui", "cli", "build.sh"), "window"], cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
    # ui/cli/build_needs.sh starts each line of its list with "  needs:". The harness shows all of them.
    needs = grep(log, r"^  needs:", 60)
    if built.returncode != 0:
        if needs:
            print("\n".join(needs))
            return 1
        print("\n".join(grep(log, r"^error", 5)))
        print(tail(log, 3), end="")
        return 1
    if needs:
        print("\n".join(needs))
    print(tail(log, 1), end="")
    return 0


def cmd_build_cli(a):
    """The command line's program, its debug information kept in a file next to it, for the host or for `a.target`."""
    import harness_window

    # a window runs the program as its session's far end, and Windows writes no program that is running
    if os.name == "nt":
        harness_window.windows_ended()
    import harness_cli

    log = os.path.join(BUILD, "cli_build.log")
    words = [harness_cli.bash(), os.path.join(ROOT, "ui", "cli", "build.sh")] + ([a.target] if a.target else [])
    with open(log, "wb") as out:
        built = subprocess.run(words, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
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
            ["cmake", "-S", MMGR, "-B", tree, "-G", "Ninja", "-DMMGR_BUILD_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_C_FLAGS=-DMMGR_CARCER_OWNERS=1024u " + a.flags, "-DFETCHCONTENT_SOURCE_DIR_EMBEDDED_TYPES=" + EMBEDDED_TYPES.replace("\\", "/")],
            cwd=ROOT, env=env, stdout=out, stderr=subprocess.STDOUT,
        )
    if configured.returncode != 0:
        print(tail(configure_log, 20), end="")
        return 1
    with open(build_log, "wb") as out:
        # two compiles at once: this host faults under more clang processes than that
        built = subprocess.run(["cmake", "--build", tree, "--parallel", "2"], cwd=ROOT, env=env, stdout=out, stderr=subprocess.STDOUT)
    if built.returncode != 0:
        print("\n".join(grep(build_log, "error", 20)))
        return 1
    ran = subprocess.run(["ctest", "--test-dir", tree, "-R", a.tests, "--output-on-failure"], cwd=ROOT, env=env, capture_output=True, text=True, errors="replace")
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
    words = "-std=c11 -I includes/mmgr/include -I includes/mmgr/src -I includes/embedded_types/include -DMMGR_ENABLE_DMA=0 -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ALIGN_BYTES=32 -DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1 -DMMGR_CARCER_OWNERS=1024u -DMMGR_CARCER_REGIONS=16u -DMMGR_CARCER_REGION_BYTES=4194304u -DMMGR_CARCER_LARGE_REGION_BYTES=68157440u"
    inside = (
        "find %s -name '*.c' -not -path '*/bundle/*' | sort > build/sonarqube/tidy_files.txt"
        " && xargs -a build/sonarqube/tidy_files.txt -P %d -I{} sh -c \"clang-tidy -quiet -checks='%s' {} -- %s 2>/dev/null\" > %s"
        "; cppcheck --quiet --enable=warning,style,performance,portability --std=c11 --language=c --inline-suppr --xml --xml-version=2"
        " -D__clang__=1 -D__GNUC__=13 -DEMBED_WORD_BITS=64 %s --file-list=build/sonarqube/tidy_files.txt --output-file=%s; wc -l < %s"
    ) % (" ".join(a.folders), FRAME_JOBS, TIDY_CHECKS, words, TIDY_OUT, " ".join(re.findall(r"-I \S+|-D\S+", words)), CPPCHECK_OUT, TIDY_OUT)
    top = ROOT.replace("\\", "/")
    ran = subprocess.run(["docker", "run", "--rm", "-v", top + ":/usr/src", "-w", "/usr/src", TEST_IMAGE, "sh", "-c", inside], cwd=ROOT, capture_output=True, text=True, errors="replace")
    print((ran.stdout + ran.stderr).strip()[-2000:])
    if ran.returncode != 0:
        return 1
    print("clang-tidy: %s, cppcheck: %s" % (TIDY_OUT, CPPCHECK_OUT))
    return 0


def cmd_source_analyze(a):
    """The clang static analyzer over the C sources named, or each under the folders named, nothing built: every
    finding written to build/harness/out/analyze.txt and the count of each checker's printed."""
    os.makedirs(OUT, exist_ok=True)
    listed = os.path.join(OUT, "analyze.txt")
    checkers = subprocess.run(["clang", "-cc1", "-analyzer-checker-help"], capture_output=True, text=True, errors="replace").stdout
    bound = "security.ArrayBound" if "security.ArrayBound" in checkers else "alpha.security.ArrayBoundV2"
    sources = []
    for named in a.sources:
        full = os.path.join(ROOT, named)
        if os.path.isdir(full):
            sources.extend(os.path.join(folder, name) for folder, _, names in os.walk(full) for name in names if name.endswith(".c"))
        else:
            sources.append(full)

    def findings_of(source):
        how = ["-fsyntax-only", "-ferror-limit=0"] if a.compile_only else ["--analyze", "--analyzer-output", "text", "-Xclang", "-analyzer-checker=" + bound, "-o", os.devnull]
        said = subprocess.run(
            ["clang", *how, "-std=c11", "-I", ROOT, "-I", os.path.join(ROOT, "build", "unity", "src"), "-DUNITY_INCLUDE_DOUBLE", "-DUNITY_NORETURN=_Noreturn", *MMGR_FLAGS, "-DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1", source],
            cwd=ROOT,
            capture_output=True,
            text=True,
            errors="replace",
        )
        types = r"warning|error|note" if a.notes else r"warning|error"
        return [line for line in (said.stdout + said.stderr).splitlines() if re.search(r":\d+:\d+: (%s):" % types, line)]

    with concurrent.futures.ThreadPoolExecutor(max_workers=FRAME_JOBS) as pool:
        written = [line for lines in pool.map(findings_of, sources) for line in lines]
    found = written if a.notes else sorted(set(written))
    with open(listed, "w", encoding="utf-8") as out:
        out.write("".join(line + "\n" for line in found))
    counts = {}
    for line in found:
        named = re.search(r"\[([\w.-]+)\]$", line)
        key = named.group(1) if named else "error"
        counts[key] = counts.get(key, 0) + 1
    for key in sorted(counts, key=counts.get, reverse=True):
        print(counts[key], key)
    print("%d findings over %d sources: %s" % (len(found), len(sources), listed))
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
    top = os.path.join(ROOT, "utils", "test", "src", "ui")
    unity = os.path.join(ROOT, "build", "unity", "src")
    sources = [
        os.path.join(folder, name)
        for folder, _, names in os.walk(top)
        for name in names
        if name.endswith(".c") and (name != "unity_runner.c")
    ]

    def errors_of(source):
        said = subprocess.run(
            ["clang", "-std=c11", "-fsyntax-only", "-I", ROOT, "-I", unity, "-DUNITY_INCLUDE_DOUBLE", "-DUNITY_NORETURN=_Noreturn", *MMGR_FLAGS, source],
            capture_output=True,
            text=True,
            errors="replace",
        )
        return [line for line in (said.stdout + said.stderr).splitlines() if "error:" in line]

    errors = []
    with concurrent.futures.ThreadPoolExecutor(FRAME_JOBS) as pool:
        for found in pool.map(errors_of, sorted(sources)):
            errors.extend(found)
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
    sources = [os.path.join(folder, name) for top in ("ui/cli/src", "includes/platform") for folder, _, names in os.walk(os.path.join(ROOT, *top.split("/"))) for name in names if name.endswith(".c")]
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
    with open(a.list, encoding="utf-8") as retained:
        objects = [line.strip() for line in retained if line.strip()]
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
    with open(path, "rb") as retained:
        return hashlib.md5(retained.read()).hexdigest()


FORMAT_BATCH = 200


def cmd_source_format(a):
    os.makedirs(OUT, exist_ok=True)
    # the lattice stays as it is until the window's own computing replaces it
    files = sorted(os.path.join(folder, name) for top in ("ui/gui/src", "ui/cli/src", "ui/query/src", "includes/platform") for folder, _, names in os.walk(os.path.join(ROOT, *top.split("/"))) for name in names if name.endswith((".c", ".h")) and name not in ("lattice.c", "lattice.h"))
    before = {path: digest(path) for path in files}
    # a few at a time, since every file on one command line passes the length Windows takes
    for at in range(0, len(files), FORMAT_BATCH):
        subprocess.run(["clang-format", "-i", *files[at:at + FORMAT_BATCH]], check=True)
    changed = [path for path in files if digest(path) != before[path]]
    with open(os.path.join(OUT, "format_changed.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write("".join(findroot.rel(path) + "\n" for path in changed))
    print(len(files))
    print(len(changed))
    return 0


def source_lines(path):
    """A C source's lines with their numbers, blank lines, comment lines and bare braces dropped and whitespace folded:
    a run of them compares by what it does."""
    folded = []
    with open(path, encoding="utf-8", errors="replace") as source:
        for number, raw in enumerate(source, 1):
            text = " ".join(raw.split())
            if not text or text.startswith("//") or text in ("{", "}", "};", "else"):
                continue
            folded.append((number, text))
    return folded


def cmd_source_repeats(a):
    """The runs of a.lines or more folded lines that are in two or more places under a.folder, each run grown while
    every place continues alike, the longest first with where each is."""
    window = a.lines
    files = sorted(os.path.join(folder, name) for folder, _, names in os.walk(os.path.join(ROOT, *a.folder.split("/"))) for name in names if name.endswith(".c"))
    per_file = {path: source_lines(path) for path in files}
    windows = {}
    for path, folded in per_file.items():
        for at in range(len(folded) - window + 1):
            windows.setdefault("\n".join(text for _, text in folded[at:at + window]), []).append((path, at))
    seen = set()
    runs = []
    for key, places in windows.items():
        if len(places) < 2 or places[0] in seen:
            continue
        length = window
        partners = places
        while True:
            advanced = [(path, at + 1) for path, at in partners]
            keys = set()
            for path, at in advanced:
                folded = per_file[path]
                if at + window > len(folded):
                    keys = None
                    break
                keys.add("\n".join(text for _, text in folded[at:at + window]))
            if keys is None or len(keys) != 1 or len(windows.get(next(iter(keys)), [])) < len(partners):
                break
            partners = advanced
            length += 1
            seen.update(partners)
        seen.update(places)
        runs.append((length, places))
    runs.sort(key=lambda run: -run[0])
    repeated = 0
    for length, places in runs:
        repeated += length * (len(places) - 1)
        spots = ", ".join("%s:%d" % (findroot.rel(path), per_file[path][at][0]) for path, at in places)
        print("%3d lines x%d: %s" % (length, len(places), spots))
    print("repeated lines: %d in %d runs" % (repeated, len(runs)))
    return 0


def bound_names(listed):
    """The names a parameter list or a destructuring binds: a default's value and a renamed key are read and not bound."""
    listed = re.sub(r"=[^,{}\[\]]*", "", listed)
    listed = re.sub(r"[A-Za-z_$][\w$]*\s*:", "", listed)
    return set(re.findall(r"[A-Za-z_$][\w$]*", listed))


def family_of(path, files):
    """The split a module belongs to: `run` for run.js and for run/panel.js where run.js is named too, else the module
    alone. Only a split's own modules are checked against each other's top-level names; two other modules may share a name."""
    stem = os.path.splitext(path)[0]
    folder = os.path.dirname(path)
    return folder if folder + ".js" in files else stem


CELL_TAKE = re.compile(r"persistent_buf_alloc\(")
CELL_RELEASE = re.compile(r"persistent_buf_release\(\s*(?:\(void \*\)\s*(?:\(uintptr_t\)\s*)?)?([A-Za-z_][\w>.\[\]-]*)")
CELL_OWNER = re.compile(r"(g_\w+_owner|args->owner|owner)\s*\)")
CELL_FUNCTION = re.compile(r"^(?:static |EMBED_INLINE |[A-Za-z_][\w *]*\b)[\w *]*?\b([A-Za-z_]\w*)\s*\([^;]*$")
CELL_ASSIGN = re.compile(r"(?<![=!<>])=(?!=)")
# a store of a name into a member, an element, a dereference or a file-scope variable, not a designated argument
CELL_STORE = r"(?:\w+(?:->|\.)\w+(?:\[[^\]]*\])?|\w+\[[^\]]*\]|\*\s*\w+|\b[sg]_\w+)\s*=\s*\(?(?:\([^()]*\)\s*)?%s\s*[;,)]"


def cell_functions(lines):
    """Each function of a C source as (name, first line, last line), the body found by its braces from the line after
    its signature; the signature's name is the last identifier before its parenthesis."""
    found = []
    name = None
    depth = 0
    start = 0
    for at, text in enumerate(lines):
        stripped = text.rstrip()
        if depth == 0:
            match = CELL_FUNCTION.match(stripped)
            if match and not stripped.endswith(";") and not stripped.startswith(("typedef", "#", "//", "struct ", "EMBED_TABLE")):
                name = match.group(1)
            if stripped == "{" and name is not None:
                depth = 1
                start = at
                continue
            if stripped.startswith("}") or stripped.endswith(";"):
                name = name if (stripped and not stripped.endswith(";")) else None
            continue
        depth += text.count("{") - text.count("}")
        if depth <= 0:
            found.append((name, start, at))
            name = None
            depth = 0
    return found


def cell_target(lines, at):
    """What the take at line `at` is assigned to: the text before the `=` on that line or the lines just above, with
    its declaration stripped; `literal` for a take inside a brace initializer, and `` for a take passed or returned
    straight."""
    joined = ""
    for back in range(0, 4):
        text = lines[at - back].strip() if at - back >= 0 else ""
        # a line above that ends a statement or opens a brace is another statement's: the take's own begins after it
        if (back > 0) and text.endswith(("{", ";", "}")):
            break
        joined = text + " " + joined
        if (back > 0) and (CELL_ASSIGN.search(text) or text.endswith(",")):
            break
    head = joined.split("persistent_buf_alloc(")[0]
    if "return " in head:
        return "return"
    assigned = CELL_ASSIGN.search(head)
    if assigned is None:
        return "literal" if head.rstrip().endswith(("{", ",")) else ""
    left = head[:assigned.start()].strip()
    # the declaration's type words stripped: the last word, with its member path, is the target
    words = re.split(r"\s+", left)
    return words[-1].strip("*") if words else ""


def cell_type(body, target):
    """The escape class of a take's cell in its function, in escape analysis's terms: `local` where the function
    releases the same name and passes it to no callee, `local argument` where it releases it after passing it to a
    callee, `stored` where the target is a member or a file-scope variable, `returned` where the name is returned or
    stored past the function, `argument` where it is passed to a callee and not released here, `unreleased` where
    the name is neither released, stored, returned nor passed, `literal` for a take inside a brace initializer and
    `unknown` where no name is read."""
    if target == "return":
        return "returned"
    if target in ("literal", ""):
        return target or "unknown"
    if "->" in target or "." in target or target.startswith("s_") or target.startswith("g_") or target.startswith("*"):
        return "stored"
    released = any(match.group(1) == target for text in body for match in CELL_RELEASE.finditer(text))
    name = re.escape(target)
    store = re.compile(CELL_STORE % name)
    others = [text for text in body if "persistent_buf_alloc" not in text and "persistent_buf_release" not in text]
    returned = any(re.search(r"return\s+\(?\s*" + name + r"\b", text) or store.search(text) or re.search(r"EXCHANGE\w*\(.*\b" + name + r"\s*\)", text) for text in others)
    if returned:
        return "returned"
    # the name passed to a callee, as a designated argument or a plain one: the callee may retain it
    argument = any(re.search(r"(?:\.\w+\s*=\s*|[(,]\s*)\(?(?:\([^()]*\)\s*)?" + name + r"\s*[,)]", text) for text in others)
    if released:
        return "local" if not argument else "local argument"
    return "argument" if argument else "unreleased"


def cmd_source_cells(a):
    """Every persistent take under a.folder listed with its function, its owner, its target, where the function
    releases that target and the cell's escape class, to build/harness/out/cells.tsv, the classes counted."""
    root = os.path.join(ROOT, *a.folder.split("/"))
    files = sorted(os.path.join(folder, name) for folder, _, names in os.walk(root) for name in names if name.endswith(".c"))
    rows = []
    for path in files:
        with open(path, encoding="utf-8", errors="replace") as source:
            lines = source.read().split("\n")
        functions = cell_functions(lines)
        for at, text in enumerate(lines):
            if not CELL_TAKE.search(text):
                continue
            function = next(((name, start, end) for name, start, end in functions if start <= at <= end), ("", at, at))
            body = lines[function[1]:function[2] + 1]
            target = cell_target(lines, at)
            owner = CELL_OWNER.search(" ".join(lines[at:at + 3]))
            releases = [str(function[1] + offset + 1) for offset, line in enumerate(body) if any(match.group(1) == target for match in CELL_RELEASE.finditer(line))]
            rows.append((os.path.relpath(path, ROOT).replace("\\", "/"), at + 1, function[0], owner.group(1) if owner else "", target, cell_type(body, target), " ".join(releases)))
    os.makedirs(OUT, exist_ok=True)
    listed = os.path.join(OUT, "cells.tsv")
    with open(listed, "w", encoding="utf-8", newline="\n") as out:
        out.write("file\tline\tfunction\towner\ttarget\tescape class\treleased at\n")
        out.write("".join("\t".join(str(field) for field in row) + "\n" for row in rows))
    classes = {}
    for row in rows:
        classes[row[5]] = classes.get(row[5], 0) + 1
    print("\n".join("%6d %s" % (count, name) for name, count in sorted(classes.items(), key=lambda pair: -pair[1])))
    print("%d takes, %s" % (len(rows), listed))
    return 0


def register(sub):
    """The build and source groups' commands added to harness.py's subparsers."""
    group = sub.add_parser("build", help="the window's program and MMgr's suites built")
    build = group.add_subparsers(dest="command", required=True)
    p = build.add_parser("gui", help="the window's release program, every window running from it ended first")
    p.set_defaults(fn=cmd_build_gui)
    p = build.add_parser("cli", help="the command line's program, its debug information next to it, every window ended first")
    p.add_argument("target", nargs="?", default="", help="the target triple, the host's where none is named")
    p.set_defaults(fn=cmd_build_cli)
    p = build.add_parser("mmgr", help="MMgr's Unity suites built and the accuracy suites run")
    p.add_argument("--suffix", default="", help="the build tree's suffix under build/mmgr_tests")
    p.add_argument("--flags", default="", help="C flags past the tree's own")
    p.add_argument("--tests", default=".", help="the suites run, as ctest matches their names; every suite built by default")
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
    p = source.add_parser("format", help="clang-format over ui/gui/src, ui/cli/src, ui/query/src and includes/platform, the files changed listed")
    p.set_defaults(fn=cmd_source_format)
    p = source.add_parser("repeats", help="the runs of C lines located in two or more places under the folder, the longest first")
    p.add_argument("folder", nargs="?", default="includes/window", help="the folder read, from the top of the tree")
    p.add_argument("lines", nargs="?", type=int, default=6, help="the fewest folded lines a run has")
    p.set_defaults(fn=cmd_source_repeats)
    p = source.add_parser("cells", help="every persistent take under the folder with its function, target, releases and escape class, to build/harness/out/cells.tsv")
    p.add_argument("folder", nargs="?", default="ui/cli/src", help="the folder under the tree to read, ui/cli/src by default")
    p.set_defaults(fn=cmd_source_cells)
    p = source.add_parser("analyze", help="the clang static analyzer over the sources or folders named, its findings under build/harness/out")
    p.add_argument("sources", nargs="+", help="C sources or folders, from the top of the tree")
    p.add_argument("--notes", action="store_true", help="each finding's path through the code written after it")
    p.add_argument("--compile-only", action="store_true", help="the compiler's errors and warnings alone, with no analysis")
    p.set_defaults(fn=cmd_source_analyze)
    p = source.add_parser("tidy", help="clang-tidy with the static analyzer, and cppcheck, in the Linux image, their reports under build/sonarqube")
    p.add_argument("--folders", nargs="+", default=["ui/cli/src", "ui/query/src", "includes/platform"], help="the folders read, from the top of the tree")
    p.set_defaults(fn=cmd_source_tidy)
    harness_rename.register(source)
    harness_move.register(source)
