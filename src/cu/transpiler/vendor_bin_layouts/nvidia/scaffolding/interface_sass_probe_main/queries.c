// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// queries.c: the queries: two containers the same, the lane written, the compiles counted, the questions read and
// compiled, the loop and the queries run
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "probe_main_internal.h"

// the kernel `name`, written again from its own listing. The call then runs both cubins and compares their answers.
// The return is 1 where the two answer the same. A written kernel that cubin_safe blocks from the part runs on
// nothing, and adds one to `same`
int sass_cubin_same(SassProbe *probe, const SassMachine *machine, const char *name, unsigned int *same)
{
    if (!sass_cubin_round(machine, probe->folder, name))
    {
        return 0;
    }
    char path[1024];
    char was[256];
    char now[256];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, "_written.cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!sass_cubin_safe(probe, path))
    {
        *same += 1u;
        return 0;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, ".cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!sass_cubin_answer_toolchain(probe, path, was, sizeof(was)))
    {
        return 0;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, "_written.cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!sass_cubin_answer(probe, path, now, sizeof(now)))
    {
        return 0;
    }
    if (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = was, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = now,
                    .ci = EMBED_FALSE))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cubin ");
            mmgr_line_put(output_line, name);
            mmgr_line_put(output_line, ": the toolchain's ");
            mmgr_line_put(output_line, was);
            mmgr_line_put(output_line, ", the one written ");
            mmgr_line_put(output_line, now);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    return 1;
}

// An own lane of the interface, written into the cubin of the resident, and a check that the resident survived it.
// What goes in is the own text of the kernel, which assembles and needs nothing of the launch. The test here is of the
// cubin writer, not of the lane. The return is 1 where every instruction of the resident is still in the cubin after
int sass_lane_written(SassProbe *probe)
{
    // A lane is a function that the resident calls, and a function ends where the call came from. A kernel body here
    // in place of this would be code that EXITs where the part expects a RET. The cubin writer refuses that, as it
    // must
    static const char s_lane[] = "\tIMAD.MOV.U32 R16, RZ, RZ, R4;\n"
                                 "\tIMAD.MOV.U32 R17, RZ, RZ, R5;\n"
                                 "\tRET.ABS.NODEC R20 0x0;\n";
    if (!sass_cubin_lane_into(&s_sass_machine, probe->folder, s_lane, "program"))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "interface sass lane: the lane did not go into the resident's cubin");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    static SassListing s_before;
    static SassListing s_after;
    if (!sass_list(probe, "resident", &s_before) || !sass_list(probe, "program", &s_after))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "interface sass lane: the cubin did not list");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    // every instruction that the cubin of the resident held, looked for in the cubin that the lane went into. The
    // function cycle_program is the bulk of both, and none of it can move. The empty cycle_lane that the build gave it
    // is two instructions, and the check expects the new cubin to lack those two
    unsigned int same_count = 0u;
    for (unsigned int number = 0u; number < s_before.count; number += 1u)
    {
        int found = 0;
        for (unsigned int at = 0u; (found == 0) && (at < s_after.count); at += 1u)
        {
            found = ((s_before.instructions[number].low == s_after.instructions[at].low) &&
                     (s_before.instructions[number].high == s_after.instructions[at].high))
                        ? 1
                        : 0;
        }
        same_count += (unsigned int)found;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass lane: ");
        mmgr_line_decimal(output_line, s_after.count, 1u);
        mmgr_line_put(output_line, " instructions in the cubin the lane went into, ");
        mmgr_line_decimal(output_line, same_count, 1u);
        mmgr_line_put(output_line, " of the resident's ");
        mmgr_line_decimal(output_line, s_before.count, 1u);
        mmgr_line_put(output_line, " still in it");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // the resident's own instructions all survive; what the empty lane contained is what the new lane replaced
    return (s_before.count - same_count) <= 2u;
}

// every kernel that interface_ptx_probe passed to the compiler of the toolchain, counted on the compile channel from
// what it printed. A line "cubin <number> <name>" is a kernel that the compiler emitted. A line "errored: nvJitLink did
// not assemble" is one that it refused. The compiler compiles the kernel and the resident before every question. They
// print no line of their own when the compiler emits them, and the call counts both with the line of the first
// question
static void sass_compiles_count(const char *output)
{
    const char *const emitted = "\ncubin 0 ";
    if (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = output, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = emitted,
                   .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL)
    {
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ANSWERS);
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ANSWERS);
    }
    for (const char *line =
             EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = output, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "\ncubin ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
         line != NULL;
         line = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = "\ncubin ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE))
    {
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ANSWERS);
    }
    for (const char *line =
             EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = output, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "errored: nvJitLink", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
         line != NULL;
         line = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = "errored: nvJitLink", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE))
    {
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ILLEGAL);
    }
}

static int sass_questions_read(SassProbe *probe, const char *output)
{
    // the line may follow others the ruleset's reader printed
    const char *const line =
        (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = output, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "sm_",
                    .ci = EMBED_FALSE))
            ? output
            : EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = output, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = "\nsm_", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    if (line == NULL)
    {
        return 0;
    }
    const unsigned long version = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                             .src = line + ((*line == '\n') ? 4 : 3), .end = NULL, .base = 10);
    {
        VerbaLinea buffer_line = mmgr_line_over(probe->architecture, sizeof(probe->architecture));
        mmgr_line_put(&buffer_line, "SM");
        mmgr_line_decimal(&buffer_line, version, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    probe->questions = 0u;
    for (const char *line =
             EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = output, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "\ncubin ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
         line != NULL;
         line = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = "\ncubin ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE))
    {
        const char *after = NULL;
        const unsigned long number =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = line + 7, .end = &after, .base = 10);
        if ((number != (unsigned long)probe->questions) || (probe->questions == SASS_QUESTIONS) || (*after != ' '))
        {
            return 0;
        }
        const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = after + 1,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\r\n");
        {
            VerbaLinea buffer_line = mmgr_line_over(probe->names[probe->questions], SASS_NAME);
            mmgr_line_put_max(&buffer_line, after + 1, (size_t)((int)length));
            (void)mmgr_line_finish(&buffer_line);
        }
        probe->questions += 1u;
    }
    return probe->questions != 0u;
}

// The loop query alone, against the machine file at `path` and the cubins already in the folder of the probe. The
// call asks the part how it loops, and does not learn the machine again. It writes the classification of the system
// beside them
int sass_loop_main(SassProbe *probe, const char *path)
{
    if (!sass_machine_read(&s_sass_machine, path))
    {
        return 2;
    }
    unsigned int queried = 0u;
    const unsigned int looped = sass_cubin_loops(probe, &s_sass_machine, &queried);
    const int written = sass_class_write(probe->folder, s_sass_machine.part);
    return ((looped != 0u) && written) ? 0 : 1;
}

// interface_ptx_probe requested to compile every question into the probe's folder, each compile counted on its channel:
// 1, or 0 with what it printed
int sass_questions_compile(SassProbe *probe)
{
    char output[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(output, sizeof(output));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/cubins.out");
        (void)mmgr_line_finish(&buffer_line);
    }
    char *const command[] = {(char *)probe->prober, "cubins", (char *)probe->folder, NULL};
    const int status = sass_run(command, output);
    sass_compiles_count(sass_output());
    if ((status != 0) || !sass_questions_read(probe, sass_output()))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  interface_ptx_probe cubins exited ");
            mmgr_line_decimal_signed(output_line, status);
            mmgr_line_put(output_line, "\n");
            mmgr_line_put(output_line, (status >= 0) ? sass_output() : "");
            platform_line_out_ended();
        }
        return 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, probe->architecture);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, probe->questions, 1u);
        mmgr_line_put(output_line, " questions");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

// The queries alone, against the machine file at `path`. The call compiles every question. Our assembler writes each
// kernel again, and the call runs it beside the kernel of the toolchain. The own questions of the interface run in
// code that no toolchain wrote, and the call weighs the codings. The machine comes from the tree, and the call does
// not learn it again. It writes the classification of the system into the folder of the probe.
//
// Exit 0 where every kernel and every question answered as it specifies, and 1 where one did not. Exit 2 where the
// machine file did not read, or nothing compiled
int sass_queries_main(SassProbe *probe, const char *path)
{
    if (!sass_machine_read(&s_sass_machine, path) || !sass_questions_compile(probe))
    {
        return 2;
    }
    // a kernel that the gate blocked from the part is no check, because the call queried nothing of it
    unsigned int checks = 0u;
    unsigned int failed = 0u;
    unsigned int same_count = 0u;
    for (unsigned int number = 0u; number <= probe->questions; number += 1u)
    {
        char name[32];
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "form_");
            mmgr_line_decimal(&buffer_line, number - 1u, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        if (number == 0u)
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "kernel");
            (void)mmgr_line_finish(&buffer_line);
        }
        const unsigned int was_same = same_count;
        const int same =
            sass_list(probe, name, &s_sass_form) && sass_cubin_same(probe, &s_sass_machine, name, &same_count);
        checks += (same_count == was_same) ? 1u : 0u;
        failed += (same || (same_count != was_same)) ? 0u : 1u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass cubin: ");
        mmgr_line_decimal(output_line, probe->questions + 1u, 1u);
        mmgr_line_put(output_line, " kernels written again, ");
        mmgr_line_decimal(output_line, checks - failed, 1u);
        mmgr_line_put(output_line, " answering as the toolchain's did, ");
        mmgr_line_decimal(output_line, same_count, 1u);
        mmgr_line_put(output_line, " blocked from the part");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
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
    checks += queried;
    failed += queried - answered;
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
    failed += sass_class_write(probe->folder, s_sass_machine.part) ? 0u : 1u;
    checks += 1u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass queries: ");
        mmgr_line_decimal(output_line, checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (failed == 0u) ? 0 : 1;
}
