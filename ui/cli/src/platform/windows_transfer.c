// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_transfer.c: a region of the transfer layer on Windows: a section of the paging file, viewed in this process,
// its handle made in another process by DuplicateHandle, which reads it and cannot write it
#include "../comms/transfer.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <stdint.h>

typedef struct
{
#if (defined(_WIN32))
    VerbaLinea *err_msg;
    const char *what;
    CliRegion *region;
    HANDLE section;
    unsigned long long length;
#endif
} WindowsTransferCtx;

// what Windows reported of the last call that failed, after `what`
EMBED_INLINE void transfer_fail(const WindowsTransferCtx *args)
{
    const DWORD error = GetLastError();
    mmgr_line_put(args->err_msg, args->what);
    mmgr_line_put(args->err_msg, ": Windows reported ");
    mmgr_line_decimal(args->err_msg, error, 1u);
}

int cli_region_with(const CliTransferRegionBytesCfg *args)
{
    const void *bytes = args->bytes;
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
    if (args->size > CLI_TRANSFER_MAX)
    {
        mmgr_line_decimal(args->err_msg, args->size, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the most a region supports");
        return 0;
    }
    const unsigned long long capacity = (args->size > 0ull) ? args->size : 1ull;
    // the section's size given as its two halves, as Windows takes it
    const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)(capacity >> 32u),
                                              (DWORD)(capacity & 0xffffffffull), NULL);
    if (section == NULL)
    {
        EMBED_CALL(transfer_fail, WindowsTransferCtx, .err_msg = args->err_msg, .what = "the system made no region");
        return 0;
    }
    unsigned char *const view =
        (unsigned char *)MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_WRITE, 0u, 0u, (SIZE_T)capacity);
    if (view == NULL)
    {
        EMBED_CALL(transfer_fail, WindowsTransferCtx, .err_msg = args->err_msg, .what = "the system showed no region");
        CloseHandle(section);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = view, .src = bytes, .bytes = (size_t)args->size});
    // the handle's value stored as a number, as the frame carries it
    args->region->section = (unsigned long long)(uintptr_t)section;
    args->region->view = view;
    args->region->length = args->size;
    args->region->mapped = 1;
    return 1;
}

int cli_region_free(const CliTransferCfg *args)
{
    unsigned long long *theirs = args->theirs;
    VerbaLinea *err_msg = args->err_msg;
    const HANDLE target = OpenProcess(PROCESS_DUP_HANDLE, FALSE, args->process);
    if (target == NULL)
    {
        char what[64];
        VerbaLinea line = {what, sizeof(what), 0ull};
        mmgr_line_put(&line, "process ");
        mmgr_line_decimal(&line, args->process, 1u);
        mmgr_line_put(&line, " was not opened");
        mmgr_line_finish(&line);
        EMBED_CALL(transfer_fail, WindowsTransferCtx, .err_msg = err_msg, .what = what);
        return 0;
    }
    HANDLE made = NULL;
    const BOOL supplied = DuplicateHandle(GetCurrentProcess(), (HANDLE)(uintptr_t)args->region_read->section, target,
                                          &made, FILE_MAP_READ, FALSE, 0u);
    const DWORD error = GetLastError();
    CloseHandle(target);
    if (!supplied)
    {
        mmgr_line_put(err_msg, "the region was not given to process ");
        mmgr_line_decimal(err_msg, args->process, 1u);
        mmgr_line_put(err_msg, ": Windows reported ");
        mmgr_line_decimal(err_msg, error, 1u);
        return 0;
    }
    *theirs = (unsigned long long)(uintptr_t)made;
    return 1;
}

// the section `section`, this process's, viewed to be read as a region of `length` bytes, the handle closed where it
// is none
EMBED_INLINE int transfer_map(const WindowsTransferCtx *args)
{
    HANDLE section = args->section;
    unsigned long long length = args->length;
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
    if (length > CLI_TRANSFER_MAX)
    {
        CloseHandle(section);
        mmgr_line_decimal(args->err_msg, length, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the most a region supports");
        return 0;
    }
    // a view longer than the section is refused by the system
    const unsigned char *const view =
        (const unsigned char *)MapViewOfFile(section, FILE_MAP_READ, 0u, 0u, (SIZE_T)((length > 0ull) ? length : 1ull));
    if (view == NULL)
    {
        const DWORD error = GetLastError();
        CloseHandle(section);
        mmgr_line_put(args->err_msg, "no region of ");
        mmgr_line_decimal(args->err_msg, length, 1u);
        mmgr_line_put(args->err_msg, " bytes: Windows reported ");
        mmgr_line_decimal(args->err_msg, error, 1u);
        return 0;
    }
    args->region->section = (unsigned long long)(uintptr_t)section;
    args->region->view = view;
    args->region->length = length;
    args->region->mapped = 1;
    return 1;
}

int cli_region_alloc(const CliTransferRegionCfg *args)
{
    CliRegion *region = args->region;
    unsigned long long length = args->length;
    VerbaLinea *err_msg = args->err_msg;
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
    const HANDLE source = OpenProcess(PROCESS_DUP_HANDLE, FALSE, args->process);
    if (source == NULL)
    {
        char what[64];
        VerbaLinea line = {what, sizeof(what), 0ull};
        mmgr_line_put(&line, "process ");
        mmgr_line_decimal(&line, args->process, 1u);
        mmgr_line_put(&line, " was not opened");
        mmgr_line_finish(&line);
        EMBED_CALL(transfer_fail, WindowsTransferCtx, .err_msg = err_msg, .what = what);
        return 0;
    }
    HANDLE own = NULL;
    const BOOL reserved =
        DuplicateHandle(source, (HANDLE)(uintptr_t)args->handle, GetCurrentProcess(), &own, FILE_MAP_READ, FALSE, 0u);
    const DWORD error = GetLastError();
    CloseHandle(source);
    if (!reserved)
    {
        mmgr_line_put(err_msg, "the region of process ");
        mmgr_line_decimal(err_msg, args->process, 1u);
        mmgr_line_put(err_msg, " was not taken: Windows reported ");
        mmgr_line_decimal(err_msg, error, 1u);
        return 0;
    }
    return EMBED_CALL(transfer_map, WindowsTransferCtx, .region = region, .section = own, .length = length,
                      .err_msg = err_msg);
}

int cli_region_receive(const CliTransferRegionCfg *args)
{
    return EMBED_CALL(transfer_map, WindowsTransferCtx, .region = args->region,
                      .section = (HANDLE)(uintptr_t)args->handle, .length = args->length, .err_msg = args->err_msg);
}

void cli_region_close(const CliTransferRegionBytesCfg *args)
{
    if (args->region->mapped)
    {
        UnmapViewOfFile(args->region->view);
        CloseHandle((HANDLE)(uintptr_t)args->region->section);
    }
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
}
#endif
