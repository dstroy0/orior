// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the interface_sass_probe*.c pieces share
#ifndef INTERFACE_SASS_PROBE_H
#define INTERFACE_SASS_PROBE_H

// The SASS probe (engine_table.md item 11(f)(a)). It finds the machine code of the device through queries to its
// disassembler. A ruleset of SASS can then come from what the probes saw.
//
// `interface_ptx_probe` assembles the kernel of each membership question, the forms of ptx.krs, into a cubin. It
// assembles the kernel with no body into another cubin. nvdisasm lists each. The operations that the listing of a
// question contains past those of the empty kernel are the machine code of its forms.
//
// The probe then decodes each operation that it saw, keyed by the low 12 bits of its encoding, with each of its 128
// bits turned over. One nvdisasm --binary call decodes all 129 encodings:
//   - a bit that changes a register operand is the field of that operand.
//   - a bit that changes the name of the operation is the field of the operation.
//   - a bit that the disassembler refuses gives an encoding that the part lacks.
//   - a bit that changes nothing in the text is control (the stall, yield and barriers that the scheduler sets), or it
//     has no use.
//
// Every process runs through the interface, whose runner loses nothing when the disassembler fails
#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../lstar/interface/interface.h"
#include "../sass_machine.h"

#include <stdio.h>
#include "../../../../../../includes/parsers/who.h"

// the longest instruction text stored, and the max instructions a listing contains
#define SASS_TEXT 192u
#define SASS_LISTING_LIMIT 4096u
// the max operands an instruction's text contains, and the max length of each
#define SASS_OPERANDS 8u
#define SASS_OPERAND_TEXT 64u
// the max questions and the max operations the probe stores, and the longest question name
#define SASS_QUESTIONS 256u
#define SASS_OPERATIONS 512u
#define SASS_NAME 160u
// an operation's encoding and the 128 encodings one bit apart from it
#define SASS_BITS 128u
#define SASS_ENCODINGS (SASS_BITS + 1u)
// how many times widening walks out a bit, each round over what the round before found. Two, because the field that the
// lane lacks is the type of an operand. A register and a number in one field are two bits apart, through the constant
// between them (sass_machine_widen)
#define SASS_WIDEN_ROUNDS 2u
// every value of SASS_OPERATION_MASK (sass_machine.h) is a question that the sweep puts to the disassembler, and no
// compiler's output bounds the search

// one instruction as the disassembler printed it: its address, its text without the ending ';', and its encoding. The
// low word holds the operation and its operands. The high word holds more operands and the control. Both are 0 where
// the listing gave no encoding
typedef struct
{
    unsigned long long address;
    char text[SASS_TEXT];
    unsigned long long low;
    unsigned long long high;
} SassInstruction;

typedef struct
{
    unsigned int count;
    SassInstruction instructions[SASS_LISTING_LIMIT];
} SassListing;

// an instruction's text in its parts: the guard predicate ("@P0", "" where it has none), the operation and each
// operand
typedef struct
{
    char predicate[SASS_OPERAND_TEXT];
    char operation[SASS_OPERAND_TEXT];
    unsigned int operands;
    char operand[SASS_OPERANDS][SASS_OPERAND_TEXT];
} SassParts;

// The channels that this system answers a question on. The .ksc holds the classification of the system as a language
// map: which return answers, which return nothing, and which are illegal on their own. A probe is not a thing of its
// own. It is a question put on one of these channels, and the answer read back.
//
// Which channels a system has is for the system to declare, and the probe never assumes them. A part with no compiler
// and no disassembler still answers on `run`, with fewer names and more slowly. The probe reaches everything above the
// same way, through the one channel that the part does have
#define SASS_CHANNELS(channel_)                                                                                        \
    channel_(RUN, "run", "the system takes the question as its own code, runs it, and a word comes back")              \
    channel_(DECODE, "decode", "the system's disassembler is passed an encoding and names it")                         \
    channel_(COMPILE, "compile", "the system's compiler is passed a source and emits instructions for it")             \
    channel_(CLOCK, "clock", "two codings of one thing are run against each other and timed")

// What came back is one of three. The system takes a question and answers it, takes it and answers nothing that a
// reader can see, or refuses it. The third prunes. An encoding that the decoder names but that no run takes is illegal
// on its own, however well it decoded
#define SASS_CLASSES(class_)                                                                                           \
    class_(ANSWERS, "answers", "the question was put and the answer came back as the question specifies")              \
        class_(NOTHING, "nothing", "the question was taken and left nothing a reader here can see")                    \
            class_(ILLEGAL, "illegal", "the question was refused: the system will not take it as it is")           \
                class_(FOLDS, "folds",                                                                                 \
                       "the compiler answered but folded the operand the question varied away, so the answer names a " \
                       "form of its "                                                                                  \
                       "own and not the operator")

#define SASS_CHANNEL_NAMED(name_, text_, why_) SASS_CHANNEL_##name_,
#define SASS_CLASS_NAMED(name_, text_, why_) SASS_CLASS_##name_,

enum SassChannel
{
    SASS_CHANNELS(SASS_CHANNEL_NAMED) SASS_CHANNEL_COUNT
};

enum SassClass
{
    SASS_CLASSES(SASS_CLASS_NAMED) SASS_CLASS_COUNT
};

// one question put on one channel, and what came back. The word is what the system answered where the channel gives
// one, and 0 otherwise
typedef struct
{
    unsigned char channel;
    unsigned char answered;
    unsigned int word;
    char question[SASS_TEXT];
} SassClassed;

// one question and its answer stored for the .ksc (interface_sass_probe_class.c)
void sass_class_take(unsigned int channel, unsigned int answered, const char *question, unsigned int word);

// one more question counted on a channel, with its text not stored. The decode channel puts tens of thousands of these,
// and only the count of each class is worth a write
void sass_class_count(unsigned int channel, unsigned int answered);

// two writings that the system answers alike, recorded in the .ksc. The compiler wrote one operation where the ruleset
// contains the other, and each gives the same machine code. A reading in one is the form of the ruleset in the other
void sass_equal_take(const char *one, const char *other);

// 1 where the .ksc records `one` and `other` as equal, in either order
int sass_equal_known(const char *one, const char *other);

// an operation that the system writes on the FMA pipe in place of the integer pipe, recorded in the .kdm of the part.
// It holds the operation, its writing there, and the lane it moves in. That lane is one whose integer pipe carries
// `least` operations or more, and `over` more than its FMA pipe. A new pipe for `operation` replaces any that the
// record already holds
void sass_pipe_take(const char *operation, const char *writing, unsigned int least, unsigned int over);

// 1 where the record holds a pipe for `operation`, with its writing in `writing`, of SASS_TEXT, and its lane in `least`
// and `over`
int sass_pipe_found(const char *operation, char *writing, unsigned int *least, unsigned int *over);

// the pipes of `machines`/<part>.kdm taken beside those recorded, as the run stores it (kdm.h): 1, or 0 where there is
// no such file
int sass_pipe_read(const char *machines, const char *part);

// `machines`/<part>.kdm passed back to the run's writer, with the recorded pipes in place of those it contained. They
// go at the end of its tail, under a who line of `who` (kdm.h). 1, or 0 with the reason printed
int sass_pipe_write_by(const char *machines, const char *part, Who who);

// sass_pipe_write_by, the who the code that calls it
#define sass_pipe_write(machines_, part_) sass_pipe_write_by((machines_), (part_), WHO_HERE)

// `machines`/<part>.ksc read back into the classification in place of the questions and counts it contained, as the run
// stores it (ksc.h). A later pass then adds to it and does not overwrite it. Its folds go among them, and its equal
// writings go beside those already taken. 1, or 0 where there is no such file (a first run has none)
int sass_class_read(const char *machines, const char *part);

// every recorded question on `channel` of class `answered` that opens with `opening`, taken out, and its count. A pass
// that queries a question again drops what an earlier pass found of it, before it takes what it finds itself
void sass_class_drop(unsigned int channel, unsigned int answered, const char *opening);

// the system's classification passed to the run's writer as `machines`/<part>.ksc. Every line after its first goes
// under a who line of `who` (ksc.h). 1, or 0 with the reason printed
int sass_class_write_by(const char *machines, const char *part, Who who);

// sass_class_write_by, the who the code that calls it
#define sass_class_write(machines_, part_) sass_class_write_by((machines_), (part_), WHO_HERE)

// one operation the listings contain, keyed by the low 12 bits of its encoding: the first instruction seen with it
typedef struct
{
    unsigned int key;
    SassInstruction first;
} SassOperation;

// what one run of the probe found so far: where its cubins are, which program assembles them, and the part. It also
// holds the questions and the operations read out of them
typedef struct
{
    const char *folder;
    const char *prober;
    // the machine that the host checks each cubin of our assembler against, before the cubin runs (cubin_safe.h)
    const SassMachine *machine;
    char architecture[16];
    unsigned int questions;
    char names[SASS_QUESTIONS][SASS_NAME];
    unsigned int operations;
    SassOperation operation[SASS_OPERATIONS];
    unsigned int failed;
} SassProbe;

// one process run through the interface, with its output written to `output_path` and read into the shared buffer
// (sass_output). Its exit status, or -1 where it did not exit, with how it ended printed
int sass_run(char *const *command, const char *output_path);

// the cubin at `path`, which our assembler wrote, checked against cubin_safe for the machine of the probe. Where it
// passes, it runs on the device through interface_ptx_probe over one case, with its answer into `answered`. 1, or 0
// with the reason printed, where cubin_safe blocked it from the part or it did not run (interface_sass_probe_query/)
int sass_cubin_answer(SassProbe *probe, const char *path, char *answered, size_t room);

// the cubin at `path` run as sass_cubin_answer runs one, where NVIDIA's toolchain made it. Our assembler wrote none of
// it, and the function does not check it against cubin_safe
int sass_cubin_answer_toolchain(SassProbe *probe, const char *path, char *answered, size_t room);

// 1 where the cubin at `path` reads and satisfies cubin_safe against the machine of the probe. 0 otherwise, with the
// rule it broke printed. Nothing that our assembler wrote goes to the part unread
int sass_cubin_safe(const SassProbe *probe, const char *path);

// every question of the interface's own put to the part in code that no toolchain wrote, with `queried` as their count.
// The return is how many the part answered as the question specifies
unsigned int sass_cubin_queries(SassProbe *probe, const SassMachine *machine, unsigned int *queried);

// every pair of codings weighed against each other in the part's own clock, with `queried` as the count of pairs. The
// return is how many gave a reading above the noise
unsigned int sass_cubin_prefers(SassProbe *probe, const SassMachine *machine, unsigned int *queried);

// How the part loops, queried of the part (engine_plan.md, `A loop is learned by querying`). The function puts every
// form that `machine` contains in the place of loop_back_if, with its operands filled by their types. That form goes at
// the end of a body. The body counts the first word of the case down with word_add, and sets the flag with
// test_word_nonzero.
//
// A form that comes back to the label on the flag answers that word. One that falls through answers 1. The function
// queries 2 first, and then queries the forms that answer it at every other count. It times and walks those that answer
// every count (sass_loop_walk), and prints the cheapest as the part's loop_back_if. `queried` counts the forms put. The
// return is how many came back on every count
unsigned int sass_cubin_loops(SassProbe *probe, const SassMachine *machine, unsigned int *queried);

// the output of the last process sass_run ran
const char *sass_output(void);

// a listing read from a disassembler's output. Each line that holds an address, "/*0000*/", is an instruction. A line
// that holds only "/* 0x... */" after one is the high word of its encoding. 0 where the listing is longer than it
// allows
int sass_listing_read(const char *output, SassListing *listing);

void sass_parts_read(const char *text, SassParts *parts);

// the encodings that the disassembler decodes for `architecture` ("SM86"), `count` of them, into `texts`. An encoding
// that the disassembler refuses has the text "illegal". One that it takes and prints no line for gets the text
// "unprinted". The function writes the encodings to `path`.bin and decodes them. Where the disassembler refuses any,
// the function writes and decodes the rest again. 1, or 0 where the disassembler failed otherwise
int sass_decode(const char *architecture, const char *path, const unsigned long long *low,
                const unsigned long long *high, unsigned int count, char (*texts)[SASS_TEXT]);

// every instruction of `listing` that the listing gave an encoding for taken into `machine` as a form
// (interface_sass_probe_machine.c)
void sass_machine_listing(SassMachine *machine, const SassListing *listing);

// Every form reachable from the listings a bit at a time, taken into `machine` as a form of its own. The function turns
// over the 128 bits of a form and decodes each. A decode that the disassembler names whole is an instruction that the
// compiler never wrote, and that the part still answers for. This is how a name reaches the machine with no guess.
// ISETP.EQ.U32.AND, ISETP.EQ.U32.AND.EX and ISETP.LT.AND are each one bit from a comparison that the listings did
// contain. `sass.krs` names all three only because the compiler read zero and below off the negations instead.
//
// The function widens each form found this way in turn, for SASS_WIDEN_ROUNDS rounds. The reach is where one bit at a
// time gets to through forms, in place of where one bit gets to. The type of an operand is a field of more than one
// bit. A register in a field and a number in that field are two bits apart, through the constant in that field that
// lies between them. One round reaches the constant, and the next reaches the number.
//
// The name of the operation does not decide this, because a form is its operation and the types of its operands
// together. Both SHF.L.U32 forms carry that name, and the assembler puts a number in a different place for each.
//
// The rounds have a bound, and the walk does not run to its own end. A measurement of the walk shows that it does not
// close. See sass_machine_widen for the reading, and for why the whole component is worse than this much of it.
//
// Two claims do not hold for a widened form. It is decodable, but no run proves it. Only a question that assembles one
// and runs it shows that the part executes it. Its operand bits are the ones that its source form carried, and the new
// operation can read them as something else. PLOP3.LUT is one bit from SHF.L.U32, and it decodes with a register where
// a predicate belongs. Its form carries that type, and no predicate operation can come from it.
//
// An operation that the walk reaches this way is the part's report that the encoding is legal. It is not a form ready
// for assembly. The return is the count taken
unsigned int sass_machine_widen(SassMachine *machine, const char *architecture, const char *folder);

// Every operation that the part has a coding for, queried of its disassembler with no start from what a compiler wrote.
//
// Widening queries what lies one bit from a form that some compiler emitted. That bounds the search by what a compiler
// happens to write. The structure of the encoding itself lifts that bound. The low 12 bits of the low word key the
// operation and the types of its operands, and the probe already found that much. 4096 questions reach every operation
// that the disassembler names.
//
// The carrier for all of them is a form that the part ran. The function cuts its key out and puts each key back in
// turn. Every bit that is not the key then carries what a real instruction carries.
//
// The function reads what comes back exactly the way widening reads it. It keeps every operation that the disassembler
// names whole, where each operand is a type that the assembler can place. It finds a form already stored by its
// operation and types, and takes it no second time. The operand bits belong to the carrier and not to the operation
// that they now sit under, the same caveat that widening carries. `sass_machine_fields` later finds the fields of each
// kept form from its own bits. The return is the count of forms that this added
unsigned int sass_machine_sweep(SassMachine *machine, const char *architecture, const char *folder);

// the operand fields of each form, found when the function turns over its 128 bits and decodes them, and the machine
// written to `folder`/machine. 1, or 0 where the function did not find the fields of a form or did not write the file
int sass_machine_fields(SassMachine *machine, const char *architecture, const char *folder);

// the machine file that the tree contains for this part, in `machines`, checked against the one that this run learned.
// 1 where the two agree, or where the tree contains none for this part. 0 where they differ, with what to do printed
int sass_machine_same(const SassMachine *machine, const char *machines);

// how a check of the assembler came out:
//   - how many instructions it wrote back.
//   - how many of them the assembler refused.
//   - how many came out as the very bytes that the listing gave.
//   - how many read back as the text that they came from.
//
// The bytes are the stronger reading, and the text is the true one. The probes found bits that an instruction carries
// and no listing prints (LDG's 32 to 39). Two instructions that print the same can differ
typedef struct
{
    unsigned int checked;
    unsigned int refused;
    unsigned int same_bits;
    unsigned int same_text;
    unsigned int by_bytes;
} SassCheck;

// every instruction of `listing` assembled from its text alone, and checked against the encoding that the listing gave
// it. The function then disassembles each one and checks it against the text that it came from, with counts into
// `tally`. The return is how many did not read back, with the first `report` of them printed
unsigned int sass_machine_check(const SassMachine *machine, const SassListing *listing, const char *architecture,
                                const char *folder, SassCheck *tally, unsigned int report);

// a listing's own output turned back into the text that it came from, one instruction or label a line, into `text`.
// That buffer has room for `room` characters. The return is how many it wrote
unsigned int sass_text_read(const char *output, const char *kernel, char *text, unsigned int room);

// the listing of `name` in `folder` turned back into the text that it came from, into `text`, and stored beside it as
// <name>.text. The return is how many characters, 0 where the function did not read the listing
unsigned int sass_cubin_text(const char *folder, const char *name, char *text, unsigned int room);

// `text` assembled into the section `kernel` of a cubin made from `pattern`.cubin, written as `into`.cubin in the
// folder. 1, or 0 with the reason printed. Every other section of the pattern carries over untouched. One function of
// the cubin changes, and the others stay as the part's own compiler wrote them
int sass_cubin_kernel(const SassMachine *machine, const char *folder, const char *pattern, const char *text,
                      const char *into, const char *kernel);

// the same, into the probe's own question kernel
int sass_cubin_from_text(const SassMachine *machine, const char *folder, const char *pattern, const char *text,
                         const char *into);

// A lane of our own written into the resident's cubin, in place of the empty cycle_lane that the resident has from its
// build. This is how a SASS program comes together. It is also why sass.krs gives program_unit as an error, and why
// that error is not a gap. The resident is 110 instructions of PTX that the part's compiler turns into SASS that
// already runs. A new derivation of it from a ruleset would throw that away. The lane is the part we write, and the
// resident is the part we request and keep
int sass_cubin_lane_into(const SassMachine *machine, const char *folder, const char *text, const char *into);

// the kernel `name` in `folder` written again out of its own listing, as <name>_written.cubin
int sass_cubin_round(const SassMachine *machine, const char *folder, const char *name);

#endif
