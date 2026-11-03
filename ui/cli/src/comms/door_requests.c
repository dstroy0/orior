// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_requests.c: what a joined process requests of the door, a frame at a time: to listen on a channel, or to send a
// body on one, the body in the frame or, where it is long, in the region handed over with it
#include "door_serve.h"

typedef struct
{
    CliIpcJoined *joined;
    const char *channel;
    const char *rest;
    unsigned long long size;
    unsigned long long *delivered;
    unsigned long long *moved;
    VerbaLinea *err_msg;
} DoorRequestsCtx;

// `size` bytes of `rest` read, a byte saying where the body is and then the body or its transfer: the body sent, how
// many received into `delivered` and the bytes a region moved into `moved`: 1, or 0 with why given
EMBED_INLINE int request_send(const DoorRequestsCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    *args->moved = 0ULL;
    if (args->size == 0ULL)
    {
        mmgr_line_put(err_msg, "a frame names where its body is");
        return 0;
    }
    if (args->rest[0] == CLI_IPC_BODY_TRANSFER)
    {
        unsigned long long region_length = 0ULL;
        unsigned long long handle = 0ULL;
        CliRegion region;
        if (!EMBED_CALL(cli_ipc_err_msg_size_internal.transfer, CliIpcErrMsgSizeCtx, .transfer = args->rest + 1,
                        .size = args->size - 1ULL, .length_pointer = &region_length, .handle_pointer = &handle,
                        .err_msg = err_msg) ||
            !EMBED_CALL(cli_ipc_err_msg_internal.transferred_over, CliIpcErrMsgCtx, .end = &args->joined->end,
                        .length = region_length, .handle = handle, .has_from = args->joined->has_peer,
                        .from = args->joined->peer, .region_out = &region, .err_msg = err_msg))
        {
            return 0;
        }
        const int sent = EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = args->joined->key,
                                    .channel = args->channel, .body = (const char *)region.view, .size = region.length,
                                    .delivered = args->delivered, .err_msg = err_msg);
        *args->moved = sent ? region.length : 0ULL;
        EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &region);
        return sent;
    }
    return EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = args->joined->key,
                      .channel = args->channel, .body = args->rest + 1, .size = args->size - 1ULL,
                      .delivered = args->delivered, .err_msg = err_msg);
}

void cli_ipc_requests_serve(const CliIpcServeCfg *args)
{
    char *const frame = args->joined->frame;
    for (;;)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        unsigned long long size = 0ULL;
        const int read =
            EMBED_CALL(cli_ipc_err_msg_end_internal.frame_read, CliIpcErrMsgEndCtx, .end = &args->joined->end,
                       .frame = frame, .capacity = CLI_IPC_FRAME, .size_pointer = &size, .err_msg = &err_msg);
        if (read == CLI_IPC_FRAME_BROKEN)
        {
            return;
        }
        if (read == CLI_IPC_FRAME_PASSED)
        {
            if (!EMBED_CALL(cli_ipc_serve_error.ipc_reject, CliIpcServeErrorCfg, .joined = args->joined,
                            .why = err_msg.out, .size = err_msg.at))
            {
                return;
            }
            continue;
        }
        const char kind = (size != 0ULL) ? frame[0] : '\0';
        const unsigned long long channel_end = cli_ipc_field_end(frame, size, 1ULL);
        char answer[32];
        VerbaLinea out = {answer, sizeof(answer), 0ULL};
        mmgr_line_ch(&out, CLI_IPC_KIND_ANSWER);
        int done = 0;
        if ((kind == CLI_IPC_KIND_LISTEN) && (channel_end != 0ULL))
        {
            done = EMBED_CALL(cli_ipc_serve_channel.ipc_listen_request, CliIpcServeChannelCfg, .joined = args->joined,
                              .channel = frame + 1, .err_msg = &err_msg);
        }
        else if ((kind == CLI_IPC_KIND_SEND) && (channel_end != 0ULL))
        {
            unsigned long long delivered = 0ULL;
            unsigned long long moved = 0ULL;
            done = EMBED_CALL(request_send, DoorRequestsCtx, .joined = args->joined, .channel = frame + 1,
                              .rest = frame + channel_end, .size = size - channel_end, .delivered = &delivered,
                              .moved = &moved, .err_msg = &err_msg);
            if (done && (moved > 0ULL))
            {
                EMBED_CALL(cli_comms_channel_bytes.transfer, CliCommsChannelBytesCfg, .channel = frame + 1,
                           .bytes = moved);
            }
            if (done)
            {
                mmgr_line_decimal(&out, delivered, 1u);
            }
        }
        else
        {
            mmgr_line_put(&err_msg, "a frame requests to listen or to send");
        }
        const int answered = done ? EMBED_CALL(cli_ipc_serve.ipc_answer, CliIpcServeCfg, .joined = args->joined,
                                               .bytes = out.out, .size = out.at)
                                  : EMBED_CALL(cli_ipc_serve_error.ipc_reject, CliIpcServeErrorCfg,
                                               .joined = args->joined, .why = err_msg.out, .size = err_msg.at);
        if (!answered)
        {
            return;
        }
    }
}
