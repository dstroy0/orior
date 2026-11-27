// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_internal.h: what interface.c and the probe files of each system share: the bounds of the clock, the polls,
// the reads and the drain of a probe, the check that every failure goes through, and the calls of each system
#ifndef INTERFACE_INTERNAL_H
#define INTERFACE_INTERNAL_H

#include "../../../../../includes/platform/platform.h"
#include "interface.h"

#include <stdlib.h>

#define INTERFACE_MILLION 1000000ULL
#define INTERFACE_THOUSAND 1000ULL
// how often the interface reads the pipe of its probe and checks whether the probe ended
#define INTERFACE_POLL_MICROSECONDS 1000ULL
// the interface reads a probe's output back in pieces this long
#define INTERFACE_READ_BYTES 4096u
// the max that the pipe's reader waits once the probe and every process that it started ended
#define INTERFACE_DRAIN_MILLISECONDS 5000ULL

#define INTERFACE_CHECK(condition_, evacaddr_, error_, type_)                                                               \
    engine_error_check((condition_), (type_), ENGINE_MODULE_INTERFACE, (unsigned int)__LINE__, (const void *)(evacaddr_),   \
                       (error_))

// the probe's output stored in the answer piece by piece and ended whole, and the host's clock in microseconds
void interface_output_appended(InterfaceAnswer *answer, unsigned long long *total, const char *piece,
                               unsigned long long read);
void interface_output_ended(InterfaceAnswer *answer, unsigned long long total);
unsigned long long interface_now(void);

// The calls of each system (platform/windows/windows_probe.c, platform/posix/posix_probe.c). The first reads the ending
// of a probe and its code into the answer by the rule of the system. The others are the lock that one start of a probe
// holds. The lock
// keeps the pipes of one probe out of another probe that a second thread forks
void interface_ending_read(const PlatformEnding *ending, InterfaceAnswer *answer);
void interface_starting_held(void);
void interface_starting_let_go(void);

#endif
