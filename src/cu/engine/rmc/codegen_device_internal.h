// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the codegen_device_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef CODEGEN_DEVICE_INTERNAL_H
#define CODEGEN_DEVICE_INTERNAL_H

#include "../../engine/analysis/cycle/cycle.h"
#include "../../engine/analysis/key_schedule/key_schedule.h"
#include "../../engine/analysis/key_schedule/key_schedule_core.h"
#include "../../engine/analysis/keymath/keymath_core.h"
#include "codegen_device.h"

#include <stddef.h>

int layout_same(const EngineRecordLayout *left, const EngineRecordLayout *right);
#if (defined(__CUDACC__))
#include <cub/cub.cuh>
#include <cuda_runtime.h>

// the threads a block of the code generator's kernels runs
#define CODEGEN_THREADS 256u

// the max a scan or a selection counts, as cub takes its counts
#define CODEGEN_COUNT_MAX 0x7FFFFFFFULL

// What the steps leave for the lane's own forms, a word each. They are the max temporaries, 64-bit temporaries and
// predicates that any step took. A flag is 1 where a step reads the tables, a form breaks the lane, or a step is one
// the lane does not use. Last, what the last step left taken in each bank, where the lane's own
// forms go on from
enum CodegenSummary
{
    CODEGEN_TEMPS_MAX = 0,
    CODEGEN_WIDES_MAX = 1,
    CODEGEN_PREDICATES_MAX = 2,
    CODEGEN_TABLES = 3,
    CODEGEN_BROKEN = 4,
    CODEGEN_STEP_FAILED = 5,
    CODEGEN_TEMPS = 6,
    CODEGEN_WIDES = 7,
    CODEGEN_PREDICATES = 8,
    CODEGEN_SUMMARY = 9
};

// The lane's parts in the text's order: the note, the header, the lane's opening and declarations, and the body's
// opening. Then the body (its opening, the steps and its close), and the lane's end
enum CodegenPart
{
    CODEGEN_NOTE = 0,
    CODEGEN_HEADER = 1,
    CODEGEN_LANE_OPEN = 2,
    CODEGEN_DECLARATIONS = 3,
    CODEGEN_BODY_OPEN = 4,
    CODEGEN_OPENED = 5,
    CODEGEN_STEPPED = 6,
    CODEGEN_CLOSED = 7,
    CODEGEN_ENDING = 8,
    CODEGEN_PARTS = 9
};

// where each part's items begin among the lane's, and how many it contains
struct CodegenParts
{
    unsigned long long at[CODEGEN_PARTS];
    unsigned long long count[CODEGEN_PARTS];
};

// a byte the assembly printer wrote that is not 0, which the text contains
struct NonzeroByte
{
    __host__ __device__ bool operator()(const unsigned char &byte) const
    {
        return byte != 0u;
    }
};

// the thread's place across the grid
__device__ static inline unsigned long long codegen_device_thread(void)
{
    return ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
}

__global__ void codegen_device_fill(unsigned int *to, unsigned long long count, unsigned int value);

__global__ void codegen_device_facts(IrProgram program, unsigned int *put_first, unsigned int *put_last,
                                     unsigned int *atom_reader, unsigned int *loops);

__global__ void codegen_device_unlaid(unsigned int *put_first, const unsigned int *put_last, unsigned int words);

__global__ void codegen_device_count(IrProgram program, const unsigned int *loop_first, unsigned long long *counts,
                                     unsigned int *errors, unsigned int *summary);

__global__ void codegen_device_write(IrProgram program, const unsigned int *loop_first,
                                     const unsigned long long *counts, const unsigned long long *item_first,
                                     MachineInstr *items);

__global__ void codegen_device_left(const unsigned int *summary, unsigned int loops, MachineFunction *lane_out);

__global__ void codegen_prologue_epilogue(IrProgram program, const unsigned int *errors, unsigned int *summary,
                                          MachineFunction *lane_out, unsigned int atoms, unsigned int places,
                                          unsigned int first, unsigned int last, int scheduled, unsigned int states,
                                          CodegenParts *parts, MachineInstr *items);

// the result of a schedule: the forms it took, the states, the max cost one state chains and the forms alone over the
// budget
struct ScheduleEnd
{
    unsigned long long count;
    unsigned int states;
    unsigned int max;
    unsigned int over;
};

__global__ void codegen_schedule(IrProgram program, unsigned int *summary, MachineFunction *lane_out, Schedule schedule,
                                 unsigned int writes, unsigned int ports, const MachineInstr *body,
                                 unsigned long long body_count, unsigned long long capacity, MachineInstr *items,
                                 ScheduleEnd *ended);

__global__ void codegen_device_record_counts(AsmPrinterLists forms, const MachineInstr *items,
                                             unsigned long long item_count, unsigned long long *record_counts,
                                             unsigned int *summary);

__global__ void codegen_device_records(AsmPrinterLists forms, const MachineInstr *items, unsigned long long item_count,
                                       const unsigned long long *record_first, unsigned int *records,
                                       unsigned long long *record_lanes);

__global__ void codegen_device_index(unsigned int *records, unsigned long long record_count,
                                     const unsigned long long *lane_first, const unsigned long long *record_lanes,
                                     unsigned int *index);

__global__ void codegen_device_bytes(const unsigned int *out, unsigned long long lanes, unsigned int out_limbs,
                                     unsigned int offset, unsigned char *bytes);

// How keymath's encode or key_schedule's layout ended on the device: 1 in `ok` where it finished, else its end and
// where. From the layout, also the file's limbs and the record's bits
struct LayoutEnd
{
    int ok;
    unsigned int end;
    unsigned int at;
    unsigned long long file_limbs;
    unsigned long long out_bits;
};

__global__ void codegen_device_encode(KeymathCoreEncode encoding, LayoutEnd *ended);

__global__ void codegen_layout(KeyScheduleCoreLayout layout, LayoutEnd *ended);

// The device memory a lane's writing takes, freed together, and 0 once a call the device errored left it unusable.
// The list of what it took begins at its first take
struct DeviceArena
{
    PlatformArray allocations;
    int ok;
};

template <typename Element> static Element *codegen_device_take(DeviceArena *memory, unsigned long long count)
{
    void *taken = NULL;
    const unsigned long long bytes = ((count == 0ULL) ? 1ULL : count) * sizeof(Element);
    if (memory->allocations.element == 0ULL)
    {
        platform_array_begun(&memory->allocations, sizeof(void *), platform_place_g_program_owner,
                             PLATFORM_TIER_TEMPORARY);
    }
    if ((memory->ok != 0) && (cudaMalloc(&taken, (size_t)bytes) == cudaSuccess))
    {
        if (platform_array_appended(&memory->allocations, &taken) != 0)
        {
            return (Element *)taken;
        }
        cudaFree(taken);
    }
    memory->ok = 0;
    return NULL;
}

template <typename Element>
static Element *codegen_device_copy(DeviceArena *memory, const Element *from, unsigned long long count)
{
    Element *const to = codegen_device_take<Element>(memory, count);
    if ((memory->ok != 0) && (count != 0ULL))
    {
        memory->ok = cudaMemcpy(to, from, (size_t)(count * sizeof(Element)), cudaMemcpyHostToDevice) == cudaSuccess;
    }
    return to;
}

template <typename Element>
static void codegen_device_read(DeviceArena *memory, Element *to, const Element *from, unsigned long long count)
{
    if ((memory->ok != 0) && (count != 0ULL))
    {
        memory->ok = cudaMemcpy(to, from, (size_t)(count * sizeof(Element)), cudaMemcpyDeviceToHost) == cudaSuccess;
    }
}

void codegen_device_release(DeviceArena *memory);

unsigned int codegen_device_blocks(unsigned long long count);

void codegen_device_launched(DeviceArena *memory);

// the exclusive running sum of `count` counts into `sums`, and their total; 0 where there are none
template <typename Counted>
static unsigned long long codegen_device_scan(DeviceArena *memory, const Counted *counts, Counted *sums,
                                              unsigned long long count)
{
    if ((memory->ok == 0) || (count == 0ULL))
    {
        return 0ULL;
    }
    size_t bytes = 0u;
    // bounded below CODEGEN_COUNT_MAX by the callers
    memory->ok = cub::DeviceScan::ExclusiveSum(NULL, bytes, counts, sums, (int)count) == cudaSuccess;
    void *const temporary = codegen_device_take<unsigned char>(memory, bytes);
    memory->ok =
        (memory->ok != 0) && (cub::DeviceScan::ExclusiveSum(temporary, bytes, counts, sums, (int)count) == cudaSuccess);
    Counted last_count = 0;
    Counted last_sum = 0;
    codegen_device_read(memory, &last_count, &counts[count - 1ULL], 1ULL);
    codegen_device_read(memory, &last_sum, &sums[count - 1ULL], 1ULL);
    return (unsigned long long)last_count + (unsigned long long)last_sum;
}

int asm_printer_program_device(const AsmPrinterProgram *program, EngineRecordLayout *text_layout, VerbaLinea *error);

int asm_printer_program_placed(const AsmPrinterRuleset *text_rules, EngineRecordLayout *text_layout, VerbaLinea *error);

int codegen_device_decide(DeviceArena *device_arena, const EngineRecordLayout *layout,
                          const DeviceRecordStep *device_steps, const unsigned int *scratch,
                          unsigned long long scratch_count, unsigned int places, const ScheduleCosts *costs,
                          ScheduleReport *report, MachineInstr **text_items, unsigned long long *item_count,
                          VerbaLinea *error);
#endif

int layout_device(const LayoutRequest *request, EngineRecordLayout *layout, VerbaLinea *error);

#endif
