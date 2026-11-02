// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_waiters.c: a request's answer looked at without waiting on it, and a request whose answer no one waits for
// let go of, its place among those waiting freed and its answer passed over when it comes
#include "dap_internal.h"

int cli_dap_answer(const CliDapErrMsgCommandSeqCfg *args)
{
    int answered = -1;
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        const CliDapWaiter *const waiter = &args->dap->waiters[at];
        if (waiter->waiting && (waiter->seq == args->seq) && waiter->answered)
        {
            answered = (waiter->error[0] == '\0') ? 1 : 0;
            mmgr_line_put(args->err_msg, waiter->error);
        }
    }
    return answered;
}

void cli_dap_forgotten(const CliDapErrMsgCommandSeqCfg *args)
{
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        if (args->dap->waiters[at].waiting && (args->dap->waiters[at].seq == args->seq))
        {
            args->dap->waiters[at].waiting = 0;
            g_cli_cells->persistent_buf_release(args->dap->waiters[at].answer.message, &g_cli_debug_cells);
            args->dap->waiters[at].answer = (CliLspAnswer){NULL, NULL, 0ull};
        }
    }
}
