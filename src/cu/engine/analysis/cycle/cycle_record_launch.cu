// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_record_launch.cu: the resident program, the launch, the check against the interpreter, the latch
#include "cycle_record_internal.h"

#include "../../engine_note.h"
#include "memoria_operor/memoria_linea.h"

// the owner that the cells of a launch count to
#define CYCLE_LAUNCH_PLACE ((unsigned int)platform_place_g_engine_owner)

// The compiled program run resident. The call arranges its block for this run and sends it to the device. It then
// launches the program, and launches it again from where its block records it, until every lane ends. The host reads
// the block back only between launches, when the seal of the block holds.
//
// Three faults error on the run. One is a device block whose seal does not match, written while no launch referenced
// it. The others are a launch that failed, and a launch that left the block anywhere but yielded or done. The copy of the host then stays
// sealed at the fault, with the module and the site of the error.
//
// The program runs on as many thread blocks as the device fits at once, or fewer where the lanes need fewer. A thread
// block takes the threads that the shared memory of its registers fits. Where the lanes are few, it takes their share
// of the processors of the device in whole warps. A launch of a few thousand lanes then reaches every processor, and not
// only a few thread blocks.
static int cycle_record_resident(const CycleRecord *record, CycleCompiledLaunch program, EngineError *error)
{
    const unsigned long long share = (program.count + record->processors - 1ULL) / record->processors;
    const unsigned long long warps = ((share + 31ULL) / 32ULL) * 32ULL;
    // at max record->threads, itself at max CYCLE_BLOCK
    const unsigned int threads = (unsigned int)((warps < record->threads) ? warps : record->threads);
    const unsigned long long needed = (program.count + threads - 1ULL) / threads;
    // the device's thread blocks at once are a small count, far under 2^31
    const unsigned int blocks = (unsigned int)((needed < record->resident) ? needed : record->resident);
    const unsigned long long register_bytes = threads * record->thread_bytes;
    EngineProgramBlock *const block = record->block;
    const unsigned long long ttl =
        1000ULL * cycle_environment_microseconds("CYCLE_RECORD_TTL", CYCLE_PROGRAM_TTL_MICROSECONDS);
    const unsigned long long wdt = 1000ULL * CYCLE_PROGRAM_WDT_MICROSECONDS;
    const EngineSignum signature = block->signature;
    const unsigned long long generation = block->generation + 1ULL;
    mmgr_bytes_set(block, 0u, sizeof(EngineProgramBlock));
    block->signature = signature;
    block->generation = generation;
    block->command = ENGINE_PROGRAM_RUN;
    block->grant_registers = record->registers;
    block->grant_threads = (unsigned long long)blocks * threads;
    block->grant_bytes = record->local_bytes * block->grant_threads;
    // the kernel's own shared memory, and its threads' registers at this launch's threads
    block->grant_shared = (record->shared_bytes - record->register_bytes) + register_bytes;
    block->state = ENGINE_PROGRAM_PLACED;
    block->span = record->file_limbs;
    block->ttl = ttl;
    block->wdt = wdt;
    block->lanes = program.count;
    // the block keeps a device address as a 64-bit word, as it keeps every word
    block->result = (unsigned long long)(uintptr_t)program.out;
    block->result_words = program.count * record->out_limbs;
    block->checksum = cycle_block_seal(block);
    program.hot = record->hot;
    // the block is 64-bit words throughout, and the program reads it as them at its device address
    program.block = (unsigned long long *)record->device_block;
    program.ttl = ttl;
    program.checkin_every = wdt / CYCLE_PROGRAM_CHECKINS_PER_WDT;
    program.places = record->places;
    EngineProgramBlock *const device_block = record->device_block;
    int ok = CYCLE_STATUS_CHECK(cudaMemcpy(device_block, block, sizeof(EngineProgramBlock), cudaMemcpyHostToDevice),
                                device_block, error) &&
             CYCLE_STATUS_CHECK(cudaMemset(record->hot, 0, sizeof(CycleHot)), record->hot, error);
    int running = ok;
    while (running != 0)
    {
        ok = CYCLE_STATUS_CHECK(cudaMemcpy(block, device_block, sizeof(EngineProgramBlock), cudaMemcpyDeviceToHost),
                                device_block, error) &&
             CYCLE_CHECK(block->checksum == cycle_block_seal(block), block, error, ENGINE_ERROR_LOGIC);
        if (ok == 0)
        {
            break;
        }
        block->launches += 1ULL;
        block->owner = block->launches;
        block->state = ENGINE_PROGRAM_RUNNING;
        program.launch_number = block->launches;
        ok = CYCLE_STATUS_CHECK(cudaMemcpy(device_block, block, sizeof(EngineProgramBlock), cudaMemcpyHostToDevice),
                                device_block, error);
        // a launch's start and its thread blocks gone begin at 0, and the next lane runs on from the last launch's
        ok = ok && CYCLE_STATUS_CHECK(cudaMemset(&record->hot->launch_start, 0, 2u * sizeof(unsigned long long)),
                                      record->hot, error);
        void *arguments[1] = {&program};
        // a library's kernel handle is what the runtime launches in place of a kernel's address
        ok = ok && CYCLE_STATUS_CHECK(cudaLaunchKernel((const void *)record->kernel, dim3(blocks), dim3(threads),
                                                       arguments, (size_t)register_bytes, 0),
                                      program.out, error);
        ok = ok && CYCLE_STATUS_CHECK(cudaDeviceSynchronize(), program.out, error);
        unsigned int error_count = 0u;
        ok = ok &&
             CYCLE_STATUS_CHECK(
                 cudaMemcpy(&error_count, record->device_error, sizeof(unsigned int), cudaMemcpyDeviceToHost),
                 record->device_error, error) &&
             CYCLE_STATUS_CHECK(cudaMemcpy(block, device_block, sizeof(EngineProgramBlock), cudaMemcpyDeviceToHost),
                                device_block, error);
        const unsigned long long state = block->state;
        ok = ok && CYCLE_CHECK((state == ENGINE_PROGRAM_YIELDED) || (state == ENGINE_PROGRAM_DONE), block, error,
                               ENGINE_ERROR_LOGIC);
        block->error = error_count;
        block->runtime += block->exectime;
        if (ok == 0)
        {
            block->state = ENGINE_PROGRAM_FAULT;
            // an engine module and a site are small non-negative counts, which fit in a 64-bit word
            block->error_module = (unsigned long long)error->module;
            block->error_site = (unsigned long long)error->site;
        }
        block->checksum = cycle_block_seal(block);
        // the sealed block goes back to the device, where the next launch finds it. A fault stays sealed on the copy of
        // the host, whether the device can still take it or not
        const cudaError_t sent = cudaMemcpy(device_block, block, sizeof(EngineProgramBlock), cudaMemcpyHostToDevice);
        ok = ok && CYCLE_STATUS_CHECK(sent, device_block, error);
        running = (ok != 0) && (state == ENGINE_PROGRAM_YIELDED);
    }
    return ok;
}

// one launch of a record program, compiled or on the interpreter, run to its end. A compiled program runs resident, and
// the call launches it again until it ends. With `nanoseconds` the call times it in whole nanoseconds on the monotonic
// clock of the host. The time runs from before the launch to the synchronization of the device after it
static int cycle_record_launch(const CycleRecord *record, const CycleRecordLaunch &launch, unsigned int blocks,
                               int compiled, unsigned long long *nanoseconds, EngineError *error)
{
    int ok = 1;
    const unsigned long long began = platform_clock_ns();
    if ((ok != 0) && (compiled != 0))
    {
        CycleCompiledLaunch program;
        mmgr_bytes_set(&program, 0u, sizeof(program));
        for (unsigned int member = 0u; member < ENGINE_RECORD_MEMBERS_MAX; member += 1u)
        {
            program.in[member] = launch.in[member];
            program.bodies[member] = launch.bodies[member];
        }
        program.index = launch.index;
        program.tables = launch.tables;
        program.out = launch.out;
        program.error = launch.error;
        program.count = launch.count;
        ok = (record->host_program != NULL) ? cycle_host_resident(record, program, error)
                                            : cycle_record_resident(record, program, error);
    }
    else if ((ok != 0) && (record->file_limbs <= 64u) && (record->divides != 0u))
    {
        cycle_record_kernel<64u, 1u><<<blocks, CYCLE_BLOCK>>>(launch);
    }
    else if ((ok != 0) && (record->file_limbs <= 64u))
    {
        cycle_record_kernel<64u, 0u><<<blocks, CYCLE_BLOCK>>>(launch);
    }
    else if ((ok != 0) && (record->divides != 0u))
    {
        cycle_record_kernel<ENGINE_RECORD_LIMBS_MAX, 1u><<<blocks, CYCLE_BLOCK>>>(launch);
    }
    else if (ok != 0)
    {
        cycle_record_kernel<ENGINE_RECORD_LIMBS_MAX, 0u><<<blocks, CYCLE_BLOCK>>>(launch);
    }
    // the call reads and resets the own error of the launch, whichever way it launched
    const cudaError_t launched = cudaGetLastError();
    ok = ok && CYCLE_STATUS_CHECK(launched, launch.out, error);
    ok = ok && CYCLE_STATUS_CHECK(cudaDeviceSynchronize(), launch.out, error);
    if ((ok != 0) && (nanoseconds != NULL))
    {
        // the clock is monotonic, and so its span is never negative
        *nanoseconds = platform_clock_ns() - began;
    }
    return ok;
}

// CYCLE_RECORD_CHECK: the compiled program ran into the records of the request. The interpreter runs the same lanes into
// records of its own. The records and the errors of both runs must be the same word for word. The call puts the errors
// of the compiled run back for the run to read
static int cycle_record_check(const CycleRecord *record, CycleRecordLaunch launch, unsigned int blocks,
                              unsigned long long compiled_nanoseconds, int report, EngineError *error)
{
    const size_t words = (size_t)launch.count * record->out_limbs;
    // the records of both runs, ephemeral work that goes back before the check returns
    unsigned int *const compiled_records =
        (unsigned int *)platform_cell_taken((unsigned long long)words * sizeof(unsigned int), CYCLE_LAUNCH_PLACE, 1);
    unsigned int *const interpreted_records =
        (unsigned int *)platform_cell_taken((unsigned long long)words * sizeof(unsigned int), CYCLE_LAUNCH_PLACE, 1);
    unsigned int compiled_error = 0u;
    unsigned int interpreted_error = 0u;
    unsigned int *interpreted = NULL;
    unsigned long long interpreted_nanoseconds = 0ULL;
    int ok = CYCLE_CHECK((compiled_records != NULL) && (interpreted_records != NULL), record, error,
                         ENGINE_ERROR_RESOURCE) &&
             CYCLE_STATUS_CHECK(
                 cudaMemcpy(&compiled_error, record->device_error, sizeof(unsigned int), cudaMemcpyDeviceToHost),
                 record->device_error, error) &&
             CYCLE_STATUS_CHECK(
                 cudaMemcpy(compiled_records, launch.out, words * sizeof(unsigned int), cudaMemcpyDeviceToHost),
                 launch.out, error) &&
             CYCLE_STATUS_CHECK(cudaMalloc((void **)&interpreted, words * sizeof(unsigned int)), &interpreted, error) &&
             CYCLE_STATUS_CHECK(cudaMemset(record->device_error, 0, sizeof(unsigned int)), record->device_error, error);
    launch.out = interpreted;
    ok = ok && cycle_record_launch(record, launch, blocks, 0, (report != 0) ? &interpreted_nanoseconds : NULL, error);
    ok = ok &&
         CYCLE_STATUS_CHECK(
             cudaMemcpy(&interpreted_error, record->device_error, sizeof(unsigned int), cudaMemcpyDeviceToHost),
             record->device_error, error) &&
         CYCLE_STATUS_CHECK(
             cudaMemcpy(interpreted_records, interpreted, words * sizeof(unsigned int), cudaMemcpyDeviceToHost),
             interpreted, error) &&
         CYCLE_STATUS_CHECK(
             cudaMemcpy(record->device_error, &compiled_error, sizeof(unsigned int), cudaMemcpyHostToDevice),
             record->device_error, error);
    cudaFree(interpreted);
    size_t differs = words;
    for (size_t at = 0u; (ok != 0) && (differs == words) && (at < words); at += 1u)
    {
        differs = (compiled_records[at] != interpreted_records[at]) ? at : words;
    }
    const int same = (differs == words) && (compiled_error == interpreted_error);
    if ((ok != 0) && (report != 0))
    {
        EngineNote note;
        engine_note_begun(&note);
        mmgr_line_put(&note.line, "  cycle: ");
        mmgr_line_decimal(&note.line, launch.count, 1u);
        mmgr_line_put(&note.line, " lanes, compiled ");
        mmgr_line_decimal(&note.line, compiled_nanoseconds, 1u);
        mmgr_line_put(&note.line, " ns, interpreted ");
        mmgr_line_decimal(&note.line, interpreted_nanoseconds, 1u);
        mmgr_line_put(&note.line, " ns, errored ");
        mmgr_line_decimal(&note.line, compiled_error, 1u);
        mmgr_line_put(&note.line, " and ");
        mmgr_line_decimal(&note.line, interpreted_error, 1u);
        mmgr_line_put(&note.line, (same != 0) ? ", the same records\n" : ", records differ\n");
        engine_note_written(&note);
    }
    if ((ok != 0) && (same == 0) && (differs != words))
    {
        EngineNote note;
        engine_note_begun(&note);
        mmgr_line_put(&note.line, "  cycle: the compiled record program differs from the interpreter at lane ");
        mmgr_line_decimal(&note.line, differs / record->out_limbs, 1u);
        mmgr_line_put(&note.line, ", word ");
        mmgr_line_decimal(&note.line, differs % record->out_limbs, 1u);
        mmgr_line_put(&note.line, ": ");
        mmgr_line_hex(&note.line, compiled_records[differs], 8u);
        mmgr_line_put(&note.line, " against ");
        mmgr_line_hex(&note.line, interpreted_records[differs], 8u);
        mmgr_line_ch(&note.line, '\n');
        engine_note_written(&note);
    }
    platform_cell_released(interpreted_records, CYCLE_LAUNCH_PLACE);
    platform_cell_released(compiled_records, CYCLE_LAUNCH_PLACE);
    return ok && CYCLE_CHECK(same != 0, record->device_error, error, ENGINE_ERROR_LOGIC);
}

extern "C" long cycle_record_run(const CycleRecordRunRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return CYCLE_ERROR;
    }
    EngineError *const error = request->error;
    if (!CYCLE_CHECK((request->record != NULL) && (request->device_out != NULL) && (request->count != 0ULL), request,
                     error, ENGINE_ERROR_REQUEST))
    {
        return CYCLE_ERROR;
    }
    const CycleRecord *const record = request->record;
    CycleRecordLaunch launch;
    mmgr_bytes_set(&launch, 0u, sizeof(launch));
    for (unsigned int member = 0u; member < record->members; member += 1u)
    {
        // with no index, lane i reads record i of a member, or its one record where it has one
        if (!CYCLE_CHECK((request->device_in[member] != NULL) && (request->bodies[member] != 0ULL) &&
                             ((request->device_index != NULL) || (request->count <= request->bodies[member]) ||
                              (request->bodies[member] == 1ULL)),
                         &request->device_in[member], error, ENGINE_ERROR_REQUEST))
        {
            return CYCLE_ERROR;
        }
        launch.in[member] = request->device_in[member];
        launch.bodies[member] = request->bodies[member];
        launch.in_limbs[member] = record->in_limbs[member];
    }
    launch.steps = record->device_steps;
    launch.index = request->device_index;
    launch.tables = record->device_tables;
    launch.out = request->device_out;
    launch.error = record->device_error;
    launch.count = request->count;
    launch.step_count = record->steps;
    launch.members = record->members;
    launch.out_limbs = record->out_limbs;
    const unsigned long long needed = (request->count + CYCLE_BLOCK - 1ULL) / CYCLE_BLOCK;
    const unsigned int blocks = (unsigned int)((needed < CYCLE_RECORD_BLOCKS_MAX) ? needed : CYCLE_RECORD_BLOCKS_MAX);
    const int compiled = (record->compiled != 0u) ? 1 : 0;
    const int report = cycle_environment_set("CYCLE_RECORD_REPORT");
    const int check = (compiled != 0) && (cycle_environment_set("CYCLE_RECORD_CHECK") != 0);
    unsigned int error_count = 1u;
    size_t stack = 0u;
    unsigned long long nanoseconds = 0ULL;
    int ok = CYCLE_STATUS_CHECK(cudaDeviceGetLimit(&stack, cudaLimitStackSize), &stack, error) &&
             CYCLE_STATUS_CHECK(cudaMemset(record->device_error, 0, sizeof(unsigned int)), record->device_error, error);
    ok = ok && cycle_record_launch(record, launch, blocks, compiled, (report != 0) ? &nanoseconds : NULL, error);
    ok = ok && ((check == 0) || cycle_record_check(record, launch, blocks, nanoseconds, report, error));
    if ((ok != 0) && (report != 0) && (check == 0))
    {
        EngineNote note;
        engine_note_begun(&note);
        mmgr_line_put(&note.line, "  cycle: ");
        mmgr_line_decimal(&note.line, request->count, 1u);
        mmgr_line_put(&note.line, (compiled != 0) ? " lanes compiled in " : " lanes interpreted in ");
        mmgr_line_decimal(&note.line, nanoseconds, 1u);
        mmgr_line_put(&note.line, " ns\n");
        engine_note_written(&note);
    }
    if ((ok != 0) && (report != 0) && (compiled != 0))
    {
        EngineNote note;
        engine_note_begun(&note);
        mmgr_line_put(&note.line, "  cycle: the program ran ");
        mmgr_line_decimal(&note.line, record->block->launches, 1u);
        mmgr_line_put(&note.line, " launches, ");
        mmgr_line_decimal(&note.line, record->block->checkin, 1u);
        mmgr_line_put(&note.line, " check-ins, ");
        mmgr_line_decimal(&note.line, record->block->runtime, 1u);
        mmgr_line_put(&note.line, " ns on the device, ");
        mmgr_line_decimal(&note.line, record->block->grant_threads, 1u);
        mmgr_line_put(&note.line, " threads\n");
        engine_note_written(&note);
    }
    // the reservation of the frame goes back whether the sweep succeeded or not
    const int returned = cycle_stack_return(stack, request->device_out, error);
    ok =
        ok && returned &&
        CYCLE_STATUS_CHECK(cudaMemcpy(&error_count, record->device_error, sizeof(unsigned int), cudaMemcpyDeviceToHost),
                           record->device_error, error) &&
        CYCLE_CHECK(error_count == 0u, record->device_error, error, ENGINE_ERROR_REQUEST);
    return (ok != 0) ? (long)request->count : CYCLE_ERROR;
}

// 1 where a record's `bits` bits at `offset` are not all zero
__device__ static int cycle_latch_valid(const unsigned int *record, unsigned int offset, unsigned int bits)
{
    unsigned int set_bits = 0u;
    for (unsigned int bit = offset; bit < (offset + bits);)
    {
        const unsigned int shift = bit % 32u;
        const unsigned int left = offset + bits - bit;
        const unsigned int taken = (left < (32u - shift)) ? left : (32u - shift);
        const unsigned int mask = (taken == 32u) ? 0xFFFFFFFFu : (((1u << taken) - 1u) << shift);
        set_bits |= record[bit / 32u] & mask;
        bit += taken;
    }
    return (set_bits != 0u) ? 1 : 0;
}

// the latch over the records. Each thread scans its lanes from its lowest and stops at the first whose output is true.
// Each warp takes the least of its threads by a tree of shuffles. Each warp then sends its least to one atomic minimum
// over the device. Every thread of a warp reaches the shuffles, since the block is whole warps
__global__ static void cycle_latch_kernel(const unsigned int *records, unsigned long long count, unsigned int out_limbs,
                                          unsigned int offset, unsigned int bits, unsigned long long *first)
{
    const unsigned long long stride = (unsigned long long)gridDim.x * blockDim.x;
    unsigned long long found = CYCLE_LATCH_NONE;
    for (unsigned long long lane = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
         (found == CYCLE_LATCH_NONE) && (lane < count); lane += stride)
    {
        found = (cycle_latch_valid(&records[lane * out_limbs], offset, bits) != 0) ? lane : CYCLE_LATCH_NONE;
    }
    for (unsigned int range = 16u; range > 0u; range /= 2u)
    {
        const unsigned long long other = __shfl_down_sync(0xFFFFFFFFu, found, range);
        found = (other < found) ? other : found;
    }
    if (((threadIdx.x % 32u) == 0u) && (found != CYCLE_LATCH_NONE))
    {
        atomicMin(first, found);
    }
}

extern "C" long cycle_record_latch(const CycleRecordLatchRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return CYCLE_ERROR;
    }
    EngineError *const error = request->error;
    if (!CYCLE_CHECK((request->records != NULL) && (request->first != NULL) && (request->count != 0ULL) &&
                         (request->out_limbs != 0u) && (request->bits != 0u) &&
                         (((unsigned long long)request->offset + request->bits) <=
                          (32ULL * (unsigned long long)request->out_limbs)),
                     request, error, ENGINE_ERROR_REQUEST))
    {
        return CYCLE_ERROR;
    }
    unsigned long long *device_first = NULL;
    unsigned long long first = CYCLE_LATCH_NONE;
    const unsigned long long needed = (request->count + CYCLE_BLOCK - 1ULL) / CYCLE_BLOCK;
    const unsigned int blocks = (unsigned int)((needed < CYCLE_RECORD_BLOCKS_MAX) ? needed : CYCLE_RECORD_BLOCKS_MAX);
    int ok = CYCLE_STATUS_CHECK(cudaMalloc((void **)&device_first, sizeof(unsigned long long)), &device_first, error) &&
             CYCLE_STATUS_CHECK(cudaMemcpy(device_first, &first, sizeof(unsigned long long), cudaMemcpyHostToDevice),
                                device_first, error);
    if (ok != 0)
    {
        cycle_latch_kernel<<<blocks, CYCLE_BLOCK>>>(request->records, request->count, request->out_limbs,
                                                    request->offset, request->bits, device_first);
    }
    ok = ok && CYCLE_STATUS_CHECK(cudaGetLastError(), device_first, error) &&
         CYCLE_STATUS_CHECK(cudaDeviceSynchronize(), device_first, error) &&
         CYCLE_STATUS_CHECK(cudaMemcpy(&first, device_first, sizeof(unsigned long long), cudaMemcpyDeviceToHost),
                            device_first, error);
    cudaFree(device_first);
    if (ok == 0)
    {
        return CYCLE_ERROR;
    }
    *request->first = first;
    return (long)request->count;
}

// limb `limb` of a record's field at `offset`, `bits` wide: the field's bits 32 limb to 32 limb + 31, zero past the
// field. The call reads the limb after the first only where the field runs into it
__device__ static unsigned int cycle_sum_limb(const unsigned int *record, unsigned int offset, unsigned int bits,
                                              unsigned int limb)
{
    const unsigned int start = offset + (32u * limb);
    const unsigned int left = offset + bits - start;
    const unsigned int shift = start % 32u;
    unsigned long long window = record[start / 32u];
    if ((shift != 0u) && (left > (32u - shift)))
    {
        window |= (unsigned long long)record[(start / 32u) + 1u] << 32u;
    }
    const unsigned int value = (unsigned int)(window >> shift);
    return (left < 32u) ? (value & ((1u << left) - 1u)) : value;
}

// the sum over the records. Each thread takes `chunk` consecutive records. Limb by limb, it adds the limb of the field
// into 64 bits for as long as its records stay in one run. It then adds that total to the total of the run by one
// atomic add. The pass past the last limb counts the fields whose sign bit is 1. A run contains at max 2^32 records, and 2^32 limbs
// fit 64 bits
__global__ static void cycle_sum_kernel(const unsigned int *records, unsigned long long count, unsigned long long group,
                                        unsigned int out_limbs, unsigned int offset, unsigned int bits,
                                        unsigned int field_limbs, unsigned long long chunk, unsigned long long *totals,
                                        unsigned long long *negatives)
{
    const unsigned long long thread = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    const unsigned long long begin = thread * chunk;
    const unsigned long long end = ((begin + chunk) < count) ? (begin + chunk) : count;
    const unsigned int top = offset + bits - 1u;
    for (unsigned int limb = 0u; (begin < end) && (limb <= field_limbs); limb += 1u)
    {
        unsigned long long run = begin / group;
        unsigned long long total = 0ULL;
        for (unsigned long long lane = begin; lane < end; lane += 1ULL)
        {
            const unsigned long long here = lane / group;
            if (here != run)
            {
                atomicAdd((limb < field_limbs) ? &totals[(run * field_limbs) + limb] : &negatives[run], total);
                run = here;
                total = 0ULL;
            }
            const unsigned int *const record = &records[lane * out_limbs];
            total += (limb < field_limbs) ? cycle_sum_limb(record, offset, bits, limb)
                                          : ((record[top / 32u] >> (top % 32u)) & 1u);
        }
        atomicAdd((limb < field_limbs) ? &totals[(run * field_limbs) + limb] : &negatives[run], total);
    }
}

// `value` added into `sum` at limb `at`, modulo 2^(32 limbs)
static void cycle_sum_add(unsigned int *sum, unsigned int limbs, unsigned int at, unsigned long long value)
{
    unsigned long long carry = value;
    for (unsigned int limb = at; (limb < limbs) && (carry != 0ULL); limb += 1u)
    {
        const unsigned long long total = (unsigned long long)sum[limb] + (carry & 0xFFFFFFFFULL);
        sum[limb] = (unsigned int)total;
        carry = (carry >> 32u) + (total >> 32u);
    }
}

// `value` times 2^bits taken from `sum`, modulo 2^(32 limbs)
static void cycle_sum_subtract(unsigned int *sum, unsigned int limbs, unsigned int bits, unsigned long long value)
{
    const unsigned int at = bits / 32u;
    const unsigned int shift = bits % 32u;
    const unsigned int pieces[3] = {(unsigned int)(value << shift),
                                    (unsigned int)((shift == 0u) ? (value >> 32u) : (value >> (32u - shift))),
                                    (unsigned int)((shift == 0u) ? 0ULL : (value >> (64u - shift)))};
    unsigned long long borrow = 0ULL;
    for (unsigned int limb = at; limb < limbs; limb += 1u)
    {
        const unsigned long long piece = ((limb - at) < 3u) ? pieces[limb - at] : 0u;
        const unsigned long long taken = piece + borrow;
        borrow = ((unsigned long long)sum[limb] < taken) ? 1ULL : 0ULL;
        sum[limb] = (unsigned int)((unsigned long long)sum[limb] + (borrow << 32u) - taken);
    }
}

// the totals of the sum on the device and its negative counts. They stay between calls, and they grow to the max bytes
// of the requests so far. A sum that a sweep requests once then takes nothing after the first
typedef struct
{
    unsigned long long *totals;
    unsigned long long *negatives;
    size_t total_bytes;
    size_t negative_bytes;
} CycleSumScratch;

static CycleSumScratch g_cycle_sum_scratch;

static int cycle_sum_scratch(size_t total_bytes, size_t negative_bytes, EngineError *error)
{
    CycleSumScratch *const scratch = &g_cycle_sum_scratch;
    int ok = 1;
    if (total_bytes > scratch->total_bytes)
    {
        cudaFree(scratch->totals);
        scratch->totals = NULL;
        scratch->total_bytes = 0u;
        ok = CYCLE_STATUS_CHECK(cudaMalloc((void **)&scratch->totals, total_bytes), &scratch->totals, error);
        scratch->total_bytes = (ok != 0) ? total_bytes : 0u;
    }
    if ((ok != 0) && (negative_bytes > scratch->negative_bytes))
    {
        cudaFree(scratch->negatives);
        scratch->negatives = NULL;
        scratch->negative_bytes = 0u;
        ok = CYCLE_STATUS_CHECK(cudaMalloc((void **)&scratch->negatives, negative_bytes), &scratch->negatives, error);
        scratch->negative_bytes = (ok != 0) ? negative_bytes : 0u;
    }
    return ok;
}

extern "C" void cycle_record_sum_release(void)
{
    cudaFree(g_cycle_sum_scratch.totals);
    cudaFree(g_cycle_sum_scratch.negatives);
    mmgr_bytes_set(&g_cycle_sum_scratch, 0u, sizeof(g_cycle_sum_scratch));
}

extern "C" long cycle_record_sum(const CycleRecordSumRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return CYCLE_ERROR;
    }
    EngineError *const error = request->error;
    if (!CYCLE_CHECK(cycle_record_sum_valid(request) != 0, request, error, ENGINE_ERROR_REQUEST))
    {
        return CYCLE_ERROR;
    }
    const unsigned long long runs = request->count / request->group;
    const unsigned int field_limbs = (request->bits + 31u) / 32u;
    const unsigned long long max = CYCLE_RECORD_BLOCKS_MAX * CYCLE_BLOCK;
    const unsigned long long chunk = (request->count + max - 1ULL) / max;
    const unsigned long long threads = (request->count + chunk - 1ULL) / chunk;
    const unsigned int blocks = (unsigned int)((threads + CYCLE_BLOCK - 1ULL) / CYCLE_BLOCK);
    const size_t total_bytes = (size_t)(runs * field_limbs) * sizeof(unsigned long long);
    const size_t negative_bytes = (size_t)runs * sizeof(unsigned long long);
    // the totals of the device, read back as ephemeral work that goes back before the sum returns
    unsigned long long *const totals = (unsigned long long *)platform_cell_taken(total_bytes, CYCLE_LAUNCH_PLACE, 1);
    unsigned long long *const negatives =
        (unsigned long long *)platform_cell_taken(negative_bytes, CYCLE_LAUNCH_PLACE, 1);
    int ok = CYCLE_CHECK((totals != NULL) && (negatives != NULL), request, error, ENGINE_ERROR_RESOURCE) &&
             cycle_sum_scratch(total_bytes, negative_bytes, error);
    unsigned long long *const device_totals = g_cycle_sum_scratch.totals;
    unsigned long long *const device_negatives = g_cycle_sum_scratch.negatives;
    ok = ok && CYCLE_STATUS_CHECK(cudaMemset(device_totals, 0, total_bytes), device_totals, error) &&
         CYCLE_STATUS_CHECK(cudaMemset(device_negatives, 0, negative_bytes), device_negatives, error);
    if (ok != 0)
    {
        cycle_sum_kernel<<<blocks, CYCLE_BLOCK>>>(request->records, request->count, request->group, request->out_limbs,
                                                  request->offset, request->bits, field_limbs, chunk, device_totals,
                                                  device_negatives);
    }
    ok = ok && CYCLE_STATUS_CHECK(cudaGetLastError(), device_totals, error) &&
         CYCLE_STATUS_CHECK(cudaDeviceSynchronize(), device_totals, error) &&
         CYCLE_STATUS_CHECK(cudaMemcpy(totals, device_totals, total_bytes, cudaMemcpyDeviceToHost), device_totals,
                            error) &&
         CYCLE_STATUS_CHECK(cudaMemcpy(negatives, device_negatives, negative_bytes, cudaMemcpyDeviceToHost),
                            device_negatives, error);
    for (unsigned long long run = 0ULL; (ok != 0) && (run < runs); run += 1ULL)
    {
        unsigned int *const sum = &request->sums[run * request->sum_limbs];
        mmgr_bytes_set(sum, 0u, request->sum_limbs * sizeof(unsigned int));
        for (unsigned int limb = 0u; limb < field_limbs; limb += 1u)
        {
            cycle_sum_add(sum, request->sum_limbs, limb, totals[(run * field_limbs) + limb]);
        }
        cycle_sum_subtract(sum, request->sum_limbs, request->bits, negatives[run]);
    }
    platform_cell_released(negatives, CYCLE_LAUNCH_PLACE);
    platform_cell_released(totals, CYCLE_LAUNCH_PLACE);
    return (ok != 0) ? (long)request->count : CYCLE_ERROR;
}
