# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The compilation database clangd reads, written to build/compile_commands.json; .clangd at the repository's root
# points clangd at it. The engine, its sims and its tests build through nvcc, whose command lines clangd cannot read,
# so each source is given here as clang parses it: a .cu as CUDA for this machine's GPU, a .c as C11, and a header as
# C where some .c file reaches it through its includes and as CUDA where only .cu files do. Every directory under the
# trees below that holds a header is on the include path, a superset of what run.sh and CMakeLists.txt name, and an
# example's own directories come before them for its sources; no two headers of the trees, or of one example, share a
# name, which is checked, so the union resolves each include the one way the builds do. Run it again after a source or
# header is added or moved.
import glob
import json
import os
import re
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
while (ROOT != os.path.dirname(ROOT)) and not os.path.isdir(os.path.join(ROOT, "src", "cu", "engine")):
    ROOT = os.path.dirname(ROOT)
TREES = ("src", "includes", os.path.join("utils", "test"), os.path.join("utils", "bench"), "examples")
SKIPPED = {"python", "build", "node_modules", ".git"}
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.MULTILINE)
OUT = os.path.join(ROOT, "build", "compile_commands.json")
# MMgr and the embedded types under it retain a library's own layout, each include named from its include and src
# folders, where two headers may share a name: their headers are outside the union and are read as C, and every
# entry is given the folders and settings file_defs.sh gives a program that links MMgr (FILE_DEFS_MMGR_FLAGS)
LIBRARIES = tuple(os.path.join(ROOT, "includes", name) for name in ("mmgr", "embedded_types"))
FILE_DEFS = os.path.join(ROOT, "includes", "file_defs", "file_defs.sh")
ASSIGNED = re.compile(r'^([A-Z_]+)="([^"]*)"$', re.MULTILINE)
MMGR_FLAGS = re.compile(r"^FILE_DEFS_MMGR_FLAGS=\(([^)]*)\)", re.MULTILINE)


def slashed(path):
    return os.path.abspath(path).replace("\\", "/")


def sources():
    found = []
    for tree in TREES:
        for top, directories, files in os.walk(os.path.join(ROOT, tree)):
            directories[:] = sorted(name for name in directories if name not in SKIPPED)
            for name in sorted(files):
                if name.endswith((".c", ".cu", ".h")):
                    found.append(slashed(os.path.join(top, name)))
    return found


def in_library(path):
    return any(slashed(path).startswith(slashed(library) + "/") for library in LIBRARIES)


# FILE_DEFS_MMGR_FLAGS as file_defs.sh writes it, each variable it names given as that file assigns it, its own
# folder located for FILE_DEFS_SOURCE
def library_flags():
    with open(FILE_DEFS, encoding="utf-8") as script:
        text = script.read()
    known = {"FILE_DEFS_SOURCE": os.path.dirname(FILE_DEFS)}
    for name, value in ASSIGNED.findall(text):
        known.setdefault(name, value)
    words = MMGR_FLAGS.search(text)
    if words is None:
        return []
    flags = []
    for word in shlex.split(words.group(1)):
        for _ in range(len(known)):
            word = re.sub(r"\$\{?([A-Z_]+)\}?", lambda used: known.get(used.group(1), used.group(0)), word)
        flags.append(slashed(word) if os.sep in word or "/" in word else word)
    return flags


def included(path):
    with open(path, encoding="utf-8", errors="replace") as source:
        return [os.path.basename(name) for name in INCLUDE.findall(source.read())]


# the program a source builds into: an example's own folder under examples, whose builds see no other example's
# headers, or "" for every other tree
def scope_of(path):
    parts = os.path.relpath(path, ROOT).replace("\\", "/").split("/")
    return "/".join(parts[:2]) if (len(parts) > 2) and (parts[0] == "examples") else ""


# the header an include of `name` in a source of `scope` reaches: its program's own first, then the trees'
def header_of(by_name, scope, name):
    return by_name.get(scope, {}).get(name) or by_name[""].get(name)


# the headers some .c file reaches through its includes, followed by name through the repository's own headers
def c_headers(files, by_name):
    reached = set()
    waiting = [path for path in files if path.endswith(".c")]
    while waiting:
        including = waiting.pop()
        for name in included(including):
            header = header_of(by_name, scope_of(including), name)
            if (header is not None) and (header not in reached):
                reached.add(header)
                waiting.append(header)
    return reached


def version_key(path):
    return [int(part) if part.isdigit() else part for part in re.split(r"[.]", os.path.basename(path))]


# CUDA from CUDA_PATH or the nvcc on the path, the GPU's architecture as run.sh reads it, and on Windows the MSVC
# toolset and Windows SDK clang's CUDA headers need, each the newest installed
def toolchain():
    cuda = os.environ.get("CUDA_PATH", "")
    if not cuda:
        nvcc = shutil.which("nvcc")
        cuda = os.path.dirname(os.path.dirname(nvcc)) if nvcc else ""
    capability = ""
    try:
        answer = subprocess.run(["nvidia-smi", "--query-gpu=compute_cap", "--format=csv,noheader"],
                                capture_output=True, text=True, timeout=30)
        lines = answer.stdout.split()
        capability = lines[0].replace(".", "") if lines else ""
    except (OSError, subprocess.SubprocessError):
        capability = ""
    host = []
    if sys.platform == "win32":
        toolsets = sorted(glob.glob("C:/Program Files (x86)/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*")
                          + glob.glob("C:/Program Files/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*"), key=version_key)
        kits = "C:/Program Files (x86)/Windows Kits/10"
        versions = sorted(glob.glob(kits + "/Include/10.*"), key=version_key)
        if toolsets:
            host += ["-Xmicrosoft-visualc-tools-root", toolsets[-1].replace("\\", "/")]
        if versions:
            host += ["-Xmicrosoft-windows-sdk-root", kits, "-Xmicrosoft-windows-sdk-version",
                     os.path.basename(versions[-1])]
    return cuda.replace("\\", "/"), "sm_" + (capability or "86"), host


def main():
    files = sources()
    by_name = {"": {}}
    for path in files:
        if path.endswith(".h") and not in_library(path):
            name = os.path.basename(path)
            named = by_name.setdefault(scope_of(path), {})
            if name in named:
                print("  two headers named %s: %s and %s; the union include path cannot tell them apart"
                      % (name, named[name], path))
                return 1
            named[name] = path
    reached = c_headers(files, by_name)
    cuda, architecture, host = toolchain()
    folders = {scope: ["-I" + directory for directory in sorted({os.path.dirname(path) for path in named.values()})]
               for scope, named in by_name.items()}
    includes = library_flags() + folders[""]
    cuda_flags = ["clang++", "-xcuda", "--cuda-gpu-arch=" + architecture, "-Wno-unknown-cuda-version", "-std=c++17"]
    if cuda:
        cuda_flags.insert(2, "--cuda-path=" + cuda)
    # CUDA 13 retains cub, thrust and libcu++ under include/cccl, which nvcc puts on the path itself and clang does not
    if cuda and os.path.isdir(cuda + "/include/cccl"):
        cuda_flags += ["-isystem", cuda + "/include/cccl"]
    c_flags = ["clang", "-xc", "-std=c11"]
    entries = []
    counts = {"cuda": 0, "c": 0}
    for path in files:
        type = "c" if (path.endswith(".c") or (path in reached) or in_library(path)) else "cuda"
        counts[type] += 1
        flags = c_flags if type == "c" else cuda_flags
        scoped = folders.get(scope_of(path), []) if scope_of(path) else []
        entries.append({"directory": slashed(ROOT), "file": path,
                        "arguments": flags + host + scoped + includes + ["-c", path]})
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="ascii", newline="\n") as out:
        json.dump(entries, out, indent=1)
        out.write("\n")
    print("  %d entries (%d as CUDA for %s, %d as C11), %d include directories, CUDA at %s"
          % (len(entries), counts["cuda"], architecture, counts["c"], sum(len(each) for each in folders.values()),
             cuda or "none found"))
    print("  written to %s" % slashed(OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
