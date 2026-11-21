// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_internal.h: what platform_pool.c and the two systems of this folder share. platform_windows.c and
// platform_posix.c each define every call below on their own system, and platform_pool.c reads them through this header
// alone. Each system also defines the host hooks that embedded_types leaves to the program: embed_word_wait,
// embed_word_wake and embed_clock_ns
#ifndef PLATFORM_INTERNAL_H
#define PLATFORM_INTERNAL_H
#include "platform_pool.h"

#include "embed_turnstile.h"

// `bytes` of pages from the system, readable and writable, or NULL where the system gives none
void *platform_pages_mapped(unsigned long long bytes);

// the pages at `at` given back to the system, all `bytes` of them
void platform_pages_released(void *at, unsigned long long bytes);

// a thread of the system started on run(context) and let go. 1 where it started, 0 where the system refused it
int platform_thread_started(void (*run)(void *context), void *context);

// the id that the system gives the thread that calls
embed_word platform_thread_id(void);

// the processor given up by the thread that calls, for the next thread that waits for it
void platform_thread_yield(void);

// the processors that the system runs threads on, 1 at the fewest
unsigned int platform_processors(void);

#endif
