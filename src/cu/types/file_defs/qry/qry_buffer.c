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

#include <stdarg.h>
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
#include <sched.h>
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

// a frame's most bytes past its blob's: its head, the longest name, the bytes' start rounded up to the alignment and
// its last word rounded up to it again
#define QRY_FRAME_MOST (sizeof(QryFrameHead) + QRY_NAME_LONGEST + (2ull * QRY_FRAME_ALIGN) + 8ull)

_Static_assert((QRY_FRAME_ALIGN >= 8ull) && ((QRY_FRAME_ALIGN & (QRY_FRAME_ALIGN - 1ull)) == 0ull),
               "QRY_FRAME_ALIGN must be a power of two of at least 8 bytes");
_Static_assert(sizeof(QryFrameHead) == 56u, "QryFrameHead must be the 56 bytes qry.h gives a frame's head");
_Static_assert((QRY_WRITE_CALL_BYTES != 0ull) && (QRY_WRITE_CALL_BYTES <= (1ull << 30u)),
               "QRY_WRITE_CALL_BYTES must be a count of bytes past 0 that every system's count of a write holds");

// a panic's or a critical blob's place in the buffer, its framed bytes, its sequence and its priority
typedef struct
{
    unsigned long long at;
    unsigned long long size;
    unsigned long long sequence;
    unsigned long long priority;
} QrySpan;

// a name's slot in the index: the name's hash, and the place of its latest blob's frame plus 1, 0 where empty
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

// A member's ring, in the shared memory: the process that holds it, 0 where none does; the bytes the writer has taken
// from it and the bytes its member has added to it, each counted from the run's start, a place in the ring the count's
// remainder; and the sequence of the last blob the writer moved out of it. Only the member moves `tail` and only the
// writer moves `head`
typedef struct
{
    _Atomic unsigned long long owner;
    _Atomic unsigned long long head;
    _Atomic unsigned long long tail;
    _Atomic unsigned long long moved;
    unsigned long long unused[4];
} QryMember;

// A blob in a member's ring: its whole bytes from its head through its bytes, a count of eight; the time it was
// handed; its bytes' count; its name's length; and its marks. Its name follows, then its bytes. Where a blob would not
// fit before the ring's end, the stretch it leaves there is one whole word, its count, and no blob
typedef struct
{
    unsigned long long whole;
    unsigned long long nanoseconds;
    unsigned long long size;
    unsigned int name_length;
    unsigned int marks;
} QryRingBlob;

// the bytes the members' heads take, a whole count of 64 KiB
#define QRY_MEMBERS_BYTES ((((unsigned long long)QRY_MEMBERS * 64ull) + 65535ull) & ~65535ull)

_Static_assert(sizeof(QryMember) == 64u, "QryMember must be 64 bytes, a member's head on a line of its own");
_Static_assert(sizeof(QryRingBlob) == 32u, "QryRingBlob must be the 32 bytes a ring's blob opens with");
_Static_assert((QRY_MEMBERS != 0u) && (QRY_MEMBER_BYTES >= 65536ull) &&
                   ((QRY_MEMBER_BYTES & (QRY_MEMBER_BYTES - 1ull)) == 0ull) && ((QRY_MEMBER_BYTES % 65536ull) == 0ull),
               "QRY_MEMBERS must be past 0 and QRY_MEMBER_BYTES a power of two of at least 64 KiB");
_Static_assert(sizeof(QryRow) == 64u, "QryRow must be the 64 bytes qry.h gives a row");
_Static_assert(sizeof(QryTableHead) == 64u, "QryTableHead must be the 64 bytes qry.h gives the table's head");
_Static_assert(((QRY_TABLE_ROWS * 64ull) % 65536ull) == 0ull, "QRY_TABLE_ROWS must fill whole 64 KiB of rows");
_Static_assert((QRY_INDEX_SLOTS & (QRY_INDEX_SLOTS - 1ull)) == 0ull, "QRY_INDEX_SLOTS must be a power of two");
_Static_assert((QRY_COMMIT_BYTES % 65536ull) == 0ull, "QRY_COMMIT_BYTES must be a whole count of 64 KiB");
_Static_assert(QRY_SLICE_BYTES != 0ull, "QRY_SLICE_BYTES must be a count of bytes past 0");

// The process's own view of the buffer: besides the shared memory's parts, the ring this process holds as a member,
// QRY_MEMBERS where it holds none; the flag its threads take turns handing by; and of the last blob it handed, whether
// it went into the ring, and where it went in at the tail, its sequence
struct QryBuffer
{
    char path[1024];
    QryHead *head;
    QrySlot *slot;
    QryMember *member;
    QryRow *row;
    unsigned char *ring;
    unsigned char *data;
    int made;
    unsigned int own;
    atomic_flag adding;
    int last_ringed;
    unsigned long long last_placed;
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

// this process's ring moved and let go; a buffer kept among those let go as the process ends, or dropped from them; and
// this process's ring moved before it reads (each below, with the rings)
static void qry_member_released(QryBuffer *buffer);
static void qry_held_kept(QryBuffer *buffer, int kept);
static void qry_own_moved(QryBuffer *buffer);

// the bytes the whole view takes: the head, the index, the members' heads, the table, the members' rings and the blobs
static unsigned long long qry_view_bytes(void)
{
    return (unsigned long long)QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES) + QRY_MEMBERS_BYTES +
           (QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow)) +
           ((unsigned long long)QRY_MEMBERS * QRY_MEMBER_BYTES) + QRY_BUFFER_BYTES;
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
        QRY_CRITICAL_ROOM, QRY_CRITICAL_SPANS, QRY_TABLE_ROWS,  (unsigned long long)sizeof(QryHead),
        QRY_MEMBERS,       QRY_MEMBER_BYTES};
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

// A blob at `at` read by its frame, every byte of it before `end`: its name's place and length, its bytes' place and
// count, its sequence, the time it was handed and its priority, and where the next frame begins. 1, or 0 where the
// frame is none qry.h gives, or its last word is not its sequence
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
static unsigned long long qry_aligned(unsigned long long count)
{
    return (count + QRY_FRAME_ALIGN - 1ull) & ~(QRY_FRAME_ALIGN - 1ull);
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

// One blob framed and put at the tail under the lock, its sequence the next, its head bearing `marks` (qry.h) and the
// time it was handed, `handed_at`, or now where that is 0. It takes room up to `room` bytes of the buffer. A panic's or
// a critical blob's place is kept for the writer and its sequence made the latest handed before the tail passes it,
// and a panic opens the line where none has. Its sequence, with `placed` 1 where it went in, or 0 where it did not fit
static unsigned long long qry_appended(QryBuffer *buffer, const char *name, size_t length, const void *bytes,
                                       unsigned long long size, unsigned int priority, unsigned int marks,
                                       unsigned long long room, unsigned long long handed_at, int *placed)
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
        unsigned long long none = 0ull;
        if (priority == QRY_PANIC)
        {
            atomic_compare_exchange_strong(&head->panicked, &none, sequence);
        }
    }
    // its row of the table, where the table holds one more: the name's hash, the places of its name and bytes in the
    // buffer, and what its frame's head says
    const unsigned long long rows = atomic_load(&head->rows);
    const int rowed = (head->table_full == 0ull) && qry_rows_committed(buffer, rows + 1ull);
    if (rowed)
    {
        // a name and a priority are each far below what an unsigned int counts
        // its CRC is the dump's to take, off the hand's way
        buffer->row[rows] = (QryRow){
            qry_hash(name, length), sequence, handed_at, 0ull, tail + sizeof(QryFrameHead), tail + bytes_at, size,
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
    qry_appended(buffer, QRY_DUMPED, strlen(QRY_DUMPED), said, (unsigned long long)length, QRY_CRITICAL, 0u,
                 head->capacity, 0ull, &placed);
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
        buffer->own = QRY_MEMBERS;
        atomic_flag_clear(&buffer->adding);
#if !defined(_WIN32)
        buffer->memory = -1;
#endif
    }
    return buffer;
}

// the head, the index, the members' heads, the table, the members' rings and the blobs found in a view starting at
// `view`
static void qry_buffer_placed(QryBuffer *buffer, unsigned char *view)
{
    unsigned char *at = view;
    buffer->head = (QryHead *)at;
    at += QRY_HEAD_BYTES;
    buffer->slot = (QrySlot *)at;
    at += QRY_INDEX_SLOTS * QRY_SLOT_BYTES;
    buffer->member = (QryMember *)at;
    at += QRY_MEMBERS_BYTES;
    buffer->row = (QryRow *)at;
    at += QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow);
    buffer->ring = at;
    at += (unsigned long long)QRY_MEMBERS * QRY_MEMBER_BYTES;
    buffer->data = at;
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
    const unsigned long long fixed =
        (unsigned long long)QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES) + QRY_MEMBERS_BYTES;
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

int qry_traced(const char *path, unsigned int priority)
{
    const char *const chosen = getenv(QRY_TRACE_ENVIRONMENT);
    if ((priority < QRY_ORDINARY) || (chosen == NULL) || (chosen[0] == '\0'))
    {
        return 1;
    }
    const size_t length = strlen(path);
    int named = 0;
    int kept = 0;
    int dropped = 0;
    const char *entry = chosen;
    while (*entry != '\0')
    {
        const size_t entry_length = strcspn(entry, ",");
        const int out = (entry[0] == '-');
        const char *const name = entry + (out ? 1 : 0);
        const size_t name_length = entry_length - (out ? 1u : 0u);
        const int same = (name_length == length) && (memcmp(name, path, length) == 0);
        if ((entry_length == 1u) && (entry[0] == '0'))
        {
            return 0;
        }
        dropped |= out && same;
        named |= !out && (name_length != 0u);
        kept |= !out && same;
        entry += entry_length + ((entry[entry_length] == ',') ? 1u : 0u);
    }
    return !dropped && (!named || kept);
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
    // the ring this process holds moved and let go before the view goes
    if ((buffer->head != NULL) && (buffer->own != QRY_MEMBERS))
    {
        qry_member_released(buffer);
        qry_held_kept(buffer, 0);
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

// the hand of qry_hand, its frame bearing `marks`
static unsigned long long qry_handed(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                                     unsigned int priority, unsigned int marks);

// `path` made whole and plain into `plain`, which holds `room`: `/` between its folders, a drive the shell writes as
// `/d/` written `D:/`, and a path from the folder the process stands in put after that folder. 1, or 0 where it climbs
// out through `..` or does not fit
static int qry_path_plain(const char *path, char *plain, size_t room)
{
    char given[1024];
    const int length = snprintf(given, sizeof(given), "%s", path);
    if ((length < 0) || ((size_t)length >= sizeof(given)) || (strstr(given, "..") != NULL))
    {
        return 0;
    }
    for (char *at = given; *at != '\0'; at += 1)
    {
        *at = (*at == '\\') ? '/' : *at;
    }
    const char *rest = given;
    while ((rest[0] == '.') && (rest[1] == '/'))
    {
        rest += 2;
    }
    int written = 0;
#if defined(_WIN32)
    const int lettered = ((rest[1] >= 'a') && (rest[1] <= 'z')) || ((rest[1] >= 'A') && (rest[1] <= 'Z'));
    const int drive = ((rest[0] >= 'a') && (rest[0] <= 'z')) || ((rest[0] >= 'A') && (rest[0] <= 'Z'));
    if ((rest[0] == '/') && lettered && (rest[2] == '/'))
    {
        // the shell's `/d/x`, the drive's letter its first folder
        written = snprintf(plain, room, "%c:%s", (char)((rest[1] >= 'a') ? (rest[1] - 'a' + 'A') : rest[1]), rest + 2);
    }
    else if (drive && (rest[1] == ':'))
    {
        written = snprintf(plain, room, "%c%s", (char)((rest[0] >= 'a') ? (rest[0] - 'a' + 'A') : rest[0]), rest + 1);
    }
    else
    {
        char here[1024];
        if (GetCurrentDirectoryA(sizeof(here), here) == 0u)
        {
            return 0;
        }
        for (char *at = here; *at != '\0'; at += 1)
        {
            *at = (*at == '\\') ? '/' : *at;
        }
        written = snprintf(plain, room, "%s/%s", here, rest);
    }
#else
    if (rest[0] == '/')
    {
        written = snprintf(plain, room, "%s", rest);
    }
    else
    {
        char here[1024];
        if (getcwd(here, sizeof(here)) == NULL)
        {
            return 0;
        }
        written = snprintf(plain, room, "%s/%s", here, rest);
    }
#endif
    // a path that does not fit its room names no file
    return (written > 0) && ((size_t)written < room);
}

int qry_file_named(const char *path, char *name, size_t room)
{
    const char *const top_given = getenv(QRY_TOP_ENVIRONMENT);
    char top[1024];
    char plain[1024];
    if ((top_given == NULL) || !qry_path_plain(top_given, top, sizeof(top)) ||
        !qry_path_plain(path, plain, sizeof(plain)))
    {
        return 0;
    }
    size_t top_length = strlen(top);
    while ((top_length != 0u) && (top[top_length - 1u] == '/'))
    {
        top_length -= 1u;
    }
#if defined(_WIN32)
    const int under = (_strnicmp(plain, top, top_length) == 0);
#else
    const int under = (strncmp(plain, top, top_length) == 0);
#endif
    if (!under || (plain[top_length] != '/') || (plain[top_length + 1u] == '\0'))
    {
        return 0;
    }
    const int written = snprintf(name, room, "%s", plain + top_length + 1u);
    // a name that does not fit its room is none
    return (written > 0) && ((size_t)written < room);
}

// the current file the name `name`, `length` bytes of it, gives, its path from the top of the tree, into `path`, which
// holds `room`: 1, or 0 where no top is named, the name climbs out of it, or the path does not fit
static int qry_file_placed(const char *name, size_t length, char *path, size_t room)
{
    const char *const top_given = getenv(QRY_TOP_ENVIRONMENT);
    char top[1024];
    if ((top_given == NULL) || !qry_path_plain(top_given, top, sizeof(top)) || (length == 0u) ||
        (length > QRY_NAME_LONGEST) || (name[0] == '/') || (memchr(name, ':', length) != NULL))
    {
        return 0;
    }
    char named[QRY_NAME_LONGEST + 1u];
    memcpy(named, name, length);
    named[length] = '\0';
    if (strstr(named, "..") != NULL)
    {
        return 0;
    }
    const int written = snprintf(path, room, "%s/%s", top, named);
    // a path that does not fit its room names no file
    return (written > 0) && ((size_t)written < room);
}

unsigned long long qry_filed_by(QryBuffer *buffer, const char *path, const void *bytes, unsigned long long size,
                                QryWho who)
{
    char name[QRY_NAME_LONGEST + 1u];
    char placed[1400];
    if ((buffer == NULL) || ((bytes == NULL) && (size != 0ull)) || !qry_file_named(path, name, sizeof(name)) ||
        !qry_file_placed(name, strlen(name), placed, sizeof(placed)))
    {
        return 0ull;
    }
    // in the .qry on disk once the hand returns; in the file's place once the run ends (qry_files_put)
    const unsigned long long sequence = qry_handed(buffer, name, bytes, size, QRY_CRITICAL, QRY_MARK_FILED);
    if (sequence == 0ull)
    {
        return 0ull;
    }
    // the write's sequence and its who, after it in the run
    unsigned long long filed[3] = {sequence, 0ull, 0ull};
    qry_who_words(who, &filed[1]);
    return (qry_hand(buffer, QRY_FILED_CALL, filed, sizeof(filed), QRY_CRITICAL) != 0ull) ? sequence : 0ull;
}

// the who table, read once a process: each number's source, its path from the top of the tree, a number the table does
// not give an empty path; and the program's own number once it is found
#define QRY_WHO_MOST 65536u
static char *s_who_source[QRY_WHO_MOST];
static unsigned int s_who_highest;
static unsigned int s_who_program;
static atomic_int s_who_state;

// the top of the tree into `top`, which holds `room`: the one QRY_TOP_ENVIRONMENT names, or where it names none, the
// tree this file was built from. 1, or 0 where neither is had
static int qry_who_top(char *top, size_t room)
{
    const char *const given = getenv(QRY_TOP_ENVIRONMENT);
    if (given != NULL)
    {
        return qry_path_plain(given, top, room);
    }
    static const char s_own[] = "src/cu/types/file_defs/qry/qry_buffer.c";
    char plain[1024];
    if (!qry_path_plain(__FILE__, plain, sizeof(plain)))
    {
        return 0;
    }
    const size_t length = strlen(plain);
    const size_t own = strlen(s_own);
    if ((length <= own) || (strcmp(plain + length - own, s_own) != 0))
    {
        return 0;
    }
    // the top stands before the file's own path and the `/` that opens it, which an int counts
    const int written = snprintf(top, room, "%.*s", (int)(length - own - 1u), plain);
    return (written > 0) && ((size_t)written < room);
}

// the path of `file` from the top of the tree into `name`, which holds `room`: 1, or 0 where it stands outside it
static int qry_who_source_named(const char *file, char *name, size_t room)
{
    char top[1024];
    char plain[1024];
    if (!qry_who_top(top, sizeof(top)) || !qry_path_plain(file, plain, sizeof(plain)))
    {
        return 0;
    }
    const size_t length = strlen(top);
#if defined(_WIN32)
    const int under = (_strnicmp(plain, top, length) == 0);
#else
    const int under = (strncmp(plain, top, length) == 0);
#endif
    if (!under || (plain[length] != '/'))
    {
        return 0;
    }
    const int written = snprintf(name, room, "%s", plain + length + 1u);
    return (written > 0) && ((size_t)written < room);
}

// the number the who table gives the source at `name`, its path from the top of the tree, 0 where it gives none
static unsigned int qry_who_number(const char *name)
{
    for (unsigned int number = 1u; number <= s_who_highest; number += 1u)
    {
        if ((s_who_source[number] != NULL) && (strcmp(s_who_source[number], name) == 0))
        {
            return number;
        }
    }
    return 0u;
}

// the number of the program's own source: the first the who table gives, a header after every other, whose name
// without its kind is the program's name without its kind; 0 where none is
static unsigned int qry_who_program_number(void)
{
    char path[1024] = "";
#if defined(_WIN32)
    const DWORD length = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
    path[(length < sizeof(path)) ? length : (sizeof(path) - 1u)] = '\0';
#else
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1u);
    // a length readlink gives past -1 is the count it read, at most the room it was given
    path[(length > 0) ? (size_t)length : 0u] = '\0';
#endif
    const char *const slash = strrchr(path, '/');
    const char *const back = strrchr(path, '\\');
    const char *base = (back > slash) ? (back + 1) : ((slash != NULL) ? (slash + 1) : path);
    const size_t stem = strcspn(base, ".");
    unsigned int header = 0u;
    for (unsigned int number = 1u; number <= s_who_highest; number += 1u)
    {
        const char *const source = s_who_source[number];
        if (source == NULL)
        {
            continue;
        }
        const char *const own = (strrchr(source, '/') != NULL) ? (strrchr(source, '/') + 1) : source;
        if ((strncmp(own, base, stem) != 0) || (own[stem] != '.'))
        {
            continue;
        }
        const char *const kind = own + stem;
        if ((strcmp(kind, ".h") != 0) && (strcmp(kind, ".cuh") != 0) && (strcmp(kind, ".hpp") != 0))
        {
            return number;
        }
        header = (header == 0u) ? number : header;
    }
    return header;
}

// the who table read, once a process, as the run holds it: every number and its source
static void qry_who_table_read(void)
{
    int unread = 0;
    if (!atomic_compare_exchange_strong(&s_who_state, &unread, 1))
    {
        while (atomic_load(&s_who_state) != 2)
        {
            qry_sleep_millisecond();
        }
        return;
    }
    char top[1024];
    char path[1400];
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (qry_who_top(top, sizeof(top)) && (snprintf(path, sizeof(path), "%s/%s", top, QRY_WHO_TABLE) > 0) &&
        qry_file_read(qry_run(), path, &bytes, &size))
    {
        unsigned long long at = 0ull;
        char line[1200];
        while (qry_line_next(bytes, size, &at, line, sizeof(line)))
        {
            line[strcspn(line, "\r\n")] = '\0';
            char *after = NULL;
            const unsigned long long number = strtoull(line, &after, 16);
            if ((after == line) || (*after != '\t') || (number == 0ull) || (number >= QRY_WHO_MOST))
            {
                continue;
            }
            const size_t length = strlen(after + 1);
            char *const source = (char *)malloc(length + 1u);
            if (source == NULL)
            {
                continue;
            }
            memcpy(source, after + 1, length + 1u);
            // a number below QRY_WHO_MOST is one an unsigned int holds
            free(s_who_source[number]);
            s_who_source[number] = source;
            s_who_highest = ((unsigned int)number > s_who_highest) ? (unsigned int)number : s_who_highest;
        }
        free(bytes);
    }
    s_who_program = qry_who_program_number();
    atomic_store(&s_who_state, 2);
}

// the last sources asked, by the pointer __FILE__ gives, each a source's own and the same at every ask, and the number
// the who table gives each; one process's, held under the lock beside them, which every thread of it takes
#define QRY_WHO_ASKED 64u
static const char *s_who_asked[QRY_WHO_ASKED];
static unsigned int s_who_asked_number[QRY_WHO_ASKED];
static unsigned int s_who_asked_next;
static atomic_flag s_who_asked_lock = ATOMIC_FLAG_INIT;

QryWho qry_who(const char *file, unsigned int line)
{
    qry_who_table_read();
    QryWho who = {0u, line, s_who_program, qry_nanoseconds()};
    while (atomic_flag_test_and_set(&s_who_asked_lock))
    {
    }
    int found = 0;
    for (unsigned int at = 0u; (at < QRY_WHO_ASKED) && !found; at += 1u)
    {
        found = (s_who_asked[at] == file);
        who.source = found ? s_who_asked_number[at] : who.source;
    }
    atomic_flag_clear(&s_who_asked_lock);
    if (found)
    {
        return who;
    }
    char name[1024];
    who.source = qry_who_source_named(file, name, sizeof(name)) ? qry_who_number(name) : 0u;
    while (atomic_flag_test_and_set(&s_who_asked_lock))
    {
    }
    s_who_asked[s_who_asked_next] = file;
    s_who_asked_number[s_who_asked_next] = who.source;
    s_who_asked_next = (s_who_asked_next + 1u) % QRY_WHO_ASKED;
    atomic_flag_clear(&s_who_asked_lock);
    return who;
}

void qry_who_words(QryWho who, unsigned long long *words)
{
    words[0] = (((unsigned long long)who.source & 0xffffull) << 48u) |
               (((unsigned long long)who.line & 0xffffull) << 32u) |
               (((unsigned long long)who.program & 0xffffull) << 16u);
    words[1] = who.nanoseconds;
}

QryWho qry_who_of_words(const unsigned long long *words)
{
    // each field is sixteen bits of the first word, which an unsigned int holds
    const QryWho who = {(unsigned int)((words[0] >> 48u) & 0xffffull), (unsigned int)((words[0] >> 32u) & 0xffffull),
                        (unsigned int)((words[0] >> 16u) & 0xffffull), words[1]};
    return who;
}

void qry_who_code(QryWho who, char *code, size_t room)
{
    unsigned long long words[2];
    qry_who_words(who, words);
    snprintf(code, room, "%016llx%016llx", words[0], words[1]);
}

int qry_who_read(const char *code, QryWho *who)
{
    if (strspn(code, "0123456789abcdefABCDEF") != QRY_WHO_CODE_LETTERS)
    {
        return 0;
    }
    char half[17];
    unsigned long long words[2];
    memcpy(half, code, 16u);
    half[16] = '\0';
    words[0] = strtoull(half, NULL, 16);
    memcpy(half, code + 16, 16u);
    words[1] = strtoull(half, NULL, 16);
    *who = qry_who_of_words(words);
    return 1;
}

// the civil date of `days` since 1970-01-01 into `year`, `month` and `day` (the proleptic Gregorian calendar)
static void qry_civil(long long days, long long *year, unsigned int *month, unsigned int *day)
{
    days += 719468ll;
    const long long era = ((days >= 0ll) ? days : (days - 146096ll)) / 146097ll;
    // the day of the era is at least 0 and below 146097, which an unsigned int holds
    const unsigned int of_era = (unsigned int)(days - (era * 146097ll));
    const unsigned int year_of_era = (of_era - (of_era / 1460u) + (of_era / 36524u) - (of_era / 146096u)) / 365u;
    const unsigned int of_year = of_era - ((365u * year_of_era) + (year_of_era / 4u) - (year_of_era / 100u));
    const unsigned int shifted = ((5u * of_year) + 2u) / 153u;
    *day = of_year - (((153u * shifted) + 2u) / 5u) + 1u;
    *month = (shifted < 10u) ? (shifted + 3u) : (shifted - 9u);
    *year = (long long)year_of_era + (era * 400ll) + ((*month <= 2u) ? 1ll : 0ll);
}

int qry_who_named(QryWho who, char *text, size_t room)
{
    qry_who_table_read();
    const char *const source =
        ((who.source != 0u) && (s_who_source[who.source] != NULL)) ? s_who_source[who.source] : "-";
    const char *const program =
        ((who.program != 0u) && (s_who_source[who.program] != NULL)) ? s_who_source[who.program] : "-";
    const unsigned long long seconds = who.nanoseconds / 1000000000ull;
    long long year = 0ll;
    unsigned int month = 0u;
    unsigned int day = 0u;
    // the days since 1970 are far below what a long long counts
    qry_civil((long long)(seconds / 86400ull), &year, &month, &day);
    const unsigned long long of_day = seconds % 86400ull;
    const int written = snprintf(text, room, "%s:%u %s %04lld-%02u-%02uT%02llu:%02llu:%02llu.%09lluZ", source, who.line,
                                 program, year, month, day, of_day / 3600ull, (of_day / 60ull) % 60ull, of_day % 60ull,
                                 who.nanoseconds % 1000000000ull);
    // a text that does not fit its room is none
    return (written > 0) && ((size_t)written < room);
}

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

// One blob put in at the buffer's tail under the lock, a dump said before it where one waits and it does not fit
// (qry_dumped_said), and counted as dumped where it is ordinary and does not fit: its sequence, with `placed` 1 where
// it went in
static unsigned long long qry_placed(QryBuffer *buffer, const char *name, size_t length, const void *bytes,
                                     unsigned long long size, unsigned int priority, unsigned int marks,
                                     unsigned long long handed_at, int *placed)
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
        qry_appended(buffer, name, length, bytes, size, priority, marks, room, handed_at, placed);
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

// this process's number, as the system gives it
static unsigned long long qry_process_self(void)
{
#if defined(_WIN32)
    return (unsigned long long)GetCurrentProcessId();
#else
    return (unsigned long long)getpid();
#endif
}

// 1 while the process `number` runs, and where the system will not say
static int qry_process_alive(unsigned long long number)
{
#if defined(_WIN32)
    const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)number);
    if (process == NULL)
    {
        return GetLastError() != ERROR_INVALID_PARAMETER;
    }
    const int alive = (WaitForSingleObject(process, 0u) == WAIT_TIMEOUT);
    CloseHandle(process);
    return alive;
#else
    return (kill((pid_t)number, 0) == 0) || (errno == EPERM);
#endif
}

// one wait while another is moved: the thread's turn given up at first, and a millisecond's sleep past
// QRY_PAUSE_YIELDS of them, `*waited` counting the waits. A sleep is the system timer's step, as much as 15 ms on
// Windows, and a move takes far less: the yields cover the moves a writer makes under load
#define QRY_PAUSE_YIELDS 20000u
static void qry_paused(unsigned int *waited)
{
    if (*waited < QRY_PAUSE_YIELDS)
    {
        *waited += 1u;
#if defined(_WIN32)
        SwitchToThread();
#else
        sched_yield();
#endif
        return;
    }
    qry_sleep_millisecond();
}

// the buffers whose rings this process holds, each let go as it ends
#define QRY_HELD_BUFFERS 8u
static QryBuffer *s_held_buffers[QRY_HELD_BUFFERS];
static atomic_flag s_held_turn = ATOMIC_FLAG_INIT;
static atomic_int s_held_ending = 0;

// every ring this process holds moved and let go, as the process ends
static void qry_held_ended(void)
{
    for (unsigned int at = 0u; at < QRY_HELD_BUFFERS; at += 1u)
    {
        if (s_held_buffers[at] != NULL)
        {
            qry_member_released(s_held_buffers[at]);
        }
    }
}

// `buffer` kept among those whose rings are let go as the process ends, or dropped from them where `kept` is 0
static void qry_held_kept(QryBuffer *buffer, int kept)
{
    while (atomic_flag_test_and_set(&s_held_turn))
    {
    }
    int registered = 0;
    for (unsigned int at = 0u; (at < QRY_HELD_BUFFERS) && !registered; at += 1u)
    {
        if (kept ? (s_held_buffers[at] == NULL) : (s_held_buffers[at] == buffer))
        {
            s_held_buffers[at] = kept ? buffer : NULL;
            registered = 1;
        }
    }
    atomic_flag_clear(&s_held_turn);
    int none = 0;
    if (kept && atomic_compare_exchange_strong(&s_held_ending, &none, 1))
    {
        atexit(qry_held_ended);
    }
}

// The ring this process holds in `buffer`, taken the first time it hands an ordinary blob: a free one, or one whose
// process has ended with nothing left in it, its memory given. Its place, or QRY_MEMBERS where none can be had. Taken
// with the process's own turn at adding held (buffer->adding)
static unsigned int qry_member_claimed(QryBuffer *buffer)
{
    if (buffer->own != QRY_MEMBERS)
    {
        return buffer->own;
    }
    const unsigned long long self = qry_process_self();
    for (unsigned int pass = 0u; pass < 2u; pass += 1u)
    {
        for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
        {
            QryMember *const member = &buffer->member[place];
            unsigned long long owner = atomic_load(&member->owner);
            // the second pass takes back a ring whose process ended with nothing left in it
            const int free_ring = (owner == 0ull) || ((pass == 1u) && (owner != self) &&
                                                      (atomic_load(&member->head) == atomic_load(&member->tail)) &&
                                                      !qry_process_alive(owner));
            if (!free_ring || !atomic_compare_exchange_strong(&member->owner, &owner, self))
            {
                continue;
            }
#if defined(_WIN32)
            // a ring is QRY_MEMBER_BYTES, which a size_t counts
            if (VirtualAlloc(buffer->ring + ((unsigned long long)place * QRY_MEMBER_BYTES), (size_t)QRY_MEMBER_BYTES,
                             MEM_COMMIT, PAGE_READWRITE) == NULL)
            {
                atomic_store(&member->owner, 0ull);
                return QRY_MEMBERS;
            }
#endif
            buffer->own = place;
            qry_held_kept(buffer, 1);
            return place;
        }
    }
    return QRY_MEMBERS;
}

// the bytes a blob of a name of `length` and `size` bytes takes in a ring, a count of eight
static unsigned long long qry_ring_whole(size_t length, unsigned long long size)
{
    return ((unsigned long long)sizeof(QryRingBlob) + (unsigned long long)length + size + 7ull) & ~7ull;
}

// `size` bytes at `bytes` of `name` added at the tail of this process's ring `own`, made whole before the tail moves
// past them, a ring that holds no room for them first waited on: 1, or 0 where the writer stopped while it waited.
// Taken with the process's own turn at adding held (buffer->adding)
static int qry_member_added(QryBuffer *buffer, unsigned int own, const char *name, size_t length, const void *bytes,
                            unsigned long long size, unsigned int marks)
{
    QryMember *const member = &buffer->member[own];
    unsigned char *const ring = buffer->ring + ((unsigned long long)own * QRY_MEMBER_BYTES);
    const unsigned long long whole = qry_ring_whole(length, size);
    const unsigned long long tail = atomic_load(&member->tail);
    const unsigned long long place = tail & (QRY_MEMBER_BYTES - 1ull);
    // a blob that does not fit before the ring's end starts again at its start, past the stretch it leaves
    const unsigned long long left = ((QRY_MEMBER_BYTES - place) < whole) ? (QRY_MEMBER_BYTES - place) : 0ull;
    unsigned int waited = 0u;
    while ((tail + left + whole - atomic_load(&member->head)) > QRY_MEMBER_BYTES)
    {
        if (!qry_writer_alive(buffer))
        {
            return 0;
        }
        qry_wake(buffer);
        qry_paused(&waited);
    }
    // the stretch left marked as no blob: its count, and where it holds a blob's head, a head with no name
    if (left >= sizeof(QryRingBlob))
    {
        const QryRingBlob none = {left, 0ull, 0ull, 0u, 0u};
        memcpy(ring + place, &none, sizeof(none));
    }
    else if (left != 0ull)
    {
        memcpy(ring + place, &left, sizeof(left));
    }
    unsigned char *const at = ring + ((tail + left) & (QRY_MEMBER_BYTES - 1ull));
    // a name is at most QRY_NAME_LONGEST bytes, which an unsigned int counts, and a blob that fits a ring is counted by
    // a size_t
    const QryRingBlob opened = {whole, qry_nanoseconds(), size, (unsigned int)length, marks};
    memcpy(at, &opened, sizeof(opened));
    memcpy(at + sizeof(opened), name, length);
    if (size != 0ull)
    {
        memcpy(at + sizeof(opened) + length, bytes, (size_t)size);
    }
    const int was_empty = (atomic_load(&member->head) == tail);
    atomic_store(&member->tail, tail + left + whole);
    // the writer told once a ring holds something again, never for each blob
    if (was_empty)
    {
        qry_wake(buffer);
    }
    return 1;
}

// Every member's ring moved into the buffer, each in its turn in the order of the rings: on a ring's turn, every blob
// from its head to its tail as the turn reads it, under the lock a panic's or a critical hand takes, each given its
// sequence as it goes in; a ring that holds nothing has a turn that does nothing. The ring's head is moved past what
// was taken once all of it is in, and the sequence of the last blob kept as the ring's. 1 where any was moved
static int qry_members_moved(QryBuffer *buffer)
{
    int moved = 0;
    for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
    {
        QryMember *const member = &buffer->member[place];
        const unsigned long long head = atomic_load(&member->head);
        const unsigned long long tail = atomic_load(&member->tail);
        if (head == tail)
        {
            continue;
        }
        const unsigned char *const ring = buffer->ring + ((unsigned long long)place * QRY_MEMBER_BYTES);
        unsigned long long last = 0ull;
        qry_lock(buffer);
        unsigned long long at = head;
        while (at < tail)
        {
            const unsigned char *const opened_at = ring + (at & (QRY_MEMBER_BYTES - 1ull));
            QryRingBlob opened;
            memcpy(&opened.whole, opened_at, sizeof(opened.whole));
            // a count that is no blob's ends the turn's reading, the ring taken whole
            if ((opened.whole < sizeof(opened.whole)) || ((opened.whole % 8ull) != 0ull) ||
                (opened.whole > (tail - at)))
            {
                break;
            }
            if (opened.whole >= sizeof(opened))
            {
                memcpy(&opened, opened_at, sizeof(opened));
            }
            // the stretch a blob left before the ring's end
            if ((opened.whole < sizeof(opened)) || (opened.name_length == 0u) ||
                (qry_ring_whole(opened.name_length, opened.size) != opened.whole))
            {
                at += opened.whole;
                continue;
            }
            int placed = 0;
            const unsigned long long sequence =
                qry_placed(buffer, (const char *)(opened_at + sizeof(opened)), (size_t)opened.name_length,
                           opened_at + sizeof(opened) + opened.name_length, opened.size, QRY_ORDINARY, opened.marks,
                           opened.nanoseconds, &placed);
            last = placed ? sequence : last;
            at += opened.whole;
            // the ring's room given back blob by blob, for a member waiting on it to go on
            if (last != 0ull)
            {
                atomic_store(&member->moved, last);
            }
            atomic_store(&member->head, at);
        }
        qry_unlock(buffer);
        atomic_store(&member->head, tail);
        moved = 1;
    }
    return moved;
}

// 1 where every member's ring holds nothing
static int qry_members_empty(QryBuffer *buffer)
{
    int empty = 1;
    for (unsigned int place = 0u; (place < QRY_MEMBERS) && empty; place += 1u)
    {
        empty = (atomic_load(&buffer->member[place].head) == atomic_load(&buffer->member[place].tail));
    }
    return empty;
}

unsigned long long qry_moved(QryBuffer *buffer)
{
    if (buffer == NULL)
    {
        return 0ull;
    }
    if (buffer->own == QRY_MEMBERS)
    {
        return buffer->last_placed;
    }
    QryMember *const member = &buffer->member[buffer->own];
    const unsigned long long tail = atomic_load(&member->tail);
    unsigned int waited = 0u;
    if (atomic_load(&member->head) < tail)
    {
        qry_wake(buffer);
    }
    while (atomic_load(&member->head) < tail)
    {
        if (!qry_writer_alive(buffer))
        {
            return 0ull;
        }
        qry_paused(&waited);
    }
    return buffer->last_ringed ? atomic_load(&member->moved) : buffer->last_placed;
}

int qry_settled(QryBuffer *buffer)
{
    if (buffer == NULL)
    {
        return 1;
    }
    unsigned long long tails[QRY_MEMBERS];
    for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
    {
        tails[place] = atomic_load(&buffer->member[place].tail);
    }
    qry_wake(buffer);
    for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
    {
        unsigned int waited = 0u;
        while (atomic_load(&buffer->member[place].head) < tails[place])
        {
            if (!qry_writer_alive(buffer))
            {
                return 0;
            }
            qry_paused(&waited);
        }
    }
    return 1;
}

// this process's ring moved into the buffer where it holds anything, before the process reads the buffer: a process
// reads what it handed itself
static void qry_own_moved(QryBuffer *buffer)
{
    if ((buffer != NULL) && (buffer->own != QRY_MEMBERS) &&
        (atomic_load(&buffer->member[buffer->own].head) != atomic_load(&buffer->member[buffer->own].tail)))
    {
        qry_moved(buffer);
    }
}

// this process's ring moved and let go, where it holds one
static void qry_member_released(QryBuffer *buffer)
{
    if (buffer->own == QRY_MEMBERS)
    {
        return;
    }
    qry_moved(buffer);
    atomic_store(&buffer->member[buffer->own].owner, 0ull);
    buffer->own = QRY_MEMBERS;
}

static unsigned long long qry_handed(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                                     unsigned int priority, unsigned int marks)
{
    size_t length = 0u;
    if ((buffer == NULL) || !qry_name_taken(name, &length) || ((bytes == NULL) && (size != 0ull)) ||
        (priority > QRY_ORDINARY))
    {
        return 0ull;
    }
    // an ordinary blob goes into this process's ring, where it holds one and the ring holds the blob within half of it.
    // The process that holds the writer has no turn to wait for: its blobs go in at the tail
    const int writer_here = (buffer->head->writer_process == qry_process_self());
    unsigned int waited = 0u;
    if (!writer_here && (priority == QRY_ORDINARY) && (marks == 0u) &&
        (qry_ring_whole(length, size) <= (QRY_MEMBER_BYTES / 2ull)))
    {
        while (atomic_flag_test_and_set(&buffer->adding))
        {
            qry_paused(&waited);
        }
        const unsigned int own = qry_member_claimed(buffer);
        const int added = (own != QRY_MEMBERS) && qry_member_added(buffer, own, name, length, bytes, size, marks);
        buffer->last_ringed = (own != QRY_MEMBERS) ? added : buffer->last_ringed;
        atomic_flag_clear(&buffer->adding);
        if (own != QRY_MEMBERS)
        {
            return added ? 1ull : 0ull;
        }
    }
    // an ordinary blob that goes in at the tail goes in after every blob this process handed before it
    if (priority == QRY_ORDINARY)
    {
        qry_moved(buffer);
    }
    QryHead *const head = buffer->head;
    qry_lock(buffer);
    const unsigned long long written = atomic_load(&head->written);
    const unsigned long long before = atomic_load(&head->tail) - written;
    int placed = 0;
    const unsigned long long sequence = qry_placed(buffer, name, length, bytes, size, priority, marks, 0ull, &placed);
    const unsigned long long after = atomic_load(&head->tail) - written;
    qry_unlock(buffer);
    if (placed)
    {
        while (atomic_flag_test_and_set(&buffer->adding))
        {
            qry_paused(&waited);
        }
        buffer->last_ringed = 0;
        buffer->last_placed = sequence;
        atomic_flag_clear(&buffer->adding);
    }
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
    qry_own_moved(buffer);
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

// the next place past `at` where a frame that reads whole begins, or `size` where none does: a frame starts on a word,
// and each word past `at` whose bytes are the frame magic is read as one
static unsigned long long qry_next_whole(const unsigned char *bytes, unsigned long long size, unsigned long long at)
{
    QryBlob blob;
    for (unsigned long long from = (at + 8ull) & ~7ull; (from < size) && ((size - from) >= sizeof(QryFrameHead));
         from += 8ull)
    {
        if ((memcmp(bytes + from, QRY_FRAME_MAGIC, 8u) == 0) && qry_blob_read(bytes, size, from, &blob))
        {
            return from;
        }
    }
    return size;
}

// The table a mapped .qry opens with (qry.h), its head copied to `table` and its rows found in place: 1, or 0 where the
// .qry opens with none, or with one whose rows or blobs do not lie inside it, and it is read by its frames
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
    // head's CRC, the root of every check past it, and rows that do not hold it leave the file read by its frames
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

// whether the blob `row` of a mapped .qry's table jumps to reads whole by its own frame, is the blob the row says, and
// holds the bytes the row's CRC was taken of: what a reader reads it reads checked, and a blob the file lost is told
// from one it holds
static int qry_row_whole(const QryMapped *mapped, const QryTableHead *table, const QryRow *row)
{
    QryBlob blob;
    const unsigned long long end = table->blobs_at + table->blobs_bytes;
    const unsigned long long opened = sizeof(QryFrameHead);
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
            snprintf(error, room, "%s: %llu bytes of %s whose frames do not read whole were stepped over", path, lost,
                     name);
        }
        return joined;
    }
    QryFiled *filed = NULL;
    size_t filed_count = 0u;
    size_t filed_room = 0u;
    unsigned long long skipped = 0ull;
    int whole = 1;
    // each blob read by its frame in place and taken where it is of `name`; a stretch no frame opens is stepped over to
    // the next frame that reads whole
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
        snprintf(error, room, "%s: %llu bytes no frame opens were stepped over", path, skipped);
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
            snprintf(error, room, "%s: %llu bytes of blobs whose frames do not read whole were stepped over", path, lost);
        }
        return 1;
    }
    // the frames counted, a name at most one a frame, and a table of twice as many slots: each frame read by its head
    // and stepped over whole, a stretch none opens stepped over to the next
    size_t slots = 64u;
    for (unsigned long long walk = 0ull; walk < mapped.size;)
    {
        QryBlob blob;
        if (qry_blob_read(mapped.bytes, mapped.size, walk, &blob))
        {
            slots += 2u;
            walk = blob.next;
        }
        else
        {
            walk = qry_next_whole(mapped.bytes, mapped.size, walk);
        }
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
        snprintf(error, room, "%s: %llu bytes no frame opens were stepped over", path, skipped);
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

// `size` bytes at `bytes` written through to the disk at `path`, anew: 1, or 0 where they were not written whole
static int qry_bytes_put(const char *path, const unsigned char *bytes, unsigned long long size)
{
#if defined(_WIN32)
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    unsigned long long done = 0ull;
    int whole = 1;
    while (whole && (done < size))
    {
        const unsigned long long piece = ((size - done) < QRY_WRITE_CALL_BYTES) ? (size - done) : QRY_WRITE_CALL_BYTES;
        DWORD wrote = 0u;
        // a piece is at most QRY_WRITE_CALL_BYTES, which a DWORD counts
        whole = WriteFile(file, bytes + done, (DWORD)piece, &wrote, NULL) && (wrote != 0u);
        done += wrote;
    }
    return (CloseHandle(file) != 0) && whole;
#else
    const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file < 0)
    {
        return 0;
    }
    unsigned long long done = 0ull;
    int whole = 1;
    while (whole && (done < size))
    {
        const unsigned long long piece = ((size - done) < QRY_WRITE_CALL_BYTES) ? (size - done) : QRY_WRITE_CALL_BYTES;
        // a piece is at most QRY_WRITE_CALL_BYTES, which a size_t counts
        const ssize_t wrote = write(file, bytes + done, (size_t)piece);
        whole = (wrote > 0) || ((wrote < 0) && (errno == EINTR));
        done += (wrote > 0) ? (unsigned long long)wrote : 0ull;
    }
    whole = whole && (fsync(file) == 0);
    return (close(file) == 0) && whole;
#endif
}

// The bytes of the frame at `at` in the buffer, a blob marked QRY_MARK_FILED, written in the place of the current file
// its name gives, its path from the top of the tree: beside it as `<file>.part` and then put in its place whole. 1, or
// 0 where they were not written whole
static int qry_file_put(QryBuffer *buffer, unsigned long long at)
{
    QryFrameHead opened;
    memcpy(&opened, buffer->data + at, sizeof(opened));
    const char *const name = (const char *)(buffer->data + at + sizeof(opened));
    const unsigned char *const bytes = buffer->data + at + opened.bytes_at;
    char placed[1400];
    char beside[1410];
    if (!qry_file_placed(name, (size_t)opened.name_length, placed, sizeof(placed)))
    {
        return 0;
    }
    snprintf(beside, sizeof(beside), "%s.part", placed);
    const int put = qry_bytes_put(beside, bytes, opened.size);
#if defined(_WIN32)
    const int moved = put && (MoveFileExA(beside, placed, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0);
#else
    const int moved = put && (rename(beside, placed) == 0);
#endif
    if (!moved)
    {
        remove(beside);
    }
    return moved;
}

// a blob marked QRY_MARK_FILED: its name's hash and place, its sequence, and its frame's place in the buffer
typedef struct
{
    unsigned long long hash;
    unsigned long long name_at;
    unsigned long long name_length;
    unsigned long long sequence;
    unsigned long long at;
} QryFileBlob;

// the order of two filed blobs: by their names, the hash first, and then by their sequences
static const unsigned char *s_file_blobs_data;
static int qry_file_blob_order(const void *left, const void *right)
{
    const QryFileBlob *const one = (const QryFileBlob *)left;
    const QryFileBlob *const two = (const QryFileBlob *)right;
    if (one->hash != two->hash)
    {
        return (one->hash < two->hash) ? -1 : 1;
    }
    const unsigned long long shorter = (one->name_length < two->name_length) ? one->name_length : two->name_length;
    // a name is at most QRY_NAME_LONGEST bytes, which a size_t counts
    const int named = memcmp(s_file_blobs_data + one->name_at, s_file_blobs_data + two->name_at, (size_t)shorter);
    if ((named != 0) || (one->name_length != two->name_length))
    {
        return (named != 0) ? named : ((one->name_length < two->name_length) ? -1 : 1);
    }
    return (one->sequence < two->sequence) ? -1 : ((one->sequence > two->sequence) ? 1 : 0);
}

// Every current file the run holds written in its place as the run ends: of each name, its latest blob marked
// QRY_MARK_FILED (qry_file_put), up to the buffer's tail `tail`. 1, or 0 where one was not written whole; `count` the
// files written
static int qry_files_put(QryBuffer *buffer, unsigned long long tail, unsigned long long *count)
{
    *count = 0ull;
    unsigned long long blobs = 0ull;
    QryBlob blob;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        blobs += 1ull;
    }
    QryFileBlob *const filed = (QryFileBlob *)malloc((size_t)(blobs + 1ull) * sizeof(QryFileBlob));
    if (filed == NULL)
    {
        return 0;
    }
    unsigned long long kept = 0ull;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        QryFrameHead opened;
        memcpy(&opened, buffer->data + walk, sizeof(opened));
        if ((opened.marks & QRY_MARK_FILED) != 0u)
        {
            filed[kept] = (QryFileBlob){qry_hash((const char *)(buffer->data + blob.name_at), (size_t)blob.name_length),
                                        blob.name_at, blob.name_length, blob.sequence, walk};
            kept += 1ull;
        }
    }
    s_file_blobs_data = buffer->data;
    qsort(filed, (size_t)kept, sizeof(QryFileBlob), qry_file_blob_order);
    int whole = 1;
    for (unsigned long long at = 0ull; at < kept; at += 1ull)
    {
        // the last of each name's run, its latest
        const int last = ((at + 1ull) == kept) || (filed[at + 1ull].hash != filed[at].hash) ||
                         (filed[at + 1ull].name_length != filed[at].name_length) ||
                         (memcmp(buffer->data + filed[at + 1ull].name_at, buffer->data + filed[at].name_at,
                                 (size_t)filed[at].name_length) != 0);
        if (last)
        {
            whole &= qry_file_put(buffer, filed[at].at);
            *count += 1ull;
        }
    }
    free(filed);
    return whole;
}

// The bytes of the frame at `at` in the buffer written where its head's marks say, before the hand is told its blob
// is on disk: into the named stream of the .qry that bears its name, `<path>:<name>`, where it is marked
// QRY_MARK_STREAMED. A blob marked QRY_MARK_FILED stays in the run until it ends (qry_files_put). 1, or 0 where it is
// marked for a stream and was not written whole; the hand reads it back and is told so
static int qry_marked_written(QryBuffer *buffer, unsigned long long at)
{
    QryFrameHead opened;
    memcpy(&opened, buffer->data + at, sizeof(opened));
    const char *const name = (const char *)(buffer->data + at + sizeof(opened));
    const unsigned char *const bytes = buffer->data + at + opened.bytes_at;
    if ((opened.marks & QRY_MARK_STREAMED) == 0u)
    {
        return 1;
    }
#if defined(_WIN32)
    char stream[sizeof(buffer->path) + QRY_NAME_LONGEST + 2u];
    // a name is at most QRY_NAME_LONGEST bytes, which an int counts
    snprintf(stream, sizeof(stream), "%s:%.*s", buffer->path, (int)opened.name_length, name);
    return qry_bytes_put(stream, bytes, opened.size);
#else
    (void)name;
    (void)bytes;
    return 0;
#endif
}

// every marked frame from `from` to `to` in the buffer written where its marks say (qry_marked_written): 1, or 0
// where one was not written whole
static int qry_marks_written(QryBuffer *buffer, unsigned long long from, unsigned long long to)
{
    int whole = 1;
    QryBlob blob;
    for (unsigned long long walk = from; (walk < to) && qry_blob_read(buffer->data, to, walk, &blob); walk = blob.next)
    {
        whole &= qry_marked_written(buffer, walk);
    }
    return whole;
}

// Every panic and critical blob handed since the writer last looked written at the file's end through to the disk
// itself, the panics first, then the critical blobs, each in the order handed, each marked one written where its marks
// say, and the latest of them made the latest on disk; the tail, the places and the latest sequence handed read with
// them kept in `state`. 1, or 0 where more were handed than the buffer keeps places for, and the caller writes the
// queue in order and flushes it through instead
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
                whole &= qry_marked_written(buffer, s_taken[place].at);
                bytes += s_taken[place].size;
                if (priority == QRY_PANIC)
                {
                    // a panic's time from its hand to the disk, its hand's time read off its frame
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
        // the slice's end stepped blob by blob from its start, each blob's end read off its own frame
        unsigned long long slice_end = at;
        QryBlob blob;
        while ((slice_end < stop) && ((slice_end - at) < QRY_SLICE_BYTES) &&
               qry_blob_read(buffer->data, stop, slice_end, &blob))
        {
            slice_end = blob.next;
        }
        // a stretch whose frames do not read is written whole to its stop, as it stands in the buffer
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
// blob, else each read off the blob's frame. NULL where there was no memory; `count` the rows
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
    // the frames from the first boundary past the rows, where the buffer's own alignment holds in the file
    table.blobs_at = qry_aligned(table.rows_at + (*count * sizeof(QryRow)));
    table.blobs_bytes = tail;
    table.frame_align = QRY_FRAME_ALIGN;
    static const unsigned char s_zeros[QRY_FRAME_ALIGN] = {0};
    const unsigned long long padding = table.blobs_at - table.rows_at - (*count * sizeof(QryRow));
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
                qry_file_added(&file, s_zeros, padding, 0) && qry_file_added(&file, buffer->data, tail, 0);
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
    int moved = 0;
    while (!stopped)
    {
        // a pass that moved a member's ring is followed by another at once: what a member added while it was taken is
        // taken on its next turn
        const unsigned long long waited = qry_milliseconds() - last;
        if (!moved)
        {
            qry_wait(buffer, (waited < QRY_WRITE_MILLISECONDS) ? (QRY_WRITE_MILLISECONDS - waited) : 1ull);
        }
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
            // them and flushed through, every marked one written where its marks say, and every one of them handed by
            // then is on disk with it
            state->spans_seen = state->spans_read;
            const unsigned long long from = atomic_load(&head->written);
            qry_range_written(buffer, &file, from, state->tail_seen, state);
            state->writes.failed |= !qry_file_durable(&file);
            state->writes.failed |= !qry_marks_written(buffer, from, state->tail_seen);
            if (state->handed_read > atomic_load(&head->critical_written))
            {
                atomic_store(&head->critical_written, state->handed_read);
            }
            last = qry_milliseconds();
        }
        // then each member in its turn
        moved = qry_members_moved(buffer);
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
        stopped = stopping && !moved && qry_members_empty(buffer) &&
                  (atomic_load(&head->tail) == atomic_load(&head->written)) &&
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
    qry_appended(buffer, "writer", strlen("writer"), account, (unsigned long long)length, QRY_ORDINARY, 0u,
                 head->capacity, 0ull, &placed);
    qry_unlock(buffer);
    qry_range_written(buffer, &file, atomic_load(&head->written), atomic_load(&head->tail), state);
    const int closed = qry_file_closed(&file);
    const int failed = state->writes.failed;
    free(state);
    // the run dumped with its table at its front, in place of the .qry written as it ran
    const unsigned long long began = qry_nanoseconds();
    unsigned long long rows = 0ull;
    const int tabled = closed && qry_table_dumped(buffer, &rows);
    // then every current file the run holds in its place, before the writer says it has stopped: a script that stops
    // its run and reads a file the run wrote reads it as the run left it
    const unsigned long long put_began = qry_nanoseconds();
    unsigned long long files = 0ull;
    const int put = qry_files_put(buffer, atomic_load(&head->tail), &files);
    atomic_store(&head->writing, 0u);
    printf("  %s: %s", buffer->path, account);
    if (tabled)
    {
        printf("  %s: dumped with its table of %llu rows in %.3f ms\n", buffer->path, rows,
               (double)(put_began - began) / 1000000.0);
    }
    else
    {
        printf("  %s: not dumped with its table; it stands as written as the run ran\n", buffer->path);
    }
    if (files != 0ull)
    {
        printf("  %s: %llu files written in their places in %.3f ms%s\n", buffer->path, files,
               (double)(qry_nanoseconds() - put_began) / 1000000.0, put ? "" : "; a file was not written whole");
    }
    return closed && !failed && put;
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
