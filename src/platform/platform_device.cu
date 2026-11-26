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
#include "embed_turnstile.h"
#include "locus_carcerum/locus_carcerum.h"
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

#if defined(__CUDA_ARCH__)

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
