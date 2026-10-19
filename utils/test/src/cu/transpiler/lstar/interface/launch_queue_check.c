// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// launch_queue_check.c: the interface's launch queue held: every child process of the tree starts through it, at most
// INTERFACE_LAUNCHES_AT_ONCE at a time and INTERFACE_LAUNCH_SPACING_MILLISECONDS apart
//
//   places    eight starts asked at once hold at most INTERFACE_LAUNCHES_AT_ONCE places at any moment
//   spacing   each start follows the one before it by the spacing at least
//   given     every place taken is given back, and the next eight starts go through
//
//     launch_queue_check
#include "../../../../../../../src/cu/transpiler/lstar/interface/interface.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#define CHECK_STARTS 8u

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// the places held at this moment, and the most held at any moment
static _Atomic unsigned int s_held = 0u;
static _Atomic unsigned int s_most_held = 0u;

// the moment each start went, in milliseconds
static unsigned long long s_started_at[CHECK_STARTS];

static unsigned long long check_now_milliseconds(void)
{
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER count;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&count);
    // the count fits a double for any run of this check
    return (unsigned long long)(((double)count.QuadPart * 1000.0) / (double)frequency.QuadPart);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((unsigned long long)now.tv_sec * 1000ull) + ((unsigned long long)now.tv_nsec / 1000000ull);
#endif
}

static void check_paused(unsigned int milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    const struct timespec pause = {0, (long)milliseconds * 1000000L};
    nanosleep(&pause, NULL);
#endif
}

// one start: a place taken, the start stamped and counted, a child's length of work, and the place given back
static void *check_started(void *argument)
{
    unsigned long long *const stamp = (unsigned long long *)argument;
    interface_launch_taken();
    const unsigned int held = atomic_fetch_add(&s_held, 1u) + 1u;
    unsigned int most = atomic_load(&s_most_held);
    while ((held > most) && !atomic_compare_exchange_weak(&s_most_held, &most, held))
    {
    }
    *stamp = check_now_milliseconds();
    interface_launch_started();
    check_paused(120u);
    atomic_fetch_sub(&s_held, 1u);
    interface_launch_given();
    return NULL;
}

// the eight starts asked at once from threads, and what they held to
static void check_round(const char *round)
{
    atomic_store(&s_held, 0u);
    atomic_store(&s_most_held, 0u);
    pthread_t thread[CHECK_STARTS];
    for (unsigned int at = 0u; at < CHECK_STARTS; at += 1u)
    {
        pthread_create(&thread[at], NULL, check_started, &s_started_at[at]);
    }
    for (unsigned int at = 0u; at < CHECK_STARTS; at += 1u)
    {
        pthread_join(thread[at], NULL);
    }
    char what[160];
    snprintf(what, sizeof(what), "%s: at most %u places held at once (%u were)", round, INTERFACE_LAUNCHES_AT_ONCE,
             atomic_load(&s_most_held));
    check_that(atomic_load(&s_most_held) <= INTERFACE_LAUNCHES_AT_ONCE, what);
    // the stamps in order, and the least gap between one and the next
    for (unsigned int at = 1u; at < CHECK_STARTS; at += 1u)
    {
        for (unsigned int back = at; (back > 0u) && (s_started_at[back - 1u] > s_started_at[back]); back -= 1u)
        {
            const unsigned long long swapped = s_started_at[back];
            s_started_at[back] = s_started_at[back - 1u];
            s_started_at[back - 1u] = swapped;
        }
    }
    unsigned long long least_gap = ~0ull;
    for (unsigned int at = 1u; at < CHECK_STARTS; at += 1u)
    {
        const unsigned long long gap = s_started_at[at] - s_started_at[at - 1u];
        least_gap = (gap < least_gap) ? gap : least_gap;
    }
    // the clock reads a start a few milliseconds early or late, within a tenth of the spacing
    const unsigned long long allowed =
        INTERFACE_LAUNCH_SPACING_MILLISECONDS - (INTERFACE_LAUNCH_SPACING_MILLISECONDS / 10u);
    snprintf(what, sizeof(what), "%s: each start follows the one before by %u ms at least (the least gap %llu ms)",
             round, INTERFACE_LAUNCH_SPACING_MILLISECONDS, least_gap);
    check_that(least_gap >= allowed, what);
}

int main(void)
{
    check_round("the first eight");
    check_round("the next eight");
    printf("launch_queue_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
