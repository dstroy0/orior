// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_internal.h: what the debug adapter sources share: the adapters held, each with its process or connection, its
// readers and the requests waiting on answers, and the queues an adapter in orior is spoken to through
#ifndef DAP_INTERNAL_H
#define DAP_INTERNAL_H

#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "dap.h"

// the most bytes of what an adapter writes to its errors that are kept
#define CLI_DAP_COMPLAINT 4096u

// what an adapter is: a program started for the session, one listening at an address, or one in orior
#define CLI_DAP_PROGRAM 0u
#define CLI_DAP_SOCKET 1u
#define CLI_DAP_WITHIN 2u

// A request waiting on its answer: its sequence number, whether it came, the error it came with, empty where none did,
// and its body, in a cell of its size given back as the request takes it
typedef struct
{
    int waiting;
    unsigned long long seq;
    int answered;
    char error[1024];
    char *body;
    unsigned long long body_size;
} CliDapWaiter;

// An adapter: whether its place is taken and how many hold it; what it is, and whether ending the session ends the
// program; its process or connection; the lock its input is written under and its writer; under its lock, the next
// sequence number, the requests waiting, what it wrote to its errors and its readers still reading, woken by its wake;
// who hears it; and its readers
struct CliDap
{
    int taken;
    unsigned int holds;
    unsigned int kind;
    int ends_program;
    CliWords words;
    char path[CLI_PATH_TEXT];
    CliSpawned spawned;
    int ended;
    CliSocket socket;
    CliLock input_lock;
    CliLspWriter input;
    CliLock lock;
    CliWake wake;
    unsigned long long next;
    CliDapWaiter waiters[CLI_DAP_WAITERS];
    char complaint[CLI_DAP_COMPLAINT * 2u];
    unsigned long long complaint_size;
    unsigned int reading;
    CliDapHeard heard;
    void *context;
    CliThread reader_thread;
    CliThread complaint_thread;
    CliLspReader reader;
};

extern CliDap g_cli_daps[CLI_DAPS_MOST];
extern CliLock g_cli_daps_lock;

// a place for an adapter taken, held once and made empty, NULL where every place is taken; and its readers started:
// what it says read on a thread of its own, each response handed to the request waiting on it, each event heard and
// each request of its refused, and, for a program, what it writes to its errors kept (dap.c)
CliDap *cli_dap_place_taken(void);
void cli_dap_readers_started(CliDap *dap, int complaints);
// the adapter ended: its connection ended, its process ended and waited on, an adapter in orior told no more comes
void cli_dap_ended(CliDap *dap);
// what an adapter in orior is spoken to through let go, as it is (dap_within.c)
void cli_dap_within_let_go(CliDap *dap);

#endif
