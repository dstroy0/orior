// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door.c: the frames that go through the door, each the length of its JSON in four bytes, least first, then the JSON;
// and the regions a frame hands over: what its transfer says of one, and one taken from what a transfer says
#include "../servers/lsp.h"
#include "door_internal.h"

int cli_door_frame_read(CliDoorEnd *end, char *frame, unsigned long long room, unsigned long long *size, CliLine *said)
{
    unsigned char head[4];
    if (!cli_door_read(end, head, sizeof(head)))
    {
        cli_line_text(said, "the door's end closed");
        return CLI_DOOR_FRAME_BROKEN;
    }
    const unsigned long long length = (unsigned long long)head[0] | ((unsigned long long)head[1] << 8u) |
                                      ((unsigned long long)head[2] << 16u) | ((unsigned long long)head[3] << 24u);
    *size = length;
    if (length > CLI_DOOR_MOST)
    {
        cli_line_text(said, "a frame of ");
        cli_line_decimal(said, length, 1u);
        cli_line_text(said, " bytes is past the most a frame takes");
        return CLI_DOOR_FRAME_BROKEN;
    }
    if (length <= room)
    {
        return cli_door_read(end, frame, length) ? CLI_DOOR_FRAME_READ : CLI_DOOR_FRAME_BROKEN;
    }
    // a frame past the room it is read into is read past, a piece at a time
    for (unsigned long long passed = 0ull; passed < length;)
    {
        const unsigned long long piece = ((length - passed) < room) ? (length - passed) : room;
        if (!cli_door_read(end, frame, piece))
        {
            return CLI_DOOR_FRAME_BROKEN;
        }
        passed += piece;
    }
    cli_line_text(said, "a frame of ");
    cli_line_decimal(said, length, 1u);
    cli_line_text(said, " bytes is past the room the door reads it in");
    return CLI_DOOR_FRAME_PASSED;
}

int cli_door_frame_written(CliDoorEnd *end, const char *json, unsigned long long size, const CliRegion *region)
{
    const unsigned char head[4] = {(unsigned char)(size & 0xffu), (unsigned char)((size >> 8u) & 0xffu),
                                   (unsigned char)((size >> 16u) & 0xffu), (unsigned char)((size >> 24u) & 0xffu)};
    // the region goes with the frame's first byte, its length's
    return cli_door_written(end, head, sizeof(head), region) && cli_door_written(end, json, size, NULL);
}

int cli_door_handed(const CliRegion *region, int has_to, unsigned int to, CliLine *out, CliLine *said)
{
    cli_line_text(out, "{\"length\":");
    cli_line_decimal(out, region->length, 1u);
#if defined(_WIN32)
    unsigned long long handle = region->section;
    if (has_to && !cli_region_given(region, to, &handle, said))
    {
        return 0;
    }
    cli_line_text(out, ",\"handle\":");
    cli_line_decimal(out, handle, 1u);
#else
    (void)has_to;
    (void)to;
    (void)said;
#endif
    cli_line_character(out, '}');
    return 1;
}

int cli_door_handed_over(CliDoorEnd *end, unsigned long long length, unsigned long long handle, int has_from,
                         unsigned int from, CliRegion *region, CliLine *said)
{
#if defined(_WIN32)
    (void)end;
    return has_from ? cli_region_taken(region, from, handle, length, said)
                    : cli_region_received(region, handle, length, said);
#else
    (void)handle;
    (void)has_from;
    (void)from;
    const int fd = cli_door_descriptor(end);
    if (fd < 0)
    {
        cli_line_text(said, "a transfer came with no region");
        return 0;
    }
    return cli_region_received(region, (unsigned long long)fd, length, said);
#endif
}

// a whole number of the transfer object, `name`, into `value`: 1, or 0 where it is none
static int door_number(const char *transfer, unsigned long long size, const char *name, unsigned long long *value)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    if (!cli_lsp_member(transfer, size, name, &start, &length) || (length == 0ull) || (length > 20ull))
    {
        return 0;
    }
    unsigned long long whole = 0ull;
    for (unsigned long long at = start; at < start + length; at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(transfer[at] - '0');
        if ((transfer[at] < '0') || (transfer[at] > '9') || (whole > (18446744073709551615ull - digit) / 10ull))
        {
            return 0;
        }
        whole = (whole * 10ull) + digit;
    }
    *value = whole;
    return 1;
}

int cli_door_transfer(const char *transfer, unsigned long long size, unsigned long long *length,
                      unsigned long long *handle, CliLine *said)
{
    *handle = 0ull;
    if (!door_number(transfer, size, "length", length))
    {
        cli_line_text(said, "a transfer names its length");
        return 0;
    }
#if defined(_WIN32)
    if (!door_number(transfer, size, "handle", handle))
    {
        cli_line_text(said, "a transfer names its handle");
        return 0;
    }
#endif
    return 1;
}
