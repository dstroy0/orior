// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_transfer.c: a region of the transfer layer on Linux, a memory file sealed against every change once written,
// and read from another process only where it carries every seal (../posix_systems.h)
#if defined(__linux__)
// memfd_create and its seals are Linux's own, outside strict C11
#define _GNU_SOURCE
#endif
#include "../../../../ui/cli/src/comms/transfer.h"
#include "../posix_systems.h"

#if defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// the seals that a region from another process carries before a read of it. It carries them against a write, a
// shrink and a growth, and against any of those seals taken off. And the calls that add them and read them, where the
// headers do not name them
#define CLI_SEALED 15
#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034
#endif
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 1u
#define MFD_ALLOW_SEALING 2u
#endif

int cli_transfer_memory_opened(VerbaLinea *err_msg)
{
    const int fd = memfd_create("orior-transfer", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
    {
        cli_transfer_failed(err_msg, "the system made no region");
    }
    return fd;
}

void *cli_transfer_region_kept(int fd, void *view, unsigned long long capacity, VerbaLinea *err_msg)
{
    // the call seals a region against a write only once no view of it can write
    munmap(view, (embed_word)capacity);
    if (fcntl(fd, F_ADD_SEALS, CLI_SEALED) != 0)
    {
        cli_transfer_failed(err_msg, "the region was not sealed");
        return NULL;
    }
    void *const sealed = mmap(NULL, (embed_word)capacity, PROT_READ, MAP_SHARED, fd, 0);
    if (sealed == MAP_FAILED)
    {
        cli_transfer_failed(err_msg, "the system showed no region");
        return NULL;
    }
    return sealed;
}

const void *cli_transfer_region_taken(int fd, unsigned long long length, unsigned long long capacity,
                                      VerbaLinea *err_msg, int *section)
{
    (void)length;
    if ((fcntl(fd, F_GET_SEALS) & CLI_SEALED) != CLI_SEALED)
    {
        mmgr_line_put(err_msg, "the region is not sealed against change");
        return NULL;
    }
    // the view is no longer than the region, which no one can shrink
    const void *const view = mmap(NULL, (embed_word)capacity, PROT_READ, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        cli_transfer_failed(err_msg, "the system showed no region");
        return NULL;
    }
    *section = fd;
    return view;
}
#endif
