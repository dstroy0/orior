// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_run.c: a question carried to the part. The carrier puts its code in the container that the system accepted,
// checks it against cubin_safe on the host, and runs it over its cases, a thread a case. This is the carrier of the run
// channel for a part behind NVIDIA's driver (run_channel.h).
//
// The carrier carries a question in a process of its own, and a part that refuses one takes only this process with it.
// With --list, it carries many untimed questions in one process, and opens the driver once. It answers each to its own
// blob as it carries it. A part that refuses one then takes the process with it. Every answer before it is already
// written, and the channel carries the questions after it again.
//
//     orior_cubin_run <machine file> <layout> <code> <registers> <cases> <answers> <probe>
//                     [<launches> [<threads> <blocks>]]
//     orior_cubin_run <machine file> <layout> --list <list>      a line a question: <code> <registers> <cases>
//                                                                <answers> <probe> [<launches>]
//     orior_cubin_run --dry <machine file> <layout> ...          either of the above, with the part taken out
//
// The carrier runs only where a writer of the run runs, the .qry that QRY names (qry_member.h). Every input named, the
// code, the cases, the list and the gated list, is the latest blob of that name in the run. Every answer, dry record
// and container goes to it, and a refusal is critical and on disk before the carrier goes on. The carrier writes no
// file. Where none runs, it carries nothing and exits 2, because the record of what the part got and of any fault would
// have nowhere to go.
//
// A dry run writes and gates every question as ever, and stops before the driver. Each question gets the answer
// `skipped dry, the part taken out`, and what it would pass the part goes to the blob dry.txt of the run
// (cubin_run_dry_written). Given the list that a dry run of the protocol writes (run_channel.h), it checks every
// question that the protocol put.
//
// After --dry come the facets. Each pops out one object of the carry, for a read off the part, and --full pops them
// all:
//   - `--query`: the query object. That is every place of the code, read back as our reader names it. It covers more
//     than the places that differ from the first question of the run.
//   - `--gate-check`: the safety word in place, and cubin_safe's verdict on what it then contains.
//   - `--diff-output-against-vendor`: the container after the safety word as it would reach the part, named as the
//     answers with .cubin. Scaffolding reads it against the vendor's own disassembler, beside the same C compiled
//     through the vendor's compiler. Our gate judges by operation key, and only the vendor's reader knows a whole
//     encoding illegal.
//
// The code is the machine code of the question, sixteen bytes an instruction. The container rows of the layout define
// the container (cubin_write.h), and the carrier puts the code, the registers and the exits in it. The cases are one a
// line, up to eight words in hex, with the words that a line leaves out zero. The kernel gets the arity of the stick:
// in, eight words a thread, out, room for eight words a thread, and the count of threads.
//
// The carrier reads the answers as the kernel wrote them. It reads how far apart the threads write, and how many words
// each writes, from where the part wrote. The answer of a case is the first two words that its thread wrote
// (cubin_run_answers_read). The launch goes in blocks of <threads>, <blocks> of them. Where the question names no
// shape, it goes in blocks of 256, as many as give every case a thread. Thread t gets case t of the cases taken round:
// the first threads answer the cases, and the rest repeat the work.
//
// The answers get one line:
//   - `answered <answer>...`: each case's two words as one value in hex, in the order of the cases.
//   - `registers <declared> <part>`: the registers that the kernel of the question declares, and the count that the
//     part reports for it through cuFuncGetAttribute. The carrier reads that off a loaded function, and does not probe
//     it. It knows the register boundary with no query that walks up to it and faults the part.
//   - `skipped <verdict> <name>`: cubin_safe blocked it from the part, and the driver never saw it.
//   - `skipped no vendor verdict`: a live run got no --gated file, and the vendor's reader never read this code for a
//     fault. The carrier blocks it from the part, and the cross must read it first (the safety airlock).
//   - `skipped <reason>`: the carrier could not write the container, or its launch gives the cases no thread each, or
//     more threads than a launch allows. The driver never saw it.
//   - `refused <error>`: the driver or the part would not take it.
//
// Every launch, the one that answers and each timed one, runs alone under cubin_safe's rule 7, between two events that
// the part stamps. The carrier reads its stop event, and does not wait on it. The read stops when the part reaches the
// event, when the driver gives an error, or when CUBIN_SAFE_LAUNCH_BOUND passes on the host's clock
// (cubin_run_bounded). The time between the events is the block latency of the launch. A launch that the driver times
// out, or one past the bound with its stop event unreached, gets the answer `refused CUDA_ERROR_LAUNCH_TIMEOUT`.
// Nothing launches after it in the process.
//
// After each launch, the carrier reads the fault telemetry of the part under cubin_safe's rule 8. That is the Xid
// critical errors that the management layer of the driver stamps. Where the system keeps a log of them, it is also
// every error that the display driver writes to the System log. Each Xid comes with its text (cubin_run_telemetry_open,
// cubin_run_telemetry_faulted). A launch that makes the part stamp one, or that the driver itself names a fault that
// corrupts the context, faulted the part. It gets the answer `refused`, and nothing launches after it in the process.
//
// This catches an out-of-range register before its fault storm reaches the watchdog of the system. That register faults
// in microseconds, and never trips the bound of rule 7.
//
// Given a count of launches, the carrier launches a question that answered that many times more, one at a time. A
// second line gives the block latencies of those launches summed, then the least and the max of them, the runs that P6
// reads (cubin_run_timed):
//
//     timed <launches> <nanoseconds> <least> <max>
//
// As a list ends, the carrier reports where its time went, in one line of its own output:
// `carrier: <questions> questions; input, gate, load, memory, launch, telemetry, passed`. That gives the seconds that
// each stage took, summed over the list (CubinRunClocks). The carrier allots the memory of the part for a launch, and
// the events that it runs between, once for the process. It keeps them at the largest that a question requires, and
// gives the module of a question back as it answers it.
//
// Each question that the gate reads is a row of the table of verdicts of the run, put past every rule or gated. Each
// launch is a row of its table of launches. Each row has its query as its key, the sequence of the code blob of the
// question in the run. The tables are verdicts.oracle.tsv and launches.oracle.tsv (qry_member.h).
//
// The carrier reaches the driver by name and by its own entry points, and compiles nothing against the toolchain of its
// vendor. Exit 0 where a line went out. Exit 2 where no writer of the run runs, or where the inputs, the machine or the
// driver were out of reach.
#include "cubin_run/cubin_run_internal.h"
#include "../../../../../includes/parsers/line_read.h"

int main(int count, char **words)
{
    // the run: every input after comes from it, and every answer goes to it. A live run's record of what the part got
    // and of any fault that it gives back is critical, and no setting turns it off. With no writer it has nowhere to
    // go, and the carrier carries nothing
    s_run = qry_member_run();
    if (s_run == NULL)
    {
        fprintf(stderr, "orior_cubin_run: no query writer runs: %s names no .qry a writer has open\n", QRY_ENVIRONMENT);
        return 2;
    }
    // `--dry` before the machine file runs every question as ever up to the driver, and none past it
    if ((count >= 2) && (strcmp(words[1], "--dry") == 0))
    {
        s_dry = 1;
        words[1] = words[0];
        words += 1;
        count -= 1;
    }
    // the facets come after --dry, each to pop out one object of the carry for a read off the part. The flag --full
    // pops them all. A dry run with no facet pops out the diff from its first question, as ever. The flag --gated
    // <file> takes the feedback of the vendor, the encodings that its disassembler blocked. On a live run, the carrier
    // blocks a probe instruction that matches one before it reaches the part
    while ((count >= 2) && (strncmp(words[1], "--", 2u) == 0) && (strcmp(words[1], "--list") != 0))
    {
        if (strcmp(words[1], "--gated") == 0)
        {
            if ((count < 3) || !cubin_run_verdict_load(words[2]))
            {
                fprintf(stderr, "the gated list %s did not read\n", (count < 3) ? "(none)" : words[2]);
                return 2;
            }
            words[2] = words[0];
            words += 2;
            count -= 2;
            continue;
        }
        // --lessons <file> takes the lessons that a conference taught, the blob of them in the run. Every encoding that
        // a lesson admits goes past rule 3 (cubin_safe_admitted_read). A run with no such blob admits none
        if (strcmp(words[1], "--lessons") == 0)
        {
            const unsigned char *lessons = NULL;
            unsigned long long lessons_size = 0ULL;
            if (count < 3)
            {
                fprintf(stderr, "--lessons names no file\n");
                return 2;
            }
            if (qry_member_blob(words[2], &lessons, &lessons_size, NULL) &&
                !cubin_safe_admitted_read((const char *)lessons, lessons_size))
            {
                fprintf(stderr, "the lessons %s did not read\n", words[2]);
                return 2;
            }
            words[2] = words[0];
            words += 2;
            count -= 2;
            continue;
        }
        if (strcmp(words[1], "--full") == 0)
        {
            s_facets = CUBIN_FACET_QUERY | CUBIN_FACET_GATE | CUBIN_FACET_VENDOR;
        }
        else if (strcmp(words[1], "--query") == 0)
        {
            s_facets |= CUBIN_FACET_QUERY;
        }
        else if (strcmp(words[1], "--gate-check") == 0)
        {
            s_facets |= CUBIN_FACET_GATE;
        }
        else if (strcmp(words[1], "--diff-output-against-vendor") == 0)
        {
            s_facets |= CUBIN_FACET_VENDOR;
        }
        else
        {
            break;
        }
        words[1] = words[0];
        words += 1;
        count -= 1;
    }
    const int listed = (count == 5) && (strcmp(words[3], "--list") == 0);
    if (!listed && (count != 8) && (count != 9) && (count != 11))
    {
        fprintf(
            stderr,
            "orior_cubin_run <machine file> <layout> <code> <registers> <cases> <answers> <probe> [<launches> "
            "[<threads> "
            "<blocks>]]\n"
            "orior_cubin_run <machine file> <layout> --list <list>\n"
            "orior_cubin_run [--dry [--full|--query|--gate-check|--diff-output-against-vendor]...] [--gated <file>] "
            "<machine file> ...\n");
        return 2;
    }
    char kernel[256];
    const unsigned long long pattern_size =
        cubin_pattern_read(words[2], s_pattern, sizeof(s_pattern), kernel, sizeof(kernel));
    if (!sass_machine_read(&s_machine, words[1]))
    {
        fprintf(stderr, "the machine file did not read\n");
        return 2;
    }
    // the gate reads every instruction of every question against the forms, and finds the places of the forms once
    sass_encoding_places_built(&s_machine);
    // the vendor's instruction set for the part, where the tables step pulled it, for the gate's rule 13
    cubin_safe_tables_read(s_machine.part);
    // the clock the part named beside the machine file, where it names one and the listener answers
    cubin_run_clock_open(words[1]);
    const CubinRunShape unshaped = {CUBIN_RUN_THREADS, 1u};
    if (!listed)
    {
        const unsigned int place = (unsigned int)strtoul(words[7], NULL, 10);
        const unsigned int launches = (count >= 9) ? (unsigned int)strtoul(words[8], NULL, 10) : 0u;
        const CubinRunShape shape = {(count == 11) ? (unsigned int)strtoul(words[9], NULL, 10) : CUBIN_RUN_THREADS,
                                     (count == 11) ? (unsigned int)strtoul(words[10], NULL, 10) : 1u};
        const int carried =
            cubin_run_question(kernel, pattern_size, words[3], (unsigned int)strtoul(words[4], NULL, 10), words[5],
                               words[6], place, launches, (count == 11) ? shape : unshaped, count == 11);
        return (carried == 2) ? 2 : 0;
    }
    // Each line of the list is one question of no shape of its own:
    // `<code> <registers> <cases> <answers> <probe> [<launches>]`. The carrier times it over its launches where it
    // gives a count that is not 0. It carries all of them in this one process, and answers each to its own blob as it
    // carries it. Each timed question's time is its own. A launch that the driver or the part refuses leaves a context
    // that answers no other question. The questions after it stay unanswered, for the channel to carry in a process of
    // their own
    CubinRunLines list;
    if (!cubin_run_lines_open(&list, words[4]))
    {
        fprintf(stderr, "the list did not read\n");
        return 2;
    }
    char line[4u * CUBIN_RUN_LINE];
    int carried = 0;
    unsigned int questions = 0u;
    // a launch that faulted the part or hung ends the round: nothing launches after it in this process. No one answers
    // the questions left, and the teacher puts them again once the conference rules
    while ((carried == 0) && !s_launch_timed_out && cubin_run_line(&list, line, sizeof(line)))
    {
        char code_path[CUBIN_RUN_LINE];
        char cases_path[CUBIN_RUN_LINE];
        char answers_path[CUBIN_RUN_LINE];
        unsigned int registers = 0u;
        unsigned int place = 0u;
        unsigned int launches = 0u;
        const char *at = line;
        if (!line_word(&at, code_path, sizeof(code_path)) || !line_unsigned(&at, &registers) ||
            !line_word(&at, cases_path, sizeof(cases_path)) || !line_word(&at, answers_path, sizeof(answers_path)) ||
            !line_unsigned(&at, &place))
        {
            continue;
        }
        // the launches follow where the list names them
        line_unsigned(&at, &launches);
        carried = cubin_run_question(kernel, pattern_size, code_path, registers, cases_path, answers_path, place,
                                     launches, unshaped, 0);
        questions += 1u;
    }
    cubin_run_lines_close(&list);
    // the carrier's account of where its time went over the list, a stage a clock, summed over every question
    printf("carrier: %u questions; input %.3f s, gate %.3f s, load %.3f s, memory %.3f s, launch %.3f s, telemetry "
           "%.3f s, passed %.3f s\n",
           questions, (double)s_clocks.input / 1000000000.0, (double)s_clocks.gate / 1000000000.0,
           (double)s_clocks.load / 1000000000.0, (double)s_clocks.memory / 1000000000.0,
           (double)s_clocks.launch / 1000000000.0, (double)s_clocks.telemetry / 1000000000.0,
           (double)s_clocks.passed / 1000000000.0);
    return (carried == 2) ? 2 : 0;
}
