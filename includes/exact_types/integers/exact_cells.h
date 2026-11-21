// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file exact_cells.h
 * @brief The cells of exact_types: the guard of their pool, their owners, and the takes of the work.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The program defines the guard and the two owners, as it defines the hooks of embedded_types. The platform of
 *       the program places the pool at the process stage, and every take before that answers NULL.
 * @note All work of exact_types is ephemeral. It is on the local stack or on the temporary tier. It goes back before
 *       the call or the build that took it ends.
 */
#ifndef EXACT_CELLS_H
#define EXACT_CELLS_H

#include "locus_carcerum/locus_carcerum.h"
#include "memoria_operor/memoria_operor.h"

/** @brief The guard of the cellblock that exact_types takes its cells from. */
extern const MinimumSecurityGuard *const g_exact_guard;

/** @brief The owner that the work of the exact integers counts to, a line of the owner table of the program. */
extern const CellOwner *const g_exact_owner;

/** @brief The owner that the build of a record program counts to, a line of the owner table of the program. */
extern const CellOwner *const g_exact_record_owner;

/**
 * @brief The ephemeral work of a call: `count` limbs on the temporary tier, zero where `zeroed` is 1.
 *
 * @param[in] count  Limbs of the work.
 * @param[in] zeroed 1 where the work starts at zero, 0 where the call writes it before it reads it.
 * @return           The work [RETURNS OWNERSHIP], or NULL where the pool cannot meet it.
 * @note A call gives the work back with exact_work_released before it returns.
 */
static inline uint32_t *exact_work_taken(size_t count, int zeroed)
{
    // Explicit cast takes the cell of the pool as the limbs that it holds
    uint32_t *const work = (uint32_t *)g_exact_guard->temporary_buf_alloc(count * sizeof(uint32_t), g_exact_owner);
    if ((work != NULL) && (zeroed != 0))
    {
        memor.set(&(MemoriaCfg){.dst = work, .bytes = count * sizeof(uint32_t), .val = 0u});
    }
    return work;
}

/**
 * @brief The ephemeral work of a call given back to the pool.
 *
 * @param[in] work The work that exact_work_taken gave, or NULL [TAKES OWNERSHIP].
 */
static inline void exact_work_released(uint32_t *work)
{
    if (work != NULL)
    {
        g_exact_guard->persistent_buf_release(work, g_exact_owner);
    }
}

/**
 * @brief A cell of the build of a record program, on the temporary tier, zero where `zeroed` is 1.
 *
 * @param[in] bytes  Bytes of the cell.
 * @param[in] zeroed 1 where the cell starts at zero, 0 where the build writes it before it reads it.
 * @return           The cell [RETURNS OWNERSHIP], or NULL where the pool cannot meet it.
 * @note The build gives every cell back at exact_record_close or before.
 */
static inline void *exact_record_cell_taken(size_t bytes, int zeroed)
{
    void *const cell = g_exact_guard->temporary_buf_alloc(bytes, g_exact_record_owner);
    if ((cell != NULL) && (zeroed != 0))
    {
        memor.set(&(MemoriaCfg){.dst = cell, .bytes = bytes, .val = 0u});
    }
    return cell;
}

/**
 * @brief A cell of the build of a record program given back to the pool.
 *
 * @param[in] cell The cell that exact_record_cell_taken gave, or NULL [TAKES OWNERSHIP].
 */
static inline void exact_record_cell_released(void *cell)
{
    if (cell != NULL)
    {
        g_exact_guard->persistent_buf_release(cell, g_exact_record_owner);
    }
}

#endif
