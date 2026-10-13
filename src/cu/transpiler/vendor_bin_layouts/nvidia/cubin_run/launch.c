// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// launch.c: the launch: the host's clock and its turn given up, the launch bounded and timed, the answers read, the container out, and a dry run written
#include "cubin_run_internal.h"

// the host's clock in nanoseconds, from a start of its own and only ever read against itself
static unsigned long long cubin_run_host_now(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (unsigned long long)(((double)counter.QuadPart * 1000000000.0) / (double)frequency.QuadPart);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((unsigned long long)now.tv_sec * 1000000000ull) + (unsigned long long)now.tv_nsec;
#endif
}

// the host's turn given up while the part runs, so that reading the stop event does not hold the processor
static void cubin_run_host_yield(void)
{
#if defined(_WIN32)
    Sleep(0);
#else
    sched_yield();
#endif
}

// 1 where the last launch broke cubin_safe's rule 6: the process launches nothing after it and leaves what the launch
// holds where it is, since a launch the part has not given back may still hold it
int s_launch_timed_out = 0;

// 1 where the last launch broke cubin_safe's rule 7, read from the part's fault telemetry or the driver's own word on
// the stop event: the part faulted, its context corrupt, and the launch is answered as refused
int s_launch_faulted = 0;

// One launch of `function` over `arguments` carried under cubin_safe's rule 6: alone between two events the part stamps
// from its own timer, its stop event read without waiting on it until the part reaches it, the driver gives an error,
// or CUBIN_SAFE_LAUNCH_BOUND passes on the host's clock. Its block latency in nanoseconds into `nanoseconds`. 0, the
// status the driver gave, or CUDA_ERROR_LAUNCH_TIMEOUT where the rule was broken, the driver's own or the bound's
static CubinRunStatus cubin_run_bounded(const CubinRunDriver *driver, void *function, void **arguments,
                                        const CubinRunShape *shape, unsigned long long *nanoseconds)
{
    void *start = NULL;
    void *stop = NULL;
    float milliseconds = 0.0f;
    *nanoseconds = 0ull;
    if (s_launch_timed_out)
    {
        return CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT;
    }
    // every fault already stamped thrown away first; only one the launch to come stamps then counts against it (rule 7)
    cubin_run_telemetry_drain();
    CubinRunStatus status = driver->event_create(&start, 0u);
    status = (status == 0) ? driver->event_create(&stop, 0u) : status;
    status = (status == 0) ? driver->event_record(start, NULL) : status;
    status = (status == 0) ? driver->launch(function, shape->blocks, 1u, 1u, shape->threads, 1u, 1u, 0u, NULL,
                                            arguments, NULL)
                           : status;
    status = (status == 0) ? driver->event_record(stop, NULL) : status;
    const unsigned long long began = cubin_run_host_now();
    unsigned long long waited = 0ull;
    while (status == 0)
    {
        const CubinRunStatus reached = driver->event_query(stop);
        waited = cubin_run_host_now() - began;
        if (reached == 0)
        {
            break;
        }
        if ((reached != CUBIN_SAFE_STATUS_NOT_READY) || (cubin_safe_launch(reached, waited) != CUBIN_SAFE))
        {
            status = reached;
            break;
        }
        cubin_run_host_yield();
    }
    // rule 7: the part's fault telemetry read, and the driver's own word on the stop event with it. A launch that
    // faulted the part halts the process and is answered as refused, named by the driver's error or, where only the
    // stamp caught it, as a failed launch
    if (cubin_safe_fault(cubin_run_telemetry_faulted(status), status) == CUBIN_SAFE_LAUNCH_FAULT)
    {
        s_launch_timed_out = 1;
        s_launch_faulted = 1;
        return (status != 0) ? status : CUBIN_SAFE_STATUS_LAUNCH_FAILED;
    }
    if (cubin_safe_launch(status, waited) != CUBIN_SAFE)
    {
        s_launch_timed_out = 1;
        return CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT;
    }
    status = (status == 0) ? driver->event_elapsed(&milliseconds, start, stop) : status;
    // the driver gives the time as milliseconds in a float; a nanosecond is a millionth of one
    *nanoseconds = (unsigned long long)((double)milliseconds * 1000000.0);
    if (start != NULL)
    {
        driver->event_destroy(start);
    }
    if (stop != NULL)
    {
        driver->event_destroy(stop);
    }
    return status;
}

// The least and the most block latency of the last timed question, the runs P6 reads (cubin_run_timed)
unsigned long long s_timed_least;

unsigned long long s_timed_most;

// the register count the part reports for the last kernel a launch loaded (cuFuncGetAttribute), or
// CUBIN_RUN_REGISTERS_UNREAD before it is read: the count read off the part, a question never walking up to the
// boundary to find it
unsigned int s_part_registers = CUBIN_RUN_REGISTERS_UNREAD;

// `launches` launches of `function` over `arguments`, one at a time, each under cubin_safe's rule 6, their block
// latencies summed into `nanoseconds` and the least and the most of them into s_timed_least and s_timed_most: 0, or the
// status the driver gave the first launch that did not return clean, none launched after it. The timer is the part's
// and not the host's, and counts the same whatever clock the part's multiprocessors run at
static CubinRunStatus cubin_run_timed(const CubinRunDriver *driver, void *function, void **arguments,
                                      const CubinRunShape *shape, unsigned int launches,
                                      unsigned long long *nanoseconds)
{
    CubinRunStatus status = 0;
    *nanoseconds = 0ull;
    s_timed_least = 0ull;
    s_timed_most = 0ull;
    for (unsigned int launch = 0u; (status == 0) && (launch < launches); launch += 1u)
    {
        unsigned long long latency = 0ull;
        status = cubin_run_bounded(driver, function, arguments, shape, &latency);
        if (status == 0)
        {
            *nanoseconds += latency;
            s_timed_least = ((launch == 0u) || (latency < s_timed_least)) ? latency : s_timed_least;
            s_timed_most = (latency > s_timed_most) ? latency : s_timed_most;
        }
    }
    return status;
}

// The answers read as the kernel wrote them into `written`, the room of `count` threads filled with
// CUBIN_RUN_FILL_WORD before the launch. Every thread writes its answer at one stride from the one before it, and the
// writes reach (count - 1) strides and the words one thread writes: the stride is that reach over the count rounded up,
// and the words a thread writes are what the last thread's write adds past the strides before it. That holds where
// the room a thread leaves past what it writes is less than the count of threads, and where the last word the last
// thread writes differs from the fill. Each case's answer into s_answers, the first two words its thread wrote, a word
// it did not write read as 0
static void cubin_run_answers_read(const unsigned int *written, unsigned int count)
{
    unsigned long long reach = (unsigned long long)count * CUBIN_RUN_OUT_ROOM;
    while ((reach > 0ull) && (written[reach - 1ull] == CUBIN_RUN_FILL_WORD))
    {
        reach -= 1ull;
    }
    const unsigned long long stride = (reach + count - 1ull) / count;
    const unsigned long long words = (stride != 0ull) ? (reach - ((unsigned long long)(count - 1u) * stride)) : 0ull;
    for (unsigned int place = 0u; place < s_case_count; place += 1u)
    {
        for (unsigned int word = 0u; word < CUBIN_RUN_OUT_WORDS; word += 1u)
        {
            s_answers[place][word] = (word < words) ? written[((unsigned long long)place * stride) + word] : 0u;
        }
    }
}

// The container in s_container loaded and its kernel launched as `shape` says, every thread given a case, thread t
// case t of the cases taken round, and each case's answer into s_answers as the kernel wrote it; where `launches`
// is not 0, that many launches after it timed into `nanoseconds`. 0, or the status the driver gave
CubinRunStatus cubin_run_launch(const CubinRunDriver *driver, const char *kernel, const CubinRunShape *shape,
                                       unsigned int launches, unsigned long long *nanoseconds)
{
    int device = 0;
    void *context = NULL;
    void *module = NULL;
    void *function = NULL;
    CubinRunAddress in = 0ull;
    CubinRunAddress out = 0ull;
    unsigned int count = shape->threads * shape->blocks;
    const size_t in_bytes = (size_t)count * sizeof(s_cases[0]);
    const size_t out_bytes = (size_t)count * CUBIN_RUN_OUT_ROOM * sizeof(unsigned int);
    unsigned int(*const given)[CUBIN_RUN_IN_WORDS] = malloc(in_bytes);
    unsigned int *const written = malloc(out_bytes);
    if ((given == NULL) || (written == NULL))
    {
        free(given);
        free(written);
        return -1;
    }
    for (unsigned int thread = 0u; thread < count; thread += 1u)
    {
        memcpy(given[thread], s_cases[thread % s_case_count], sizeof(s_cases[0]));
    }
    CubinRunStatus status = driver->init(0u);
    status = (status == 0) ? driver->device_get(&device, 0) : status;
    status = (status == 0) ? driver->context_retain(&context, device) : status;
    status = (status == 0) ? driver->context_set(context) : status;
    status = (status == 0) ? driver->module_load(&module, s_container) : status;
    status = (status == 0) ? driver->function_get(&function, module, kernel) : status;
    if (status == 0)
    {
        // the part's own count of the registers the kernel allots, read off the loaded function before any launch: a
        // count the attribute holds in a signed int is never negative
        int reported = 0;
        s_part_registers =
            ((driver->function_attribute(&reported, CUBIN_RUN_ATTRIBUTE_NUM_REGS, function) == 0) && (reported >= 0))
                ? (unsigned int)reported
                : CUBIN_RUN_REGISTERS_UNREAD;
    }
    status = (status == 0) ? driver->allocate(&in, in_bytes) : status;
    status = (status == 0) ? driver->allocate(&out, out_bytes) : status;
    status = (status == 0) ? driver->copy_in(in, given, in_bytes) : status;
    free(given);
    status = (status == 0) ? driver->clear(out, CUBIN_RUN_FILL, out_bytes) : status;
    void *arguments[] = {&in, &out, &count};
    unsigned long long answered_latency = 0ull;
    status = (status == 0) ? cubin_run_bounded(driver, function, arguments, shape, &answered_latency) : status;
    if (s_launch_timed_out)
    {
        free(written);
        *nanoseconds = 0ull;
        return status;
    }
    status = (status == 0) ? driver->copy_out(written, out, out_bytes) : status;
    if (status == 0)
    {
        cubin_run_answers_read(written, count);
    }
    free(written);
    *nanoseconds = 0ull;
    status = ((status == 0) && (launches != 0u))
                 ? cubin_run_timed(driver, function, arguments, shape, launches, nanoseconds)
                 : status;
    if (s_launch_timed_out)
    {
        return status;
    }
    // what the question held given back, so that a process carrying many holds one question's at a time
    if (in != 0ull)
    {
        driver->release(in);
    }
    if (out != 0ull)
    {
        driver->release(out);
    }
    if (module != NULL)
    {
        driver->module_unload(module);
    }
    return status;
}

// the driver, opened once a question first passes the gate, and 1 once it is
CubinRunDriver s_driver;

int s_driver_opened = 0;

// 1 where the run is dry: every question is written and held to the gate as ever, and none reaches the driver
int s_dry = 0;

                                 // against the vendor's own disassembler, which scaffolding compiles the same C through
unsigned int s_facets = 0u;

// The container as it would reach the part, `size` bytes of it, handed as the run's blob named as `answers_path` with
// .cubin for its extension: the --diff-output-against-vendor facet's object, for scaffolding to read against the
// vendor's own disassembler. It is the post-safety-word container, handed whatever the gate's verdict; a question the
// gate held is read too
void cubin_run_container_out(const char *answers_path, const unsigned char *container, unsigned long long size)
{
    char path[CUBIN_RUN_LINE];
    const char *const dot = strrchr(answers_path, '.');
    const int stem = (dot != NULL) ? (int)(dot - answers_path) : (int)strlen(answers_path);
    snprintf(path, sizeof(path), "%.*s.cubin", stem, answers_path);
    qry_hand(s_run, path, container, size, QRY_ORDINARY);
}

// What a dry run would have handed the part, a record of each question: the question's shape, the gate's verdict, its
// cases, and each place of its code. With the --query facet every place is written; without it, only the places that
// are not as the run's first question holds them, read back as our reader reads them. The first question's code is
// kept as dry_base.bin, and every place of it is written. The record is a blob dry.txt of the run's buffer, a blob
// a question, and the base the blob dry_base.bin
void cubin_run_dry_written(unsigned long long code_size, unsigned int registers, unsigned int launches,
                                  const CubinRunShape *shape, unsigned int verdict, unsigned long long at)
{
    const char *const path = "dry.txt";
    const char *const base_path = "dry_base.bin";
    CubinRunSaying *const dry = &s_dry_said;
    cubin_run_say(dry,
                  "question: %llu bytes of code, %u registers, %u threads in %u blocks, %u launches, %u cases; the "
                  "gate: %s",
                  code_size, registers, shape->threads, shape->blocks, launches, s_case_count,
                  cubin_safe_name(verdict));
    if (verdict != CUBIN_SAFE)
    {
        cubin_run_say(dry, " at byte %llu", at);
    }
    // the first two words of each case, the first 64 cases where a question holds more
    cubin_run_say(dry, "\n  cases");
    for (unsigned int place = 0u; (place < s_case_count) && (place < 64u); place += 1u)
    {
        cubin_run_say(dry, " %x,%x", s_cases[place][0], s_cases[place][1]);
    }
    cubin_run_say(dry, "%s\n", (s_case_count > 64u) ? " ..." : "");
    static unsigned char s_base[CUBIN_RUN_BYTES];
    const unsigned long long base_size = cubin_run_input(base_path, s_base, sizeof(s_base));
    if (base_size == 0ull)
    {
        qry_hand(s_run, base_path, s_code, code_size, QRY_ORDINARY);
    }
    for (unsigned long long place = 0ull; (place + 16ull) <= code_size; place += 16ull)
    {
        const int base_held = ((place + 16ull) <= base_size) && (memcmp(&s_code[place], &s_base[place], 16u) == 0);
        if (base_held && !(s_facets & CUBIN_FACET_QUERY))
        {
            continue;
        }
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            low |= (unsigned long long)s_code[place + byte] << (8u * byte);
            high |= (unsigned long long)s_code[place + 8u + byte] << (8u * byte);
        }
        char text[256];
        if (!sass_encoding_read(&s_machine, low, high, place, text, sizeof(text)))
        {
            snprintf(text, sizeof(text), "no form");
        }
        cubin_run_say(dry, "  %04llx  %016llx %016llx  %s\n", place, low, high, text);
    }
    cubin_run_said(dry, path, QRY_ORDINARY, 0);
}
