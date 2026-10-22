// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// zmtp.c: ZeroMQ's wire protocol, ZMTP 3.0 with its NULL mechanism, as a Jupyter kernel speaks it on the machine's own
// loopback: the greeting, the READY command that names a socket's type, and messages of one or more frames. orior_cli is
// the connecting side of each of a kernel's sockets, a DEALER to its ROUTERs, a SUB to its PUB and a REQ to its REP. A
// SUB subscribes as ZMTP 3.0 has it, by a message whose first byte is 1, which a peer that speaks 3.1 takes from a 3.0
// peer as well
#include "zmtp.h"

#define CLI_ZMTP_MORE 0x01u

#define CLI_ZMTP_LONG 0x02u

#define CLI_ZMTP_COMMAND 0x04u

// one frame written: its flags, its size, one byte or eight, and its body
static int cli_zmtp_frame_written(const CliSocket *socket, const void *body, unsigned long long size,
                                  unsigned int flags)
{
    unsigned char head[9];
    unsigned int length = 0u;
    if (size > 255ull)
    {
        head[0] = (unsigned char)(flags | CLI_ZMTP_LONG);
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            head[1u + at] = (unsigned char)((size >> (56u - (8u * at))) & 0xffull);
        }
        length = 9u;
    }
    else
    {
        head[0] = (unsigned char)flags;
        // a size at most 255 fits a byte
        head[1] = (unsigned char)size;
        length = 2u;
    }
    return cli_socket_written(socket, head, length) && cli_socket_written(socket, body, size);
}

// what reading a frame came to: the connection failed, the frame read, or its body larger than the room and passed
// over
#define CLI_ZMTP_FRAME_FAILED 0
#define CLI_ZMTP_FRAME_READ 1
#define CLI_ZMTP_FRAME_PASSED 2

// one frame read: its flags and its body into `body`, which holds `room`, its size into `size`: what the read came to
static int cli_zmtp_frame_read(const CliSocket *socket, unsigned int *flags, unsigned char *body,
                               unsigned long long room, unsigned long long *size)
{
    unsigned char first = 0u;
    if (!cli_socket_read(socket, &first, 1ull))
    {
        return 0;
    }
    *flags = first;
    *size = 0ull;
    if ((first & CLI_ZMTP_LONG) != 0u)
    {
        unsigned char length[8];
        if (!cli_socket_read(socket, length, 8ull))
        {
            return 0;
        }
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            *size = (*size << 8u) | length[at];
        }
    }
    else
    {
        unsigned char length = 0u;
        if (!cli_socket_read(socket, &length, 1ull))
        {
            return 0;
        }
        *size = length;
    }
    if (*size <= room)
    {
        return cli_socket_read(socket, body, *size) ? CLI_ZMTP_FRAME_READ : CLI_ZMTP_FRAME_FAILED;
    }
    // a body larger than the room is read and let go, so that the next frame is read where it starts
    unsigned char piece[4096];
    for (unsigned long long left = *size; left > 0ull;)
    {
        const unsigned long long taken = (left < sizeof(piece)) ? left : sizeof(piece);
        if (!cli_socket_read(socket, piece, taken))
        {
            return CLI_ZMTP_FRAME_FAILED;
        }
        left -= taken;
    }
    return CLI_ZMTP_FRAME_PASSED;
}

// the greeting: ZMTP 3, the NULL mechanism, and the READY command that names the socket's type
static int cli_zmtp_greeted(const CliSocket *socket, const char *kind, CliLine *said)
{
    unsigned char greeting[64];
    memor.set(&(MemoriaCfg){.dst = greeting, .val = 0, .bytes = sizeof(greeting)});
    greeting[0] = 0xffu;
    greeting[9] = 0x7fu;
    greeting[10] = 3u;
    greeting[11] = 0u;
    memor.cpy(&(MemoriaCfg){.dst = &greeting[12], .src = "NULL", .bytes = 4u});
    unsigned char theirs[64];
    if (!cli_socket_written(socket, greeting, sizeof(greeting)) || !cli_socket_read(socket, theirs, sizeof(theirs)))
    {
        cli_line_text(said, "the kernel's greeting was not read");
        return 0;
    }
    if ((theirs[0] != 0xffu) || (theirs[9] != 0x7fu) || (theirs[10] < 3u))
    {
        cli_line_text(said, "the peer speaks no ZMTP 3");
        return 0;
    }
    if (memor.cmp(&(MemoriaCfg){.src = &theirs[12], .other = "NULL", .bytes = 4u}) != 0)
    {
        cli_line_text(said, "the peer asks for the ");
        unsigned int length = 0u;
        while ((length < 20u) && (theirs[12u + length] != 0u))
        {
            length += 1u;
        }
        cli_line_counted(said, (const char *)&theirs[12], length);
        cli_line_text(said, " mechanism, where orior_cli speaks NULL");
        return 0;
    }
    unsigned char ready[64];
    const size_t kind_length = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL});
    unsigned int at = 0u;
    ready[at++] = 5u;
    memor.cpy(&(MemoriaCfg){.dst = &ready[at], .src = "READY", .bytes = 5u});
    at += 5u;
    ready[at++] = 11u;
    memor.cpy(&(MemoriaCfg){.dst = &ready[at], .src = "Socket-Type", .bytes = 11u});
    at += 11u;
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        // a socket's type is a few letters, which a 32-bit length holds
        ready[at++] = (unsigned char)((kind_length >> (24u - (8u * byte))) & 0xffu);
    }
    memor.cpy(&(MemoriaCfg){.dst = &ready[at], .src = kind, .bytes = kind_length});
    at += (unsigned int)kind_length;
    unsigned char answer[4096];
    unsigned int flags = 0u;
    unsigned long long size = 0ull;
    if (!cli_zmtp_frame_written(socket, ready, at, CLI_ZMTP_COMMAND) ||
        (cli_zmtp_frame_read(socket, &flags, answer, sizeof(answer), &size) != CLI_ZMTP_FRAME_READ) ||
        ((flags & CLI_ZMTP_COMMAND) == 0u) || (size < 6ull) ||
        (memor.cmp(&(MemoriaCfg){.src = answer, .other = "\x05READY", .bytes = 6u}) != 0))
    {
        cli_line_text(said, "the peer answered the greeting with no READY");
        return 0;
    }
    return 1;
}

int cli_zmtp_connected(CliSocket *socket, const char *host, unsigned int port, const char *kind, CliLine *said)
{
    if (!cli_socket_connected(socket, host, port, said))
    {
        return 0;
    }
    if (!cli_zmtp_greeted(socket, kind, said))
    {
        cli_socket_closed(socket);
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "SUB", .cap = CLI_CELLS_POOL}))
    {
        const unsigned char subscribe = 1u;
        const CliZmtpFrame frame = {&subscribe, 1ull};
        return cli_zmtp_sent(socket, &frame, 1u);
    }
    return 1;
}

int cli_zmtp_sent(const CliSocket *socket, const CliZmtpFrame *frames, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int more = ((at + 1u) < count) ? CLI_ZMTP_MORE : 0u;
        if (!cli_zmtp_frame_written(socket, frames[at].bytes, frames[at].size, more))
        {
            return 0;
        }
    }
    return 1;
}

unsigned int cli_zmtp_received(const CliSocket *socket, unsigned char *room, unsigned long long room_size,
                               CliZmtpFrame *frames, unsigned int most)
{
    unsigned int count = 0u;
    unsigned long long used = 0ull;
    int whole = 1;
    for (;;)
    {
        unsigned int flags = 0u;
        unsigned long long size = 0ull;
        const int read = cli_zmtp_frame_read(socket, &flags, room + used, room_size - used, &size);
        if (read == CLI_ZMTP_FRAME_FAILED)
        {
            return 0u;
        }
        // the commands between messages passed over
        if ((flags & CLI_ZMTP_COMMAND) != 0u)
        {
            continue;
        }
        whole = whole && (read == CLI_ZMTP_FRAME_READ);
        if (whole && (count < most))
        {
            frames[count].bytes = room + used;
            frames[count].size = size;
            count += 1u;
        }
        used += whole ? size : 0ull;
        if ((flags & CLI_ZMTP_MORE) != 0u)
        {
            continue;
        }
        if (whole)
        {
            return count;
        }
        // a message larger than the room is passed over whole, and the next one read
        count = 0u;
        used = 0ull;
        whole = 1;
    }
}
