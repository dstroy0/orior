// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_posix.h: the calls and the facts that the POSIX files of the platform take from the file of each POSIX
// system. The folder linux defines them for Linux, and the folder macos defines them for macOS. A POSIX system with
// no file of its own fails at the link, which names each call that it lacks
#ifndef PLATFORM_POSIX_H
#define PLATFORM_POSIX_H

#include "../platform_internal.h"

#if !defined(_WIN32)
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>

// the time that the file of `found` last changed, in milliseconds since 1970, which each system keeps in a field of
// its own name
unsigned long long platform_stat_changed_milliseconds(const struct stat *found);

// the process at the other end of the local socket `fd`, as the system reports it. 1 with its number in `peer`, or 0
// with 0 there where the system does not report it
int platform_socket_peer(int fd, unsigned int *peer);

// the advice that gives up the bytes of pages. Linux drops them at once, and macOS marks them free and takes them back
// as it needs the memory
extern const int g_platform_pages_discard;

// The thread that calls asleep while `word` holds `old_value`, for `nanoseconds` at most, or with no end where it is
// EMBED_WAIT_INFINITE. The sleep can end early, and the caller tests the word again
void platform_word_slept(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds);

// every thread asleep on `word` woken
void platform_word_woken(embed_atomic_word *word);

// A pipe made whose two ends close on exec. A child that another thread forks then takes neither end into the program
// that it runs. 1, or 0 with both ends -1
int platform_pipe_made(int ends[2]);

// The child of a fork put on the processors and on the policy that `start` asks for, before it runs its program. A
// system that has no such call leaves the child as it is
void platform_child_placed(const PlatformStart *start);

// The wait for the end of the child `child` through a descriptor of the process, `microseconds` at most, or with no
// end where it is PLATFORM_WAIT_FOREVER. 1 where it waited, and 0 where the system gives no such descriptor and the
// caller waits in steps. The wait can end early, and the caller looks at the child again
int platform_child_watched(pid_t child, unsigned long long microseconds);

// A pseudo-terminal made, its leader in `leader` and its follower in `follower`, the follower `size` large: 1, or 0
// where the system makes none
int platform_terminal_pair(int *leader, int *follower, struct winsize *size);

// the child of a fork made the leader of a session, with `follower` as the terminal that controls it, on its standard
// descriptors. 1, or 0 where the system refuses
int platform_terminal_led(int follower);
#endif

#endif
