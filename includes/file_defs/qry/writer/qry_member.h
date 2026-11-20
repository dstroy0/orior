// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_member.h: a module of the run: its ring in the run's shared memory, which its writer drains (qry_writer.c), and
// the run's records, read where the writer puts them in the order of its schedule
#ifndef QRY_MEMBER_H
#define QRY_MEMBER_H

#include "../qry.h"

#include <stddef.h>

// the bytes of a member's ring, a power of two: a record takes at max half of it. And the members a run allocates a
// ring for at max, its region positioned with fewer where its request is smaller
#define QRY_WRITER_RING_BYTES (64ULL << 20u)
#define QRY_WRITER_MEMBERS QRY_WRITER_MEMBERS_MAX

// the longest path of the top of the tree, and of a run's .qry
#define QRY_WRITER_TOP_MAX_LEN 1024u

// the shared memory of the run, and the ring of a member in it. qry_writer_internal.h arranges both, where the writer
// and the member are. A module of the run sees the fields of neither
typedef struct QryShared QryShared;
typedef struct QryRing QryRing;

#ifdef __cplusplus
extern "C"
{
#endif

// the size of a file where the call cannot read it whole
#define QRY_MEMBER_UNREAD 0xffffffffffffffffULL

// the environment variable that names the .qry a run's writer has open, which every member of the run joins
#define QRY_ENVIRONMENT "QRY"

// the name of a panic's record on the line, and of each account a party gives of a question at its conference
#define QRY_PANIC_CALL "panic"
#define QRY_CONFERENCE_CALL "conference"

    // the shared memory of the run that QRY in the environment names, mapped once a process. The result is NULL where
    // QRY names none, or where its writer made none
    QryShared *qry_member_run(void);

    // the path of the run's .qry: NULL where there is no run
    const char *qry_member_path(void);

    // the top of the tree that the writer of the run got, with each file of the run named under it. The result is NULL
    // where there is no run
    const char *qry_member_top(void);

    // this process made a member of the run of the .qry at `path`, in place of its old run. The result is its shared
    // memory, or NULL where no writer made it
    QryShared *qry_member_joined(const char *path);

    // this process a member of no run: its ring waited on until the writer took it all and given back, the run's
    // memory let go
    void qry_member_parted(void);

    // 1 while the writer of the run of the .qry at `path` writes, not yet requested to stop, whether this process
    // is a member of it
    int qry_member_writer_runs(const char *path);

    // the ring of this thread in the run, one module of the run for each thread. The thread takes it and gives it
    // memory the first time it appends to the run. The result is NULL where there is no run or no ring is free
    QryRing *qry_member_ring(void);

    // The record of `name` that holds the `blob_length` bytes at `blob`, at `priority`. The call writes it once into the
    // ring of this thread at its tail, with its time on the one clock and its CRC. Where the ring has no room for it,
    // the call first waits for room.
    //
    // For a panic or a critical record, the call waits until the record is on the disk. The result is its time, which
    // keys it among the records of the run. It is 0 where there is no ring, or where the record is larger than half a
    // ring. It is also 0 where the name is not a name of a record
    unsigned long long qry_member_append(const char *name, const void *blob, unsigned long long blob_length,
                                         unsigned int priority);

    // a wait until the writer takes each record in the ring of this thread. The result is the places that the schedule
    // then holds, a place past each record that this thread appended
    unsigned long long qry_member_settled(void);

    // a wait until the writer takes each record in each ring at the time of the call. The result is the places that the
    // schedule then holds
    unsigned long long qry_member_rings_settled(void);

    // The record at place `*at` of the schedule of the writer, where it is in the run, and `*at` moved past it. A reader
    // starts at 0 and tracks `*at`, and each read goes on from where the last read stopped. The result is 1, or 0 where
    // the schedule holds nothing past `*at`.
    //
    // The name of the record follows its head, and its blob follows its name. A read from 0, and each latest find,
    // first waits until the writer takes what this thread passed (qry_member_settled)
    int qry_member_read(unsigned long long *at, const QryRecord **record);

    // The record at place `*at` of the schedule of the run that `run` maps, and `*at` moved past it. This process views
    // that run and is no member of it (qry_member_shared). The result is 1, or 0 where the schedule holds nothing past
    // `*at`
    int qry_member_viewed_read(QryShared *run, unsigned long long *at, const QryRecord **record);

    // 1 while the writer of the run `run` maps writes, not yet requested to stop
    int qry_member_viewed_writing(const QryShared *run);

    // the place plus 1 of the panic on the line of the run `run` maps, 0 while its line is quiet
    unsigned long long qry_member_viewed_panicked(const QryShared *run);

    // each record of `name` in the run, with the blobs joined into `gathered` in the order of the schedule of the writer.
    // `gathered` has room for `room`. The result is their bytes, or QRY_MEMBER_UNREAD where they do not fit
    unsigned long long qry_member_gathered(const char *name, unsigned char *gathered, unsigned long long room);

    // the latest record of `name`, the schedule read from its end back, and its place in `*place` where `place` is
    // not NULL: 1, or 0 where the run contains none
    int qry_member_latest(const char *name, const QryRecord **record, unsigned long long *place);

    // the blob of the latest record of `name` where it is, its bytes, and its place where `place` is not NULL: 1,
    // or 0 where the run contains none
    int qry_member_blob(const char *name, const unsigned char **blob, unsigned long long *size,
                        unsigned long long *place);

    // The file at `path` as the run stores it: the latest record of its name, its path from the top of the tree. Where
    // the run stores no record of the file, the call reads it from the disk and takes it into the run. The call then
    // finds that record. The result is 1, or 0 where the path is outside the top, or where neither the run nor the disk
    // holds the file
    int qry_member_file(const char *path, const QryRecord **record);

    // the name of the file at `path` in the run, its path from the top of the tree, where it is in `path`. The result
    // is NULL where there is no run, or where the path is outside the top
    const char *qry_member_file_name(const char *path);

    // the blob of the file at `path` as the run stores it (qry_member_file), where it is, and its bytes. With no
    // run, the disk's file mapped where it lies: 1, or 0 where the file is neither the run's nor the disk's
    int qry_member_file_blob(const char *path, const unsigned char **blob, unsigned long long *size);

    // the `size` bytes at `bytes`, appended as the file at `path`. The name of its record is its path from the top of the
    // tree. The writer writes it in the place of the file as the run ends. The result is the time of the record, or 0
    // where the path is outside the top or the record did not go in
    unsigned long long qry_member_filed(const char *path, const void *bytes, unsigned long long size);

    // The `size` bytes at `bytes`, appended critical as `:<name>`. The writer writes them into the named stream
    // `<qry>:<name>` of the .qry of the run as it takes them. This is for a program that reads its input only from the
    // path of a file, as the disassembler of the vendor does.
    //
    // The path of the stream goes into `path`, which has room for `room`. The result is the time of the record once the
    // stream holds the bytes. It is 0 where the call cannot get the stream: no run, a name with `/`, `\` or `:`, or a
    // system that gives a file no named stream
    unsigned long long qry_member_streamed(const char *name, const void *bytes, unsigned long long size, char *path,
                                           size_t room);

    // the place plus 1 of the panic on the line, 0 while the line is quiet
    unsigned long long qry_member_panicked(void);

    // the line shut on the decision of the panic at `panic`, place plus 1, as qry_member_panicked gives it
    void qry_member_line_closed(unsigned long long panic);

    // the writer requested to write the rest of the run and stop
    void qry_member_stopped(void);

    // the one clock every process of the run reads, in nanoseconds
    unsigned long long qry_member_clock(void);

    // the shared memory of the run at `path`, which its writer made: NULL where it made none
    QryShared *qry_member_shared(const char *path);

    // the shared memory qry_member_shared mapped let go
    void qry_member_unshared(QryShared *run);

    // this process's number, as the system gives it
    unsigned long long qry_member_process(void);

    // the path of the .qry the environment's QRY names into `path`, which has room for `room`: 1, or 0 where it names
    // none or the path does not fit
    int qry_member_named(char *path, size_t room);

    // the path of this process's program file into `path`, which has room for `room`: 1, or 0 where it does not fit or
    // the system gives none
    int qry_member_program(char *path, size_t room);

    // the file at `path` mapped whole, for a read where it lies, with its size in `*size`. The result is its bytes, or
    // NULL where the call cannot map it. A file of no bytes maps to bytes of none
    const unsigned char *qry_member_file_mapped(const char *path, unsigned long long *size);

    // the `size` bytes at `bytes` qry_member_file_mapped mapped let go
    void qry_member_file_unmapped(const unsigned char *bytes, unsigned long long size);

    // the file at `path` read whole into `bytes`, which has room for `room`. The call reads to its end, whatever length
    // the system gives it. The result is the bytes it holds. It is QRY_MEMBER_UNREAD where the call cannot read it whole,
    // or where it is larger than `room`
    unsigned long long qry_member_file_read(const char *path, unsigned char *bytes, unsigned long long room);

#ifdef __cplusplus
}
#endif

#endif
