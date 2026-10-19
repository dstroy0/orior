// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "cu_target.h"

// the lane as CUDA C++: cu.krs, which nvcc compiles under the prelude, its file and signs in shared memory
CuTarget::CuTarget(void) : CodeGenerator("cu.krs")
{
}

CuTarget &cu_target(void)
{
    static CuTarget generator;
    return generator;
}
