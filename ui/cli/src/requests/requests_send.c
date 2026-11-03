// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_send.c: a request sent and its answer read, a redirect followed up to ten times where the request follows
// them, a 303, or a 301 or 302 of a POST, going on as a GET with no body; the request at a line of a file sent and its
// answer stored; and a GET for orior's own reading. Two work states contain what a request sends and reads, a third
// request waiting for one
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the most requests sent at once, each in a work state that is a cell of its own; how many are, their lock, and the
// wake of a request waiting for one to end
#define CLI_REQUESTS_WORK_STATES 2u

static unsigned int s_capacities_used;
static CliWake s_wake = CLI_WAKE_INIT;

CliRequestsWorkState *cli_requests_work_state_alloc(void)
{
    while (s_capacities_used == CLI_REQUESTS_WORK_STATES)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    CliRequestsWorkState *work_state = g_cli_cells->persistent_buf_alloc(
        sizeof(CliRequestsWorkState) + (unsigned long long)CLI_REQUESTS_READ_STATE, &g_cli_requests_cells);
    if (work_state != NULL)
    {
        work_state->read_state = (unsigned char *)(void *)(work_state + 1);
        work_state->unpacked = NULL;
        work_state->unpacked_capacity = 0ULL;
        char *const body_cell = g_cli_cells->temporary_buf_alloc(CLI_REQUESTS_BODY + 1ULL, &g_cli_requests_cells);
        char *const raw_cell = g_cli_cells->temporary_buf_alloc(CLI_REQUESTS_BODY + 1ULL, &g_cli_requests_cells);
        work_state->body = mmgr_line_over(body_cell, CLI_REQUESTS_BODY + 1ULL);
        work_state->raw = mmgr_line_over(raw_cell, CLI_REQUESTS_BODY + 1ULL);
        if ((body_cell == NULL) || (raw_cell == NULL))
        {
            g_cli_cells->persistent_buf_release(work_state->raw.out, &g_cli_requests_cells);
            g_cli_cells->persistent_buf_release(work_state->body.out, &g_cli_requests_cells);
            g_cli_cells->persistent_buf_release(work_state, &g_cli_requests_cells);
            work_state = NULL;
        }
    }
    s_capacities_used += (work_state != NULL) ? 1u : 0u;
    return work_state;
}

void cli_requests_work_state_free(const CliRequestsSizeErrMsgCtx *args)
{
    if (args->work_state == NULL)
    {
        return;
    }
    g_cli_cells->persistent_buf_release(args->work_state->unpacked, &g_cli_requests_cells);
    // what a request sent and what came back may carry a secret, and the pool leaves a cell as it is
    mmgr_zero_buf(args->work_state->raw.out, args->work_state->raw.at);
    mmgr_zero_buf(args->work_state->body.out, args->work_state->body.at);
    g_cli_cells->persistent_buf_release(args->work_state->raw.out, &g_cli_requests_cells);
    g_cli_cells->persistent_buf_release(args->work_state->body.out, &g_cli_requests_cells);
    mmgr_zero_buf(args->work_state, sizeof(*args->work_state));
    g_cli_cells->persistent_buf_release(args->work_state, &g_cli_requests_cells);
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
