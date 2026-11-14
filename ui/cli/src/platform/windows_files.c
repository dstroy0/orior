// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_files.c: the command line's file sizes, moves and writes, and the wall clock on Windows
#include "../command_line/cli_internal.h"
#include "../text/text.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

unsigned int cli_system_error(void)
{
    // the system's error codes are below what an unsigned int stores
    return (unsigned int)GetLastError();
}

void cli_system_reason(const CliErrorCtx *args)
{
    char reason[512];
    DWORD size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, args->error, 0u,
                                reason, sizeof(reason), NULL);
    size = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = reason, .at = size,
                      .set = "\r\n ");
    mmgr_line_put(args->err_msg, ": ");
    mmgr_line_put_n(args->err_msg, reason, size);
    mmgr_line_put(args->err_msg, (size > 0u) ? " (os error " : "(os error ");
    mmgr_line_decimal(args->err_msg, args->error, 1u);
    mmgr_line_ch(args->err_msg, ')');
}

int cli_file_size(const CliPathSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    WIN32_FILE_ATTRIBUTE_DATA found;
    if (!GetFileAttributesExA(args->path, GetFileExInfoStandard, &found))
    {
        return 0;
    }
    *size = ((unsigned long long)found.nFileSizeHigh << 32u) | found.nFileSizeLow;
    return 1;
}

typedef struct
{
#if (defined(_WIN32))
    HANDLE file;
    unsigned long long from;
    unsigned char *bytes;
    unsigned long long count;
    unsigned long long *got;
    const char *path;
#endif
} WindowsFilesCtx;
// `count` bytes of the open `file` from `from` read into `bytes`, their count into `got`, fewer where the file ends
EMBED_INLINE void file_read_from(const WindowsFilesCtx *args)
{
    unsigned char *bytes = args->bytes;
    unsigned long long *got = args->got;
    *got = 0ull;
    int read_state = 1;
    while (read_state && (*got < args->count))
    {
        OVERLAPPED at;
        memor.set(&(MemoriaCfg){.dst = &at, .val = 0, .bytes = sizeof(at)});
        // the offset's low and high words, each what a DWORD stores
        at.Offset = (DWORD)((args->from + *got) & 0xffffffffull);
        at.OffsetHigh = (DWORD)((args->from + *got) >> 32u);
        // one read takes at most what a DWORD counts
        const DWORD chunk = (DWORD)(((args->count - *got) < 0x40000000ull) ? (args->count - *got) : 0x40000000ull);
        DWORD read = 0u;
        const BOOL done = ReadFile(args->file, bytes + *got, chunk, &read, &at);
        // a read past the file's end is the file's end
        read_state = (done || (GetLastError() == ERROR_HANDLE_EOF)) && (read != 0u);
        *got += read;
    }
}

// the file at `path` opened to read, shared with every other opener, read once through
EMBED_INLINE HANDLE file_open(const WindowsFilesCtx *args)
{
    return CreateFileA(args->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
}

int cli_file_read_at(const CliPathFromCtx *args)
{
    unsigned long long *got = args->got;
    const HANDLE file = EMBED_CALL(file_open, WindowsFilesCtx, .path = args->path);
    *got = 0ull;
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    EMBED_CALL(file_read_from, WindowsFilesCtx, .file = file, .from = args->from, .bytes = args->bytes_bytes,
               .count = args->count_u64, .got = got);
    CloseHandle(file);
    return 1;
}

int cli_file_stamp(const CliPathSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    unsigned long long *changed = args->changed;
    WIN32_FILE_ATTRIBUTE_DATA found;
    if (!GetFileAttributesExA(args->path, GetFileExInfoStandard, &found))
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

int cli_move(const CliErrMsgFromCtx *args)
{
    wchar_t wide_from[CLI_PATH_MAX_LEN];
    wchar_t wide_to[CLI_PATH_MAX_LEN];
    if ((MultiByteToWideChar(CP_UTF8, 0, args->from_text, -1, wide_from, CLI_PATH_MAX_LEN) == 0) ||
        (MultiByteToWideChar(CP_UTF8, 0, args->to, -1, wide_to, CLI_PATH_MAX_LEN) == 0))
    {
        return CLI_MOVE_FAILED;
    }
    if (MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING))
    {
        return CLI_MOVE_DONE;
    }
    return (GetLastError() == ERROR_NOT_SAME_DEVICE) ? CLI_MOVE_ACROSS : CLI_MOVE_FAILED;
}

int cli_file_append(const CliPathBytesCtx *args)
{
    wchar_t pixel_width[CLI_PATH_MAX_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, args->path, -1, pixel_width, CLI_PATH_MAX_LEN) == 0)
    {
        return 0;
    }
    const HANDLE file = CreateFileW(pixel_width, args->fresh ? GENERIC_WRITE : FILE_APPEND_DATA, 0u, NULL,
                                    args->fresh ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    const unsigned char *at = (const unsigned char *)args->bytes;
    unsigned long long left = args->size;
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

int cli_resource_add(const CliPathErrMsgCtx *args)
{
    wchar_t pixel_width[CLI_PATH_MAX_LEN];
    wchar_t named[CLI_NAME_MAX_LEN];
    if ((MultiByteToWideChar(CP_UTF8, 0, args->path, -1, pixel_width, CLI_PATH_MAX_LEN) == 0) ||
        (MultiByteToWideChar(CP_UTF8, 0, args->name, -1, named, CLI_NAME_MAX_LEN) == 0))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": the path or the name is too long");
        return 0;
    }
    if (args->size > 0xffffffffull)
    {
        mmgr_line_put(args->err_msg, "the blob is past 4 GB");
        return 0;
    }
    const HANDLE update = BeginUpdateResourceW(pixel_width, FALSE);
    if (update == NULL)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": Windows reported ");
        mmgr_line_decimal(args->err_msg, GetLastError(), 1u);
        return 0;
    }
    // the blob's size is below 4 GB, as checked above, which a DWORD counts; the RCDATA type's number stands for a
    // name, as Windows takes it
    if (!UpdateResourceW(update, MAKEINTRESOURCEW(10u), named, 0u, (LPVOID)(embed_word)args->data, (DWORD)args->size))
    {
        const DWORD error = GetLastError();
        EndUpdateResourceW(update, TRUE);
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": Windows reported ");
        mmgr_line_decimal(args->err_msg, error, 1u);
        return 0;
    }
    if (!EndUpdateResourceW(update, FALSE))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": Windows reported ");
        mmgr_line_decimal(args->err_msg, GetLastError(), 1u);
        return 0;
    }
    return 1;
}

// the bytes of randomness a temporary file's name carries
#define CLI_TEMPORARY_NAME_BYTES 8u

int cli_temporary_file_init(const CliFileTemporaryFileCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = path);
    unsigned char random[CLI_TEMPORARY_NAME_BYTES];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = random, .size = sizeof(random)))
    {
        return 0;
    }
    VerbaLinea named = {path, sizeof(path), cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES})};
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
    const HANDLE made =
        CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    // a handle is a pointer's width, which an unsigned long long stores
    args->file_temporary_file_pointer->handle =
        (made != INVALID_HANDLE_VALUE) ? (unsigned long long)(embed_word)made : 0ull;
    return made != INVALID_HANDLE_VALUE;
}

int cli_temporary_file_write(const CliBytesCtx *args)
{
    const void *bytes = args->bytes;
    const HANDLE handle = (HANDLE)(embed_word)args->file_temporary_file_pointer_other->handle;
    const unsigned char *const from = (const unsigned char *)bytes;
    unsigned long long done = 0ull;
    int written = 1;
    while (written && (done < args->size))
    {
        OVERLAPPED place;
        memor.set(&(MemoriaCfg){.dst = &place, .val = 0, .bytes = sizeof(place)});
        // the offset's low and high words, each what a DWORD stores
        place.Offset = (DWORD)((args->at + done) & 0xffffffffull);
        place.OffsetHigh = (DWORD)((args->at + done) >> 32u);
        // a piece is at most a gibibyte, which a DWORD counts
        const DWORD chunk = (DWORD)(((args->size - done) < (1ull << 30u)) ? (args->size - done) : (1ull << 30u));
        DWORD wrote = 0u;
        written = WriteFile(handle, from + done, chunk, &wrote, &place) && (wrote == chunk);
        done += wrote;
    }
    return written;
}

int cli_temporary_file_read(const CliSizeCtx *args)
{
    unsigned long long got = 0ull;
    EMBED_CALL(file_read_from, WindowsFilesCtx,
               .file = (HANDLE)(embed_word)args->file_temporary_file_pointer_other->handle, .from = args->at,
               .bytes = (unsigned char *)args->bytes_out, .count = args->size, .got = &got);
    return got == args->size;
}

void cli_temporary_file_let_go(const CliFileTemporaryFileCtx *args)
{
    if (args->file_temporary_file_pointer->handle != 0ull)
    {
        CloseHandle((HANDLE)(embed_word)args->file_temporary_file_pointer->handle);
    }
    args->file_temporary_file_pointer->handle = 0ull;
}
#endif
