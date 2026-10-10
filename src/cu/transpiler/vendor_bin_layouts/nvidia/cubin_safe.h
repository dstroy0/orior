// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Whether a kernel's machine code may be put to the part, read on the host before the driver is handed it
#ifndef CUBIN_SAFE_H
#define CUBIN_SAFE_H

// A cubin the driver is handed runs as it is written. Machine code our own assembler wrote, or a probe turned one bit
// of, is read here first with our own reader against the machine file, and goes to the part only where every
// instruction it reaches holds to these, in order:
//
//   1. its scheduler's bits wait on all six barriers, and where no form holds the instruction, stall the longest. A
//      stall short of the soonest a result is read gives a wrong answer and never a kernel that does not return, and
//      the run channel asks below it to find where that soonest lies (sass_operation_schedule);
//   2. where a form of the machine file holds its encoding, that form neither transfers control nor waits
//      (sass_operation_control_or_wait), EXIT alone excepted;
//   3. where no form holds it, its operation key holds forms and no form under the key transfers control or waits.
//      Whatever its other bits say, it falls through to the next instruction. One instruction at most is held by no
//      form: the one a probe asks about;
//   4. an EXIT with no guard is reached: no thread runs off the end of the code;
//   5. an operation whose result is back in a fixed count of cycles sets no barrier, unless the encoding the system
//      wrote its form with sets one. Nothing releases one it sets, and the next instruction's wait on all six never
//      ends: the kernel never returns;
//   6. every operand that names a register names one the kernel allots: a register below the count the kernel holds, or
//      RZ, register 255, the zero register always there. A register at the count or above it is one the kernel never
//      allotted, and the part faults on it, an out-of-range register warp exception its whole machine over. Where no
//      count is read here the part's own ceiling bounds it, the last register it answers a code may name (register_last,
//      the .ksc beside the machine file). Which bits of an instruction name a register is the form's, found by the
//      probe, and the whole fixed eight-bit field each register sits in is read, never a partial run of it. A form
//      whose fields are not yet asked names no register here: the cross holds that code off the part instead, on the
//      vendor's own reading of every register it names (measuring_stick_query.py), and the verdict reaches this gate.
//
// The code is read from its first instruction to its first EXIT with no guard, and nothing past it is read, since
// nothing reaches it: the self branch and the padding after a kernel's last exit are never run. Nothing here changes
// the code.
//
// Code that holds to these is then carried under two rules more, read off the part as each launch runs:
//
//   7. a launch is carried alone, between two events the part stamps from its own timer, and its stop event is read
//      without waiting on it until the part reaches it, the driver gives an error on it, or CUBIN_SAFE_LAUNCH_BOUND
//      passes on the host's clock. The time between the two events is the launch's block latency. A launch the driver
//      ends with CUDA_ERROR_LAUNCH_TIMEOUT, or one past the bound with its stop event unreached, breaks this rule: it
//      is answered as refused, so that R holds it off the part ever after, and nothing is launched after it in that
//      process. No launch waits queued behind another, and a launch that does not return is read as the driver's
//      timeout and not left for the system's watchdog;
//   8. the part's own fault telemetry is read after each launch: the Xid critical errors the driver's management layer
//      (NVML) stamps against the device. A launch that makes the part stamp one has faulted it, its result wrong and
//      its context corrupt, every launch after it in the process invalid; it breaks this rule. A fault the driver names
//      itself on the launch's stop event, an illegal address or instruction, a hardware stack error, a misaligned
//      address, an address space it may not reach, a program counter out of range, or a launch it failed outright, is
//      read the same way, since the stamp may reach the host a moment behind the driver's own word. The launch is
//      answered as refused, so that R holds it off the part ever after, and nothing is launched after it in that
//      process. An out-of-range register the part faults on in microseconds is caught here, where rule 6's bound, read
//      for a launch that runs too long, never sees it.

#include "sass_machine.h"

// what the code was found to be: safe, or the first rule an instruction broke
enum CubinSafe
{
    CUBIN_SAFE = 0,
    CUBIN_SAFE_SIZE = 1,
    CUBIN_SAFE_STALL = 2,
    CUBIN_SAFE_WAIT = 3,
    CUBIN_SAFE_CONTROL = 4,
    CUBIN_SAFE_KEY = 5,
    CUBIN_SAFE_UNHELD = 6,
    CUBIN_SAFE_EXIT = 7,
    CUBIN_SAFE_BARRIER = 8,
    CUBIN_SAFE_LAUNCH_TIMEOUT = 9,
    CUBIN_SAFE_LAUNCH_FAULT = 10,
    CUBIN_SAFE_REGISTER = 11,
    CUBIN_SAFE_HELD = 12
};

// What the driver gives for a launch the system's watchdog ended (CUDA_ERROR_LAUNCH_TIMEOUT), and for an event the part
// has not reached yet (CUDA_ERROR_NOT_READY)
#define CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT 702
#define CUBIN_SAFE_STATUS_NOT_READY 600

// What the driver gives for a launch that faulted the part and left its context corrupt, every launch after it in the
// process invalid: an illegal address, a hardware stack error, an illegal instruction, a misaligned address, an
// address space the part may not reach, a program counter out of range, or a launch the part failed outright
#define CUBIN_SAFE_STATUS_ILLEGAL_ADDRESS 700
#define CUBIN_SAFE_STATUS_HARDWARE_STACK 714
#define CUBIN_SAFE_STATUS_ILLEGAL_INSTRUCTION 715
#define CUBIN_SAFE_STATUS_MISALIGNED_ADDRESS 716
#define CUBIN_SAFE_STATUS_INVALID_ADDRESS_SPACE 717
#define CUBIN_SAFE_STATUS_INVALID_PC 718
#define CUBIN_SAFE_STATUS_LAUNCH_FAILED 719

// How long a launch's stop event is read for on the host's clock, in nanoseconds: twice the delay Windows gives a
// launch before it resets the part (TdrDelay, 2 s where the system sets none), so that a reset at that delay comes back
// as CUDA_ERROR_LAUNCH_TIMEOUT inside the bound
#define CUBIN_SAFE_LAUNCH_BOUND 4000000000ull

// The verdict on one launch carried under rule 7: `status` the driver's last word on its stop event, 0 where the part
// reached it, and `waited` the nanoseconds the host read it for. CUBIN_SAFE_LAUNCH_TIMEOUT where the driver gave
// CUDA_ERROR_LAUNCH_TIMEOUT, or where the event was not reached and the bound passed; CUBIN_SAFE otherwise, an error
// of another kind being the part's refusal and no breach of this rule
unsigned int cubin_safe_launch(int status, unsigned long long waited);

// The verdict on one launch read under rule 8: `part_faulted` 1 where the part stamped an Xid critical error during the
// launch, `status` the driver's own word on its stop event. CUBIN_SAFE_LAUNCH_FAULT where the part faulted, by the
// stamp or by a `status` that corrupts the context; CUBIN_SAFE otherwise
unsigned int cubin_safe_fault(int part_faulted, int status);

// The vendor's safety verdict, the gate's and shared by every path that reaches the part: the airlock. The cross
// writes a held file, a line `<low> <high>` in hex, the exact encodings the vendor's reader called illegal; this reads
// it, the scheduler's bits masked off each since whatever reaches the part sets them. Every part-touching path loads
// the verdict before it may launch, and holds off the part any encoding the verdict holds. The verdict only ever
// vetoes; nothing the vendor said enters the learning, the protocol's own order of questions, which holds for every
// language. 1, or 0 where the file did not read
int cubin_safe_verdict_load(const char *path);

// 1 once a verdict is loaded, however many encodings it held: the airlock a live run passes through. A path that
// reaches the part with none carries code the vendor's reader never read for a fault, and an unread encoding can be the
// very one that faults the part and takes the machine with it: it reaches no part
int cubin_safe_verdict_ready(void);

// 1 where the instruction `low` and `high`, its scheduler's bits masked off, is one the loaded verdict holds off the
// part
int cubin_safe_verdict_holds(unsigned long long low, unsigned long long high);

// The verdict on `code`, `code_size` bytes of sixteen-byte instructions, against `machine`, for a kernel that allots
// `registers` registers a thread, 0 where none is known and the part's ceiling (register_last) bounds them instead
// (rule 6): CUBIN_SAFE, or the rule broken, with the offset of the instruction that broke it through `at`.
// CUBIN_SAFE_SIZE where the size is 0 or not a whole count of instructions, CUBIN_SAFE_EXIT where the code ends before
// an EXIT with no guard
unsigned int cubin_safe(const SassMachine *machine, const unsigned char *code, unsigned long long code_size,
                        unsigned int registers, unsigned long long *at);

// The verdict on every code section of the cubin `image`, `size` bytes, each held to cubin_safe for a kernel of
// `registers` registers one section after another: CUBIN_SAFE where every section is, or the first rule broken, with
// the offset in the image of the instruction that broke it through `at`. CUBIN_SAFE_SIZE where the image holds no code
// section it can read
unsigned int cubin_safe_image(const SassMachine *machine, const unsigned char *image, unsigned long long size,
                              unsigned int registers, unsigned long long *at);

// the verdict's name, for a line that reports it
const char *cubin_safe_name(unsigned int verdict);

#endif
