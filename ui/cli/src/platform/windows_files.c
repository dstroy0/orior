// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_files.c: the command line's file sizes, moves and writes, and the wall clock on Windows
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

unsigned int cli_system_error(void)
{
    // the system's error codes are below what an unsigned int stores
    return (unsigned int)GetLastError();
}

void cli_system_reason(unsigned int error, VerbaLinea *err_msg)
{
    char reason[512];
    DWORD size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0u, reason,
                                sizeof(reason), NULL);
    while ((size > 0u) && ((reason[size - 1u] == '\r') || (reason[size - 1u] == '\n') || (reason[size - 1u] == ' ')))
    {
        size -= 1u;
    }
    mmgr_line_put(err_msg, ": ");
    mmgr_line_put_n(err_msg, reason, size);
    mmgr_line_put(err_msg, (size > 0u) ? " (os error " : "(os error ");
    mmgr_line_decimal(err_msg, error, 1u);
    mmgr_line_ch(err_msg, ')');
}

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

// `count` bytes of the open `file` from `from` read into `bytes`, their count into `got`, fewer where the file ends
static void file_read_from(HANDLE file, unsigned long long from, unsigned char *bytes, unsigned long long count,
                           unsigned long long *got)
{
    *got = 0ull;
    int reading = 1;
    while (reading && (*got < count))
    {
        OVERLAPPED at;
        memor.set(&(MemoriaCfg){.dst = &at, .val = 0, .bytes = sizeof(at)});
        // the offset's low and high words, each what a DWORD stores
        at.Offset = (DWORD)((from + *got) & 0xffffffffull);
        at.OffsetHigh = (DWORD)((from + *got) >> 32u);
        // one read takes at most what a DWORD counts
        const DWORD chunk = (DWORD)(((count - *got) < 0x40000000ull) ? (count - *got) : 0x40000000ull);
        DWORD read = 0u;
        const BOOL done = ReadFile(file, bytes + *got, chunk, &read, &at);
        // a read past the file's end is the file's end
        reading = (done || (GetLastError() == ERROR_HANDLE_EOF)) && (read != 0u);
        *got += read;
    }
}

// the file at `path` opened to read, shared with every other opener, read once through
static HANDLE file_opened(const char *path)
{
    return CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
}

int cli_file_read_at(const char *path, unsigned long long from, unsigned char *bytes, unsigned long long count,
                     unsigned long long *got)
{
    const HANDLE file = file_opened(path);
    *got = 0ull;
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    file_read_from(file, from, bytes, count, got);
    CloseHandle(file);
    return 1;
}

int cli_file_read_whole(const char *path, unsigned char *(*room)(void *context, unsigned long long size), void *context,
                        unsigned long long *got)
{
    *got = 0ull;
    const HANDLE file = file_opened(path);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    LARGE_INTEGER measured;
    if (!GetFileSizeEx(file, &measured))
    {
        CloseHandle(file);
        return 0;
    }
    // a file's size is not negative
    const unsigned long long size = (unsigned long long)measured.QuadPart;
    unsigned char *const bytes = room(context, size);
    if (bytes == NULL)
    {
        CloseHandle(file);
        return 0;
    }
    file_read_from(file, 0ull, bytes, size, got);
    CloseHandle(file);
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
    wchar_t wide_from[CLI_PATH_MAX_LEN];
    wchar_t wide_to[CLI_PATH_MAX_LEN];
    if ((MultiByteToWideChar(CP_UTF8, 0, from, -1, wide_from, CLI_PATH_MAX_LEN) == 0) ||
        (MultiByteToWideChar(CP_UTF8, 0, to, -1, wide_to, CLI_PATH_MAX_LEN) == 0))
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
    wchar_t wide[CLI_PATH_MAX_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, CLI_PATH_MAX_LEN) == 0)
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
        const DWORD chunk = (DWORD)((left < (1ull << 30u)) ? left : (1ull << 30u));
        DWORD done = 0u;
        written = WriteFile(file, at, chunk, &done, NULL) && (done == chunk);
        at += done;
        left -= done;
    }
    return CloseHandle(file) && written;
}

int cli_resource_added(const char *path, const char *name, const void *data, unsigned long long size,
                       VerbaLinea *err_msg)
{
    wchar_t wide[CLI_PATH_MAX_LEN];
    wchar_t named[CLI_NAME_MAX_LEN];
    if ((MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, CLI_PATH_MAX_LEN) == 0) ||
        (MultiByteToWideChar(CP_UTF8, 0, name, -1, named, CLI_NAME_MAX_LEN) == 0))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": the path or the name is too long");
        return 0;
    }
    if (size > 0xffffffffull)
    {
        mmgr_line_put(err_msg, "the blob is past 4 GB");
        return 0;
    }
    const HANDLE update = BeginUpdateResourceW(wide, FALSE);
    if (update == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": Windows reported ");
        mmgr_line_decimal(err_msg, GetLastError(), 1u);
        return 0;
    }
    // the blob's size is below 4 GB, as checked above, which a DWORD counts; the RCDATA type's number stands for a
    // name, as Windows takes it
    if (!UpdateResourceW(update, MAKEINTRESOURCEW(10u), named, 0u, (LPVOID)(uintptr_t)data, (DWORD)size))
    {
        const DWORD error = GetLastError();
        EndUpdateResourceW(update, TRUE);
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": Windows reported ");
        mmgr_line_decimal(err_msg, error, 1u);
        return 0;
    }
    if (!EndUpdateResourceW(update, FALSE))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": Windows reported ");
        mmgr_line_decimal(err_msg, GetLastError(), 1u);
        return 0;
    }
    return 1;
}

// the bytes of randomness a temporary file's name carries
#define CLI_TEMPORARY_NAME_BYTES 8u

int cli_temporary_file_made(CliTemporaryFile *file)
{
    char path[CLI_PATH_MAX_LEN];
    cli_temp_folder(path);
    unsigned char random[CLI_TEMPORARY_NAME_BYTES];
    if (!cli_random_bytes(random, sizeof(random)))
    {
        return 0;
    }
    VerbaLinea named = {path, sizeof(path), cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL})};
    mmgr_line_put(&named, "\\orior-");
    mmgr_line_decimal(&named, GetCurrentProcessId(), 1u);
    mmgr_line_ch(&named, '-');
    for (unsigned int at = 0u; at < CLI_TEMPORARY_NAME_BYTES; at += 1u)
    {
        mmgr_line_hex(&named, random[at], 2u);
    }
    mmgr_line_put(&named, ".tmp");
    if (mmgr_line_finish(&named) == 0ull)
    {
        return 0;
    }
    // shared so that the system deletes it as it is let go, or as the process ends however it ends
    const HANDLE made = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_NEW,
                                    FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    // a handle is a pointer's width, which an unsigned long long stores
    file->handle = (made != INVALID_HANDLE_VALUE) ? (unsigned long long)(uintptr_t)made : 0ull;
    return made != INVALID_HANDLE_VALUE;
}

int cli_temporary_file_written(const CliTemporaryFile *file, unsigned long long at, const void *bytes,
                               unsigned long long size)
{
    const HANDLE handle = (HANDLE)(uintptr_t)file->handle;
    const unsigned char *const from = (const unsigned char *)bytes;
    unsigned long long done = 0ull;
    int written = 1;
    while (written && (done < size))
    {
        OVERLAPPED place;
        memor.set(&(MemoriaCfg){.dst = &place, .val = 0, .bytes = sizeof(place)});
        // the offset's low and high words, each what a DWORD stores
        place.Offset = (DWORD)((at + done) & 0xffffffffull);
        place.OffsetHigh = (DWORD)((at + done) >> 32u);
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD chunk = (DWORD)(((size - done) < (1ull << 30u)) ? (size - done) : (1ull << 30u));
        DWORD wrote = 0u;
        written = WriteFile(handle, from + done, chunk, &wrote, &place) && (wrote == chunk);
        done += wrote;
    }
    return written;
}

int cli_temporary_file_read(const CliTemporaryFile *file, unsigned long long at, void *bytes, unsigned long long size)
{
    unsigned long long got = 0ull;
    file_read_from((HANDLE)(uintptr_t)file->handle, at, (unsigned char *)bytes, size, &got);
    return got == size;
}

void cli_temporary_file_let_go(CliTemporaryFile *file)
{
    if (file->handle != 0ull)
    {
        CloseHandle((HANDLE)(uintptr_t)file->handle);
    }
    file->handle = 0ull;
}
#endif
