# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The query's questions read against the vendor's own disassembler, the object the carrier popped out under
# --diff-output-against-vendor. Each container the carrier would hand the part is disassembled by nvdisasm; a question
# whose code the vendor calls illegal, or reads as a control transfer our gate's operation key could not, is one the
# gate passed that must be held before a series reaches the part. Our gate judges by operation key and cannot know a
# whole encoding illegal while the operation it probes is still unlearned; only the vendor's reader can. Nothing here
# runs on the part.
#
#     measuring_stick_query.py <question-list> <nvdisasm-output-dir> <report> <held>
#
# The list is a line a question, `<code> <registers> <cases> <answers> <slot> [<launches>]` (run_channel.h). The
# directory holds one <name>.sass a container, each nvdisasm's reading of the popped container, <name> the answers
# file's stem. The report lists every question held; <held> is the feedback the carrier reads next run (--held): a
# line `<low> <high>` an encoding, the slot instruction of each held question, for the carrier to hold off the part.
# The vendor names no operation and nothing it says enters a form: the held file is only the exact words not to ask.
# Exits nonzero where any question is held, to stop the loop before the part.
import os, re, struct, sys

# the operations that transfer control or wait, which a question's slot must never be (cubin_safe.c, the same list)
CONTROL = ("BRA", "BRX", "JMP", "JMX", "CALL", "RET", "EXIT", "BSSY", "BSYNC", "BREAK", "BMOV", "WARPSYNC", "YIELD",
           "BAR", "DEPBAR", "NANOSLEEP", "BPT", "RTT", "KILL", "RPCMOV", "RETIRE", "PMTRIG")


def operation_at(text, offset):
    match = re.search(r"/\*0*%x\*/\s+(.*?);" % offset, text)
    if match is None:
        return None
    body = re.sub(r"^@!?U?P[T0-9]+\s+", "", match.group(1).strip())
    first = body.split()[0] if body.split() else ""
    return first.split(".")[0]


def slot_encoding(code_path, slot):
    with open(code_path, "rb") as code:
        code.seek(slot * 16)
        word = code.read(16)
    if len(word) < 16:
        return None
    low = struct.unpack("<Q", word[0:8])[0]
    high = struct.unpack("<Q", word[8:16])[0]
    return low, high


def main(argv):
    if len(argv) != 5:
        sys.stderr.write("measuring_stick_query.py <question-list> <nvdisasm-output-dir> <report> <held>\n")
        return 2
    listing, directory, report, held = argv[1], argv[2], argv[3], argv[4]
    read = 0
    holds = []
    with open(listing) as lines:
        for line in lines:
            field = line.split()
            if len(field) < 5:
                continue
            code_path, slot = field[0], int(field[4])
            answers = field[3]
            stem = os.path.splitext(os.path.basename(answers))[0]
            sass = os.path.join(directory, stem + ".sass")
            if not os.path.exists(sass):
                continue
            read += 1
            text = open(sass, errors="replace").read()
            why = None
            if "Illegal instruction found" in text:
                why = "the vendor reads an illegal instruction our gate's key passed"
            else:
                operation = operation_at(text, slot * 16)
                if operation in CONTROL:
                    why = "the vendor reads %s at the slot, a control transfer" % operation
            if why is not None:
                encoding = slot_encoding(code_path, slot)
                holds.append((stem, why, encoding))

    with open(held, "w", newline="\n") as out:
        for stem, why, encoding in holds:
            if encoding is not None:
                out.write("%016x %016x\n" % encoding)
    with open(report, "w", newline="\n") as out:
        out.write("questions read: %d; held off the part: %d\n" % (read, len(holds)))
        for stem, why, encoding in holds:
            where = ("%016x %016x" % encoding) if encoding else "(no slot encoding)"
            out.write("  %s  %s  [%s]\n" % (stem, why, where))

    sys.stdout.write("  questions read: %d; the vendor holds %d off the part\n" % (read, len(holds)))
    for stem, why, encoding in holds:
        sys.stdout.write("    %s  %s\n" % (stem, why))
    return 1 if holds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
