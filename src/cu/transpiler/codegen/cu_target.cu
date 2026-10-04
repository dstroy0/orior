// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "cu_target.h"

// the lane as CUDA C++: cu.krs, which nvcc compiles under the prelude, its file and signs in shared memory
CuTarget::CuTarget(void) : CodeGenerator("cu.krs", "nvcc", "prelude", SCHEDULE_UNBOUNDED, 1)
{
}

CuTarget &cu_target(void)
{
    static CuTarget generator;
    return generator;
}
