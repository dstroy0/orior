// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_buffer.h: a query run's buffer, its .qry in shared memory (qry.h). One writer process owns it for the run and is
// the only thing that writes the .qry to disk. Every other process of the run maps it by the .qry's path and is a
// member of the run: it hands its ordinary blobs into a ring of its own in the buffer, at the ring's tail, and nothing
// more, with no lock another member takes. The writer takes the members in turn, in the order of their rings, and on a
// member's turn moves every blob from its ring's head to its tail as it stands into the run's buffer in one go, each
// given its sequence as it is moved; the member keeps adding past what is taken, and a member whose ring holds nothing
// has a turn that does nothing. A panic and a critical blob wait for no turn: each goes in at the buffer's tail as it
// is handed, ahead of every member. The stretch past what is on disk is the writer's input queue. A process reads what
// it needs straight from the buffer, the latest blob of a name or every blob of it, before or after it reaches disk,
// once the writer has moved it; a read waits first for the reading process's own ring to be moved (qry_moved).
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
// The buffer keeps the run's table beside its blobs, a row a blob added as it goes in: the name's hash, the
// sequence, the time, the priority and where the blob's name and bytes sit. A read of every blob of a name, or of
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
// writer's is refused the buffer. The host's own, the widths, alignments and steps its sweep of timings found fastest,
// stand in qry_host.h beside this file where the sweep has written one (qry_sweep.c), and set those it names; the rest
// hold the values below.
#if defined(__has_include)
#if __has_include("qry_host.h")
#include "qry_host.h"
#endif
#endif
//
// the bytes of blobs a run's buffer holds, reserved at once and given memory QRY_COMMIT_BYTES at a time as it fills
#ifndef QRY_BUFFER_BYTES
#define QRY_BUFFER_BYTES (64ull << 30u)
#endif
#ifndef QRY_COMMIT_BYTES
#define QRY_COMMIT_BYTES (64ull << 20u)
#endif

// the boundary every frame and every blob's bytes start on, a power of two of at least eight bytes
#ifndef QRY_FRAME_ALIGN
#define QRY_FRAME_ALIGN 64ull
#endif

// the most one write to the disk takes: a stretch past it goes in writes of this many bytes
#ifndef QRY_WRITE_CALL_BYTES
#define QRY_WRITE_CALL_BYTES (1ull << 30u)
#endif

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

// the members a run's buffer keeps a ring for, each a process that hands the run an ordinary blob, and the bytes of
// each ring, a power of two. A ring's memory is given as a member takes it, and a ring whose member ended with nothing
// left in it is taken again. An ordinary blob past half a ring goes in at the buffer's tail as a critical one does,
// once its member's ring is moved
#ifndef QRY_MEMBERS
#define QRY_MEMBERS 256u
#endif
#ifndef QRY_MEMBER_BYTES
#define QRY_MEMBER_BYTES (4ull << 20u)
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

// the name a question's conference is handed under, followed by an underscore and the question's identity as R keeps
// it (record.h), each space of it an underscore, since a name takes no space: every party with an account of a
// question the part refused hands it under that name, and a reader joins them to hear the conference whole
// (overseer.h)
#define QRY_CONFERENCE_CALL "conference"

// the environment variable that names the run's .qry
#define QRY_ENVIRONMENT "QRY"

// the environment variable that names the top of the tree the run's current files stand in: a file is handed under
// its path from there (qry_file_named), and the run's writer writes no file outside it
#define QRY_TOP_ENVIRONMENT "QRY_TOP"

// The environment variable that chooses which paths hand their trace to the run's .qry (qry_traced). Unset, every path
// a run takes does. Set, it is a list of path names apart by commas: where it names any path plainly, only those
// trace and every other is kept out; a path named after a `-` is kept out and every other traces; `0` keeps every
// path out. A path's name is its own: klq_identity, the learner's trace and log; teacher, a line for every question
// and what came of it (run_channel.h); airlock, what its pop and its cross printed of each round;
// measuring_stick_query, what the vendor printed of each reading; measuring_stick_engine, each kernel's trace; and a
// program's own name, what it printed, where a script hands that to the run (qry_writer tee). A panic and a critical
// blob always reach the run, whatever path hands them: no setting keeps one out. What the run works from, a question,
// an answer, a container, a held list or a conference, is no trace and is never kept out
#define QRY_TRACE_ENVIRONMENT "QRY_TRACE"

    typedef struct QryBuffer QryBuffer;

    // the buffer of the .qry at `path` made, for its writer: NULL where one is open by that name already, or no memory
    QryBuffer *qry_create(const char *path);

    // the buffer of the .qry at `path`, made by a writer that runs: NULL where none is
    QryBuffer *qry_open(const char *path);

    // the run's buffer, the .qry QRY_ENVIRONMENT names, opened once a process: NULL where it names none or no writer
    // runs
    QryBuffer *qry_run(void);

    // 1 where the path named `path` hands its trace of `priority` to the run: always a panic's or a critical one's,
    // which nothing keeps out, and an ordinary one's where QRY_TRACE_ENVIRONMENT does not keep it out; 0 otherwise
    int qry_traced(const char *path, unsigned int priority);

    // the buffer let go by this process
    void qry_close(QryBuffer *buffer);

    // the path of the .qry the buffer is of
    const char *qry_path(const QryBuffer *buffer);

    // `size` bytes at `bytes` handed as a blob of `name` at `priority`. An ordinary blob goes into this process's ring
    // and has its sequence once the writer moves it (qry_moved): 1, or 0 where the ring stayed full until the writer
    // stopped. A panic or a critical blob goes in at the buffer's tail: its sequence, or 0 where it was dumped or it is
    // critical and could not be put on disk. A critical blob is on disk when this returns; a panic is in the buffer,
    // the writer woken for it and the line open, and nothing waits. 0 too where the name is none a blob takes or the
    // priority is past QRY_ORDINARY
    unsigned long long qry_hand(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                                unsigned int priority);

    // every ordinary blob this process has handed moved into the run's buffer, waiting for the writer's turn of this
    // process where any waits: the sequence of the last of them, or 0 where it handed none or the writer stopped first.
    // A process takes this before another process is to read what it handed: before it starts one, and before it waits
    // on an answer to what it handed
    unsigned long long qry_moved(QryBuffer *buffer);

    // every ordinary blob any member has handed by now moved into the run's buffer, waiting for each member's turn: 1,
    // or 0 where the writer stopped first. A process that started another takes this once the other has ended, and
    // then reads what the other handed, whether it ended clean or not
    int qry_settled(QryBuffer *buffer);

    // `size` bytes at `bytes` handed as a critical blob of `name` that the writer also writes into a named stream of
    // the .qry it writes, `<the .qry's path>:<name>`, for a program that reads its input only from a file's path, as
    // the vendor's disassembler does: no file but the .qry is written, and the stream goes with the .qry the writer
    // puts in its place as the run ends. Its sequence once the stream holds those bytes, read back and held to them,
    // with the stream's path in `path`, which holds `room`; 0 where it could not be had: the name holds `/`, `\` or
    // `:`, the system gives a file no named stream, or the writer did not write it. Only NTFS gives a file named
    // streams
    unsigned long long qry_streamed(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                                    char *path, size_t room);

    // The name the file at `path` is handed under, into `name`, which holds `room`: its path from the top of the tree
    // QRY_TOP_ENVIRONMENT names, `/` between its folders, where it stands under that top. 1, or 0 where it stands
    // outside it, no top is named, or the name does not fit
    int qry_file_named(const char *path, char *name, size_t room);

// the who table, from the top of the tree: every C and C++ source numbered in hex, a number never given twice
// (who_table.sh)
#define QRY_WHO_TABLE "src/cu/types/file_defs/who/who.tsv"

// the name of the blob each file written is followed by in the run: its sequence and the who of the write, three words
#define QRY_FILED_CALL "filed"

// the letters of a who's code, without its end
#define QRY_WHO_CODE_LETTERS 32u

    // A who, the code that made a write and when, as numbers in place of names: the number the who table gives the
    // source file, the line in it, the number it gives the program's own source, the source of the program's name,
    // and the time in nanoseconds since 1970 in UTC. A number of 0 is a source the table does not give. Its code is
    // its two words in hex, the first the source, the line and the program sixteen bits each above sixteen of 0, the
    // second the time, read back through the table as a stack trace is through its symbols
    typedef struct
    {
        unsigned int source;
        unsigned int line;
        unsigned int program;
        unsigned long long nanoseconds;
    } QryWho;

    // the who of the code at line `line` of the source `file`, as __FILE__ and __LINE__ give them, in this program,
    // now. The table is read once a process, as the run holds it
    QryWho qry_who(const char *file, unsigned int line);

// the who of the code this stands in
#define QRY_WHO_HERE qry_who(__FILE__, (unsigned int)__LINE__)

    // `who` as its two words, into `words`
    void qry_who_words(QryWho who, unsigned long long *words);

    // the who two words `words` hold
    QryWho qry_who_of_words(const unsigned long long *words);

    // `who`'s code into `code`, which holds `room`, QRY_WHO_CODE_LETTERS hex digits and its end
    void qry_who_code(QryWho who, char *code, size_t room);

    // the code `code` read back into `who`: 1, or 0 where it is not QRY_WHO_CODE_LETTERS hex digits
    int qry_who_read(const char *code, QryWho *who);

    // `who` named into `text`, which holds `room`: the source's path from the top of the tree and the line, the
    // program's source, and the time in UTC to the nanosecond, `-` for a number the table does not give. 1, or 0
    // where it does not fit
    int qry_who_named(QryWho who, char *text, size_t room);

    // `size` bytes at `bytes` handed as the current file at `path`, critical: the run's writer, the only thing that
    // writes a file, puts them in the .qry under the file's name (qry_file_named) before the hand returns, and once the
    // run ends writes the latest of the name in the file's place, beside it first and then put in its place whole. A
    // file does not change under a run in progress: while it runs, every program of the run reads the file as the run
    // holds it (qry_file_read). Every file a run's programs make, their thin wrappers of this, is written so: the
    // writer keeps the pressure of every write and the memory every one takes. The write is followed in the run by a
    // blob QRY_FILED_CALL, critical, its sequence and `who` made the write. Its sequence once the .qry holds it, or 0
    // where it does not or the path stands outside the top of the tree
    unsigned long long qry_filed_by(QryBuffer *buffer, const char *path, const void *bytes, unsigned long long size,
                                    QryWho who);

// qry_filed_by, the who the code that calls it
#define qry_filed(buffer_, path_, bytes_, size_) qry_filed_by((buffer_), (path_), (bytes_), (size_), QRY_WHO_HERE)

    // The file at `path` as the run holds it: the latest blob of its name (qry_file_named) in the run's buffer, or
    // where the run holds none, the file read whole from the disk and taken into the run under its name, ordinary, so
    // that every read of it after, by any process of the run, is from memory. Into memory of the caller's to free; 1,
    // or 0 where neither is there. With no run, `buffer` NULL, the file is read from the disk
    int qry_file_read(QryBuffer *buffer, const char *path, unsigned char **bytes, unsigned long long *size);

    // 1 where the run holds the file at `path` as a current file it is to write in its place as it ends (qry_filed),
    // 0 where it does not or the path stands outside the top of the tree
    int qry_file_held(QryBuffer *buffer, const char *path);

    // the line of `bytes`, `size` of them, from `*at` through its line's end copied into `line`, which holds `room`,
    // ended by a zero byte and cut to the room as fgets cuts one, and `*at` moved past what was copied: 1, or 0 where
    // no byte is left. A file read whole (qry_file_read) is read line by line by this as by fgets
    int qry_line_next(const unsigned char *bytes, unsigned long long size, unsigned long long *at, char *line,
                      size_t room);

    // text a program puts together in memory before it hands it to the run's writer whole: its bytes, their count and
    // the room they have, and 1 in `failed` where the system gave no memory for some of it. A text starts as zeros
    typedef struct
    {
        char *bytes;
        unsigned long long size;
        unsigned long long room;
        int failed;
    } QryText;

    // `format` and what follows it added to the end of `text`, as fprintf writes them
    void qry_text_printed(QryText *text, const char *format, ...);

    // `size` bytes at `bytes` added to the end of `text`
    void qry_text_added(QryText *text, const void *bytes, unsigned long long size);

    // `text` handed to the run's writer as the current file at `path` (qry_filed_by), written by `who`, and let go: its
    // sequence, or 0 where it was not written or some of it had no memory
    unsigned long long qry_text_filed_by(QryBuffer *buffer, const char *path, QryText *text, QryWho who);

// qry_text_filed_by, the who the code that calls it
#define qry_text_filed(buffer_, path_, text_) qry_text_filed_by((buffer_), (path_), (text_), QRY_WHO_HERE)

    // `text` let go unhanded
    void qry_text_freed(QryText *text);

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
    // the file no frame of qry.h opens, the ordinary bytes a run's end left unwritten before a panic written through
    // after them, is stepped over to the next frame that reads whole, and `error` says how many bytes were stepped
    // over; it is empty where none were
    int qry_file_joined(const char *path, const char *name, unsigned char **bytes, unsigned long long *size,
                        char *error, size_t room);

    // every name of the .qry file at `path` handed to `each` with `context` once, its latest blob by sequence, in the
    // order of those blobs' sequences: the name, the bytes and their count, in the mapped file and standing for the
    // call alone. A finished run's blobs are carried into a new run by this. 1, or 0 and why in `error` where the file
    // could not be read; a stretch no frame opens is stepped over and said, as qry_file_joined does
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
