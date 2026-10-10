# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The query's questions read against the vendor's own disassembler, the object the carrier popped out under
# --diff-output-against-vendor. A question whose slot instruction the vendor calls illegal, reads as a control transfer
# our gate's operation key could not, reads as naming a register at or past the count its kernel allots, reads as
# writing a register the code after the slot addresses memory through, or reads as a memory access of its own, is one
# the gate passed that must be held before a series reaches the part. Our gate judges by operation key and by the fields
# a form has learned: it cannot know a whole encoding illegal while the operation it probes is still unlearned, nor
# where a register sits in a form whose fields are not yet asked. The vendor's reader prints both. A register past the
# count faults the part at once, an out-of-range register warp exception, and a run of them takes the display and the
# machine down with it. A slot that overwrites an address the kernel stores through, or that reads or writes memory
# through registers the question set no address in, faults it with an illegal address. Nothing here runs on the part.
#
#     measuring_stick_query.py <question-list> <nvdisasm> <architecture> <folder> <report> <held>
#
# The list is a line a question, `<code> <registers> <cases> <answers> <slot> [<launches>]` (run_channel.h), and the
# carrier has popped each question's container beside its answers file as <stem>.cubin. Every container is the kernel
# the system accepted with the question's code in it, and differs from the next only in that code; the slot instruction
# of each, as the part would be handed it, is read out of the container's code section. The slot instructions are read
# by the vendor's disassembler together, as one raw run of encodings (nvdisasm --binary <architecture>), into <folder>.
# The vendor stops at the first instruction it calls illegal and names its address: that question is held, the run
# before it is read again for its operations, and the reading goes on past it. A stop that names no address is read
# again a question at a time. Each slot the vendor reads whole has every register it names held to the question's
# <registers>, RZ aside, a register read as a pair (.64, .WIDE) reaching its second word and one of .128 its fourth.
# The code past each slot is read once, to its first EXIT with no guard, for the registers its memory operands read an
# address from; a slot that writes one of them is held, as is a slot with a memory operand of its own.
# The report lists every question held; <held> is the feedback the carrier reads next run
# (--held): a line `<low> <high>` an encoding, the slot instruction of each held question, for the carrier to hold off
# the part. The vendor names no operation and nothing it says enters a form: the held file is only the exact words not
# to ask. Exits nonzero where any question is held, to stop the loop before the part.
import os, re, struct, subprocess, sys

# the operations that transfer control or wait, which a question's slot must never be (cubin_safe.c, the same list)
CONTROL = ("BRA", "BRX", "JMP", "JMX", "CALL", "RET", "EXIT", "BSSY", "BSYNC", "BREAK", "BMOV", "WARPSYNC", "YIELD",
           "BAR", "DEPBAR", "NANOSLEEP", "BPT", "RTT", "KILL", "RPCMOV", "RETIRE", "PMTRIG")

# an executable section of an ELF, by the flag its header carries
SECTION_EXECUTABLE = 0x4

ILLEGAL = "the vendor reads an illegal instruction our gate's key passed"


def slot_encoding(code_path, slot):
    """the question's own slot instruction, its two words, or None"""
    with open(code_path, "rb") as code:
        code.seek(slot * 16)
        word = code.read(16)
    if len(word) < 16:
        return None
    return struct.unpack("<Q", word[0:8])[0], struct.unpack("<Q", word[8:16])[0]


def container_code(cubin_path):
    """the first executable section of the container, its bytes, or None"""
    with open(cubin_path, "rb") as container:
        data = container.read()
    if (len(data) < 0x40) or (data[:4] != b"\x7fELF"):
        return None
    section_offset = struct.unpack_from("<Q", data, 0x28)[0]
    entry_size, entries = struct.unpack_from("<HH", data, 0x3A)
    for index in range(entries):
        header = section_offset + (index * entry_size)
        if header + 0x28 > len(data):
            return None
        flags = struct.unpack_from("<Q", data, header + 0x08)[0]
        offset, size = struct.unpack_from("<QQ", data, header + 0x18)
        if (flags & SECTION_EXECUTABLE) and (size != 0):
            return data[offset:offset + size]
    return None


def operations(listing):
    """each instruction's operation in a listing and its text past the guard, by its offset"""
    found = {}
    for match in re.finditer(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;", listing):
        body = re.sub(r"^@!?U?P[T0-9]+\s+", "", match.group(2).strip())
        first = body.split()[0] if body.split() else ""
        found[int(match.group(1), 16)] = (first.split(".")[0], body)
    return found


# a general register as the vendor prints it, with the width of a pair or a quad where the operand names one; RZ and the
# uniform registers (UR) are not matched
REGISTER = re.compile(r"\bR(\d+)(\.64|\.128)?\b")


# a memory operand, read through registers: a bracket that opens no constant bank, `c[...][...]`
MEMORY = re.compile(r"(?<![\]cC])\[")
# a general register a memory operand reads its address from, with the pair's second word where it reads .64
ADDRESS = re.compile(r"(?<![\]cC])\[\s*R(\d+)(\.64)?")


def addresses_after(listing):
    """every general register the code in `listing` reads an address from, up to its first EXIT with no guard: the
    register a memory operand names, and the register after it where the operand reads a pair"""
    found = set()
    for match in re.finditer(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;", listing):
        text = match.group(2).strip()
        for address in ADDRESS.finditer(text):
            found.add(int(address.group(1)))
            if address.group(2):
                found.add(int(address.group(1)) + 1)
        if re.match(r"EXIT\b", text):
            break
    return found


def registers_written(body):
    """the general registers the instruction `body` writes: its first operand where that is one, and the rest of a pair
    or a quad where the operation reads .WIDE, .64 or .128"""
    words = body.split()
    first = re.match(r"R(\d+)\b", words[1]) if len(words) > 1 else None
    if first is None:
        return set()
    number = int(first.group(1))
    width = 3 if ".128" in words[0] else (1 if (".WIDE" in words[0]) or (".64" in words[0]) else 0)
    return set(range(number, number + width + 1))


def register_reached(body, registers):
    """the highest register the instruction `body` reaches at or past `registers`, the count its kernel allots: a pair
    reaches the register after the one named and a quad the third after it, as does every register of an operation read
    .WIDE, .64 or .128; None where every register it reaches lies below the count, or no count is given"""
    if registers == 0:
        return None
    words = body.split()
    operation = words[0] if words else ""
    widened = 3 if ".128" in operation else (1 if (".WIDE" in operation) or (".64" in operation) else 0)
    highest = None
    for match in REGISTER.finditer(body[len(operation):]):
        width = 3 if match.group(2) == ".128" else (1 if match.group(2) == ".64" else widened)
        reached = int(match.group(1)) + width
        if (reached >= registers) and ((highest is None) or (reached > highest)):
            highest = reached
    return highest


class Reader:
    """the vendor's disassembler over raw runs of instructions, each run's listing kept in a folder"""

    def __init__(self, nvdisasm, architecture, folder):
        self.nvdisasm, self.architecture, self.folder, self.calls = nvdisasm, architecture, folder, 0

    def read(self, words):
        """the vendor's reading of `words` as one raw run: its exit status and everything it printed"""
        self.calls += 1
        path = os.path.join(self.folder, "run%u.bin" % self.calls)
        with open(path, "wb") as run:
            run.write(b"".join(words))
        done = subprocess.run([self.nvdisasm, "--binary", self.architecture, path], capture_output=True, text=True,
                              errors="replace")
        printed = done.stdout + done.stderr
        with open(os.path.join(self.folder, "run%u.txt" % self.calls), "w", newline="\n") as listing:
            listing.write(printed)
        return done.returncode, printed


def readings(reader, pending):
    """each pending question's reading, illegal or its slot's operation: `pending` a list of (place, word), and the
    readings by place"""
    read = {}
    while pending:
        status, printed = reader.read([word for _, word in pending])
        if status == 0:
            found = operations(printed)
            for at, (place, _) in enumerate(pending):
                read[place] = found.get(at * 16)
            return read
        stop = re.search(r"at address 0x([0-9a-f]+)", printed)
        if (stop is None) or ((int(stop.group(1), 16) // 16) >= len(pending)):
            if len(pending) == 1:
                read[pending[0][0]] = None if "Illegal instruction found" not in printed else ILLEGAL
                return read
            for one in pending:
                read.update(readings(reader, [one]))
            return read
        held = int(stop.group(1), 16) // 16
        if held > 0:
            read.update(readings(reader, pending[:held]))
        read[pending[held][0]] = ILLEGAL
        pending = pending[held + 1:]
    return read


def main(argv):
    if len(argv) != 7:
        sys.stderr.write("measuring_stick_query.py <question-list> <nvdisasm> <architecture> <folder> <report> <held>\n")
        return 2
    listing, nvdisasm, architecture, folder, report, held = argv[1:7]
    os.makedirs(folder, exist_ok=True)
    # a question with a slot is read by its slot instruction, among every other's; one with none, the kernel's own, is
    # read whole, once a code
    questions = []
    whole = {}
    with open(listing) as lines:
        for line in lines:
            field = line.split()
            if len(field) < 5:
                continue
            code_path, registers, answers, slot = field[0], int(field[1]), field[3], int(field[4])
            cubin = os.path.splitext(answers)[0] + ".cubin"
            code = container_code(cubin) if os.path.exists(cubin) else None
            if code is None:
                continue
            stem = os.path.splitext(os.path.basename(answers))[0]
            if (slot * 16) + 16 <= len(code):
                questions.append((stem, code_path, slot, code[slot * 16:(slot * 16) + 16], code, registers))
            else:
                questions.append((stem, code_path, slot, None, code, registers))
                whole.setdefault(code, None)
    reader = Reader(nvdisasm, architecture, folder)
    read = readings(reader, [(place, question[3]) for place, question in enumerate(questions)
                             if question[3] is not None])
    for code in whole:
        status, printed = reader.read([code])
        whole[code] = ILLEGAL if ((status != 0) or ("Illegal instruction found" in printed)) else None
    # the registers the code past each slot reads an address from, read once for each code past a slot, the kernel's own
    # code being the same in every question but the slot
    addressed = {}
    for stem, code_path, slot, word, code, registers in questions:
        tail = code[(slot + 1) * 16:] if word is not None else b""
        if (word is not None) and (tail not in addressed):
            status, printed = reader.read([tail])
            addressed[tail] = addresses_after(printed)
    holds = []
    for place, (stem, code_path, slot, word, code, registers) in enumerate(questions):
        reading = read.get(place) if word is not None else whole.get(code)
        operation, body = reading if isinstance(reading, tuple) else (reading, "")
        reached = register_reached(body, registers)
        tail = code[(slot + 1) * 16:] if word is not None else b""
        overwritten = sorted(registers_written(body) & addressed.get(tail, set()))
        operands = body[len(body.split()[0]):] if body.split() else ""
        why = None
        if operation == ILLEGAL:
            why = ILLEGAL
        elif operation in CONTROL:
            why = "the vendor reads %s at the slot, a control transfer" % operation
        elif reached is not None:
            why = "the vendor reads R%u at the slot, past the %u registers its kernel allots" % (reached, registers)
        elif overwritten:
            why = "the vendor reads R%u written at the slot, a register the code after it addresses memory through" \
                  % overwritten[0]
        elif MEMORY.search(operands):
            why = "the vendor reads a memory access at the slot, through registers the question sets no address in"
        if why is not None:
            holds.append((stem, why, slot_encoding(code_path, slot)))

    with open(held, "w", newline="\n") as out:
        for stem, why, encoding in holds:
            if encoding is not None:
                out.write("%016x %016x\n" % encoding)
    with open(report, "w", newline="\n") as out:
        out.write("questions read: %d in %d readings; held off the part: %d\n" % (len(questions), reader.calls,
                                                                                    len(holds)))
        for stem, why, encoding in holds:
            where = ("%016x %016x" % encoding) if encoding else "(no slot encoding)"
            out.write("  %s  %s  [%s]\n" % (stem, why, where))

    sys.stdout.write("  questions read: %d in %d readings; the vendor holds %d off the part\n" % (len(questions),
                                                                                              reader.calls, len(holds)))
    for stem, why, encoding in holds:
        sys.stdout.write("    %s  %s\n" % (stem, why))
    return 1 if holds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
