// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_windows.c: the platform on Windows. The pages come from VirtualAlloc, a thread from CreateThread, an address
// wait from WaitOnAddress, the clock from the performance counter, and the streams from WriteFile
#include "platform_internal.h"

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
    const embed_u32 place = embed_atomic_u32_fetch_add(&s_threads_given, 1u);
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

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds)
{
    embed_atomic_word_value compare_value = old_value;
    if (nanoseconds == 0u)
    {
        return embed_atomic_word_load(word) != old_value;
    }
    // WaitOnAddress takes whole milliseconds. The limit rounds up, and a limit past what a DWORD counts waits that long
    const embed_u64 milliseconds =
        ((embed_u64)nanoseconds / 1000000ULL) + (((embed_u64)nanoseconds % 1000000ULL) != 0u);
    const DWORD waited = (nanoseconds == EMBED_WAIT_INFINITE) ? INFINITE
                         : (milliseconds < 0xfffffffeULL)     ? (DWORD)milliseconds
                                                              : (DWORD)0xfffffffeULL;
    (void)WaitOnAddress(&word->value, &compare_value, sizeof(compare_value), waited);
    return embed_atomic_word_load(word) != old_value;
}

void embed_word_wake(embed_atomic_word *word)
{
    // WakeByAddressAll takes the address of the word as a plain pointer. The address goes through uintptr_t, and so no
    // cast removes the qualifier of the atomic value
    WakeByAddressAll((void *)(uintptr_t)&word->value);
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

#endif
