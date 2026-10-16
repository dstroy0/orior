// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_buffer_internal.h: what the modules of the run's buffer share: the buffer in shared memory and its head, index, members, rings and table as qry.h lays them out, the frames and files they hold, and the state and functions one module reads from another (qry_buffer.h)
#ifndef QRY_BUFFER_INTERNAL_H
#define QRY_BUFFER_INTERNAL_H

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0A00
#endif
#if !defined(_WIN32)
// shared memory, the robust process-shared lock, the named semaphore, the clocks and the writes at an offset are POSIX,
// outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif

#include "../qry_buffer.h"

#include "../../../../includes/codecs/crc/crc_key.h"
#include "../qry.h"

#include <ctype.h>
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
// `critical_written` the latest on disk; `panicked` the panic on the line, the first handed and the first after a conference closed the line, and `closed` the panic whose line a conference's ruling closed (qry_line_closed)
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
    // the blobs the writer found in a ring whose bytes did not hold to the CRC their member took, and left there
    unsigned long long corrupt;
    unsigned long long index_full;
    unsigned long long writer_process;
    _Atomic unsigned long long spans;
    _Atomic unsigned long long tail;
    _Atomic unsigned long long written;
    _Atomic unsigned long long critical_handed;
    _Atomic unsigned long long critical_written;
    _Atomic unsigned long long panicked;
    _Atomic unsigned long long closed;
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
// handed; its bytes' count; the CRC-64/XZ of its bytes, taken by the member as it handed them, which the writer holds
// the bytes to as it moves them and the row carries; its name's length; and its marks. Its name follows, then its
// bytes. Where a blob would not fit before the ring's end, the stretch it leaves there is one whole word, its count,
// and no blob
typedef struct
{
    unsigned long long whole;
    unsigned long long nanoseconds;
    unsigned long long size;
    unsigned long long crc;
    unsigned int name_length;
    unsigned int marks;
} QryRingBlob;

// the bytes the members' heads take, a whole count of 64 KiB
#define QRY_MEMBERS_BYTES ((((unsigned long long)QRY_MEMBERS * 64ull) + 65535ull) & ~65535ull)

_Static_assert(sizeof(QryMember) == 64u, "QryMember must be 64 bytes, a member's head on a line of its own");

_Static_assert(sizeof(QryRingBlob) == 40u, "QryRingBlob must be the 40 bytes a ring's blob opens with");

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
    // the writer's tap, the pipe's name while it is open and empty where it is not (tap.c)
    char tap_name[128];
#if defined(_WIN32)
    HANDLE section;
    HANDLE lock;
    HANDLE wake;
    HANDLE writer;
    HANDLE tap;
    int tap_connected;
#else
    unsigned char *view;
    unsigned long long view_bytes;
    int memory;
    sem_t *wake;
    int tap;
    char memory_name[64];
    char wake_name[64];
#endif
};

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

// the who table, read once a process: each number's source, its path from the top of the tree, a number the table does
// not give an empty path; and the program's own number once it is found
#define QRY_WHO_MOST 65536u

// the last sources asked, by the pointer __FILE__ gives, each a source's own and the same at every ask, and the number
// the who table gives each; one process's, held under the lock beside them, which every thread of it takes
#define QRY_WHO_ASKED 64u

// one wait while another is moved: the thread's turn given up at first, and a millisecond's sleep past
// QRY_PAUSE_YIELDS of them, `*waited` counting the waits. A sleep is the system timer's step, as much as 15 ms on
// Windows, and a move takes far less: the yields cover the moves a writer makes under load
#define QRY_PAUSE_YIELDS 20000u

// the buffers whose rings this process holds, each let go as it ends
#define QRY_HELD_BUFFERS 8u

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

// a blob marked QRY_MARK_FILED: its name's hash and place, its sequence, and its frame's place in the buffer
typedef struct
{
    unsigned long long hash;
    unsigned long long name_at;
    unsigned long long name_length;
    unsigned long long sequence;
    unsigned long long at;
} QryFileBlob;

// the words of a row before its columns: the who's two words
#define QRY_ROW_OPENING 2u

extern const unsigned char *s_file_blobs_data;

unsigned long long qry_view_bytes(void);
unsigned long long qry_hash(const char *text, size_t length);
unsigned long long qry_crc(const unsigned char *bytes, unsigned long long size);
unsigned long long qry_sizes(void);
unsigned long long qry_nanoseconds(void);
unsigned long long qry_milliseconds(void);
void qry_sleep_millisecond(void);
int qry_name_taken(const char *name, size_t *length);
void qry_names(const char *path, char *memory, char *lock, char *wake, size_t room);

// The writer's tap (tap.c): the pipe named from the hash of the .qry's path as the wake is, `\\.\pipe\qry-<hash>-tap`
// on Windows and the FIFO `/tmp/qry-<hash>-tap` on POSIX; opened by the writer as it starts, 1, or 0 where the system
// gives no pipe and the run goes on with no tap; one line a blob written as the writer places it, `<sequence>
// <nanoseconds> <priority> <member> <name> <size>`, the member its ring's place or `-` for a blob that went in at the
// tail, a line with no reader dropped and the writer never waiting on it; and closed as the writer ends
void qry_tap_name(const char *path, char *name, size_t room);
int qry_tap_opened(QryBuffer *buffer);
void qry_tapped(QryBuffer *buffer, unsigned long long sequence, unsigned long long nanoseconds, unsigned int priority,
                unsigned int member, const char *name, size_t name_length, unsigned long long size);
void qry_tap_closed(QryBuffer *buffer);
void qry_lock(QryBuffer *buffer);
void qry_unlock(QryBuffer *buffer);
void qry_wake(QryBuffer *buffer);
void qry_wait(QryBuffer *buffer, unsigned long long milliseconds);
int qry_writer_alive(QryBuffer *buffer);
int qry_committed(QryBuffer *buffer, unsigned long long need);
int qry_rows_committed(QryBuffer *buffer, unsigned long long rows);
int qry_blob_read(const unsigned char *data, unsigned long long end, unsigned long long at, QryBlob *blob);
unsigned long long qry_aligned(unsigned long long count);
unsigned long long qry_index_find(QryBuffer *buffer, const char *name, size_t length);
unsigned long long qry_appended(QryBuffer *buffer, const char *name, size_t length, const void *bytes, unsigned long long size, unsigned long long crc, unsigned int priority, unsigned int marks, unsigned long long room, unsigned long long handed_at, int *placed);
void qry_dumped_said(QryBuffer *buffer);
int qry_critical_waited(QryBuffer *buffer, unsigned long long sequence);
int qry_file_placed(const char *name, size_t length, char *path, size_t room);
unsigned long long qry_placed(QryBuffer *buffer, const char *name, size_t length, const void *bytes, unsigned long long size, unsigned long long crc, unsigned int priority, unsigned int marks, unsigned long long handed_at, int *placed);
void qry_held_kept(QryBuffer *buffer, int kept);
int qry_members_moved(QryBuffer *buffer);
int qry_members_empty(QryBuffer *buffer);
void qry_own_moved(QryBuffer *buffer);
void qry_member_released(QryBuffer *buffer);
unsigned long long qry_handed(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size, unsigned int priority, unsigned int marks);
int qry_join_added(unsigned char **joined, unsigned long long *held, unsigned long long *room, const unsigned char *bytes, unsigned long long size);
int qry_mapped(QryMapped *mapped, const char *path);
void qry_unmapped(QryMapped *mapped);
unsigned long long qry_next_whole(const unsigned char *bytes, unsigned long long size, unsigned long long at);
int qry_table_found(const QryMapped *mapped, QryTableHead *table, const QryRow **rows);
int qry_row_inside(const QryTableHead *table, const QryRow *row);
int qry_row_whole(const QryMapped *mapped, const QryTableHead *table, const QryRow *row);
int qry_file_opened(QryFile *file, const char *path);
int qry_file_added(QryFile *file, const unsigned char *bytes, unsigned long long size, int through);
int qry_file_durable(QryFile *file);
int qry_file_closed(QryFile *file);
void qry_write_counted(QryWrites *writes, unsigned long long began, unsigned long long bytes, int whole);
int qry_bytes_put(const char *path, const unsigned char *bytes, unsigned long long size);
int qry_file_put(QryBuffer *buffer, unsigned long long at);
int qry_file_blob_order(const void *left, const void *right);
int qry_files_put(QryBuffer *buffer, unsigned long long tail, unsigned long long *count);
int qry_marks_written(QryBuffer *buffer, unsigned long long from, unsigned long long to);
int qry_firsts_written(QryBuffer *buffer, QryFile *file, QryWriterState *state);
void qry_range_written(QryBuffer *buffer, QryFile *file, unsigned long long from, unsigned long long to, QryWriterState *state);
int qry_table_dumped(QryBuffer *buffer, unsigned long long *count);

#endif
