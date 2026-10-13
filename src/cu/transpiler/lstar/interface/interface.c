// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface.c: a probe run through the interface: its output kept, ended and read back, the host's clock, and the
// probe run to its end on whichever platform holds it (platform/windows.c, platform/posix.c)
#include "platform/interface_internal.h"

// `read` bytes of the probe's output at `piece` kept in the answer, as many as its capacity holds past the `*total`
// before them, and counted whole
void interface_output_kept(InterfaceAnswer *answer, unsigned long long *total, const char *piece,
                                  unsigned long long read)
{
    const unsigned long long kept_capacity =
        (answer->output_capacity != 0ull) ? (answer->output_capacity - 1ull) : 0ull;
    const unsigned long long kept = (*total < kept_capacity) ? (kept_capacity - *total) : 0ull;
    const unsigned long long copied = (kept < read) ? kept : read;
    if ((answer->output != NULL) && (copied != 0ull))
    {
        // a copy is at most one piece, which a size_t holds
        memcpy(answer->output + *total, piece, (size_t)copied);
    }
    *total += read;
}

// the answer's output ended by a zero byte inside its capacity, and its length whole
void interface_output_ended(InterfaceAnswer *answer, unsigned long long total)
{
    if ((answer->output != NULL) && (answer->output_capacity != 0ull))
    {
        const unsigned long long end =
            (total < (answer->output_capacity - 1ull)) ? total : (answer->output_capacity - 1ull);
        // the end is inside the capacity, which the caller's buffer holds
        answer->output[(size_t)end] = '\0';
    }
    answer->output_bytes = total;
}

unsigned long long interface_now(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    // a performance counter and its frequency are positive
    const unsigned long long ticks = (unsigned long long)counter.QuadPart;
    // a performance counter and its frequency are positive
    const unsigned long long rate = (unsigned long long)frequency.QuadPart;
    return ((ticks / rate) * INTERFACE_MILLION) + (((ticks % rate) * INTERFACE_MILLION) / rate);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // a monotonic clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * INTERFACE_MILLION) + ((unsigned long long)now.tv_nsec / 1000ull);
#endif
}


// the probe's output read back: its length whole, and as much of it as the answer's capacity holds
static long interface_output_read(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    FILE *const file = fopen(probe->output_path, "rb");
    if (!INTERFACE_CHECK(file != NULL, probe->output_path, error, ENGINE_ERROR_RESOURCE))
    {
        return -1L;
    }
    char piece[INTERFACE_READ_BYTES];
    unsigned long long total = 0ull;
    size_t read = fread(piece, 1u, sizeof(piece), file);
    while (read != 0u)
    {
        interface_output_kept(answer, &total, piece, read);
        read = fread(piece, 1u, sizeof(piece), file);
    }
    const int complete = ferror(file) == 0;
    fclose(file);
    interface_output_ended(answer, total);
    return INTERFACE_CHECK(complete, probe->output_path, error, ENGINE_ERROR_RESOURCE) ? 0L : -1L;
}

long interface_probe_run(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    answer->ending = INTERFACE_ENDING_NOT_STARTED;
    answer->code = 0ull;
    answer->fault = INTERFACE_FAULT_NONE;
    answer->output_bytes = 0ull;
    answer->microseconds = 0ull;
    if ((answer->output != NULL) && (answer->output_capacity != 0ull))
    {
        answer->output[0] = '\0';
    }
    if (!INTERFACE_CHECK((probe->command != NULL) && (probe->command[0] != NULL), probe, error, ENGINE_ERROR_REQUEST))
    {
        return -1L;
    }
    if (interface_probe_start(probe, answer, error) != 0L)
    {
        return -1L;
    }
    // output that came through the pipe is in the answer already
    return (probe->output_path != NULL) ? interface_output_read(probe, answer, error) : 0L;
}
