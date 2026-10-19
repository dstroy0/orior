// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// zmtp.h: ZeroMQ's wire protocol as a Jupyter kernel speaks it (zmtp.c): a connection to one of a peer's sockets,
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
// said
int cli_zmtp_connected(CliSocket *socket, const char *host, unsigned int port, const char *kind, CliLine *said);
// a message of the frames given sent: 1, or 0 where the connection failed
int cli_zmtp_sent(const CliSocket *socket, const CliZmtpFrame *frames, unsigned int count);
// the next message's frames, the commands between messages passed over, each body in `room`, a message larger than
// the room passed over whole and the next one read: their count, or 0 where the connection failed
unsigned int cli_zmtp_received(const CliSocket *socket, unsigned char *room, unsigned long long room_size,
                               CliZmtpFrame *frames, unsigned int most);

#endif
