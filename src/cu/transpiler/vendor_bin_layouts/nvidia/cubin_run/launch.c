// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// launch.c: the launch: the host's clock and its turn given up, the launch bounded and timed, the answers read, the
// container out, and a dry run written
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "cubin_run_internal.h"
#include "memoria_operor/memoria_operor.h"

CubinRunClocks s_clocks;

// the host's clock in nanoseconds, from a start of its own and only ever read against itself
unsigned long long cubin_run_host_now(void)
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
    return ((unsigned long long)now.tv_sec * 1000000000ULL) + (unsigned long long)now.tv_nsec;
#endif
}

// the host's turn given up while the part runs, so that reading the stop event does not occupy the processor
static void cubin_run_host_yield(void)
{
    platform_paused(0ULL);
}

// 1 where the last launch broke rule 7 of cubin_safe. The process launches nothing after it, and leaves the memory that
// the launch holds where it is. A launch that the part did not give back can still use it
int s_launch_timed_out = 0;

// 1 where the last launch broke rule 8 of cubin_safe. The call reads it from the fault telemetry of the part, or the
// own word of the driver on the stop event. Then the part faulted, its context is corrupt, and the program answers the
// launch as refused
int s_launch_faulted = 0;

// the events that a launch goes between, made once for the process and recorded again for each launch
static void *s_start = NULL;

static void *s_stop = NULL;

// the memory of the part that a launch takes, allotted once and kept at the largest size that a question of the
// process needs. It holds the cases, a word for each thread, and the room where each thread writes its answer
static CubinRunAddress s_in = 0ULL;

static size_t s_in_room = 0u;

static CubinRunAddress s_out = 0ULL;

static size_t s_out_room = 0u;

// the part's memory at `*address`, `*room` bytes of it, grown to `needed`: given back and allotted again where it is
// smaller, retained where it is not. 0, or the status the driver gave
static CubinRunStatus cubin_run_room(const CubinRunDriver *driver, CubinRunAddress *address, size_t *room,
                                     size_t needed)
{
    if (*room >= needed)
    {
        return 0;
    }
    if (*address != 0ULL)
    {
        driver->release(*address);
        *address = 0ULL;
        *room = 0u;
    }
    const CubinRunStatus status = driver->allocate(address, needed);
    *room = (status == 0) ? needed : 0u;
    return status;
}

// One launch of `function` over `arguments` under rule 7 of cubin_safe. It runs alone between two events that the part
// stamps from its own timer. The call reads its stop event with no wait on it. It reads until the part reaches it, the
// driver gives an error, or CUBIN_SAFE_LAUNCH_BOUND passes on the host clock. Its block latency in nanoseconds goes
// into `nanoseconds`.
//
// The return is 0, or the status that the driver gave. It is CUDA_ERROR_LAUNCH_TIMEOUT where the launch
// broke the rule, by the own timeout of the driver or by the bound
static CubinRunStatus cubin_run_bounded(const CubinRunDriver *driver, void *function, void **arguments,
                                        const CubinRunShape *shape, unsigned long long *nanoseconds)
{
    float milliseconds = 0.0f;
    *nanoseconds = 0ULL;
    if (s_launch_timed_out)
    {
        return CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT;
    }
    // every fault already stamped thrown away first; only one the launch to come stamps then counts against it (rule 8)
    const unsigned long long drain_began = cubin_run_host_now();
    cubin_run_telemetry_drain();
    const unsigned long long launch_began = cubin_run_host_now();
    s_clocks.telemetry += launch_began - drain_began;
    CubinRunStatus status = (s_start != NULL) ? 0 : driver->event_create(&s_start, 0u);
    status = ((status == 0) && (s_stop == NULL)) ? driver->event_create(&s_stop, 0u) : status;
    status = (status == 0) ? driver->event_record(s_start, NULL) : status;
    status = (status == 0)
                 ? driver->launch(function, shape->blocks, 1u, 1u, shape->threads, 1u, 1u, 0u, NULL, arguments, NULL)
                 : status;
    status = (status == 0) ? driver->event_record(s_stop, NULL) : status;
    const unsigned long long began = cubin_run_host_now();
    unsigned long long waited = 0ULL;
    while (status == 0)
    {
        const CubinRunStatus reached = driver->event_query(s_stop);
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
    // rule 8: the call reads the fault telemetry of the part, and the own word of the driver on the stop event with
    // it. A launch that faulted the part halts the process, and the program answers it as refused. The error of the
    // driver names it. Where only the stamp caught it, the name is a failed launch
    const unsigned long long faulted_began = cubin_run_host_now();
    s_clocks.launch += faulted_began - launch_began;
    const int faulted = cubin_run_telemetry_faulted(status);
    s_clocks.telemetry += cubin_run_host_now() - faulted_began;
    if (cubin_safe_fault(faulted, status) == CUBIN_SAFE_LAUNCH_FAULT)
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
    status = (status == 0) ? driver->event_elapsed(&milliseconds, s_start, s_stop) : status;
    // the driver gives the time as milliseconds in a float; a nanosecond is a millionth of one
    *nanoseconds = (unsigned long long)((double)milliseconds * 1000000.0);
    return status;
}

// The least and the max block latency of the last timed question, the runs P6 reads (cubin_run_timed)
unsigned long long s_timed_least;

unsigned long long s_timed_max;

// the register count that the part reports for the last kernel that a launch loaded (cuFuncGetAttribute), or
// CUBIN_RUN_REGISTERS_UNREAD before the call reads it. The program reads the count from the part, and no question walks
// up to the boundary to find it
unsigned int s_part_registers = CUBIN_RUN_REGISTERS_UNREAD;

// `launches` launches of `function` over `arguments`, one at a time, each under rule 7 of cubin_safe. The call sums
// their block latencies into `nanoseconds`, and puts the least and the max of them into s_timed_least and
// s_timed_max. The return is 0, or the status that the driver gave the first launch that did not return clean. The
// call launches none after it. The timer belongs to the part and not to the host, and counts the same at any clock of
// the multiprocessors of the part
static CubinRunStatus cubin_run_timed(const CubinRunDriver *driver, void *function, void **arguments,
                                      const CubinRunShape *shape, unsigned int launches,
                                      unsigned long long *nanoseconds)
{
    CubinRunStatus status = 0;
    *nanoseconds = 0ULL;
    s_timed_least = 0ULL;
    s_timed_max = 0ULL;
    for (unsigned int launch = 0u; (status == 0) && (launch < launches); launch += 1u)
    {
        unsigned long long latency = 0ULL;
        // the timer register read before and after the launch where one is open: the launch's cost is its
        // advance, in the clock's own count. Otherwise the driver's event time
        unsigned int before = 0u;
        unsigned int after = 0u;
        const int clocked = cubin_run_clock_read(&before);
        status = cubin_run_bounded(driver, function, arguments, shape, &latency);
        if ((status == 0) && clocked && cubin_run_clock_read(&after))
        {
            latency = (unsigned long long)(after - before);
        }
        if (status == 0)
        {
            *nanoseconds += latency;
            s_timed_least = ((launch == 0u) || (latency < s_timed_least)) ? latency : s_timed_least;
            s_timed_max = (latency > s_timed_max) ? latency : s_timed_max;
        }
    }
    return status;
}

// The answers, read as the kernel wrote them into `written`. Before the launch, the room of `count` threads holds
// CUBIN_RUN_FILL_WORD. Every thread writes its answer one stride from the thread before it. The writes reach
// `(count - 1)` strides plus the words that one thread writes. The stride is that reach over the count, rounded up. The
// words that a thread writes are what the write of the last thread adds past the strides before it.
//
// That is true where the room that a thread leaves past what it writes is less than the count of threads. It must also
// be true that the last word that the last thread writes differs from the fill. The answer of each case goes into
// s_answers. It is the first two words that its thread wrote, with a word that it did not write read as 0
static void cubin_run_answers_read(const unsigned int *written, unsigned int count)
{
    unsigned long long reach = (unsigned long long)count * CUBIN_RUN_OUT_ROOM;
    while ((reach > 0ULL) && (written[reach - 1ULL] == CUBIN_RUN_FILL_WORD))
    {
        reach -= 1ULL;
    }
    const unsigned long long stride = (reach + count - 1ULL) / count;
    const unsigned long long words = (stride != 0ULL) ? (reach - ((unsigned long long)(count - 1u) * stride)) : 0ULL;
    for (unsigned int place = 0u; place < s_case_count; place += 1u)
    {
        for (unsigned int word = 0u; word < CUBIN_RUN_OUT_WORDS; word += 1u)
        {
            s_answers[place][word] = (word < words) ? written[((unsigned long long)place * stride) + word] : 0u;
        }
    }
}

// The container in s_container, loaded, with its kernel launched as `shape` specifies. Every thread gets a case: thread
// t takes case t of the cases, taken round. The answer of each case goes into s_answers as the kernel wrote it. Where
// `launches` is not 0, the call times that many launches after it into `nanoseconds`. The return is 0, or the status
// that the driver gave
CubinRunStatus cubin_run_launch(const CubinRunDriver *driver, const char *kernel, const CubinRunShape *shape,
                                unsigned int launches, unsigned long long *nanoseconds)
{
    // the driver and the part's primary context, reached once for the process
    static int s_context_set = 0;
    int device = 0;
    void *context = NULL;
    void *module = NULL;
    void *function = NULL;
    unsigned int count = shape->threads * shape->blocks;
    const size_t in_bytes = (size_t)count * sizeof(s_cases[0]);
    const size_t out_bytes = (size_t)count * CUBIN_RUN_OUT_ROOM * sizeof(unsigned int);
    unsigned int (*const given)[CUBIN_RUN_IN_WORDS] = platform_cell_taken(in_bytes, platform_place_g_program_owner, 0);
    unsigned int *const written = platform_cell_taken(out_bytes, platform_place_g_program_owner, 0);
    if ((given == NULL) || (written == NULL))
    {
        platform_cell_released(given, platform_place_g_program_owner);
        platform_cell_released(written, platform_place_g_program_owner);
        return -1;
    }
    for (unsigned int thread = 0u; thread < count; thread += 1u)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = given[thread], .src = s_cases[thread % s_case_count],
                   .bytes = sizeof(s_cases[0]));
    }
    const unsigned long long load_began = cubin_run_host_now();
    CubinRunStatus status = s_context_set ? 0 : driver->init(0u);
    status = ((status == 0) && !s_context_set) ? driver->device_get(&device, 0) : status;
    status = ((status == 0) && !s_context_set) ? driver->context_retain(&context, device) : status;
    status = ((status == 0) && !s_context_set) ? driver->context_set(context) : status;
    s_context_set = s_context_set || (status == 0);
    status = (status == 0) ? driver->module_load(&module, s_container) : status;
    status = (status == 0) ? driver->function_get(&function, module, kernel) : status;
    if (status == 0)
    {
        // the count of the registers that the kernel allots, as the part counts it, read from the loaded function
        // before any launch. A count that the attribute holds in a signed int is never negative
        int reported = 0;
        s_part_registers =
            ((driver->function_attribute(&reported, CUBIN_RUN_ATTRIBUTE_NUM_REGS, function) == 0) && (reported >= 0))
                ? (unsigned int)reported
                : CUBIN_RUN_REGISTERS_UNREAD;
    }
    const unsigned long long memory_began = cubin_run_host_now();
    s_clocks.load += memory_began - load_began;
    status = (status == 0) ? cubin_run_room(driver, &s_in, &s_in_room, in_bytes) : status;
    status = (status == 0) ? cubin_run_room(driver, &s_out, &s_out_room, out_bytes) : status;
    status = (status == 0) ? driver->copy_in(s_in, given, in_bytes) : status;
    platform_cell_released(given, platform_place_g_program_owner);
    status = (status == 0) ? driver->clear(s_out, CUBIN_RUN_FILL, out_bytes) : status;
    s_clocks.memory += cubin_run_host_now() - memory_began;
    void *arguments[] = {&s_in, &s_out, &count};
    unsigned long long answered_latency = 0ULL;
    status = (status == 0) ? cubin_run_bounded(driver, function, arguments, shape, &answered_latency) : status;
    if (s_launch_timed_out)
    {
        platform_cell_released(written, platform_place_g_program_owner);
        *nanoseconds = 0ULL;
        return status;
    }
    const unsigned long long read_began = cubin_run_host_now();
    status = (status == 0) ? driver->copy_out(written, s_out, out_bytes) : status;
    if (status == 0)
    {
        cubin_run_answers_read(written, count);
    }
    platform_cell_released(written, platform_place_g_program_owner);
    s_clocks.memory += cubin_run_host_now() - read_began;
    *nanoseconds = 0ULL;
    status = ((status == 0) && (launches != 0u))
                 ? cubin_run_timed(driver, function, arguments, shape, launches, nanoseconds)
                 : status;
    if (s_launch_timed_out)
    {
        return status;
    }
    // the module of the question, given back, so that a process that carries many loads the code of one question at
    // a time. The call keeps the memory that its launch took for the next question
    const unsigned long long freed_began = cubin_run_host_now();
    if (module != NULL)
    {
        driver->module_unload(module);
    }
    s_clocks.memory += cubin_run_host_now() - freed_began;
    return status;
}

// the driver, opened once a question first passes the gate, and 1 once it is
CubinRunDriver s_driver;

int s_driver_opened = 0;

// 1 where the run is dry. The program writes every question and checks it against the gate as always, and none
// reaches the driver
int s_dry = 0;

// against the vendor's own disassembler, which scaffolding compiles the same C through
unsigned int s_facets = 0u;

// The container as it would reach the part, `size` bytes of it, passed as the blob of the run named as `answers_path`
// with .cubin for its extension. It is the object of the --diff-output-against-vendor facet, for scaffolding to read
// against the own disassembler of the vendor. It is the container after the safety word, passed whatever the verdict
// of the gate. The call also passes a question that the gate blocked
void cubin_run_container_out(const char *answers_path, const unsigned char *container, unsigned long long size)
{
    char path[CUBIN_RUN_LINE];
    const char *const dot = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = answers_path,
                                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    const int stem = (dot != NULL) ? (int)(dot - answers_path)
                                   : (int)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = answers_path,
                                                     .cap = PLATFORM_TEXT_TO_TERMINATOR);
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put_max(&buffer_line, answers_path, (size_t)(stem));
        mmgr_line_put(&buffer_line, ".cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    qry_member_append(path, container, size, QRY_ORDINARY);
}

// What a dry run would pass the part, a record for each question. It has the shape of the question, the verdict of the
// gate, its cases, and each place of its code. With the --query facet, the call writes every place. Without it, it
// writes only the places that differ from the first question of the run, read back as our reader reads them. The call
// stores the code of the first question as dry_base.bin, and writes every place of it. The record is a blob dry.txt of
// the run, a blob for each question, and the base, the blob dry_base.bin
void cubin_run_dry_written(unsigned long long code_size, unsigned int registers, unsigned int launches,
                           const CubinRunShape *shape, unsigned int verdict, unsigned long long at)
{
    const char *const path = "dry.txt";
    const char *const base_path = "dry_base.bin";
    VerbaLinea *const dry = &s_dry_line;
    mmgr_line_put(dry, "question: ");
    mmgr_line_decimal(dry, (unsigned long long)(code_size), 0u);
    mmgr_line_put(dry, " bytes of code, ");
    mmgr_line_decimal(dry, (unsigned long long)(registers), 0u);
    mmgr_line_put(dry, " registers, ");
    mmgr_line_decimal(dry, (unsigned long long)(shape->threads), 0u);
    mmgr_line_put(dry, " threads in ");
    mmgr_line_decimal(dry, (unsigned long long)(shape->blocks), 0u);
    mmgr_line_put(dry, " blocks, ");
    mmgr_line_decimal(dry, (unsigned long long)(launches), 0u);
    mmgr_line_put(dry, " launches, ");
    mmgr_line_decimal(dry, (unsigned long long)(s_case_count), 0u);
    mmgr_line_put(dry, " cases; the gate: ");
    mmgr_line_put(dry, cubin_safe_name(verdict));
    if (verdict != CUBIN_SAFE)
    {
        mmgr_line_put(dry, " at byte ");
        mmgr_line_decimal(dry, (unsigned long long)(at), 0u);
    }
    // the first two words of each case, the first 64 cases where a question contains more
    mmgr_line_put(dry, "\n  cases");
    for (unsigned int place = 0u; (place < s_case_count) && (place < 64u); place += 1u)
    {
        mmgr_line_put(dry, " ");
        mmgr_line_hex(dry, (unsigned long long)(s_cases[place][0]), 0u);
        mmgr_line_put(dry, ",");
        mmgr_line_hex(dry, (unsigned long long)(s_cases[place][1]), 0u);
    }
    mmgr_line_put(dry, (s_case_count > 64u) ? " ..." : "");
    mmgr_line_put(dry, "\n");
    static unsigned char s_base[CUBIN_RUN_BYTES];
    const unsigned long long base_size = cubin_run_input(base_path, s_base, sizeof(s_base));
    if (base_size == 0ULL)
    {
        qry_member_append(base_path, s_code, code_size, QRY_ORDINARY);
    }
    for (unsigned long long place = 0ULL; (place + 16ULL) <= code_size; place += 16ULL)
    {
        const int base_matches =
            ((place + 16ULL) <= base_size) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &s_code[place], .other = &s_base[place], .bytes = 16u) == 0);
        if (base_matches && !(s_facets & CUBIN_FACET_QUERY))
        {
            continue;
        }
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            low |= (unsigned long long)s_code[place + byte] << (8u * byte);
            high |= (unsigned long long)s_code[place + 8u + byte] << (8u * byte);
        }
        char text[256];
        if (!sass_encoding_read(&s_machine, low, high, place, text, sizeof(text)))
        {
            VerbaLinea buffer_line = mmgr_line_over(text, sizeof(text));
            mmgr_line_put(&buffer_line, "no form");
            (void)mmgr_line_finish(&buffer_line);
        }
        mmgr_line_put(dry, "  ");
        mmgr_line_hex(dry, (unsigned long long)(place), 4u);
        mmgr_line_put(dry, "  ");
        mmgr_line_hex(dry, (unsigned long long)(low), 16u);
        mmgr_line_put(dry, " ");
        mmgr_line_hex(dry, (unsigned long long)(high), 16u);
        mmgr_line_put(dry, "  ");
        mmgr_line_put(dry, text);
        mmgr_line_put(dry, "\n");
    }
    cubin_run_member_written(dry, path, QRY_ORDINARY, 0);
}
