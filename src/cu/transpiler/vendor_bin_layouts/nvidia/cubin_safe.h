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
//      ends: the kernel never returns.
//
// The code is read from its first instruction to its first EXIT with no guard, and nothing past it is read, since
// nothing reaches it: the self branch and the padding after a kernel's last exit are never run. Nothing here changes
// the code.
//
// Code that holds to these is then carried under one rule more, read off the part as each launch runs:
//
//   6. a launch is carried alone, between two events the part stamps from its own timer, and its stop event is read
//      without waiting on it until the part reaches it, the driver gives an error on it, or CUBIN_SAFE_LAUNCH_BOUND
//      passes on the host's clock. The time between the two events is the launch's block latency. A launch the driver
//      ends with CUDA_ERROR_LAUNCH_TIMEOUT, or one past the bound with its stop event unreached, breaks this rule: it
//      is answered as refused, so that R holds it off the part ever after, and nothing is launched after it in that
//      process. No launch waits queued behind another, and a launch that does not return is read as the driver's
//      timeout and not left for the system's watchdog.

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
    CUBIN_SAFE_LAUNCH_TIMEOUT = 9
};

// What the driver gives for a launch the system's watchdog ended (CUDA_ERROR_LAUNCH_TIMEOUT), and for an event the part
// has not reached yet (CUDA_ERROR_NOT_READY)
#define CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT 702
#define CUBIN_SAFE_STATUS_NOT_READY 600

// How long a launch's stop event is read for on the host's clock, in nanoseconds: twice the delay Windows gives a
// launch before it resets the part (TdrDelay, 2 s where the system sets none), so that a reset at that delay comes back
// as CUDA_ERROR_LAUNCH_TIMEOUT inside the bound
#define CUBIN_SAFE_LAUNCH_BOUND 4000000000ull

// The verdict on one launch carried under rule 6: `status` the driver's last word on its stop event, 0 where the part
// reached it, and `waited` the nanoseconds the host read it for. CUBIN_SAFE_LAUNCH_TIMEOUT where the driver gave
// CUDA_ERROR_LAUNCH_TIMEOUT, or where the event was not reached and the bound passed; CUBIN_SAFE otherwise, an error
// of another kind being the part's refusal and no breach of this rule
unsigned int cubin_safe_launch(int status, unsigned long long waited);

// The verdict on `code`, `code_size` bytes of sixteen-byte instructions, against `machine`: CUBIN_SAFE, or the rule
// broken, with the offset of the instruction that broke it through `at`. CUBIN_SAFE_SIZE where the size is 0 or not a
// whole count of instructions, CUBIN_SAFE_EXIT where the code ends before an EXIT with no guard
unsigned int cubin_safe(const SassMachine *machine, const unsigned char *code, unsigned long long code_size,
                        unsigned long long *at);

// The verdict on every code section of the cubin `image`, `size` bytes, held to cubin_safe one section after another:
// CUBIN_SAFE where every section is, or the first rule broken, with the offset in the image of the instruction that
// broke it through `at`. CUBIN_SAFE_SIZE where the image holds no code section it can read
unsigned int cubin_safe_image(const SassMachine *machine, const unsigned char *image, unsigned long long size,
                              unsigned long long *at);

// the verdict's name, for a line that reports it
const char *cubin_safe_name(unsigned int verdict);

#endif
