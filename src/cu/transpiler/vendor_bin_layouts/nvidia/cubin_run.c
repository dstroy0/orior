// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_run.c: one question carried to the part: its code put in the container the system accepted, held to
// cubin_safe on the host, and run over its cases, a thread a case. This is the run channel's carrier for a part behind
// NVIDIA's driver (run_channel.h), and a question is carried in a process of its own, so a part that refuses one
// takes this process with it and nothing else.
//
//     cubin_run <machine file> <ksc> <code> <registers> <cases> <answers> [<launches> [<threads> <blocks>]]
//
// The code is the question's machine code, sixteen bytes an instruction. The .ksc's container rows hold the container
// (cubin_write.h), and the code, the registers and the exits are put in it. The cases are one a line, up to
// eight words in hex, the words a line leaves out zero, and the kernel is handed the stick's frame: in, eight words a
// thread, out, two words a thread, and the count of threads. It is launched in blocks of <threads>, <blocks> of them,
// blocks of 256 where the question names no shape, as many as give every case a thread, and every thread is given a
// case, thread t case t of the cases taken round: the cases are answered by the first threads, and the rest do the
// same work over again. One line is written to the answers:
//
//     answered <answer>...      each case's two words as one value in hex, in the order of the cases
//     skipped <verdict> <name>  cubin_safe held it off the part, and the driver never saw it
//     refused <error>           the driver or the part would not take it
//
// Given a count of launches, a question that answered is launched that many times more, and a second line gives the
// time they took together on the part's own timer, which the driver's events stamp (cubin_run_timed):
//
//     timed <launches> <nanoseconds>
//
// The driver is reached by name and by its own entry points, and nothing of its vendor's toolchain is compiled
// against. Exit 0 where a line was written, 2 where the files, the machine or the driver were not reached.
#include "cubin_safe.h"
#include "cubin_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// the most bytes a container and its code take, and the longest line of the cases
#define CUBIN_RUN_BYTES 262144u
#define CUBIN_RUN_LINE 1024u
// the words of a case and of an answer, the stick's frame, and the threads a block holds where the question names
// no shape of its own
#define CUBIN_RUN_IN_WORDS 8u
#define CUBIN_RUN_OUT_WORDS 2u
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
    CubinRunStatus (*event_elapsed)(float *, void *, void *);
    CubinRunStatus (*event_destroy)(void *);
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
    *(void **)&driver->event_elapsed = cubin_run_entry(library, "cuEventElapsedTime");
    *(void **)&driver->event_destroy = cubin_run_entry(library, "cuEventDestroy_v2");
    return (driver->init != NULL) && (driver->device_get != NULL) && (driver->context_retain != NULL) &&
           (driver->context_set != NULL) && (driver->module_load != NULL) && (driver->function_get != NULL) &&
           (driver->allocate != NULL) && (driver->copy_in != NULL) && (driver->clear != NULL) &&
           (driver->launch != NULL) && (driver->synchronize != NULL) && (driver->copy_out != NULL) &&
           (driver->error_name != NULL) && (driver->event_create != NULL) && (driver->event_record != NULL) &&
           (driver->event_synchronize != NULL) && (driver->event_elapsed != NULL) && (driver->event_destroy != NULL);
}

// `launches` launches of `function` over `arguments` timed together, between two events the part stamps from its own
// timer, the time in nanoseconds into `nanoseconds`: 0, or the status the driver gave. The timer is the part's and
// not the host's, and counts the same whatever clock the part's multiprocessors run at
static CubinRunStatus cubin_run_timed(const CubinRunDriver *driver, void *function, void **arguments,
                                      const CubinRunShape *shape, unsigned int launches,
                                      unsigned long long *nanoseconds)
{
    void *start = NULL;
    void *stop = NULL;
    float milliseconds = 0.0f;
    CubinRunStatus status = driver->event_create(&start, 0u);
    status = (status == 0) ? driver->event_create(&stop, 0u) : status;
    status = (status == 0) ? driver->event_record(start, NULL) : status;
    for (unsigned int launch = 0u; (status == 0) && (launch < launches); launch += 1u)
    {
        status = driver->launch(function, shape->blocks, 1u, 1u, shape->threads, 1u, 1u, 0u, NULL, arguments, NULL);
    }
    status = (status == 0) ? driver->event_record(stop, NULL) : status;
    status = (status == 0) ? driver->event_synchronize(stop) : status;
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

// The container in s_container loaded and its kernel launched as `shape` says, every thread given a case, thread t
// case t of the cases taken round, and the first threads' two words each into s_answers, one a case; where `launches`
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
    const size_t out_bytes = (size_t)count * sizeof(s_answers[0]);
    unsigned int(*const given)[CUBIN_RUN_IN_WORDS] = malloc(in_bytes);
    if (given == NULL)
    {
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
    status = (status == 0) ? driver->clear(out, 0u, out_bytes) : status;
    void *arguments[] = {&in, &out, &count};
    status = (status == 0) ? driver->launch(function, shape->blocks, 1u, 1u, shape->threads, 1u, 1u, 0u, NULL,
                                            arguments, NULL)
                           : status;
    status = (status == 0) ? driver->synchronize() : status;
    status = (status == 0) ? driver->copy_out(s_answers, out, (size_t)s_case_count * sizeof(s_answers[0])) : status;
    *nanoseconds = 0ull;
    status = ((status == 0) && (launches != 0u))
                 ? cubin_run_timed(driver, function, arguments, shape, launches, nanoseconds)
                 : status;
    return status;
}

int main(int count, char **words)
{
    if ((count != 7) && (count != 8) && (count != 10))
    {
        fprintf(stderr,
                "cubin_run <machine file> <ksc> <code> <registers> <cases> <answers> [<launches> [<threads> <blocks>]]\n");
        return 2;
    }
    const unsigned int launches = (count >= 8) ? (unsigned int)strtoul(words[7], NULL, 10) : 0u;
    CubinRunShape shape = {(count == 10) ? (unsigned int)strtoul(words[8], NULL, 10) : CUBIN_RUN_THREADS,
                           (count == 10) ? (unsigned int)strtoul(words[9], NULL, 10) : 1u};
    char kernel[256];
    const unsigned long long pattern_size = cubin_pattern_read(words[2], s_pattern, sizeof(s_pattern), kernel,
                                                               sizeof(kernel));
    const unsigned long long code_size = cubin_run_file(words[3], s_code, sizeof(s_code));
    FILE *const answers = fopen(words[6], "wb");
    if ((pattern_size == 0ull) || (code_size == 0ull) || !cubin_run_cases(words[5]) || (answers == NULL) ||
        !sass_machine_read(&s_machine, words[1]))
    {
        fprintf(stderr, "the container, the code, the cases, the answers or the machine file did not read\n");
        return 2;
    }
    // a question that names no shape is given blocks of CUBIN_RUN_THREADS, as many as give every case a thread. A
    // launch gives every case a thread, and gives a case each to no more threads than it holds them for
    shape.blocks = (count == 10) ? shape.blocks : ((s_case_count + CUBIN_RUN_THREADS - 1u) / CUBIN_RUN_THREADS);
    const unsigned long long threads = (unsigned long long)shape.threads * shape.blocks;
    if ((threads < s_case_count) || (threads > CUBIN_RUN_THREADS_MOST))
    {
        fprintf(answers, "refused a launch of %u threads in %u blocks for %u cases\n", shape.threads, shape.blocks,
                s_case_count);
        fclose(answers);
        return 0;
    }
    CubinWrite written;
    memset(&written, 0, sizeof(written));
    written.pattern = s_pattern;
    written.pattern_size = pattern_size;
    written.kernel = kernel;
    written.code = s_code;
    written.code_size = code_size;
    written.registers = (unsigned int)strtoul(words[4], NULL, 10);
    written.exit_count = cubin_exits_find(s_code, code_size, sass_exit_encoding(&s_machine), s_exits, CUBIN_RUN_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ull;
    if (!cubin_write(&written, s_container, sizeof(s_container), &size))
    {
        fprintf(answers, "refused not written\n");
        fclose(answers);
        return 0;
    }
    // read on the host before the driver sees it: code that breaks a rule of cubin_safe is answered without the part
    unsigned long long at = 0ull;
    const unsigned int verdict = cubin_safe_image(&s_machine, s_container, size, &at);
    if (verdict != CUBIN_SAFE)
    {
        fprintf(answers, "skipped %u %s at byte %llu\n", verdict, cubin_safe_name(verdict), at);
        fclose(answers);
        return 0;
    }
    CubinRunDriver driver;
    memset(&driver, 0, sizeof(driver));
    if (!cubin_run_driver(&driver))
    {
        fprintf(stderr, "the driver was not reached\n");
        fclose(answers);
        return 2;
    }
    unsigned long long nanoseconds = 0ull;
    const CubinRunStatus status = cubin_run_launch(&driver, kernel, &shape, launches, &nanoseconds);
    if (status != 0)
    {
        const char *name = NULL;
        driver.error_name(status, &name);
        fprintf(answers, "refused %s\n", (name != NULL) ? name : "unnamed");
        fclose(answers);
        return 0;
    }
    fprintf(answers, "answered");
    for (unsigned int place = 0u; place < s_case_count; place += 1u)
    {
        fprintf(answers, " %llx", ((unsigned long long)s_answers[place][1] << 32u) | s_answers[place][0]);
    }
    fprintf(answers, "\n");
    if (launches != 0u)
    {
        fprintf(answers, "timed %u %llu\n", launches, nanoseconds);
    }
    fclose(answers);
    return 0;
}
