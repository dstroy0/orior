// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "c_target.h"

// the lane as C source: c.krs, which NVRTC compiles under the prelude, its file and signs in shared memory
CTarget::CTarget(void) : CodeGenerator("c.krs")
{
}

CTarget &c_target(void)
{
    static CTarget generator;
    return generator;
}
