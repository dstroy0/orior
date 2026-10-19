// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_asked.c: what a joined process asks of the door, a frame at a time: to listen on a channel, or to say a body
// on one, the body in the frame or, where it is long, in the region handed over with it
#include "door_serve.h"

#include "../servers/lsp.h"
#include "../text/json_tree.h"

// a body said, written again as serde_json writes it, through the transfer layer where the frame hands a region over:
// the joined process's frame of `size` bytes, its transfer or body read: said, how many heard into `heard` and the
// bytes moved into `moved`: 1, or 0 with why said
static int asked_say(CliDoorJoined *joined, const char *channel, unsigned long long size, unsigned long long *heard,
                     unsigned long long *moved, CliLine *said)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    *moved = 0ull;
    if (cli_lsp_member(joined->frame, size, "transfer", &start, &length))
    {
        unsigned long long region_length = 0ull;
        unsigned long long handle = 0ull;
        CliRegion region;
        if (!cli_door_transfer(joined->frame + start, length, &region_length, &handle, said) ||
            !cli_door_handed_over(&joined->end, region_length, handle, joined->has_peer, joined->peer, &region, said))
        {
            return 0;
        }
        // the region's bytes read as JSON, with nothing written where they are, before they are said
        CliLine none = {NULL, 0ull, 0ull};
        if (!cli_json_compacted((const char *)region.view, region.length, &none))
        {
            cli_region_closed(&region);
            cli_line_text(said, "the region holds no JSON");
            return 0;
        }
        const int spoke = cli_comms_say(joined->key, channel, (const char *)region.view, region.length, heard, said);
        *moved = spoke ? region.length : 0ull;
        cli_region_closed(&region);
        return spoke;
    }
    // a frame that names no body says null
    CliLine body = {joined->body, sizeof(joined->body), 0ull};
    const int has_body = cli_lsp_member(joined->frame, size, "body", &start, &length);
    if (!cli_json_compacted(has_body ? joined->frame + start : "null", has_body ? length : 4ull, &body))
    {
        cli_line_text(said, "the frame's body is no JSON, or past the room it is written in");
        return 0;
    }
    return cli_comms_say(joined->key, channel, body.out, body.at, heard, said);
}

void cli_door_asked(CliDoorJoined *joined)
{
    for (;;)
    {
        char why[CLI_SAID_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        unsigned long long size = 0ull;
        const int read = cli_door_frame_read(&joined->end, joined->frame, sizeof(joined->frame), &size, &said);
        if (read == CLI_DOOR_FRAME_BROKEN)
        {
            return;
        }
        if (read == CLI_DOOR_FRAME_PASSED)
        {
            if (!cli_door_refused(joined, said.out, said.at))
            {
                return;
            }
            continue;
        }
        char channel[CLI_NAME_LONGEST];
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        char answer[CLI_NAME_LONGEST + 64u];
        CliLine out = {answer, sizeof(answer), 0ull};
        int answered = 0;
        if (cli_lsp_member(joined->frame, size, "listen", &start, &length) &&
            cli_lsp_string(joined->frame + start, length, channel, sizeof(channel)))
        {
            if (cli_door_listen_asked(joined, channel, &said))
            {
                cli_line_text(&out, "{\"ok\":\"listen\",\"channel\":");
                cli_json_quoted_lossy(&out, (const unsigned char *)channel,
                                      cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}));
                cli_line_character(&out, '}');
                answered = cli_door_answered(joined, out.out, out.at);
            }
            else
            {
                answered = cli_door_refused(joined, said.out, said.at);
            }
        }
        else if (cli_lsp_member(joined->frame, size, "say", &start, &length) &&
                 cli_lsp_string(joined->frame + start, length, channel, sizeof(channel)))
        {
            unsigned long long heard = 0ull;
            unsigned long long moved = 0ull;
            if (asked_say(joined, channel, size, &heard, &moved, &said))
            {
                if (moved > 0ull)
                {
                    cli_comms_transferred(channel, moved);
                }
                cli_line_text(&out, "{\"ok\":\"say\",\"heard\":");
                cli_line_decimal(&out, heard, 1u);
                cli_line_character(&out, '}');
                answered = cli_door_answered(joined, out.out, out.at);
            }
            else
            {
                answered = cli_door_refused(joined, said.out, said.at);
            }
        }
        else
        {
            static const char s_neither[] = "a frame asks to listen or to say";
            answered = cli_door_refused(joined, s_neither, sizeof(s_neither) - 1u);
        }
        if (!answered)
        {
            return;
        }
    }
}
