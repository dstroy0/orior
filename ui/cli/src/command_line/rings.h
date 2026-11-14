// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rings.h: the one aligned block every ring of the process lays its bytes in. A ring's record is its module's, settled
// by the linker; its bytes are a part of this block, each part on a CLI_RINGS_ALIGN boundary, sized by the module's own
// counts. Nothing of a ring comes from the shared cells, so no ring punches a hole in the persistent tier (rings.c)
#ifndef CLI_RINGS_H
#define CLI_RINGS_H
#include "embed_dispatch_layout.h"

typedef struct CliRingsPartCfg CliRingsPartCfg;

// the boundary each part starts on, and `bytes` rounded up to it
#define CLI_RINGS_ALIGN 64ULL
#define CLI_RINGS_ALIGNED(bytes_)                                                                                      \
    ((((unsigned long long)(bytes_)) + CLI_RINGS_ALIGN - 1ULL) & ~(CLI_RINGS_ALIGN - 1ULL))

// the parts: the notices' list, the comms channels' last messages, the debug console's lines, the run's retained
// errors, the Run pane's lines and its reports' slots, the runner's lines, the servers' jobs slots, the door's client
// and joined slots, the debug adapter's queue slots, the reach door's slots, the check's two rings' slots, the
// problems' slots, the pair answers' slots, and the view's events, orders, maps and map slots
#define CLI_RINGS_PARTS(part_)                                                                                         \
    part_(NOTICES) part_(COMMS) part_(DEBUG_CONSOLE) part_(REPORT_ERRORS) part_(JOB_RUNS) part_(JOB_RUNS_REPORTS)      \
        part_(RUNNER_LINES) part_(SERVERS_JOBS) part_(DOOR_CLIENTS) part_(DOOR_JOINED) part_(DAP) part_(REACH)         \
            part_(SERVERS_CHECK) part_(PROBLEMS) part_(VIEW_PAIRS) part_(VIEW)
#define CLI_RINGS_PART_NAMED(name_) CLI_RINGS_##name_,
enum
{
    CLI_RINGS_PARTS(CLI_RINGS_PART_NAMED) CLI_RINGS_COUNT
};

// the bytes of part `part`, where they lie in the block; how many the part has; and every ring record that holds its
// slots by pointer laid over its part, as the pool is made, before any thread takes a ticket
unsigned char *cli_rings_part(const CliRingsPartCfg *args);
unsigned long long cli_rings_bytes(const CliRingsPartCfg *args);
void cli_rings_laid(void);

struct CliRingsPartCfg
{
    unsigned int part;
};

typedef struct
{
    unsigned char *(*part)(const CliRingsPartCfg *args);
    unsigned long long (*bytes)(const CliRingsPartCfg *args);
    void (*laid)(void);
} CliRingsNs;
EMBED_TABLE_LAYOUT(CliRingsNs, part, bytes, laid);
EMBED_TABLE_STORAGE CliRingsNs cli_rings EMBED_UNUSED = {
    .part = cli_rings_part,
    .bytes = cli_rings_bytes,
    .laid = cli_rings_laid,
};

#endif
