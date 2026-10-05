// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_channel.c: each question written to the channel's folder and carried by the carrier in a process of its own,
// and the line the carrier wrote read back as the question's answer
#include "run_channel.h"

#include "../interface/interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most words that start a carrier, the longest path the channel names, and the longest line a carrier answers
#define RUN_CARRIER_WORDS 8u
#define RUN_PATH_LONGEST 1024u
// the longest name of a file the channel keeps in its folder, with the slash before it
#define RUN_NAME_LONGEST 16u
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

// the line the carrier wrote, read into `asked`: the outcome it names, and each case's answer where it answered
static void run_channel_read(RunQuestion *asked, const char *answers_path)
{
    asked->outcome = RUN_NOTHING;
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
    run_channel_path(code_path, "question.bin");
    run_channel_path(cases_path, "cases.txt");
    run_channel_path(answers_path, "answers.txt");
    run_channel_path(output_path, "carrier.txt");
    snprintf(registers, sizeof(registers), "%u", asked->registers);
    remove(answers_path);
    if (!run_channel_write(asked, code_path, cases_path))
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the question could not be written to %.80s", s_channel.folder);
        return 0;
    }
    // the interface's command is a list of words it does not write to; the cast only meets its declared type
    char *command[RUN_CARRIER_WORDS + 5u];
    unsigned int words = 0u;
    for (; words < s_channel.words; words += 1u)
    {
        command[words] = s_channel.word[words];
    }
    command[words] = code_path;
    command[words + 1u] = registers;
    command[words + 2u] = cases_path;
    command[words + 3u] = answers_path;
    command[words + 4u] = NULL;
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

void run_channel_close(void)
{
    memset(&s_channel, 0, sizeof(s_channel));
}

const char *run_channel_carrier(void)
{
    return s_channel.open ? s_channel.word[0] : "none";
}
