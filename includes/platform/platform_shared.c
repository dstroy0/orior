// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_shared.c: the calls of the platform that are the same on every system. Each is built on the calls of the
// system files (windows/platform_windows.c, posix/platform_posix.c) and their facts, and none calls the system itself.
// It takes no cell of the pool, and so a program with a pool of its own links it too
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "platform_internal.h"

unsigned long long platform_epoch_seconds(void)
{
    return platform_epoch_microseconds() / 1000000ULL;
}

int platform_folder_made(const char *path)
{
    // a folder that platform_folder_made makes is the reader's to change, and every reader's to read and to search
    return platform_folder_made_as(path, 0755u);
}

int platform_folders_made(const char *path, unsigned int mode)
{
    char prefix[PLATFORM_PATH_BYTES];
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = sizeof(prefix));
    if (length >= sizeof(prefix))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = prefix, .src = path, .bytes = length + 1u);
    // Each run of bytes up to a separator is one folder along the path, and the end of the path ends the last. An
    // empty run is no folder: it is a separator after a separator, or the start of a shared path. A run that ends in
    // the mark of a drive is the drive's own name, which is no folder to make
    size_t at = 0u;
    while (at < length)
    {
        const size_t run = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = prefix + at, .cap = length - at,
                                      .other = g_platform_separators);
        at += run;
        // an empty run reads no byte before it, since `at` stays where it was
        const int drive = (run != 0u) && (g_platform_drive_mark != '\0') && (prefix[at - 1u] == g_platform_drive_mark);
        if ((run != 0u) && !drive)
        {
            const char kept = prefix[at];
            prefix[at] = '\0';
            (void)platform_folder_made_as(prefix, mode);
            prefix[at] = kept;
        }
        at += 1u;
    }
    return platform_folder_is(path);
}
