// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows.c: the run on Windows: the shared memory a named section reserved whole and given memory as it is used, the
// .qry a file handle, the one clock the performance counter, and the pause a sleep
#include "../qry_writer_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <stddef.h>
#include <string.h>

// the most wide letters of a path, the longest a run's file is named under the top with a stream's name after it
#define QRY_WIDE_LONGEST (QRY_WRITER_TOP_LONGEST + QRY_NAME_LONGEST + 2u)

// a path of UTF-8 as Windows' wide letters into `wide`, which holds QRY_WIDE_LONGEST: 1, or 0 where it is no
// UTF-8 or does not fit
static int qry_wide(const char *path, wchar_t *wide)
{
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, (int)QRY_WIDE_LONGEST) != 0;
}

// the section's name, `Local\qry-` and the hash of the .qry's path in hex
static void qry_shared_named(const char *path, char *name)
{
    static const char s_prefix[] = "Local\\qry-";
    static const char s_digits[] = "0123456789abcdef";
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        // Windows reads a path the same either way its separators are written and whatever its
        // letters' case; every written form of one path therefore names one mapping
        const unsigned char folded = (*at == '\\')                    ? (unsigned char)'/'
                                     : ((*at >= 'A') && (*at <= 'Z')) ? (unsigned char)(*at + ('a' - 'A'))
                                                                      : (unsigned char)*at;
        hash = (hash ^ folded) * 0x100000001b3ull;
    }
    memcpy(name, s_prefix, sizeof(s_prefix) - 1u);
    for (unsigned int digit = 0u; digit < 16u; digit += 1u)
    {
        name[sizeof(s_prefix) - 1u + digit] = s_digits[(hash >> (60u - (4u * digit))) & 0xfull];
    }
    name[sizeof(s_prefix) - 1u + 16u] = '\0';
}

static HANDLE s_section = NULL;

QryShared *qry_writer_shared_made(const char *path, unsigned long long bytes)
{
    char name[32];
    qry_shared_named(path, name);
    // the region made whole, zeros, its bytes the system's to hold from here
    s_section = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)(bytes >> 32u),
                                   (DWORD)(bytes & 0xffffffffull), name);
    if ((s_section == NULL) || (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        return NULL;
    }
    return (QryShared *)MapViewOfFile(s_section, FILE_MAP_ALL_ACCESS, 0u, 0u, 0u);
}

void qry_writer_shared_released(QryShared *shared)
{
    UnmapViewOfFile(shared);
    CloseHandle(s_section);
}

long long qry_writer_file_made(const char *path)
{
    wchar_t wide[QRY_WIDE_LONGEST];
    const HANDLE file = qry_wide(path, wide) ? CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                                                           FILE_ATTRIBUTE_NORMAL, NULL)
                                             : INVALID_HANDLE_VALUE;
    return (file == INVALID_HANDLE_VALUE) ? -1 : (long long)(intptr_t)file;
}

int qry_writer_file_put(long long file, const unsigned char *bytes, unsigned long long size)
{
    unsigned long long done = 0ull;
    while (done < size)
    {
        // a write is at most QRY_WRITER_PIECE_BYTES, at most a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((size - done) < QRY_WRITER_PIECE_BYTES) ? (size - done) : QRY_WRITER_PIECE_BYTES);
        DWORD wrote = 0u;
        if (!WriteFile((HANDLE)(intptr_t)file, bytes + done, piece, &wrote, NULL) || (wrote == 0u))
        {
            return 0;
        }
        done += wrote;
    }
    return 1;
}

int qry_writer_file_durable(long long file)
{
    return FlushFileBuffers((HANDLE)(intptr_t)file) != 0;
}

int qry_writer_file_closed(long long file)
{
    const int flushed = qry_writer_file_durable(file);
    return (CloseHandle((HANDLE)(intptr_t)file) != 0) && flushed;
}

int qry_writer_file_removed(const char *path)
{
    wchar_t wide[QRY_WIDE_LONGEST];
    return qry_wide(path, wide) && (DeleteFileW(wide) != 0);
}

int qry_writer_file_placed(const char *beside, const char *placed)
{
    wchar_t wide_beside[QRY_WIDE_LONGEST];
    wchar_t wide_placed[QRY_WIDE_LONGEST];
    const int named = qry_wide(beside, wide_beside) && qry_wide(placed, wide_placed);
    if (named && (MoveFileExW(wide_beside, wide_placed, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0))
    {
        return 1;
    }
    if (named)
    {
        DeleteFileW(wide_beside);
    }
    return 0;
}

int qry_writer_stream_put(const char *qry, const char *name, unsigned int name_length, const unsigned char *bytes,
                          unsigned long long size)
{
    char path[QRY_WRITER_TOP_LONGEST + QRY_NAME_LONGEST + 2u];
    const size_t length = strlen(qry);
    if ((length >= QRY_WRITER_TOP_LONGEST) || (name_length > QRY_NAME_LONGEST))
    {
        return 0;
    }
    memcpy(path, qry, length);
    path[length] = ':';
    memcpy(path + length + 1u, name, name_length);
    path[length + 1u + name_length] = '\0';
    wchar_t wide[QRY_WIDE_LONGEST];
    const HANDLE file = qry_wide(path, wide)
                            ? CreateFileW(wide, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                            : INVALID_HANDLE_VALUE;
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    unsigned long long done = 0ull;
    int whole = 1;
    while (whole && (done < size))
    {
        // a write is at most QRY_WRITER_PIECE_BYTES, at most a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((size - done) < QRY_WRITER_PIECE_BYTES) ? (size - done) : QRY_WRITER_PIECE_BYTES);
        DWORD put = 0u;
        whole = WriteFile(file, bytes + done, piece, &put, NULL) && (put == piece);
        done += put;
    }
    whole = FlushFileBuffers(file) && whole;
    CloseHandle(file);
    return whole;
}

unsigned long long qry_writer_input_read(unsigned char *bytes, unsigned long long room)
{
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    unsigned long long size = 0ull;
    for (;;)
    {
        // a read is at most a gibibyte, which a DWORD counts
        const unsigned long long left = room - size;
        const DWORD piece = (DWORD)((left < (1ull << 30u)) ? left : (1ull << 30u));
        DWORD got = 0u;
        if (!ReadFile(input, bytes + size, piece, &got, NULL))
        {
            // a pipe whose writer has closed it ends a read so
            return (GetLastError() == ERROR_BROKEN_PIPE) ? size : QRY_MEMBER_UNREAD;
        }
        if (got == 0u)
        {
            return size;
        }
        size += got;
        if (size == room)
        {
            return QRY_MEMBER_UNREAD;
        }
    }
}

void qry_writer_paused(void)
{
    Sleep(1u);
}

void qry_writer_raised(void)
{
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
}

unsigned long long qry_member_file_read(const char *path, unsigned char *bytes, unsigned long long room)
{
    wchar_t wide[QRY_WIDE_LONGEST];
    const HANDLE file = qry_wide(path, wide) ? CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
                                             : INVALID_HANDLE_VALUE;
    LARGE_INTEGER length;
    if ((file == INVALID_HANDLE_VALUE) || !GetFileSizeEx(file, &length) || ((unsigned long long)length.QuadPart > room))
    {
        if (file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file);
        }
        return QRY_MEMBER_UNREAD;
    }
    // a file's length is not negative and fits `room`
    const unsigned long long size = (unsigned long long)length.QuadPart;
    unsigned long long done = 0ull;
    while (done < size)
    {
        // a read is at most a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((size - done) < (1ull << 30u)) ? (size - done) : (1ull << 30u));
        DWORD got = 0u;
        if (!ReadFile(file, bytes + done, piece, &got, NULL) || (got == 0u))
        {
            CloseHandle(file);
            return QRY_MEMBER_UNREAD;
        }
        done += got;
    }
    CloseHandle(file);
    return size;
}

QryShared *qry_member_shared(const char *path)
{
    char name[32];
    qry_shared_named(path, name);
    const HANDLE section = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (section == NULL)
    {
        return NULL;
    }
    // the view holds the section for as long as it is mapped
    QryShared *const shared = (QryShared *)MapViewOfFile(section, FILE_MAP_ALL_ACCESS, 0u, 0u, 0u);
    CloseHandle(section);
    // a region whose whole the writer has not written yet is no run yet
    if ((shared != NULL) && (atomic_load(&shared->whole) == 0ull))
    {
        UnmapViewOfFile(shared);
        return NULL;
    }
    return shared;
}

void qry_member_unshared(QryShared *run)
{
    UnmapViewOfFile(run);
}

unsigned long long qry_member_clock(void)
{
    LARGE_INTEGER count;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    // the counter and its frequency are not negative; the seconds and the rest are turned apart so neither overflows
    const unsigned long long ticks = (unsigned long long)count.QuadPart;
    const unsigned long long rate = (unsigned long long)frequency.QuadPart;
    return ((ticks / rate) * 1000000000ull) + (((ticks % rate) * 1000000000ull) / rate);
}

unsigned long long qry_member_process(void)
{
    return (unsigned long long)GetCurrentProcessId();
}

const unsigned char *qry_member_file_mapped(const char *path, unsigned long long *size)
{
    static const unsigned char s_none[1] = {0u};
    wchar_t wide[QRY_WIDE_LONGEST];
    const HANDLE file = qry_wide(path, wide) ? CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
                                             : INVALID_HANDLE_VALUE;
    LARGE_INTEGER length;
    if ((file == INVALID_HANDLE_VALUE) || !GetFileSizeEx(file, &length))
    {
        if (file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file);
        }
        return NULL;
    }
    // a file's length is not negative
    *size = (unsigned long long)length.QuadPart;
    if (*size == 0ull)
    {
        CloseHandle(file);
        return s_none;
    }
    const HANDLE section = CreateFileMappingA(file, NULL, PAGE_READONLY, 0u, 0u, NULL);
    CloseHandle(file);
    if (section == NULL)
    {
        return NULL;
    }
    const unsigned char *const view = (const unsigned char *)MapViewOfFile(section, FILE_MAP_READ, 0u, 0u, 0u);
    CloseHandle(section);
    return view;
}

void qry_member_file_unmapped(const unsigned char *bytes, unsigned long long size)
{
    if ((bytes != NULL) && (size != 0ull))
    {
        UnmapViewOfFile(bytes);
    }
}

int qry_member_named(char *path, size_t room)
{
    // the room of a path is far below what a DWORD counts
    const DWORD length = GetEnvironmentVariableA(QRY_ENVIRONMENT, path, (DWORD)room);
    return (length != 0u) && ((size_t)length < room);
}

int qry_member_program(char *path, size_t room)
{
    wchar_t wide[QRY_WIDE_LONGEST];
    const DWORD length = GetModuleFileNameW(NULL, wide, QRY_WIDE_LONGEST);
    // the room of a path is far below what an int counts
    return (length != 0u) && (length < QRY_WIDE_LONGEST) &&
           (WideCharToMultiByte(CP_UTF8, 0, wide, -1, path, (int)room, NULL, NULL) != 0);
}
#endif
