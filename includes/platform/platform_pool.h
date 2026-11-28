// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_pool.h: the pool of the program as the C model above the boundary reaches it: the guard of its cellblock and
// the owners that each take names (owners.h). platform_open of platform.h places the pool
#ifndef PLATFORM_POOL_H
#define PLATFORM_POOL_H
#include "owners.h"
#include "platform.h"

EMBED_BEGIN_DECLS

// the movable cells that the pool names at once
#define PLATFORM_POOL_MOVABLE 131073u

// the guard of the cellblock of the pool. A module takes a cell with persistent_buf_alloc(bytes, owner), and gives it
// back with persistent_buf_release(cell, owner). The owner is the line of the module in owners.h. A take that the
// cellblock cannot meet answers NULL, and every take before platform_open answers NULL
extern const MinimumSecurityGuard *const g_platform_guard;

// The pool emptied for a request of `bytes`, which the pool has to cover. The mover ends first, and it starts again on
// the emptied pool or after a refusal. 1, or 0 where a cell is still out, the request passes the pool, or no pool is
// open. A program that runs one request after another empties its pool between them
int platform_pool_emptied(unsigned long long bytes);

// the record of the cellblock, which mmgr_owner_of_buf and the readers of the peaks take
const SeriesCellularum *platform_pool_series(void);

// the sorts of the cellblock that the mover started since the program began
unsigned long long platform_pool_sorts(void);

// The pages of the cellblock that no cell holds given back to the system. They are the gap between the tiers and the
// room of each region above its depth, cut to whole pages. The call gives back only a span of 64 KiB or more. The
// working set then
// reads the live cells, and not every page that a take touched. A thread that no cell work runs on calls it
void platform_pool_idle_released(void);

EMBED_END_DECLS

#endif
