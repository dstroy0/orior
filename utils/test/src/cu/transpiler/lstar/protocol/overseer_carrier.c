// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer_carrier.c: a carrier for overseer_check that reaches no part, and the airlock's pop and cross beside it.
// Given `refuse`, it hands the question it is handed the answer the carrier hands where the part refused a question
// and stayed whole, with the accounts a refusal carries (cubin_run.c); given `hang`, it outlasts the time it is given
// and answers nothing. A `--held <name>` after either is the held list a carrier reads, and is passed over. Given
// `pop`, it pops nothing and ends clean. As the airlock's cross, the round's list put after its first word, it hands an
// empty held list given `cross`, ends as a cross that did not read given `unread`, and ends clean having handed nothing
// given `silent` (run_channel.h). Given `registers`, it hands an empty held list and holds each question of fewer than
// OVERSEER_CARRIER_CLEARING registers for a register past its count, the holds it hands the teacher naming that count;
// given `illegal`, it holds each question for an instruction no count clears. Either ends as the cross does where it
// holds some
//
//     overseer_carrier refuse|hang [--held <name>] <code> <registers> <cases> <answers> <slot>
//     overseer_carrier pop ... --list <list>
//     overseer_carrier <list> cross|unread|silent|registers|illegal <held> [<holds> [<read>]]
//
// Exit 0 where it answered, popped, crossed or hung out its time, 1 where the answer could not be handed, 2 where the
// words are none of the above or the cross did not read
#if !defined(_POSIX_C_SOURCE) && !defined(_WIN32)
// nanosleep is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../../../../../src/cu/types/file_defs/qry/qry_buffer.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

// the seconds `hang` outlasts, past any time overseer_check gives a question
#define OVERSEER_CARRIER_HANG_SECONDS 5u
// the count of registers that clears a hold the `registers` cross says, R40 the register it reads
#define OVERSEER_CARRIER_CLEARING 41u

// the refusal and its accounts, as the carrier says them after a launch the part refused
static const char s_refused[] =
    "refused CUDA_ERROR_ILLEGAL_ADDRESS\n"
    "proctor: the launch of question.bin, 32 bytes of code, 16 registers, 32 threads in 1 blocks over 1 cases, 0 "
    "launches timed after it, was refused, the part left whole: the driver's status CUDA_ERROR_ILLEGAL_ADDRESS (700); "
    "cause address\n"
    "listener: the part stamped 1 Xid critical errors against the launch, Xid 31; cause address\n"
    "gate: the code read safe, its register fields bound by 16; the held list loaded; the code holds no slot\n";

// The holds of the cross `mode`, `registers` or `illegal`, over the list `list_name` of `run`, handed as `holds_name`,
// and an empty held list as `held_name`: 1 where it held any, 0 where it held none, 2 where the list or a blob was not
// had
static int overseer_carrier_held(QryBuffer *run, const char *mode, const char *list_name, const char *held_name,
                                 const char *holds_name)
{
    const unsigned char *list = NULL;
    unsigned long long size = 0ull;
    if (!qry_latest(run, list_name, &list, &size, NULL) || (qry_hand(run, held_name, "", 0ull, QRY_ORDINARY) == 0ull))
    {
        return 2;
    }
    const int illegal = (strcmp(mode, "illegal") == 0);
    char holds[4096];
    size_t length = 0u;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned char *const end = (const unsigned char *)memchr(list + at, '\n', (size_t)(size - at));
        const size_t line_length = (end != NULL) ? (size_t)(end - (list + at)) : (size_t)(size - at);
        char line[512];
        snprintf(line, sizeof(line), "%.*s", (int)line_length, (const char *)(list + at));
        at += (unsigned long long)line_length + 1ull;
        char code[256];
        char cases[256];
        char answers[256];
        unsigned int registers = 0u;
        if ((sscanf(line, "%255s %u %255s %255s", code, &registers, cases, answers) != 4) ||
            (!illegal && (registers >= OVERSEER_CARRIER_CLEARING)))
        {
            continue;
        }
        answers[strcspn(answers, ".")] = '\0';
        const int written = illegal
                                ? snprintf(holds + length, sizeof(holds) - length,
                                           "%s 0 the stand-in reads an illegal instruction\n", answers)
                                : snprintf(holds + length, sizeof(holds) - length,
                                           "%s %u the stand-in reads R40, past the %u registers its kernel allots\n",
                                           answers, OVERSEER_CARRIER_CLEARING, registers);
        // a hold's line fits what is left of the holds, and its length is not negative
        length += ((written > 0) && ((size_t)written < (sizeof(holds) - length))) ? (size_t)written : 0u;
    }
    if (qry_hand(run, holds_name, holds, (unsigned long long)length, QRY_ORDINARY) == 0ull)
    {
        return 2;
    }
    return (length != 0u) ? 1 : 0;
}

int main(int count, char **words)
{
    if ((count >= 2) && (strcmp(words[1], "pop") == 0))
    {
        return 0;
    }
    // the cross's words: the list, the mode, the held list, and after them the holds and the read list the airlock
    // names, which this cross passes over
    if ((count >= 4) && (count <= 6) &&
        ((strcmp(words[2], "cross") == 0) || (strcmp(words[2], "unread") == 0) || (strcmp(words[2], "silent") == 0) ||
         (strcmp(words[2], "registers") == 0) || (strcmp(words[2], "illegal") == 0)))
    {
        if (strcmp(words[2], "unread") == 0)
        {
            return 2;
        }
        QryBuffer *const run = qry_run();
        if ((strcmp(words[2], "registers") == 0) || (strcmp(words[2], "illegal") == 0))
        {
            return ((run == NULL) || (count < 5)) ? 2
                                                  : overseer_carrier_held(run, words[2], words[1], words[3], words[4]);
        }
        if ((strcmp(words[2], "cross") == 0) &&
            ((run == NULL) || (qry_hand(run, words[3], "", 0ull, QRY_ORDINARY) == 0ull)))
        {
            return 2;
        }
        return 0;
    }
    const int held = (count >= 4) && (strcmp(words[2], "--held") == 0);
    const int first = held ? 4 : 2;
    if (((count - first) != 5) || ((strcmp(words[1], "refuse") != 0) && (strcmp(words[1], "hang") != 0)))
    {
        fprintf(stderr, "overseer_carrier refuse|hang [--held <name>] <code> <registers> <cases> <answers> <slot>\n");
        return 2;
    }
    if (strcmp(words[1], "hang") == 0)
    {
#if defined(_WIN32)
        Sleep(OVERSEER_CARRIER_HANG_SECONDS * 1000u);
#else
        const struct timespec hung = {OVERSEER_CARRIER_HANG_SECONDS, 0};
        nanosleep(&hung, NULL);
#endif
        return 0;
    }
    QryBuffer *const run = qry_run();
    if ((run == NULL) || (qry_hand(run, words[first + 3], s_refused, sizeof(s_refused) - 1u, QRY_ORDINARY) == 0ull))
    {
        fprintf(stderr, "overseer_carrier: the answer could not be handed to the run\n");
        return 1;
    }
    return 0;
}
