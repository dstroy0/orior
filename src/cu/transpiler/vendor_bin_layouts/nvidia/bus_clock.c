// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bus_clock.c: the timer register, found by querying. The ADVANCES qualifier put at each word of a span of the part's
// register aperture, read through the BAR0 driver (scaffolding/mmio_read.h, the BAR0 driver), and the first register
// that advances stored in the part's .ksc as the clock the query's cost is read off (query_put.h, Q1).
//
//     orior_bus_clock <ksc> <from> <words> <turns>
//
// Every number is read as hexadecimal. `from` is a byte offset into BAR0, `words` how many 32-bit words from
// it are queried, four bytes apart, and `turns` how many reads ADVANCES puts at a word after its first before the word
// reads as not advancing. Nothing is written to the part: the BAR0 driver reads alone. Each register queried is a line
// of the run's blob clock.txt, `<offset> <bit> <step>`, the step the word moved by between the reads that differed; the
// plan passed first, `clock <words>`. The first register that advances is written to the .ksc as `run answers <offset>
// clock bus`, in place of the row before it, and the row let go recorded in the .krf beside it. Exit 0 where a clock
// was found, 1 where none was in the span, 2 where the BAR0 driver was not reached or the words are not these.
#include "../../../../../includes/file_defs/krf/krf.h"
#include "../../../../../includes/file_defs/ksc/ksc.h"
#include "../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../includes/parsers/who.h"
#include "../../../../../includes/platform/platform.h"
#include "../../../engine/engine_note.h"
#include "../../lstar/protocol/query/query_put.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if defined(_WIN32)
#include "scaffolding/mmio_read.h"

// the max words one span queries, the lines of clock.txt stored in memory before they are passed
#define BUS_CLOCK_WORDS_MAX 65536u

static char s_lines[BUS_CLOCK_WORDS_MAX * 32u];

// the clock's row put in the .ksc at `ksc` in place of any clock row before it, through the run's writer: 1, or 0
static int bus_clock_recorded(const char *ksc, unsigned int offset)
{
    char row[96];
    {
        VerbaLinea buffer_line = mmgr_line_over(row, sizeof(row));
        mmgr_line_put(&buffer_line, "run answers ");
        mmgr_line_hex(&buffer_line, offset, 8u);
        mmgr_line_put(&buffer_line, " clock address");
        (void)mmgr_line_finish(&buffer_line);
    }
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
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_bus_clock: the clock row let go from ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " could not be recorded in ");
            mmgr_line_put(output_line, krf);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    kfile_lines_free(&removed);
    return written;
}

int program_main(int count, char **words)
{
    if (count != 5)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_bus_clock <ksc> <from> <words> <turns>, every number hexadecimal");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const char *const ksc = words[1];
    const unsigned long long from =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[2], .end = NULL, .base = 16);
    const unsigned long long address_given =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[3], .end = NULL, .base = 16);
    const unsigned long long turns =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[4], .end = NULL, .base = 16);
    if ((address_given == 0ULL) || (address_given > BUS_CLOCK_WORDS_MAX) || ((from % 4ULL) != 0ULL))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_bus_clock: from 1 to ");
            mmgr_line_decimal(&engine_note.line, BUS_CLOCK_WORDS_MAX, 1u);
            mmgr_line_put(&engine_note.line, " words, from a whole word's offset");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    HANDLE device = mmio_open();
    if (device == INVALID_HANDLE_VALUE)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_bus_clock: the BAR0 driver is not reached: its service GpuBar0 is not "
                                       "started (install_bar0.ps1)");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    unsigned long long base = 0ULL;
    unsigned long long size = 0ULL;
    if (!mmio_extent(device, &base, &size) || ((from + (address_given * 4ULL)) > size))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_bus_clock: the span 0x");
            mmgr_line_hex(output_line, from, 1u);
            mmgr_line_put(output_line, " to 0x");
            mmgr_line_hex(output_line, from + (address_given * 4ULL), 1u);
            mmgr_line_put(output_line, " is not inside BAR0 of ");
            mmgr_line_decimal(output_line, size, 1u);
            mmgr_line_put(output_line, " bytes");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        mmio_close(device);
        return 2;
    }
    QryShared *const run = qry_member_run();
    char plan[64];
    VerbaLinea formed_line_1 = mmgr_line_over(plan, sizeof(plan));
    mmgr_line_put(&formed_line_1, "clock ");
    mmgr_line_decimal(&formed_line_1, address_given, 1u);
    mmgr_line_put(&formed_line_1, "\n");
    (void)mmgr_line_finish(&formed_line_1);
    const int plan_length = (int)formed_line_1.at;
    if ((plan_length > 0) && (run != NULL))
    {
        qry_member_append("plan.txt", plan, (unsigned long long)plan_length, QRY_ORDINARY);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  plan: clock ");
        mmgr_line_decimal(output_line, address_given, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned long long value_read = 0ULL;
    unsigned int clock = 0u;
    unsigned int step = 0u;
    int found = 0;
    for (unsigned long long at = 0ULL; at < address_given; at += 1ULL)
    {
        const unsigned int offset = (unsigned int)(from + (at * 4ULL));
        unsigned int first = 0u;
        unsigned int then = 0u;
        if (!mmio_read(device, offset, 1u, &first))
        {
            break;
        }
        then = first;
        for (unsigned long long turn = 0ULL; (turn < turns) && (then == first); turn += 1ULL)
        {
            if (!mmio_read(device, offset, 1u, &then))
            {
                then = first;
                break;
            }
        }
        const unsigned int advanced = query_advanced(first, then);
        VerbaLinea formed_line_2 = mmgr_line_over(s_lines + value_read, sizeof(s_lines) - value_read);
        mmgr_line_hex(&formed_line_2, offset, 1u);
        mmgr_line_put(&formed_line_2, " ");
        mmgr_line_decimal(&formed_line_2, advanced, 1u);
        mmgr_line_put(&formed_line_2, " ");
        mmgr_line_decimal(&formed_line_2, then - first, 1u);
        mmgr_line_put(&formed_line_2, "\n");
        (void)mmgr_line_finish(&formed_line_2);
        const int length = (int)formed_line_2.at;
        // a line of three short numbers fits what is left of the room while the words are bounded
        value_read += (length > 0) ? (unsigned long long)length : 0ULL;
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
        qry_member_append("clock.txt", s_lines, value_read, QRY_ORDINARY);
    }
    if (!found)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_bus_clock: no register of the ");
            mmgr_line_decimal(output_line, address_given, 1u);
            mmgr_line_put(output_line, " from 0x");
            mmgr_line_hex(output_line, from, 1u);
            mmgr_line_put(output_line, " advances over ");
            mmgr_line_decimal(output_line, turns, 1u);
            mmgr_line_put(output_line, " reads; the part names no clock there");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    if (!bus_clock_recorded(ksc, clock))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_bus_clock: the clock could not be written to ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  clock: the timer register at 0x");
        mmgr_line_hex(output_line, clock, 1u);
        mmgr_line_put(output_line, " advances, by ");
        mmgr_line_decimal(output_line, step, 1u);
        mmgr_line_put(output_line, " between two reads; stored in ");
        mmgr_line_put(output_line, ksc);
        mmgr_line_put(output_line, " as the clock the query reads");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}

#else

int program_main(int count, char **words)
{
    (void)count;
    (void)words;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  orior_bus_clock: the BAR0 driver is a Windows kernel service, and this host has none");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 2;
}

#endif
