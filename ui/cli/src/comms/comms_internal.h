// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_internal.h: what the comms sources share: the parts, the listeners and each channel's traffic, under one lock;
// and what a grant names (comms_grants.c)
#ifndef COMMS_INTERNAL_H
#define COMMS_INTERNAL_H

#include "../command_line/cells.h"
#include "comms.h"

// the most parts, listeners and channels, and the places each of their tables is first taken with; how many of each
// channel's last messages are stored, and the bytes past which a body stored among them is given by its size
#define CLI_COMMS_PARTS 256u
#define CLI_COMMS_LISTENERS 512u
#define CLI_COMMS_CHANNELS 256u
#define CLI_COMMS_STORED_MAX 20u
#define CLI_COMMS_STORED_BODY_MAX 2048u

// the cells the comms layer owns: its tables, each part's grants, and each message stored (comms.c)
extern CellOwner g_cli_comms_cells;

// A part: whether the place is taken, its key, its name, the process it was given, whether the door takes its key
// from any process, its grants to listen and to send, both in one cell of their count, `listen` its first byte and
// `send_grant` past the grants to listen, and how many messages it sent and had refused
typedef struct
{
    int taken;
    char key[CLI_COMMS_KEY];
    char name[CLI_NAME_MAX_LEN];
    int has_process;
    unsigned int process;
    int persistent;
    char (*listen)[CLI_NAME_MAX_LEN];
    unsigned int listens;
    char (*send_grant)[CLI_NAME_MAX_LEN];
    unsigned int send_grants;
    unsigned long long sent_count;
    unsigned long long refused;
} CliCommsPart;

// A listener: whether the place is taken, the part it is of, the channel or grant it listens on, whether it is
// handed each message under the lock, and where messages go
typedef struct
{
    int taken;
    unsigned int part;
    char pattern[CLI_NAME_MAX_LEN];
    int queued;
    CliCommsRecvFn recv;
    void *context;
} CliCommsListener;

// A message stored as a channel's last, in a cell of `room` bytes, `from` its first byte: the part that sent it, ended
// by a 0, and its body after, whole where it is shorter than CLI_COMMS_STORED_BODY_MAX and else given by its size,
// `size` bytes
typedef struct
{
    char *from;
    const char *body;
    unsigned long long size;
    unsigned long long room;
} CliCommsStoredMsg;

// A channel's traffic: its name, how many messages and bytes it carried, the bytes that went through the transfer
// layer, how many messages it refused, and its last messages, oldest first from `first`
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    unsigned long long messages;
    unsigned long long bytes;
    unsigned long long transferred;
    unsigned long long refused;
    CliCommsStoredMsg last[CLI_COMMS_STORED_MAX];
    unsigned int first;
    unsigned int count;
} CliCommsTraffic;

// The comms layer: its parts, listeners and channels, each table in a cell of `*_room` places at their most, and their
// lock
typedef struct
{
    CliCommsPart *parts;
    unsigned int parts_room;
    CliCommsListener *listeners;
    unsigned int listeners_room;
    CliCommsTraffic *traffic;
    unsigned int channels;
    unsigned int channels_room;
    CliLock lock;
} CliCommsRoom;

extern CliCommsRoom g_cli_comms;

#endif
