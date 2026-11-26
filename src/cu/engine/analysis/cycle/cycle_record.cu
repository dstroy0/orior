// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_record.cu: the record kernel, the block seal and the shared lanes
#include "cycle_record_internal.h"

#include "../../engine_note.h"
#include "memoria_operor/memoria_operor.h"

// the owner that the cells of a loaded record count to
#define CYCLE_RECORD_PLACE ((unsigned int)platform_place_g_engine_owner)

// a resource of `bytes`, all zero, or NULL where the pool cannot meet it
static void *cycle_record_cell(unsigned long long bytes)
{
    void *const cell = platform_cell_taken(bytes, CYCLE_RECORD_PLACE, 0);
    if (cell != NULL)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = cell, .bytes = (size_t)bytes, .val = 0u);
    }
    return cell;
}

// the block's CRC-64 over every word before its checksum
unsigned long long cycle_block_seal(const EngineProgramBlock *block)
{
    // the block is 64-bit words throughout, asserted above. It reads as them
    return crc_words(CRC_TABLE, (const unsigned long long *)block, offsetof(EngineProgramBlock, checksum) / 8u);
}

extern "C" int cycle_record_compiled(const CycleRecord *record)
{
    return ((record != NULL) && (record->compiled != 0u)) ? 1 : 0;
}

extern "C" const EngineProgramBlock *cycle_record_block(const CycleRecord *record)
{
    return (record != NULL) ? record->block : NULL;
}

// The registers of the compiled program arranged in shared memory. The places of a thread take a word each, and the
// signs of the file a byte each. The threads are as many as the shared memory of one thread block fits beside that of
// the kernel. They are a whole number of warps, up to CYCLE_BLOCK where a warp fits.
//
// A program written as PTX that calls nothing allocates its registers itself, takes no places, and runs CYCLE_BLOCK
// threads a thread block. The kernel may take that much, and it prefers shared memory to L1. The call asks the device
// how many such thread blocks it fits at once. 0 where not one thread fits its registers, or where the runtime errors
// on any of it. The program then stays on the interpreter
static int cycle_record_share(CycleRecord *record, size_t kernel_bytes)
{
    int device = 0;
    int max = 0;
    int processors = 0;
    if ((cudaGetDevice(&device) != cudaSuccess) ||
        (cudaDeviceGetAttribute(&max, cudaDevAttrMaxSharedMemoryPerBlockOptin, device) != cudaSuccess) ||
        (cudaDeviceGetAttribute(&processors, cudaDevAttrMultiProcessorCount, device) != cudaSuccess))
    {
        return 0;
    }
    const unsigned long long thread_bytes =
        (record->places != 0u) ? ((4ULL * record->places) + record->file_limbs) : 0ULL;
    // a device's shared memory a thread block is never negative
    const unsigned long long capacity =
        ((unsigned long long)max > kernel_bytes) ? ((unsigned long long)max - kernel_bytes) : 0ULL;
    const unsigned long long fit = (thread_bytes != 0ULL) ? (capacity / thread_bytes) : CYCLE_BLOCK;
    const unsigned long long block_threads = (fit < CYCLE_BLOCK) ? fit : CYCLE_BLOCK;
    const unsigned long long threads =
        (block_threads >= 32ULL) ? (block_threads - (block_threads % 32ULL)) : block_threads;
    if (threads == 0ULL)
    {
        return 0;
    }
    // at max CYCLE_BLOCK threads, and at max the device's shared memory a thread block, both far under 2^31
    record->threads = (unsigned int)threads;
    record->thread_bytes = thread_bytes;
    // a count of processors is positive where the runtime gave it
    record->processors = (unsigned long long)processors;
    record->register_bytes = threads * thread_bytes;
    record->shared_bytes = record->register_bytes + kernel_bytes;
    int blocks = 0;
    const int ok =
        (cudaFuncSetAttribute((const void *)record->kernel, cudaFuncAttributeMaxDynamicSharedMemorySize,
                              (int)record->register_bytes) == cudaSuccess) &&
        (cudaFuncSetAttribute((const void *)record->kernel, cudaFuncAttributePreferredSharedMemoryCarveout,
                              (int)cudaSharedmemCarveoutMaxShared) == cudaSuccess) &&
        (cudaOccupancyMaxActiveBlocksPerMultiprocessor(&blocks, (const void *)record->kernel, (int)record->threads,
                                                       (size_t)record->register_bytes) == cudaSuccess) &&
        (blocks > 0);
    // a count of thread blocks and of processors are positive where the runtime gave them
    record->resident = (ok != 0) ? ((unsigned long long)blocks * (unsigned long long)processors) : 0ULL;
    return ok;
}

extern "C" long cycle_record_load(const EngineRecordLayout *layout, CycleRecord **record_out, EngineError *error)
{
    if (error == NULL)
    {
        return CYCLE_ERROR;
    }
    int valid = CYCLE_CHECK((layout != NULL) && (record_out != NULL), layout, error, ENGINE_ERROR_REQUEST) &&
                CYCLE_CHECK((layout->step_table != NULL) && (layout->steps != 0u) &&
                                (layout->file_limbs <= ENGINE_RECORD_LIMBS_MAX) && (layout->out_limbs != 0u) &&
                                (layout->members != 0u) && (layout->members <= ENGINE_RECORD_MEMBERS_MAX),
                            layout, error, ENGINE_ERROR_REQUEST);
    for (unsigned int member = 0u; (valid != 0) && (member < layout->members); member += 1u)
    {
        valid = CYCLE_CHECK(layout->in_limbs[member] != 0u, &layout->in_limbs[member], error, ENGINE_ERROR_REQUEST);
    }
    if (valid == 0)
    {
        return CYCLE_ERROR;
    }
    *record_out = NULL;
    CycleRecord *const record = (CycleRecord *)cycle_record_cell(sizeof(CycleRecord));
    int ok = CYCLE_CHECK(record != NULL, record_out, error, ENGINE_ERROR_RESOURCE);
    // The steps, the tables, the error count, the block and the hot words of the record are persistent cells of the
    // pool of the device. The record holds them from its load to its release, across every run
    void *cell = NULL;
    const unsigned long long step_bytes = (unsigned long long)layout->steps * sizeof(DeviceRecordStep);
    ok = ok && CYCLE_CHECK(tessera_job_persistent_taken(step_bytes, CYCLE_RECORD_PLACE, &cell) &&
                               tessera_job_crossed_into(cell, layout->step_table, step_bytes),
                           layout, error, ENGINE_ERROR_RESOURCE);
    // Explicit casts read each cell as the words that it holds
    record->device_steps = (ok != 0) ? (DeviceRecordStep *)cell : NULL;
    ok = ok && CYCLE_CHECK(tessera_job_persistent_taken(sizeof(unsigned int), CYCLE_RECORD_PLACE, &cell), layout, error,
                           ENGINE_ERROR_RESOURCE);
    record->device_error = (ok != 0) ? (unsigned int *)cell : NULL;
    const unsigned long long table_bytes = layout->table_word_count * sizeof(unsigned int);
    if ((ok != 0) && (table_bytes != 0ULL))
    {
        ok = CYCLE_CHECK(tessera_job_persistent_taken(table_bytes, CYCLE_RECORD_PLACE, &cell) &&
                             tessera_job_crossed_into(cell, layout->table_values, table_bytes),
                         layout, error, ENGINE_ERROR_RESOURCE);
        record->device_tables = (ok != 0) ? (unsigned int *)cell : NULL;
    }
    // the block lies in device memory, where the thread blocks of the program check in and stay on the device. The host
    // retains its own copy, which it reads back once each launch ends
    record->block = (EngineProgramBlock *)cycle_record_cell(sizeof(EngineProgramBlock));
    ok = ok && CYCLE_CHECK(record->block != NULL, &record->block, error, ENGINE_ERROR_RESOURCE) &&
         CYCLE_CHECK(tessera_job_persistent_taken(sizeof(EngineProgramBlock), CYCLE_RECORD_PLACE, &cell), layout, error,
                     ENGINE_ERROR_RESOURCE);
    record->device_block = (ok != 0) ? (EngineProgramBlock *)cell : NULL;
    ok = ok && CYCLE_CHECK(tessera_job_persistent_taken(sizeof(CycleHot), CYCLE_RECORD_PLACE, &cell), layout, error,
                           ENGINE_ERROR_RESOURCE);
    record->hot = (ok != 0) ? (CycleHot *)cell : NULL;
    if (ok == 0)
    {
        cycle_record_release(record);
        return CYCLE_ERROR;
    }
    // the program is its steps, its sizes and its tables, and the signum reads all three in that order
    const unsigned int extent[6] = {layout->members,     layout->in_limbs[0], layout->in_limbs[1],
                                    layout->in_limbs[2], layout->out_bits,    layout->file_limbs};
    // the bytes that the signum reads, ephemeral work that goes back after it
    const unsigned long long program_bytes = step_bytes + sizeof(extent) + table_bytes;
    // Explicit cast reads the cell as the bytes that it holds
    unsigned char *const program = (unsigned char *)platform_cell_taken(program_bytes, CYCLE_RECORD_PLACE, 1);
    if (!CYCLE_CHECK(program != NULL, record, error, ENGINE_ERROR_RESOURCE))
    {
        cycle_record_release(record);
        return CYCLE_ERROR;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = program, .src = layout->step_table, .bytes = (size_t)step_bytes);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = program + step_bytes, .src = extent, .bytes = sizeof(extent));
    if (table_bytes != 0ULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = program + step_bytes + sizeof(extent), .src = layout->table_values,
                   .bytes = (size_t)table_bytes);
    }
    const ObsignatioSignumRequest signum = {
        program, (size_t)program_bytes, NULL, OBSIGNATIO_MODE_HASH, record->block->signature.bytes, ENGINE_SIGNUM_BYTES,
        error};
    const int signed_program = obsignatio_signum(&signum) != OBSIGNATIO_ERROR;
    platform_cell_released(program, CYCLE_RECORD_PLACE);
    if (!signed_program)
    {
        cycle_record_release(record);
        return CYCLE_ERROR;
    }
    record->block->span = layout->file_limbs;
    record->block->state = ENGINE_PROGRAM_PLACED;
    record->block->checksum = cycle_block_seal(record->block);
    if (!CYCLE_CHECK(tessera_job_crossed_into(record->device_block, record->block, sizeof(EngineProgramBlock)),
                     record->device_block, error, ENGINE_ERROR_RESOURCE))
    {
        cycle_record_release(record);
        return CYCLE_ERROR;
    }
    record->steps = layout->steps;
    record->members = layout->members;
    record->file_limbs = layout->file_limbs;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = record->in_limbs, .src = layout->in_limbs,
               .bytes = sizeof(record->in_limbs));
    record->out_limbs = layout->out_limbs;
    record->table_words = layout->table_word_count;
    for (unsigned int step = 0u; step < layout->steps; step += 1u)
    {
        const unsigned int operation = layout->step_table[step].operation;
        record->divides |= ((operation == ENGINE_RECORD_QUOTIENT) || (operation == ENGINE_RECORD_REMAINDER) ||
                            (operation == ENGINE_RECORD_GCD) || (operation == ENGINE_RECORD_EXACT_QUOTIENT))
                               ? 1u
                               : 0u;
    }
    const int compiled =
        (cycle_environment_set("CYCLE_RECORD_INTERPRET") == 0) ? cycle_record_compile(layout, record, error) : 0;
    if (compiled == (int)CYCLE_ERROR)
    {
        cycle_record_release(record);
        return CYCLE_ERROR;
    }
    if ((cycle_environment_set("CYCLE_RECORD_INTERPRET") == 0) && (compiled == 0))
    {
        // every call above succeeded, and an error that the runtime still retains belongs to the attempt. The
        // interpreter runs in its place, and the call drops the error before a run reads the last error of the runtime
        // as its own
        cudaGetLastError();
    }
    // a host program has no device grant: its thread blocks are the host's threads, of one thread each
    const int host = (record->compiled != 0u) && (record->host_program != NULL);
    record->threads = (host != 0) ? 1u : record->threads;
    cudaFuncAttributes attributes;
    const int attributed = (record->compiled != 0u) && (host == 0) &&
                           (cudaFuncGetAttributes(&attributes, (const void *)record->kernel) == cudaSuccess);
    if (attributed != 0)
    {
        // a register count and a frame's bytes are never negative
        record->registers = (unsigned long long)attributes.numRegs;
        record->local_bytes = (unsigned long long)attributes.localSizeBytes;
    }
    // a compiled program whose registers shared memory cannot fit runs on the interpreter
    record->compiled =
        ((host != 0) || ((attributed != 0) && cycle_record_share(record, attributes.sharedSizeBytes))) ? 1u : 0u;
    if (cycle_environment_set("CYCLE_RECORD_REPORT") != 0)
    {
        EngineNote note;
        engine_note_begun(&note);
        if (host != 0)
        {
            mmgr_line_put(&note.line, "  cycle: the program uses ");
            mmgr_line_decimal(&note.line, record->places, 1u);
            mmgr_line_put(&note.line, " places a thread, and runs on the host's threads\n");
        }
        else if (record->compiled != 0u)
        {
            mmgr_line_put(&note.line, "  cycle: the program uses ");
            mmgr_line_decimal(&note.line, record->registers, 1u);
            mmgr_line_put(&note.line, " registers a thread, a ");
            mmgr_line_decimal(&note.line, record->local_bytes, 1u);
            mmgr_line_put(&note.line, "-byte local frame and ");
            mmgr_line_decimal(&note.line, record->places, 1u);
            mmgr_line_put(&note.line, " places a thread in shared memory: ");
            mmgr_line_decimal(&note.line, record->threads, 1u);
            mmgr_line_put(&note.line, " threads a thread block in ");
            mmgr_line_decimal(&note.line, record->shared_bytes, 1u);
            mmgr_line_put(&note.line, " bytes, ");
            mmgr_line_decimal(&note.line, record->resident, 1u);
            mmgr_line_put(&note.line, " thread blocks at once\n");
        }
        else if (attributed != 0)
        {
            mmgr_line_put(&note.line, "  cycle: a program of ");
            mmgr_line_decimal(&note.line, record->places, 1u);
            mmgr_line_put(&note.line, " places a thread runs on the interpreter (shared memory does not fit one "
                                      "thread's)\n");
        }
        if (note.line.at != 0u)
        {
            engine_note_written(&note);
        }
    }
    // a grant that the runtime could not read stays 0, and the call drops its error as that of the attempt
    cudaGetLastError();
    *record_out = record;
    return (long)layout->out_bits;
}

extern "C" void cycle_record_release(CycleRecord *record)
{
    if (record == NULL)
    {
        return;
    }
    // the persistent cells go back newest first, and tessera_job_given passes over a NULL cell
    tessera_job_given(record->hot, CYCLE_RECORD_PLACE);
    tessera_job_given(record->device_block, CYCLE_RECORD_PLACE);
    tessera_job_given(record->device_tables, CYCLE_RECORD_PLACE);
    tessera_job_given(record->device_error, CYCLE_RECORD_PLACE);
    tessera_job_given(record->device_steps, CYCLE_RECORD_PLACE);
    // the record gives back its reference to the program it loaded, compiled or left on the interpreter after
    cycle_program_release(record->kernel);
    cycle_host_program_release(record->host_program);
    platform_cell_released(record->block, CYCLE_RECORD_PLACE);
    platform_cell_released(record, CYCLE_RECORD_PLACE);
}

extern "C" unsigned int cycle_record_out_limbs(const CycleRecord *record)
{
    return (record != NULL) ? record->out_limbs : 0u;
}

extern "C" unsigned int cycle_record_members(const CycleRecord *record)
{
    return (record != NULL) ? record->members : 0u;
}

extern "C" unsigned int cycle_record_in_limbs(const CycleRecord *record, unsigned int member)
{
    return ((record != NULL) && (member < record->members)) ? record->in_limbs[member] : 0u;
}
