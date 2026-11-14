// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_send.c: a request sent and its answer read, a redirect followed up to ten times where the request follows
// them, a 303, or a 301 or 302 of a POST, going on as a GET with no body; the request at a line of a file sent and its
// answer stored; and a GET for orior's own reading. Two work states contain what a request sends and reads, a third
// request waiting for one
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the max requests sent at once, each in a fixed work state outside the pool with its body, its raw answer and the
// answer undone, where they punch no hole in the persistent tier; how many are taken, and the wake of a request waiting
// for one to end
#define CLI_REQUESTS_WORK_STATES 2u
#define REQUESTS_WORK_BYTES                                                                                            \
    ((unsigned long long)CLI_REQUESTS_READ_STATE + (3ULL * ((unsigned long long)CLI_REQUESTS_BODY + 1ULL)))
static CliRequestsWorkState s_work_states[CLI_REQUESTS_WORK_STATES];
static unsigned char s_work_bytes[CLI_REQUESTS_WORK_STATES][REQUESTS_WORK_BYTES];
static unsigned char s_work_taken[CLI_REQUESTS_WORK_STATES];

static unsigned int s_capacities_used;
static CliWake s_wake = CLI_WAKE_INIT;

CliRequestsWorkState *cli_requests_work_state_alloc(void)
{
    while (s_capacities_used == CLI_REQUESTS_WORK_STATES)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    // a free work state: its read state, the body, the raw answer and the answer undone in its fixed bytes, which the
    // request's thread reads and writes while the request goes
    unsigned int place = 0u;
    while ((place < CLI_REQUESTS_WORK_STATES) && (s_work_taken[place] != 0u))
    {
        place += 1u;
    }
    const unsigned long long body_bytes = (unsigned long long)CLI_REQUESTS_BODY + 1ULL;
    CliRequestsWorkState *const work_state = &s_work_states[place];
    s_work_taken[place] = 1u;
    memor.set(&(MemoriaCfg){.dst = work_state, .val = 0, .bytes = sizeof(*work_state)});
    work_state->read_state = s_work_bytes[place];
    char *const body_cell = (char *)(void *)(work_state->read_state + CLI_REQUESTS_READ_STATE);
    work_state->body = mmgr_line_over(body_cell, body_bytes);
    work_state->raw = mmgr_line_over(body_cell + body_bytes, body_bytes);
    work_state->unpacked = (unsigned char *)(void *)(body_cell + (2ULL * body_bytes));
    work_state->unpacked_capacity = CLI_REQUESTS_BODY;
    s_capacities_used += 1u;
    return work_state;
}

void cli_requests_work_state_free(const CliRequestsSizeErrMsgCtx *args)
{
    if (args->work_state == NULL)
    {
        return;
    }
    // what a request sent and what came back may carry a secret, so its bytes are zeroed before the place is free
    mmgr_zero_buf(args->work_state->raw.out, args->work_state->raw.at);
    mmgr_zero_buf(args->work_state->body.out, args->work_state->body.at);
    mmgr_zero_buf(args->work_state->unpacked, args->work_state->answer.size);
    const unsigned int place = (unsigned int)(args->work_state - s_work_states);
    mmgr_zero_buf(args->work_state, sizeof(*args->work_state));
    s_work_taken[place] = 0u;
    s_capacities_used -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

typedef struct
{
    CliRequestsWorkState *work_state;
    const char *failed;
    unsigned long long size;
    const CliRequestsSent *sent;
} RequestsSendCtx;
