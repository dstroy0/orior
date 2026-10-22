// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_member.c: a module's run found, its ring taken, a record written once into the ring with its time and CRC, the
// run's records read where the writer puts them, and the panic line
#include "qry_writer_internal.h"

#include "../qry_read.h"

#include <string.h>

// the run this process is a member of, the path of its .qry, and how many runs it has joined
static QryShared *s_run = NULL;
static int s_found = 0;
static char s_path[QRY_WRITER_TOP_LONGEST];
static unsigned long long s_joins = 0ull;

// the ring a thread of this process took, a module of the run each, one producer each ring, and the join of the run
// it was taken in
static _Thread_local QryRing *s_ring = NULL;
static _Thread_local unsigned long long s_ring_join = 0ull;

QryShared *qry_member_run(void)
{
    if (!s_found)
    {
        static char s_named[QRY_WRITER_TOP_LONGEST];
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
    s_joins += (s_run != NULL) ? 1ull : 0ull;
    return s_run;
}

void qry_member_parted(void)
{
    if ((s_run != NULL) && (s_ring != NULL) && (s_ring_join == s_joins))
    {
        qry_member_settled();
        atomic_store(&s_ring->owner, 0ull);
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
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
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
    QryShared *const run = qry_member_run();
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
    for (unsigned long long place = 0ull; place < run->members; place += 1ull)
    {
        unsigned long long none = 0ull;
        QryRing *const ring = qry_shared_ring(run, place);
        if (atomic_compare_exchange_strong(&ring->owner, &none, self))
        {
            s_ring = ring;
            break;
        }
    }
    return s_ring;
}

unsigned long long qry_member_append(const char *name, const void *blob, unsigned long long blob_length,
                                     unsigned int priority)
{
    QryRing *const ring = qry_member_ring();
    const size_t name_length = strlen(name);
    const unsigned long long bytes = QRY_RECORD_BYTES(name_length, blob_length);
    if ((ring == NULL) || (name_length == 0u) || (name_length > QRY_NAME_LONGEST) || (priority > QRY_ORDINARY) ||
        (bytes > (QRY_WRITER_RING_BYTES / 2ull)))
    {
        return 0ull;
    }
    const unsigned long long tail = atomic_load(&ring->tail);
    const unsigned long long place = tail & (QRY_WRITER_RING_BYTES - 1ull);
    const unsigned long long left = ((QRY_WRITER_RING_BYTES - place) < bytes) ? (QRY_WRITER_RING_BYTES - place) : 0ull;
    while ((tail + left + bytes - atomic_load(&ring->head)) > QRY_WRITER_RING_BYTES)
    {
    }
    if (left != 0ull)
    {
        const unsigned long long wrap = QRY_WRITER_WRAP;
        memcpy(ring->bytes + place, &wrap, sizeof(wrap));
    }
    unsigned char *const at = ring->bytes + ((tail + left) & (QRY_WRITER_RING_BYTES - 1ull));
    // a name is at most QRY_NAME_LONGEST bytes, which an unsigned int counts
    const QryRecord record = {qry_member_clock(), blob_length, (unsigned int)name_length, priority};
    const unsigned long long filled = sizeof(record) + name_length + blob_length;
    memcpy(at, &record, sizeof(record));
    memcpy(at + sizeof(record), name, name_length);
    // a record fits half a ring, whose bytes a size_t counts
    memcpy(at + sizeof(record) + name_length, blob, (size_t)blob_length);
    memset(at + filled, 0, (size_t)(bytes - 8ull - filled));
    const unsigned long long crc = qry_record_crc(at, bytes - 8ull);
    memcpy(at + bytes - 8ull, &crc, sizeof(crc));
    atomic_store(&ring->tail, tail + left + bytes);
    if (priority != QRY_ORDINARY)
    {
        qry_member_settled();
    }
    return record.time;
}

unsigned long long qry_member_settled(void)
{
    QryRing *const ring = qry_member_ring();
    while ((ring != NULL) && (atomic_load(&ring->head) < atomic_load(&ring->tail)))
    {
    }
    QryShared *const run = qry_member_run();
    return (run != NULL) ? atomic_load(&run->scheduled) : 0ull;
}

unsigned long long qry_member_rings_settled(void)
{
    unsigned long long tails[QRY_WRITER_MEMBERS_MOST];
    QryShared *const run = qry_member_run();
    if (run == NULL)
    {
        return 0ull;
    }
    for (unsigned long long place = 0ull; place < run->members; place += 1ull)
    {
        tails[place] = atomic_load(&qry_shared_ring(run, place)->tail);
    }
    for (unsigned long long place = 0ull; place < run->members; place += 1ull)
    {
        while (atomic_load(&qry_shared_ring(run, place)->head) < tails[place])
        {
        }
    }
    return atomic_load(&run->scheduled);
}

int qry_member_viewed_read(QryShared *run, unsigned long long *at, const QryRecord **record)
{
    if ((run == NULL) || (*at >= atomic_load(&run->scheduled)))
    {
        return 0;
    }
    *record = (const QryRecord *)(qry_shared_records(run) + qry_shared_schedule(run)[*at]);
    *at += 1ull;
    return 1;
}

int qry_member_read(unsigned long long *at, const QryRecord **record)
{
    // a walk begins once what this thread handed is taken into the run, so that it reads what this thread wrote
    if (*at == 0ull)
    {
        qry_member_settled();
    }
    return qry_member_viewed_read(qry_member_run(), at, record);
}

int qry_member_viewed_writing(QryShared *run)
{
    return (run != NULL) && (atomic_load(&run->stop) == 0u);
}

unsigned long long qry_member_viewed_panicked(QryShared *run)
{
    if (run == NULL)
    {
        return 0ull;
    }
    const unsigned long long panic = atomic_load(&run->panicked);
    return (panic != atomic_load(&run->closed)) ? panic : 0ull;
}

int qry_member_latest(const char *name, const QryRecord **record, unsigned long long *place)
{
    // what this thread handed taken into the run first, so that the latest is what this thread last wrote
    qry_member_settled();
    QryShared *const run = qry_member_run();
    const size_t length = strlen(name);
    for (unsigned long long at = (run != NULL) ? atomic_load(&run->scheduled) : 0ull; at != 0ull; at -= 1ull)
    {
        const QryRecord *const held =
            (const QryRecord *)(qry_shared_records(run) + qry_shared_schedule(run)[at - 1ull]);
        if ((held->name_length == length) && (memcmp(held + 1, name, length) == 0))
        {
            *record = held;
            if (place != NULL)
            {
                *place = at - 1ull;
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
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    const int appended = (bytes != NULL) && (size <= (QRY_WRITER_RING_BYTES / 2ull)) &&
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
    QryShared *const run = qry_member_run();
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
    return (name != NULL) ? qry_member_append(name, bytes, size, QRY_ORDINARY) : 0ull;
}

unsigned long long qry_member_streamed(const char *name, const void *bytes, unsigned long long size, char *path,
                                       size_t room)
{
    char streamed[QRY_NAME_LONGEST + 1u];
    const char *const qry = qry_member_path();
    const size_t length = strlen(name);
    const size_t qry_length = (qry != NULL) ? strlen(qry) : 0u;
    if ((qry == NULL) || (length == 0u) || (length >= QRY_NAME_LONGEST) || (strpbrk(name, "/\\:") != NULL) ||
        ((qry_length + 1u + length) >= room))
    {
        return 0ull;
    }
    streamed[0] = ':';
    memcpy(streamed + 1u, name, length + 1u);
    const unsigned long long time = qry_member_append(streamed, bytes, size, QRY_CRITICAL);
    memcpy(path, qry, qry_length);
    path[qry_length] = ':';
    memcpy(path + qry_length + 1u, name, length + 1u);
    return time;
}
