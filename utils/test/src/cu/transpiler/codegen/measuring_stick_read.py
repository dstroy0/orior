# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# measuring_stick_read.py: nvcc's listing of the measuring stick read kernel by kernel, the answer key what the engine
# builds for each function of the language is held against. A kernel's operations are its instructions' operations,
# the guard cut and NOP left out, each as many times as it is written. Every operation nvcc writes that no form of
# sass.krs writes is where sass.krs falls short, and the record counts the kernels each is in.
#
#
# Given the run of measuring_stick_engine.sh, each kernel it writes through cu.krs and sass.krs is held against nvcc's:
# the operations nvdisasm reads back from the kernel's encodings, against the operations of nvcc's listing. A kernel is
# at parity where the two are the same multiset. The read back is checked against the kernel's own text, instruction by
# instruction: the assembler and the disassembler agree on every operation the comparison counts.
#
#     python measuring_stick_read.py <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine directory>]
import collections
import os
import re
import sys


def operation_of(line):
    """the operation of a listing's instruction line, its guard cut, or None"""
    found = re.match(r"\s*/\*[0-9a-f]{4,}\*/\s+(.*?)\s*;", line)
    if found is None:
        return None
    text = re.sub(r"^@!?U?P[T0-9]+\s+", "", found.group(1))
    return text.split()[0] if text.split() else None


def nvcc_read(path):
    """each kernel's operations from cuobjdump's listing, NOP left out"""
    kernels = {}
    name = None
    for line in open(path, encoding="utf-8", errors="replace"):
        found = re.match(r"\s*Function : (\S+)", line)
        if found:
            name = found.group(1)
            kernels[name] = []
            continue
        operation = operation_of(line)
        if operation and (operation != "NOP") and (name is not None):
            kernels[name].append(operation)
    return kernels


def listing_operations(path):
    """the operations of a listing nvdisasm wrote, in order, NOP left out"""
    operations = []
    for line in open(path, encoding="utf-8", errors="replace"):
        operation = operation_of(line)
        if operation and (operation != "NOP"):
            operations.append(operation)
    return operations


def lane_operations(path):
    """the operations of a lane's text, in order: each instruction's first word, its guard cut"""
    operations = []
    for line in open(path, encoding="utf-8", errors="replace"):
        text = line.strip()
        if (not text) or text.startswith(("//", ".")) or text.endswith(":"):
            continue
        text = re.sub(r"^@!?U?P[T0-9]+\s+", "", text)
        operations.append(text.split()[0].rstrip(";"))
    return operations


def engine_read(table, directory):
    """each kernel's line of the engine's run, and for each it answers its operations as nvdisasm reads them back
    and how many of them agree with the lane's text, position by position"""
    lines = {}
    for line in open(table, encoding="utf-8"):
        number, answer, steps, instructions, note = line.rstrip("\n").split("\t", 4)
        if number == "number":
            continue
        entry = {"answer": answer, "steps": int(steps), "note": note}
        if answer == "answered":
            back = listing_operations(os.path.join(directory, number + ".dis"))
            text = [operation for operation in lane_operations(os.path.join(directory, number + ".sass"))
                    if operation != "NOP"]
            entry["operations"] = back
            entry["agree"] = sum(1 for left, right in zip(back, text) if left == right)
            entry["text"] = len(text)
        lines["measuring_stick_" + number] = entry
    return lines


def ruleset_operations(path):
    """the operations sass.krs's forms write: each instruction of each form's text, its guard cut"""
    operations = set()
    for line in open(path, encoding="utf-8"):
        if not line.startswith("form "):
            continue
        text = line.split(" = ", 1)[1] if " = " in line else ""
        for instruction in text.split("\\n"):
            instruction = instruction.replace("\\t", " ").strip()
            instruction = re.sub(r"^@!?\{?[A-Za-z0-9_]+\}?\s+", "", instruction)
            if instruction and not instruction.startswith(("//", ".")) and not instruction.endswith(":"):
                operations.add(instruction.split()[0].rstrip(";"))
    return operations


def main():
    if len(sys.argv) not in (5, 7):
        sys.stderr.write("measuring_stick_read.py <nvcc listing> <manifest> <sass.krs> <record> "
                         "[<engine.tsv> <engine directory>]\n")
        return 2
    engine = engine_read(sys.argv[5], sys.argv[6]) if len(sys.argv) == 7 else {}
    nvcc = nvcc_read(sys.argv[1])
    manifest = {}
    for line in open(sys.argv[2], encoding="utf-8"):
        number, category, text = line.rstrip("\n").split("\t", 2)
        if number != "number":
            manifest["measuring_stick_" + number] = (category, text)
    held = ruleset_operations(sys.argv[3])
    stick = sorted(name for name in nvcc if name in manifest)

    categories = collections.OrderedDict()
    missing = {}
    for name in stick:
        counts = categories.setdefault(manifest[name][0], [0, 0])
        counts[0] += 1
        counts[1] += len(nvcc[name])
        for operation in sorted(set(nvcc[name]) - held):
            missing.setdefault(operation, []).append(name)
    written = {operation for name in stick for operation in nvcc[name]}

    with open(sys.argv[4], "w", encoding="utf-8", newline="\n") as out:
        out.write("# The measuring stick: nvcc's listing, the answer key\n\n")
        out.write("Written by `measuring_stick.sh` whole on every run. Each kernel of the measuring stick "
                  "(measuring_stick.py), every function of the CUDA language in a frame of its own, is compiled by "
                  "nvcc for sm_86 and read here. What the engine builds for each function is held against it, a kernel "
                  "at parity where the engine's code uses the same operations as nvcc's, each as many times.\n\n")
        out.write("- kernels: %u\n- instructions: %u\n- operations nvcc writes over the stick: %u, of which sass.krs "
                  "writes %u\n\n" % (len(stick), sum(len(nvcc[name]) for name in stick), len(written),
                                     len(written & held)))
        if engine:
            answered = [name for name in stick if engine.get(name, {}).get("answer") == "answered"]
            parity = [name for name in answered
                      if collections.Counter(engine[name]["operations"]) == collections.Counter(nvcc[name])]
            read_back = sum(len(engine[name]["operations"]) for name in answered)
            agree = sum(engine[name]["agree"] for name in answered)
            text = sum(engine[name]["text"] for name in answered)
            out.write("- kernels the engine answers: %u, of which at parity with nvcc: %u; kernels that put a "
                      "question: %u\n- the engine's instructions: %u in its lanes' text, %u read back by nvdisasm, "
                      "%u of them the operation the text wrote\n\n"
                      % (len(answered), len(parity), len(stick) - len(answered), text, read_back, agree))
            out.write("## The engine against nvcc\n\nEach kernel the engine answers: its record steps, nvcc's "
                      "instructions and the engine's as nvdisasm reads them back, and the operations only one of "
                      "the two writes, each by how many more times it writes it.\n\n"
                      "| kernel | function | steps | nvcc | engine | only nvcc | only the engine |\n"
                      "|---|---|---|---|---|---|---|\n")
            for name in answered:
                ours = collections.Counter(engine[name]["operations"])
                theirs = collections.Counter(nvcc[name])
                out.write("| %s | `%s` | %u | %u | %u | %s | %s |\n" % (
                    name[len("measuring_stick_"):], manifest[name][1].replace("|", "\\|"), engine[name]["steps"],
                    len(nvcc[name]), len(engine[name]["operations"]),
                    ", ".join("%s %u" % item for item in sorted((theirs - ours).items())),
                    ", ".join("%s %u" % item for item in sorted((ours - theirs).items()))))
            questions = collections.OrderedDict()
            for name in stick:
                entry = engine.get(name)
                if (entry is not None) and (entry["answer"] != "answered"):
                    questions.setdefault(entry["note"], []).append(name)
            out.write("\n## The questions the engine puts\n\nEach kernel the engine does not answer, by the question "
                      "it puts.\n\n| question | kernels | first kernel |\n|---|---|---|\n")
            for note, names in sorted(questions.items(), key=lambda item: (-len(item[1]), item[0])):
                out.write("| %s | %u | %s |\n" % (note.replace("|", "\\|"), len(names),
                                                  names[0][len("measuring_stick_"):]))
            out.write("\n")
        out.write("## By category\n\n| category | kernels | nvcc instructions |\n|---|---|---|\n")
        for category, (count, instructions) in categories.items():
            out.write("| %s | %u | %u |\n" % (category, count, instructions))
        out.write("\n## Operations nvcc writes that no form of sass.krs writes\n\n"
                  "| operation | kernels it is in | first kernel | its function |\n|---|---|---|---|\n")
        for operation, names in sorted(missing.items(), key=lambda item: (-len(item[1]), item[0])):
            first = names[0]
            out.write("| `%s` | %u | %s | `%s` |\n" % (operation, len(names), first[len("measuring_stick_"):],
                                                       manifest[first][1].replace("|", "\\|")))
        out.write("\n## Every kernel\n\n| kernel | category | function | nvcc | its operations |\n"
                  "|---|---|---|---|---|\n")
        for name in stick:
            category, text = manifest[name]
            counted = collections.Counter(nvcc[name])
            out.write("| %s | %s | `%s` | %u | %s |\n" % (name[len("measuring_stick_"):], category,
                                                         text.replace("|", "\\|"), len(nvcc[name]),
                                                         ", ".join("%s %u" % item for item in sorted(counted.items()))))
    print("measuring stick: %u kernels, %u instructions in nvcc's listing, %u operations, %u of them written by "
          "sass.krs" % (len(stick), sum(len(nvcc[name]) for name in stick), len(written), len(written & held)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
