// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// transfer.h: the container's transfer layer: what is too large for a message, a file's text, a statement's rows, a
// run's output, goes through memory shared with the process it is for, and the message carries only its length and the
// means to reach it.
//
// A region has no name another process could find. On Windows it is a section of the paging file, and the container
// makes its handle in the other process itself, as a broker does: a process of the container's hands over a region by
// the handle it has, and is handed one by a handle made for it, which reads the region and cannot write it. On Linux a
// region is a memory file sealed against every change once written, and on macOS a file in the reader's own temporary
// folder, unlinked as soon as it is made; either goes over the door's socket with the frame that names it, and a region
// from another process is read only once its seals hold, or on macOS copied whole into a region of this process's own
// (platform/windows_transfer.c, platform/posix_transfer.c, platform/posix_descriptors.c)
#ifndef TRANSFER_H
#define TRANSFER_H

#include "../command_line/cli_internal.h"

// the least body, in bytes, that goes through the transfer layer, a smaller one going in its frame; and the most a
// region holds
#define CLI_TRANSFER_SMALL (64u << 10u)
#define CLI_TRANSFER_MOST (1ull << 30u)

// A region of memory shared with another process: its section's handle on Windows, its descriptor elsewhere, this
// process's view of it, its length, and whether it holds one
typedef struct
{
    unsigned long long section;
    const unsigned char *view;
    unsigned long long length;
    int held;
} CliRegion;

// a region holding `size` bytes of `bytes`, sealed on Linux against every change from now on; the region of `length`
// bytes the container made in this process as `handle`, or whose descriptor `handle` came from another process; and
// the region let go. Each 1, or 0 with why said
int cli_region_with(CliRegion *region, const void *bytes, unsigned long long size, CliLine *said);
int cli_region_received(CliRegion *region, unsigned long long handle, unsigned long long length, CliLine *said);
void cli_region_closed(CliRegion *region);

#if defined(_WIN32)
// a handle to the region made in the process `process`, which reads the region and cannot write it, its value there
// into `theirs`; and the region of `length` bytes the process `process` names by its handle `handle` there, taken into
// this process to be read. Each 1, or 0 with why said
int cli_region_given(const CliRegion *region, unsigned int process, unsigned long long *theirs, CliLine *said);
int cli_region_taken(CliRegion *region, unsigned int process, unsigned long long handle, unsigned long long length,
                     CliLine *said);
#else
// the most descriptors kept that came with bytes read and are not yet taken
#define CLI_DESCRIPTORS_KEPT 64u

// The descriptors that came with the bytes read from a socket and are not yet taken, in the order they came, and their
// lock; those never taken are closed with them
typedef struct
{
    int fd[CLI_DESCRIPTORS_KEPT];
    unsigned int start;
    unsigned int count;
    CliLock lock;
} CliDescriptors;

// `size` bytes of `frame` written to `socket`, the descriptor `fd` going with the first byte: 1, or 0 where it could
// not be; what came on `socket` read into `buffer`, which holds `room`, each descriptor that came with it kept in
// `kept`: the count read, 0 at the end, -1 where the read failed; the first descriptor kept taken, -1 where none is;
// and every descriptor kept closed
int cli_socket_sent_with(int socket, const void *frame, unsigned long long size, int fd);
long long cli_socket_received(int socket, void *buffer, unsigned long long room, CliDescriptors *kept);
int cli_descriptor_taken(CliDescriptors *kept);
void cli_descriptors_closed(CliDescriptors *kept);
#endif

#endif
