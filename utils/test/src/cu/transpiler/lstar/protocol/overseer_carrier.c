// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer_carrier.c: a carrier for overseer_check that reaches no part, and the airlock's pop and cross beside it.
// Given `refuse`, it passes the question it is passed the answer the carrier passes where the part refused a question
// and stayed whole, with the accounts a refusal carries (cubin_run.c); given `hang`, it outlasts the time it is given
// and answers nothing. A `--gated <name>` after either is the gated list a carrier reads, and is passed over. Given
// `pop`, it pops nothing and ends clean. As the airlock's cross, the round's list put after its first word, it passes
// an empty gated list given `cross`, ends as a cross that did not read given `unread`, and ends clean having passed
// nothing given `silent` (run_channel.h). Given `registers`, it passes an empty gated list and refuses each question of
// fewer than OVERSEER_CARRIER_CLEARING registers for a register past its count, the refusals it passes the teacher
// naming that count; given `illegal`, it refuses each question for an instruction no count clears. Either ends as the
// cross does where it refuses some. Given `panic` and a round's list, it passes one record of its start, puts a panic
// on the line in place of its first question's answer, and ends as a carrier that lost its mind, code 3: the check
// that passes it a round holds that nothing is carried after it (scheduler_check)
//
//     overseer_carrier refuse|hang [--gated <name>] <code> <registers> <cases> <answers> <probe>
//     overseer_carrier pop ... --list <list>
//     overseer_carrier panic [--gated <name>] --list <list>
//     overseer_carrier <list> cross|unread|silent|registers|illegal <gated> [<refusals> [<read>]]
//
// Exit 0 where it answered, popped, crossed or hung out its time, 1 where the answer could not be passed, 2 where the
// words are none of the above or the cross did not read, 3 where it panicked as asked
#if !defined(_POSIX_C_SOURCE) && !defined(_WIN32)
// nanosleep is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../../../src/platform/platform.h"
#include "memoria_operor/memoria_operor.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

// the seconds `hang` outlasts, past any time overseer_check gives a question
#define OVERSEER_CARRIER_HANG_SECONDS 5u
// the count of registers that clears a refusal the `registers` cross reports, R40 the register it reads
#define OVERSEER_CARRIER_CLEARING 41u

// the record each start of the panicking carrier passes, and the panic it puts on the line
#define OVERSEER_CARRIER_STARTED "carrier_started"
#define OVERSEER_CARRIER_PANIC                                                                                         \
    "carrier: the part faulted on the first question of the list, and the carrier lost its mind\n"

// the refusal and its accounts, as the carrier reports them after a launch the part refused
static const char s_refused[] =
    "refused CUDA_ERROR_ILLEGAL_ADDRESS\n"
    "proctor: the launch of question.bin, 32 bytes of code, 16 registers, 32 threads in 1 blocks over 1 cases, 0 "
    "launches timed after it, was refused, the part left whole: the driver's status CUDA_ERROR_ILLEGAL_ADDRESS (700); "
    "cause address\n"
    "listener: the part stamped 1 Xid critical errors against the launch, Xid 31; cause address\n"
    "gate: the code read safe, its register fields bound by 16; the gated list loaded; the code contains no probe\n";

// The refusals of the cross `mode`, `registers` or `illegal`, over the list `list_name` of `run`, passed as
// `gated_list_name`, and an empty gated list as `gated_name`: 1 where it refused any, 0 where it refused none, 2 where
// the list or a blob was not had
static int overseer_carrier_gated(QryShared *run, const char *mode, const char *list_name, const char *gated_name,
                                  const char *gated_list_name)
{
    const unsigned char *list = NULL;
    unsigned long long size = 0ull;
    if (!qry_member_blob(list_name, &list, &size, NULL) ||
        (qry_member_append(gated_name, "", 0ull, QRY_ORDINARY) == 0ull))
    {
        return 2;
    }
    const int illegal = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = mode, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "illegal", .ci = EMBED_FALSE));
    char gated_text[4096];
    size_t length = 0u;
    for (unsigned long long at = 0ull; at < size;)
    {
        // Explicit cast reads the found place as the bytes of the list
        const unsigned char *const end = (const unsigned char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = list + at,
                                                                           .bytes = (size_t)(size - at), .val = '\n');
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
        answers[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = answers, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = ".")] = '\0';
        const int written = illegal
                                ? snprintf(gated_text + length, sizeof(gated_text) - length,
                                           "%s 0 the stand-in reads an illegal instruction\n", answers)
                                : snprintf(gated_text + length, sizeof(gated_text) - length,
                                           "%s %u the stand-in reads R40, past the %u registers its kernel allots\n",
                                           answers, OVERSEER_CARRIER_CLEARING, registers);
        // a refusal's line fits what is left of the refusals, and its length is not negative
        length += ((written > 0) && ((size_t)written < (sizeof(gated_text) - length))) ? (size_t)written : 0u;
    }
    if (qry_member_append(gated_list_name, gated_text, (unsigned long long)length, QRY_ORDINARY) == 0ull)
    {
        return 2;
    }
    return (length != 0u) ? 1 : 0;
}

int program_main(int count, char **words)
{
    if ((count >= 2) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "pop", .ci = EMBED_FALSE)))
    {
        return 0;
    }
    if ((count >= 2) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "panic", .ci = EMBED_FALSE)))
    {
        // the gated list and the round's list are passed over: the panic answers the first question whatever it is
        QryShared *const run = qry_member_run();
        if ((run == NULL) || (qry_member_append(OVERSEER_CARRIER_STARTED, "started\n", 8ull, QRY_ORDINARY) == 0ull) ||
            (qry_member_append(QRY_PANIC_CALL, OVERSEER_CARRIER_PANIC, sizeof(OVERSEER_CARRIER_PANIC) - 1u,
                               QRY_PANIC) == 0ull))
        {
            fprintf(stderr, "overseer_carrier: the panic could not be passed to the run\n");
            return 1;
        }
        return 3;
    }
    // the cross's words: the list, the mode, the gated list, and after them the refusals and the read list the airlock
    // names, which this cross passes over
    if ((count >= 4) && (count <= 6) &&
        ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cross",
                     .ci = EMBED_FALSE)) ||
         (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "unread",
                     .ci = EMBED_FALSE)) ||
         (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "silent",
                     .ci = EMBED_FALSE)) ||
         (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = "registers", .ci = EMBED_FALSE)) ||
         (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = "illegal", .ci = EMBED_FALSE))))
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "unread", .ci = EMBED_FALSE))
        {
            return 2;
        }
        QryShared *const run = qry_member_run();
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "registers", .ci = EMBED_FALSE)) ||
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "illegal", .ci = EMBED_FALSE)))
        {
            return ((run == NULL) || (count < 5)) ? 2
                                                  : overseer_carrier_gated(run, words[2], words[1], words[3], words[4]);
        }
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "cross", .ci = EMBED_FALSE)) &&
            ((run == NULL) || (qry_member_append(words[3], "", 0ull, QRY_ORDINARY) == 0ull)))
        {
            return 2;
        }
        return 0;
    }
    const int gated_mode =
        (count >= 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "--gated", .ci = EMBED_FALSE));
    const int first = gated_mode ? 4 : 2;
    if (((count - first) != 5) ||
        ((!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                      .other = "refuse", .ci = EMBED_FALSE)) &&
         (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "hang",
                      .ci = EMBED_FALSE))))
    {
        fprintf(stderr, "overseer_carrier refuse|hang [--gated <name>] <code> <registers> <cases> <answers> <probe>\n");
        return 2;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "hang",
                   .ci = EMBED_FALSE))
    {
#if defined(_WIN32)
        Sleep(OVERSEER_CARRIER_HANG_SECONDS * 1000u);
#else
        const struct timespec hung = {OVERSEER_CARRIER_HANG_SECONDS, 0};
        nanosleep(&hung, NULL);
#endif
        return 0;
    }
    QryShared *const run = qry_member_run();
    if ((run == NULL) || (qry_member_append(words[first + 3], s_refused, sizeof(s_refused) - 1u, QRY_ORDINARY) == 0ull))
    {
        fprintf(stderr, "overseer_carrier: the answer could not be passed to the run\n");
        return 1;
    }
    return 0;
}
