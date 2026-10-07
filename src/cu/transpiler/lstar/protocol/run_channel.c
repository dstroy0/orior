// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_channel.c: each question written to the channel's folder and carried by the carrier in a process of its own, or
// many in one, and the line the carrier wrote for each read back as that question's answer
#include "run_channel.h"

#include "../interface/interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most words that start a carrier, the longest path the channel names, and the longest line a carrier answers
#define RUN_CARRIER_WORDS 8u
#define RUN_PATH_LONGEST 1024u
// the longest name of a file the channel keeps in its folder, with the slash before it
#define RUN_NAME_LONGEST 32u
#define RUN_ANSWER_LONGEST (16u + (RUN_CASES_MOST * 17u))

typedef struct
{
    char word[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int words;
    char folder[RUN_PATH_LONGEST];
    unsigned long long limit_microseconds;
    int open;
} RunChannel;

static RunChannel s_channel;
static char s_answer_line[RUN_ANSWER_LONGEST];
static char s_carrier_output[4096];

int run_channel_open(const char *const *carrier, const char *folder, unsigned long long limit_microseconds)
{
    memset(&s_channel, 0, sizeof(s_channel));
    unsigned int words = 0u;
    while ((carrier[words] != NULL) && (words < RUN_CARRIER_WORDS))
    {
        snprintf(s_channel.word[words], RUN_PATH_LONGEST, "%s", carrier[words]);
        words += 1u;
    }
    if ((words == 0u) || (carrier[words] != NULL))
    {
        printf("  run_channel: a carrier is from 1 to %u words\n", RUN_CARRIER_WORDS);
        return 0;
    }
    s_channel.words = words;
    snprintf(s_channel.folder, sizeof(s_channel.folder), "%s", folder);
    s_channel.limit_microseconds = limit_microseconds;
    s_channel.open = 1;
    return 1;
}

// `name` in the channel's folder, into `path`, which holds RUN_PATH_LONGEST and the longest name the channel gives
static void run_channel_path(char *path, const char *name)
{
    snprintf(path, RUN_PATH_LONGEST + RUN_NAME_LONGEST, "%s/%s", s_channel.folder, name);
}

// the question's code and cases written to the channel's folder: 1, or 0 where either could not be written
static int run_channel_write(const RunQuestion *asked, const char *code_path, const char *cases_path)
{
    FILE *const code = fopen(code_path, "wb");
    if (code == NULL)
    {
        return 0;
    }
    const int code_written = (fwrite(asked->code, 1u, (size_t)asked->code_size, code) == asked->code_size);
    fclose(code);
    FILE *const cases = fopen(cases_path, "wb");
    if (cases == NULL)
    {
        return 0;
    }
    for (unsigned int place = 0u; place < asked->cases; place += 1u)
    {
        for (unsigned int at = 0u; at < RUN_IN_WORDS; at += 1u)
        {
            fprintf(cases, (at == 0u) ? "%x" : " %x", asked->word[place][at]);
        }
        fprintf(cases, "\n");
    }
    return (fclose(cases) == 0) && code_written;
}

// the lines the carrier wrote, read into `asked`: the outcome the first names, each case's answer where it answered,
// and where the question is timed, the time the second gives. A timed question the carrier gives no time for answered
// nothing worth reading
static void run_channel_read(RunQuestion *asked, const char *answers_path)
{
    asked->outcome = RUN_NOTHING;
    asked->nanoseconds = 0ull;
    FILE *const answers = fopen(answers_path, "rb");
    if ((answers == NULL) || (fgets(s_answer_line, sizeof(s_answer_line), answers) == NULL))
    {
        snprintf(asked->refused, sizeof(asked->refused), "the carrier wrote no answer");
        if (answers != NULL)
        {
            fclose(answers);
        }
        return;
    }
    char timed_line[64];
    unsigned int launches = 0u;
    const int timed = (fgets(timed_line, sizeof(timed_line), answers) != NULL) &&
                      (sscanf(timed_line, "timed %u %llu", &launches, &asked->nanoseconds) == 2) &&
                      (launches == asked->launches);
    fclose(answers);
    s_answer_line[strcspn(s_answer_line, "\r\n")] = '\0';
    if (strncmp(s_answer_line, "answered", 8u) != 0)
    {
        asked->outcome = (strncmp(s_answer_line, "skipped", 7u) == 0) ? (unsigned int)RUN_HELD : (unsigned int)RUN_ILLEGAL;
        snprintf(asked->refused, sizeof(asked->refused), "%.120s", s_answer_line);
        return;
    }
    const char *at = s_answer_line + 8u;
    for (unsigned int place = 0u; place < asked->cases; place += 1u)
    {
        char *after = NULL;
        asked->answered[place] = strtoull(at, &after, 16);
        if (after == at)
        {
            snprintf(asked->refused, sizeof(asked->refused), "the carrier answered %u of %u cases", place,
                     asked->cases);
            return;
        }
        at = after;
    }
    if ((asked->launches != 0u) && !timed)
    {
        snprintf(asked->refused, sizeof(asked->refused), "the carrier gave no time for %u launches", asked->launches);
        return;
    }
    asked->outcome = RUN_ANSWERED;
}

int run_channel_ask(RunQuestion *asked)
{
    asked->refused[0] = '\0';
    if (!s_channel.open)
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the channel is not open");
        return 0;
    }
    char code_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
    char cases_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
    char answers_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
    char output_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
    char registers[16];
    char launches[16];
    char threads[16];
    char blocks[16];
    run_channel_path(code_path, "question.bin");
    run_channel_path(cases_path, "cases.txt");
    run_channel_path(answers_path, "answers.txt");
    run_channel_path(output_path, "carrier.txt");
    snprintf(registers, sizeof(registers), "%u", asked->registers);
    snprintf(launches, sizeof(launches), "%u", asked->launches);
    snprintf(threads, sizeof(threads), "%u", asked->threads);
    snprintf(blocks, sizeof(blocks), "%u", asked->blocks);
    remove(answers_path);
    if (!run_channel_write(asked, code_path, cases_path))
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the question could not be written to %.80s", s_channel.folder);
        return 0;
    }
    // the interface's command is a list of words it does not write to; the cast only meets its declared type
    char *command[RUN_CARRIER_WORDS + 8u];
    unsigned int words = 0u;
    for (; words < s_channel.words; words += 1u)
    {
        command[words] = s_channel.word[words];
    }
    command[words] = code_path;
    command[words + 1u] = registers;
    command[words + 2u] = cases_path;
    command[words + 3u] = answers_path;
    words += 4u;
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
    const InterfaceProbe probe = {command, output_path, s_channel.limit_microseconds};
    InterfaceAnswer answer = {0};
    answer.output = s_carrier_output;
    answer.output_capacity = sizeof(s_carrier_output);
    EngineError error = {0};
    if ((interface_probe_run(&probe, &answer, &error) != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier %.96s was not started", s_channel.word[0]);
        return 0;
    }
    if ((answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        // a carrier that did not end clean wrote no answer worth reading: its ending is the answer
        asked->outcome = (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL
                                                                     : (unsigned int)RUN_NOTHING;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier %s, code %llx",
                 interface_ending_name(answer.ending), answer.code);
        return 0;
    }
    run_channel_read(asked, answers_path);
    return asked->outcome == RUN_ANSWERED;
}

unsigned int run_channel_ask_many(RunQuestion *const *asked, unsigned int count)
{
    unsigned int answered = 0u;
    if (!s_channel.open)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            asked[at]->outcome = RUN_NO_CHANNEL;
            snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the channel is not open");
        }
        return 0u;
    }
    char list_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
    char output_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
    run_channel_path(list_path, "questions.txt");
    run_channel_path(output_path, "carrier.txt");
    // each process carries the questions from `first` on, and the next carries those after the first it left
    // unanswered
    unsigned int first = 0u;
    while (first < count)
    {
        FILE *const list = fopen(list_path, "wb");
        int written = (list != NULL);
        for (unsigned int at = first; written && (at < count); at += 1u)
        {
            char name[RUN_NAME_LONGEST];
            char code_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
            char cases_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
            char answers_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
            snprintf(name, sizeof(name), "question%u.bin", at);
            run_channel_path(code_path, name);
            snprintf(name, sizeof(name), "cases%u.txt", at);
            run_channel_path(cases_path, name);
            snprintf(name, sizeof(name), "answers%u.txt", at);
            run_channel_path(answers_path, name);
            remove(answers_path);
            asked[at]->refused[0] = '\0';
            written = run_channel_write(asked[at], code_path, cases_path) &&
                      (fprintf(list, "%s %u %s %s\n", code_path, asked[at]->registers, cases_path, answers_path) > 0);
        }
        if ((list == NULL) || (fclose(list) != 0) || !written)
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the questions could not be written to %.70s",
                         s_channel.folder);
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
        command[words + 1u] = list_path;
        command[words + 2u] = NULL;
        // the process is given each question's limit for every question it carries
        const InterfaceProbe probe = {command, output_path, s_channel.limit_microseconds * (count - first)};
        InterfaceAnswer answer = {0};
        answer.output = s_carrier_output;
        answer.output_capacity = sizeof(s_carrier_output);
        EngineError error = {0};
        if ((interface_probe_run(&probe, &answer, &error) != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier %.96s was not started",
                         s_channel.word[0]);
            }
            return answered;
        }
        // every question the process answered, in order, up to the first it left unanswered
        unsigned int at = first;
        for (; at < count; at += 1u)
        {
            char name[RUN_NAME_LONGEST];
            char answers_path[RUN_PATH_LONGEST + RUN_NAME_LONGEST];
            snprintf(name, sizeof(name), "answers%u.txt", at);
            run_channel_path(answers_path, name);
            FILE *const held = fopen(answers_path, "rb");
            if (held == NULL)
            {
                break;
            }
            fclose(held);
            run_channel_read(asked[at], answers_path);
            answered += (asked[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
        }
        // the first question left unanswered by a process that did not end clean ended it, and its ending is its
        // answer; one left by a process that ended clean was left for a refusal before it, and is
        // carried again
        if ((at < count) && ((answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull)))
        {
            asked[at]->outcome =
                (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL : (unsigned int)RUN_NOTHING;
            snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier %s, code %llx",
                     interface_ending_name(answer.ending), answer.code);
            at += 1u;
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

void run_channel_close(void)
{
    memset(&s_channel, 0, sizeof(s_channel));
}

const char *run_channel_carrier(void)
{
    return s_channel.open ? s_channel.word[0] : "none";
}
