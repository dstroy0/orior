// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_writer.c: a run's writer. It makes the run's shared memory and the members' rings, and until the run is stopped
// takes each ring in turn and holds every record in it to its CRC. A record that holds goes into the run's records at
// their end, its place the schedule's next, where every reader reads it in the order the writer took it, and is
// written at the .qry's end. The rings are taken in the order their records arrived, the earliest time first, a
// panic before a critical record and a critical record before an ordinary one. The .qry is the records end to end
// (qry.h). A record whose name opens with `:` is written as well into the .qry's named stream of the rest of its
// name, as it is taken. As the run ends, every record whose name is a path under the top of the tree, the latest of
// each name, is written in that file's place.
//
//     qry_writer <qry> <top of the tree>       the run's writer, until it is asked to stop
//     qry_writer --stop <qry>                    the writer of the run of <qry> asked to stop, as a member of it
//     qry_writer --filed <qry> <path>            the standard input appended as the file at <path>, as a member
//
// The two member words are for a script, which is no member itself. Exit 0 where every record and file went whole
// or the member did what it was asked, 1 where not, 2 where the words are not those
#include "../ksg/ksg_writing.h"
#include "qry_read.h"
#include "writer/qry_writer_internal.h"

#include <string.h>

// the path of the .qry the writer writes
static const char *s_qry;

// The writer's account of its run, its last record, QRY_WRITER_ACCOUNT: the records it put, their bytes, the records
// that did not hold to their CRC, those the run had no room for, the panics, and the nanoseconds from its start to
// its account on the run's one clock, eight bytes each
#define QRY_WRITER_ACCOUNT "writer"
static unsigned long long s_account[6];

// the most files the run writes in their places, and the slots that find which names are written, twice as many
#define QRY_WRITER_FILES (1u << 16u)
#define QRY_WRITER_FILE_SLOTS (QRY_WRITER_FILES * 2u)

// memory given to the `bytes` at `at` of a region whose given memory ends at `*committed`, a step at a time up to
// `whole`: 1, or 0 where the system gives none
static int qry_writer_grown(unsigned char *at, unsigned long long *committed, unsigned long long need,
                            unsigned long long whole)
{
    if (need <= *committed)
    {
        return 1;
    }
    const unsigned long long stepped =
        ((need + QRY_WRITER_COMMIT_BYTES - 1ull) / QRY_WRITER_COMMIT_BYTES) * QRY_WRITER_COMMIT_BYTES;
    const unsigned long long to = (stepped < whole) ? stepped : whole;
    if (!qry_shared_committed(at + *committed, to - *committed))
    {
        return 0;
    }
    *committed = to;
    return 1;
}

// The `bytes` bytes of the record at `record` put at the end of the run's records, its place the schedule's next, and
// written at the .qry's end: 1, or 0 where it was not written whole. A record the run or the schedule holds no room
// for is counted and goes nowhere
static int qry_writer_record_put(QryShared *shared, const unsigned char *record, unsigned long long bytes,
                                 long long file)
{
    const unsigned long long tail = atomic_load(&shared->tail);
    const unsigned long long scheduled = atomic_load(&shared->scheduled);
    if ((bytes > (QRY_WRITER_RUN_BYTES - tail)) || (scheduled == QRY_WRITER_SCHEDULE_PLACES) ||
        !qry_writer_grown(shared->records, &shared->records_committed, tail + bytes, QRY_WRITER_RUN_BYTES) ||
        !qry_writer_grown((unsigned char *)shared->schedule, &shared->schedule_committed, (scheduled + 1ull) * 8ull,
                          QRY_WRITER_SCHEDULE_PLACES * 8ull))
    {
        atomic_fetch_add(&shared->unplaced, 1ull);
        return 1;
    }
    // a record fits half a ring, whose bytes a size_t counts
    memcpy(shared->records + tail, record, (size_t)bytes);
    s_account[0] += 1ull;
    s_account[1] += bytes;
    shared->schedule[scheduled] = tail;
    atomic_store(&shared->tail, tail + bytes);
    atomic_store(&shared->scheduled, scheduled + 1ull);
    QryRecord head;
    memcpy(&head, record, sizeof(head));
    // the line opened by the first panic, and by the first after a ruling closed it
    const unsigned long long panicked = atomic_load(&shared->panicked);
    s_account[4] += (head.priority == QRY_PANIC) ? 1ull : 0ull;
    if ((head.priority == QRY_PANIC) && ((panicked == 0ull) || (panicked == atomic_load(&shared->closed))))
    {
        atomic_store(&shared->panicked, scheduled + 1ull);
    }
    // a panic or a critical record is on the disk before its member's ring moves past it, and so is its stream
    const int put = qry_writer_file_put(file, record, bytes);
    const char *const name = (const char *)(record + sizeof(head));
    const int streamed = (head.name_length < 2u) || (name[0] != ':') ||
                         qry_writer_stream_put(s_qry, name + 1, head.name_length - 1u,
                                               record + sizeof(head) + head.name_length, head.blob_length);
    return put && streamed && ((head.priority == QRY_ORDINARY) || qry_writer_file_durable(file));
}

// the record at the head of `ring`, past a wrap mark, its head into `head`: where it stands, or NULL where the ring
// holds none
static const unsigned char *qry_writer_ring_head(QryRing *ring, QryRecord *head)
{
    const unsigned long long tail = atomic_load(&ring->tail);
    unsigned long long at = atomic_load(&ring->head);
    if (at >= tail)
    {
        return NULL;
    }
    unsigned long long place = at & (QRY_WRITER_RING_BYTES - 1ull);
    unsigned long long time = 0ull;
    memcpy(&time, ring->bytes + place, sizeof(time));
    if (time == QRY_WRITER_WRAP)
    {
        at += QRY_WRITER_RING_BYTES - place;
        atomic_store(&ring->head, at);
        place = 0ull;
        if (at >= tail)
        {
            return NULL;
        }
    }
    memcpy(head, ring->bytes + place, sizeof(*head));
    return ring->bytes + place;
}

// The next record taken, of every ring's head the one of the most urgent priority and of those the earliest time,
// held to its CRC and put (qry_writer_record_put), its ring's head moved past it; `*taken` 0 where no ring holds one.
// 1, or 0 where it was not written whole
static int qry_writer_next_taken(QryShared *shared, long long file, int *taken)
{
    QryRing *chosen = NULL;
    const unsigned char *at = NULL;
    QryRecord first;
    for (unsigned int place = 0u; place < QRY_WRITER_MEMBERS; place += 1u)
    {
        QryRing *const ring = &shared->ring[place];
        QryRecord head;
        const unsigned char *const held =
            (atomic_load(&ring->owner) != 0ull) ? qry_writer_ring_head(ring, &head) : NULL;
        if ((held != NULL) && ((chosen == NULL) || (head.priority < first.priority) ||
                               ((head.priority == first.priority) && (head.time < first.time))))
        {
            chosen = ring;
            at = held;
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
    if ((first.name_length > QRY_NAME_LONGEST) || (bytes > (QRY_WRITER_RING_BYTES / 2ull)))
    {
        // a head that holds no record's length: the ring is past reading, and what it holds goes nowhere
        atomic_fetch_add(&shared->corrupt, 1ull);
        atomic_store(&chosen->head, atomic_load(&chosen->tail));
        return 1;
    }
    unsigned long long crc = 0ull;
    memcpy(&crc, at + bytes - 8ull, sizeof(crc));
    int whole = 1;
    if (qry_record_crc(at, bytes - 8ull) == crc)
    {
        whole = qry_writer_record_put(shared, at, bytes, file);
    }
    else
    {
        atomic_fetch_add(&shared->corrupt, 1ull);
    }
    atomic_store(&chosen->head, head + bytes);
    return whole;
}

// The latest record of each name that is a path under the top of the tree, the schedule read from its end back,
// written whole in its file's place, and sealed beside it where it is of a kind ksg seals (ksg_writing.h): 1, or 0
// where one was not
static int qry_writer_files_placed(QryShared *shared)
{
    static unsigned long long s_written[QRY_WRITER_FILE_SLOTS];
    static char s_placed[QRY_WRITER_TOP_LONGEST + QRY_NAME_LONGEST + 2u];
    static KsgWriting s_writing;
    _Static_assert(sizeof(s_placed) <= KSG_PATH_LONGEST, "a placed file's path fits a sealed writer's");
    const size_t top = strlen(shared->top);
    int whole = 1;
    for (unsigned long long at = atomic_load(&shared->scheduled); (top != 0u) && (at != 0ull); at -= 1ull)
    {
        const QryRecord *const record = (const QryRecord *)(shared->records + shared->schedule[at - 1ull]);
        const char *const name = (const char *)(record + 1);
        if ((memchr(name, '/', record->name_length) == NULL) || (name[0] == '/') ||
            (memchr(name, ':', record->name_length) != NULL))
        {
            continue;
        }
        // the name's hash, never 0, which marks an empty slot; a name found in its slot was written later in the run
        unsigned long long hash = 0xcbf29ce484222325ull;
        for (unsigned int letter = 0u; letter < record->name_length; letter += 1u)
        {
            hash = (hash ^ (unsigned char)name[letter]) * 0x100000001b3ull;
        }
        hash = (hash == 0ull) ? 1ull : hash;
        unsigned long long slot = hash & (QRY_WRITER_FILE_SLOTS - 1u);
        while ((s_written[slot] != 0ull) && (s_written[slot] != hash))
        {
            slot = (slot + 1ull) & (QRY_WRITER_FILE_SLOTS - 1u);
        }
        if (s_written[slot] == hash)
        {
            continue;
        }
        s_written[slot] = hash;
        memcpy(s_placed, shared->top, top);
        s_placed[top] = '/';
        memcpy(s_placed + top + 1u, name, record->name_length);
        s_placed[top + 1u + record->name_length] = '\0';
        if (strstr(s_placed + top + 1u, "..") != NULL)
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

// the standard input, which the --filed word appends as a file: at most what a record holds
static unsigned char s_input[QRY_WRITER_RING_BYTES / 2ull];

// the member's words: the writer of the run of `qry` asked to stop, or the standard input appended as the file at
// `path` where `path` is not NULL. 0 where it was done, 1 where not
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
        done = (size != QRY_MEMBER_UNREAD) && (qry_member_filed(path, s_input, size) != 0ull);
    }
    qry_member_parted();
    return done ? 0 : 1;
}

int main(int count, char **words)
{
    if ((count == 3) && (strcmp(words[1], "--stop") == 0))
    {
        return qry_writer_member(words[2], NULL);
    }
    if ((count == 4) && (strcmp(words[1], "--filed") == 0))
    {
        return qry_writer_member(words[2], words[3]);
    }
    if ((count != 3) || (words[1][0] == '-') || (strlen(words[2]) >= QRY_WRITER_TOP_LONGEST))
    {
        return 2;
    }
    QryShared *const shared = qry_writer_shared_made(words[1]);
    const long long file = qry_writer_file_made(words[1]);
    if ((shared == NULL) || (file < 0))
    {
        return 1;
    }
    memcpy(shared->top, words[2], strlen(words[2]) + 1u);
    s_qry = words[1];
    const unsigned long long started = qry_member_clock();
    int whole = 1;
    int stopping = 0;
    int taken = 1;
    // once stopped, every record the rings hold is taken before the writer ends
    while (!stopping || taken)
    {
        stopping = stopping || (atomic_load(&shared->stop) != 0u);
        whole &= qry_writer_next_taken(shared, file, &taken);
        if (!taken && !stopping)
        {
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
    memcpy(s_record, &head, sizeof(head));
    memcpy(s_record + sizeof(head), QRY_WRITER_ACCOUNT, sizeof(QRY_WRITER_ACCOUNT) - 1u);
    memcpy(s_record + sizeof(head) + sizeof(QRY_WRITER_ACCOUNT) - 1u, s_account, sizeof(s_account));
    const unsigned long long crc = qry_record_crc(s_record, sizeof(s_record) - 8u);
    memcpy(s_record + sizeof(s_record) - 8u, &crc, sizeof(crc));
    whole &= qry_writer_record_put(shared, s_record, sizeof(s_record), file);
    whole &= qry_writer_file_closed(file);
    whole &= qry_writer_files_placed(shared);
    // the run written whole: a member reading the stop sees the writer ended
    atomic_store(&shared->stop, 2u);
    qry_writer_shared_released(shared);
    return whole ? 0 : 1;
}
