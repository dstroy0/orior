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

// `bytes` of addresses reserved, with no page of them readable or writable until a commit, or NULL where the system
// gives none
void *platform_pages_reserved(unsigned long long bytes);

// the `bytes` of reserved pages at `at` made readable and writable, each byte 0: 1, or 0 where the system refuses
int platform_pages_committed(void *at, unsigned long long bytes);

// The bytes of the `bytes` pages at `at` given up, while the pages stay mapped. The system takes them back at once or
// as it needs memory, and a later read finds 0 or the old bytes
void platform_pages_discarded(void *at, unsigned long long bytes);

// what a thread of the system runs: its call and the context of the call
typedef struct
{
    void (*run)(void *context); // The call that the thread runs.
    void *context;              // What the call reads [BORROWS].
} PlatformThread;

// A thread of the system started on thread->run(thread->context) and let go to end on its own. The caller keeps the
// record for the life of the thread. 1 where it started, 0 where the system refused it
int platform_thread_let_go(PlatformThread *thread);

// a thread of the system started on run(context) and let go, its record one of a few that the platform keeps. 1 where
// it started, 0 where the system refused it or no record is free
int platform_thread_started(void (*run)(void *context), void *context);

// the id that the system gives the thread that calls
embed_word platform_thread_id(void);

// the processor given up by the thread that calls, for the next thread that waits for it
void platform_thread_yield(void);

// The thread that calls put at `level`, one of PLATFORM_PRIORITY_WORK and the rest. On Linux and macOS a thread of the
// run level keeps the level it started at, since a rise takes a right that the program does not ask for
void platform_thread_priority(unsigned int level);

// the place that MMgr knows the thread that calls by, given back for the next thread. A thread's entry calls this as
// its work returns, and so the places in use stay within the live threads
void platform_thread_place_released(void);

// A file of the system, on the handle that platform_file.c holds. A type of 0 reads the file. A type of 1 writes a
// file made new in place of any file there. A type of 2 writes after the end of the file, which the call makes where
// none is there. The answer is the handle, and 0 where the system refused
unsigned long long platform_stream_opened(const char *path, int type);

// `count` bytes of `bytes` written to the file: the count that went, `count` where every byte went
unsigned long long platform_stream_put(unsigned long long handle, const void *bytes, unsigned long long count);

// at max `capacity` bytes of the file into `out`: the count read, and 0 at the end of the file or where the read failed
unsigned long long platform_stream_got(unsigned long long handle, void *out, unsigned long long capacity);

// The place of the file moved to `offset`. A `whence` of 0 counts from the start, 1 from the place now and 2 from the
// end. The answer is the place after the move, and -1 where the system refused
long long platform_stream_placed(unsigned long long handle, long long offset, int whence);

// the file of the handle closed: 1, or 0 where the system refused
int platform_stream_shut(unsigned long long handle);

// The return addresses of the calls that led to the caller onto `line`, at max PLATFORM_STACK_FRAMES of them. On
// Windows each is an offset from the start of the program's image, which a symbolizer reads with the program's PDB
#define PLATFORM_STACK_FRAMES 12u
void platform_stack_put(VerbaLinea *line);

// the bytes that separate the folders of a path on this system, terminated, and the byte that ends a drive's name,
// 0 on a system with no drives
extern const char g_platform_separators[];
extern const char g_platform_drive_mark;

// the folder at `path` made with the permission bits `mode` of POSIX, which Windows takes no note of: 1 where it is
// there after the call, made now or before
int platform_folder_made_as(const char *path, unsigned int mode);

// the bytes of the longest command line that Windows starts a program with, its terminator among them
#define PLATFORM_COMMAND_LINE_BYTES 32768u

// The process ended at once with `code` as its exit code. Nothing of the platform closes first.
// platform_process_ended closes the platform, then calls this
void platform_process_exited(int code);

#endif
