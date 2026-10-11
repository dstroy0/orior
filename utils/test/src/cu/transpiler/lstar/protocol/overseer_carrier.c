// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer_carrier.c: a carrier for overseer_check that reaches no part. Given `refuse`, it hands the question it is
// handed the answer the carrier hands where the part refused a question and stayed whole, with the accounts a refusal
// carries (cubin_run.c); given `hang`, it outlasts the time it is given and answers nothing
//
//     overseer_carrier refuse|hang <code> <registers> <cases> <answers> <slot>
//
// Exit 0 where it answered or hung out its time, 1 where the answer could not be handed, 2 where the words are none of
// the above
#if !defined(_WIN32)
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

// the refusal and its accounts, as the carrier says them after a launch the part refused
static const char s_refused[] =
    "refused CUDA_ERROR_ILLEGAL_ADDRESS\n"
    "proctor: the launch of question.bin, 32 bytes of code, 16 registers, 32 threads in 1 blocks over 1 cases, 0 "
    "launches timed after it, was refused, the part left whole: the driver's status CUDA_ERROR_ILLEGAL_ADDRESS (700); "
    "cause address\n"
    "listener: the part stamped 1 Xid critical errors against the launch, Xid 31; cause address\n"
    "gate: the code read safe, its register fields bound by 16; the held list loaded; the code holds no slot\n";

int main(int count, char **words)
{
    if ((count != 7) || ((strcmp(words[1], "refuse") != 0) && (strcmp(words[1], "hang") != 0)))
    {
        fprintf(stderr, "overseer_carrier refuse|hang <code> <registers> <cases> <answers> <slot>\n");
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
    if ((run == NULL) || (qry_hand(run, words[5], s_refused, sizeof(s_refused) - 1u, QRY_ORDINARY) == 0ull))
    {
        fprintf(stderr, "overseer_carrier: the answer could not be handed to the run\n");
        return 1;
    }
    return 0;
}
