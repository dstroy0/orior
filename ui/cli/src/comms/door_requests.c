// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_requests.c: what a joined process requests of the door, a frame at a time: to listen on a channel, or to send a
// body on one, the body in the frame or, where it is long, in the region handed over with it
#include "door_serve.h"

#include "../servers/lsp.h"
#include "../text/json_tree.h"

typedef struct
{
    CliDoorJoined *joined;
    const char *channel;
    unsigned long long size;
    unsigned long long *delivered;
    unsigned long long *moved;
    VerbaLinea *err_msg;
} DoorRequestsCtx;

// a body sent, written again as serde_json writes it, through the transfer layer where the frame hands a region over:
// the joined process's frame of `size` bytes, its transfer or body read: sent, how many received into `delivered` and
// the bytes moved into `moved`: 1, or 0 with why given
EMBED_INLINE int request_send(const DoorRequestsCtx *args)
{
    const char *channel = args->channel;
    unsigned long long size = args->size;
    unsigned long long *delivered = args->delivered;
    unsigned long long *moved = args->moved;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    *moved = 0ull;
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->joined->frame, .size_value = size,
                   .name = "transfer", .start = &start, .length_pointer = &length))
    {
        unsigned long long region_length = 0ull;
        unsigned long long handle = 0ull;
        CliRegion region;
        if (!EMBED_CALL(cli_door_err_msg_size_internal.transfer, CliDoorErrMsgSizeCtx,
                        .transfer = args->joined->frame + start, .size = length, .length_pointer = &region_length,
                        .handle_pointer = &handle, .err_msg = err_msg) ||
            !EMBED_CALL(cli_door_err_msg_internal.handed_over, CliDoorErrMsgCtx, .end = &args->joined->end,
                        .length = region_length, .handle = handle, .has_from = args->joined->has_peer,
                        .from = args->joined->peer, .region_out = &region, .err_msg = err_msg))
        {
            return 0;
        }
        // the region's bytes read as JSON, with nothing written where they are, before they are sent
        VerbaLinea none = {NULL, 0ull, 0ull};
        if (!EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = (const char *)region.view,
                        .size = region.length, .out = &none))
        {
            EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &region);
            mmgr_line_put(err_msg, "the region contains no JSON");
            return 0;
        }
        const int sent = EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = args->joined->key,
                                    .channel = channel, .body = (const char *)region.view, .size = region.length,
                                    .delivered = delivered, .err_msg = err_msg);
        *moved = sent ? region.length : 0ull;
        EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &region);
        return sent;
    }
    // a frame that names no body means null
    VerbaLinea body = {args->joined->body, sizeof(args->joined->body), 0ull};
    const int has_body = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->joined->frame, .size_value = size,
                                    .name = "body", .start = &start, .length_pointer = &length);
    if (!EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg,
                    .text = has_body ? args->joined->frame + start : "null", .size = has_body ? length : 4ull,
                    .out = &body))
    {
        mmgr_line_put(err_msg, "the frame's body is no JSON, or past the room it is written in");
        return 0;
    }
    return EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = args->joined->key,
                      .channel = channel, .body = body.out, .size = body.at, .delivered = delivered,
                      .err_msg = err_msg);
}

void cli_door_requests_serve(const CliDoorServeCfg *args)
{
    for (;;)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        unsigned long long size = 0ull;
        const int read = EMBED_CALL(cli_door_err_msg_end_internal.frame_read, CliDoorErrMsgEndCtx,
                                    .end = &args->joined->end, .frame = args->joined->frame,
                                    .room = sizeof(args->joined->frame), .size_pointer = &size, .err_msg = &err_msg);
        if (read == CLI_DOOR_FRAME_BROKEN)
        {
            return;
        }
        if (read == CLI_DOOR_FRAME_PASSED)
        {
            if (!EMBED_CALL(cli_door_serve_why.door_reject, CliDoorServeWhyCfg, .joined = args->joined,
                            .why = err_msg.out, .size = err_msg.at))
            {
                return;
            }
            continue;
        }
        char channel[CLI_NAME_MAX_LEN];
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        char answer[CLI_NAME_MAX_LEN + 64u];
        VerbaLinea out = {answer, sizeof(answer), 0ull};
        int answered = 0;
        if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->joined->frame, .size_value = size,
                       .name = "listen", .start = &start, .length_pointer = &length) &&
            EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = args->joined->frame + start, .length = length,
                       .out_room = channel, .room = sizeof(channel)))
        {
            if (EMBED_CALL(cli_door_serve_channel.door_listen_request, CliDoorServeChannelCfg, .joined = args->joined,
                           .channel = channel, .err_msg = &err_msg))
            {
                mmgr_line_put(&out, "{\"ok\":\"listen\",\"channel\":");
                EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
                           .text_bytes = (const unsigned char *)channel,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}));
                mmgr_line_ch(&out, '}');
                answered = EMBED_CALL(cli_door_serve.door_answer, CliDoorServeCfg, .joined = args->joined,
                                      .json = out.out, .size = out.at);
            }
            else
            {
                answered = EMBED_CALL(cli_door_serve_why.door_reject, CliDoorServeWhyCfg, .joined = args->joined,
                                      .why = err_msg.out, .size = err_msg.at);
            }
        }
        else if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->joined->frame, .size_value = size,
                            .name = "send", .start = &start, .length_pointer = &length) &&
                 EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = args->joined->frame + start, .length = length,
                            .out_room = channel, .room = sizeof(channel)))
        {
            unsigned long long delivered = 0ull;
            unsigned long long moved = 0ull;
            if (EMBED_CALL(request_send, DoorRequestsCtx, .joined = args->joined, .channel = channel, .size = size,
                           .delivered = &delivered, .moved = &moved, .err_msg = &err_msg))
            {
                if (moved > 0ull)
                {
                    EMBED_CALL(cli_comms_channel_bytes.transfer, CliCommsChannelBytesCfg, .channel = channel,
                               .bytes = moved);
                }
                mmgr_line_put(&out, "{\"ok\":\"send\",\"delivered\":");
                mmgr_line_decimal(&out, delivered, 1u);
                mmgr_line_ch(&out, '}');
                answered = EMBED_CALL(cli_door_serve.door_answer, CliDoorServeCfg, .joined = args->joined,
                                      .json = out.out, .size = out.at);
            }
            else
            {
                answered = EMBED_CALL(cli_door_serve_why.door_reject, CliDoorServeWhyCfg, .joined = args->joined,
                                      .why = err_msg.out, .size = err_msg.at);
            }
        }
        else
        {
            static const char s_neither[] = "a frame requests to listen or to send";
            answered = EMBED_CALL(cli_door_serve_why.door_reject, CliDoorServeWhyCfg, .joined = args->joined,
                                  .why = s_neither, .size = sizeof(s_neither) - 1u);
        }
        if (!answered)
        {
            return;
        }
    }
}
