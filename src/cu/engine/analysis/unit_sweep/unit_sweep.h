// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef UNIT_SWEEP_H
#define UNIT_SWEEP_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UNIT_SWEEP_ERROR (-1L)

    typedef struct
    {
        const unsigned short *device_volume;
        const unsigned int *device_planes;
        unsigned int input_bits;
        unsigned int depth;
        unsigned int height;
        unsigned int width;
        unsigned int smooth_orders[ENGINE_AXES];
        unsigned int background_orders[ENGINE_AXES];
        unsigned int limbs;
        unsigned int *device_out;
        EngineError *error;
        // the comb, as EngineResidualRequest's
        unsigned int comb[ENGINE_AXES];
        // the spaced pairs, as EngineResidualRequest's
        unsigned int smooth_spaced[ENGINE_AXES][ENGINE_SPACINGS];
        unsigned int background_spaced[ENGINE_AXES][ENGINE_SPACINGS];
    } UnitSweepRequest;

    long unit_sweep_residual(const UnitSweepRequest *request);

    // the bits that hold the request's residual, its sign with them: the input's, 1 for each unit order, 2 for each
    // spaced pair, and a comb's bit_length(n - 1)
    unsigned long long unit_sweep_bits(const UnitSweepRequest *request);

    // the device bytes unit_sweep_residual holds for the request: its two planes, narrow and wide, and its two
    // counters, each in whole pages (DEVICE_POOL_PAGE_BYTES)
    unsigned long long unit_sweep_bytes(const UnitSweepRequest *request);

    typedef struct
    {
        const unsigned int *device_left;
        const unsigned int *device_right;
        unsigned long long lanes;
        unsigned int limbs;
        unsigned long long *disagreements;
        EngineError *error;
    } UnitSweepComparison;

    long unit_sweep_lanes_compare(const UnitSweepComparison *comparison);

    void unit_sweep_release(void);

#ifdef __cplusplus
}
#endif

#endif
