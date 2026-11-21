// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// owners.h: the owners that the cells of a program count to, the one list of the program. platform_pool.c positions the
// owner table of MMgr from this list with MMGR_OWNERS. Each owner is a const pointer into that table, and the linker
// resolves it. A module that takes cells has one line here, its identity and the name that a reader prints
#ifndef PLATFORM_OWNERS_H
#define PLATFORM_OWNERS_H
#include "locus_carcerum/locus_carcerum.h"

// clang-format off
#define PLATFORM_OWNERS(owner_)                         \
    owner_(g_exact_owner, "exact_types")                \
    owner_(g_exact_record_owner, "exact_record")        \
    owner_(g_engine_owner, "engine")                    \
    owner_(g_program_owner, "program")
// clang-format on

EMBED_BEGIN_DECLS

PLATFORM_OWNERS(MMGR_OWNER_EXTERN)

EMBED_END_DECLS

#endif
