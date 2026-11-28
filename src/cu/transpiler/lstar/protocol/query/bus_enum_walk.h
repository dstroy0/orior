// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef BUS_ENUM_WALK_H
#define BUS_ENUM_WALK_H

// The classify walk, driven over a span in probes that the interface can lose, and the enumeration region read from
// the types that it collects. `walk->program` is the classify_walk program. The call uses its from, count, stride,
// output_path and limit, and ignores the qualifier, turns and word.
#include "bus_enum.h"
#include "query_interface.h"

// the type of each address of the walk, read into `types` (walk->count of them), with NOTHING for an address that ends
// its probe. Then the enumeration region, read into `base`, `stride_out` and `records`. The return is 1 where the call
// finds a region, and 0 where it finds none. It is -1 where the interface or a probe failed, with `error` filled.
int bus_enum_walk(const QueryWalk *walk, unsigned int *types, unsigned long long *base, unsigned long long *stride_out,
                  unsigned long long *records, EngineError *error);

#endif
