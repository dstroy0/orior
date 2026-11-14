// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rings.c: the one aligned block every ring of the process positions its bytes in, each part's bytes the sum its module's
// header gives, each part on the block's boundary
#include "rings.h"

#include "../../../gui/src/thread.h"
#include "../../../gui/src/window.h"
#include "../comms/comms_internal.h"
#include "../comms/interface_internal.h"
#include "../comms/interface_serve.h"
#include "../debug/dap_internal.h"
#include "../debug/debug_pane_internal.h"
#include "../editor/view.h"
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../runs/job_runs_internal.h"
#include "../servers/servers_internal.h"
#include "notices_internal.h"

// each part's bytes, in the parts' order
static const unsigned long long s_bytes[CLI_RINGS_COUNT] = {
    CLI_NOTICES_PART_BYTES,       CLI_COMMS_RING_BYTES,
    CLI_DEBUG_CONSOLE_RING_BYTES, CLI_REPORT_ERRORS_RING_BYTES,
    CLI_JOB_RUNS_RING_BYTES,      CLI_JOB_RUNS_REPORTS_RING_BYTES,
    CLI_RUNNER_LINES_RING_BYTES,  CLI_SERVERS_JOBS_RING_BYTES,
    CLI_INTERFACE_CLIENTS_RING_BYTES,  CLI_INTERFACE_JOINED_RING_BYTES,
    CLI_DAP_RING_BYTES,           CLI_REACH_RING_BYTES,
    CLI_SERVERS_CHECK_RING_BYTES, CLI_PROBLEMS_RING_BYTES,
    CLI_VIEW_PAIR_RING_BYTES,     CLI_VIEW_RING_BYTES(GUI_WINDOW_HIT_CLIENTS),
};

// the block, every part's bytes rounded to the boundary, one after another in the parts' order
#define CLI_RINGS_BLOCK_BYTES                                                                                          \
    (CLI_RINGS_ALIGNED(CLI_NOTICES_PART_BYTES) + CLI_RINGS_ALIGNED(CLI_COMMS_RING_BYTES) +                             \
     CLI_RINGS_ALIGNED(CLI_DEBUG_CONSOLE_RING_BYTES) + CLI_RINGS_ALIGNED(CLI_REPORT_ERRORS_RING_BYTES) +               \
     CLI_RINGS_ALIGNED(CLI_JOB_RUNS_RING_BYTES) + CLI_RINGS_ALIGNED(CLI_JOB_RUNS_REPORTS_RING_BYTES) +                 \
     CLI_RINGS_ALIGNED(CLI_RUNNER_LINES_RING_BYTES) + CLI_RINGS_ALIGNED(CLI_SERVERS_JOBS_RING_BYTES) +                 \
     CLI_RINGS_ALIGNED(CLI_INTERFACE_CLIENTS_RING_BYTES) + CLI_RINGS_ALIGNED(CLI_INTERFACE_JOINED_RING_BYTES) +                  \
     CLI_RINGS_ALIGNED(CLI_DAP_RING_BYTES) + CLI_RINGS_ALIGNED(CLI_REACH_RING_BYTES) +                                 \
     CLI_RINGS_ALIGNED(CLI_SERVERS_CHECK_RING_BYTES) + CLI_RINGS_ALIGNED(CLI_PROBLEMS_RING_BYTES) +                    \
     CLI_RINGS_ALIGNED(CLI_VIEW_PAIR_RING_BYTES) + CLI_RINGS_ALIGNED(CLI_VIEW_RING_BYTES(GUI_WINDOW_HIT_CLIENTS)))
EMBED_ALIGN(CLI_RINGS_ALIGN) static unsigned char s_block[CLI_RINGS_BLOCK_BYTES];

unsigned char *cli_rings_part(const CliRingsPartCfg *args)
{
    if (args->part >= CLI_RINGS_COUNT)
    {
        return NULL;
    }
    unsigned long long at = 0ULL;
    for (unsigned int part = 0u; part < args->part; part += 1u)
    {
        at += CLI_RINGS_ALIGNED(s_bytes[part]);
    }
    return s_block + at;
}

unsigned long long cli_rings_bytes(const CliRingsPartCfg *args)
{
    return (args->part < CLI_RINGS_COUNT) ? s_bytes[args->part] : 0ULL;
}

void cli_rings_positioned(void)
{
    cli_servers_jobs_positioned();
}
