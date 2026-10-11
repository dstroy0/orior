// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_buffer.c: a query run's buffer in shared memory, the lock its processes share, its index of names, the hands
// into it and the reads out of it, the line a panic opens, and the writer that alone writes it to disk (qry_buffer.h)
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0A00
#endif
#if !defined(_WIN32)
// shared memory, the robust process-shared lock, the named semaphore, the clocks and the writes at an offset are POSIX,
// outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif

#include "qry_buffer.h"

#include "../../../includes/codecs/crc/crc_key.h"
#include "qry.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

// the bytes the head takes before the index, and the index's slot
#define QRY_HEAD_BYTES 65536u
#define QRY_SLOT_BYTES 16u

// the head's first word once it is made whole
#define QRY_MAGIC "qrybuf1"

// the longest begin tag: the tag, a name, four counts of twenty digits, `begin`, the spaces and the line's end
#define QRY_BEGIN_LONGEST (QRY_NAME_LONGEST + 112u)

// the most one write call takes, which every system's count of a write holds
#define QRY_WRITE_CALL_BYTES (1ull << 30u)

// a panic's or a critical blob's place in the buffer, its framed bytes, its sequence and its priority
typedef struct
{
    unsigned long long at;
    unsigned long long size;
    unsigned long long sequence;
    unsigned long long priority;
} QrySpan;

// a name's slot in the index: the name's hash, and the place of its latest blob's begin tag plus 1, 0 where empty
typedef struct
{
    _Atomic unsigned long long hash;
    _Atomic unsigned long long at;
} QrySlot;

// The head of the buffer, in the shared memory. Its plain words are changed by a hand or the writer holding the lock;
// its atomic words are read without it. `tail` is where the next blob goes, every byte before it handed whole;
// `written` the bytes on disk in order from the start, the writer's alone. A panic's or a critical blob's place is kept
// in `span` before the tail passes it, and the writer reads the places and the tail together under the lock: it
// finds every one before the tail it reads. `critical_handed` is the latest panic or critical sequence handed and
// `critical_written` the latest on disk; `panicked` the run's first panic, the line open from then on
typedef struct
{
    char magic[8];
    unsigned long long capacity;
    unsigned long long slots;
    unsigned long long sizes;
    unsigned long long committed;
    unsigned long long sequence;
    unsigned long long dumped_blobs;
    unsigned long long dumped_bytes;
    unsigned long long dumped_first;
    unsigned long long dumped_last;
    unsigned long long dumped_all;
    unsigned long long index_full;
    unsigned long long writer_process;
    _Atomic unsigned long long spans;
    _Atomic unsigned long long tail;
    _Atomic unsigned long long written;
    _Atomic unsigned long long critical_handed;
    _Atomic unsigned long long critical_written;
    _Atomic unsigned long long panicked;
    _Atomic unsigned int stopping;
    _Atomic unsigned int writing;
    // the rows of the table published, each of a blob before the tail; the table's bytes given memory; and 1 once a
    // blob was handed past the table's last row, which leaves the rows short of the blobs
    _Atomic unsigned long long rows;
    unsigned long long rows_committed;
    unsigned long long table_full;
#if !defined(_WIN32)
    pthread_mutex_t lock;
#endif
    QrySpan span[QRY_CRITICAL_SPANS];
} QryHead;

_Static_assert(sizeof(QryHead) <= QRY_HEAD_BYTES, "QryHead must fit in QRY_HEAD_BYTES");
_Static_assert(sizeof(QryRow) == 64u, "QryRow must be the 64 bytes qry.h gives a row");
_Static_assert(sizeof(QryTableHead) == 64u, "QryTableHead must be the 64 bytes qry.h gives the table's head");
_Static_assert(((QRY_TABLE_ROWS * 64ull) % 65536ull) == 0ull, "QRY_TABLE_ROWS must fill whole 64 KiB of rows");
_Static_assert((QRY_INDEX_SLOTS & (QRY_INDEX_SLOTS - 1ull)) == 0ull, "QRY_INDEX_SLOTS must be a power of two");
_Static_assert((QRY_COMMIT_BYTES % 65536ull) == 0ull, "QRY_COMMIT_BYTES must be a whole count of 64 KiB");
_Static_assert(QRY_SLICE_BYTES != 0ull, "QRY_SLICE_BYTES must be a count of bytes past 0");

struct QryBuffer
{
    char path[1024];
    QryHead *head;
    QrySlot *slot;
    QryRow *row;
    unsigned char *data;
    int made;
#if defined(_WIN32)
    HANDLE section;
    HANDLE lock;
    HANDLE wake;
    HANDLE writer;
#else
    unsigned char *view;
    unsigned long long view_bytes;
    int memory;
    sem_t *wake;
    char memory_name[64];
    char wake_name[64];
#endif
};

// the bytes the whole view takes: the head, the index, the table and the blobs
static unsigned long long qry_view_bytes(void)
{
    return (unsigned long long)QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES) +
           (QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow)) + QRY_BUFFER_BYTES;
}

// the hash of `length` bytes at `text`, never 0, which marks an empty slot
static unsigned long long qry_hash(const char *text, size_t length)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (size_t at = 0u; at < length; at += 1u)
    {
        hash = (hash ^ (unsigned char)text[at]) * 0x100000001b3ull;
    }
    return (hash == 0ull) ? 1ull : hash;
}

// CRC-64/XZ, the tree's own table (crc_key.h) and seven more made from it once a process, so that eight bytes are
// taken a step, each table carrying a byte one place further
static const unsigned long long s_qry_crc_table[256] = CRC_KEY_TABLE;
static unsigned long long s_qry_crc_slices[8][256];
static _Atomic int s_qry_crc_made = 0;

static void qry_crc_slices_made(void)
{
    if (atomic_load(&s_qry_crc_made) != 0)
    {
        return;
    }
    // two threads that make them at once make the same words
    for (unsigned int at = 0u; at < 256u; at += 1u)
    {
        s_qry_crc_slices[0][at] = s_qry_crc_table[at];
    }
    for (unsigned int slice = 1u; slice < 8u; slice += 1u)
    {
        for (unsigned int at = 0u; at < 256u; at += 1u)
        {
            const unsigned long long before = s_qry_crc_slices[slice - 1u][at];
            s_qry_crc_slices[slice][at] = (before >> 8u) ^ s_qry_crc_table[before & 0xffull];
        }
    }
    atomic_store(&s_qry_crc_made, 1);
}

// the CRC-64/XZ of `size` bytes at `bytes`
static unsigned long long qry_crc(const unsigned char *bytes, unsigned long long size)
{
    qry_crc_slices_made();
    unsigned long long crc = ~0ull;
    unsigned long long at = 0ull;
    for (; (size - at) >= 8ull; at += 8ull)
    {
        unsigned long long word = 0ull;
        memcpy(&word, bytes + at, 8u);
        word ^= crc;
        crc = s_qry_crc_slices[7][word & 0xffull] ^ s_qry_crc_slices[6][(word >> 8u) & 0xffull] ^
              s_qry_crc_slices[5][(word >> 16u) & 0xffull] ^ s_qry_crc_slices[4][(word >> 24u) & 0xffull] ^
              s_qry_crc_slices[3][(word >> 32u) & 0xffull] ^ s_qry_crc_slices[2][(word >> 40u) & 0xffull] ^
              s_qry_crc_slices[1][(word >> 48u) & 0xffull] ^ s_qry_crc_slices[0][word >> 56u];
    }
    for (; at < size; at += 1ull)
    {
        crc = s_qry_crc_table[(crc ^ bytes[at]) & 0xffull] ^ (crc >> 8u);
    }
    return ~crc;
}

// the sizes this process was built with and the head's own, as one word: a process built with other sizes than the
// writer's reads the buffer wrong, and is refused it
static unsigned long long qry_sizes(void)
{
    const unsigned long long sizes[] = {
        QRY_BUFFER_BYTES,  QRY_INDEX_SLOTS,    QRY_WRITE_BYTES, QRY_WRITE_MILLISECONDS,
        QRY_CRITICAL_ROOM, QRY_CRITICAL_SPANS, QRY_TABLE_ROWS,  (unsigned long long)sizeof(QryHead)};
    return qry_hash((const char *)sizes, sizeof(sizes));
}

// the wall clock's time, nanoseconds since 1970 in UTC
static unsigned long long qry_nanoseconds(void)
{
#if defined(_WIN32)
    FILETIME now;
    GetSystemTimePreciseAsFileTime(&now);
    const unsigned long long ticks = ((unsigned long long)now.dwHighDateTime << 32u) | now.dwLowDateTime;
    // the file time counts hundreds of nanoseconds from 1601; the count from 1601 to 1970 is taken off
    return (ticks - 116444736000000000ull) * 100ull;
#else
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return ((unsigned long long)now.tv_sec * 1000000000ull) + (unsigned long long)now.tv_nsec;
#endif
}

// a clock that only goes forward, in milliseconds
static unsigned long long qry_milliseconds(void)
{
#if defined(_WIN32)
    return (unsigned long long)GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((unsigned long long)now.tv_sec * 1000ull) + ((unsigned long long)now.tv_nsec / 1000000ull);
#endif
}

static void qry_sleep_millisecond(void)
{
#if defined(_WIN32)
    Sleep(1u);
#else
    const struct timespec pause = {0, 1000000};
    nanosleep(&pause, NULL);
#endif
}

// 1 where `name` is one a blob takes: from 1 to QRY_NAME_LONGEST bytes, no space and no line's end
static int qry_name_taken(const char *name, size_t *length)
{
    *length = (name != NULL) ? strlen(name) : 0u;
    return (*length != 0u) && (*length <= QRY_NAME_LONGEST) && (strpbrk(name, " \t\r\n\v\f") == NULL);
}

// the names the buffer's shared objects go by, from the hash of its path
static void qry_names(const char *path, char *memory, char *lock, char *wake, size_t room)
{
    const unsigned long long hash = qry_hash(path, strlen(path));
#if defined(_WIN32)
    snprintf(memory, room, "Local\\qry-%016llx", hash);
    snprintf(lock, room, "Local\\qry-%016llx-lock", hash);
    snprintf(wake, room, "Local\\qry-%016llx-wake", hash);
#else
    snprintf(memory, room, "/qry-%016llx", hash);
    snprintf(lock, room, "/qry-%016llx-lock", hash);
    snprintf(wake, room, "/qry-%016llx-wake", hash);
#endif
}

static void qry_lock(QryBuffer *buffer)
{
#if defined(_WIN32)
    // a hand ended mid-way left the lock abandoned: its blob was never published, and the lock is held as ever
    WaitForSingleObject(buffer->lock, INFINITE);
#else
    if (pthread_mutex_lock(&buffer->head->lock) == EOWNERDEAD)
    {
        pthread_mutex_consistent(&buffer->head->lock);
    }
#endif
}

static void qry_unlock(QryBuffer *buffer)
{
#if defined(_WIN32)
    ReleaseMutex(buffer->lock);
#else
    pthread_mutex_unlock(&buffer->head->lock);
#endif
}

static void qry_wake(QryBuffer *buffer)
{
#if defined(_WIN32)
    SetEvent(buffer->wake);
#else
    sem_post(buffer->wake);
#endif
}

// the writer's wait for a wake or `milliseconds`
static void qry_wait(QryBuffer *buffer, unsigned long long milliseconds)
{
#if defined(_WIN32)
    WaitForSingleObject(buffer->wake, (DWORD)milliseconds);
#else
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    const unsigned long long nanoseconds = (unsigned long long)until.tv_nsec + ((milliseconds % 1000ull) * 1000000ull);
    until.tv_sec += (time_t)((milliseconds / 1000ull) + (nanoseconds / 1000000000ull));
    until.tv_nsec = (long)(nanoseconds % 1000000000ull);
    while ((sem_timedwait(buffer->wake, &until) != 0) && (errno == EINTR))
    {
    }
#endif
}

// 1 while the buffer's writer runs
static int qry_writer_alive(QryBuffer *buffer)
{
    if (atomic_load(&buffer->head->writing) == 0u)
    {
        return 0;
    }
#if defined(_WIN32)
    return (buffer->writer == NULL) || (WaitForSingleObject(buffer->writer, 0u) == WAIT_TIMEOUT);
#else
    return (kill((pid_t)buffer->head->writer_process, 0) == 0) || (errno == EPERM);
#endif
}

// Memory given to the blobs through `need`, QRY_COMMIT_BYTES at a time, under the lock: 1, or 0 where the system gives
// none. Memory a process gives through its own view is the section's, and every process's view reads it
static int qry_committed(QryBuffer *buffer, unsigned long long need)
{
    QryHead *const head = buffer->head;
    if (need <= head->committed)
    {
        return 1;
    }
    unsigned long long to = ((need + QRY_COMMIT_BYTES - 1ull) / QRY_COMMIT_BYTES) * QRY_COMMIT_BYTES;
    to = (to < head->capacity) ? to : head->capacity;
#if defined(_WIN32)
    // a step of memory is at most QRY_COMMIT_BYTES past need, which a size_t holds
    if (VirtualAlloc(buffer->data + head->committed, (size_t)(to - head->committed), MEM_COMMIT, PAGE_READWRITE) ==
        NULL)
    {
        return 0;
    }
#endif
    head->committed = to;
    return 1;
}

// Memory given to the table through `rows` rows, QRY_COMMIT_BYTES at a time, under the lock: 1, or 0 where the system
// gives none or the table holds no more rows
static int qry_rows_committed(QryBuffer *buffer, unsigned long long rows)
{
    QryHead *const head = buffer->head;
    const unsigned long long need = rows * (unsigned long long)sizeof(QryRow);
    const unsigned long long whole = QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow);
    if (rows > QRY_TABLE_ROWS)
    {
        return 0;
    }
    if (need <= head->rows_committed)
    {
        return 1;
    }
    unsigned long long to = ((need + QRY_COMMIT_BYTES - 1ull) / QRY_COMMIT_BYTES) * QRY_COMMIT_BYTES;
    to = (to < whole) ? to : whole;
#if defined(_WIN32)
    // a step of memory is at most QRY_COMMIT_BYTES past need, which a size_t holds
    if (VirtualAlloc((unsigned char *)buffer->row + head->rows_committed, (size_t)(to - head->rows_committed),
                     MEM_COMMIT, PAGE_READWRITE) == NULL)
    {
        return 0;
    }
#endif
    head->rows_committed = to;
    return 1;
}

// A blob at `at` read by its tags, every byte of it before `end`: its name's place and length, its bytes' place and
// count, its sequence, the time it was handed and its priority, and where the next blob begins. 1, or 0 where a tag is
// none qry.h gives
typedef struct
{
    unsigned long long name_at;
    unsigned long long name_length;
    unsigned long long bytes_at;
    unsigned long long size;
    unsigned long long sequence;
    unsigned long long nanoseconds;
    unsigned long long priority;
    unsigned long long next;
} QryBlob;

static int qry_blob_read(const unsigned char *data, unsigned long long end, unsigned long long at, QryBlob *blob)
{
    const unsigned long long reach = ((end - at) < QRY_BEGIN_LONGEST) ? (end - at) : QRY_BEGIN_LONGEST;
    const unsigned char *const line_end = (const unsigned char *)memchr(data + at, '\n', (size_t)reach);
    const size_t tag_length = strlen(QRY_TAG);
    if ((line_end == NULL) || (reach < tag_length + 1u) || (memcmp(data + at, QRY_TAG, tag_length) != 0) ||
        (data[at + tag_length] != ' '))
    {
        return 0;
    }
    // the begin tag's words: its name, then four counts, then `begin`
    const unsigned long long name_at = at + tag_length + 1u;
    const unsigned char *const name_end =
        (const unsigned char *)memchr(data + name_at, ' ', (size_t)(line_end - (data + name_at)));
    if ((name_end == NULL) || (name_end == data + name_at))
    {
        return 0;
    }
    unsigned long long counts[4] = {0ull, 0ull, 0ull, 0ull};
    const unsigned char *cursor = name_end + 1;
    for (unsigned int place = 0u; place < 4u; place += 1u)
    {
        if ((cursor >= line_end) || (*cursor < '0') || (*cursor > '9'))
        {
            return 0;
        }
        while ((cursor < line_end) && (*cursor >= '0') && (*cursor <= '9'))
        {
            counts[place] = (counts[place] * 10ull) + (unsigned long long)(*cursor - '0');
            cursor += 1;
        }
        if ((cursor >= line_end) || (*cursor != ' '))
        {
            return 0;
        }
        cursor += 1;
    }
    const size_t begin_length = strlen(QRY_BEGIN);
    if (((size_t)(line_end - cursor) != begin_length) || (memcmp(cursor, QRY_BEGIN, begin_length) != 0))
    {
        return 0;
    }
    blob->name_at = name_at;
    blob->name_length = (unsigned long long)(name_end - (data + name_at));
    blob->bytes_at = (unsigned long long)((line_end + 1) - data);
    blob->size = counts[0];
    blob->sequence = counts[1];
    blob->nanoseconds = counts[2];
    blob->priority = counts[3];
    // the end tag, `qry <name> end`, right after the bytes
    if (blob->size > (end - blob->bytes_at))
    {
        return 0;
    }
    const unsigned long long end_at = blob->bytes_at + blob->size;
    const unsigned long long end_length = tag_length + 1u + blob->name_length + 1u + strlen(QRY_END) + 1u;
    if ((end - end_at) < end_length)
    {
        return 0;
    }
    const unsigned char *const closing = data + end_at;
    if ((memcmp(closing, QRY_TAG, tag_length) != 0) || (closing[tag_length] != ' ') ||
        (memcmp(closing + tag_length + 1u, data + name_at, (size_t)blob->name_length) != 0) ||
        (closing[tag_length + 1u + blob->name_length] != ' ') ||
        (memcmp(closing + tag_length + 2u + blob->name_length, QRY_END, strlen(QRY_END)) != 0) ||
        (closing[end_length - 1u] != '\n'))
    {
        return 0;
    }
    blob->next = end_at + end_length;
    return 1;
}

// the place plus 1 of the latest blob of `name` the index holds, or 0
static unsigned long long qry_index_find(QryBuffer *buffer, const char *name, size_t length)
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

// One blob framed and put at the tail under the lock, its sequence the next. It takes room up to `room` bytes of the
// buffer. A panic's or a critical blob's place is kept for the writer and its sequence made the latest handed before
// the tail passes it, and a panic opens the line where none has. Its sequence, with `placed` 1 where it went in, or 0
// where it did not fit
static unsigned long long qry_appended(QryBuffer *buffer, const char *name, size_t length, const void *bytes,
                                       unsigned long long size, unsigned int priority, unsigned long long room,
                                       int *placed)
{
    QryHead *const head = buffer->head;
    head->sequence += 1ull;
    const unsigned long long sequence = head->sequence;
    char begin[QRY_BEGIN_LONGEST];
    char end[QRY_NAME_LONGEST + 16u];
    const unsigned long long handed_at = qry_nanoseconds();
    const int begin_length = snprintf(begin, sizeof(begin), "%s %.*s %llu %llu %llu %u %s\n", QRY_TAG, (int)length,
                                      name, size, sequence, handed_at, priority, QRY_BEGIN);
    const int end_length = snprintf(end, sizeof(end), "%s %.*s %s\n", QRY_TAG, (int)length, name, QRY_END);
    const unsigned long long tail = atomic_load(&head->tail);
    const unsigned long long framed = (unsigned long long)begin_length + size + (unsigned long long)end_length;
    *placed = 0;
    if ((framed > room) || (tail > (room - framed)) || !qry_committed(buffer, tail + framed))
    {
        return sequence;
    }
    memcpy(buffer->data + tail, begin, (size_t)begin_length);
    if (size != 0ull)
    {
        // the blob fits the buffer, whose bytes a size_t counts
        memcpy(buffer->data + tail + (unsigned long long)begin_length, bytes, (size_t)size);
    }
    memcpy(buffer->data + tail + (unsigned long long)begin_length + size, end, (size_t)end_length);
    if (priority < QRY_ORDINARY)
    {
        const unsigned long long spans = atomic_load(&head->spans);
        head->span[spans % QRY_CRITICAL_SPANS] = (QrySpan){tail, framed, sequence, priority};
        atomic_store(&head->spans, spans + 1ull);
        atomic_store(&head->critical_handed, sequence);
        unsigned long long none = 0ull;
        if (priority == QRY_PANIC)
        {
            atomic_compare_exchange_strong(&head->panicked, &none, sequence);
        }
    }
    // its row of the table, where the table holds one more: the name's hash, the places of its tag, name and bytes in
    // the buffer, and what its begin tag says
    const unsigned long long rows = atomic_load(&head->rows);
    const int rowed = (head->table_full == 0ull) && qry_rows_committed(buffer, rows + 1ull);
    if (rowed)
    {
        // a name and a priority are each far below what an unsigned int counts
        // its CRC is the dump's to take, off the hand's way
        buffer->row[rows] = (QryRow){qry_hash(name, length),
                                     sequence,
                                     handed_at,
                                     0ull,
                                     tail + strlen(QRY_TAG) + 1ull,
                                     tail + (unsigned long long)begin_length,
                                     size,
                                     (unsigned int)length,
                                     priority};
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
static void qry_dumped_said(QryBuffer *buffer)
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
    qry_appended(buffer, QRY_DUMPED, strlen(QRY_DUMPED), said, (unsigned long long)length, QRY_CRITICAL, head->capacity,
                 &placed);
    if (placed)
    {
        head->dumped_blobs = 0ull;
        head->dumped_bytes = 0ull;
        head->dumped_first = 0ull;
        head->dumped_last = 0ull;
    }
}

static QryBuffer *qry_buffer_new(const char *path)
{
    QryBuffer *const buffer = (QryBuffer *)calloc(1u, sizeof(QryBuffer));
    if (buffer != NULL)
    {
        snprintf(buffer->path, sizeof(buffer->path), "%s", path);
#if !defined(_WIN32)
        buffer->memory = -1;
#endif
    }
    return buffer;
}

// the head, the index, the table and the blobs found in a view starting at `view`
static void qry_buffer_placed(QryBuffer *buffer, unsigned char *view)
{
    buffer->head = (QryHead *)view;
    buffer->slot = (QrySlot *)(view + QRY_HEAD_BYTES);
    buffer->row = (QryRow *)(view + QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES));
    buffer->data = view + QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES) +
                   (QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow));
}

QryBuffer *qry_create(const char *path)
{
    char memory_name[64];
    char lock_name[64];
    char wake_name[64];
    qry_names(path, memory_name, lock_name, wake_name, sizeof(memory_name));
    QryBuffer *const buffer = qry_buffer_new(path);
    if (buffer == NULL)
    {
        return NULL;
    }
    buffer->made = 1;
    const unsigned long long view_bytes = qry_view_bytes();
#if defined(_WIN32)
    // the whole view reserved and only the head and the index given memory now; the blobs' memory is given as they come
    buffer->section = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_RESERVE,
                                         (DWORD)(view_bytes >> 32u), (DWORD)(view_bytes & 0xffffffffull), memory_name);
    const int fresh = (buffer->section != NULL) && (GetLastError() != ERROR_ALREADY_EXISTS);
    unsigned char *const view =
        fresh ? (unsigned char *)MapViewOfFile(buffer->section, FILE_MAP_ALL_ACCESS, 0u, 0u, 0u) : NULL;
    const unsigned long long fixed = (unsigned long long)QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES);
    if ((view == NULL) || (VirtualAlloc(view, (size_t)fixed, MEM_COMMIT, PAGE_READWRITE) == NULL))
    {
        if (view != NULL)
        {
            UnmapViewOfFile(view);
        }
        if (buffer->section != NULL)
        {
            CloseHandle(buffer->section);
        }
        free(buffer);
        return NULL;
    }
    qry_buffer_placed(buffer, view);
    buffer->lock = CreateMutexA(NULL, FALSE, lock_name);
    buffer->wake = CreateEventA(NULL, FALSE, FALSE, wake_name);
    if ((buffer->lock == NULL) || (buffer->wake == NULL))
    {
        qry_close(buffer);
        return NULL;
    }
    buffer->head->writer_process = (unsigned long long)GetCurrentProcessId();
#else
    snprintf(buffer->memory_name, sizeof(buffer->memory_name), "%s", memory_name);
    snprintf(buffer->wake_name, sizeof(buffer->wake_name), "%s", wake_name);
    buffer->memory = shm_open(memory_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if ((buffer->memory < 0) || (ftruncate(buffer->memory, (off_t)view_bytes) != 0))
    {
        if (buffer->memory >= 0)
        {
            close(buffer->memory);
            shm_unlink(memory_name);
        }
        free(buffer);
        return NULL;
    }
    buffer->view_bytes = view_bytes;
    buffer->view =
        (unsigned char *)mmap(NULL, (size_t)view_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, buffer->memory, 0);
    buffer->wake = sem_open(wake_name, O_CREAT | O_EXCL, 0600, 0u);
    if ((buffer->view == MAP_FAILED) || (buffer->wake == SEM_FAILED))
    {
        buffer->view = (buffer->view == MAP_FAILED) ? NULL : buffer->view;
        buffer->wake = (buffer->wake == SEM_FAILED) ? NULL : buffer->wake;
        qry_close(buffer);
        return NULL;
    }
    qry_buffer_placed(buffer, buffer->view);
    pthread_mutexattr_t kind;
    pthread_mutexattr_init(&kind);
    pthread_mutexattr_setpshared(&kind, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&kind, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&buffer->head->lock, &kind);
    pthread_mutexattr_destroy(&kind);
    buffer->head->writer_process = (unsigned long long)getpid();
#endif
    QryHead *const head = buffer->head;
    head->capacity = QRY_BUFFER_BYTES;
    head->slots = QRY_INDEX_SLOTS;
    head->sizes = qry_sizes();
    atomic_store(&head->writing, 1u);
    // the magic written last: a process that opens the buffer finds it whole or not at all
    atomic_thread_fence(memory_order_release);
    memcpy(head->magic, QRY_MAGIC, sizeof(QRY_MAGIC));
    return buffer;
}

QryBuffer *qry_open(const char *path)
{
    char memory_name[64];
    char lock_name[64];
    char wake_name[64];
    qry_names(path, memory_name, lock_name, wake_name, sizeof(memory_name));
    QryBuffer *const buffer = qry_buffer_new(path);
    if (buffer == NULL)
    {
        return NULL;
    }
#if defined(_WIN32)
    buffer->section = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, memory_name);
    unsigned char *const view = (buffer->section != NULL)
                                    ? (unsigned char *)MapViewOfFile(buffer->section, FILE_MAP_ALL_ACCESS, 0u, 0u, 0u)
                                    : NULL;
    if (view != NULL)
    {
        qry_buffer_placed(buffer, view);
    }
    buffer->lock = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, lock_name);
    buffer->wake = OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, wake_name);
    if ((view == NULL) || (buffer->lock == NULL) || (buffer->wake == NULL))
    {
        qry_close(buffer);
        return NULL;
    }
#else
    buffer->memory = shm_open(memory_name, O_RDWR, 0600);
    struct stat held;
    if ((buffer->memory < 0) || (fstat(buffer->memory, &held) != 0) ||
        ((unsigned long long)held.st_size != qry_view_bytes()))
    {
        qry_close(buffer);
        return NULL;
    }
    buffer->view_bytes = (unsigned long long)held.st_size;
    buffer->view =
        (unsigned char *)mmap(NULL, (size_t)buffer->view_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, buffer->memory, 0);
    buffer->wake = sem_open(wake_name, 0);
    if ((buffer->view == MAP_FAILED) || (buffer->wake == SEM_FAILED))
    {
        buffer->view = (buffer->view == MAP_FAILED) ? NULL : buffer->view;
        buffer->wake = (buffer->wake == SEM_FAILED) ? NULL : buffer->wake;
        qry_close(buffer);
        return NULL;
    }
    qry_buffer_placed(buffer, buffer->view);
#endif
    atomic_thread_fence(memory_order_acquire);
    if ((memcmp(buffer->head->magic, QRY_MAGIC, sizeof(QRY_MAGIC)) != 0) || (buffer->head->sizes != qry_sizes()))
    {
        qry_close(buffer);
        return NULL;
    }
#if defined(_WIN32)
    buffer->writer = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)buffer->head->writer_process);
#endif
    if (!qry_writer_alive(buffer))
    {
        qry_close(buffer);
        return NULL;
    }
    return buffer;
}

QryBuffer *qry_run(void)
{
    // tried once a process, by the first thread to ask; the rest wait for its answer
    static _Atomic int s_state = 0;
    static QryBuffer *s_run = NULL;
    int expected = 0;
    if (atomic_compare_exchange_strong(&s_state, &expected, 1))
    {
        const char *const path = getenv(QRY_ENVIRONMENT);
        s_run = ((path != NULL) && (path[0] != '\0')) ? qry_open(path) : NULL;
        atomic_store(&s_state, 2);
    }
    while (atomic_load(&s_state) != 2)
    {
        qry_sleep_millisecond();
    }
    return s_run;
}

void qry_close(QryBuffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }
#if defined(_WIN32)
    if (buffer->head != NULL)
    {
        UnmapViewOfFile(buffer->head);
    }
    HANDLE handles[4] = {buffer->section, buffer->lock, buffer->wake, buffer->writer};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (handles[at] != NULL)
        {
            CloseHandle(handles[at]);
        }
    }
#else
    if (buffer->view != NULL)
    {
        munmap(buffer->view, (size_t)buffer->view_bytes);
    }
    if (buffer->memory >= 0)
    {
        close(buffer->memory);
    }
    if (buffer->wake != NULL)
    {
        sem_close(buffer->wake);
    }
    if (buffer->made)
    {
        shm_unlink(buffer->memory_name);
        sem_unlink(buffer->wake_name);
    }
#endif
    free(buffer);
}

const char *qry_path(const QryBuffer *buffer)
{
    return buffer->path;
}

unsigned long long qry_panicked(QryBuffer *buffer)
{
    return (buffer != NULL) ? atomic_load(&buffer->head->panicked) : 0ull;
}

// 1 once the critical blob of `sequence` is on disk, 0 where the writer stopped or QRY_CRITICAL_WAIT_MILLISECONDS
// passed first
static int qry_critical_waited(QryBuffer *buffer, unsigned long long sequence)
{
    const unsigned long long began = qry_milliseconds();
    while (atomic_load(&buffer->head->critical_written) < sequence)
    {
        if (!qry_writer_alive(buffer) || ((qry_milliseconds() - began) > QRY_CRITICAL_WAIT_MILLISECONDS))
        {
            return 0;
        }
        qry_sleep_millisecond();
    }
    return 1;
}

unsigned long long qry_hand(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                            unsigned int priority)
{
    size_t length = 0u;
    if ((buffer == NULL) || !qry_name_taken(name, &length) || ((bytes == NULL) && (size != 0ull)) ||
        (priority > QRY_ORDINARY))
    {
        return 0ull;
    }
    QryHead *const head = buffer->head;
    qry_lock(buffer);
    const unsigned long long room = (priority < QRY_ORDINARY) ? head->capacity : (head->capacity - QRY_CRITICAL_ROOM);
    // a dump said once, before the first blob that goes in after it: a blob that would be dumped too says nothing, and
    // a run of dumps is one blob QRY_DUMPED. The frame is taken at its most, every count twenty digits
    const unsigned long long framed_most = size + (2ull * (unsigned long long)length) + 128ull;
    if (atomic_load(&head->tail) <= ((room > framed_most) ? (room - framed_most) : 0ull))
    {
        qry_dumped_said(buffer);
    }
    const unsigned long long written = atomic_load(&head->written);
    const unsigned long long before = atomic_load(&head->tail) - written;
    int placed = 0;
    const unsigned long long sequence = qry_appended(buffer, name, length, bytes, size, priority, room, &placed);
    if (!placed && (priority == QRY_ORDINARY))
    {
        head->dumped_first = (head->dumped_blobs == 0ull) ? sequence : head->dumped_first;
        head->dumped_last = sequence;
        head->dumped_blobs += 1ull;
        head->dumped_bytes += size;
        head->dumped_all += 1ull;
    }
    const unsigned long long after = atomic_load(&head->tail) - written;
    qry_unlock(buffer);
    // the writer woken at once for a panic or a critical blob, and once as the queue crosses QRY_WRITE_BYTES, never
    // for each ordinary blob
    if (placed && ((priority < QRY_ORDINARY) || ((before < QRY_WRITE_BYTES) && (after >= QRY_WRITE_BYTES))))
    {
        qry_wake(buffer);
    }
    if (!placed)
    {
        return 0ull;
    }
    // a panic waits on nothing; a critical blob is on disk before its hand returns
    return ((priority != QRY_CRITICAL) || qry_critical_waited(buffer, sequence)) ? sequence : 0ull;
}

int qry_latest(QryBuffer *buffer, const char *name, const unsigned char **bytes, unsigned long long *size,
               unsigned long long *sequence)
{
    size_t length = 0u;
    if ((buffer == NULL) || !qry_name_taken(name, &length))
    {
        return 0;
    }
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    unsigned long long at = qry_index_find(buffer, name, length);
    QryBlob blob;
    if (at == 0ull)
    {
        if (buffer->head->index_full == 0ull)
        {
            return 0;
        }
        // a name the full index could not take is found by reading every blob
        for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
             walk = blob.next)
        {
            if ((blob.name_length == length) && (memcmp(buffer->data + blob.name_at, name, length) == 0))
            {
                at = walk + 1ull;
            }
        }
        if (at == 0ull)
        {
            return 0;
        }
    }
    if (!qry_blob_read(buffer->data, tail, at - 1ull, &blob))
    {
        return 0;
    }
    *bytes = buffer->data + blob.bytes_at;
    *size = blob.size;
    if (sequence != NULL)
    {
        *sequence = blob.sequence;
    }
    return 1;
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
static int qry_join_added(unsigned char **joined, unsigned long long *held, unsigned long long *room,
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

// a blob of a name a file holds: its sequence, and its bytes' place and count in the bytes read off the file
typedef struct
{
    unsigned long long sequence;
    unsigned long long at;
    unsigned long long size;
} QryFiled;

static int qry_filed_order(const void *left, const void *right)
{
    const unsigned long long one = ((const QryFiled *)left)->sequence;
    const unsigned long long two = ((const QryFiled *)right)->sequence;
    return (one < two) ? -1 : ((one > two) ? 1 : 0);
}

// A .qry file mapped whole for reading, the system reading it in at its own pace as the walk reaches it: its bytes and
// their count. 1, or 0 where it could not be opened or mapped; an empty file maps to no bytes and a count of 0
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
#if defined(_WIN32)
    HANDLE file;
    HANDLE section;
#else
    int file;
#endif
} QryMapped;

static int qry_mapped(QryMapped *mapped, const char *path)
{
    memset(mapped, 0, sizeof(*mapped));
#if defined(_WIN32)
    mapped->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    LARGE_INTEGER length;
    if ((mapped->file == INVALID_HANDLE_VALUE) || !GetFileSizeEx(mapped->file, &length))
    {
        if (mapped->file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(mapped->file);
        }
        mapped->file = NULL;
        return 0;
    }
    // a file's length is not negative
    mapped->size = (unsigned long long)length.QuadPart;
    if (mapped->size == 0ull)
    {
        return 1;
    }
    mapped->section = CreateFileMappingA(mapped->file, NULL, PAGE_READONLY, 0u, 0u, NULL);
    mapped->bytes = (mapped->section != NULL)
                        ? (const unsigned char *)MapViewOfFile(mapped->section, FILE_MAP_READ, 0u, 0u, 0u)
                        : NULL;
    return mapped->bytes != NULL;
#else
    mapped->file = open(path, O_RDONLY);
    struct stat held;
    if ((mapped->file < 0) || (fstat(mapped->file, &held) != 0))
    {
        return 0;
    }
    mapped->size = (unsigned long long)held.st_size;
    if (mapped->size == 0ull)
    {
        return 1;
    }
    void *const view = mmap(NULL, (size_t)mapped->size, PROT_READ, MAP_PRIVATE, mapped->file, 0);
    mapped->bytes = (view != MAP_FAILED) ? (const unsigned char *)view : NULL;
    return mapped->bytes != NULL;
#endif
}

static void qry_unmapped(QryMapped *mapped)
{
#if defined(_WIN32)
    if (mapped->bytes != NULL)
    {
        UnmapViewOfFile(mapped->bytes);
    }
    if (mapped->section != NULL)
    {
        CloseHandle(mapped->section);
    }
    if (mapped->file != NULL)
    {
        CloseHandle(mapped->file);
    }
#else
    if (mapped->bytes != NULL)
    {
        munmap((void *)mapped->bytes, (size_t)mapped->size);
    }
    if (mapped->file >= 0)
    {
        close(mapped->file);
    }
#endif
    memset(mapped, 0, sizeof(*mapped));
}

// the next place past `at` where a blob whose tags read whole begins, at the start of a line, or `size` where none does
static unsigned long long qry_next_whole(const unsigned char *bytes, unsigned long long size, unsigned long long at)
{
    QryBlob blob;
    unsigned long long from = at;
    while (from < size)
    {
        // the next line's start, past the line end at or after `from`: every turn steps past one line end
        const unsigned char *const line_end = (const unsigned char *)memchr(bytes + from, '\n', (size_t)(size - from));
        if (line_end == NULL)
        {
            return size;
        }
        from = (unsigned long long)(line_end - bytes) + 1ull;
        if ((from < size) && qry_blob_read(bytes, size, from, &blob))
        {
            return from;
        }
    }
    return size;
}

// The table a mapped .qry opens with (qry.h), its head copied to `table` and its rows found in place: 1, or 0 where the
// .qry opens with none, or with one whose rows or blobs do not lie inside it, and it is read by its tags
static int qry_table_found(const QryMapped *mapped, QryTableHead *table, const QryRow **rows)
{
    if ((mapped->size < sizeof(QryTableHead)) || (memcmp(mapped->bytes, QRY_TABLE_MAGIC, sizeof(table->magic)) != 0))
    {
        return 0;
    }
    memcpy(table, mapped->bytes, sizeof(*table));
    const int sized = (table->row_bytes == sizeof(QryRow)) && (table->rows_at >= sizeof(QryTableHead)) &&
                      (table->rows_at <= mapped->size) &&
                      (table->rows <= ((mapped->size - table->rows_at) / sizeof(QryRow)));
    const unsigned long long rows_end = sized ? (table->rows_at + (table->rows * sizeof(QryRow))) : 0ull;
    if (!sized || (table->blobs_at < rows_end) || (table->blobs_at > mapped->size) ||
        (table->blobs_bytes > (mapped->size - table->blobs_at)))
    {
        return 0;
    }
    // the rows start on a whole row past the mapping's start, which the system places on a page; they are held to the
    // head's CRC, the root of every check past it, and rows that do not hold it leave the file read by its tags
    *rows = (const QryRow *)(const void *)(mapped->bytes + table->rows_at);
    return qry_crc(mapped->bytes + table->rows_at, table->rows * sizeof(QryRow)) == table->rows_crc;
}

// whether `row` of a mapped .qry's table has its name and its bytes inside the blobs
static int qry_row_inside(const QryTableHead *table, const QryRow *row)
{
    const unsigned long long end = table->blobs_at + table->blobs_bytes;
    return (row->name_at >= table->blobs_at) && (row->name_length <= end) &&
           (row->name_at <= (end - row->name_length)) && (row->bytes_at >= table->blobs_at) && (row->bytes_at <= end) &&
           (row->size <= (end - row->bytes_at));
}

// whether `row` of a mapped .qry's table names `name`, `length` bytes whose hash is `hash`, with its name and its bytes
// inside the blobs
static int qry_row_filed(const QryMapped *mapped, const QryTableHead *table, const QryRow *row, unsigned long long hash,
                         const char *name, size_t length)
{
    return (row->hash == hash) && (row->name_length == length) && qry_row_inside(table, row) &&
           (memcmp(mapped->bytes + row->name_at, name, length) == 0);
}

// whether the blob `row` of a mapped .qry's table jumps to reads whole by its own tags, is the blob the row says, and
// holds the bytes the row's CRC was taken of: what a reader reads it reads checked, and a blob the file lost is told
// from one it holds
static int qry_row_whole(const QryMapped *mapped, const QryTableHead *table, const QryRow *row)
{
    QryBlob blob;
    const unsigned long long end = table->blobs_at + table->blobs_bytes;
    const unsigned long long opened = strlen(QRY_TAG) + 1ull;
    return (row->name_at >= (table->blobs_at + opened)) && (row->name_at < end) &&
           qry_blob_read(mapped->bytes, end, row->name_at - opened, &blob) && (blob.name_at == row->name_at) &&
           (blob.bytes_at == row->bytes_at) && (blob.size == row->size) && (blob.sequence == row->sequence) &&
           (qry_crc(mapped->bytes + row->bytes_at, row->size) == row->crc);
}

// the first of `count` rows whose hash is `hash`, or the first past it where none is: the rows stand in the order of
// their hashes, and a search halves them
static unsigned long long qry_rows_searched(const QryRow *rows, unsigned long long count, unsigned long long hash)
{
    unsigned long long low = 0ull;
    unsigned long long high = count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        if (rows[middle].hash < hash)
        {
            low = middle + 1ull;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

int qry_file_joined(const char *path, const char *name, unsigned char **bytes, unsigned long long *size, char *error,
                    size_t room)
{
    size_t length = 0u;
    *bytes = NULL;
    *size = 0ull;
    if (room != 0u)
    {
        error[0] = '\0';
    }
    if (!qry_name_taken(name, &length))
    {
        snprintf(error, room, "%s is no name a blob takes", (name != NULL) ? name : "(none)");
        return 0;
    }
    QryMapped mapped;
    if (!qry_mapped(&mapped, path))
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s could not be opened", path);
        return 0;
    }
    // a .qry that opens with its table: the name's rows found by its hash and their bytes joined, the rows of one name
    // standing in the order of their sequences, and no other blob read
    QryTableHead table;
    const QryRow *rows = NULL;
    if (qry_table_found(&mapped, &table, &rows))
    {
        const unsigned long long hash = qry_hash(name, length);
        unsigned long long joined_room = 0ull;
        unsigned long long lost = 0ull;
        int joined = 1;
        for (unsigned long long row = qry_rows_searched(rows, table.rows, hash);
             joined && (row < table.rows) && (rows[row].hash == hash); row += 1ull)
        {
            if (rows[row].name_length != length)
            {
                continue;
            }
            // a blob of the name's hash that does not read whole may be the name's, its own name lost with it: it is
            // counted as lost, never passed over unsaid
            if (!qry_row_whole(&mapped, &table, &rows[row]))
            {
                lost += rows[row].size;
                continue;
            }
            if (qry_row_filed(&mapped, &table, &rows[row], hash, name, length))
            {
                joined = qry_join_added(bytes, size, &joined_room, mapped.bytes + rows[row].bytes_at, rows[row].size);
            }
        }
        qry_unmapped(&mapped);
        if (!joined)
        {
            free(*bytes);
            *bytes = NULL;
            *size = 0ull;
            snprintf(error, room, "%s: no memory to join the blobs of %s", path, name);
        }
        else if (lost != 0ull)
        {
            snprintf(error, room, "%s: %llu bytes of %s whose tags do not read whole were stepped over", path, lost,
                     name);
        }
        return joined;
    }
    QryFiled *filed = NULL;
    size_t filed_count = 0u;
    size_t filed_room = 0u;
    unsigned long long skipped = 0ull;
    int whole = 1;
    // each blob read by its tags in place and taken where it is of `name`; a stretch no tag opens is stepped over to
    // the next blob whose tags read whole
    unsigned long long at = 0ull;
    while (whole && (at < mapped.size))
    {
        QryBlob blob;
        if (!qry_blob_read(mapped.bytes, mapped.size, at, &blob))
        {
            const unsigned long long next = qry_next_whole(mapped.bytes, mapped.size, at);
            skipped += next - at;
            at = next;
            continue;
        }
        if ((blob.name_length == length) && (memcmp(mapped.bytes + blob.name_at, name, length) == 0))
        {
            if (filed_count == filed_room)
            {
                filed_room = (filed_room == 0u) ? 64u : (filed_room * 2u);
                QryFiled *const larger = (QryFiled *)realloc(filed, filed_room * sizeof(QryFiled));
                whole = (larger != NULL);
                filed = (larger != NULL) ? larger : filed;
            }
            if (whole)
            {
                filed[filed_count] = (QryFiled){blob.sequence, blob.bytes_at, blob.size};
                filed_count += 1u;
            }
        }
        at = blob.next;
    }
    // the blobs put back in the order handed, by their sequence, and joined straight from the mapped file
    if (whole && (filed_count != 0u))
    {
        qsort(filed, filed_count, sizeof(QryFiled), qry_filed_order);
    }
    unsigned long long joined_room = 0ull;
    for (size_t place = 0u; whole && (place < filed_count); place += 1u)
    {
        whole = qry_join_added(bytes, size, &joined_room, mapped.bytes + filed[place].at, filed[place].size);
    }
    free(filed);
    qry_unmapped(&mapped);
    if (!whole)
    {
        free(*bytes);
        *bytes = NULL;
        *size = 0ull;
        snprintf(error, room, "%s: no memory to join the blobs of %s", path, name);
        return 0;
    }
    if (skipped != 0ull)
    {
        snprintf(error, room, "%s: %llu bytes no tag opens were stepped over", path, skipped);
    }
    return 1;
}

// a name's latest blob in a file: the name's hash, place and length, and the blob's sequence, bytes' place and count
typedef struct
{
    unsigned long long hash;
    unsigned long long name_at;
    unsigned long long name_length;
    unsigned long long sequence;
    unsigned long long bytes_at;
    unsigned long long size;
} QryLatest;

static int qry_latest_order(const void *left, const void *right)
{
    const unsigned long long one = ((const QryLatest *)left)->sequence;
    const unsigned long long two = ((const QryLatest *)right)->sequence;
    return (one < two) ? -1 : ((one > two) ? 1 : 0);
}

int qry_file_latest_each(const char *path,
                         void (*each)(void *context, const char *name, const unsigned char *bytes,
                                      unsigned long long size),
                         void *context, char *error, size_t room)
{
    if (room != 0u)
    {
        error[0] = '\0';
    }
    QryMapped mapped;
    if (!qry_mapped(&mapped, path))
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s could not be opened", path);
        return 0;
    }
    // a .qry that opens with its table: the rows gone down a hash at a time, each name's latest the last of its rows,
    // which stand in the order of their sequences, and only those blobs read
    QryTableHead front;
    const QryRow *front_rows = NULL;
    if (qry_table_found(&mapped, &front, &front_rows))
    {
        QryLatest *const latest = (QryLatest *)malloc((size_t)(front.rows + 1ull) * sizeof(QryLatest));
        if (latest == NULL)
        {
            qry_unmapped(&mapped);
            snprintf(error, room, "%s: no memory to read its names", path);
            return 0;
        }
        size_t kept = 0u;
        unsigned long long lost = 0ull;
        unsigned long long run = 0ull;
        while (run < front.rows)
        {
            unsigned long long end = run;
            while ((end < front.rows) && (front_rows[end].hash == front_rows[run].hash))
            {
                end += 1ull;
            }
            // each name of the hash kept once, its latest row in its place; two names share a hash only where their
            // hashes meet, and then the names of the run are few
            const size_t first = kept;
            for (unsigned long long row = run; row < end; row += 1ull)
            {
                const QryRow *const one = &front_rows[row];
                if (!qry_row_inside(&front, one) || !qry_row_whole(&mapped, &front, one))
                {
                    lost += one->size;
                    continue;
                }
                size_t named = first;
                while ((named < kept) && !((latest[named].name_length == one->name_length) &&
                                           (memcmp(mapped.bytes + latest[named].name_at, mapped.bytes + one->name_at,
                                                   one->name_length) == 0)))
                {
                    named += 1u;
                }
                latest[named] =
                    (QryLatest){one->hash, one->name_at, one->name_length, one->sequence, one->bytes_at, one->size};
                kept += (named == kept) ? 1u : 0u;
            }
            run = end;
        }
        qsort(latest, kept, sizeof(QryLatest), qry_latest_order);
        char name[QRY_NAME_LONGEST + 1u];
        for (size_t place = 0u; place < kept; place += 1u)
        {
            snprintf(name, sizeof(name), "%.*s", (int)latest[place].name_length,
                     (const char *)(mapped.bytes + latest[place].name_at));
            each(context, name, mapped.bytes + latest[place].bytes_at, latest[place].size);
        }
        free(latest);
        qry_unmapped(&mapped);
        if (lost != 0ull)
        {
            snprintf(error, room, "%s: %llu bytes of blobs whose tags do not read whole were stepped over", path, lost);
        }
        return 1;
    }
    // the names counted by the blobs, at most one a blob, and a table of twice as many slots
    size_t slots = 64u;
    for (unsigned long long at = 0ull; at < mapped.size; at += 1ull)
    {
        slots += (mapped.bytes[at] == '\n') ? 1u : 0u;
    }
    size_t power = 64u;
    while (power < slots)
    {
        power *= 2u;
    }
    QryLatest *const table = (QryLatest *)calloc(power, sizeof(QryLatest));
    if (table == NULL)
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s: no memory to read its names", path);
        return 0;
    }
    unsigned long long skipped = 0ull;
    unsigned long long at = 0ull;
    while (at < mapped.size)
    {
        QryBlob blob;
        if (!qry_blob_read(mapped.bytes, mapped.size, at, &blob))
        {
            const unsigned long long next = qry_next_whole(mapped.bytes, mapped.size, at);
            skipped += next - at;
            at = next;
            continue;
        }
        // the blob kept where its name is new, or where it is later than the one kept; a hash is never 0, the mark of a
        // slot no name holds
        const unsigned long long hash = qry_hash((const char *)(mapped.bytes + blob.name_at), (size_t)blob.name_length);
        for (size_t probe = (size_t)hash & (power - 1u);; probe = (probe + 1u) & (power - 1u))
        {
            QryLatest *const slot = &table[probe];
            if (slot->hash == 0ull)
            {
                *slot = (QryLatest){hash, blob.name_at, blob.name_length, blob.sequence, blob.bytes_at, blob.size};
                break;
            }
            if ((slot->hash == hash) && (slot->name_length == blob.name_length) &&
                (memcmp(mapped.bytes + slot->name_at, mapped.bytes + blob.name_at, (size_t)blob.name_length) == 0))
            {
                if (blob.sequence > slot->sequence)
                {
                    slot->sequence = blob.sequence;
                    slot->bytes_at = blob.bytes_at;
                    slot->size = blob.size;
                }
                break;
            }
        }
        at = blob.next;
    }
    // the kept blobs gathered and put in the order of their sequences, each handed with its name
    size_t kept = 0u;
    for (size_t probe = 0u; probe < power; probe += 1u)
    {
        if (table[probe].hash != 0ull)
        {
            table[kept] = table[probe];
            kept += 1u;
        }
    }
    qsort(table, kept, sizeof(QryLatest), qry_latest_order);
    char name[QRY_NAME_LONGEST + 1u];
    for (size_t place = 0u; place < kept; place += 1u)
    {
        snprintf(name, sizeof(name), "%.*s", (int)table[place].name_length,
                 (const char *)(mapped.bytes + table[place].name_at));
        each(context, name, mapped.bytes + table[place].bytes_at, table[place].size);
    }
    free(table);
    qry_unmapped(&mapped);
    if (skipped != 0ull)
    {
        snprintf(error, room, "%s: %llu bytes no tag opens were stepped over", path, skipped);
    }
    return 1;
}

// The .qry the writer writes: a handle that writes through the system's memory for the queue in bulk, and one that
// writes through to the disk itself for panics and critical blobs, both at the end the writer keeps, `length`
typedef struct
{
#if defined(_WIN32)
    HANDLE bulk;
    HANDLE through;
#else
    int bulk;
    int through;
#endif
    unsigned long long length;
} QryFile;

// the .qry at `path` made anew and both its handles opened: 1, or 0 where either was not
static int qry_file_opened(QryFile *file, const char *path)
{
    file->length = 0ull;
#if defined(_WIN32)
    file->bulk = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    file->through = (file->bulk != INVALID_HANDLE_VALUE)
                        ? CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL)
                        : INVALID_HANDLE_VALUE;
    return (file->bulk != INVALID_HANDLE_VALUE) && (file->through != INVALID_HANDLE_VALUE);
#else
    file->bulk = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    file->through = (file->bulk >= 0) ? open(path, O_WRONLY | O_DSYNC) : -1;
    return (file->bulk >= 0) && (file->through >= 0);
#endif
}

// `size` bytes at `bytes` written at the file's end, through to the disk itself where `through`: 1, or 0 where they
// did not all go
static int qry_file_added(QryFile *file, const unsigned char *bytes, unsigned long long size, int through)
{
    unsigned long long done = 0ull;
    while (done < size)
    {
        const unsigned long long piece = ((size - done) < QRY_WRITE_CALL_BYTES) ? (size - done) : QRY_WRITE_CALL_BYTES;
        const unsigned long long offset = file->length + done;
#if defined(_WIN32)
        OVERLAPPED place;
        memset(&place, 0, sizeof(place));
        place.Offset = (DWORD)(offset & 0xffffffffull);
        place.OffsetHigh = (DWORD)(offset >> 32u);
        DWORD wrote = 0u;
        // a piece is at most QRY_WRITE_CALL_BYTES, which a DWORD counts
        if (!WriteFile(through ? file->through : file->bulk, bytes + done, (DWORD)piece, &wrote, &place) ||
            (wrote == 0u))
        {
            return 0;
        }
        done += wrote;
#else
        const ssize_t wrote = pwrite(through ? file->through : file->bulk, bytes + done, (size_t)piece, (off_t)offset);
        if ((wrote < 0) && (errno == EINTR))
        {
            continue;
        }
        if (wrote <= 0)
        {
            return 0;
        }
        done += (unsigned long long)wrote;
#endif
    }
    file->length += size;
    return 1;
}

// the bytes written through the bulk handle flushed through to the disk itself: 1, or 0 where they were not
static int qry_file_durable(QryFile *file)
{
#if defined(_WIN32)
    return FlushFileBuffers(file->bulk) != 0;
#else
    return fsync(file->bulk) == 0;
#endif
}

// the file flushed through to the disk itself and both handles closed: 1, or 0 where either failed
static int qry_file_closed(QryFile *file)
{
    const int durable = qry_file_durable(file);
#if defined(_WIN32)
    const int closed = (CloseHandle(file->bulk) != 0) & (CloseHandle(file->through) != 0);
#else
    const int closed = (close(file->bulk) == 0) & (close(file->through) == 0);
#endif
    return durable && closed;
}

// What the writer's writes came to: the bytes, the writes, the slowest of them and its bytes, the time in all, the
// panics and the longest any took from its hand to the disk, and whether a write did not go whole
typedef struct
{
    unsigned long long bytes;
    unsigned long long writes;
    unsigned long long slowest_bytes;
    double slowest;
    double all;
    unsigned long long panics;
    double panic_slowest;
    int failed;
} QryWrites;

// a write of `bytes` that took from `began` to now, counted
static void qry_write_counted(QryWrites *writes, unsigned long long began, unsigned long long bytes, int whole)
{
    const double milliseconds = (double)(qry_nanoseconds() - began) / 1000000.0;
    writes->bytes += bytes;
    writes->writes += 1ull;
    writes->all += milliseconds;
    writes->failed |= !whole;
    if (milliseconds > writes->slowest)
    {
        writes->slowest = milliseconds;
        writes->slowest_bytes = bytes;
    }
}

// The writer's own memory of its work: the panics and critical blobs it wrote ahead of the queue, in the order of their
// places, which the queue's write steps over; the places it has read; what it read together under the lock the last
// time it looked, the tail, the places and the latest panic or critical sequence handed; and what its writes came to
typedef struct
{
    QrySpan early[2u * QRY_CRITICAL_SPANS];
    size_t early_count;
    unsigned long long spans_seen;
    unsigned long long tail_seen;
    unsigned long long spans_read;
    unsigned long long handed_read;
    QryWrites writes;
} QryWriterState;

// Every panic and critical blob handed since the writer last looked written at the file's end through to the disk
// itself, the panics first, then the critical blobs, each in the order handed, and the latest of them made the latest
// on disk; the tail, the places and the latest sequence handed read with them kept in `state`. 1, or 0 where more were
// handed than the buffer keeps places for, and the caller writes the queue in order and flushes it through instead
static int qry_firsts_written(QryBuffer *buffer, QryFile *file, QryWriterState *state)
{
    QryHead *const head = buffer->head;
    static QrySpan s_taken[QRY_CRITICAL_SPANS];
    size_t count = 0u;
    qry_lock(buffer);
    const unsigned long long spans = atomic_load(&head->spans);
    const unsigned long long handed = atomic_load(&head->critical_handed);
    state->tail_seen = atomic_load(&head->tail);
    state->spans_read = spans;
    state->handed_read = handed;
    unsigned long long *const tail_seen = &state->tail_seen;
    const int kept = ((spans - state->spans_seen) <= QRY_CRITICAL_SPANS) &&
                     ((state->early_count + (size_t)(spans - state->spans_seen)) <= (2u * QRY_CRITICAL_SPANS));
    for (unsigned long long place = state->spans_seen; kept && (place < spans); place += 1ull)
    {
        s_taken[count] = head->span[place % QRY_CRITICAL_SPANS];
        count += 1u;
    }
    qry_unlock(buffer);
    if (!kept)
    {
        return 0;
    }
    if (count != 0u)
    {
        const unsigned long long began = qry_nanoseconds();
        int whole = 1;
        unsigned long long bytes = 0ull;
        // the panics first, then the critical blobs, each kind in the order handed
        for (unsigned int priority = QRY_PANIC; priority < QRY_ORDINARY; priority += 1u)
        {
            for (size_t place = 0u; place < count; place += 1u)
            {
                if (s_taken[place].priority != priority)
                {
                    continue;
                }
                whole &= qry_file_added(file, buffer->data + s_taken[place].at, s_taken[place].size, 1);
                bytes += s_taken[place].size;
                if (priority == QRY_PANIC)
                {
                    // a panic's time from its hand to the disk, its hand's time read off its begin tag
                    QryBlob blob;
                    const unsigned long long now = qry_nanoseconds();
                    if (qry_blob_read(buffer->data, *tail_seen, s_taken[place].at, &blob) && (now > blob.nanoseconds))
                    {
                        const double milliseconds = (double)(now - blob.nanoseconds) / 1000000.0;
                        state->writes.panic_slowest =
                            (milliseconds > state->writes.panic_slowest) ? milliseconds : state->writes.panic_slowest;
                    }
                    state->writes.panics += 1ull;
                }
            }
        }
        // kept in the order of their places, the order handed, for the queue's write to step over
        for (size_t place = 0u; place < count; place += 1u)
        {
            state->early[state->early_count] = s_taken[place];
            state->early_count += 1u;
        }
        qry_write_counted(&state->writes, began, bytes, whole);
    }
    state->spans_seen = spans;
    if (handed > atomic_load(&head->critical_written))
    {
        atomic_store(&head->critical_written, handed);
    }
    return 1;
}

// The buffer's bytes `from` to `to` written in order at the file's end a slice at a time, the blobs written ahead of it
// stepped over and let go, and between slices every panic or critical blob handed meanwhile let through first: one
// write, timed. A slice ends on a blob's end, the first past QRY_SLICE_BYTES or the stretch's, never inside a blob: a
// blob let through between two slices stands whole between two whole blobs
static void qry_range_written(QryBuffer *buffer, QryFile *file, unsigned long long from, unsigned long long to,
                              QryWriterState *state)
{
    if (to <= from)
    {
        return;
    }
    const unsigned long long began = qry_nanoseconds();
    unsigned long long at = from;
    unsigned long long bytes = 0ull;
    int whole = 1;
    // where more were handed than the buffer keeps places for, the rest of the stretch looks for none: they are written
    // in order and flushed through by the writer's loop
    int overflowed = 0;
    while (at < to)
    {
        // the next blob written ahead within the stretch, the earliest place, where there is one
        size_t next = state->early_count;
        for (size_t place = 0u; place < state->early_count; place += 1u)
        {
            if ((state->early[place].at >= at) && (state->early[place].at < to) &&
                ((next == state->early_count) || (state->early[place].at < state->early[next].at)))
            {
                next = place;
            }
        }
        const unsigned long long stop = (next != state->early_count) ? state->early[next].at : to;
        // the slice's end stepped blob by blob from its start, each blob's end read off its own tags
        unsigned long long slice_end = at;
        QryBlob blob;
        while ((slice_end < stop) && ((slice_end - at) < QRY_SLICE_BYTES) &&
               qry_blob_read(buffer->data, stop, slice_end, &blob))
        {
            slice_end = blob.next;
        }
        // a stretch whose tags do not read is written whole to its stop, as it stands in the buffer
        slice_end = ((slice_end == at) && (at < stop)) ? stop : slice_end;
        whole &= qry_file_added(file, buffer->data + at, slice_end - at, 0);
        bytes += slice_end - at;
        at = slice_end;
        if ((next != state->early_count) && (at == state->early[next].at))
        {
            at += state->early[next].size;
            state->early[next] = state->early[state->early_count - 1u];
            state->early_count -= 1u;
        }
        // a panic or critical blob handed since the stretch was read goes before the next slice; its place lies past
        // the stretch, which was read with every place before it
        if (!overflowed && (at < to) && (atomic_load(&buffer->head->spans) != state->spans_seen))
        {
            overflowed = !qry_firsts_written(buffer, file, state);
        }
    }
    qry_write_counted(&state->writes, began, bytes, whole);
    atomic_store(&buffer->head->written, to);
}

static int qry_row_order(const void *left, const void *right)
{
    const QryRow *const one = (const QryRow *)left;
    const QryRow *const two = (const QryRow *)right;
    if (one->hash != two->hash)
    {
        return (one->hash < two->hash) ? -1 : 1;
    }
    return (one->sequence < two->sequence) ? -1 : ((one->sequence > two->sequence) ? 1 : 0);
}

// a row of every blob before `tail`, in memory of the caller's to free: the table's own where it holds a row of every
// blob, else each read off the blob's tags. NULL where there was no memory; `count` the rows
static QryRow *qry_rows_taken(QryBuffer *buffer, unsigned long long tail, unsigned long long rows,
                              unsigned long long *count)
{
    *count = 0ull;
    if (buffer->head->table_full == 0ull)
    {
        QryRow *const taken = (QryRow *)malloc((size_t)(rows + 1ull) * sizeof(QryRow));
        if (taken != NULL)
        {
            // the rows fit the buffer's table, whose bytes a size_t counts
            memcpy(taken, buffer->row, (size_t)(rows * sizeof(QryRow)));
            *count = rows;
        }
        return taken;
    }
    unsigned long long blobs = 0ull;
    QryBlob blob;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        blobs += 1ull;
    }
    QryRow *const taken = (QryRow *)malloc((size_t)(blobs + 1ull) * sizeof(QryRow));
    for (unsigned long long walk = 0ull;
         (taken != NULL) && (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob); walk = blob.next)
    {
        // a name and a priority are each far below what an unsigned int counts
        taken[*count] = (QryRow){qry_hash((const char *)(buffer->data + blob.name_at), (size_t)blob.name_length),
                                 blob.sequence,
                                 blob.nanoseconds,
                                 0ull,
                                 blob.name_at,
                                 blob.bytes_at,
                                 blob.size,
                                 (unsigned int)blob.name_length,
                                 (unsigned int)blob.priority};
        *count += 1ull;
    }
    return taken;
}

// The run dumped from the buffer as the .qry readers take (qry.h): the table's head, every row put in the order of its
// name's hash and then its sequence with its places made the file's, then every blob in the order handed, written as
// it stands in the buffer; written beside the .qry, flushed through to the disk, and put in the .qry's place. The .qry
// the writer wrote as the run ran is left as it is where any of that fails. 1, or 0; `count` the rows dumped
static int qry_table_dumped(QryBuffer *buffer, unsigned long long *count)
{
    qry_lock(buffer);
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    const unsigned long long rows = atomic_load(&buffer->head->rows);
    qry_unlock(buffer);
    QryRow *const taken = qry_rows_taken(buffer, tail, rows, count);
    if (taken == NULL)
    {
        return 0;
    }
    qsort(taken, (size_t)*count, sizeof(QryRow), qry_row_order);
    QryTableHead table;
    memset(&table, 0, sizeof(table));
    memcpy(table.magic, QRY_TABLE_MAGIC, sizeof(table.magic));
    table.rows = *count;
    table.rows_at = sizeof(QryTableHead);
    table.row_bytes = sizeof(QryRow);
    table.blobs_at = table.rows_at + (*count * sizeof(QryRow));
    table.blobs_bytes = tail;
    // each row given its blob's CRC and its places made the file's, and the rows' CRC the root of the file's checks
    for (unsigned long long row = 0ull; row < *count; row += 1ull)
    {
        taken[row].crc = qry_crc(buffer->data + taken[row].bytes_at, taken[row].size);
        taken[row].name_at += table.blobs_at;
        taken[row].bytes_at += table.blobs_at;
    }
    table.rows_crc = qry_crc((const unsigned char *)taken, *count * sizeof(QryRow));
    char dumped[sizeof(buffer->path) + 8u];
    snprintf(dumped, sizeof(dumped), "%s.tmp", buffer->path);
    QryFile file;
    int whole = qry_file_opened(&file, dumped);
    if (whole)
    {
        whole = qry_file_added(&file, (const unsigned char *)&table, sizeof(table), 0) &&
                qry_file_added(&file, (const unsigned char *)taken, *count * sizeof(QryRow), 0) &&
                qry_file_added(&file, buffer->data, tail, 0);
        whole = qry_file_closed(&file) && whole;
    }
    free(taken);
#if defined(_WIN32)
    whole = whole && (MoveFileExA(dumped, buffer->path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0);
#else
    whole = whole && (rename(dumped, buffer->path) == 0);
#endif
    if (!whole)
    {
        remove(dumped);
    }
    return whole;
}

int qry_write_until_stopped(QryBuffer *buffer)
{
    QryHead *const head = buffer->head;
    QryFile file;
    QryWriterState *const state = (QryWriterState *)calloc(1u, sizeof(QryWriterState));
    if ((state == NULL) || !qry_file_opened(&file, buffer->path))
    {
        free(state);
        atomic_store(&head->writing, 0u);
        return 0;
    }
    unsigned long long last = qry_milliseconds();
    int stopped = 0;
    while (!stopped)
    {
        const unsigned long long waited = qry_milliseconds() - last;
        qry_wait(buffer, (waited < QRY_WRITE_MILLISECONDS) ? (QRY_WRITE_MILLISECONDS - waited) : 1ull);
        const int stopping = (atomic_load(&head->stopping) != 0u);
        // a dump no blob after it came to say, said by the writer once the run stops
        if (stopping)
        {
            qry_lock(buffer);
            qry_dumped_said(buffer);
            qry_unlock(buffer);
        }
        // panics and critical blobs first, every time the writer wakes
        if (!qry_firsts_written(buffer, &file, state))
        {
            // more were handed than the buffer keeps places for: the queue written in order through the tail read with
            // them and flushed through, and every one of them handed by then is on disk with it
            state->spans_seen = state->spans_read;
            qry_range_written(buffer, &file, atomic_load(&head->written), state->tail_seen, state);
            state->writes.failed |= !qry_file_durable(&file);
            if (state->handed_read > atomic_load(&head->critical_written))
            {
                atomic_store(&head->critical_written, state->handed_read);
            }
            last = qry_milliseconds();
        }
        const unsigned long long tail = state->tail_seen;
        const unsigned long long written = atomic_load(&head->written);
        const int due = stopping || ((tail - written) >= QRY_WRITE_BYTES) ||
                        ((qry_milliseconds() - last) >= QRY_WRITE_MILLISECONDS) ||
                        (state->early_count >= QRY_CRITICAL_SPANS);
        if ((tail > written) && due)
        {
            qry_range_written(buffer, &file, written, tail, state);
            last = qry_milliseconds();
        }
        stopped = stopping && (atomic_load(&head->tail) == atomic_load(&head->written)) &&
                  (atomic_load(&head->critical_written) >= atomic_load(&head->critical_handed)) &&
                  (head->dumped_blobs == 0ull);
    }
    // the writer's own account, the run's last blob, written and flushed through to the disk
    char account[384];
    const QryWrites *const writes = &state->writes;
    const int length = snprintf(account, sizeof(account),
                                "%llu blobs handed, %llu dumped, %llu bytes in %llu writes, the slowest %.3f ms for "
                                "%llu bytes, %.3f ms writing in all; %llu panics, the slowest %.3f ms from its hand to "
                                "the disk%s\n",
                                head->sequence, head->dumped_all, writes->bytes, writes->writes, writes->slowest,
                                writes->slowest_bytes, writes->all, writes->panics, writes->panic_slowest,
                                writes->failed ? "; a write did not reach the file whole" : "");
    qry_lock(buffer);
    int placed = 0;
    qry_appended(buffer, "writer", strlen("writer"), account, (unsigned long long)length, QRY_ORDINARY, head->capacity,
                 &placed);
    qry_unlock(buffer);
    qry_range_written(buffer, &file, atomic_load(&head->written), atomic_load(&head->tail), state);
    const int closed = qry_file_closed(&file);
    const int failed = state->writes.failed;
    free(state);
    // the run dumped with its table at its front, in place of the .qry written as it ran
    const unsigned long long began = qry_nanoseconds();
    unsigned long long rows = 0ull;
    const int tabled = closed && qry_table_dumped(buffer, &rows);
    atomic_store(&head->writing, 0u);
    printf("  %s: %s", buffer->path, account);
    if (tabled)
    {
        printf("  %s: dumped with its table of %llu rows in %.3f ms\n", buffer->path, rows,
               (double)(qry_nanoseconds() - began) / 1000000.0);
    }
    else
    {
        printf("  %s: not dumped with its table; it stands as written as the run ran\n", buffer->path);
    }
    return closed && !failed;
}

int qry_stop(QryBuffer *buffer, unsigned int milliseconds)
{
    atomic_store(&buffer->head->stopping, 1u);
    qry_wake(buffer);
    const unsigned long long began = qry_milliseconds();
    while (atomic_load(&buffer->head->writing) != 0u)
    {
        if ((qry_milliseconds() - began) > milliseconds)
        {
            return 0;
        }
        qry_sleep_millisecond();
    }
    return 1;
}
