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
    // the background's gain: its order on each axis and 2 bits for each of its spaced pairs
    unsigned long long gain = 0ull;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        gain += request->background_orders[axis];
        for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
        {
            gain += 2ull * request->background_spaced[axis][spacing];
        }
    }
    if (!RESIDUAL_CHECK(gain <= 0xFFFFFFFFull, request->background_spaced, error, ENGINE_ERROR_REQUEST))
    {
        return RESIDUAL_ERROR;
    }
    memset(program, 0, RESIDUAL_STEPS * sizeof(EngineStep));
    unsigned int at = 0u;
    // the comb runs before the keep, and both terms take it alike
    program[at].operation = ENGINE_COMB;
    memcpy(program[at].orders, request->comb, sizeof(program[at].orders));
    at += 1u;
    program[at].operation = ENGINE_SMOOTH;
    memcpy(program[at].orders, request->smooth_orders, sizeof(program[at].orders));
    at += 1u;
    for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
    {
        program[at].operation = ENGINE_SPACED;
        program[at].shift = spacing;
        for (unsigned int axis = 0u; axis < 3u; axis += 1u)
        {
            program[at].orders[axis] = request->smooth_spaced[axis][spacing];
        }
        at += 1u;
    }
    program[at].operation = ENGINE_KEEP;
    at += 1u;
    program[at].operation = ENGINE_SMOOTH;
    memcpy(program[at].orders, request->background_orders, sizeof(program[at].orders));
    at += 1u;
    for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
    {
        program[at].operation = ENGINE_SPACED;
        program[at].shift = spacing;
        for (unsigned int axis = 0u; axis < 3u; axis += 1u)
        {
            program[at].orders[axis] = request->background_spaced[axis][spacing];
        }
        at += 1u;
    }
    program[at].operation = ENGINE_SCALE_SUBTRACT;
    // the gain is held at or below 2^32 - 1 above, and narrows to unsigned int exactly
    program[at].shift = (unsigned int)gain;
    return (long)RESIDUAL_STEPS;
}
