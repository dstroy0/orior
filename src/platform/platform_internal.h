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

// A file of the system, on the handle that platform_file.c holds. A kind of 0 reads the file. A kind of 1 writes a
// file made new in place of any file there. A kind of 2 writes after the end of the file, which the call makes where
// none is there. The answer is the handle, and 0 where the system refused
unsigned long long platform_stream_opened(const char *path, int kind);

// `count` bytes of `bytes` written to the file: the count that went, `count` where every byte went
unsigned long long platform_stream_put(unsigned long long handle, const void *bytes, unsigned long long count);

// at max `capacity` bytes of the file into `out`: the count read, and 0 at the end of the file or where the read failed
unsigned long long platform_stream_got(unsigned long long handle, void *out, unsigned long long capacity);

// The place of the file moved to `offset`. A `whence` of 0 counts from the start, 1 from the place now and 2 from the
// end. The answer is the place after the move, and -1 where the system refused
long long platform_stream_placed(unsigned long long handle, long long offset, int whence);

// the file of the handle closed: 1, or 0 where the system refused
int platform_stream_shut(unsigned long long handle);

#endif
