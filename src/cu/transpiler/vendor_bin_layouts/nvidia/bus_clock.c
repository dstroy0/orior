// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bus_clock.c: the timer register, found by asking. The ADVANCES qualifier put at each word of a span of the part's register
// aperture, read through the BAR0 driver (scaffolding/mmio_read.h, the BAR0 driver), and the first register that advances
// kept in the part's .ksc as the clock the ask's cost is read off (query_ask.h, Q1).
//
//     orior_bus_clock <ksc> <from> <words> <turns>
//
// Every number is read as hexadecimal. `from` is a byte offset into BAR0, `words` how many 32-bit words from
// it are asked, four bytes apart, and `turns` how many reads ADVANCES puts at a word after its first before the word
// reads as not advancing. Nothing is written to the part: the BAR0 driver reads alone. Each register asked is a line of the
// run's blob clock.txt, `<offset> <bit> <step>`, the step the word moved by between the reads that differed; the plan
// handed first, `clock <words>`. The first register that advances is written to the .ksc as `run answers <offset> clock
// bus`, in place of the row before it, and the row let go kept in the .krf beside it. Exit 0 where a clock was
// found, 1 where none was in the span, 2 where the BAR0 driver was not reached or the words are not these.
#include "../../../includes/file_defs/krf/krf.h"
#include "../../../includes/file_defs/ksc/ksc.h"
#include "../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../lstar/protocol/query/query_ask.h"
#include "../../../includes/parsers/who.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include "scaffolding/mmio_read.h"

// the most words one span asks, the lines of clock.txt held in memory before they are handed
#define BUS_CLOCK_WORDS_MOST 65536u

static char s_lines[BUS_CLOCK_WORDS_MOST * 32u];

// the clock's row put in the .ksc at `ksc` in place of any clock row before it, through the run's writer: 1, or 0
static int bus_clock_kept(const char *ksc, unsigned int offset)
{
    char row[96];
    snprintf(row, sizeof(row), "run answers %08x clock address", offset);
    KscFile file = {0};
    KfileLines removed = {0};
    const char *const rows[1] = {row};
    const Who who = WHO_HERE;
    const int written = ksc_read(&file, ksc) &&
                        ksc_rows_replaced_traced_by(&file, "run", "clock", rows, 1u, who, &removed) &&
                        ksc_write_by(&file, ksc, who);
    ksc_free(&file);
    if (written && (removed.lines != 0u))
    {
        char krf[1200];
        krf_path_beside(ksc, krf, sizeof(krf));
        if (!krf_removed_by(krf, "ksc", "replaced", (const char *const *)removed.line, removed.lines, who))
        {
            printf("  orior_bus_clock: the clock row let go from %s could not be kept in %s\n", ksc, krf);
        }
    }
    kfile_lines_free(&removed);
    return written;
}

int main(int count, char **words)
{
    if (count != 5)
    {
        fprintf(stderr, "orior_bus_clock <ksc> <from> <words> <turns>, every number hexadecimal\n");
        return 2;
    }
    const char *const ksc = words[1];
    const unsigned long long from = strtoull(words[2], NULL, 16);
    const unsigned long long asked = strtoull(words[3], NULL, 16);
    const unsigned long long turns = strtoull(words[4], NULL, 16);
    if ((asked == 0ull) || (asked > BUS_CLOCK_WORDS_MOST) || ((from % 4ull) != 0ull))
    {
        fprintf(stderr, "orior_bus_clock: from 1 to %u words, from a whole word's offset\n", BUS_CLOCK_WORDS_MOST);
        return 2;
    }
    HANDLE device = mmio_open();
    if (device == INVALID_HANDLE_VALUE)
    {
        printf("  orior_bus_clock: the BAR0 driver is not reached: its service GpuBar0 is not started (install_bar0.ps1)\n");
        return 2;
    }
    unsigned long long base = 0ull;
    unsigned long long size = 0ull;
    if (!mmio_extent(device, &base, &size) || ((from + (asked * 4ull)) > size))
    {
        printf("  orior_bus_clock: the span 0x%llx to 0x%llx is not inside BAR0 of %llu bytes\n", from,
               from + (asked * 4ull), size);
        mmio_close(device);
        return 2;
    }
    QryShared *const run = qry_member_run();
    char plan[64];
    const int plan_length = snprintf(plan, sizeof(plan), "clock %llu\n", asked);
    if ((plan_length > 0) && (run != NULL))
    {
        qry_member_append("plan.txt", plan, (unsigned long long)plan_length, QRY_ORDINARY);
    }
    printf("  plan: clock %llu\n", asked);
    unsigned long long held = 0ull;
    unsigned int clock = 0u;
    unsigned int step = 0u;
    int found = 0;
    for (unsigned long long at = 0ull; at < asked; at += 1ull)
    {
        const unsigned int offset = (unsigned int)(from + (at * 4ull));
        unsigned int first = 0u;
        unsigned int then = 0u;
        if (!mmio_read(device, offset, 1u, &first))
        {
            break;
        }
        then = first;
        for (unsigned long long turn = 0ull; (turn < turns) && (then == first); turn += 1ull)
        {
            if (!mmio_read(device, offset, 1u, &then))
            {
                then = first;
                break;
            }
        }
        const unsigned int advanced = query_advanced(first, then);
        const int length = snprintf(s_lines + held, sizeof(s_lines) - held, "%x %u %u\n", offset, advanced, then - first);
        // a line of three short numbers fits what is left of the room while the words are bounded
        held += (length > 0) ? (unsigned long long)length : 0ull;
        if (advanced && !found)
        {
            found = 1;
            clock = offset;
            step = then - first;
        }
    }
    mmio_close(device);
    if (run != NULL)
    {
        qry_member_append("clock.txt", s_lines, held, QRY_ORDINARY);
    }
    if (!found)
    {
        printf("  orior_bus_clock: no register of the %llu from 0x%llx advances over %llu reads; the part names no clock there\n",
               asked, from, turns);
        return 1;
    }
    if (!bus_clock_kept(ksc, clock))
    {
        printf("  orior_bus_clock: the clock could not be written to %s\n", ksc);
        return 2;
    }
    printf("  clock: the timer register at 0x%x advances, by %u between two reads; kept in %s as the clock the ask reads\n",
           clock, step, ksc);
    return 0;
}

#else

int main(int count, char **words)
{
    (void)count;
    (void)words;
    printf("  orior_bus_clock: the BAR0 driver is a Windows kernel service, and this host has none\n");
    return 2;
}

#endif
