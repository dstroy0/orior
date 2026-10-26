// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_requests.c: what a joined process requests of the door, a frame at a time: to listen on a channel, or to send a
// body on one, the body in the frame or, where it is long, in the region handed over with it
#include "door_serve.h"

#include "../servers/lsp.h"
#include "../text/json_tree.h"

// a body sent, written again as serde_json writes it, through the transfer layer where the frame hands a region over:
// the joined process's frame of `size` bytes, its transfer or body read: sent, how many received into `delivered` and
// the bytes moved into `moved`: 1, or 0 with why given
static int request_send(CliDoorJoined *joined, const char *channel, unsigned long long size,
                        unsigned long long *delivered, unsigned long long *moved, VerbaLinea *err_msg)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    *moved = 0ull;
    if (cli_lsp_member(joined->frame, size, "transfer", &start, &length))
    {
        unsigned long long region_length = 0ull;
        unsigned long long handle = 0ull;
        CliRegion region;
        if (!cli_door_transfer(joined->frame + start, length, &region_length, &handle, err_msg) ||
            !cli_door_handed_over(&joined->end, region_length, handle, joined->has_peer, joined->peer, &region,
                                  err_msg))
        {
            return 0;
        }
        // the region's bytes read as JSON, with nothing written where they are, before they are sent
        VerbaLinea none = {NULL, 0ull, 0ull};
        if (!cli_json_compacted((const char *)region.view, region.length, &none))
        {
            cli_region_closed(&region);
            mmgr_line_put(err_msg, "the region contains no JSON");
            return 0;
        }
        const int sent =
            cli_comms_send(joined->key, channel, (const char *)region.view, region.length, delivered, err_msg);
        *moved = sent ? region.length : 0ull;
        cli_region_closed(&region);
        return sent;
    }
    // a frame that names no body means null
    VerbaLinea body = {joined->body, sizeof(joined->body), 0ull};
    const int has_body = cli_lsp_member(joined->frame, size, "body", &start, &length);
    if (!cli_json_compacted(has_body ? joined->frame + start : "null", has_body ? length : 4ull, &body))
    {
        mmgr_line_put(err_msg, "the frame's body is no JSON, or past the room it is written in");
        return 0;
    }
    return cli_comms_send(joined->key, channel, body.out, body.at, delivered, err_msg);
}

void cli_door_requests_served(CliDoorJoined *joined)
{
    for (;;)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        unsigned long long size = 0ull;
        const int read = cli_door_frame_read(&joined->end, joined->frame, sizeof(joined->frame), &size, &err_msg);
        if (read == CLI_DOOR_FRAME_BROKEN)
        {
            return;
        }
        if (read == CLI_DOOR_FRAME_PASSED)
        {
            if (!cli_door_refused(joined, err_msg.out, err_msg.at))
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
        if (cli_lsp_member(joined->frame, size, "listen", &start, &length) &&
            cli_lsp_string(joined->frame + start, length, channel, sizeof(channel)))
        {
            if (cli_door_listen_requested(joined, channel, &err_msg))
            {
                mmgr_line_put(&out, "{\"ok\":\"listen\",\"channel\":");
                cli_json_quoted_lossy(&out, (const unsigned char *)channel,
                                      cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}));
                mmgr_line_ch(&out, '}');
                answered = cli_door_answered(joined, out.out, out.at);
            }
            else
            {
                answered = cli_door_refused(joined, err_msg.out, err_msg.at);
            }
        }
        else if (cli_lsp_member(joined->frame, size, "send", &start, &length) &&
                 cli_lsp_string(joined->frame + start, length, channel, sizeof(channel)))
        {
            unsigned long long delivered = 0ull;
            unsigned long long moved = 0ull;
            if (request_send(joined, channel, size, &delivered, &moved, &err_msg))
            {
                if (moved > 0ull)
                {
                    cli_comms_transferred(channel, moved);
                }
                mmgr_line_put(&out, "{\"ok\":\"send\",\"delivered\":");
                mmgr_line_decimal(&out, delivered, 1u);
                mmgr_line_ch(&out, '}');
                answered = cli_door_answered(joined, out.out, out.at);
            }
            else
            {
                answered = cli_door_refused(joined, err_msg.out, err_msg.at);
            }
        }
        else
        {
            static const char s_neither[] = "a frame requests to listen or to send";
            answered = cli_door_refused(joined, s_neither, sizeof(s_neither) - 1u);
        }
        if (!answered)
        {
            return;
        }
    }
}
