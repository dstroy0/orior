// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_main.c: each form's operations, each operation's fields, and main. The first argument is
// interface_ptx_probe's path, the second a folder for the cubins, listings and decodings. Exit 0 where every cubin was
// listed and every operation decoded, 1 where one was not, 2 where the probe could not query at all. Given `loop` and a
// machine file after those two, the folder is one an earlier run left its cubins in and only the loop query is put:
// exit 0 where a form came back on every count, 1 where none did, 2 where the machine file was not read. Given
// `queries` and a machine file, the questions are compiled and only the queries are put (sass_queries_main)
#include "../../../../../../includes/platform/platform.h"
#include "../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "interface_sass_probe_main/probe_main_internal.h"
#include "memoria_operor/memoria_operor.h"

int program_main(int count, char **arguments)
{
    if (count < 3)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  interface_sass_probe: <interface_ptx_probe> <output folder> "
                                             "[<machines> | loop <machine file> | queries <machine file>]");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    SassProbe *const probe = &s_sass_probe;
    probe->folder = arguments[2];
    probe->prober = arguments[1];
    probe->machine = &s_sass_machine;
    if ((count > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[3], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "loop", .ci = EMBED_FALSE)))
    {
        return sass_loop_main(probe, arguments[4]);
    }
    if ((count > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[3], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "queries", .ci = EMBED_FALSE)))
    {
        return sass_queries_main(probe, arguments[4]);
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
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "form_");
            mmgr_line_decimal(&buffer_line, number, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        if (!sass_list(probe, name, &s_sass_form))
        {
            probe->failed += 1u;
            continue;
        }
        sass_form_print(probe->names[number], &s_sass_form, &s_sass_kernel);
        sass_operations_take(probe, &s_sass_form);
        sass_machine_listing(&s_sass_machine, &s_sass_form);
    }
    // The resident, the kernel that the host launches, which no question covers. Its PTX already runs, and the own
    // compiler of the part turned it into SASS that already runs. The call reads that SASS here. It is the floor that a
    // new arrangement must beat. It also has the operations that a whole coherent program needs and no single question
    // reaches.
    //
    // The listing puts it after the questions, and the order is important. The key of a form is its operation and the
    // types of its operands. Two calls are one form: CALL.ABS.NOINC
    // `(cycle_lane) in the resident and CALL.ABS.NOINC `(__cuda_sm20_div_u64) in the division question. The first
    // listing that the call sees keeps the form. The assembler carries a symbol operand by the text that the form came
    // with, and cannot write a relocation for another. Thus the call that the listing puts first is the only call that
    // assembles.
    //
    // With the resident first, it took that form, and the program could no longer write the division question. That
    // limit on symbol operands is real in both orders. This order keeps it from the loss of a question that passed
    // before
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
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass probe: ");
        mmgr_line_decimal(output_line, probe->questions, 1u);
        mmgr_line_put(output_line, " questions, ");
        mmgr_line_decimal(output_line, probe->operations, 1u);
        mmgr_line_put(output_line, " operations, ");
        mmgr_line_decimal(output_line, probe->failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // every operation one bit from one that the listings gave. The call queries the disassembler for them before it
    // finds the fields, so that the widened forms get their operand runs in the same pass
    sass_machine_widen(&s_sass_machine, probe->architecture, probe->folder);
    // and every operation that the part has any coding for, queried with no start from the output of a compiler
    sass_machine_sweep(&s_sass_machine, probe->architecture, probe->folder);
    // the forms the listings contain and the bits each one's operands sit in, written out for the assembler
    probe->failed += sass_machine_fields(&s_sass_machine, probe->architecture, probe->folder) ? 0u : 1u;
    if (count > 3)
    {
        probe->failed += sass_machine_same(&s_sass_machine, arguments[3]) ? 0u : 1u;
    }
    // every instruction listed, assembled back from its text alone, then disassembled and same to that text
    SassCheck tally;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &tally, .bytes = sizeof(tally), .val = 0);
    unsigned int differed = 0u;
    if (sass_list(probe, "kernel", &s_sass_kernel))
    {
        differed += sass_machine_check(&s_sass_machine, &s_sass_kernel, probe->architecture, probe->folder, &tally, 4u);
    }
    for (unsigned int number = 0u; number < probe->questions; number += 1u)
    {
        char name[32];
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "form_");
            mmgr_line_decimal(&buffer_line, number, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        if (sass_list(probe, name, &s_sass_form))
        {
            differed += sass_machine_check(&s_sass_machine, &s_sass_form, probe->architecture, probe->folder, &tally,
                                           (differed < 4u) ? 4u : 0u);
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass assemble: ");
        mmgr_line_decimal(output_line, tally.checked, 1u);
        mmgr_line_put(output_line, " written back, ");
        mmgr_line_decimal(output_line, tally.refused, 1u);
        mmgr_line_put(output_line, " refused, ");
        mmgr_line_decimal(output_line, tally.same_bits, 1u);
        mmgr_line_put(output_line, " the same bytes, ");
        mmgr_line_decimal(output_line, tally.same_text, 1u);
        mmgr_line_put(output_line, " read back as the same text, ");
        mmgr_line_decimal(output_line, tally.by_bytes, 1u);
        mmgr_line_put(output_line, " same to their bytes alone, ");
        mmgr_line_decimal(output_line, differed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    probe->failed += (differed == 0u) ? 0u : 1u;
    // The resident, put through the same check and counted apart. All above came from a membership question. An
    // instruction of one that no longer assembles is a break of this assembler.
    //
    // The resident is a whole program that the compiler of the part wrote, and it reaches operands that no question
    // ever made. Examples are a convergence barrier B0, the predicate file PR, a uniform predicate UP0 and a constant
    // bank past 0. Another is a call to a symbol that the form did not come with. The reader refuses those and never
    // guesses at them, as it must.
    //
    // A count of them beside the questions would read as a break where it is a frontier. The call counts and names them
    // on their own
    SassCheck reached;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &reached, .bytes = sizeof(reached), .val = 0);
    unsigned int resident_differed = 0u;
    if (sass_list(probe, "resident", &s_sass_resident))
    {
        resident_differed =
            sass_machine_check(&s_sass_machine, &s_sass_resident, probe->architecture, probe->folder, &reached, 8u);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass resident: ");
        mmgr_line_decimal(output_line, reached.checked, 1u);
        mmgr_line_put(output_line, " written back, ");
        mmgr_line_decimal(output_line, reached.refused, 1u);
        mmgr_line_put(output_line, " refused, ");
        mmgr_line_decimal(output_line, reached.same_bits, 1u);
        mmgr_line_put(output_line, " the same bytes, ");
        mmgr_line_decimal(output_line, reached.same_text, 1u);
        mmgr_line_put(output_line, " read back as the same text, ");
        mmgr_line_decimal(output_line, reached.by_bytes, 1u);
        mmgr_line_put(output_line, " same to their bytes alone, ");
        mmgr_line_decimal(output_line, resident_differed, 1u);
        mmgr_line_put(output_line, " the assembler does not reach yet");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // A lane of our own, put into the cubin of the resident: how the program builds a SASS program. The resident keeps
    // the code that the own compiler of the part gave it, and the call replaces only cycle_lane. This checks that a
    // replacement of one function of a cubin leaves the other as it was, byte for byte
    probe->failed += sass_lane_written(probe) ? 0u : 1u;
    // each kernel written again into a cubin of its own, loaded and run, and its answer same to the toolchain's
    unsigned int cubins = 0u;
    unsigned int same = 0u;
    unsigned int same_count = 0u;
    same += sass_cubin_same(probe, &s_sass_machine, "kernel", &same_count) ? 1u : 0u;
    cubins += 1u;
    for (unsigned int number = 0u; number < probe->questions; number += 1u)
    {
        char name[32];
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "form_");
            mmgr_line_decimal(&buffer_line, number, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        same += sass_cubin_same(probe, &s_sass_machine, name, &same_count) ? 1u : 0u;
        cubins += 1u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass cubin: ");
        mmgr_line_decimal(output_line, cubins, 1u);
        mmgr_line_put(output_line, " kernels written again, ");
        mmgr_line_decimal(output_line, same, 1u);
        mmgr_line_put(output_line, " answering as the toolchain's did, ");
        mmgr_line_decimal(output_line, same_count, 1u);
        mmgr_line_put(output_line, " blocked from the part");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    probe->failed += ((same + same_count) == cubins) ? 0u : 1u;
    // the interface's own questions, in code no toolchain wrote
    unsigned int queried = 0u;
    const unsigned int answered = sass_cubin_queries(probe, &s_sass_machine, &queried);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass query: ");
        mmgr_line_decimal(output_line, queried, 1u);
        mmgr_line_put(output_line, " questions queried in the part's own code, ");
        mmgr_line_decimal(output_line, answered, 1u);
        mmgr_line_put(output_line, " answered as the question specifies");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    probe->failed += (answered == queried) ? 0u : 1u;
    // and the questions of preference: which of two codings of one thing the part prefers. A reading
    // that does not come back is not a failure, since nothing yet depends on one
    unsigned int weighed = 0u;
    const unsigned int read = sass_cubin_prefers(probe, &s_sass_machine, &weighed);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass prefer: ");
        mmgr_line_decimal(output_line, weighed, 1u);
        mmgr_line_put(output_line, " codings weighed against each other, ");
        mmgr_line_decimal(output_line, read, 1u);
        mmgr_line_put(output_line, " read in the part's own clock");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // How the program queried this system, and what came back. The call writes it beside the machine that this run
    // learned, in the own folder of the run. A person copies it into the tree, as with the machine. A test writes
    // nothing into the source tree. A record of what the part reported is worth a read before it replaces the one
    // already there
    probe->failed += sass_class_write(probe->folder, s_sass_machine.part) ? 0u : 1u;
    return (probe->failed == 0u) ? 0 : 1;
}
