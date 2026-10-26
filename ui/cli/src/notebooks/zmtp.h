// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// zmtp.h: ZeroMQ's wire protocol as a Jupyter kernel uses it (zmtp.c): a connection to one of a peer's sockets,
// greeted as the type named, and messages of frames sent and received
#ifndef ZMTP_H
#define ZMTP_H

#include "../command_line/cli_internal.h"

// one frame of a message: its bytes and their count
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
} CliZmtpFrame;

// a connection to `host`:`port` as a socket of the type named, DEALER, SUB or REQ, the peer greeted: 1, or 0 with why
// given
int cli_zmtp_connected(CliSocket *socket, const char *host, unsigned int port, const char *kind, VerbaLinea *err_msg);
// a message of the frames given written whole as the wire carries it, each frame's head before its body, in a cell of
// its size the caller gives back with g_cli_cells->persistent_buf_release, its size into `size`: the message, or NULL
// where the cells cannot meet it; and the frames given sent as one message in one write: 1, or 0 where the connection
// failed or the cells cannot meet the message
unsigned char *cli_zmtp_message_made(const CliZmtpFrame *frames, unsigned int count, unsigned long long *size);
int cli_zmtp_sent(const CliSocket *socket, const CliZmtpFrame *frames, unsigned int count);
// the next message's frames, the commands between messages passed over, each body in `room`, a message larger than
// the room passed over whole and the next one read: their count, or 0 where the connection failed
unsigned int cli_zmtp_received(const CliSocket *socket, unsigned char *room, unsigned long long room_size,
                               CliZmtpFrame *frames, unsigned int most);

#endif
