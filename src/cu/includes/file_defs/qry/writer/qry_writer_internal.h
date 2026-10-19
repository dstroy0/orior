// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_writer_internal.h: what the run's writer, its members and their platforms share: the run's shared memory, the
// members' rings, the writer's schedule and the run's records, and the calls each platform makes by its own means
// (platform/windows.c, platform/posix.c)
#ifndef QRY_WRITER_INTERNAL_H
#define QRY_WRITER_INTERNAL_H

#include "qry_member.h"

#include <stdatomic.h>

// the members the run keeps a ring for; the places the writer's schedule keeps, a record each; and the bytes of the
// run's records
#define QRY_WRITER_MEMBERS 256u
#define QRY_WRITER_SCHEDULE_PLACES (1ull << 24u)
#define QRY_WRITER_RUN_BYTES (64ull << 30u)

// The host's own steps, the fastest its sweeps of timings found, stand in qry_host.h beside this file where a sweep has
// written one (`orior protocol sweep`), and set those it names; the rest hold the values below
#if defined(__has_include)
#if __has_include("qry_host.h")
#include "qry_host.h"
#endif
#endif

// the step the writer gives memory to the schedule and the records in, and the most bytes one write call puts in a
// file, at most a gibibyte, which a DWORD counts
#ifndef QRY_WRITER_COMMIT_BYTES
#define QRY_WRITER_COMMIT_BYTES (64ull << 20u)
#endif

#ifndef QRY_WRITER_PIECE_BYTES
#define QRY_WRITER_PIECE_BYTES (1ull << 30u)
#endif

_Static_assert((QRY_WRITER_PIECE_BYTES != 0ull) && (QRY_WRITER_PIECE_BYTES <= (1ull << 30u)),
               "QRY_WRITER_PIECE_BYTES must be a write call's bytes, at most a gibibyte");

// a ring's mark where a record would not fit before the ring's end: a time no clock gives, the ring going on at its
// start
#define QRY_WRITER_WRAP 0xffffffffffffffffull

// A member's ring: the process that holds it, 0 where none does; the bytes the writer has taken from it and the bytes
// its member has added, each counted from the run's start, a place in the ring the count's remainder; and the bytes.
// Only the member moves `tail` and only the writer moves `head`
struct QryRing
{
    _Atomic unsigned long long owner;
    _Atomic unsigned long long head;
    _Atomic unsigned long long tail;
    unsigned long long unused[5];
    unsigned char bytes[QRY_WRITER_RING_BYTES];
};

// The run's shared memory. The top of the tree, a record whose name is a path under it its file; the writer's stop;
// the records whose bytes did not hold to their CRC; the records the schedule had no place or the run no room for;
// the places the schedule holds, each the place of a whole record in the order the writer took it; the bytes of the
// records; the panic line, the place plus 1 of the panic that opened it and of the panic whose ruling closed it, the
// line open while they differ; the bytes of schedule and records given memory; the rings; the schedule; and the
// run's records
struct QryShared
{
    char top[QRY_WRITER_TOP_LONGEST];
    _Atomic unsigned int stop;
    unsigned int unused;
    _Atomic unsigned long long corrupt;
    _Atomic unsigned long long unplaced;
    _Atomic unsigned long long scheduled;
    _Atomic unsigned long long tail;
    _Atomic unsigned long long panicked;
    _Atomic unsigned long long closed;
    unsigned long long schedule_committed;
    unsigned long long records_committed;
    QryRing ring[QRY_WRITER_MEMBERS];
    unsigned long long schedule[QRY_WRITER_SCHEDULE_PLACES];
    unsigned char records[];
};

// the bytes the run's shared memory takes
#define QRY_WRITER_SHARED_BYTES ((unsigned long long)sizeof(QryShared) + QRY_WRITER_RUN_BYTES)

// the run's shared memory made under the name `path` gives, zeros, only its head given memory: NULL where it could
// not be made
QryShared *qry_writer_shared_made(const char *path);

// the run's shared memory let go
void qry_writer_shared_released(QryShared *shared);

// memory given to the `bytes` bytes at `at` in the run's shared memory: 1, or 0 where the system gives none
int qry_shared_committed(void *at, unsigned long long bytes);

// the file at `path` made anew: its handle, or -1 where it could not be made
long long qry_writer_file_made(const char *path);

// `size` bytes at `bytes` written at the end of the file `file`: 1, or 0 where they did not all go
int qry_writer_file_put(long long file, const unsigned char *bytes, unsigned long long size);

// the bytes written to the file flushed through to the disk: 1, or 0 where they were not
int qry_writer_file_durable(long long file);

// the file flushed through to the disk and closed: 1, or 0 where either failed
int qry_writer_file_closed(long long file);

// the file at `path` taken away: 1, or 0 where it was not
int qry_writer_file_removed(const char *path);

// the file at `beside` put in the place of the file at `placed`, whole: 1, or 0 where it was not
int qry_writer_file_placed(const char *beside, const char *placed);

// the writer's pause between turns over the rings
void qry_writer_paused(void);

// the standard input read whole into `bytes`, which holds `room`: its size, or QRY_MEMBER_UNREAD's value,
// 0xffffffffffffffff, where it could not be read whole or is larger than `room`
unsigned long long qry_writer_input_read(unsigned char *bytes, unsigned long long room);

// the `size` bytes at `bytes` written whole into the named stream of the .qry at `qry` the `name_length` letters at
// `name` name, `<qry>:<name>`, for a program that reads its input only from a file's path: 1, or 0 where it was not,
// the system giving the file no named stream among those
int qry_writer_stream_put(const char *qry, const char *name, unsigned int name_length, const unsigned char *bytes,
                          unsigned long long size);

#endif
