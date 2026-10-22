// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_internal.h: what the language server sources share: the servers running, each with its process, its readers,
// the requests waiting on answers, what it said it can do and what it wrote to its errors
#ifndef LSP_INTERNAL_H
#define LSP_INTERNAL_H

#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "lsp.h"

// A request waiting on its answer: its id, where its result is written, whether it came, and the error it came with,
// empty where none did
typedef struct
{
    int waiting;
    unsigned long long id;
    CliLine *result;
    int answered;
    char error[1024];
} CliLspWaiter;

// A server: whether its place is taken and how many hold it; its words, settings and process, whether it ended; the
// lock its input is written under, and the writer of its input; under its lock, the next id, the requests waiting,
// what it wrote to its errors, and its readers still reading, woken by its wake; what it said it can do, in a cell of
// its size given back with the server; who hears it; and its readers
struct CliLsp
{
    int taken;
    unsigned int holds;
    unsigned int number;
    CliWords words;
    CliSetting settings[64];
    CliSpawned spawned;
    int ended;
    CliLock input_lock;
    CliLspWriter input;
    CliLock lock;
    CliWake wake;
    unsigned long long next;
    CliLspWaiter waiters[CLI_LSP_WAITERS];
    char complaint[CLI_LSP_COMPLAINT * 2u];
    unsigned long long complaint_size;
    unsigned int reading;
    char *capabilities;
    unsigned long long capabilities_size;
    CliLspHeard heard;
    void *context;
    CliThread reader_thread;
    CliThread complaint_thread;
    CliLspReader reader;
};

// the lock a server's number and holds are counted under; each server is a cell of its own, taken as it starts and
// given back once its readers end (lsp.c)
extern CliLock g_cli_lsps_lock;

// the server's readers started: what it says read on a thread of its own, each answer handed to the request waiting on
// it and what it says on its own to who hears it; and what it writes to its errors kept, its last CLI_LSP_COMPLAINT
// bytes (lsp.c)
void cli_lsp_readers_started(CliLsp *lsp);
// the server's process ended where it runs, and waited on (lsp.c)
void cli_lsp_ended(CliLsp *lsp);
// each file a server has open let go, as it is let go; and the cells the files open are held in, counted: the table of
// them, each one's URI while it is open, and the copies one walk over a server's files takes (lsp_files.c)
void cli_lsp_files_let_go(const CliLsp *lsp);
extern CliCellsTally g_cli_lsp_files_cells;

#endif
