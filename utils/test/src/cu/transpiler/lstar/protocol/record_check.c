// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record_check.c: R held to what the .kqr says of it, on a record this check writes, and on a part's own record
//
//   found      an ask R holds is found by its identity, with the word of every case
//   timed out  an ask that timed out is no answer, and the answer that resolves it takes its place
//   unchanged  a record nothing was added to is written back as it was read, every line
//   added      what a run puts is added after every line held, under one cycle numbered after the record's own
//   a part's   every ask of the record named on the command line, where one is, is found by its identity
//
//     record_check <folder> [<a part's .kqr>]
#include "../../../../../../../src/cu/transpiler/lstar/protocol/record_R/record.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// the whole file at `path` into `text`, which holds `room`: its length, or 0 where it did not read
static size_t file_read(const char *path, char *text, size_t room)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0u;
    }
    const size_t read = fread(text, 1u, room - 1u, file);
    fclose(file);
    text[read] = '\0';
    return read;
}

static const char *const s_written =
    "kqr part\n"
    "cycle 1 queue\n"
    "ask 00000000000000aa 12 0 0 0 3 00000000000000bb answers 1 2 ffffffff\n"
    "ask 00000000000000ac 12 0 0 0 2 00000000000000bb illegal ILLEGAL_INSTRUCTION\n"
    "ask 00000000000000ad 12 0 0 0 2 00000000000000bb timed_out the carrier out of time\n"
    "cycle 2 pair\n"
    "seed 1\n"
    "round 1\n"
    "sample 00000000000000ae 12 256 4096 100 648 00000000000000bb answers 1800000\n"
    "pair word_add word_sub 0123456789abcdef closed 1006:0040 1,1,0->2\n";

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("record_check <folder> [<a part's .kqr>]\n");
        return 2;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/record_check.kqr", words[1]);
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", path);
        return 2;
    }
    fputs(s_written, file);
    fclose(file);

    static RecordAsk s_held;
    check_that(record_open(path, "part", "check") == 1, "the record opens");
    check_that(record_answer("00000000000000aa 12 0 0 0 3 00000000000000bb", &s_held) == 1, "an ask held is found");
    check_that((strcmp(s_held.answer, "answers") == 0) && (s_held.words == 3u) && (s_held.word[2] == 0xffffffffull),
               "with the word of every case");
    check_that(record_answer("00000000000000ac 12 0 0 0 2 00000000000000bb", &s_held) == 1, "an illegal ask is found");
    check_that((strcmp(s_held.answer, "illegal") == 0) && (strcmp(s_held.refusal, "ILLEGAL_INSTRUCTION") == 0),
               "with its refusal");
    check_that(record_answer("00000000000000ad 12 0 0 0 2 00000000000000bb", &s_held) == 0,
               "an ask that timed out is no answer");
    check_that(record_answer("00000000000000ff 12 0 0 0 2 00000000000000bb", &s_held) == 0, "an ask not held is none");
    check_that(record_close() == 1, "the record closes");

    static char s_text[1u << 16u];
    file_read(path, s_text, sizeof(s_text));
    check_that(strcmp(s_text, s_written) == 0, "a record nothing was added to is as it was read");

    check_that(record_open(path, "part", "check") == 1, "the record opens again");
    RecordAsk resolved;
    memset(&resolved, 0, sizeof(resolved));
    snprintf(resolved.answer, sizeof(resolved.answer), "answers");
    resolved.word[0] = 7ull;
    resolved.word[1] = 9ull;
    resolved.words = 2u;
    record_keep("00000000000000ad 12 0 0 0 2 00000000000000bb", &resolved);
    record_keep("00000000000000aa 12 0 0 0 3 00000000000000bb", &resolved);
    record_keep("00000000000000f0 12 0 0 0 2 00000000000000bb", &resolved);
    record_sample("00000000000000f1 12 256 4096 100 648 00000000000000bb", "answers", 42ull, 0ull, 0ull, "");
    record_sample("00000000000000f2 12 256 4096 100 648 00000000000000bb", "answers", 300ull, 2ull, 5ull, "");
    check_that(record_answer("00000000000000ad 12 0 0 0 2 00000000000000bb", &s_held) == 1,
               "the answer that resolves a timed out ask takes its place");
    check_that(record_answer("00000000000000aa 12 0 0 0 3 00000000000000bb", &s_held) && (s_held.words == 3u),
               "an ask held that did not time out keeps its answer");
    check_that(record_answer("00000000000000f0 12 0 0 0 2 00000000000000bb", &s_held) && (s_held.word[1] == 9ull),
               "a new ask is held");
    check_that(record_close() == 1, "the record is written back");
    file_read(path, s_text, sizeof(s_text));
    check_that(strstr(s_text, "ask 00000000000000ad 12 0 0 0 2 00000000000000bb answers 7 9\n") != NULL,
               "the resolved ask stands where the timed out one stood");
    check_that(strstr(s_text, "cycle 3 check\nask 00000000000000f0 12 0 0 0 2 00000000000000bb answers 7 9\n"
                              "sample 00000000000000f1 12 256 4096 100 648 00000000000000bb answers 42\n") != NULL,
               "what this run put comes after one cycle numbered after the record's own");
    check_that(strstr(s_text, "sample 00000000000000f2 12 256 4096 100 648 00000000000000bb answers 300 2 5\n") != NULL,
               "a sample with its launches' least and most keeps both after the time they took together");
    check_that(strstr(s_text, "seed 1\nround 1\n") != NULL, "every other line is kept as it was");
    check_that(strstr(s_text, "pair word_add word_sub 0123456789abcdef closed 1006:0040 1,1,0->2\n") != NULL,
               "and a pair's path with it");

    if (count >= 3)
    {
        FILE *const part = fopen(words[2], "rb");
        unsigned int asks = 0u;
        unsigned int found = 0u;
        static char s_line[1u << 20u];
        check_that(part != NULL, "a part's record reads");
        check_that(record_open(words[2], "part", "check") == 1, "a part's record opens");
        while ((part != NULL) && (fgets(s_line, sizeof(s_line), part) != NULL))
        {
            if (strncmp(s_line, "ask ", 4u) != 0)
            {
                continue;
            }
            char identity[160];
            const char *at = s_line + 4;
            const char *end = at;
            for (unsigned int word = 0u; (word < 7u) && (end != NULL); word += 1u)
            {
                end = strchr(end, ' ');
                end += ((end != NULL) && (word < 6u)) ? 1 : 0;
            }
            if ((end == NULL) || ((size_t)(end - at) >= sizeof(identity)))
            {
                continue;
            }
            memcpy(identity, at, (size_t)(end - at));
            identity[end - at] = '\0';
            asks += 1u;
            const int answered = record_answer(identity, &s_held);
            found += (answered || (strstr(s_line, " timed_out") != NULL)) ? 1u : 0u;
        }
        if (part != NULL)
        {
            fclose(part);
        }
        record_close();
        printf("  %s: %u asks, %u found by their identity\n", words[2], asks, found);
        check_that((asks != 0u) && (found == asks), "every ask of a part's record is found by its identity");
    }

    printf("record_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
