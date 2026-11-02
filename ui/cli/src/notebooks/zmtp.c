// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// zmtp.c: ZeroMQ's wire protocol, ZMTP 3.0 with its NULL mechanism, as a Jupyter kernel uses it on the machine's own
// loopback: the greeting, the READY command that names a socket's type, and messages of one or more frames. orior_cli
// is the connecting side of each of a kernel's sockets, a DEALER to its ROUTERs, a SUB to its PUB and a REQ to its REP.
// A SUB subscribes as ZMTP 3.0 has it, by a message whose first byte is 1, which a peer that uses 3.1 takes from
// a 3.0 peer as well
#include "zmtp.h"

#define CLI_ZMTP_MORE 0x01u

#define CLI_ZMTP_LONG 0x02u

#define CLI_ZMTP_COMMAND 0x04u

typedef struct
{
    unsigned char *head;
    unsigned long long size;
    unsigned int flags;
    const CliSocket *socket;
    const void *body;
    unsigned int *flags_pointer;
    unsigned char *body_bytes;
    unsigned long long room;
    unsigned long long *size_pointer;
    const char *kind;
    VerbaLinea *err_msg;
} ZmtpCtx;
// the head of a frame of `size` bytes with `flags` written into `head`, sized for 9: how many bytes it takes
EMBED_INLINE unsigned int cli_zmtp_head(const ZmtpCtx *args)
{
    if (args->size > 255ull)
    {
        args->head[0] = (unsigned char)(args->flags | CLI_ZMTP_LONG);
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            args->head[1u + at] = (unsigned char)((args->size >> (56u - (8u * at))) & 0xffull);
        }
        return 9u;
    }
    args->head[0] = (unsigned char)args->flags;
    // a size at most 255 fits a byte
    args->head[1] = (unsigned char)args->size;
    return 2u;
}

// one frame of the greeting written: its flags, its size, one byte or eight, and its body
EMBED_INLINE int cli_zmtp_frame_write(const ZmtpCtx *args)
{
    unsigned long long size = args->size;
    unsigned char head[9];
    const unsigned int length = EMBED_CALL(cli_zmtp_head, ZmtpCtx, .head = head, .size = size, .flags = args->flags);
    return EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->socket, .bytes = head,
                      .size = length) &&
           EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->socket, .bytes = args->body,
                      .size = size);
}

// what reading a frame came to: the connection failed, the frame read, or its body larger than the room and passed
// over
#define CLI_ZMTP_FRAME_FAILED 0
#define CLI_ZMTP_FRAME_READ 1
#define CLI_ZMTP_FRAME_PASSED 2

// one frame read: its flags and its body into `body`, sized for `room`, its size into `size`: what the read came to
static int cli_zmtp_frame_read(const ZmtpCtx *args)
{
    unsigned int *flags = args->flags_pointer;
    unsigned long long *size = args->size_pointer;
    unsigned char first = 0u;
    if (!EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = args->socket, .bytes_out = &first,
                    .size = 1ull))
    {
        return 0;
    }
    *flags = first;
    *size = 0ull;
    if ((first & CLI_ZMTP_LONG) != 0u)
    {
        unsigned char length[8];
        if (!EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = args->socket, .bytes_out = length,
                        .size = 8ull))
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
        if (!EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = args->socket,
                        .bytes_out = &length, .size = 1ull))
        {
            return 0;
        }
        *size = length;
    }
    if (*size <= args->room)
    {
        return EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = args->socket,
                          .bytes_out = args->body_bytes, .size = *size)
                   ? CLI_ZMTP_FRAME_READ
                   : CLI_ZMTP_FRAME_FAILED;
    }
    // a body larger than the room is read and let go, so that the next frame is read where it starts
    unsigned char piece[4096];
    for (unsigned long long left = *size; left > 0ull;)
    {
        const unsigned long long taken = (left < sizeof(piece)) ? left : sizeof(piece);
        if (!EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = args->socket, .bytes_out = piece,
                        .size = taken))
        {
            return CLI_ZMTP_FRAME_FAILED;
        }
        left -= taken;
    }
    return CLI_ZMTP_FRAME_PASSED;
}

// the greeting: ZMTP 3, the NULL mechanism, and the READY command that names the socket's type
EMBED_INLINE int cli_zmtp_greet(const ZmtpCtx *args)
{
    unsigned char greeting[64];
    memor.set(&(MemoriaCfg){.dst = greeting, .val = 0, .bytes = sizeof(greeting)});
    greeting[0] = 0xffu;
    greeting[9] = 0x7fu;
    greeting[10] = 3u;
    greeting[11] = 0u;
    memor.cpy(&(MemoriaCfg){.dst = &greeting[12], .src = "NULL", .bytes = 4u});
    unsigned char theirs[64];
    if (!EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->socket, .bytes = greeting,
                    .size = sizeof(greeting)) ||
        !EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = args->socket, .bytes_out = theirs,
                    .size = sizeof(theirs)))
    {
        mmgr_line_put(args->err_msg, "the kernel's greeting was not read");
        return 0;
    }
    if ((theirs[0] != 0xffu) || (theirs[9] != 0x7fu) || (theirs[10] < 3u))
    {
        mmgr_line_put(args->err_msg, "the peer uses no ZMTP 3");
        return 0;
    }
    if (memor.cmp(&(MemoriaCfg){.src = &theirs[12], .other = "NULL", .bytes = 4u}) != 0)
    {
        mmgr_line_put(args->err_msg, "the peer requests the ");
        unsigned int length = 0u;
        const unsigned char *const ended = memor.chr(&(MemoriaCfg){.src = &theirs[12], .val = 0, .bytes = 20u});
        // a 0 found lies in the 20 bytes, which an unsigned int stores
        length = (ended != NULL) ? (unsigned int)(ended - &theirs[12]) : 20u;
        mmgr_line_put_n(args->err_msg, (const char *)&theirs[12], length);
        mmgr_line_put(args->err_msg, " mechanism, where orior_cli supports NULL");
        return 0;
    }
    unsigned char ready[64];
    const size_t kind_length = cellul.len(&(CatenaFinitaCfg){.src = args->kind, .cap = CLI_CELLS_POOL});
    unsigned int at = 0u;
    ready[at++] = 5u;
    memor.cpy(&(MemoriaCfg){.dst = &ready[at], .src = "READY", .bytes = 5u});
    at += 5u;
    ready[at++] = 11u;
    memor.cpy(&(MemoriaCfg){.dst = &ready[at], .src = "Socket-Type", .bytes = 11u});
    at += 11u;
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        // a socket's type is a few letters, which a 32-bit length stores
        ready[at++] = (unsigned char)((kind_length >> (24u - (8u * byte))) & 0xffu);
    }
    memor.cpy(&(MemoriaCfg){.dst = &ready[at], .src = args->kind, .bytes = kind_length});
    at += (unsigned int)kind_length;
    unsigned char answer[4096];
    unsigned int flags = 0u;
    unsigned long long size = 0ull;
    if (!EMBED_CALL(cli_zmtp_frame_write, ZmtpCtx, .socket = args->socket, .body = ready, .size = at,
                    .flags = CLI_ZMTP_COMMAND) ||
        (EMBED_CALL(cli_zmtp_frame_read, ZmtpCtx, .socket = args->socket, .flags_pointer = &flags, .body_bytes = answer,
                    .room = sizeof(answer), .size_pointer = &size) != CLI_ZMTP_FRAME_READ) ||
        ((flags & CLI_ZMTP_COMMAND) == 0u) || (size < 6ull) ||
        (memor.cmp(&(MemoriaCfg){.src = answer, .other = "\x05READY", .bytes = 6u}) != 0))
    {
        mmgr_line_put(args->err_msg, "the peer answered the greeting with no READY");
        return 0;
    }
    return 1;
}

int cli_zmtp_connect(const CliZmtpSocketHostCfg *args)
{
    if (!EMBED_CALL(cli_err_msg_internal.socket_connect, CliErrMsgCtx, .sock_out = args->socket_out, .host = args->host,
                    .port = args->port, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_zmtp_greet, ZmtpCtx, .socket = args->socket_out, .kind = args->kind, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = args->socket_out);
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "SUB", .cap = CLI_CELLS_POOL}))
    {
        const unsigned char subscribe = 1u;
        const CliZmtpFrame frame = {&subscribe, 1ull};
        return EMBED_CALL(cli_zmtp_frames_count.send, CliZmtpFramesCountCfg, .socket = args->socket_out,
                          .frames = &frame, .count = 1u);
    }
    return 1;
}

unsigned char *cli_zmtp_message_init(const CliZmtpCfg *args)
{
    unsigned long long *size = args->size;
    unsigned long long total = 0ull;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        total += ((args->frames[at].size > 255ull) ? 9ull : 2ull) + args->frames[at].size;
    }
    unsigned char *const message = g_cli_cells->persistent_buf_alloc(total, &g_cli_notebook_cells);
    if (message == NULL)
    {
        return NULL;
    }
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        used += EMBED_CALL(cli_zmtp_head, ZmtpCtx, .head = message + used, .size = args->frames[at].size,
                           .flags = ((at + 1u) < args->count) ? CLI_ZMTP_MORE : 0u);
        // a frame's body is below the message's size, which a cell of it stores
        memor.cpy(&(MemoriaCfg){
            .dst = message + used, .src = args->frames[at].bytes, .bytes = (size_t)args->frames[at].size});
        used += args->frames[at].size;
    }
    *size = total;
    return message;
}

int cli_zmtp_send(const CliZmtpFramesCountCfg *args)
{
    unsigned long long size = 0ull;
    unsigned char *const message =
        EMBED_CALL(cli_zmtp.message_init, CliZmtpCfg, .frames = args->frames, .count = args->count, .size = &size);
    const int sent = (message != NULL) && EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx,
                                                     .sock = args->socket, .bytes = message, .size = size);
    g_cli_cells->persistent_buf_release(message, &g_cli_notebook_cells);
    return sent;
}

unsigned int cli_zmtp_receive(const CliZmtpSocketCfg *args)
{
    unsigned int count = 0u;
    unsigned long long used = 0ull;
    int whole = 1;
    for (;;)
    {
        unsigned int flags = 0u;
        unsigned long long size = 0ull;
        const int read =
            EMBED_CALL(cli_zmtp_frame_read, ZmtpCtx, .socket = args->socket, .flags_pointer = &flags,
                       .body_bytes = args->room + used, .room = args->room_size - used, .size_pointer = &size);
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
        if (whole && (count < args->most))
        {
            args->frames_out[count].bytes = args->room + used;
            args->frames_out[count].size = size;
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
