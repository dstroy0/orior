// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// owners.h: the owners of platform_places.h as MMgr reaches them. platform_pool.c positions the owner table of MMgr
// from the list with MMGR_OWNERS. Each owner is a const pointer into that table, and the linker resolves it
#ifndef PLATFORM_OWNERS_H
#define PLATFORM_OWNERS_H
#include "locus_carcerum/locus_carcerum.h"
#include "platform_places.h"

EMBED_BEGIN_DECLS

PLATFORM_OWNERS(MMGR_OWNER_EXTERN)

EMBED_END_DECLS

#endif
