// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_writer_internal.h: what the run's writer, its members and their platforms share: the run's shared memory, the
// members' rings, the writer's schedule and the run's records, and the calls each platform makes by its own means
// (platform/windows.c, platform/posix.c)
#ifndef QRY_WRITER_INTERNAL_H
#define QRY_WRITER_INTERNAL_H

#include "qry_member.h"

#include <stdatomic.h>

// The run's shared region is laid from the bytes the writer is started with, the ask, and made whole: half of it
// rings, a member each, the most rings QRY_WRITER_MEMBERS_MOST; a place of the schedule for each
// QRY_WRITER_PLACE_BYTES of it; and the rest the run's records. A writer started with no ask takes
// QRY_WRITER_RUN_DEFAULT
#define QRY_WRITER_MEMBERS_MOST 256u
#define QRY_WRITER_PLACE_BYTES 256ull
#define QRY_WRITER_RUN_DEFAULT (1ull << 30u)

// The host's own steps, the fastest its sweeps of timings found, stand in qry_host.h beside this file where a sweep has
// written one (`orior protocol sweep`), and set those it names; the rest hold the values below
#if defined(__has_include)
#if __has_include("qry_host.h")
#include "qry_host.h"
#endif
#endif

// the most bytes one write call puts in a file, at most a gibibyte, which a DWORD counts
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

// The run's shared memory, its head. The top of the tree, a record whose name is a path under it its file; the
// writer's stop; the records whose bytes did not hold to their CRC; the records the schedule had no place or the run no
// room for; the places the schedule holds, each the place of a whole record in the order the writer took it; the bytes
// of the records; the panic line, the place plus 1 of the panic that opened it and of the panic whose ruling closed
// it, the line open while they differ; and the region's lay: the rings, a member each, the schedule's places and the
// records' bytes, and where each starts from the region's first byte, with the region's whole bytes, which the writer
// writes last as it makes the region; a member that finds the region before them therefore finds no run yet. The rings, the
// schedule and the records follow the head in the region
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
    unsigned long long members;
    unsigned long long places;
    unsigned long long run_bytes;
    unsigned long long rings_at;
    unsigned long long schedule_at;
    unsigned long long records_at;
    _Atomic unsigned long long whole;
};

// The region's lay from the bytes asked: the members, the places and the records' bytes into each, and the whole the
// region takes, the head and each part on a 64-byte boundary
static inline unsigned long long qry_shared_laid(unsigned long long asked, unsigned long long *members,
                                                 unsigned long long *places, unsigned long long *run_bytes)
{
    const unsigned long long head = (sizeof(QryShared) + 63ull) & ~63ull;
    const unsigned long long rings = (asked / 2ull) / sizeof(QryRing);
    *members = (rings == 0ull) ? 1ull : ((rings > QRY_WRITER_MEMBERS_MOST) ? QRY_WRITER_MEMBERS_MOST : rings);
    *places = (asked / QRY_WRITER_PLACE_BYTES) + 1ull;
    const unsigned long long laid = head + (*members * sizeof(QryRing)) + ((*places * 8ull + 63ull) & ~63ull);
    *run_bytes = (asked > (laid + (1ull << 20u))) ? (asked - laid) : (1ull << 20u);
    return laid + *run_bytes;
}

// a ring of the region, the schedule and the records, each where the head says it starts
static inline QryRing *qry_shared_ring(QryShared *shared, unsigned long long place)
{
    return (QryRing *)(void *)((unsigned char *)shared + shared->rings_at + (place * sizeof(QryRing)));
}

static inline unsigned long long *qry_shared_schedule(QryShared *shared)
{
    return (unsigned long long *)(void *)((unsigned char *)shared + shared->schedule_at);
}

static inline unsigned char *qry_shared_records(QryShared *shared)
{
    return (unsigned char *)shared + shared->records_at;
}

// the run's shared memory made under the name `path` gives, `bytes` whole, zeros: NULL where it could not be made
QryShared *qry_writer_shared_made(const char *path, unsigned long long bytes);
// the run's shared memory let go
void qry_writer_shared_released(QryShared *shared);

// the file at `path` made anew: its handle, or -1 where it could not be made
long long qry_writer_file_made(const char *path);

// `size` bytes at `bytes` written at the end of the file `file`: 1, or 0 where they did not all go
int qry_writer_file_put(long long file, const unsigned char *bytes, unsigned long long size);

// the bytes written to the file transferred to the storage device: 1, or 0 where they were not
int qry_writer_file_durable(long long file);

// the file transferred to the storage device and closed: 1, or 0 where either failed
int qry_writer_file_closed(long long file);

// the file at `path` taken away: 1, or 0 where it was not
int qry_writer_file_removed(const char *path);

// the file at `beside` put in the place of the file at `placed`, whole: 1, or 0 where it was not
int qry_writer_file_placed(const char *beside, const char *placed);

// the writer's pause between turns over the rings; and the writer put above the work it writes for, where the system
// lets a process raise itself
void qry_writer_paused(void);
void qry_writer_raised(void);

// the standard input read whole into `bytes`, which holds `room`: its size, or QRY_MEMBER_UNREAD's value,
// 0xffffffffffffffff, where it could not be read whole or is larger than `room`
unsigned long long qry_writer_input_read(unsigned char *bytes, unsigned long long room);

// the `size` bytes at `bytes` written whole into the named stream of the .qry at `qry` the `name_length` letters at
// `name` name, `<qry>:<name>`, for a program that reads its input only from a file's path: 1, or 0 where it was not,
// the system giving the file no named stream among those
int qry_writer_stream_put(const char *qry, const char *name, unsigned int name_length, const unsigned char *bytes,
                          unsigned long long size);

#endif
