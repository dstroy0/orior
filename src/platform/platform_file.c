// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_file.c: a file as a stream, over the calls of the system that platform_windows.c and platform_posix.c give.
// A file written goes through a line of MMgr over its buffer, and the line passes the buffer to the system as it fills.
// A file read holds the bytes of its last read in the same buffer. Each call answers as the call of the C library that
// its comment in platform.h names
#include "platform_internal.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "memoria_operor/memoria_linea.h"

// a text file writes a line's end as two bytes on Windows, and on no other system, as the C library does
#if defined(_WIN32)
#define PLATFORM_FILE_TEXT_ENDS 1
#else
#define PLATFORM_FILE_TEXT_ENDS 0
#endif

// the bytes of the line passed to the system, each line's end of a text file as two bytes: 1 where every byte went
static int platform_file_passed(void *context, const char *bytes, size_t size)
{
    PlatformFile *const file = (PlatformFile *)context;
    size_t from = 0u;
    int went = 1;
    for (size_t at = 0u; went && (file->text != 0) && (at < size); at += 1u)
    {
        if (bytes[at] == '\n')
        {
            const unsigned long long piece = (unsigned long long)(at - from);
            went = (platform_stream_put(file->handle, bytes + from, piece) == piece) &&
                   (platform_stream_put(file->handle, "\r\n", 2ULL) == 2ULL);
            from = at + 1u;
        }
    }
    const unsigned long long rest = (unsigned long long)(size - from);
    went = went && (platform_stream_put(file->handle, bytes + from, rest) == rest);
    file->failed = file->failed || !went;
    return went;
}

PlatformFile *platform_file_open(const char *path, const char *mode)
{
    const int kind = (mode[0] == 'r') ? 0 : ((mode[0] == 'w') ? 1 : ((mode[0] == 'a') ? 2 : -1));
    const int binary = (mode[0] != '\0') && (mode[1] == 'b');
    const int whole = (mode[0] != '\0') && ((binary != 0) ? (mode[2] == '\0') : (mode[1] == '\0'));
    if ((kind < 0) || !whole)
    {
        return NULL;
    }
    const unsigned long long handle = platform_stream_opened(path, kind);
    if (handle == 0ULL)
    {
        return NULL;
    }
    // a file written after its end tells its end before the first write, as the C library places it
    if ((kind == 2) && (platform_stream_placed(handle, 0LL, 2) < 0))
    {
        (void)platform_stream_shut(handle);
        return NULL;
    }
    PlatformFile *const file =
        (PlatformFile *)platform_cell_taken(sizeof(PlatformFile), (unsigned int)platform_place_g_program_owner, 0);
    if (file == NULL)
    {
        (void)platform_stream_shut(handle);
        return NULL;
    }
    file->handle = handle;
    file->writing = (kind != 0) ? 1 : 0;
    file->text = ((binary == 0) && (PLATFORM_FILE_TEXT_ENDS != 0)) ? 1 : 0;
    file->failed = 0;
    file->ended = 0;
    file->held = 0u;
    file->taken = 0u;
    // a file read holds a line of no room. A write to it goes nowhere
    file->line = (file->writing != 0)
                     ? (VerbaLinea){file->buffer, sizeof(file->buffer), 0u, platform_file_passed, file, 0u}
                     : mmgr_line_over(NULL, 0u);
    return file;
}

int platform_file_flush(PlatformFile *file)
{
    if ((file->writing != 0) && (file->line.at != 0u) && !mmgr_line_flush(&file->line))
    {
        file->failed = 1;
    }
    return (file->failed != 0) ? PLATFORM_FILE_END : 0;
}

int platform_file_close(PlatformFile *file)
{
    if (file == NULL)
    {
        return PLATFORM_FILE_END;
    }
    int answer = (file->writing != 0) ? platform_file_flush(file) : 0;
    if (!platform_stream_shut(file->handle))
    {
        answer = PLATFORM_FILE_END;
    }
    platform_cell_released(file, (unsigned int)platform_place_g_program_owner);
    return answer;
}

VerbaLinea *platform_file_line(PlatformFile *file)
{
    return &file->line;
}

size_t platform_file_write_items(const void *bytes, size_t size, size_t count, PlatformFile *file)
{
    if ((size == 0u) || (count == 0u))
    {
        return 0u;
    }
    const size_t total = size * count;
    // Explicit cast reads the source as its bytes
    const size_t taken = mmgr_line_put_n(&file->line, (const char *)bytes, total);
    if (taken < total)
    {
        file->failed = 1;
    }
    return taken / size;
}

int platform_file_text_put(const char *text, PlatformFile *file)
{
    const size_t length = mmgr_text_length(text, PLATFORM_TEXT_TO_TERMINATOR);
    return (platform_file_write_items(text, 1u, length, file) == length) ? 0 : PLATFORM_FILE_END;
}

int platform_file_char_put(int character, PlatformFile *file)
{
    // Explicit casts take the low byte of the character, as fputc writes it, and answer it as unsigned
    const char byte = (char)(unsigned char)character;
    return (platform_file_write_items(&byte, 1u, 1u, file) == 1u) ? (int)(unsigned char)character : PLATFORM_FILE_END;
}

// the buffer of a file read holds a byte that the program has not taken: 1, or 0 at the end of the file
static int platform_file_refilled(PlatformFile *file)
{
    if (file->taken < file->held)
    {
        return 1;
    }
    if ((file->ended != 0) || (file->failed != 0) || (file->writing != 0))
    {
        return 0;
    }
    // Explicit cast takes a count of at max the buffer's bytes
    const size_t got = (size_t)platform_stream_got(file->handle, file->buffer, sizeof(file->buffer));
    file->held = got;
    file->taken = 0u;
    file->ended = (got == 0u) ? 1 : 0;
    return got != 0u;
}

// the next byte of a file read, a line's end of a text file as one byte, or PLATFORM_FILE_END at the end of the file
static int platform_file_byte(PlatformFile *file)
{
    if (!platform_file_refilled(file))
    {
        return PLATFORM_FILE_END;
    }
    // Explicit cast reads the byte as unsigned, as fgetc answers it
    const int byte = (int)(unsigned char)file->buffer[file->taken];
    file->taken += 1u;
    if ((file->text != 0) && (byte == '\r') && platform_file_refilled(file) && (file->buffer[file->taken] == '\n'))
    {
        file->taken += 1u;
        return '\n';
    }
    return byte;
}

size_t platform_file_read_items(void *out, size_t size, size_t count, PlatformFile *file)
{
    if ((size == 0u) || (count == 0u))
    {
        return 0u;
    }
    const size_t total = size * count;
    // Explicit cast writes the destination as its bytes
    char *const into = (char *)out;
    size_t got = 0u;
    while ((got < total) && (file->text == 0) && platform_file_refilled(file))
    {
        const size_t left = file->held - file->taken;
        const size_t piece = ((total - got) < left) ? (total - got) : left;
        (void)mmgr_bytes_copy(into + got, file->buffer + file->taken, piece);
        file->taken += piece;
        got += piece;
    }
    while ((got < total) && (file->text != 0))
    {
        const int byte = platform_file_byte(file);
        if (byte == PLATFORM_FILE_END)
        {
            break;
        }
        // Explicit cast stores the byte that platform_file_byte read as unsigned
        into[got] = (char)byte;
        got += 1u;
    }
    return got / size;
}

char *platform_file_text_read(char *out, int capacity, PlatformFile *file)
{
    if (capacity <= 0)
    {
        return NULL;
    }
    int byte = 0;
    int at = 0;
    while (at < (capacity - 1))
    {
        byte = platform_file_byte(file);
        if (byte == PLATFORM_FILE_END)
        {
            break;
        }
        // Explicit cast stores the byte that platform_file_byte read as unsigned
        out[at] = (char)byte;
        at += 1;
        if (byte == '\n')
        {
            break;
        }
    }
    out[at] = '\0';
    return ((at == 0) && (byte == PLATFORM_FILE_END)) ? NULL : out;
}

int platform_file_seek(PlatformFile *file, long long offset, int whence)
{
    long long distance = offset;
    if (file->writing != 0)
    {
        (void)platform_file_flush(file);
    }
    else
    {
        // the buffer holds bytes that the program has not taken. They lie after the place that the program sees
        // Explicit cast takes a count of at max the buffer's bytes
        distance -= (whence == 1) ? (long long)(file->held - file->taken) : 0LL;
        file->held = 0u;
        file->taken = 0u;
    }
    file->ended = 0;
    return (platform_stream_placed(file->handle, distance, whence) < 0) ? -1 : 0;
}

long long platform_file_tell(PlatformFile *file)
{
    const long long place = platform_stream_placed(file->handle, 0LL, 1);
    if (place < 0)
    {
        return -1LL;
    }
    if (file->writing == 0)
    {
        // Explicit cast takes a count of at max the buffer's bytes
        return place - (long long)(file->held - file->taken);
    }
    // what the line holds and has not passed to the system, each line's end of a text file as two bytes
    long long pending = 0LL;
    for (size_t at = 0u; at < file->line.at; at += 1u)
    {
        pending += ((file->text != 0) && (file->buffer[at] == '\n')) ? 2LL : 1LL;
    }
    return place + pending;
}

int platform_file_failed(const PlatformFile *file)
{
    return file->failed;
}

int platform_file_ended(const PlatformFile *file)
{
    return file->ended;
}
