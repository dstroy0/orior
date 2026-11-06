# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py cli test: orior_cli's Unity suites generated, built and run.

    harness.py cli test [name...]        every suite, or the suites named
    harness.py cli test --sanitize address|thread   the same built and run under that sanitizer, a report failing its suite
    harness.py cli test --cases name     what the generator registers in a suite, and what it would skip
    harness.py cli coverage build       the library, the suites, the program and the window built with --coverage
    harness.py cli coverage test         the suites built for coverage run
    harness.py cli coverage report       their counts merged through gcovr into build/sonarqube/coverage.xml

A suite is a directory, utils/test/src/ui/cli/unit/<module>/test_<module>/, holding one test_<module>.c with file-scope
`void test_<name>(void)` cases and the generated unity_runner.c beside it. The generator reads case names out of the source
text: a function it does not collect never runs and the suite still passes. A near miss is reported and fails the run.
Unity is fetched once into build/unity at the tag MMgr's suites pin.
"""

import collections
import os
import re
import shutil
import subprocess
import sys

ROOT = None
UNITY_REPOSITORY = "https://github.com/ThrowTheSwitch/Unity.git"
UNITY_TAG = "v2.6.1"
GENERATED_RUNNER = "unity_runner.c"
SUITES = os.path.join("utils", "test", "src", "ui", "cli", "unit")
SUPPORT = os.path.join(SUITES, "support", "cli_unity.c")
RUNNER_OPTIONS = os.path.join(SUITES, "support", "unity_runner.yml")
# The suffix of a coverage build's folders, as ORIOR_COVERAGE names them in ui/cli/build.sh.
COVERAGE = "-coverage"

# What Unity's generate_test_runner.rb collects, and the shape a case has to have to be collected.
UNITY_CASE = re.compile(r"^[ \t]*void[ \t]+(test_\w+)[ \t]*\([ \t]*(?:void)?[ \t]*\)", re.M)
NEAR_MISS = re.compile(r"^[ \t]*void[ \t]+(\w+)[ \t]*\([ \t]*(?:void)?[ \t]*\)[ \t]*\r?\n[ \t]*\{", re.M)
NOT_A_CASE = ("setUp", "tearDown", "main", "suiteSetUp", "suiteTearDown")
# Unity's last line of a run: "3 Tests 0 Failures 0 Ignored"
RESULT = re.compile(r"^(\d+) Tests (\d+) Failures (\d+) Ignored", re.M)


def runner_cases(path):
    """The cases Unity's generator registers in `path`, and the functions it would skip."""
    with open(path, encoding="utf-8") as source:
        text = source.read()
    found = UNITY_CASE.findall(text)
    missed = [name for name in NEAR_MISS.findall(text) if name not in found and name not in NOT_A_CASE]
    return found, missed


def unity_folder(root):
    """Unity's source tree under build/, cloned at its tag where it is not there yet."""
    folder = os.path.join(root, "build", "unity")
    if not os.path.isfile(os.path.join(folder, "src", "unity.c")):
        subprocess.run(["git", "clone", "--quiet", "--depth", "1", "--branch", UNITY_TAG, UNITY_REPOSITORY, folder], check=True)
    return folder


def generate_runner(root, suite_dir, unity, extension=".c"):
    """Write suite_dir/unity_runner<extension> from the suite's one source of that extension, .cu for a suite nvcc compiles. Line ends
    are LF and the path is relative to the tree, which leaves the tracked file the same on every machine."""
    runner = GENERATED_RUNNER[: -len(".c")] + extension
    sources = [name for name in sorted(os.listdir(suite_dir)) if name.endswith(extension) and name != runner]
    holding = [name for name in sources if runner_cases(os.path.join(suite_dir, name))[0]]
    if len(holding) != 1:
        raise SystemExit("unit: %s has test cases in %d sources, and a runner registers exactly one: %s" % (suite_dir, len(holding), ", ".join(holding)))
    ruby = shutil.which("ruby")
    if ruby is None:
        raise SystemExit("unit: ruby is not on PATH, and Unity's generator needs it")
    source = os.path.join(suite_dir, holding[0])
    out = os.path.join(suite_dir, runner)
    subprocess.run([ruby, os.path.join(unity, "auto", "generate_test_runner.rb"), os.path.join(root, RUNNER_OPTIONS), source, out], check=True)
    with open(out, "rb") as written:
        text = written.read()
    fixed = text.replace(b"\r\n", b"\n")
    fixed = fixed.replace(root.replace("\\", "/").encode() + b"/", b"").replace(root.replace("/", "\\").encode() + b"\\", b"")
    if fixed != text:
        with open(out, "wb") as rewritten:
            rewritten.write(fixed)
    return source


def host_target():
    """The clang target triple and the program suffix and libraries for this machine, matching what ui/cli/build.sh builds."""
    if sys.platform == "win32":
        return "x86_64-pc-windows-msvc", ".exe", ["-lws2_32", "-lsynchronization", "-lsecur32", "-lcrypt32", "-lshell32", "-ladvapi32", "-luser32", "-lole32", "-luuid"]
    machine = os.uname().machine
    if sys.platform == "darwin":
        return machine + "-apple-darwin", "", ["-lpthread", "-lm"]
    ldd = subprocess.run(["ldd", "--version"], capture_output=True, text=True, check=False)
    if "musl" in (ldd.stdout + ldd.stderr).lower():
        return machine + "-alpine-linux-musl", "", ["-static", "-lpthread", "-lm"]
    return machine + "-unknown-linux-gnu", "", ["-lpthread", "-ldl", "-lm", "-lrt"]


def link_words(root):
    """The thin LTO link flags matching ui/cli/lto_words.sh. The library is bitcode, and a suite links it through lld."""
    cache = os.path.join(root, "build", "lto_cache").replace("\\", "/")
    os.makedirs(cache, exist_ok=True)
    words = ["-flto=thin"]
    if sys.platform == "win32":
        return words + ["-fuse-ld=lld", "-Wl,/lldltocache:" + cache, "-Wl,/lldltocachepolicy:cache_size_bytes=8g"]
    if sys.platform == "darwin":
        return words + ["-Wl,-cache_path_lto," + cache]
    for name in ("ld.lld", "ld.lld-21", "ld.lld-20", "ld.lld-19", "ld.lld-18", "ld.lld-17"):
        found = shutil.which(name)
        if found:
            return words + ["-fuse-ld=" + found, "-Wl,--thinlto-cache-dir=" + cache, "-Wl,--thinlto-cache-policy=cache_size_bytes=8g"]
    return words


def compile_words(root):
    """The include roots and MMgr's configuration, matching what ui/cli/build.sh compiles the library with."""
    mmgr = os.path.join(root, "src", "cu", "includes", "mmgr")
    return ["-I", root, "-I", os.path.join(mmgr, "include"), "-I", os.path.join(mmgr, "src"), "-I", os.path.join(root, "src", "cu", "includes", "embedded_types", "include"),
            "-DMMGR_ENABLE_DMA=0", "-DMMGR_ENABLE_EXTRAM=0", "-DMMGR_ALIGN_BYTES=32", "-DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1", "-DMMGR_CARCER_OWNERS=1024u"]


def suite_dirs(root, names, suites=SUITES, extension=".c"):
    """Each suite's folder, sorted: those named, or every one under `suites`."""
    base = os.path.join(root, suites)
    found = {}
    for folder, _, files in os.walk(base):
        leaf = os.path.basename(folder)
        if leaf.startswith("test_") and (leaf + extension) in files:
            found[leaf[len("test_"):]] = folder
    if not names:
        return [found[name] for name in sorted(found)]
    missing = [name for name in names if name not in found]
    if missing:
        raise SystemExit("unit: no suite named %s under %s" % (", ".join(missing), suites))
    return [found[name] for name in names]


def plain_words(root):
    """The compile and link flags of a suite, the library being bitcode that a suite links through LTO."""
    return ["-O1", "-g", "-flto=thin", *link_words(root)]


def sanitize_words(root, sanitize):
    """The compile and link flags of a suite built under a sanitizer, matching ORIOR_SANITIZE in ui/cli/build.sh: on Windows the C
    runtime's DLL and no bitcode."""
    words = ["-fsanitize=" + sanitize, "-fsanitize-recover=" + sanitize, "-fno-omit-frame-pointer"]
    if sys.platform == "win32":
        return ["-O1", "-g", "-fuse-ld=lld", *words, "-fms-runtime-lib=dll", "-D_DLL"]
    return [*plain_words(root), *words]


def sanitize_env(sanitize, report_path, env):
    """`env` with a sanitizer's reports not halting and written from `report_path`, and on Windows the address sanitizer's
    runtime, which lies beside the compiler's libraries, on the PATH. `env` as it is where no sanitizer is named."""
    if not sanitize:
        return env
    for name in ("ASAN_OPTIONS", "TSAN_OPTIONS"):
        given = os.environ.get(name, "")
        env[name] = "halt_on_error=0:log_path=" + report_path + ((":" + given) if given else "")
    if sys.platform == "win32":
        found = subprocess.run([os.environ.get("CC", "clang"), "-print-file-name=clang_rt.asan_dynamic-x86_64.dll"], capture_output=True, text=True, check=False)
        env["PATH"] = os.path.dirname(found.stdout.strip()) + os.pathsep + env.get("PATH", "")
    return env


def coverage_words():
    """The compile and link flags of a suite built for coverage, matching ORIOR_COVERAGE in ui/cli/build.sh."""
    words = ["-O0", "-g", "--coverage", "-fprofile-update=atomic"]
    return words + ["-fuse-ld=lld"] if sys.platform == "win32" else words


def run_folder(root, build_suffix):
    """The folder a suite's program is built and run in: a coverage or sanitizer build's is apart from the plain build's."""
    return os.path.join(root, "build", "cli_unit" + build_suffix)


def build_suite(root, suite_dir, unity, library, target, suffix, libraries, run, flags):
    """The suite's program built in `run` with `flags`: its path, or None with the compiler's output printed."""
    name = os.path.basename(suite_dir)
    source = generate_runner(root, suite_dir, unity)
    _, missed = runner_cases(source)
    if missed:
        print("  %s: the generator skips %s; rename each to test_<name>" % (name, ", ".join(missed)), flush=True)
        return None
    program = os.path.join(run, name + suffix)
    command = [os.environ.get("CC", "clang"), "--target=" + target, "-std=c11", *flags, *compile_words(root),
               # Unity's default includes <stdnoreturn.h>, whose `noreturn` macro breaks `__declspec(noreturn)` in the MSVC headers
               "-I", os.path.join(unity, "src"), "-DUNITY_INCLUDE_DOUBLE", "-DUNITY_NORETURN=_Noreturn", "-o", program, source, os.path.join(suite_dir, GENERATED_RUNNER),
               os.path.join(unity, "src", "unity.c"), os.path.join(root, SUPPORT), library, *libraries]
    built = subprocess.run(command, capture_output=True, text=True, check=False)
    if built.returncode != 0:
        print("  %s did not compile:\n%s" % (name, "\n".join((built.stdout + built.stderr).splitlines()[-24:])), flush=True)
        return None
    return program


def lowered_priority():
    """Tests run below the window and the machine's own work."""
    if sys.platform == "win32":
        import ctypes  # pylint: disable=import-outside-toplevel

        below_normal = 0x00004000
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), below_normal)
    else:
        os.nice(9)


def suite_env():
    """A suite's environment: its fixtures' commits and tags are not signed, and git leaves their line ends as written."""
    return dict(os.environ, GIT_CONFIG_COUNT="3", GIT_CONFIG_KEY_0="commit.gpgsign", GIT_CONFIG_VALUE_0="false", GIT_CONFIG_KEY_1="tag.gpgsign", GIT_CONFIG_VALUE_1="false", GIT_CONFIG_KEY_2="core.autocrlf", GIT_CONFIG_VALUE_2="false")


def sanitizer_reports(name, run):
    """Each report a sanitizer wrote in `run` saved as <name>.sanitizer.<pid>, its summary lines printed: how many there were."""
    reports = [one for one in os.listdir(run) if one.startswith("sanitizer.")]
    for one in reports:
        path = os.path.join(run, one)
        with open(path, encoding="utf-8", errors="replace") as report:
            summaries = collections.Counter(line.rstrip() for line in report if line.startswith("SUMMARY:"))
        for line, count in sorted(summaries.items()):
            print("  sanitizer %d %s" % (count, line), flush=True)
        os.replace(path, os.path.join(run, name + "." + one))
    return len(reports)


def program_ran(name, program, run, env):
    """A suite's program run in `run`, a line printed for it: its tests, its failures, and whether the suite failed. A sanitizer's
    report fails it, and the pool's and owners' lines it prints under ORIOR_CELLS_OWNERS are printed.
    The program writes into files and never into pipes. A process a suite starts and leaves behind inherits the suite's output handles,
    and a pipe stays open while that process lives, which a read of the pipe would wait out; a file's end is read as the
    program ends whatever else holds the file."""
    out_path = os.path.join(run, name + ".out.txt")
    err_path = os.path.join(run, name + ".err.txt")
    with open(out_path, "w", encoding="utf-8") as out, open(err_path, "w", encoding="utf-8") as err:
        try:
            ran = subprocess.run([program], cwd=run, env=env, stdin=subprocess.DEVNULL, stdout=out, stderr=err, timeout=300, check=False)
        except subprocess.TimeoutExpired:
            print("  %s: stopped after 300 s" % name, flush=True)
            return 0, 0, True
    with open(out_path, encoding="utf-8", errors="replace") as out:
        stdout = out.read()
    with open(err_path, encoding="utf-8", errors="replace") as err:
        stderr = err.read()
    for line in stdout.splitlines():
        if line.startswith("cells "):
            print(line, flush=True)
    result = RESULT.search(stdout)
    failed = (sanitizer_reports(name, run) > 0) or result is None or ran.returncode != 0
    if failed:
        print("\n".join(line for line in stdout.splitlines() if ":FAIL" in line) or stdout[-3000:], flush=True)
        print(stderr[-1000:], flush=True)
        print("  %s: exit %d" % (name, ran.returncode), flush=True)
    if result is None:
        return 0, 0, failed
    print("  %s: %s tests, %s failures, %s ignored" % (name, result.group(1), result.group(2), result.group(3)), flush=True)
    return int(result.group(1)), int(result.group(2)), failed


def totals_printed(suites, tests, failures, failing, label="cli"):
    """The run's totals printed under `label`, read by harness.py run as `N checks, M failed`: the process's exit code."""
    print("%s test: %d suites, %d checks, %d failed, %d suites failing%s" % (label, suites, tests, failures, len(failing), (": " + ", ".join(failing)) if failing else ""), flush=True)
    return 1 if failing else 0


def run_suites(root, names, library_built, sanitize=""):
    """Every suite named built and run, under the sanitizer named where one is. `library_built(target)` builds orior_cli for the target
    and answers its archive, or None. Prints a line for each suite and the totals, and answers the process's exit code."""
    lowered_priority()
    target, suffix, libraries = host_target()
    library = library_built(target)
    if library is None:
        return 1
    unity = unity_folder(root)
    run = run_folder(root, ("-" + sanitize) if sanitize else "")
    os.makedirs(run, exist_ok=True)
    flags = sanitize_words(root, sanitize) if sanitize else plain_words(root)
    env = sanitize_env(sanitize, "sanitizer", suite_env())
    tests = 0
    failures = 0
    failing = []
    suites = suite_dirs(root, names)
    for suite_dir in suites:
        name = os.path.basename(suite_dir)
        program = build_suite(root, suite_dir, unity, library, target, suffix, libraries, run, flags)
        if program is None:
            failing.append(name)
            continue
        ran_tests, ran_failures, failed = program_ran(name, program, run, env)
        tests += ran_tests
        failures += ran_failures
        if failed:
            failing.append(name)
    return totals_printed(len(suites), tests, failures, failing)


def coverage_folders(root, target):
    """The folders holding a coverage build's objects and programs, where each writes its .gcno and .gcda."""
    return [os.path.join(root, "build", "cli", target + COVERAGE), run_folder(root, COVERAGE)]


def coverage_built(root, names, library_built):
    """Every suite named built for coverage and not run, for a build wrapper to record without recording what a test compiles.
    `library_built(target)` builds orior_cli for coverage and answers its archive, or None. Answers the process's exit code."""
    lowered_priority()
    target, suffix, libraries = host_target()
    library = library_built(target)
    if library is None:
        return 1
    unity = unity_folder(root)
    run = run_folder(root, COVERAGE)
    os.makedirs(run, exist_ok=True)
    flags = coverage_words()
    suites = suite_dirs(root, names)
    failing = [os.path.basename(suite_dir) for suite_dir in suites if build_suite(root, suite_dir, unity, library, target, suffix, libraries, run, flags) is None]
    print("cli coverage: %d suites built, %d did not%s" % (len(suites) - len(failing), len(failing), (": " + ", ".join(failing)) if failing else ""), flush=True)
    return 1 if failing else 0


def coverage_ran(root, names):
    """Every suite named run from its coverage build, the .gcda of any earlier run removed first, which leaves the counts this run's alone.
    Prints a line for each suite and the totals, and answers the process's exit code."""
    lowered_priority()
    target, suffix, _ = host_target()
    for folder in coverage_folders(root, target):
        for top, _, files in os.walk(folder):
            for name in files:
                if name.endswith(".gcda"):
                    os.remove(os.path.join(top, name))
    run = run_folder(root, COVERAGE)
    env = suite_env()
    tests = 0
    failures = 0
    failing = []
    suites = suite_dirs(root, names)
    for suite_dir in suites:
        name = os.path.basename(suite_dir)
        program = os.path.join(run, name + suffix)
        if not os.path.isfile(program):
            print("  %s: not built, `cli coverage build` builds it" % name, flush=True)
            failing.append(name)
            continue
        ran_tests, ran_failures, failed = program_ran(name, program, run, env)
        tests += ran_tests
        failures += ran_failures
        if failed:
            failing.append(name)
    return totals_printed(len(suites), tests, failures, failing)


def coverage_report(root):
    """The .gcda merged through gcovr into build/sonarqube/coverage.xml, SonarQube's generic coverage format, the summary printed.
    llvm-cov reads the counts clang's --coverage writes, and test code, build products and MMgr, which has its own scan, are not counted."""
    target = host_target()[0]
    os.makedirs(os.path.join(root, "build", "sonarqube"), exist_ok=True)
    command = [sys.executable, "-m", "gcovr", "--root", ".", "--gcov-executable", "llvm-cov gcov", "--sonarqube", "build/sonarqube/coverage.xml",
               "--exclude", "utils/test/", "--exclude", "build/", "--exclude", "src/cu/includes/mmgr/", "--exclude", "src/cu/includes/embedded_types/",
               "--print-summary", "-j", str(os.cpu_count() or 1), *coverage_folders(root, target)]
    return subprocess.run(command, cwd=root, check=False).returncode


def show_cases(root, name):
    """What the generator registers in a suite, and what it would skip."""
    folder = suite_dirs(root, [name])[0]
    found, missed = runner_cases(os.path.join(folder, os.path.basename(folder) + ".c"))
    print("%s: %d cases\n  %s" % (name, len(found), "\n  ".join(found)))
    if missed:
        print("skipped by the generator: %s" % ", ".join(missed))
    return 1 if missed else 0
