// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_member.h: a module of the run: its ring in the run's shared memory, which its writer drains (qry_writer.c), and
// the run's records, read where the writer puts them in the order of its schedule
#ifndef QRY_MEMBER_H
#define QRY_MEMBER_H

#include "../qry.h"

#include <stddef.h>

// the bytes of a member's ring, a power of two: a record takes at most half of it; and the members a run keeps a ring
// for at the most, its region laid with fewer where its ask is smaller
#define QRY_WRITER_RING_BYTES (64ull << 20u)
#define QRY_WRITER_MEMBERS QRY_WRITER_MEMBERS_MOST

// the longest path of the top of the tree, and of a run's .qry
#define QRY_WRITER_TOP_LONGEST 1024u

// the run's shared memory and a member's ring in it, laid out where the writer and the member are
// (qry_writer_internal.h): a module of the run sees neither's fields
typedef struct QryShared QryShared;
typedef struct QryRing QryRing;

#ifdef __cplusplus
extern "C"
{
#endif

// a file's size where it could not be read whole
#define QRY_MEMBER_UNREAD 0xffffffffffffffffull

// the environment variable that names the .qry a run's writer holds open, which every member of the run joins
#define QRY_ENVIRONMENT "QRY"

// the name of a panic's record on the line, and of each account a party gives of a question at its conference
#define QRY_PANIC_CALL "panic"
#define QRY_CONFERENCE_CALL "conference"

    // the run's shared memory, the run the environment's QRY names, mapped once a process: NULL where none is named
    // or its writer made none
    QryShared *qry_member_run(void);

    // the path of the run's .qry: NULL where there is no run
    const char *qry_member_path(void);

    // the top of the tree the run's writer was given, every file of the run named under it: NULL where there is no
    // run
    const char *qry_member_top(void);

    // this process made a member of the run of the .qry at `path` in place of the one it was a member of: its shared
    // memory, or NULL where no writer made it
    QryShared *qry_member_joined(const char *path);

    // this process a member of no run: its ring waited on until the writer took it all and given back, the run's
    // memory let go
    void qry_member_parted(void);

    // 1 while the writer of the run of the .qry at `path` writes, not yet asked to stop, whether this process
    // is a member of it
    int qry_member_writer_runs(const char *path);

    // this thread's ring in the run, a module of the run each thread, taken and given memory the first time the thread
    // appends to the run: NULL where there is no run or every ring is taken
    QryRing *qry_member_ring(void);

    // The record of `name` holding the `blob_length` bytes at `blob`, at `priority`, written once into this thread's
    // ring at its tail with its time on the one clock and its CRC, a ring that holds no room for it first waited on;
    // a panic or a critical record waited on until it is on the disk. Its time, which keys it among the run's
    // records, or 0 where there is no ring, the record is larger than half a ring or the name is none a record takes
    unsigned long long qry_member_append(const char *name, const void *blob, unsigned long long blob_length,
                                         unsigned int priority);

    // this thread's ring waited on until the writer has taken every record in it: the places the schedule then
    // holds, a place past every record this thread appended
    unsigned long long qry_member_settled(void);

    // every ring waited on until the writer has taken every record each kept as this was called: the places the
    // schedule then holds
    unsigned long long qry_member_rings_settled(void);

    // The record at place `*at` of the writer's schedule, where it stands in the run, and `*at` moved past it: a
    // reader starts at 0 and keeps `*at`, each read going on from where the last stopped. 1, or 0 where the schedule
    // holds nothing past `*at`. The record's name follows its head, and its blob its name. A read from 0, and every
    // latest found, waits first until the writer has taken what this thread handed (qry_member_settled)
    int qry_member_read(unsigned long long *at, const QryRecord **record);

    // The record at place `*at` of the schedule of the run `run` maps, which this process views without being a
    // member of it (qry_member_shared), and `*at` moved past it: 1, or 0 where the schedule holds nothing past `*at`
    int qry_member_viewed_read(QryShared *run, unsigned long long *at, const QryRecord **record);

    // 1 while the writer of the run `run` maps writes, not yet asked to stop
    int qry_member_viewed_writing(QryShared *run);

    // the place plus 1 of the panic on the line of the run `run` maps, 0 while its line is quiet
    unsigned long long qry_member_viewed_panicked(QryShared *run);

    // every record of `name` the run holds, the blobs joined into `gathered`, which holds `room`, in the order of the
    // writer's schedule: their bytes, or QRY_MEMBER_UNREAD where they do not fit
    unsigned long long qry_member_gathered(const char *name, unsigned char *gathered, unsigned long long room);

    // the latest record of `name`, the schedule read from its end back, and its place in `*place` where `place` is
    // not NULL: 1, or 0 where the run holds none
    int qry_member_latest(const char *name, const QryRecord **record, unsigned long long *place);

    // the blob of the latest record of `name` where it stands, its bytes, and its place where `place` is not NULL: 1,
    // or 0 where the run holds none
    int qry_member_blob(const char *name, const unsigned char **blob, unsigned long long *size,
                        unsigned long long *place);

    // The file at `path` as the run holds it: the latest record of its name, its path from the top of the tree. A
    // file the run holds no record of is read from the disk and taken into the run, and that record found. 1, or 0
    // where the path stands outside the top or the file is neither the run's nor the disk's
    int qry_member_file(const char *path, const QryRecord **record);

    // the name the file at `path` goes by in the run, its path from the top of the tree, where it stands in `path`:
    // NULL where there is no run or the path stands outside the top
    const char *qry_member_file_name(const char *path);

    // the blob of the file at `path` as the run holds it (qry_member_file), where it stands, and its bytes; with no
    // run, the disk's file mapped where it lies: 1, or 0 where the file is neither the run's nor the disk's
    int qry_member_file_blob(const char *path, const unsigned char **blob, unsigned long long *size);

    // the `size` bytes at `bytes` appended as the file at `path`, its record named by its path from the top of the
    // tree, which the writer writes in the file's place as the run ends: the record's time, or 0 where the path
    // stands outside the top or the record did not go in
    unsigned long long qry_member_filed(const char *path, const void *bytes, unsigned long long size);

    // The `size` bytes at `bytes` appended critical as `:<name>`, which the writer writes into the run's .qry's named
    // stream `<qry>:<name>` as it takes it, for a program that reads its input only from a file's path, as the
    // vendor's disassembler does; the stream's path into `path`, which holds `room`. The record's time once the
    // stream holds the bytes, or 0 where it could not be had: no run, a name that holds `/`, `\` or `:`, or a system
    // that gives a file no named stream
    unsigned long long qry_member_streamed(const char *name, const void *bytes, unsigned long long size, char *path,
                                           size_t room);

    // the place plus 1 of the panic on the line, 0 while the line is quiet
    unsigned long long qry_member_panicked(void);

    // the line closed on the ruling of the panic at `panic`, place plus 1, as qry_member_panicked gives it
    void qry_member_line_closed(unsigned long long panic);

    // the writer asked to write the rest of the run and stop
    void qry_member_stopped(void);

    // the one clock every process of the run reads, in nanoseconds
    unsigned long long qry_member_clock(void);

    // the shared memory of the run at `path`, which its writer made: NULL where it made none
    QryShared *qry_member_shared(const char *path);

    // the shared memory qry_member_shared mapped let go
    void qry_member_unshared(QryShared *run);

    // this process's number, as the system gives it
    unsigned long long qry_member_process(void);

    // the path of the .qry the environment's QRY names into `path`, which holds `room`: 1, or 0 where it names none
    // or the path does not fit
    int qry_member_named(char *path, size_t room);

    // the path of this process's program file into `path`, which holds `room`: 1, or 0 where it does not fit or the
    // system gives none
    int qry_member_program(char *path, size_t room);

    // the file at `path` mapped whole to be read where it lies, its size in `*size`: its bytes, or NULL where it
    // could not be mapped; a file of no bytes maps to bytes of none
    const unsigned char *qry_member_file_mapped(const char *path, unsigned long long *size);

    // the `size` bytes at `bytes` qry_member_file_mapped mapped let go
    void qry_member_file_unmapped(const unsigned char *bytes, unsigned long long size);

    // the file at `path` read whole into `bytes`, which holds `room`: its size, or QRY_MEMBER_UNREAD where it could
    // not be read whole or is larger than `room`
    unsigned long long qry_member_file_read(const char *path, unsigned char *bytes, unsigned long long room);

#ifdef __cplusplus
}
#endif

#endif
