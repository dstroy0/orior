// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_serve.h: the door's container side: each joined process's place, its end, its key, what it listens on, and the
// queue of what it hears (door_serve.c, door_asked.c)
#ifndef DOOR_SERVE_H
#define DOOR_SERVE_H

#include "comms.h"
#include "door_internal.h"

// the most processes joined at once; the messages queued for one process; and the channels one process listens on
#define CLI_DOOR_JOINED 16u
#define CLI_DOOR_SLOTS 128u
#define CLI_DOOR_LISTENS 32u

// A joined process's place, a cell of the cellblock from its join to the end of its last thread: where it stands among
// the places and how many of its two threads go on; its end and the process at the other end; its key; the lock its
// writes are held by; the frame read and a body said written again; the messages queued for it, their lock and wake,
// and whether its reader goes on; the listeners it holds; and its two threads
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
    CliDoorSlot slots[CLI_DOOR_SLOTS];
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
// places, NULL where none is taken, and the lock they are held by
typedef struct
{
    int open;
    char address[CLI_PATH_LONGEST];
    CliThread accepter;
    CliDoorJoined *joined[CLI_DOOR_JOINED];
    CliLock lock;
} CliDoorServed;

// the door's container side, and the cells its places and the processes it lists hold
extern CliDoorServed g_cli_door_served;
extern CliCellsTally g_cli_door_cells;

// `channel` listened on for the joined process: 1, or 0 with why said; a refusal answered, `size` bytes of why; and an
// answer of `size` bytes of JSON written: each 1, or 0 where its end is broken (door_serve.c)
int cli_door_listen_asked(CliDoorJoined *joined, const char *channel, CliLine *said);
int cli_door_refused(CliDoorJoined *joined, const char *why, unsigned long long size);
int cli_door_answered(CliDoorJoined *joined, const char *json, unsigned long long size);

// what a joined process asks, a frame at a time, answered until its end breaks (door_asked.c)
void cli_door_asked(CliDoorJoined *joined);

#endif
