// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_internal.h: what the language server sources share: the servers running, each with its process, its readers,
// the requests waiting on answers, what it reported it can do and what it wrote to its errors
#ifndef LSP_INTERNAL_H
#define LSP_INTERNAL_H
typedef struct CliLspCtx CliLspCtx;
typedef struct CliLspKindCtx CliLspKindCtx;

#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "embed_dispatch_layout.h"
#include "lsp.h"

// A request waiting on its answer: its id, where its answer is handed over, whether it came, and the error it came
// with, empty where none did
typedef struct
{
    int waiting;
    unsigned long long id;
    CliLspAnswer *answer;
    int answered;
    char error[1024];
} CliLspWaiter;

// A server: whether its place is taken and how many reference it; its words, settings and process, whether it ended;
// the lock its input is written under, and the writer of its input; under its lock, the next id, the requests waiting,
// what it wrote to its errors, and its readers still reading, woken by its wake; what it reported it can do, in a cell
// of its size given back with the server; its receiver; and its readers
struct CliLsp
{
    int reserved;
    unsigned int refs;
    unsigned int number;
    CliWords words;
    CliSetting settings[64];
    CliSpawned spawned;
    int ended;
    CliLspWriter input;
    CliWake wake;
    unsigned long long next;
    CliLspWaiter waiters[CLI_LSP_WAITERS];
    char complaint[CLI_LSP_COMPLAINT * 2u];
    unsigned long long complaint_size;
    unsigned int read_state;
    char *capabilities;
    unsigned long long capabilities_size;
    CliLspRecvFn recv;
    void *context;
    CliThread reader_thread;
    CliThread complaint_thread;
    CliLspReader reader;
};

// the lock a server's number and references are counted under; each server is a cell of its own, taken as it starts and
// given back once its readers end (lsp.c)

// the server's readers started: what it writes read on a thread of its own, each answer handed to the request waiting
// on it and what it sends unprompted to its receiver; and what it writes to its errors retained, its last
// CLI_LSP_COMPLAINT bytes (lsp.c)
void cli_lsp_readers_start(const CliLspCtx *args);
// the server's process ended where it runs, and waited on (lsp.c)
void cli_lsp_end(const CliLspCtx *args);
// each file a server has open let go, as it is let go; and the cells the files open are allocated in, counted: the
// table of them, each one's URI while it is open, and the copies one walk over a server's files takes (lsp_files.c)
void cli_lsp_files_let_go(const CliLspKindCtx *args);
extern CellOwner g_cli_lsp_files_cells;

struct CliLspCtx
{
    CliLsp *lsp;
};

struct CliLspKindCtx
{
    const CliLsp *lsp_read;
};

typedef struct
{
    void (*readers_start)(const CliLspCtx *args);
    void (*end)(const CliLspCtx *args);
} CliLspNsTable;
EMBED_TABLE_LAYOUT(CliLspNsTable, readers_start, end);
EMBED_TABLE_STORAGE CliLspNsTable cli_lsp_internal EMBED_UNUSED = {
    .readers_start = cli_lsp_readers_start,
    .end = cli_lsp_end,
};

typedef struct
{
    void (*files_let_go)(const CliLspKindCtx *args);
} CliLspKindNs;
EMBED_TABLE_LAYOUT(CliLspKindNs, files_let_go);
EMBED_TABLE_STORAGE CliLspKindNs cli_lsp_kind_internal EMBED_UNUSED = {
    .files_let_go = cli_lsp_files_let_go,
};

#endif
