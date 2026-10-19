// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_waiters.c: a request's answer looked at without waiting on it, and a request whose answer no one waits for
// let go of, its place among those waiting freed and its answer passed over when it comes
#include "dap_internal.h"

int cli_dap_answered(CliDap *dap, unsigned long long seq, CliLine *said)
{
    int answered = -1;
    cli_lock_take(&dap->lock);
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        const CliDapWaiter *const waiter = &dap->waiters[at];
        if (waiter->waiting && (waiter->seq == seq) && waiter->answered)
        {
            answered = (waiter->error[0] == '\0') ? 1 : 0;
            cli_line_text(said, waiter->error);
        }
    }
    cli_lock_give(&dap->lock);
    return answered;
}

void cli_dap_forgotten(CliDap *dap, unsigned long long seq)
{
    cli_lock_take(&dap->lock);
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        if (dap->waiters[at].waiting && (dap->waiters[at].seq == seq))
        {
            dap->waiters[at].waiting = 0;
        }
    }
    cli_lock_give(&dap->lock);
}
