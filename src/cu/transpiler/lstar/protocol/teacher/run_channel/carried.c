// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carried.c: the round carried: one question or many passed critical, carried by the carrier and answered
#include "run_channel_internal.h"

#include <time.h>

unsigned long long run_channel_now(void)
{
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) != TIME_UTC)
    {
        return 0ULL;
    }
    // a clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * 1000000000ULL) + (unsigned long long)now.tv_nsec;
}

void run_channel_clocks(unsigned long long *airlock, unsigned long long *pop, unsigned long long *cross,
                        unsigned long long *carried)
{
    *airlock = s_channel.airlock_nanoseconds;
    *pop = s_channel.pop_nanoseconds;
    *cross = s_channel.cross_nanoseconds;
    *carried = s_channel.carried_nanoseconds;
}

// `question` carried as run_channel_put carries it, R aside
int run_channel_carried(RunQuestion *question)
{
    if (run_channel_panic_ruled())
    {
        RunQuestion *const one[1] = {question};
        run_channel_halted(one, 0u, 1u);
        return 0;
    }
    if (s_channel.open && s_channel.dry)
    {
        run_channel_dry_written(question);
        return 0;
    }
    if (!s_channel.open)
    {
        question->outcome = RUN_NO_CHANNEL;
        run_channel_refused(question, "the channel is not open");
        return 0;
    }
    if (run_channel_on_line() != 0ULL)
    {
        RunQuestion *const one[1] = {question};
        run_channel_halted(one, 0u, 1u);
        return 0;
    }
    char code_name[] = "question.bin";
    char cases_name[] = "cases.txt";
    char answers_name[] = "answers.txt";
    char registers[16];
    char place_text[16];
    char launches[16];
    char threads[16];
    char blocks[16];
    char line[RUN_LINE_MAX_LEN];
    run_channel_counted(place_text, sizeof(place_text), question->place);
    run_channel_counted(launches, sizeof(launches), question->launches);
    run_channel_counted(threads, sizeof(threads), question->threads);
    run_channel_counted(blocks, sizeof(blocks), question->blocks);
    if (!run_channel_write(question, code_name, cases_name))
    {
        question->outcome = RUN_NO_CHANNEL;
        run_channel_refused(question, "the question could not be passed to the run");
        return 0;
    }
    // the round, this one question, checked before the part, and fixed where a count of registers fixes it. Then the
    // call passes its line as critical before the carrier starts
    RunQuestion *const one[1] = {question};
    const int listed = run_channel_relisted(one, 0u, 1u, 1);
    const unsigned long long airlock_began = run_channel_now();
    const int checked = listed ? run_channel_checked(one, 0u, 1u, 1, question->refused, sizeof(question->refused)) : 1;
    s_channel.airlock_nanoseconds += run_channel_now() - airlock_began;
    if (!checked)
    {
        question->outcome = RUN_NO_CHANNEL;
        return 0;
    }
    // the count of registers that the check left the question, which the call passes to the carrier
    run_channel_list_line(0u, line);
    run_channel_counted(registers, sizeof(registers), question->registers);
    const unsigned long long after = listed ? run_channel_round_passed() : 0ULL;
    if (after == 0ULL)
    {
        question->outcome = RUN_NO_CHANNEL;
        run_channel_refused(question, "the round could not be put on disk, and it is not carried");
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
    command[words + 4u] = place_text;
    words += 5u;
    // an untimed question with no shape of its own goes to the carrier with the words that it always had. A timed one
    // also has its count of launches after. One with a shape has its count of launches, 0 where the question times no
    // launches, and its shape
    const int shaped = (question->threads != 0u) || (question->blocks != 0u);
    if ((question->launches != 0u) || shaped)
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
    // the output of the carrier comes back through a pipe, and the call makes no file
    const InterfaceProbe probe = {command, NULL, s_channel.limit_microseconds};
    InterfaceAnswer answer = {0};
    answer.output = s_carrier_output;
    answer.output_capacity = sizeof(s_carrier_output);
    EngineError error = {0};
    // what this process passed moved into the run before the carrier reads it, and what it passed once it ends
    qry_member_settled();
    const unsigned long long carrier_began = run_channel_now();
    const long ran = interface_probe_run(&probe, &answer, &error);
    s_channel.carried_nanoseconds += run_channel_now() - carrier_began;
    qry_member_rings_settled();
    run_channel_output_passed(&answer);
    // a panic on the line, the carrier's or its ending's: the teacher's account passed
    run_channel_panic_written(1u, after, &answer, 0u, line);
    if ((ran != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
    {
        question->outcome = RUN_NO_CHANNEL;
        run_channel_started_refused(question);
        return 0;
    }
    if ((answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ULL))
    {
        // a carrier that did not end clean wrote no answer worth a read, and its ending is the answer. Where it did not
        // end by itself, or a panic is on the line, it ended while it carried this question, which calls a conference
        question->outcome =
            (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL : (unsigned int)RUN_NOTHING;
        run_channel_ending_refused(question, &answer);
        if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ULL))
        {
            run_channel_conference_called(question, question->refused, NULL, 0ULL);
        }
        return 0;
    }
    s_channel.carrier_clean = 1;
    run_channel_read(question, answers_name, after);
    return question->outcome == RUN_ANSWERED;
}

// The questions of `questions`, `count` of them and at max RUN_QUESTIONS_MAX, queried as run_channel_put_many queries
// them
static unsigned int run_channel_put_some(RunQuestion *const *questions, unsigned int count)
{
    // the questions R answers taken first, and only the rest carried, each stored in R once it comes back
    static RunQuestion *s_left[RUN_QUESTIONS_MAX];
    RunQuestion **const left = s_left;
    // a stopped run carries nothing and answers nothing from R. The call blocks every question with the reason of the
    // stop
    if (run_channel_panic_ruled())
    {
        run_channel_halted(questions, 0u, count);
        return 0u;
    }
    unsigned int recorded = 0u;
    unsigned int lefts = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        questions[at]->refused[0] = '\0';
        if (run_channel_recorded(questions[at]))
        {
            recorded += (questions[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
            RunQuestion *const one[1] = {questions[at]};
            run_channel_traced(one, 1u, "from R");
            continue;
        }
        left[lefts] = questions[at];
        lefts += 1u;
    }
    const unsigned int carried = (lefts != 0u) ? run_channel_carried_many(left, lefts) : 0u;
    for (unsigned int at = 0u; (at < lefts) && !s_channel.dry && !s_channel.stopped; at += 1u)
    {
        run_channel_stored(left[at]);
    }
    run_channel_traced(left, lefts, s_channel.dry ? "written dry" : "carried");
    return recorded + carried;
}

unsigned int run_channel_put_many(RunQuestion *const *questions, unsigned int count)
{
    unsigned int answered = 0u;
    for (unsigned int first = 0u; first < count; first += RUN_QUESTIONS_MAX)
    {
        const unsigned int some = ((count - first) < RUN_QUESTIONS_MAX) ? (count - first) : RUN_QUESTIONS_MAX;
        answered += run_channel_put_some(questions + first, some);
    }
    return answered;
}

// the questions of `questions` carried as run_channel_put_many carries them, R aside
unsigned int run_channel_carried_many(RunQuestion *const *questions, unsigned int count)
{
    unsigned int answered = 0u;
    if (!s_channel.open)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            questions[at]->outcome = RUN_NO_CHANNEL;
            run_channel_refused(questions[at], "the channel is not open");
        }
        return 0u;
    }
    char list_name[] = "questions.txt";
    // each process carries the questions from `first` on, and the next carries those after the first that it left
    // unanswered. The airlock reads the whole round once, before the first process. The gated list that it passes
    // covers every process after, each with questions that it read
    unsigned int first = 0u;
    int airlocked = 0;
    while (first < count)
    {
        // a panic on the line, or one ruled on inside this round. The call blocks the questions that remain, and starts
        // no process after
        if ((run_channel_on_line() != 0ULL) || run_channel_panic_ruled())
        {
            run_channel_halted(questions, first, count);
            return answered;
        }
        int written = 1;
        for (unsigned int at = first; written && (at < count); at += 1u)
        {
            char code_name[RUN_NAME_MAX_LEN];
            char cases_name[RUN_NAME_MAX_LEN];
            char answers_name[RUN_NAME_MAX_LEN];
            run_channel_named(at, 0, code_name, cases_name, answers_name);
            questions[at]->refused[0] = '\0';
            written = run_channel_write(questions[at], code_name, cases_name);
        }
        written = written && run_channel_relisted(questions, first, count, 0);
        // the round, checked before the part and fixed where a count of registers fixes it, where no process of it
        // passed the check yet. The call does not carry a question that does not pass
        if (written && !airlocked)
        {
            char refused[sizeof(questions[first]->refused)];
            const unsigned long long airlock_began = run_channel_now();
            const int checked = run_channel_checked(questions, first, count, 0, refused, sizeof(refused));
            s_channel.airlock_nanoseconds += run_channel_now() - airlock_began;
            if (!checked)
            {
                for (unsigned int at = first; at < count; at += 1u)
                {
                    questions[at]->outcome = RUN_NO_CHANNEL;
                    run_channel_refused(questions[at], refused);
                }
                return answered;
            }
            airlocked = 1;
        }
        // a dry channel: the round passed the airlock as a live round does, and nothing carries it
        if (s_channel.dry)
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                questions[at]->outcome = RUN_GATED;
                run_channel_refused(questions[at], "dry: written, carried by nothing");
            }
            s_channel.dry_written += count - first;
            return answered;
        }
        // the round passed critical, on disk before its carrier starts
        const unsigned long long after = written ? run_channel_round_passed() : 0ULL;
        if (after == 0ULL)
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                questions[at]->outcome = RUN_NO_CHANNEL;
                run_channel_refused(questions[at], "the round could not be put on disk, and it is not carried");
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
        // the call gives the process the limit of each question for every question that it carries, and its output
        // comes back through a pipe
        const InterfaceProbe probe = {command, NULL, s_channel.limit_microseconds * (count - first)};
        InterfaceAnswer answer = {0};
        answer.output = s_carrier_output;
        answer.output_capacity = sizeof(s_carrier_output);
        EngineError error = {0};
        // what this process passed moved into the run before the carrier reads it, and what it passed once it ends
        qry_member_settled();
        const unsigned long long carrier_began = run_channel_now();
        const long ran = interface_probe_run(&probe, &answer, &error);
        s_channel.carried_nanoseconds += run_channel_now() - carrier_began;
        qry_member_rings_settled();
        run_channel_output_passed(&answer);
        if ((ran != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                questions[at]->outcome = RUN_NO_CHANNEL;
                run_channel_started_refused(questions[at]);
            }
            return answered;
        }
        // every question the process answered, in order, up to the first it left unanswered: an answer is one the
        // carrier passed after the round
        unsigned int at = first;
        const unsigned int answered_before = answered;
        for (; at < count; at += 1u)
        {
            char answers_name[RUN_NAME_MAX_LEN];
            char code_name[RUN_NAME_MAX_LEN];
            char cases_name[RUN_NAME_MAX_LEN];
            run_channel_named(at, 0, code_name, cases_name, answers_name);
            const unsigned char *bytes = NULL;
            unsigned long long size = 0ULL;
            unsigned long long sequence = 0ULL;
            if (!qry_member_blob(answers_name, &bytes, &size, &sequence) || (sequence < after))
            {
                break;
            }
            s_channel.carrier_clean = (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ULL);
            run_channel_read(questions[at], answers_name, after);
            answered += (questions[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
        }
        // where a process did not end clean, the first question that it left unanswered ended it, and its ending is
        // its answer. Where a process ended clean, a question that it left waited on a refusal before it, and the call
        // carries it again
        const int clean = (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ULL);
        const unsigned int unanswered = at;
        if ((at < count) && !clean)
        {
            questions[at]->outcome =
                (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL : (unsigned int)RUN_NOTHING;
            run_channel_ending_refused(questions[at], &answer);
            // a carrier that something ended, or one that left a panic on the line, ended while it carried this
            // question: a conference
            if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ULL))
            {
                run_channel_conference_called(questions[at], questions[at]->refused, NULL, 0ULL);
            }
            at += 1u;
        }
        // a panic on the line, or one ruled on as the call read the answers. The call passes the account of the
        // teacher where the line is open. The carrier carried the question that its ending answered.
        // Where it ended clean, it is the last that it answered, with its refusal. The call puts no question after it
        if ((run_channel_on_line() != 0ULL) || run_channel_panic_ruled())
        {
            const unsigned int carrying = (!clean || (unanswered == first)) ? unanswered : (unanswered - 1u);
            char line[RUN_LINE_MAX_LEN];
            run_channel_list_line(carrying - first, line);
            run_channel_panic_written(count - first, after, &answer, answered - answered_before, line);
            run_channel_halted(questions, at, count);
            return answered;
        }
        if (at == first)
        {
            // a process that answered nothing and ended clean carried nothing, and the call does not carry the
            // questions after it again
            for (; at < count; at += 1u)
            {
                questions[at]->outcome = RUN_NO_CHANNEL;
                run_channel_refused(questions[at], "the carrier answered none of the questions");
            }
        }
        first = at;
    }
    return answered;
}
