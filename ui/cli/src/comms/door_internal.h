// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_internal.h: what the door's sources share: one end of a connection, the platform's pipe or socket, and the
// frames and regions that go over it
#ifndef DOOR_INTERNAL_H
#define DOOR_INTERNAL_H
typedef struct CliDoorErrMsgCtx CliDoorErrMsgCtx;
typedef struct CliDoorListenerCtx CliDoorListenerCtx;
typedef struct CliDoorErrMsgEndCtx CliDoorErrMsgEndCtx;
typedef struct CliDoorErrMsgRegionCtx CliDoorErrMsgRegionCtx;
typedef struct CliDoorErrMsgSizeCtx CliDoorErrMsgSizeCtx;
typedef struct CliDoorEndCtx CliDoorEndCtx;
typedef struct CliDoorEndSizeCtx CliDoorEndSizeCtx;
typedef struct CliDoorCtx CliDoorCtx;
typedef struct CliDoorListenerAddressCtx CliDoorListenerAddressCtx;
typedef struct CliDoorEndBytesCtx CliDoorEndBytesCtx;

#include "door.h"
#include "embed_dispatch_layout.h"
#include "transfer.h"

// the longest frame taken, in bytes, a longer body going through the transfer layer; the bytes of a frame read into
// the room it is read in, and of a body written again; and the bytes of a body stored in a message's buffer, a longer
// one stored in a region
#define CLI_DOOR_MOST (16u << 20u)
#define CLI_DOOR_FRAME (1u << 18u)
#define CLI_DOOR_ENTRY_BODY 4096u

// A message queued: its channel, its speaker's name, its body's size, and its body, in the entry, or in a region where
// it is longer
typedef struct
{
    char channel[CLI_NAME_MAX_LEN];
    char from[CLI_NAME_MAX_LEN];
    unsigned long long size;
    char body[CLI_DOOR_ENTRY_BODY];
    int has_region;
    CliRegion region;
} CliDoorEntry;

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
    CliDescriptors pending;
#endif
} CliDoorEnd;

// A listener of its own: on Windows the pipe instance waiting for the next process to join and the pipe's name,
// elsewhere the socket it listens on; and the lock over them, never acquired while a join is waited for, so a second
// listener is made while the first waits
typedef struct
{
    unsigned long long waiting;
#if defined(_WIN32)
    wchar_t name[CLI_PATH_MAX_LEN];
#endif
} CliDoorListener;

// a listener made into `listener`, its address into `address`, sized for CLI_PATH_MAX_LEN; the next process to
// join it taken,
// its end into `end` and the process at the other end into `peer`, where the system reports it; the door at `address`
// joined, waiting up to five seconds while every instance of a pipe is taken; `size` bytes read whole; `size` bytes
// written whole, the region's descriptor going with the first byte on a socket; the next descriptor that came with
// what was read, -1 where none did or on Windows; a reader of the end woken to its end; and the end let go
// (platform/windows_door.c, platform/posix_door.c)
int cli_door_listening(const CliDoorListenerAddressCtx *args);
int cli_door_accept(const CliDoorListenerCtx *args);
int cli_door_connect(const CliDoorCtx *args);
int cli_door_read(const CliDoorEndBytesCtx *args);
int cli_door_write(const CliDoorEndSizeCtx *args);
int cli_door_descriptor(const CliDoorErrMsgCtx *args);
void cli_door_shut(const CliDoorCtx *args);
void cli_door_close(const CliDoorCtx *args);

// what reading a frame came to: read whole, the end broken or the frame past the most, or the frame past the room it
// is read into, read past and passed over
#define CLI_DOOR_FRAME_READ 1
#define CLI_DOOR_FRAME_BROKEN 0
#define CLI_DOOR_FRAME_PASSED (-1)

// a frame read into `frame`, sized for `room`, its size into `size`: what it came to, why given where it broke; a
// frame of `size` bytes of `json` written, the region handed over with it where one is given: 1, or 0 where it could
// not be; what a transfer handing over `region` describes, the handle made in the process `to` where it is given on
// Windows, written into `out`: 1, or 0 with why given; the region the transfer of `length` and `handle` hands over,
// taken from the process `from` where it is given on Windows, else from the descriptors that came with the frame: 1,
// or 0 with why given; and the transfer object at `transfer`, `size` bytes, read: 1, or 0 with why given (door.c)
int cli_door_frame_read(const CliDoorErrMsgEndCtx *args);
int cli_door_frame_write(const CliDoorEndCtx *args);
int cli_door_dispatch(const CliDoorErrMsgRegionCtx *args);
int cli_door_handed_over(const CliDoorErrMsgCtx *args);
int cli_door_transfer(const CliDoorErrMsgSizeCtx *args);

struct CliDoorErrMsgCtx
{
    VerbaLinea *err_msg;
    CliDoorEnd *end;
    unsigned long long length;
    unsigned long long handle;
    int has_from;
    unsigned int from;
    CliRegion *region_out;
};

struct CliDoorListenerCtx
{
    CliDoorListener *listener;
    VerbaLinea *err_msg;
    CliDoorEnd *end;
    int *has_peer;
    unsigned int *peer;
};

struct CliDoorErrMsgEndCtx
{
    VerbaLinea *err_msg;
    CliDoorEnd *end;
    char *frame;
    unsigned long long room;
    unsigned long long *size_pointer;
};

struct CliDoorErrMsgRegionCtx
{
    VerbaLinea *err_msg;
    const CliRegion *region;
    int has_to;
    unsigned int to;
    VerbaLinea *out;
};

struct CliDoorErrMsgSizeCtx
{
    VerbaLinea *err_msg;
    unsigned long long size;
    const char *transfer;
    unsigned long long *length_pointer;
    unsigned long long *handle_pointer;
};

struct CliDoorEndCtx
{
    CliDoorEnd *end;
    unsigned long long size;
    const CliRegion *region;
    const char *json;
};

struct CliDoorEndSizeCtx
{
    CliDoorEnd *end;
    unsigned long long size;
    const void *bytes_read;
    const CliRegion *region;
};

struct CliDoorCtx
{
    VerbaLinea *err_msg;
    CliDoorEnd *end;
    const char *address_read;
};

struct CliDoorListenerAddressCtx
{
    CliDoorListener *listener;
    char *address;
    VerbaLinea *err_msg;
};

struct CliDoorEndBytesCtx
{
    CliDoorEnd *end;
    void *bytes;
    unsigned long long size;
};

typedef struct
{
    int (*listening)(const CliDoorListenerAddressCtx *args);
} CliDoorListenerAddressNs;
EMBED_TABLE_LAYOUT(CliDoorListenerAddressNs, listening);
EMBED_TABLE_STORAGE CliDoorListenerAddressNs cli_door_listener_address_internal EMBED_UNUSED = {
    .listening = cli_door_listening,
};

typedef struct
{
    int (*accept)(const CliDoorListenerCtx *args);
} CliDoorListenerNs;
EMBED_TABLE_LAYOUT(CliDoorListenerNs, accept);
EMBED_TABLE_STORAGE CliDoorListenerNs cli_door_listener_internal EMBED_UNUSED = {
    .accept = cli_door_accept,
};

typedef struct
{
    int (*connect)(const CliDoorCtx *args);
    void (*shut)(const CliDoorCtx *args);
    void (*close)(const CliDoorCtx *args);
} CliDoorNsTable;
EMBED_TABLE_LAYOUT(CliDoorNsTable, connect, shut, close);
EMBED_TABLE_STORAGE CliDoorNsTable cli_door_internal EMBED_UNUSED = {
    .connect = cli_door_connect,
    .shut = cli_door_shut,
    .close = cli_door_close,
};

typedef struct
{
    int (*read)(const CliDoorEndBytesCtx *args);
} CliDoorEndBytesNs;
EMBED_TABLE_LAYOUT(CliDoorEndBytesNs, read);
EMBED_TABLE_STORAGE CliDoorEndBytesNs cli_door_end_bytes_internal EMBED_UNUSED = {
    .read = cli_door_read,
};

typedef struct
{
    int (*write)(const CliDoorEndSizeCtx *args);
} CliDoorEndSizeNs;
EMBED_TABLE_LAYOUT(CliDoorEndSizeNs, write);
EMBED_TABLE_STORAGE CliDoorEndSizeNs cli_door_end_size_internal EMBED_UNUSED = {
    .write = cli_door_write,
};

typedef struct
{
    int (*descriptor)(const CliDoorErrMsgCtx *args);
    int (*handed_over)(const CliDoorErrMsgCtx *args);
} CliDoorErrMsgNsTable;
EMBED_TABLE_LAYOUT(CliDoorErrMsgNsTable, descriptor, handed_over);
EMBED_TABLE_STORAGE CliDoorErrMsgNsTable cli_door_err_msg_internal EMBED_UNUSED = {
    .descriptor = cli_door_descriptor,
    .handed_over = cli_door_handed_over,
};

typedef struct
{
    int (*frame_read)(const CliDoorErrMsgEndCtx *args);
} CliDoorErrMsgEndNs;
EMBED_TABLE_LAYOUT(CliDoorErrMsgEndNs, frame_read);
EMBED_TABLE_STORAGE CliDoorErrMsgEndNs cli_door_err_msg_end_internal EMBED_UNUSED = {
    .frame_read = cli_door_frame_read,
};

typedef struct
{
    int (*frame_write)(const CliDoorEndCtx *args);
} CliDoorEndNs;
EMBED_TABLE_LAYOUT(CliDoorEndNs, frame_write);
EMBED_TABLE_STORAGE CliDoorEndNs cli_door_end_internal EMBED_UNUSED = {
    .frame_write = cli_door_frame_write,
};

typedef struct
{
    int (*dispatch)(const CliDoorErrMsgRegionCtx *args);
} CliDoorErrMsgRegionNs;
EMBED_TABLE_LAYOUT(CliDoorErrMsgRegionNs, dispatch);
EMBED_TABLE_STORAGE CliDoorErrMsgRegionNs cli_door_err_msg_region_internal EMBED_UNUSED = {
    .dispatch = cli_door_dispatch,
};

typedef struct
{
    int (*transfer)(const CliDoorErrMsgSizeCtx *args);
} CliDoorErrMsgSizeNs;
EMBED_TABLE_LAYOUT(CliDoorErrMsgSizeNs, transfer);
EMBED_TABLE_STORAGE CliDoorErrMsgSizeNs cli_door_err_msg_size_internal EMBED_UNUSED = {
    .transfer = cli_door_transfer,
};

#endif
