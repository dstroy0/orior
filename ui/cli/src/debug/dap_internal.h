// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_internal.h: what the debug adapter sources share: the adapters open, each with its process or connection, its
// readers and the requests waiting on answers, and the queues an adapter in orior_cli is connected to through
#ifndef DAP_INTERNAL_H
#define DAP_INTERNAL_H
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "dap.h"
#include "embed_dispatch_layout.h"

typedef struct CliDapCtx CliDapCtx;

// the most bytes of what an adapter writes to its errors that are retained
#define CLI_DAP_COMPLAINT 4096u

// what an adapter is: a program started for the session, one listening at an address, or one in orior
#define CLI_DAP_PROGRAM 0u
#define CLI_DAP_SOCKET 1u
#define CLI_DAP_WITHIN 2u

// A request waiting on its answer: its sequence number, whether it came, the error it came with, empty where none did,
// and the message it came in with its body, handed over as the request takes it and given back where it never does
typedef struct
{
    int waiting;
    unsigned long long seq;
    int answered;
    char error[1024];
    CliLspAnswer answer;
} CliDapWaiter;

// An adapter: whether its place is taken and how many reference it; what it is, and whether ending the session ends the
// program; its process or connection; the lock its input is written under and its writer; under its lock, the next
// sequence number, the requests waiting, what it wrote to its errors and its readers still reading, woken by its wake;
// who receives it; and its readers
struct CliDap
{
    int reserved;
    unsigned int refs;
    unsigned int kind;
    int ends_program;
    CliWords words;
    char path[CLI_PATH_TEXT];
    CliSpawned spawned;
    int ended;
    CliSocket socket;
    CliLspWriter input;
    CliWake wake;
    unsigned long long next;
    CliDapWaiter waiters[CLI_DAP_WAITERS];
    char complaint[CLI_DAP_COMPLAINT * 2u];
    unsigned long long complaint_size;
    unsigned int read_state;
    CliDapEventFn emit;
    void *context;
    CliThread reader_thread;
    CliThread complaint_thread;
    CliLspReader reader;
};

extern CliDap g_cli_daps[CLI_DAPS_MAX];

// a place for an adapter taken, referenced once and made empty, NULL where every place is taken; and its readers
// started: what it writes read on a thread of its own, each response handed to the request waiting on it, each event
// received and each request of its refused, and, for a program, what it writes to its errors retained (dap.c)
CliDap *cli_dap_position_alloc(void);
void cli_dap_readers_start(const CliDapCtx *args);
// the adapter ended: its connection ended, its process ended and waited on, an adapter in orior_cli notified no more
// comes
void cli_dap_end(const CliDapCtx *args);
// what an adapter in orior_cli is connected to through let go, as it is (dap_within.c)
void cli_dap_within_let_go(const CliDapCtx *args);

struct CliDapCtx
{
    CliDap *dap;
    int complaints;
};

typedef struct
{
    void (*readers_start)(const CliDapCtx *args);
    void (*end)(const CliDapCtx *args);
    void (*within_let_go)(const CliDapCtx *args);
} CliDapNs;
EMBED_TABLE_LAYOUT(CliDapNs, readers_start, end, within_let_go);
EMBED_TABLE_STORAGE CliDapNs cli_dap_internal EMBED_UNUSED = {
    .readers_start = cli_dap_readers_start,
    .end = cli_dap_end,
    .within_let_go = cli_dap_within_let_go,
};

#endif
