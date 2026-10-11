// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scheduler_check.c: a round held to P13 on the run channel's dry carrier, which writes each question it is handed in
// the order it is handed them and answers none
//
//   order      the question that settles the most entries is put first, and two that settle as many in the order
//              handed
//   all        every question of a round is put, a round past SCHEDULER_ROUND_MOST to more than one process
//   held       a question nothing carried answers nothing, and the round counts none answered
//
//     scheduler_check <folder>
//
// The dry channel hands every question to the run's buffer (qry_buffer.h); the check makes that buffer itself in
// <folder>, with no writer process, as a dry channel hands nothing critical, and reads the round back from it
#if !defined(_WIN32)
// setenv is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../../../../../src/cu/transpiler/lstar/protocol/order/scheduler.h"
#include "../../../../../../../src/cu/types/file_defs/qry/qry_buffer.h"

#include <stdio.h>
#include <stdlib.h>
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

#define CHECK_QUESTIONS 300u

int main(int count, char **words)
{
    if (count != 2)
    {
        printf("scheduler_check <folder>\n");
        return 2;
    }
    // the run's buffer, made here, named to the channel as a run names it
    char path[1024];
    snprintf(path, sizeof(path), "%s/scheduler_check.qry", words[1]);
    QryBuffer *const run = qry_create(path);
    check_that(run != NULL, "the run's buffer is made");
#if defined(_WIN32)
    _putenv_s(QRY_ENVIRONMENT, path);
#else
    setenv(QRY_ENVIRONMENT, path, 1);
#endif
    const char *const carrier[] = {"dry", NULL};
    check_that(run_channel_open(carrier, 1000000ull) == 1, "the dry channel opens");
    RunQuestion *const pool = (RunQuestion *)calloc(CHECK_QUESTIONS, sizeof(RunQuestion));
    if (pool == NULL)
    {
        printf("  no room for the questions\n");
        return 2;
    }
    static unsigned char s_code[CHECK_QUESTIONS][16];
    RunQuestion *question[CHECK_QUESTIONS];
    unsigned int settles[CHECK_QUESTIONS];
    // each question's code its own number, so that the order written says which was put when
    for (unsigned int at = 0u; at < CHECK_QUESTIONS; at += 1u)
    {
        s_code[at][0] = (unsigned char)(at & 0xffu);
        s_code[at][1] = (unsigned char)(at >> 8u);
        pool[at].code = s_code[at];
        pool[at].code_size = 16ull;
        pool[at].registers = 8u;
        pool[at].cases = 1u;
        question[at] = &pool[at];
        // every seventh question settles three entries, the rest one
        settles[at] = ((at % 7u) == 0u) ? 3u : 1u;
    }
    const unsigned int answered = scheduler_round(question, settles, CHECK_QUESTIONS);
    run_channel_close();
    check_that(answered == 0u, "a round nothing carried answers nothing");
    unsigned int held = 0u;
    for (unsigned int at = 0u; at < CHECK_QUESTIONS; at += 1u)
    {
        held += (pool[at].outcome == RUN_HELD) ? 1u : 0u;
    }
    check_that(held == CHECK_QUESTIONS, "every question of the round is put, and reads held");

    // the order the dry channel handed them in, read back off each question's code in the run's buffer
    unsigned int written[CHECK_QUESTIONS];
    unsigned int written_count = 0u;
    const unsigned char *list = NULL;
    unsigned long long list_size = 0ull;
    const int listed = qry_latest(run, "questions.txt", &list, &list_size, NULL);
    check_that(listed, "the dry channel handed its list to the run's buffer");
    unsigned long long at_line = 0ull;
    while (listed && (at_line < list_size) && (written_count < CHECK_QUESTIONS))
    {
        const unsigned char *const line_end =
            (const unsigned char *)memchr(list + at_line, '\n', (size_t)(list_size - at_line));
        const size_t line_length =
            (line_end != NULL) ? (size_t)(line_end - (list + at_line)) : (size_t)(list_size - at_line);
        char line[256];
        const size_t kept = (line_length < (sizeof(line) - 1u)) ? line_length : (sizeof(line) - 1u);
        memcpy(line, list + at_line, kept);
        line[kept] = '\0';
        at_line += (unsigned long long)line_length + 1ull;
        char code_name[256];
        const unsigned char *code = NULL;
        unsigned long long code_size = 0ull;
        if ((sscanf(line, "%255s", code_name) == 1) && qry_latest(run, code_name, &code, &code_size, NULL) &&
            (code_size >= 2ull))
        {
            written[written_count] = (unsigned int)code[0] | ((unsigned int)code[1] << 8u);
            written_count += 1u;
        }
    }
    check_that(written_count == CHECK_QUESTIONS, "the dry channel handed every question of the round");
    // within each process the three-settling questions come first, in the order handed, then the rest
    int ordered = 1;
    for (unsigned int first = 0u; first < written_count; first += SCHEDULER_ROUND_MOST)
    {
        const unsigned int last = ((first + SCHEDULER_ROUND_MOST) < written_count) ? (first + SCHEDULER_ROUND_MOST)
                                                                                    : written_count;
        for (unsigned int at = first + 1u; at < last; at += 1u)
        {
            const unsigned int before = written[at - 1u];
            const unsigned int now = written[at];
            const unsigned int before_weight = ((before % 7u) == 0u) ? 3u : 1u;
            const unsigned int now_weight = ((now % 7u) == 0u) ? 3u : 1u;
            ordered = ordered && ((before_weight > now_weight) || ((before_weight == now_weight) && (before < now)));
        }
    }
    check_that(ordered, "the most settling first, and the order handed between equals");
    qry_close(run);
    free(pool);
    printf("scheduler_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
