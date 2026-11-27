// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the interface_ptx_probe_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef INTERFACE_PTX_PROBE_INTERNAL_H
#define INTERFACE_PTX_PROBE_INTERNAL_H

// A probe for the interface's PTX test (engine_table.md item 11(f) 4): questions queried of the device in the ruleset's
// own words. Each question's kernel is written from ptx.krs, one form by its name at a time (target.h), around a kernel
// of this probe's own that loads a case's eight input words and stores four output words, and nvJitLink assembles it as
// the engine's PTX path does. The header is requested of NVRTC. One question a process, named by the first word:
//   membership   every arithmetic, test and conversion form the code generator writes, over 65,536 cases of input
//   words, each
//                answer checked against the host's integers; a form's answer where PTX leaves it undefined (a divisor
//                of 0) is printed and not checked. Exit 0 where every defined answer agrees, 1 where one does not
//   address      a load from address 16, which no allocation covers
//   misaligned   a 32-bit load from an address one byte past an allocation's start
//   trap         PTX's trap instruction
//   lacking      elect.sync, which PTX gives sm_90 and later, in a kernel for this device
//   alive        one form over one case, to show a fresh process's device answers
//   cubins <folder>  the membership questions' kernels and the kernel alone, assembled and written as cubins for the
//                SASS probe (interface_sass_probe.c), nothing run
// A question the device errors prints "error <code> <name>" for the CUDA error it gave, then the error the next
// allocation gives, "after <code> <name>", and exits 3. One the toolchain errors prints "errored" and its log, and
// exits 4. Exit 2 where the probe could not query at all
#include "../../../../../../includes/platform/platform.h"
#include "../../../../engine/rmc/machine_ir_builder.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "ptx_target.h"

#include <cuda_runtime.h>
#include <nvJitLink.h>
#include <nvrtc.h>

#include <stddef.h>

#define PROBE_CASES 65536u
#define PROBE_IN_WORDS 8u
#define PROBE_OUT_WORDS 4u
#define PROBE_THREADS 256u
// the operands every case draws the majority of its words from, the edges of 32-bit arithmetic and of a shift
#define PROBE_EDGES 14u

// the bytes of a register's name, its terminator among them; of PTX's header; and of a kernel's text at max
#define PROBE_NAME_BYTES 64u
#define PROBE_HEADER_BYTES 512u
#define PROBE_KERNEL_BYTES (1ULL << 24u)

// the questions at max, and the bytes of all their bodies, each terminated
#define PROBE_QUESTIONS_MAX 32u
#define PROBE_BODIES_BYTES (1u << 20u)

// the answer the host's integers give a case: the output words, and 0 where the rule leaves the answer undefined
typedef int (*ProbeRule)(const unsigned int *in, unsigned int *out);

// one question: its name, the forms that pose it as the kernel's body, the output words it writes and the host's rule.
// The body lies in the probe's bodies (interface_ptx_probe_questions.cu), terminated [BORROWS]
struct ProbeQuestion
{
    const char *name;
    const char *body;
    unsigned int outputs;
    ProbeRule rule;
};

// a register's name as the ruleset writes it, terminated, and empty where the ruleset names none
struct ProbeName
{
    char text[PROBE_NAME_BYTES];
};

// the registers of each bank a kernel declares: the questions' own below, and above them the scratch a construct
// takes, numbered on through the whole process so that no two scratch registers share a number
#define PROBE_TEMPORARIES 12u
#define PROBE_WIDES 4u
#define PROBE_PREDICATES 4u
#define PROBE_DECLARED 1024u

// the ruleset's written forms and a writer for its forms, which marks the text broken where a form is not written. The
// next scratch register of each bank a construct may take
struct ProbeWriter
{
    const Ruleset *rules;
    int broken;
    unsigned int scratch_temporary;
    unsigned int scratch_wide;
    unsigned int scratch_predicate;
};

ProbeName probe_temporary(ProbeWriter *writer, unsigned int number);

ProbeName probe_wide(ProbeWriter *writer, unsigned int number);

// form `name` written onto `text` with its `count` arguments, the text marked broken where the ruleset does not write
// it
void probe_form(ProbeWriter *writer, VerbaLinea *text, const char *name, const char *const *arguments,
                unsigned int count);

// form `name` written onto `text` with the arguments after it, each a terminated text
#define PROBE_FORM(writer_, text_, name_, ...)                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        const char *const arguments_[] = {__VA_ARGS__};                                                                \
        probe_form(writer_, text_, name_, arguments_, (unsigned int)(sizeof(arguments_) / sizeof(arguments_[0])));     \
    } while (0)

void probe_case(unsigned int number, unsigned int *in);

// the questions, `*count` of them, their bodies written once a process [BORROWS]
const ProbeQuestion *probe_questions(ProbeWriter *writer, unsigned int *count);

// the probe's runs (interface_ptx_probe_runs.cu): PTX's header into `header`, the cubins, and the membership, single,
// clocks and cubin probes main runs
size_t probe_header(int major, int minor, char *header, size_t room);
int probe_cubins(ProbeWriter *writer, const char *header, int major, int minor, const char *folder);
int probe_membership(ProbeWriter *writer, const char *header, int major, int minor);
int probe_single(ProbeWriter *writer, const char *header, const char *body, int major, int minor);
int probe_clocks(int device);
int probe_cubin_run(const char *path, int count, char **arguments);

#endif
