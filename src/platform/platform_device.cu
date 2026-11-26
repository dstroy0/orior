// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_device.cu: the hooks of embedded_types and MMgr in the device pass of nvcc, and the device copy of the owner
// table. A device has no weak default, and the device link of a program that links MMgr's device objects names each of
// them. The host pass of this unit defines no hook. platform_windows.c, platform_posix.c and platform_pool.c define the
// host copy of each hook and of the table, and so no hook has two definitions in the host link.
//
// The device pass defines each hook as the contract of its declaration says. The wait spins on the word for its time,
// as a device has no address wait. The wake has nothing to do, as a spin sees the word move. The clock is the global
// timer of the device, in nanoseconds. The place of a thread is its index in the grid
#include "platform_device.h"

#include "embed_turnstile.h"
#include "locus_carcerum/locus_carcerum.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_linea.h"

#include "platform_places.h"

#include <stdio.h>

// The device copy of the owner table and its count, the device half of MMGR_OWNERS. Its places and its records come
// from the list that platform_pool.c positions the host copy from. Both passes see the two objects, as the host pass
// registers each device object of the unit. Neither pass defines the host copy, which platform_pool.c defines
enum
{
    PLATFORM_OWNERS(MMGR_OWNER_PLACE) platform_device_owners_positioned
};
EMBED_STATIC_ASSERT((size_t)platform_device_owners_positioned <= (size_t)MMGR_CARCER_OWNERS,
                    "more owners are declared than MMGR_CARCER_OWNERS allows");
__device__ CellOwner mmgr_owner_table_device[] = {PLATFORM_OWNERS(MMGR_OWNER_RECORD)};
__device__ const size_t mmgr_owner_table_count_device = (size_t)platform_device_owners_positioned;

// The cells of the movable option of the pool of the device, and the rows of its touches. Then the lanes that its
// turnstiles may have open while it reads quiet. Nothing moves a cell on its own on the device, as it has no thread for
// a mover
#define PLATFORM_DEVICE_MOVABLE 4096u
#define PLATFORM_DEVICE_WIDTH 32u
#define PLATFORM_DEVICE_TOUCH_ROWS 64u

// The bytes of the pool of the device, in the memory of the device alone. The regions of the temporary tier come off
// its top beside the bytes that platform_device.h asks for, as they do on the host
EMBED_ALIGN(MMGR_ALIGN_BYTES)
static __device__ uint8_t s_device_storage[PLATFORM_DEVICE_POOL_BYTES + (unsigned long long)MMGR_CARCER_REGION_AREA];

// The cellblock of the device over that pool, and the storage of its movable option. Each pass of nvcc has its own copy
// of the two records, and a kernel reads the copy of the device. Minimum security: a release leaves the bytes of a cell
// as they are
ParsMemoriaeHospes(platform_device_pool);
LocusCarcerum(platform_device_site, MMGR_MINIMUM_SECURITY_HOSPES(platform_device_pool));
ParsMemoriaeInternae(platform_device_movable, MMGR_MOVABLE_STORAGE_BYTES(PLATFORM_DEVICE_MOVABLE, PLATFORM_DEVICE_WIDTH,
                                                                         PLATFORM_DEVICE_TOUCH_ROWS));

// 1 once the pool lies on the memory of the device. Both passes see it, as the host pass registers each device object
static __device__ int s_device_placed;

#if defined(__CUDA_ARCH__)

// the id of the thread that calls, in the width that MMgr badges a pass with: its index in the grid
static __device__ size_t platform_device_thread_id(void)
{
    return (size_t)embed_thread_place();
}

extern "C" __device__ int platform_device_open(void)
{
    if (s_device_placed != 0)
    {
        return 1;
    }
    if (!mmgr_series_cellularum_placed(&platform_device_site_platform_device_pool_ctx, s_device_storage,
                                       sizeof(s_device_storage)))
    {
        return 0;
    }
    // a refusal leaves the pool with no movable option, and its movable takes then answer NULL
    (void)EMBED_CALL(platform_device_site.platform_device_pool.movable_init, CarcerMovableInitCfg,
                     .storage = mmgr_pars_storage_platform_device_movable,
                     .storage_bytes = platform_device_movable_bytes, .cells = PLATFORM_DEVICE_MOVABLE,
                     .width = PLATFORM_DEVICE_WIDTH, .rows = PLATFORM_DEVICE_TOUCH_ROWS, .thread_started = NULL,
                     .thread_id = platform_device_thread_id, .thread_wait = NULL, .thread_yield = NULL,
                     .thread_moving = NULL);
    s_device_placed = 1;
    return 1;
}

extern "C" __device__ void *platform_device_cell_taken(unsigned long long bytes, unsigned int place, int ephemeral)
{
    if ((s_device_placed == 0) || (place >= (unsigned int)platform_device_owners_positioned))
    {
        return NULL;
    }
    const CellOwner *const owner = &mmgr_owner_table_device[place];
    // Explicit casts take the count of bytes in memory, which a size_t counts
    return (ephemeral != 0) ? platform_device_site.platform_device_pool.temporary_buf_alloc((size_t)bytes, owner)
                            : platform_device_site.platform_device_pool.persistent_buf_alloc((size_t)bytes, owner);
}

extern "C" __device__ void *platform_device_cell_cleared(unsigned long long bytes, unsigned int place, int ephemeral)
{
    void *const cell = platform_device_cell_taken(bytes, place, ephemeral);
    if (cell != NULL)
    {
        // Explicit cast takes the count of bytes in memory, which a size_t counts
        EMBED_CALL(memor.set, MemoriaCfg, .dst = cell, .bytes = (size_t)bytes, .val = 0u);
    }
    return cell;
}

extern "C" __device__ void platform_device_cell_released(void *cell, unsigned int place)
{
    if ((cell != NULL) && (s_device_placed != 0) && (place < (unsigned int)platform_device_owners_positioned))
    {
        (void)platform_device_site.platform_device_pool.persistent_buf_release(cell, &mmgr_owner_table_device[place]);
    }
}

extern "C" __device__ const void *platform_device_movable_taken(unsigned long long bytes, unsigned int place)
{
    if ((s_device_placed == 0) || (place >= (unsigned int)platform_device_owners_positioned))
    {
        return NULL;
    }
    // Explicit cast takes the count of bytes in memory, which a size_t counts
    return platform_device_site.platform_device_pool.movable_buf_alloc((size_t)bytes, &mmgr_owner_table_device[place]);
}

extern "C" __device__ int platform_device_movable_written(const void *cell, unsigned long long offset, const void *from,
                                                          unsigned long long count)
{
    // Explicit casts read the handle as the entry of the pool that it is, and the counts as sizes in memory
    return (cell != NULL) && EMBED_CALL(platform_device_site.platform_device_pool.movable_buf_write, CarcerMovableCfg,
                                        .cell = (const CarcerMovableEntry *)cell, .offset = (size_t)offset, .src = from,
                                        .bytes = (size_t)count);
}

extern "C" __device__ int platform_device_movable_read(const void *cell, unsigned long long offset, void *into,
                                                       unsigned long long count)
{
    // Explicit casts read the handle as the entry of the pool that it is, and the counts as sizes in memory
    return (cell != NULL) && EMBED_CALL(platform_device_site.platform_device_pool.movable_buf_read, CarcerMovableCfg,
                                        .cell = (const CarcerMovableEntry *)cell, .offset = (size_t)offset, .dst = into,
                                        .bytes = (size_t)count);
}

extern "C" __device__ void platform_device_movable_released(const void *cell, unsigned int place)
{
    if ((cell != NULL) && (s_device_placed != 0) && (place < (unsigned int)platform_device_owners_positioned))
    {
        // Explicit cast reads the handle as the entry of the pool that it is
        (void)platform_device_site.platform_device_pool.movable_buf_release((const CarcerMovableEntry *)cell,
                                                                            &mmgr_owner_table_device[place]);
    }
}

extern "C" __device__ unsigned long long platform_device_available(void)
{
    return (s_device_placed != 0) ? (unsigned long long)platform_device_site.platform_device_pool.buf_available()
                                  : 0ULL;
}

EMBED_HOST_DEVICE embed_word embed_clock_ns(void)
{
    unsigned long long clock_ns;
    asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(clock_ns));
    // Explicit cast takes the nanoseconds modulo the word, as the contract of the clock says
    return (embed_word)clock_ns;
}

EMBED_HOST_DEVICE int embed_word_wait(const EmbedWordWaitCfg *args)
{
    const embed_word start_ns = embed_clock_ns();
    while (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) == args->old_value)
    {
        // Explicit cast takes the difference modulo the word, which is correct across a roll of the clock
        const embed_word elapsed_ns = (embed_word)(embed_clock_ns() - start_ns);
        if ((args->nanoseconds != EMBED_WAIT_INFINITE) && (elapsed_ns >= args->nanoseconds))
        {
            return 0;
        }
    }
    return 1;
}

EMBED_HOST_DEVICE void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)args;
}

EMBED_HOST_DEVICE embed_u32 embed_thread_place(void)
{
    const embed_u32 block = blockIdx.x + (gridDim.x * (blockIdx.y + (gridDim.y * blockIdx.z)));
    const embed_u32 thread = threadIdx.x + (blockDim.x * (threadIdx.y + (blockDim.y * threadIdx.z)));
    return (block * (blockDim.x * blockDim.y * blockDim.z)) + thread;
}

// the clock that badges a take of a cell, the clock of the device, as the host's badges a take on the host
EMBED_HOST_DEVICE uint64_t mmgr_carcer_hw_clock_ns(void)
{
    return (uint64_t)embed_clock_ns();
}

// The note that platform_pool.c writes on the host for a give that names an owner other than the cell's, written by
// the device. It names both owners, and a reader then finds the give in the wrong place
EMBED_HOST_DEVICE void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    EMBED_ALIGN(MMGR_ALIGN_BYTES) char text[192];
    VerbaLinea line = mmgr_line_over(text, sizeof(text));
    mmgr_line_put(&line, "  platform: a cell of ");
    mmgr_line_decimal(&line, bytes, 1u);
    mmgr_line_put(&line, " bytes went back on the device as a cell of ");
    mmgr_line_put(&line, (named != NULL) ? named->name : "no owner");
    mmgr_line_put(&line, ", and it counts to ");
    mmgr_line_put(&line, (cell_owner != NULL) ? cell_owner->name : "no owner");
    mmgr_line_ch(&line, '\n');
    (void)mmgr_line_finish(&line);
    printf("%s", text);
}

#endif
