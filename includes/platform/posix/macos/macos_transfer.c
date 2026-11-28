// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_transfer.c: a region of the transfer layer on macOS, an unlinked file in the reader's own temporary folder,
// and copied whole into a region of this process's own as it comes from another process (../posix_systems.h)
#if defined(__APPLE__)
// sendmsg's control headers and pread are POSIX, outside strict C11
#define _DARWIN_C_SOURCE
#endif
#include "../../../../ui/cli/src/comms/transfer.h"
#include "../posix_systems.h"

#if defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int cli_transfer_memory_opened(VerbaLinea *err_msg)
{
    unsigned char secret[8];
    char folder[CLI_PATH_MAX_LEN];
    char name[96];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = secret, .size = sizeof(secret)))
    {
        mmgr_line_put(err_msg, "the system gave no secret");
        return -1;
    }
    VerbaLinea named = {name, sizeof(name), 0ULL};
    mmgr_line_put(&named, "orior-transfer-");
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '-');
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&named);
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = path);
    const int fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0)
    {
        cli_transfer_failed(err_msg, path);
        return -1;
    }
    (void)unlink(path);
    return fd;
}

void *cli_transfer_region_kept(int fd, void *view, unsigned long long capacity, VerbaLinea *err_msg)
{
    // macOS has no seal, and the reader copies the region whole as it comes
    (void)fd;
    (void)capacity;
    (void)err_msg;
    return view;
}

const void *cli_transfer_region_taken(int fd, unsigned long long length, unsigned long long capacity,
                                      VerbaLinea *err_msg, int *section)
{
    // the process that made the region could change it still. So the call copies it whole into a region of this
    // process's
    // own, which no one else has
    const int own = cli_transfer_region_made(capacity, err_msg);
    if (own < 0)
    {
        return NULL;
    }
    void *const view = mmap(NULL, (embed_word)capacity, PROT_READ | PROT_WRITE, MAP_SHARED, own, 0);
    if (view == MAP_FAILED)
    {
        cli_transfer_failed(err_msg, "the system showed no region");
        close(own);
        return NULL;
    }
    for (unsigned long long at = 0ULL; at < length;)
    {
        const ssize_t got = pread(fd, (unsigned char *)view + at, (embed_word)(length - at), (off_t)at);
        if (got <= 0)
        {
            mmgr_line_put(err_msg, "the region ended at ");
            mmgr_line_decimal(err_msg, at, 1u);
            mmgr_line_put(err_msg, " of its ");
            mmgr_line_decimal(err_msg, length, 1u);
            mmgr_line_put(err_msg, " bytes");
            munmap(view, (embed_word)capacity);
            close(own);
            return NULL;
        }
        // a count read is positive here
        at += (unsigned long long)got;
    }
    close(fd);
    *section = own;
    return view;
}
#endif
