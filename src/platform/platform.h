// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform.h: the boundary between a program and its system. Every call into the system goes through platform_windows.c
// or platform_posix.c of this folder, and through nothing else. The model above this boundary includes no header of the
// C library and no header of the system.
//
// A program runs in one order: the process, the resource acquisitions, the callers, and the ephemeral work last.
// platform_open is the process stage. It places the one pool of the program over pages that the system gives. Every
// cell of the program comes from that pool. It also positions the block of fixed parts beside the pool.
//
// A module takes its resources from the pool on the persistent tier or as movable cells. A caller gives its storage to
// the calls below it [BORROWS]. The ephemeral work is on the temporary tier or on the local stack, and it goes back
// first.
//
// This header takes plain arguments alone, and a C++ unit includes it as it is. The guard of the pool and the owners
// are in platform_pool.h, for the C model above the boundary.
#ifndef PLATFORM_H
#define PLATFORM_H
#include "verba_scribo/verba_linea.h"

EMBED_BEGIN_DECLS

// the fewest bytes that the pool holds. A request for fewer gets this many
#define PLATFORM_POOL_BYTES_LEAST (64ULL << 20u)

// The program above the boundary. The platform owns main (platform_process.c): main runs the process stage, then this,
// then the end of the stage. A program defines program_main and no main. Its answer is the exit code of the process
int program_main(int argc, char **argv);

// the bytes of the pool that the program asks for. A program that needs more than PLATFORM_POOL_BYTES_LEAST defines
// this constant, and every other program takes the default of the platform
extern const unsigned long long g_program_pool_bytes;

// the process stage, which main runs. The pool of `bytes` goes over pages from the system, at least
// PLATFORM_POOL_BYTES_LEAST. The movable option and the parts of the block take their places after it. 1, or 0 where
// the system gives no pages. The first call places the pool, and a later call answers 1 and changes nothing
int platform_open(unsigned long long bytes);

// the end of the process stage, which main runs after program_main returns: the mover ended and the pages given back
void platform_close(void);

// `length` bytes of `text` written to the output stream of the process, or to its error stream where `errors` is 1
void platform_terminal_write(int errors, const char *text, unsigned long long length);

// the line of the output stream: a line over the part of the block that platform_open positions. Its flush writes
// what it holds to the output stream, and so the line never fills. platform_line_out answers the same line on each call
VerbaLinea *platform_line_out(void);

// what the line of the output stream holds, written to the output stream, and the line emptied
void platform_line_flushed(void);

EMBED_END_DECLS

#endif
