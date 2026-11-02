// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door.c: the frames that go through the door, each the length of its JSON in four bytes, least first, then the JSON;
// and the regions a frame hands over: what its transfer describes of one, and one taken from what a transfer describes
#include "../servers/lsp.h"
#include "door_internal.h"

int cli_door_frame_read(const CliDoorErrMsgEndCtx *args)
{
    unsigned long long *size = args->size_pointer;
    unsigned char head[4];
    if (!EMBED_CALL(cli_door_end_bytes_internal.read, CliDoorEndBytesCtx, .end = args->end, .bytes = head,
                    .size = sizeof(head)))
    {
        mmgr_line_put(args->err_msg, "the door's end closed");
        return CLI_DOOR_FRAME_BROKEN;
    }
    const unsigned long long length = (unsigned long long)head[0] | ((unsigned long long)head[1] << 8u) |
                                      ((unsigned long long)head[2] << 16u) | ((unsigned long long)head[3] << 24u);
    *size = length;
    if (length > CLI_DOOR_MOST)
    {
        mmgr_line_put(args->err_msg, "a frame of ");
        mmgr_line_decimal(args->err_msg, length, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the most a frame takes");
        return CLI_DOOR_FRAME_BROKEN;
    }
    if (length <= args->room)
    {
        return EMBED_CALL(cli_door_end_bytes_internal.read, CliDoorEndBytesCtx, .end = args->end, .bytes = args->frame,
                          .size = length)
                   ? CLI_DOOR_FRAME_READ
                   : CLI_DOOR_FRAME_BROKEN;
    }
    // a frame past the room it is read into is read past, a piece at a time
    for (unsigned long long passed = 0ull; passed < length;)
    {
        const unsigned long long piece = ((length - passed) < args->room) ? (length - passed) : args->room;
        if (!EMBED_CALL(cli_door_end_bytes_internal.read, CliDoorEndBytesCtx, .end = args->end, .bytes = args->frame,
                        .size = piece))
        {
            return CLI_DOOR_FRAME_BROKEN;
        }
        passed += piece;
    }
    mmgr_line_put(args->err_msg, "a frame of ");
    mmgr_line_decimal(args->err_msg, length, 1u);
    mmgr_line_put(args->err_msg, " bytes is past the room the door reads it in");
    return CLI_DOOR_FRAME_PASSED;
}

int cli_door_frame_write(const CliDoorEndCtx *args)
{
    const unsigned char head[4] = {(unsigned char)(args->size & 0xffu), (unsigned char)((args->size >> 8u) & 0xffu),
                                   (unsigned char)((args->size >> 16u) & 0xffu),
                                   (unsigned char)((args->size >> 24u) & 0xffu)};
    // the region goes with the frame's first byte, its length's
    return EMBED_CALL(cli_door_end_size_internal.write, CliDoorEndSizeCtx, .end = args->end, .bytes_read = head,
                      .size = sizeof(head), .region = args->region) &&
           EMBED_CALL(cli_door_end_size_internal.write, CliDoorEndSizeCtx, .end = args->end, .bytes_read = args->json,
                      .size = args->size, .region = NULL);
}

int cli_door_dispatch(const CliDoorErrMsgRegionCtx *args)
{
    mmgr_line_put(args->out, "{\"length\":");
    mmgr_line_decimal(args->out, args->region->length, 1u);
#if defined(_WIN32)
    unsigned long long handle = args->region->section;
    if (args->has_to && !EMBED_CALL(cli_region_free, CliTransferCfg, .region_read = args->region, .process = args->to,
                                    .theirs = &handle, .err_msg = args->err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, ",\"handle\":");
    mmgr_line_decimal(args->out, handle, 1u);
#else
#endif
    mmgr_line_ch(args->out, '}');
    return 1;
}

int cli_door_handed_over(const CliDoorErrMsgCtx *args)
{
#if defined(_WIN32)
    return args->has_from
               ? EMBED_CALL(cli_region_take, CliTransferRegionCfg, .region = args->region_out, .process = args->from,
                            .handle = args->handle, .length = args->length, .err_msg = args->err_msg)
               : EMBED_CALL(cli_transfer_region.region_receive, CliTransferRegionCfg, .region = args->region_out,
                            .handle = args->handle, .length = args->length, .err_msg = args->err_msg);
#else
    const int fd = cli_door_err_msg_internal.descriptor(args);
    if (fd < 0)
    {
        mmgr_line_put(args->err_msg, "a transfer came with no region");
        return 0;
    }
    return EMBED_CALL(cli_transfer_region.region_receive, CliTransferRegionCfg, .region = args->region_out,
                      .handle = (unsigned long long)fd, .length = args->length, .err_msg = args->err_msg);
#endif
}

typedef struct
{
    const char *transfer;
    unsigned long long size;
    const char *name;
    unsigned long long *value;
} DoorCtx;
// a whole number of the transfer object, `name`, into `value`: 1, or 0 where it is none
EMBED_INLINE int door_number(const DoorCtx *args)
{
    unsigned long long size = args->size;
    const char *name = args->name;
    unsigned long long *value = args->value;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->transfer, .size_value = size, .name = name,
                    .start = &start, .length_pointer = &length) ||
        (length == 0ull) || (length > 20ull))
    {
        return 0;
    }
    unsigned long long whole = 0ull;
    for (unsigned long long at = start; at < start + length; at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(args->transfer[at] - '0');
        if ((args->transfer[at] < '0') || (args->transfer[at] > '9') ||
            (whole > (18446744073709551615ull - digit) / 10ull))
        {
            return 0;
        }
        whole = (whole * 10ull) + digit;
    }
    *value = whole;
    return 1;
}

int cli_door_transfer(const CliDoorErrMsgSizeCtx *args)
{
    unsigned long long *handle = args->handle_pointer;
    *handle = 0ull;
    if (!EMBED_CALL(door_number, DoorCtx, .transfer = args->transfer, .size = args->size, .name = "length",
                    .value = args->length_pointer))
    {
        mmgr_line_put(args->err_msg, "a transfer names its length");
        return 0;
    }
#if defined(_WIN32)
    if (!EMBED_CALL(door_number, DoorCtx, .transfer = args->transfer, .size = args->size, .name = "handle",
                    .value = handle))
    {
        mmgr_line_put(args->err_msg, "a transfer names its handle");
        return 0;
    }
#endif
    return 1;
}
