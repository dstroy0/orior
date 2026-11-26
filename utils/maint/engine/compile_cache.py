# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The build's compile cache. Each source that nvcc gets compiles once to an object. A later build with the same flags
links that object. It compiles the source again only where the source or a file that its compile read is newer.

    compile_cache.py key <root> ...           prints the key of the roots
    compile_cache.py nvcc <real nvcc> <argument> ...

`key` is what the objects of a build depend on, past their flags and the files that each compile reads. It holds the
roots, the headers whose names two of the roots' headers share, and the version of the toolkit. The script
build_stamp.sh computes it once a build and exports it as COMPILE_CACHE_KEY. It also writes the roots and the
shared-name headers beside the objects.

`nvcc` takes the place of nvcc. nvcc compiles each .cu, .c or .cpp of the arguments alone, with `-c` and each other
argument but the output. The object goes into COMPILE_CACHE_DIR under a name that the key, those arguments and the
path of the source make. Each compile writes the files that it reads into the .d file of the object (-MD). An object
compiles again where it is not there, or where its source or a file of its .d file is newer than it.

Those compiles run COMPILE_CACHE_JOBS at a time: 4 where the caller sets none, and the processors of a suite where
the harness sets it. Then the call runs again with each source replaced by its object, and that call only links. Each
source of a call compiles apart, and with -rdc the link of the objects is the device link. The objects are the objects
that nvcc makes itself.

Any other call goes to nvcc as it is. Such a call asks for -E, -M, -x, -dc, -dlink, -ptx, -cubin, -fatbin, -lib,
-shared or --version, or names no source. Each call goes to nvcc as it is where COMPILE_CACHE_KEY or
COMPILE_CACHE_DIR has no value.

The name of an object leaves out an include folder inside the roots, unless a shared-name header lies under it. Each
other header there has one file of its name. Any set of folders finds the same file for it, and so the suites link the
same engine objects with folders of their own. A build waits for an object that another build compiles (its .lock),
for up to COMPILE_CACHE_WAIT seconds (900 where the caller sets none). After that time it compiles the object too.
"""

import concurrent.futures
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time

SOURCES = (".cu", ".c", ".cpp")
HEADERS = (".h", ".cuh", ".hpp")
VALUED = {"-o", "-I", "-L", "-l", "-D", "-U", "-gencode", "-Xcompiler", "-Xlinker", "-Xptxas", "-Xnvlink", "-ccbin",
          "-arch", "-code", "-include", "-isystem", "-std", "--output-file", "--compiler-bindir"}
LINK_ONLY = {"-l", "-L", "-Xlinker", "-Xnvlink"}
FOLDERS = {"-I", "-isystem"}
BYPASS = {"-E", "-M", "-MM", "-MD", "-MMD", "-x", "-dc", "-dlink", "-ptx", "-cubin", "-fatbin", "-lib", "-shared",
          "--shared", "--version", "-V", "--preprocess", "--device-c", "--device-link", "--lib", "-run", "--run",
          "-dryrun", "--dryrun"}


def git(root, *arguments):
    return subprocess.run(["git", "-C", root] + list(arguments), capture_output=True, check=True).stdout


def same_path(path):
    return os.path.normcase(os.path.abspath(path))


def key(roots):
    digest = hashlib.sha256()
    headers = {}
    for root in roots:
        digest.update(b"root\0" + os.path.abspath(root).encode() + b"\0")
        if os.path.isfile(os.path.join(root, ".pinned")):
            for folder, _, names in os.walk(root):
                for name in names:
                    if name.lower().endswith(HEADERS):
                        headers.setdefault(name, set()).add(same_path(os.path.join(folder, name)))
            continue
        for path in git(root, "ls-files", "-co", "--exclude-standard", "-z").split(b"\0"):
            name = os.path.basename(path.decode())
            if name.lower().endswith(HEADERS):
                headers.setdefault(name, set()).add(same_path(os.path.join(root, path.decode())))
    shared = sorted(path for paths in headers.values() if len(paths) > 1 for path in paths)
    for path in shared:
        digest.update(b"shared\0" + path.encode() + b"\0")
    real = shutil.which("nvcc")
    if real:
        digest.update(subprocess.run([real, "--version"], capture_output=True).stdout)
    answer = digest.hexdigest()
    folder = os.environ.get("COMPILE_CACHE_DIR", "")
    if folder:
        os.makedirs(folder, exist_ok=True)
        lines = ["root " + same_path(root) for root in roots] + ["shared " + path for path in shared]
        written = os.path.join(folder, answer + ".roots")
        partial = "%s.%d.part" % (written, os.getpid())
        with open(partial, "w", encoding="utf-8") as handle:
            handle.write("\n".join(lines) + "\n")
        os.replace(partial, written)
    return answer


def split(arguments):
    """(the flags a source compiles with, the output or None, [(index, source)]), or None where the call is not a
    plain compile-and-link of sources. The compile flags leave out the output, the other files and what only the
    link reads (-l, -L, -Xlinker)."""
    flags = []
    output = None
    sources = []
    at = 0
    while at < len(arguments):
        argument = arguments[at]
        name = argument.split("=", 1)[0]
        if name in BYPASS or argument.startswith(("-M", "--generate-dependencies")):
            return None
        if argument in VALUED and at + 1 < len(arguments):
            if argument in ("-o", "--output-file"):
                output = arguments[at + 1]
            elif argument not in LINK_ONLY:
                flags += [argument, arguments[at + 1]]
            at += 2
            continue
        if not argument.startswith("-"):
            if argument.lower().endswith(SOURCES):
                sources.append((at, argument))
        elif name in ("-o", "--output-file"):
            output = argument.split("=", 1)[1]
        elif not argument.startswith(("-l", "-L")):
            flags.append(argument)
        at += 1
    return (flags, output, sources) if sources else None


def named_flags(flags, folder, cache_key):
    """The flags that make the name of an object. They leave out an include folder inside the roots where no
    shared-name header lies under it. Without the roots file they hold every flag."""
    try:
        with open(os.path.join(folder, cache_key + ".roots"), encoding="utf-8") as handle:
            lines = handle.read().splitlines()
    except OSError:
        return flags
    roots = [line[5:] for line in lines if line.startswith("root ")]
    shared = [line[7:] for line in lines if line.startswith("shared ")]

    def plain(include):
        path = same_path(include)
        inside = any(path == root or path.startswith(root.rstrip(os.sep) + os.sep) for root in roots)
        under = path.rstrip(os.sep) + os.sep
        return inside and not any(header.startswith(under) for header in shared)

    retained = []
    at = 0
    while at < len(flags):
        if flags[at] in FOLDERS and at + 1 < len(flags):
            if not plain(flags[at + 1]):
                retained += flags[at:at + 2]
            at += 2
            continue
        if flags[at].startswith("-I") and len(flags[at]) > 2 and plain(flags[at][2:]):
            at += 1
            continue
        retained.append(flags[at])
        at += 1
    return retained


def stale(source, target):
    """True where the object target of source compiles again. The object or its .d file is not there. Or the source
    or a file that the .d file names is newer than the object. A space that the .d file escapes stays in its path."""
    try:
        made = os.path.getmtime(target)
        with open(target + ".d", encoding="utf-8", errors="replace") as handle:
            written = handle.read()
    except OSError:
        return True
    words = re.split(r"(?<!\\)\s+", written.replace("\\\n", " "))
    # the first word is the object, which the .d file names as its target, and its colon follows it
    for word in [source] + words[1:]:
        path = word.replace("\\ ", " ")
        if not path or path == ":" or path.endswith(":"):
            continue
        try:
            if os.path.getmtime(path) > made:
                return True
        except OSError:
            return True
    return False


def main():
    if sys.argv[1] == "key":
        print(key(sys.argv[2:]))
        return 0
    real = sys.argv[2]
    arguments = sys.argv[3:]
    cache_key = os.environ.get("COMPILE_CACHE_KEY", "")
    folder = os.environ.get("COMPILE_CACHE_DIR", "")
    parts = split(arguments) if cache_key and folder else None
    if parts is None:
        return subprocess.run([real] + arguments).returncode
    flags, output, sources = parts
    compile_only = "-c" in flags
    if compile_only and (len(sources) != 1 or output is None):
        return subprocess.run([real] + arguments).returncode
    extension = ".obj" if os.name == "nt" or sys.platform in ("msys", "cygwin") or real.lower().endswith(".exe") \
        else ".o"
    compile_flags = [f for f in flags if f != "-c"]
    name_flags = named_flags(compile_flags, folder, cache_key)
    wait = float(os.environ.get("COMPILE_CACHE_WAIT", "900"))
    os.makedirs(folder, exist_ok=True)

    def object_of(source):
        name = hashlib.sha256("\0".join([cache_key, same_path(source)] + name_flags).encode()).hexdigest()
        return os.path.join(folder, os.path.splitext(os.path.basename(source))[0] + "_" + name[:24] + extension)

    def build(source):
        target = object_of(source)
        lock = target + ".lock"
        began = time.monotonic()
        held = False
        while stale(source, target):
            try:
                os.close(os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
                held = True
                break
            except FileExistsError:
                if time.monotonic() - began > wait:
                    break
                time.sleep(1.0)
        try:
            if not stale(source, target):
                return 0
            partial = "%s.%d.part%s" % (target, os.getpid(), extension)
            status = subprocess.run([real, "-c"] + compile_flags + ["-MD", "-MF", partial + ".d", source, "-o",
                                                                     partial]).returncode
            if status == 0 and os.path.isfile(partial) and os.path.isfile(partial + ".d"):
                os.replace(partial + ".d", target + ".d")
                os.replace(partial, target)
                return 0
            for written in (partial, partial + ".d"):
                if os.path.isfile(written):
                    os.remove(written)
            return status or 1
        finally:
            if held:
                os.remove(lock)

    jobs = max(1, int(os.environ.get("COMPILE_CACHE_JOBS", "4")))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        statuses = list(pool.map(build, [source for _, source in sources]))
    if any(statuses):
        return next(s for s in statuses if s)
    if compile_only:
        shutil.copyfile(object_of(sources[0][1]), output)
        return 0
    linked = list(arguments)
    for index, source in sources:
        linked[index] = object_of(source)
    return subprocess.run([real] + linked).returncode


sys.exit(main())
