// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef TARGET_H
#define TARGET_H

// The code generator: the lane of a record program, written as text for its target, in the ruleset of that target
// (lstar/protocol/table). It decides what each step does, and the ruleset writes it. It reads no device and loads no
// library. The record machine (cycle/cycle_{sweep,launch}.cu) picks the language. It compiles, links, caches and
// launches what the code generator writes. This header is the base that every language inherits, and each language is
// the file of its ruleset (code_generator.h)

#include "../../../../includes/platform/platform.h"
#include "../../engine/engine_config.h"

// the scratch of the division operations for each lane, in limbs:
//
// - long division stores the normalized numerator (one limb over) and divisor.
// - the gcd stores its three remainders ahead of that.
// - the exact quotient stores the shifted numerator and divisor, the inverse with its step product, and the next
//   inverse.
#define CODEGEN_RECORD_SCRATCH(wide_) ((5u * (wide_)) + 4u)

// what the thread blocks of a compiled program share on the device across one run: the next lane to take. Within one
// launch, its start, the thread blocks gone and the check-ins made, arranged as the own of the kernel (s_cycle_prelude)
struct CycleHot
{
    unsigned long long next_lane;
    unsigned long long launch_start;
    unsigned long long finished;
    unsigned long long checkins;
};

// the argument of the compiled program, arranged as the own of the kernel (g_cycle_prelude) declares it. The block is
// the EngineProgramBlock of the program as 64-bit words, at its device address. It places the registers of a thread in
// shared memory
struct CycleCompiledLaunch
{
    const unsigned int *in[ENGINE_RECORD_MEMBERS_MAX];
    const unsigned int *index;
    const unsigned int *tables;
    unsigned int *out;
    unsigned int *error;
    unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX];
    unsigned long long count;
    CycleHot *hot;
    unsigned long long *block;
    unsigned long long ttl;
    unsigned long long checkin_every;
    unsigned long long launch_number;
    unsigned long long places;
};

// what a lane is for, which its first line names. It is the target, by the hash of its text (the device, the type of
// link and the prelude). Then the compute capability of the device, and the version of NVRTC. Then the prelude that a
// lane opens with, where the header of its ruleset is the prelude
struct TargetInfo
{
    unsigned long long block_hash;
    int major;
    int minor;
    int nvrtc_major;
    int nvrtc_minor;
    const char *prelude;
};

// the place among a lane's forms of a text a code generator takes whole and writes as no form, a header
// (codegen_core.h, MachineInstr)
#define ASM_PRINTER_ALL_ONES 0xFFFFFFFFu

// a ruleset read against its code generator's schema, retained once a process
struct Ruleset;

// what a language's code generator requires of its ruleset (ruleset_reader.h)
struct RulesetSchema;

// The base of the code generator: what every language shares, and no language. It reads the ruleset of a language from
// its .krs file, against the schema that the language gives it. The schema is the file, and the toolchain and header
// that its path builds with. It is also the forms, banks and fixed registers that its code generator writes. The base
// writes forms in it. A language is the code generator of the file of its ruleset (code_generator.h), which writes the
// lane of a program in that ruleset
class Target
{
  public:
    Target(const Target &) = delete;
    Target &operator=(const Target &) = delete;
    virtual ~Target();

    // the language's ruleset, read once a process; NULL where it errors, `report` reporting which on stderr
    const Ruleset *ruleset(int report);

    // the schema the language reads its ruleset against, read or not
    const RulesetSchema *schema(void) const;

    // the folds of the classification beside the ruleset read again, for a pass that has just written it
    void folds_read(void);

    // The lane of a program in the language under `header`, onto `text`, which the caller began. Also the places that a
    // thread uses in shared memory, and the max words that it uses live at once. Those are 0 where the language does
    // not count them. The return is 1 where it wrote the lane. It is 0, with `text` empty, where the read of the
    // ruleset failed, or the language does not support the program
    virtual int program(const EngineRecordLayout *layout, const TargetInfo *target, const char *header,
                        PlatformText *text, unsigned int *places, unsigned int *live) = 0;

  protected:
    explicit Target(const RulesetSchema *schema);

    // the ruleset once ruleset() read it, else NULL
    const Ruleset *ready(void) const;

  private:
    Ruleset *rules;
};

// A ruleset read from outside the code generator, by the names that its .krs file gives. It is for a probe that
// queries the target how it answers each form (the membership queries of the cell, engine_table.md item 11(f) 4). The
// code generator itself writes by the places in its schema

// A scratch register by the name of its bank, which `taken` writes onto `name`. `taken` answers 0, and writes nothing,
// where the writer has none of that bank to give
struct RulesetNamedScratch
{
    int (*taken)(void *context, const char *bank, VerbaLinea *name);
    void *context;
};

// Form `name` written onto `text`, with its `count` arguments in the order of its parameters. The return is 0, with
// nothing written, where the ruleset writes no form of that name, or the form takes another count of arguments.
//
// The ruleset can give the form as a construct: a line `construct <name> <parameter>...`, with its lines to `end`.
// Then each scratch register `{bank:n}` that it names comes from `scratch` by the name of the bank, once for each
// write. The answer is 0 where `scratch` gives none. A `scratch` of NULL gives none
int ruleset_form_written(const Ruleset *rules, const char *name, const char *const *arguments, unsigned int count,
                         const RulesetNamedScratch *scratch, VerbaLinea *text);

// register `number` of the bank `bank` written onto `text`. 0, and nothing written, where the ruleset has no bank of
// that name
int ruleset_register_written(const Ruleset *rules, const char *bank, unsigned int number, VerbaLinea *text);

// the register every lane uses throughout named `name`, terminated. NULL where the ruleset contains none of that name
// [BORROWS]
const char *ruleset_fixed_named(const Ruleset *rules, const char *name);

#endif
