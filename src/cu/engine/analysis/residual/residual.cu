// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "residual.h"

#include <stdlib.h>
#include <string.h>

#define RESIDUAL_CHECK(condition_, evacaddr_, error_, kind_)                                                           \
    engine_error_check((condition_), (kind_), ENGINE_MODULE_RESIDUAL, (unsigned int)__LINE__,                          \
                       (const void *)(evacaddr_), (error_))

extern "C" long residual_program(const EngineResidualRequest *request, EngineStep program[RESIDUAL_STEPS])
{
    if ((request == NULL) || (request->error == NULL))
    {
        return RESIDUAL_ERROR;
    }
    EngineError *const error = request->error;
    if (!RESIDUAL_CHECK(program != NULL, &program, error, ENGINE_ERROR_REQUEST))
    {
        return RESIDUAL_ERROR;
    }
    // an odd smooth order moves both terms' centers half a voxel alike; an odd background order would move the wide
    // term's alone, and the two would be subtracted half a voxel apart
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        if (!RESIDUAL_CHECK((request->background_orders[axis] & 1u) == 0u, &request->background_orders[axis], error,
                            ENGINE_ERROR_REQUEST))
        {
            return RESIDUAL_ERROR;
        }
    }
    const unsigned int gain =
        request->background_orders[0] + request->background_orders[1] + request->background_orders[2];
    memset(program, 0, RESIDUAL_STEPS * sizeof(EngineStep));
    // the comb runs before the keep, and both terms take it alike
    program[0].operation = ENGINE_COMB;
    memcpy(program[0].orders, request->comb, sizeof(program[0].orders));
    program[1].operation = ENGINE_SMOOTH;
    memcpy(program[1].orders, request->smooth_orders, sizeof(program[1].orders));
    program[2].operation = ENGINE_KEEP;
    program[3].operation = ENGINE_SMOOTH;
    memcpy(program[3].orders, request->background_orders, sizeof(program[3].orders));
    program[4].operation = ENGINE_SCALE_SUBTRACT;
    program[4].shift = gain;
    return (long)RESIDUAL_STEPS;
}
