// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// members.c: the members: a process's ring claimed, added to, moved by the writer and let go, the buffers held as a process ends, and the latest blob of a name
#include "qry_buffer_internal.h"

// this process's number, as the system gives it
static unsigned long long qry_process_self(void)
{
#if defined(_WIN32)
    return (unsigned long long)GetCurrentProcessId();
#else
    return (unsigned long long)getpid();
#endif
}

// 1 while the process `number` runs, and where the system will not say
static int qry_process_alive(unsigned long long number)
{
#if defined(_WIN32)
    const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)number);
    if (process == NULL)
    {
        return GetLastError() != ERROR_INVALID_PARAMETER;
    }
    const int alive = (WaitForSingleObject(process, 0u) == WAIT_TIMEOUT);
    CloseHandle(process);
    return alive;
#else
    return (kill((pid_t)number, 0) == 0) || (errno == EPERM);
#endif
}

static void qry_paused(unsigned int *waited)
{
    if (*waited < QRY_PAUSE_YIELDS)
    {
        *waited += 1u;
#if defined(_WIN32)
        SwitchToThread();
#else
        sched_yield();
#endif
        return;
    }
    qry_sleep_millisecond();
}

static QryBuffer *s_held_buffers[QRY_HELD_BUFFERS];
static atomic_flag s_held_turn = ATOMIC_FLAG_INIT;
static atomic_int s_held_ending = 0;

// every ring this process holds moved and let go, as the process ends
static void qry_held_ended(void)
{
    for (unsigned int at = 0u; at < QRY_HELD_BUFFERS; at += 1u)
    {
        if (s_held_buffers[at] != NULL)
        {
            qry_member_released(s_held_buffers[at]);
        }
    }
}

// `buffer` kept among those whose rings are let go as the process ends, or dropped from them where `kept` is 0
void qry_held_kept(QryBuffer *buffer, int kept)
{
    while (atomic_flag_test_and_set(&s_held_turn))
    {
    }
    int registered = 0;
    for (unsigned int at = 0u; (at < QRY_HELD_BUFFERS) && !registered; at += 1u)
    {
        if (kept ? (s_held_buffers[at] == NULL) : (s_held_buffers[at] == buffer))
        {
            s_held_buffers[at] = kept ? buffer : NULL;
            registered = 1;
        }
    }
    atomic_flag_clear(&s_held_turn);
    int none = 0;
    if (kept && atomic_compare_exchange_strong(&s_held_ending, &none, 1))
    {
        atexit(qry_held_ended);
    }
}

// The ring this process holds in `buffer`, taken the first time it hands an ordinary blob: a free one, or one whose
// process has ended with nothing left in it, its memory given. Its place, or QRY_MEMBERS where none can be had. Taken
// with the process's own turn at adding held (buffer->adding)
static unsigned int qry_member_claimed(QryBuffer *buffer)
{
    if (buffer->own != QRY_MEMBERS)
    {
        return buffer->own;
    }
    const unsigned long long self = qry_process_self();
    for (unsigned int pass = 0u; pass < 2u; pass += 1u)
    {
        for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
        {
            QryMember *const member = &buffer->member[place];
            unsigned long long owner = atomic_load(&member->owner);
            // the second pass takes back a ring whose process ended with nothing left in it
            const int free_ring = (owner == 0ull) || ((pass == 1u) && (owner != self) &&
                                                      (atomic_load(&member->head) == atomic_load(&member->tail)) &&
                                                      !qry_process_alive(owner));
            if (!free_ring || !atomic_compare_exchange_strong(&member->owner, &owner, self))
            {
                continue;
            }
#if defined(_WIN32)
            // a ring is QRY_MEMBER_BYTES, which a size_t counts
            if (VirtualAlloc(buffer->ring + ((unsigned long long)place * QRY_MEMBER_BYTES), (size_t)QRY_MEMBER_BYTES,
                             MEM_COMMIT, PAGE_READWRITE) == NULL)
            {
                atomic_store(&member->owner, 0ull);
                return QRY_MEMBERS;
            }
#endif
            buffer->own = place;
            qry_held_kept(buffer, 1);
            return place;
        }
    }
    return QRY_MEMBERS;
}

// the bytes a blob of a name of `length` and `size` bytes takes in a ring, a count of eight
static unsigned long long qry_ring_whole(size_t length, unsigned long long size)
{
    return ((unsigned long long)sizeof(QryRingBlob) + (unsigned long long)length + size + 7ull) & ~7ull;
}

// `size` bytes at `bytes` of `name` added at the tail of this process's ring `own`, made whole before the tail moves
// past them, a ring that holds no room for them first waited on: 1, or 0 where the writer stopped while it waited.
// Taken with the process's own turn at adding held (buffer->adding)
static int qry_member_added(QryBuffer *buffer, unsigned int own, const char *name, size_t length, const void *bytes,
                            unsigned long long size, unsigned int marks)
{
    QryMember *const member = &buffer->member[own];
    unsigned char *const ring = buffer->ring + ((unsigned long long)own * QRY_MEMBER_BYTES);
    const unsigned long long whole = qry_ring_whole(length, size);
    const unsigned long long tail = atomic_load(&member->tail);
    const unsigned long long place = tail & (QRY_MEMBER_BYTES - 1ull);
    // a blob that does not fit before the ring's end starts again at its start, past the stretch it leaves
    const unsigned long long left = ((QRY_MEMBER_BYTES - place) < whole) ? (QRY_MEMBER_BYTES - place) : 0ull;
    unsigned int waited = 0u;
    while ((tail + left + whole - atomic_load(&member->head)) > QRY_MEMBER_BYTES)
    {
        if (!qry_writer_alive(buffer))
        {
            return 0;
        }
        qry_wake(buffer);
        qry_paused(&waited);
    }
    // the stretch left marked as no blob: its count, and where it holds a blob's head, a head with no name
    if (left >= sizeof(QryRingBlob))
    {
        const QryRingBlob none = {left, 0ull, 0ull, 0ull, 0u, 0u};
        memcpy(ring + place, &none, sizeof(none));
    }
    else if (left != 0ull)
    {
        memcpy(ring + place, &left, sizeof(left));
    }
    unsigned char *const at = ring + ((tail + left) & (QRY_MEMBER_BYTES - 1ull));
    // a name is at most QRY_NAME_LONGEST bytes, which an unsigned int counts, and a blob that fits a ring is counted by
    // a size_t. The CRC is taken here, by the member, over the bytes as it hands them
    const QryRingBlob opened = {whole, qry_nanoseconds(), size, qry_crc((const unsigned char *)bytes, size),
                                (unsigned int)length, marks};
    memcpy(at, &opened, sizeof(opened));
    memcpy(at + sizeof(opened), name, length);
    if (size != 0ull)
    {
        memcpy(at + sizeof(opened) + length, bytes, (size_t)size);
    }
    const int was_empty = (atomic_load(&member->head) == tail);
    atomic_store(&member->tail, tail + left + whole);
    // the writer told once a ring holds something again, never for each blob
    if (was_empty)
    {
        qry_wake(buffer);
    }
    return 1;
}

// Every member's ring moved into the buffer, each in its turn in the order of the rings: on a ring's turn, every blob
// from its head to its tail as the turn reads it, under the lock a panic's or a critical hand takes, each given its
// sequence as it goes in; a ring that holds nothing has a turn that does nothing. The ring's head is moved past what
// was taken once all of it is in, and the sequence of the last blob kept as the ring's. 1 where any was moved
int qry_members_moved(QryBuffer *buffer)
{
    int moved = 0;
    for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
    {
        QryMember *const member = &buffer->member[place];
        const unsigned long long head = atomic_load(&member->head);
        const unsigned long long tail = atomic_load(&member->tail);
        if (head == tail)
        {
            continue;
        }
        const unsigned char *const ring = buffer->ring + ((unsigned long long)place * QRY_MEMBER_BYTES);
        unsigned long long last = 0ull;
        qry_lock(buffer);
        unsigned long long at = head;
        while (at < tail)
        {
            const unsigned char *const opened_at = ring + (at & (QRY_MEMBER_BYTES - 1ull));
            QryRingBlob opened;
            memcpy(&opened.whole, opened_at, sizeof(opened.whole));
            // a count that is no blob's ends the turn's reading, the ring taken whole
            if ((opened.whole < sizeof(opened.whole)) || ((opened.whole % 8ull) != 0ull) ||
                (opened.whole > (tail - at)))
            {
                break;
            }
            if (opened.whole >= sizeof(opened))
            {
                memcpy(&opened, opened_at, sizeof(opened));
            }
            // the stretch a blob left before the ring's end
            if ((opened.whole < sizeof(opened)) || (opened.name_length == 0u) ||
                (qry_ring_whole(opened.name_length, opened.size) != opened.whole))
            {
                at += opened.whole;
                continue;
            }
            // the bytes held to the CRC the member took as it handed them: a blob that does not hold is left in the
            // ring, counted, and never enters the run
            if (qry_crc(opened_at + sizeof(opened) + opened.name_length, opened.size) != opened.crc)
            {
                buffer->head->corrupt += 1ull;
                at += opened.whole;
                atomic_store(&member->head, at);
                continue;
            }
            int placed = 0;
            const unsigned long long sequence =
                qry_placed(buffer, (const char *)(opened_at + sizeof(opened)), (size_t)opened.name_length,
                           opened_at + sizeof(opened) + opened.name_length, opened.size, opened.crc, QRY_ORDINARY,
                           opened.marks, opened.nanoseconds, &placed);
            last = placed ? sequence : last;
            if (placed)
            {
                qry_tapped(buffer, sequence, opened.nanoseconds, QRY_ORDINARY, place,
                           (const char *)(opened_at + sizeof(opened)), (size_t)opened.name_length, opened.size);
            }
            at += opened.whole;
            // the ring's room given back blob by blob, for a member waiting on it to go on
            if (last != 0ull)
            {
                atomic_store(&member->moved, last);
            }
            atomic_store(&member->head, at);
        }
        qry_unlock(buffer);
        atomic_store(&member->head, tail);
        moved = 1;
    }
    return moved;
}

// 1 where every member's ring holds nothing
int qry_members_empty(QryBuffer *buffer)
{
    int empty = 1;
    for (unsigned int place = 0u; (place < QRY_MEMBERS) && empty; place += 1u)
    {
        empty = (atomic_load(&buffer->member[place].head) == atomic_load(&buffer->member[place].tail));
    }
    return empty;
}

unsigned long long qry_moved(QryBuffer *buffer)
{
    if (buffer == NULL)
    {
        return 0ull;
    }
    if (buffer->own == QRY_MEMBERS)
    {
        return buffer->last_placed;
    }
    QryMember *const member = &buffer->member[buffer->own];
    const unsigned long long tail = atomic_load(&member->tail);
    unsigned int waited = 0u;
    if (atomic_load(&member->head) < tail)
    {
        qry_wake(buffer);
    }
    while (atomic_load(&member->head) < tail)
    {
        if (!qry_writer_alive(buffer))
        {
            return 0ull;
        }
        qry_paused(&waited);
    }
    return buffer->last_ringed ? atomic_load(&member->moved) : buffer->last_placed;
}

int qry_settled(QryBuffer *buffer)
{
    if (buffer == NULL)
    {
        return 1;
    }
    unsigned long long tails[QRY_MEMBERS];
    for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
    {
        tails[place] = atomic_load(&buffer->member[place].tail);
    }
    qry_wake(buffer);
    for (unsigned int place = 0u; place < QRY_MEMBERS; place += 1u)
    {
        unsigned int waited = 0u;
        while (atomic_load(&buffer->member[place].head) < tails[place])
        {
            if (!qry_writer_alive(buffer))
            {
                return 0;
            }
            qry_paused(&waited);
        }
    }
    return 1;
}

// this process's ring moved into the buffer where it holds anything, before the process reads the buffer: a process
// reads what it handed itself
void qry_own_moved(QryBuffer *buffer)
{
    if ((buffer != NULL) && (buffer->own != QRY_MEMBERS) &&
        (atomic_load(&buffer->member[buffer->own].head) != atomic_load(&buffer->member[buffer->own].tail)))
    {
        qry_moved(buffer);
    }
}

// this process's ring moved and let go, where it holds one
void qry_member_released(QryBuffer *buffer)
{
    if (buffer->own == QRY_MEMBERS)
    {
        return;
    }
    qry_moved(buffer);
    atomic_store(&buffer->member[buffer->own].owner, 0ull);
    buffer->own = QRY_MEMBERS;
}

unsigned long long qry_handed(QryBuffer *buffer, const char *name, const void *bytes, unsigned long long size,
                                     unsigned int priority, unsigned int marks)
{
    size_t length = 0u;
    if ((buffer == NULL) || !qry_name_taken(name, &length) || ((bytes == NULL) && (size != 0ull)) ||
        (priority > QRY_ORDINARY))
    {
        return 0ull;
    }
    // an ordinary blob goes into this process's ring, where it holds one and the ring holds the blob within half of it.
    // The process that holds the writer has no turn to wait for: its blobs go in at the tail
    const int writer_here = (buffer->head->writer_process == qry_process_self());
    unsigned int waited = 0u;
    if (!writer_here && (priority == QRY_ORDINARY) && (marks == 0u) &&
        (qry_ring_whole(length, size) <= (QRY_MEMBER_BYTES / 2ull)))
    {
        while (atomic_flag_test_and_set(&buffer->adding))
        {
            qry_paused(&waited);
        }
        const unsigned int own = qry_member_claimed(buffer);
        const int added = (own != QRY_MEMBERS) && qry_member_added(buffer, own, name, length, bytes, size, marks);
        buffer->last_ringed = (own != QRY_MEMBERS) ? added : buffer->last_ringed;
        atomic_flag_clear(&buffer->adding);
        if (own != QRY_MEMBERS)
        {
            return added ? 1ull : 0ull;
        }
    }
    // an ordinary blob that goes in at the tail goes in after every blob this process handed before it
    if (priority == QRY_ORDINARY)
    {
        qry_moved(buffer);
    }
    QryHead *const head = buffer->head;
    // the CRC taken by the hander, before the lock: a panic's or a critical blob's bytes held to it on every read
    const unsigned long long crc = qry_crc((const unsigned char *)bytes, size);
    qry_lock(buffer);
    const unsigned long long written = atomic_load(&head->written);
    const unsigned long long before = atomic_load(&head->tail) - written;
    int placed = 0;
    const unsigned long long sequence =
        qry_placed(buffer, name, length, bytes, size, crc, priority, marks, 0ull, &placed);
    const unsigned long long after = atomic_load(&head->tail) - written;
    // an ordinary blob the writer's own process hands goes in here, past every ring: its line to the tap now; a panic
    // or a critical blob reaches the tap as the writer writes it first
    if (placed && (priority == QRY_ORDINARY))
    {
        qry_tapped(buffer, sequence, qry_nanoseconds(), priority, buffer->own, name, length, size);
    }
    qry_unlock(buffer);
    if (placed)
    {
        while (atomic_flag_test_and_set(&buffer->adding))
        {
            qry_paused(&waited);
        }
        buffer->last_ringed = 0;
        buffer->last_placed = sequence;
        atomic_flag_clear(&buffer->adding);
    }
    // the writer woken at once for a panic or a critical blob, and once as the queue crosses QRY_WRITE_BYTES, never
    // for each ordinary blob
    if (placed && ((priority < QRY_ORDINARY) || ((before < QRY_WRITE_BYTES) && (after >= QRY_WRITE_BYTES))))
    {
        qry_wake(buffer);
    }
    if (!placed)
    {
        return 0ull;
    }
    // a panic waits on nothing; a critical blob is on disk before its hand returns
    return ((priority != QRY_CRITICAL) || qry_critical_waited(buffer, sequence)) ? sequence : 0ull;
}

int qry_latest(QryBuffer *buffer, const char *name, const unsigned char **bytes, unsigned long long *size,
               unsigned long long *sequence)
{
    size_t length = 0u;
    if ((buffer == NULL) || !qry_name_taken(name, &length))
    {
        return 0;
    }
    qry_own_moved(buffer);
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    unsigned long long at = qry_index_find(buffer, name, length);
    QryBlob blob;
    if (at == 0ull)
    {
        if (buffer->head->index_full == 0ull)
        {
            return 0;
        }
        // a name the full index could not take is found by reading every blob
        for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
             walk = blob.next)
        {
            if ((blob.name_length == length) && (memcmp(buffer->data + blob.name_at, name, length) == 0))
            {
                at = walk + 1ull;
            }
        }
        if (at == 0ull)
        {
            return 0;
        }
    }
    if (!qry_blob_read(buffer->data, tail, at - 1ull, &blob))
    {
        return 0;
    }
    *bytes = buffer->data + blob.bytes_at;
    *size = blob.size;
    if (sequence != NULL)
    {
        *sequence = blob.sequence;
    }
    return 1;
}
