// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// clock.c: the clock the part named, read off the bus around a launch. The part's .ksc beside the machine file keeps
// the word of its register aperture that advances, `run answers <offset> clock bus` (bus_clock.c), and the carrier
// reads that word through the listener (scaffolding/mmio_read.h) before and after each launch it times: the cost of
// the launch is the word's advance, in the clock's own count (query_ask.h, Q1). Where the .ksc names no clock or
// the listener is not reached, the launches are timed on the driver's event clock as before, and the answer says
// which clock timed them
#include "cubin_run_internal.h"
#include "../../../../types/file_defs/ksc/ksc.h"

#if defined(_WIN32)
#include "../scaffolding/mmio_read.h"

static HANDLE s_bus = INVALID_HANDLE_VALUE;
static unsigned int s_clock_offset = 0u;
static int s_clock_read = 0;

int cubin_run_clock_open(const char *machine_path)
{
    // the .ksc beside the machine file, its stem the machine file's
    char ksc[1200];
    const size_t length = strlen(machine_path);
    const size_t stem = (length > 4u) && (strcmp(machine_path + length - 4u, ".khw") == 0) ? length - 4u : length;
    snprintf(ksc, sizeof(ksc), "%.*s.ksc", (int)stem, machine_path);
    KscFile file = {0};
    static KscRow s_row;
    int named = 0;
    if (ksc_read(&file, ksc))
    {
        for (unsigned int at = 0u; !named && (at < file.lines); at += 1u)
        {
            if (ksc_row_read(file.line[at], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
                (strcmp(s_row.question, "clock bus") == 0))
            {
                // an offset into the aperture is far below what an unsigned int holds
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
        printf("  clock: the part names the word at 0x%x and the listener is not reached; the driver's clock times "
               "the launches\n",
               s_clock_offset);
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
