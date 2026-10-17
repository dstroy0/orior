// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.c: text built in memory and handed, a file the run holds read, and a blob handed, streamed or placed in a member's ring
#include "qry_buffer_internal.h"

int qry_line_next(const unsigned char *bytes, unsigned long long size, unsigned long long *at, char *line, size_t room)
{
    if ((bytes == NULL) || (*at >= size) || (room < 2u))
    {
        return 0;
    }
    size_t length = 0u;
    while ((*at < size) && (length < (room - 1u)))
    {
        const char letter = (char)bytes[*at];
        line[length] = letter;
        length += 1u;
        *at += 1ull;
        if (letter == '\n')
        {
            break;
        }
    }
    line[length] = '\0';
    return 1;
}

// room for `more` bytes past what `text` holds, and one past them for a printed text's end: 1, or 0 with the text
// marked failed where the system gives no memory
static int qry_text_roomed(QryText *text, unsigned long long more)
{
    if (text->failed)
    {
        return 0;
    }
    const unsigned long long needed = text->size + more + 1ull;
    if (needed > text->room)
    {
        unsigned long long grown = (text->room != 0ull) ? text->room : 4096ull;
        while (grown < needed)
        {
            grown *= 2ull;
        }
        // a text is at most what the run's buffer takes, which a size_t counts
        char *const larger = (char *)realloc(text->bytes, (size_t)grown);
        if (larger == NULL)
        {
            text->failed = 1;
            return 0;
        }
        text->bytes = larger;
        text->room = grown;
    }
    return 1;
}

void qry_text_printed(QryText *text, const char *format, ...)
{
    va_list values;
    va_start(values, format);
    va_list measured;
    va_copy(measured, values);
    const int length = vsnprintf(NULL, 0u, format, measured);
    va_end(measured);
    if ((length > 0) && qry_text_roomed(text, (unsigned long long)length))
    {
        // the room holds the printed text and its end
        vsnprintf(text->bytes + text->size, (size_t)length + 1u, format, values);
        text->size += (unsigned long long)length;
    }
    text->failed |= (length < 0) ? 1 : 0;
    va_end(values);
}

void qry_text_added(QryText *text, const void *bytes, unsigned long long size)
{
    if ((size != 0ull) && qry_text_roomed(text, size))
    {
        // the room holds the bytes, which a size_t counts
        memcpy(text->bytes + text->size, bytes, (size_t)size);
        text->size += size;
    }
}

unsigned long long qry_text_filed_by(QryBuffer *buffer, const char *path, QryText *text, QryWho who)
{
    const unsigned long long sequence =
        text->failed ? 0ull : qry_filed_by(buffer, path, (text->bytes != NULL) ? text->bytes : "", text->size, who);
    qry_text_freed(text);
    return sequence;
}

void qry_text_freed(QryText *text)
{
    free(text->bytes);
    memset(text, 0, sizeof(*text));
}

int qry_file_held(QryBuffer *buffer, const char *path)
{
    char name[QRY_NAME_LONGEST + 1u];
    if ((buffer == NULL) || !qry_file_named(path, name, sizeof(name)))
    {
        return 0;
    }
    qry_own_moved(buffer);
    const size_t length = strlen(name);
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    QryBlob blob;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        QryFrameHead opened;
        memcpy(&opened, buffer->data + walk, sizeof(opened));
        if (((opened.marks & QRY_MARK_FILED) != 0u) && (blob.name_length == length) &&
            (memcmp(buffer->data + blob.name_at, name, length) == 0))
        {
            return 1;
        }
    }
    return 0;
}

int qry_file_read(QryBuffer *buffer, const char *path, unsigned char **bytes, unsigned long long *size)
{
    *bytes = NULL;
    *size = 0ull;
    char name[QRY_NAME_LONGEST + 1u];
    const unsigned char *held = NULL;
    unsigned long long held_size = 0ull;
    const int named = (buffer != NULL) && qry_file_named(path, name, sizeof(name));
    if (named && qry_latest(buffer, name, &held, &held_size, NULL))
    {
        // a blob is at most the buffer's bytes, which a size_t counts
        *bytes = (unsigned char *)malloc((size_t)held_size + 1u);
        if (*bytes == NULL)
        {
            return 0;
        }
        memcpy(*bytes, held, (size_t)held_size);
        *size = held_size;
        return 1;
    }
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    size_t room = 65536u;
    unsigned char *read = (unsigned char *)malloc(room + 1u);
    size_t length = 0u;
    size_t got = 0u;
    while ((read != NULL) && ((got = fread(read + length, 1u, room - length, file)) != 0u))
    {
        length += got;
        if (length == room)
        {
            unsigned char *const larger = (unsigned char *)realloc(read, (room * 2u) + 1u);
            if (larger == NULL)
            {
                free(read);
                read = NULL;
                break;
            }
            read = larger;
            room *= 2u;
        }
    }
    fclose(file);
    *bytes = read;
    *size = length;
    // a file the run reads from the disk is taken into the run as it read it: every read of it after, by any process
    // of the run, is from memory, and the file does not change under the run
    if ((read != NULL) && named)
    {
        qry_hand(buffer, name, read, (unsigned long long)length, QRY_ORDINARY);
    }
    return read != NULL;
}

unsigned long long qry_hand(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                            unsigned int priority)
{
    return qry_handed(buffer, name, bytes, size, priority, 0u);
}

unsigned long long qry_streamed(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                                char *path, size_t room)
{
    if ((buffer == NULL) || (strpbrk(name, "/\\:") != NULL))
    {
        return 0ull;
    }
    const int named = snprintf(path, room, "%s:%s", buffer->path, name);
    // a path that does not fit its room names no stream
    if ((named < 0) || ((size_t)named >= room))
    {
        return 0ull;
    }
#if defined(_WIN32)
    const unsigned long long sequence = qry_handed(buffer, name, bytes, size, QRY_CRITICAL, QRY_MARK_STREAMED);
    if (sequence == 0ull)
    {
        return 0ull;
    }
    // the stream read back and held to the bytes handed: a stream the writer did not write, or one an earlier hand of
    // the name left, holds other bytes
    HANDLE stream = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (stream == INVALID_HANDLE_VALUE)
    {
        return 0ull;
    }
    const unsigned char *const expected = (const unsigned char *)bytes;
    unsigned char piece[65536];
    unsigned long long read = 0ull;
    int held = 1;
    DWORD got = 0u;
    while (held && ReadFile(stream, piece, sizeof(piece), &got, NULL) && (got != 0u))
    {
        held = ((read + got) <= size) && (memcmp(piece, expected + read, got) == 0);
        read += got;
    }
    CloseHandle(stream);
    return (held && (read == size)) ? sequence : 0ull;
#else
    (void)bytes;
    (void)size;
    return 0ull;
#endif
}

// The blob the writer placed said where the writer says what it places, its own output, one line, `qry_blob
// <sequence> <nanoseconds> <priority> <member> <size> <name>`, the member QRY_MEMBERS where the writer's own process
// handed it: whatever started the writer hears every blob of the run as the writer places it, in the writer's order.
// Nothing where `buffer` is not the writer's as it writes the run
void qry_blob_said(QryBuffer *buffer, unsigned long long sequence, unsigned long long nanoseconds,
                   unsigned int priority, unsigned int member, const char *name, size_t length,
                   unsigned long long size)
{
    if (buffer->saying == NULL)
    {
        return;
    }
    fprintf(buffer->saying, "qry_blob %llu %llu %u %u %llu %.*s\n", sequence, nanoseconds, priority, member, size,
            (int)length, name);
}

// One blob put in at the buffer's tail under the lock, a dump said before it where one waits and it does not fit
// (qry_dumped_said), and counted as dumped where it is ordinary and does not fit: its sequence, with `placed` 1 where
// it went in
unsigned long long qry_placed(QryBuffer *buffer, const char *name, size_t length, const void *bytes,
                                     unsigned long long size, unsigned long long crc, unsigned int priority,
                                     unsigned int marks, unsigned long long handed_at, int *placed)
{
    QryHead *const head = buffer->head;
    const unsigned long long room = (priority < QRY_ORDINARY) ? head->capacity : (head->capacity - QRY_CRITICAL_ROOM);
    // a dump said once, before the first blob that goes in after it: a blob that would be dumped too says nothing, and
    // a run of dumps is one blob QRY_DUMPED. The frame is taken at its most
    const unsigned long long framed_most = size + QRY_FRAME_MOST;
    if (atomic_load(&head->tail) <= ((room > framed_most) ? (room - framed_most) : 0ull))
    {
        qry_dumped_said(buffer);
    }
    const unsigned long long sequence =
        qry_appended(buffer, name, length, bytes, size, crc, priority, marks, room, handed_at, placed);
    if (!*placed && (priority == QRY_ORDINARY))
    {
        head->dumped_first = (head->dumped_blobs == 0ull) ? sequence : head->dumped_first;
        head->dumped_last = sequence;
        head->dumped_blobs += 1ull;
        head->dumped_bytes += size;
        head->dumped_all += 1ull;
    }
    return sequence;
}
