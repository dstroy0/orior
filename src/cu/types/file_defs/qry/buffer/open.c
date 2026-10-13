// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// open.c: the buffer created, opened and run, the trace it is opened by, its close, its panic line and a critical blob waited for
#include "qry_buffer_internal.h"

static QryBuffer *qry_buffer_new(const char *path)
{
    QryBuffer *const buffer = (QryBuffer *)calloc(1u, sizeof(QryBuffer));
    if (buffer != NULL)
    {
        snprintf(buffer->path, sizeof(buffer->path), "%s", path);
        buffer->own = QRY_MEMBERS;
        atomic_flag_clear(&buffer->adding);
#if !defined(_WIN32)
        buffer->memory = -1;
#endif
    }
    return buffer;
}

// the head, the index, the members' heads, the table, the members' rings and the blobs found in a view starting at
// `view`
static void qry_buffer_placed(QryBuffer *buffer, unsigned char *view)
{
    unsigned char *at = view;
    buffer->head = (QryHead *)at;
    at += QRY_HEAD_BYTES;
    buffer->slot = (QrySlot *)at;
    at += QRY_INDEX_SLOTS * QRY_SLOT_BYTES;
    buffer->member = (QryMember *)at;
    at += QRY_MEMBERS_BYTES;
    buffer->row = (QryRow *)at;
    at += QRY_TABLE_ROWS * (unsigned long long)sizeof(QryRow);
    buffer->ring = at;
    at += (unsigned long long)QRY_MEMBERS * QRY_MEMBER_BYTES;
    buffer->data = at;
}

QryBuffer *qry_create(const char *path)
{
    char memory_name[64];
    char lock_name[64];
    char wake_name[64];
    qry_names(path, memory_name, lock_name, wake_name, sizeof(memory_name));
    QryBuffer *const buffer = qry_buffer_new(path);
    if (buffer == NULL)
    {
        return NULL;
    }
    buffer->made = 1;
    const unsigned long long view_bytes = qry_view_bytes();
#if defined(_WIN32)
    // the whole view reserved and only the head and the index given memory now; the blobs' memory is given as they come
    buffer->section = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_RESERVE,
                                         (DWORD)(view_bytes >> 32u), (DWORD)(view_bytes & 0xffffffffull), memory_name);
    const int fresh = (buffer->section != NULL) && (GetLastError() != ERROR_ALREADY_EXISTS);
    unsigned char *const view =
        fresh ? (unsigned char *)MapViewOfFile(buffer->section, FILE_MAP_ALL_ACCESS, 0u, 0u, 0u) : NULL;
    const unsigned long long fixed =
        (unsigned long long)QRY_HEAD_BYTES + (QRY_INDEX_SLOTS * QRY_SLOT_BYTES) + QRY_MEMBERS_BYTES;
    if ((view == NULL) || (VirtualAlloc(view, (size_t)fixed, MEM_COMMIT, PAGE_READWRITE) == NULL))
    {
        if (view != NULL)
        {
            UnmapViewOfFile(view);
        }
        if (buffer->section != NULL)
        {
            CloseHandle(buffer->section);
        }
        free(buffer);
        return NULL;
    }
    qry_buffer_placed(buffer, view);
    buffer->lock = CreateMutexA(NULL, FALSE, lock_name);
    buffer->wake = CreateEventA(NULL, FALSE, FALSE, wake_name);
    if ((buffer->lock == NULL) || (buffer->wake == NULL))
    {
        qry_close(buffer);
        return NULL;
    }
    buffer->head->writer_process = (unsigned long long)GetCurrentProcessId();
#else
    snprintf(buffer->memory_name, sizeof(buffer->memory_name), "%s", memory_name);
    snprintf(buffer->wake_name, sizeof(buffer->wake_name), "%s", wake_name);
    buffer->memory = shm_open(memory_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if ((buffer->memory < 0) || (ftruncate(buffer->memory, (off_t)view_bytes) != 0))
    {
        if (buffer->memory >= 0)
        {
            close(buffer->memory);
            shm_unlink(memory_name);
        }
        free(buffer);
        return NULL;
    }
    buffer->view_bytes = view_bytes;
    buffer->view =
        (unsigned char *)mmap(NULL, (size_t)view_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, buffer->memory, 0);
    buffer->wake = sem_open(wake_name, O_CREAT | O_EXCL, 0600, 0u);
    if ((buffer->view == MAP_FAILED) || (buffer->wake == SEM_FAILED))
    {
        buffer->view = (buffer->view == MAP_FAILED) ? NULL : buffer->view;
        buffer->wake = (buffer->wake == SEM_FAILED) ? NULL : buffer->wake;
        qry_close(buffer);
        return NULL;
    }
    qry_buffer_placed(buffer, buffer->view);
    pthread_mutexattr_t kind;
    pthread_mutexattr_init(&kind);
    pthread_mutexattr_setpshared(&kind, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&kind, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&buffer->head->lock, &kind);
    pthread_mutexattr_destroy(&kind);
    buffer->head->writer_process = (unsigned long long)getpid();
#endif
    QryHead *const head = buffer->head;
    head->capacity = QRY_BUFFER_BYTES;
    head->slots = QRY_INDEX_SLOTS;
    head->sizes = qry_sizes();
    atomic_store(&head->writing, 1u);
    // the magic written last: a process that opens the buffer finds it whole or not at all
    atomic_thread_fence(memory_order_release);
    memcpy(head->magic, QRY_MAGIC, sizeof(QRY_MAGIC));
    return buffer;
}

QryBuffer *qry_open(const char *path)
{
    char memory_name[64];
    char lock_name[64];
    char wake_name[64];
    qry_names(path, memory_name, lock_name, wake_name, sizeof(memory_name));
    QryBuffer *const buffer = qry_buffer_new(path);
    if (buffer == NULL)
    {
        return NULL;
    }
#if defined(_WIN32)
    buffer->section = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, memory_name);
    unsigned char *const view = (buffer->section != NULL)
                                    ? (unsigned char *)MapViewOfFile(buffer->section, FILE_MAP_ALL_ACCESS, 0u, 0u, 0u)
                                    : NULL;
    if (view != NULL)
    {
        qry_buffer_placed(buffer, view);
    }
    buffer->lock = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, lock_name);
    buffer->wake = OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, wake_name);
    if ((view == NULL) || (buffer->lock == NULL) || (buffer->wake == NULL))
    {
        qry_close(buffer);
        return NULL;
    }
#else
    buffer->memory = shm_open(memory_name, O_RDWR, 0600);
    struct stat held;
    if ((buffer->memory < 0) || (fstat(buffer->memory, &held) != 0) ||
        ((unsigned long long)held.st_size != qry_view_bytes()))
    {
        qry_close(buffer);
        return NULL;
    }
    buffer->view_bytes = (unsigned long long)held.st_size;
    buffer->view =
        (unsigned char *)mmap(NULL, (size_t)buffer->view_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, buffer->memory, 0);
    buffer->wake = sem_open(wake_name, 0);
    if ((buffer->view == MAP_FAILED) || (buffer->wake == SEM_FAILED))
    {
        buffer->view = (buffer->view == MAP_FAILED) ? NULL : buffer->view;
        buffer->wake = (buffer->wake == SEM_FAILED) ? NULL : buffer->wake;
        qry_close(buffer);
        return NULL;
    }
    qry_buffer_placed(buffer, buffer->view);
#endif
    atomic_thread_fence(memory_order_acquire);
    if ((memcmp(buffer->head->magic, QRY_MAGIC, sizeof(QRY_MAGIC)) != 0) || (buffer->head->sizes != qry_sizes()))
    {
        qry_close(buffer);
        return NULL;
    }
#if defined(_WIN32)
    buffer->writer = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)buffer->head->writer_process);
#endif
    if (!qry_writer_alive(buffer))
    {
        qry_close(buffer);
        return NULL;
    }
    return buffer;
}

int qry_traced(const char *path, unsigned int priority)
{
    const char *const chosen = getenv(QRY_TRACE_ENVIRONMENT);
    if ((priority < QRY_ORDINARY) || (chosen == NULL) || (chosen[0] == '\0'))
    {
        return 1;
    }
    const size_t length = strlen(path);
    int named = 0;
    int kept = 0;
    int dropped = 0;
    const char *entry = chosen;
    while (*entry != '\0')
    {
        const size_t entry_length = strcspn(entry, ",");
        const int out = (entry[0] == '-');
        const char *const name = entry + (out ? 1 : 0);
        const size_t name_length = entry_length - (out ? 1u : 0u);
        const int same = (name_length == length) && (memcmp(name, path, length) == 0);
        if ((entry_length == 1u) && (entry[0] == '0'))
        {
            return 0;
        }
        dropped |= out && same;
        named |= !out && (name_length != 0u);
        kept |= !out && same;
        entry += entry_length + ((entry[entry_length] == ',') ? 1u : 0u);
    }
    return !dropped && (!named || kept);
}

QryBuffer *qry_run(void)
{
    // tried once a process, by the first thread to ask; the rest wait for its answer
    static _Atomic int s_state = 0;
    static QryBuffer *s_run = NULL;
    int expected = 0;
    if (atomic_compare_exchange_strong(&s_state, &expected, 1))
    {
        const char *const path = getenv(QRY_ENVIRONMENT);
        s_run = ((path != NULL) && (path[0] != '\0')) ? qry_open(path) : NULL;
        atomic_store(&s_state, 2);
    }
    while (atomic_load(&s_state) != 2)
    {
        qry_sleep_millisecond();
    }
    return s_run;
}

void qry_close(QryBuffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }
    // the ring this process holds moved and let go before the view goes
    if ((buffer->head != NULL) && (buffer->own != QRY_MEMBERS))
    {
        qry_member_released(buffer);
        qry_held_kept(buffer, 0);
    }
#if defined(_WIN32)
    if (buffer->head != NULL)
    {
        UnmapViewOfFile(buffer->head);
    }
    HANDLE handles[4] = {buffer->section, buffer->lock, buffer->wake, buffer->writer};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (handles[at] != NULL)
        {
            CloseHandle(handles[at]);
        }
    }
#else
    if (buffer->view != NULL)
    {
        munmap(buffer->view, (size_t)buffer->view_bytes);
    }
    if (buffer->memory >= 0)
    {
        close(buffer->memory);
    }
    if (buffer->wake != NULL)
    {
        sem_close(buffer->wake);
    }
    if (buffer->made)
    {
        shm_unlink(buffer->memory_name);
        sem_unlink(buffer->wake_name);
    }
#endif
    free(buffer);
}

const char *qry_path(const QryBuffer *buffer)
{
    return buffer->path;
}

unsigned long long qry_panicked(QryBuffer *buffer)
{
    if (buffer == NULL)
    {
        return 0ull;
    }
    const unsigned long long panic = atomic_load(&buffer->head->panicked);
    return (panic != atomic_load(&buffer->head->closed)) ? panic : 0ull;
}

void qry_line_closed(QryBuffer *buffer, unsigned long long panic)
{
    if (buffer != NULL)
    {
        atomic_store(&buffer->head->closed, panic);
    }
}

// 1 once the critical blob of `sequence` is on disk, 0 where the writer stopped or QRY_CRITICAL_WAIT_MILLISECONDS
// passed first
int qry_critical_waited(QryBuffer *buffer, unsigned long long sequence)
{
    const unsigned long long began = qry_milliseconds();
    while (atomic_load(&buffer->head->critical_written) < sequence)
    {
        if (!qry_writer_alive(buffer) || ((qry_milliseconds() - began) > QRY_CRITICAL_WAIT_MILLISECONDS))
        {
            return 0;
        }
        qry_sleep_millisecond();
    }
    return 1;
}
