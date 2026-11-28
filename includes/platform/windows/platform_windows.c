// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_windows.c: the platform on Windows. The pages come from VirtualAlloc, a thread from CreateThread, an address
// wait from WaitOnAddress, the clock from the performance counter, and the streams from WriteFile
#include "../platform_internal.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include "platform_windows.h"

// the library of the address wait, which the linker takes from this object, and so no build names it
#if defined(_MSC_VER)
#pragma comment(lib, "synchronization.lib")
#endif

int platform_wide(const char *text, wchar_t *wide, unsigned int capacity)
{
    // a capacity past what an int counts reads as the most that it counts
    const int room = (capacity < 0x7FFFFFFFu) ? (int)capacity : 0x7FFFFFFF;
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, room) != 0;
}

int platform_narrow(const wchar_t *wide, char *text, unsigned int capacity)
{
    // a capacity past what an int counts reads as the most that it counts
    const int room = (capacity < 0x7FFFFFFFu) ? (int)capacity : 0x7FFFFFFF;
    return WideCharToMultiByte(CP_UTF8, 0u, wide, -1, text, room, NULL, NULL) != 0;
}

void *platform_pages_mapped(unsigned long long bytes)
{
    return VirtualAlloc(NULL, (SIZE_T)bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void platform_pages_released(void *at, unsigned long long bytes)
{
    // a release of a whole allocation names no size
    (void)bytes;
    (void)VirtualFree(at, 0u, MEM_RELEASE);
}

unsigned long long platform_page_bytes(void)
{
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    // Explicit cast widens the page size that the system gives
    return (unsigned long long)system.dwPageSize;
}

void *platform_pages_reserved(unsigned long long bytes)
{
    return VirtualAlloc(NULL, (SIZE_T)bytes, MEM_RESERVE, PAGE_NOACCESS);
}

int platform_pages_committed(void *at, unsigned long long bytes)
{
    return VirtualAlloc(at, (SIZE_T)bytes, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

typedef DWORD(WINAPI *PlatformPagesDiscard)(PVOID at, SIZE_T bytes);

void platform_pages_discarded(void *at, unsigned long long bytes)
{
    // The discard of the system takes the pages out of the working set at once, where the system has it. Else the
    // pages are marked unneeded, and the system takes them as it trims. The search runs once, on the first call
    static PlatformPagesDiscard s_discard;
    static int s_sought;
    if (!s_sought)
    {
        // Explicit cast takes the address of the procedure to the type of the call
        s_discard =
            (PlatformPagesDiscard)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "DiscardVirtualMemory");
        s_sought = 1;
    }
    if (s_discard != NULL)
    {
        (void)s_discard(at, (SIZE_T)bytes);
    }
    else
    {
        (void)VirtualAlloc(at, (SIZE_T)bytes, MEM_RESET, PAGE_READWRITE);
    }
}

// the threads that platform_thread_started keeps the records of. The mover is the single thread that the platform
// starts
#define PLATFORM_THREADS_MAX 4u
static PlatformThread s_threads[PLATFORM_THREADS_MAX];
static embed_atomic_u32 s_threads_given = EMBED_ATOMIC_INIT(0u);

static DWORD WINAPI platform_thread_entered(LPVOID supplied)
{
    // Explicit cast takes the pointer that the starter gave as the thread that it holds
    const PlatformThread *const thread = (const PlatformThread *)supplied;
    thread->run(thread->context);
    return 0u;
}

int platform_thread_let_go(PlatformThread *thread)
{
    const HANDLE started = CreateThread(NULL, 0u, platform_thread_entered, thread, 0u, NULL);
    if (started == NULL)
    {
        return 0;
    }
    (void)CloseHandle(started);
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

embed_word platform_thread_id(void)
{
    return (embed_word)GetCurrentThreadId();
}

// the entry of a worker's thread: its call run on its context
static DWORD WINAPI platform_worker_entered(LPVOID supplied)
{
    // Explicit cast takes the pointer that the starter gave as the worker that it holds
    const PlatformWorker *const worker = (const PlatformWorker *)supplied;
    worker->run(worker->context);
    return 0u;
}

int platform_worker_started(PlatformWorker *worker, void (*run)(void *context), void *context)
{
    worker->run = run;
    worker->context = context;
    worker->running = 0;
    const HANDLE started = CreateThread(NULL, 0u, platform_worker_entered, worker, 0u, NULL);
    if (started == NULL)
    {
        return 0;
    }
    _Static_assert(sizeof(HANDLE) <= PLATFORM_WORKER_HANDLE_BYTES, "a HANDLE fits the handle of a worker");
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
    HANDLE started = NULL;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &started, .src = worker->handle, .bytes = sizeof(started));
    (void)WaitForSingleObject(started, INFINITE);
    (void)CloseHandle(started);
    worker->running = 0;
}

void platform_thread_yield(void)
{
    (void)SwitchToThread();
}

void platform_thread_priority(unsigned int level)
{
    // the levels of the platform in their order, each as the priority of a thread of Windows
    static const int s_levels[4] = {THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_BELOW_NORMAL,
                                    THREAD_PRIORITY_IDLE};
    (void)SetThreadPriority(GetCurrentThread(), s_levels[(level < 4u) ? level : PLATFORM_PRIORITY_WORK]);
}

void platform_paused(unsigned long long milliseconds)
{
    // a pause past what a DWORD counts sleeps the most that it counts below INFINITE
    Sleep((milliseconds < 0xFFFFFFFEULL) ? (DWORD)milliseconds : (DWORD)0xFFFFFFFEULL);
}

void platform_stack_put(VerbaLinea *line)
{
    void *frames[PLATFORM_STACK_FRAMES];
    const USHORT count = RtlCaptureStackBackTrace(1u, PLATFORM_STACK_FRAMES, frames, NULL);
    // Explicit casts read the image's start and each frame as the addresses that they are
    const uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    for (USHORT at = 0u; at < count; at += 1u)
    {
        const uintptr_t address = (uintptr_t)frames[at];
        mmgr_line_put(line, " +0x");
        mmgr_line_hex(line, (uint64_t)((address >= base) ? (address - base) : address), 1u);
    }
}

unsigned int platform_processors(void)
{
    const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return (count != 0u) ? (unsigned int)count : 1u;
}

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    embed_atomic_word *const word = args->word;
    const embed_atomic_word_value old_value = args->old_value;
    const embed_word nanoseconds = args->nanoseconds;
    embed_atomic_word_value compare_value = old_value;
    if (nanoseconds == 0u)
    {
        return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = word) != old_value;
    }
    // WaitOnAddress takes whole milliseconds. The limit rounds up, and a limit past what a DWORD counts waits that long
    const embed_u64 milliseconds =
        ((embed_u64)nanoseconds / 1000000ULL) + (((embed_u64)nanoseconds % 1000000ULL) != 0u);
    const DWORD waited = (nanoseconds == EMBED_WAIT_INFINITE) ? INFINITE
                         : (milliseconds < 0xfffffffeULL)     ? (DWORD)milliseconds
                                                              : (DWORD)0xfffffffeULL;
    (void)WaitOnAddress(&word->value, &compare_value, sizeof(compare_value), waited);
    return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = word) != old_value;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    // WakeByAddressAll takes the address of the word as a plain pointer. The address goes through uintptr_t, and so no
    // cast removes the qualifier of the atomic value
    WakeByAddressAll((void *)(uintptr_t)&args->word->value);
}

embed_word embed_clock_ns(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter_ticks;
    (void)QueryPerformanceFrequency(&frequency);
    (void)QueryPerformanceCounter(&counter_ticks);
    // The seconds of the ticks and their rest scale apart, and so the product stays within 64 bits. Explicit casts take
    // the counts as unsigned, and Windows gives both counts as positive values
    const embed_u64 clock_ns =
        ((embed_u64)(counter_ticks.QuadPart / frequency.QuadPart) * 1000000000ULL) +
        (((embed_u64)(counter_ticks.QuadPart % frequency.QuadPart) * 1000000000ULL) / (embed_u64)frequency.QuadPart);
    // Explicit cast takes the nanoseconds modulo the word, as the contract of the clock says
    return (embed_word)clock_ns;
}

void platform_terminal_write(int errors, const char *text, unsigned long long length)
{
    const HANDLE output = GetStdHandle(errors ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    unsigned long long put = 0ULL;
    while (put < length)
    {
        // a piece is a gibibyte at max, which a DWORD counts
        const DWORD piece = (DWORD)(((length - put) < (1ULL << 30u)) ? (length - put) : (1ULL << 30u));
        DWORD wrote = 0u;
        if (!WriteFile(output, text + put, piece, &wrote, NULL) || (wrote == 0u))
        {
            return;
        }
        put += wrote;
    }
}

int platform_terminal_is(int errors)
{
    // a console answers its mode, and a pipe or a file answers none
    DWORD mode = 0u;
    return (GetConsoleMode(GetStdHandle(errors ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE), &mode) != 0) ? 1 : 0;
}

// the UTF-16 units of a name or a value of the environment, its terminator among them, as Windows bounds them
#define PLATFORM_WIDE_ENVIRONMENT 32768u

int platform_environment_set(const char *name, const char *value)
{
    wchar_t wide_name[PLATFORM_WIDE_ENVIRONMENT];
    wchar_t wide_value[PLATFORM_WIDE_ENVIRONMENT];
    return platform_wide(name, wide_name, PLATFORM_WIDE_ENVIRONMENT) &&
           platform_wide(value, wide_value, PLATFORM_WIDE_ENVIRONMENT) &&
           (SetEnvironmentVariableW(wide_name, wide_value) != 0);
}

unsigned long long platform_environment(const char *name, char *out, unsigned long long capacity)
{
    if (capacity != 0ULL)
    {
        out[0] = '\0';
    }
    wchar_t wide_name[PLATFORM_WIDE_ENVIRONMENT];
    wchar_t wide_value[PLATFORM_WIDE_ENVIRONMENT];
    // the system answers the length where the value fits, and the room it needs where it does not
    const DWORD length = platform_wide(name, wide_name, PLATFORM_WIDE_ENVIRONMENT)
                             ? GetEnvironmentVariableW(wide_name, wide_value, PLATFORM_WIDE_ENVIRONMENT)
                             : 0u;
    // a capacity past what an unsigned int counts reads as the most that it counts
    const unsigned int room = (unsigned int)((capacity < 0xFFFFFFFFULL) ? capacity : 0xFFFFFFFFULL);
    if ((length == 0u) || (length >= PLATFORM_WIDE_ENVIRONMENT) || !platform_narrow(wide_value, out, room))
    {
        if (capacity != 0ULL)
        {
            out[0] = '\0';
        }
        return 0ULL;
    }
    return (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = out, .cap = (size_t)capacity);
}

int platform_file_size(const char *path, unsigned long long *bytes)
{
    *bytes = 0ULL;
    wchar_t wide[PLATFORM_WIDE_PATH];
    WIN32_FILE_ATTRIBUTE_DATA found;
    if (!platform_wide(path, wide, PLATFORM_WIDE_PATH) || !GetFileAttributesExW(wide, GetFileExInfoStandard, &found))
    {
        return 0;
    }
    *bytes = ((unsigned long long)found.nFileSizeHigh << 32u) | (unsigned long long)found.nFileSizeLow;
    return 1;
}

// the file at the UTF-8 `path` opened to read, shared with every other opener, or INVALID_HANDLE_VALUE
static HANDLE platform_file_opened(const char *path)
{
    wchar_t wide[PLATFORM_WIDE_PATH];
    return platform_wide(path, wide, PLATFORM_WIDE_PATH)
               ? CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
               : INVALID_HANDLE_VALUE;
}

int platform_file_read(const char *path, void *out, unsigned long long capacity, unsigned long long *bytes)
{
    *bytes = 0ULL;
    const HANDLE file = platform_file_opened(path);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    LARGE_INTEGER size;
    // a file size is never negative
    int ok = (GetFileSizeEx(file, &size) != 0) && ((unsigned long long)size.QuadPart <= capacity);
    const unsigned long long whole = ok ? (unsigned long long)size.QuadPart : 0ULL;
    // Explicit cast reads the destination as its bytes
    char *const into = (char *)out;
    while (ok && (*bytes < whole))
    {
        // a piece is a gibibyte at max, which a DWORD counts
        const DWORD piece = (DWORD)(((whole - *bytes) < (1ULL << 30u)) ? (whole - *bytes) : (1ULL << 30u));
        DWORD got = 0u;
        ok = (ReadFile(file, into + *bytes, piece, &got, NULL) != 0) && (got != 0u);
        *bytes += got;
    }
    (void)CloseHandle(file);
    return ok;
}

// `count` bytes of `data` written to the file at the UTF-8 `path`, made new in place of any file there where `fresh`
// is 1, and after the end of the file there where `fresh` is 0: 1 where every byte went
static int platform_file_put(const char *path, const void *data, unsigned long long count, int fresh)
{
    wchar_t wide[PLATFORM_WIDE_PATH];
    const HANDLE file = platform_wide(path, wide, PLATFORM_WIDE_PATH)
                            ? CreateFileW(wide, (fresh != 0) ? GENERIC_WRITE : FILE_APPEND_DATA, 0u, NULL,
                                          (fresh != 0) ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
                            : INVALID_HANDLE_VALUE;
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    // Explicit cast reads the source as its bytes
    const char *const from = (const char *)data;
    unsigned long long put = 0ULL;
    int ok = 1;
    while (ok && (put < count))
    {
        // a piece is a gibibyte at max, which a DWORD counts
        const DWORD piece = (DWORD)(((count - put) < (1ULL << 30u)) ? (count - put) : (1ULL << 30u));
        DWORD wrote = 0u;
        ok = (WriteFile(file, from + put, piece, &wrote, NULL) != 0) && (wrote != 0u);
        put += wrote;
    }
    ok = (CloseHandle(file) != 0) && ok;
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
    const DWORD access = (type == 0) ? GENERIC_READ : ((type == 1) ? GENERIC_WRITE : FILE_APPEND_DATA);
    const DWORD disposition = (type == 0) ? OPEN_EXISTING : ((type == 1) ? CREATE_ALWAYS : OPEN_ALWAYS);
    // The file shares reads and writes with the other handles of it, as fopen shares them. A read then opens a file
    // that another handle holds open to write. A handle that shares no write keeps the refusal it asks for
    wchar_t wide[PLATFORM_WIDE_PATH];
    const HANDLE file = platform_wide(path, wide, PLATFORM_WIDE_PATH)
                            ? CreateFileW(wide, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, disposition,
                                          FILE_ATTRIBUTE_NORMAL, NULL)
                            : INVALID_HANDLE_VALUE;
    // Explicit cast carries the handle as the integer that platform_file.c holds, and a handle is never 0
    return (file == INVALID_HANDLE_VALUE) ? 0ULL : (unsigned long long)(uintptr_t)file;
}

unsigned long long platform_stream_put(unsigned long long handle, const void *bytes, unsigned long long count)
{
    // Explicit casts read the integer back as the handle and the source as its bytes
    const HANDLE file = (HANDLE)(uintptr_t)handle;
    const char *const from = (const char *)bytes;
    unsigned long long put = 0ULL;
    while (put < count)
    {
        // a piece is a gibibyte at max, which a DWORD counts
        const DWORD piece = (DWORD)(((count - put) < (1ULL << 30u)) ? (count - put) : (1ULL << 30u));
        DWORD wrote = 0u;
        if ((WriteFile(file, from + put, piece, &wrote, NULL) == 0) || (wrote == 0u))
        {
            return put;
        }
        put += wrote;
    }
    return put;
}

unsigned long long platform_stream_got(unsigned long long handle, void *out, unsigned long long capacity)
{
    // Explicit cast reads the integer back as the handle
    const HANDLE file = (HANDLE)(uintptr_t)handle;
    // a piece is a gibibyte at max, which a DWORD counts
    const DWORD piece = (DWORD)((capacity < (1ULL << 30u)) ? capacity : (1ULL << 30u));
    DWORD read = 0u;
    return (ReadFile(file, out, piece, &read, NULL) != 0) ? (unsigned long long)read : 0ULL;
}

long long platform_stream_placed(unsigned long long handle, long long offset, int whence)
{
    // Explicit cast reads the integer back as the handle
    const HANDLE file = (HANDLE)(uintptr_t)handle;
    LARGE_INTEGER distance;
    LARGE_INTEGER placed;
    distance.QuadPart = offset;
    const DWORD method = (whence == 0) ? FILE_BEGIN : ((whence == 1) ? FILE_CURRENT : FILE_END);
    return (SetFilePointerEx(file, distance, &placed, method) != 0) ? (long long)placed.QuadPart : -1LL;
}

int platform_stream_shut(unsigned long long handle)
{
    // Explicit cast reads the integer back as the handle
    return CloseHandle((HANDLE)(uintptr_t)handle) != 0;
}

int platform_file_relocated(const char *from, const char *to)
{
    wchar_t wide_from[PLATFORM_WIDE_PATH];
    wchar_t wide_to[PLATFORM_WIDE_PATH];
    if (!platform_wide(from, wide_from, PLATFORM_WIDE_PATH) || !platform_wide(to, wide_to, PLATFORM_WIDE_PATH))
    {
        return PLATFORM_MOVE_FAILED;
    }
    if (MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING) != 0)
    {
        return PLATFORM_MOVE_DONE;
    }
    return (GetLastError() == ERROR_NOT_SAME_DEVICE) ? PLATFORM_MOVE_ACROSS : PLATFORM_MOVE_FAILED;
}

int platform_file_removed(const char *path)
{
    wchar_t wide[PLATFORM_WIDE_PATH];
    return platform_wide(path, wide, PLATFORM_WIDE_PATH) && (DeleteFileW(wide) != 0);
}

// 1 where the UTF-16 `wide` names a folder
static int platform_folder_is_wide(const wchar_t *wide)
{
    const DWORD attributes = GetFileAttributesW(wide);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u);
}

int platform_folder_is(const char *path)
{
    wchar_t wide[PLATFORM_WIDE_PATH];
    return platform_wide(path, wide, PLATFORM_WIDE_PATH) && platform_folder_is_wide(wide);
}

// the separators of a path on Windows, the one that Windows writes, and the mark that ends a drive's name
const char g_platform_separators[] = "/\\";
const char g_platform_separator[] = "\\";
const char g_platform_drive_mark = ':';
const char g_platform_list_separator = ';';

int platform_folder_made_as(const char *path, unsigned int mode)
{
    // a folder made on Windows takes the rights of the folder above it, and so the mode names nothing here
    (void)mode;
    wchar_t wide[PLATFORM_WIDE_PATH];
    return platform_wide(path, wide, PLATFORM_WIDE_PATH) &&
           ((CreateDirectoryW(wide, NULL) != 0) || platform_folder_is_wide(wide));
}

int platform_folder_listed(const char *path, PlatformFolderEntry each, void *context)
{
    char pattern[PLATFORM_PATH_BYTES];
    VerbaLinea line = mmgr_line_over(pattern, sizeof(pattern));
    mmgr_line_put(&line, path);
    mmgr_line_put(&line, "\\*");
    (void)mmgr_line_finish(&line);
    wchar_t wide[PLATFORM_WIDE_PATH];
    wchar_t wide_pattern[PLATFORM_WIDE_PATH];
    if (!mmgr_line_ok(&line) || !platform_wide(path, wide, PLATFORM_WIDE_PATH) || !platform_folder_is_wide(wide) ||
        !platform_wide(pattern, wide_pattern, PLATFORM_WIDE_PATH))
    {
        return 0;
    }
    WIN32_FIND_DATAW entry;
    const HANDLE search =
        FindFirstFileExW(wide_pattern, FindExInfoBasic, &entry, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (search == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    do
    {
        // a name of a file is at max MAX_PATH units, and each unit is at max three bytes of UTF-8
        char name[(3u * MAX_PATH) + 1u];
        const int named = platform_narrow(entry.cFileName, name, sizeof(name));
        const int dot = EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = ".", .ci = EMBED_FALSE) ||
                        EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "..", .ci = EMBED_FALSE);
        if (named && !dot)
        {
            each(context, name, (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u);
        }
    } while (FindNextFileW(search, &entry) != 0);
    (void)FindClose(search);
    return 1;
}

unsigned long long platform_process_id(void)
{
    return (unsigned long long)GetCurrentProcessId();
}

// the 100 ns ticks of the system's time from the start of 1601 to the start of 1970
#define PLATFORM_EPOCH_TICKS 116444736000000000ULL

unsigned long long platform_epoch_microseconds(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    // Explicit casts carry the two halves of the 100 ns ticks into one word
    const unsigned long long ticks =
        ((unsigned long long)now.dwHighDateTime << 32u) | (unsigned long long)now.dwLowDateTime;
    return (ticks > PLATFORM_EPOCH_TICKS) ? ((ticks - PLATFORM_EPOCH_TICKS) / 10ULL) : 0ULL;
}

void platform_process_exited(int code)
{
    // Explicit cast carries the exit code to the unsigned code of the system, as the C library carries it
    ExitProcess((UINT)code);
}

void *platform_library_opened(const char *name)
{
    // the call keeps a module handle as a plain pointer, as the other systems keep theirs
    return (void *)LoadLibraryA(name);
}

void *platform_library_symbol(void *library, const char *name)
{
    // the address of a symbol is the function or the object that it names. It stays a plain pointer until the caller
    // casts it back
    return (void *)GetProcAddress((HMODULE)library, name);
}

void platform_library_closed(void *library)
{
    (void)FreeLibrary((HMODULE)library);
}

long platform_program_run(const char *const *arguments, const char *log)
{
    return platform_program_run_in(arguments, NULL, log, NULL, 0);
}

// a file of the output of a program, open for the program to write and to pass on. The call makes or replaces it, or
// writes after its end where `append` is 1
static HANDLE platform_run_file(const char *path, int append)
{
    SECURITY_ATTRIBUTES inherited = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    return CreateFileA(path, (append != 0) ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ, &inherited,
                       (append != 0) ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

long platform_program_run_in(const char *const *arguments, const char *folder, const char *output, const char *errors,
                             int append)
{
    // the command line: each argument in quotes, a space between them. The line has the bound that Windows sets on a
    // command line, and a line past it does not start
    char line[PLATFORM_COMMAND_LINE_BYTES];
    VerbaLinea command = mmgr_line_over(line, sizeof(line));
    for (unsigned long long at = 0ULL; arguments[at] != NULL; at += 1ULL)
    {
        mmgr_line_put(&command, (at == 0ULL) ? "\"" : " \"");
        mmgr_line_put(&command, arguments[at]);
        mmgr_line_ch(&command, '"');
    }
    (void)mmgr_line_finish(&command);
    if (!mmgr_line_ok(&command))
    {
        return -1L;
    }
    const HANDLE out_file = platform_run_file(output, append);
    const HANDLE error_file = (errors != NULL) ? platform_run_file(errors, append) : out_file;
    if ((out_file == INVALID_HANDLE_VALUE) || (error_file == INVALID_HANDLE_VALUE))
    {
        if (out_file != INVALID_HANDLE_VALUE)
        {
            (void)CloseHandle(out_file);
        }
        if ((errors != NULL) && (error_file != INVALID_HANDLE_VALUE))
        {
            (void)CloseHandle(error_file);
        }
        return -1L;
    }
    STARTUPINFOA startup = {sizeof(STARTUPINFOA)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = out_file;
    startup.hStdError = error_file;
    PROCESS_INFORMATION process = {NULL, NULL, 0u, 0u};
    DWORD status = 1u;
    const BOOL started =
        CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, folder, &startup, &process);
    if (started)
    {
        (void)WaitForSingleObject(process.hProcess, INFINITE);
        (void)GetExitCodeProcess(process.hProcess, &status);
        (void)CloseHandle(process.hThread);
        (void)CloseHandle(process.hProcess);
    }
    (void)CloseHandle(out_file);
    if (errors != NULL)
    {
        (void)CloseHandle(error_file);
    }
    // an exit code of the system is a DWORD, and a long holds every code that a program returns
    return started ? (long)status : -1L;
}

#endif
