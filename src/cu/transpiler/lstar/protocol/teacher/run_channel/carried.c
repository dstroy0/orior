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
    if (run_channel_on_line() != 0ull)
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
    // the round, this one question, checked before the part and fixed where a count of registers fixes it, then its
    // line passed critical before the carrier starts
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
    // the count of registers the check left the question, which the carrier is passed
    run_channel_list_line(0u, line);
    run_channel_counted(registers, sizeof(registers), question->registers);
    const unsigned long long after = listed ? run_channel_round_passed() : 0ull;
    if (after == 0ull)
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
    // an untimed question of no shape of its own is carried with the words it always was; a timed one with its count
    // of launches after, and one with a shape with its count of launches, 0 where it is untimed, and its shape
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
    // the carrier's output comes back through a pipe, and no file is made
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
    if ((answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        // a carrier that did not end clean wrote no answer worth reading: its ending is the answer, and where it was
        // ended, or a panic is on the line, it ended carrying this question, which calls a conference
        question->outcome =
            (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL : (unsigned int)RUN_NOTHING;
        run_channel_ending_refused(question, &answer);
        if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ull))
        {
            run_channel_conference_called(question, question->refused, NULL, 0ull);
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
    // a stopped run carries nothing and answers nothing from R: every question is blocked with the stop's reason
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
    // each process carries the questions from `first` on, and the next carries those after the first it left
    // unanswered. The airlock reads the whole round once, before the first process: the gated list it passes covers
    // every process after, each carrying questions it read
    unsigned int first = 0u;
    int airlocked = 0;
    while (first < count)
    {
        // a panic on the line, or one ruled on inside this round: the questions left are blocked, and no process after
        if ((run_channel_on_line() != 0ull) || run_channel_panic_ruled())
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
        // the round checked before the part and fixed where a count of registers fixes it, where no process of it has
        // passed the check yet: one it does not pass is not carried
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
        // a dry channel: the round has passed the airlock as a live round does, and nothing carries it
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
        const unsigned long long after = written ? run_channel_round_passed() : 0ull;
        if (after == 0ull)
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
        // the process is given each question's limit for every question it carries, and its output comes back
        // through a pipe
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
            unsigned long long size = 0ull;
            unsigned long long sequence = 0ull;
            if (!qry_member_blob(answers_name, &bytes, &size, &sequence) || (sequence < after))
            {
                break;
            }
            s_channel.carrier_clean = (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ull);
            run_channel_read(questions[at], answers_name, after);
            answered += (questions[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
        }
        // the first question left unanswered by a process that did not end clean ended it, and its ending is its
        // answer; one left by a process that ended clean was left for a refusal before it, and is
        // carried again
        const int clean = (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ull);
        const unsigned int unanswered = at;
        if ((at < count) && !clean)
        {
            questions[at]->outcome =
                (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL : (unsigned int)RUN_NOTHING;
            run_channel_ending_refused(questions[at], &answer);
            // a carrier ended, or one that left a panic on the line, ended carrying this question: a conference
            if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ull))
            {
                run_channel_conference_called(questions[at], questions[at]->refused, NULL, 0ull);
            }
            at += 1u;
        }
        // a panic on the line, or one ruled on as the answers were read: the teacher's account passed where the line is
        // open, the question the carrier was carrying the one its ending answered or, where it ended clean, the last
        // it answered, its refusal; and no question is put after it
        if ((run_channel_on_line() != 0ull) || run_channel_panic_ruled())
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
            // a process that answered nothing and ended clean carried nothing, and the questions after are not carried
            // again
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
