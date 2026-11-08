// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// zmtp.h: ZeroMQ's wire protocol as a Jupyter kernel uses it (zmtp.c): a connection to one of a peer's sockets,
// greeted as the type named, and messages of frames sent and received
#ifndef ZMTP_H
#define ZMTP_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliZmtpSocketHostCfg CliZmtpSocketHostCfg;
typedef struct CliZmtpSocketCfg CliZmtpSocketCfg;
typedef struct CliZmtpCfg CliZmtpCfg;
typedef struct CliZmtpFramesCountCfg CliZmtpFramesCountCfg;

// one frame of a message: its bytes and their count
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
} CliZmtpFrame;

// a connection to `host`:`port` as a socket of the type named, DEALER, SUB or REQ, the peer greeted: 1, or 0 with why
// given
int cli_zmtp_connect(const CliZmtpSocketHostCfg *args);
// a message of the frames given written whole as the wire carries it, each frame's head before its body, in a cell of
// its size the caller gives back with g_cli_guard->persistent_buf_release, its size into `size`: the message, or NULL
// where the cells cannot meet it; and the frames given sent as one message in one write: 1, or 0 where the connection
// failed or the cells cannot meet the message
unsigned char *cli_zmtp_message_init(const CliZmtpCfg *args);
int cli_zmtp_send(const CliZmtpFramesCountCfg *args);
// the next message's frames, the commands between messages passed over, each body in `capacity`, a message larger than
// the capacity passed over whole and the next one read: their count, or 0 where the connection failed
unsigned int cli_zmtp_receive(const CliZmtpSocketCfg *args);

struct CliZmtpSocketHostCfg
{
    CliSocket *socket_out;
    const char *host;
    unsigned int port;
    const char *type;
    VerbaLinea *err_msg;
};

struct CliZmtpSocketCfg
{
    const CliSocket *socket;
    unsigned char *capacity;
    unsigned long long capacity_bytes;
    CliZmtpFrame *frames_out;
    unsigned int most;
};

struct CliZmtpCfg
{
    const CliZmtpFrame *frames;
    unsigned int count;
    unsigned long long *size;
};

struct CliZmtpFramesCountCfg
{
    const CliZmtpFrame *frames;
    unsigned int count;
    const CliSocket *socket;
};

typedef struct
{
    int (*connect)(const CliZmtpSocketHostCfg *args);
} CliZmtpSocketHostNs;
EMBED_TABLE_LAYOUT(CliZmtpSocketHostNs, connect);
EMBED_TABLE_STORAGE CliZmtpSocketHostNs cli_zmtp_socket_host EMBED_UNUSED = {
    .connect = cli_zmtp_connect,
};

typedef struct
{
    unsigned char *(*message_init)(const CliZmtpCfg *args);
} CliZmtpNs;
EMBED_TABLE_LAYOUT(CliZmtpNs, message_init);
EMBED_TABLE_STORAGE CliZmtpNs cli_zmtp EMBED_UNUSED = {
    .message_init = cli_zmtp_message_init,
};

typedef struct
{
    int (*send)(const CliZmtpFramesCountCfg *args);
} CliZmtpFramesCountNs;
EMBED_TABLE_LAYOUT(CliZmtpFramesCountNs, send);
EMBED_TABLE_STORAGE CliZmtpFramesCountNs cli_zmtp_frames_count EMBED_UNUSED = {
    .send = cli_zmtp_send,
};

typedef struct
{
    unsigned int (*receive)(const CliZmtpSocketCfg *args);
} CliZmtpSocketNs;
EMBED_TABLE_LAYOUT(CliZmtpSocketNs, receive);
EMBED_TABLE_STORAGE CliZmtpSocketNs cli_zmtp_socket EMBED_UNUSED = {
    .receive = cli_zmtp_receive,
};

#endif
