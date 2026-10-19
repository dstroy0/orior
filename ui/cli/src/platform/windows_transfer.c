// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_transfer.c: a region of the transfer layer on Windows: a section of the paging file, viewed in this process,
// its handle made in another process by DuplicateHandle, which reads it and cannot write it
#include "../comms/transfer.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <stdint.h>

// what Windows said of the last call that failed, after `what`
static void transfer_failed(CliLine *said, const char *what)
{
    const DWORD error = GetLastError();
    cli_line_text(said, what);
    cli_line_text(said, ": Windows said ");
    cli_line_decimal(said, error, 1u);
}

int cli_region_with(CliRegion *region, const void *bytes, unsigned long long size, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
    if (size > CLI_TRANSFER_MOST)
    {
        cli_line_decimal(said, size, 1u);
        cli_line_text(said, " bytes is past the most a region holds");
        return 0;
    }
    const unsigned long long room = (size > 0ull) ? size : 1ull;
    // the section's size given as its two halves, as Windows takes it
    const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)(room >> 32u),
                                              (DWORD)(room & 0xffffffffull), NULL);
    if (section == NULL)
    {
        transfer_failed(said, "the system made no region");
        return 0;
    }
    unsigned char *const view =
        (unsigned char *)MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_WRITE, 0u, 0u, (SIZE_T)room);
    if (view == NULL)
    {
        transfer_failed(said, "the system showed no region");
        CloseHandle(section);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = view, .src = bytes, .bytes = (size_t)size});
    // the handle's value kept as a number, as the frame carries it
    region->section = (unsigned long long)(uintptr_t)section;
    region->view = view;
    region->length = size;
    region->held = 1;
    return 1;
}

int cli_region_given(const CliRegion *region, unsigned int process, unsigned long long *theirs, CliLine *said)
{
    const HANDLE target = OpenProcess(PROCESS_DUP_HANDLE, FALSE, process);
    if (target == NULL)
    {
        char what[64];
        CliLine line = {what, sizeof(what), 0ull};
        cli_line_text(&line, "process ");
        cli_line_decimal(&line, process, 1u);
        cli_line_text(&line, " was not opened");
        cli_line_closed(&line);
        transfer_failed(said, what);
        return 0;
    }
    HANDLE made = NULL;
    const BOOL given = DuplicateHandle(GetCurrentProcess(), (HANDLE)(uintptr_t)region->section, target, &made,
                                       FILE_MAP_READ, FALSE, 0u);
    const DWORD error = GetLastError();
    CloseHandle(target);
    if (!given)
    {
        cli_line_text(said, "the region was not given to process ");
        cli_line_decimal(said, process, 1u);
        cli_line_text(said, ": Windows said ");
        cli_line_decimal(said, error, 1u);
        return 0;
    }
    *theirs = (unsigned long long)(uintptr_t)made;
    return 1;
}

// the section `section`, this process's, viewed to be read as a region of `length` bytes, the handle closed where it
// is none
static int transfer_mapped(CliRegion *region, HANDLE section, unsigned long long length, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
    if (length > CLI_TRANSFER_MOST)
    {
        CloseHandle(section);
        cli_line_decimal(said, length, 1u);
        cli_line_text(said, " bytes is past the most a region holds");
        return 0;
    }
    // a view longer than the section is refused by the system
    const unsigned char *const view =
        (const unsigned char *)MapViewOfFile(section, FILE_MAP_READ, 0u, 0u, (SIZE_T)((length > 0ull) ? length : 1ull));
    if (view == NULL)
    {
        const DWORD error = GetLastError();
        CloseHandle(section);
        cli_line_text(said, "no region of ");
        cli_line_decimal(said, length, 1u);
        cli_line_text(said, " bytes: Windows said ");
        cli_line_decimal(said, error, 1u);
        return 0;
    }
    region->section = (unsigned long long)(uintptr_t)section;
    region->view = view;
    region->length = length;
    region->held = 1;
    return 1;
}

int cli_region_taken(CliRegion *region, unsigned int process, unsigned long long handle, unsigned long long length,
                     CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
    const HANDLE source = OpenProcess(PROCESS_DUP_HANDLE, FALSE, process);
    if (source == NULL)
    {
        char what[64];
        CliLine line = {what, sizeof(what), 0ull};
        cli_line_text(&line, "process ");
        cli_line_decimal(&line, process, 1u);
        cli_line_text(&line, " was not opened");
        cli_line_closed(&line);
        transfer_failed(said, what);
        return 0;
    }
    HANDLE own = NULL;
    const BOOL taken =
        DuplicateHandle(source, (HANDLE)(uintptr_t)handle, GetCurrentProcess(), &own, FILE_MAP_READ, FALSE, 0u);
    const DWORD error = GetLastError();
    CloseHandle(source);
    if (!taken)
    {
        cli_line_text(said, "the region of process ");
        cli_line_decimal(said, process, 1u);
        cli_line_text(said, " was not taken: Windows said ");
        cli_line_decimal(said, error, 1u);
        return 0;
    }
    return transfer_mapped(region, own, length, said);
}

int cli_region_received(CliRegion *region, unsigned long long handle, unsigned long long length, CliLine *said)
{
    return transfer_mapped(region, (HANDLE)(uintptr_t)handle, length, said);
}

void cli_region_closed(CliRegion *region)
{
    if (region->held)
    {
        UnmapViewOfFile(region->view);
        CloseHandle((HANDLE)(uintptr_t)region->section);
    }
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
}
#endif
