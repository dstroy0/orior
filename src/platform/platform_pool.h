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
#define PLATFORM_POOL_MOVABLE 65537u

// the guard of the cellblock of the pool. A module takes a cell with persistent_buf_alloc(bytes, owner), and gives it
// back with persistent_buf_release(cell, owner). The owner is the line of the module in owners.h. A take that the
// cellblock cannot meet answers NULL, and every take before platform_open answers NULL
extern const MinimumSecurityGuard *const g_platform_guard;

EMBED_END_DECLS

#endif
