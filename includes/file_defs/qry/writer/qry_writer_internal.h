// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_writer_internal.h: what the run's writer, its members and their platforms share: the run's shared memory, the
// members' rings, the writer's schedule and the run's records, and the calls each platform makes by its own means
// (platform/windows.c, platform/posix.c)
#ifndef QRY_WRITER_INTERNAL_H
#define QRY_WRITER_INTERNAL_H

#include "qry_member.h"

#include <stdatomic.h>

// The shared region of the run takes its layout from the bytes that the writer starts with, the request. The writer
// makes it whole:
//   - half of it is rings, a member each, with QRY_WRITER_MEMBERS_MAX rings at max.
//   - a place of the schedule for each QRY_WRITER_PLACE_BYTES of it.
//   - the rest is the records of the run.
//
// A writer that starts with no request takes QRY_WRITER_RUN_DEFAULT
#define QRY_WRITER_MEMBERS_MAX 256u
#define QRY_WRITER_PLACE_BYTES 256ULL
#define QRY_WRITER_RUN_DEFAULT (1ULL << 30u)

// The own steps of the host, the fastest that its sweeps of timings found, are in qry_host.h beside this file. That
// file exists where a sweep wrote one (`orior protocol sweep`), and it sets the steps that it names. The rest take the
// values below
#if defined(__has_include)
#if __has_include("qry_host.h")
#include "qry_host.h"
#endif
#endif

// the max bytes one write call puts in a file, at max a gibibyte, which a DWORD counts
#ifndef QRY_WRITER_PIECE_BYTES
#define QRY_WRITER_PIECE_BYTES (1ULL << 30u)
#endif

_Static_assert((QRY_WRITER_PIECE_BYTES != 0ULL) && (QRY_WRITER_PIECE_BYTES <= (1ULL << 30u)),
               "QRY_WRITER_PIECE_BYTES must be a write call's bytes, at max a gibibyte");

// the mark of a ring where a record does not fit before the end of the ring. It is a time that no clock gives, and the
// ring goes on at its start
#define QRY_WRITER_WRAP 0xffffffffffffffffULL

// The ring of a member:
//   - the process that owns it, 0 where none does.
//   - the bytes that the writer took from it, and the bytes that its member added. Each is a count from the start of
//     the run, and the remainder of the count is a place in the ring.
//   - the bytes.
//
// Only the member moves `tail`, and only the writer moves `head`
struct QryRing
{
    _Atomic unsigned long long owner;
    _Atomic unsigned long long head;
    _Atomic unsigned long long tail;
    unsigned long long unused[5];
    unsigned char bytes[QRY_WRITER_RING_BYTES];
};

// The shared memory of the run, its head:
//   - the top of the tree. A record whose name is a path under it is its file.
//   - the stop of the writer.
//   - the records whose bytes did not match their CRC.
//   - the records that had no place in the schedule or no room in the run.
//   - the places in the schedule, each the place of a whole record in the order that the writer took it.
//   - the bytes of the records.
//   - the panic line: the place plus 1 of the panic that opened it, and of the panic whose decision shut it. The line
//     is open while they differ.
//   - the layout of the region: the rings, a member each, the places of the schedule and the bytes of the records.
//     Then where each starts from the first byte of the region.
//   - the whole bytes of the region. The writer writes them last as it makes the region. A member that finds the
//     region before them thus finds no run yet.
//
// The rings, the schedule and the records follow the head in the region
struct QryShared
{
    char top[QRY_WRITER_TOP_MAX_LEN];
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

// The layout of the region from the requested bytes. That is the members, the places and the bytes of the records into
// each, and the whole that the region takes. The head and each part are on a 64-byte boundary
static inline unsigned long long qry_shared_positioned(unsigned long long requested, unsigned long long *members,
                                                 unsigned long long *places, unsigned long long *run_bytes)
{
    const unsigned long long head = (sizeof(QryShared) + 63ULL) & ~63ULL;
    const unsigned long long rings = (requested / 2ULL) / sizeof(QryRing);
    const unsigned long long fitted = (rings > QRY_WRITER_MEMBERS_MAX) ? QRY_WRITER_MEMBERS_MAX : rings;
    *members = (rings == 0ULL) ? 1ULL : fitted;
    *places = (requested / QRY_WRITER_PLACE_BYTES) + 1ULL;
    const unsigned long long positioned = head + (*members * sizeof(QryRing)) + ((*places * 8ULL + 63ULL) & ~63ULL);
    *run_bytes = (requested > (positioned + (1ULL << 20u))) ? (requested - positioned) : (1ULL << 20u);
    return positioned + *run_bytes;
}

// a ring of the region, the schedule and the records, each where the head specifies it starts
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

// the shared memory of the run, made under the name that `path` gives, `bytes` whole, zeros. NULL where the make failed
QryShared *qry_writer_shared_made(const char *path, unsigned long long bytes);
// the run's shared memory let go
void qry_writer_shared_released(QryShared *shared);

// the file at `path`, made anew: its handle, or -1 where the make failed
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

// the writer's pause between turns over the rings. And the writer put above the work it writes for, where the system
// lets a process raise itself
void qry_writer_paused(void);
void qry_writer_raised(void);

// 1 where the process `process` (qry_member_process) ended, and 0 where it runs or the system does not tell
int qry_writer_process_ended(unsigned long long process);

// the standard input, read whole into `bytes`, which has room for `room`. The result is its size. It is the value of
// QRY_MEMBER_UNREAD, 0xffffffffffffffff, where the read was not whole or the input is larger than `room`
unsigned long long qry_writer_input_read(unsigned char *bytes, unsigned long long room);

// the `size` bytes at `bytes`, written whole into a named stream of the .qry at `qry`, `<qry>:<name>`. The
// `name_length` characters at `name` name the stream. This is for a program that reads its input only from the path of
// a file. The result is 1, or 0 where the write failed, as where the system gives the file no named streams
int qry_writer_stream_put(const char *qry, const char *name, unsigned int name_length, const unsigned char *bytes,
                          unsigned long long size);

#endif
