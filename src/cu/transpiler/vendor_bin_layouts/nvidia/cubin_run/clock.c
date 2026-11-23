// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// clock.c: the timer register, read through the BAR0 driver around a launch. The part's .ksc beside the machine file
// records the word of its BAR0 registers that advances, `run answers <offset> clock address` (bus_clock.c), and the
// carrier reads that word through the BAR0 driver (scaffolding/mmio_read.h) before and after each launch it times: the
// cost of the launch is the word's advance, in the clock's own count (query_put.h, Q1). Where the .ksc names no clock
// or the BAR0 driver is not reached, the launches are timed on the driver's event timer as before, and the answer
// reports which clock timed them
#include "../../../../../../includes/file_defs/ksc/ksc.h"
#include "../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "cubin_run_internal.h"

#if defined(_WIN32)
#include "../scaffolding/mmio_read.h"

static HANDLE s_bus = INVALID_HANDLE_VALUE;
static unsigned int s_clock_offset = 0u;
static int s_clock_read = 0;

int cubin_run_clock_open(const char *machine_path)
{
    // the .ksc beside the machine file, its stem the machine file's
    char ksc[1200];
    const size_t length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = machine_path, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t stem =
        (length > 4u) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = machine_path + length - 4u,
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".khw", .ci = EMBED_FALSE))
            ? length - 4u
            : length;
    {
        VerbaLinea buffer_line = mmgr_line_over(ksc, sizeof(ksc));
        mmgr_line_put_max(&buffer_line, machine_path, (size_t)((int)stem));
        mmgr_line_put(&buffer_line, ".ksc");
        (void)mmgr_line_finish(&buffer_line);
    }
    KscFile file = {0};
    static KscRow s_row;
    int named = 0;
    if (ksc_read(&file, ksc))
    {
        for (unsigned int at = 0u; !named && (at < file.lines); at += 1u)
        {
            if (ksc_row_read(file.line[at], "run", &s_row) &&
                (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "answers", .ci = EMBED_FALSE)) &&
                (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "clock address", .ci = EMBED_FALSE)))
            {
                // an offset into BAR0 is far below what an unsigned int represents
                s_clock_offset = (unsigned int)s_row.value;
                named = 1;
            }
        }
    }
    ksc_free(&file);
    if (!named)
    {
        return 0;
    }
    s_bus = mmio_open();
    s_clock_read = (s_bus != INVALID_HANDLE_VALUE);
    if (!s_clock_read)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  clock: the part names the timer register at 0x");
        mmgr_line_hex(output_line, s_clock_offset, 1u);
        mmgr_line_put(output_line, " and the BAR0 driver is not reached; the driver's clock times the launches");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return s_clock_read;
}

int cubin_run_clock_read(unsigned int *word)
{
    return s_clock_read && mmio_read(s_bus, s_clock_offset, 1u, word);
}

unsigned int cubin_run_clock_offset(void)
{
    return s_clock_read ? s_clock_offset : 0u;
}

void cubin_run_clock_close(void)
{
    if (s_clock_read)
    {
        mmio_close(s_bus);
        s_bus = INVALID_HANDLE_VALUE;
        s_clock_read = 0;
    }
}

#else

int cubin_run_clock_open(const char *machine_path)
{
    (void)machine_path;
    return 0;
}

int cubin_run_clock_read(unsigned int *word)
{
    (void)word;
    return 0;
}

unsigned int cubin_run_clock_offset(void)
{
    return 0u;
}

void cubin_run_clock_close(void)
{
}

#endif
