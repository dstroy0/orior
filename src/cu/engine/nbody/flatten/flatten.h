// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef FLATTEN_H
#define FLATTEN_H

#include "../max_tree/max_tree.h"

// the orders of one sample, as those of EngineResidualRequest: every background order even, and a smooth order odd or
// even. Then the length of the comb on each axis, 0 or 1 where there is none
typedef struct
{
    unsigned int smooth[ENGINE_AXES];
    unsigned int background[ENGINE_AXES];
    unsigned int comb[ENGINE_AXES];
} FlattenOrders;

// `orders` gives each of the `count` samples its own orders. Where it is NULL, every sample takes `smooth_orders` and
// `background_orders`.
typedef struct
{
    const char *set;
    char *const *names;
    unsigned int count;
    unsigned int smooth_orders[ENGINE_AXES];
    unsigned int background_orders[ENGINE_AXES];
    const FlattenOrders *orders;
    EngineError *error;
} FlattenSetRequest;

int flatten_set(const FlattenSetRequest *request);

// A flattened set. `orders` holds the orders of each sample. `offset_halves` holds the residual place of each sample
// for each axis in half voxels, three for each sample, as engine_residual gives it. That is -1 on an axis whose smooth
// order and the `n - 1` of its comb sum to an odd number, and 0 on the others. The sums of a body sit on the residual
// grid of its own sample.
typedef struct
{
    MaxTreeLayout layout;
    unsigned int samples;
    char **names;
    FlattenOrders *orders;
    int *offset_halves;
    unsigned long long bodies;
    unsigned int *magnitudes;
} FlattenResident;

int flatten_read(const char *set, FlattenResident *resident, EngineError *error);

void flatten_release(FlattenResident *resident);

#endif
