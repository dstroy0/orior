// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef RESIDUAL_H
#define RESIDUAL_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RESIDUAL_ERROR (-1L)

// the comb, the smooth, the keep, the background's smooth and the scaled subtraction
#define RESIDUAL_STEPS 5u

    long residual_program(const EngineResidualRequest *request, EngineStep program[RESIDUAL_STEPS]);

#ifdef __cplusplus
}
#endif

#endif
