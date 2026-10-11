// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_buffer.h: a query run's buffer, its .qry in shared memory (qry.h). One writer process owns it for the run and is
// the only thing that writes the .qry to disk. Every other process of the run maps it by the .qry's path and hands its
// blobs into it: a hand is a copy to the buffer's tail under a lock the processes share, and nothing more, and the
// stretch past what is on disk is the writer's input queue. A process reads what it needs straight from the buffer, the
// latest blob of a name or every blob of it, before or after it reaches disk.
//
// Every blob is handed with a priority, and the lowest number wins: everything stops to let it through.
//
// QRY_PANIC is the carrier losing its mind, a launch that faulted the part or hung past its bound, or a carrier that
// ended other than clean. A panic is written the moment the writer sees it, ahead of everything waiting: the writer
// writes its queue a QRY_SLICE_BYTES slice at a time and looks for a panic between slices, and a panic waits on at most
// one slice. Its hand does not wait on the disk: the process that hands it goes straight on to its own response, to get
// off the part, and once the panic is in the buffer that process's end cannot lose it. A panic also opens the line
// (qry_panicked): every party of the run sees it at once, the carrier, the part's fault telemetry and the gate hand
// their accounts of it, each a panic of the name QRY_PANIC_CALL, and the teacher hands its own and puts no question
// after it.
//
// QRY_CRITICAL is the round about to reach the part and a dump said: written next, and its hand returns only once it
// is on disk, a wait that comes before the part is reached. Panics and critical blobs are written through a handle
// that writes through to the disk itself: they never wait on the system flushing the ordinary bytes before them, and
// no setting turns either off.
//
// QRY_ORDINARY is every other blob: the writer writes the queue in one sequential stretch once QRY_WRITE_BYTES wait or
// QRY_WRITE_MILLISECONDS pass with anything waiting, and the rest when it is stopped. An ordinary blob that would leave
// less than QRY_CRITICAL_ROOM for the others is dumped, and a blob named QRY_DUMPED says which.
//
// The buffer keeps the run's table beside its blobs, a row a blob added as it is handed: the name's hash, the
// sequence, the time, the priority and where the blob's tag, name and bytes sit. A read of every blob of a name, or of
// its first, goes down the rows and touches no blob but its own. As the run ends the writer dumps the run from memory
// as the .qry: the table first, its rows put in the order of their names' hashes and then their sequences, then every
// blob in the order handed (qry.h), in its place of the .qry the writer wrote as the run ran.
//
// Every process of a run names the .qry by the same string, the one QRY_ENVIRONMENT holds: the buffer is found by it
#ifndef QRY_BUFFER_H
#define QRY_BUFFER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// The sizes every process of a run is built with, each a build's to set; a process built with other sizes than the
// writer's is refused the buffer.
//
// the bytes of blobs a run's buffer holds, reserved at once and given memory QRY_COMMIT_BYTES at a time as it fills
#ifndef QRY_BUFFER_BYTES
#define QRY_BUFFER_BYTES (64ull << 30u)
#endif
#define QRY_COMMIT_BYTES (64ull << 20u)

// the names the buffer finds a blob by, a power of two
#ifndef QRY_INDEX_SLOTS
#define QRY_INDEX_SLOTS (1ull << 22u)
#endif

// the rows of the run's table, a row a blob, kept in the buffer as each blob is handed and dumped at the front of the
// .qry as the run ends (qry.h)
#ifndef QRY_TABLE_ROWS
#define QRY_TABLE_ROWS (1ull << 24u)
#endif

// the writer writes what waits once this many bytes wait, or once this long has passed with anything waiting, and
// writes it a slice at a time, a panic let through between slices
#ifndef QRY_WRITE_BYTES
#define QRY_WRITE_BYTES (8ull << 20u)
#endif
#ifndef QRY_WRITE_MILLISECONDS
#define QRY_WRITE_MILLISECONDS 1000u
#endif
#ifndef QRY_SLICE_BYTES
#define QRY_SLICE_BYTES (1ull << 20u)
#endif

// the room an ordinary blob leaves for panics and critical ones; the panics and critical blobs whose places the buffer
// keeps for the writer to write them first; and how long a critical hand waits for the disk
#ifndef QRY_CRITICAL_ROOM
#define QRY_CRITICAL_ROOM (256ull << 20u)
#endif
#define QRY_CRITICAL_SPANS 1024u
#ifndef QRY_CRITICAL_WAIT_MILLISECONDS
#define QRY_CRITICAL_WAIT_MILLISECONDS 30000u
#endif

// the longest name a blob takes
#define QRY_NAME_LONGEST 255u

// a blob's priority, the lowest number winning: a panic, a critical blob, and an ordinary one
#define QRY_PANIC 0u
#define QRY_CRITICAL 1u
#define QRY_ORDINARY 2u

// the name each party's account of a panic is handed under, the carrier's, the fault telemetry's, the gate's and the
// teacher's, each a panic; a reader joins them to hear the whole call
#define QRY_PANIC_CALL "panic"

// the environment variable that names the run's .qry
#define QRY_ENVIRONMENT "QRY"

    typedef struct QryBuffer QryBuffer;

    // the buffer of the .qry at `path` made, for its writer: NULL where one is open by that name already, or no memory
    QryBuffer *qry_create(const char *path);

    // the buffer of the .qry at `path`, made by a writer that runs: NULL where none is
    QryBuffer *qry_open(const char *path);

    // the run's buffer, the .qry QRY_ENVIRONMENT names, opened once a process: NULL where it names none or no writer
    // runs
    QryBuffer *qry_run(void);

    // the buffer let go by this process
    void qry_close(QryBuffer *buffer);

    // the path of the .qry the buffer is of
    const char *qry_path(const QryBuffer *buffer);

    // `size` bytes at `bytes` handed as a blob of `name` at `priority`: its sequence, or 0 where it was dumped, the
    // name is none a blob takes, the priority is past QRY_ORDINARY, or it is critical and could not be put on disk. A
    // critical blob is on disk when this returns; a panic is in the buffer, the writer woken for it and the line open,
    // and nothing waits
    unsigned long long qry_hand(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                                unsigned int priority);

    // the sequence of the first panic handed in the run, the line open from then on, or 0 where none has been
    unsigned long long qry_panicked(QryBuffer *buffer);

    // the latest blob of `name`, its bytes in the buffer and their count, and its sequence where `sequence` is not
    // NULL: 1, or 0 where none was handed. A reader that handed something before a blob it waits on tells that blob
    // from an older one of the name by the sequence. The bytes stand as long as this process holds the buffer
    int qry_latest(QryBuffer *buffer, const char *name, const unsigned char **bytes, unsigned long long *size,
                   unsigned long long *sequence);

    // the first blob of `name` handed in the run, its bytes in the buffer and their count: 1, or 0 where none was
    // handed. A run that hands each file it reads as it starts reads that one back by this, after it has handed what it
    // changed. The bytes stand as long as this process holds the buffer
    int qry_first(QryBuffer *buffer, const char *name, const unsigned char **bytes, unsigned long long *size);

    // every blob of `name` in the buffer, in their order, joined into memory of the caller's to free: 1, or 0 where it
    // could not be had. None joins to a count of 0
    int qry_joined(QryBuffer *buffer, const char *name, unsigned char **bytes, unsigned long long *size);

    // every blob of `name` in the .qry file at `path`, put in order by their sequence and joined into memory of the
    // caller's to free: 1, or 0 and why in `error`, which holds `room`, where the file could not be read. A stretch of
    // the file no tag of qry.h opens, the ordinary bytes a run's end left unwritten before a panic written through
    // after them, is stepped over to the next blob whose tags read whole, and `error` says how many bytes were stepped
    // over; it is empty where none were
    int qry_file_joined(const char *path, const char *name, unsigned char **bytes, unsigned long long *size,
                        char *error, size_t room);

    // every name of the .qry file at `path` handed to `each` with `context` once, its latest blob by sequence, in the
    // order of those blobs' sequences: the name, the bytes and their count, in the mapped file and standing for the
    // call alone. A finished run's blobs are carried into a new run by this. 1, or 0 and why in `error` where the file
    // could not be read; a stretch no tag opens is stepped over and said, as qry_file_joined does
    int qry_file_latest_each(const char *path,
                             void (*each)(void *context, const char *name, const unsigned char *bytes,
                                          unsigned long long size),
                             void *context, char *error, size_t room);

    // The writer's work, its process's own: every blob handed written to the .qry at the buffer's path until the run is
    // stopped, then the rest, the writer's own account of its writes handed last as the blob `writer`, and that
    // printed. 1, or 0 where the file could not be written
    int qry_write_until_stopped(QryBuffer *buffer);

    // the writer asked to write the rest and stop, and waited for: 1, or 0 where it did not stop within `milliseconds`
    int qry_stop(QryBuffer *buffer, unsigned int milliseconds);

#ifdef __cplusplus
}
#endif

#endif
