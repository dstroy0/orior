// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_writer.c: the writer of a run. It makes the shared memory of the run and the rings of the members. Until the run
// stops, it takes each ring in turn and checks each record in it against its CRC.
//
// A record that matches goes at the end of the records of the run, at the next place of the schedule. Each reader
// reads it in the order that the writer took it. The writer also writes it at the end of the .qry.
//
// The writer takes the rings in the order that their records arrived, the earliest time first. A panic comes before a
// critical record, and a critical record comes before an ordinary record. The .qry is the records end to end (qry.h).
//
// Where the name of a record opens with `:`, the writer also writes it into a named stream of the .qry. The rest of
// its name names the stream, and the write comes as the writer takes it.
// As the run ends, the writer takes each record whose name is a path under the top of the tree. It writes the latest
// record of each name in the place of that file.
//
//   - orior_qry_writer <qry> <top of the tree>      the writer of the run, until a stop request.
//   - orior_qry_writer --stop <qry>                 a request, as a member, that the writer of the run of <qry> stops.
//   - orior_qry_writer --filed <qry> <path>         the standard input, appended as a member as the file at <path>.
//
// The two member words are for a script, which is no member itself. The exit is 0 where each record and file went
// whole, or where the member did its task. It is 1 where not, and 2 where the words are not those
#include "../ksg/ksg_writing.h"
#include "../../../src/platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "qry_read.h"
#include "writer/qry_writer_internal.h"

#include <stdlib.h>
#include <string.h>

// the path of the .qry the writer writes
static const char *s_qry;

// The account of the writer of its run, its last record, QRY_WRITER_ACCOUNT. It holds the records that it put, their
// bytes, and the records that did not match their CRC. It holds the records that did not fit the run, and the panics.
// It holds the nanoseconds from its start to its account on the one clock of the run. Each is eight bytes
#define QRY_WRITER_ACCOUNT "writer"
static unsigned long long s_account[6];

// the most files that the run writes in their places, and the positions that find which names the run writes, twice as
// many
#define QRY_WRITER_FILES (1u << 16u)
#define QRY_WRITER_FILE_SLOTS (QRY_WRITER_FILES * 2u)

// The `bytes` bytes of the record at `record`, put at the end of the records of the run, at the next place of the
// schedule. The call also writes them at the end of the .qry. The result is 1, or 0 where the write was not whole.
// The call counts a record that does not fit the run or the schedule, and it goes nowhere
static int qry_writer_record_put(QryShared *shared, const unsigned char *record, unsigned long long bytes,
                                 long long file)
{
    const unsigned long long tail = atomic_load(&shared->tail);
    const unsigned long long scheduled = atomic_load(&shared->scheduled);
    if ((bytes > (shared->run_bytes - tail)) || (scheduled == shared->places))
    {
        atomic_fetch_add(&shared->unplaced, 1ULL);
        return 1;
    }
    // a record fits half a ring, whose bytes a size_t counts
    mmgr_bytes_copy(qry_shared_records(shared) + tail, record, (size_t)bytes);
    s_account[0] += 1ULL;
    s_account[1] += bytes;
    qry_shared_schedule(shared)[scheduled] = tail;
    atomic_store(&shared->tail, tail + bytes);
    atomic_store(&shared->scheduled, scheduled + 1ULL);
    QryRecord head;
    mmgr_bytes_copy(&head, record, sizeof(head));
    // the line that the first panic opens, and the first panic after a decision shut it
    const unsigned long long panicked = atomic_load(&shared->panicked);
    s_account[4] += (head.priority == QRY_PANIC) ? 1ULL : 0ULL;
    if ((head.priority == QRY_PANIC) && ((panicked == 0ULL) || (panicked == atomic_load(&shared->closed))))
    {
        atomic_store(&shared->panicked, scheduled + 1ULL);
    }
    // a panic or a critical record is on the disk before its member's ring moves past it, and so is its stream
    const int put = qry_writer_file_put(file, record, bytes);
    const char *const name = (const char *)(record + sizeof(head));
    const int streamed = (head.name_length < 2u) || (name[0] != ':') ||
                         qry_writer_stream_put(s_qry, name + 1, head.name_length - 1u,
                                               record + sizeof(head) + head.name_length, head.blob_length);
    return put && streamed && ((head.priority == QRY_ORDINARY) || qry_writer_file_durable(file));
}

// the record at the head of `ring`, past a wrap mark, its head into `head`: where it is, or NULL where the ring
// contains none
static const unsigned char *qry_writer_ring_head(QryRing *ring, QryRecord *head)
{
    const unsigned long long tail = atomic_load(&ring->tail);
    unsigned long long at = atomic_load(&ring->head);
    if (at >= tail)
    {
        return NULL;
    }
    unsigned long long place = at & (QRY_WRITER_RING_BYTES - 1ULL);
    unsigned long long time = 0ULL;
    mmgr_bytes_copy(&time, ring->bytes + place, sizeof(time));
    if (time == QRY_WRITER_WRAP)
    {
        at += QRY_WRITER_RING_BYTES - place;
        atomic_store(&ring->head, at);
        place = 0ULL;
        if (at >= tail)
        {
            return NULL;
        }
    }
    mmgr_bytes_copy(head, ring->bytes + place, sizeof(*head));
    return ring->bytes + place;
}

// the nanoseconds between the writer's looks over the rings for a member whose process ended
#define QRY_WRITER_RECLAIM_NANOSECONDS 1000000000ULL

// Each ring whose member process ended goes back, after the writer takes all that it holds, for another member to
// take. A member that ended without a goodbye, by a fault or a kill, keeps no ring.
// The code reads the process as ended before it reads the ring as empty. An ended process appends nothing after
static void qry_writer_rings_reclaimed(QryShared *shared)
{
    for (unsigned long long place = 0ULL; place < shared->members; place += 1ULL)
    {
        QryRing *const ring = qry_shared_ring(shared, place);
        unsigned long long owner = atomic_load(&ring->owner);
        const int ended = (owner != 0ULL) && qry_writer_process_ended(owner);
        if (ended && (atomic_load(&ring->head) == atomic_load(&ring->tail)))
        {
            atomic_compare_exchange_strong(&ring->owner, &owner, 0ULL);
        }
    }
}

// The next record to take. Of the heads of the rings, it is the head of the highest priority, and of those the earliest
// time. The call checks it against its CRC, puts it (qry_writer_record_put), and moves the head of its ring past it.
// `*taken` is 0 where no ring holds one. The result is 1, or 0 where the write was not whole
static int qry_writer_next_taken(QryShared *shared, long long file, int *taken)
{
    QryRing *chosen = NULL;
    const unsigned char *at = NULL;
    QryRecord first;
    for (unsigned long long place = 0ULL; place < shared->members; place += 1ULL)
    {
        QryRing *const ring = qry_shared_ring(shared, place);
        QryRecord head;
        const unsigned char *const record_at =
            (atomic_load(&ring->owner) != 0ULL) ? qry_writer_ring_head(ring, &head) : NULL;
        if ((record_at != NULL) && ((chosen == NULL) || (head.priority < first.priority) ||
                                    ((head.priority == first.priority) && (head.time < first.time))))
        {
            chosen = ring;
            at = record_at;
            first = head;
        }
    }
    *taken = (chosen != NULL);
    if (chosen == NULL)
    {
        return 1;
    }
    const unsigned long long head = atomic_load(&chosen->head);
    const unsigned long long bytes = QRY_RECORD_BYTES(first.name_length, first.blob_length);
    if ((first.name_length > QRY_NAME_MAX_LEN) || (bytes > (QRY_WRITER_RING_BYTES / 2ULL)))
    {
        // a head that contains no record's length: the ring is past reading, and what it contains goes nowhere
        atomic_fetch_add(&shared->corrupt, 1ULL);
        atomic_store(&chosen->head, atomic_load(&chosen->tail));
        return 1;
    }
    unsigned long long crc = 0ULL;
    mmgr_bytes_copy(&crc, at + bytes - 8ULL, sizeof(crc));
    int whole = 1;
    if (qry_record_crc(at, bytes - 8ULL) == crc)
    {
        whole = qry_writer_record_put(shared, at, bytes, file);
    }
    else
    {
        atomic_fetch_add(&shared->corrupt, 1ULL);
    }
    atomic_store(&chosen->head, head + bytes);
    return whole;
}

// The latest record of each name that is a path under the top of the tree, with the schedule read from its end back.
// The call writes each whole in the place of its file. Where it is of a type that ksg seals, the call seals it beside
// the file (ksg_writing.h). The result is 1, or 0 where one did not go
static int qry_writer_files_placed(QryShared *shared)
{
    static unsigned long long s_written[QRY_WRITER_FILE_SLOTS];
    static char s_placed[QRY_WRITER_TOP_MAX_LEN + QRY_NAME_MAX_LEN + 2u];
    static KsgWriting s_writing;
    _Static_assert(sizeof(s_placed) <= KSG_PATH_MAX_LEN, "a placed file's path fits a sealed writer's");
    const size_t top = mmgr_text_length(shared->top, PLATFORM_TEXT_TO_TERMINATOR);
    int whole = 1;
    for (unsigned long long at = atomic_load(&shared->scheduled); (top != 0u) && (at != 0ULL); at -= 1ULL)
    {
        const QryRecord *const record =
            (const QryRecord *)(qry_shared_records(shared) + qry_shared_schedule(shared)[at - 1ULL]);
        const char *const name = (const char *)(record + 1);
        if ((memchr(name, '/', record->name_length) == NULL) || (name[0] == '/') ||
            (memchr(name, ':', record->name_length) != NULL))
        {
            continue;
        }
        // the hash of the name, never 0, which marks an empty position. A name that the search finds in its position
        // came later in the run
        unsigned long long hash = 0xcbf29ce484222325ULL;
        for (unsigned int character = 0u; character < record->name_length; character += 1u)
        {
            hash = (hash ^ (unsigned char)name[character]) * 0x100000001b3ULL;
        }
        hash = (hash == 0ULL) ? 1ULL : hash;
        unsigned long long slot = hash & (QRY_WRITER_FILE_SLOTS - 1u);
        while ((s_written[slot] != 0ULL) && (s_written[slot] != hash))
        {
            slot = (slot + 1ULL) & (QRY_WRITER_FILE_SLOTS - 1u);
        }
        if (s_written[slot] == hash)
        {
            continue;
        }
        s_written[slot] = hash;
        mmgr_bytes_copy(s_placed, shared->top, top);
        s_placed[top] = '/';
        mmgr_bytes_copy(s_placed + top + 1u, name, record->name_length);
        s_placed[top + 1u + record->name_length] = '\0';
        if (mmgr_text_find(s_placed + top + 1u, "..", PLATFORM_TEXT_TO_TERMINATOR) != NULL)
        {
            continue;
        }
        const char *const blob = name + record->name_length;
        const int begun = ksg_writing_begun(&s_writing, s_placed, ".part");
        const int put = begun && ksg_writing_put(&s_writing, blob, record->blob_length);
        whole &= ksg_writing_ended(&s_writing, put) && put;
    }
    return whole;
}

// the standard input, which the --filed word appends as a file: at max what a record contains
static unsigned char s_input[QRY_WRITER_RING_BYTES / 2ULL];

// the member words. They ask the writer of the run of `qry` to stop. Where `path` is not NULL, they append the standard
// input as the file at `path`. The result is 0 where the task succeeded, 1 where not
static int qry_writer_member(const char *qry, const char *path)
{
    if (qry_member_joined(qry) == NULL)
    {
        return 1;
    }
    int done = 1;
    if (path == NULL)
    {
        qry_member_stopped();
    }
    else
    {
        const unsigned long long size = qry_writer_input_read(s_input, sizeof(s_input));
        done = (size != QRY_MEMBER_UNREAD) && (qry_member_filed(path, s_input, size) != 0ULL);
    }
    qry_member_parted();
    return done ? 0 : 1;
}

int main(int count, char **words)
{
    if ((count == 3) && (mmgr_text_equal(words[1], "--stop", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return qry_writer_member(words[2], NULL);
    }
    if ((count == 4) && (mmgr_text_equal(words[1], "--filed", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return qry_writer_member(words[2], words[3]);
    }
    if (((count != 3) && (count != 4)) || (words[1][0] == '-') ||
        (mmgr_text_length(words[2], PLATFORM_TEXT_TO_TERMINATOR) >= QRY_WRITER_TOP_MAX_LEN))
    {
        return 2;
    }
    // the region positioned from the request, the fourth word, and made whole
    const unsigned long long requested_bytes = (count == 4) ? strtoull(words[3], NULL, 10) : QRY_WRITER_RUN_DEFAULT;
    unsigned long long members = 0ULL;
    unsigned long long places = 0ULL;
    unsigned long long run_bytes = 0ULL;
    const unsigned long long region_bytes = qry_shared_positioned(
        (requested_bytes != 0ULL) ? requested_bytes : QRY_WRITER_RUN_DEFAULT, &members, &places, &run_bytes);
    QryShared *const shared = qry_writer_shared_made(words[1], region_bytes);
    const long long file = qry_writer_file_made(words[1]);
    if ((shared == NULL) || (file < 0))
    {
        return 1;
    }
    mmgr_bytes_copy(shared->top, words[2], mmgr_text_length(words[2], PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    shared->members = members;
    shared->places = places;
    shared->run_bytes = run_bytes;
    shared->rings_at = (sizeof(QryShared) + 63ULL) & ~63ULL;
    shared->schedule_at = shared->rings_at + (members * sizeof(QryRing));
    shared->records_at = shared->schedule_at + ((places * 8ULL + 63ULL) & ~63ULL);
    // the whole last, which a member reads to know that the run is in place
    atomic_store(&shared->whole, region_bytes);
    qry_writer_raised();
    s_qry = words[1];
    const unsigned long long started = qry_member_clock();
    int whole = 1;
    int stopping = 0;
    int taken = 1;
    unsigned long long reclaimed = started;
    // once the run stops, the writer takes each record in the rings before it ends
    while (!stopping || taken)
    {
        stopping = stopping || (atomic_load(&shared->stop) != 0u);
        whole &= qry_writer_next_taken(shared, file, &taken);
        if (!taken && !stopping)
        {
            const unsigned long long now = qry_member_clock();
            if ((now - reclaimed) >= QRY_WRITER_RECLAIM_NANOSECONDS)
            {
                qry_writer_rings_reclaimed(shared);
                reclaimed = now;
            }
            qry_writer_paused();
        }
    }
    // the account, put as the run's last record
    static unsigned char s_record[QRY_RECORD_BYTES(sizeof(QRY_WRITER_ACCOUNT) - 1u, sizeof(s_account))];
    const unsigned long long now = qry_member_clock();
    const QryRecord head = {now, sizeof(s_account), sizeof(QRY_WRITER_ACCOUNT) - 1u, QRY_ORDINARY};
    s_account[2] = atomic_load(&shared->corrupt);
    s_account[3] = atomic_load(&shared->unplaced);
    s_account[5] = now - started;
    mmgr_bytes_copy(s_record, &head, sizeof(head));
    mmgr_bytes_copy(s_record + sizeof(head), QRY_WRITER_ACCOUNT, sizeof(QRY_WRITER_ACCOUNT) - 1u);
    mmgr_bytes_copy(s_record + sizeof(head) + sizeof(QRY_WRITER_ACCOUNT) - 1u, s_account, sizeof(s_account));
    const unsigned long long crc = qry_record_crc(s_record, sizeof(s_record) - 8u);
    mmgr_bytes_copy(s_record + sizeof(s_record) - 8u, &crc, sizeof(crc));
    whole &= qry_writer_record_put(shared, s_record, sizeof(s_record), file);
    whole &= qry_writer_file_closed(file);
    whole &= qry_writer_files_placed(shared);
    // the run written whole: a member reading the stop sees the writer ended
    atomic_store(&shared->stop, 2u);
    qry_writer_shared_released(shared);
    return whole ? 0 : 1;
}
