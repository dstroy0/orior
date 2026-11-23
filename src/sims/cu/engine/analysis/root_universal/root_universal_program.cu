// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// root_universal_program.cu: floors, regions, programs, volumes and the crystal
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "root_universal_internal.h"

static unsigned int root_floors(const unsigned long long extent[4])
{
    unsigned long long left[4];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = left, .src = extent, .bytes = sizeof(left));
    unsigned int floors = 0u;
    while ((left[0] > 1ULL) || (left[1] > 1ULL) || (left[2] > 1ULL) || (left[3] > 1ULL))
    {
        floors += 1u;
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            left[axis] = (left[axis] + 1ULL) / 2ULL;
        }
    }
    return floors;
}

// the coefficients an edge at `floor` acts on: the approximation after `floor` halvings of every axis
unsigned long long root_region(const unsigned long long extent[4], unsigned int floor)
{
    unsigned long long left[4];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = left, .src = extent, .bytes = sizeof(left));
    for (unsigned int level = 0u; level < floor; level += 1u)
    {
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            left[axis] = (left[axis] + 1ULL) / 2ULL;
        }
    }
    return left[0] * left[1] * left[2] * left[3];
}

void root_permutation(unsigned int *table, unsigned int bits, unsigned long long key)
{
    const unsigned int entries = 1u << bits;
    for (unsigned int index = 0u; index < entries; index += 1u)
    {
        table[index] = index;
    }
    for (unsigned int index = entries - 1u; index > 0u; index -= 1u)
    {
        // the draw below index + 1 is at max index, a position inside the table
        const unsigned int other = (unsigned int)sim_draw_below(key, index, (unsigned long long)index + 1ULL);
        const unsigned int temporary = table[index];
        table[index] = table[other];
        table[other] = temporary;
    }
}

void root_program_draw(RootProgram *program, unsigned int floors, unsigned long long key, int range_widest)
{
    // the draw below the edge ceiling is far under 2^32
    program->count = 1u + (unsigned int)sim_draw_below(key, 0ULL, ROOT_EDGES_DRAWN_MAX);
    for (unsigned int at = 0u; at < program->count; at += 1u)
    {
        const unsigned long long counter = 1ULL + (3ULL * at);
        TowerEdge *const edge = &program->edge[at];
        // the floor draw is at max floors and the width draw below the width ceiling, both under 2^32
        edge->floor = (unsigned int)sim_draw_below(key, counter, (unsigned long long)floors + 1ULL);
        edge->index_bits = 1u + (unsigned int)sim_draw_below(key, counter + 1ULL, ROOT_BITS_DRAWN_MAX);
        if ((range_widest != 0) && (at == 0u))
        {
            edge->index_bits = TOWER_EDGE_INDEX_BITS_MAX;
        }
        root_permutation(program->table[at], edge->index_bits, sim_draw(key, counter + 2ULL));
        edge->forward = program->table[at];
    }
}

// one edge of `bits` on every floor from `first` to `last`, each its own keyed permutation
void root_program_place(RootProgram *program, unsigned int bits, unsigned int first, unsigned int last,
                        unsigned long long key)
{
    program->count = 0u;
    for (unsigned int floor = first; floor <= last; floor += 1u)
    {
        const unsigned int at = program->count;
        TowerEdge *const edge = &program->edge[at];
        edge->floor = floor;
        edge->index_bits = bits;
        root_permutation(program->table[at], bits, sim_draw(key, floor));
        edge->forward = program->table[at];
        program->count += 1u;
    }
}

int root_volume_open(SimResults *results, RootVolume *volume, const unsigned long long extent[4])
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &volume->scene, .bytes = sizeof(volume->scene), .val = 0);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = volume->extent, .src = extent, .bytes = sizeof(volume->extent));
    volume->scene.frames = extent[0];
    volume->scene.extent[0] = extent[1];
    volume->scene.extent[1] = extent[2];
    volume->scene.extent[2] = extent[3];
    volume->scene.background = 200ULL;
    volume->scene.ramp = 1ULL;
    volume->scene.bodies = ROOT_FOUNDERS;
    volume->lanes = extent[0] * extent[1] * extent[2] * extent[3];
    volume->floors = root_floors(extent);
    volume->host = (unsigned short *)platform_cell_taken((size_t)volume->lanes * sizeof(unsigned short),
                                                         platform_place_g_program_owner, 0);
    volume->rebuilt = (unsigned short *)platform_cell_taken((size_t)volume->lanes * sizeof(unsigned short),
                                                            platform_place_g_program_owner, 0);
    int ok = (volume->host != NULL) && (volume->rebuilt != NULL);
    sim_check(results, ok, "the volume's host lanes were allocated");
    ok = ok && sim_status_check(results, cudaMalloc((void **)&volume->device, volume->lanes * sizeof(unsigned short)),
                                "device lanes");
    return ok;
}

void root_volume_close(RootVolume *volume)
{
    cudaFree(volume->device);
    platform_cell_released(volume->host, platform_place_g_program_owner);
    platform_cell_released(volume->rebuilt, platform_place_g_program_owner);
}

int root_volume_render(SimResults *results, RootVolume *volume, const SimCamera *camera)
{
    SimDraws draws;
    draws.key = camera->key ^ ROOT_FOUNDER_PURPOSE;
    draws.counter = 0ULL;
    sim_founders_draw(&draws, &volume->scene, volume->body, ROOT_FOUNDERS);
    volume->scene.body = volume->body;
    unsigned long long clipped = 0ULL;
    int ok = sim_render(results, &volume->scene, camera, volume->device, NULL, NULL, &clipped);
    sim_check(results, ok && (clipped == 0ULL), "the volume rendered with nothing clipped");
    ok = ok && (clipped == 0ULL) &&
         sim_status_check(
             results,
             cudaMemcpy(volume->host, volume->device, volume->lanes * sizeof(unsigned short), cudaMemcpyDeviceToHost),
             "lanes read");
    return ok;
}

int root_lift_crystal(SimResults *results, const RootVolume *volume, const TowerEdge *edges, unsigned int edge_count,
                      int *crystal)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    unsigned int floors = 0u;
    TowerLiftRequest lift;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &lift, .bytes = sizeof(lift), .val = 0);
    lift.device_lanes = volume->device;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = lift.extent, .src = volume->extent, .bytes = sizeof(lift.extent));
    lift.coefficients = &coefficients;
    lift.scratch = &scratch;
    lift.floors = &floors;
    lift.edges = edges;
    lift.edge_count = edge_count;
    lift.error = &error;
    int ok = (tower_lift(&lift) == 0L) && (error.type == ENGINE_ERROR_NONE);
    sim_check(results, ok, "tower_lift accepted the edge program");
    ok = ok && sim_status_check(results,
                                cudaMemcpy(crystal, coefficients, volume->lanes * sizeof(int), cudaMemcpyDeviceToHost),
                                "crystal read");
    return ok;
}

// the full crystal path: lift with the edges, code, wipe the retained coefficients so only the stream carries
// them, decode, lower with the same edges, and compare the rebuilt lanes with the rendered ones
int root_crystal_trip(SimResults *results, RootVolume *volume, const TowerEdge *edges, unsigned int edge_count,
                      int *crystal, unsigned long long *bits, int *exact)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    *exact = 0;
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    unsigned int floors = 0u;
    TowerLiftRequest lift;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &lift, .bytes = sizeof(lift), .val = 0);
    lift.device_lanes = volume->device;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = lift.extent, .src = volume->extent, .bytes = sizeof(lift.extent));
    lift.coefficients = &coefficients;
    lift.scratch = &scratch;
    lift.floors = &floors;
    lift.edges = edges;
    lift.edge_count = edge_count;
    lift.error = &error;
    int ok = (tower_lift(&lift) == 0L) && (error.type == ENGINE_ERROR_NONE);
    sim_check(results, ok, "tower_lift accepted the edge program");
    if (ok && (crystal != NULL))
    {
        ok = sim_status_check(results,
                              cudaMemcpy(crystal, coefficients, volume->lanes * sizeof(int), cudaMemcpyDeviceToHost),
                              "crystal read");
    }
    unsigned long long chunks = 0ULL;
    const unsigned long long *offsets = NULL;
    const unsigned int *stream = NULL;
    if (ok)
    {
        CompressionEncodeRequest code;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &code, .bytes = sizeof(code), .val = 0);
        code.device_coefficients = coefficients;
        code.count = volume->lanes;
        code.device_scratch = scratch;
        code.chunks = &chunks;
        code.bits = bits;
        code.offsets = &offsets;
        code.stream = &stream;
        code.error = &error;
        ok = (compression_encode(&code) == 0L) && (error.type == ENGINE_ERROR_NONE);
        sim_check(results, ok, "compression_encode coded the crystal");
    }
    int *device_buffer = NULL;
    if (ok)
    {
        ok = (tower_capacity(volume->extent, &device_buffer, &error) == 0L) &&
             sim_status_check(results, cudaMemset(device_buffer, 0, volume->lanes * sizeof(int)), "crystal wipe");
        sim_check(results, ok, "the stored crystal was wiped before decoding");
    }
    if (ok)
    {
        CompressionDecodeRequest decode;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &decode, .bytes = sizeof(decode), .val = 0);
        decode.offsets = offsets;
        decode.chunks = chunks;
        decode.stream = stream;
        decode.bits = *bits;
        decode.count = volume->lanes;
        decode.device_coefficients = device_buffer;
        decode.error = &error;
        ok = (compression_decode(&decode) == 0L) && (error.type == ENGINE_ERROR_NONE);
        sim_check(results, ok, "compression_decode read the crystal back");
    }
    unsigned long long mismatches = 1ULL;
    if (ok)
    {
        const unsigned short *device_rebuilt = NULL;
        TowerLowerRequest lower;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &lower, .bytes = sizeof(lower), .val = 0);
        lower.device_lanes = volume->device;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = lower.extent, .src = volume->extent, .bytes = sizeof(lower.extent));
        lower.mismatches = &mismatches;
        lower.device_rebuilt = &device_rebuilt;
        lower.rebuilt = volume->rebuilt;
        lower.edges = edges;
        lower.edge_count = edge_count;
        lower.error = &error;
        ok = (tower_lower(&lower) == 0L) && (error.type == ENGINE_ERROR_NONE);
        sim_check(results, ok, "tower_lower accepted the edge program");
    }
    if (ok)
    {
        *exact =
            (mismatches == 0ULL) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = volume->rebuilt, .other = volume->host,
                                                .bytes = (size_t)volume->lanes * sizeof(unsigned short)) == 0);
    }
    return ok;
}
