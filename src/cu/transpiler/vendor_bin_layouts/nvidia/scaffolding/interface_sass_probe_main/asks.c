// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// asks.c: the asks: two containers the same, the lane written, the compiles counted, the questions read and compiled, the loop and the asks run
#include "probe_main_internal.h"

// the kernel `name` written again from its own listing, then both cubins run and their answers compared: 1 where the
// two answer the same. A written kernel cubin_safe holds off the part is run on nothing, and adds one to `held`
int sass_cubin_same(SassProbe *probe, const SassMachine *machine, const char *name, unsigned int *held)
{
    if (!sass_cubin_round(machine, probe->folder, name))
    {
        return 0;
    }
    char path[1024];
    char was[256];
    char now[256];
    snprintf(path, sizeof(path), "%s/%s_written.cubin", probe->folder, name);
    if (!sass_cubin_safe(probe, path))
    {
        *held += 1u;
        return 0;
    }
    snprintf(path, sizeof(path), "%s/%s.cubin", probe->folder, name);
    if (!sass_cubin_answer_toolchain(probe, path, was, sizeof(was)))
    {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/%s_written.cubin", probe->folder, name);
    if (!sass_cubin_answer(probe, path, now, sizeof(now)))
    {
        return 0;
    }
    if (strcmp(was, now) != 0)
    {
        printf("  cubin %s: the toolchain's %s, the one written %s\n", name, was, now);
        return 0;
    }
    return 1;
}

// A lane of the interface's own written into the resident's cubin, and the resident checked for having survived it. What
// goes in is the kernel's own text, which assembles and asks nothing of the launch: under test here is the cubin
// writer instead of the lane. 1 where every instruction of the resident is still in the cubin afterwards
int sass_lane_written(SassProbe *probe)
{
    // A lane is a function the resident calls, and a function ends where it was called from. Putting a kernel's
    // body here in place of this would be writing something that EXITs where the part expects a RET, which the
    // cubin writer refuses and should
    static const char s_lane[] = "\tIMAD.MOV.U32 R16, RZ, RZ, R4;\n"
                                 "\tIMAD.MOV.U32 R17, RZ, RZ, R5;\n"
                                 "\tRET.ABS.NODEC R20 0x0;\n";
    if (!sass_cubin_lane_into(&s_sass_machine, probe->folder, s_lane, "program"))
    {
        printf("interface sass lane: the lane did not go into the resident's cubin\n");
        return 0;
    }
    static SassListing s_before;
    static SassListing s_after;
    if (!sass_list(probe, "resident", &s_before) || !sass_list(probe, "program", &s_after))
    {
        printf("interface sass lane: the cubin did not list\n");
        return 0;
    }
    // every instruction the resident's cubin held, looked for in the one the lane went into. cycle_program is the
    // bulk of both and none of it may have moved; the empty cycle_lane it was built with is two instructions, and
    // those are the ones expected to be gone
    unsigned int held = 0u;
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
        held += (unsigned int)found;
    }
    printf("interface sass lane: %u instructions in the cubin the lane went into, %u of the resident's %u still in it\n",
           s_after.count, held, s_before.count);
    // the resident's own instructions all survive; what the empty lane held is what the new lane replaced
    return (s_before.count - held) <= 2u;
}

// every kernel interface_ptx_probe handed the toolchain's compiler, counted on the compile channel from what it printed:
// a line "cubin <number> <name>" is a kernel the compiler emitted, and a line "errored: nvJitLink did not assemble" is
// one it refused. The kernel and the resident are compiled before every question and print no line of their own when
// they are emitted: both are counted with the first question's line
static void sass_compiles_count(const char *output)
{
    const char *const emitted = "\ncubin 0 ";
    if (strstr(output, emitted) != NULL)
    {
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ANSWERS);
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ANSWERS);
    }
    for (const char *line = strstr(output, "\ncubin "); line != NULL; line = strstr(line + 1, "\ncubin "))
    {
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ANSWERS);
    }
    for (const char *line = strstr(output, "errored: nvJitLink"); line != NULL;
         line = strstr(line + 1, "errored: nvJitLink"))
    {
        sass_class_count(SASS_CHANNEL_COMPILE, SASS_CLASS_ILLEGAL);
    }
}

static int sass_questions_read(SassProbe *probe, const char *output)
{
    // the line may follow others the ruleset's reader printed
    const char *const line = (strncmp(output, "sm_", 3u) == 0) ? output : strstr(output, "\nsm_");
    if (line == NULL)
    {
        return 0;
    }
    const unsigned long version = strtoul(line + ((*line == '\n') ? 4 : 3), NULL, 10);
    snprintf(probe->architecture, sizeof(probe->architecture), "SM%lu", version);
    probe->questions = 0u;
    for (const char *line = strstr(output, "\ncubin "); line != NULL; line = strstr(line + 1, "\ncubin "))
    {
        char *after = NULL;
        const unsigned long number = strtoul(line + 7, &after, 10);
        if ((number != (unsigned long)probe->questions) || (probe->questions == SASS_QUESTIONS) || (*after != ' '))
        {
            return 0;
        }
        const size_t length = strcspn(after + 1, "\r\n");
        snprintf(probe->names[probe->questions], SASS_NAME, "%.*s", (int)length, after + 1);
        probe->questions += 1u;
    }
    return probe->questions != 0u;
}

// The loop ask alone, against the machine file at `path` and the cubins already in the probe's folder: the part is
// asked how it loops without the machine being learned again. The system's classification is written beside them
int sass_loop_main(SassProbe *probe, const char *path)
{
    if (!sass_machine_read(&s_sass_machine, path))
    {
        return 2;
    }
    unsigned int asked = 0u;
    const unsigned int kept = sass_cubin_loops(probe, &s_sass_machine, &asked);
    const int written = sass_class_write(probe->folder, s_sass_machine.part);
    return ((kept != 0u) && written) ? 0 : 1;
}

// interface_ptx_probe asked to compile every question into the probe's folder, each compile counted on its channel: 1,
// or 0 with what it printed
int sass_questions_compile(SassProbe *probe)
{
    char output[1024];
    snprintf(output, sizeof(output), "%s/cubins.out", probe->folder);
    char *const command[] = {(char *)probe->prober, "cubins", (char *)probe->folder, NULL};
    const int status = sass_run(command, output);
    sass_compiles_count(sass_output());
    if ((status != 0) || !sass_questions_read(probe, sass_output()))
    {
        printf("  interface_ptx_probe cubins exited %d\n%s", status, (status >= 0) ? sass_output() : "");
        return 0;
    }
    printf("%s: %u questions\n", probe->architecture, probe->questions);
    return 1;
}

// The asks alone, against the machine file at `path`: every question compiled, each kernel written again by our
// assembler and run beside the toolchain's, the interface's own questions run in code no toolchain wrote, and the
// codings weighed, with the machine taken from the tree and not learned again. The system's classification is
// written into the probe's folder. Exit 0 where every kernel and every question answered as it says, 1 where one did
// not, 2 where the machine file was not read or nothing compiled
int sass_asks_main(SassProbe *probe, const char *path)
{
    if (!sass_machine_read(&s_sass_machine, path) || !sass_questions_compile(probe))
    {
        return 2;
    }
    // a kernel held off the part is no check: nothing was asked of it
    unsigned int checks = 0u;
    unsigned int failed = 0u;
    unsigned int held = 0u;
    for (unsigned int number = 0u; number <= probe->questions; number += 1u)
    {
        char name[32];
        snprintf(name, sizeof(name), "form_%u", number - 1u);
        if (number == 0u)
        {
            snprintf(name, sizeof(name), "kernel");
        }
        const unsigned int was_held = held;
        const int same = sass_list(probe, name, &s_sass_form) && sass_cubin_same(probe, &s_sass_machine, name, &held);
        checks += (held == was_held) ? 1u : 0u;
        failed += (same || (held != was_held)) ? 0u : 1u;
    }
    printf("interface sass cubin: %u kernels written again, %u answering as the toolchain's did, %u held off the part\n",
           probe->questions + 1u, checks - failed, held);
    unsigned int asked = 0u;
    const unsigned int answered = sass_cubin_asks(probe, &s_sass_machine, &asked);
    printf("interface sass ask: %u questions asked in the part's own code, %u answered as the question says\n", asked,
           answered);
    checks += asked;
    failed += asked - answered;
    unsigned int weighed = 0u;
    const unsigned int read = sass_cubin_prefers(probe, &s_sass_machine, &weighed);
    printf("interface sass prefer: %u codings weighed against each other, %u read in the part's own clock\n", weighed,
           read);
    failed += sass_class_write(probe->folder, s_sass_machine.part) ? 0u : 1u;
    checks += 1u;
    printf("interface sass asks: %u checks, %u failed\n", checks, failed);
    return (failed == 0u) ? 0 : 1;
}
