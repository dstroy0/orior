// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_windows.c: the platform on Windows. The pages come from VirtualAlloc, a thread from CreateThread, an address
// wait from WaitOnAddress, the clock from the performance counter, and the streams from WriteFile
#include "platform_internal.h"
#include "cellularum_laboro/cellularum_linea.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the library of the address wait, which the linker takes from this object, and so no build names it
#if defined(_MSC_VER)
#pragma comment(lib, "synchronization.lib")
#endif

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

// what a thread of the platform runs, which the starter keeps for the life of the thread
typedef struct
{
    void (*run)(void *context);
    void *context;
} PlatformThread;

// the threads that the platform starts at once. The mover is the single thread that the platform starts
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

int platform_thread_started(void (*run)(void *context), void *context)
{
    const embed_u32 place =
        EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_threads_given, .value = 1u);
    if (place >= PLATFORM_THREADS_MAX)
    {
        return 0;
    }
    s_threads[place] = (PlatformThread){run, context};
    const HANDLE started = CreateThread(NULL, 0u, platform_thread_entered, &s_threads[place], 0u, NULL);
    if (started == NULL)
    {
        return 0;
    }
    (void)CloseHandle(started);
    return 1;
}

embed_word platform_thread_id(void)
{
    return (embed_word)GetCurrentThreadId();
}

void platform_thread_yield(void)
{
    (void)SwitchToThread();
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

int platform_environment_set(const char *name, const char *value)
{
    return SetEnvironmentVariableA(name, value) != 0;
}

unsigned long long platform_environment(const char *name, char *out, unsigned long long capacity)
{
    // a capacity past what a DWORD counts reads as the most that it counts
    const DWORD room = (DWORD)((capacity < 0xFFFFFFFFULL) ? capacity : 0xFFFFFFFFULL);
    const DWORD length = GetEnvironmentVariableA(name, out, room);
    // the system answers the length where the value fits, and the room it needs where it does not
    if ((length == 0u) || (length >= room))
    {
        if (room != 0u)
        {
            out[0] = '\0';
        }
        return 0ULL;
    }
    return (unsigned long long)length;
}

int platform_file_size(const char *path, unsigned long long *bytes)
{
    const HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    LARGE_INTEGER size;
    const int read = GetFileSizeEx(file, &size) != 0;
    (void)CloseHandle(file);
    // a file size is never negative
    *bytes = read ? (unsigned long long)size.QuadPart : 0ULL;
    return read;
}

int platform_file_read(const char *path, void *out, unsigned long long capacity, unsigned long long *bytes)
{
    *bytes = 0ULL;
    const HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

int platform_file_written(const char *path, const void *data, unsigned long long count)
{
    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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

unsigned long long platform_stream_opened(const char *path, int kind)
{
    const DWORD access = (kind == 0) ? GENERIC_READ : ((kind == 1) ? GENERIC_WRITE : FILE_APPEND_DATA);
    const DWORD disposition = (kind == 0) ? OPEN_EXISTING : ((kind == 1) ? CREATE_ALWAYS : OPEN_ALWAYS);
    const HANDLE file = CreateFileA(path, access, FILE_SHARE_READ, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
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

int platform_file_moved(const char *from, const char *to)
{
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
}

int platform_file_removed(const char *path)
{
    return DeleteFileA(path) != 0;
}

int platform_folder_made(const char *path)
{
    if (CreateDirectoryA(path, NULL) != 0)
    {
        return 1;
    }
    const DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u);
}

unsigned long long platform_process_id(void)
{
    return (unsigned long long)GetCurrentProcessId();
}

// the 100 ns ticks of the system's time from the start of 1601 to the start of 1970
#define PLATFORM_EPOCH_TICKS 116444736000000000ULL

unsigned long long platform_epoch_seconds(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    // Explicit casts carry the two halves of the 100 ns ticks into one word
    const unsigned long long ticks =
        ((unsigned long long)now.dwHighDateTime << 32u) | (unsigned long long)now.dwLowDateTime;
    return (ticks > PLATFORM_EPOCH_TICKS) ? ((ticks - PLATFORM_EPOCH_TICKS) / 10000000ULL) : 0ULL;
}

void platform_process_ended(int code)
{
    platform_close();
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
    // the command line: each argument in quotes, a space between them
    unsigned long long length = 1ULL;
    for (unsigned long long at = 0ULL; arguments[at] != NULL; at += 1ULL)
    {
        length += 3ULL + (unsigned long long)mmgr_text_length(arguments[at], (size_t)-1);
    }
    char *const line = (char *)platform_cell_taken(length, (unsigned int)platform_place_g_program_owner, 1);
    if (line == NULL)
    {
        return -1L;
    }
    VerbaLinea command = mmgr_line_over(line, (size_t)length);
    for (unsigned long long at = 0ULL; arguments[at] != NULL; at += 1ULL)
    {
        mmgr_line_put(&command, (at == 0ULL) ? "\"" : " \"");
        mmgr_line_put(&command, arguments[at]);
        mmgr_line_ch(&command, '"');
    }
    (void)mmgr_line_finish(&command);
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
        platform_cell_released(line, (unsigned int)platform_place_g_program_owner);
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
    platform_cell_released(line, (unsigned int)platform_place_g_program_owner);
    // an exit code of the system is a DWORD, and a long holds every code that a program returns
    return started ? (long)status : -1L;
}

#endif
