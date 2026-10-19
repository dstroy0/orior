// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_files.c: the command line's file sizes, moves and writes, and the wall clock on Windows
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

int cli_file_size(const char *path, unsigned long long *size)
{
    WIN32_FILE_ATTRIBUTE_DATA found;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &found))
    {
        return 0;
    }
    *size = ((unsigned long long)found.nFileSizeHigh << 32u) | found.nFileSizeLow;
    return 1;
}

int cli_file_stamp(const char *path, unsigned long long *size, unsigned long long *changed)
{
    WIN32_FILE_ATTRIBUTE_DATA found;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &found))
    {
        return 0;
    }
    *size = ((unsigned long long)found.nFileSizeHigh << 32u) | found.nFileSizeLow;
    const unsigned long long ticks =
        ((unsigned long long)found.ftLastWriteTime.dwHighDateTime << 32u) | found.ftLastWriteTime.dwLowDateTime;
    // the file time counts tenths of a microsecond from 1601; 1970 is 11644473600 seconds after
    *changed = (ticks >= 116444736000000000ull) ? ((ticks / 10000ull) - 11644473600000ull) : 0ull;
    return 1;
}

unsigned long long cli_wall_milliseconds(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const unsigned long long ticks = ((unsigned long long)now.dwHighDateTime << 32u) | now.dwLowDateTime;
    // the file time counts tenths of a microsecond from 1601; 1970 is 11644473600 seconds after
    return (ticks / 10000ull) - 11644473600000ull;
}

int cli_moved(const char *from, const char *to)
{
    wchar_t wide_from[CLI_PATH_LONGEST];
    wchar_t wide_to[CLI_PATH_LONGEST];
    if ((MultiByteToWideChar(CP_UTF8, 0, from, -1, wide_from, CLI_PATH_LONGEST) == 0) ||
        (MultiByteToWideChar(CP_UTF8, 0, to, -1, wide_to, CLI_PATH_LONGEST) == 0))
    {
        return CLI_MOVE_FAILED;
    }
    if (MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING))
    {
        return CLI_MOVE_DONE;
    }
    return (GetLastError() == ERROR_NOT_SAME_DEVICE) ? CLI_MOVE_ACROSS : CLI_MOVE_FAILED;
}

int cli_file_appended(const char *path, const void *bytes, unsigned long long size, int fresh)
{
    wchar_t wide[CLI_PATH_LONGEST];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, CLI_PATH_LONGEST) == 0)
    {
        return 0;
    }
    const HANDLE file = CreateFileW(wide, fresh ? GENERIC_WRITE : FILE_APPEND_DATA, 0u, NULL,
                                    fresh ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    const unsigned char *at = (const unsigned char *)bytes;
    unsigned long long left = size;
    int written = 1;
    while (written && (left > 0ull))
    {
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD asked = (DWORD)((left < (1ull << 30u)) ? left : (1ull << 30u));
        DWORD done = 0u;
        written = WriteFile(file, at, asked, &done, NULL) && (done == asked);
        at += done;
        left -= done;
    }
    return CloseHandle(file) && written;
}

int cli_resource_added(const char *path, const char *name, const void *data, unsigned long long size, CliLine *said)
{
    wchar_t wide[CLI_PATH_LONGEST];
    wchar_t named[CLI_NAME_LONGEST];
    if ((MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, CLI_PATH_LONGEST) == 0) ||
        (MultiByteToWideChar(CP_UTF8, 0, name, -1, named, CLI_NAME_LONGEST) == 0))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": the path or the name is too long");
        return 0;
    }
    if (size > 0xffffffffull)
    {
        cli_line_text(said, "the blob is past 4 GB");
        return 0;
    }
    const HANDLE update = BeginUpdateResourceW(wide, FALSE);
    if (update == NULL)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": Windows said ");
        cli_line_decimal(said, GetLastError(), 1u);
        return 0;
    }
    // the blob's size is below 4 GB, as checked above, which a DWORD counts; the RCDATA type's number stands for a
    // name, as Windows takes it
    if (!UpdateResourceW(update, MAKEINTRESOURCEW(10u), named, 0u, (LPVOID)(uintptr_t)data, (DWORD)size))
    {
        const DWORD error = GetLastError();
        EndUpdateResourceW(update, TRUE);
        cli_line_text(said, path);
        cli_line_text(said, ": Windows said ");
        cli_line_decimal(said, error, 1u);
        return 0;
    }
    if (!EndUpdateResourceW(update, FALSE))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": Windows said ");
        cli_line_decimal(said, GetLastError(), 1u);
        return 0;
    }
    return 1;
}
#endif
