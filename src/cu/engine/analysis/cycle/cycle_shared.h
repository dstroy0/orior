// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CYCLE_SHARED_H
#define CYCLE_SHARED_H

// What the files of the record machine share: the headers, the checks, and the record that stores a program. The
// machine has these functional parts:
//
// - cycle_{sweep,launch}.cu: the key sweep.
// - cycle_record_*.cu: the record interpreter and the record calls.
// - cycle_prelude.cu: the prelude that a C lane opens with.
// - cycle_compile_*.cu: NVRTC, nvJitLink, the cache and a compiled program.
//
// A kernel stays in the file that launches it. The kernel of a compiled program belongs to its own text (program_unit)

#include "cycle.h"

// the CRC that seals a program's block, and the signum that names its program
#include "../../../../../includes/codecs/crc/crc.h"
#include "../../runtime/obsignatio/obsignatio.h"

// the code generator, which writes the lane of a program as PTX or C source for the target named here. Then the launch
// that it reads. Then its assembly printer, which the device runs to write the text of the lane again
#include "../../../engine/rmc/asm_printer.h"
#include "../../../engine/rmc/code_generator.h"
#include "../../../engine/rmc/codegen_device.h"

#include <cooperative_groups.h>
#include <cuda_runtime.h>
// the prototypes of NVRTC and nvJitLink only. The program loads both libraries at run time, and a build links nothing
// beyond them
#include <nvJitLink.h>
#include <nvrtc.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <process.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define CYCLE_BLOCK 256u

static_assert(sizeof(unsigned int) == 4u, "cycle: unsigned int must be 32 bits, a limb");
static_assert(sizeof(unsigned long long) == 8u, "cycle: unsigned long long must be 64 bits, a limb product");

static_assert(cudaSuccess == 0, "the engine reads a CUDA status of 0 as success");

// cudaError_t enumerates non-negative codes below INT_MAX; the status converts to int exactly
#define CYCLE_STATUS_CHECK(call_, evacaddr_, error_)                                                                   \
    engine_status_check((int)(call_), ENGINE_MODULE_CYCLE, (unsigned int)__LINE__, (const void *)(evacaddr_), (error_))

#define CYCLE_CHECK(condition_, evacaddr_, error_, type_)                                                              \
    engine_error_check((condition_), (type_), ENGINE_MODULE_CYCLE, (unsigned int)__LINE__, (const void *)(evacaddr_),  \
                       (error_))

// the fields of CycleRecord:
//
// - `compiled`: 1 where the program runs as its own kernel (below, the record program compiled), whose library the
//   cache of the process holds. It is 0 where the program runs on the interpreter.
// - `block`: the copy of the host of the EngineProgramBlock of the program. Its thread blocks write the copy at
//   `device_block`.
// - `hot`: what the thread blocks share on the device.
// - `registers` and `local_bytes`: the registers and the local frame of the compiled kernel for each thread, the grant
//   that it runs within.
// - `places`: the registers of a thread in shared memory. `threads` is the threads of a thread block.
// - `register_bytes`: the shared memory that the registers of a thread block take, the dynamic shared memory of the
//   launch. `shared_bytes` is all that a thread block uses, with the own memory of the kernel beside them.
// - `resident`: the thread blocks that the device fits at once.
// - `host_program`: the resident of a program that the compiler of the host built (CYCLE_RECORD_HOST_C=1), which runs
//   on the threads of the host. `host_grid` sets the threads that it runs on. Both are NULL where the program belongs
//   to the device.
// - `table_words`: the words of the tables of the program.
typedef void (*CycleHostEntry)(CycleCompiledLaunch launch);

typedef void (*CycleHostGrid)(unsigned int blocks);

struct CycleRecord
{
    unsigned int steps;
    unsigned int members;
    unsigned int file_limbs;
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX];
    unsigned int out_limbs;
    unsigned int divides;
    unsigned int compiled;
    cudaKernel_t kernel;
    DeviceRecordStep *device_steps;
    unsigned int *device_error;
    unsigned int *device_tables;
    EngineProgramBlock *block;
    EngineProgramBlock *device_block;
    struct CycleHot *hot;
    unsigned long long registers;
    unsigned long long local_bytes;
    unsigned int places;
    unsigned int threads;
    unsigned long long thread_bytes;
    unsigned long long register_bytes;
    unsigned long long shared_bytes;
    unsigned long long resident;
    unsigned long long processors;
    CycleHostEntry host_program;
    CycleHostGrid host_grid;
    unsigned long long table_words;
};

// a launch's time to live where CYCLE_RECORD_TTL names none, well inside Windows' 2 s watchdog
#define CYCLE_PROGRAM_TTL_MICROSECONDS 500000ULL

// the check-in the scheduler checks a program against, and how often each of its thread blocks checks in within it
#define CYCLE_PROGRAM_WDT_MICROSECONDS 100000ULL

#define CYCLE_PROGRAM_CHECKINS_PER_WDT 4ULL

// the stack a launch grew past `before` given back (cycle_launch.cu)
int cycle_stack_return(size_t before, const void *evacaddr, EngineError *error);

// 1 where the environment names `name` as 1, and the microseconds it names, else `otherwise`
// (cycle_compile_toolchain.cu)
int cycle_environment_set(const char *name);

unsigned long long cycle_environment_microseconds(const char *name, unsigned long long otherwise);

// the lane of a program, compiled and cached for `record`, or 0 where it stays on the interpreter. CYCLE_ERROR goes in
// `error` where the device wrote the lane apart from that of the host. Then the library of a cached kernel, given
// back (cycle_compile_route.cu)
int cycle_record_compile(const EngineRecordLayout *layout, CycleRecord *record, EngineError *error);

void cycle_program_release(cudaKernel_t kernel);

// a program's C source built by the host's compiler and loaded for `record`, 0 where it did not build; a cached host
// program given back. And a host program run resident on the host (cycle_compile.cu)
int cycle_host_program_load(const EngineRecordLayout *layout, CycleRecord *record, const char *source,
                            unsigned int places, unsigned long long written, int report);

void cycle_host_program_release(CycleHostEntry entry);

int cycle_host_resident(const CycleRecord *record, CycleCompiledLaunch program, EngineError *error);

// the prelude a C lane opens with, which names the launch and the lane's types (cycle_prelude.cu)
extern const char g_cycle_prelude[];

#endif
