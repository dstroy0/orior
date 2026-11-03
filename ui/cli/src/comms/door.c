// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door.c: the frames that go through the door, each its bytes' count in four bytes, least first, then its bytes; and
// the regions a frame hands over: what its transfer describes of one, and one taken from what a transfer describes
#include "door_internal.h"

// A transfer as it stands in a frame: the region's length, and on Windows the handle made for it
typedef struct
{
    unsigned long long length;
    unsigned long long handle;
} CliIpcTransfer;

int cli_ipc_frame_read(const CliIpcErrMsgEndCtx *args)
{
    unsigned long long *size = args->size_pointer;
    unsigned char head[4];
    if (!EMBED_CALL(cli_ipc_end_bytes_internal.read, CliIpcEndBytesCtx, .end = args->end, .bytes = head,
                    .size = sizeof(head)))
    {
        mmgr_line_put(args->err_msg, "the door's end closed");
        return CLI_IPC_FRAME_BROKEN;
    }
    const unsigned long long length = (unsigned long long)head[0] | ((unsigned long long)head[1] << 8u) |
                                      ((unsigned long long)head[2] << 16u) | ((unsigned long long)head[3] << 24u);
    *size = length;
    if (length > CLI_IPC_MAX)
    {
        mmgr_line_put(args->err_msg, "a frame of ");
        mmgr_line_decimal(args->err_msg, length, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the most a frame takes");
        return CLI_IPC_FRAME_BROKEN;
    }
    if (length <= args->capacity)
    {
        return EMBED_CALL(cli_ipc_end_bytes_internal.read, CliIpcEndBytesCtx, .end = args->end, .bytes = args->frame,
                          .size = length)
                   ? CLI_IPC_FRAME_READ
                   : CLI_IPC_FRAME_BROKEN;
    }
    // a frame past the capacity it is read into is read past, a piece at a time
    for (unsigned long long passed = 0ULL; passed < length;)
    {
        const unsigned long long piece = ((length - passed) < args->capacity) ? (length - passed) : args->capacity;
        if (!EMBED_CALL(cli_ipc_end_bytes_internal.read, CliIpcEndBytesCtx, .end = args->end, .bytes = args->frame,
                        .size = piece))
        {
            return CLI_IPC_FRAME_BROKEN;
        }
        passed += piece;
    }
    mmgr_line_put(args->err_msg, "a frame of ");
    mmgr_line_decimal(args->err_msg, length, 1u);
    mmgr_line_put(args->err_msg, " bytes is past the capacity of the buffer the door reads it in");
    return CLI_IPC_FRAME_PASSED;
}

int cli_ipc_frame_write(const CliIpcEndCtx *args)
{
    const unsigned char head[4] = {(unsigned char)(args->size & 0xffu), (unsigned char)((args->size >> 8u) & 0xffu),
                                   (unsigned char)((args->size >> 16u) & 0xffu),
                                   (unsigned char)((args->size >> 24u) & 0xffu)};
    // the region goes with the frame's first byte, its length's
    return EMBED_CALL(cli_ipc_end_size_internal.write, CliIpcEndSizeCtx, .end = args->end, .bytes_read = head,
                      .size = sizeof(head), .region = args->region) &&
           EMBED_CALL(cli_ipc_end_size_internal.write, CliIpcEndSizeCtx, .end = args->end, .bytes_read = args->bytes,
                      .size = args->size, .region = NULL);
}

unsigned long long cli_ipc_field_end(const char *frame, unsigned long long size, unsigned long long at)
{
    if (at >= size)
    {
        return 0ULL;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = frame + at, .cap = size - at});
    return ((at + length) < size) ? (at + length + 1ULL) : 0ULL;
}

int cli_ipc_dispatch(const CliIpcErrMsgRegionCtx *args)
{
    CliIpcTransfer transfer = {args->region->length, 0ULL};
#if defined(_WIN32)
    transfer.handle = args->region->section;
    if (args->has_to && !EMBED_CALL(cli_region_free, CliTransferCfg, .region_read = args->region, .process = args->to,
                                    .theirs = &transfer.handle, .err_msg = args->err_msg))
    {
        return 0;
    }
#endif
    mmgr_line_put_n(args->out, (const char *)&transfer, sizeof(transfer));
    return 1;
}

int cli_ipc_transferred_over(const CliIpcErrMsgCtx *args)
{
#if defined(_WIN32)
    return args->has_from
               ? EMBED_CALL(cli_region_alloc, CliTransferRegionCfg, .region = args->region_out, .process = args->from,
                            .handle = args->handle, .length = args->length, .err_msg = args->err_msg)
               : EMBED_CALL(cli_transfer_region.region_receive, CliTransferRegionCfg, .region = args->region_out,
                            .handle = args->handle, .length = args->length, .err_msg = args->err_msg);
#else
    const int fd = cli_ipc_err_msg_internal.descriptor(args);
    if (fd < 0)
    {
        mmgr_line_put(args->err_msg, "a transfer came with no region");
        return 0;
    }
    return EMBED_CALL(cli_transfer_region.region_receive, CliTransferRegionCfg, .region = args->region_out,
                      .handle = (unsigned long long)fd, .length = args->length, .err_msg = args->err_msg);
#endif
}

int cli_ipc_transfer(const CliIpcErrMsgSizeCtx *args)
{
    if (args->size != sizeof(CliIpcTransfer))
    {
        mmgr_line_put(args->err_msg, "a transfer names its length");
        return 0;
    }
    CliIpcTransfer transfer;
    memor.cpy(&(MemoriaCfg){.dst = &transfer, .src = args->transfer, .bytes = sizeof(transfer)});
    *args->length_pointer = transfer.length;
    *args->handle_pointer = transfer.handle;
    return 1;
}
