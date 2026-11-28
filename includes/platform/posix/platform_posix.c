// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_posix.c: the platform on the POSIX systems. The pages come from mmap, a thread from pthread_create, the
// clock from the monotonic clock, and the streams from write. The address wait, the id and the level of a thread are
// the system's own (linux/platform_linux.c, macos/platform_macos.c)
#if !defined(_WIN32)
// the count of processors online and the monotonic clock are the system's, outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "platform_posix.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if !defined(_WIN32)
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// the environment of the process, which posix_spawnp passes to the program that it starts
extern char **environ;

void *platform_pages_mapped(unsigned long long bytes)
{
    void *const at = mmap(NULL, (size_t)bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (at == MAP_FAILED) ? NULL : at;
}

void platform_pages_released(void *at, unsigned long long bytes)
{
    (void)munmap(at, (size_t)bytes);
}

unsigned long long platform_page_bytes(void)
{
    // The kernel sets the page size, and it is not the same on all machines. If the system does not answer, the
    // value is 65536, the largest page of aarch64. A page that is too large keeps a release inside its span. A page
    // that is too small lets madvise clear the bytes after the span.
    const long page = sysconf(_SC_PAGESIZE);
    // Explicit cast takes the positive count that sysconf gives as the bytes it counts
    return (page > 0L) ? (unsigned long long)page : 65536ULL;
}

void *platform_pages_reserved(unsigned long long bytes)
{
    // the reserve maps the addresses with no access, and a commit of a span inside it opens that span
    void *const at = mmap(NULL, (size_t)bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (at == MAP_FAILED) ? NULL : at;
}

int platform_pages_committed(void *at, unsigned long long bytes)
{
    return mprotect(at, (size_t)bytes, PROT_READ | PROT_WRITE) == 0;
}

void platform_pages_discarded(void *at, unsigned long long bytes)
{
    (void)madvise(at, (size_t)bytes, g_platform_pages_discard);
}

// the threads that platform_thread_started keeps the records of. The mover is the single thread that the platform
// starts
#define PLATFORM_THREADS_MAX 4u
static PlatformThread s_threads[PLATFORM_THREADS_MAX];
static embed_atomic_u32 s_threads_given = EMBED_ATOMIC_INIT(0u);

static void *platform_thread_entered(void *supplied)
{
    const PlatformThread *const thread = supplied;
    thread->run(thread->context);
    return NULL;
}

int platform_thread_let_go(PlatformThread *thread)
{
    pthread_t started;
    if (pthread_create(&started, NULL, platform_thread_entered, thread) != 0)
    {
        return 0;
    }
    (void)pthread_detach(started);
    return 1;
}

int platform_thread_started(void (*run)(void *context), void *context)
{
    const embed_u32 place =
        EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_threads_given, .value = 1u);
    if (place >= PLATFORM_THREADS_MAX)
    {
        return 0;
    }
    s_threads[place] = (PlatformThread){run, context};
    return platform_thread_let_go(&s_threads[place]);
}


// the entry of a worker's thread: its call run on its context
static void *platform_worker_entered(void *supplied)
{
    // Explicit cast takes the pointer that the starter gave as the worker that it holds
    const PlatformWorker *const worker = (const PlatformWorker *)supplied;
    worker->run(worker->context);
    return NULL;
}

int platform_worker_started(PlatformWorker *worker, void (*run)(void *context), void *context)
{
    worker->run = run;
    worker->context = context;
    worker->running = 0;
    pthread_t started;
    if (pthread_create(&started, NULL, platform_worker_entered, worker) != 0)
    {
        return 0;
    }
    _Static_assert(sizeof(pthread_t) <= PLATFORM_WORKER_HANDLE_BYTES, "a pthread_t fits the handle of a worker");
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = worker->handle, .src = &started, .bytes = sizeof(started));
    worker->running = 1;
    return 1;
}

void platform_worker_joined(PlatformWorker *worker)
{
    if (worker->running == 0)
    {
        return;
    }
    pthread_t started;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &started, .src = worker->handle, .bytes = sizeof(started));
    (void)pthread_join(started, NULL);
    worker->running = 0;
}

void platform_thread_yield(void)
{
    (void)sched_yield();
}


void platform_paused(unsigned long long milliseconds)
{
    if (milliseconds == 0ULL)
    {
        (void)sched_yield();
        return;
    }
    // the seconds and the nanoseconds of the pause, each in the range of its field
    struct timespec pause;
    pause.tv_sec = (time_t)(milliseconds / 1000ULL);
    pause.tv_nsec = (long)((milliseconds % 1000ULL) * 1000000ULL);
    // a signal that ends the sleep early leaves the rest in `pause`, and the sleep goes on with it. Any other failure
    // ends the pause
    while ((nanosleep(&pause, &pause) != 0) && (errno == EINTR))
    {
    }
}

void platform_stack_put(VerbaLinea *line)
{
    void *frames[PLATFORM_STACK_FRAMES + 1u];
    const int count = backtrace(frames, (int)(PLATFORM_STACK_FRAMES + 1u));
    // the first frame is this call's own, and each after it is a return address in the object that holds it
    for (int at = 1; at < count; at += 1)
    {
        Dl_info where;
        // Explicit casts read each frame and its object's start as the addresses that they are
        const uintptr_t address = (uintptr_t)frames[at];
        const uintptr_t base =
            ((dladdr(frames[at], &where) != 0) && (where.dli_fbase != NULL)) ? (uintptr_t)where.dli_fbase : 0u;
        mmgr_line_put(line, " +0x");
        mmgr_line_hex(line, (uint64_t)(address - base), 1u);
    }
}

unsigned int platform_processors(void)
{
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    // a count of processors is positive and small, and an unsigned int holds it
    return (count > 0L) ? (unsigned int)count : 1u;
}

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    embed_atomic_word *const word = args->word;
    const embed_atomic_word_value old_value = args->old_value;
    const embed_word nanoseconds = args->nanoseconds;
    if (nanoseconds == 0u)
    {
        return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = word) != old_value;
    }
    platform_word_slept(word, old_value, nanoseconds);
    return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = word) != old_value;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    platform_word_woken(args->word);
}

embed_word embed_clock_ns(void)
{
    struct timespec clock_time;
    (void)clock_gettime(CLOCK_MONOTONIC, &clock_time);
    // the monotonic clock gives no negative field, and so both casts keep the value
    const embed_u64 clock_ns = ((embed_u64)clock_time.tv_sec * 1000000000ULL) + (embed_u64)clock_time.tv_nsec;
    // Explicit cast takes the nanoseconds modulo the word, as the contract of the clock says
    return (embed_word)clock_ns;
}

void platform_terminal_write(int errors, const char *text, unsigned long long length)
{
    unsigned long long put = 0ULL;
    while (put < length)
    {
        const ssize_t wrote = write(errors ? 2 : 1, text + put, (size_t)(length - put));
        if (wrote <= 0)
        {
            return;
        }
        // a write that put bytes answers a positive count
        put += (unsigned long long)wrote;
    }
}

int platform_terminal_is(int errors)
{
    return (isatty(errors ? 2 : 1) != 0) ? 1 : 0;
}

int platform_environment_set(const char *name, const char *value)
{
    return setenv(name, value, 1) == 0;
}

unsigned long long platform_environment(const char *name, char *out, unsigned long long capacity)
{
    const char *const value = getenv(name);
    if ((value == NULL) || (capacity == 0ULL))
    {
        if (capacity != 0ULL)
        {
            out[0] = '\0';
        }
        return 0ULL;
    }
    const unsigned long long length =
        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = value, .cap = (size_t)capacity);
    if (length >= capacity)
    {
        out[0] = '\0';
        return 0ULL;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = out, .src = value, .bytes = (size_t)length + 1u);
    return length;
}

int platform_file_size(const char *path, unsigned long long *bytes)
{
    struct stat status;
    const int read = stat(path, &status) == 0;
    // a file size is never negative
    *bytes = read ? (unsigned long long)status.st_size : 0ULL;
    return read;
}

int platform_file_read(const char *path, void *out, unsigned long long capacity, unsigned long long *bytes)
{
    *bytes = 0ULL;
    const int file = open(path, O_RDONLY);
    if (file < 0)
    {
        return 0;
    }
    struct stat status;
    // a file size is never negative
    int ok = (fstat(file, &status) == 0) && ((unsigned long long)status.st_size <= capacity);
    // The read runs to the end of the file, since a file of /proc gives a size of 0 and holds its text all the same. A
    // byte past `capacity` leaves the file unread. Explicit cast reads the destination as its bytes
    char *const into = (char *)out;
    while (ok)
    {
        char past;
        char *const at = (*bytes < capacity) ? (into + *bytes) : &past;
        const size_t wanted = (*bytes < capacity) ? (size_t)(capacity - *bytes) : 1u;
        const ssize_t got = read(file, at, wanted);
        if (got == 0)
        {
            break;
        }
        ok = (got > 0) && (*bytes < capacity);
        // a read that took bytes answers a positive count
        *bytes += ok ? (unsigned long long)got : 0ULL;
    }
    (void)close(file);
    return ok;
}

// `count` bytes of `data` written to the file at `path`. Where `fresh` is 1, the call makes the file new in place of
// any file there. Where `fresh` is 0, it writes after the end of the file there. The return is 1 where every byte
// went
static int platform_file_put(const char *path, const void *data, unsigned long long count, int fresh)
{
    // a file made new is the reader's to read and write, and its group's to read
    const int file = open(path, (fresh != 0) ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY | O_APPEND), 0640);
    if (file < 0)
    {
        return 0;
    }
    // Explicit cast reads the source as its bytes
    const char *const from = (const char *)data;
    unsigned long long put = 0ULL;
    int ok = 1;
    while (ok && (put < count))
    {
        const ssize_t wrote = write(file, from + put, (size_t)(count - put));
        ok = wrote > 0;
        // a write that put bytes answers a positive count
        put += ok ? (unsigned long long)wrote : 0ULL;
    }
    ok = (close(file) == 0) && ok;
    return ok;
}

int platform_file_written(const char *path, const void *data, unsigned long long count)
{
    return platform_file_put(path, data, count, 1);
}

int platform_file_appended(const char *path, const void *data, unsigned long long count)
{
    return platform_file_put(path, data, count, 0);
}

unsigned long long platform_stream_opened(const char *path, int type)
{
    // the flags of fopen for r, w and a, and the mode fopen gives a file it makes, which the umask then narrows
    const int flags =
        (type == 0) ? O_RDONLY : ((type == 1) ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY | O_CREAT | O_APPEND));
    const int file = open(path, flags, 0666);
    // Explicit cast carries the descriptor one up as the handle. A descriptor of 0 is a handle of 1
    return (file < 0) ? 0ULL : (unsigned long long)file + 1ULL;
}

unsigned long long platform_stream_put(unsigned long long handle, const void *bytes, unsigned long long count)
{
    // Explicit casts read the handle back as the descriptor and the source as its bytes
    const int file = (int)(handle - 1ULL);
    const char *const from = (const char *)bytes;
    unsigned long long put = 0ULL;
    while (put < count)
    {
        const ssize_t wrote = write(file, from + put, (size_t)(count - put));
        if (wrote <= 0)
        {
            return put;
        }
        // a write that put bytes answers a positive count
        put += (unsigned long long)wrote;
    }
    return put;
}

unsigned long long platform_stream_got(unsigned long long handle, void *out, unsigned long long capacity)
{
    // Explicit cast reads the handle back as the descriptor
    const ssize_t read_count = read((int)(handle - 1ULL), out, (size_t)capacity);
    // a read that took bytes answers a positive count
    return (read_count > 0) ? (unsigned long long)read_count : 0ULL;
}

long long platform_stream_placed(unsigned long long handle, long long offset, int whence)
{
    const int from = (whence == 0) ? SEEK_SET : ((whence == 1) ? SEEK_CUR : SEEK_END);
    // Explicit casts read the handle back as the descriptor and carry the place in the system's width
    const off_t placed = lseek((int)(handle - 1ULL), (off_t)offset, from);
    return (placed < 0) ? -1LL : (long long)placed;
}

int platform_stream_shut(unsigned long long handle)
{
    // Explicit cast reads the handle back as the descriptor
    return close((int)(handle - 1ULL)) == 0;
}

int platform_file_relocated(const char *from, const char *to)
{
    if (rename(from, to) == 0)
    {
        return PLATFORM_MOVE_DONE;
    }
    return (errno == EXDEV) ? PLATFORM_MOVE_ACROSS : PLATFORM_MOVE_FAILED;
}

int platform_file_removed(const char *path)
{
    return unlink(path) == 0;
}

// the separators of a path on the POSIX systems, and the one that they write. They have no mark for the name of a
// drive, because they have no drives
const char g_platform_separators[] = "/";
const char g_platform_separator[] = "/";
const char g_platform_drive_mark = '\0';
const char g_platform_list_separator = ':';

int platform_folder_is(const char *path)
{
    struct stat status;
    return (stat(path, &status) == 0) && S_ISDIR(status.st_mode);
}

int platform_folder_made_as(const char *path, unsigned int mode)
{
    // Explicit cast carries the permission bits to the mode type of the system
    return (mkdir(path, (mode_t)mode) == 0) || platform_folder_is(path);
}

int platform_folder_listed(const char *path, PlatformFolderEntry each, void *context)
{
    DIR *const folder = opendir(path);
    if (folder == NULL)
    {
        return 0;
    }
    for (const struct dirent *entry = readdir(folder); entry != NULL; entry = readdir(folder))
    {
        const int dot = EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = entry->d_name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = ".", .ci = EMBED_FALSE) ||
                        EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = entry->d_name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "..", .ci = EMBED_FALSE);
        if (dot)
        {
            continue;
        }
        // The entry's type comes with the entry on Linux and macOS. A stat runs only where the type is unknown, or
        // where the entry is a link, which can name a folder
        int is_folder = entry->d_type == DT_DIR;
        if ((entry->d_type == DT_UNKNOWN) || (entry->d_type == DT_LNK))
        {
            char full[PLATFORM_PATH_BYTES];
            VerbaLinea line = mmgr_line_over(full, sizeof(full));
            mmgr_line_put(&line, path);
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, entry->d_name);
            (void)mmgr_line_finish(&line);
            struct stat status;
            is_folder = mmgr_line_ok(&line) && (stat(full, &status) == 0) && S_ISDIR(status.st_mode);
        }
        each(context, entry->d_name, is_folder);
    }
    (void)closedir(folder);
    return 1;
}

unsigned long long platform_process_id(void)
{
    // a process id is a positive count
    return (unsigned long long)getpid();
}

unsigned long long platform_epoch_microseconds(void)
{
    struct timespec now;
    // Explicit casts carry the seconds and the nanoseconds of a time after 1970 to the unsigned count
    return ((clock_gettime(CLOCK_REALTIME, &now) == 0) && (now.tv_sec > 0))
               ? (((unsigned long long)now.tv_sec * 1000000ULL) + ((unsigned long long)now.tv_nsec / 1000ULL))
               : 0ULL;
}

void platform_process_exited(int code)
{
    _exit(code);
}

void *platform_library_opened(const char *name)
{
    return dlopen(name, RTLD_NOW | RTLD_LOCAL);
}

void *platform_library_symbol(void *library, const char *name)
{
    return dlsym(library, name);
}

void platform_library_closed(void *library)
{
    (void)dlclose(library);
}

long platform_program_run(const char *const *arguments, const char *log)
{
    return platform_program_run_in(arguments, NULL, log, NULL, 0);
}

long platform_program_run_in(const char *const *arguments, const char *folder, const char *output, const char *errors,
                             int append)
{
    const int flags = O_WRONLY | O_CREAT | ((append != 0) ? O_APPEND : O_TRUNC);
    posix_spawn_file_actions_t actions;
    (void)posix_spawn_file_actions_init(&actions);
    // the files open before the move to the folder, and so their paths count from the folder of this process
    (void)posix_spawn_file_actions_addopen(&actions, 1, output, flags, 0644);
    if (errors != NULL)
    {
        (void)posix_spawn_file_actions_addopen(&actions, 2, errors, flags, 0644);
    }
    else
    {
        (void)posix_spawn_file_actions_adddup2(&actions, 1, 2);
    }
    if (folder != NULL)
    {
        (void)posix_spawn_file_actions_addchdir_np(&actions, folder);
    }
    pid_t child = 0;
    // posix_spawnp takes the arguments as plain pointers, and it reads them alone
    const int started =
        posix_spawnp(&child, arguments[0], &actions, NULL, (char *const *)(uintptr_t)arguments, environ) == 0;
    (void)posix_spawn_file_actions_destroy(&actions);
    if (!started)
    {
        return -1L;
    }
    int status = 1;
    if (waitpid(child, &status, 0) != child)
    {
        return -1L;
    }
    return WIFEXITED(status) ? (long)WEXITSTATUS(status) : -1L;
}

#endif
