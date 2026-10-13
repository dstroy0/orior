// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measures.c: the buffer's measures and its lock: the bytes its view takes, the hash and crc of bytes, the clock, the names of its memory, lock and wake, the lock taken and let go, the wake and the wait, and the bytes and rows committed
#include "qry_buffer_internal.h"

// the bytes the whole view takes: the head, the index, the members' heads, the table, the members' rings and the blobs
unsigned long long qry_view_bytes(void)
{
    return (unsigned long long)QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES) + QRY_MEMBERS_BYTES +
           (QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow)) +
           ((unsigned long long)QRY_MEMBERS * QRY_MEMBER_BYTES) + QRY_BUFFER_BYTES;
}

// the hash of `length` bytes at `text`, never 0, which marks an empty slot
unsigned long long qry_hash(const char *text, size_t length)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (size_t at = 0u; at < length; at += 1u)
    {
        hash = (hash ^ (unsigned char)text[at]) * 0x100000001b3ull;
    }
    return (hash == 0ull) ? 1ull : hash;
}

// CRC-64/XZ, the tree's own table (crc_key.h) and seven more made from it once a process, so that eight bytes are
// taken a step, each table carrying a byte one place further
static const unsigned long long s_qry_crc_table[256] = CRC_KEY_TABLE;

static unsigned long long s_qry_crc_slices[8][256];
static _Atomic int s_qry_crc_made = 0;

static void qry_crc_slices_made(void)
{
    if (atomic_load(&s_qry_crc_made) != 0)
    {
        return;
    }
    // two threads that make them at once make the same words
    for (unsigned int at = 0u; at < 256u; at += 1u)
    {
        s_qry_crc_slices[0][at] = s_qry_crc_table[at];
    }
    for (unsigned int slice = 1u; slice < 8u; slice += 1u)
    {
        for (unsigned int at = 0u; at < 256u; at += 1u)
        {
            const unsigned long long before = s_qry_crc_slices[slice - 1u][at];
            s_qry_crc_slices[slice][at] = (before >> 8u) ^ s_qry_crc_table[before & 0xffull];
        }
    }
    atomic_store(&s_qry_crc_made, 1);
}

// the CRC-64/XZ of `size` bytes at `bytes`
unsigned long long qry_crc(const unsigned char *bytes, unsigned long long size)
{
    qry_crc_slices_made();
    unsigned long long crc = ~0ull;
    unsigned long long at = 0ull;
    for (; (size - at) >= 8ull; at += 8ull)
    {
        unsigned long long word = 0ull;
        memcpy(&word, bytes + at, 8u);
        word ^= crc;
        crc = s_qry_crc_slices[7][word & 0xffull] ^ s_qry_crc_slices[6][(word >> 8u) & 0xffull] ^
              s_qry_crc_slices[5][(word >> 16u) & 0xffull] ^ s_qry_crc_slices[4][(word >> 24u) & 0xffull] ^
              s_qry_crc_slices[3][(word >> 32u) & 0xffull] ^ s_qry_crc_slices[2][(word >> 40u) & 0xffull] ^
              s_qry_crc_slices[1][(word >> 48u) & 0xffull] ^ s_qry_crc_slices[0][word >> 56u];
    }
    for (; at < size; at += 1ull)
    {
        crc = s_qry_crc_table[(crc ^ bytes[at]) & 0xffull] ^ (crc >> 8u);
    }
    return ~crc;
}

// the sizes this process was built with and the head's own, as one word: a process built with other sizes than the
// writer's reads the buffer wrong, and is refused it
unsigned long long qry_sizes(void)
{
    const unsigned long long sizes[] = {
        QRY_BUFFER_BYTES,  QRY_INDEX_SLOTS,    QRY_WRITE_BYTES, QRY_WRITE_MILLISECONDS,
        QRY_CRITICAL_ROOM, QRY_CRITICAL_SPANS, QRY_TABLE_ROWS,  (unsigned long long)sizeof(QryHead),
        QRY_MEMBERS,       QRY_MEMBER_BYTES};
    return qry_hash((const char *)sizes, sizeof(sizes));
}

// the wall clock's time, nanoseconds since 1970 in UTC
unsigned long long qry_nanoseconds(void)
{
#if defined(_WIN32)
    FILETIME now;
    GetSystemTimePreciseAsFileTime(&now);
    const unsigned long long ticks = ((unsigned long long)now.dwHighDateTime << 32u) | now.dwLowDateTime;
    // the file time counts hundreds of nanoseconds from 1601; the count from 1601 to 1970 is taken off
    return (ticks - 116444736000000000ull) * 100ull;
#else
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return ((unsigned long long)now.tv_sec * 1000000000ull) + (unsigned long long)now.tv_nsec;
#endif
}

// a clock that only goes forward, in milliseconds
unsigned long long qry_milliseconds(void)
{
#if defined(_WIN32)
    return (unsigned long long)GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((unsigned long long)now.tv_sec * 1000ull) + ((unsigned long long)now.tv_nsec / 1000000ull);
#endif
}

void qry_sleep_millisecond(void)
{
#if defined(_WIN32)
    Sleep(1u);
#else
    const struct timespec pause = {0, 1000000};
    nanosleep(&pause, NULL);
#endif
}

// 1 where `name` is one a blob takes: from 1 to QRY_NAME_LONGEST bytes, no space and no line's end
int qry_name_taken(const char *name, size_t *length)
{
    *length = (name != NULL) ? strlen(name) : 0u;
    return (*length != 0u) && (*length <= QRY_NAME_LONGEST) && (strpbrk(name, " \t\r\n\v\f") == NULL);
}

// the names the buffer's shared objects go by, from the hash of its path
void qry_names(const char *path, char *memory, char *lock, char *wake, size_t room)
{
    const unsigned long long hash = qry_hash(path, strlen(path));
#if defined(_WIN32)
    snprintf(memory, room, "Local\\qry-%016llx", hash);
    snprintf(lock, room, "Local\\qry-%016llx-lock", hash);
    snprintf(wake, room, "Local\\qry-%016llx-wake", hash);
#else
    snprintf(memory, room, "/qry-%016llx", hash);
    snprintf(lock, room, "/qry-%016llx-lock", hash);
    snprintf(wake, room, "/qry-%016llx-wake", hash);
#endif
}

void qry_lock(QryBuffer *buffer)
{
#if defined(_WIN32)
    // a hand ended mid-way left the lock abandoned: its blob was never published, and the lock is held as ever
    WaitForSingleObject(buffer->lock, INFINITE);
#else
    if (pthread_mutex_lock(&buffer->head->lock) == EOWNERDEAD)
    {
        pthread_mutex_consistent(&buffer->head->lock);
    }
#endif
}

void qry_unlock(QryBuffer *buffer)
{
#if defined(_WIN32)
    ReleaseMutex(buffer->lock);
#else
    pthread_mutex_unlock(&buffer->head->lock);
#endif
}

void qry_wake(QryBuffer *buffer)
{
#if defined(_WIN32)
    SetEvent(buffer->wake);
#else
    sem_post(buffer->wake);
#endif
}

// the writer's wait for a wake or `milliseconds`
void qry_wait(QryBuffer *buffer, unsigned long long milliseconds)
{
#if defined(_WIN32)
    WaitForSingleObject(buffer->wake, (DWORD)milliseconds);
#else
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    const unsigned long long nanoseconds = (unsigned long long)until.tv_nsec + ((milliseconds % 1000ull) * 1000000ull);
    until.tv_sec += (time_t)((milliseconds / 1000ull) + (nanoseconds / 1000000000ull));
    until.tv_nsec = (long)(nanoseconds % 1000000000ull);
    while ((sem_timedwait(buffer->wake, &until) != 0) && (errno == EINTR))
    {
    }
#endif
}

// 1 while the buffer's writer runs
int qry_writer_alive(QryBuffer *buffer)
{
    if (atomic_load(&buffer->head->writing) == 0u)
    {
        return 0;
    }
#if defined(_WIN32)
    return (buffer->writer == NULL) || (WaitForSingleObject(buffer->writer, 0u) == WAIT_TIMEOUT);
#else
    return (kill((pid_t)buffer->head->writer_process, 0) == 0) || (errno == EPERM);
#endif
}

// Memory given to the blobs through `need`, QRY_COMMIT_BYTES at a time, under the lock: 1, or 0 where the system gives
// none. Memory a process gives through its own view is the section's, and every process's view reads it
int qry_committed(QryBuffer *buffer, unsigned long long need)
{
    QryHead *const head = buffer->head;
    if (need <= head->committed)
    {
        return 1;
    }
    unsigned long long to = ((need + QRY_COMMIT_BYTES - 1ull) / QRY_COMMIT_BYTES) * QRY_COMMIT_BYTES;
    to = (to < head->capacity) ? to : head->capacity;
#if defined(_WIN32)
    // a step of memory is at most QRY_COMMIT_BYTES past need, which a size_t holds
    if (VirtualAlloc(buffer->data + head->committed, (size_t)(to - head->committed), MEM_COMMIT, PAGE_READWRITE) ==
        NULL)
    {
        return 0;
    }
#endif
    head->committed = to;
    return 1;
}

// Memory given to the table through `rows` rows, QRY_COMMIT_BYTES at a time, under the lock: 1, or 0 where the system
// gives none or the table holds no more rows
int qry_rows_committed(QryBuffer *buffer, unsigned long long rows)
{
    QryHead *const head = buffer->head;
    const unsigned long long need = rows * (unsigned long long)sizeof(QryRow);
    const unsigned long long whole = QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow);
    if (rows > QRY_TABLE_ROWS)
    {
        return 0;
    }
    if (need <= head->rows_committed)
    {
        return 1;
    }
    unsigned long long to = ((need + QRY_COMMIT_BYTES - 1ull) / QRY_COMMIT_BYTES) * QRY_COMMIT_BYTES;
    to = (to < whole) ? to : whole;
#if defined(_WIN32)
    // a step of memory is at most QRY_COMMIT_BYTES past need, which a size_t holds
    if (VirtualAlloc((unsigned char *)buffer->row + head->rows_committed, (size_t)(to - head->rows_committed),
                     MEM_COMMIT, PAGE_READWRITE) == NULL)
    {
        return 0;
    }
#endif
    head->rows_committed = to;
    return 1;
}
