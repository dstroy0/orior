// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_run.c: one question carried to the part: its code put in the container the system accepted, held to
// cubin_safe on the host, and run over its cases, a thread a case. This is the run channel's carrier for a part behind
// NVIDIA's driver (run_channel.h), and a question is carried in a process of its own, so a part that refuses one
// takes this process with it and nothing else.
//
//     cubin_run <machine file> <ksc> <code> <registers> <cases> <answers>
//
// The code is the question's machine code, sixteen bytes an instruction. The .ksc's container rows hold the container
// (cubin_write.h), and the code, the registers and the exits are put in it. The cases are one a line, up to
// eight words in hex, the words a line leaves out zero, and the kernel is handed the stick's frame: in, eight words a
// thread, out, two words a thread, and the count of cases. One line is written to the answers:
//
//     answered <answer>...      each case's two words as one value in hex, in the order of the cases
//     skipped <verdict> <name>  cubin_safe held it off the part, and the driver never saw it
//     refused <error>           the driver or the part would not take it
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
// the words of a case and of an answer, the stick's frame, and the threads a launch gives the kernel, which leaves
// every thread past the case count at once
#define CUBIN_RUN_IN_WORDS 8u
#define CUBIN_RUN_OUT_WORDS 2u
#define CUBIN_RUN_THREADS 256u
// the exits a question's code holds at most
#define CUBIN_RUN_EXITS 256u

// the driver's own types as its entry points take them: a status, handles, and a device address
typedef int CubinRunStatus;
typedef unsigned long long CubinRunAddress;

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
} CubinRunDriver;

static SassMachine s_machine;
static unsigned char s_pattern[CUBIN_RUN_BYTES];
static unsigned char s_code[CUBIN_RUN_BYTES];
static unsigned char s_container[CUBIN_RUN_BYTES];
static unsigned int s_exits[CUBIN_RUN_EXITS];
static unsigned int s_cases[CUBIN_RUN_THREADS][CUBIN_RUN_IN_WORDS];
static unsigned int s_case_count;
static unsigned int s_answers[CUBIN_RUN_THREADS][CUBIN_RUN_OUT_WORDS];

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
        fits = (count < CUBIN_RUN_THREADS);
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
    return (driver->init != NULL) && (driver->device_get != NULL) && (driver->context_retain != NULL) &&
           (driver->context_set != NULL) && (driver->module_load != NULL) && (driver->function_get != NULL) &&
           (driver->allocate != NULL) && (driver->copy_in != NULL) && (driver->clear != NULL) &&
           (driver->launch != NULL) && (driver->synchronize != NULL) && (driver->copy_out != NULL) &&
           (driver->error_name != NULL);
}

// the container in s_container loaded and its kernel run over every case, each case's two words into s_answers: 0, or
// the status the driver gave
static CubinRunStatus cubin_run_launch(const CubinRunDriver *driver, const char *kernel)
{
    int device = 0;
    void *context = NULL;
    void *module = NULL;
    void *function = NULL;
    CubinRunAddress in = 0ull;
    CubinRunAddress out = 0ull;
    unsigned int count = s_case_count;
    const size_t in_bytes = (size_t)s_case_count * sizeof(s_cases[0]);
    const size_t out_bytes = (size_t)s_case_count * sizeof(s_answers[0]);
    CubinRunStatus status = driver->init(0u);
    status = (status == 0) ? driver->device_get(&device, 0) : status;
    status = (status == 0) ? driver->context_retain(&context, device) : status;
    status = (status == 0) ? driver->context_set(context) : status;
    status = (status == 0) ? driver->module_load(&module, s_container) : status;
    status = (status == 0) ? driver->function_get(&function, module, kernel) : status;
    status = (status == 0) ? driver->allocate(&in, in_bytes) : status;
    status = (status == 0) ? driver->allocate(&out, out_bytes) : status;
    status = (status == 0) ? driver->copy_in(in, s_cases, in_bytes) : status;
    status = (status == 0) ? driver->clear(out, 0u, out_bytes) : status;
    void *arguments[] = {&in, &out, &count};
    status = (status == 0) ? driver->launch(function, 1u, 1u, 1u, CUBIN_RUN_THREADS, 1u, 1u, 0u, NULL, arguments, NULL)
                           : status;
    status = (status == 0) ? driver->synchronize() : status;
    status = (status == 0) ? driver->copy_out(s_answers, out, out_bytes) : status;
    return status;
}

int main(int count, char **words)
{
    if (count != 7)
    {
        fprintf(stderr, "cubin_run <machine file> <ksc> <code> <registers> <cases> <answers>\n");
        return 2;
    }
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
    const CubinRunStatus status = cubin_run_launch(&driver, kernel);
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
    fclose(answers);
    return 0;
}
