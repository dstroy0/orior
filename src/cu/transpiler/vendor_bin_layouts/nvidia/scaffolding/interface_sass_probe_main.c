// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_main.c: each form's operations, each operation's fields, and main. The first argument is
// interface_ptx_probe's path, the second a folder for the cubins, listings and decodings. Exit 0 where every cubin was
// listed and every operation decoded, 1 where one was not, 2 where the probe could not ask at all. Given `loop` and a
// machine file after those two, the folder is one an earlier run left its cubins in and only the loop ask is put:
// exit 0 where a form came back on every count, 1 where none did, 2 where the machine file was not read. Given `asks`
// and a machine file, the questions are compiled and only the asks are put (sass_asks_main)
#include "interface_sass_probe_main/probe_main_internal.h"

int main(int count, char **arguments)
{
    if (count < 3)
    {
        fprintf(stderr, "  interface_sass_probe: <interface_ptx_probe> <output folder> [<machines> | loop <machine file> "
                        "| asks <machine file>]\n");
        return 2;
    }
    SassProbe *const probe = &s_sass_probe;
    probe->folder = arguments[2];
    probe->prober = arguments[1];
    probe->machine = &s_sass_machine;
    if ((count > 4) && (strcmp(arguments[3], "loop") == 0))
    {
        return sass_loop_main(probe, arguments[4]);
    }
    if ((count > 4) && (strcmp(arguments[3], "asks") == 0))
    {
        return sass_asks_main(probe, arguments[4]);
    }
    if (!sass_questions_compile(probe))
    {
        return 2;
    }
    if (!sass_list(probe, "kernel", &s_sass_kernel))
    {
        return 2;
    }
    sass_operations_take(probe, &s_sass_kernel);
    sass_machine_listing(&s_sass_machine, &s_sass_kernel);
    for (unsigned int number = 0u; number < probe->questions; number += 1u)
    {
        char name[32];
        snprintf(name, sizeof(name), "form_%u", number);
        if (!sass_list(probe, name, &s_sass_form))
        {
            probe->failed += 1u;
            continue;
        }
        sass_form_print(probe->names[number], &s_sass_form, &s_sass_kernel);
        sass_operations_take(probe, &s_sass_form);
        sass_machine_listing(&s_sass_machine, &s_sass_form);
    }
    // The resident, the kernel the host launches, which no question covers. Its PTX already runs and the part's own
    // compiler turned it into SASS that already runs. What is read here is that SASS: the floor a rearrangement
    // has to beat, and the operations a whole coherent program needs that no single question reaches.
    //
    // It is listed after the questions, and the order carries weight. A form is keyed by its operation and the kinds
    // of its operands: the resident's CALL.ABS.NOINC `(cycle_lane) and the division question's
    // CALL.ABS.NOINC `(__cuda_sm20_div_u64) are one form, and the first listing seen keeps it. The assembler holds
    // a symbol operand by the text the form was learned with, having no way to write a relocation for another:
    // whichever call is listed first is the only call that assembles. Listing the resident first took that form and
    // the division question stopped being writable. That limit on symbol operands is real and stands either way;
    // the order keeps it from costing a question that used to pass
    if (sass_list(probe, "resident", &s_sass_resident))
    {
        sass_form_print("the program resident", &s_sass_resident, &s_sass_kernel);
        sass_operations_take(probe, &s_sass_resident);
        sass_machine_listing(&s_sass_machine, &s_sass_resident);
    }
    else
    {
        probe->failed += 1u;
    }
    for (unsigned int number = 0u; number < probe->operations; number += 1u)
    {
        probe->failed += sass_fields(probe, &probe->operation[number]) ? 0u : 1u;
    }
    printf("interface sass probe: %u questions, %u operations, %u failed\n", probe->questions, probe->operations,
           probe->failed);
    // every operation one bit from one the listings gave, asked of the disassembler before the fields are found, so
    // that the widened forms get their operand runs in the same pass
    sass_machine_widen(&s_sass_machine, probe->architecture, probe->folder);
    // and every operation the part has a coding for at all, asked without starting from a compiler's output
    sass_machine_sweep(&s_sass_machine, probe->architecture, probe->folder);
    // the forms the listings hold and the bits each one's operands sit in, written out for the assembler
    probe->failed += sass_machine_fields(&s_sass_machine, probe->architecture, probe->folder) ? 0u : 1u;
    if (count > 3)
    {
        probe->failed += sass_machine_same(&s_sass_machine, arguments[3]) ? 0u : 1u;
    }
    // every instruction listed, assembled back from its text alone, then disassembled and same to that text
    SassCheck tally;
    memset(&tally, 0, sizeof(tally));
    unsigned int differed = 0u;
    if (sass_list(probe, "kernel", &s_sass_kernel))
    {
        differed += sass_machine_check(&s_sass_machine, &s_sass_kernel, probe->architecture, probe->folder, &tally, 4u);
    }
    for (unsigned int number = 0u; number < probe->questions; number += 1u)
    {
        char name[32];
        snprintf(name, sizeof(name), "form_%u", number);
        if (sass_list(probe, name, &s_sass_form))
        {
            differed += sass_machine_check(&s_sass_machine, &s_sass_form, probe->architecture, probe->folder, &tally,
                                           (differed < 4u) ? 4u : 0u);
        }
    }
    printf("interface sass assemble: %u written back, %u refused, %u the same bytes, %u read back as the same text, %u "
           "same to their bytes alone, %u failed\n",
           tally.checked, tally.refused, tally.same_bits, tally.same_text, tally.by_bytes, differed);
    probe->failed += (differed == 0u) ? 0u : 1u;
    // The resident put through the same check, counted apart. Everything above came from a membership question, and
    // an instruction of one that stops assembling is this assembler breaking. The resident is a whole program the
    // part's compiler wrote, and it reaches operands no question ever produced - a convergence barrier B0, the
    // predicate file PR, a uniform predicate UP0, a constant bank past 0, a call to a symbol the form was not
    // learned with. Those are refused and never guessed at: the reader working. Counting them beside the
    // questions would read as a break where it is a frontier: they are counted and named on their own
    SassCheck reached;
    memset(&reached, 0, sizeof(reached));
    unsigned int resident_differed = 0u;
    if (sass_list(probe, "resident", &s_sass_resident))
    {
        resident_differed =
            sass_machine_check(&s_sass_machine, &s_sass_resident, probe->architecture, probe->folder, &reached, 8u);
    }
    printf("interface sass resident: %u written back, %u refused, %u the same bytes, %u read back as the same text, %u "
           "same to their bytes alone, %u the assembler does not reach yet\n",
           reached.checked, reached.refused, reached.same_bits, reached.same_text, reached.by_bytes,
           resident_differed);
    // A lane of our own put into the resident's cubin: how a SASS program is built. The resident keeps the
    // code the part's own compiler gave it and only cycle_lane is replaced: what is checked here is that
    // replacing one function of a cubin leaves the other as it was, byte for byte
    probe->failed += sass_lane_written(probe) ? 0u : 1u;
    // each kernel written again into a cubin of its own, loaded and run, and its answer same to the toolchain's
    unsigned int cubins = 0u;
    unsigned int same = 0u;
    unsigned int held = 0u;
    same += sass_cubin_same(probe, &s_sass_machine, "kernel", &held) ? 1u : 0u;
    cubins += 1u;
    for (unsigned int number = 0u; number < probe->questions; number += 1u)
    {
        char name[32];
        snprintf(name, sizeof(name), "form_%u", number);
        same += sass_cubin_same(probe, &s_sass_machine, name, &held) ? 1u : 0u;
        cubins += 1u;
    }
    printf("interface sass cubin: %u kernels written again, %u answering as the toolchain's did, %u held off the part\n",
           cubins, same, held);
    probe->failed += ((same + held) == cubins) ? 0u : 1u;
    // the interface's own questions, in code no toolchain wrote
    unsigned int asked = 0u;
    const unsigned int answered = sass_cubin_asks(probe, &s_sass_machine, &asked);
    printf("interface sass ask: %u questions asked in the part's own code, %u answered as the question says\n", asked,
           answered);
    probe->failed += (answered == asked) ? 0u : 1u;
    // and the questions of preference: which of two codings of one thing the part prefers. A reading
    // that does not come back is not a failure, since nothing yet depends on one
    unsigned int weighed = 0u;
    const unsigned int read = sass_cubin_prefers(probe, &s_sass_machine, &weighed);
    printf("interface sass prefer: %u codings weighed against each other, %u read in the part's own clock\n", weighed,
           read);
    // How this system was asked and what came back. Written beside the machine this run learned, in the run's own
    // folder, and copied into the tree by a hand the same way the machine is: a test writes nothing into the source
    // tree, and a record of what the part said is worth reading before it replaces the one already there
    probe->failed += sass_class_write(probe->folder, s_sass_machine.part) ? 0u : 1u;
    return (probe->failed == 0u) ? 0 : 1;
}
