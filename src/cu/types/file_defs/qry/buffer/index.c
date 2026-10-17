// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// index.c: the index: a blob read by its frame, a name found in the index and set there, a blob appended, and the first blob of a name or every blob of it joined, read out
#include "qry_buffer_internal.h"

int qry_blob_read(const unsigned char *data, unsigned long long end, unsigned long long at, QryBlob *blob)
{
    QryFrameHead head;
    if ((at > end) || ((end - at) < sizeof(head)))
    {
        return 0;
    }
    memcpy(&head, data + at, sizeof(head));
    const unsigned long long opened = sizeof(head) + (unsigned long long)head.name_length;
    if ((memcmp(head.magic, QRY_FRAME_MAGIC, sizeof(head.magic)) != 0) || (head.framed > (end - at)) ||
        ((head.framed % 8ull) != 0ull) || (head.framed < (opened + 8ull)) || (head.name_length == 0u) ||
        (head.name_length > QRY_NAME_LONGEST) || (head.bytes_at < opened) || (head.bytes_at > (head.framed - 8ull)) ||
        (head.size > (head.framed - 8ull - head.bytes_at)))
    {
        return 0;
    }
    unsigned long long last = 0ull;
    memcpy(&last, data + at + head.framed - 8ull, sizeof(last));
    if (last != head.sequence)
    {
        return 0;
    }
    blob->name_at = at + sizeof(head);
    blob->name_length = head.name_length;
    blob->bytes_at = at + head.bytes_at;
    blob->size = head.size;
    blob->sequence = head.sequence;
    blob->nanoseconds = head.nanoseconds;
    blob->priority = head.priority;
    blob->next = at + head.framed;
    return 1;
}

// `count` rounded up to the frames' alignment
unsigned long long qry_aligned(unsigned long long count)
{
    return (count + QRY_FRAME_ALIGN - 1ull) & ~(QRY_FRAME_ALIGN - 1ull);
}

// the place plus 1 of the latest blob of `name` the index holds, or 0
unsigned long long qry_index_find(QryBuffer *buffer, const char *name, size_t length)
{
    const unsigned long long hash = qry_hash(name, length);
    const unsigned long long mask = buffer->head->slots - 1ull;
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    for (unsigned long long probe = 0ull; probe < buffer->head->slots; probe += 1ull)
    {
        QrySlot *const slot = &buffer->slot[(hash + probe) & mask];
        const unsigned long long at = atomic_load(&slot->at);
        if (at == 0ull)
        {
            return 0ull;
        }
        QryBlob blob;
        if ((atomic_load(&slot->hash) == hash) && ((at - 1ull) < tail) &&
            qry_blob_read(buffer->data, tail, at - 1ull, &blob) && (blob.name_length == length) &&
            (memcmp(buffer->data + blob.name_at, name, length) == 0))
        {
            return at;
        }
    }
    return 0ull;
}

// the blob of `name` at `at` made the latest the index holds of it, under the lock: 1, or 0 where the index is full
static int qry_index_set(QryBuffer *buffer, const char *name, size_t length, unsigned long long at)
{
    const unsigned long long hash = qry_hash(name, length);
    const unsigned long long mask = buffer->head->slots - 1ull;
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    for (unsigned long long probe = 0ull; probe < buffer->head->slots; probe += 1ull)
    {
        QrySlot *const slot = &buffer->slot[(hash + probe) & mask];
        const unsigned long long held = atomic_load(&slot->at);
        QryBlob blob;
        const int same = (held != 0ull) && (atomic_load(&slot->hash) == hash) &&
                         qry_blob_read(buffer->data, tail, held - 1ull, &blob) && (blob.name_length == length) &&
                         (memcmp(buffer->data + blob.name_at, name, length) == 0);
        if ((held == 0ull) || same)
        {
            atomic_store(&slot->hash, hash);
            atomic_store(&slot->at, at + 1ull);
            return 1;
        }
    }
    buffer->head->index_full = 1ull;
    return 0;
}

// One blob framed and put at the tail under the lock, its sequence the next, its head bearing `marks` (qry.h) and the
// time it was handed, `handed_at`, or now where that is 0, and its row bearing `crc`, the CRC-64/XZ of its bytes the
// member took as it handed them. It takes room up to `room` bytes of the buffer. A panic's or a critical blob's place
// is kept for the writer and its sequence made the latest handed before the tail passes it, and a panic opens the line
// where none has. Its sequence, with `placed` 1 where it went in, or 0 where it did not fit
unsigned long long qry_appended(QryBuffer *buffer, const char *name, size_t length, const void *bytes,
                                       unsigned long long size, unsigned long long crc, unsigned int priority,
                                       unsigned int marks, unsigned long long room, unsigned long long handed_at,
                                       int *placed)
{
    QryHead *const head = buffer->head;
    head->sequence += 1ull;
    const unsigned long long sequence = head->sequence;
    handed_at = (handed_at != 0ull) ? handed_at : qry_nanoseconds();
    // the frame: its head and the name, the bytes from the next boundary, and the sequence as the last word, the whole
    // a count of boundaries
    const unsigned long long bytes_at = qry_aligned(sizeof(QryFrameHead) + (unsigned long long)length);
    const unsigned long long framed = qry_aligned(bytes_at + size + 8ull);
    const unsigned long long tail = atomic_load(&head->tail);
    *placed = 0;
    if ((framed < size) || (framed > room) || (tail > (room - framed)) || !qry_committed(buffer, tail + framed))
    {
        return sequence;
    }
    unsigned char *const frame = buffer->data + tail;
    // a name is at most QRY_NAME_LONGEST bytes and a priority at most QRY_ORDINARY, and the bytes start within the
    // frame's first QRY_FRAME_MOST bytes: each fits an unsigned int
    QryFrameHead opened = {{0},      framed, sequence, handed_at, size, (unsigned int)length, (unsigned int)bytes_at,
                           priority, marks};
    memcpy(opened.magic, QRY_FRAME_MAGIC, sizeof(opened.magic));
    memcpy(frame, &opened, sizeof(opened));
    memcpy(frame + sizeof(opened), name, length);
    memset(frame + sizeof(opened) + length, 0, (size_t)(bytes_at - sizeof(opened) - length));
    if (size != 0ull)
    {
        // the blob fits the buffer, whose bytes a size_t counts
        memcpy(frame + bytes_at, bytes, (size_t)size);
    }
    memset(frame + bytes_at + size, 0, (size_t)(framed - 8ull - bytes_at - size));
    memcpy(frame + framed - 8ull, &sequence, sizeof(sequence));
    if (priority < QRY_ORDINARY)
    {
        const unsigned long long spans = atomic_load(&head->spans);
        head->span[spans % QRY_CRITICAL_SPANS] = (QrySpan){tail, framed, sequence, priority};
        atomic_store(&head->spans, spans + 1ull);
        atomic_store(&head->critical_handed, sequence);
        if (priority == QRY_PANIC)
        {
            // the line opened by the first panic, and by the first after a conference closed it
            unsigned long long before = atomic_load(&head->panicked);
            while (((before == 0ull) || (before == atomic_load(&head->closed))) &&
                   !atomic_compare_exchange_weak(&head->panicked, &before, sequence))
            {
            }
        }
    }
    // its row of the table, where the table holds one more: the name's hash, the places of its name and bytes in the
    // buffer, and what its frame's head says
    const unsigned long long rows = atomic_load(&head->rows);
    const int rowed = (head->table_full == 0ull) && qry_rows_committed(buffer, rows + 1ull);
    if (rowed)
    {
        // a name and a priority are each far below what an unsigned int counts
        buffer->row[rows] = (QryRow){
            qry_hash(name, length), sequence, handed_at, crc, tail + sizeof(QryFrameHead), tail + bytes_at, size,
            (unsigned int)length,   priority};
    }
    head->table_full = rowed ? head->table_full : 1ull;
    // published before it is indexed or rowed: a reader that finds it by the index or the table finds every byte of
    // it before the tail
    atomic_store(&head->tail, tail + framed);
    if (rowed)
    {
        atomic_store(&head->rows, rows + 1ull);
    }
    qry_index_set(buffer, name, length, tail);
    *placed = 1;
    return sequence;
}

// The blobs dumped since the last said, said in a critical blob QRY_DUMPED under the lock, which takes room left for
// the lower numbers
void qry_dumped_said(QryBuffer *buffer)
{
    QryHead *const head = buffer->head;
    if (head->dumped_blobs == 0ull)
    {
        return;
    }
    char said[192];
    const int length = snprintf(said, sizeof(said), "%llu blobs, %llu bytes, sequences %llu to %llu\n",
                                head->dumped_blobs, head->dumped_bytes, head->dumped_first, head->dumped_last);
    int placed = 0;
    // the length is positive here, and counts the bytes said
    qry_appended(buffer, QRY_DUMPED, strlen(QRY_DUMPED), said, (unsigned long long)length,
                 qry_crc((const unsigned char *)said, (unsigned long long)length), QRY_CRITICAL, 0u, head->capacity,
                 0ull, &placed);
    if (placed)
    {
        head->dumped_blobs = 0ull;
        head->dumped_bytes = 0ull;
        head->dumped_first = 0ull;
        head->dumped_last = 0ull;
    }
}

// the memory `*memory`, which holds `*room`, grown to hold `needed`: 1, or 0 where the system gives none
static int qry_room_made(unsigned char **memory, unsigned long long *room, unsigned long long needed)
{
    if (needed <= *room)
    {
        return 1;
    }
    unsigned long long grown = (*room == 0ull) ? 65536ull : *room;
    while (grown < needed)
    {
        grown *= 2ull;
    }
    // the bytes fit memory, whose bytes a size_t counts
    unsigned char *const larger = (unsigned char *)realloc(*memory, (size_t)grown);
    if (larger == NULL)
    {
        return 0;
    }
    *memory = larger;
    *room = grown;
    return 1;
}

// `size` bytes at `bytes` added to the end of the joined memory `*joined`, which holds `*held` of `*room`
int qry_join_added(unsigned char **joined, unsigned long long *held, unsigned long long *room,
                          const unsigned char *bytes, unsigned long long size)
{
    if (!qry_room_made(joined, room, *held + size))
    {
        return 0;
    }
    if (size != 0ull)
    {
        memcpy(*joined + *held, bytes, (size_t)size);
    }
    *held += size;
    return 1;
}

// whether row `row` of the buffer's table is of the name `name`, `length` bytes, whose hash is `hash`
static int qry_row_named(const QryBuffer *buffer, const QryRow *row, unsigned long long hash, const char *name,
                         size_t length)
{
    return (row->hash == hash) && (row->name_length == length) &&
           (memcmp(buffer->data + row->name_at, name, length) == 0);
}

int qry_first(QryBuffer *buffer, const char *name, const unsigned char **bytes, unsigned long long *size)
{
    size_t length = 0u;
    if ((buffer == NULL) || !qry_name_taken(name, &length))
    {
        return 0;
    }
    qry_own_moved(buffer);
    // the rows down, in the order handed, where the table holds a row of every blob
    const unsigned long long hash = qry_hash(name, length);
    const unsigned long long rows = atomic_load(&buffer->head->rows);
    for (unsigned long long at = 0ull; (buffer->head->table_full == 0ull) && (at < rows); at += 1ull)
    {
        if (qry_row_named(buffer, &buffer->row[at], hash, name, length))
        {
            *bytes = buffer->data + buffer->row[at].bytes_at;
            *size = buffer->row[at].size;
            return 1;
        }
    }
    if (buffer->head->table_full == 0ull)
    {
        return 0;
    }
    // the blobs in the buffer stand in the order of their sequence, and the first of the name is the first found
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    QryBlob blob;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        if ((blob.name_length == length) && (memcmp(buffer->data + blob.name_at, name, length) == 0))
        {
            *bytes = buffer->data + blob.bytes_at;
            *size = blob.size;
            return 1;
        }
    }
    return 0;
}

int qry_joined(QryBuffer *buffer, const char *name, unsigned char **bytes, unsigned long long *size)
{
    size_t length = 0u;
    *bytes = NULL;
    *size = 0ull;
    if ((buffer == NULL) || !qry_name_taken(name, &length))
    {
        return 0;
    }
    qry_own_moved(buffer);
    unsigned long long room = 0ull;
    // the rows down, in the order handed, where the table holds a row of every blob: no blob but the name's is read
    if (buffer->head->table_full == 0ull)
    {
        const unsigned long long hash = qry_hash(name, length);
        const unsigned long long rows = atomic_load(&buffer->head->rows);
        for (unsigned long long at = 0ull; at < rows; at += 1ull)
        {
            const QryRow *const row = &buffer->row[at];
            if (qry_row_named(buffer, row, hash, name, length) &&
                !qry_join_added(bytes, size, &room, buffer->data + row->bytes_at, row->size))
            {
                free(*bytes);
                *bytes = NULL;
                *size = 0ull;
                return 0;
            }
        }
        return 1;
    }
    // the blobs in the buffer stand in the order of their sequence, each published whole before the tail
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    QryBlob blob;
    for (unsigned long long walk = 0ull; walk < tail; walk = blob.next)
    {
        if (!qry_blob_read(buffer->data, tail, walk, &blob) ||
            ((blob.name_length == length) && (memcmp(buffer->data + blob.name_at, name, length) == 0) &&
             !qry_join_added(bytes, size, &room, buffer->data + blob.bytes_at, blob.size)))
        {
            free(*bytes);
            *bytes = NULL;
            *size = 0ull;
            return 0;
        }
    }
    return 1;
}
