// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_run.c: a question carried to the part: its code put in the container the system accepted, held to
// cubin_safe on the host, and run over its cases, a thread a case. This is the run channel's carrier for a part behind
// NVIDIA's driver (run_channel.h). A question is carried in a process of its own, so a part that refuses one takes
// this process with it and nothing else, or with --list, many untimed questions in one process, the driver opened
// once, each answered to its own file as it is carried: a part that refuses one takes the process with it, every
// answer written before it stands, and the questions after it are left for the channel to carry again.
//
//     cubin_run <machine file> <layout> <code> <registers> <cases> <answers> <slot> [<launches> [<threads> <blocks>]]
//     cubin_run <machine file> <layout> --list <list>      a line a question, <code> <registers> <cases> <answers> <slot>
//     [<launches>]
//     cubin_run --dry <machine file> <layout> ...          either of the above, with the part taken out
//
// A dry run writes and holds every question as ever and stops before the driver: each question is answered
// `skipped dry, the part taken out`, and what it would have handed the part is added to dry.txt beside its answers
// (cubin_run_dry_written). Handed the list a dry run of the protocol writes (run_channel.h), it checks every
// question the protocol put.
//
// After --dry come the facets, each popping out one object of the carry so it can be read off the part; --full pops
// them all:
//   --query                      the query object: every place of the code read back as our reader names it, not
//                                only the places that are not as the run's first question holds them
//   --gate-check                 the safety word laid on and cubin_safe's verdict on what it then holds
//   --diff-output-against-vendor the post-safety-word container as it would reach the part, written beside the answers
//                                with .cubin, for scaffolding to read against the vendor's own disassembler, the same
//                                C compiled through the vendor's compiler beside it. Our gate judges by operation key;
//                                only the vendor's reader knows a whole encoding illegal
//
// The code is the question's machine code, sixteen bytes an instruction. The layout's container rows hold the container
// (cubin_write.h), and the code, the registers and the exits are put in it. The cases are one a line, up to
// eight words in hex, the words a line leaves out zero, and the kernel is handed the stick's arity: in, eight words a
// thread, out, room for eight words a thread, and the count of threads. The answers are read as the kernel wrote them:
// how far apart its threads write and how many words each writes are read from where the part wrote, and a case's
// answer is the first two words its thread wrote (cubin_run_answers_read). It is launched in blocks of <threads>, <blocks> of them,
// blocks of 256 where the question names no shape, as many as give every case a thread, and every thread is given a
// case, thread t case t of the cases taken round: the cases are answered by the first threads, and the rest do the
// same work over again. One line is written to the answers:
//
//     answered <answer>...      each case's two words as one value in hex, in the order of the cases
//     skipped <verdict> <name>  cubin_safe held it off the part, and the driver never saw it
//     skipped no vendor verdict a live run was given no --held file: the vendor's reader never read this code for a
//                               fault. It is held off the part, the cross must read it first (the safety airlock)
//     skipped <reason>          the container could not be written, or its launch gives the cases no thread each
//                               or more threads than a launch holds, and the driver never saw it
//     refused <error>           the driver or the part would not take it
//
// Every launch, the one that answers and each timed one, is carried alone under cubin_safe's rule 6: between two events
// the part stamps, its stop event read without waiting on it until the part reaches it, the driver gives an error, or
// CUBIN_SAFE_LAUNCH_BOUND passes on the host's clock (cubin_run_bounded). The time between the events is the launch's
// block latency. A launch the driver times out, or one past the bound with its stop event unreached, is answered
// `refused CUDA_ERROR_LAUNCH_TIMEOUT`, and nothing is launched after it in the process.
//
// After each launch the part's own fault telemetry is read, the Xid critical errors the driver's management layer
// stamps (cubin_run_telemetry_open, cubin_run_telemetry_faulted), under cubin_safe's rule 7. A launch that makes the
// part stamp one, or one the driver names a context-corrupting fault itself, has faulted the part: it is answered
// `refused`, and nothing is launched after it in the process. This catches an out-of-range register, which faults in
// microseconds and so never trips rule 6's bound, before its fault storm reaches the system's watchdog.
//
// Given a count of launches, a question that answered is launched that many times more, one at a time, and a second
// line gives the block latencies of those launches summed, then the least and the most of them, the runs P6 reads
// (cubin_run_timed):
//
//     timed <launches> <nanoseconds> <least> <most>
//
// The driver is reached by name and by its own entry points, and nothing of its vendor's toolchain is compiled
// against. Exit 0 where a line was written, 2 where the files, the machine or the driver were not reached.
#include "cubin_safe.h"
#include "cubin_write.h"
#include "sass_assemble.h"
#include "sass_machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <sched.h>
#include <time.h>
#endif

// the most bytes a container and its code take, and the longest line of the cases
#define CUBIN_RUN_BYTES 262144u
#define CUBIN_RUN_LINE 1024u
// the words of a case and of an answer, the stick's arity, and the threads a block holds where the question names
// no shape of its own
#define CUBIN_RUN_IN_WORDS 8u
#define CUBIN_RUN_OUT_WORDS 2u
// the room each thread is given to write its answer in, as many words as a case, and the byte that room is filled
// with before a launch, so that the words the kernel wrote are told from the words it did not
#define CUBIN_RUN_OUT_ROOM CUBIN_RUN_IN_WORDS
#define CUBIN_RUN_FILL 0xffu
#define CUBIN_RUN_FILL_WORD (CUBIN_RUN_FILL * 0x01010101u)
#define CUBIN_RUN_THREADS 256u
// the most threads one launch gives a case each, and the most cases a question gives: every case is a thread's
#define CUBIN_RUN_THREADS_MOST (1u << 20u)
// the exits a question's code holds at most
#define CUBIN_RUN_EXITS 256u

// the driver's own types as its entry points take them: a status, handles, and a device address
typedef int CubinRunStatus;
typedef unsigned long long CubinRunAddress;

// how a question is launched: the threads of a block and the blocks of a launch
typedef struct
{
    unsigned int threads;
    unsigned int blocks;
} CubinRunShape;

// the driver's entry points this reaches, each by the name the driver exports it under
typedef struct
{
    CubinRunStatus (*init)(unsigned int);
    CubinRunStatus (*device_get)(int *, int);
    CubinRunStatus (*context_retain)(void **, int);
    CubinRunStatus (*context_set)(void *);
    CubinRunStatus (*module_load)(void **, const void *);
    CubinRunStatus (*function_get)(void **, void *, const char *);
    CubinRunStatus (*allocate)(CubinRunAddress *, size_t);
    CubinRunStatus (*copy_in)(CubinRunAddress, const void *, size_t);
    CubinRunStatus (*clear)(CubinRunAddress, unsigned char, size_t);
    CubinRunStatus (*launch)(void *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int,
                             unsigned int, void *, void **, void **);
    CubinRunStatus (*synchronize)(void);
    CubinRunStatus (*copy_out)(void *, CubinRunAddress, size_t);
    CubinRunStatus (*error_name)(CubinRunStatus, const char **);
    CubinRunStatus (*event_create)(void **, unsigned int);
    CubinRunStatus (*event_record)(void *, void *);
    CubinRunStatus (*event_synchronize)(void *);
    CubinRunStatus (*event_query)(void *);
    CubinRunStatus (*event_elapsed)(float *, void *, void *);
    CubinRunStatus (*event_destroy)(void *);
    CubinRunStatus (*module_unload)(void *);
    CubinRunStatus (*release)(CubinRunAddress);
} CubinRunDriver;

static SassMachine s_machine;
static unsigned char s_pattern[CUBIN_RUN_BYTES];
static unsigned char s_code[CUBIN_RUN_BYTES];
static unsigned char s_container[CUBIN_RUN_BYTES];
static unsigned int s_exits[CUBIN_RUN_EXITS];
static unsigned int s_cases[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_IN_WORDS];
static unsigned int s_case_count;
static unsigned int s_answers[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_OUT_WORDS];

// `path` read whole into `bytes`, which holds `room`: the bytes read, or 0 where it was not read or does not fit
static unsigned long long cubin_run_file(const char *path, unsigned char *bytes, unsigned long long room)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0ull;
    }
    const unsigned long long read = (unsigned long long)fread(bytes, 1u, (size_t)room, file);
    fclose(file);
    return (read < room) ? read : 0ull;
}

// the cases file at `path` read into s_cases: 1, or 0 where it does not read, holds no case or holds more than a
// launch's threads
static int cubin_run_cases(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    char line[CUBIN_RUN_LINE];
    unsigned int count = 0u;
    int fits = 1;
    while (fits && (fgets(line, sizeof(line), file) != NULL))
    {
        unsigned int words[CUBIN_RUN_IN_WORDS] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        const int read = sscanf(line, "%x %x %x %x %x %x %x %x", &words[0], &words[1], &words[2], &words[3], &words[4],
                                &words[5], &words[6], &words[7]);
        if (read <= 0)
        {
            continue;
        }
        fits = (count < CUBIN_RUN_THREADS_MOST);
        if (fits)
        {
            memcpy(s_cases[count], words, sizeof(words));
            count += 1u;
        }
    }
    fclose(file);
    s_case_count = count;
    return fits && (count != 0u);
}

// the address of the entry point `name` in `library`, or NULL where it exports none
static void *cubin_run_entry(void *library, const char *name)
{
#if defined(_WIN32)
    return (void *)GetProcAddress((HMODULE)library, name);
#else
    return dlsym(library, name);
#endif
}

// the driver opened by name and every entry point taken: 1, or 0 where the driver or an entry point is not there
static int cubin_run_driver(CubinRunDriver *driver)
{
#if defined(_WIN32)
    void *const library = (void *)LoadLibraryA("nvcuda.dll");
#else
    void *const library = dlopen("libcuda.so.1", RTLD_NOW);
#endif
    if (library == NULL)
    {
        return 0;
    }
    // each entry point's address is the function the driver exports under that name, which takes what its member
    // says: an object pointer is turned into a function pointer here and nowhere else
    *(void **)&driver->init = cubin_run_entry(library, "cuInit");
    *(void **)&driver->device_get = cubin_run_entry(library, "cuDeviceGet");
    *(void **)&driver->context_retain = cubin_run_entry(library, "cuDevicePrimaryCtxRetain");
    *(void **)&driver->context_set = cubin_run_entry(library, "cuCtxSetCurrent");
    *(void **)&driver->module_load = cubin_run_entry(library, "cuModuleLoadData");
    *(void **)&driver->function_get = cubin_run_entry(library, "cuModuleGetFunction");
    *(void **)&driver->allocate = cubin_run_entry(library, "cuMemAlloc_v2");
    *(void **)&driver->copy_in = cubin_run_entry(library, "cuMemcpyHtoD_v2");
    *(void **)&driver->clear = cubin_run_entry(library, "cuMemsetD8_v2");
    *(void **)&driver->launch = cubin_run_entry(library, "cuLaunchKernel");
    *(void **)&driver->synchronize = cubin_run_entry(library, "cuCtxSynchronize");
    *(void **)&driver->copy_out = cubin_run_entry(library, "cuMemcpyDtoH_v2");
    *(void **)&driver->error_name = cubin_run_entry(library, "cuGetErrorName");
    *(void **)&driver->event_create = cubin_run_entry(library, "cuEventCreate");
    *(void **)&driver->event_record = cubin_run_entry(library, "cuEventRecord");
    *(void **)&driver->event_synchronize = cubin_run_entry(library, "cuEventSynchronize");
    *(void **)&driver->event_query = cubin_run_entry(library, "cuEventQuery");
    *(void **)&driver->event_elapsed = cubin_run_entry(library, "cuEventElapsedTime");
    *(void **)&driver->event_destroy = cubin_run_entry(library, "cuEventDestroy_v2");
    *(void **)&driver->module_unload = cubin_run_entry(library, "cuModuleUnload");
    *(void **)&driver->release = cubin_run_entry(library, "cuMemFree_v2");
    return (driver->module_unload != NULL) && (driver->release != NULL) && (driver->init != NULL) &&
           (driver->device_get != NULL) && (driver->context_retain != NULL) && (driver->context_set != NULL) &&
           (driver->module_load != NULL) && (driver->function_get != NULL) && (driver->allocate != NULL) &&
           (driver->copy_in != NULL) && (driver->clear != NULL) && (driver->launch != NULL) &&
           (driver->synchronize != NULL) && (driver->copy_out != NULL) && (driver->error_name != NULL) &&
           (driver->event_create != NULL) && (driver->event_record != NULL) && (driver->event_synchronize != NULL) &&
           (driver->event_query != NULL) && (driver->event_elapsed != NULL) && (driver->event_destroy != NULL);
}

// The part's fault telemetry, the driver's management layer (NVML), reached by name and by its own entry points as the
// driver is, and nothing of its vendor's toolchain compiled against. What cubin_safe's rule 7 reads after each launch:
// the Xid critical errors the part stamps against the device
typedef struct
{
    int (*init)(void);
    int (*shutdown)(void);
    int (*device_get)(unsigned int, void **);
    int (*set_create)(void **);
    int (*register_events)(void *, unsigned long long, void *);
    int (*set_wait)(void *, void *, unsigned int);
    int (*set_free)(void *);
} CubinRunTelemetry;

// the event type the Xid critical errors are registered and read under, and the event a wait fills: the part's handle,
// the type stamped and its datum, with room past them for the fields a newer management layer adds; an older one's
// shorter event is read into that same space
#define CUBIN_RUN_XID 0x0000000000000008ull
typedef struct
{
    void *device;
    unsigned long long event_type;
    unsigned long long event_data;
    unsigned int gpu_instance;
    unsigned int compute_instance;
    unsigned char room[32];
} CubinRunXidEvent;

static CubinRunTelemetry s_telemetry;
static void *s_telemetry_device = NULL;
static void *s_telemetry_set = NULL;
static int s_telemetry_ready = 0;

// The part's fault telemetry opened and its Xid critical errors registered on device 0, the device the carrier runs:
// 1 where it is ready to read, 0 with a warning where its library, an entry point, or the registration is not there.
// A part that reports no such error still opens, and the wait then only ever comes back empty
static int cubin_run_telemetry_open(void)
{
#if defined(_WIN32)
    void *const library = (void *)LoadLibraryA("nvml.dll");
#else
    void *const library = dlopen("libnvidia-ml.so.1", RTLD_NOW);
#endif
    if (library == NULL)
    {
        fprintf(stderr, "the part's fault telemetry was not reached: rule 7 does not guard this run\n");
        return 0;
    }
    *(void **)&s_telemetry.init = cubin_run_entry(library, "nvmlInit_v2");
    *(void **)&s_telemetry.shutdown = cubin_run_entry(library, "nvmlShutdown");
    *(void **)&s_telemetry.device_get = cubin_run_entry(library, "nvmlDeviceGetHandleByIndex_v2");
    *(void **)&s_telemetry.set_create = cubin_run_entry(library, "nvmlEventSetCreate");
    *(void **)&s_telemetry.register_events = cubin_run_entry(library, "nvmlDeviceRegisterEvents");
    *(void **)&s_telemetry.set_wait = cubin_run_entry(library, "nvmlEventSetWait_v2");
    *(void **)&s_telemetry.set_free = cubin_run_entry(library, "nvmlEventSetFree");
    if ((s_telemetry.init == NULL) || (s_telemetry.device_get == NULL) || (s_telemetry.set_create == NULL) ||
        (s_telemetry.register_events == NULL) || (s_telemetry.set_wait == NULL))
    {
        fprintf(stderr, "the part's fault telemetry has no entry point: rule 7 does not guard this run\n");
        return 0;
    }
    if ((s_telemetry.init() != 0) || (s_telemetry.device_get(0u, &s_telemetry_device) != 0) ||
        (s_telemetry.set_create(&s_telemetry_set) != 0) ||
        (s_telemetry.register_events(s_telemetry_device, CUBIN_RUN_XID, s_telemetry_set) != 0))
    {
        fprintf(stderr, "the part's fault telemetry did not register its critical errors: rule 7 does not guard this "
                        "run\n");
        return 0;
    }
    s_telemetry_ready = 1;
    return 1;
}

// Every Xid critical error already stamped read off and thrown away: only one stamped after this counts against the
// launch to come. Nothing where the telemetry is not ready
static void cubin_run_telemetry_drain(void)
{
    CubinRunXidEvent event;
    while (s_telemetry_ready)
    {
        memset(&event, 0, sizeof(event));
        if (s_telemetry.set_wait(s_telemetry_set, &event, 0u) != 0)
        {
            break;
        }
    }
}

// 1 where the part stamped an Xid critical error, read without waiting: the launch just carried faulted the part. 0
// where none is stamped or the telemetry is not ready
static int cubin_run_telemetry_faulted(void)
{
    if (!s_telemetry_ready)
    {
        return 0;
    }
    CubinRunXidEvent event;
    memset(&event, 0, sizeof(event));
    return (s_telemetry.set_wait(s_telemetry_set, &event, 0u) == 0) && ((event.event_type & CUBIN_RUN_XID) != 0ull);
}

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
static int s_launch_timed_out = 0;

// 1 where the last launch broke cubin_safe's rule 7, read from the part's fault telemetry or the driver's own word on
// the stop event: the part faulted, its context corrupt, and the launch is answered as refused
static int s_launch_faulted = 0;

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
    if (cubin_safe_fault(cubin_run_telemetry_faulted(), status) == CUBIN_SAFE_LAUNCH_FAULT)
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
static unsigned long long s_timed_least;
static unsigned long long s_timed_most;

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
static CubinRunStatus cubin_run_launch(const CubinRunDriver *driver, const char *kernel, const CubinRunShape *shape,
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
static CubinRunDriver s_driver;
static int s_driver_opened = 0;

// 1 where the run is dry: every question is written and held to the gate as ever, and none reaches the driver
static int s_dry = 0;

// the facets a dry run pops out, each the object a stage of the carry hands on, for any one to be examined off
// the part. The flags that set them come after --dry; --full sets them all
#define CUBIN_FACET_QUERY 1u     // --query: the query object, the whole code read back, every place our reader names it
#define CUBIN_FACET_GATE 2u      // --gate-check: the safety word laid on and cubin_safe's verdict on what it then holds
#define CUBIN_FACET_VENDOR 4u    // --diff-output-against-vendor: the post-safety-word container written out to be read
                                 // against the vendor's own disassembler, which scaffolding compiles the same C through
static unsigned int s_facets = 0u;

// The vendor's held list and the airlock it gives are the gate's, shared by every path that reaches the part
// (cubin_safe_verdict_load, cubin_safe_verdict_ready, cubin_safe_verdict_holds): the carrier reads the --held file into
// the gate and asks the gate before the driver, a slot instruction the verdict holds never reaching the part.

// The container as it would reach the part, `size` bytes of it, written beside `answers_path` with .cubin for its
// extension: the --diff-output-against-vendor facet's object, for scaffolding to read against the vendor's own
// disassembler. It is the post-safety-word container, written whatever the gate's verdict; a question the gate
// held is read too
static void cubin_run_container_out(const char *answers_path, const unsigned char *container, unsigned long long size)
{
    char path[CUBIN_RUN_LINE];
    const char *const dot = strrchr(answers_path, '.');
    const int stem = (dot != NULL) ? (int)(dot - answers_path) : (int)strlen(answers_path);
    snprintf(path, sizeof(path), "%.*s.cubin", stem, answers_path);
    FILE *const out = fopen(path, "wb");
    if (out != NULL)
    {
        fwrite(container, 1u, (size_t)size, out);
        fclose(out);
    }
}

// What a dry run would have handed the part, added to dry.txt in the folder of `answers_path`: the question's
// shape, the gate's verdict, its cases, and each place of its code. With the --query facet every place is written;
// without it, only the places that are not as the run's first question holds them, read back as our reader reads them.
// The first question's code is kept beside it as dry_base.bin, and every place of it is written
static void cubin_run_dry_written(const char *answers_path, unsigned long long code_size, unsigned int registers,
                                  unsigned int launches, const CubinRunShape *shape, unsigned int verdict,
                                  unsigned long long at)
{
    char path[CUBIN_RUN_LINE];
    const char *const forward = strrchr(answers_path, '/');
    const char *const back = strrchr(answers_path, '\\');
    const char *const slash = (back == NULL) ? forward : ((forward == NULL) || (back > forward)) ? back : forward;
    const int folder = (slash != NULL) ? (int)(slash - answers_path) : 1;
    snprintf(path, sizeof(path), "%.*s/dry.txt", folder, (slash != NULL) ? answers_path : ".");
    FILE *const dry = fopen(path, "ab");
    if (dry == NULL)
    {
        return;
    }
    fprintf(dry, "question: %llu bytes of code, %u registers, %u threads in %u blocks, %u launches, %u cases; the "
                 "gate: %s",
            code_size, registers, shape->threads, shape->blocks, launches, s_case_count, cubin_safe_name(verdict));
    if (verdict != CUBIN_SAFE)
    {
        fprintf(dry, " at byte %llu", at);
    }
    // the first two words of each case, the first 64 cases where a question holds more
    fprintf(dry, "\n  cases");
    for (unsigned int place = 0u; (place < s_case_count) && (place < 64u); place += 1u)
    {
        fprintf(dry, " %x,%x", s_cases[place][0], s_cases[place][1]);
    }
    fprintf(dry, "%s\n", (s_case_count > 64u) ? " ..." : "");
    static unsigned char s_base[CUBIN_RUN_BYTES];
    char base_path[CUBIN_RUN_LINE];
    snprintf(base_path, sizeof(base_path), "%.*s/dry_base.bin", folder, (slash != NULL) ? answers_path : ".");
    unsigned long long base_size = cubin_run_file(base_path, s_base, sizeof(s_base));
    if (base_size == 0ull)
    {
        FILE *const base = fopen(base_path, "wb");
        if (base != NULL)
        {
            fwrite(s_code, 1u, (size_t)code_size, base);
            fclose(base);
        }
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
        fprintf(dry, "  %04llx  %016llx %016llx  %s\n", place, low, high, text);
    }
    fclose(dry);
}

// One question carried: its code at `code_path` put in the container of `pattern_size` bytes in s_pattern, its kernel
// `kernel`, declaring `registers`, over the cases at `cases_path`, launched as `shape` says where `shaped`, timed over
// `launches` where that is not 0, and its line written to `answers_path`. 0 where a line was written, 1 where the line
// is the driver's or the part's refusal of a launch, after which the context answers no other question, and 2
// where the files, the machine or the driver were not reached
// one word of an instruction in `code` at `byte`, read and written back
static unsigned long long cubin_run_word(const unsigned char *code, unsigned int byte)
{
    unsigned long long value = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        value |= (unsigned long long)code[byte + at] << (8u * at);
    }
    return value;
}

static void cubin_run_word_set(unsigned char *code, unsigned int byte, unsigned long long value)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        code[byte + at] = (unsigned char)((value >> (8u * at)) & 0xffull);
    }
}

// the bits `first` to `last` of the instruction `low` and `high` make set to `value`
static void cubin_run_bits_set(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last,
                               unsigned long long value)
{
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        unsigned long long *const word = (bit < 64u) ? low : high;
        const unsigned long long one = (value >> (bit - first)) & 1ull;
        *word = (*word & ~(1ull << (bit % 64u))) | (one << (bit % 64u));
    }
}

// The safety word applied to `code` before the part sees it (P9): every instruction made to wait on all six barriers,
// and the slot at `slot`, where the code holds one, made to stall the longest and hold no barrier; a form the part
// has not named then waits on everything, and a barrier nothing releases leaves the wait after it standing. The protocol
// sends the code as the part accepted it and names the slot; the scheduler's own word is the carrier's to set
static void cubin_run_safe_word(unsigned char *code, unsigned long long code_size, unsigned int slot)
{
    const unsigned int places = (unsigned int)(code_size / 16ull);
    for (unsigned int place = 0u; place < places; place += 1u)
    {
        unsigned long long low = cubin_run_word(code, place * 16u);
        unsigned long long high = cubin_run_word(code, (place * 16u) + 8u);
        cubin_run_bits_set(&low, &high, SASS_WAIT_FIRST, SASS_WAIT_FIRST + 5u, SASS_WAIT_EVERY);
        if (place == slot)
        {
            cubin_run_bits_set(&low, &high, SASS_STALL_FIRST, SASS_STALL_FIRST + 3u, SASS_STALL_LONGEST);
            cubin_run_bits_set(&low, &high, SASS_WRITE_BARRIER_FIRST, SASS_WRITE_BARRIER_FIRST + 2u, SASS_BARRIER_NONE);
            cubin_run_bits_set(&low, &high, SASS_READ_BARRIER_FIRST, SASS_READ_BARRIER_FIRST + 2u, SASS_BARRIER_NONE);
        }
        cubin_run_word_set(code, place * 16u, low);
        cubin_run_word_set(code, (place * 16u) + 8u, high);
    }
}

static int cubin_run_question(const char *kernel, unsigned long long pattern_size, const char *code_path,
                              unsigned int registers, const char *cases_path, const char *answers_path,
                              unsigned int slot, unsigned int launches, CubinRunShape shape, int shaped)
{
    const unsigned long long code_size = cubin_run_file(code_path, s_code, sizeof(s_code));
    FILE *const answers = fopen(answers_path, "wb");
    if ((pattern_size == 0ull) || (code_size == 0ull) || !cubin_run_cases(cases_path) || (answers == NULL))
    {
        fprintf(stderr, "the container, the code, the cases or the answers did not read\n");
        if (answers != NULL)
        {
            fclose(answers);
        }
        return 2;
    }
    // a question that names no shape is given blocks of CUBIN_RUN_THREADS, as many as give every case a thread. A
    // launch gives every case a thread, and gives a case each to no more threads than it holds them for
    shape.blocks = shaped ? shape.blocks : ((s_case_count + CUBIN_RUN_THREADS - 1u) / CUBIN_RUN_THREADS);
    const unsigned long long threads = (unsigned long long)shape.threads * shape.blocks;
    if ((threads < s_case_count) || (threads > CUBIN_RUN_THREADS_MOST))
    {
        fprintf(answers, "skipped a launch of %u threads in %u blocks for %u cases\n", shape.threads, shape.blocks,
                s_case_count);
        fclose(answers);
        return 0;
    }
    // the scheduler's own word set here, before the gate and the part: the protocol sent the code as the part
    // accepted it and named the slot
    cubin_run_safe_word(s_code, code_size, slot);
    CubinWrite written;
    memset(&written, 0, sizeof(written));
    written.pattern = s_pattern;
    written.pattern_size = pattern_size;
    written.kernel = kernel;
    written.code = s_code;
    written.code_size = code_size;
    written.registers = registers;
    written.exit_count = cubin_exits_find(s_code, code_size, sass_exit_encoding(&s_machine), s_exits, CUBIN_RUN_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ull;
    if (!cubin_write(&written, s_container, sizeof(s_container), &size))
    {
        fprintf(answers, "skipped not written\n");
        fclose(answers);
        return 0;
    }
    // read on the host before the driver sees it: code that breaks a rule of cubin_safe is answered without the part,
    // its register fields bound by the count this question's kernel allots (rule 6)
    unsigned long long at = 0ull;
    const unsigned int verdict = cubin_safe_image(&s_machine, s_container, size, registers, &at);
    if (s_dry)
    {
        if (s_facets & CUBIN_FACET_VENDOR)
        {
            cubin_run_container_out(answers_path, s_container, size);
        }
        cubin_run_dry_written(answers_path, code_size, registers, launches, &shape, verdict, at);
    }
    if (verdict != CUBIN_SAFE)
    {
        fprintf(answers, "skipped %u %s at byte %llu\n", verdict, cubin_safe_name(verdict), at);
        fclose(answers);
        return 0;
    }
    // a dry run stops here, before the driver: the question is answered as one held off the part
    if (s_dry)
    {
        fprintf(answers, "skipped dry, the part taken out\n");
        fclose(answers);
        return 0;
    }
    // the safety airlock: a live run reaches the part only with the vendor's verdict in hand (--held). Without it this
    // code was never read for a fault, and an unread encoding can fault the part and crash the machine: it is held off
    // the part here, the cross must read it first
    if (!cubin_safe_verdict_ready())
    {
        fprintf(answers, "skipped no vendor verdict, the cross must read this code before the part\n");
        fclose(answers);
        return 0;
    }
    // a slot instruction the vendor's reader held off the part was already caught by cubin_safe_image above, where the
    // verdict is loaded (cubin_safe_verdict_holds): the gate holds the whole code, not the slot alone
    if (!s_driver_opened)
    {
        memset(&s_driver, 0, sizeof(s_driver));
        if (!cubin_run_driver(&s_driver))
        {
            fprintf(stderr, "the driver was not reached\n");
            fclose(answers);
            return 2;
        }
        // the part's fault telemetry opened beside the driver, for rule 7 to read after each launch; a run goes on
        // under rule 6 alone where it is not there, the warning printed
        cubin_run_telemetry_open();
        s_driver_opened = 1;
    }
    unsigned long long nanoseconds = 0ull;
    const CubinRunStatus status = cubin_run_launch(&s_driver, kernel, &shape, launches, &nanoseconds);
    if (status != 0)
    {
        const char *name = NULL;
        s_driver.error_name(status, &name);
        fprintf(answers, "refused %s%s\n", (name != NULL) ? name : "CUDA_ERROR_LAUNCH_FAILED",
                s_launch_faulted ? " the part faulted" : "");
        fclose(answers);
        return 1;
    }
    fprintf(answers, "answered");
    for (unsigned int place = 0u; place < s_case_count; place += 1u)
    {
        fprintf(answers, " %llx", ((unsigned long long)s_answers[place][1] << 32u) | s_answers[place][0]);
    }
    fprintf(answers, "\n");
    if (launches != 0u)
    {
        fprintf(answers, "timed %u %llu %llu %llu\n", launches, nanoseconds, s_timed_least, s_timed_most);
    }
    fclose(answers);
    return 0;
}

int main(int count, char **words)
{
    // `--dry` before the machine file runs every question as ever up to the driver, and none past it
    if ((count >= 2) && (strcmp(words[1], "--dry") == 0))
    {
        s_dry = 1;
        words[1] = words[0];
        words += 1;
        count -= 1;
    }
    // the facets come after --dry, each popping out one object of the carry to be read off the part; --full pops
    // them all. A dry run with no facet pops out the diff from its first question, as ever. --held <file> takes the
    // vendor's feedback, the encodings its disassembler held, and a matching slot is held before the part on a live run
    while ((count >= 2) && (strncmp(words[1], "--", 2u) == 0) && (strcmp(words[1], "--list") != 0))
    {
        if (strcmp(words[1], "--held") == 0)
        {
            if ((count < 3) || !cubin_safe_verdict_load(words[2]))
            {
                fprintf(stderr, "the held list %s did not read\n", (count < 3) ? "(none)" : words[2]);
                return 2;
            }
            words[2] = words[0];
            words += 2;
            count -= 2;
            continue;
        }
        if (strcmp(words[1], "--full") == 0)
        {
            s_facets = CUBIN_FACET_QUERY | CUBIN_FACET_GATE | CUBIN_FACET_VENDOR;
        }
        else if (strcmp(words[1], "--query") == 0)
        {
            s_facets |= CUBIN_FACET_QUERY;
        }
        else if (strcmp(words[1], "--gate-check") == 0)
        {
            s_facets |= CUBIN_FACET_GATE;
        }
        else if (strcmp(words[1], "--diff-output-against-vendor") == 0)
        {
            s_facets |= CUBIN_FACET_VENDOR;
        }
        else
        {
            break;
        }
        words[1] = words[0];
        words += 1;
        count -= 1;
    }
    const int listed = (count == 5) && (strcmp(words[3], "--list") == 0);
    if (!listed && (count != 8) && (count != 9) && (count != 11))
    {
        fprintf(stderr,
                "cubin_run <machine file> <layout> <code> <registers> <cases> <answers> <slot> [<launches> [<threads> "
                "<blocks>]]\n"
                "cubin_run <machine file> <layout> --list <list>\n"
                "cubin_run [--dry [--full|--query|--gate-check|--diff-output-against-vendor]...] [--held <file>] "
                "<machine file> ...\n");
        return 2;
    }
    char kernel[256];
    const unsigned long long pattern_size =
        cubin_pattern_read(words[2], s_pattern, sizeof(s_pattern), kernel, sizeof(kernel));
    if (!sass_machine_read(&s_machine, words[1]))
    {
        fprintf(stderr, "the machine file did not read\n");
        return 2;
    }
    // the gate reads every instruction of every question against the forms, and the forms' places are found once
    sass_encoding_places_hold(&s_machine);
    const CubinRunShape unshaped = {CUBIN_RUN_THREADS, 1u};
    if (!listed)
    {
        const unsigned int slot = (unsigned int)strtoul(words[7], NULL, 10);
        const unsigned int launches = (count >= 9) ? (unsigned int)strtoul(words[8], NULL, 10) : 0u;
        const CubinRunShape shape = {(count == 11) ? (unsigned int)strtoul(words[9], NULL, 10) : CUBIN_RUN_THREADS,
                                     (count == 11) ? (unsigned int)strtoul(words[10], NULL, 10) : 1u};
        const int carried =
            cubin_run_question(kernel, pattern_size, words[3], (unsigned int)strtoul(words[4], NULL, 10), words[5],
                               words[6], slot, launches, (count == 11) ? shape : unshaped, count == 11);
        return (carried == 2) ? 2 : 0;
    }
    // Each line of the list one question of no shape of its own, `<code> <registers> <cases> <answers> <slot>
    // [<launches>]`, timed over its launches where it gives a count that is not 0, all carried in this one process and
    // each answered to its own file as it is carried, each timed question's time its own. A launch the driver or the
    // part refuses leaves a context that answers no other question, and the questions after it are left unanswered, for
    // the channel to carry in a process of their own
    FILE *const list = fopen(words[4], "rb");
    if (list == NULL)
    {
        fprintf(stderr, "the list did not read\n");
        return 2;
    }
    char line[4u * CUBIN_RUN_LINE];
    int carried = 0;
    while ((carried == 0) && (fgets(line, sizeof(line), list) != NULL))
    {
        char code_path[CUBIN_RUN_LINE];
        char cases_path[CUBIN_RUN_LINE];
        char answers_path[CUBIN_RUN_LINE];
        unsigned int registers = 0u;
        unsigned int slot = 0u;
        unsigned int launches = 0u;
        if (sscanf(line, "%1023s %u %1023s %1023s %u %u", code_path, &registers, cases_path, answers_path, &slot,
                   &launches) < 5)
        {
            continue;
        }
        carried = cubin_run_question(kernel, pattern_size, code_path, registers, cases_path, answers_path, slot,
                                     launches, unshaped, 0);
    }
    fclose(list);
    return (carried == 2) ? 2 : 0;
}
