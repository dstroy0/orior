// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Whether a kernel's machine code may be put to the part, read on the host before the driver is passed it
#ifndef CUBIN_SAFE_H
#define CUBIN_SAFE_H

// A cubin that the driver gets runs exactly as its bytes say. Our own reader reads machine code from our own
// assembler, or from a probe that turned one bit, against the machine file. The code goes to the part only where
// every instruction that it reaches satisfies these rules, in order:
//
//   1. its scheduler's bits wait on all six barriers. Where no form matches the instruction, they stall the longest.
//      A stall short of the soonest read of a result gives a wrong answer. It never gives a kernel that does not
//      return. The run channel queries below that stall to find where the soonest read lies
//      (sass_operation_schedule).
//   2. where a form of the machine file matches its encoding, that form neither transfers control nor waits
//      (sass_operation_control_or_wait). EXIT alone is the exception.
//   3. where no form matches it, its operation key has forms, and no form under the key transfers control or waits.
//      Whatever its other bits specify, it falls through to the next instruction.
//
//      At max one instruction matches no form: the one that a probe queries. A conference must admit an encoding
//      whose key has no form. The gate then puts it exactly as its lesson gives it (overseer.h,
//      cubin_safe_admitted). The cross read it as a legal instruction: no control transfer, wait or memory access.
//      It names no register past the count, and no party blocked it.
//   4. the walk reaches an EXIT with no guard: no thread runs off the end of the code.
//   5. an operation whose result is back in a fixed count of cycles sets no barrier. The exception is where the
//      encoding that the system wrote its form with sets one. Nothing releases a barrier that it sets. The wait of
//      the next instruction on all six then never ends, and the kernel never returns.
//   6. every operand that names a register names one that the kernel allots. That is a register below the count
//      that the kernel allots, or RZ, the zero register 255. A register at the count or above it is one that the
//      kernel never allotted. The part faults on it, with an out-of-range register warp exception over its whole
//      machine.
//
//      Where the gate reads no count, the ceiling of the part bounds it: register_last, in the .ksc beside the
//      machine file. The form found by the probe says which bits of an instruction name a register. The gate reads
//      the whole fixed eight-bit field of each register, and never a partial run of it. A form with no query of its
//      fields yet names no register here. The cross then blocks that code from the part, on the vendor's own reading
//      of each register. The verdict reaches this gate (measuring_stick_query.c).
//
// The gate reads the code from its first instruction to its first EXIT with no guard. It reads nothing past that,
// because nothing reaches it: the self branch and the padding after the last exit of a kernel never run. Nothing
// here changes the code.
//
// Code that satisfies these then runs under two rules more, read off the part as each launch runs:
//
//   7. a launch runs alone, between two events that the part stamps from its own timer. The host reads its stop
//      event and does not wait on it. The read stops when the part reaches the event, or when the driver gives an
//      error on it. It also stops when CUBIN_SAFE_LAUNCH_BOUND passes on the clock of the host. The time between the
//      two events is the block latency of the launch.
//
//      A launch that the driver ends with CUDA_ERROR_LAUNCH_TIMEOUT breaks this rule. So does one past the bound
//      with its stop event unreached. The answer for it is a refusal, and R then blocks it from the part ever after.
//      Nothing launches after it in that process. No launch waits in a queue behind another. A launch that does not
//      return reads as the driver's timeout, and not as a case for the system's watchdog.
//   8. the host reads the fault telemetry of the part after each launch. That is the Xid critical errors that the
//      management layer of the driver (NVML) stamps against the device. A launch that makes the part stamp one
//      faulted it. Its result is wrong, and its context is corrupt. Every launch after it in the process is invalid,
//      and it breaks this rule.
//
//      The host reads a fault that the driver names itself on the stop event of the launch the same way. That is an
//      illegal address or instruction, a hardware stack error, a misaligned address, or an address space out of
//      reach. It is also a program counter out of range, or a launch that the part failed outright. The stamp may
//      reach the host a moment behind the driver's own word.
//
//      The answer for the launch is a refusal, and R then blocks it from the part ever after. Nothing launches after
//      it in that process. This catches an out-of-range register that the part faults on in microseconds. The bound
//      of rule 6, read for a launch that runs too long, never sees it.
//
// Where the tables step pulls the vendor's instruction set, rules 2 and 3 read the class of each encoding off it as
// well. They treat an operation that it names as a control transfer or a wait. The name that the form of the machine
// file gives the encoding does not matter (class.c). A kernel's own places, passed as forms with no operation of
// their own, read the same way, by what they are. Two rules more read the code against the instruction set:
//
//  13. an instruction of a class that reads the constant bank reads bank 0, the bank of the parameters. That is the
//      only bank that a container of ours carries a section for. The read ends inside that section
//      (cubin_safe_tables_read, constant.c).
//  14. an encoding satisfies the conditions of its class on its registers, OOR_REG_ERROR and
//      MISALIGNED_REG_ERROR, as the instruction set states them. A register is in range for its width, and a 64-bit
//      register pair is even.
//
//      This is the vendor's pipe, with its rules read off its text before any query of the part. It is apart from
//      ours: the answers of the part, as the learned files store them:
//        - the forms of the .khw.
//        - the answers and the refusals of the .ksc.
//        - the ruleset of the .krs, the record of the .kqr, and R over them.
//
//      The verdict names which pipe blocked a question, and a comparison of the two is then possible (class.c,
//      tables_class_legal).

#include "sass_machine.h"

// what the code is: safe, or the first rule that an instruction broke
enum CubinSafe
{
    CUBIN_SAFE = 0,
    CUBIN_SAFE_SIZE = 1,
    CUBIN_SAFE_STALL = 2,
    CUBIN_SAFE_WAIT = 3,
    CUBIN_SAFE_CONTROL = 4,
    CUBIN_SAFE_KEY = 5,
    CUBIN_SAFE_UNGATED = 6,
    CUBIN_SAFE_EXIT = 7,
    CUBIN_SAFE_BARRIER = 8,
    CUBIN_SAFE_LAUNCH_TIMEOUT = 9,
    CUBIN_SAFE_LAUNCH_FAULT = 10,
    CUBIN_SAFE_REGISTER = 11,
    CUBIN_SAFE_GATED = 12,
    CUBIN_SAFE_CONSTANT = 13,
    CUBIN_SAFE_ILLEGAL = 14
};

// the vendor's instruction set as tables/read.h reads it
struct TablesSetTag;

// The vendor's instruction set for `part`, read once off build/vendor/nvidia/tables/<part>_instructions.txt, the file
// that the tables step pulls, for rule 13. 1 where it read and contains a class. 0 where the file is absent or contains
// none, and rule 13 then blocks nothing
int cubin_safe_tables_read(const char *part);

int cubin_safe_tables_ready(void);

// Rule 13: 1 where the encoding `low` and `high` belongs to a class of `tables` that reads the constant bank, and reads
// a bank other than 0. Bank 0 is the bank of the parameters, the one that a container of ours carries a section for. It
// is also 1 where `bank_size` is not 0 and a word ends past the `bank_size` bytes of the bank. The field of the offset
// counts words of four. 0 for an encoding of no class, or of a class that reads no bank
int cubin_constant_out_of_bank(const struct TablesSetTag *tables, unsigned long long low, unsigned long long high,
                               unsigned long long bank_size);

// What the driver gives for a launch the system's watchdog ended (CUDA_ERROR_LAUNCH_TIMEOUT), and for an event the part
// has not reached yet (CUDA_ERROR_NOT_READY)
#define CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT 702
#define CUBIN_SAFE_STATUS_NOT_READY 600

// What the driver gives for a launch that faulted the part and left its context corrupt, with every launch after it in
// the process invalid. The causes are an illegal address, a hardware stack error, an illegal instruction, or a
// misaligned address. They are also an address space out of reach, a program counter out of range, or a launch that the
// part failed outright
#define CUBIN_SAFE_STATUS_ILLEGAL_ADDRESS 700
#define CUBIN_SAFE_STATUS_HARDWARE_STACK 714
#define CUBIN_SAFE_STATUS_ILLEGAL_INSTRUCTION 715
#define CUBIN_SAFE_STATUS_MISALIGNED_ADDRESS 716
#define CUBIN_SAFE_STATUS_INVALID_ADDRESS_SPACE 717
#define CUBIN_SAFE_STATUS_INVALID_PC 718
#define CUBIN_SAFE_STATUS_LAUNCH_FAILED 719

// How long the host reads the stop event of a launch on its own clock, in nanoseconds. It is twice the delay that
// Windows gives a launch before it resets the part (TdrDelay, 2 s where the system sets none). A reset at that delay
// then comes back as CUDA_ERROR_LAUNCH_TIMEOUT inside the bound
#define CUBIN_SAFE_LAUNCH_BOUND 4000000000ULL

// The verdict on one launch carried under rule 7. `status` is the driver's last word on its stop event, 0 where the
// part reached it. `waited` is the nanoseconds that the host read it for. CUBIN_SAFE_LAUNCH_TIMEOUT where the driver
// gave CUDA_ERROR_LAUNCH_TIMEOUT, or where the part did not reach the event and the bound passed. CUBIN_SAFE otherwise,
// because an error of another type is the part's refusal and no breach of this rule
unsigned int cubin_safe_launch(int status, unsigned long long waited);

// The verdict on one launch read under rule 8. `part_faulted` is 1 where the part stamped an Xid critical error during
// the launch, and `status` is the driver's own word on its stop event. `part_faulted` is also 1 where the display
// driver wrote an error to the system log while the launch ran. That is nvlddmkm at the levels critical and error, read
// by the listener (cubin_run/driver.c). The driver's word is the part's fault telemetry as much as the part's own
// stamp.
//
// CUBIN_SAFE_LAUNCH_FAULT where the part faulted, by the stamp or by a `status` that corrupts the context. The launch
// is then a panic (qry_member.h), and the rest give CUBIN_SAFE
unsigned int cubin_safe_fault(int part_faulted, int status);

// The vendor's safety verdict, the gate's, shared by every path that reaches the part: the airlock. The cross writes a
// gated file, a line `<low> <high>` in hex for each encoding that the vendor's reader called illegal. This reads it,
// with the scheduler's bits masked off each, because whatever reaches the part sets them. Every path that touches the
// part loads the verdict before it may launch, and blocks from the part any encoding that the verdict lists. The
// verdict only ever vetoes, and nothing that the vendor reported enters the learning: the protocol's own order of
// questions, valid for every language. 1, or 0 where the file did not read
int cubin_safe_verdict_load(const char *path);

// The same verdict, read from `size` bytes at `bytes`, the gated list as the cross passes it to the run (qry_member.h).
// 1, or 0 where it did not fit memory. Then nothing loads, and the gate blocks every code from the part
int cubin_safe_verdict_read(const char *bytes, unsigned long long size);

// The encodings of `size` bytes at `bytes`, a line `<low> <high>` each, added to the verdict already loaded and listed
// off the part with it. These are the encodings that faulted the part in a run before, which it never forgets
// (cubin_run.c, faulted.txt). 1, or 0 where it did not fit memory. Then nothing loads, and the gate blocks every code
// from the part
int cubin_safe_verdict_added(const char *bytes, unsigned long long size);

// 1 once a verdict is in, however many encodings it listed: the airlock that a live run passes through. A path that
// reaches the part with none carries code that the vendor's reader never read for a fault. An unread encoding can be
// the very one that faults the part and takes the machine with it. It therefore reaches no part
int cubin_safe_verdict_ready(void);

// 1 where the instruction `low` and `high`, its scheduler's bits masked off, is one the loaded verdict blocks from the
// part
int cubin_safe_verdict_gated(unsigned long long low, unsigned long long high);

// The encodings that a conference admitted past rule 3, read from `size` bytes at `bytes`, the lessons blob as the
// overseer passes it (overseer.h). Each line `lesson admitted * <low> <high>` is an encoding, with its scheduler's bits
// masked off, and the read passes over every other line. Whatever came in before goes. 1, or 0 where the list did not
// fit memory, and then nothing comes in
int cubin_safe_admitted_read(const char *bytes, unsigned long long size);

// 1 where the instruction `low` and `high`, its scheduler's bits masked off, is one a conference admitted
int cubin_safe_admitted(unsigned long long low, unsigned long long high);

// The verdict on `code`, `code_size` bytes of sixteen-byte instructions, against `machine`. The kernel allots
// `registers` registers a thread, or 0 where the count is unknown and the part's ceiling (register_last) bounds them
// instead (rule 6). CUBIN_SAFE, or the rule broken, with the offset of the instruction that broke it through `at`.
// CUBIN_SAFE_SIZE where the size is 0 or not a whole count of instructions. CUBIN_SAFE_EXIT where the code ends before
// an EXIT with no guard
unsigned int cubin_safe(const SassMachine *machine, const unsigned char *code, unsigned long long code_size,
                        unsigned int registers, unsigned long long *at);

// The verdict on every code section of the cubin `image`, `size` bytes, each checked against cubin_safe for a kernel of
// `registers` registers, one section after another. CUBIN_SAFE where every section is safe, or the first rule broken,
// with the offset in the image of the instruction that broke it through `at`. CUBIN_SAFE_SIZE where the image contains
// no code section that it can read
unsigned int cubin_safe_image(const SassMachine *machine, const unsigned char *image, unsigned long long size,
                              unsigned int registers, unsigned long long *at);

// the verdict's name, for a line that reports it
const char *cubin_safe_name(unsigned int verdict);

#endif
