// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipc_internal.h: what the door's sources share: one end of a connection, the platform's pipe or socket, and the
// frames and regions that go over it
#ifndef DOOR_INTERNAL_H
#define DOOR_INTERNAL_H
#include "door.h"
#include "embed_atomic.h"
#include "embed_dispatch_layout.h"
#include "embed_ticket_ring.h"
#include "transfer.h"

// the most clients in one process, the messages queued for one client as a two's power, and the bytes their rings'
// slots take, the clients' part of the rings block
#define CLI_IPC_CLIENTS 2u
#define CLI_IPC_RECV_SHIFT 6u
#define CLI_IPC_RECV_QUEUE (1u << CLI_IPC_RECV_SHIFT)
#define CLI_DOOR_CLIENTS_RING_BYTES                                                                                    \
    ((unsigned long long)CLI_IPC_CLIENTS * CLI_IPC_RECV_QUEUE * sizeof(EmbedTicketRingSlot))

typedef struct CliIpcErrMsgCtx CliIpcErrMsgCtx;
typedef struct CliIpcListenerCtx CliIpcListenerCtx;
typedef struct CliIpcErrMsgEndCtx CliIpcErrMsgEndCtx;
typedef struct CliIpcErrMsgRegionCtx CliIpcErrMsgRegionCtx;
typedef struct CliIpcErrMsgSizeCtx CliIpcErrMsgSizeCtx;
typedef struct CliIpcEndCtx CliIpcEndCtx;
typedef struct CliIpcEndSizeCtx CliIpcEndSizeCtx;
typedef struct CliIpcCtx CliIpcCtx;
typedef struct CliIpcListenerAddressCtx CliIpcListenerAddressCtx;
typedef struct CliIpcEndBytesCtx CliIpcEndBytesCtx;

// the longest frame taken, in bytes, a longer body going through the transfer layer; the bytes of a frame read into
// the capacity it is read in, and of a body written again; and the bytes of a body stored in a message's buffer, a
// longer one stored in a region
#define CLI_IPC_MAX (16u << 20u)
#define CLI_IPC_FRAME (1u << 18u)
#define CLI_IPC_ENTRY_BODY 4096u

// A frame is its bytes' count in four bytes, least first, then its type's byte and its fields, each text ended by a 0.
// A process's first frame joins by its key; each after it listens on a channel, or sends on one a byte saying where the
// body is and the body or a transfer. The door answers each with a refusal and why, or an answer and what it has to
// say, a name or a count; and it sends what comes on a channel as a message: the channel, who sent it, and the same
// byte and body
#define CLI_IPC_TYPE_JOIN 'J'
#define CLI_IPC_TYPE_LISTEN 'L'
#define CLI_IPC_TYPE_SEND 'S'
#define CLI_IPC_TYPE_REFUSED 'R'
#define CLI_IPC_TYPE_ANSWER 'A'
#define CLI_IPC_TYPE_MESSAGE 'M'
#define CLI_IPC_BODY_INLINE 'B'
#define CLI_IPC_BODY_TRANSFER 'T'

// A message queued: its channel, its speaker's name, its body's size, and its body, in the entry, or in a region where
// it is longer
typedef struct
{
    char channel[CLI_NAME_MAX_LEN];
    char from[CLI_NAME_MAX_LEN];
    unsigned long long size;
    char *body;
    int has_region;
    CliRegion region;
} CliIpcEntry;

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
} CliIpcEnd;

// A listener of its own: on Windows the pipe instance waiting for the next process to join and the pipe's name,
// elsewhere the socket it listens on; and the lock over them, never acquired while a join is waited for, so a second
// listener is made while the first waits
typedef struct
{
    unsigned long long waiting;
#if defined(_WIN32)
    wchar_t name[CLI_PATH_MAX_LEN];
#endif
} CliIpcListener;

// a listener made into `listener`, its address into `address`, sized for CLI_PATH_MAX_LEN; the next process to
// join it taken,
// its end into `end` and the process at the other end into `peer`, where the system reports it; the door at `address`
// joined, waiting up to five seconds while every instance of a pipe is taken; `size` bytes read whole; `size` bytes
// written whole, the region's descriptor going with the first byte on a socket; the next descriptor that came with
// what was read, -1 where none did or on Windows; a reader of the end woken to its end; and the end let go
// (platform/windows_ipc.c, platform/posix_ipc.c)
int cli_ipc_listening(const CliIpcListenerAddressCtx *args);
int cli_ipc_accept(const CliIpcListenerCtx *args);
int cli_ipc_connect(const CliIpcCtx *args);
int cli_ipc_read(const CliIpcEndBytesCtx *args);
int cli_ipc_write(const CliIpcEndSizeCtx *args);
int cli_ipc_descriptor(const CliIpcErrMsgCtx *args);
void cli_ipc_shut(const CliIpcCtx *args);
void cli_ipc_close(const CliIpcCtx *args);

// what reading a frame came to: read whole, the end broken or the frame past the most, or the frame past the capacity
// it is read into, read past and passed over
#define CLI_IPC_FRAME_READ 1
#define CLI_IPC_FRAME_BROKEN 0
#define CLI_IPC_FRAME_PASSED (-1)

// a frame read into `frame`, sized for `capacity`, its size into `size`: what it came to, why given where it broke; a
// frame of `size` bytes at `bytes` written, the region handed over with it where one is given: 1, or 0 where it could
// not be; a text field of `size` bytes of `frame` from `at`: where it ends, past its 0, or 0 where it is not ended;
// what a transfer handing over `region` describes, its length and the handle made in the process `to` where it is given
// on Windows, written into `out`: 1, or 0 with why given; the region the transfer of `length` and `handle` hands over,
// taken from the process `from` where it is given on Windows, else from the descriptors that came with the frame: 1,
// or 0 with why given; and the transfer at `transfer`, `size` bytes, read: 1, or 0 with why given (door.c)
int cli_ipc_frame_read(const CliIpcErrMsgEndCtx *args);
int cli_ipc_frame_write(const CliIpcEndCtx *args);
unsigned long long cli_ipc_field_end(const char *frame, unsigned long long size, unsigned long long at);
int cli_ipc_dispatch(const CliIpcErrMsgRegionCtx *args);
int cli_ipc_transferred_over(const CliIpcErrMsgCtx *args);
int cli_ipc_transfer(const CliIpcErrMsgSizeCtx *args);

struct CliIpcErrMsgCtx
{
    VerbaLinea *err_msg;
    CliIpcEnd *end;
    unsigned long long length;
    unsigned long long handle;
    int has_from;
    unsigned int from;
    CliRegion *region_out;
};

struct CliIpcListenerCtx
{
    CliIpcListener *listener;
    VerbaLinea *err_msg;
    CliIpcEnd *end;
    int *has_peer;
    unsigned int *peer;
};

struct CliIpcErrMsgEndCtx
{
    VerbaLinea *err_msg;
    CliIpcEnd *end;
    char *frame;
    unsigned long long capacity;
    unsigned long long *size_pointer;
};

struct CliIpcErrMsgRegionCtx
{
    VerbaLinea *err_msg;
    const CliRegion *region;
    int has_to;
    unsigned int to;
    VerbaLinea *out;
};

struct CliIpcErrMsgSizeCtx
{
    VerbaLinea *err_msg;
    unsigned long long size;
    const char *transfer;
    unsigned long long *length_pointer;
    unsigned long long *handle_pointer;
};

struct CliIpcEndCtx
{
    CliIpcEnd *end;
    unsigned long long size;
    const CliRegion *region;
    const char *bytes;
};

struct CliIpcEndSizeCtx
{
    CliIpcEnd *end;
    unsigned long long size;
    const void *bytes_read;
    const CliRegion *region;
};

struct CliIpcCtx
{
    VerbaLinea *err_msg;
    CliIpcEnd *end;
    const char *address_read;
};

struct CliIpcListenerAddressCtx
{
    CliIpcListener *listener;
    char *address;
    VerbaLinea *err_msg;
};

struct CliIpcEndBytesCtx
{
    CliIpcEnd *end;
    void *bytes;
    unsigned long long size;
};

typedef struct
{
    int (*listening)(const CliIpcListenerAddressCtx *args);
} CliIpcListenerAddressNs;
EMBED_TABLE_LAYOUT(CliIpcListenerAddressNs, listening);
EMBED_TABLE_STORAGE CliIpcListenerAddressNs cli_ipc_listener_address_internal EMBED_UNUSED = {
    .listening = cli_ipc_listening,
};

typedef struct
{
    int (*accept)(const CliIpcListenerCtx *args);
} CliIpcListenerNs;
EMBED_TABLE_LAYOUT(CliIpcListenerNs, accept);
EMBED_TABLE_STORAGE CliIpcListenerNs cli_ipc_listener_internal EMBED_UNUSED = {
    .accept = cli_ipc_accept,
};

typedef struct
{
    int (*connect)(const CliIpcCtx *args);
    void (*shut)(const CliIpcCtx *args);
    void (*close)(const CliIpcCtx *args);
} CliIpcNsTable;
EMBED_TABLE_LAYOUT(CliIpcNsTable, connect, shut, close);
EMBED_TABLE_STORAGE CliIpcNsTable cli_ipc_internal EMBED_UNUSED = {
    .connect = cli_ipc_connect,
    .shut = cli_ipc_shut,
    .close = cli_ipc_close,
};

typedef struct
{
    int (*read)(const CliIpcEndBytesCtx *args);
} CliIpcEndBytesNs;
EMBED_TABLE_LAYOUT(CliIpcEndBytesNs, read);
EMBED_TABLE_STORAGE CliIpcEndBytesNs cli_ipc_end_bytes_internal EMBED_UNUSED = {
    .read = cli_ipc_read,
};

typedef struct
{
    int (*write)(const CliIpcEndSizeCtx *args);
} CliIpcEndSizeNs;
EMBED_TABLE_LAYOUT(CliIpcEndSizeNs, write);
EMBED_TABLE_STORAGE CliIpcEndSizeNs cli_ipc_end_size_internal EMBED_UNUSED = {
    .write = cli_ipc_write,
};

typedef struct
{
    int (*descriptor)(const CliIpcErrMsgCtx *args);
    int (*transferred_over)(const CliIpcErrMsgCtx *args);
} CliIpcErrMsgNsTable;
EMBED_TABLE_LAYOUT(CliIpcErrMsgNsTable, descriptor, transferred_over);
EMBED_TABLE_STORAGE CliIpcErrMsgNsTable cli_ipc_err_msg_internal EMBED_UNUSED = {
    .descriptor = cli_ipc_descriptor,
    .transferred_over = cli_ipc_transferred_over,
};

typedef struct
{
    int (*frame_read)(const CliIpcErrMsgEndCtx *args);
} CliIpcErrMsgEndNs;
EMBED_TABLE_LAYOUT(CliIpcErrMsgEndNs, frame_read);
EMBED_TABLE_STORAGE CliIpcErrMsgEndNs cli_ipc_err_msg_end_internal EMBED_UNUSED = {
    .frame_read = cli_ipc_frame_read,
};

typedef struct
{
    int (*frame_write)(const CliIpcEndCtx *args);
} CliIpcEndNs;
EMBED_TABLE_LAYOUT(CliIpcEndNs, frame_write);
EMBED_TABLE_STORAGE CliIpcEndNs cli_ipc_end_internal EMBED_UNUSED = {
    .frame_write = cli_ipc_frame_write,
};

typedef struct
{
    int (*dispatch)(const CliIpcErrMsgRegionCtx *args);
} CliIpcErrMsgRegionNs;
EMBED_TABLE_LAYOUT(CliIpcErrMsgRegionNs, dispatch);
EMBED_TABLE_STORAGE CliIpcErrMsgRegionNs cli_ipc_err_msg_region_internal EMBED_UNUSED = {
    .dispatch = cli_ipc_dispatch,
};

typedef struct
{
    int (*transfer)(const CliIpcErrMsgSizeCtx *args);
} CliIpcErrMsgSizeNs;
EMBED_TABLE_LAYOUT(CliIpcErrMsgSizeNs, transfer);
EMBED_TABLE_STORAGE CliIpcErrMsgSizeNs cli_ipc_err_msg_size_internal EMBED_UNUSED = {
    .transfer = cli_ipc_transfer,
};

#endif
