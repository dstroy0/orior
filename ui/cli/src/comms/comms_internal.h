// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_internal.h: what the comms sources share: the parts, the listeners and each channel's traffic, under one lock;
// and what a grant names (comms_grants.c)
#ifndef COMMS_INTERNAL_H
#define COMMS_INTERNAL_H

#include "../command_line/pool.h"
#include "comms.h"
#include "embed_atomic.h"
#include "memoria_anularis/memoria_anularis.h"

// the max parts, listeners and channels, and the places each of their tables is first taken with; how many of each
// channel's last messages are stored, the segments of the ring that stores them, a power of two over that, and the
// bytes past which a body stored among them is given by its size
#define CLI_COMMS_PARTS 256u
#define CLI_COMMS_LISTENERS 512u
#define CLI_COMMS_CHANNELS 256u
#define CLI_COMMS_STORED_MAX 20u
#define CLI_COMMS_STORED_RING 32u
#define CLI_COMMS_STORED_BODY_MAX 2048u
EMBED_STATIC_ASSERT(MMGR_RING_POW2(CLI_COMMS_STORED_RING) && (CLI_COMMS_STORED_RING >= CLI_COMMS_STORED_MAX),
                    "a channel's stored ring has a power of two of segments over the messages stored");

// A part: whether the place is taken, its key, its name, the process it was given, whether the interface takes its key
// from any process, its grants to listen and to send, both in one movable cell of their count, the grants to listen
// first and those to send after them, each CLI_NAME_MAX_LEN bytes, and how many messages it sent and had refused
typedef struct
{
    int reserved;
    char key[CLI_COMMS_KEY];
    char name[CLI_NAME_MAX_LEN];
    int has_process;
    unsigned int process;
    int persistent;
    const CarcerMovableEntry *grants;
    unsigned int listens;
    unsigned int send_grants;
    unsigned long long sent_count;
    unsigned long long refused;
} CliCommsPart;

// A listener: whether the place is taken, the part it is of, the channel or grant it listens on, whether it is
// passed each message under the lock, and where messages go
typedef struct
{
    int reserved;
    unsigned int part;
    char pattern[CLI_NAME_MAX_LEN];
    int queued;
    CliCommsRecvFn recv;
    void *context;
} CliCommsListener;

// A message stored as a channel's last, in a movable cell of `capacity` bytes: the part that sent it, ended by a 0,
// `from_size` bytes with the 0, and its body after, whole where it is shorter than CLI_COMMS_STORED_BODY_MAX and else
// given by its size, `size` bytes
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long from_size;
    unsigned long long size;
    unsigned long long capacity;
} CliCommsStoredMsg;

// A channel's traffic: its name, how many messages and bytes it carried, the bytes that went through the transfer
// layer, and how many messages it refused; its last messages are in the stored ring at the traffic's place
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    unsigned long long messages;
    unsigned long long bytes;
    unsigned long long transferred;
    unsigned long long refused;
} CliCommsTraffic;

// A channel's last messages, MMgr's ring over its segments, a stored message each, the messages the channel's span of
// the rings block; the bytes one channel's messages take, and the bytes every channel's take, the comms' part of it
typedef struct
{
    mmgr_ring ring;
    CliCommsStoredMsg *last;
} CliCommsStored;
#define CLI_COMMS_STORED_BYTES ((unsigned long long)CLI_COMMS_STORED_RING * sizeof(CliCommsStoredMsg))
#define CLI_COMMS_RING_BYTES ((unsigned long long)CLI_COMMS_CHANNELS * CLI_COMMS_STORED_BYTES)

// The comms layer: its parts, listeners and channels, the three tables in one movable cell of `*_capacity` places at
// their max, NULL until the first is needed; each channel's stored ring, CLI_COMMS_CHANNELS of them fixed outside the
// pool and emptied with the tables, a ring positioned as its channel's traffic is made; and the change stamp, moved on by
// every register, bind, leave, listen, unlisten, send and transfer, which the view compares with the stamp it last read
// so that it lists the layer again only where the layer moved
typedef struct
{
    const CarcerMovableEntry *tables;
    CliCommsStored *stored;
    unsigned int parts_capacity;
    unsigned int listeners_capacity;
    unsigned int channels;
    unsigned int channels_capacity;
    embed_atomic_u32 stamp;
} CliCommsWorkState;

extern CliCommsWorkState g_cli_comms;

// where each table starts in the tables' movable cell, and the cell's bytes: the parts, the listeners, then each
// channel's traffic; a record is read and written at its place through the guard
#define COMMS_PARTS_AT 0ULL
#define COMMS_LISTENERS_AT (COMMS_PARTS_AT + ((unsigned long long)CLI_COMMS_PARTS * sizeof(CliCommsPart)))
#define COMMS_TRAFFIC_AT (COMMS_LISTENERS_AT + ((unsigned long long)CLI_COMMS_LISTENERS * sizeof(CliCommsListener)))
#define COMMS_TABLES_BYTES (COMMS_TRAFFIC_AT + ((unsigned long long)CLI_COMMS_CHANNELS * sizeof(CliCommsTraffic)))

#endif
