// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_run.c: a question carried to the part: its code put in the container the system accepted, held to
// cubin_safe on the host, and run over its cases, a thread a case. This is the run channel's carrier for a part behind
// NVIDIA's driver (run_channel.h). A question is carried in a process of its own, so a part that refuses one takes
// this process with it and nothing else, or with --list, many untimed questions in one process, the driver opened
// once, each answered to its own blob as it is carried: a part that refuses one takes the process with it, every
// answer written before it stands, and the questions after it are left for the channel to carry again.
//
//     cubin_run <machine file> <layout> <code> <registers> <cases> <answers> <slot> [<launches> [<threads> <blocks>]]
//     cubin_run <machine file> <layout> --list <list>      a line a question, <code> <registers> <cases> <answers>
//     <slot>
//     [<launches>]
//     cubin_run --dry <machine file> <layout> ...          either of the above, with the part taken out
//
// The carrier runs only where a run's writer runs, the .qry QRY names (qry_buffer.h): every input named, the code, the
// cases, the list and the held list, is the latest blob of that name in the run's buffer, and every answer, dry record
// and container is handed to it, a refusal critical and on disk before the carrier goes on: the carrier writes no
// file. Where none runs it carries nothing and exits 2, since the record of what the part was handed and of any fault
// would have nowhere to go.
//
// A dry run writes and holds every question as ever and stops before the driver: each question is answered
// `skipped dry, the part taken out`, and what it would have handed the part is handed as the run's blob dry.txt
// (cubin_run_dry_written). Handed the list a dry run of the protocol writes (run_channel.h), it checks every
// question the protocol put.
//
// After --dry come the facets, each popping out one object of the carry so it can be read off the part; --full pops
// them all:
//   --query                      the query object: every place of the code read back as our reader names it, not
//                                only the places that are not as the run's first question holds them
//   --gate-check                 the safety word laid on and cubin_safe's verdict on what it then holds
//   --diff-output-against-vendor the post-safety-word container as it would reach the part, handed named as the answers
//                                with .cubin, for scaffolding to read against the vendor's own disassembler, the same
//                                C compiled through the vendor's compiler beside it. Our gate judges by operation key;
//                                only the vendor's reader knows a whole encoding illegal
//
// The code is the question's machine code, sixteen bytes an instruction. The layout's container rows hold the container
// (cubin_write.h), and the code, the registers and the exits are put in it. The cases are one a line, up to
// eight words in hex, the words a line leaves out zero, and the kernel is handed the stick's arity: in, eight words a
// thread, out, room for eight words a thread, and the count of threads. The answers are read as the kernel wrote them:
// how far apart its threads write and how many words each writes are read from where the part wrote, and a case's
// answer is the first two words its thread wrote (cubin_run_answers_read). It is launched in blocks of <threads>,
// <blocks> of them, blocks of 256 where the question names no shape, as many as give every case a thread, and every
// thread is given a case, thread t case t of the cases taken round: the cases are answered by the first threads, and
// the rest do the same work over again. One line is written to the answers:
//
//     answered <answer>...      each case's two words as one value in hex, in the order of the cases
//     registers <declared> <part> the registers the question's kernel declares, and the count the part reports for it
//                               through cuFuncGetAttribute: read off a loaded function and not probed, the register
//                               boundary known without an ask that walks up to it and faults the part
//     skipped <verdict> <name>  cubin_safe held it off the part, and the driver never saw it
//     skipped no vendor verdict a live run was given no --held file: the vendor's reader never read this code for a
//                               fault. It is held off the part, the cross must read it first (the safety airlock)
//     skipped <reason>          the container could not be written, or its launch gives the cases no thread each
//                               or more threads than a launch holds, and the driver never saw it
//     refused <error>           the driver or the part would not take it
//
// Every launch, the one that answers and each timed one, is carried alone under cubin_safe's rule 6: between two events
// the part stamps, its stop event read without waiting on it until the part reaches it, the driver gives an error, or
// CUBIN_SAFE_LAUNCH_BOUND passes on the host's clock (cubin_run_bounded). The time between the events is the launch's
// block latency. A launch the driver times out, or one past the bound with its stop event unreached, is answered
// `refused CUDA_ERROR_LAUNCH_TIMEOUT`, and nothing is launched after it in the process.
//
// After each launch the part's own fault telemetry is read, the Xid critical errors the driver's management layer
// stamps and, where the system keeps a log of them, every error the display driver writes to the System log, each Xid
// with its text (cubin_run_telemetry_open, cubin_run_telemetry_faulted), under cubin_safe's rule 7. A launch that makes the
// part stamp one, or one the driver names a context-corrupting fault itself, has faulted the part: it is answered
// `refused`, and nothing is launched after it in the process. This catches an out-of-range register, which faults in
// microseconds and so never trips rule 6's bound, before its fault storm reaches the system's watchdog.
//
// Given a count of launches, a question that answered is launched that many times more, one at a time, and a second
// line gives the block latencies of those launches summed, then the least and the most of them, the runs P6 reads
// (cubin_run_timed):
//
//     timed <launches> <nanoseconds> <least> <most>
//
// As a list ends the carrier says where its time went, one line of its own output: `carrier: <questions> questions;
// input, gate, load, memory, launch, telemetry, handed`, the seconds each stage took summed over the list
// (CubinRunClocks). The part's memory a launch takes and the events it is carried between are allotted once for the
// process and kept to the largest a question asks; a question's module is given back as it is answered.
//
// Each question the gate reads is a row of the run's table of verdicts, put past every rule or held, and each launch a
// row of its table of launches, each keyed to its ask, the sequence of the question's code's blob in the run
// (verdicts.oracle.tsv, launches.oracle.tsv, qry_buffer.h).
//
// The driver is reached by name and by its own entry points, and nothing of its vendor's toolchain is compiled
// against. Exit 0 where a line was written, 2 where no run's writer runs, or the inputs, the machine or the driver were
// not reached.
#include "cubin_run/cubin_run_internal.h"

int main(int count, char **words)
{
    // the run's buffer: every input after is read from it and every answer handed to it. A live run's record of what
    // the part was handed and of any fault it gives back is critical, no setting turns it off, and with no writer it
    // has nowhere to go: nothing is carried
    s_run = qry_run();
    if (s_run == NULL)
    {
        fprintf(stderr, "cubin_run: no query writer runs: %s names no .qry a writer holds open\n", QRY_ENVIRONMENT);
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
    // the facets come after --dry, each popping out one object of the carry to be read off the part; --full pops
    // them all. A dry run with no facet pops out the diff from its first question, as ever. --held <file> takes the
    // vendor's feedback, the encodings its disassembler held, and a matching slot is held before the part on a live run
    while ((count >= 2) && (strncmp(words[1], "--", 2u) == 0) && (strcmp(words[1], "--list") != 0))
    {
        if (strcmp(words[1], "--held") == 0)
        {
            if ((count < 3) || !cubin_run_verdict_load(words[2]))
            {
                fprintf(stderr, "the held list %s did not read\n", (count < 3) ? "(none)" : words[2]);
                return 2;
            }
            words[2] = words[0];
            words += 2;
            count -= 2;
            continue;
        }
        // --lessons <file> takes the lessons a conference taught, the blob of them in the run: every encoding a
        // lesson admits is put past rule 3 (cubin_safe_admitted_read). A run that holds no such blob admits none
        if (strcmp(words[1], "--lessons") == 0)
        {
            const unsigned char *lessons = NULL;
            unsigned long long lessons_size = 0ull;
            if (count < 3)
            {
                fprintf(stderr, "--lessons names no file\n");
                return 2;
            }
            if (qry_latest(s_run, words[2], &lessons, &lessons_size, NULL) &&
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
        fprintf(stderr,
                "cubin_run <machine file> <layout> <code> <registers> <cases> <answers> <slot> [<launches> [<threads> "
                "<blocks>]]\n"
                "cubin_run <machine file> <layout> --list <list>\n"
                "cubin_run [--dry [--full|--query|--gate-check|--diff-output-against-vendor]...] [--held <file>] "
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
    // the gate reads every instruction of every question against the forms, and the forms' places are found once
    sass_encoding_places_hold(&s_machine);
    // the clock the part named beside the machine file, where it names one and the listener answers
    cubin_run_clock_open(words[1]);
    const CubinRunShape unshaped = {CUBIN_RUN_THREADS, 1u};
    if (!listed)
    {
        const unsigned int slot = (unsigned int)strtoul(words[7], NULL, 10);
        const unsigned int launches = (count >= 9) ? (unsigned int)strtoul(words[8], NULL, 10) : 0u;
        const CubinRunShape shape = {(count == 11) ? (unsigned int)strtoul(words[9], NULL, 10) : CUBIN_RUN_THREADS,
                                     (count == 11) ? (unsigned int)strtoul(words[10], NULL, 10) : 1u};
        const int carried =
            cubin_run_question(kernel, pattern_size, words[3], (unsigned int)strtoul(words[4], NULL, 10), words[5],
                               words[6], slot, launches, (count == 11) ? shape : unshaped, count == 11);
        return (carried == 2) ? 2 : 0;
    }
    // Each line of the list one question of no shape of its own, `<code> <registers> <cases> <answers> <slot>
    // [<launches>]`, timed over its launches where it gives a count that is not 0, all carried in this one process and
    // each answered to its own blob as it is carried, each timed question's time its own. A launch the driver or the
    // part refuses leaves a context that answers no other question, and the questions after it are left unanswered, for
    // the channel to carry in a process of their own
    CubinRunLines list;
    if (!cubin_run_lines_open(&list, words[4]))
    {
        fprintf(stderr, "the list did not read\n");
        return 2;
    }
    char line[4u * CUBIN_RUN_LINE];
    int carried = 0;
    unsigned int questions = 0u;
    // a launch that faulted the part or hung ends the round: nothing is launched after it in this process, and the
    // questions left are answered by no one, for the teacher to put again once the conference has ruled
    while ((carried == 0) && !s_launch_timed_out && cubin_run_line(&list, line, sizeof(line)))
    {
        char code_path[CUBIN_RUN_LINE];
        char cases_path[CUBIN_RUN_LINE];
        char answers_path[CUBIN_RUN_LINE];
        unsigned int registers = 0u;
        unsigned int slot = 0u;
        unsigned int launches = 0u;
        if (sscanf(line, "%1023s %u %1023s %1023s %u %u", code_path, &registers, cases_path, answers_path, &slot,
                   &launches) < 5)
        {
            continue;
        }
        carried = cubin_run_question(kernel, pattern_size, code_path, registers, cases_path, answers_path, slot,
                                     launches, unshaped, 0);
        questions += 1u;
    }
    cubin_run_lines_close(&list);
    // the carrier's account of where its time went over the list, a stage a clock, summed over every question
    printf("carrier: %u questions; input %.3f s, gate %.3f s, load %.3f s, memory %.3f s, launch %.3f s, telemetry "
           "%.3f s, handed %.3f s\n",
           questions, (double)s_clocks.input / 1000000000.0, (double)s_clocks.gate / 1000000000.0,
           (double)s_clocks.load / 1000000000.0, (double)s_clocks.memory / 1000000000.0,
           (double)s_clocks.launch / 1000000000.0, (double)s_clocks.telemetry / 1000000000.0,
           (double)s_clocks.handed / 1000000000.0);
    return (carried == 2) ? 2 : 0;
}
