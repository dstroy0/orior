# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The scaffolding's cubins read against the vendor's own disassembler before any reaches the part: the airlock the
# scaffolding shares with the carrier. Each cubin a list names is decoded as one raw run (nvdisasm --binary), and an
# encoding the vendor calls illegal, which our gate's operation key passed while the operation it probes is still
# unlearned, is held off the part. The kernel the scaffolding writes is a real listing with one slot instruction varied,
# so a legal pattern decodes whole and only an illegal slot stops the reader. Nothing here runs on the part.
#
#     interface_sass_held.py <list> <nvdisasm> <architecture> <folder> <held>
#
# The list is a line a cubin, `<cubin-path> <kernel>` (interface_sass_run's own list); a `skip` line is passed over.
# <held> is the feedback interface_sass_run reads next (--held): a line `<low> <high>` an encoding to hold off the part.
# Exits 1 where the vendor holds one or more, 0 where none, 2 where a tool or file was not reached.
import os, re, struct, subprocess, sys

# an executable section of an ELF, by the flag its header carries
SECTION_EXECUTABLE = 0x4


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


class Reader:
    """the vendor's disassembler over raw runs of instructions, each run's binary kept in a folder"""

    def __init__(self, nvdisasm, architecture, folder):
        self.nvdisasm, self.architecture, self.folder, self.calls, self.reached = nvdisasm, architecture, folder, 0, True

    def read(self, words):
        """the vendor's reading of `words` as one raw run: its exit status and everything it printed"""
        self.calls += 1
        path = os.path.join(self.folder, "run%u.bin" % self.calls)
        with open(path, "wb") as run:
            run.write(b"".join(words))
        try:
            done = subprocess.run([self.nvdisasm, "--binary", self.architecture, path], capture_output=True,
                                  text=True, errors="replace")
        except OSError:
            self.reached = False
            return -1, ""
        return done.returncode, done.stdout + done.stderr


def illegal_indices(reader, words):
    """the indices of `words`, each a 16-byte encoding, the vendor calls illegal: read as one raw run and split past
    each stop, since the reader stops at the first illegal encoding and names its address, every encoding before it
    read as legal"""
    held = set()
    segments = [(0, words)]
    while segments:
        base, run = segments.pop()
        if not run:
            continue
        status, printed = reader.read(run)
        if not reader.reached:
            return held
        if status == 0:
            continue
        stop = re.search(r"at address 0x([0-9a-f]+)", printed)
        if (stop is None) or ((int(stop.group(1), 16) // 16) >= len(run)):
            # a refusal naming no encoding in the run: halve it to find the one, a single encoding read on its own word
            if len(run) == 1:
                if "Illegal instruction found" in printed:
                    held.add(base)
                continue
            middle = len(run) // 2
            segments.append((base + middle, run[middle:]))
            segments.append((base, run[:middle]))
            continue
        place = int(stop.group(1), 16) // 16
        held.add(base + place)
        segments.append((base + place + 1, run[place + 1:]))
    return held


def main(argv):
    if len(argv) != 6:
        sys.stderr.write("interface_sass_held.py <list> <nvdisasm> <architecture> <folder> <held>\n")
        return 2
    listing, nvdisasm, architecture, folder, held = argv[1:6]
    if not os.path.exists(listing):
        sys.stderr.write("  the list %s was not found\n" % listing)
        return 2
    os.makedirs(folder, exist_ok=True)
    reader = Reader(nvdisasm, architecture, folder)
    # every cubin's encodings in one order, each word remembered by its place: an illegal one is held by its encoding
    words = []
    cubins = 0
    with open(listing) as lines:
        for line in lines:
            field = line.split()
            if (len(field) < 1) or (field[0] == "skip"):
                continue
            code = container_code(field[0]) if os.path.exists(field[0]) else None
            if code is None:
                continue
            cubins += 1
            whole = len(code) - (len(code) % 16)
            for at in range(0, whole, 16):
                words.append(code[at:at + 16])
    illegal = illegal_indices(reader, words)
    if not reader.reached:
        sys.stderr.write("  the vendor's disassembler %s was not reached\n" % nvdisasm)
        return 2
    encodings = []
    seen = set()
    for place in sorted(illegal):
        low = struct.unpack("<Q", words[place][0:8])[0]
        high = struct.unpack("<Q", words[place][8:16])[0]
        if (low, high) not in seen:
            seen.add((low, high))
            encodings.append((low, high))
    with open(held, "w", newline="\n") as out:
        for low, high in encodings:
            out.write("%016x %016x\n" % (low, high))
    sys.stdout.write("  cross: %d cubin(s) read, the vendor holds %d encoding(s) off the part\n"
                     % (cubins, len(encodings)))
    return 1 if encodings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
