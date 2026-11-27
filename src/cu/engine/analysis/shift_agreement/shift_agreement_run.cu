// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shift_agreement_run.cu: the axis, volumes and the run
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "shift_agreement_internal.h"

static int agreement_axis(unsigned int *values, AgreementLayout layout, unsigned int axis, int inverse)
{
    const unsigned int length = layout.padded[axis];
    if (length < 2u)
    {
        return 1;
    }
    const unsigned int stride = layout.padded_strides[axis];
    const unsigned int blocks = (layout.total + SHIFT_AGREEMENT_BLOCK - 1u) / SHIFT_AGREEMENT_BLOCK;
    const AxisTables *const tables = axis_tables(length);
    int ok = (tables != NULL) ? 1 : 0;
    unsigned int logarithm = 0u;
    while ((1u << logarithm) < length)
    {
        logarithm += 1u;
    }
    if (inverse == 0)
    {
        for (unsigned int level = 0u; (ok != 0) && (level < logarithm); level += 2u)
        {
            const unsigned int levels = ((level + 1u) < logarithm) ? 2u : 1u;
            forward_kernel<<<blocks, SHIFT_AGREEMENT_BLOCK>>>(values, tables->roots[0], level, levels, length, stride,
                                                              layout.total);
            ok = agreement_launched();
        }
        return ok;
    }
    unsigned int remaining = logarithm;
    while ((ok != 0) && (remaining > 0u))
    {
        const unsigned int levels = (((remaining % 2u) == 1u) && (remaining == logarithm)) ? 1u : 2u;
        const unsigned int level = remaining - levels;
        inverse_kernel<<<blocks, SHIFT_AGREEMENT_BLOCK>>>(values, tables->roots[1], level, levels, length, stride,
                                                          layout.total);
        ok = agreement_launched();
        remaining = level;
    }
    return ok;
}

// The layout for `axes` extents, `weights` NULL for none. The layout pads each axis to the power of two at or above
// 2e - 1. That is the length that a cyclic transform needs to hold every lag without wrap. The answer is 0 for extents
// that the run errors. Those are an extent of 0 or past half the longest axis, a voxel count at the prime or past it,
// or a padded total past 2^31 - 1.
static int agreement_layout(unsigned int axes, const unsigned int *extents, const unsigned int *weights,
                            AgreementLayout *layout)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = layout, .bytes = sizeof(*layout), .val = 0);
    layout->axes = axes;
    unsigned long long voxels = 1ULL;
    unsigned long long padded_total = 1ULL;
    for (unsigned int axis = 0u; axis < axes; axis += 1u)
    {
        if ((extents[axis] == 0u) || (extents[axis] > (SHIFT_AGREEMENT_LONGEST_AXIS / 2u)))
        {
            return 0;
        }
        layout->extents[axis] = extents[axis];
        layout->weights[axis] = (weights != NULL) ? weights[axis] : 0u;
        voxels *= (unsigned long long)extents[axis];
        unsigned long long power = 1ULL;
        while (power < ((2ULL * (unsigned long long)extents[axis]) - 1ULL))
        {
            power <<= 1u;
        }
        // the extent is at max 2^22, and its power at max 2^23
        layout->padded[axis] = (unsigned int)power;
        padded_total *= power;
        if ((voxels >= (unsigned long long)SHIFT_AGREEMENT_PRIME) || (padded_total > 0x7FFFFFFFULL))
        {
            return 0;
        }
    }
    // the voxel count is below the prime, under 2^30, and the padded total at max 2^31 - 1
    layout->voxels = (unsigned int)voxels;
    layout->total = (unsigned int)padded_total;
    unsigned int stride = layout->total;
    unsigned int table_offset = 0u;
    for (unsigned int axis = 0u; axis < axes; axis += 1u)
    {
        stride /= layout->padded[axis];
        layout->padded_strides[axis] = stride;
        layout->table_offsets[axis] = table_offset;
        table_offset += layout->padded[axis];
    }
    return 1;
}

#define SHIFT_AGREEMENT_NONE_CACHED SHIFT_AGREEMENT_VOLUMES

static VolumesResident s_volumes_resident;

#define SHIFT_AGREEMENT_SLICES (2u + SHIFT_AGREEMENT_VOLUMES)

// The bytes of the slices for `total` padded elements and `words` bit words, in the order that the run takes them.
// First the before and after words, then the four volumes. The reserve of a caller plans them in this order
static DevicePoolPlan plan_volumes(size_t total, size_t words)
{
    const unsigned long long bytes[SHIFT_AGREEMENT_SLICES] = {
        (unsigned long long)words * sizeof(unsigned long long), (unsigned long long)words * sizeof(unsigned long long),
        (unsigned long long)total * sizeof(unsigned int),       (unsigned long long)total * sizeof(unsigned int),
        (unsigned long long)total * sizeof(unsigned int),       (unsigned long long)total * sizeof(unsigned int)};
    DevicePoolPlan plan = {0ULL, 0ULL, 0};
    for (unsigned int at = 0u; at < SHIFT_AGREEMENT_SLICES; at += 1u)
    {
        device_pool_plan_slice(&plan, bytes[at]);
    }
    return plan;
}

// the six cells of the volumes given back to the pool of the device, the newest first, and the record empty
static void release_volumes(VolumesResident *resident)
{
    for (unsigned int at = SHIFT_AGREEMENT_VOLUMES; at > 0u; at -= 1u)
    {
        agreement_cell_given((void **)&resident->volumes[at - 1u]);
    }
    agreement_cell_given((void **)&resident->after);
    agreement_cell_given((void **)&resident->before);
    platform_cell_released(resident->cached_words, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = resident, .bytes = sizeof(*resident), .val = 0);
    resident->cached = SHIFT_AGREEMENT_NONE_CACHED;
}

static int reserve_volumes(size_t total, size_t words)
{
    VolumesResident *const resident = &s_volumes_resident;
    if ((resident->total == total) && (resident->words == words) && (total != 0u))
    {
        return 1;
    }
    release_volumes(resident);
    // the words and the volumes, persistent cells of the pool of the device until the next size of the run
    const unsigned long long word_bytes = (unsigned long long)words * sizeof(unsigned long long);
    const unsigned long long volume_bytes = (unsigned long long)total * sizeof(unsigned int);
    int ok = agreement_cell_taken((void **)&resident->before, word_bytes) &&
             agreement_cell_taken((void **)&resident->after, word_bytes);
    for (unsigned int at = 0u; (ok != 0) && (at < SHIFT_AGREEMENT_VOLUMES); at += 1u)
    {
        ok = agreement_cell_taken((void **)&resident->volumes[at], volume_bytes);
    }
    resident->cached_words = (unsigned long long *)platform_cell_taken(words * sizeof(unsigned long long),
                                                                       platform_place_g_program_owner, 0);
    ok = ok && (resident->cached_words != NULL);
    if (ok == 0)
    {
        release_volumes(resident);
        return 0;
    }
    resident->total = total;
    resident->words = words;
    return 1;
}

static NegationResident s_negation_resident;

static int reserve_negation(const AgreementLayout *layout)
{
    NegationResident *const resident = &s_negation_resident;
    if ((resident->negation != NULL) && (resident->axes == layout->axes) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = resident->padded, .other = layout->padded,
                    .bytes = sizeof(resident->padded)) == 0))
    {
        return 1;
    }
    s_volumes_resident.cached = SHIFT_AGREEMENT_NONE_CACHED;
    agreement_cell_given((void **)&resident->negation);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = resident, .bytes = sizeof(*resident), .val = 0);
    size_t entries = 0u;
    for (unsigned int axis = 0u; axis < layout->axes; axis += 1u)
    {
        entries += (size_t)layout->padded[axis];
    }
    unsigned int *const host = (unsigned int *)platform_cell_taken(
        (entries + 1u) * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    int ok = (host != NULL) ? 1 : 0;
    for (unsigned int axis = 0u; (ok != 0) && (axis < layout->axes); axis += 1u)
    {
        if (layout->padded[axis] < 2u)
        {
            host[layout->table_offsets[axis]] = 0u;
            continue;
        }
        const AxisTables *const tables = axis_tables(layout->padded[axis]);
        ok = (tables != NULL) ? 1 : 0;
        if (ok != 0)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &host[layout->table_offsets[axis]], .src = tables->negation,
                       .bytes = (size_t)layout->padded[axis] * sizeof(unsigned int));
        }
    }
    ok = ok &&
         agreement_cell_taken((void **)&resident->negation,
                              ((unsigned long long)entries + 1ULL) * sizeof(unsigned int)) &&
         tessera_job_crossed_into(resident->negation, host, (unsigned long long)entries * sizeof(unsigned int));
    platform_cell_released(host, platform_place_g_program_owner);
    if (ok == 0)
    {
        agreement_cell_given((void **)&resident->negation);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = resident, .bytes = sizeof(*resident), .val = 0);
        return 0;
    }
    resident->axes = layout->axes;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = resident->padded, .src = layout->padded,
               .bytes = sizeof(resident->padded));
    return 1;
}

extern "C" unsigned long long shift_agreement_reserve_bytes(unsigned int axes, const unsigned int *extents)
{
    AgreementLayout layout;
    if ((extents == NULL) || (axes == 0u) || (axes > SHIFT_AGREEMENT_AXES) ||
        (agreement_layout(axes, extents, NULL, &layout) == 0))
    {
        return 0ULL;
    }
    const DevicePoolPlan plan = plan_volumes((size_t)layout.total, ((size_t)layout.voxels + 63u) / 64u);
    // the negation table, one entry past every padded coordinate, and each distinct length's two root tables
    unsigned long long tables = 1ULL;
    for (unsigned int axis = 0u; axis < axes; axis += 1u)
    {
        tables += layout.padded[axis];
        unsigned int earlier = 0u;
        while ((earlier < axis) && (layout.padded[earlier] != layout.padded[axis]))
        {
            earlier += 1u;
        }
        tables += ((earlier == axis) && (layout.padded[axis] >= 2u)) ? (2ULL * layout.padded[axis]) : 0ULL;
    }
    // The tables are small allocations beside the pool, which the device maps into shared pages. The count takes them
    // together, rounded up to the page
    DevicePoolPlan tables_plan = {0ULL, 0ULL, 0};
    device_pool_plan_slice(&tables_plan, tables * sizeof(unsigned int));
    return device_pool_plan_bytes(&plan) + device_pool_plan_bytes(&tables_plan);
}

extern "C" long shift_agreement_run(ShiftAgreementRequest *args)
{
    if ((args == NULL) || (args->before == NULL) || (args->after == NULL) || (args->axes == 0u) ||
        (args->axes > SHIFT_AGREEMENT_AXES))
    {
        return SHIFT_AGREEMENT_ERROR;
    }
    AgreementLayout layout;
    if (agreement_layout(args->axes, args->extents, args->weights, &layout) == 0)
    {
        return SHIFT_AGREEMENT_ERROR;
    }
    int devices = 0;
    if ((cudaGetDeviceCount(&devices) != cudaSuccess) || (devices < 1))
    {
        return SHIFT_AGREEMENT_ERROR;
    }

    const size_t total = (size_t)layout.total;
    const size_t words = ((size_t)layout.voxels + 63u) / 64u;
    unsigned int *const host_counts =
        (args->counts != NULL)
            ? (unsigned int *)platform_cell_taken(total * sizeof(unsigned int), platform_place_g_program_owner, 0)
            : NULL;
    int ok = ((args->counts == NULL) || (host_counts != NULL)) ? 1 : 0;
    ok = ok && reserve_volumes(total, words);
    ok = ok && reserve_negation(&layout);
    VolumesResident *const resident = &s_volumes_resident;
    unsigned long long *const device_before = resident->before;
    unsigned long long *const device_after = resident->after;
    // the kept transform has its own extents. One padding and one word count cover several extents
    const int reuse = (ok != 0) && (resident->cached != SHIFT_AGREEMENT_NONE_CACHED) &&
                      (resident->cached_axes == layout.axes) &&
                      (EMBED_CALL(memor.cmp, MemoriaCfg, .src = resident->cached_extents, .other = layout.extents,
                                  .bytes = sizeof(resident->cached_extents)) == 0) &&
                      (EMBED_CALL(memor.cmp, MemoriaCfg, .src = resident->cached_words, .other = args->before,
                                  .bytes = words * sizeof(unsigned long long)) == 0);
    unsigned int *roles[SHIFT_AGREEMENT_VOLUMES - 1u] = {NULL, NULL, NULL};
    unsigned int filled = 0u;
    for (unsigned int at = 0u; (ok != 0) && (at < SHIFT_AGREEMENT_VOLUMES); at += 1u)
    {
        if (((reuse != 0) && (at == resident->cached)) || (filled == (SHIFT_AGREEMENT_VOLUMES - 1u)))
        {
            continue;
        }
        roles[filled] = resident->volumes[at];
        filled += 1u;
    }
    unsigned int *reflected = roles[0];
    unsigned int *moved = roles[1];
    unsigned int *spare = roles[2];
    const unsigned int blocks = (layout.total + SHIFT_AGREEMENT_BLOCK - 1u) / SHIFT_AGREEMENT_BLOCK;
    const unsigned long long word_bytes = (unsigned long long)words * sizeof(unsigned long long);
    const unsigned long long volume_bytes = (unsigned long long)total * sizeof(unsigned int);
    ok = ok && ((reuse != 0) || tessera_job_crossed_into(device_before, args->before, word_bytes));
    ok = ok && tessera_job_crossed_into(device_after, args->after, word_bytes);
    ok = ok && ((reuse != 0) || tessera_job_cleared(reflected, volume_bytes));
    ok = ok && tessera_job_cleared(moved, volume_bytes);
    const unsigned int voxel_blocks = (layout.voxels + SHIFT_AGREEMENT_BLOCK - 1u) / SHIFT_AGREEMENT_BLOCK;
    if (ok != 0)
    {
        scatter_kernel<<<voxel_blocks, SHIFT_AGREEMENT_BLOCK>>>(device_before, device_after, (reuse != 0) ? 0u : 1u,
                                                                layout, reflected, moved);
        ok = agreement_launched();
    }
    if ((ok != 0) && (reuse != 0))
    {
        negate_kernel<<<blocks, SHIFT_AGREEMENT_BLOCK>>>(resident->volumes[resident->cached],
                                                         s_negation_resident.negation, layout, reflected);
        ok = agreement_launched();
    }

    for (unsigned int axis = 0u; (axis < layout.axes) && (ok != 0); axis += 1u)
    {
        ok = (reuse != 0) || (agreement_axis(reflected, layout, axis, 0) != 0);
        ok = ok && agreement_axis(moved, layout, axis, 0);
    }
    if (ok != 0)
    {
        multiply_kernel<<<blocks, SHIFT_AGREEMENT_BLOCK>>>(
            reflected, moved, device_agreement_power(layout.total, SHIFT_AGREEMENT_PRIME - 2u), layout.total);
        ok = agreement_launched();
    }
    for (unsigned int axis = 0u; (axis < layout.axes) && (ok != 0); axis += 1u)
    {
        ok = agreement_axis(reflected, layout, axis, 1);
    }
    if ((ok != 0) && (host_counts != NULL))
    {
        ok = tessera_job_crossed_out(host_counts, reflected, volume_bytes);
    }

    if (ok != 0)
    {
        choice_kernel<<<blocks, SHIFT_AGREEMENT_BLOCK>>>(layout.total, spare);
        ok = agreement_launched();
    }
    for (unsigned int stride = 1u; (stride < layout.total) && (ok != 0); stride <<= 1u)
    {
        const unsigned int pairs = (layout.total + (2u * stride) - 1u) / (2u * stride);
        const unsigned int pair_blocks = (pairs + SHIFT_AGREEMENT_BLOCK - 1u) / SHIFT_AGREEMENT_BLOCK;
        tournament_kernel<<<pair_blocks, SHIFT_AGREEMENT_BLOCK>>>(reflected, spare, stride, pairs, layout);
        ok = agreement_launched();
    }
    unsigned int winner = 0u;
    unsigned int winner_count = 0u;
    ok = ok && tessera_job_crossed_out(&winner, spare, sizeof(unsigned int));
    ok = ok && tessera_job_crossed_out(&winner_count, &reflected[winner], sizeof(unsigned int));

    long answer = SHIFT_AGREEMENT_ERROR;
    if (ok != 0)
    {
        const size_t best = (size_t)winner;
        size_t rest = best;
        for (unsigned int axis = layout.axes; axis > 0u; axis -= 1u)
        {
            const long long coordinate = (long long)(rest % layout.padded[axis - 1u]);
            rest /= layout.padded[axis - 1u];
            args->lag[axis - 1u] = (int)((coordinate < (long long)(layout.padded[axis - 1u] / 2u))
                                             ? coordinate
                                             : (coordinate - (long long)layout.padded[axis - 1u]));
        }
        for (unsigned int axis = 0u; axis < SHIFT_AGREEMENT_AXES; axis += 1u)
        {
            args->padded[axis] = (axis < layout.axes) ? layout.padded[axis] : 0u;
            if (axis >= layout.axes)
            {
                args->lag[axis] = 0;
            }
        }
        args->agreement = winner_count;
        if (args->counts != NULL)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = args->counts, .src = host_counts,
                       .bytes = total * sizeof(unsigned int));
        }
        answer = 0L;
    }
    resident->cached = SHIFT_AGREEMENT_NONE_CACHED;
    for (unsigned int at = 0u; (answer == 0L) && (at < SHIFT_AGREEMENT_VOLUMES); at += 1u)
    {
        if (resident->volumes[at] == moved)
        {
            resident->cached = at;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = resident->cached_words, .src = args->after,
                       .bytes = words * sizeof(unsigned long long));
            resident->cached_axes = layout.axes;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = resident->cached_extents, .src = layout.extents,
                       .bytes = sizeof(resident->cached_extents));
        }
    }
    platform_cell_released(host_counts, platform_place_g_program_owner);
    return answer;
}
