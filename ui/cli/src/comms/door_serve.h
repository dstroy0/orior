// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipc_serve.h: the door's container side: each joined process's place, its end, its key, what it listens on, and the
// queue of what it receives (ipc_serve.c, ipc_requests.c)
#ifndef DOOR_SERVE_H
#define DOOR_SERVE_H
typedef struct CliIpcServeCfg CliIpcServeCfg;
typedef struct CliIpcServeChannelCfg CliIpcServeChannelCfg;
typedef struct CliIpcServeErrorCfg CliIpcServeErrorCfg;

#include "comms.h"
#include "door_internal.h"
#include "embed_dispatch_layout.h"

// the most processes joined at once; the messages queued for one process; and the channels one process listens on
#define CLI_IPC_JOINED 16u
#define CLI_IPC_ENTRIES 128u
#define CLI_IPC_LISTENS 32u

// A joined process's place, a cell of the cellblock from its join to the end of its last thread: where it stands among
// the places and how many of its two threads go on; its end and the process at the other end; its key; the lock its
// writes are guarded by; the frame read; the messages queued for it, their lock and wake,
// and whether its reader goes on; the listeners it owns; and its two threads
typedef struct
{
    unsigned int place;
    unsigned int going;
    CliIpcEnd end;
    int has_peer;
    unsigned int peer;
    char key[CLI_COMMS_KEY];
    char *frame;
    CliIpcEntry entries[CLI_IPC_ENTRIES];
    unsigned int first;
    unsigned int queued;
    CliWake queue_wake;
    int read_state;
    unsigned int listeners[CLI_IPC_LISTENS];
    unsigned int listens;
    CliThread reader;
    CliThread reporter;
} CliIpcJoined;

// The door's container side: whether it is open and its address, the thread taking the processes that join, their
// places, NULL where none is taken, and the lock they are guarded by
typedef struct
{
    int open;
    char address[CLI_PATH_MAX_LEN];
    CliThread accepter;
    CliIpcJoined *joined[CLI_IPC_JOINED];
} CliIpcServed;

// the door's container side, and the cells its places and the processes it lists occupy
extern CliIpcServed g_cli_ipc_served;
extern CellOwner g_cli_ipc_cells;

// `channel` listened on for the joined process: 1, or 0 with why given; a refusal answered, `size` bytes of why; and an
// answer of `size` bytes written: each 1, or 0 where its end is broken (ipc_serve.c)
int cli_ipc_listen_request(const CliIpcServeChannelCfg *args);
int cli_ipc_reject(const CliIpcServeErrorCfg *args);
int cli_ipc_answer(const CliIpcServeCfg *args);

// what a joined process requests, a frame at a time, answered until its end breaks (ipc_requests.c)
void cli_ipc_requests_serve(const CliIpcServeCfg *args);

struct CliIpcServeCfg
{
    CliIpcJoined *joined;
    unsigned long long size;
    const char *bytes;
};

struct CliIpcServeChannelCfg
{
    CliIpcJoined *joined;
    const char *channel;
    VerbaLinea *err_msg;
};

struct CliIpcServeErrorCfg
{
    CliIpcJoined *joined;
    const char *why;
    unsigned long long size;
};

typedef struct
{
    int (*ipc_listen_request)(const CliIpcServeChannelCfg *args);
} CliIpcServeChannelNs;
EMBED_TABLE_LAYOUT(CliIpcServeChannelNs, ipc_listen_request);
EMBED_TABLE_STORAGE CliIpcServeChannelNs cli_ipc_serve_channel EMBED_UNUSED = {
    .ipc_listen_request = cli_ipc_listen_request,
};

typedef struct
{
    int (*ipc_reject)(const CliIpcServeErrorCfg *args);
} CliIpcServeErrorNs;
EMBED_TABLE_LAYOUT(CliIpcServeErrorNs, ipc_reject);
EMBED_TABLE_STORAGE CliIpcServeErrorNs cli_ipc_serve_error EMBED_UNUSED = {
    .ipc_reject = cli_ipc_reject,
};

typedef struct
{
    int (*ipc_answer)(const CliIpcServeCfg *args);
    void (*ipc_requests_serve)(const CliIpcServeCfg *args);
} CliIpcServeNs;
EMBED_TABLE_LAYOUT(CliIpcServeNs, ipc_answer, ipc_requests_serve);
EMBED_TABLE_STORAGE CliIpcServeNs cli_ipc_serve EMBED_UNUSED = {
    .ipc_answer = cli_ipc_answer,
    .ipc_requests_serve = cli_ipc_requests_serve,
};

#endif
