// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_places.h: the owners that the cells of a program count to, as one list and nothing else. owners.h makes the
// owner table of MMgr from this list, and platform.h makes the places of its owners from it. A module that takes cells
// has one line here, its identity and the name that a reader prints
#ifndef PLATFORM_PLACES_H
#define PLATFORM_PLACES_H

// clang-format off
#define PLATFORM_OWNERS(owner_)                         \
    owner_(g_exact_owner, "exact_types")                \
    owner_(g_exact_record_owner, "exact_record")        \
    owner_(g_engine_owner, "engine")                    \
    owner_(g_program_owner, "program")
// clang-format on

#endif
