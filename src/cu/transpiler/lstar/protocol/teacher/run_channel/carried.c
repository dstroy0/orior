// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carried.c: the round carried: one question or many handed critical, carried by the carrier and answered
#include "run_channel_internal.h"

#include <time.h>

unsigned long long run_channel_now(void)
{
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) != TIME_UTC)
    {
        return 0ull;
    }
    // a clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * 1000000000ull) + (unsigned long long)now.tv_nsec;
}

void run_channel_clocks(unsigned long long *airlock, unsigned long long *pop, unsigned long long *cross,
                        unsigned long long *carried)
{
    *airlock = s_channel.airlock_nanoseconds;
    *pop = s_channel.pop_nanoseconds;
    *cross = s_channel.cross_nanoseconds;
    *carried = s_channel.carried_nanoseconds;
}

// `asked` carried as run_channel_ask carries it, R aside
int run_channel_carried(RunQuestion *asked)
{
    if (run_channel_panic_ruled())
    {
        RunQuestion *const one[1] = {asked};
        run_channel_halted(one, 0u, 1u);
        return 0;
    }
    if (s_channel.open && s_channel.dry)
    {
        run_channel_dry_written(asked);
        return 0;
    }
    if (!s_channel.open)
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the channel is not open");
        return 0;
    }
    if (run_channel_on_line() != 0ull)
    {
        RunQuestion *const one[1] = {asked};
        run_channel_halted(one, 0u, 1u);
        return 0;
    }
    char code_name[] = "question.bin";
    char cases_name[] = "cases.txt";
    char answers_name[] = "answers.txt";
    char registers[16];
    char slot[16];
    char launches[16];
    char threads[16];
    char blocks[16];
    char line[RUN_LINE_LONGEST];
    snprintf(slot, sizeof(slot), "%u", asked->slot);
    snprintf(launches, sizeof(launches), "%u", asked->launches);
    snprintf(threads, sizeof(threads), "%u", asked->threads);
    snprintf(blocks, sizeof(blocks), "%u", asked->blocks);
    if (!run_channel_write(asked, code_name, cases_name))
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the question could not be handed to the run's buffer");
        return 0;
    }
    // the round, this one question, checked before the part and fixed where a count of registers fixes it, then its
    // line handed critical before the carrier starts
    RunQuestion *const one[1] = {asked};
    const int listed = run_channel_relisted(one, 0u, 1u, 1);
    const unsigned long long airlock_began = run_channel_now();
    const int checked = listed ? run_channel_checked(one, 0u, 1u, 1, asked->refused, sizeof(asked->refused)) : 1;
    s_channel.airlock_nanoseconds += run_channel_now() - airlock_began;
    if (!checked)
    {
        asked->outcome = RUN_NO_CHANNEL;
        return 0;
    }
    // the count of registers the check left the question, which the carrier is handed
    run_channel_list_line(0u, line);
    snprintf(registers, sizeof(registers), "%u", asked->registers);
    const unsigned long long after = listed ? run_channel_round_handed() : 0ull;
    if (after == 0ull)
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the round could not be put on disk, and it is not carried");
        return 0;
    }
    // the interface's command is a list of words it does not write to; the cast only meets its declared type
    char *command[RUN_CARRIER_WORDS + 8u];
    unsigned int words = 0u;
    for (; words < s_channel.words; words += 1u)
    {
        command[words] = s_channel.word[words];
    }
    command[words] = code_name;
    command[words + 1u] = registers;
    command[words + 2u] = cases_name;
    command[words + 3u] = answers_name;
    command[words + 4u] = slot;
    words += 5u;
    // an untimed question of no shape of its own is carried with the words it always was; a timed one with its count
    // of launches after, and one with a shape with its count of launches, 0 where it is untimed, and its shape
    const int shaped = (asked->threads != 0u) || (asked->blocks != 0u);
    if ((asked->launches != 0u) || shaped)
    {
        command[words] = launches;
        words += 1u;
    }
    if (shaped)
    {
        command[words] = threads;
        command[words + 1u] = blocks;
        words += 2u;
    }
    command[words] = NULL;
    // the carrier's output comes back through a pipe, and no file is made
    const InterfaceProbe probe = {command, NULL, s_channel.limit_microseconds};
    InterfaceAnswer answer = {0};
    answer.output = s_carrier_output;
    answer.output_capacity = sizeof(s_carrier_output);
    EngineError error = {0};
    // what this process handed moved into the run before the carrier reads it, and what it handed once it ends
    qry_moved(s_channel.buffer);
    const unsigned long long carrier_began = run_channel_now();
    const long ran = interface_probe_run(&probe, &answer, &error);
    s_channel.carried_nanoseconds += run_channel_now() - carrier_began;
    qry_settled(s_channel.buffer);
    run_channel_output_handed(&answer);
    // a panic on the line, the carrier's or its ending's: the teacher's account handed
    run_channel_teacher_said(1u, after, &answer, 0u, line);
    if ((ran != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier %.96s was not started", s_channel.word[0]);
        return 0;
    }
    if ((answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        // a carrier that did not end clean wrote no answer worth reading: its ending is the answer, and where it was
        // ended, or a panic is on the line, it ended carrying this question, which calls a conference
        asked->outcome = (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL
                                                                     : (unsigned int)RUN_NOTHING;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier %s, code %llx",
                 interface_ending_name(answer.ending), answer.code);
        if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ull))
        {
            run_channel_conference_called(asked, asked->refused, NULL, 0ull);
        }
        return 0;
    }
    run_channel_read(asked, answers_name, after);
    return asked->outcome == RUN_ANSWERED;
}

unsigned int run_channel_ask_many(RunQuestion *const *asked, unsigned int count)
{
    // the questions R answers taken first, and only the rest carried, each kept in R once it comes back
    RunQuestion **const left = (RunQuestion **)malloc((size_t)(count + 1u) * sizeof(RunQuestion *));
    if (left == NULL)
    {
        return run_channel_carried_many(asked, count);
    }
    // a stopped run carries nothing and answers nothing from R: every question is held with the stop's reason
    if (run_channel_panic_ruled())
    {
        run_channel_halted(asked, 0u, count);
        free(left);
        return 0u;
    }
    unsigned int recorded = 0u;
    unsigned int lefts = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        asked[at]->refused[0] = '\0';
        if (run_channel_recorded(asked[at]))
        {
            recorded += (asked[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
            RunQuestion *const one[1] = {asked[at]};
            run_channel_traced(one, 1u, "from R");
            continue;
        }
        left[lefts] = asked[at];
        lefts += 1u;
    }
    const unsigned int carried = (lefts != 0u) ? run_channel_carried_many(left, lefts) : 0u;
    for (unsigned int at = 0u; (at < lefts) && !s_channel.dry && !s_channel.stopped; at += 1u)
    {
        run_channel_kept(left[at]);
    }
    run_channel_traced(left, lefts, s_channel.dry ? "written dry" : "carried");
    free(left);
    return recorded + carried;
}

// the questions of `asked` carried as run_channel_ask_many carries them, R aside
unsigned int run_channel_carried_many(RunQuestion *const *asked, unsigned int count)
{
    unsigned int answered = 0u;
    if (s_channel.open && s_channel.dry)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            asked[at]->refused[0] = '\0';
            run_channel_dry_written(asked[at]);
        }
        return 0u;
    }
    if (!s_channel.open)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            asked[at]->outcome = RUN_NO_CHANNEL;
            snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the channel is not open");
        }
        return 0u;
    }
    char list_name[] = "questions.txt";
    // each process carries the questions from `first` on, and the next carries those after the first it left
    // unanswered. The airlock reads the whole round once, before the first process: the held list it hands holds for
    // every process after, each carrying questions it read
    unsigned int first = 0u;
    int airlocked = 0;
    while (first < count)
    {
        if (run_channel_on_line() != 0ull)
        {
            run_channel_halted(asked, first, count);
            return answered;
        }
        int written = 1;
        for (unsigned int at = first; written && (at < count); at += 1u)
        {
            char code_name[RUN_NAME_LONGEST];
            char cases_name[RUN_NAME_LONGEST];
            char answers_name[RUN_NAME_LONGEST];
            run_channel_named(at, 0, code_name, cases_name, answers_name);
            asked[at]->refused[0] = '\0';
            written = run_channel_write(asked[at], code_name, cases_name);
        }
        written = written && run_channel_relisted(asked, first, count, 0);
        // the round checked before the part and fixed where a count of registers fixes it, where no process of it has
        // passed the check yet: one it does not pass is not carried
        if (written && !airlocked)
        {
            char refused[sizeof(asked[first]->refused)];
            const unsigned long long airlock_began = run_channel_now();
            const int checked = run_channel_checked(asked, first, count, 0, refused, sizeof(refused));
            s_channel.airlock_nanoseconds += run_channel_now() - airlock_began;
            if (!checked)
            {
                for (unsigned int at = first; at < count; at += 1u)
                {
                    asked[at]->outcome = RUN_NO_CHANNEL;
                    snprintf(asked[at]->refused, sizeof(asked[at]->refused), "%s", refused);
                }
                return answered;
            }
            airlocked = 1;
        }
        // the round handed critical, on disk before its carrier starts
        const unsigned long long after = written ? run_channel_round_handed() : 0ull;
        if (after == 0ull)
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused),
                         "the round could not be put on disk, and it is not carried");
            }
            return answered;
        }
        // the interface's command is a list of words it does not write to; the cast only meets its declared type
        char *command[RUN_CARRIER_WORDS + 4u];
        unsigned int words = 0u;
        for (; words < s_channel.words; words += 1u)
        {
            command[words] = s_channel.word[words];
        }
        command[words] = (char *)"--list";
        command[words + 1u] = list_name;
        command[words + 2u] = NULL;
        // the process is given each question's limit for every question it carries, and its output comes back
        // through a pipe
        const InterfaceProbe probe = {command, NULL, s_channel.limit_microseconds * (count - first)};
        InterfaceAnswer answer = {0};
        answer.output = s_carrier_output;
        answer.output_capacity = sizeof(s_carrier_output);
        EngineError error = {0};
        // what this process handed moved into the run before the carrier reads it, and what it handed once it ends
        qry_moved(s_channel.buffer);
        const unsigned long long carrier_began = run_channel_now();
        const long ran = interface_probe_run(&probe, &answer, &error);
        s_channel.carried_nanoseconds += run_channel_now() - carrier_began;
        qry_settled(s_channel.buffer);
        run_channel_output_handed(&answer);
        if ((ran != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier %.96s was not started",
                         s_channel.word[0]);
            }
            return answered;
        }
        // every question the process answered, in order, up to the first it left unanswered: an answer is one the
        // carrier handed after the round
        unsigned int at = first;
        const unsigned int answered_before = answered;
        for (; at < count; at += 1u)
        {
            char answers_name[RUN_NAME_LONGEST];
            snprintf(answers_name, sizeof(answers_name), "answers%u.txt", at);
            const unsigned char *bytes = NULL;
            unsigned long long size = 0ull;
            unsigned long long sequence = 0ull;
            if (!qry_latest(s_channel.buffer, answers_name, &bytes, &size, &sequence) || (sequence <= after))
            {
                break;
            }
            run_channel_read(asked[at], answers_name, after);
            answered += (asked[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
        }
        // the first question left unanswered by a process that did not end clean ended it, and its ending is its
        // answer; one left by a process that ended clean was left for a refusal before it, and is
        // carried again
        const int clean = (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ull);
        const unsigned int unanswered = at;
        if ((at < count) && !clean)
        {
            asked[at]->outcome =
                (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL : (unsigned int)RUN_NOTHING;
            snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier %s, code %llx",
                     interface_ending_name(answer.ending), answer.code);
            // a carrier ended, or one that left a panic on the line, ended carrying this question: a conference
            if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ull))
            {
                run_channel_conference_called(asked[at], asked[at]->refused, NULL, 0ull);
            }
            at += 1u;
        }
        // a panic on the line: the teacher's account handed, the question the carrier was carrying the one its ending
        // answered or, where it ended clean, the last it answered, its refusal; and no question is put after it
        if (run_channel_on_line() != 0ull)
        {
            const unsigned int carrying = (!clean || (unanswered == first)) ? unanswered : (unanswered - 1u);
            char line[RUN_LINE_LONGEST];
            run_channel_list_line(carrying - first, line);
            run_channel_teacher_said(count - first, after, &answer, answered - answered_before, line);
            run_channel_halted(asked, at, count);
            return answered;
        }
        if (at == first)
        {
            // a process that answered nothing and ended clean carried nothing, and the questions after are not carried
            // again
            for (; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier answered none of the questions");
            }
        }
        first = at;
    }
    return answered;
}
