// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#if !defined(_WIN32)
// nanosleep is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
// A probe for the interface test: one question a process, named by its first word, posed to the host part and its
// system. Built with no optimization: each question reaches the part as written. The operands are volatile: no compiler
// folds a division it can see is undefined, and the part itself answers
#include "../../../../../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define NOMINMAX
#include <intrin.h>
#include <windows.h>
#else
#include <time.h>
#endif

// the exit of a question this part is not requested
#define PROBE_NOT_REQUESTED 77

// a question this probe does not know
#define PROBE_UNKNOWN 64

static volatile int s_probe_numerator = 7;
static volatile int s_probe_zero = 0;
static volatile int s_probe_least = INT_MIN;
static volatile int s_probe_minus_one = -1;

// each frame occupies a page the compiler cannot drop, and the call is not the frame's last act. No call becomes a
// jump. The recursion without end is the question, and MSVC's warning that it overflows the stack is its answer
// foretold
#if defined(_MSC_VER)
#pragma warning(disable : 4717)
#endif
static int probe_descend(int depth)
{
    volatile char page[4096];
    page[0] = (char)depth;
    page[sizeof(page) - 1u] = page[0];
    return probe_descend(depth + 1) + page[sizeof(page) - 1u];
}

static void probe_pause_second(void)
{
#if defined(_WIN32)
    Sleep(1000u);
#else
    struct timespec second;
    second.tv_sec = 1;
    second.tv_nsec = 0L;
    nanosleep(&second, NULL);
#endif
}

int program_main(int count, char **arguments)
{
    const char *const question = (count > 1) ? arguments[1] : "";
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "exit",
                    .ci = EMBED_FALSE)) &&
        (count > 2))
    {
        return atoi(arguments[2]);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "write",
                    .ci = EMBED_FALSE)) &&
        (count > 2))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "out: ");
            mmgr_line_put(output_line, arguments[2]);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
        // the errors go out after the output, through the platform as the output did, and so each line ends in one byte
        platform_terminal_write(1, "err: ", 5ULL);
        platform_terminal_write(1, arguments[2],
                                (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = arguments[2],
                                                               .cap = PLATFORM_TEXT_TO_TERMINATOR));
        platform_terminal_write(1, "\n", 1ULL);
        return 0;
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "flood",
                    .ci = EMBED_FALSE)) &&
        (count > 2))
    {
        // more than any pipe buffers, written a piece at a time: an interface that does not read a pipe as it is
        // written blocks the probe here
        char piece[4096];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = piece, .bytes = sizeof(piece), .val = 'x');
        unsigned long left = strtoul(arguments[2], NULL, 10);
        while (left != 0ul)
        {
            const size_t now = (left < sizeof(piece)) ? (size_t)left : sizeof(piece);
            {
                VerbaLinea *const output_line = platform_line_out();
                (void)mmgr_line_put_n(output_line, (const char *)(piece), (size_t)(1u) * (size_t)(now));
                platform_line_out_ended();
            }
            left -= (unsigned long)now;
        }
        platform_line_flushed();
        return 0;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "divide_by_zero", .ci = EMBED_FALSE))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_decimal_signed(output_line, s_probe_numerator / s_probe_zero);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "divide_overflow", .ci = EMBED_FALSE))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_decimal_signed(output_line, s_probe_least / s_probe_minus_one);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "read_address", .ci = EMBED_FALSE))
    {
        // the sixteenth byte of the address space, in the first page, which no process maps
        volatile const int *const address = (volatile const int *)(uintptr_t)16u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_decimal_signed(output_line, *address);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "illegal_instruction", .ci = EMBED_FALSE))
    {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        __ud2();
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
        __asm__ volatile("ud2");
#elif defined(__GNUC__) && defined(__aarch64__)
        __asm__ volatile("udf #0");
#else
        return PROBE_NOT_REQUESTED;
#endif
        return 0;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "breakpoint", .ci = EMBED_FALSE))
    {
#if defined(_MSC_VER)
        __debugbreak();
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
        __asm__ volatile("int3");
#elif defined(__GNUC__) && defined(__aarch64__)
        __asm__ volatile("brk #0");
#else
        return PROBE_NOT_REQUESTED;
#endif
        return 0;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "stack",
                   .ci = EMBED_FALSE))
    {
        return probe_descend(0);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "abort",
                   .ci = EMBED_FALSE))
    {
        abort();
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "hang",
                   .ci = EMBED_FALSE))
    {
        for (;;)
        {
            probe_pause_second();
        }
    }
    fprintf(stderr, "interface_probe: no question \"%s\"\n", question);
    return PROBE_UNKNOWN;
}
