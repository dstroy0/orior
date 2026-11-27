// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface.c: a probe run through the interface. The probe starts through the platform's programs, its output comes
// back through a pipe or a file, and it runs to its end or its limit. The rule of its end is the system's
// (platform/windows/windows_probe.c, platform/posix/posix_probe.c)
#include "memoria_operor/memoria_operor.h"
#include "interface_internal.h"

// `read` bytes of the probe's output at `piece` stored in the answer, as many as its capacity holds past the `*total`
// before them, and counted whole
void interface_output_appended(InterfaceAnswer *answer, unsigned long long *total, const char *piece,
                               unsigned long long read)
{
    const unsigned long long capacity = (answer->output_capacity != 0ULL) ? (answer->output_capacity - 1ULL) : 0ULL;
    const unsigned long long room_left = (*total < capacity) ? (capacity - *total) : 0ULL;
    const unsigned long long copied = (room_left < read) ? room_left : read;
    if ((answer->output != NULL) && (copied != 0ULL))
    {
        // a copy is at max one piece, which a size_t represents
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = answer->output + *total, .src = piece, .bytes = (size_t)copied);
    }
    *total += read;
}

// the answer's output ended by a zero byte inside its capacity, and its length whole
void interface_output_ended(InterfaceAnswer *answer, unsigned long long total)
{
    if ((answer->output != NULL) && (answer->output_capacity != 0ULL))
    {
        const unsigned long long end =
            (total < (answer->output_capacity - 1ULL)) ? total : (answer->output_capacity - 1ULL);
        // the end is inside the capacity, which the caller's buffer allocates
        answer->output[(size_t)end] = '\0';
    }
    answer->output_bytes = total;
}

unsigned long long interface_now(void)
{
    return platform_clock_ns() / INTERFACE_THOUSAND;
}

// what the pipe `pipe` holds now read into the answer: 1 once the pipe is at its end
static int interface_pipe_taken(unsigned long long pipe, InterfaceAnswer *answer, unsigned long long *total)
{
    char piece[INTERFACE_READ_BYTES];
    int ended = 0;
    unsigned long long got = platform_program_read_ready(pipe, piece, sizeof(piece), &ended);
    while (got != 0ULL)
    {
        interface_output_appended(answer, total, piece, got);
        got = platform_program_read_ready(pipe, piece, sizeof(piece), &ended);
    }
    return ended;
}

// The probe started in a job or a group of its own, its output the file or a pipe and its input empty. It then runs to
// its end or its limit, and the interface reads the pipe as the probe writes it. 0 where a wait read the probe's end.
// -1 where the system made no output file or pipe, or no wait read the probe's end
static long interface_probe_start(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    char why[ENGINE_PATH_CAPACITY];
    VerbaLinea err_msg = mmgr_line_over(why, sizeof(why));
    PlatformRefusal refusal = {0u, 0ULL};
    // the probe's command is the words of a program that it runs and writes to none of
    const PlatformStart start = {.words = (const char *const *)probe->command,
                                 .output = (probe->output_path != NULL) ? PLATFORM_OUTPUT_FILE : PLATFORM_OUTPUT_JOINED,
                                 .output_path = probe->output_path,
                                 .detached = 1,
                                 .found_on_path = 1,
                                 .fault_silent = 1,
                                 .err_msg = &err_msg,
                                 .refusal = &refusal};
    interface_launch_taken();
    interface_starting_held();
    const unsigned long long began = interface_now();
    PlatformProgram program;
    const int made = platform_program_started(&start, &program);
    interface_starting_let_go();
    interface_launch_started();
    if (!made)
    {
        interface_launch_given();
        const void *const named = (probe->output_path != NULL) ? (const void *)probe->output_path : (const void *)probe;
        if (!INTERFACE_CHECK(refusal.stage != PLATFORM_REFUSED_ENDS, named, error, ENGINE_ERROR_RESOURCE))
        {
            return -1L;
        }
        answer->ending = INTERFACE_ENDING_NOT_STARTED;
        answer->code = refusal.code;
        return 0L;
    }
    unsigned long long total = 0ULL;
    int drained = program.out == 0ULL;
    int out_of_time = 0;
    while (!out_of_time && !platform_program_ended_within(&program, INTERFACE_POLL_MICROSECONDS))
    {
        drained = drained || interface_pipe_taken(program.out, answer, &total);
        out_of_time = (probe->limit_microseconds != 0ULL) && ((interface_now() - began) >= probe->limit_microseconds);
    }
    if (out_of_time)
    {
        platform_program_group_killed(&program);
    }
    PlatformEnding ending = {0, 0ULL};
    const int waited = platform_program_ending(&program, &ending);
    answer->microseconds = interface_now() - began;
    // every process that the probe started and left ends with it
    platform_program_group_killed(&program);
    // The loop reads the rest of the pipe to its end, once every process with its write end ended. The loop leaves a
    // process outside the job or the group that still holds it after INTERFACE_DRAIN_MILLISECONDS, with the read so far
    // kept
    if (program.out != 0ULL)
    {
        const unsigned long long drained_from = interface_now();
        while (!drained && ((interface_now() - drained_from) < (INTERFACE_DRAIN_MILLISECONDS * INTERFACE_THOUSAND)))
        {
            drained = interface_pipe_taken(program.out, answer, &total);
            if (!drained)
            {
                platform_paused((unsigned int)(INTERFACE_POLL_MICROSECONDS / INTERFACE_THOUSAND));
            }
        }
        interface_output_ended(answer, total);
    }
    platform_program_closed(&program);
    // the probe ended, however it ended, and its place in the queue goes back
    interface_launch_given();
    if (!INTERFACE_CHECK(waited, probe->output_path, error, ENGINE_ERROR_RESOURCE))
    {
        return -1L;
    }
    interface_ending_read(&ending, answer);
    if (out_of_time)
    {
        answer->ending = INTERFACE_ENDING_OUT_OF_TIME;
        answer->fault = INTERFACE_FAULT_NONE;
    }
    return 0L;
}

// the probe's output file read back: its length whole, and as much of it as the answer's capacity holds. 0, or -1 where
// no read took the file whole
static long interface_output_file_read(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    PlatformFile *const file = platform_file_open(probe->output_path, "rb");
    if (!INTERFACE_CHECK(file != NULL, probe->output_path, error, ENGINE_ERROR_RESOURCE))
    {
        return -1L;
    }
    char piece[INTERFACE_READ_BYTES];
    unsigned long long total = 0ULL;
    size_t got = platform_file_read_items(piece, 1u, sizeof(piece), file);
    while (got != 0u)
    {
        interface_output_appended(answer, &total, piece, got);
        got = platform_file_read_items(piece, 1u, sizeof(piece), file);
    }
    const int whole = !platform_file_failed(file);
    platform_file_close(file);
    interface_output_ended(answer, total);
    return INTERFACE_CHECK(whole, probe->output_path, error, ENGINE_ERROR_RESOURCE) ? 0L : -1L;
}

long interface_probe_run(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error)
{
    answer->ending = INTERFACE_ENDING_NOT_STARTED;
    answer->code = 0ULL;
    answer->fault = INTERFACE_FAULT_NONE;
    answer->output_bytes = 0ULL;
    answer->microseconds = 0ULL;
    if ((answer->output != NULL) && (answer->output_capacity != 0ULL))
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
    return (probe->output_path != NULL) ? interface_output_file_read(probe, answer, error) : 0L;
}
