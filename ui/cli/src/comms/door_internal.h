// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_internal.h: what the door's sources share: one end of a connection, the platform's pipe or socket, and the
// frames and regions that go over it
#ifndef DOOR_INTERNAL_H
#define DOOR_INTERNAL_H

#include "door.h"
#include "transfer.h"

// the longest frame taken, in bytes, a longer body going through the transfer layer; the bytes of a frame read into
// the room it is read in, and of a body written again; and the bytes of a body kept in a message's slot, a longer one
// kept in a region
#define CLI_DOOR_MOST (16u << 20u)
#define CLI_DOOR_FRAME (1u << 18u)
#define CLI_DOOR_SLOT_BODY 4096u

// A message queued: its channel, its speaker's name, its body's size, and its body, in the slot, or in a region where
// it is longer
typedef struct
{
    char channel[CLI_NAME_LONGEST];
    char from[CLI_NAME_LONGEST];
    unsigned long long size;
    char body[CLI_DOOR_SLOT_BODY];
    int has_region;
    CliRegion region;
} CliDoorSlot;

// One end of a connection: the pipe's handle on Windows, the socket's descriptor elsewhere, whether it is open, on
// Windows the event set once it is shut, which no read or write after it waits past, and on a socket the descriptors
// that came with what was read
typedef struct
{
    unsigned long long handle;
    int open;
#if defined(_WIN32)
    unsigned long long shut;
#endif
#if !defined(_WIN32)
    CliDescriptors kept;
#endif
} CliDoorEnd;

// A listener of its own: on Windows the pipe instance waiting for the next process to join and the pipe's name,
// elsewhere the socket it listens on; and the lock over them, never held while a join is waited for, so a second
// listener is made while the first waits
typedef struct
{
    unsigned long long waiting;
    CliLock lock;
#if defined(_WIN32)
    wchar_t name[CLI_PATH_LONGEST];
#endif
} CliDoorListener;

// a listener made into `listener`, its address into `address`, which holds CLI_PATH_LONGEST; the next process to
// join it taken,
// its end into `end` and the process at the other end into `peer`, where the system says it; the door at `address`
// joined, waiting up to five seconds while every instance of a pipe is taken; `size` bytes read whole; `size` bytes
// written whole, the region's descriptor going with the first byte on a socket; the next descriptor that came with
// what was read, -1 where none did or on Windows; a reader of the end woken to its end; and the end let go
// (platform/windows_door.c, platform/posix_door.c)
int cli_door_listening(CliDoorListener *listener, char *address, CliLine *said);
int cli_door_accepted(CliDoorListener *listener, CliDoorEnd *end, int *has_peer, unsigned int *peer, CliLine *said);
int cli_door_connected(const char *address, CliDoorEnd *end, CliLine *said);
int cli_door_read(CliDoorEnd *end, void *bytes, unsigned long long size);
int cli_door_written(CliDoorEnd *end, const void *bytes, unsigned long long size, const CliRegion *region);
int cli_door_descriptor(CliDoorEnd *end);
void cli_door_shut(CliDoorEnd *end);
void cli_door_closed(CliDoorEnd *end);

// what reading a frame came to: read whole, the end broken or the frame past the most, or the frame past the room it
// is read into, read past and passed over
#define CLI_DOOR_FRAME_READ 1
#define CLI_DOOR_FRAME_BROKEN 0
#define CLI_DOOR_FRAME_PASSED (-1)

// a frame read into `frame`, which holds `room`, its size into `size`: what it came to, why said where it broke; a
// frame of `size` bytes of `json` written, the region handed over with it where one is given: 1, or 0 where it could
// not be; what a transfer handing over `region` says, the handle made in the process `to` where it is given on
// Windows, written into `out`: 1, or 0 with why said; the region the transfer of `length` and `handle` hands over,
// taken from the process `from` where it is given on Windows, else from the descriptors that came with the frame: 1,
// or 0 with why said; and the transfer object at `transfer`, `size` bytes, read: 1, or 0 with why said (door.c)
int cli_door_frame_read(CliDoorEnd *end, char *frame, unsigned long long room, unsigned long long *size, CliLine *said);
int cli_door_frame_written(CliDoorEnd *end, const char *json, unsigned long long size, const CliRegion *region);
int cli_door_handed(const CliRegion *region, int has_to, unsigned int to, CliLine *out, CliLine *said);
int cli_door_handed_over(CliDoorEnd *end, unsigned long long length, unsigned long long handle, int has_from,
                         unsigned int from, CliRegion *region, CliLine *said);
int cli_door_transfer(const char *transfer, unsigned long long size, unsigned long long *length,
                      unsigned long long *handle, CliLine *said);

#endif
