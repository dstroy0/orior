// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// widened.c: the machine widened: a form's fields, an operation named, what a widening contains, the machine widened,
// its fields, the sweep's carriers, the machine swept, and two machines the same
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "probe_machine_internal.h"

// the fields of one form. The call turns over each of its 128 bits and decodes it. It keeps each run of bits that
// changes one printed operand as the run of that operand. Then it keeps the run of an operand that the form carries and
// does not print. The return is 1, or 0 where the disassembler failed
static int sass_form_fields(SassForm *form, const char *architecture, const char *folder, unsigned int number)
{
    sass_form_turned(form);
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/form_");
        mmgr_line_decimal(&buffer_line, number, 3u);
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!sass_decode(architecture, path, s_low, s_high, SASS_ENCODINGS, s_texts))
    {
        return 0;
    }
    SassInstructionParts base;
    sass_instruction_read(s_texts[0], &base);
    form->runs = 0u;
    unsigned int running = base.operands;
    unsigned int first = 0u;
    for (unsigned int bit = 0u; bit <= SASS_BITS; bit += 1u)
    {
        const unsigned int changed = (bit < SASS_BITS) ? sass_operand_changed(&base, s_texts[1u + bit]) : base.operands;
        // a run ends where the operand it changes does, and the two words never share one
        const int joined = (changed == running) && (changed != base.operands) && (bit != 64u);
        if (!joined && (running != base.operands) && (form->runs < SASS_MACHINE_RUNS))
        {
            form->run[form->runs].operand = running;
            form->run[form->runs].first = first;
            form->run[form->runs].last = bit - 1u;
            form->run[form->runs].place = SASS_RUN_NO_PLACE;
            form->runs += 1u;
            // an operand that prints the same field gets the same run
            const unsigned int twin = sass_operand_twin(&base, s_texts[1u + first], running);
            if ((twin != base.operands) && (form->runs < SASS_MACHINE_RUNS))
            {
                form->run[form->runs] = form->run[form->runs - 1u];
                form->run[form->runs].operand = twin;
                form->runs += 1u;
            }
        }
        if (!joined)
        {
            running = changed;
            first = bit;
        }
    }
    return sass_form_unprinted(form, &base, architecture, folder, number);
}

// 1 where the disassembler named every modifier of `operation`. It reports that it could not in three ways. It prints
// INVALID<n> or ???<n>, or it leaves the last dot with nothing after it. An encoding that the disassembler does not
// define is not a form. An assembly from it would write bits that nothing can show the part reads. The three are one
// refusal with three definitions, and a form stored under any of them is a false branch
static int sass_operation_named(const char *operation)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (length != 0u) && (operation[length - 1u] != '.') &&
           (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "INVALID", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) == NULL) &&
           (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "???", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) == NULL);
}

// 1 where `text` is an instruction that the machine can store a form from. The disassembler took it, and it names an
// operation that the disassembler could define whole. The assembler knows where to put each operand type that it
// prints.
//
// The name of the operation alone does not set which form this is. The key of a form is its operation and the type of
// each operand together, because those decide where the assembler puts a number. SHF.L.U32 with a register in the shift
// field and SHF.L.U32 with a number there are two forms with one name. Thus the call turns nothing away here because it
// names the operation that the turn came from. The call sass_machine_take finds the operation and the types. It turns
// away what is truly the form already in hand, such as one with its registers or control bits moved
static int sass_widened_matches(const char *text, SassInstructionParts *parts)
{
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "illegal",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "unprinted",
                    .ci = EMBED_FALSE)))
    {
        return 0;
    }
    sass_instruction_read(text, parts);
    if (!sass_operation_named(parts->operation))
    {
        return 0;
    }
    for (unsigned int place = 0u; place < parts->operands; place += 1u)
    {
        if (parts->type[place] == SASS_OPERAND_UNKNOWN)
        {
            return 0;
        }
    }
    return 1;
}

unsigned int sass_machine_widen(SassMachine *machine, const char *architecture, const char *folder)
{
    // the forms that the listings gave, before the forms that this pass adds. The widen step starts a form from an
    // instruction that the part ran
    const unsigned int listed = machine->forms;
    unsigned int failed = 0u;
    unsigned int rounds = 0u;
    // A form one bit from a listed form is a form that the next round can start one bit from. A widen step over the
    // listings alone reaches what is one bit out and stops. A form two bits out stays out of reach, even where the bit
    // between them is a form that the part takes. The type of a field is such a field, and the lane is short of it.
    //
    // The low word picks a register, a number or a constant for the second and the third operand. On sm_86 those read
    // 0x2 for a register in both, and 0xa and 0x8 for a constant and a number in the second. They read 0x6 and 0x4 for
    // a constant and a number in the third. A register to a number is two bits either way, through the constant that
    // lies between. One round reaches the constant, and the round after it reaches the number.
    //
    // SASS_WIDEN_ROUNDS is that reach. A walk that runs to its own end does not close, and finds more and more. The set
    // of encodings that a walk can reach one bit at a time is most of what the part decodes.
    //
    // Two things are wrong with a walk over all of it. First, it does not close near the 16384 forms that a machine
    // holds: the run ends in `refused` and reports nothing. Second, a form far out carries the operand bits of a chain
    // of unrelated forms. The PLOP3.LUT reading below is such a form: it decodes, and no operation can come from it.
    // The bound is the measured field that the lane is short of. It is not a guess at how far is far enough
    unsigned int done = 0u;
    while ((done < machine->forms) && (rounds < SASS_WIDEN_ROUNDS))
    {
        const unsigned int reach = machine->forms;
        for (unsigned int number = done; number < reach; number += 1u)
        {
            sass_form_turned(&machine->form[number]);
            char path[1024];
            {
                VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
                mmgr_line_put(&buffer_line, folder);
                mmgr_line_put(&buffer_line, "/widen_");
                mmgr_line_decimal(&buffer_line, number, 4u);
                (void)mmgr_line_finish(&buffer_line);
            }
            if (!sass_decode(architecture, path, s_low, s_high, SASS_ENCODINGS, s_texts))
            {
                failed += 1u;
                continue;
            }
            for (unsigned int bit = 0u; bit < SASS_BITS; bit += 1u)
            {
                SassInstructionParts parts;
                SassForm *found_form = NULL;
                const char *const text = s_texts[1u + bit];
                // what the decoder reported about this one bit, for the .ksc. It refused the encoding, took it and
                // printed no line, or named it. Every bit of every form goes through here. The call counts them, and
                // does not store them whole
                sass_class_count(
                    SASS_CHANNEL_DECODE,
                    (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = "illegal", .ci = EMBED_FALSE))
                        ? SASS_CLASS_ILLEGAL
                    : (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "unprinted", .ci = EMBED_FALSE))
                        ? SASS_CLASS_NOTHING
                        : SASS_CLASS_ANSWERS);
                if (sass_widened_matches(text, &parts))
                {
                    sass_machine_take(machine, text, s_low[1u + bit], s_high[1u + bit], &found_form);
                }
            }
        }
        done = reach;
        rounds += 1u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass widen: ");
        mmgr_line_decimal(output_line, listed, 1u);
        mmgr_line_put(output_line, " forms listed, ");
        mmgr_line_decimal(output_line, machine->forms - listed, 1u);
        mmgr_line_put(output_line, " a bit at a time from them over ");
        mmgr_line_decimal(output_line, rounds, 1u);
        mmgr_line_put(output_line, " rounds, ");
        mmgr_line_decimal(output_line, failed, 1u);
        mmgr_line_put(output_line, " forms the disassembler failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return machine->forms - listed;
}

int sass_machine_fields(SassMachine *machine, const char *architecture, const char *folder)
{
    unsigned int failed = 0u;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        failed += sass_form_fields(&machine->form[number], architecture, folder, number) ? 0u : 1u;
    }
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/machine");
        (void)mmgr_line_finish(&buffer_line);
    }
    // the disassembler defines the part SM86 and everything else here defines it sm_86
    {
        VerbaLinea buffer_line = mmgr_line_over(machine->part, sizeof(machine->part));
        mmgr_line_put(&buffer_line, "sm_");
        mmgr_line_put(&buffer_line, architecture + 2);
        (void)mmgr_line_finish(&buffer_line);
    }
    const int written = sass_machine_write(machine, path);
    // the file read back, so that the assembler reading it elsewhere is reading what this wrote
    static SassMachine s_again;
    const int again = written && sass_machine_read(&s_again, path);
    unsigned int differed = again ? 0u : machine->forms;
    for (unsigned int number = 0u; again && (number < machine->forms); number += 1u)
    {
        const SassForm *const was = &machine->form[number];
        const SassForm *const now = &s_again.form[number];
        differed += ((number >= s_again.forms) || (was->low != now->low) || (was->high != now->high) ||
                     (was->runs != now->runs) ||
                     (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = was->text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = now->text, .ci = EMBED_FALSE)))
                        ? 1u
                        : 0u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass machine: ");
        mmgr_line_decimal(output_line, machine->forms, 1u);
        mmgr_line_put(output_line, " forms of the ");
        mmgr_line_decimal(output_line, (unsigned int)SASS_MACHINE_FORMS, 1u);
        mmgr_line_put(output_line, " it contains, ");
        mmgr_line_decimal(output_line, failed, 1u);
        mmgr_line_put(output_line, " without fields, ");
        mmgr_line_put(output_line, written ? "written" : "not written");
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, differed, 1u);
        mmgr_line_put(output_line, " differing when read back, ");
        mmgr_line_decimal(output_line, machine->refused, 1u);
        mmgr_line_put(output_line, " forms it had no room for");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // a machine that filled up does not have forms that nothing named. That is a truncation, not a reading
    return (written != 0) && (failed == 0u) && (differed == 0u) && (machine->refused == 0u);
}

// The carriers that the call picks from the forms in the machine. It takes the first form, then each later form whose
// operands have a new shape. A shape is the count of operands and the type of each. It sets where the bits outside the
// key sit. The return is how many the call took
static unsigned int sass_sweep_carriers(const SassMachine *machine, unsigned int *carrier)
{
    unsigned int taken = 0u;
    for (unsigned int number = 0u; (number < machine->forms) && (taken < SASS_CARRIERS); number += 1u)
    {
        const SassForm *const one = &machine->form[number];
        int seen = 0;
        for (unsigned int at = 0u; at < taken; at += 1u)
        {
            const SassForm *const already = &machine->form[carrier[at]];
            int same = (already->operands == one->operands);
            for (unsigned int place = 0u; same && (place < one->operands); place += 1u)
            {
                same = (already->type[place] == one->type[place]);
            }
            seen = seen || same;
        }
        if (!seen)
        {
            carrier[taken] = number;
            taken += 1u;
        }
    }
    return taken;
}

unsigned int sass_machine_sweep(SassMachine *machine, const char *architecture, const char *folder)
{
    const unsigned int forms_before = machine->forms;
    if (forms_before == 0u)
    {
        return 0u;
    }
    unsigned int carrier[SASS_CARRIERS];
    const unsigned int carriers = sass_sweep_carriers(machine, carrier);
    unsigned int failed = 0u;
    unsigned int named = 0u;
    for (unsigned int which = 0u; which < carriers; which += 1u)
    {
        // a carrier that the part takes, with its operation cut away. Every question below is this encoding with one
        // of the 4096 keys put back. Each bit outside the key stays as a real instruction carried it
        const unsigned long long low = machine->form[carrier[which]].low & ~SASS_OPERATION_MASK;
        const unsigned long long high = machine->form[carrier[which]].high;
        for (unsigned int first = 0u; first <= SASS_OPERATION_MASK; first += SASS_BITS)
        {
            const unsigned int count = (((unsigned long long)first + SASS_BITS) > (SASS_OPERATION_MASK + 1ULL))
                                           ? ((unsigned int)(SASS_OPERATION_MASK + 1ULL) - first)
                                           : SASS_BITS;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                s_low[at] = low | (unsigned long long)(first + at);
                s_high[at] = high;
            }
            char path[1024];
            {
                VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
                mmgr_line_put(&buffer_line, folder);
                mmgr_line_put(&buffer_line, "/sweep_");
                mmgr_line_decimal(&buffer_line, which, 1u);
                mmgr_line_put(&buffer_line, "_");
                mmgr_line_decimal(&buffer_line, first, 4u);
                (void)mmgr_line_finish(&buffer_line);
            }
            if (!sass_decode(architecture, path, s_low, s_high, count, s_texts))
            {
                failed += 1u;
                continue;
            }
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                const char *const text = s_texts[at];
                sass_class_count(
                    SASS_CHANNEL_DECODE,
                    (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = "illegal", .ci = EMBED_FALSE))
                        ? SASS_CLASS_ILLEGAL
                    : (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "unprinted", .ci = EMBED_FALSE))
                        ? SASS_CLASS_NOTHING
                        : SASS_CLASS_ANSWERS);
                named += ((!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                       .other = "illegal", .ci = EMBED_FALSE)) &&
                          (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                       .other = "unprinted", .ci = EMBED_FALSE)))
                             ? 1u
                             : 0u;
                SassInstructionParts parts;
                SassForm *found_form = NULL;
                // a swept encoding came from no form, and the call reads it as it reads a widened one. The call keeps
                // each operation that the decoder names whose operands are all types that the assembler can put
                if (sass_widened_matches(text, &parts))
                {
                    sass_machine_take(machine, text, s_low[at], s_high[at], &found_form);
                }
            }
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass sweep: ");
        mmgr_line_decimal(output_line, (unsigned int)(SASS_OPERATION_MASK + 1ULL), 1u);
        mmgr_line_put(output_line, " keys put to the disassembler from each of ");
        mmgr_line_decimal(output_line, carriers, 1u);
        mmgr_line_put(output_line, " carriers, ");
        mmgr_line_decimal(output_line, named, 1u);
        mmgr_line_put(output_line, " named, ");
        mmgr_line_decimal(output_line, machine->forms - forms_before, 1u);
        mmgr_line_put(output_line, " forms it had not already, ");
        mmgr_line_decimal(output_line, failed, 1u);
        mmgr_line_put(output_line, " batches the disassembler failed, ");
        mmgr_line_decimal(output_line, machine->refused, 1u);
        mmgr_line_put(output_line, " forms past what the machine contains");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return machine->forms - forms_before;
}

int sass_machine_same(const SassMachine *machine, const char *machines)
{
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, machines);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, machine->part);
        mmgr_line_put(&buffer_line, ".khw");
        (void)mmgr_line_finish(&buffer_line);
    }
    static SassMachine s_tree;
    if (!sass_machine_read(&s_tree, path))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "interface sass machine: the tree contains no ");
            mmgr_line_put(output_line, machine->part);
            mmgr_line_put(output_line, ", so this part's is the probe's alone");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    unsigned int differed = (s_tree.forms == machine->forms) ? 0u : 1u;
    for (unsigned int number = 0u; (number < machine->forms) && (number < s_tree.forms); number += 1u)
    {
        const SassForm *const filled = &s_tree.form[number];
        const SassForm *const found = &machine->form[number];
        differed += ((filled->low != found->low) || (filled->high != found->high) ||
                     (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = filled->text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = found->text, .ci = EMBED_FALSE)) ||
                     (filled->runs != found->runs))
                        ? 1u
                        : 0u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass machine: the tree's ");
        mmgr_line_put(output_line, machine->part);
        mmgr_line_put(output_line, " contains ");
        mmgr_line_decimal(output_line, s_tree.forms, 1u);
        mmgr_line_put(output_line, " forms, ");
        mmgr_line_decimal(output_line, differed, 1u);
        mmgr_line_put(output_line, " of them differing from this run's");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (differed != 0u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the part or its toolchain has moved: copy the run's machine over ");
        mmgr_line_put(output_line, path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (differed == 0u) ? 1 : 0;
}
