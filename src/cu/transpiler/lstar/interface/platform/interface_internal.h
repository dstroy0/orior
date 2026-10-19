// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_internal.h: what interface.c and the platforms' probes share: the includes, the bounds of a probe's
// clock, polling, reads and drain, the check every failure goes through, and the functions each reads from the other
#ifndef INTERFACE_INTERNAL_H
#define INTERFACE_INTERNAL_H

#if !defined(_WIN32)
// fork, exec, the process group, the clock and nanosleep are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../interface.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

#define INTERFACE_MILLION 1000000ull
#define INTERFACE_THOUSAND 1000ull
// how often a POSIX interface asks whether its probe has ended
#define INTERFACE_POLL_MICROSECONDS 1000ull
// a probe's output is read back in pieces this long
#define INTERFACE_READ_BYTES 4096u
// the most a pipe's reader is waited on once the probe and every process it started have ended
#define INTERFACE_DRAIN_MILLISECONDS 5000ull

#define INTERFACE_CHECK(condition_, evacaddr_, error_, kind_)                                                               \
    engine_error_check((condition_), (kind_), ENGINE_MODULE_INTERFACE, (unsigned int)__LINE__, (const void *)(evacaddr_),   \
                       (error_))

// the probe's output kept in the answer piece by piece and ended whole, and the host's clock in microseconds
void interface_output_kept(InterfaceAnswer *answer, unsigned long long *total, const char *piece,
                           unsigned long long read);
void interface_output_ended(InterfaceAnswer *answer, unsigned long long total);
unsigned long long interface_now(void);
// the probe started and run to its end or its limit by the platform's own means (platform/windows.c,
// platform/posix.c): 0 where it was waited on to its end; -1 where the output file or the pipe could not be made
long interface_probe_start(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error);
// the probe's output file read back by the platform's own means: its length whole, and as much of it as the answer's
// capacity holds; 0, or -1 where the file could not be read whole
long interface_output_file_read(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error);

#endif
