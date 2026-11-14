// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipc_serve.h: the interface's container side: each joined process's place, its end, its key, what it listens on, and the
// queue of what it receives (ipc_serve.c, ipc_requests.c)
#ifndef INTERFACE_SERVE_H
#define INTERFACE_SERVE_H
#include "comms.h"
#include "interface_internal.h"
#include "embed_dispatch_layout.h"
#include "embed_ticket_ring.h"

typedef struct CliIpcServeCfg CliIpcServeCfg;
typedef struct CliIpcServeChannelCfg CliIpcServeChannelCfg;
typedef struct CliIpcServeErrorCfg CliIpcServeErrorCfg;

// the max processes joined at once; the messages queued for one process, a power of two; the channels one process
// listens on; and the bytes of an answer waiting to be written
#define CLI_IPC_JOINED 16u
#define CLI_IPC_ENTRIES_SHIFT 7u
#define CLI_IPC_ENTRIES (1u << CLI_IPC_ENTRIES_SHIFT)
#define CLI_IPC_LISTENS 32u
// the bytes the joined processes' rings' slots take, the joined part of the rings block
#define CLI_INTERFACE_JOINED_RING_BYTES ((unsigned long long)CLI_IPC_JOINED * CLI_IPC_ENTRIES * sizeof(EmbedTicketRingSlot))
#define CLI_IPC_SERVE_ANSWER (CLI_ERR_MSG_MAX_LEN + CLI_NAME_MAX_LEN + 8u)

// A joined process's place, a cell of the cellblock from its join to the end of its last thread: where it is among
// the places and how many of its two threads go on; its end and the process at the other end; its key; the frame read;
// the messages queued for it on a ticket ring, any thread taking a ticket and pushing the entry of its slot, the writer
// of the end popping them in the order of their tickets, the ring's turnstiles and slots, the next ticket taken and the
// next popped; the answer its reader has for the writer, and whether one waits; the wake of the two threads; whether
// its reader goes on and whether its end still takes what is written; the listeners it owns; and its two threads. The
// end has one writer, the thread that sends the messages, which writes the reader's answers too, a frame never falling
// inside another
typedef struct
{
    unsigned int place;
    embed_atomic_u32 going;
    CliIpcEnd end;
    int has_peer;
    unsigned int peer;
    char key[CLI_COMMS_KEY];
    char *frame;
    CliIpcEntry entries[CLI_IPC_ENTRIES];
    EmbedTurnstiles turnstiles;
    EmbedTicketRing ring;
    embed_atomic_u32 claimed;
    embed_u32 taken;
    char *answer;
    unsigned long long answer_size;
    embed_atomic_u32 answer_waits;
    CliWake queue_wake;
    embed_atomic_u32 read_state;
    embed_atomic_u32 sending;
    unsigned int listeners[CLI_IPC_LISTENS];
    unsigned int listens;
    CliThread reader;
    CliThread reporter;
} CliIpcJoined;

// The interface's container side: whether it is open and its address, the thread taking the processes that join, their
// places, NULL where none is taken, and the lock they are guarded by
typedef struct
{
    int open;
    char address[CLI_PATH_MAX_LEN];
    CliThread accepter;
    CliIpcJoined *joined[CLI_IPC_JOINED];
} CliIpcServed;

// the interface's container side, and the cells its places and the processes it lists occupy
extern CliIpcServed g_cli_ipc_served;

// `channel` listened on for the joined process: 1, or 0 with why given; a refusal answered, `size` bytes of why; and an
// answer of `size` bytes passed to the writer of its end: each 1, or 0 where its end is broken (ipc_serve.c)
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
