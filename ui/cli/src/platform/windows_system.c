// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_system.c: what the command line asks of Windows itself: its randomness, the folder it keeps passing files in,
// and an event a started process inherits, by which ipykernel is interrupted
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <ntsecapi.h>

#include <stdint.h>

int cli_random_bytes(void *bytes, unsigned long long size)
{
    unsigned char *const into = (unsigned char *)bytes;
    unsigned long long given = 0ull;
    while (given < size)
    {
        const unsigned long long left = size - given;
        // a piece is at most what a ULONG counts
        const ULONG piece = (left > 0xFFFFFFFFull) ? 0xFFFFFFFFul : (ULONG)left;
        if (!RtlGenRandom(into + given, piece))
        {
            return 0;
        }
        given += piece;
    }
    return 1;
}

void cli_temp_folder(char *folder)
{
    const DWORD length = GetTempPathA(CLI_PATH_LONGEST, folder);
    if ((length == 0u) || (length >= CLI_PATH_LONGEST))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
        return;
    }
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    while ((end > 3u) && ((folder[end - 1u] == '\\') || (folder[end - 1u] == '/')))
    {
        end -= 1u;
    }
    folder[end] = '\0';
}

unsigned long long cli_event_inheritable(void)
{
    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    const HANDLE event = CreateEventW(&attributes, FALSE, FALSE, NULL);
    // a handle is a pointer's width, which an unsigned long long holds
    return (event != NULL) ? (unsigned long long)(uintptr_t)event : 0ull;
}

int cli_event_set(unsigned long long event)
{
    return (event != 0ull) && SetEvent((HANDLE)(uintptr_t)event);
}

void cli_event_closed(unsigned long long event)
{
    if (event != 0ull)
    {
        CloseHandle((HANDLE)(uintptr_t)event);
    }
}

// a folder and a file of the reader's own are the reader's alone by the profile's own rights on Windows
int cli_private_folder_made(const char *folder)
{
    return query_folder_made(folder);
}

int cli_private_file_written(const char *path, const void *bytes, unsigned long long size)
{
    return query_file_written(path, bytes, size);
}

#endif
