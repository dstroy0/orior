// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_member.c: a module's run found, its ring taken, a record written once into the ring with its time and CRC, the
// run's records read where the writer puts them, and the panic line
#include "qry_writer_internal.h"

#include "../qry_read.h"

#include <stdlib.h>
#include <string.h>

// the run this process is a member of, the path of its .qry, and how many runs it has joined
static QryShared *s_run = NULL;
static int s_found = 0;
static char s_path[QRY_WRITER_TOP_MAX_LEN];
static unsigned long long s_joins = 0ULL;

// the ring a thread of this process took, a module of the run each, one producer each ring, and the join of the run
// it was taken in
static _Thread_local QryRing *s_ring = NULL;
static _Thread_local unsigned long long s_ring_join = 0ULL;

// 1 once this process's exit is set to give its ring back (qry_member_exited)
static _Atomic int s_exit_set = 0;

// At this process's exit, the ring its exiting thread took given back once the writer has taken all it contains, or
// at once where the writer has ended, and the run's memory let go: a ring outlives none of its members, which the run
// has a fixed count of rings for
static void qry_member_exited(void)
{
    if ((s_run != NULL) && (s_ring != NULL) && (s_ring_join == s_joins))
    {
        while ((atomic_load(&s_ring->head) < atomic_load(&s_ring->tail)) && (atomic_load(&s_run->stop) != 2u))
        {
            // the ring waits for the writer to take what it holds, or to end
        }
        atomic_store(&s_ring->owner, 0ULL);
        s_ring = NULL;
    }
    qry_member_parted();
}

QryShared *qry_member_run(void)
{
    if (!s_found)
    {
        static char s_named[QRY_WRITER_TOP_MAX_LEN];
        qry_member_joined(qry_member_named(s_named, sizeof(s_named)) ? s_named : NULL);
    }
    return s_run;
}

QryShared *qry_member_joined(const char *path)
{
    qry_member_parted();
    s_found = 1;
    const size_t length = (path != NULL) ? strlen(path) : 0u;
    if ((length == 0u) || (length >= sizeof(s_path)))
    {
        return NULL;
    }
    memcpy(s_path, path, length + 1u);
    s_run = qry_member_shared(s_path);
    s_joins += (s_run != NULL) ? 1ULL : 0ULL;
    return s_run;
}

void qry_member_parted(void)
{
    if ((s_run != NULL) && (s_ring != NULL) && (s_ring_join == s_joins))
    {
        qry_member_settled();
        atomic_store(&s_ring->owner, 0ULL);
    }
    if (s_run != NULL)
    {
        qry_member_unshared(s_run);
    }
    s_run = NULL;
    s_ring = NULL;
    s_path[0] = '\0';
}

unsigned long long qry_member_gathered(const char *name, unsigned char *gathered, unsigned long long room)
{
    const size_t named = strlen(name);
    unsigned long long at = 0ULL;
    unsigned long long size = 0ULL;
    const QryRecord *record = NULL;
    while (qry_member_read(&at, &record))
    {
        if ((record->name_length != named) || (memcmp(QRY_RECORD_NAME(record), name, named) != 0))
        {
            continue;
        }
        if (record->blob_length > (room - size))
        {
            return QRY_MEMBER_UNREAD;
        }
        // a record fits half a ring, whose bytes a size_t counts
        memcpy(gathered + size, QRY_RECORD_BLOB(record), (size_t)record->blob_length);
        size += record->blob_length;
    }
    return size;
}

int qry_member_writer_runs(const char *path)
{
    QryShared *const run = qry_member_shared(path);
    const int runs = qry_member_viewed_writing(run);
    if (run != NULL)
    {
        qry_member_unshared(run);
    }
    return runs;
}

const char *qry_member_path(void)
{
    return (qry_member_run() != NULL) ? s_path : NULL;
}

const char *qry_member_top(void)
{
    const QryShared *const run = qry_member_run();
    return (run != NULL) ? run->top : NULL;
}

QryRing *qry_member_ring(void)
{
    QryShared *const run = qry_member_run();
    if (run == NULL)
    {
        return NULL;
    }
    if (s_ring_join == s_joins)
    {
        return s_ring;
    }
    // the first append of this thread in this run takes it a ring
    s_ring_join = s_joins;
    s_ring = NULL;
    const unsigned long long self = qry_member_process();
    for (unsigned long long place = 0ULL; place < run->members; place += 1ULL)
    {
        unsigned long long none = 0ULL;
        QryRing *const ring = qry_shared_ring(run, place);
        if (atomic_compare_exchange_strong(&ring->owner, &none, self))
        {
            s_ring = ring;
            break;
        }
    }
    // the exit set once a process, by the first of its threads to take a ring
    const int set = (s_ring != NULL) ? atomic_exchange(&s_exit_set, 1) : 1;
    if (set == 0)
    {
        atexit(qry_member_exited);
    }
    return s_ring;
}

unsigned long long qry_member_append(const char *name, const void *blob, unsigned long long blob_length,
                                     unsigned int priority)
{
    QryRing *const ring = qry_member_ring();
    const size_t name_length = strlen(name);
    const unsigned long long bytes = QRY_RECORD_BYTES(name_length, blob_length);
    if ((ring == NULL) || (name_length == 0u) || (name_length > QRY_NAME_MAX_LEN) || (priority > QRY_ORDINARY) ||
        (bytes > (QRY_WRITER_RING_BYTES / 2ULL)))
    {
        return 0ULL;
    }
    const unsigned long long tail = atomic_load(&ring->tail);
    const unsigned long long place = tail & (QRY_WRITER_RING_BYTES - 1ULL);
    const unsigned long long left = ((QRY_WRITER_RING_BYTES - place) < bytes) ? (QRY_WRITER_RING_BYTES - place) : 0ULL;
    while ((tail + left + bytes - atomic_load(&ring->head)) > QRY_WRITER_RING_BYTES)
    {
        // the record waits for the writer to take enough of the ring to fit it
    }
    if (left != 0ULL)
    {
        const unsigned long long wrap = QRY_WRITER_WRAP;
        memcpy(ring->bytes + place, &wrap, sizeof(wrap));
    }
    unsigned char *const at = ring->bytes + ((tail + left) & (QRY_WRITER_RING_BYTES - 1ULL));
    // a name is at most QRY_NAME_MAX_LEN bytes, which an unsigned int counts
    const QryRecord record = {qry_member_clock(), blob_length, (unsigned int)name_length, priority};
    const unsigned long long filled = sizeof(record) + name_length + blob_length;
    memcpy(at, &record, sizeof(record));
    memcpy(at + sizeof(record), name, name_length);
    // a record fits half a ring, whose bytes a size_t counts
    memcpy(at + sizeof(record) + name_length, blob, (size_t)blob_length);
    memset(at + filled, 0, (size_t)(bytes - 8ULL - filled));
    const unsigned long long crc = qry_record_crc(at, bytes - 8ULL);
    memcpy(at + bytes - 8ULL, &crc, sizeof(crc));
    atomic_store(&ring->tail, tail + left + bytes);
    if (priority != QRY_ORDINARY)
    {
        qry_member_settled();
    }
    return record.time;
}

unsigned long long qry_member_settled(void)
{
    const QryRing *const ring = qry_member_ring();
    while ((ring != NULL) && (atomic_load(&ring->head) < atomic_load(&ring->tail)))
    {
        // this thread waits for the writer to take what it handed
    }
    const QryShared *const run = qry_member_run();
    return (run != NULL) ? atomic_load(&run->scheduled) : 0ULL;
}

unsigned long long qry_member_rings_settled(void)
{
    unsigned long long tails[QRY_WRITER_MEMBERS_MOST];
    QryShared *const run = qry_member_run();
    if (run == NULL)
    {
        return 0ULL;
    }
    for (unsigned long long place = 0ULL; place < run->members; place += 1ULL)
    {
        tails[place] = atomic_load(&qry_shared_ring(run, place)->tail);
    }
    for (unsigned long long place = 0ULL; place < run->members; place += 1ULL)
    {
        while (atomic_load(&qry_shared_ring(run, place)->head) < tails[place])
        {
            // each ring waits for the writer to take it as far as its tail stood
        }
    }
    return atomic_load(&run->scheduled);
}

// the places of `run`'s schedule its writer has filled, no more than the schedule holds: 0 where there is no run
static unsigned long long qry_member_scheduled(const QryShared *run)
{
    if (run == NULL)
    {
        return 0ULL;
    }
    const unsigned long long scheduled = atomic_load(&run->scheduled);
    return (scheduled < run->places) ? scheduled : run->places;
}

int qry_member_viewed_read(QryShared *run, unsigned long long *at, const QryRecord **record)
{
    if ((run == NULL) || (*at >= qry_member_scheduled(run)))
    {
        return 0;
    }
    *record = (const QryRecord *)(qry_shared_records(run) + qry_shared_schedule(run)[*at]);
    *at += 1ULL;
    return 1;
}

int qry_member_read(unsigned long long *at, const QryRecord **record)
{
    // a walk begins once what this thread handed is taken into the run, so that it reads what this thread wrote
    if (*at == 0ULL)
    {
        qry_member_settled();
    }
    return qry_member_viewed_read(qry_member_run(), at, record);
}

int qry_member_viewed_writing(const QryShared *run)
{
    return (run != NULL) && (atomic_load(&run->stop) == 0u);
}

unsigned long long qry_member_viewed_panicked(const QryShared *run)
{
    if (run == NULL)
    {
        return 0ULL;
    }
    const unsigned long long panic = atomic_load(&run->panicked);
    return (panic != atomic_load(&run->closed)) ? panic : 0ULL;
}

int qry_member_latest(const char *name, const QryRecord **record, unsigned long long *place)
{
    // what this thread handed taken into the run first, so that the latest is what this thread last wrote
    qry_member_settled();
    QryShared *const run = qry_member_run();
    const size_t length = strlen(name);
    for (unsigned long long at = qry_member_scheduled(run); at != 0ULL; at -= 1ULL)
    {
        const QryRecord *const latest =
            (const QryRecord *)(qry_shared_records(run) + qry_shared_schedule(run)[at - 1ULL]);
        if ((latest->name_length == length) && (memcmp(latest + 1, name, length) == 0))
        {
            *record = latest;
            if (place != NULL)
            {
                *place = at - 1ULL;
            }
            return 1;
        }
    }
    return 0;
}

int qry_member_file(const char *path, const QryRecord **record)
{
    // a file the run takes in fits half a ring, as every record does, and is appended from its bytes mapped
    const char *const name = qry_member_file_name(path);
    if (name == NULL)
    {
        return 0;
    }
    if (qry_member_latest(name, record, NULL))
    {
        return 1;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    const int appended = (bytes != NULL) && (size <= (QRY_WRITER_RING_BYTES / 2ULL)) &&
                         qry_member_append(name, bytes, size, QRY_ORDINARY);
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    if (!appended)
    {
        return 0;
    }
    qry_member_settled();
    return qry_member_latest(name, record, NULL);
}

unsigned long long qry_member_panicked(void)
{
    return qry_member_viewed_panicked(qry_member_run());
}

void qry_member_line_closed(unsigned long long panic)
{
    QryShared *const run = qry_member_run();
    if (run != NULL)
    {
        atomic_store(&run->closed, panic);
    }
}

void qry_member_stopped(void)
{
    QryShared *const run = qry_member_run();
    if (run != NULL)
    {
        atomic_store(&run->stop, 1u);
    }
}

int qry_member_blob(const char *name, const unsigned char **blob, unsigned long long *size, unsigned long long *place)
{
    const QryRecord *record = NULL;
    if (!qry_member_latest(name, &record, place))
    {
        return 0;
    }
    *blob = QRY_RECORD_BLOB(record);
    *size = record->blob_length;
    return 1;
}

const char *qry_member_file_name(const char *path)
{
    const QryShared *const run = qry_member_run();
    const size_t top = (run != NULL) ? strlen(run->top) : 0u;
    if ((top == 0u) || (strlen(path) <= (top + 1u)) || (memcmp(path, run->top, top) != 0) || (path[top] != '/'))
    {
        return NULL;
    }
    return path + top + 1u;
}

int qry_member_file_blob(const char *path, const unsigned char **blob, unsigned long long *size)
{
    if (qry_member_run() == NULL)
    {
        // with no run, the file is the disk's, mapped where it lies for as long as the process runs
        *blob = qry_member_file_mapped(path, size);
        return *blob != NULL;
    }
    const QryRecord *record = NULL;
    if (!qry_member_file(path, &record))
    {
        return 0;
    }
    *blob = QRY_RECORD_BLOB(record);
    *size = record->blob_length;
    return 1;
}

unsigned long long qry_member_filed(const char *path, const void *bytes, unsigned long long size)
{
    const char *const name = qry_member_file_name(path);
    return (name != NULL) ? qry_member_append(name, bytes, size, QRY_ORDINARY) : 0ULL;
}

unsigned long long qry_member_streamed(const char *name, const void *bytes, unsigned long long size, char *path,
                                       size_t room)
{
    char streamed[QRY_NAME_MAX_LEN + 1u];
    const char *const qry = qry_member_path();
    const size_t length = strlen(name);
    const size_t qry_length = (qry != NULL) ? strlen(qry) : 0u;
    if ((qry == NULL) || (length == 0u) || (length >= QRY_NAME_MAX_LEN) || (strpbrk(name, "/\\:") != NULL) ||
        ((qry_length + 1u + length) >= room))
    {
        return 0ULL;
    }
    streamed[0] = ':';
    memcpy(streamed + 1u, name, length + 1u);
    const unsigned long long time = qry_member_append(streamed, bytes, size, QRY_CRITICAL);
    memcpy(path, qry, qry_length);
    path[qry_length] = ':';
    memcpy(path + qry_length + 1u, name, length + 1u);
    return time;
}
