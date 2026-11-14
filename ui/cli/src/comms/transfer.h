// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// transfer.h: the container's transfer layer: what is too large for a message, a file's text, a statement's rows, a
// run's output, goes through memory shared with the process it is for, and the message carries only its length and the
// means to reach it.
//
// A region has no name another process could find. On Windows it is a section of the paging file, and the container
// makes its handle in the other process itself, as a broker does: a process of the container's passes over a region by
// the handle it has, and is passed one by a handle made for it, which reads the region and cannot write it. On Linux a
// region is a memory file sealed against every change once written, and on macOS a file in the reader's own temporary
// folder, unlinked as soon as it is made; either goes over the interface's socket with the frame that names it, and a region
// from another process is read only once its seals are applied, or on macOS copied whole into a region of this
// process's own (platform/windows_transfer.c, platform/posix_transfer.c, platform/posix_descriptors.c)
#ifndef TRANSFER_H
#define TRANSFER_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliTransferRegionCfg CliTransferRegionCfg;
typedef struct CliTransferCfg CliTransferCfg;
typedef struct CliTransferRegionBytesCfg CliTransferRegionBytesCfg;
typedef struct CliTransferSocketCfg CliTransferSocketCfg;
typedef struct CliTransferSizeCfg CliTransferSizeCfg;

// the least body, in bytes, that goes through the transfer layer, a smaller one going in its frame; and the max a
// region contains
#define CLI_TRANSFER_SMALL (64u << 10u)
#define CLI_TRANSFER_MAX (1ull << 30u)

// A region of memory shared with another process: its section's handle on Windows, its descriptor elsewhere, this
// process's view of it, its length, and whether it contains one
typedef struct
{
    unsigned long long section;
    const unsigned char *view;
    unsigned long long length;
    int mapped;
} CliRegion;

// a region containing `size` bytes of `bytes`, sealed on Linux against every change from now on; the region of `length`
// bytes the container made in this process as `handle`, or whose descriptor `handle` came from another process; and
// the region let go. Each 1, or 0 with why given
int cli_region_with(const CliTransferRegionBytesCfg *args);
int cli_region_receive(const CliTransferRegionCfg *args);
void cli_region_close(const CliTransferRegionBytesCfg *args);

#if defined(_WIN32)
// a handle to the region made in the process `process`, which reads the region and cannot write it, its value there
// into `theirs`; and the region of `length` bytes the process `process` names by its handle `handle` there, taken into
// this process to be read. Each 1, or 0 with why given
int cli_region_free(const CliTransferCfg *args);
int cli_region_alloc(const CliTransferRegionCfg *args);
#else
// the max descriptors retained that came with bytes read and are not yet taken
#define CLI_DESCRIPTORS_PENDING_MAX 64u

// The descriptors that came with the bytes read from a socket and are not yet taken, in the order they came, and their
// lock; those never taken are closed with them
typedef struct
{
    int fd[CLI_DESCRIPTORS_PENDING_MAX];
    unsigned int start;
    unsigned int count;
} CliDescriptors;

// `size` bytes of `frame` written to `socket`, the descriptor `fd` going with the first byte: 1, or 0 where it could
// not be; what came on `socket` read into `buffer`, sized for `capacity`, each descriptor that came with it stored in
// `pending`: the count read, 0 at the end, -1 where the read failed; the first descriptor stored taken, -1 where none
// is; and every descriptor pending closed
int cli_socket_sent_with(const CliTransferSizeCfg *args);
long long cli_socket_receive(const CliTransferSocketCfg *args);
int cli_descriptor_alloc(const CliTransferSocketCfg *args);
void cli_descriptors_close(const CliTransferSocketCfg *args);
#endif

struct CliTransferRegionCfg
{
    CliRegion *region;
    VerbaLinea *err_msg;
    unsigned long long handle;
    unsigned long long length;
#if (defined(_WIN32))
    unsigned int process;
#endif
};

struct CliTransferCfg
{
    VerbaLinea *err_msg;
#if (defined(_WIN32))
    const CliRegion *region_read;
    unsigned int process;
    unsigned long long *theirs;
#endif
};

struct CliTransferRegionBytesCfg
{
    CliRegion *region;
    const void *bytes;
    unsigned long long size;
    VerbaLinea *err_msg;
};

struct CliTransferSocketCfg
{
#if !((defined(_WIN32)))
    int socket;
    void *buffer;
    unsigned long long capacity;
    CliDescriptors *pending;
#endif
};

struct CliTransferSizeCfg
{
    unsigned long long size;
#if !((defined(_WIN32)))
    int socket;
    const void *frame;
    int fd;
#endif
};

typedef struct
{
    int (*region_with)(const CliTransferRegionBytesCfg *args);
    void (*region_close)(const CliTransferRegionBytesCfg *args);
} CliTransferRegionBytesNs;
EMBED_TABLE_LAYOUT(CliTransferRegionBytesNs, region_with, region_close);
EMBED_TABLE_STORAGE CliTransferRegionBytesNs cli_transfer_region_bytes EMBED_UNUSED = {
    .region_with = cli_region_with,
    .region_close = cli_region_close,
};

typedef struct
{
    int (*region_receive)(const CliTransferRegionCfg *args);
} CliTransferRegionNs;
EMBED_TABLE_LAYOUT(CliTransferRegionNs, region_receive);
EMBED_TABLE_STORAGE CliTransferRegionNs cli_transfer_region EMBED_UNUSED = {
    .region_receive = cli_region_receive,
};

#endif
