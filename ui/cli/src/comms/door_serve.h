// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_serve.h: the door's container side: each joined process's place, its end, its key, what it listens on, and the
// queue of what it receives (door_serve.c, door_requests.c)
#ifndef DOOR_SERVE_H
#define DOOR_SERVE_H
typedef struct CliDoorServeCfg CliDoorServeCfg;
typedef struct CliDoorServeChannelCfg CliDoorServeChannelCfg;
typedef struct CliDoorServeWhyCfg CliDoorServeWhyCfg;

#include "comms.h"
#include "door_internal.h"
#include "embed_dispatch_layout.h"

// the most processes joined at once; the messages queued for one process; and the channels one process listens on
#define CLI_DOOR_JOINED 16u
#define CLI_DOOR_ENTRIES 128u
#define CLI_DOOR_LISTENS 32u

// A joined process's place, a cell of the cellblock from its join to the end of its last thread: where it stands among
// the places and how many of its two threads go on; its end and the process at the other end; its key; the lock its
// writes are guarded by; the frame read and a body sent written again; the messages queued for it, their lock and wake,
// and whether its reader goes on; the listeners it owns; and its two threads
typedef struct
{
    unsigned int place;
    unsigned int going;
    CliDoorEnd end;
    int has_peer;
    unsigned int peer;
    char key[CLI_COMMS_KEY];
    CliLock output;
    char frame[CLI_DOOR_FRAME];
    char body[CLI_DOOR_FRAME];
    CliDoorEntry entries[CLI_DOOR_ENTRIES];
    unsigned int first;
    unsigned int queued;
    CliLock queue_lock;
    CliWake queue_wake;
    int reading;
    unsigned int listeners[CLI_DOOR_LISTENS];
    unsigned int listens;
    CliThread reader;
    CliThread teller;
} CliDoorJoined;

// The door's container side: whether it is open and its address, the thread taking the processes that join, their
// places, NULL where none is taken, and the lock they are guarded by
typedef struct
{
    int open;
    char address[CLI_PATH_MAX_LEN];
    CliThread accepter;
    CliDoorJoined *joined[CLI_DOOR_JOINED];
    CliLock lock;
} CliDoorServed;

// the door's container side, and the cells its places and the processes it lists occupy
extern CliDoorServed g_cli_door_served;
extern CellOwner g_cli_door_cells;

// `channel` listened on for the joined process: 1, or 0 with why given; a refusal answered, `size` bytes of why; and an
// answer of `size` bytes of JSON written: each 1, or 0 where its end is broken (door_serve.c)
int cli_door_listen_request(const CliDoorServeChannelCfg *args);
int cli_door_reject(const CliDoorServeWhyCfg *args);
int cli_door_answer(const CliDoorServeCfg *args);

// what a joined process requests, a frame at a time, answered until its end breaks (door_requests.c)
void cli_door_requests_serve(const CliDoorServeCfg *args);

struct CliDoorServeCfg
{
    CliDoorJoined *joined;
    unsigned long long size;
    const char *json;
};

struct CliDoorServeChannelCfg
{
    CliDoorJoined *joined;
    const char *channel;
    VerbaLinea *err_msg;
};

struct CliDoorServeWhyCfg
{
    CliDoorJoined *joined;
    const char *why;
    unsigned long long size;
};

typedef struct
{
    int (*door_listen_request)(const CliDoorServeChannelCfg *args);
} CliDoorServeChannelNs;
EMBED_TABLE_LAYOUT(CliDoorServeChannelNs, door_listen_request);
EMBED_TABLE_STORAGE CliDoorServeChannelNs cli_door_serve_channel EMBED_UNUSED = {
    .door_listen_request = cli_door_listen_request,
};

typedef struct
{
    int (*door_reject)(const CliDoorServeWhyCfg *args);
} CliDoorServeWhyNs;
EMBED_TABLE_LAYOUT(CliDoorServeWhyNs, door_reject);
EMBED_TABLE_STORAGE CliDoorServeWhyNs cli_door_serve_why EMBED_UNUSED = {
    .door_reject = cli_door_reject,
};

typedef struct
{
    int (*door_answer)(const CliDoorServeCfg *args);
    void (*door_requests_serve)(const CliDoorServeCfg *args);
} CliDoorServeNs;
EMBED_TABLE_LAYOUT(CliDoorServeNs, door_answer, door_requests_serve);
EMBED_TABLE_STORAGE CliDoorServeNs cli_door_serve EMBED_UNUSED = {
    .door_answer = cli_door_answer,
    .door_requests_serve = cli_door_requests_serve,
};

#endif
