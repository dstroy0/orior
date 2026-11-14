// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface.h: the comms layer reached from another process: a named pipe of the container's own on Windows, which takes no
// one from another machine, and elsewhere a socket in a folder only the reader can open.
//
// A frame is the length of its JSON in four bytes, least first, then the JSON. A process joins by its first frame,
// {"key": its key}; the container takes it only where the key is a part's and the key was given to the process at the
// other end, as the system reports, or to a process it is under, as a shell of the window's is given a key for
// every program run in it. A part registered as persistent is taken from any process with its key, which is stored
// where only the reader can read it. Then the process requests {"listen": channel} and {"send": channel, "body": what},
// each answered with
// {"ok": what was done} or {"refused": why}, and each message sent on a channel it listens on comes as {"message":
// {"channel", "from", "body"}}.
//
// A body longer than the transfer layer's SMALL goes through it: the frame contains {"transfer": {"length"}} where the
// body would be, and the region passed over with the frame contains the body's JSON. On Windows the transfer names the
// region's handle too, the one the sending process has, which the container takes from it, or the one the container
// made in the listening process (interface.c, ipc_serve.c, ipc_client.c, platform/windows_ipc.c, platform/posix_ipc.c)
#ifndef INTERFACE_H
#define INTERFACE_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliIpcErrMsgCfg CliIpcErrMsgCfg;
typedef struct CliIpcClientCfg CliIpcClientCfg;
typedef struct CliIpcErrMsgAddressCfg CliIpcErrMsgAddressCfg;
typedef struct CliIpcAddressCfg CliIpcAddressCfg;
typedef struct CliIpcCfg CliIpcCfg;

// One process's end of the interface: what it joined as, the answers to what it requests, and the messages on the channels
// it listens on
typedef struct CliIpcClient CliIpcClient;

// the interface opened to the comms layer, once, its address into `address`, sized for CLI_PATH_MAX_LEN: 1, or 0 with why
// given (ipc_serve.c)
int cli_ipc_open(const CliIpcAddressCfg *args);

// the interface at `address` joined with `key`: the client, or NULL with why given; the interface the variables name joined with
// the key they contain, where this process was started with both, NULL where it was not or the interface refused; the name
// the client joined as; and the client let go (ipc_client.c)
CliIpcClient *cli_ipc_join(const CliIpcErrMsgAddressCfg *args);
const char *cli_ipc_name(const CliIpcCfg *args);
void cli_ipc_leave(const CliIpcClientCfg *args);

// `channel` listened on; `size` bytes of `body`, JSON, sent on `channel`, through the transfer layer where it is long,
// how many listeners received it into `delivered`; each 1, or 0 with why given; and the next message on a channel the
// client listens on, waited for up to `wait` milliseconds, its channel and its speaker's name each into what is sized
// for CLI_NAME_MAX_LEN and its body into `body`: 1, or 0 where none came
int cli_ipc_listen(const CliIpcErrMsgCfg *args);
int cli_ipc_send(const CliIpcErrMsgCfg *args);
int cli_ipc_next(const CliIpcClientCfg *args);

struct CliIpcErrMsgCfg
{
    VerbaLinea *err_msg;
    CliIpcClient *client;
    const char *channel;
    const char *body;
    unsigned long long size;
    unsigned long long *delivered;
};

struct CliIpcClientCfg
{
    CliIpcClient *client;
    unsigned long long wait;
    char *channel_out;
    char *from;
    VerbaLinea *body_verba_linea_pointer;
};

struct CliIpcErrMsgAddressCfg
{
    VerbaLinea *err_msg;
    const char *address_read;
    const char *key;
};

struct CliIpcAddressCfg
{
    char *address;
    VerbaLinea *err_msg;
};

struct CliIpcCfg
{
    const CliIpcClient *client_read;
};

typedef struct
{
    int (*open)(const CliIpcAddressCfg *args);
} CliIpcAddressNs;
EMBED_TABLE_LAYOUT(CliIpcAddressNs, open);
EMBED_TABLE_STORAGE CliIpcAddressNs cli_ipc_address EMBED_UNUSED = {
    .open = cli_ipc_open,
};

typedef struct
{
    CliIpcClient *(*join)(const CliIpcErrMsgAddressCfg *args);
} CliIpcErrMsgAddressNs;
EMBED_TABLE_LAYOUT(CliIpcErrMsgAddressNs, join);
EMBED_TABLE_STORAGE CliIpcErrMsgAddressNs cli_ipc_err_msg_address EMBED_UNUSED = {
    .join = cli_ipc_join,
};

typedef struct
{
    const char *(*name)(const CliIpcCfg *args);
} CliIpcNs;
EMBED_TABLE_LAYOUT(CliIpcNs, name);
EMBED_TABLE_STORAGE CliIpcNs cli_ipc_table EMBED_UNUSED = {
    .name = cli_ipc_name,
};

typedef struct
{
    void (*leave)(const CliIpcClientCfg *args);
    int (*next)(const CliIpcClientCfg *args);
} CliIpcClientNs;
EMBED_TABLE_LAYOUT(CliIpcClientNs, leave, next);
EMBED_TABLE_STORAGE CliIpcClientNs cli_ipc_client EMBED_UNUSED = {
    .leave = cli_ipc_leave,
    .next = cli_ipc_next,
};

typedef struct
{
    int (*listen)(const CliIpcErrMsgCfg *args);
    int (*send)(const CliIpcErrMsgCfg *args);
} CliIpcErrMsgNs;
EMBED_TABLE_LAYOUT(CliIpcErrMsgNs, listen, send);
EMBED_TABLE_STORAGE CliIpcErrMsgNs cli_ipc_err_msg EMBED_UNUSED = {
    .listen = cli_ipc_listen,
    .send = cli_ipc_send,
};

#endif
